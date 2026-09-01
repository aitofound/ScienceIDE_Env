#!/usr/bin/env python3
"""Static authority for the one-script-per-check official Athena++ suite."""
from __future__ import annotations

import argparse
import ast
import hashlib
import json
import re
from pathlib import Path
from typing import Any

PIN = "823614c90b594472747a0ac2a699e4a454f300d2"
INVENTORY_SCHEMA = "athena-sr-mhd-inventory/v6"
SPEC_SCHEMA = "athena-official-check/v1"
REGISTRY_SCHEMA = "athena-upstream-official-scripts/v1"
EXPECTED_COUNT = 24
UNIVERSE_COUNT = 84
ALLOWED_CLASSES = {
    "core-sr-mhd": 5,
    "sr-hydro-support": 5,
    "newtonian-mhd-shared": 8,
    "generic-mhd-infrastructure": 3,
    "gr-mhd-minkowski-mirror": 3,
}
SLUG_RE = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
SHA_RE = re.compile(r"^[0-9a-f]{64}$")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for key, value in pairs:
        if key in out:
            raise ValueError(f"duplicate JSON key: {key}")
        out[key] = value
    return out


def strict_load(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_pairs)


def suite_root() -> Path:
    return Path(__file__).resolve().parent


def leaf_root() -> Path:
    return suite_root().parent


def load_inventory(root: Path | None = None) -> dict[str, Any]:
    tests = root or suite_root()
    value = strict_load(tests / "inventory.json")
    if not isinstance(value, dict):
        raise ValueError("inventory must be a JSON object")
    return value


def load_registry(root: Path | None = None) -> dict[str, Any]:
    tests = root or suite_root()
    value = strict_load(tests / "upstream-official-scripts.json")
    if not isinstance(value, dict):
        raise ValueError("source registry must be a JSON object")
    return value


def load_spec(slug: str, root: Path | None = None) -> dict[str, Any]:
    tests = root or suite_root()
    value = strict_load(tests / "checks" / slug / "spec.json")
    if not isinstance(value, dict):
        raise ValueError(f"{slug}: spec must be a JSON object")
    return value


def selected_specs(root: Path | None = None) -> list[dict[str, Any]]:
    tests = root or suite_root()
    inv = load_inventory(tests)
    return [load_spec(item["slug"], tests) for item in inv["checks"]]


def validate(tests: Path, source: Path | None) -> dict[str, Any]:
    problems: list[str] = []
    try:
        inv = load_inventory(tests)
        registry = load_registry(tests)
    except Exception as exc:
        return {"ok": False, "problems": [f"metadata load failed: {exc}"]}

    checks = inv.get("checks")
    if inv.get("schema") != INVENTORY_SCHEMA:
        problems.append("inventory schema mismatch")
    if inv.get("source_commit") != PIN:
        problems.append("inventory source pin mismatch")
    if not isinstance(checks, list) or len(checks) != EXPECTED_COUNT:
        problems.append(f"inventory must contain exactly {EXPECTED_COUNT} checks")
        checks = checks if isinstance(checks, list) else []
    if inv.get("selected_official_count") != EXPECTED_COUNT:
        problems.append("selected_official_count mismatch")
    if inv.get("official_script_universe") != UNIVERSE_COUNT:
        problems.append("official script universe count mismatch")
    reward = inv.get("reward", {})
    if reward.get("denominator") != EXPECTED_COUNT or reward.get("direct_check_count") != EXPECTED_COUNT:
        problems.append("reward denominator/count is not 24")
    if inv.get("selection_classes") != ALLOWED_CLASSES | {"radiation-chemistry-selected": 0}:
        problems.append("selection class accounting mismatch")

    if registry.get("schema") != REGISTRY_SCHEMA or registry.get("source_commit") != PIN:
        problems.append("official-script registry schema/source mismatch")
    scripts = registry.get("scripts")
    if not isinstance(scripts, dict) or len(scripts) != UNIVERSE_COUNT:
        problems.append(f"registry must bind exactly {UNIVERSE_COUNT} scripts")
        scripts = scripts if isinstance(scripts, dict) else {}
    if registry.get("script_count") != UNIVERSE_COUNT:
        problems.append("registry script_count mismatch")
    if registry.get("runner") != "tst/regression/run_tests.py" or not SHA_RE.fullmatch(str(registry.get("runner_sha256", ""))):
        problems.append("registry runner identity malformed")

    slugs: list[str] = []
    modules: list[str] = []
    selected_paths: list[str] = []
    class_counts = {name: 0 for name in ALLOWED_CLASSES}
    for index, item in enumerate(checks, 1):
        if not isinstance(item, dict):
            problems.append(f"check {index}: inventory entry is not an object")
            continue
        slug = item.get("slug")
        if not isinstance(slug, str) or not SLUG_RE.fullmatch(slug):
            problems.append(f"check {index}: malformed slug")
            continue
        slugs.append(slug)
        try:
            spec_path = tests / "checks" / slug / "spec.json"
            spec = load_spec(slug, tests)
            if sha256_file(spec_path) != item.get("spec_sha256"):
                problems.append(f"{slug}: inventory spec hash mismatch")
            if spec.get("schema") != SPEC_SCHEMA or spec.get("source_commit") != PIN:
                problems.append(f"{slug}: spec schema/source mismatch")
            for key in ("id", "slug", "title", "classification", "official_script",
                        "official_module", "script_sha256", "direct_invocation_count",
                        "workload", "acceptance", "caveats"):
                if item.get(key) != spec.get(key):
                    problems.append(f"{slug}: inventory/spec drift in {key}")
            if spec.get("runner") != registry.get("runner") or spec.get("runner_sha256") != registry.get("runner_sha256"):
                problems.append(f"{slug}: runner identity drift")
            if spec.get("direct_invocation_count") != 1:
                problems.append(f"{slug}: direct invocation count is not one")
            classification = spec.get("classification")
            if classification not in class_counts:
                problems.append(f"{slug}: unapproved classification {classification!r}")
            else:
                class_counts[classification] += 1
            official = spec.get("official_script")
            prefix = "tst/regression/scripts/tests/"
            if not isinstance(official, str) or not official.startswith(prefix):
                problems.append(f"{slug}: malformed official script path")
            else:
                relative = official[len(prefix):]
                selected_paths.append(relative)
                if registry.get("scripts", {}).get(relative) != spec.get("script_sha256"):
                    problems.append(f"{slug}: script hash is not registry-authoritative")
                expected_module = relative[:-3] if relative.endswith(".py") else ""
                if spec.get("official_module") != expected_module:
                    problems.append(f"{slug}: runner module does not match script")
                modules.append(str(spec.get("official_module")))
            labels = strict_load(tests / "checks" / slug / "check.json")
            expected_labels = ["official-upstream", classification]
            if slug == "mhd-convergence":
                expected_labels.append("acceleration")
            if labels != {"labels": expected_labels}:
                problems.append(f"{slug}: check labels drift")
        except Exception as exc:
            problems.append(f"{slug}: metadata validation failed: {exc}")

    if len(slugs) != len(set(slugs)):
        problems.append("duplicate check slug")
    if len(modules) != len(set(modules)) or len(selected_paths) != len(set(selected_paths)):
        problems.append("official scripts are not one-to-one with direct checks")
    if class_counts != ALLOWED_CLASSES:
        problems.append(f"classification totals drift: {class_counts}")
    active = sorted(path.name for path in (tests / "checks").iterdir() if path.is_dir())
    if active != sorted(slugs):
        problems.append("active check directories differ from inventory")

    if source is not None:
        source = source.resolve()
        test_root = source / "tst/regression/scripts/tests"
        actual_paths = sorted(
            [p.relative_to(test_root).as_posix() for p in test_root.glob("*.py") if p.name != "__init__.py"]
            + [p.relative_to(test_root).as_posix() for p in test_root.glob("*/*.py") if p.name != "__init__.py"]
        )
        if actual_paths != sorted(scripts):
            problems.append("pinned source does not contain exactly the registered 84 official scripts")
        for relative, digest in sorted(scripts.items()):
            path = test_root / relative
            if not path.is_file() or sha256_file(path) != digest:
                problems.append(f"upstream source identity mismatch: {relative}")
        runner = source / str(registry.get("runner"))
        if not runner.is_file() or sha256_file(runner) != registry.get("runner_sha256"):
            problems.append("upstream runner identity mismatch")
        for relative in selected_paths:
            path = test_root / relative
            if not path.is_file():
                continue
            try:
                tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
                funcs = {node.name for node in tree.body if isinstance(node, ast.FunctionDef)}
                if not {"prepare", "run", "analyze"}.issubset(funcs):
                    problems.append(f"selected script lacks native prepare/run/analyze: {relative}")
            except Exception as exc:
                problems.append(f"cannot parse selected script {relative}: {exc}")

    return {
        "schema": "athena-official-suite-validation/v1",
        "ok": not problems,
        "source_commit": PIN,
        "official_script_universe": len(scripts),
        "selected_official_count": len(checks),
        "one_to_one_count": len(set(selected_paths)),
        "selection_classes": class_counts,
        "source_checked": source is not None,
        "problems": problems,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tests", type=Path, default=suite_root())
    parser.add_argument("--source", type=Path)
    args = parser.parse_args()
    result = validate(args.tests.resolve(), args.source)
    print(json.dumps(result, sort_keys=True, indent=2))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
