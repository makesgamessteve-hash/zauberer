# SPDX-License-Identifier: MIT
"""Publishes a built web directory to the gh-pages branch (docs/release.md).

    python3 tools/deploy_web.py build/dist/zauberer-<version>-web [--dry-run]

The directory is usually the folder `tools/release.py web` assembles (the
single-threaded build plus the threaded engine); any folder with an index.html
works. The tool

  1. fetches the branch (default gh-pages) from the remote (default origin),
     or starts it without history when the remote has none,
  2. checks it out in a temporary `git worktree` (detached: no local branch is
     created or moved), replaces its files with the build, and adds
     `.nojekyll` (GitHub Pages serves the files as they are) and `version.txt`
     (the release version and the source commit),
  3. commits with a message naming the version and the source commit, and
  4. pushes the commit to the branch, retrying up to 4 times with exponential
     backoff (2, 4, 8, 16 s) when the push fails.

--dry-run does steps 1 and 2 and prints what would change, without a commit or
a push. --no-push commits in the temporary worktree but does not push (for a
look at the result). The temporary worktree is always removed again.

Exit codes: 0 deployed (or nothing to change), 2 usage, I/O or git error.
Standard library only.
"""
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Callable

import release
from tool_common import EXIT_ERROR, EXIT_OK, REPO_ROOT, ToolError

DEFAULT_BRANCH = "gh-pages"
DEFAULT_REMOTE = "origin"
PUSH_RETRIES = 4
BACKOFF_SECONDS = 2.0
COMMITTER_FALLBACK = {"GIT_AUTHOR_NAME": "Zauberer release", "GIT_AUTHOR_EMAIL": "release@zauberer.invalid",
                      "GIT_COMMITTER_NAME": "Zauberer release", "GIT_COMMITTER_EMAIL": "release@zauberer.invalid"}


def log(message: str) -> None:
    print(f"deploy_web: {message}", flush=True)


def git(cwd: Path, *args: str, env: dict | None = None, check: bool = True) -> subprocess.CompletedProcess:
    try:
        result = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True, env=env)
    except OSError as error:
        raise ToolError(f"cannot run git: {error}") from error
    if check and result.returncode != 0:
        raise ToolError(f"git {' '.join(args)} failed: {(result.stderr or result.stdout).strip()}")
    return result


def push_with_retries(push: Callable[[], bool], retries: int = PUSH_RETRIES, backoff: float = BACKOFF_SECONDS,
                      sleep: Callable[[float], None] | None = None) -> int:
    """Calls push() until it succeeds: once, then up to `retries` more times,
    waiting backoff, 2*backoff, 4*backoff, ... seconds before each retry.
    Returns the number of attempts; raises ToolError when all fail."""
    sleep = sleep or time.sleep
    attempts = 0
    while True:
        attempts += 1
        if push():
            return attempts
        if attempts > retries:
            raise ToolError(f"push failed {attempts} times; giving up")
        wait = backoff * (2 ** (attempts - 1))
        log(f"push failed (attempt {attempts} of {retries + 1}); retrying in {wait:g} s")
        sleep(wait)


def _commit_env(repo: Path) -> dict:
    """The environment for git commit-tree: the user's identity, or a fallback
    when the repository has none configured."""
    env = dict(os.environ)
    name = git(repo, "config", "user.name", check=False).stdout.strip()
    email = git(repo, "config", "user.email", check=False).stdout.strip()
    if not name or not email:
        for key, value in COMMITTER_FALLBACK.items():
            env.setdefault(key, value)
    return env


def _replace_contents(worktree: Path, source: Path, version: str, commit: str) -> None:
    for entry in worktree.iterdir():
        if entry.name == ".git":
            continue
        if entry.is_dir() and not entry.is_symlink():
            shutil.rmtree(entry)
        else:
            entry.unlink()
    shutil.copytree(source, worktree, dirs_exist_ok=True)
    (worktree / ".nojekyll").write_text("", encoding="utf-8")
    (worktree / "version.txt").write_text(f"{version}\n{commit}\n", encoding="utf-8")


def deploy(source: Path, repo: Path, remote: str = DEFAULT_REMOTE, branch: str = DEFAULT_BRANCH,
           version: str | None = None, commit: str | None = None, dry_run: bool = False, push: bool = True,
           retries: int = PUSH_RETRIES, sleep: Callable[[float], None] | None = None) -> str | None:
    """Deploys source to remote/branch. Returns the new commit hash, or None for
    a dry run or when nothing changed."""
    if not (source / "index.html").is_file():
        raise ToolError(f"{source} is not a web build (no index.html)")
    version = version or release.read_version(repo)
    commit = commit or git(repo, "rev-parse", "HEAD").stdout.strip()
    short = commit[:12]

    remote_head = git(repo, "ls-remote", "--heads", remote, f"refs/heads/{branch}").stdout.split()
    parent = None
    if remote_head:
        git(repo, "fetch", "--quiet", remote, f"refs/heads/{branch}")
        parent = git(repo, "rev-parse", "FETCH_HEAD").stdout.strip()
        log(f"{remote}/{branch} is at {parent[:12]}")
    else:
        log(f"{remote} has no branch {branch}; it will be started")

    tmp = Path(tempfile.mkdtemp(prefix="zb-deploy-"))
    worktree = tmp / "site"
    try:
        git(repo, "worktree", "add", "--quiet", "--detach", str(worktree), parent or "HEAD")
        _replace_contents(worktree, source, version, commit)
        git(worktree, "add", "--all", ".")
        if parent is None:
            # A new branch has no history: only the build is in the tree.
            changes = [f"A  {name}" for name in git(worktree, "ls-files").stdout.splitlines()]
        else:
            changes = git(worktree, "status", "--porcelain").stdout.splitlines()
        if not changes:
            log(f"{remote}/{branch} already has this build; nothing to deploy")
            return None
        log(f"{len(changes)} change(s) for {remote}/{branch}:")
        for line in changes:
            print(f"  {line}")
        message = f"Deploy web build {version} (commit {short})"
        if dry_run:
            log(f"dry run: would commit '{message}' and push it to {remote}/{branch}")
            return None
        tree = git(worktree, "write-tree").stdout.strip()
        args = ["commit-tree", tree, "-m", message, "-m", f"Built from {commit}."]
        if parent:
            args[2:2] = ["-p", parent]
        new = git(worktree, *args, env=_commit_env(repo)).stdout.strip()
        log(f"committed {new[:12]}: {message}")
        if not push:
            log("--no-push: not pushed")
            return new

        def attempt() -> bool:
            result = git(repo, "push", "--quiet", remote, f"{new}:refs/heads/{branch}", check=False)
            if result.returncode != 0:
                log(f"push: {(result.stderr or result.stdout).strip()}")
            return result.returncode == 0

        attempts = push_with_retries(attempt, retries=retries, sleep=sleep)
        log(f"pushed {new[:12]} to {remote}/{branch} ({attempts} attempt(s))")
        return new
    finally:
        git(repo, "worktree", "remove", "--force", str(worktree), check=False)
        git(repo, "worktree", "prune", check=False)
        shutil.rmtree(tmp, ignore_errors=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Publish a built web directory to the gh-pages branch.")
    parser.add_argument("source", type=Path, help="the web build (a folder with index.html)")
    parser.add_argument("--repo", type=Path, default=REPO_ROOT, help="git repository (default: %(default)s)")
    parser.add_argument("--remote", default=DEFAULT_REMOTE, help="remote to publish to (default: %(default)s)")
    parser.add_argument("--branch", default=DEFAULT_BRANCH, help="branch to publish to (default: %(default)s)")
    parser.add_argument("--version", help="release version (default: ZB_VERSION of the repository)")
    parser.add_argument("--commit", help="source commit named in the message (default: HEAD)")
    parser.add_argument("--retries", type=int, default=PUSH_RETRIES, help="push retries (default: %(default)s)")
    parser.add_argument("--dry-run", action="store_true", help="print what would change; no commit, no push")
    parser.add_argument("--no-push", action="store_true", help="commit but do not push")
    try:
        args = parser.parse_args(argv)
    except SystemExit as exit_:
        return exit_.code if isinstance(exit_.code, int) else EXIT_ERROR
    try:
        deploy(args.source.resolve(), args.repo.resolve(), args.remote, args.branch, args.version, args.commit,
               dry_run=args.dry_run, push=not args.no_push, retries=args.retries)
    except (ToolError, OSError) as error:
        print(f"deploy_web: error: {error}", file=sys.stderr)
        return EXIT_ERROR
    return EXIT_OK


if __name__ == "__main__":
    sys.exit(main())
