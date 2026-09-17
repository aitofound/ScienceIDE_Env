#!/usr/bin/env python3
"""sab: the ScienceAccelBench packaging CLI, run from the pinned pipeline.

The pipeline (the `sciaccel_pipeline` package, the skill, the SPEC, the
pitfalls reference and the templates) lives in
https://github.com/huangzesen/sciaccelbench-pipeline; this repository carries
no copy of it. This loader finds a checkout of it, in order:

  $SAB_PIPELINE                          a clone, any path
  <this repo>/.pipeline                  what CI checks out
  <this repo>/../sciaccelbench-pipeline  a sibling clone
  an installed `sciaccel_pipeline`       pip install <clone>

and runs its CLI with ROOT anchored to this repository (SAB_ROOT still wins;
SAB_PIPE_DIR keeps its meaning; templates come from the package).

PIPELINE_REVISION next to SKILL.md names the commit `main` runs. The loader
warns on stderr when the clone it found is on another commit and does not
refuse: the review page prints which CLI produced a brief.
"""
import os
import subprocess
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parents[1]
ROOT = SKILL.parents[1]
REPO_URL = "https://github.com/huangzesen/sciaccelbench-pipeline"


def pinned() -> str | None:
    try:
        return (SKILL / "PIPELINE_REVISION").read_text(encoding="utf-8").split()[0]
    except (OSError, IndexError):
        return None


def find_clone() -> Path | None:
    env = os.environ.get("SAB_PIPELINE")
    candidates = ([Path(env).expanduser()] if env else []) + [ROOT / ".pipeline", ROOT.parent / "sciaccelbench-pipeline"]
    for c in candidates:
        if (c / "src" / "sciaccel_pipeline" / "__init__.py").is_file():
            return c.resolve()
    return None


clone = find_clone()
if clone is not None:
    sys.path.insert(0, str(clone / "src"))
    pin = pinned()
    if pin:
        try:
            head = subprocess.run(["git", "-C", str(clone), "rev-parse", "HEAD"],
                                  capture_output=True, text=True, timeout=5).stdout.strip()
        except (OSError, subprocess.SubprocessError):
            head = ""
        if head and head != pin:
            print(f"sab: {clone} is at {head[:12]}; main pins {pin[:12]} "
                  f"(git -C {clone} checkout {pin[:12]})", file=sys.stderr)

try:
    from sciaccel_pipeline import config
except ImportError:
    sys.exit(f"sab: the pipeline is not installed. Clone {REPO_URL} next to this repository "
             f"(or set SAB_PIPELINE to a clone, or pip install one) at the commit in {SKILL / 'PIPELINE_REVISION'}.")

config.configure(root=ROOT)

from sciaccel_pipeline.cli import main  # noqa: E402

if __name__ == "__main__":
    main()
