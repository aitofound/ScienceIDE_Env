#!/usr/bin/env python3
"""validate-harbor-task.py 批量发现的三条边界(PR319 validate CI 红的根因与修法):

  1. 只有 code/ 的「拆分前草稿」目录不进 --all 批量发现(不再让每个无关 PR 的 CI 红);
  2. 显式验证同一个草稿目录仍然如实 FAIL(缺 task.toml 等);
  3. 真正不完整的 leaf(有 task.toml 却缺必需文件)在 --all 下仍然 FAIL —— 发现没有被削弱。

    python3 tests/test_validate_discovery.py      # 或 pytest
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path

VALIDATOR = Path(__file__).resolve().parents[1] / "scripts" / "validate-harbor-task.py"


def run(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, str(VALIDATOR), *args], capture_output=True, text=True)


def draft(tasks: Path, name: str) -> Path:
    d = tasks / name / "code" / "cb"
    d.mkdir(parents=True)
    (d / "main.c").write_text("int main(void){return 0;}\n")
    return tasks / name


def test_code_only_draft_is_excluded_from_bulk_discovery():
    with tempfile.TemporaryDirectory() as s:
        tasks = Path(s) / "tasks"; draft(tasks, "pluto-draft")
        r = run("--all", str(tasks))
        assert r.returncode == 0 and "pluto-draft" not in r.stdout and "0 Harbor leaves" in r.stdout, r.stdout


def test_explicit_validation_of_draft_still_fails_incomplete():
    with tempfile.TemporaryDirectory() as s:
        d = draft(Path(s) / "tasks", "pluto-draft")
        r = run(str(d))
        assert r.returncode == 1 and "FAIL" in r.stdout and "[missing] task.toml" in r.stdout, r.stdout


def test_real_incomplete_leaf_still_fails_under_bulk_discovery():
    with tempfile.TemporaryDirectory() as s:
        tasks = Path(s) / "tasks"
        leaf = draft(tasks, "half-leaf"); (leaf / "task.toml").write_text('[metadata.sciaccel]\nslug = "half-leaf"\n')
        grouped = draft(tasks / "grp", "half-grouped"); (grouped / "environment").mkdir()    # 一级分组下的半成品
        r = run("--all", str(tasks))
        assert r.returncode == 1 and "FAIL" in r.stdout, r.stdout
        assert "half-leaf" in r.stdout and "half-grouped" in r.stdout and "[missing] tests/test.sh" in r.stdout, r.stdout


if __name__ == "__main__":
    bad = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_"):
            try:
                fn(); print(f"  ✓ {name}")
            except Exception as e:                # noqa: BLE001
                bad += 1; print(f"  ✗ {name}: {type(e).__name__}: {e}")
    sys.exit(1 if bad else 0)
