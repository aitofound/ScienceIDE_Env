#!/usr/bin/env python3
"""Build a task image after copying one shared source into a temporary context."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

SOURCE_RE = re.compile(r"[a-z0-9]+(?:-[a-z0-9]+)*\Z")


class StageError(ValueError):
    pass


def repo_and_source(task: Path, source: str) -> tuple[Path, Path]:
    if SOURCE_RE.fullmatch(source) is None:
        raise StageError("--source must be lower-kebab-case")
    for parent in (task, *task.parents):
        source_dir = parent / "code" / source
        helper = parent / "scripts" / "stage-task-source.py"
        if source_dir.is_dir() and helper.is_file():
            if source_dir.is_symlink():
                raise StageError(f"shared source must be a real directory: {source_dir}")
            return parent, source_dir
    raise StageError(f"cannot find repository code/{source} above task {task}")


def task_file(task: Path, value: str) -> Path:
    relative = Path(value)
    if relative.is_absolute() or ".." in relative.parts:
        raise StageError(f"Dockerfile path must stay inside task: {value!r}")
    path = task / relative
    if path.is_symlink() or not path.is_file():
        raise StageError(f"Dockerfile is not a regular task file: {value}")
    return relative


def build(task: Path, source: str, dockerfile: str, tag: str, docker_args: list[str]) -> int:
    task = task.resolve()
    if task.is_symlink() or not task.is_dir():
        raise StageError(f"task must be a real directory: {task}")
    repo_root, source_dir = repo_and_source(task, source)
    try:
        task.relative_to(repo_root / "tasks")
    except ValueError as exc:
        raise StageError(f"task must live under {repo_root / 'tasks'}") from exc
    relative_dockerfile = task_file(task, dockerfile)

    with tempfile.TemporaryDirectory(prefix=f"sciaccel-{task.name}-build-") as raw:
        context = Path(raw)
        shutil.copytree(
            task,
            context,
            symlinks=True,
            ignore=shutil.ignore_patterns("code"),
            dirs_exist_ok=True,
        )
        staged_source = context / "code" / source
        staged_source.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(source_dir, staged_source, symlinks=True)
        command = [
            "docker",
            "build",
            "--file",
            str(context / relative_dockerfile),
            "--tag",
            tag,
            *docker_args,
            str(context),
        ]
        print(f"STAGE code/{source} into temporary context", file=sys.stderr)
        print("BUILD", " ".join(command), file=sys.stderr)
        try:
            return subprocess.run(command, check=False).returncode
        except OSError as exc:
            print(f"stage-task-source.py: unable to execute docker: {exc}", file=sys.stderr)
            return 127


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--task", type=Path, required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--dockerfile", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("docker_args", nargs=argparse.REMAINDER)
    args = parser.parse_args(argv)
    docker_args = list(args.docker_args)
    if docker_args[:1] == ["--"]:
        docker_args.pop(0)
    try:
        return build(args.task, args.source, args.dockerfile, args.tag, docker_args)
    except StageError as exc:
        print(f"stage-task-source.py: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
