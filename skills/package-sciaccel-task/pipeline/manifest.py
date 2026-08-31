#!/usr/bin/env python3
"""兼容 re-export —— human-approved task manifest 契约已并入 scripts/_shared.py。

老代码 `import manifest` 拿到的名字(MANIFEST_VERSION / SOURCE_TYPES / ManifestError /
manifest_path_for / validate / load / scope_fingerprint / official_source_by_id)在这里
原样可用;新代码请直接用 scripts/_shared.py。字段语义与校验规则见
references/two-cli-architecture.md §2,没有第二份定义。
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from _shared import (  # noqa: E402,F401
    MANIFEST_VERSION,
    SOURCE_TYPES,
    ManifestError,
    manifest_path_for,
    official_source_by_id,
    scope_fingerprint,
)
from _shared import manifest_load as load  # noqa: E402,F401
from _shared import manifest_validate as validate  # noqa: E402,F401
