#!/usr/bin/env python3
"""Deterministic synthetic fixtures for every check's validator.

`build_reference` fabricates a fully self-consistent artifact pair for a check
from its rubric alone (no Athena++ run): smooth positive fields, native-style
face arrays, history/error rows, checkpoints and provenance that satisfy every
structural and binding rule.  `variants` then derives accept trees (byte copy,
re-encoded/re-run provenance) and reject trees, including near-misses that are
correct on the cell-centred state but wrong in the CT/face, div-B, history,
provenance or lattice-order evidence.  The suite runner in
`negative_fixtures.py` asserts each verdict, so the discrimination of the
visible predicate is testable without an oracle run.  Fixture values are not
physics; they only exercise the verifier.
"""
from __future__ import annotations

import array
import base64
import hashlib
import json
import math
import shutil
from pathlib import Path
from typing import Any, Callable

import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from case_spec import PIPELINE, RAW_BUILDS, RAW_RUNS, SOURCE_COMMIT, VARIABLES, CheckSpec, RunPlan, deck_sha256, load_check  # noqa: E402
from extract_mhd import (  # noqa: E402
    ERROR_FILES, FACE_NAMES, block_columns_from_global, check_level_evidence, decode, derived_observables, encode, face_scale, face_shapes,
    observables_document, state_document,
)

STATE = "mhd_state.json"
OBSERVABLES = "mhd_observables.json"
EXTENTS = {
    "linear_wave": ((0.0, 3.0), (0.0, 1.5), (0.0, 1.5)),
    "cpaw": ((0.0, 2.236068), (0.0, 1.118034), (-0.5, 0.5)),
    "shock_tube": ((-0.5, 0.5), (-0.5, 0.5), (-0.5, 0.5)),
    "quirk": ((0.0, 1.0), (-0.0625, 0.0625), (-0.5, 0.5)),
}
HST_COLUMNS = ["time", "dt", "mass", "1-mom", "2-mom", "3-mom", "1-KE", "2-KE", "3-KE", "tot-E", "1-ME", "2-ME", "3-ME"]


def _fake_hash(*parts: Any) -> str:
    return hashlib.sha256(json.dumps(parts, sort_keys=True, default=str).encode()).hexdigest()


def face_digest(blocks: list[dict[str, Any]]) -> str:
    """The same digest `extract_mhd._face_blocks` derives from the native faces."""
    digest = hashlib.sha256()
    for block in blocks:
        concatenated = array.array("d")
        for name in FACE_NAMES:
            concatenated.extend(block["faces"][name])
        digest.update(concatenated.tobytes())
    return digest.hexdigest()


def _mesh_size(plan: RunPlan) -> dict[str, Any]:
    (x1a, x1b), (x2a, x2b), (x3a, x3b) = EXTENTS[plan.problem]
    return {"x1min": x1a, "x2min": x2a, "x3min": x3a, "x1max": x1b, "x2max": x2b, "x3max": x3b, "x1rat": 1.0, "x2rat": 1.0, "x3rat": 1.0,
            "nx1": plan.dimensions[0], "nx2": plan.dimensions[1], "nx3": plan.dimensions[2]}


def _field(seed: float, x: float, y: float, z: float, t: float) -> float:
    return 1.0e-3 * math.sin(2.1 * x + seed) * math.cos(1.3 * y - seed) * math.cos(0.7 * z + 0.5 * seed + t)


def _columns(coords: tuple[array.array, array.array, array.array], t: float, eos: str, seed: float) -> dict[str, array.array]:
    x1, x2, x3 = coords
    n = len(x1)
    cols = {"x1": x1, "x2": x2, "x3": x3}
    cols["rho"] = array.array("d", (1.0 + _field(seed, x1[i], x2[i], x3[i], t) for i in range(n)))
    cols["press"] = array.array("d", (0.6 + _field(seed + 1.0, x1[i], x2[i], x3[i], t) for i in range(n))) if eos == "adiabatic" else array.array("d", cols["rho"])
    for k, name in enumerate(("vel1", "vel2", "vel3")):
        cols[name] = array.array("d", (_field(seed + 2.0 + k, x1[i], x2[i], x3[i], t) for i in range(n)))
    for k, name in enumerate(("Bcc1", "Bcc2", "Bcc3")):
        base = (1.0, math.sqrt(2.0), 0.5)[k]
        cols[name] = array.array("d", (base + _field(seed + 5.0 + k, x1[i], x2[i], x3[i], t) for i in range(n)))
    return {name: cols[name] for name in VARIABLES}


def _faces(shape: list[int], t: float, seed: float) -> dict[str, array.array]:
    faces = {}
    for k, name in enumerate(FACE_NAMES):
        fz, fy, fx = face_shapes(shape)[name]
        base = (1.0, math.sqrt(2.0), 0.5)[k]
        faces[name] = array.array("d", (base + 1.0e-3 * math.sin(0.37 * n + seed + k + t) for n in range(fz * fy * fx)))
    return faces


def _global_coords(plan: RunPlan, mesh: dict[str, Any]) -> tuple[array.array, array.array, array.array]:
    nx, ny, nz = plan.dimensions
    axes = []
    for axis, n in ((1, nx), (2, ny), (3, nz)):
        lo, hi = mesh[f"x{axis}min"], mesh[f"x{axis}max"]
        axes.append([lo + (hi - lo) * (i + 0.5) / n for i in range(n)])
    x1, x2, x3 = array.array("d"), array.array("d"), array.array("d")
    for k in range(nz):
        for j in range(ny):
            for i in range(nx):
                x1.append(axes[0][i]); x2.append(axes[1][j]); x3.append(axes[2][k])
    return x1, x2, x3


def _block_layout(plan: RunPlan) -> list[dict[str, Any]]:
    """Deterministic two-level static-refinement tree: refine the first eight root blocks."""
    nbx = [n // b for n, b in zip(plan.dimensions, plan.meshblock)]
    roots = [(i, j, k) for k in range(nbx[2]) for j in range(nbx[1]) for i in range(nbx[0])]
    refined = set(roots[:8])
    blocks = []
    for (i, j, k) in roots:
        if (i, j, k) in refined:
            for c in range(8):
                blocks.append({"level": 1, "location": [2 * i + (c & 1), 2 * j + ((c >> 1) & 1), 2 * k + ((c >> 2) & 1)]})
        else:
            blocks.append({"level": 0, "location": [i, j, k]})
    if len(blocks) != plan.expected_blocks:
        raise ValueError(f"fixture tree has {len(blocks)} blocks, plan expects {plan.expected_blocks}")
    return blocks


def _block_coords(plan: RunPlan, mesh: dict[str, Any], block: dict[str, Any]) -> tuple[array.array, array.array, array.array]:
    bx, by, bz = plan.meshblock
    level = block["level"]
    axes = []
    for axis, (n_cells, n_mesh) in enumerate(((bx, mesh["nx1"]), (by, mesh["nx2"]), (bz, mesh["nx3"]))):
        lo, hi = mesh[f"x{axis + 1}min"], mesh[f"x{axis + 1}max"]
        dx = (hi - lo) / (n_mesh * (1 << level))
        origin = lo + block["location"][axis] * n_cells * dx
        axes.append([origin + (i + 0.5) * dx for i in range(n_cells)])
    x1, x2, x3 = array.array("d"), array.array("d"), array.array("d")
    for k in range(bz):
        for j in range(by):
            for i in range(bx):
                x1.append(axes[0][i]); x2.append(axes[1][j]); x3.append(axes[2][k])
    return x1, x2, x3


def _uniform_tree(plan: RunPlan) -> list[dict[str, Any]]:
    nbx = [n // b for n, b in zip(plan.dimensions, plan.meshblock)]
    return [{"level": 0, "location": [i, j, k]} for k in range(nbx[2]) for j in range(nbx[1]) for i in range(nbx[0])]


def _frame(plan: RunPlan, mesh: dict[str, Any], fi: int, t: float, gamma: float, seed: float) -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    cycle = fi * 7
    rst_file = f"Fixture.{'final' if t == plan.tlim else f'{fi:05d}'}.rst"
    rst = {"file": rst_file, "sha256": _fake_hash("rst", plan.run_id, fi, seed), "bytes": 1000 + fi, "time": t, "dt": 0.01, "ncycle": cycle,
           "nbtotal": plan.expected_blocks, "root_level": 1, "mesh_size": mesh, "meshblock": list(plan.meshblock), "nhydro": 4 if plan.eos == "isothermal" else 5}
    bx, by, bz = plan.meshblock
    shape = [bz, by, bx]
    tree = _uniform_tree(plan) if plan.refinement == "none" else _block_layout(plan)
    blocks = []
    global_columns = None
    if plan.refinement == "none":
        global_columns = _columns(_global_coords(plan, mesh), t, plan.eos, seed)
    for gid, block in enumerate(tree):
        loc = {"level": block["level"], "lx1": block["location"][0], "lx2": block["location"][1], "lx3": block["location"][2]}
        columns = (block_columns_from_global(global_columns, plan.dimensions, plan.meshblock, block["location"]) if global_columns is not None
                   else _columns(_block_coords(plan, mesh, block), t, plan.eos, seed + gid))
        blocks.append({"gid": gid, "location": loc, "shape": shape, "columns": columns, "faces": _faces(shape, t, seed + gid)})
    derived = derived_observables(blocks, mesh, gamma, plan.eos, global_columns, plan.dimensions)
    records = [{"gid": b["gid"], "level": b["location"]["level"], "location": [b["location"]["lx1"], b["location"]["lx2"], b["location"]["lx3"]], "shape": shape,
                "shapes": face_shapes(shape), "components": {n: encode(b["faces"][n]) for n in FACE_NAMES}} for b in blocks]
    face_field = {"source": "native restart checkpoint b.x1f/b.x2f/b.x3f", "layout": "blocks", "blocks": records,
                  "sha256": face_digest(blocks), "max_abs_face": max(face_scale(b["faces"]) for b in blocks)}
    if plan.refinement == "none":
        nx, ny, nz = plan.dimensions
        state = {"time": t, "cycle": cycle, "layout": "global", "cell_count": nx * ny * nz, "columns": {n: encode(global_columns[n]) for n in VARIABLES}}
    else:
        state = {"time": t, "cycle": cycle, "layout": "blocks", "cell_count": plan.expected_blocks * bx * by * bz,
                 "blocks": [{"gid": b["gid"], "level": b["location"]["level"], "location": [b["location"]["lx1"], b["location"]["lx2"], b["location"]["lx3"]], "shape": shape,
                             "columns": {n: encode(b["columns"][n]) for n in VARIABLES}} for b in blocks]}
    obs = {"time": t, "cycle": cycle, "native_restart": rst, "face_field": face_field, **derived}
    return state, obs, rst


def build_reference(spec: CheckSpec, out_dir: Path, seed: float = 0.0, provenance_salt: str = "reference") -> None:
    """Write a self-consistent synthetic artifact pair for every run of the check."""
    rubric = spec.rubric
    gamma = float(rubric["gamma"])
    state_runs, obs_runs = [], []
    parent_checkpoints: dict[str, list[dict[str, Any]]] = {}
    parent_inventory: dict[str, list[dict[str, Any]]] = {}
    hst_columns = HST_COLUMNS if spec.runs[0].eos == "adiabatic" else [c for c in HST_COLUMNS if c != "tot-E"]
    for plan in spec.runs:
        mesh = _mesh_size(plan)
        frames_state, frames_obs, checkpoints = [], [], []
        hst_rows = []
        for fi, t in enumerate(plan.expected_times):
            state, obs, rst = _frame(plan, mesh, fi, t, gamma, seed + 10.0 * fi)
            checkpoints.append({"file": rst["file"], "sha256": rst["sha256"], "bytes": rst["bytes"], "time": rst["time"], "ncycle": rst["ncycle"]})
            row = [t, 0.01] + [1.0 + 0.001 * (fi + n) for n in range(len(hst_columns) - 2)]
            hst_rows.append(row)
            obs["native_history"] = {"row_index": fi, "values": dict(zip(hst_columns, row))}
            frames_state.append(state)
            frames_obs.append(obs)
        if not plan.is_restart:
            # interior checkpoint at the first step at or after checkpoint_dt when the schedule needs one
            if plan.checkpoint_dt < plan.tlim:
                interior_time = plan.checkpoint_dt * 1.05
                checkpoints.insert(1, {"file": "Fixture.00001.rst", "sha256": _fake_hash("rst-interior", plan.run_id, seed), "bytes": 1001, "time": interior_time, "ncycle": 3})
        hst = {"name": "Fixture.hst", "sha256": _fake_hash("hst", plan.run_id, seed), "bytes": 500, "columns": hst_columns, "rows": hst_rows, "header_present": not plan.is_restart}
        error = {"name": ERROR_FILES[plan.problem], "sha256": _fake_hash("err", plan.run_id, seed), "bytes": 200, "header": "Nx1 Nx2 Nx3 Ncycle RMS", "rows": [[float(plan.dimensions[0]), float(plan.dimensions[1]), float(plan.dimensions[2]), 7.0, 1.0e-7 + seed * 1e-9]]}
        inventory = [{"name": c["file"], "bytes": c["bytes"], "sha256": c["sha256"]} for c in checkpoints]
        inventory += [{"name": hst["name"], "bytes": hst["bytes"], "sha256": hst["sha256"]}, {"name": error["name"], "bytes": error["bytes"], "sha256": error["sha256"]}]
        inventory += [{"name": f"Fixture.block{g}.out2.{fi:05d}.tab", "bytes": 10, "sha256": _fake_hash("tab", plan.run_id, g, fi)} for g in range(plan.expected_blocks) for fi in range(len(plan.expected_times))]
        restart_record = None
        deck_path = f"/fixture/{provenance_salt}/tests/checks/{spec.case}/{rubric['check_deck']}"
        binary_path = f"/fixture/{provenance_salt}/cache/athena-{plan.build_key}"
        if plan.is_restart:
            parent = parent_checkpoints[plan.restart_from["run_id"]]
            chosen = min((c for c in parent if c["time"] >= plan.restart_from["checkpoint_time"] and c["time"] < plan.tlim and c["ncycle"] > 0 and not c["file"].endswith("final.rst")), key=lambda c: c["time"])
            restart_record = {"run_id": plan.restart_from["run_id"], "file": chosen["file"], "sha256": chosen["sha256"], "time": chosen["time"], "ncycle": chosen["ncycle"], "checkpoint_time": plan.restart_from["checkpoint_time"]}
            inventory.append({"name": chosen["file"], "bytes": chosen["bytes"], "sha256": chosen["sha256"]})
            checkpoints.append({"file": chosen["file"], "sha256": chosen["sha256"], "bytes": chosen["bytes"], "time": chosen["time"], "ncycle": chosen["ncycle"]})
            command = [*plan.launcher_prefix(), binary_path, "-r", chosen["file"], *plan.overrides()]
        else:
            command = [*plan.launcher_prefix(), binary_path, "-i", deck_path, *plan.overrides()]
        provenance = {
            "source_commit": SOURCE_COMMIT,
            "binary": {"path": binary_path, "sha256": _fake_hash("binary", plan.build_key, provenance_salt), "bytes": 123456, "origin": "built",
                       "configure": ["/usr/bin/python3", "-B", "configure.py", *plan.configure_args], "make": ["make", "-j2"], "build_key": plan.build_key},
            "deck": {"name": spec.deck.name, "path": deck_path, "sha256": deck_sha256(spec), "bytes": spec.deck.stat().st_size},
            "command": command, "runtime_overrides": plan.overrides(),
            "launcher": {"kind": plan.launcher, "threads": plan.threads, "ranks": plan.ranks, "prefix": plan.launcher_prefix(), "env": {}},
            "restart_from": restart_record, "cwd": f"/fixture/{provenance_salt}/run-{plan.run_id}", "exit_code": 0, "fatal_marker": False,
            "wall_seconds": 1.5, "cap_seconds": plan.cap_seconds, "stdout_sha256": _fake_hash("stdout", plan.run_id, provenance_salt),
            "stderr_sha256": _fake_hash("stderr", plan.run_id, provenance_salt), "native_outputs": inventory,
            "source": {"path": "/fixture/src", "claimed_commit": SOURCE_COMMIT, "manifest": None,
                       "tree": {"files": 664, "bytes": 1, "digest": _fake_hash("source-tree", provenance_salt)},
                       "matches_pinned_manifest": True, "reason": "synthetic fixture"},
            "completion": {"terminated": "Terminating on time limit", "end_time": plan.expected_times[-1],
                           "end_cycle": (len(plan.expected_times) - 1) * 7, "openmp_runtime_record": plan.launcher == "openmp",
                           "fatal_marker": False},
            "probe": {
                "mesh_probe": ({"command": plan.mesh_probe_argv(binary_path, deck_path), "exit_code": 0,
                                "files": {"structure": "mesh_structure.dat", "stdout": "mesh.stdout.log", "stderr": "mesh.stderr.log"},
                                "receipt": {"root_grid": [n // b for n, b in zip(plan.dimensions, plan.meshblock)],
                                            "total_blocks": plan.expected_blocks, "ranks": plan.ranks,
                                            "blocks_per_rank": {"0": plan.expected_blocks}, "blocks_per_level": {"0": plan.expected_blocks},
                                            "root_level": 0, "evidence": "synthetic fixture"}}
                               if plan.mesh_probe_supported else None),
                "launcher_probe": ({"command": plan.launcher_probe_argv(), "exit_code": 0, "files": {"log": "launcher.log"}}
                                   if plan.launcher == "mpi" else None),
                "skipped_reason": None if plan.mesh_probe_supported else "synthetic fixture: unprobeable plan",
            },
            "retained": {"native": f"{RAW_RUNS}/{plan.run_id}/native", "logs": f"{RAW_RUNS}/{plan.run_id}/logs",
                         "probe": f"{RAW_RUNS}/{plan.run_id}/probe" if (plan.mesh_probe_supported or plan.launcher == "mpi") else None,
                         "build": f"{RAW_BUILDS}/{plan.build_key}"},
        }
        parent_checkpoints[plan.run_id] = checkpoints
        parent_inventory[plan.run_id] = inventory
        state_runs.append({"run_id": plan.run_id, "mesh": {"dimensions": list(plan.dimensions), "meshblock": list(plan.meshblock), "refinement": plan.refinement, "mesh_size": mesh},
                           "frames": frames_state, "provenance": provenance})
        obs_runs.append({"run_id": plan.run_id, "frames": frames_obs, "native_history_file": hst, "native_error_file": error, "native_checkpoints": checkpoints,
                         "mechanism_evidence": {"launcher": provenance["launcher"], "refinement": plan.refinement, "restart_from": restart_record,
                                                "meshblocks": plan.expected_blocks, "levels": list(plan.expected_levels)}})
    check_level = check_level_evidence(spec, state_runs)
    out_dir.mkdir(parents=True, exist_ok=True)
    _write(out_dir / STATE, state_document(spec.case, rubric["id"], state_runs))
    _write(out_dir / OBSERVABLES, observables_document(spec.case, rubric["id"], obs_runs, check_level))


def _write(path: Path, document: dict[str, Any], indent: int | None = None) -> None:
    path.write_text(json.dumps(document, allow_nan=False, separators=(",", ":") if indent is None else None, indent=indent) + "\n", encoding="utf-8")


def _load(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def _perturb_column(text: str, index: int, delta: float) -> str:
    values = decode(text)
    values[index] += delta
    return encode(values)


def _first_frame(doc: dict[str, Any], key: str) -> dict[str, Any]:
    return doc["runs"][0]["frames"][-1] if key == "last" else doc["runs"][0]["frames"][0]


def variants(spec: CheckSpec, reference: Path, root: Path) -> list[tuple[str, bool]]:
    """Write accept*/reject* trees derived from the reference; return (name, expected_pass)."""
    results: list[tuple[str, bool]] = []

    def derive(name: str, expected: bool, mutate: Callable[[Path], None]) -> None:
        target = root / name
        shutil.copytree(reference, target)
        mutate(target)
        results.append((name, expected))

    derive("accept-identical", True, lambda d: None)

    def reencode(d: Path) -> None:
        for name in (STATE, OBSERVABLES):
            _write(d / name, _load(d / name), indent=1)
    derive("accept-reencoded-json", True, reencode)

    def rerun_provenance(d: Path) -> None:
        doc = _load(d / STATE)
        for run in doc["runs"]:
            prov = run["provenance"]
            # a rerun may legitimately reuse a cached binary, but a cache hit
            # still carries the configure/make argv of the build that made it
            prov["binary"] = {**prov["binary"], "sha256": _fake_hash("other-build"), "path": "/elsewhere/athena", "origin": "build-cache"}
            prov["command"] = [*prov["launcher"]["prefix"], "/elsewhere/athena", *prov["command"][len(prov["launcher"]["prefix"]) + 1:]]
            prov["wall_seconds"] = 9.75
            prov["cwd"] = "/elsewhere/run"
            prov["stdout_sha256"] = _fake_hash("other-stdout")
        _write(d / STATE, doc)
    derive("accept-independent-rerun-provenance", True, rerun_provenance)

    def perturb_state(d: Path) -> None:
        doc = _load(d / STATE)
        frame = _first_frame(doc, "last")
        if frame["layout"] == "global":
            frame["columns"]["rho"] = _perturb_column(frame["columns"]["rho"], 0, 1.0e-3)
        else:
            frame["blocks"][0]["columns"]["rho"] = _perturb_column(frame["blocks"][0]["columns"]["rho"], 0, 1.0e-3)
        _write(d / STATE, doc)
    derive("reject-perturbed-state", False, perturb_state)

    def near_miss_faces(d: Path) -> None:
        """State identical; one native face value moved by one ulp with every derived observable recomputed."""
        obs = _load(d / OBSERVABLES)
        state = _load(d / STATE)
        plan = spec.runs[0]
        frame = obs["runs"][0]["frames"][-1]
        sframe = state["runs"][0]["frames"][-1]
        mesh = state["runs"][0]["mesh"]["mesh_size"]
        gamma = float(spec.rubric["gamma"])
        blocks = []
        for bi, record in enumerate(frame["face_field"]["blocks"]):
            faces = {n: decode(record["components"][n]) for n in FACE_NAMES}
            if bi == 0:
                faces["B1f"][0] = math.nextafter(faces["B1f"][0], math.inf)
                record["components"] = {n: encode(faces[n]) for n in FACE_NAMES}
            loc = {"level": record["level"], "lx1": record["location"][0], "lx2": record["location"][1], "lx3": record["location"][2]}
            columns = None if sframe["layout"] == "global" else {n: decode(sframe["blocks"][bi]["columns"][n]) for n in VARIABLES}
            blocks.append({"gid": bi, "location": loc, "shape": record["shape"], "faces": faces, "columns": columns})
        global_columns = {n: decode(sframe["columns"][n]) for n in VARIABLES} if sframe["layout"] == "global" else None
        frame["face_field"]["max_abs_face"] = max(face_scale(b["faces"]) for b in blocks)
        frame["face_field"]["sha256"] = face_digest(blocks)
        frame.update(derived_observables(blocks, mesh, gamma, plan.eos, global_columns, plan.dimensions))
        _write(d / OBSERVABLES, obs)
    derive("reject-near-miss-native-faces", False, near_miss_faces)

    def face_digest_only(d: Path) -> None:
        """Every face component is the reference's; only the reported digest is wrong."""
        obs = _load(d / OBSERVABLES)
        obs["runs"][0]["frames"][-1]["face_field"]["sha256"] = _fake_hash("unrecomputed-face-digest")
        _write(d / OBSERVABLES, obs)
    derive("reject-unrecomputed-face-digest", False, face_digest_only)

    def check_level_policy(d: Path) -> None:
        """The owner-pending cross-run determinism slot is filled in with a claim."""
        obs = _load(d / OBSERVABLES)
        obs["check_level"]["cross_run_determinism_policy"] = "serial==openmp==mpi"
        _write(d / OBSERVABLES, obs)
    derive("reject-unowned-cross-run-policy", False, check_level_policy)

    def fatal_exit_zero(d: Path) -> None:
        """Athena++ returns 0 after a fatal error; the marker must still fail the run."""
        doc = _load(d / STATE)
        doc["runs"][0]["provenance"]["fatal_marker"] = True
        doc["runs"][0]["provenance"]["completion"]["fatal_marker"] = True
        _write(d / STATE, doc)
    derive("reject-fatal-marker-with-exit-zero", False, fatal_exit_zero)

    def unbound_retention(d: Path) -> None:
        """The declared retained-evidence paths no longer point at this run's closure."""
        doc = _load(d / STATE)
        doc["runs"][0]["provenance"]["retained"]["native"] = "raw/runs/elsewhere/native"
        _write(d / STATE, doc)
    derive("reject-unbound-retained-paths", False, unbound_retention)

    def near_miss_divb_report(d: Path) -> None:
        obs = _load(d / OBSERVABLES)
        obs["runs"][0]["frames"][-1]["discrete_div_b"]["l1"] = 0.0
        obs["runs"][0]["frames"][-1]["discrete_div_b"]["linf"] = 0.0
        obs["runs"][0]["frames"][-1]["discrete_div_b"]["max_abs"] = 0.0
        _write(d / OBSERVABLES, obs)
    derive("reject-near-miss-divb-not-recomputed", False, near_miss_divb_report)

    # A single-MeshBlock run has no neighbour, so its real shared_face_consistency
    # is already {mismatches: 0, max_abs_difference: 0.0}: tampering it that way is
    # indistinguishable from the untouched reference and would not be a near-miss.
    # Target the first run that actually has interior shared faces to compare.
    multiblock_run = next((i for i, plan in enumerate(spec.runs) if plan.expected_blocks > 1), None)

    def near_miss_shared_face_report(d: Path) -> None:
        obs = _load(d / OBSERVABLES)
        obs["runs"][multiblock_run]["frames"][-1]["shared_face_consistency"]["mismatches"] = 0
        obs["runs"][multiblock_run]["frames"][-1]["shared_face_consistency"]["max_abs_difference"] = 0.0
        _write(d / OBSERVABLES, obs)
    if multiblock_run is not None:
        derive("reject-near-miss-shared-face-report", False, near_miss_shared_face_report)

    def _unused(d: Path) -> None:
        pass
    def near_miss_history(d: Path) -> None:
        obs = _load(d / OBSERVABLES)
        run = obs["runs"][0]
        run["native_history_file"]["rows"][-1][2] += 1.0e-9
        run["frames"][-1]["native_history"]["values"] = dict(zip(run["native_history_file"]["columns"], run["native_history_file"]["rows"][-1]))
        _write(d / OBSERVABLES, obs)
    derive("reject-near-miss-native-history", False, near_miss_history)

    def near_miss_selector(d: Path) -> None:
        doc = _load(d / STATE)
        prov = doc["runs"][0]["provenance"]
        overrides = list(prov["runtime_overrides"])
        overrides[-1] = "problem/compute_error=false" if overrides[-1] != "problem/compute_error=false" else "problem/compute_error=true"
        prov["runtime_overrides"] = overrides
        prov["command"] = prov["command"][:len(prov["command"]) - len(overrides)] + overrides
        _write(d / STATE, doc)
    derive("reject-near-miss-runtime-selector", False, near_miss_selector)

    def near_miss_deck(d: Path) -> None:
        doc = _load(d / STATE)
        doc["runs"][0]["provenance"]["deck"]["sha256"] = _fake_hash("other-deck")
        _write(d / STATE, doc)
    derive("reject-near-miss-deck-hash", False, near_miss_deck)

    def near_miss_error_rows(d: Path) -> None:
        obs = _load(d / OBSERVABLES)
        obs["runs"][0]["native_error_file"]["rows"][0][-1] *= 2.0
        _write(d / OBSERVABLES, obs)
    derive("reject-near-miss-analytic-error", False, near_miss_error_rows)

    plan0 = spec.runs[0]
    nbx0 = [n // b for n, b in zip(plan0.dimensions, plan0.meshblock)]
    # block-major listing differs from the global lattice only when blocks tile more than one axis
    if plan0.refinement == "none" and sum(1 for n in nbx0 if n > 1) >= 1 and (nbx0[0] > 1 and (plan0.dimensions[1] > 1 or plan0.dimensions[2] > 1)):
        def block_major(d: Path) -> None:
            """Cells listed block-by-block instead of the global lattice: same multiset, wrong placement."""
            doc = _load(d / STATE)
            plan = spec.runs[0]
            frame = doc["runs"][0]["frames"][-1]
            nx, ny, nz = plan.dimensions
            bx, by, bz = plan.meshblock
            order = []
            for k0 in range(0, nz, bz):
                for j0 in range(0, ny, by):
                    for i0 in range(0, nx, bx):
                        for k in range(k0, k0 + bz):
                            for j in range(j0, j0 + by):
                                for i in range(i0, i0 + bx):
                                    order.append((k * ny + j) * nx + i)
            for name in VARIABLES:
                values = decode(frame["columns"][name])
                frame["columns"][name] = encode(array.array("d", (values[g] for g in order)))
            _write(d / STATE, doc)
        derive("reject-near-miss-block-major-order", False, block_major)

    if len(spec.runs) > 1:
        def drop_run(d: Path) -> None:
            for name in (STATE, OBSERVABLES):
                doc = _load(d / name)
                doc["runs"] = doc["runs"][:-1]
                _write(d / name, doc)
        derive("reject-missing-run", False, drop_run)

    if any(plan.is_restart for plan in spec.runs):
        def restart_claim(d: Path) -> None:
            """The restart-reproduction map is a recomputed observable, not a report."""
            obs = _load(d / OBSERVABLES)
            for run_id, value in list(obs["check_level"]["restart_reproduces_parent"].items()):
                obs["check_level"]["restart_reproduces_parent"][run_id] = not value
            _write(d / OBSERVABLES, obs)
        derive("reject-unrecomputed-restart-claim", False, restart_claim)

        def wrong_checkpoint(d: Path) -> None:
            doc = _load(d / STATE)
            for run in doc["runs"]:
                if run["provenance"]["restart_from"]:
                    run["provenance"]["restart_from"]["sha256"] = _fake_hash("foreign-checkpoint")
            _write(d / STATE, doc)
        derive("reject-near-miss-foreign-restart-checkpoint", False, wrong_checkpoint)

    def malformed(d: Path) -> None:
        (d / STATE).write_text('{"schema":', encoding="utf-8")
    derive("reject-malformed-json", False, malformed)

    def extra_file(d: Path) -> None:
        (d / "unexpected.txt").write_text("not an artifact\n", encoding="utf-8")
    derive("reject-unexpected-artifact", False, extra_file)

    def symlink(d: Path) -> None:
        (d / STATE).unlink()
        (d / STATE).symlink_to(reference / STATE)
    derive("reject-symlink-artifact", False, symlink)

    def nonfinite(d: Path) -> None:
        doc = _load(d / STATE)
        frame = _first_frame(doc, "last")
        container = frame["columns"] if frame["layout"] == "global" else frame["blocks"][0]["columns"]
        values = decode(container["vel1"])
        values[0] = math.nan
        container["vel1"] = encode(values)
        _write(d / STATE, doc)
    derive("reject-non-finite-state", False, nonfinite)

    def negative_density(d: Path) -> None:
        doc = _load(d / STATE)
        frame = _first_frame(doc, "last")
        container = frame["columns"] if frame["layout"] == "global" else frame["blocks"][0]["columns"]
        values = decode(container["rho"])
        values[0] = -values[0]
        container["rho"] = encode(values)
        _write(d / STATE, doc)
    derive("reject-non-positive-density", False, negative_density)

    def no_native_faces(d: Path) -> None:
        obs = _load(d / OBSERVABLES)
        obs["ct_update_evidence"]["native_face_checkpoint"] = False
        _write(d / OBSERVABLES, obs)
    derive("reject-no-native-face-claim", False, no_native_faces)

    def owner_tolerance_claim(d: Path) -> None:
        doc = _load(d / STATE)
        doc["runs"][0]["provenance"]["exit_code"] = 1
        _write(d / STATE, doc)
    derive("reject-nonzero-exit", False, owner_tolerance_claim)
    return results


def make(check_dir: Path, root: Path) -> list[tuple[str, bool]]:
    spec = load_check(check_dir)
    reference = root / "reference"
    build_reference(spec, reference)
    return variants(spec, reference, root)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: fixtures.py CHECK_DIR OUT_ROOT")
    for name, expected in make(Path(sys.argv[1]), Path(sys.argv[2])):
        print(f"{name}: expected passed={expected}")
