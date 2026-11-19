# SPDX-License-Identifier: MIT
# REUSE-IgnoreStart
"""Tests for tools/release.py and the manual release workflow. Fast: no real
builds; package runs use --dry-run or a fake command runner, archives are made
in temporary folders."""
import contextlib
import hashlib
import io
import os
import subprocess
import sys
import tarfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from toolsupport import REPO_ROOT, TempDirTestCase, run_main  # noqa: E402

import release  # noqa: E402

try:
    import yaml
except ImportError:  # pragma: no cover - PyYAML is part of the CI images
    yaml = None

VERSION_H = '/* SPDX-License-Identifier: MIT */\n#define ZB_VERSION "{v}"\n#define ZB_API_VERSION "1.0"\n'
MANIFEST = '-- SPDX-License-Identifier: CC-BY-4.0\nreturn {{ id = "base", name = "Base", version = "{v}", api = "1.0" }}\n'
CHANGELOG = "# Changelog\n\n## [{v}] - 2026-11-19\n\n- Things.\n"


class FakeRunner:
    """Records commands instead of running them; fails the commands named in fail."""

    def __init__(self, fail=(), on_run=None):
        self.commands = []
        self.fail = fail
        self.on_run = on_run

    def __call__(self, argv, cwd=None, env=None):
        self.commands.append(list(argv))
        if self.on_run:
            self.on_run(list(argv))
        return 1 if argv[0] in self.fail else 0


def make_tar(path: Path, top: str, files: dict, modes: dict | None = None) -> Path:
    with tarfile.open(path, "w:gz") as archive:
        for name, data in files.items():
            info = tarfile.TarInfo(f"{top}/{name}")
            if name.endswith("/"):
                info.type = tarfile.DIRTYPE
                info.mode = 0o755
                archive.addfile(info)
                continue
            info.size = len(data)
            info.mode = (modes or {}).get(name, 0o644)
            archive.addfile(info, io.BytesIO(data))
    return path


LAYOUT = {"zauberer": b"\x7fELF", "zauberer-relay": b"\x7fELF", "assets/base/package.lua": b"return {}",
          "assets/base/credits.lua": b"return {}", "mods/README.md": b"# Mods", "LICENSES/MIT.txt": b"MIT",
          "LICENSE": b"MIT", "README.md": b"# Zauberer", "CHANGELOG.md": b"# Changelog",
          "CREDITS.md": b"# Credits", "THIRD_PARTY_NOTICES.md": b"# Notices"}
EXEC = {"zauberer": 0o755, "zauberer-relay": 0o755}


class ReleaseRepoTestCase(TempDirTestCase):
    def make_repo(self, version="1.0.0-rc.1", manifest_version=None, changelog=None, git=False):
        self.write({
            "src/core/version.h": VERSION_H.format(v=version),
            "assets/base/package.lua": MANIFEST.format(v=manifest_version or version),
            "CHANGELOG.md": CHANGELOG.format(v=version) if changelog is None else changelog,
        })
        if git:
            env = {"GIT_CONFIG_GLOBAL": str(self.root / ".gitconfig-none"), "GIT_CONFIG_NOSYSTEM": "1",
                   "HOME": str(self.root), "PATH": os.environ.get("PATH", "")}
            for args in (["init", "-q"], ["add", "-A"],
                         ["-c", "user.name=T", "-c", "user.email=t@example.invalid", "-c", "commit.gpgsign=false",
                          "commit", "-q", "-m", "init"]):
                subprocess.run(["git", *args], cwd=self.root, check=True, env=env, capture_output=True)

    def main(self, *args, env=None, runner=None):
        output = io.StringIO()
        with contextlib.redirect_stdout(output), contextlib.redirect_stderr(output):
            code = release.main(["--root", str(self.root), *[str(a) for a in args]], env=env or {},
                                runner=runner or FakeRunner())
        return code, output.getvalue()


class VersionCheckTest(ReleaseRepoTestCase):
    def test_repository_version_is_consistent(self):
        # spec: release-pipeline: One place to change the version
        version = release.read_version(REPO_ROOT)
        self.assertRegex(version, release.SEMVER_RE)
        self.assertEqual(release.manifest_version(REPO_ROOT), version)
        self.assertTrue(release.changelog_has_entry((REPO_ROOT / "CHANGELOG.md").read_text(encoding="utf-8"),
                                                    version))
        # CMake reads ZB_VERSION from the header instead of repeating it.
        cmake = (REPO_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("src/core/version.h", cmake)
        self.assertNotIn(version, cmake)
        self.assertNotIn(version.split("-")[0], cmake)
        code, out = run_main(release.main, "version")
        self.assertEqual((code, out.strip()), (0, version))

    def test_consistent_release_passes(self):
        # spec: release-pipeline: A consistent release passes the check
        self.make_repo(git=True)
        code, out = self.main("check")
        self.assertEqual(code, 0, out)
        self.assertIn("1.0.0-rc.1 is ready to release", out)

    def test_version_mismatch_is_refused(self):
        # spec: release-pipeline: Version mismatch is refused
        self.make_repo(version="1.0.0-rc.1", manifest_version="0.1.0")
        code, out = self.main("check", "--allow-dirty")
        self.assertEqual(code, 1)
        self.assertIn("assets/base/package.lua: version '0.1.0' does not match ZB_VERSION '1.0.0-rc.1'", out)

    def test_missing_changelog_entry_is_refused(self):
        # spec: release-pipeline: Missing changelog entry is refused
        self.make_repo(changelog="# Changelog\n\n## [0.9.0] - 2026-01-01\n\n- Older. 1.0.0-rc.1 is mentioned.\n")
        code, out = self.main("check", "--allow-dirty")
        self.assertEqual(code, 1)
        self.assertIn("CHANGELOG.md has no entry for 1.0.0-rc.1", out)

    def test_dirty_tree_is_refused_unless_allowed(self):
        # spec: release-pipeline: A dirty tree is refused
        self.make_repo(git=True)
        self.write({"stray.txt": "x"})
        code, out = self.main("check")
        self.assertEqual(code, 1)
        self.assertIn("the git tree is not clean (1 change(s): stray.txt)", out)
        code, out = self.main("check", "--allow-dirty")
        self.assertEqual(code, 0, out)
        self.assertIn("clean-tree check skipped", out)

    def test_bad_version_strings(self):
        self.make_repo(version="1.0")
        problems = release.check_release(self.root, allow_dirty=True)
        self.assertTrue(any("not a semantic version" in p for p in problems), problems)
        for good in ("1.0.0", "1.0.0-rc.1", "2.10.3-beta.2+build.5"):
            self.assertRegex(good, release.SEMVER_RE)
        for bad in ("1.0", "01.0.0", "1.0.0-", "v1.0.0"):
            self.assertNotRegex(bad, release.SEMVER_RE)
        self.assertTrue(release.changelog_has_entry("## 1.0.0\n", "1.0.0"))
        self.assertFalse(release.changelog_has_entry("## [1.0.0-rc.10]\n", "1.0.0-rc.1"))

    def test_missing_version_define_is_an_error(self):
        self.write({"src/core/version.h": "#define OTHER 1\n"})
        code, out = self.main("version")
        self.assertEqual(code, 2)
        self.assertIn("no '#define ZB_VERSION", out)


class PackageTest(ReleaseRepoTestCase):
    def test_platform_tags_and_names(self):
        self.assertEqual(release.platform_tag("Linux", "x86_64"), "linux-x86_64")
        self.assertEqual(release.platform_tag("Windows", "AMD64"), "windows-x86_64")
        self.assertEqual(release.platform_tag("Darwin", "arm64"), "macos-arm64")
        self.assertEqual(release.platform_tag("Linux", "aarch64"), "linux-arm64")
        self.assertEqual(release.archive_extension("linux-x86_64"), ".tar.gz")
        self.assertEqual(release.archive_extension("windows-x86_64"), ".zip")
        self.assertEqual(release.archive_extension("macos-arm64"), ".zip")

    def test_dry_run_prints_the_plan(self):
        # spec: release-pipeline: Package plan without building
        self.make_repo()
        runner = FakeRunner()
        for tag, archive in (("linux-x86_64", "zauberer-1.0.0-rc.1-linux-x86_64.tar.gz"),
                             ("windows-x86_64", "zauberer-1.0.0-rc.1-windows-x86_64.zip"),
                             ("macos-arm64", "zauberer-1.0.0-rc.1-macos-arm64.zip")):
            code, out = self.main("package", "--dry-run", "--platform", tag, runner=runner)
            self.assertEqual(code, 0, out)
            self.assertIn(f"package {archive} (preset release, platform {tag}) - dry run", out)
            self.assertIn(f"would run: cmake --preset release -DZB_PACKAGE_PLATFORM={tag}", out)
            self.assertIn("would run: cmake --build --preset release", out)
            self.assertIn("would run: cpack --config", out)
            self.assertIn(f"build/dist/{archive}", out)
        self.assertEqual(runner.commands, [])
        self.assertFalse((self.root / "build").exists())

    def test_package_runs_cmake_and_cpack_and_verifies_the_archive(self):
        self.make_repo()
        tag = "linux-x86_64"
        top = "zauberer-1.0.0-rc.1-linux-x86_64"

        def cpack_writes_archive(argv):
            if argv[0] == "cpack":
                out = self.root / "build" / "release" / "packages"
                out.mkdir(parents=True, exist_ok=True)
                make_tar(out / f"{top}.tar.gz", top, LAYOUT, EXEC)

        runner = FakeRunner(on_run=cpack_writes_archive)
        code, out = self.main("package", "--platform", tag, "--configure-arg=-DX=1", runner=runner)
        self.assertEqual(code, 0, out)
        self.assertEqual([c[0] for c in runner.commands], ["cmake", "cmake", "cpack"])
        self.assertIn("-DX=1", runner.commands[0])
        self.assertTrue((self.root / "build" / "dist" / f"{top}.tar.gz").is_file())

    def test_failed_build_stops_the_package(self):
        self.make_repo()
        code, out = self.main("package", "--platform", "linux-x86_64", runner=FakeRunner(fail=("cpack",)))
        self.assertEqual(code, 2)
        self.assertIn("failed with exit code 1", out)
        code, out = self.main("package", "--platform", "linux-x86_64", runner=FakeRunner())
        self.assertEqual(code, 2)
        self.assertIn("CPack did not write", out)

    def test_package_layout(self):
        # spec: release-pipeline: Package has the shipped layout
        top = "zauberer-1.0.0-rc.1-linux-x86_64"
        good = make_tar(self.root / f"{top}.tar.gz", top, LAYOUT, EXEC)
        self.assertEqual(release.verify_package(good, "linux-x86_64"), [])
        # The CMake install rules ship the same entries.
        cmake = (REPO_ROOT / "cmake" / "ZbPackaging.cmake").read_text(encoding="utf-8")
        for needle in ("install(TARGETS zauberer RUNTIME DESTINATION .)", "zauberer-relay", "DESTINATION assets",
                       "DESTINATION mods", "DESTINATION LICENSES", "CHANGELOG.md", "THIRD_PARTY_NOTICES.md",
                       "CREDITS.md", 'CPACK_PACKAGE_FILE_NAME "zauberer-${ZB_VERSION_FULL}-${ZB_PACKAGE_PLATFORM}"'):
            self.assertIn(needle, cmake)
        # Windows: a zip with .exe files.
        wtop = "zauberer-1.0.0-rc.1-windows-x86_64"
        wzip = self.root / f"{wtop}.zip"
        with zipfile.ZipFile(wzip, "w") as archive:
            for name, data in LAYOUT.items():
                name = {"zauberer": "zauberer.exe", "zauberer-relay": "zauberer-relay.exe"}.get(name, name)
                archive.writestr(f"{wtop}/{name}", data)
        self.assertEqual(release.verify_package(wzip, "windows-x86_64"), [])

    def test_package_without_the_base_game_is_refused(self):
        # spec: release-pipeline: Incomplete package is refused
        top = "zauberer-1.0.0-rc.1-linux-x86_64"
        files = {k: v for k, v in LAYOUT.items() if not k.startswith(("assets/", "CREDITS"))}
        bad = make_tar(self.root / f"{top}.tar.gz", top, files, {"zauberer": 0o644, "zauberer-relay": 0o755})
        problems = release.verify_package(bad, "linux-x86_64")
        self.assertIn(f"{top}.tar.gz: missing assets/base/package.lua", problems)
        self.assertIn(f"{top}.tar.gz: missing CREDITS.md", problems)
        self.assertIn(f"{top}.tar.gz: zauberer is not executable", problems)
        loose = make_tar(self.root / "loose.tar.gz", "other", LAYOUT, EXEC)
        self.assertIn("expected one top-level folder 'loose'", release.verify_package(loose, "linux-x86_64")[0])
        (self.root / "junk.zip").write_bytes(b"not a zip")
        self.assertIn("cannot read the archive", release.verify_package(self.root / "junk.zip", "macos-arm64")[0])


class WebPackageTest(ReleaseRepoTestCase):
    def make_web(self, name, engine):
        self.write({f"{name}/index.html": "<html>", f"{name}/zauberer-loader.js": "loader",
                    f"{name}/{engine}.js": f"{engine} js", f"{name}/{engine}.wasm": f"{engine} wasm",
                    f"{name}/assets/base/package.lua": "return {}"})
        return self.root / name

    def test_web_archive_from_both_builds(self):
        # spec: release-pipeline: Web archive from the web builds
        self.make_repo()
        self.make_web("build/web-st/bin", "zauberer-st")
        self.make_web("build/web/bin", "zauberer")
        code, out = self.main("web")
        self.assertEqual(code, 0, out)
        archive = self.root / "build" / "dist" / "zauberer-1.0.0-rc.1-web.zip"
        with zipfile.ZipFile(archive) as zf:
            names = sorted(zf.namelist())
        top = "zauberer-1.0.0-rc.1-web/"
        self.assertEqual(names, [top + n for n in ("assets/base/package.lua", "index.html", "zauberer-loader.js",
                                                   "zauberer-st.js", "zauberer-st.wasm", "zauberer.js",
                                                   "zauberer.wasm")])
        first = archive.read_bytes()
        self.main("web")
        self.assertEqual(archive.read_bytes(), first, "the web zip is reproducible")
        # Only one build: that one.
        code, out = self.main("web", "--web-st", "build/web-st/bin", "--dist", "out")
        self.assertEqual(code, 0, out)
        with zipfile.ZipFile(self.root / "out" / "zauberer-1.0.0-rc.1-web.zip") as zf:
            self.assertNotIn(top + "zauberer.wasm", zf.namelist())
        code, out = self.main("web", "--web", "build/web/bin", "--dist", "out2", "--dry-run")
        self.assertEqual(code, 0, out)
        self.assertIn("would assemble", out)
        self.assertFalse((self.root / "out2").exists())

    def test_missing_web_build_is_refused(self):
        # spec: release-pipeline: Missing web build is refused
        self.make_repo()
        code, out = self.main("web")
        self.assertEqual(code, 2)
        self.assertIn("no web build found", out)
        self.write({"empty/readme.txt": "x"})
        code, out = self.main("web", "--web-st", "empty")
        self.assertEqual(code, 2)
        self.assertIn("is not a web build (no index.html)", out)


class SumsAndSigningTest(ReleaseRepoTestCase):
    def test_checksums_for_every_artefact(self):
        # spec: release-pipeline: Checksums for every artefact
        self.make_repo()
        files = {"build/dist/zauberer-1.0.0-rc.1-linux-x86_64.tar.gz": b"linux",
                 "build/dist/zauberer-1.0.0-rc.1-web.zip": b"web",
                 "build/dist/zauberer-1.0.0-rc.1-windows-x86_64.zip": b"windows",
                 "build/dist/old.zip.asc": b"signature", "build/dist/zauberer-1.0.0-rc.1-web/index.html": b"x"}
        self.write(files)
        code, out = self.main("sums")
        self.assertEqual(code, 0, out)
        self.assertIn("GPG signing skipped: ZB_SIGN_GPG_KEY is not set", out)
        lines = (self.root / "build" / "dist" / "SHA256SUMS").read_text(encoding="utf-8").splitlines()
        expected = [f"{hashlib.sha256(data).hexdigest()}  {Path(name).name}" for name, data in sorted(files.items())
                    if name.count("/") == 2 and not name.endswith(".asc")]
        self.assertEqual(lines, expected)
        self.assertEqual(len(lines), 3)
        # Running it again does not checksum the checksum file.
        self.main("sums")
        self.assertEqual(len((self.root / "build" / "dist" / "SHA256SUMS").read_text().splitlines()), 3)
        code, out = self.main("sums", "--dist", "nowhere")
        self.assertEqual(code, 2)
        self.assertIn("no artefacts", out)

    def test_signing_skipped_without_keys(self):
        # spec: release-pipeline: Signing is skipped without keys
        self.write({"stage/pkg/zauberer.exe": "x", "stage/pkg/zauberer-relay.exe": "x",
                    "stage/pkg/zauberer": "x", "stage/pkg/zauberer-relay": "x", "dist/a.zip": "a"})
        runner = FakeRunner()
        for os_name, line in (("windows", "Windows signing skipped: ZB_SIGN_WINDOWS_CERT is not set"),
                              ("macos", "macOS signing skipped: ZB_SIGN_MACOS_IDENTITY is not set"),
                              ("linux", "executable signing not applicable on linux")):
            code, out = self.main("sign-binaries", self.root / "stage", "--os", os_name, runner=runner)
            self.assertEqual(code, 0, out)
            self.assertIn(line, out)
        code, out = self.main("sums", "--dist", "dist", runner=runner)
        self.assertEqual(code, 0, out)
        self.assertIn("GPG signing skipped", out)
        self.assertEqual(runner.commands, [])

    def test_signing_with_configured_keys(self):
        # spec: release-pipeline: Signing with configured keys
        self.write({"stage/pkg/zauberer.exe": "x", "stage/pkg/zauberer-relay.exe": "x",
                    "stage/pkg/zauberer": "x", "stage/pkg/zauberer-relay": "x", "dist/a.zip": "a"})
        stage = self.root / "stage" / "pkg"
        env = {"ZB_SIGN_WINDOWS_CERT": "cert.pfx", "ZB_SIGN_WINDOWS_PASSWORD": "secret",
               "ZB_SIGN_MACOS_IDENTITY": "Developer ID Application: X", "ZB_SIGN_GPG_KEY": "ABCD1234"}
        runner = FakeRunner()
        code, out = self.main("sign-binaries", self.root / "stage", "--os", "windows", env=env, runner=runner)
        self.assertEqual(code, 0, out)
        self.assertEqual(runner.commands[0][:6], ["signtool", "sign", "/fd", "SHA256", "/f", "cert.pfx"])
        self.assertEqual(runner.commands[0][-2:], [str(stage / "zauberer-relay.exe"), str(stage / "zauberer.exe")])
        self.assertNotIn("secret", out)
        runner = FakeRunner()
        code, out = self.main("sign-binaries", self.root / "stage", "--os", "macos", env=env, runner=runner)
        self.assertEqual(code, 0, out)
        self.assertEqual(runner.commands[0], ["codesign", "--force", "--timestamp", "--options", "runtime", "--sign",
                                              "Developer ID Application: X", str(stage / "zauberer")])
        self.assertEqual(runner.commands[1], ["codesign", "--verify", "--strict", str(stage / "zauberer")])
        self.assertEqual(len(runner.commands), 4)
        runner = FakeRunner()
        code, out = self.main("sums", "--dist", "dist", env=env, runner=runner)
        self.assertEqual(code, 0, out)
        sums = self.root / "dist" / "SHA256SUMS"
        self.assertEqual(runner.commands, [["gpg", "--batch", "--yes", "--local-user", "ABCD1234", "--armor",
                                            "--detach-sign", "--output", f"{sums}.asc", str(sums)]])
        # A failing signing tool fails the step.
        code, out = self.main("sign-binaries", self.root / "stage", "--os", "macos", env=env,
                              runner=FakeRunner(fail=("codesign",)))
        self.assertEqual(code, 2)
        self.assertIn("signing failed: codesign", out)
        code, out = self.main("sums", "--dist", "dist", env=env, runner=FakeRunner(fail=("gpg",)))
        self.assertEqual(code, 2)
        code, out = self.main("sign-binaries", self.root / "dist", "--os", "windows", env=env)
        self.assertEqual(code, 2)
        self.assertIn("no executables to sign", out)


class CreditsTest(ReleaseRepoTestCase):
    def test_credits_list_packages_and_third_party_code(self):
        self.make_repo()
        self.write({
            "assets/base/credits.lua": 'return { { file = "a.png", author = "Ann", source = "gen.py", '
                                       'license = "CC-BY-4.0" } }\n',
            "assets/base/data/third_party.lua": 'return { { name = "Lua", version = "5.4", license = "MIT", '
                                                'source = "https://www.lua.org" } }\n',
            "mods/demo/package.lua": 'return { id = "demo", name = "Demo | mod", version = "1.0.0" }\n',
        })
        out = self.root / "out" / "CREDITS.md"
        code, text = self.main("credits", "--out", out)
        self.assertEqual(code, 0, text)
        credits = out.read_text(encoding="utf-8")
        self.assertIn("| Lua | 5.4 | MIT | https://www.lua.org |", credits)
        self.assertIn("## Base (`assets/base`)", credits)
        self.assertIn("| `a.png` | Ann | CC-BY-4.0 | gen.py |", credits)
        self.assertIn("## Demo \\| mod (`mods/demo`)", credits)
        self.assertIn("No asset files.", credits)


@unittest.skipIf(yaml is None, "PyYAML is not installed")
class ReleaseWorkflowTest(unittest.TestCase):
    def test_release_workflow_runs_only_by_hand(self):
        # spec: release-pipeline: Release workflow runs only by hand
        text = (REPO_ROOT / ".github" / "workflows" / "release.yml").read_text(encoding="utf-8")
        workflow = yaml.safe_load(text)
        triggers = workflow.get("on", workflow.get(True))
        self.assertEqual(list(triggers), ["workflow_dispatch"])
        self.assertNotIn("secrets.", text)
        self.assertEqual(workflow["permissions"], {"contents": "read"})
        jobs = workflow["jobs"]
        self.assertEqual(set(jobs), {"linux", "windows", "web", "checksums"})
        for name in ("linux", "windows", "web"):
            steps = jobs[name]["steps"]
            script = " ".join(s.get("run", "") for s in steps)
            self.assertIn("tools/release.py", script)
            self.assertTrue(any(s.get("uses", "").startswith("actions/upload-artifact") for s in steps), name)
        self.assertIn("release.py sums", " ".join(s.get("run", "") for s in jobs["checksums"]["steps"]))
        # Every workflow of the repository starts by hand (update-ci-on-demand).
        for path in sorted((REPO_ROOT / ".github" / "workflows").glob("*.yml")):
            data = yaml.safe_load(path.read_text(encoding="utf-8"))
            self.assertEqual(list(data.get("on", data.get(True))), ["workflow_dispatch"], path.name)


if __name__ == "__main__":
    unittest.main()
