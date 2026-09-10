#!/usr/bin/env python3
"""Run the 15-min December 2006 AMPS cutoff-morphology experiment.

This runner extends the observation-specific C9/C10 calculations to a common
rigidity grid and two fixed altitude shells.  It deliberately imports the C10
Tecplot parser, GEO-to-AACGM conversion, and ACCESS_T50 reducer; therefore the
research product and the POES/MetOp validation cannot drift into different
definitions of access state or half transmission.

The default BATCHED layout evaluates up to ``--epochs-per-batch`` epochs and
both shells in one AMPS process.  It uses Mode3D ``SNAPSHOT_LIST`` so the AMR
mesh topology is allocated once for the batch.  The TS05/IGRF field values,
coordinate state, and compact interpolation arrays are nevertheless rebuilt
at every epoch; only the time-invariant mesh allocation is reused.  PER_EPOCH
provides one process per epoch and both shells, while STANDALONE runs one
process per epoch and altitude for an independent equivalence baseline.

Preparation-only mode renders every AMPS input, writes the
complete command inventory, and validates the requested epochs without launching
MPI. This is useful on login nodes and for advance review of computational cost.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
import os
import re
import shutil
import sys
from datetime import datetime, timedelta
from pathlib import Path
from types import SimpleNamespace
from typing import Dict, Iterable, List, Mapping, Optional, Sequence, Tuple

from study_common import (
    DriverRow, default_output_root, driver_dict, format_utc,
    interpolate_driver, load_config, parse_utc, read_driver,
    resolve_output_path, sha256, write_csv,
)


def load_c10_module(root: Path):
    """Load the bundled C10 runner as a private scientific-method library."""
    path = root / "vendor" / "C10" / "run_C10.py"
    spec = importlib.util.spec_from_file_location("dec2006_c10_core", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load C10 implementation from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def load_c9_module(root: Path):
    """Load C9 only for its audited reference/profile selection routines.

    Reusing the C9 selector is important: a future change to its SMOKE or
    ROUTINE epoch list must automatically change the shared production workset.
    Duplicating those timestamps here would eventually make the observation
    runner request a snapshot that the mesh-reuse run never produced.
    """

    path = root / "vendor" / "C9" / "run_C9.py"
    spec = importlib.util.spec_from_file_location("dec2006_c9_core", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load C9 implementation from {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def cadence_epochs(start: datetime, end: datetime, minutes: int) -> List[datetime]:
    """Return an inclusive, UTC-aligned cadence sequence."""
    if minutes < 1:
        raise ValueError("cadence must be positive")
    result: List[datetime] = []
    current = start
    step = timedelta(minutes=minutes)
    while current <= end:
        result.append(current)
        current += step
    if result[-1] != end:
        result.append(end)
    return result


def theil_sen_slope(rows: Sequence[DriverRow]) -> float:
    """Robust SYM-H slope in nT/h for the objective quiet-window test."""
    slopes: List[float] = []
    for left_index, left in enumerate(rows):
        for right in rows[left_index + 1:]:
            hours = (right.epoch - left.epoch).total_seconds() / 3600.0
            if hours > 0:
                slopes.append((right.symh_nt - left.symh_nt) / hours)
    slopes.sort()
    if not slopes:
        return float("inf")
    middle = len(slopes) // 2
    return slopes[middle] if len(slopes) % 2 else 0.5 * (
        slopes[middle - 1] + slopes[middle]
    )


def event_landmarks(driver: Sequence[DriverRow], config: Mapping[str, object]) -> Dict[str, datetime]:
    """Select quiet, compression, and main-phase times from fixed rules."""
    event = config["event"]  # type: ignore[index]
    search_start = parse_utc(event["compression_search_start_utc"])  # type: ignore[index]
    search_end = parse_utc(event["compression_search_end_utc"])  # type: ignore[index]
    arrival = [row for row in driver if search_start <= row.epoch <= search_end]
    if len(arrival) < 2:
        raise ValueError("compression search window is not covered by the driver")
    compression = max(
        zip(arrival[1:], arrival[:-1]),
        key=lambda pair: math.log(pair[0].pdyn_npa) - math.log(pair[1].pdyn_npa),
    )[0].epoch

    production_start = parse_utc(event["production_start_utc"])  # type: ignore[index]
    production_end = parse_utc(event["production_end_utc"])  # type: ignore[index]
    production = [row for row in driver if production_start <= row.epoch <= production_end]
    main_phase = min(production, key=lambda row: row.symh_nt).epoch

    window_hours = float(event["quiet_window_hours"])  # type: ignore[index]
    max_median = float(event["quiet_median_abs_symh_max_nt"])  # type: ignore[index]
    max_slope = float(event["quiet_abs_theil_sen_slope_max_nt_per_hour"])  # type: ignore[index]
    window = timedelta(hours=window_hours)
    candidates: List[Tuple[datetime, List[DriverRow]]] = []
    for end_row in driver:
        if end_row.epoch >= compression:
            break
        start_time = end_row.epoch - window
        subset = [row for row in driver if start_time <= row.epoch <= end_row.epoch]
        expected = int(round(window.total_seconds() / 300.0)) + 1
        if len(subset) == expected:
            candidates.append((end_row.epoch, subset))
    eligible = []
    for end_time, subset in candidates:
        absolute = sorted(abs(row.symh_nt) for row in subset)
        median = absolute[len(absolute) // 2]
        if median <= max_median and abs(theil_sen_slope(subset)) <= max_slope:
            eligible.append((end_time, subset))
    if not eligible:
        raise ValueError("no driver interval satisfies the configured quiet-window rule")
    quiet_rows = eligible[-1][1]
    quiet = quiet_rows[len(quiet_rows) // 2].epoch
    return {"quiet": quiet, "compression": compression, "main_phase": main_phase}


def selected_epochs(config: Mapping[str, object], profile: str,
                    landmarks: Mapping[str, datetime],
                    extra_epochs: Sequence[datetime] = ()) -> List[datetime]:
    """Construct the production workset and merge explicitly required epochs.

    ``extra_epochs`` is used by the shared-observation path.  C9 and C10 compare
    measurements at exact interval midpoints which do not all lie on the
    regular morphology cadence.  Adding them to the same sorted set lets those
    observation operators consume the shared AMPS products without shifting an
    observation in time or running a second field mesh.
    """

    profile_cfg = config["profiles"][profile]  # type: ignore[index]
    if "explicit_epochs_utc" in profile_cfg:
        result = {parse_utc(value) for value in profile_cfg["explicit_epochs_utc"]}
    else:
        event = config["event"]  # type: ignore[index]
        start = parse_utc(event["production_start_utc"])  # type: ignore[index]
        end = parse_utc(event["production_end_utc"])  # type: ignore[index]
        result = set(cadence_epochs(start, end, int(profile_cfg["cadence_minutes"])))
        if profile_cfg.get("include_landmarks", False):
            result.update(landmarks.values())
        if "rapid_window_half_width_minutes" in profile_cfg:
            half_width = int(profile_cfg["rapid_window_half_width_minutes"])
            rapid_cadence = int(profile_cfg["rapid_window_cadence_minutes"])
            for center in (landmarks["compression"], landmarks["main_phase"]):
                result.update(cadence_epochs(
                    max(start, center - timedelta(minutes=half_width)),
                    min(end, center + timedelta(minutes=half_width)),
                    rapid_cadence,
                ))
    result.update(extra_epochs)
    return sorted(result)


def observation_epochs(root: Path, config: Mapping[str, object], profile: str
                       ) -> Tuple[List[datetime], List[datetime]]:
    """Return the exact C9 and C10 epochs selected by the requested profile.

    The function calls the vendored runners' own reference loaders and profile
    selectors.  Consequently the shared calculation remains tied to the same
    immutable observation tables and does not maintain a second timestamp list.
    The current study uses one field snapshot per observational interval, which
    is the default and publication configuration of the top-level runner.
    """

    c9 = load_c9_module(root)
    c10 = load_c10_module(root)
    pamela_reference = root / config["data"]["pamela_reference"]  # type: ignore[index]
    poes_reference = root / config["data"]["poes_reference"]  # type: ignore[index]
    selector_args = SimpleNamespace(profile=profile, timestamps="")
    pamela = c9.selected_midpoints(c9.load_reference(pamela_reference), selector_args)
    poes = c10.selected_midpoints(c10.load_reference(poes_reference), selector_args)
    return list(pamela), list(poes)


def runner_namespace(config: Mapping[str, object], args: argparse.Namespace,
                     altitude_km: float) -> SimpleNamespace:
    """Translate the study configuration to the exact controls used by C10."""
    model = config["model"]  # type: ignore[index]
    execution = config["execution"]  # type: ignore[index]
    return SimpleNamespace(
        rigidity_min_gv=min(model["rigidities_gv"]),
        rigidity_max_gv=max(model["rigidities_gv"]),
        cutoff_evaluation="DIRECT_ACCESS",
        cutoff_scan_n=120,
        rigidities_gv=list(model["rigidities_gv"]),
        access_abs_lat_min_deg=float(model["latitude_band_abs_deg"][0]),
        access_abs_lat_max_deg=float(model["latitude_band_abs_deg"][1]),
        max_trace_time=float(model["max_trace_time_s"]),
        altitude_km=altitude_km,
        shell_lon_res_deg=float(model["shell_longitude_step_deg"]),
        shell_lat_res_deg=float(model["shell_latitude_step_deg"]),
        mode3d_mesh_res_earth_re=float(model["mode3d_mesh_res_earth_re"]),
        mode3d_mesh_res_boundary_re=float(model["mode3d_mesh_res_boundary_re"]),
        mode3d_mesh_coarsening=str(model["mode3d_mesh_coarsening"]),
        mode3d_mesh_exponent=float(model["mode3d_mesh_exponent"]),
        mpirun=args.mpirun,
        np=args.np if args.np is not None else int(execution["mpi_ranks"]),
        nt=args.nt if args.nt is not None else int(execution["threads_per_rank"]),
        scheduler=str(execution["scheduler"]),
        dynamic_chunk=int(execution["dynamic_chunk"]),
        cutoff_trace_policy="ACCURATE",
        mode3d_parallel_field_init=bool(execution["parallel_field_initialization"]),
        mover=str(model["mover"]),
    )


def command_text(command: Sequence[str]) -> str:
    """Make a readable command record without shell-dependent quoting tricks."""
    import shlex
    return " ".join(shlex.quote(token) for token in command)


def _isotonic_non_decreasing(values: Sequence[float]) -> List[float]:
    """Return an equal-weight pool-adjacent-violators fit.

    Individual trajectory classifications can alternate across a penumbra even
    though the large-scale access probability must increase with rigidity. A
    monotone fit defines a reproducible 50% cutoff without deleting those
    alternations; their number is archived separately as a map-quality field.
    """

    blocks: List[List[float]] = []
    for value in values:
        blocks.append([float(value), 1.0])  # weighted sum, weight
        while len(blocks) >= 2:
            left = blocks[-2][0] / blocks[-2][1]
            right = blocks[-1][0] / blocks[-1][1]
            if left <= right + 1.0e-15:
                break
            newest = blocks.pop()
            blocks[-1][0] += newest[0]
            blocks[-1][1] += newest[1]
    fitted: List[float] = []
    for total, weight in blocks:
        fitted.extend([total / weight] * int(round(weight)))
    return fitted


def derive_cutoff_rigidity_map(access: Sequence[object],
                               expected_rigidities: Sequence[float]
                               ) -> List[Dict[str, object]]:
    """Invert exact-rigidity access states into a quality-controlled R50 map.

    The result intentionally distinguishes five outcomes. ``BRACKETED`` is the
    only state with a reported numerical cutoff. ``BELOW_RANGE`` and
    ``ABOVE_RANGE`` are scientifically useful one-sided limits. ``UNBRACKETED``
    denotes a mixed penumbra whose isotonic curve never brackets 0.5, and
    ``INCOMPLETE`` denotes missing/duplicate grid coverage or fewer than two
    resolved rigidity samples. Unresolved trajectories are never converted to
    allowed or forbidden states.
    """

    expected = sorted(float(value) for value in expected_rigidities)
    grouped: Dict[Tuple[float, float], List[object]] = {}
    for row in access:
        key = (round(float(row.longitude_deg) % 360.0, 8),
               round(float(row.latitude_deg), 8))
        grouped.setdefault(key, []).append(row)
    output: List[Dict[str, object]] = []
    for (longitude, latitude), rows in sorted(grouped.items()):
        by_rigidity: Dict[float, object] = {}
        duplicate = False
        for row in rows:
            # Seven decimal places are far tighter than the minimum spacing in
            # the configured list while absorbing harmless Tecplot formatting
            # roundoff. This also avoids an O(N^2) nearest-neighbor search for
            # every spatial cell in the FULL event.
            rigidity = round(float(row.rigidity_gv), 7)
            if rigidity in by_rigidity:
                duplicate = True
            by_rigidity[rigidity] = row
        ordered = []
        missing = 0
        expected_keys = {round(value, 7) for value in expected}
        unexpected = len(set(by_rigidity).difference(expected_keys))
        for rigidity in expected:
            match = by_rigidity.get(round(rigidity, 7))
            if match is None:
                missing += 1
            else:
                ordered.append(match)
        resolved = [row for row in ordered if int(row.access_state) != 2]
        unresolved = len(ordered) - len(resolved)
        status = "INCOMPLETE"
        cutoff = lower = upper = span = None
        transition_count = 0
        nonmonotonic_count = 0
        if not duplicate and missing == 0 and unexpected == 0 and len(resolved) >= 2:
            resolved.sort(key=lambda row: float(row.rigidity_gv))
            states = [int(row.access_state) for row in resolved]
            transition_count = sum(left != right for left, right in zip(states, states[1:]))
            nonmonotonic_count = sum(
                left == 1 and right == 0 for left, right in zip(states, states[1:])
            )
            if all(state == 1 for state in states):
                status = "BELOW_RANGE"
            elif all(state == 0 for state in states):
                status = "ABOVE_RANGE"
            else:
                rigidities = [float(row.rigidity_gv) for row in resolved]
                fitted = _isotonic_non_decreasing(states)
                if fitted[0] < 0.5 - 1.0e-12 and fitted[-1] > 0.5 + 1.0e-12:
                    equal = [index for index, value in enumerate(fitted)
                             if abs(value - 0.5) <= 1.0e-12]
                    if equal:
                        lower = rigidities[equal[0]]
                        upper = rigidities[equal[-1]]
                        cutoff = 0.5 * (lower + upper)
                    else:
                        crossing = next(
                            index for index in range(len(fitted) - 1)
                            if fitted[index] < 0.5 < fitted[index + 1]
                        )
                        lower, upper = rigidities[crossing:crossing + 2]
                        fraction = ((0.5 - fitted[crossing]) /
                                    (fitted[crossing + 1] - fitted[crossing]))
                        cutoff = lower + fraction * (upper - lower)
                    span = upper - lower
                    status = "BRACKETED"
                else:
                    status = "UNBRACKETED"
        representative = rows[0]
        output.append({
            "longitude_geo_deg": longitude,
            "latitude_geo_deg": latitude,
            "aacgm_latitude_deg": representative.aacgm_latitude_deg,
            "mlt_hour": representative.mlt_hour,
            "cutoff_rigidity_r50_gv": cutoff,
            "cutoff_status": status,
            "cutoff_lower_bracket_gv": lower,
            "cutoff_upper_bracket_gv": upper,
            "cutoff_bracket_span_gv": span,
            "n_expected_rigidities": len(expected),
            "n_present_rigidities": len(ordered),
            "n_unexpected_rigidities": unexpected,
            "n_resolved_rigidities": len(resolved),
            "n_unresolved_rigidities": unresolved,
            "resolved_rigidity_fraction": (
                len(resolved) / len(expected) if expected else 0.0
            ),
            "access_transition_count": transition_count,
            "nonmonotonic_transition_count": nonmonotonic_count,
            "duplicate_rigidity": duplicate,
            "sampled_rigidity_min_gv": expected[0] if expected else None,
            "sampled_rigidity_max_gv": expected[-1] if expected else None,
        })
    return output


ZONE_ALTITUDE_PATTERNS = (
    re.compile(r"alt[_\s-]*km\s*=\s*([0-9eE+\-.]+)", re.IGNORECASE),
    re.compile(r"alt(?:itude)?\s*=\s*([0-9eE+\-.]+)\s*km", re.IGNORECASE),
    re.compile(r"altitude[_\s-]*km\s*[:=]\s*([0-9eE+\-.]+)", re.IGNORECASE),
)

# Some AMPS products identify the shell in a zone title (``Shell_0``), while
# the current Mode3D DIRECT_ACCESS writer uses one generic zone and puts the
# zero-based identity in a ``shell_index`` column on every numerical record.
# Both contracts are explicit and safe because SHELL_ALTS_KM defines the index
# ordering.  We never infer a shell merely from row count or encounter order.
ZONE_SHELL_INDEX_PATTERN = re.compile(
    r"\bshell(?:[_\s-]*(?:index)?[_\s-]*)?(\d+)\b", re.IGNORECASE
)


def _zone_altitude_km(line: str) -> Optional[float]:
    """Extract an AMPS shell altitude from a Tecplot ZONE declaration."""

    for pattern in ZONE_ALTITUDE_PATTERNS:
        match = pattern.search(line)
        if match:
            return float(match.group(1))
    return None


def _zone_shell_index(line: str, shell_count: int) -> Optional[int]:
    """Return an explicit zero-based shell index from a Tecplot zone title."""

    match = ZONE_SHELL_INDEX_PATTERN.search(line)
    if not match:
        return None
    index = int(match.group(1))
    return index if 0 <= index < shell_count else None


def split_multishell_access(source: Path, expected_altitudes: Sequence[float],
                            destinations: Mapping[float, Path], c10) -> Dict[float, int]:
    """Split one multi-shell access product into strict single-shell products.

    AMPS represents multiple shell altitudes as Tecplot zones in one file.  C9
    and C10 historically consume single-shell files, so the shared runner must
    split the product without guessing from row order.  Each row must identify
    its shell through an altitude column, a zero-based ``shell_index`` column,
    or an altitude/index-bearing ZONE header.  Missing, unexpected, ambiguous,
    non-integral, or internally contradictory shell identifiers are hard
    errors; silently assigning half the rows to each altitude could create a
    plausible but invalid cutoff.

    The numerical records are kept verbatim.  This avoids changing binary64
    decimal renderings before the observation runners parse the staged files.
    """

    expected = [float(value) for value in expected_altitudes]
    if not expected or len(set(expected)) != len(expected):
        raise ValueError("expected shell altitudes must be nonempty and unique")
    if set(float(key) for key in destinations) != set(expected):
        raise ValueError("destination altitudes do not match expected shells")

    original_variables: List[str] = []
    normalized_variables: List[str] = []
    grouped: Dict[float, List[str]] = {altitude: [] for altitude in expected}
    current_altitude: Optional[float] = None
    reading_variables = False

    def canonical_altitude(value: float, line_number: int) -> float:
        matches = [altitude for altitude in expected if abs(value - altitude) <= 1.0e-5]
        if len(matches) != 1:
            raise ValueError(
                f"{source}:{line_number} altitude {value:g} km does not identify "
                f"exactly one expected shell {expected}"
            )
        return matches[0]

    with source.open("r", encoding="utf-8", errors="replace") as stream:
        for line_number, raw in enumerate(stream, start=1):
            text = raw.strip()
            if not text:
                continue
            upper = text.upper()
            if upper.startswith("VARIABLES"):
                if original_variables:
                    raise ValueError(f"{source} contains multiple VARIABLES declarations")
                reading_variables = True
                original_variables.extend(re.findall(r'"([^"]+)"', text))
                continue
            if reading_variables:
                quoted = re.findall(r'"([^"]+)"', text)
                if quoted and not upper.startswith("ZONE"):
                    original_variables.extend(quoted)
                    continue
                reading_variables = False
                normalized_variables = [
                    c10.normalize_tecplot_variable_name(name)
                    for name in original_variables
                ]
            if upper.startswith("ZONE"):
                altitude = _zone_altitude_km(text)
                if altitude is not None:
                    current_altitude = canonical_altitude(altitude, line_number)
                else:
                    shell_index = _zone_shell_index(text, len(expected))
                    current_altitude = (
                        expected[shell_index] if shell_index is not None else None
                    )
                continue
            if upper.startswith(("TITLE", "AUXDATA", "DATASETAUXDATA", "#", "!")):
                continue
            if not original_variables:
                raise ValueError(f"{source}:{line_number} data precedes VARIABLES")
            if not normalized_variables:
                normalized_variables = [
                    c10.normalize_tecplot_variable_name(name)
                    for name in original_variables
                ]
            tokens = text.replace(",", " ").split()
            if len(tokens) != len(original_variables):
                raise ValueError(
                    f"{source}:{line_number} has {len(tokens)} columns; "
                    f"VARIABLES defines {len(original_variables)}"
                )
            try:
                values = [float(token) for token in tokens]
            except ValueError as exc:
                raise ValueError(f"{source}:{line_number} is not numerical") from exc

            altitude_index = next((
                normalized_variables.index(name)
                for name in ("alt_km", "altitude_km", "altitude")
                if name in normalized_variables
            ), None)
            shell_column_index = (
                normalized_variables.index("shell_index")
                if "shell_index" in normalized_variables else None
            )
            column_shell_altitude: Optional[float] = None
            if shell_column_index is not None:
                # C++ emits an integer, but parse the decimal representation
                # defensively and reject values such as 0.5 rather than letting
                # int() silently truncate them to a valid-looking shell.
                shell_value = values[shell_column_index]
                if not math.isfinite(shell_value):
                    raise ValueError(
                        f"{source}:{line_number} shell_index is not finite"
                    )
                shell_index = int(round(shell_value))
                if (abs(shell_value - shell_index) > 1.0e-9
                        or not 0 <= shell_index < len(expected)):
                    raise ValueError(
                        f"{source}:{line_number} shell_index={shell_value:g} is "
                        f"not a valid zero-based index for {len(expected)} shell(s)"
                    )
                column_shell_altitude = expected[shell_index]
            if altitude_index is not None:
                row_altitude = canonical_altitude(values[altitude_index], line_number)
                if current_altitude is not None and row_altitude != current_altitude:
                    raise ValueError(
                        f"{source}:{line_number} altitude column disagrees with ZONE"
                    )
                if (column_shell_altitude is not None
                        and row_altitude != column_shell_altitude):
                    raise ValueError(
                        f"{source}:{line_number} altitude column disagrees with "
                        "shell_index column"
                    )
            elif column_shell_altitude is not None:
                row_altitude = column_shell_altitude
                if current_altitude is not None and row_altitude != current_altitude:
                    raise ValueError(
                        f"{source}:{line_number} shell_index column disagrees with ZONE"
                    )
            elif current_altitude is not None:
                row_altitude = current_altitude
            elif len(expected) == 1:
                # Historical single-shell AMPS files may use a generic ZONE name.
                # That is unambiguous only when the caller expects one altitude.
                row_altitude = expected[0]
            else:
                raise ValueError(
                    f"{source}:{line_number} has no altitude column, shell_index "
                    "column, or altitude/index-bearing ZONE declaration"
                )
            grouped[row_altitude].append(raw.rstrip("\n"))

    if not original_variables:
        raise ValueError(f"{source} has no VARIABLES declaration")
    missing = [altitude for altitude, rows in grouped.items() if not rows]
    if missing:
        raise ValueError(f"{source} is missing shell zone(s): {missing}")

    variables_line = "VARIABLES=" + " ".join(f'"{name}"' for name in original_variables)
    counts: Dict[float, int] = {}
    for altitude in expected:
        destination = destinations[altitude]
        destination.parent.mkdir(parents=True, exist_ok=True)
        rows = grouped[altitude]
        destination.write_text(
            f'TITLE="AMPS shared-mesh shell access at {altitude:g} km"\n'
            + variables_line + "\n"
            + f'ZONE T="Alt_km={altitude:g}"\n'
            + "\n".join(rows) + "\n",
            encoding="utf-8",
        )
        # Reuse the C10 strict state parser as a second schema/consistency gate.
        # This verifies access_state, allowed, unresolved, and all row widths.
        counts[altitude] = len(c10.parse_tecplot_shell_access(destination))
    return counts


def stage_single_shell_product(source: Path, destination: Path) -> None:
    """Expose a shared raw product at the historical C9/C10 path efficiently."""

    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() or destination.is_symlink():
        destination.unlink()
    try:
        # Hard links avoid duplicating the large access table while preserving a
        # normal file interface for the unmodified observation runners.
        os.link(source, destination)
    except OSError:
        # Different filesystems do not permit hard links.  A byte-for-byte copy
        # is portable and keeps the scientific result identical.
        shutil.copy2(source, destination)


def write_shared_product_receipt(destination: Path, source: Path,
                                 consumer: str, epoch: datetime,
                                 altitude_km: float, layout: str) -> Path:
    """Write a local provenance receipt beside a staged C9/C10 raw product."""

    receipt = destination.parent / "SHARED_MODEL_PRODUCT.json"
    receipt.write_text(json.dumps({
        "consumer": consumer,
        "epoch_utc": format_utc(epoch),
        "altitude_km": altitude_km,
        "execution_layout": layout,
        "source": str(source),
        "staged_path": str(destination),
        "sha256": sha256(source),
        "amps_executed_by_observation_runner": False,
        "field_reinitialized_at_this_epoch": True,
    }, indent=2) + "\n", encoding="utf-8")
    return receipt


def observation_destination(output_root: Path, epoch: datetime) -> Path:
    """Return the GRIDDED, one-sample directory used by both C9 and C10."""

    token = epoch.strftime("%Y%m%dT%H%M%S")
    return output_root / "gridded" / token / f"sample_00_{token}"


def chunks(values: Sequence[datetime], size: int) -> List[List[datetime]]:
    """Partition epochs into restartable, true multi-epoch AMPS batches."""

    if size < 1:
        raise ValueError("batch size must be positive")
    return [list(values[index:index + size]) for index in range(0, len(values), size)]


def snapshot_suffix(snapshot_index: int, epoch: datetime) -> str:
    """Reproduce Mode3D's output suffix for exact product addressing.

    Native Mode3D appends ``_snapshot_NNNNNN_<sanitized UTC>`` before ``.dat``
    whenever ``SNAPSHOT_LIST`` is active.  Mirroring that deterministic rule is
    safer than selecting files with a wildcard: a stale output from an earlier
    batch must never satisfy the completion check for a newly requested epoch.
    """

    utc = epoch.strftime("%Y-%m-%dT%H:%M:%S")
    token = re.sub(r"[^A-Za-z0-9]", "_", utc)[:48]
    return f"_snapshot_{snapshot_index:06d}_{token}"


def write_snapshot_list(path: Path, epochs: Sequence[datetime]) -> None:
    """Write the explicit sorted epoch workset consumed by native Mode3D."""

    ordered = sorted(set(epochs))
    if not ordered or len(ordered) != len(epochs):
        raise ValueError("a temporal batch must contain unique epochs")
    path.write_text(
        "# One frozen IGRF+TS05 field realization per UTC epoch.\n"
        + "\n".join(epoch.strftime("%Y-%m-%dT%H:%M:%S") for epoch in ordered)
        + "\n",
        encoding="utf-8",
    )


def enable_snapshot_list(input_path: Path, snapshot_filename: str) -> None:
    """Insert the parser-supported temporal block into a rendered SHELLS deck.

    The checked-in multi-shell template remains a valid single-snapshot input.
    Only BATCHED generated cases receive this active block, which makes
    PER_EPOCH and STANDALONE useful controls for the new native batching path.
    """

    text = input_path.read_text(encoding="utf-8")
    marker = "!#END"
    if text.count(marker) != 1:
        raise ValueError(f"{input_path}: expected exactly one {marker} marker")
    temporal = (
        "#TEMPORAL\n"
        "TEMPORAL_MODE          SNAPSHOT_LIST\n"
        f"SNAPSHOT_LIST_FILE     {snapshot_filename}\n\n"
    )
    input_path.write_text(text.replace(marker, temporal + marker), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--driver", type=Path,
                        help="Override the configured driver for a labeled TS05 sensitivity run")
    parser.add_argument("--profile", choices=("SMOKE", "ROUTINE", "FULL"), default="SMOKE")
    parser.add_argument("--amps", type=Path, default=Path("./amps"))
    parser.add_argument("--mpirun", default="mpirun")
    parser.add_argument("-np", type=int, default=None)
    parser.add_argument("-nt", type=int, default=None)
    parser.add_argument(
        "--output-root", type=Path, default=default_output_root() / "morphology",
        help="Morphology output directory beneath the shared study run root",
    )
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--skip-run", action="store_true",
                        help="Parse existing AMPS outputs without launching the executable")
    parser.add_argument("--keep", action="store_true",
                        help="Do not overwrite an existing per-epoch directory")
    parser.add_argument(
        "--mesh-layout", choices=("BATCHED", "PER_EPOCH", "STANDALONE"),
        default="BATCHED", type=str.upper,
        help=("BATCHED runs up to N epochs and both shells in one native "
              "SNAPSHOT_LIST process with one mesh allocation; PER_EPOCH uses "
              "one epoch/two shells per process; STANDALONE retains the "
              "one-epoch/one-shell equivalence baseline"),
    )
    parser.add_argument(
        "--epochs-per-batch", type=int, default=None,
        help=("Maximum epochs in one BATCHED AMPS process. Defaults to "
              "execution.epochs_per_batch"),
    )
    parser.add_argument(
        "--include-observation-epochs", action="store_true",
        help="Add the exact C9 and C10 profile midpoints to the morphology workset",
    )
    parser.add_argument(
        "--c9-output-root", type=Path,
        help="Stage the shared 475-km raw products for a subsequent C9 --skip-run",
    )
    parser.add_argument(
        "--c10-output-root", type=Path,
        help="Stage the shared 850-km raw products for a subsequent C10 --skip-run",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root, config = load_config(args.config)
    c10 = load_c10_module(root)
    driver_path = args.driver or (root / config["data"]["driver"])  # type: ignore[index]
    if not driver_path.is_absolute():
        driver_path = (root / driver_path).resolve()
    driver = read_driver(driver_path)
    landmarks = event_landmarks(driver, config)
    pamela_epochs: List[datetime] = []
    poes_epochs: List[datetime] = []
    if args.include_observation_epochs:
        pamela_epochs, poes_epochs = observation_epochs(root, config, args.profile)
    epochs = selected_epochs(
        config, args.profile, landmarks, pamela_epochs + poes_epochs
    )
    for epoch in epochs:
        interpolate_driver(driver, epoch)  # coverage and interpolation guard

    output_root = resolve_output_path(args.output_root)
    output_root.mkdir(parents=True, exist_ok=True)
    (output_root / "event_landmarks.json").write_text(json.dumps(
        {name: format_utc(value) for name, value in landmarks.items()}, indent=2
    ) + "\n", encoding="utf-8")

    commands: List[Dict[str, object]] = []
    boundaries: List[Dict[str, object]] = []
    cutoff_map_manifest: List[Dict[str, object]] = []
    driver_samples: List[Dict[str, object]] = []
    staged_products: List[Dict[str, object]] = []
    failures: List[str] = []
    model = config["model"]  # type: ignore[index]
    execution = config["execution"]  # type: ignore[index]
    altitudes = [float(value) for value in model["shell_altitudes_km"]]
    if len(set(altitudes)) != len(altitudes):
        raise SystemExit("configured shell altitudes must be unique")
    batch_size = (
        args.epochs_per_batch if args.epochs_per_batch is not None
        else int(execution.get(
            "epochs_per_batch", execution.get("epochs_per_batch_group", 16)
        ))
    )
    if batch_size < 1:
        raise SystemExit("--epochs-per-batch must be positive")
    mlt_bins = [3.0 * index for index in range(8)]
    n_case_slots = len(epochs) * len(altitudes)

    # A run specification represents exactly one MPI process.  In BATCHED mode
    # it owns several epochs; Mode3D allocates the mesh once outside its native
    # snapshot loop, then rebuilds the field and cutoff products for each epoch.
    # PER_EPOCH and STANDALONE remain intentionally independent controls.
    run_specs: List[Tuple[str, List[datetime], List[float], Path]] = []
    if args.mesh_layout == "BATCHED":
        for batch_index, batch_epochs in enumerate(chunks(epochs, batch_size)):
            tag = f"batch_{batch_index:04d}"
            run_specs.append((tag, batch_epochs, altitudes,
                              output_root / "shared_mesh" / tag))
    elif args.mesh_layout == "PER_EPOCH":
        for epoch in epochs:
            token = epoch.strftime("%Y%m%dT%H%M%S")
            run_specs.append((f"epoch_{token}", [epoch], altitudes,
                              output_root / "shared_mesh" / token))
    else:
        for epoch in epochs:
            token = epoch.strftime("%Y%m%dT%H%M%S")
            for altitude in altitudes:
                tag = f"alt_{altitude:g}km/{token}"
                run_specs.append((tag, [epoch], [altitude], output_root / tag))

    def raw_access_path(case_dir: Path, batch_epochs: Sequence[datetime],
                        epoch: datetime) -> Path:
        """Return the exact native filename for one epoch in a run spec."""

        if epoch not in batch_epochs:
            raise ValueError("requested epoch does not belong to this run specification")
        if args.mesh_layout == "BATCHED":
            index = list(batch_epochs).index(epoch)
            return case_dir / (
                "cutoff_3d_shells_access" + snapshot_suffix(index, epoch) + ".dat"
            )
        return case_dir / "cutoff_3d_shells_access.dat"

    def spec_is_complete(spec: Tuple[str, List[datetime], List[float], Path]) -> bool:
        _tag, spec_epochs, _spec_altitudes, case_dir = spec
        # Existence alone is insufficient after an interrupted MPI write: an
        # empty placeholder must trigger a fresh batch rather than be accepted
        # by --keep and fail later in the splitter. Full schema validation is
        # intentionally deferred to postprocessing so a parser-only fix can be
        # applied to preserved nonempty products without repeating AMPS.
        products = [raw_access_path(case_dir, spec_epochs, epoch)
                    for epoch in spec_epochs]
        return all(path.is_file() and path.stat().st_size > 0 for path in products)

    if args.keep and not (args.prepare_only or args.skip_run):
        n_launches = sum(not spec_is_complete(spec) for spec in run_specs)
    else:
        n_launches = len(run_specs)
    n_preexisting_complete = len(run_specs) - n_launches
    launch_index = 0
    print(
        "Morphology execution plan: "
        f"{len(epochs)} epoch(s) x {len(altitudes)} altitude(s) = "
        f"{n_case_slots} logical snapshot-shell product(s); layout={args.mesh_layout}; "
        f"epochs_per_launch={batch_size if args.mesh_layout == 'BATCHED' else 1}; "
        f"{n_launches} AMPS launch(es) required by this invocation",
        flush=True,
    )
    if args.include_observation_epochs:
        print(
            "Shared observation epochs: "
            f"PAMELA={len(pamela_epochs)}, POES/MetOp={len(poes_epochs)}, "
            f"unique total workset={len(epochs)}",
            flush=True,
        )

    amps = args.amps.expanduser()
    if not amps.is_absolute():
        amps = (Path.cwd() / amps).resolve()
    if not (args.prepare_only or args.skip_run):
        if not amps.is_file() or not os.access(amps, os.X_OK):
            raise SystemExit(f"AMPS executable is missing or not executable: {amps}")

    for epoch in epochs:
        driver_samples.append(driver_dict(interpolate_driver(driver, epoch)))

    pamela_epoch_set = set(pamela_epochs)
    poes_epoch_set = set(poes_epochs)
    c9_output_root = (resolve_output_path(args.c9_output_root)
                      if args.c9_output_root else None)
    c10_output_root = (resolve_output_path(args.c10_output_root)
                       if args.c10_output_root else None)

    for tag, spec_epochs, spec_altitudes, case_dir in run_specs:
        case_dir.mkdir(parents=True, exist_ok=True)
        controls = runner_namespace(config, args, spec_altitudes[0])
        controls.shell_altitudes_km = list(spec_altitudes)
        local_driver = case_dir / "ts05_driver.txt"
        if not args.skip_run:
            shutil.copy2(driver_path, local_driver)
            template = (
                root / "inputs" / "AMPS_PARAM_DEC2006_multishell.in"
                if len(spec_altitudes) > 1 or args.mesh_layout != "STANDALONE"
                else root / "inputs" /
                    f"AMPS_PARAM_DEC2006_{int(round(spec_altitudes[0]))}km.in"
            )
            generated_input = case_dir / "AMPS_PARAM_C10.in"
            c10.render_input(
                template, generated_input, spec_epochs[0], local_driver,
                controls, "GRIDDED"
            )
            if args.mesh_layout == "BATCHED":
                snapshot_file = case_dir / "snapshot_epochs.txt"
                write_snapshot_list(snapshot_file, spec_epochs)
                enable_snapshot_list(generated_input, snapshot_file.name)

        command = c10.command_for(controls, amps, "GRIDDED", spec_epochs[0])
        if args.mesh_layout == "BATCHED":
            # The epoch comes from SNAPSHOT_LIST_FILE.  Removing the single-
            # epoch CLI override makes the saved command express the temporal
            # contract unambiguously and prevents future precedence changes in
            # the parser from collapsing the batch to its first timestamp.
            epoch_option = command.index("--epoch")
            del command[epoch_option:epoch_option + 2]
        commands.append({
            "execution_layout": args.mesh_layout,
            "epoch_utc": [format_utc(epoch) for epoch in spec_epochs],
            "altitude_km": list(spec_altitudes),
            "cwd": str(case_dir), "command": command,
            "command_line": command_text(command),
            "reuses_mesh_across_shells": len(spec_altitudes) > 1,
            "reuses_mesh_across_epochs": len(spec_epochs) > 1,
            "reinitializes_field_each_epoch": True,
        })
        print(f"[{tag}] {command_text(command)}", flush=True)
        if args.prepare_only:
            continue

        if not args.skip_run:
            if args.keep and spec_is_complete((tag, spec_epochs, spec_altitudes, case_dir)):
                print(f"[{tag}] keeping complete existing AMPS batch", flush=True)
            else:
                # Remove only the precisely named products owned by this run
                # specification.  This prevents a stale successful snapshot
                # from masking a failed rerun of the same batch.
                for epoch in spec_epochs:
                    stale = raw_access_path(case_dir, spec_epochs, epoch)
                    if stale.exists():
                        stale.unlink()
                launch_index += 1
                completed_overall = n_preexisting_complete + launch_index - 1
                remaining_after_current = len(run_specs) - completed_overall - 1
                print(
                    "Morphology AMPS progress: "
                    f"completed={completed_overall}/{len(run_specs)} "
                    f"(preexisting={n_preexisting_complete}); "
                    f"starting={launch_index}/{n_launches} required now; "
                    f"remaining_after_current={remaining_after_current}; case={tag}; "
                    f"epochs={len(spec_epochs)}; shells={len(spec_altitudes)}",
                    flush=True,
                )
                return_code = c10.run_process(command, case_dir, case_dir / "AMPS.log")
                if return_code != 0:
                    message = f"{tag}: AMPS exited with {return_code}"
                    failures.append(message)
                    print(f"ERROR: {message}", file=sys.stderr, flush=True)
                    continue

        for epoch in spec_epochs:
            access_path = raw_access_path(case_dir, spec_epochs, epoch)
            if not access_path.exists():
                message = f"{tag}/{format_utc(epoch)}: missing {access_path.name}"
                failures.append(message)
                print(f"ERROR: {message}", file=sys.stderr, flush=True)
                continue
            split_paths = {
                altitude: case_dir / "split" / epoch.strftime("%Y%m%dT%H%M%S") /
                    f"cutoff_3d_shells_access_{altitude:g}km.dat"
                for altitude in spec_altitudes
            }
            try:
                split_multishell_access(
                    access_path, spec_altitudes, split_paths, c10
                )
            except Exception as exc:
                message = (
                    f"{tag}/{format_utc(epoch)}: multi-shell split failed: {exc}"
                )
                failures.append(message)
                print(f"ERROR: {message}", file=sys.stderr, flush=True)
                continue

            for altitude in spec_altitudes:
                controls = runner_namespace(config, args, altitude)
                shell_path = split_paths[altitude]
                try:
                    access = c10.parse_tecplot_shell_access(shell_path)
                    access = c10.select_common_access_band(
                        access, controls.access_abs_lat_min_deg,
                        controls.access_abs_lat_max_deg,
                    )
                    c10.add_aacgm_lat_mlt(access, epoch, altitude)
                    estimates, profile_rows = c10.estimate_access_t50_boundaries(
                        access, controls.rigidities_gv, mlt_bins, ("N", "S"), 8,
                        0.25, 0.66, 1.0,
                    )
                    unresolved = sum(row.access_state == 2 for row in access)
                    unresolved_fraction = unresolved / len(access) if access else 1.0
                    product_dir = (
                        output_root / f"alt_{altitude:g}km" /
                        epoch.strftime("%Y%m%dT%H%M%S")
                    )
                    product_dir.mkdir(parents=True, exist_ok=True)

                    # Invert the exact-rigidity access sequence independently at
                    # every geographic grid cell. The map is saved beside the
                    # boundary products so later visualization never has to
                    # reopen or reinterpret the large Tecplot trajectory table.
                    cutoff_map = derive_cutoff_rigidity_map(
                        access, controls.rigidities_gv
                    )
                    for row in cutoff_map:
                        row["epoch_utc"] = format_utc(epoch)
                        row["altitude_km"] = altitude
                    cutoff_map_path = product_dir / "cutoff_rigidity_map.csv"
                    write_csv(cutoff_map_path, cutoff_map)
                    status_counts = {
                        status: sum(row["cutoff_status"] == status
                                    for row in cutoff_map)
                        for status in (
                            "BRACKETED", "BELOW_RANGE", "ABOVE_RANGE",
                            "UNBRACKETED", "INCOMPLETE",
                        )
                    }
                    cutoff_map_manifest.append({
                        "epoch_utc": format_utc(epoch),
                        "altitude_km": altitude,
                        "map_path": cutoff_map_path.relative_to(output_root).as_posix(),
                        "n_spatial_cells": len(cutoff_map),
                        **{f"n_{name.lower()}": count
                           for name, count in status_counts.items()},
                        "sampled_rigidity_min_gv": min(controls.rigidities_gv),
                        "sampled_rigidity_max_gv": max(controls.rigidities_gv),
                    })
                    for estimate in estimates:
                        for mlt, boundary in sorted(estimate.boundary_by_mlt.items()):
                            boundaries.append({
                                "epoch_utc": format_utc(epoch),
                                "altitude_km": altitude,
                                "rigidity_gv": estimate.rigidity_gv,
                                "hemisphere": estimate.hemisphere,
                                "mlt_hour": mlt,
                                "boundary_aacgm_lat_deg": boundary,
                                "n_valid_mlt": estimate.n_valid_mlt,
                                "n_requested_mlt": estimate.n_requested_mlt,
                                "unresolved_access_fraction": unresolved_fraction,
                                "field_model": "IGRF+TS05",
                                "observation_operator": "VERTICAL_ACCESS_T50",
                            })
                    c10.write_dict_rows(product_dir / "snapshot_boundaries.csv", [
                        c10._estimate_row(estimate) for estimate in estimates
                    ])
                    c10.write_dict_rows(
                        product_dir / "snapshot_t50_profiles.csv", profile_rows
                    )

                    # Stage the exact same raw bytes at the legacy observation
                    # paths.  C9/C10 then run with --skip-run and apply their own
                    # reference-specific reducers and acceptance criteria.
                    if altitude == 475.0 and epoch in pamela_epoch_set and c9_output_root:
                        destination = (
                            observation_destination(c9_output_root, epoch)
                            / "cutoff_3d_shells_access.dat"
                        )
                        stage_single_shell_product(shell_path, destination)
                        receipt = write_shared_product_receipt(
                            destination, shell_path, "C9", epoch, altitude,
                            args.mesh_layout,
                        )
                        staged_products.append({
                            "consumer": "C9", "epoch_utc": format_utc(epoch),
                            "altitude_km": altitude, "source": str(shell_path),
                            "destination": str(destination),
                            "receipt": str(receipt),
                        })
                    if altitude == 850.0 and epoch in poes_epoch_set and c10_output_root:
                        destination = (
                            observation_destination(c10_output_root, epoch)
                            / "cutoff_3d_shells_access.dat"
                        )
                        stage_single_shell_product(shell_path, destination)
                        receipt = write_shared_product_receipt(
                            destination, shell_path, "C10", epoch, altitude,
                            args.mesh_layout,
                        )
                        staged_products.append({
                            "consumer": "C10", "epoch_utc": format_utc(epoch),
                            "altitude_km": altitude, "source": str(shell_path),
                            "destination": str(destination),
                            "receipt": str(receipt),
                        })
                except Exception as exc:  # keep other expensive cases usable
                    message = (
                        f"{tag}/{format_utc(epoch)}/{altitude:g}km: "
                        f"postprocessing failed: {exc}"
                    )
                    failures.append(message)
                    print(f"ERROR: {message}", file=sys.stderr, flush=True)

    (output_root / "command_inventory.json").write_text(
        json.dumps(commands, indent=2) + "\n", encoding="utf-8"
    )
    write_csv(output_root / "driver_at_model_epochs.csv", driver_samples)
    (output_root / "staged_observation_products.json").write_text(
        json.dumps(staged_products, indent=2) + "\n", encoding="utf-8"
    )
    if boundaries:
        write_csv(output_root / "morphology_boundaries.csv", boundaries)
    if cutoff_map_manifest:
        write_csv(
            output_root / "cutoff_rigidity_map_manifest.csv",
            cutoff_map_manifest,
        )
    result = {
        "study_id": config["study_id"], "profile": args.profile,
        "n_epochs": len(epochs), "n_altitudes": len(altitudes),
        "n_cases": n_case_slots, "n_amps_launches": len(run_specs),
        "n_amps_launches_required_this_invocation": n_launches,
        "n_preexisting_complete_launches": n_preexisting_complete,
        "mesh_layout": args.mesh_layout,
        "epochs_per_batch": batch_size if args.mesh_layout == "BATCHED" else 1,
        # Preserve the original manifest key as a compatibility alias.  It now
        # describes a real AMPS batch rather than directory-only grouping.
        "epochs_per_batch_group": batch_size if args.mesh_layout == "BATCHED" else 1,
        "mesh_reused_across_shells": args.mesh_layout in ("BATCHED", "PER_EPOCH"),
        "mesh_reused_across_epochs": (
            args.mesh_layout == "BATCHED" and any(len(item[1]) > 1 for item in run_specs)
        ),
        "magnetic_field_reinitialized_each_epoch": True,
        "n_boundary_rows": len(boundaries),
        "n_cutoff_rigidity_maps": len(cutoff_map_manifest),
        "n_staged_observation_products": len(staged_products),
        "prepare_only": args.prepare_only, "skip_run": args.skip_run,
        "failures": failures, "passed": not failures,
    }
    (output_root / "morphology_result.json").write_text(
        json.dumps(result, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(result, indent=2))
    return 0 if not failures else 1


if __name__ == "__main__":
    raise SystemExit(main())
