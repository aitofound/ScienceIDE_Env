#!/usr/bin/env python3
"""Run the C19A package checks without observational downloads or AMPS."""

from __future__ import annotations

import csv
import gzip
import math
import re
import py_compile
import shutil
import subprocess
import sys
import tempfile
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run(command):
    print("+", " ".join(str(value) for value in command), flush=True)
    completed = subprocess.run(command, cwd=str(ROOT), check=False)
    if completed.returncode != 0:
        raise SystemExit(completed.returncode)


def write_reference(path: Path) -> None:
    fields = [
        "utc", "spacecraft", "channel", "energy_min_mev", "energy_max_mev",
        "telemetry_head_east", "telemetry_head_west",
        "east_west_ratio", "log10_east_west_ratio",
        "east_flux_background_subtracted", "west_flux_background_subtracted",
        "flux_product_policy", "east_flux_variable", "west_flux_variable",
        "east_flux_correction_state", "west_flux_correction_state",
        "longitude_deg_east", "latitude_deg", "altitude_km", "position_source",
    ]
    rows = []
    for hour in (6, 7):
        utc = "2012-05-17T%02d:00:00Z" % hour
        for spacecraft, longitude in (("GOES13", -75.0), ("GOES15", -135.0)):
            for channel, lo, hi, ratio in (("P4", 15.0, 40.0, 0.50),
                                           ("P5", 38.0, 82.0, 0.70)):
                rows.append({
                    "utc": utc,
                    "spacecraft": spacecraft,
                    "channel": channel,
                    "energy_min_mev": lo,
                    "energy_max_mev": hi,
                    # Mimic the May-2012 yaw mapping: GOES13 physical-east stream is
                    # telemetry W, whereas GOES15 physical-east stream is telemetry E.
                    # This proves that attitude lookup is by actual detector ID rather
                    # than by the compatibility words EAST/WEST.
                    "telemetry_head_east": ("W" if spacecraft == "GOES13" else "E"),
                    "telemetry_head_west": ("E" if spacecraft == "GOES13" else "W"),
                    "east_west_ratio": ratio,
                    "log10_east_west_ratio": math.log10(ratio),
                    # P1.3 needs two positive physical-WEST channel intensities per
                    # epoch.  Values are synthetic but decrease with energy so the
                    # default OBSERVED_WEST fit is well-defined.
                    "east_flux_background_subtracted": (50.0 if channel == "P4" else 10.0) * ratio,
                    "west_flux_background_subtracted": 50.0 if channel == "P4" else 10.0,
                    "flux_product_policy": "UNCORRECTED",
                    "east_flux_variable": "%s_SYNTH_UNCOR_FLUX" % channel,
                    "west_flux_variable": "%s_SYNTH_UNCOR_FLUX" % channel,
                    "east_flux_correction_state": "UNCORRECTED",
                    "west_flux_correction_state": "UNCORRECTED",
                    "longitude_deg_east": longitude,
                    "latitude_deg": 0.0,
                    "altitude_km": 35786.0,
                    "position_source": "SYNTHETIC",
                })
    with gzip.open(path, "wt", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def write_smoke_sync_reference(path: Path) -> None:
    """Write a reference that exposes the historical asynchronous-SMOKE bug.

    GOES13 has exactly three epochs. GOES15 has the same three common epochs plus
    extra late-time samples. The former per-spacecraft SMOKE selector would choose
    a different GOES15 middle/last epoch and therefore create five unique field
    snapshots. The synchronized selector must instead keep only the three common
    epochs for both spacecraft.
    """
    write_reference(path)
    with gzip.open(path, "rt", newline="") as stream:
        reader = csv.DictReader(stream)
        fieldnames = list(reader.fieldnames or [])
        source_rows = list(reader)

    templates = {}
    for row in source_rows:
        templates.setdefault((row["spacecraft"], row["channel"]), row)

    epoch_map = {
        "GOES13": (
            "2012-05-17T06:00:00Z",
            "2012-05-17T06:30:00Z",
            "2012-05-17T07:00:00Z",
        ),
        "GOES15": (
            "2012-05-17T06:00:00Z",
            "2012-05-17T06:30:00Z",
            "2012-05-17T06:50:00Z",
            "2012-05-17T07:00:00Z",
            "2012-05-17T07:05:00Z",
        ),
    }

    rows = []
    for spacecraft, epochs in epoch_map.items():
        for epoch in epochs:
            for channel in ("P4", "P5"):
                row = dict(templates[(spacecraft, channel)])
                row["utc"] = epoch
                rows.append(row)

    with gzip.open(path, "wt", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def write_driver(path: Path) -> None:
    start = datetime(2012, 5, 17, 5, 55, tzinfo=timezone.utc)
    lines = [
        "# YYYY-MM-DDTHH:MM:SS Bx By Bz Vx Vy Vz Np Temp SYM-H "
        "IMFflag SWflag Tilt Pdyn W1 W2 W3 W4 W5 W6"
    ]
    for index in range(15):
        epoch = start + timedelta(minutes=5 * index)
        values = [1.0, 2.0, -3.0, -450.0, 0.0, 0.0, 5.0, 100000.0,
                  -20.0, 1.0, 1.0, 0.01 * index, 2.0,
                  0.1, 0.2, 0.3, 0.4, 0.5, 0.6]
        lines.append("%s %s" % (
            epoch.strftime("%Y-%m-%dT%H:%M:%S"),
            " ".join(str(value) for value in values)))
    path.write_text("\n".join(lines) + "\n")


def write_orientation(path: Path) -> None:
    """Write non-antipodal per-head attitude records for vector-aperture validation."""
    fields = [
        "utc", "spacecraft", "detector", "frame",
        "boresight_x", "boresight_y", "boresight_z",
        "aperture_north_x", "aperture_north_y", "aperture_north_z", "source",
    ]
    rows = []
    for hour in (6, 7):
        for spacecraft in ("GOES13", "GOES15"):
            # Deliberately make raw telemetry head W non-antipodal to head E to
            # prove that neither the runner nor AMPS derives one detector direction
            # by negating the other. The reference decides which head is numerator.
            for detector, bore in (("E", (0.0, 1.0, 0.0)),
                                   ("W", (0.15, -0.97, 0.18))):
                rows.append({
                    "utc": "2012-05-17T%02d:00:00Z" % hour,
                    "spacecraft": spacecraft, "detector": detector, "frame": "SM",
                    "boresight_x": bore[0], "boresight_y": bore[1], "boresight_z": bore[2],
                    "aperture_north_x": 0.0, "aperture_north_y": 0.0,
                    "aperture_north_z": 1.0, "source": "SYNTHETIC_C19_TEST",
                })
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader(); writer.writerows(rows)


def integration_dry_run() -> None:
    with tempfile.TemporaryDirectory(prefix="C19A_integration_") as temporary:
        root = Path(temporary)
        reference = root / "reference.csv.gz"
        driver = root / "driver.txt"
        output = root / "output"
        orientation = root / "orientation.csv"
        write_reference(reference)
        write_driver(driver)
        write_orientation(orientation)

        # Synchronized-SMOKE regression.  This synthetic reference deliberately has
        # extra GOES15 epochs that would have made the former per-spacecraft selector
        # create five unique field snapshots.  SMOKE must now choose only the three
        # epochs common to both spacecraft (with both P4 and P5 present), and the
        # batch manifest must contain both spacecraft at every retained snapshot.
        smoke_reference = root / "reference_smoke_sync.csv.gz"
        smoke_output = root / "integration_smoke_sync"
        write_smoke_sync_reference(smoke_reference)
        run([
            sys.executable, str(ROOT / "run_C19.py"),
            "--profile", "SMOKE", "--solver", "GRIDDED", "--models", "T05",
            "--reference", str(smoke_reference), "--driver", str(driver),
            "--output-root", str(smoke_output),
            "--amps", "./amps-not-required-for-dry-run", "-np", "1", "-nt", "1",
            "--dry-run",
        ])
        smoke_snapshot_files = list(smoke_output.glob("**/C19_snapshot_epochs.txt"))
        smoke_manifest_files = list(smoke_output.glob("**/C19_batch_manifest.csv"))
        if len(smoke_snapshot_files) != 1 or len(smoke_manifest_files) != 1:
            raise SystemExit("synchronized SMOKE dry-run did not produce one GRIDDED batch")
        smoke_snapshots = [
            line.strip() for line in smoke_snapshot_files[0].read_text().splitlines()
            if line.strip() and not line.startswith("#")
        ]
        expected_smoke_snapshots = [
            "2012-05-17T06:00:00",
            "2012-05-17T06:30:00",
            "2012-05-17T07:00:00",
        ]
        if smoke_snapshots != expected_smoke_snapshots:
            raise SystemExit(
                "SMOKE did not select first/middle/last common epochs: %s" %
                smoke_snapshots)
        smoke_manifest = list(csv.DictReader(smoke_manifest_files[0].open(newline="")))
        by_epoch = {}
        for row in smoke_manifest:
            by_epoch.setdefault(row["epoch"], set()).add(row["spacecraft"])
        if len(smoke_manifest) != 6 or any(
                spacecraft != {"GOES13", "GOES15"} for spacecraft in by_epoch.values()):
            raise SystemExit(
                "SMOKE batch manifest does not contain both spacecraft at all three common epochs")

        run([
            sys.executable, str(ROOT / "run_C19.py"),
            "--profile", "FULL",
            "--solver", "BOTH",
            "--models", "T05",
            "--reference", str(reference),
            "--driver", str(driver),
            "--output-root", str(output),
            "--amps", "./amps-not-required-for-dry-run",
            "-np", "2", "-nt", "2",
            "--dry-run",
        ])
        rendered = list(output.glob("**/AMPS_PARAM_C19.in"))
        trajectories = list(output.glob("**/C19_trajectory.txt"))
        # Four GRIDLESS cases remain independent, while all four GRIDDED cases share
        # one Mode3D process/mesh. The resulting five decks are the central batching
        # invariant protected by this integration test.
        if len(rendered) != 5 or len(trajectories) != 5:
            raise SystemExit(
                "C19A integration dry-run wrote %d inputs and %d trajectories; expected 5" %
                (len(rendered), len(trajectories)))
        aperture_files = list(output.glob("**/C19_directional_apertures.dat"))
        if len(aperture_files) != 5:
            raise SystemExit("C19A integration dry-run wrote %d aperture files; expected 5" %
                             len(aperture_files))
        for aperture_file in aperture_files:
            text = aperture_file.read_text()
            if not re.search(r"(?:^|_)EAST LOCAL_SM", text, re.MULTILINE) or not re.search(
                    r"(?:^|_)WEST LOCAL_SM", text, re.MULTILINE):
                raise SystemExit("SM_PROXY did not render generic LOCAL_SM aperture vectors: %s" %
                                 aperture_file)

        batch_inputs = [path for path in rendered if "/gridded/" in str(path).lower()]
        if len(batch_inputs) != 1:
            raise SystemExit("C19A dry-run did not create exactly one GRIDDED batch deck")
        batch_dir = batch_inputs[0].parent
        batch_trajectory = (batch_dir / "C19_trajectory.txt").read_text().splitlines()
        batch_snapshots = [line for line in
                           (batch_dir / "C19_snapshot_epochs.txt").read_text().splitlines()
                           if line and not line.startswith("#")]
        if len(batch_trajectory) != 4 or len(batch_snapshots) != 2:
            raise SystemExit(
                "GRIDDED batch did not retain four cases at two unique snapshots")
        batch_manifest = list(csv.DictReader(
            (batch_dir / "C19_batch_manifest.csv").open(newline="")))
        if len(batch_manifest) != 4:
            raise SystemExit("GRIDDED batch manifest does not contain four logical cases")
        expected_local_ids = {
            (row["epoch"], row["spacecraft"]): int(row["snapshot_local_location_id"])
            for row in batch_manifest
        }
        if set(expected_local_ids.values()) != {0, 1}:
            raise SystemExit("snapshot-local location IDs were not reset per epoch")
        batch_apertures = (batch_dir / "C19_directional_apertures.dat").read_text()
        for global_location_id in range(4):
            if "LOCATION=%d" % global_location_id not in batch_apertures:
                raise SystemExit(
                    "GRIDDED batch aperture file lacks LOCATION=%d" % global_location_id)
        for path in rendered:
            text = path.read_text()
            if re.search(r"__[A-Z0-9_]+__", text):
                raise SystemExit("unresolved input placeholder in %s" % path)
            if "DRIVER_FILE" not in text or str(driver.resolve()) not in text:
                raise SystemExit("generated input does not contain absolute driver path: %s" % path)
            # Production C19 now defaults to the optimized direct science product:
            # trace only the requested A(E,Omega) samples and preserve trace-limit
            # outcomes as UNRESOLVED. PENUMBRA_SCAN remains an explicit diagnostic mode.
            if not re.search(r"^CUTOFF_SEARCH_ALGORITHM\s+DIRECT_ACCESS\s*$",
                             text, re.MULTILINE):
                raise SystemExit("normal C19 dry-run did not select DIRECT_ACCESS: %s" % path)
            if not re.search(r"^CUTOFF_TRACE_LIMIT_POLICY\s+UNRESOLVED\s*$",
                             text, re.MULTILINE):
                raise SystemExit("normal C19 dry-run did not select UNRESOLVED policy: %s" % path)
            # GRIDDED and GRIDLESS must now receive the identical detector-response
            # rigidity list.  This is the key input-level invariant that makes BOTH an
            # apples-to-apples direct A(E,Omega) comparison instead of comparing two
            # different observables.
            if not re.search(r"^CUTOFF_RIGIDITY_LIST_GV\s+[0-9]", text, re.MULTILINE):
                raise SystemExit("C19 input did not contain direct-access rigidity list: %s" % path)
            # Adaptive DIRECT_ACCESS is the production default.  The explicit seed
            # list stays common to both solvers; C++ refines each sky direction
            # independently from those seeds.  Keeping these directives in both
            # rendered decks protects GRIDDED/GRIDLESS science equivalence.
            if not re.search(r"^CUTOFF_DIRECT_ACCESS_ADAPTIVE\s+T\s*$", text, re.MULTILINE):
                raise SystemExit("C19 input did not enable adaptive DIRECT_ACCESS: %s" % path)
            if not re.search(r"^CUTOFF_DIRECT_ACCESS_ADAPTIVE_MAX_DEPTH\s+6\s*$", text, re.MULTILINE):
                raise SystemExit("C19 input did not render adaptive max depth 6: %s" % path)
            if not re.search(r"^CUTOFF_DIRECT_ACCESS_ADAPTIVE_GUARD_DEPTH\s+1\s*$", text, re.MULTILINE):
                raise SystemExit("C19 input did not render adaptive guard depth 1: %s" % path)
            # GRIDLESS-specific parallel contract.  The normal runner must render a
            # shared-memory worker backend and leave the MPI dynamic chunk in AUTO
            # mode so the C++ resolver can size one fetch from the actual worker count.
            if "/gridless/" in str(path).lower():
                if not re.search(r"^GRIDLESS_PARALLEL\s+THREADS\s*$", text, re.MULTILINE):
                    raise SystemExit("GRIDLESS input did not enable THREADS: %s" % path)
                if not re.search(r"^GRIDLESS_THREADS\s+2\s*$", text, re.MULTILINE):
                    raise SystemExit("GRIDLESS input did not propagate -nt 2: %s" % path)
                if not re.search(r"^GRIDLESS_MPI_DYNAMIC_CHUNK\s+2\s*$", text, re.MULTILINE):
                    raise SystemExit("adaptive GRIDLESS input did not use worker-sized dynamic chunk: %s" % path)
            else:
                if not re.search(r"^TEMPORAL_MODE\s+SNAPSHOT_LIST\s*$", text, re.MULTILINE):
                    raise SystemExit("GRIDDED batch input did not enable SNAPSHOT_LIST: %s" % path)
                if not re.search(r"^SNAPSHOT_LIST_FILE\s+C19_snapshot_epochs\.txt\s*$",
                                 text, re.MULTILINE):
                    raise SystemExit("GRIDDED batch input does not reference its snapshot list: %s" % path)

        # Single-workflow regression.  P0/P1/P2 are implementation history, not
        # mutually exclusive runner modes.  The ordinary dry run above must therefore
        # use the current production settings directly.
        import json
        commands_path = output / "C19_commands.json"
        if not commands_path.exists():
            raise SystemExit("single-workflow dry-run did not write C19_commands.json")
        commands = json.loads(commands_path.read_text())
        if not commands:
            raise SystemExit("single-workflow dry-run wrote an empty command list")
        for record in commands:
            command = record["command"]
            joined = " ".join(str(value) for value in command)
            if "-cutoff-search DIRECT_ACCESS" not in joined:
                raise SystemExit("current workflow did not select optimized DIRECT_ACCESS")
            if "-cutoff-upper-scan-n" in joined:
                raise SystemExit(
                    "DIRECT_ACCESS command unexpectedly requests PENUMBRA upper-scan work")
            if record.get("cutoff_search_algorithm") != "DIRECT_ACCESS":
                raise SystemExit("command provenance did not record DIRECT_ACCESS")
            if record.get("adaptive_access") is not True:
                raise SystemExit("production command provenance did not enable adaptive access")
            if int(record.get("adaptive_access_seed_points", 0)) != 12:
                raise SystemExit("production command did not use the 12-point adaptive seed grid")
            for token in (
                    "-cutoff-direct-access-adaptive T",
                    "-cutoff-direct-access-adaptive-max-depth 6",
                    "-cutoff-direct-access-adaptive-guard-depth 1"):
                if token not in joined:
                    raise SystemExit("adaptive DIRECT_ACCESS command is missing %s" % token)
            if "-cutoff-dirmap-coverage VECTOR_APERTURES" not in joined:
                raise SystemExit("current workflow command did not select VECTOR_APERTURES coverage")
            if "-cutoff-dirmap-aperture-file C19_directional_apertures.dat" not in joined:
                raise SystemExit("current workflow command lacks generic aperture-vector file")
            if int(record.get("n_direct_access_rigidities", 0)) <= 0:
                raise SystemExit("current %s workflow did not request direct A(E,Omega)" % record["solver"])
            if record["solver"] == "GRIDDED":
                if record.get("execution_mode") != "MODE3D_SNAPSHOT_LIST_BATCH":
                    raise SystemExit("GRIDDED command was not recorded as one snapshot-list batch")
                if int(record.get("mesh_initializations_expected", 0)) != 1:
                    raise SystemExit("GRIDDED batch does not record one expected mesh allocation")
                if int(record.get("case_count", 0)) != 4 or int(record.get("snapshot_count", 0)) != 2:
                    raise SystemExit("GRIDDED batch command has incorrect case/snapshot counts")
                if "-mode3d-mesh-res-earth-re 0.025" not in joined:
                    raise SystemExit("current GRIDDED workflow did not use 0.025 Re near-Earth mesh")
                if "-mode3d-mesh-res-boundary-re 1.0" not in joined:
                    raise SystemExit("current GRIDDED workflow did not use 1.0 Re boundary mesh")
            elif record["solver"] == "GRIDLESS":
                if "-gridless-parallel THREADS" not in joined:
                    raise SystemExit("current GRIDLESS workflow did not request THREADS")
                if "-gridless-threads 2" not in joined:
                    raise SystemExit("current GRIDLESS workflow did not propagate -nt 2")
                # Adaptive DIRECT_ACCESS uses one heavy direction task per local
                # worker.  With -nt 2 the runner therefore requests chunk=2 rather
                # than the generic dense AUTO heuristic (~4 tasks/worker), which would
                # hoard expensive adaptive directions on a rank and worsen the tail.
                if "-gridless-mpi-dynamic-chunk 2" not in joined:
                    raise SystemExit("adaptive GRIDLESS workflow did not use worker-sized MPI chunk")

        # The generated parameter decks must use the finest P2.1 production sky grid.
        for path in rendered:
            text = path.read_text()
            if not re.search(r"^DIRMAP_LON_RES\s+2.5\s*$", text, re.MULTILINE):
                raise SystemExit("current workflow did not use 2.5-degree longitude grid: %s" % path)
            if not re.search(r"^DIRMAP_LAT_RES\s+2.5\s*$", text, re.MULTILINE):
                raise SystemExit("current workflow did not use 2.5-degree latitude grid: %s" % path)
            if not re.search(r"^DIRMAP_COVERAGE\s+VECTOR_APERTURES\s*$", text, re.MULTILINE):
                raise SystemExit("current workflow did not select generic vector-aperture coverage: %s" % path)
            if not re.search(r"^DIRMAP_APERTURE_FILE\s+C19_directional_apertures\.dat\s*$", text, re.MULTILINE):
                raise SystemExit("current workflow did not reference its aperture-vector file: %s" % path)

        # Confirm that the public CLI no longer exposes historical P0/P2/legacy-fold
        # selectors. --cutoff-search is intentionally public again, but only for the
        # two current products DIRECT_ACCESS and PENUMBRA_SCAN.
        help_text = subprocess.check_output(
            [sys.executable, str(ROOT / "run_C19.py"), "--help"],
            cwd=str(ROOT), text=True)
        for obsolete in ("--p0-diagnostic", "--p2-diagnostic",
                         "--trace-limit-policy", "--response-fold"):
            if obsolete in help_text:
                raise SystemExit("obsolete alternate-mode option still exposed: %s" % obsolete)
        if "--cutoff-search" not in help_text or "DIRECT_ACCESS" not in help_text or "PENUMBRA_SCAN" not in help_text:
            raise SystemExit("current DIRECT_ACCESS/PENUMBRA_SCAN selector is missing from runner CLI")
        if "--direction-coverage" not in help_text:
            raise SystemExit("directional coverage selector is missing from runner CLI")
        if "--access-angular-points" not in help_text:
            raise SystemExit("angular point-count resolution selector is missing from runner CLI")
        if "--access-energy-points" not in help_text:
            raise SystemExit("energy point-count resolution selector is missing from runner CLI")
        if "--publication" not in help_text:
            raise SystemExit("diagnostic-free publication plot selector is missing from runner CLI")
        if "--gridded-batch" not in help_text:
            raise SystemExit("GRIDDED mesh-reuse/compatibility selector is missing from runner CLI")
        if "--max-discrete-transition-fraction" not in help_text:
            raise SystemExit("finite-rigidity-grid uncertainty control is missing from runner CLI")
        if "epead_response_C19_uncorrected_extended.csv" not in help_text:
            # argparse normally exposes the default only when help includes it, so test
            # the source constant as a second layer below if formatting changes.
            runner_text = (ROOT / "run_C19.py").read_text()
            if 'DEFAULT_RESPONSE = SCRIPT_DIR / "data" / "epead_response_C19_uncorrected_extended.csv"' not in runner_text:
                raise SystemExit("runner no longer defaults to the extended uncorrected response")

        # End-to-end resolution CLI regression.  Verify the generated input rather
        # than only argparse so --access-angular-points cannot become a no-op again.
        angular_output = root / "integration_angular_points"
        run([
            sys.executable, str(ROOT / "run_C19.py"),
            "--profile", "SMOKE", "--solver", "GRIDDED", "--models", "T05",
            "--reference", str(reference), "--driver", str(driver),
            "--output-root", str(angular_output),
            "--amps", "./amps-not-required-for-dry-run", "-np", "1", "-nt", "1",
            "--access-angular-points", "288", "--access-energy-points", "64",
            "--dry-run",
        ])
        angular_decks = list(angular_output.glob("**/AMPS_PARAM_C19.in"))
        if not angular_decks:
            raise SystemExit("angular point-count dry-run produced no input deck")
        for deck in angular_decks:
            deck_text = deck.read_text()
            if not re.search(r"^DIRMAP_LON_RES\s+1\.25\s*$", deck_text, re.MULTILINE):
                raise SystemExit("--access-angular-points 288 did not render 1.25-deg longitude grid")
            if not re.search(r"^DIRMAP_LAT_RES\s+1\.25\s*$", deck_text, re.MULTILINE):
                raise SystemExit("--access-angular-points 288 did not render 1.25-deg latitude grid")
        observation_lookup = angular_output / "C19_observation_ids.csv"
        if not observation_lookup.exists():
            raise SystemExit("dry-run did not write C19_observation_ids.csv")
        lookup_text = observation_lookup.read_text()
        if "observation_id,utc" not in lookup_text or "T01," not in lookup_text:
            raise SystemExit("observation-ID lookup is missing compact Txx labels")

        # Exact attitude regression: the preferred orientation schema supplies one
        # arbitrary vector per *actual telemetry head*. Head W is deliberately non-
        # antipodal to E. The reference maps physical numerator/denominator streams to
        # those IDs, and the generated AMPS aperture file must preserve both vectors.
        attitude_output = root / "integration_attitude_vectors"
        run([
            sys.executable, str(ROOT / "run_C19.py"),
            "--profile", "SMOKE", "--solver", "GRIDDED", "--models", "T05",
            "--reference", str(reference), "--driver", str(driver),
            "--output-root", str(attitude_output),
            "--amps", "./amps-not-required-for-dry-run", "-np", "1", "-nt", "1",
            "--detector-orientation-source", "FILE",
            "--detector-orientation-file", str(orientation), "--dry-run",
        ])
        vector_files = list(attitude_output.glob("**/C19_directional_apertures.dat"))
        if not vector_files:
            raise SystemExit("FILE attitude dry-run did not write a generic aperture file")
        vector_text = vector_files[0].read_text()
        if "_E SM 0 1 0" not in vector_text:
            raise SystemExit("FILE attitude telemetry-head E vector was not propagated")
        west_lines = [line for line in vector_text.splitlines()
                      if re.match(r"^L\d+_W SM ", line)]
        if not west_lines:
            raise SystemExit("FILE attitude telemetry-head W vector was not propagated")
        for west_line in west_lines:
            parts = west_line.split()
            west_bore = tuple(float(value) for value in parts[2:5])
            # Head E is exactly (0,1,0); a derived antipode would be (0,-1,0).
            if all(abs(a-b) < 1.0e-12 for a, b in zip(west_bore, (0.0, -1.0, 0.0))):
                raise SystemExit("FILE attitude head W vector was incorrectly derived as -head E")

        # FULL_SPHERE remains an explicit alternative to the optimized default.
        full_output = root / "integration_full_sphere"
        run([
            sys.executable, str(ROOT / "run_C19.py"),
            "--profile", "SMOKE", "--solver", "GRIDDED", "--models", "T05",
            "--reference", str(reference), "--driver", str(driver),
            "--output-root", str(full_output),
            "--amps", "./amps-not-required-for-dry-run", "-np", "1", "-nt", "1",
            "--direction-coverage", "FULL_SPHERE", "--dry-run",
        ])
        full_inputs = list(full_output.glob("**/AMPS_PARAM_C19.in"))
        if not full_inputs:
            raise SystemExit("FULL_SPHERE dry-run did not render an AMPS input")
        if not all(re.search(r"^DIRMAP_COVERAGE\s+FULL_SPHERE\s*$",
                             path.read_text(), re.MULTILINE) for path in full_inputs):
            raise SystemExit("FULL_SPHERE runner option was not propagated into input decks")

        # Backward-compatibility regression. The optimization is additive: OFF must
        # retain the historical one-process-per-spacecraft-epoch layout and ordinary
        # TEMPORAL_MODE=SNAPSHOT behavior without requiring a snapshot-list file.
        legacy_output = root / "integration_gridded_batch_off"
        run([
            sys.executable, str(ROOT / "run_C19.py"),
            "--profile", "FULL", "--solver", "GRIDDED", "--models", "T05",
            "--reference", str(reference), "--driver", str(driver),
            "--output-root", str(legacy_output),
            "--amps", "./amps-not-required-for-dry-run", "-np", "1", "-nt", "1",
            "--gridded-batch", "OFF", "--dry-run",
        ])
        legacy_inputs = list(legacy_output.glob("**/AMPS_PARAM_C19.in"))
        if len(legacy_inputs) != 4:
            raise SystemExit("--gridded-batch OFF did not render four independent cases")
        if not all(re.search(r"^TEMPORAL_MODE\s+SNAPSHOT\s*$",
                             path.read_text(), re.MULTILINE) for path in legacy_inputs):
            raise SystemExit("--gridded-batch OFF changed legacy SNAPSHOT semantics")


def validate_committed_inputs() -> None:
    required_directives = {
        "CALC_TARGET", "FIELD_EVAL_METHOD", "CUTOFF_EMIN", "CUTOFF_EMAX",
        "CUTOFF_NENERGY", "CUTOFF_SEARCH_ALGORITHM",
        "CUTOFF_TRACE_LIMIT_POLICY", "CUTOFF_UPPER_SCAN_N", "CUTOFF_RIGIDITY_LIST_GV",
        "CUTOFF_MAX_TRAJ_TIME", "DIRECTIONAL_MAP",
        "DIRMAP_LON_RES", "DIRMAP_LAT_RES", "DIRMAP_COVERAGE",
        "DIRMAP_APERTURE_FILE",
        "SPECIES", "CHARGE",
        "MASS_AMU", "FIELD_MODEL", "EPOCH", "DRIVER_FILE",
        "DOMAIN_X_MIN", "DOMAIN_X_MAX", "DOMAIN_Y_MIN", "DOMAIN_Y_MAX",
        "DOMAIN_Z_MIN", "DOMAIN_Z_MAX", "R_INNER", "SPECTRUM_TYPE",
        "SPEC_GAMMA", "OUTPUT_MODE", "TRAJ_FRAME", "TRAJ_FILE",
        "OUTPUT_COORDS", "DT_TRACE", "MAX_STEPS", "MAX_TRACE_TIME",
        "MAX_TRACE_DISTANCE", "TRAP_DETECTION",
        "TRAP_DRIFT_DETECTION", "TRAP_MIN_DRIFT_REVOLUTIONS",
        "TRAP_DRIFT_RADIAL_GROWTH_TOL_RE", "TRAP_DRIFT_RADIAL_REL_TOL",
        "TRAP_DRIFT_LATITUDE_TOL", "TRAP_DRIFT_PITCH_COS2_TOL",
        "TRAP_DRIFT_PROFILE_BINS", "TRAP_DRIFT_MIN_PROFILE_COVERAGE",
        "TRAP_DRIFT_MIN_MATCHED_BIN_FRACTION", "TRAP_ENERGY_REL_TOL",
    }
    for name in ("AMPS_PARAM_C19_gridless.in", "AMPS_PARAM_C19_mode3d.in"):
        path = ROOT / name
        text = path.read_text()
        if re.search(r"__[A-Z0-9_]+__", text):
            raise SystemExit("committed input contains a macro placeholder: %s" % path)
        directives = set()
        for raw in text.splitlines():
            stripped = raw.strip()
            if stripped and not stripped.startswith(("!", "#")):
                directives.add(stripped.split(None, 1)[0].upper())
        missing = sorted(required_directives - directives)
        if missing:
            raise SystemExit("%s lacks explicit directive(s): %s" %
                             (path, ", ".join(missing)))

        # C19 deliberately disables the historical fixed path-length cap.  A non-zero
        # constant MAX_TRACE_DISTANCE gives high-energy protons a shorter physical trace
        # time than low-energy protons and was a major source of unresolved EAST access.
        values = {}
        for raw in text.splitlines():
            stripped = raw.strip()
            if stripped and not stripped.startswith(("!", "#")):
                parts = stripped.split(None, 1)
                if len(parts) == 2:
                    values[parts[0].upper()] = parts[1].split("!", 1)[0].strip()
        if float(values["MAX_TRACE_DISTANCE"]) != 0.0:
            raise SystemExit("%s must keep C19 MAX_TRACE_DISTANCE disabled by default" % path)
        if values["TRAP_DRIFT_DETECTION"].upper() not in ("T", "TRUE", "1", "YES", "ON"):
            raise SystemExit("%s must enable C19 drift trapping by default" % path)
        if int(values["TRAP_MIN_DRIFT_REVOLUTIONS"]) < 3:
            raise SystemExit("%s must require at least three drift revolutions for two consecutive recurrence comparisons" % path)
        if name == "AMPS_PARAM_C19_mode3d.in":
            for temporal_directive in ("TEMPORAL_MODE", "SNAPSHOT_LIST_FILE"):
                if temporal_directive not in directives:
                    raise SystemExit(
                        "%s lacks batching directive %s" % (path, temporal_directive))
    trajectory = ROOT / "C19_trajectory.txt"
    if not trajectory.exists() or not trajectory.read_text().strip():
        raise SystemExit("committed default C19_trajectory.txt is missing or empty")

    # The default C19 observational reference is UNCORRECTED, so its synthetic detector
    # model must not silently stop at the nominal P5 82-MeV boundary.  Protect the
    # committed response contract here without importing run_C19.py: both documented
    # high-energy secondary components and the 190-MeV P5 upper support must remain.
    response_path = ROOT / "data" / "epead_response_C19_uncorrected_extended.csv"
    if not response_path.exists():
        raise SystemExit("extended uncorrected EPEAD response is missing: %s" % response_path)
    with response_path.open(newline="") as stream:
        response_rows = list(csv.DictReader(stream))
    secondary = [row for row in response_rows
                 if row.get("response_component", "").upper().startswith("SECONDARY")]
    if not secondary:
        raise SystemExit("extended EPEAD response contains no secondary proton components")
    p5_hi = max(float(row["energy_max_mev"]) for row in response_rows
                if row.get("channel", "").upper() == "P5"
                and float(row.get("relative_response", 0.0)) > 0.0)
    if not math.isclose(p5_hi, 190.0, rel_tol=0.0, abs_tol=1.0e-12):
        raise SystemExit("extended P5 response no longer reaches 190 MeV: %.12g" % p5_hi)



def validate_directional_coverage_source_contract() -> None:
    """Static regression checks for the AMPS-side aperture selector.

    The package self-tests run without building AMPS, so they cannot execute the C++
    trajectory scheduler.  These checks still protect the cross-layer contract that is
    easiest to break during future refactors: parser fields, generic cutoff CLI override,
    and both Mode3D/gridless compact full-grid-id mappings must remain present together.
    """
    src_earth = ROOT.parents[1]
    checks = {
        src_earth / "util" / "amps_param_parser.cpp": (
            "DIRMAP_COVERAGE", "DIRMAP_APERTURE_FILE",
            "DIRMAP_APERTURE", "VECTOR_APERTURES"),
        src_earth / "util" / "cutoff_cli.cpp": (
            "-cutoff-dirmap-coverage",
            "-cutoff-dirmap-aperture-file",
            "-cutoff-dirmap-aperture"),
        src_earth / "3d" / "CutoffRigidityMode3D.cpp": (
            "ApplyDirectionalMapCoverage3D", "VECTOR_APERTURES",
            "LOCAL_SM", "fullGridCellIds", "selectedCellIdsByLocation",
            "DirectionalMapLocationCellId3D", "locationTaskOffsets",
            "locationDirectionalCellCounts", "directionalDirectAccess",
            "DIRECT_ACCESS: skipped scalar cutoff"),
        src_earth / "gridless" / "CutoffRigidityGridless.cpp": (
            "VECTOR_APERTURES", "LOCAL_SM", "dirMapFullCellIds",
            "DIRMAP coverage", "TASK_DIRACCESS",
            "saveDirectionalAccessStates",
            "cutoff_gridless_dir_access_point_",
            "ClassifyCutoffSampleDetailed", "directAccessOnly",
            "DIRECT_ACCESS: skipped scalar cutoff"),
    }
    for path, needles in checks.items():
        text = path.read_text(errors="replace")
        for needle in needles:
            if needle not in text:
                raise SystemExit("directional-coverage source contract missing %r in %s" %
                                 (needle, path))

    # Location-aware Mode3D regression.  A combined C19 aperture file may contain
    # LOCATION-qualified records for several spacecraft.  The solver must preserve a
    # per-location cell list and decode variable task counts through a prefix table;
    # reverting to taskId/tasksPerLocation would silently recreate the four-lobe union
    # at every spacecraft and the associated Cartesian-product trajectory overhead.
    mode3d_text = (src_earth / "3d" / "CutoffRigidityMode3D.cpp").read_text(
        errors="replace")
    forbidden_legacy_decode = "taskId/tasksPerLocation"
    if forbidden_legacy_decode in mode3d_text:
        raise SystemExit(
            "Mode3D directional scheduler regressed to fixed tasksPerLocation decoding")
    for required in (
            "cfg.selectedCellIdsByLocation.assign",
            "DirectionalMapLocationCellCount3D(dirMapCfg,locationIndex)",
            "taskId-locationTaskOffsets",
            "DirectionalMapLocationCellId3D(",
            "dirMapCfg,globalIdx,localCellOrdinal"):
        if required not in mode3d_text:
            raise SystemExit(
                "Mode3D location-aware directional scheduling is missing %r" % required)

    # GRIDLESS execution contract: standalone -mode gridless must return before the
    # historical mesh initialization path, and cutoff/direct-access work must contain
    # the same local THREADS/OPENMP machinery used by Mode3D.  These are static checks
    # because package tests intentionally do not link the full AMPS executable.
    main_text = (src_earth / "main.cpp").read_text(errors="replace")
    gridless_begin = main_text.find('if (m=="GRIDLESS")')
    mode3d_begin = main_text.find('if (m=="3D")', gridless_begin)
    if gridless_begin < 0 or mode3d_begin < 0:
        raise SystemExit("could not isolate standalone GRIDLESS dispatch in main.cpp")
    gridless_dispatch = main_text[gridless_begin:mode3d_begin]
    if re.search(r"^\s*amps_init_mesh\s*\(", gridless_dispatch, re.MULTILINE):
        raise SystemExit("standalone GRIDLESS dispatch must not call amps_init_mesh()")
    if "AMR mesh initialization: SKIPPED" not in gridless_dispatch:
        raise SystemExit("standalone GRIDLESS no-mesh invariant is not documented/logged")

    gridless_source = (src_earth / "gridless" / "CutoffRigidityGridless.cpp").read_text(errors="replace")
    for needle in (
            "ResolveGridlessParallelBackend_", "ResolveGridlessThreadCount_",
            "ApplyWideAffinityForGridlessThreadsOnce_", "std::vector<std::thread>",
            "ProcessMappedTaskRange", "InstallSharedModelState",
            "UsePreinstalledSharedModelState", "oneFrozenFieldSnapshot",
            "ResolveMpiDynamicChunk(prm,gridlessThreadCount,totalTasks)",
            # Live-progress contract: THREADS workers publish successful task
            # completion through a local atomic while the rank/main thread polls and
            # performs the MPI RMA update.  This prevents the bar from waiting for an
            # entire dynamic scheduler chunk to join.
            "completedInBatch.fetch_add", "progressPollInterval",
            "PublishCompletedTasks", "activeWorkers.load",
            # Quiet-progress contract: polling stays fine-grained, but stdout must not
            # repeat an unchanged task count every second and startup ETA is gated.
            "doneTasks <= doneLast", "minTaskDelta", "etaMinTasks",
            "(LocEq ", "rank 0/global over"):
        if needle not in gridless_source:
            raise SystemExit("GRIDLESS threading source contract missing %r" % needle)

    # CLI application must be common to GRIDLESS and Mode3D; otherwise the parser can
    # accept -gridless-parallel/-gridless-threads but GRIDLESS still runs serially.
    for needle in ("p.mode3d.densityParallelBackend = backend",
                   "p.mode3d.densityThreads = cli.densityThreads"):
        common_begin = main_text.find("bool ApplyCommonBackwardCli")
        common_end = main_text.find("return true;", common_begin)
        if needle not in main_text[common_begin:common_end]:
            raise SystemExit("common backward CLI does not apply GRIDLESS worker setting %r" % needle)

    # GRIDLESS science-equivalence contract.  C19 must require the same direct
    # A(E,Omega) product from both solvers; a future refactor must not silently
    # restore the old effective-cutoff-only GRIDLESS observational path.
    runner_text = (ROOT / "run_C19.py").read_text(errors="replace")
    for needle in (
            "cutoff_gridless_dir_access_point_0000.dat",
            "parse_directional_access(direct_path)",
            "direction_map_from_access_cube(access_cube)",
            "direct A(E,Omega) output is required but missing",
            "n_direct_access_rigidities"):
        if needle not in runner_text:
            raise SystemExit("GRIDLESS direct-access runner contract missing %r" % needle)

    parser_source = (src_earth / "util" / "amps_param_parser.cpp").read_text(errors="replace")
    if 'p.cutoff.searchAlgorithm="DIRECT_ACCESS"' not in parser_source:
        raise SystemExit("AMPS parameter parser no longer preserves DIRECT_ACCESS as a distinct algorithm")

    # Adaptive DIRECT_ACCESS cross-layer contract. The implementation is intentionally
    # shared between Mode3D and GRIDLESS: both build the same deterministic candidate
    # tree and call the same state-driven refinement helper. Sparse -1 candidate slots
    # are omitted only by adaptive writers; dense mode still treats them as errors.
    adaptive_header = (src_earth / "util" / "AdaptiveDirectAccess.h").read_text(errors="replace")
    for needle in ("BuildAdaptiveDirectAccessGrid",
                   "EvaluateAdaptiveDirectAccessDirection",
                   "guardProbe", "visibleAmbiguity",
                   "AdaptiveDirectAccessMidpointGV"):
        if needle not in adaptive_header:
            raise SystemExit("adaptive direct-access helper contract missing %r" % needle)
    for source in (src_earth / "3d" / "CutoffRigidityMode3D.cpp",
                   src_earth / "gridless" / "CutoffRigidityGridless.cpp"):
        text = source.read_text(errors="replace")
        for needle in ("AdaptiveDirectAccess.h",
                       "BuildAdaptiveDirectAccessGrid",
                       "EvaluateAdaptiveDirectAccessDirection",
                       "directAccessAdaptiveGuardDepth",
                       "if (state<0 && adaptiveSparse) continue"):
            if needle not in text:
                raise SystemExit("adaptive direct-access producer contract missing %r in %s" %
                                 (needle, source))
    adaptive_wiring = {
        src_earth / "util" / "amps_param_parser.cpp": "directAccessAdaptive",
        src_earth / "util" / "cutoff_cli.cpp": "cutoffDirectAccessAdaptive",
        src_earth / "main.cpp": "directAccessAdaptive",
    }
    for source, needle in adaptive_wiring.items():
        text = source.read_text(errors="replace")
        if needle not in text:
            raise SystemExit("adaptive direct-access configuration is not wired through %s" % source)

    # Regression for a subtle but critical distinction: DIRECT_ACCESS is a C19
    # point/trajectory product, whereas RIGIDITY_LIST is the historical shell product.
    # CLI normalization must never map DIRECT_ACCESS back to RIGIDITY_LIST.
    common_begin = main_text.find("bool ApplyCommonBackwardCli")
    common_end = main_text.find("return true;", common_begin)
    common_cli = main_text[common_begin:common_end]
    if ('alg=="DIRECT_ACCESS"' not in common_cli or
            not re.search(r'p\.cutoff\.searchAlgorithm\s*=\s*"DIRECT_ACCESS"', common_cli)):
        raise SystemExit("common cutoff CLI no longer preserves DIRECT_ACCESS token")

    # Both C++ producers must advertise the exact same public access-state columns.
    # File stems differ for backward-compatible solver naming, but post-processing is
    # intentionally schema-agnostic.
    direct_columns = ("lon_deg", "lat_deg", "rigidity_GV", "energy_MeV",
                      "access_state", "allowed", "unresolved")
    for source in (src_earth / "3d" / "CutoffRigidityMode3D.cpp",
                   src_earth / "gridless" / "CutoffRigidityGridless.cpp"):
        text = source.read_text(errors="replace")
        for column in direct_columns:
            if column not in text:
                raise SystemExit("direct-access column %r missing from %s" %
                                 (column, source))

    # Geometry sanity check for the documented C19 default.  This mirrors the
    # local SM_PROXY aperture test at a representative equatorial GEO position.
    # The exact retained count varies slightly with spacecraft longitude/grid phase,
    # but it must be far smaller than the 10,512-cell 2.5-degree full sphere while
    # remaining of order 1.7k cells.
    lon_res = lat_res = 2.5
    n_lon = round(360.0 / lon_res)
    n_lat = round(180.0 / lat_res) + 1
    full = n_lon * n_lat
    radial = (1.0, 0.0, 0.0)
    east = (0.0, 1.0, 0.0)
    horizontal = radial
    vertical = (0.0, 0.0, 1.0)
    selected = 0
    for j in range(n_lat):
        lat = -90.0 + lat_res * j
        clat = math.cos(math.radians(lat))
        for i in range(n_lon):
            lon = lon_res * i
            arrival = (clat * math.cos(math.radians(lon)),
                       clat * math.sin(math.radians(lon)),
                       math.sin(math.radians(lat)))
            look = tuple(-value for value in arrival)
            inside = False
            for sign in (1.0, -1.0):
                boresight = tuple(sign * value for value in east)
                forward = sum(a*b for a, b in zip(look, boresight))
                if forward <= 0.0:
                    continue
                ah = math.degrees(math.atan2(
                    sum(a*b for a, b in zip(look, horizontal)), forward))
                av = math.degrees(math.atan2(
                    sum(a*b for a, b in zip(look, vertical)), forward))
                if (ah/30.0)**2 + (av/60.0)**2 <= 1.0 + 1.0e-12:
                    inside = True
                    break
            if inside:
                selected += 1
    if full != 10512 or not (1600 <= selected <= 1900):
        raise SystemExit("unexpected generic local-vector aperture geometry: selected=%d full=%d" %
                         (selected, full))


def validate_direct_plot_contract() -> None:
    """Protect the calculated/accepted DIRECT_ACCESS plotting separation.

    This is a cheap source-contract regression in addition to run_C19.py's executable
    synthetic self-test.  It catches accidental removal of the shared selector or a
    return to the old status-only scatter/parity filtering before an expensive C19 run
    is attempted.
    """
    text = (ROOT / "run_C19.py").read_text()
    required = (
        "direct_calculated_log10_east_west_ratio",
        "direct_bound_midpoint_log10_east_west_ratio",
        "def direct_plot_groups",
        "DIRECT_CALCULATED_NOT_ACCEPTED",
        '"plot_consistency": plot_consistency',
        "direct_convergence_status",
        # Core comparison plots must remain recoverable even if the richer primary
        # plot family encounters a matplotlib/data-shape exception.
        "def make_core_comparison_fallback_plots",
        "core_comparison_fallback",
        # Reference/model timestamps must share datetime axis units.
        "observed_times = [parse_utc(row.utc) for row in reference_panel]",
        # Publication/traceability contract: point labels must map to exact UTC and
        # every PNG figure must have a vector EPS companion.
        "def observation_id_map",
        "def annotate_observation_point",
        "C19_observation_ids.csv",
        "def save_figure_png_eps",
        'with_suffix(".eps")',
        # Opt-in manuscript figures must be a separate diagnostic-free family so
        # validation plots keep their full information content by default.
        "def make_publication_comparison_plots",
        '"--publication", "-publication"',
        "C19_publication_comparison_",
        "C19_publication_scatter_",
        "C19_publication_parity_",
        "row.direct_scalar_accepted",
    )
    for needle in required:
        if needle not in text:
            raise SystemExit(
                "C19 calculated/accepted plotting contract missing %r" % needle)




def validate_field_initialization_progress_contract() -> None:
    """Protect the Mode3D background-field progress-display contract.

    The full AMPS executable is intentionally not linked by the package self-test,
    therefore this inexpensive source check guards the user-visible invariants that
    motivated the progress revision: field initialization must use the same #/ - bar
    grammar as cutoff tracing, must be globally completion-counted, must print at the
    quieter two-second cadence, and must flush every emitted progress line.
    """
    src_earth = ROOT.parents[1]
    text = (src_earth / "3d" / "Mode3D.cpp").read_text(errors="replace")
    required = (
        "kFieldInitProgressPrintIntervalSeconds = 2.0",
        "kFieldInitProgressBarWidth = 36",
        "i<filled ? \"#\" : \"-\"",
        "[Mode3D field INITIALIZATION]",
        "rank 0/global over",
        "std::cout.flush()",
        "DynamicMpiProgressCounter",
        "MPI_GLOBAL_COMMUNICATOR",
        "completedLocal.fetch_add",
        "workersFinished.load",
        "MPI_THREAD_MULTIPLE",
    )
    for needle in required:
        if needle not in text:
            raise SystemExit(
                "Mode3D field-initialization progress contract missing %r" % needle)


def validate_response_weighted_guardrail_contract() -> None:
    """Protect the C19 response-weighted frozen-field validity guardrail.

    The historical implementation rejected a complete aperture whenever *any*
    contributing trajectory exceeded a wall-clock threshold.  That behavior is
    intentionally obsolete: the current guard must preserve detector/spectrum
    weighting, distinguish warning from hard rejection, and expose an observable
    sensitivity interval.  This source-contract test is deliberately cheap and
    complements the executable synthetic guardrail cases in run_C19.py --self-test.
    """
    text = (ROOT / "run_C19.py").read_text()
    required = (
        "def weighted_quantile_from_pairs",
        "long_trace_response_weight_fraction",
        "long_trace_unresolved_weight_fraction",
        "static_field_log10_east_west_bound_width",
        "STATIC_FIELD_DOMINATED",
        "ACCEPTED_WITH_STATIC_FIELD_WARNING",
        "ACCEPTED_WITH_STATIC_FIELD_DOMINANCE_WARNING",
        "--frozen-field-time-tolerance-seconds",
        "--max-long-trace-response-fraction",
        "--max-long-unresolved-response-fraction",
        "--max-static-field-ratio-bound-width-log10",
        "def make_static_field_guardrail_plots",
        "C19_static_field_guardrail_",
    )
    for needle in required:
        if needle not in text:
            raise SystemExit(
                "C19 response-weighted frozen-field guardrail contract missing %r" % needle)



def validate_unresolved_extension_contract() -> None:
    """Protect staged unresolved-only convergence and its audit trail.

    The full AMPS application is not linked by this Python package test, so this
    inexpensive cross-layer source check complements run_C19.py's executable synthetic
    fold regression.  It prevents future refactors from silently removing the C++
    extension controls/provenance, the conservative secular-drift veto, or the new
    diagnostic plot while leaving the CLI superficially intact.
    """
    src_earth = ROOT.parents[1]
    runner = (ROOT / "run_C19.py").read_text(errors="replace")
    parser_h = (src_earth / "util" / "amps_param_parser.h").read_text(errors="replace")
    direct_h = (src_earth / "util" / "AdaptiveDirectAccess.h").read_text(errors="replace")
    mode3d = (src_earth / "3d" / "CutoffRigidityMode3D.cpp").read_text(errors="replace")
    gridless = (src_earth / "gridless" / "CutoffRigidityGridless.cpp").read_text(errors="replace")
    trap = (src_earth / "util" / "TrajectoryTrapDetector.h").read_text(errors="replace")

    required_runner = (
        "--unresolved-extension-passes",
        "--unresolved-extension-factor",
        "primary_termination_code",
        "trace_extension_count",
        "response_east_extension_resolved_fraction",
        "response_east_extension_unresolved_fraction",
        "def make_trace_extension_plots",
        "C19_trace_extension_",
    )
    for needle in required_runner:
        if needle not in runner:
            raise SystemExit("C19 unresolved-extension runner contract missing %r" % needle)

    for needle in ("unresolvedExtensionPasses", "unresolvedExtensionFactor",
                   "trapDriftMaxMeanRadiusChange_Re"):
        if needle not in parser_h:
            raise SystemExit("C19 unresolved-extension parser contract missing %r" % needle)
    for needle in ("primaryTerminationCode", "primaryTraceTime_s",
                   "traceExtensionCount", "finalTraceLimit_s",
                   "driftMeanRadiusChange_Re"):
        if needle not in direct_h:
            raise SystemExit("DIRECT_ACCESS extension provenance missing %r" % needle)
    for source_name, text in (("Mode3D", mode3d), ("GRIDLESS", gridless)):
        for needle in ("unresolvedExtensionPasses", "ExtendableLimit",
                       "TrajectoryTermination::TimeLimit",
                       "TrajectoryTermination::StepLimit",
                       "desiredSteps", "traceExtensionCount"):
            if needle not in text:
                raise SystemExit("%s unresolved-extension contract missing %r" %
                                 (source_name, needle))
    for needle in ("driftMaxMeanRadiusChange_m", "secularDriftOk",
                   "ProfileMeanRadius"):
        if needle not in trap:
            raise SystemExit("trap secular-drift contract missing %r" % needle)

def main() -> int:
    validate_committed_inputs()
    validate_directional_coverage_source_contract()
    validate_direct_plot_contract()
    validate_field_initialization_progress_contract()
    validate_response_weighted_guardrail_contract()
    validate_unresolved_extension_contract()
    scripts = (ROOT / "run_C19.py", ROOT / "run_C19_convergence.py",
               ROOT / "build_goes_reference.py")
    for script in scripts:
        print("Compiling", script.name, flush=True)
        py_compile.compile(str(script), doraise=True)

    run([sys.executable, str(ROOT / "build_goes_reference.py"), "--self-test"])
    run([sys.executable, str(ROOT / "run_C19.py"), "--self-test"])
    integration_dry_run()
    run([sys.executable, str(ROOT / "build_goes_reference.py"), "--help"])
    run([sys.executable, str(ROOT / "run_C19.py"), "--help"])
    run([sys.executable, str(ROOT / "run_C19_convergence.py"), "--help"])

    # Compile the recurrence regressions when a C++17 compiler is available.
    #
    # test_trap_recurrence.cpp exercises the detector with controlled synthetic
    # recurring/non-recurring profiles.  test_trap_dipole_full_orbit.cpp then advances
    # physical Lorentz orbits in an analytic centered dipole with a self-contained
    # relativistic Boris step: a 15-MeV GEO orbit must establish drift recurrence while
    # a 100-MeV outward orbit must escape without a false trapped verdict.  The latter is
    # deliberately stronger than feeding hand-made samples, but neither test replaces
    # the full linked AMPS DIPOLE/F3 executable regression required for production.
    compiler = shutil.which("g++") or shutil.which("c++")
    if compiler:
        with tempfile.TemporaryDirectory(prefix="c19_trap_test_") as tmp:
            src_earth = ROOT.parents[1]
            for source_name in (
                    "test_trap_recurrence.cpp",
                    "test_trap_dipole_full_orbit.cpp"):
                exe = Path(tmp) / Path(source_name).stem
                run([compiler, "-std=c++17", "-O2", "-I", str(src_earth),
                     str(ROOT / "tests" / source_name), "-o", str(exe)])
                run([str(exe)])
    else:
        print("C19 trap recurrence compile regressions: SKIP (no C++ compiler)")

    print("C19A package self-tests: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
