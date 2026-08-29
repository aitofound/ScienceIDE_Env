#!/usr/bin/env python3
"""Run one exact-pin Athena++ MHD case and publish task-local artifacts."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from extract_mhd import extract  # noqa: E402

COMMIT = "823614c90b594472747a0ac2a699e4a454f300d2"


def fail(message: str, code: int = 2) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(code)


def positive_int(text: str, name: str) -> int:
    try:
        value = int(text)
    except ValueError:
        fail(f"{name} must be a positive integer")
    if value <= 0:
        fail(f"{name} must be a positive integer")
    return value


def run_case(args: argparse.Namespace) -> None:
    dimensions = [positive_int(value, "dimension") for value in args.dimensions.split(",")]
    if len(dimensions) != 3:
        fail("--dimensions requires nx1,nx2,nx3")
    meshblock = [positive_int(value, "meshblock dimension") for value in args.meshblock.split(",")]
    if len(meshblock) != 3:
        fail("--meshblock requires nx1,nx2,nx3")
    if any(n % b for n, b in zip(dimensions, meshblock)):
        fail("mesh dimensions must be divisible by meshblock dimensions")
    try:
        tlim = float(args.tlim)
        output_dt = float(args.output_dt)
        cap = float(args.cap)
    except ValueError:
        fail("tlim, output-dt, and cap must be finite numbers")
    if not all(math.isfinite(value) and value > 0.0 for value in (tlim, output_dt, cap)):
        fail("tlim, output-dt, and cap must be finite positive numbers")
    if args.eos not in {"adiabatic", "isothermal"}:
        fail("unsupported EOS")
    results = Path(args.results)
    if results.is_symlink():
        fail("artifact output directory must not be a symlink")
    if results.exists() and (not results.is_dir() or any(results.iterdir())):
        fail(f"artifact output directory must be new and empty: {results}")
    results.mkdir(parents=True, exist_ok=True)
    for name in ("mhd_state.json", "mhd_observables.json"):
        path = results / name
        if path.exists() or path.is_symlink():
            fail(f"artifact output path already exists: {path}")

    source = Path(args.source)
    binary = Path(args.binary)
    build_root: Path | None = None
    cache_dir = Path(args.build_cache) if args.build_cache else None
    build_key = f"{args.problem}-{args.eos}-{args.flux}"
    if not (binary.is_file() and os.access(binary, os.X_OK)) and cache_dir is not None:
        if cache_dir.is_symlink():
            fail("build cache must not be a symlink")
        cache_dir.mkdir(parents=True, exist_ok=True)
        cached = cache_dir / f"athena-{build_key}"
        if cached.is_file() and os.access(cached, os.X_OK):
            binary = cached
    if not (binary.is_file() and os.access(binary, os.X_OK)):
        if source.is_symlink() or not source.is_dir():
            fail("no runnable ATHENA_BINARY and no usable exact-pin ATHENA_SOURCE_DIR")
        build_root = Path(tempfile.mkdtemp(prefix="athena-newtonian-mhd-candidate-"))
        staged_source = build_root / "athena"
        shutil.copytree(source, staged_source, symlinks=False)
        configure = [
            sys.executable, "-B", "configure.py", "-b",
            f"--prob={args.problem}", "--coord=cartesian", f"--flux={args.flux}",
            "--cflag=-O2 -g0",
        ]
        if args.eos == "isothermal":
            configure.append("--eos=isothermal")
        subprocess.run(configure, cwd=staged_source, check=True)
        subprocess.run(
            ["make", "-j" + str(args.jobs), "EXE_DIR=" + str(build_root / "bin") + "/", "OBJ_DIR=" + str(build_root / "obj") + "/"],
            cwd=staged_source,
            check=True,
        )
        binary = build_root / "bin" / "athena"
        if cache_dir is not None:
            cached = cache_dir / f"athena-{build_key}"
            if not cached.exists():
                shutil.copy2(binary, cached)
                cached.chmod(0o755)
                binary = cached
    if not (binary.is_file() and os.access(binary, os.X_OK)):
        fail(f"Athena++ executable is not runnable: {binary}")

    run_dir = Path(tempfile.mkdtemp(prefix="athena-newtonian-mhd-run-"))
    overrides = [
        "output2/file_type=tab",
        "output2/variable=prim",
        f"output2/dt={args.output_dt:g}",
        f"time/tlim={args.tlim:g}",
        "time/ncycle_out=0",
        # Uniform-grid checks deliberately do not activate the deck's optional
        # static refinement regions; AMR is a framework witness, not this leaf's
        # core owner, and would change the effective row count.
        f"mesh/nx1={dimensions[0]}", f"mesh/nx2={dimensions[1]}", f"mesh/nx3={dimensions[2]}",
        f"meshblock/nx1={meshblock[0]}", f"meshblock/nx2={meshblock[1]}", f"meshblock/nx3={meshblock[2]}",
        "problem/compute_error=true",
    ]
    # The upstream CPAW uses this selector for the left-going wave in its
    # breadth family; keeping the default right-going wave makes this case
    # deterministic and still exercises isothermal MHD + CT.
    if args.problem == "cpaw" and not args.runtime_override:
        overrides.append("problem/dir=1")
    overrides.extend(args.runtime_override)
    command = [str(binary), "-i", str(args.config), *overrides]
    try:
        subprocess.run(command, cwd=run_dir, check=True, timeout=cap)
    except subprocess.TimeoutExpired:
        fail(f"Athena++ case {args.case} exceeded {cap:g}s operational cap", 124)
    config_sha = hashlib.sha256(args.config.read_bytes()).hexdigest()
    selector_text = ",".join(overrides)
    metadata = {
        "source_commit": COMMIT,
        "source_path": "ATHENA_SOURCE_DIR exact-pin vendored source",
        "case": args.case,
        "problem": args.problem,
        "eos": args.eos,
        "solver": args.flux,
        "config_sha256": config_sha,
        "runtime_overrides": selector_text,
        "compiled_path": (
            f"-b Newtonian MHD; source_commit={COMMIT}; solver={args.flux}; "
            f"eos={args.eos}; problem={args.problem}; config_sha256={config_sha}; "
            f"runtime_overrides={selector_text}; multi-step MeshBlock task integration"
        ),
    }
    extract(
        run_dir,
        results / "mhd_state.json",
        results / "mhd_observables.json",
        args.case,
        dimensions,
        args.boundary,
        args.gamma,
        metadata,
    )
    print(f"case={args.case} source_commit={COMMIT} results={results} run_dir={run_dir}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", required=True)
    parser.add_argument("--problem", required=True)
    parser.add_argument("--eos", required=True)
    parser.add_argument("--flux", required=True)
    parser.add_argument("--config", required=True, type=Path)
    parser.add_argument("--dimensions", required=True)
    parser.add_argument("--meshblock", required=True)
    parser.add_argument("--tlim", required=True, type=float)
    parser.add_argument("--output-dt", required=True, type=float)
    parser.add_argument("--boundary", required=True, choices=["periodic", "outflow"])
    parser.add_argument("--results", required=True, type=Path)
    parser.add_argument("--binary", default=os.environ.get("ATHENA_BINARY", "/opt/athena/bin/athena"), type=Path)
    parser.add_argument("--source", default=os.environ.get("ATHENA_SOURCE_DIR", "/opt/athena"), type=Path)
    parser.add_argument("--build-cache", default=os.environ.get("ATHENA_BUILD_CACHE_DIR"), type=Path)
    parser.add_argument("--jobs", default=int(os.environ.get("ATHENA_MAKE_JOBS", "2")), type=int)
    parser.add_argument("--cap", default=os.environ.get("ATHENA_OPERATIONAL_CAP_SECONDS", "120"))
    parser.add_argument("--gamma", default="1.666666666666667", type=float)
    parser.add_argument("--runtime-override", action="append", default=[], help="Athena key=value runtime selector; repeatable")
    args = parser.parse_args()
    if args.jobs <= 0:
        fail("--jobs must be positive")
    run_case(args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
