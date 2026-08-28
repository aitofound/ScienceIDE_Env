"""Build deterministic, tiny PLUTO-shaped validator fixtures.

These trees exercise only the verifier predicate, not the scientific solver.
They contain no clocks, RNG, network access, or committed binary blobs.  The
caller must provide a fresh output directory; this builder never deletes or
cleans an existing path.

Fixture ``grid.out``/``dbl.out`` are written in PLUTO's real record shape
(see ``pluto_io.py``: ``#``-commented header, active X1..X<n> point(s)/ghosts
records, three active/degenerate body blocks, and literal ``little``/``big``
endian token) at a fixed synthetic ghost width, not a simplified stand-in --
the point is to exercise the same parser a genuine CPU oracle output will go
through.

Entropy synthesis is EOS-aware and intentionally duplicates (rather than
imports) generic_validator.py's formula, matching this codebase's existing
"independent verifier-side calculation" pattern (see taub.py's docstring):
fixture generation and validation are two separately-written, cross-checked
implementations of the same PLUTO source formula
(Src/RHD/rhd_energy_solve.c:193-201), not one shared code path.
"""
from __future__ import annotations

import json
import math
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)
from taub import reconstruct

FIXTURE_GHOSTS = 2  # synthetic only; not claimed to match any row's real stencil width


def _fresh(path: str) -> None:
    if os.path.exists(path):
        raise FileExistsError(f"fixture output already exists: {path}")
    os.makedirs(path)


def _write_grid_out(path: str, dims: tuple[int, int, int], geometry: str,
                    ghosts: int = FIXTURE_GHOSTS) -> None:
    """Real PLUTO grid.out shape (Src/set_grid.c:156-193): a '#'-commented
    header whose active 'X<n>: [...], <N> point(s), <G> ghosts' records carry
    the active point count, followed by three body blocks of active/degenerate
    'index xl xr' lines (ghost coordinates are not written)."""
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        stream.write("# ******************************************************\n")
        stream.write("# PLUTO 4.4-patch4 Grid File (synthetic fixture)\n")
        stream.write("# \n")
        dimensions = sum(1 for value in dims if value > 1) or 1
        stream.write(f"# DIMENSIONS: {dimensions}\n")
        stream.write(f"# GEOMETRY:   {geometry.upper()}\n")
        for axis, points in enumerate(dims[:dimensions], start=1):
            stream.write(f"# X{axis}: [ 0.000000,  1.000000], {points} point(s), {ghosts} ghosts\n")
        stream.write("# ******************************************************\n")
        # Src/set_grid.c writes one active block per axis, including one-point
        # blocks for degenerate axes, and does not write ghost coordinates.
        for axis, points in enumerate(dims, start=1):
            total = points if axis <= dimensions else 1
            stream.write(f"{total} \n")
            for i in range(total):
                stream.write(f" {i + 1}   {float(i):18.12e}    {float(i + 1):18.12e}\n")


def _write_malformed_grid_out(path: str, dims: tuple[int, int, int], geometry: str) -> None:
    """A well-formed '#' header (so header parsing succeeds) followed by an
    unparseable body line -- exercises the body-block validation itself,
    distinct from reject-missing-grid's simpler 'file absent' case."""
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        stream.write("# ******************************************************\n")
        stream.write("# PLUTO 4.4-patch4 Grid File (synthetic fixture)\n")
        stream.write(f"# GEOMETRY:   {geometry.upper()}\n")
        for axis, points in enumerate(dims, start=1):
            stream.write(f"# X{axis}: [ 0.000000,  1.000000], {points} point(s), {FIXTURE_GHOSTS} ghosts\n")
        stream.write("# ******************************************************\n")
        stream.write("not-a-point-count\n")


def _write_side_files(path: str) -> None:
    """Genuine PLUTO/harness side files (pluto_io.TOLERATED_SIDE_FILES) a
    real results directory may legitimately carry; must be tolerated."""
    with open(os.path.join(path, "pluto.ini"), "w", encoding="utf-8") as stream:
        stream.write("% harness-copied deck (tests/checks/<check>/run.sh); not part of the output contract\n")
    with open(os.path.join(path, "restart.out"), "wb") as stream:
        stream.write(b"\x00" * 8)
    with open(os.path.join(path, "pluto.0.log"), "w", encoding="utf-8") as stream:
        stream.write("PLUTO log (PARALLEL builds only; Src/output_log.c:342-360)\n")


def _write_tree(path: str, rubric: dict, states: list[list[list[float]]],
                dims: tuple[int, int, int], geometry: str, *,
                steps: list[int] | None = None, dts: list[float] | None = None,
                names: list[str] | None = None, extra_file: bool = False,
                drop_grid: bool = False, malformed_grid: bool = False,
                endian: str = "little", side_files: bool = False) -> None:
    _fresh(path)
    variables = list(names or rubric["output"]["variables"])
    if malformed_grid:
        _write_malformed_grid_out(os.path.join(path, "grid.out"), dims, geometry)
    elif not drop_grid:
        _write_grid_out(os.path.join(path, "grid.out"), dims, geometry)
    with open(os.path.join(path, "dbl.out"), "w", encoding="utf-8", newline="\n") as stream:
        for frame, state in enumerate(states):
            dt = dts[frame] if dts is not None else 0.01
            step = steps[frame] if steps is not None else frame * 10
            stream.write(f"{frame} {frame * 0.01:.17g} {dt:.17g} {step} "
                         f"single_file {endian} {' '.join(variables)}\n")
            with open(os.path.join(path, f"data.{frame:04d}.dbl"), "wb") as binary:
                for field in state:
                    binary.write(struct.pack(f"<{len(field)}d", *field))
    if extra_file:
        with open(os.path.join(path, "unexpected.txt"), "w", encoding="utf-8") as stream:
            stream.write("not part of the PLUTO output contract\n")
    if side_files:
        _write_side_files(path)


def _states(rubric: dict) -> tuple[list[list[list[float]]], tuple[int, int, int], str]:
    physics = rubric["physics"]
    dims = tuple(int(value) for value in physics["fixture_dimensions"])
    geometry = physics["geometry"]
    variables = list(rubric["output"]["variables"])
    ncell = dims[0] * dims[1] * dims[2]
    eos = physics["eos"]
    gamma_raw = physics.get("gamma")
    # TAUB carries no compiled g_gamma (Src/globals.h:117-121); reconstruct()
    # never reads gamma on that branch, and the ideal-only entropy formula
    # below is only reached when eos == "ideal", where a rubric gamma is
    # always present -- NaN is a safe, never-dereferenced placeholder.
    gamma = float(gamma_raw) if gamma_raw is not None else float("nan")
    states = []
    for frame in range(3):
        primitive = {name: [] for name in variables}
        for index in range(ncell):
            phase = 2.0 * math.pi * index / max(1, ncell)
            rho = 1.0 + 0.05 * math.sin(phase) + 0.01 * frame
            vx1 = 0.12 * math.cos(phase)
            vx2 = 0.03 * math.sin(phase)
            vx3 = 0.02 * math.cos(2.0 * phase)
            prs = 0.7 + 0.03 * math.cos(phase) + 0.01 * frame
            primitive["rho"].append(rho)
            primitive["vx1"].append(vx1)
            primitive["vx2"].append(vx2)
            primitive["vx3"].append(vx3)
            primitive["prs"].append(prs)
            if "entropy" in primitive:
                # Matches generic_validator.py's _expected_entropy exactly:
                # the two branches PLUTO compiles (Src/RHD/rhd_energy_solve.c
                # :193-201). This branch is retained for any future rubric
                # that explicitly declares an emitted entropy variable; the
                # current official entropy-switch decks emit only rho/vx/prs.
                speed2 = vx1 * vx1 + vx2 * vx2 + vx3 * vx3
                lorentz2 = 1.0 / max(1.0 - speed2, 1.0e-300)
                lor = math.sqrt(lorentz2)
                if eos == "ideal":
                    entropy = prs * lor / (rho ** (gamma - 1.0))
                elif eos == "taub":
                    theta = prs / rho
                    entropy = (prs * lor / (rho ** (2.0 / 3.0))
                               * (1.5 * theta + math.sqrt(2.25 * theta * theta + 1.0)))
                else:
                    raise ValueError(f"no entropy formula for eos {eos!r}")
                primitive["entropy"].append(entropy)
        state = [primitive[name] for name in variables]
        # Reconstruct once here as an explicit fixture assertion.  The output
        # contract remains primitive-only; validators independently rebuild the
        # conserved state from those fields.
        reconstructed = reconstruct(primitive, eos, gamma)
        if any(not math.isfinite(value) for values in reconstructed.values() for value in values):
            raise ValueError("fixture reconstruction unexpectedly nonfinite")
        states.append(state)
    return states, dims, geometry


def build(check_dir: str, output: str) -> None:
    with open(os.path.join(check_dir, "rubric.json"), "r", encoding="utf-8") as stream:
        rubric = json.load(stream)
    _fresh(output)
    states, dims, geometry = _states(rubric)
    variables = list(rubric["output"]["variables"])
    _write_tree(os.path.join(output, "reference"), rubric, states, dims, geometry)
    _write_tree(os.path.join(output, "accept-identity"), rubric, states, dims, geometry)
    signed = [[list(field) for field in state] for state in states]
    zero_index = variables.index("vx2")
    for state in signed:
        state[zero_index][0] = -0.0
    _write_tree(os.path.join(output, "accept-arithmetic-variation"), rubric,
                signed, dims, geometry)
    _write_tree(os.path.join(output, "accept-with-side-files"), rubric, states, dims, geometry,
                side_files=True)

    near = [[list(field) for field in state] for state in states]
    near[1][variables.index("rho")][1] += 1.0e-3
    _write_tree(os.path.join(output, "reject-near-miss"), rubric, near, dims, geometry)
    nan_state = [[list(field) for field in state] for state in states]
    nan_state[1][variables.index("prs")][2] = float("nan")
    _write_tree(os.path.join(output, "reject-nan"), rubric, nan_state, dims, geometry)
    wrong_steps = [0, 10, 21]
    _write_tree(os.path.join(output, "reject-wrong-nstep"), rubric, states, dims, geometry,
                steps=wrong_steps)
    wrong_dts = [0.01, 0.01, 0.011]
    _write_tree(os.path.join(output, "reject-wrong-dt"), rubric, states, dims, geometry,
                dts=wrong_dts)
    _write_tree(os.path.join(output, "reject-omitted-variable"), rubric, states, dims, geometry,
                names=variables[:-1])
    _write_tree(os.path.join(output, "reject-wrong-name"), rubric, states, dims, geometry,
                names=[*variables[:-1], f"not_{variables[-1]}"])
    _write_tree(os.path.join(output, "reject-extra-file"), rubric, states, dims, geometry,
                extra_file=True)
    _write_tree(os.path.join(output, "reject-missing-grid"), rubric, states, dims, geometry,
                drop_grid=True)
    _write_tree(os.path.join(output, "reject-malformed-grid"), rubric, states, dims, geometry,
                malformed_grid=True)
    # 'little_endian' is the retired fabricated-contract token this leaf used
    # to accept; asserting it is now rejected is a direct regression guard.
    _write_tree(os.path.join(output, "reject-wrong-endian"), rubric, states, dims, geometry,
                endian="little_endian")

    ncell = dims[0] * dims[1] * dims[2]
    print(f"{rubric['check']}: 3 frames, {ncell} cells, {len(variables)} variables; "
          f"identity/signed-zero/side-file accepts and contract rejects emitted")
