# SPDX-License-Identifier: MIT
"""Release tool: checks, desktop and web packages, signing and checksums.

Subcommands (docs/release.md describes the whole release):

  version         print the release version (ZB_VERSION in src/core/version.h)
  check           the version is consistent (src/core/version.h and
                  assets/base/package.lua), CHANGELOG.md has an entry for it and
                  the git tree is clean (--allow-dirty skips the last check)
  package         configure and build a preset (default: release), run CPack and
                  copy the verified archive into the dist folder; --dry-run only
                  prints the plan
  web             zip the web builds (build/web-st/bin and/or build/web/bin) as
                  zauberer-<version>-web.zip, next to the assembled folder
  sums            write SHA256SUMS for every artefact in the dist folder (and
                  sign it with GPG when ZB_SIGN_GPG_KEY is set)
  sign-binaries   sign the executables of a staged package (run by CPack through
                  cmake/ZbSign.cmake)
  credits         write CREDITS.md from the packages' credits.lua files

Signing is optional. It happens only when these environment variables are set,
and is skipped with a log line otherwise:
  ZB_SIGN_WINDOWS_CERT    .pfx certificate for signtool (ZB_SIGN_WINDOWS_PASSWORD,
                          ZB_SIGN_TIMESTAMP_URL and ZB_SIGNTOOL are optional)
  ZB_SIGN_MACOS_IDENTITY  codesign identity (ZB_CODESIGN is optional)
  ZB_SIGN_GPG_KEY         GPG key that signs SHA256SUMS (ZB_GPG is optional)

Exit codes: 0 success, 1 a check failed, 2 usage, I/O or command error.
Standard library only.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import platform as _platform
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path
from typing import Callable, Iterable, Mapping, Sequence

import lua_literal
from tool_common import EXIT_ERROR, EXIT_FAIL, EXIT_OK, REPO_ROOT, ToolError

VERSION_HEADER = "src/core/version.h"
BASE_MANIFEST = "assets/base/package.lua"
CHANGELOG = "CHANGELOG.md"
SUMS_FILE = "SHA256SUMS"
DEFAULT_PRESET = "release"
DEFAULT_DIST = "build/dist"
WEB_ST_DIR = "build/web-st/bin"
WEB_MT_DIR = "build/web/bin"
SIGNATURE_SUFFIXES = (".asc", ".sig")

_VERSION_DEFINE_RE = re.compile(r'^\s*#\s*define\s+ZB_VERSION\s+"([^"]*)"', re.MULTILINE)
SEMVER_RE = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)"
                       r"(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
                       r"(?:\+([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$")

# A command runner: runs argv (in cwd, with env) and returns the exit code.
Runner = Callable[..., int]


def run_command(argv: Sequence[str], cwd: Path | None = None, env: Mapping[str, str] | None = None) -> int:
    """Runs a command with inherited output and returns its exit code."""
    try:
        return subprocess.run(list(argv), cwd=cwd, env=dict(env) if env is not None else None).returncode
    except OSError as error:
        raise ToolError(f"cannot run {argv[0]}: {error}") from error


def log(message: str) -> None:
    print(f"release: {message}", flush=True)


# --- Version ------------------------------------------------------------------

def read_version(root: Path) -> str:
    """ZB_VERSION from src/core/version.h: the one source of the release version."""
    path = root / VERSION_HEADER
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as error:
        raise ToolError(f"cannot read {VERSION_HEADER}: {error}") from error
    match = _VERSION_DEFINE_RE.search(text)
    if not match:
        raise ToolError(f"{VERSION_HEADER}: no '#define ZB_VERSION \"...\"'")
    return match.group(1)


def manifest_version(root: Path) -> str | None:
    """The version of the base package's package.lua (None if absent)."""
    path = root / BASE_MANIFEST
    try:
        data = lua_literal.parse_file(path)
    except OSError as error:
        raise ToolError(f"cannot read {BASE_MANIFEST}: {error}") from error
    except lua_literal.LuaSyntaxError as error:
        raise ToolError(f"cannot parse {BASE_MANIFEST}: {error}") from error
    value = data.get("version") if isinstance(data, dict) else None
    return value if isinstance(value, str) else None


def changelog_has_entry(text: str, version: str) -> bool:
    """True if a level-2 heading names the version: '## [1.0.0] - date' or '## 1.0.0'."""
    pattern = re.compile(r"^##\s+\[?v?" + re.escape(version) + r"\]?(\s|$)", re.MULTILINE)
    return pattern.search(text) is not None


def dirty_files(root: Path) -> list[str]:
    """`git status --porcelain` lines of the tree (empty when clean)."""
    try:
        result = subprocess.run(["git", "status", "--porcelain", "--untracked-files=normal"], cwd=root,
                                capture_output=True, text=True)
    except OSError as error:
        raise ToolError(f"cannot run git: {error}") from error
    if result.returncode != 0:
        raise ToolError(f"git status failed in {root}: {result.stderr.strip()}")
    return [line for line in result.stdout.splitlines() if line.strip()]


def check_release(root: Path, allow_dirty: bool = False) -> list[str]:
    """Problems that stop a release (empty when it may go ahead)."""
    problems = []
    version = read_version(root)
    if not SEMVER_RE.match(version):
        problems.append(f"{VERSION_HEADER}: ZB_VERSION '{version}' is not a semantic version")
    base = manifest_version(root)
    if base != version:
        problems.append(f"{BASE_MANIFEST}: version '{base}' does not match ZB_VERSION '{version}' "
                        f"in {VERSION_HEADER}")
    changelog = root / CHANGELOG
    if not changelog.is_file():
        problems.append(f"{CHANGELOG} is missing")
    elif not changelog_has_entry(changelog.read_text(encoding="utf-8"), version):
        problems.append(f"{CHANGELOG} has no entry for {version} (a heading '## [{version}] - <date>')")
    if not allow_dirty:
        dirty = dirty_files(root)
        if dirty:
            shown = ", ".join(line[3:] for line in dirty[:5]) + (" ..." if len(dirty) > 5 else "")
            problems.append(f"the git tree is not clean ({len(dirty)} change(s): {shown})")
    return problems


# --- Desktop packages ---------------------------------------------------------

def normalize_os(name: str) -> str:
    name = name.lower()
    if name.startswith("win") or name in ("cygwin", "msys"):
        return "windows"
    if name in ("darwin", "macos", "mac"):
        return "macos"
    return name


def normalize_arch(machine: str) -> str:
    machine = machine.lower()
    if machine in ("amd64", "x64", "x86_64"):
        return "x86_64"
    if machine in ("aarch64", "arm64"):
        return "arm64"
    return machine


def platform_tag(system: str | None = None, machine: str | None = None) -> str:
    """'<os>-<arch>' as cmake/ZbPackaging.cmake names it, e.g. 'linux-x86_64'."""
    return f"{normalize_os(system or _platform.system())}-{normalize_arch(machine or _platform.machine())}"


def archive_extension(tag: str) -> str:
    """.tar.gz on Linux (and other Unix), .zip on Windows and macOS."""
    return ".zip" if tag.split("-")[0] in ("windows", "macos") else ".tar.gz"


def package_basename(version: str, tag: str) -> str:
    return f"zauberer-{version}-{tag}"


def package_plan(root: Path, preset: str, tag: str, configure_args: Sequence[str] = ()) -> list[list[str]]:
    """The commands that build and package a preset."""
    binary_dir = root / "build" / preset
    return [
        ["cmake", "--preset", preset, f"-DZB_PACKAGE_PLATFORM={tag}", *configure_args],
        ["cmake", "--build", "--preset", preset],
        ["cpack", "--config", str(binary_dir / "CPackConfig.cmake"), "-B", str(binary_dir / "packages")],
    ]


def _archive_members(path: Path) -> dict[str, int]:
    """Member name -> unix mode (0 when unknown) of a .tar.gz or .zip archive."""
    members: dict[str, int] = {}
    if path.name.endswith(".zip"):
        with zipfile.ZipFile(path) as archive:
            for info in archive.infolist():
                mode = (info.external_attr >> 16) & 0o7777
                members[info.filename.rstrip("/")] = mode
                if info.is_dir():
                    members[info.filename.rstrip("/") + "/"] = mode
    else:
        with tarfile.open(path, "r:*") as archive:
            for info in archive.getmembers():
                members[info.name.rstrip("/")] = info.mode
                if info.isdir():
                    members[info.name.rstrip("/") + "/"] = info.mode
    return members


def verify_package(path: Path, tag: str) -> list[str]:
    """Checks a desktop archive against the shipped layout (concept §9.2): one
    folder named like the archive with the executables, assets/base, mods/,
    the licenses, notices, credits, readme and changelog."""
    name = path.name
    for ext in (".tar.gz", ".zip"):
        if name.endswith(ext):
            name = name[:-len(ext)]
    windows = tag.startswith("windows")
    exe = ".exe" if windows else ""
    try:
        members = _archive_members(path)
    except (OSError, tarfile.TarError, zipfile.BadZipFile) as error:
        return [f"{path.name}: cannot read the archive: {error}"]
    problems = []
    top = {member.split("/")[0] for member in members if member}
    if top != {name}:
        problems.append(f"{path.name}: expected one top-level folder '{name}', found {sorted(top)}")
    required = [f"zauberer{exe}", f"zauberer-relay{exe}", "assets/base/package.lua", "assets/base/credits.lua",
                "mods/", "LICENSES/", "LICENSE", "README.md", "CHANGELOG.md", "CREDITS.md",
                "THIRD_PARTY_NOTICES.md"]
    for entry in required:
        wanted = f"{name}/{entry}"
        present = (any(member.startswith(wanted) for member in members) if entry.endswith("/")
                   else wanted in members)
        if not present:
            problems.append(f"{path.name}: missing {entry}")
    if not windows:
        for binary in ("zauberer", "zauberer-relay"):
            mode = members.get(f"{name}/{binary}")
            if mode is not None and mode and not mode & 0o111:
                problems.append(f"{path.name}: {binary} is not executable")
    return problems


def build_packages(root: Path, preset: str, dist: Path, tag: str, configure_args: Sequence[str],
                   dry_run: bool, runner: Runner = run_command) -> list[Path]:
    version = read_version(root)
    basename = package_basename(version, tag)
    archive = basename + archive_extension(tag)
    built = root / "build" / preset / "packages" / archive
    commands = package_plan(root, preset, tag, configure_args)
    log(f"package {archive} (preset {preset}, platform {tag}){' - dry run' if dry_run else ''}")
    for command in commands:
        log(("would run: " if dry_run else "run: ") + " ".join(command))
    log(f"{'would copy' if dry_run else 'copy'} {built} -> {dist / archive}")
    if dry_run:
        return [dist / archive]
    for command in commands:
        code = runner(command, cwd=root)
        if code != 0:
            raise ToolError(f"'{' '.join(command)}' failed with exit code {code}")
    if not built.is_file():
        found = sorted(p.name for p in built.parent.glob("zauberer-*")) if built.parent.is_dir() else []
        raise ToolError(f"CPack did not write {built} (found: {', '.join(found) or 'nothing'})")
    problems = verify_package(built, tag)
    if problems:
        raise ToolError("the package does not have the shipped layout:\n  " + "\n  ".join(problems))
    dist.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, dist / archive)
    log(f"wrote {dist / archive} ({(dist / archive).stat().st_size / 1e6:.1f} MB)")
    return [dist / archive]


# --- Web package ----------------------------------------------------------------

def _is_web_build(path: Path) -> bool:
    return (path / "index.html").is_file()


def assemble_web(web_st: Path | None, web_mt: Path | None, out: Path) -> list[str]:
    """Puts the web build(s) in one folder, as the CI web job does: the whole
    single-threaded build, plus the threaded engine (zauberer.js/.wasm and any
    worker) from the threaded one. Returns the relative file names."""
    if web_st is None and web_mt is None:
        raise ToolError("no web build given")
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    if web_st is not None:
        shutil.copytree(web_st, out, dirs_exist_ok=True)
        if web_mt is not None:
            for path in sorted(web_mt.iterdir()):
                if path.is_file() and path.name.startswith("zauberer."):
                    shutil.copy2(path, out / path.name)
    else:
        shutil.copytree(web_mt, out, dirs_exist_ok=True)
    return sorted(p.relative_to(out).as_posix() for p in out.rglob("*") if p.is_file())


def zip_folder(folder: Path, archive: Path) -> None:
    """Zips a folder (as its own top-level folder) reproducibly: sorted entries,
    fixed timestamps, permissions kept."""
    archive.parent.mkdir(parents=True, exist_ok=True)
    files = sorted(p for p in folder.rglob("*") if p.is_file())
    with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as out:
        for path in files:
            info = zipfile.ZipInfo(f"{folder.name}/{path.relative_to(folder).as_posix()}", (1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            mode = 0o755 if path.stat().st_mode & stat.S_IXUSR else 0o644
            info.external_attr = (stat.S_IFREG | mode) << 16
            out.writestr(info, path.read_bytes())


def build_web(root: Path, web_st: Path | None, web_mt: Path | None, dist: Path, dry_run: bool,
              explicit: bool = False) -> Path:
    """Assembles and zips the web build as zauberer-<version>-web.zip."""
    version = read_version(root)
    builds = []
    for label, path in (("single-threaded", web_st), ("threaded", web_mt)):
        if path is None:
            continue
        if _is_web_build(path):
            builds.append((label, path))
        elif explicit:
            raise ToolError(f"{path} is not a web build (no index.html); build the web presets first")
    if not builds:
        raise ToolError(f"no web build found (looked in {web_st} and {web_mt}); "
                        "build the web-st and/or web presets first")
    found = dict(builds)
    st = found.get("single-threaded")
    mt = found.get("threaded")
    folder = dist / f"zauberer-{version}-web"
    archive = dist / f"zauberer-{version}-web.zip"
    log(f"web {archive.name} from {', '.join(f'{label} {path}' for label, path in builds)}"
        f"{' - dry run' if dry_run else ''}")
    if dry_run:
        log(f"would assemble {folder} and zip it as {archive}")
        return archive
    names = assemble_web(st, mt, folder)
    zip_folder(folder, archive)
    log(f"wrote {archive} ({len(names)} files, {archive.stat().st_size / 1e6:.1f} MB); folder {folder}")
    return archive


# --- Checksums and signing ----------------------------------------------------------

def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def artefacts(dist: Path) -> list[Path]:
    """The files a release publishes: every regular file directly in dist except
    the checksum file and signatures."""
    return sorted(p for p in dist.iterdir() if p.is_file() and p.name != SUMS_FILE
                  and not p.name.endswith(SIGNATURE_SUFFIXES))


def write_sums(dist: Path) -> Path:
    """Writes SHA256SUMS ('<hex>  <name>' per artefact, sorted; `sha256sum -c` reads it)."""
    files = artefacts(dist) if dist.is_dir() else []
    if not files:
        raise ToolError(f"no artefacts in {dist}")
    sums = dist / SUMS_FILE
    sums.write_text("".join(f"{sha256_of(p)}  {p.name}\n" for p in files), encoding="utf-8")
    log(f"wrote {sums} ({len(files)} artefact(s))")
    return sums


def gpg_sign(paths: Iterable[Path], env: Mapping[str, str], runner: Runner = run_command) -> list[Path]:
    """Detached ASCII-armoured GPG signatures (<file>.asc) with ZB_SIGN_GPG_KEY."""
    key = env.get("ZB_SIGN_GPG_KEY", "")
    if not key:
        log("GPG signing skipped: ZB_SIGN_GPG_KEY is not set")
        return []
    gpg = env.get("ZB_GPG", "gpg")
    signatures = []
    for path in paths:
        signature = path.with_name(path.name + ".asc")
        command = [gpg, "--batch", "--yes", "--local-user", key, "--armor", "--detach-sign",
                   "--output", str(signature), str(path)]
        log(f"GPG signing {path.name}")
        if runner(command) != 0:
            raise ToolError(f"gpg could not sign {path}")
        signatures.append(signature)
    return signatures


def find_executables(staged: Path, os_name: str) -> list[Path]:
    """The game and relay executables below a staged package folder."""
    names = {"zauberer.exe", "zauberer-relay.exe"} if os_name == "windows" else {"zauberer", "zauberer-relay"}
    return sorted(p for p in staged.rglob("*") if p.is_file() and p.name in names)


def sign_binaries(staged: Path, os_name: str, env: Mapping[str, str], runner: Runner = run_command) -> list[Path]:
    """Signs the executables of a staged package when the platform's signing
    variable is set: signtool on Windows, codesign on macOS. Linux packages are
    covered by the GPG signature of SHA256SUMS."""
    binaries = find_executables(staged, os_name)
    if os_name == "windows":
        cert = env.get("ZB_SIGN_WINDOWS_CERT", "")
        if not cert:
            log("Windows signing skipped: ZB_SIGN_WINDOWS_CERT is not set")
            return []
        command = [env.get("ZB_SIGNTOOL", "signtool"), "sign", "/fd", "SHA256", "/f", cert]
        if env.get("ZB_SIGN_WINDOWS_PASSWORD"):
            command += ["/p", env["ZB_SIGN_WINDOWS_PASSWORD"]]
        command += ["/tr", env.get("ZB_SIGN_TIMESTAMP_URL", "http://timestamp.digicert.com"), "/td", "SHA256"]
        commands = [command + [str(p) for p in binaries]] if binaries else []
    elif os_name == "macos":
        identity = env.get("ZB_SIGN_MACOS_IDENTITY", "")
        if not identity:
            log("macOS signing skipped: ZB_SIGN_MACOS_IDENTITY is not set")
            return []
        codesign = env.get("ZB_CODESIGN", "codesign")
        commands = []
        for path in binaries:
            commands.append([codesign, "--force", "--timestamp", "--options", "runtime", "--sign", identity, str(path)])
            commands.append([codesign, "--verify", "--strict", str(path)])
    else:
        log(f"executable signing not applicable on {os_name}: the GPG signature of {SUMS_FILE} covers the archive")
        return []
    if not binaries:
        raise ToolError(f"no executables to sign below {staged}")
    for command in commands:
        log("signing: " + " ".join(c if c != env.get("ZB_SIGN_WINDOWS_PASSWORD") else "***" for c in command))
        if runner(command) != 0:
            raise ToolError(f"signing failed: {command[0]}")
    return binaries


# --- Credits ------------------------------------------------------------------

def _md(text) -> str:
    return str(text).replace("|", "\\|").replace("\n", " ")


def credits_markdown(root: Path) -> str:
    """CREDITS.md: the third-party code and every package's asset credits."""
    lines = ["# Credits", "",
             "Generated from the packages' `credits.lua` and `data/third_party.lua` files by "
             "`tools/release.py credits`. The game shows the same list in its credits screen (F1).", ""]
    packages = []
    for base in ("assets", "mods"):
        if (root / base).is_dir():
            packages += sorted(p.parent for p in (root / base).glob("*/package.lua"))
    for package in packages:
        third_party = package / "data" / "third_party.lua"
        if third_party.is_file():
            entries = lua_literal.parse_file(third_party) or []
            lines += [f"## Third-party code ({package.name})", "", "| Component | Version | License | Source |",
                      "| --- | --- | --- | --- |"]
            for entry in entries if isinstance(entries, list) else []:
                lines.append(f"| {_md(entry.get('name', '?'))} | {_md(entry.get('version', ''))} | "
                             f"{_md(entry.get('license', '?'))} | {_md(entry.get('source', ''))} |")
            lines.append("")
    for package in packages:
        manifest = lua_literal.parse_file(package / "package.lua")
        title = manifest.get("name", package.name) if isinstance(manifest, dict) else package.name
        credits_file = package / "credits.lua"
        entries = lua_literal.parse_file(credits_file) if credits_file.is_file() else []
        entries = entries if isinstance(entries, list) else []
        rel = package.relative_to(root).as_posix()
        lines += [f"## {_md(title)} (`{rel}`)", ""]
        if not entries:
            lines += ["No asset files.", ""]
            continue
        lines += ["| File | Author | License | Source |", "| --- | --- | --- | --- |"]
        for entry in entries:
            lines.append(f"| `{_md(entry.get('file', '?'))}` | {_md(entry.get('author', '?'))} | "
                         f"{_md(entry.get('license', '?'))} | {_md(entry.get('source', ''))} |")
        lines.append("")
    return "\n".join(lines)


# --- Command line ---------------------------------------------------------------

def _resolve(root: Path, path: Path) -> Path:
    return path if path.is_absolute() else root / path


def main(argv: list[str] | None = None, env: Mapping[str, str] | None = None,
         runner: Runner = run_command) -> int:
    parser = argparse.ArgumentParser(description="Zauberer release tool (docs/release.md).")
    parser.add_argument("--root", type=Path, default=REPO_ROOT, help="repository root (default: %(default)s)")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("version", help="print the release version")
    p_check = sub.add_parser("check", help="version, changelog and clean-tree checks")
    p_check.add_argument("--allow-dirty", action="store_true", help="skip the clean-tree check")
    p_package = sub.add_parser("package", help="build and package a desktop release")
    p_package.add_argument("--preset", default=DEFAULT_PRESET, help="CMake preset (default: %(default)s)")
    p_package.add_argument("--dist", type=Path, default=Path(DEFAULT_DIST), help="output folder (default: %(default)s)")
    p_package.add_argument("--platform", help="platform tag of the package name (default: this machine's)")
    p_package.add_argument("--configure-arg", action="append", default=[], metavar="ARG",
                           help="extra argument for the configure step (repeatable)")
    p_package.add_argument("--dry-run", action="store_true", help="print the plan without building")
    p_web = sub.add_parser("web", help="zip the web build(s)")
    p_web.add_argument("--web-st", type=Path, help=f"single-threaded build (default: {WEB_ST_DIR})")
    p_web.add_argument("--web", type=Path, dest="web_mt", help=f"threaded build (default: {WEB_MT_DIR})")
    p_web.add_argument("--dist", type=Path, default=Path(DEFAULT_DIST), help="output folder (default: %(default)s)")
    p_web.add_argument("--dry-run", action="store_true", help="print the plan without writing")
    p_sums = sub.add_parser("sums", help="write (and optionally sign) SHA256SUMS")
    p_sums.add_argument("--dist", type=Path, default=Path(DEFAULT_DIST), help="artefact folder (default: %(default)s)")
    p_sign = sub.add_parser("sign-binaries", help="sign the executables of a staged package")
    p_sign.add_argument("staged", type=Path)
    p_sign.add_argument("--os", help="windows, macos or linux (default: this machine's)")
    p_credits = sub.add_parser("credits", help="write CREDITS.md")
    p_credits.add_argument("--out", type=Path, required=True)
    try:
        args = parser.parse_args(argv)
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR

    env = os.environ if env is None else env
    root = args.root.resolve()
    try:
        if args.command == "version":
            print(read_version(root))
        elif args.command == "check":
            version = read_version(root)
            problems = check_release(root, allow_dirty=args.allow_dirty)
            for problem in problems:
                print(f"release: {problem}")
            if problems:
                print(f"release: {version} is not ready ({len(problems)} problem(s))")
                return EXIT_FAIL
            log(f"{version} is ready to release" + (" (clean-tree check skipped)" if args.allow_dirty else ""))
        elif args.command == "package":
            tag = args.platform or platform_tag()
            build_packages(root, args.preset, _resolve(root, args.dist), tag, args.configure_arg, args.dry_run,
                           runner)
        elif args.command == "web":
            explicit = args.web_st is not None or args.web_mt is not None
            web_st = _resolve(root, args.web_st) if args.web_st else (None if explicit else root / WEB_ST_DIR)
            web_mt = _resolve(root, args.web_mt) if args.web_mt else (None if explicit else root / WEB_MT_DIR)
            build_web(root, web_st, web_mt, _resolve(root, args.dist), args.dry_run, explicit)
        elif args.command == "sums":
            sums = write_sums(_resolve(root, args.dist))
            gpg_sign([sums], env, runner)
        elif args.command == "sign-binaries":
            sign_binaries(args.staged, normalize_os(args.os or _platform.system()), env, runner)
        elif args.command == "credits":
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(credits_markdown(root), encoding="utf-8")
            log(f"wrote {args.out}")
    except (ToolError, lua_literal.LuaSyntaxError, OSError) as error:
        print(f"release: error: {error}", file=sys.stderr)
        return EXIT_ERROR
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
