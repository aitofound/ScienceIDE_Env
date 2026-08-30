"""Verifier-owned reader for Athena++ ``athinput`` decks and ``-n`` parameter dumps.

The reader reproduces the parts of ``ParameterInput::LoadFromStream`` that
matter for binding a public check deck to a real run: blocks are ``<name>``
lines, a parameter is ``name = value`` with an optional ``#`` comment, blank and
comment lines are ignored, a repeated block continues the existing block, and
a repeated parameter overwrites the earlier value (last assignment wins).
"""
from __future__ import annotations

import math
import re
from dataclasses import dataclass, field
from typing import Any

PAR_DUMP_MARKER = "#------------------------- PAR_DUMP -------------------------"
# Athena++ writes TAB coordinates and data with the deck's ``data_format``
# (src/outputs/formatted_table.cpp:112-131) and the frame time with a fixed
# ``%e`` (formatted_table.cpp:85).  Both facts are load-bearing for the
# verifier's representation bounds, so the accepted format shape is exact.
DATA_FORMAT_RE = re.compile(r"^%(?P<flags>[-+ #0]*)(?P<width>\d*)\.(?P<precision>\d+)e$")
TAB_TIME_FORMAT_PRECISION = 6  # "%e" prints six digits after the decimal point


def parse_athinput(text: str) -> dict[str, dict[str, str]]:
    """Return ``{block: {parameter: value}}`` with Athena++ last-wins semantics."""
    blocks: dict[str, dict[str, str]] = {}
    current: str | None = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("<"):
            end = line.find(">")
            if end < 0:
                raise ValueError(f"malformed block header: {raw!r}")
            name = line[1:end].strip()
            if name == "par_end":
                break
            if not name:
                raise ValueError("empty block name")
            current = name
            blocks.setdefault(current, {})
            continue
        if current is None:
            raise ValueError(f"parameter outside any block: {raw!r}")
        equal = line.find("=")
        if equal < 0:
            raise ValueError(f"parameter line without '=': {raw!r}")
        name = line[:equal].strip()
        rest = line[equal + 1:]
        hash_index = rest.find("#")
        value = (rest if hash_index < 0 else rest[:hash_index]).strip()
        if not name:
            raise ValueError(f"parameter line without a name: {raw!r}")
        blocks[current][name] = value
    return blocks


def parse_pardump(text: str) -> dict[str, dict[str, str]]:
    """Parse the ``PAR_DUMP`` section printed by ``athena -i <deck> -n``."""
    lines = text.splitlines()
    starts = [index for index, line in enumerate(lines) if line.strip() == PAR_DUMP_MARKER]
    if len(starts) != 2:
        raise ValueError("parameter dump must contain exactly two PAR_DUMP markers")
    body = "\n".join(lines[starts[0] + 1:starts[1]])
    return parse_athinput(body)


def _number(value: str, *, kind: str) -> float:
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{kind} is not numeric: {value!r}") from exc
    if not math.isfinite(number):
        raise ValueError(f"{kind} is not finite: {value!r}")
    return number


def _integer(value: str, *, kind: str) -> int:
    try:
        number = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{kind} is not an integer: {value!r}") from exc
    return number


@dataclass(frozen=True)
class TabOutput:
    block: str
    file_id: str
    dt: float
    data_format: str
    x2_slice: float | None
    x3_slice: float | None


@dataclass(frozen=True)
class Region:
    block: str
    level: int
    bounds: tuple[tuple[float, float], tuple[float, float], tuple[float, float]]


@dataclass(frozen=True)
class DeckGeometry:
    problem_id: str
    root: tuple[int, int, int]
    xmin: tuple[float, float, float]
    xmax: tuple[float, float, float]
    meshblock: tuple[int, int, int]
    refinement: str
    regions: tuple[Region, ...]
    boundaries: dict[str, str]
    tab: TabOutput
    tlim: float
    integrator: str
    xorder: str
    parameters: dict[str, dict[str, str]] = field(repr=False)

    @property
    def root_blocks(self) -> tuple[int, int, int]:
        return tuple(self.root[axis] // self.meshblock[axis] for axis in range(3))  # type: ignore[return-value]

    @property
    def dx(self) -> tuple[float, float, float]:
        return tuple((self.xmax[axis] - self.xmin[axis]) / self.root[axis] for axis in range(3))  # type: ignore[return-value]

    @property
    def cells_per_block(self) -> int:
        return self.meshblock[0] * self.meshblock[1] * self.meshblock[2]


def deck_geometry(params: dict[str, dict[str, str]]) -> DeckGeometry:
    """Derive the mesh/output facts a run must reproduce from parsed deck parameters."""
    job = params.get("job", {})
    mesh = params.get("mesh", {})
    time = params.get("time", {})
    problem_id = job.get("problem_id")
    if not problem_id:
        raise ValueError("deck lacks job/problem_id")
    root = tuple(_integer(mesh.get(f"nx{axis}", ""), kind=f"mesh/nx{axis}") for axis in (1, 2, 3))
    xmin = tuple(_number(mesh.get(f"x{axis}min", ""), kind=f"mesh/x{axis}min") for axis in (1, 2, 3))
    xmax = tuple(_number(mesh.get(f"x{axis}max", ""), kind=f"mesh/x{axis}max") for axis in (1, 2, 3))
    for axis in range(3):
        if root[axis] <= 0 or not xmax[axis] > xmin[axis]:
            raise ValueError(f"mesh axis {axis + 1} is malformed")
        ratio = mesh.get(f"x{axis + 1}rat", "1.0")
        if _number(ratio, kind="mesh ratio") != 1.0:
            raise ValueError("only uniform Cartesian meshes are supported by this verifier")
    block = params.get("meshblock", {})
    # Mesh::Mesh reads meshblock/nx2 only for 2-D/3-D meshes and meshblock/nx3
    # only for 3-D meshes; a collapsed axis always has a one-cell block.
    meshblock = tuple(
        1 if root[axis - 1] == 1 else (_integer(block[f"nx{axis}"], kind=f"meshblock/nx{axis}") if f"nx{axis}" in block else root[axis - 1])
        for axis in (1, 2, 3)
    )
    for axis in range(3):
        if meshblock[axis] <= 0 or root[axis] % meshblock[axis] != 0:
            raise ValueError(f"meshblock axis {axis + 1} does not tile the root mesh")
    refinement = mesh.get("refinement", "none")
    if refinement not in {"none", "static"}:
        raise ValueError(f"unsupported refinement mode {refinement!r}")
    regions: list[Region] = []
    if refinement == "static":
        for name, values in params.items():
            if not name.startswith("refinement"):
                continue
            level = _integer(values.get("level", ""), kind=f"{name}/level")
            bounds = []
            for axis in (1, 2, 3):
                low = _number(values.get(f"x{axis}min", str(xmin[axis - 1])), kind=f"{name}/x{axis}min")
                high = _number(values.get(f"x{axis}max", str(xmax[axis - 1])), kind=f"{name}/x{axis}max")
                bounds.append((low, high))
            regions.append(Region(name, level, (bounds[0], bounds[1], bounds[2])))
        if not regions:
            raise ValueError("static refinement without any <refinement*> region")
    boundaries = {
        key: mesh[key]
        for key in ("ix1_bc", "ox1_bc", "ix2_bc", "ox2_bc", "ix3_bc", "ox3_bc")
        if key in mesh
    }
    tab: TabOutput | None = None
    for name, values in params.items():
        if not name.startswith("output"):
            continue
        if values.get("file_type") != "tab":
            continue
        if values.get("variable") != "prim":
            raise ValueError(f"{name} tab output must write prim variables")
        if tab is not None:
            raise ValueError("deck declares more than one tab output block")
        suffix = name[len("output"):]
        if not suffix.isdigit():
            raise ValueError(f"malformed output block name {name!r}")
        tab = TabOutput(
            block=name,
            file_id=f"out{int(suffix)}",
            dt=_number(values.get("dt", ""), kind=f"{name}/dt"),
            data_format=values.get("data_format", "%12.5e"),
            x2_slice=_number(values["x2_slice"], kind=f"{name}/x2_slice") if "x2_slice" in values else None,
            x3_slice=_number(values["x3_slice"], kind=f"{name}/x3_slice") if "x3_slice" in values else None,
        )
    if tab is None:
        raise ValueError("deck declares no prim tab output block")
    if tab.dt <= 0.0:
        raise ValueError("tab output dt must be positive")
    tlim = _number(time.get("tlim", ""), kind="time/tlim")
    if tlim <= 0.0:
        raise ValueError("time/tlim must be positive")
    return DeckGeometry(
        problem_id=problem_id,
        root=root,  # type: ignore[arg-type]
        xmin=xmin,  # type: ignore[arg-type]
        xmax=xmax,  # type: ignore[arg-type]
        meshblock=meshblock,  # type: ignore[arg-type]
        refinement=refinement,
        regions=tuple(regions),
        boundaries=boundaries,
        tab=tab,
        tlim=tlim,
        integrator=time.get("integrator", "vl2"),
        xorder=time.get("xorder", "2"),
        parameters=params,
    )


def data_format_precision(data_format: str) -> int:
    """Digits after the decimal point that ``data_format`` prints (``%12.5e`` -> 5).

    Athena++ passes the deck's ``output*/data_format`` straight to ``fprintf``
    (src/outputs/formatted_table.cpp:112-131) after prepending one space
    (src/outputs/outputs.cpp).  Only the ``%[flags][width].[precision]e`` shape
    used by every check-owned deck is accepted; anything else is refused rather
    than guessed, because the verifier derives its coordinate bounds from it.
    """
    match = DATA_FORMAT_RE.match(data_format.strip())
    if not match:
        raise ValueError(f"unsupported tab data_format for a check-owned deck: {data_format!r}")
    return int(match.group("precision"))


def bind_pardump(deck: dict[str, dict[str, str]], dump: dict[str, dict[str, str]]) -> None:
    """Every deck parameter (except the free-form <comment> block) must appear unchanged in the dump."""
    for block, values in deck.items():
        if block == "comment":
            continue
        dumped = dump.get(block)
        if dumped is None:
            raise ValueError(f"parameter dump lacks block <{block}>")
        for name, value in values.items():
            if name not in dumped:
                raise ValueError(f"parameter dump lacks {block}/{name}")
            if dumped[name] != value:
                raise ValueError(f"parameter dump {block}/{name}={dumped[name]!r} differs from deck {value!r}")


def summary(geometry: DeckGeometry) -> dict[str, Any]:
    return {
        "problem_id": geometry.problem_id,
        "root": list(geometry.root),
        "meshblock": list(geometry.meshblock),
        "refinement": geometry.refinement,
        "regions": len(geometry.regions),
        "tab_output": geometry.tab.file_id,
        "tab_dt": geometry.tab.dt,
        "tlim": geometry.tlim,
        "integrator": geometry.integrator,
        "xorder": geometry.xorder,
        "boundaries": dict(geometry.boundaries),
    }
