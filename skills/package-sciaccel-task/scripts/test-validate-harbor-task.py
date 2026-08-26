#!/usr/bin/env python3
"""Focused tests for validate-harbor-task.py."""

from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path

SCRIPT = Path(__file__).with_name("validate-harbor-task.py")
SPEC = importlib.util.spec_from_file_location("validate_harbor_task", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot import {SCRIPT}")
MOD = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MOD
SPEC.loader.exec_module(MOD)

BASE_DIRS = {"environment", "tests", "solution", "target"}
BASE_FILES = {
    "task.toml",
    "instruction.md",
    "environment/Dockerfile",
    "tests/Dockerfile",
    "tests/test.sh",
    "solution/solve.sh",
    "target/b200.json",
}


class LayoutTests(unittest.TestCase):
    def problems(self, files=BASE_FILES, dirs=BASE_DIRS, specials=(), invalid_json=()):
        return MOD.validate_entries(set(files), set(dirs), set(specials), set(invalid_json))

    def assert_problem(self, problems, code, path):
        if not any(problem.code == code and problem.path == path for problem in problems):
            self.fail(
                f"expected [{code}] {path}, got {[problem.render() for problem in problems]}"
            )

    def test_minimal_task_is_valid(self):
        self.assertEqual(self.problems(), [])

    def test_comment_is_optional_and_opaque(self):
        files = set(BASE_FILES) | {
            "comment/pass-policy-rationale.md",
            "comment/scientist-notes/round-1.txt",
        }
        dirs = set(BASE_DIRS) | {"comment", "comment/scientist-notes"}
        self.assertEqual(self.problems(files, dirs), [])

    def test_harbor_subtrees_are_opaque(self):
        files = set(BASE_FILES) | {
            "environment/build/context.tar",
            "tests/grader.py",
            "tests/cases/aw/input.bin",
            "solution/lib/reference.py",
        }
        dirs = set(BASE_DIRS) | {
            "environment/build",
            "tests/cases",
            "tests/cases/aw",
            "solution/lib",
        }
        specials = {"tests/model-current"}
        self.assertEqual(self.problems(files, dirs, specials), [])

    def test_unexpected_root_file_fails(self):
        got = self.problems(set(BASE_FILES) | {"README.md"})
        self.assert_problem(got, "unexpected-file", "README.md")

    def test_unexpected_root_directory_fails(self):
        got = self.problems(dirs=set(BASE_DIRS) | {"checks"})
        self.assert_problem(got, "unexpected-dir", "checks")

    def test_missing_required_entry_fails(self):
        got = self.problems(set(BASE_FILES) - {"tests/test.sh"})
        self.assert_problem(got, "missing", "tests/test.sh")

    def test_required_entry_wrong_type_fails(self):
        got = self.problems(
            files=set(BASE_FILES) - {"solution/solve.sh"},
            dirs=set(BASE_DIRS) | {"solution/solve.sh"},
        )
        self.assert_problem(got, "wrong-type", "solution/solve.sh")

    def test_comment_must_be_root_directory(self):
        got = self.problems(files=set(BASE_FILES) | {"comment"})
        self.assert_problem(got, "wrong-type", "comment")

    def test_target_is_flat(self):
        got = self.problems(dirs=set(BASE_DIRS) | {"target/b200"})
        self.assert_problem(got, "target-not-flat", "target/b200")

    def test_target_is_json_only(self):
        got = self.problems(files=set(BASE_FILES) | {"target/README.md"})
        self.assert_problem(got, "unexpected-file", "target/README.md")

    def test_target_must_not_be_empty(self):
        got = self.problems(files=set(BASE_FILES) - {"target/b200.json"})
        self.assert_problem(got, "empty-target", "target")

    def test_at_least_one_target_is_active(self):
        files = (set(BASE_FILES) - {"target/b200.json"}) | {"target/_b200.json"}
        got = self.problems(files=files)
        self.assert_problem(got, "no-active-target", "target")

    def test_invalid_target_json_fails(self):
        got = self.problems(invalid_json={"target/b200.json"})
        self.assert_problem(got, "invalid-json", "target/b200.json")

    def test_outer_symlink_fails(self):
        got = self.problems(specials={"target/alias.json"})
        self.assert_problem(got, "unsupported-type", "target/alias.json")

    def test_legacy_package_has_no_harbor_markers(self):
        class FakePath:
            def is_dir(self):
                return True

            def __truediv__(self, name):
                class Child:
                    def exists(self):
                        return False

                return Child()

        self.assertFalse(MOD.is_harbor_module_task(FakePath()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
