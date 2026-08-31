#!/usr/bin/env python3
"""Readers for the non-array native evidence retained beside every run.

The state/observable JSON products are a decoded index of one execution; the
bytes they were derived from are retained under ``raw/`` in the same result
directory.  This module reads the channels that are not arrays:

* the pinned ``ShowConfig()`` banner (``athena -c``) of the retained binary,
  which reports the compile-time problem generator, coordinate system, EOS,
  Riemann solver, ghost-cell count and MPI/OpenMP support of the build that
  actually ran;
* Athena++ stdout/stderr of the graded run, whose ``### FATAL ERROR`` marker,
  ``Terminating on ...`` reason, final ``time=/cycle=`` line and
  ``omp wtime used`` line are the completion evidence (the pinned executable
  returns 0 even after a fatal error, so the exit code alone proves nothing);
* ``mesh_structure.dat`` and its stdout, written by ``Mesh::OutputMeshStructure``
  under ``athena -m <nproc>``, which record the MeshBlock tree *and* the rank
  that owns every block, i.e. the decomposition the same deck/launcher produces;
* the MPI launcher probe, one line per rank straight from ``mpirun``;
* the parameter dump embedded in every ``.rst`` file, which is the running
  parameter set (deck plus command-line overrides) as the executable saw it.

Nothing here encodes a numerical tolerance or a scientific pass policy.
"""
from __future__ import annotations

import hashlib
import os
import re
import sys
from pathlib import Path
from typing import Any

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from case_spec import RunPlan  # noqa: E402
from native_restart import block_param, RestartFormatError  # noqa: E402

FATAL = "### FATAL ERROR"
SETUP = "Setup complete, entering main loop..."
NORMAL_TERMINATION = "Terminating on time limit"
OTHER_TERMINATIONS = ("Terminating on Terminate signal", "Terminating on Interrupt signal",
                      "Terminating on wall-time limit", "Terminating on cycle limit")
OMP_MARKER = "omp wtime used"
END_TIME_RE = re.compile(r"(?m)^time=(\S+) cycle=(\d+)\s*$")
TLIM_RE = re.compile(r"(?m)^tlim=(\S+) nlim=(-?\d+)\s*$")
CONFIG_RE = re.compile(r"(?m)^  (?P<key>[A-Za-z][^:]*?):\s+(?P<value>.*?)\s*$")
MESH_BLOCK_RE = re.compile(r"(?m)^#MeshBlock (?P<gid>\d+) on rank=(?P<rank>\d+) with cost=(?P<cost>\S+)\s*$")
MESH_LOC_RE = re.compile(r"(?m)^#  Logical level (?P<level>\d+), location = \((?P<lx1>-?\d+) (?P<lx2>-?\d+) (?P<lx3>-?\d+)\)\s*$")
ROOT_GRID_RE = re.compile(r"(?m)^Root grid = (\d+) x (\d+) x (\d+) MeshBlocks\s*$")
TOTAL_BLOCKS_RE = re.compile(r"(?m)^Total number of MeshBlocks = (\d+)\s*$")
PHYSICAL_LEVELS_RE = re.compile(r"(?m)^Number of physical refinement levels = (\d+)\s*$")
LOGICAL_LEVELS_RE = re.compile(r"(?m)^Number of logical  refinement levels = (\d+)\s*$")
RANKS_RE = re.compile(r"(?m)^Number of parallel ranks = (\d+)\s*$")
RANK_ROW_RE = re.compile(r"(?m)^  Rank = (\d+): (\d+) MeshBlocks, cost = (\S+)\s*$")
LEVEL_ROW_RE = re.compile(r"(?m)^  Physical level = (\d+) \(logical level = (\d+)\): (\d+) MeshBlocks, cost = (\S+)\s*$")
LAUNCHER_ROW_RE = re.compile(r"(?m)^rank=(\d+) size=(\d+) pid=(\d+) host=(\S*)\s*$")
ELF_MAGIC = b"\x7fELF"


class EvidenceError(ValueError):
    """Raised for any unusable, missing or contradicting native evidence."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_text(path: Path) -> str:
    if path.is_symlink() or not path.is_file():
        raise EvidenceError(f"retained evidence file is missing or a symlink: {path.name}")
    return path.read_text(encoding="utf-8", errors="replace")


def closure(root: Path, prefix: str) -> list[dict[str, Any]]:
    """Hash every regular file below ``root``; refuse symlinks and specials.

    The returned list (sorted by relative path) is the exact retained-byte
    closure of a result root; both the writer and the verifier build it the
    same way, so a missing, extra or rewritten file is a mismatch.
    """
    entries: list[dict[str, Any]] = []
    if not root.is_dir() or root.is_symlink():
        return entries
    for directory, dirs, files in os.walk(root):
        for name in sorted(dirs):
            if os.path.islink(os.path.join(directory, name)):
                raise EvidenceError(f"retained evidence contains a symlinked directory: {name}")
        for name in sorted(files):
            full = Path(directory) / name
            if full.is_symlink():
                raise EvidenceError(f"retained evidence contains a symlink: {full.name}")
            if not full.is_file():
                raise EvidenceError(f"retained evidence contains a non-regular file: {full.name}")
            relative = os.path.join(prefix, os.path.relpath(full, root)).replace(os.sep, "/")
            entries.append({"path": relative, "bytes": full.stat().st_size, "sha256": sha256_file(full)})
    entries.sort(key=lambda entry: entry["path"])
    return entries


# ----------------------------------------------------------------- ShowConfig banner
def parse_show_config(text: str) -> dict[str, str]:
    if "This Athena++ executable is configured with:" not in text:
        raise EvidenceError("build banner is not the pinned ShowConfig() output")
    return {match.group("key"): match.group("value") for match in CONFIG_RE.finditer(text)}


def check_show_config(text: str, plan: RunPlan) -> dict[str, str]:
    """Bind the retained binary's compile-time configuration to the run plan."""
    banner = parse_show_config(text)
    for key, expected in plan.expected_show_config.items():
        actual = banner.get(key)
        if actual is None:
            raise EvidenceError(f"build banner does not report {key!r}")
        if actual != expected:
            raise EvidenceError(f"build banner {key!r} is {actual!r}, the run plan requires {expected!r}")
    return banner


def check_binary(path: Path, expected_sha256: str, expected_bytes: int) -> str:
    """Bind the retained executable image to the recorded build identity.

    The pinned Debian build produces an ELF image; the format is recorded
    rather than mandated, because the hash and the `athena -c` banner are what
    actually bind the executable, and a wrapper is not by itself dishonest.
    """
    if path.is_symlink() or not path.is_file():
        raise EvidenceError("the retained executable is missing or a symlink")
    size = path.stat().st_size
    if size <= 0 or size != expected_bytes:
        raise EvidenceError("the retained executable size does not match the recorded build identity")
    with open(path, "rb") as handle:
        head = handle.read(4)
    if head[:4] == ELF_MAGIC:
        image = "elf"
    elif head[:2] == b"#!":
        image = "script"
    else:
        raise EvidenceError("the retained executable is neither an ELF image nor an executable script")
    if sha256_file(path) != expected_sha256:
        raise EvidenceError("the retained executable does not hash to the recorded build identity")
    return image


# ----------------------------------------------------------------- run stdout/stderr
def fatal_marker(*texts: str) -> bool:
    return any(FATAL in text for text in texts)


def check_completion(stdout: str, stderr: str, plan: RunPlan, final_time: float, final_cycle: int) -> dict[str, Any]:
    """Re-derive completion from the retained logs of the graded run.

    Athena++ exits 0 after a fatal error, so the retained stdout/stderr are the
    evidence that the executable actually reached the end of the integration
    loop at the scheduled endpoint, and the OpenMP timing line is a runtime
    witness that an OpenMP-enabled build ran.
    """
    if fatal_marker(stdout, stderr):
        raise EvidenceError("retained log carries the Athena++ fatal-error marker (the executable exits 0 after it)")
    if SETUP not in stdout:
        raise EvidenceError("retained stdout does not show the main integration loop being entered")
    for other in OTHER_TERMINATIONS:
        if other in stdout:
            raise EvidenceError(f"retained stdout terminates abnormally: {other}")
    if NORMAL_TERMINATION not in stdout:
        raise EvidenceError("retained stdout does not show normal termination on the time limit")
    end = END_TIME_RE.search(stdout)
    if end is None:
        raise EvidenceError("retained stdout has no final 'time=... cycle=...' record")
    time_text, cycle_text = end.group(1), end.group(2)
    try:
        end_time = float(time_text)
    except ValueError as exc:
        raise EvidenceError("retained stdout final time is not a number") from exc
    if end_time != final_time and ("%g" % final_time) != time_text:
        raise EvidenceError(f"retained stdout ends at time {time_text}, the artifact's last frame is {final_time!r}")
    if int(cycle_text) != final_cycle:
        raise EvidenceError(f"retained stdout ends at cycle {cycle_text}, the artifact's last frame is {final_cycle}")
    limits = TLIM_RE.search(stdout)
    if limits is None or float(limits.group(1)) != plan.tlim:
        raise EvidenceError("retained stdout does not report the rubric time limit")
    if "zone-cycles/cpu_second" not in stdout:
        raise EvidenceError("retained stdout lacks the end-of-run zone-cycle record")
    omp = OMP_MARKER in stdout
    if omp != (plan.launcher == "openmp"):
        raise EvidenceError(
            "retained stdout OpenMP timing record contradicts the plan launcher "
            f"({'present' if omp else 'absent'} for launcher {plan.launcher!r})")
    return {"terminated": NORMAL_TERMINATION, "end_time": end_time, "end_cycle": int(cycle_text),
            "openmp_runtime_record": omp, "fatal_marker": False}


def parse_launcher_probe(text: str, ranks: int) -> dict[str, Any]:
    """One line per MPI rank, produced by the launcher itself, not by Athena++."""
    rows = LAUNCHER_ROW_RE.findall(text)
    if len(rows) != ranks:
        raise EvidenceError(f"launcher probe produced {len(rows)} rank lines, the plan launches {ranks}")
    seen_ranks = {int(row[0]) for row in rows}
    pids = {int(row[2]) for row in rows}
    if seen_ranks != set(range(ranks)):
        raise EvidenceError("launcher probe rank ids are not exactly 0..ranks-1")
    if any(int(row[1]) != ranks for row in rows):
        raise EvidenceError("launcher probe world size differs from the plan rank count")
    if len(pids) != ranks:
        raise EvidenceError("launcher probe ranks do not have distinct process ids")
    return {"ranks": ranks, "world_size": ranks, "distinct_pids": len(pids)}


# ----------------------------------------------------------------- mesh/rank probe
def parse_mesh_structure(text: str) -> list[dict[str, Any]]:
    blocks: list[dict[str, Any]] = []
    headers = list(MESH_BLOCK_RE.finditer(text))
    if not headers:
        raise EvidenceError("mesh_structure.dat carries no MeshBlock ownership records")
    for header in headers:
        location = MESH_LOC_RE.search(text, header.end())
        if location is None:
            raise EvidenceError(f"mesh_structure.dat has no logical location for MeshBlock {header.group('gid')}")
        blocks.append({
            "gid": int(header.group("gid")), "rank": int(header.group("rank")),
            "logical_level": int(location.group("level")),
            "location": [int(location.group("lx1")), int(location.group("lx2")), int(location.group("lx3"))],
        })
    return blocks


def parse_mesh_probe_stdout(text: str) -> dict[str, Any]:
    root_grid = ROOT_GRID_RE.search(text)
    total = TOTAL_BLOCKS_RE.search(text)
    physical = PHYSICAL_LEVELS_RE.search(text)
    logical = LOGICAL_LEVELS_RE.search(text)
    ranks = RANKS_RE.search(text)
    if not (root_grid and total and physical and logical and ranks):
        raise EvidenceError("mesh probe stdout is not the pinned OutputMeshStructure report")
    return {
        "root_grid": [int(root_grid.group(i)) for i in (1, 2, 3)],
        "total_blocks": int(total.group(1)),
        "physical_levels": int(physical.group(1)),
        "logical_levels": int(logical.group(1)),
        "root_level": int(logical.group(1)) - int(physical.group(1)),
        "ranks": int(ranks.group(1)),
        "blocks_per_rank": {int(rank): int(count) for rank, count, _ in RANK_ROW_RE.findall(text)},
        "blocks_per_level": {int(level): int(count) for level, _, count, _ in LEVEL_ROW_RE.findall(text)},
    }


def check_mesh_probe(stdout: str, structure: str, plan: RunPlan, tree: set[tuple[int, int, int, int]]) -> dict[str, Any]:
    """Verify the decomposition and rank ownership of a run from native bytes.

    ``tree`` is the (level, lx1, lx2, lx3) set re-derived from the graded run's
    own native restart dumps; the probe is executed with the same binary, deck,
    override list and launcher, so its MeshBlock tree must be the same tree and
    its rank rows must cover it exactly once.
    """
    if fatal_marker(stdout):
        raise EvidenceError("mesh probe stdout carries the Athena++ fatal-error marker")
    report = parse_mesh_probe_stdout(stdout)
    blocks = parse_mesh_structure(structure)
    root_blocks = [n // b for n, b in zip(plan.dimensions, plan.meshblock)]
    if report["root_grid"] != root_blocks:
        raise EvidenceError(f"mesh probe root grid {report['root_grid']} is not the rubric root grid {root_blocks}")
    if report["total_blocks"] != plan.expected_blocks or len(blocks) != plan.expected_blocks:
        raise EvidenceError("mesh probe MeshBlock count differs from the rubric")
    if report["ranks"] != plan.ranks:
        raise EvidenceError(f"mesh probe reports {report['ranks']} ranks, the plan launches {plan.ranks}")
    if sorted(report["blocks_per_rank"]) != list(range(plan.ranks)):
        raise EvidenceError("mesh probe does not report one row per launched rank")
    if sum(report["blocks_per_rank"].values()) != plan.expected_blocks:
        raise EvidenceError("mesh probe per-rank MeshBlock counts do not cover the mesh")
    if any(count <= 0 for count in report["blocks_per_rank"].values()):
        raise EvidenceError("mesh probe leaves a launched rank without a MeshBlock")
    owned: dict[int, int] = {rank: 0 for rank in report["blocks_per_rank"]}
    probe_tree: set[tuple[int, int, int, int]] = set()
    gids = set()
    for block in blocks:
        if block["rank"] not in owned:
            raise EvidenceError("mesh_structure.dat assigns a block to an unreported rank")
        owned[block["rank"]] += 1
        gids.add(block["gid"])
        level = block["logical_level"] - report["root_level"]
        if level < 0:
            raise EvidenceError("mesh_structure.dat records a block below the root level")
        probe_tree.add((level, *block["location"]))
    if gids != set(range(plan.expected_blocks)):
        raise EvidenceError("mesh_structure.dat MeshBlock ids are not the contiguous gid range")
    if owned != report["blocks_per_rank"]:
        raise EvidenceError("mesh_structure.dat rank ownership disagrees with the per-rank summary")
    if len(probe_tree) != plan.expected_blocks:
        raise EvidenceError("mesh_structure.dat repeats a logical location")
    if probe_tree != tree:
        raise EvidenceError("the mesh probe tree is not the MeshBlock tree of the run's own native restart dumps")
    if sorted(report["blocks_per_level"]) != list(plan.expected_levels):
        raise EvidenceError("mesh probe refinement levels differ from the rubric")
    return {
        "root_grid": report["root_grid"], "total_blocks": report["total_blocks"], "ranks": report["ranks"],
        "blocks_per_rank": {str(rank): count for rank, count in sorted(report["blocks_per_rank"].items())},
        "blocks_per_level": {str(level): count for level, count in sorted(report["blocks_per_level"].items())},
        "root_level": report["root_level"],
        "evidence": "pinned Mesh::OutputMeshStructure under the run's own binary, deck, overrides and launcher",
    }


# ----------------------------------------------------------------- restart parameter dump
def check_parameters(param_text: str, plan: RunPlan) -> dict[str, str]:
    """Bind the executed configuration to the rubric from the native dump.

    Every ``.rst`` file embeds ``ParameterInput::ParameterDump`` of the running
    parameter set, so the mesh, MeshBlock, refinement, thread count, output
    layout, time limit and every rubric selector can be read back from the
    native bytes rather than trusted from the runner's argv record.
    """
    found: dict[str, str] = {}
    for key, (kind, expected) in plan.deck_parameters().items():
        block, name = key.split("/", 1)
        try:
            actual = block_param(param_text, block, name)
        except RestartFormatError as exc:
            raise EvidenceError(f"native parameter dump lacks {key}: {exc}") from exc
        found[key] = actual
        if kind == "int":
            ok = actual.isdigit() and int(actual) == int(expected)
        elif kind == "float":
            try:
                ok = float(actual) == float(expected)
            except ValueError:
                ok = False
        else:
            ok = actual == expected
        if not ok:
            raise EvidenceError(f"native parameter dump has {key}={actual!r}, the rubric requires {expected!r}")
    return found
