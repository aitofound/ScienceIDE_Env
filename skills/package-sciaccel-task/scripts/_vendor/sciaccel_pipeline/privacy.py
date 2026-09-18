"""Privacy and path-containment helpers for the codebase metadata report.

Everything that decides what may leave the local machine in a report lives
here: secret-bearing key and value detection, local/absolute path redaction,
raw-log dropping, and the safe-relative-path rules used for containment.
"""
from __future__ import annotations

import re
from pathlib import Path

_METADATA_PROHIBITED_KEYS = {"tolerance", "tolerances", "reward", "rewards", "speedup", "speedups",
                             "benchmark_result", "benchmark_results", "merge_readiness", "merge-ready",
                             "suitability", "pass_policy", "pass_policies"}
_METADATA_DANGEROUS_KEYS = ("password", "passwd", "secret", "token", "api_key", "apikey", "authorization",
                            "credential", "private_key")
_METADATA_DROP_KEYS = {"stdout", "stderr", "raw_log", "raw_logs", "traceback", "raw_trace", "raw_traces"}
_METADATA_ABS_PATH = re.compile(
    r"\bfile:(?://)?/|"
    r"(?<![A-Za-z0-9_/])/(?!/)|"
    r"(?<![A-Za-z0-9_])[A-Za-z]:[\\/]|"
    r"(?<![A-Za-z0-9_])~[A-Za-z0-9._-]*[\\/]|"
    r"(?<![A-Za-z0-9_])(?:\$(?:HOME|USERPROFILE)|\$\{(?:HOME|USERPROFILE)\}|%(?:HOME|USERPROFILE)%)[\\/]",
    re.IGNORECASE,
)
_METADATA_SECRET_VALUE = re.compile(
    r"(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|sk-[A-Za-z0-9]{20,}|"
    r"xox[baprs]-[A-Za-z0-9-]{20,}|glpat-[A-Za-z0-9_-]{20,}|AKIA[0-9A-Z]{16}|"
    r"AIza[A-Za-z0-9_-]{20,}|Bearer\s+[A-Za-z0-9._~+/=-]{20,}|"
    r"-----BEGIN [^-\n]{0,32}PRIVATE KEY-----)", re.IGNORECASE)


def _metadata_key(key) -> str:
    return str(key).strip().lower().replace(" ", "_").replace("-", "_")


def _metadata_is_relative(path: str) -> bool:
    if not isinstance(path, str) or not path.strip() or "\\" in path:
        return False
    p = Path(path)
    return not p.is_absolute() and ".." not in p.parts


def _metadata_rel(root: Path, path: str) -> Path | None:
    if not _metadata_is_relative(path):
        return None
    candidate = (root / path).resolve(strict=False)
    try:
        candidate.relative_to(root.resolve())
    except ValueError:
        return None
    return candidate


def _metadata_scan_unsafe(value, where: str = "metadata") -> list[str]:
    errors: list[str] = []
    if isinstance(value, dict):
        for key, child in value.items():
            low = _metadata_key(key)
            if any(word in low for word in _METADATA_DANGEROUS_KEYS):
                errors.append(f"{where}.{key}: secret-bearing fields are not allowed")
                continue
            errors.extend(_metadata_scan_unsafe(child, f"{where}.{key}"))
    elif isinstance(value, list):
        for i, child in enumerate(value):
            errors.extend(_metadata_scan_unsafe(child, f"{where}[{i}]"))
    elif isinstance(value, str):
        if _METADATA_ABS_PATH.search(value):
            errors.append(f"{where}: local/private absolute paths are not allowed")
        if _METADATA_SECRET_VALUE.search(value):
            errors.append(f"{where}: secret-like values are not allowed")
    return errors


def _metadata_copy_public(value, warnings: list[str], where: str = "metadata", depth: int = 0):
    """Copy review-safe JSON; explicit result/policy fields are omitted, ordinary scientific prose is retained."""
    if depth > 8:
        warnings.append(f"{where}: nesting deeper than eight levels was omitted")
        return None
    if isinstance(value, dict):
        out = {}
        for key, child in value.items():
            low = _metadata_key(key)
            if any(word in low for word in _METADATA_DANGEROUS_KEYS) or any(marker in low for marker in _METADATA_DROP_KEYS):
                warnings.append(f"{where}.{key}: secret/raw-log field omitted")
                continue
            if low in {_metadata_key(key) for key in _METADATA_PROHIBITED_KEYS}:
                warnings.append(f"{where}.{key}: task-result/policy field omitted from codebase metadata")
                continue
            copied = _metadata_copy_public(child, warnings, f"{where}.{key}", depth + 1)
            if copied is not None:
                out[str(key)] = copied
        return out
    if isinstance(value, list):
        return [_metadata_copy_public(child, warnings, f"{where}[{i}]", depth + 1) for i, child in enumerate(value)]
    if isinstance(value, str):
        if _METADATA_SECRET_VALUE.search(value):
            warnings.append(f"{where}: secret-like value redacted")
            return "<secret-like value redacted>"
        if _METADATA_ABS_PATH.search(value):
            warnings.append(f"{where}: local/private absolute path redacted")
            return "<local path redacted>"
        if len(value) > 20_000:
            warnings.append(f"{where}: text longer than 20,000 characters was truncated")
        return value[:20_000]
    if value is None or isinstance(value, (bool, int, float)):
        return value
    warnings.append(f"{where}: unsupported value type omitted")
    return None
