"""Verifier-owned provenance: bind native Athena++ output, decks, builds and receipts.

Nothing in a submitted receipt is taken as true.  The verifier locates native
output by convention (``<root>/.runs/<folder>/<subcase>/``), parses every
MeshBlock TAB file itself, proves that the cells tile the public deck's domain
exactly once at the declared refinement levels, binds every frame to the exact
printed time token the check owns, re-derives the compared artifact from the
native tabs, binds the ``-n`` parameter dump to the deck, parses the raw
Athena++ stdout/stderr for a source-backed *normal* completion, and recomputes
every hash the receipt claims (deck, binaries, build trees, native files, all
raw logs, artifact).

What this can and cannot establish
----------------------------------
These are *consistency bindings* between check-owned inputs and the raw bytes
present in one output root, plus role separation between the trusted oracle
role and a candidate role.  They are not cryptographic authentication: nothing
in a package can prove that arbitrary candidate code, rather than a replay,
produced a root.  Trusted execution evidence has to come from the orchestrator
that actually ran the containers (``solution/solve.sh`` host receipts, the
Docker daemon's image/container identities, and the grader's own run records).
The verifier therefore refuses to call anything a real execution unless the
role, the evidence class and the host-side Docker receipt all agree, and it
rejects roots that are marked as synthetic verifier fixtures outright.
"""
from __future__ import annotations

import hashlib
import json
import math
import re
from pathlib import Path
from typing import Any, Callable, Iterable

from athinput import (
    DeckGeometry, TAB_TIME_FORMAT_PRECISION, bind_pardump, data_format_precision, deck_geometry,
    parse_athinput, parse_pardump,
)
from extract_tab import TabFile, derive_document, parse_tab

RECEIPT_SCHEMA = "athena-hydro-execution/v3"
DOCKER_RECEIPT_SCHEMA = "athena-hydro-docker-run/v2"
SOURCE_IDENTITY_FILE = Path(__file__).with_name("source_identity.json")
RUN_DIR = ".runs"
NATIVE_LOGS = (
    "pardump.stdout.log", "pardump.stderr.log",
    "athena.stdout.log", "athena.stderr.log",
    "extractor.stdout.log", "extractor.stderr.log",
)
BUILD_LOGS = ("configure_stdout", "configure_stderr", "make_stdout", "make_stderr")
HEX64 = re.compile(r"^[0-9a-f]{64}$")
NGHOST4_TOKENS = {"3", "3c", "3f", "4", "4c", "wenoz", "wenomz"}
PROBLEM_BY_KIND = {"wave": "linear_wave", "sod": "shock_tube", "contact": "shock_tube", "quirk": "quirk"}

# Roles and evidence classes.  A root's role decides which contract it must
# satisfy; its evidence class decides whether it may be called a real execution.
ROLE_ORACLE = "reference-oracle"
ROLE_CANDIDATE = "candidate"
ROLES = (ROLE_ORACLE, ROLE_CANDIDATE)
EVIDENCE_DOCKER = "docker-oracle-run"
EVIDENCE_HOST_NATIVE = "host-native-oracle-run"
EVIDENCE_CANDIDATE = "candidate-run"
EVIDENCE_SYNTHETIC = "synthetic-fixture"
EVIDENCE_CLASSES = (EVIDENCE_DOCKER, EVIDENCE_HOST_NATIVE, EVIDENCE_CANDIDATE, EVIDENCE_SYNTHETIC)
SYNTHETIC_MARKER = ".synthetic-fixture"

# Source-backed Athena++ terminal semantics (code/athena/src/main.cpp:449,614-647
# and src/defs.hpp.in:155-158).  Fatal exceptions are caught in main() and
# return 0, so the exit status alone proves nothing; the raw log must show one
# normal time-limit termination and no fatal signature.
SETUP_BANNER = "Setup complete, entering main loop..."
NORMAL_ENDING = "Terminating on time limit"
ABNORMAL_ENDINGS = (
    "Terminating on Terminate signal",
    "Terminating on Interrupt signal",
    "Terminating on wall-time limit",
    "Terminating on cycle limit",
)
FATAL_RE = re.compile(r"#{3,}\s*FATAL ERROR")
TIME_CYCLE_RE = re.compile(r"^time=\s*(?P<time>\S+)\s+cycle=\s*(?P<cycle>-?\d+)\s*$")
TLIM_NLIM_RE = re.compile(r"^tlim=\s*(?P<tlim>\S+)\s+nlim=\s*(?P<nlim>-?\d+)\s*$")
ZONE_CYCLES_RE = re.compile(r"^zone-cycles\s*=\s*(?P<zone_cycles>\d+)\s*$")
CPU_TIME_RE = re.compile(r"^cpu time used\s*=\s*(?P<cpu>\S+)\s*$")
ZC_PER_SECOND_RE = re.compile(r"^zone-cycles/cpu_second\s*=\s*(?P<rate>\S+)\s*$")
# std::cout's default float format keeps six significant digits (main.cpp:625,626
# with time/ncycle_out=0); with cycle diagnostics the stream is left in
# scientific mode with 16 digits.  The looser of the two governs the bound at
# which a printed terminal time may be compared with the deck's tlim.
OSTREAM_DEFAULT_SIGNIFICANT_DIGITS = 6
COMPLETION_TIME_REL_BOUND = 10.0 ** (-(OSTREAM_DEFAULT_SIGNIFICANT_DIGITS - 1))

# Files configure.py/make add inside the build copy of the pinned source tree.
GENERATED_BUILD_FILES = frozenset({"Makefile", "src/defs.hpp", "configure.log"})
GENERATED_BUILD_DIRS = ("obj/", "bin/")
EXECUTABLE_MAGICS = (
    b"\x7fELF",                       # ELF
    b"\xcf\xfa\xed\xfe", b"\xce\xfa\xed\xfe",  # Mach-O 64/32 little endian
    b"\xfe\xed\xfa\xcf", b"\xfe\xed\xfa\xce",  # Mach-O big endian
    b"\xca\xfe\xba\xbe",              # Mach-O universal
)
MIN_BINARY_BYTES = 65536
CONFIGURE_SUMMARY_LINE = "Your Athena++ distribution has now been configured with the following options"
# Every physics-relevant configure.py summary line the CPU oracle contract fixes
# (code/athena/configure.py:1000-1039).  A substituted or extra physics flag
# changes one of these values, so they are bound exactly.
FIXED_CONFIGURE_OPTIONS = {
    "Coordinate system": "cartesian",
    "Equation of state": "adiabatic",
    "Magnetic fields": "OFF",
    "Number of scalars": "0",
    "Number of chemical species": "0",
    "Special relativity": "OFF",
    "General relativity": "OFF",
    "Radiative Transfer": "OFF",
    "Implicit Radiation": "OFF",
    "Cosmic Ray Transport": "OFF",
    "Cosmic Ray Diffusion": "OFF",
    "Frame transformations": "OFF",
    "Self-Gravity": "OFF",
    "Super-Time-Stepping": "OFF",
    "Chemistry": "OFF",
    "Debug flags": "OFF",
    "Code coverage flags": "OFF",
    "Floating-point precision": "double",
    "MPI parallelism": "OFF",
    "OpenMP parallelism": "OFF",
    "FFT": "OFF",
    "HDF5 output": "OFF",
}
MAKE_LOG_REQUIRED = ("-c src/main.cpp", "-o bin/athena")


class ProvenanceError(ValueError):
    """Raised when evidence does not bind; the caller treats it as fail-closed."""


# ---------------------------------------------------------------------------
# hashing and safe path handling
# ---------------------------------------------------------------------------

def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def tree_digest(root: Path, skip: Callable[[str], bool] | None = None) -> tuple[str, int, int]:
    """Content digest of a source tree: sorted relative path + sha256 of every regular file."""
    if root.is_symlink() or not root.is_dir():
        raise ProvenanceError(f"source tree is not a real directory: {root}")
    digest = hashlib.sha256()
    files = 0
    total = 0
    for path in sorted(root.rglob("*"), key=lambda item: item.relative_to(root).as_posix()):
        relative = path.relative_to(root).as_posix()
        if skip is not None and skip(relative):
            continue
        if path.is_symlink():
            raise ProvenanceError(f"source tree contains a symlink: {relative}")
        if not path.is_file():
            continue
        digest.update(relative.encode("utf-8"))
        digest.update(b"\0")
        digest.update(sha256_file(path).encode("ascii"))
        digest.update(b"\n")
        files += 1
        total += path.stat().st_size
    return digest.hexdigest(), files, total


def _generated_build_path(relative: str) -> bool:
    return relative in GENERATED_BUILD_FILES or relative.startswith(GENERATED_BUILD_DIRS)


_BUILD_TREE_DIGESTS: dict[str, tuple[str, int, int]] = {}


def build_tree_digest(tree: Path) -> tuple[str, int, int]:
    """Digest of a build copy of the pinned source, ignoring configure/make products.

    Memoized per absolute path for the lifetime of one verifier process: the same
    shared build tree backs many subcases and the roots do not change while the
    verifier runs.
    """
    key = str(tree.resolve())
    if key not in _BUILD_TREE_DIGESTS:
        _BUILD_TREE_DIGESTS[key] = tree_digest(tree, skip=_generated_build_path)
    return _BUILD_TREE_DIGESTS[key]


def load_source_identity() -> dict[str, Any]:
    document = json.loads(SOURCE_IDENTITY_FILE.read_text(encoding="utf-8"))
    for key in ("commit", "tree_sha256", "file_count", "byte_count"):
        if key not in document:
            raise ProvenanceError(f"source identity lacks {key}")
    return document


def safe_child(root: Path, value: object, *, kind: str, directory: bool = False) -> Path:
    if not isinstance(value, str) or not value or Path(value).is_absolute():
        raise ProvenanceError(f"{kind} must be a relative path")
    relative = Path(value)
    if ".." in relative.parts or "." in relative.parts:
        raise ProvenanceError(f"{kind} escapes its root")
    current = root
    for part in relative.parts:
        current = current / part
        if current.is_symlink():
            raise ProvenanceError(f"{kind} contains a symlink: {value}")
    if directory:
        if not current.is_dir():
            raise ProvenanceError(f"{kind} is not a directory: {value}")
    elif not current.is_file():
        raise ProvenanceError(f"{kind} is not a regular file: {value}")
    return current


def root_is_synthetic(root: Path) -> bool:
    """True when a root carries the generator's synthetic-fixture marker."""
    marker = root / SYNTHETIC_MARKER
    return marker.is_file() or marker.is_symlink()


# ---------------------------------------------------------------------------
# strict JSON
# ---------------------------------------------------------------------------

def _pairs(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ProvenanceError(f"duplicate JSON key {key!r}")
        result[key] = value
    return result


def _constant(value: str) -> None:
    raise ProvenanceError(f"non-standard JSON constant {value}")


def load_strict_json(path: Path) -> Any:
    if path.is_symlink() or not path.is_file():
        raise ProvenanceError(f"{path.name} is missing or not a regular file")
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream, object_pairs_hook=_pairs, parse_constant=_constant)


# ---------------------------------------------------------------------------
# build selection and canonical commands
# ---------------------------------------------------------------------------

def build_key(spec: dict[str, Any]) -> tuple[str, str, int]:
    """Shared (problem, Riemann solver, NGHOST) build selection for one subcase."""
    problem = PROBLEM_BY_KIND.get(spec.get("kind", "wave"), "linear_wave")
    solver = str(spec.get("solver", "hllc"))
    nghost = 4 if str(spec.get("xorder", "2")) in NGHOST4_TOKENS else 2
    return problem, solver, nghost


def canonical_configure_argv(spec: dict[str, Any]) -> list[str]:
    """The one allowed configure.py argv (after the interpreter) for a build key."""
    problem, solver, nghost = build_key(spec)
    return ["configure.py", f"--prob={problem}", "--coord=cartesian", f"--flux={solver}",
            f"--nghost={nghost}", "--cflag=-O2 -g0"]


def canonical_make_argv(jobs: int) -> list[str]:
    return ["make", f"-j{int(jobs)}"]


def _interpreter_prefix(command: list[Any]) -> tuple[list[str], list[str]]:
    """Split ``[python, -B, configure.py, ...]`` into (interpreter tokens, argv)."""
    tokens = [str(item) for item in command]
    for index, token in enumerate(tokens):
        if Path(token).name == "configure.py":
            return tokens[:index], [Path(token).name] + tokens[index + 1:]
    raise ProvenanceError("configure command does not invoke configure.py")


def check_configure_command(command: object, spec: dict[str, Any]) -> None:
    if not isinstance(command, list) or not command:
        raise ProvenanceError("build configure_command is not an argv list")
    prefix, argv = _interpreter_prefix(command)
    if not prefix or not Path(prefix[0]).name.startswith("python") or "-B" not in prefix:
        raise ProvenanceError("build configure_command must run configure.py through python -B")
    expected = canonical_configure_argv(spec)
    if argv != expected:
        raise ProvenanceError(f"build configure argv {argv} is not the canonical {expected}")


def check_make_command(command: object, jobs: object) -> None:
    if not isinstance(command, list) or not command:
        raise ProvenanceError("build make_command is not an argv list")
    tokens = [str(item) for item in command]
    if not isinstance(jobs, int) or isinstance(jobs, bool) or jobs <= 0:
        raise ProvenanceError("build receipt make_jobs is not a positive integer")
    if tokens != canonical_make_argv(jobs):
        raise ProvenanceError(f"build make argv {tokens} is not the canonical {canonical_make_argv(jobs)}")


def parse_configure_log(text: str) -> dict[str, str]:
    if CONFIGURE_SUMMARY_LINE not in text:
        raise ProvenanceError("configure log is not the pinned configure.py summary output")
    options: dict[str, str] = {}
    for line in text.splitlines():
        if not line.startswith("  ") or ":" not in line:
            continue
        name, _, value = line.partition(":")
        options[name.strip()] = value.strip()
    return options


def check_configure_log(text: str, spec: dict[str, Any]) -> None:
    """The pinned configure.py summary must describe exactly this build key."""
    problem, solver, nghost = build_key(spec)
    options = parse_configure_log(text)
    required = dict(FIXED_CONFIGURE_OPTIONS)
    required.update({
        "Problem generator": problem,
        "Riemann solver": solver,
        "Number of ghost cells": str(nghost),
    })
    for name, value in required.items():
        if name not in options:
            raise ProvenanceError(f"configure log lacks the {name!r} option line")
        if options[name] != value:
            raise ProvenanceError(f"configure log reports {name}={options[name]!r}, contract requires {value!r}")


def check_make_log(text: str) -> None:
    for needle in MAKE_LOG_REQUIRED:
        if needle not in text:
            raise ProvenanceError(f"make log does not contain the expected build step {needle!r}")


def check_binary_format(path: Path) -> int:
    with path.open("rb") as stream:
        head = stream.read(8)
    if not any(head.startswith(magic) for magic in EXECUTABLE_MAGICS):
        raise ProvenanceError(f"{path.name} is not a native executable object file")
    size = path.stat().st_size
    if size < MIN_BINARY_BYTES:
        raise ProvenanceError(f"{path.name} is {size} bytes, too small to be a compiled Athena++ binary")
    return size


# ---------------------------------------------------------------------------
# decks, mesh structure and representation-derived bounds
# ---------------------------------------------------------------------------

def resolve_deck(checks_root: Path, folder: str, rubric: dict[str, Any]) -> tuple[Path, str]:
    """Resolve rubric ``check_deck`` relative to the check folder; it must stay inside tests/checks."""
    value = rubric.get("check_deck")
    if not isinstance(value, str) or not value or Path(value).is_absolute():
        raise ProvenanceError("rubric check_deck must be a relative path")
    checks_real = checks_root.resolve()
    deck = (checks_root / folder / value).resolve()
    try:
        relative = deck.relative_to(checks_real)
    except ValueError as exc:
        raise ProvenanceError("rubric check_deck escapes tests/checks") from exc
    if deck.is_symlink() or not deck.is_file():
        raise ProvenanceError(f"check deck is not a regular file: {value}")
    return deck, relative.as_posix()


def root_leaf_blocks(geometry: DeckGeometry) -> list[tuple[int, int, int, int]]:
    nb = geometry.root_blocks
    return [(0, i, j, k) for k in range(nb[2]) for j in range(nb[1]) for i in range(nb[0])]


def leaf_blocks(rubric: dict[str, Any], geometry: DeckGeometry) -> list[tuple[int, int, int, int]]:
    mesh = rubric.get("mesh")
    if not isinstance(mesh, dict):
        raise ProvenanceError("rubric lacks mesh declaration")
    if list(mesh.get("root", [])) != list(geometry.root):
        raise ProvenanceError("rubric mesh.root does not match the deck")
    if list(mesh.get("meshblock", [])) != list(geometry.meshblock):
        raise ProvenanceError("rubric mesh.meshblock does not match the deck")
    if mesh.get("refinement") != geometry.refinement:
        raise ProvenanceError("rubric mesh.refinement does not match the deck")
    declared = mesh.get("leaf_blocks")
    if geometry.refinement == "none":
        if declared is not None:
            raise ProvenanceError("unrefined rubric must not list leaf_blocks")
        blocks = root_leaf_blocks(geometry)
    else:
        if not isinstance(declared, list) or not declared:
            raise ProvenanceError("static-refinement rubric must list its real leaf_blocks")
        blocks = []
        for item in declared:
            if not isinstance(item, list) or len(item) != 4 or any(isinstance(v, bool) or not isinstance(v, int) or v < 0 for v in item):
                raise ProvenanceError("rubric leaf_blocks entries must be [level, lx1, lx2, lx3]")
            blocks.append((item[0], item[1], item[2], item[3]))
        if len(set(blocks)) != len(blocks):
            raise ProvenanceError("rubric leaf_blocks contains duplicates")
    if mesh.get("leaf_block_count") != len(blocks):
        raise ProvenanceError("rubric mesh.leaf_block_count does not match its block list")
    return blocks


def expected_rows(rubric: dict[str, Any], geometry: DeckGeometry, blocks: list[tuple[int, int, int, int]]) -> int:
    rows = len(blocks) * geometry.cells_per_block
    declared = rubric.get("expected_rows", math.prod(rubric["dimensions"]))
    if isinstance(declared, bool) or not isinstance(declared, int) or declared != rows:
        raise ProvenanceError(f"rubric expected_rows {declared!r} does not match {len(blocks)} blocks x {geometry.cells_per_block} cells")
    return rows


def geometry_tolerance(geometry: DeckGeometry, max_level: int) -> float:
    """Check-owned geometric slack, derived from the deck itself.

    Cell centres are printed with the deck's ``data_format``: a value carries at
    most ``0.5 * 10**-precision * |value|`` of rounding error, and a geometric
    comparison combines at most four printed coordinates, so the representation
    bound is ``2 * 10**-precision * scale``.  The comparisons only have to
    resolve grid positions, so the verifier uses a quarter of the finest cell
    width and refuses any deck whose print precision cannot support that.
    """
    precision = data_format_precision(geometry.tab.data_format)
    scale = max([1.0] + [abs(value) for value in geometry.xmin + geometry.xmax])
    representation = 2.0 * (10.0 ** (-precision)) * scale
    spacings = [geometry.dx[axis] / (2 ** max_level) for axis in range(3) if geometry.root[axis] > 1]
    if not spacings:
        raise ProvenanceError("deck has no resolved axis to bind geometry against")
    finest = min(spacings)
    if representation > 0.05 * finest:
        raise ProvenanceError(
            f"deck data_format {geometry.tab.data_format!r} prints coordinates too coarsely "
            f"({representation:g}) to bind a {finest:g} cell width")
    return 0.25 * finest


# ---------------------------------------------------------------------------
# raw Athena++ log semantics
# ---------------------------------------------------------------------------

def _reject_fatal(text: str, *, label: str) -> None:
    if FATAL_RE.search(text):
        raise ProvenanceError(f"{label} contains an Athena++ fatal-error signature (pinned main.cpp returns 0 on these paths)")


def verify_pardump_log(stdout_text: str, stderr_text: str) -> None:
    _reject_fatal(stdout_text, label="parameter-dump stdout")
    _reject_fatal(stderr_text, label="parameter-dump stderr")
    for ending in ABNORMAL_ENDINGS + (NORMAL_ENDING,):
        if ending in stdout_text:
            raise ProvenanceError("parameter-dump run must exit after the dump, not run the integrator")


def verify_completion(stdout_text: str, stderr_text: str, geometry: DeckGeometry, params: dict[str, dict[str, str]]) -> dict[str, Any]:
    """Require the exact normal terminal record pinned Athena++ prints for these decks."""
    _reject_fatal(stdout_text, label="athena stdout")
    _reject_fatal(stderr_text, label="athena stderr")
    for ending in ABNORMAL_ENDINGS:
        if ending in stdout_text or ending in stderr_text:
            raise ProvenanceError(f"athena log reports an abnormal ending: {ending}")
    lines = stdout_text.splitlines()
    if sum(1 for line in lines if line.strip() == SETUP_BANNER) != 1:
        raise ProvenanceError("athena stdout does not contain exactly one setup banner")
    endings = [index for index, line in enumerate(lines) if line.strip() == NORMAL_ENDING]
    if len(endings) != 1:
        raise ProvenanceError(f"athena stdout has {len(endings)} normal terminations, expected exactly one")
    banner = next(index for index, line in enumerate(lines) if line.strip() == SETUP_BANNER)
    if endings[0] < banner:
        raise ProvenanceError("athena stdout terminates before the main loop starts")
    tail = [line.strip() for line in lines[endings[0] + 1:]]
    fields: dict[str, Any] = {}
    for pattern, keys in (
        (TIME_CYCLE_RE, ("time", "cycle")),
        (TLIM_NLIM_RE, ("tlim", "nlim")),
        (ZONE_CYCLES_RE, ("zone_cycles",)),
        (CPU_TIME_RE, ("cpu",)),
        (ZC_PER_SECOND_RE, ("rate",)),
    ):
        match = next((pattern.match(line) for line in tail if pattern.match(line)), None)
        if match is None:
            raise ProvenanceError("athena stdout lacks the complete normal completion diagnostics")
        for key in keys:
            fields[key] = match.group(key)
    try:
        final_time = float(fields["time"])
        final_cycle = int(fields["cycle"])
        reported_tlim = float(fields["tlim"])
        reported_nlim = int(fields["nlim"])
        zone_cycles = int(fields["zone_cycles"])
        cpu_time = float(fields["cpu"])
        rate = float(fields["rate"])
    except ValueError as exc:
        raise ProvenanceError(f"athena completion diagnostics are not numeric: {exc}") from exc
    if not all(math.isfinite(value) for value in (final_time, reported_tlim, cpu_time, rate)):
        raise ProvenanceError("athena completion diagnostics are not finite")
    bound = COMPLETION_TIME_REL_BOUND * max(1.0, abs(geometry.tlim))
    if abs(final_time - geometry.tlim) > bound or abs(reported_tlim - geometry.tlim) > bound:
        raise ProvenanceError(
            f"athena terminated at time={final_time!r}/tlim={reported_tlim!r}, which is not the deck tlim {geometry.tlim!r}")
    deck_nlim = int(float(params.get("time", {}).get("nlim", "-1")))
    if reported_nlim != deck_nlim:
        raise ProvenanceError(f"athena reports nlim={reported_nlim}, deck declares {deck_nlim}")
    if final_cycle < 1:
        raise ProvenanceError("athena reports a non-positive final cycle for a time-limited run")
    if deck_nlim >= 0 and final_cycle >= deck_nlim:
        raise ProvenanceError("athena reached the deck cycle limit instead of the time limit")
    if zone_cycles <= 0 or cpu_time <= 0.0 or rate <= 0.0:
        raise ProvenanceError("athena completion diagnostics report non-positive work")
    return {
        "final_time": final_time,
        "final_cycle": final_cycle,
        "zone_cycles": zone_cycles,
        "cpu_time_seconds": cpu_time,
        "termination": NORMAL_ENDING,
    }


# ---------------------------------------------------------------------------
# native output binding
# ---------------------------------------------------------------------------

def _axis_values(tab: TabFile, axis: int) -> list[float] | None:
    name = f"x{axis + 1}v"
    if name not in tab.columns:
        return None
    return sorted({row[axis] for row in tab.rows})


def _block_identity(tab: TabFile, geometry: DeckGeometry, max_level: int, tolerance: float) -> tuple[int, int, int, int]:
    level: int | None = None
    logical = [0, 0, 0]
    for axis in range(3):
        n = geometry.meshblock[axis]
        values = _axis_values(tab, axis)
        if n == 1:
            if values is not None:
                raise ProvenanceError(f"{tab.path.name}: axis {axis + 1} column present for a one-cell block")
            if geometry.root[axis] != 1:
                raise ProvenanceError(f"{tab.path.name}: cannot locate a one-cell block along a multi-cell root axis {axis + 1}")
            continue
        if values is None:
            raise ProvenanceError(f"{tab.path.name}: missing coordinate column x{axis + 1}v")
        if len(values) != n:
            raise ProvenanceError(f"{tab.path.name}: axis {axis + 1} has {len(values)} distinct centres, expected {n}")
        spacing = (values[-1] - values[0]) / (n - 1)
        for previous, current in zip(values, values[1:]):
            if abs((current - previous) - spacing) > tolerance:
                raise ProvenanceError(f"{tab.path.name}: axis {axis + 1} cell centres are not uniformly spaced")
        dx = geometry.dx[axis]
        ratio = dx / spacing
        axis_level = int(round(math.log2(ratio))) if ratio > 0 else -1
        if axis_level < 0 or abs(dx / (2 ** axis_level) - spacing) > tolerance:
            raise ProvenanceError(f"{tab.path.name}: axis {axis + 1} spacing is not root dx / 2^level")
        if level is None:
            level = axis_level
        elif level != axis_level:
            raise ProvenanceError(f"{tab.path.name}: refinement level differs between axes")
        width = n * spacing
        start = values[0] - spacing / 2.0 - geometry.xmin[axis]
        index = start / width
        if abs(index - round(index)) * width > tolerance:
            raise ProvenanceError(f"{tab.path.name}: axis {axis + 1} block is not aligned to the level grid")
        logical[axis] = int(round(index))
        end = values[-1] + spacing / 2.0
        if end > geometry.xmax[axis] + tolerance or values[0] - spacing / 2.0 < geometry.xmin[axis] - tolerance:
            raise ProvenanceError(f"{tab.path.name}: axis {axis + 1} block lies outside the mesh")
    if level is None:
        raise ProvenanceError(f"{tab.path.name}: no coordinate axis to locate the block")
    if level > max_level:
        raise ProvenanceError(f"{tab.path.name}: level {level} exceeds the declared maximum {max_level}")
    if len(tab.rows) != geometry.cells_per_block:
        raise ProvenanceError(f"{tab.path.name}: {len(tab.rows)} rows, expected {geometry.cells_per_block}")
    return level, logical[0], logical[1], logical[2]


def _coverage(tabs: list[TabFile], geometry: DeckGeometry, max_level: int, identities: dict[int, tuple[int, int, int, int]], tolerance: float) -> None:
    fine = [geometry.root[axis] * (2 ** max_level) for axis in range(3)]
    marks = bytearray(fine[0] * fine[1] * fine[2])
    for tab in tabs:
        level = identities[tab.gid][0]
        span = 2 ** (max_level - level)
        spacing = [geometry.dx[axis] / (2 ** level) for axis in range(3)]
        hf = [geometry.dx[axis] / (2 ** max_level) for axis in range(3)]
        for row in tab.rows:
            start = []
            for axis in range(3):
                if geometry.root[axis] == 1:
                    start.append(0)
                    continue
                offset = (row[axis] - spacing[axis] / 2.0 - geometry.xmin[axis]) / hf[axis]
                index = int(round(offset))
                if abs(offset - index) * hf[axis] > tolerance or index < 0 or index + span > fine[axis]:
                    raise ProvenanceError(f"{tab.path.name}: cell centre off the fine grid on axis {axis + 1}")
                start.append(index)
            spans = [1 if geometry.root[axis] == 1 else span for axis in range(3)]
            for k in range(start[2], start[2] + spans[2]):
                for j in range(start[1], start[1] + spans[1]):
                    base = (k * fine[1] + j) * fine[0] + start[0]
                    for offset in range(spans[0]):
                        if marks[base + offset]:
                            raise ProvenanceError(f"{tab.path.name}: a cell is covered twice")
                        marks[base + offset] = 1
    if any(mark == 0 for mark in marks):
        raise ProvenanceError("native MeshBlocks do not cover the whole mesh")


def _schedule_tokens(rubric: dict[str, Any]) -> list[str]:
    tokens = rubric.get("expected_time_tokens")
    times = rubric.get("expected_times")
    if not isinstance(tokens, list) or not isinstance(times, list) or len(tokens) != len(times):
        raise ProvenanceError("rubric lacks a printed-token frame schedule")
    for token, value in zip(tokens, times):
        if not isinstance(token, str) or not token:
            raise ProvenanceError("rubric frame token is malformed")
        if f"%.{TAB_TIME_FORMAT_PRECISION}e" % float(value) != token:
            raise ProvenanceError(f"rubric frame token {token!r} is not the %e rendering of {value!r}")
    return tokens


def verify_native_run(run_dir: Path, deck_path: Path, rubric: dict[str, Any], spec: dict[str, Any], *,
                      require_athena_completion: bool = True) -> dict[str, Any]:
    """Parse and bind one subcase's native output; return derived evidence."""
    if run_dir.is_symlink() or not run_dir.is_dir():
        raise ProvenanceError(f"native run directory missing: {run_dir}")
    entries = sorted(run_dir.iterdir())
    for entry in entries:
        if entry.is_symlink() or not entry.is_file():
            raise ProvenanceError(f"native run directory contains a non-regular entry: {entry.name}")
    names = {entry.name for entry in entries}
    for log in NATIVE_LOGS:
        if log not in names:
            raise ProvenanceError(f"native run lacks {log}")
    log_hashes = {name: sha256_file(run_dir / name) for name in NATIVE_LOGS}
    params = parse_athinput(deck_path.read_text(encoding="utf-8"))
    geometry = deck_geometry(params)
    if list(rubric["dimensions"]) != list(geometry.root):
        raise ProvenanceError("rubric dimensions do not match the deck root mesh")
    blocks = leaf_blocks(rubric, geometry)
    rows = expected_rows(rubric, geometry, blocks)
    max_level = max(block[0] for block in blocks)
    declared = set(blocks)
    tolerance = geometry_tolerance(geometry, max_level)

    pardump_stdout = (run_dir / "pardump.stdout.log").read_text(encoding="utf-8", errors="replace")
    pardump_stderr = (run_dir / "pardump.stderr.log").read_text(encoding="utf-8", errors="replace")
    verify_pardump_log(pardump_stdout, pardump_stderr)
    dump = parse_pardump(pardump_stdout)
    bind_pardump(params, dump)
    floor = rubric.get("floor_witness")
    if isinstance(floor, dict):
        hydro = dump.get("hydro", {})
        for key in ("dfloor", "pfloor"):
            if float(hydro.get(key, "nan")) != float(floor[key]):
                raise ProvenanceError(f"parameter dump hydro/{key} does not match the rubric floor witness")

    athena_stdout = (run_dir / "athena.stdout.log").read_text(encoding="utf-8", errors="replace")
    athena_stderr = (run_dir / "athena.stderr.log").read_text(encoding="utf-8", errors="replace")
    completion: dict[str, Any] | None = None
    if require_athena_completion:
        completion = verify_completion(athena_stdout, athena_stderr, geometry, params)
    else:
        # A candidate port need not reproduce Athena++'s diagnostics, but it may
        # not present a run whose own log shows a fatal or abnormal ending.
        _reject_fatal(athena_stdout, label="candidate run stdout")
        _reject_fatal(athena_stderr, label="candidate run stderr")
        for ending in ABNORMAL_ENDINGS:
            if ending in athena_stdout or ending in athena_stderr:
                raise ProvenanceError(f"candidate run log reports an abnormal ending: {ending}")

    tab_paths = [entry for entry in entries if entry.suffix == ".tab"]
    if not tab_paths:
        raise ProvenanceError("native run has no TAB output")
    tabs: list[TabFile] = []
    for path in tab_paths:
        tab = parse_tab(path)
        if tab.base != geometry.problem_id or tab.out != geometry.tab.file_id:
            raise ProvenanceError(f"{path.name}: filename does not match the deck problem_id/output block")
        tabs.append(tab)
    frames: dict[int, list[TabFile]] = {}
    for tab in tabs:
        frames.setdefault(tab.frame, []).append(tab)
    expected_times = rubric["expected_times"]
    tokens = _schedule_tokens(rubric)
    if sorted(frames) != list(range(len(expected_times))):
        raise ProvenanceError(f"native frames {sorted(frames)} do not match the {len(expected_times)}-frame schedule")
    identities: dict[int, tuple[int, int, int, int]] = {}
    for frame_number, members in sorted(frames.items()):
        gids = sorted(tab.gid for tab in members)
        if gids != list(range(len(blocks))):
            raise ProvenanceError(f"frame {frame_number:05d} has MeshBlocks {gids[:8]}..., expected 0..{len(blocks) - 1}")
        for tab in members:
            # The schedule is bound to the exact token Athena++ printed, not to a
            # tolerance: "%e" is the whole representation the check owns.
            if tab.time_token != tokens[frame_number] or tab.time != float(expected_times[frame_number]):
                raise ProvenanceError(
                    f"{tab.path.name}: printed time {tab.time_token!r} is not the check-owned frame token {tokens[frame_number]!r}")
            identity = _block_identity(tab, geometry, max_level, tolerance)
            if frame_number == 0:
                identities[tab.gid] = identity
            elif identities.get(tab.gid) != identity:
                raise ProvenanceError(f"{tab.path.name}: MeshBlock geometry changed between frames")
        if frame_number == 0:
            found = set(identities.values())
            if found != declared:
                missing = sorted(declared - found)[:4]
                extra = sorted(found - declared)[:4]
                raise ProvenanceError(f"MeshBlock set differs from the declared real structure (missing {missing}, extra {extra})")
        _coverage(members, geometry, max_level, identities, tolerance)
    document = derive_document(tabs, rubric["case"], list(geometry.root), rows)
    return {
        "geometry": geometry,
        "deck_params": params,
        "leaf_blocks": blocks,
        "expected_rows": rows,
        "geometry_tolerance": tolerance,
        "native_files": {path.name: sha256_file(path) for path in tab_paths},
        "log_files": log_hashes,
        "completion": completion,
        "document": document,
        "frame_cycles": [document["frames"][index]["cycle"] for index in range(len(document["frames"]))],
    }


# ---------------------------------------------------------------------------
# receipt binding
# ---------------------------------------------------------------------------

def _relative_suffix(value: object, expected: str) -> bool:
    return isinstance(value, str) and (value == expected or value.endswith("/" + expected))


def _check_extract_command(command: object, *, name: str, spec: dict[str, Any], rows: int, run_dir: str, artifact: str) -> None:
    if not isinstance(command, list) or len(command) < 4:
        raise ProvenanceError("record extract_command is not an argv list")
    tokens = [str(item) for item in command]
    script_index = next((index for index, token in enumerate(tokens) if Path(token).name == "extract_tab.py"), None)
    if script_index is None:
        raise ProvenanceError("record extract_command does not invoke extract_tab.py")
    prefix, argv = tokens[:script_index], tokens[script_index + 1:]
    if not prefix or not Path(prefix[0]).name.startswith("python") or "-B" not in prefix:
        raise ProvenanceError("record extract_command must run the extractor through python -B")
    if len(argv) != 10:
        raise ProvenanceError("record extract_command does not carry the exact extractor argument set")
    flags = dict(zip(argv[0::2], argv[1::2]))
    if set(flags) != {"--input", "--output", "--case", "--dimensions", "--expected-rows"}:
        raise ProvenanceError("record extract_command flags are not the canonical extractor flags")
    if not (flags["--input"] == run_dir or flags["--input"].endswith("/" + run_dir)):
        raise ProvenanceError("record extract_command does not read this subcase's native run directory")
    output_name = Path(flags["--output"]).name
    if not (output_name == Path(artifact).name or output_name.startswith(".primitive_tab.json.")):
        raise ProvenanceError("record extract_command does not write this subcase's artifact")
    if flags["--case"] != name:
        raise ProvenanceError("record extract_command case does not name this subcase")
    if flags["--dimensions"] != ",".join(str(value) for value in spec["dimensions"]):
        raise ProvenanceError("record extract_command dimensions are not the contract dimensions")
    if flags["--expected-rows"] != str(rows):
        raise ProvenanceError("record extract_command expected-rows is not the contract row count")


def _bind_logs(root: Path, record: dict[str, Any], run_dir: str, evidence: dict[str, Any]) -> None:
    for key, expected in (
        ("pardump_stdout", f"{run_dir}/pardump.stdout.log"), ("pardump_stderr", f"{run_dir}/pardump.stderr.log"),
        ("athena_stdout", f"{run_dir}/athena.stdout.log"), ("athena_stderr", f"{run_dir}/athena.stderr.log"),
        ("extractor_stdout", f"{run_dir}/extractor.stdout.log"), ("extractor_stderr", f"{run_dir}/extractor.stderr.log"),
    ):
        if record.get(key) != expected:
            raise ProvenanceError(f"record {key} does not name the conventional log path")
        safe_child(root, expected, kind=key)
    logs = record.get("log_files")
    if not isinstance(logs, list) or len(logs) != len(NATIVE_LOGS):
        raise ProvenanceError("record does not hash-bind every raw log used for acceptance")
    claimed: dict[str, Any] = {}
    for item in logs:
        if not isinstance(item, dict) or not isinstance(item.get("path"), str) or not item["path"].startswith(run_dir + "/"):
            raise ProvenanceError("record log inventory entry is malformed")
        claimed[item["path"][len(run_dir) + 1:]] = item.get("sha256")
    if claimed != evidence["log_files"]:
        raise ProvenanceError("record log hashes do not match the raw logs present")


def bind_candidate_build(root: Path, build: dict[str, Any]) -> None:
    """A candidate port declares its own build; only consistency is bound.

    A port is not required to be a pristine Athena++ CPU build, so no canonical
    configure/make argv, configure.py summary or pinned-source digest is demanded
    here.  What is demanded is that the binary and every log the receipt names are
    present in the root and hash exactly as claimed.
    """
    if not isinstance(build, dict):
        raise ProvenanceError("candidate record has no build receipt")
    if build.get("status") not in ("compiled", "built"):
        raise ProvenanceError("candidate build receipt does not claim a successful build")
    for key, value in build.items():
        if key.endswith("_exit") and value != 0:
            raise ProvenanceError(f"candidate build {key} is not zero")
    binary = safe_child(root, build.get("binary"), kind="candidate build binary")
    if not HEX64.match(str(build.get("binary_sha256", ""))) or sha256_file(binary) != build["binary_sha256"]:
        raise ProvenanceError("candidate build binary hash does not match the binary present in the root")
    logs = build.get("log_files")
    if not isinstance(logs, dict) or not logs:
        raise ProvenanceError("candidate build receipt must hash-bind its build logs")
    for key, digest in logs.items():
        path = safe_child(root, build.get(key), kind=f"candidate build {key}")
        if sha256_file(path) != digest:
            raise ProvenanceError(f"candidate build {key} hash does not match the log present in the root")


def bind_build(root: Path, build: dict[str, Any], spec: dict[str, Any], *, role: str, synthetic: bool) -> None:
    """Bind one build receipt to the build products actually present in the root."""
    if role != ROLE_ORACLE:
        bind_candidate_build(root, build)
        return
    problem, solver, nghost = build_key(spec)
    if build.get("status") != "compiled" or build.get("configure_exit") != 0 or build.get("make_exit") != 0:
        raise ProvenanceError("build receipt is not a positive fresh compile")
    if (build.get("problem"), build.get("solver"), build.get("nghost")) != (problem, solver, nghost):
        raise ProvenanceError("build receipt key does not match the subcase build selection")
    if build.get("binary_exists") is not True:
        raise ProvenanceError("build receipt does not claim a produced binary")
    check_configure_command(build.get("configure_command"), spec)
    check_make_command(build.get("make_command"), build.get("make_jobs"))
    binary = safe_child(root, build.get("binary"), kind="build binary")
    if sha256_file(binary) != build.get("binary_sha256"):
        raise ProvenanceError("build receipt binary hash does not match the binary present in the root")
    if not synthetic:
        size = check_binary_format(binary)
        if build.get("binary_bytes") != size:
            raise ProvenanceError("build receipt binary size does not match the binary present in the root")
    logs = build.get("log_files")
    if not isinstance(logs, dict) or set(logs) != set(BUILD_LOGS):
        raise ProvenanceError("build receipt does not hash-bind its four configure/make logs")
    contents: dict[str, str] = {}
    for key in BUILD_LOGS:
        path = safe_child(root, build.get(key), kind=f"build {key}")
        if sha256_file(path) != logs.get(key):
            raise ProvenanceError(f"build {key} hash does not match the log present in the root")
        contents[key] = path.read_text(encoding="utf-8", errors="replace")
        if key.endswith("stdout") and not contents[key].strip():
            raise ProvenanceError(f"build {key} is empty")
    check_configure_log(contents["configure_stdout"], spec)
    check_make_log(contents["make_stdout"])
    identity = load_source_identity()
    if build.get("source_tree_sha256") != identity["tree_sha256"]:
        raise ProvenanceError("build receipt does not claim the pinned Athena++ source tree")
    tree = safe_child(root, build.get("source_build_tree"), kind="build source tree", directory=True)
    digest, files, total = build_tree_digest(tree)
    if (digest, files, total) != (identity["tree_sha256"], identity["file_count"], identity["byte_count"]):
        raise ProvenanceError(
            f"the tree this binary was built from is not the pinned Athena++ snapshot "
            f"({digest}, {files} files, {total} bytes)")


def bind_record(root: Path, record: dict[str, Any], *, folder: str, name: str, deck_relative: str, deck_sha256: str,
                spec: dict[str, Any], evidence: dict[str, Any], builds: dict[str, dict[str, Any]], artifact_sha256: str,
                artifact_size: int, role: str = ROLE_ORACLE, synthetic: bool = False) -> None:
    """Every receipt claim must equal what the verifier recomputed."""
    run_dir = f"{RUN_DIR}/{folder}/{name}"
    if record.get("status") != "complete":
        raise ProvenanceError("record is not complete")
    if record.get("deck") != deck_relative or record.get("deck_sha256") != deck_sha256:
        raise ProvenanceError("record deck identity does not match the check-owned deck")
    if record.get("expected_rows") != evidence["expected_rows"] or list(record.get("dimensions", [])) != list(spec["dimensions"]):
        raise ProvenanceError("record geometry does not match the check contract")
    for key in ("pardump_exit", "athena_exit", "extract_exit"):
        if record.get(key) != 0:
            raise ProvenanceError(f"record {key} is not zero")
    binary_relative = record.get("binary")
    binary = safe_child(root, binary_relative, kind="record binary")
    if not HEX64.match(str(record.get("binary_sha256", ""))) or sha256_file(binary) != record["binary_sha256"]:
        raise ProvenanceError("record binary hash does not match the binary present in the root")
    command = record.get("athena_command")
    if not isinstance(command, list) or len(command) != 3 or command[1] != "-i":
        raise ProvenanceError("record athena_command is not '<binary> -i <deck>'")
    if not _relative_suffix(command[0], binary_relative) or not _relative_suffix(command[2], deck_relative):
        raise ProvenanceError("record athena_command does not name the bound binary and deck")
    if record.get("pardump_command") != list(command) + ["-n"]:
        raise ProvenanceError("record pardump_command is not the bound command plus -n")
    _bind_logs(root, record, run_dir, evidence)
    if evidence.get("completion") is not None and record.get("completion") != evidence["completion"]:
        raise ProvenanceError("record completion record is not the normal completion the verifier parsed from the raw log")
    native = record.get("native_files")
    if not isinstance(native, list) or record.get("native_file_count") != len(native):
        raise ProvenanceError("record native file inventory is malformed")
    claimed = {}
    for item in native:
        if not isinstance(item, dict) or not isinstance(item.get("path"), str) or not item["path"].startswith(run_dir + "/"):
            raise ProvenanceError("record native file entry is malformed")
        claimed[item["path"][len(run_dir) + 1:]] = item.get("sha256")
    if claimed != evidence["native_files"]:
        raise ProvenanceError("record native file hashes do not match the files present")
    if record.get("artifact") != f"{folder}/{name}/primitive_tab.json":
        raise ProvenanceError("record artifact path mismatch")
    if record.get("artifact_sha256") != artifact_sha256 or record.get("artifact_size") != artifact_size:
        raise ProvenanceError("record artifact hash/size does not match the shipped artifact")
    _check_extract_command(record.get("extract_command"), name=name, spec=spec, rows=evidence["expected_rows"],
                           run_dir=run_dir, artifact=record["artifact"])
    build = builds.get(binary_relative)
    if build is None or build != record.get("build"):
        raise ProvenanceError("record build receipt is not the inventory entry for its binary")
    if build.get("binary_sha256") != record["binary_sha256"]:
        raise ProvenanceError("build receipt binary hash differs from the record")
    bind_build(root, build, spec, role=role, synthetic=synthetic)


def load_receipt(root: Path, catalog: dict[str, Any], *, position: str) -> tuple[dict[str, Any], dict[tuple[str, str], dict[str, Any]], dict[str, dict[str, Any]]]:
    """Top-level receipt checks; returns (receipt, records by identity, builds by binary path)."""
    receipt = load_strict_json(root / "execution_manifest.json")
    if not isinstance(receipt, dict):
        raise ProvenanceError("execution receipt must be a JSON object")
    if receipt.get("schema") != RECEIPT_SCHEMA or receipt.get("status") != "complete":
        raise ProvenanceError(f"execution receipt is not a complete {RECEIPT_SCHEMA} record")
    role = receipt.get("role")
    if role not in ROLES:
        raise ProvenanceError("execution receipt does not declare a known role")
    evidence_class = receipt.get("evidence_class")
    if evidence_class not in EVIDENCE_CLASSES:
        raise ProvenanceError("execution receipt does not declare a known evidence class")
    synthetic = root_is_synthetic(root)
    if synthetic != (evidence_class == EVIDENCE_SYNTHETIC):
        raise ProvenanceError("synthetic-fixture marker and declared evidence class disagree")
    if position == "reference" and role != ROLE_ORACLE:
        raise ProvenanceError("the reference position requires a trusted reference-oracle root")
    if role == ROLE_ORACLE and evidence_class == EVIDENCE_CANDIDATE:
        raise ProvenanceError("an oracle role cannot declare candidate-run evidence")
    if role == ROLE_CANDIDATE and evidence_class in (EVIDENCE_DOCKER, EVIDENCE_HOST_NATIVE):
        raise ProvenanceError("a candidate role cannot declare oracle-run evidence")
    if role == ROLE_ORACLE:
        identity = load_source_identity()
        if receipt.get("source_commit") != identity["commit"] or receipt.get("source_tree_sha256") != identity["tree_sha256"]:
            raise ProvenanceError("execution receipt source identity is not the pinned source tree")
        if receipt.get("source_file_count") != identity["file_count"] or receipt.get("source_byte_count") != identity["byte_count"]:
            raise ProvenanceError("execution receipt source inventory is not the pinned source tree")
    else:
        implementation = receipt.get("implementation")
        if not isinstance(implementation, dict) or not isinstance(implementation.get("name"), str) or not implementation["name"].strip():
            raise ProvenanceError("candidate receipt must name the implementation it ran")
        for key in ("build_command", "run_command"):
            if not isinstance(implementation.get(key), (str, list)) or not implementation[key]:
                raise ProvenanceError(f"candidate receipt must record its {key}")
    nonce = receipt.get("run_nonce")
    if not isinstance(nonce, str) or not re.fullmatch(r"[0-9a-f]{32}", nonce):
        raise ProvenanceError("execution receipt lacks a 128-bit run nonce")
    hostname = receipt.get("container_hostname")
    if not isinstance(hostname, str) or not hostname:
        raise ProvenanceError("execution receipt lacks the container hostname")
    for key in ("started_at", "finished_at"):
        if not isinstance(receipt.get(key), str) or not receipt[key].endswith("Z"):
            raise ProvenanceError(f"execution receipt lacks UTC {key}")
    count = catalog["check_count"]
    total = sum(len(item["subcases"]) for item in catalog["checks"])
    if receipt.get("check_count") != count or receipt.get("subcase_count") != total or receipt.get("artifact_count") != total:
        raise ProvenanceError(f"execution receipt counts are not {count}/{total}/{total}")
    records = receipt.get("records")
    if not isinstance(records, list) or len(records) != total:
        raise ProvenanceError(f"execution receipt must contain {total} records")
    by_identity: dict[tuple[str, str], dict[str, Any]] = {}
    for record in records:
        if not isinstance(record, dict):
            raise ProvenanceError("execution record is malformed")
        key = (record.get("folder"), record.get("subcase"))
        if key in by_identity:
            raise ProvenanceError(f"duplicate execution record {key}")
        by_identity[key] = record
    expected = {(item["folder"], spec["name"]) for item in catalog["checks"] for spec in item["subcases"]}
    if set(by_identity) != expected:
        raise ProvenanceError("execution receipt does not enumerate exactly the declared subcases")
    builds_list = receipt.get("binary_builds")
    if not isinstance(builds_list, list) or not builds_list or receipt.get("binary_build_count") != len(builds_list):
        raise ProvenanceError("execution receipt binary build inventory is malformed")
    builds: dict[str, dict[str, Any]] = {}
    for build in builds_list:
        if not isinstance(build, dict) or not isinstance(build.get("binary"), str) or build["binary"] in builds:
            raise ProvenanceError("execution receipt build inventory is malformed")
        builds[build["binary"]] = build
    used = {record.get("binary") for record in records}
    if used != set(builds):
        raise ProvenanceError("execution receipt build inventory does not match the binaries used")
    return receipt, by_identity, builds


def docker_receipt(root: Path) -> dict[str, Any]:
    """The host-side receipt written by solution/solve.sh for one Dockerized oracle run."""
    candidates = sorted(path for path in root.iterdir() if path.name.startswith("docker-receipt-") and path.name.endswith(".json"))
    if len(candidates) != 1:
        raise ProvenanceError(f"expected exactly one docker receipt, found {len(candidates)}")
    document = load_strict_json(candidates[0])
    if not isinstance(document, dict):
        raise ProvenanceError("docker receipt must be a JSON object")
    required = {
        "schema", "role", "evidence_class", "run_id", "image", "image_id", "container", "container_id",
        "container_exit", "source_pin", "shared_source", "dockerfile", "run_nonce", "container_hostname",
        "started_at", "finished_at", "make_jobs", "operational_cap_seconds", "build_command", "run_command",
        "checks", "subcases", "execution_manifest_sha256",
    }
    missing = sorted(required - set(document))
    if missing:
        raise ProvenanceError(f"docker receipt is missing required fields: {missing[:6]}")
    if document.get("schema") != DOCKER_RECEIPT_SCHEMA:
        raise ProvenanceError("docker receipt is not the current host-receipt schema")
    if document.get("role") != ROLE_ORACLE:
        raise ProvenanceError("docker receipt does not declare the reference-oracle role")
    if document.get("evidence_class") not in (EVIDENCE_DOCKER, EVIDENCE_SYNTHETIC):
        raise ProvenanceError("docker receipt does not declare a Docker oracle evidence class")
    if document.get("container_exit") != 0 or document.get("dockerfile") != "tests/Dockerfile":
        raise ProvenanceError("docker receipt is not a successful tests/Dockerfile oracle run")
    container = document.get("container_id")
    if not isinstance(container, str) or not HEX64.match(container):
        raise ProvenanceError("docker receipt lacks a 64-hex container id")
    image = document.get("image_id")
    if not isinstance(image, str) or not image.startswith("sha256:") or not HEX64.match(image[len("sha256:"):]):
        raise ProvenanceError("docker receipt lacks a sha256:<64 hex> image id")
    if document.get("container_hostname") != container[:12]:
        raise ProvenanceError("docker receipt hostname is not the container id prefix")
    if not isinstance(document.get("run_nonce"), str) or not re.fullmatch(r"[0-9a-f]{32}", document["run_nonce"]):
        raise ProvenanceError("docker receipt lacks the in-container run nonce")
    for key in ("started_at", "finished_at"):
        if not isinstance(document.get(key), str) or not document[key].endswith("Z"):
            raise ProvenanceError(f"docker receipt lacks UTC {key}")
    if document.get("source_pin") != load_source_identity()["commit"]:
        raise ProvenanceError("docker receipt source pin is not the task pin")
    run_command = document.get("run_command")
    if not isinstance(run_command, str) or "docker run" not in run_command:
        raise ProvenanceError("docker receipt does not record its docker run command")
    if "--network none" not in run_command:
        raise ProvenanceError("docker receipt run command did not isolate the container network")
    image_name = document.get("image")
    if not isinstance(image_name, str) or not run_command.rstrip().endswith(image_name):
        raise ProvenanceError(
            "docker receipt run command must end with the image and pass no command, so the image ENTRYPOINT "
            "runs the no-argument solve.sh")
    if not isinstance(document.get("make_jobs"), int) or document["make_jobs"] <= 0:
        raise ProvenanceError("docker receipt make_jobs is malformed")
    cap = document.get("operational_cap_seconds")
    if isinstance(cap, bool) or not isinstance(cap, (int, float)) or not math.isfinite(float(cap)) or float(cap) <= 0:
        raise ProvenanceError("docker receipt operational cap is malformed")
    inner = root / "execution_manifest.json"
    if inner.is_symlink() or not inner.is_file():
        raise ProvenanceError("docker receipt has no in-root execution manifest to bind")
    if document.get("execution_manifest_sha256") != sha256_file(inner):
        raise ProvenanceError("docker receipt does not bind the execution manifest actually present in the root")
    return document


def self_test_independence(reference: Path, candidate: Path, reference_receipt: dict[str, Any], candidate_receipt: dict[str, Any],
                           subcases: Iterable[tuple[str, str]]) -> list[str]:
    """Reasons the two roots are NOT two independent real oracle executions (empty when they are)."""
    problems: list[str] = []
    dockers = {}
    for label, root, receipt in (("reference", reference, reference_receipt), ("candidate", candidate, candidate_receipt)):
        if root_is_synthetic(root):
            problems.append(f"{label}: root is a synthetic verifier fixture, not an execution")
        if receipt.get("role") != ROLE_ORACLE:
            problems.append(f"{label}: root does not declare the reference-oracle role")
        if receipt.get("evidence_class") != EVIDENCE_DOCKER:
            problems.append(f"{label}: evidence class {receipt.get('evidence_class')!r} is not a Docker oracle run")
        try:
            document = docker_receipt(root)
        except (ProvenanceError, ValueError, OSError) as exc:
            problems.append(f"{label}: {exc}")
            continue
        dockers[label] = document
        if document["container_id"][:12] != receipt.get("container_hostname"):
            problems.append(f"{label}: in-container hostname is not bound to the host-observed container id")
        if document.get("run_nonce") != receipt.get("run_nonce"):
            problems.append(f"{label}: host docker receipt and in-container receipt disagree on the run nonce")
        if document.get("started_at") != receipt.get("started_at") or document.get("finished_at") != receipt.get("finished_at"):
            problems.append(f"{label}: host docker receipt and in-container receipt disagree on the run window")
        if document.get("subcases") != receipt.get("subcase_count") or document.get("checks") != receipt.get("check_count"):
            problems.append(f"{label}: host docker receipt and in-container receipt disagree on the check inventory")
    if len(dockers) == 2:
        for key in ("run_id", "container", "container_id", "image_id", "run_nonce", "container_hostname",
                    "started_at", "execution_manifest_sha256"):
            if dockers["reference"].get(key) == dockers["candidate"].get(key):
                problems.append(f"both roots share the same docker {key}")
    for key in ("run_nonce", "container_hostname", "started_at"):
        if reference_receipt.get(key) == candidate_receipt.get(key):
            problems.append(f"both execution receipts share the same {key}")
    identical = 0
    compared = 0
    for folder, name in subcases:
        try:
            left = (reference / RUN_DIR / folder / name / "athena.stdout.log").read_bytes()
            right = (candidate / RUN_DIR / folder / name / "athena.stdout.log").read_bytes()
        except OSError:
            continue
        compared += 1
        if left == right:
            identical += 1
    if compared == 0 or identical == compared:
        problems.append("every Athena++ stdout log is byte-identical between the roots (copied execution evidence)")
    return problems
