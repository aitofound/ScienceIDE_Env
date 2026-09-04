#!/usr/bin/env python3
"""sab: the ScienceAccelBench packaging CLI (thin offline wrapper).

GENERATED FILE - do not edit by hand. The canonical source is
https://github.com/aitofound/sciaccelbench-pipeline (see vendor-manifest.json
next to SKILL.md, and scripts/vendor_sync.py to verify or regenerate).

The wrapper anchors the vendored sciaccel_pipeline package to this repository
(the historical defaults of the monolithic sab.py): ROOT is the repository
that contains this skill, TEMPLATES is the skill's templates/ directory.
SAB_ROOT and SAB_PIPE_DIR still override, exactly as before.
"""
import sys
from pathlib import Path

_SKILL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(_SKILL / "scripts" / "_vendor"))

from sciaccel_pipeline import config  # noqa: E402

config.configure(root=_SKILL.parents[1], templates=_SKILL / "templates")

from sciaccel_pipeline.cli import main  # noqa: E402

if __name__ == "__main__":
    main()
