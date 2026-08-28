"""Deterministic non-binary Harbor reward aggregation."""
from __future__ import annotations


def aggregate(verdicts: list[dict]) -> dict:
    total = len(verdicts)
    passed = sum(bool(item.get("passed")) for item in verdicts)
    return {
        "reward": (passed / total) if total else 0.0,
        "passed": passed,
        "checks": total,
        "status": "passed" if total and passed == total else "failed",
    }
