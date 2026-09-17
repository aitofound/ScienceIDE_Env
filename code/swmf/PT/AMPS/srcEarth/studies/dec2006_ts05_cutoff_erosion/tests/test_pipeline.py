"""Small integration tests for generated inputs and comparison normalization."""

from __future__ import annotations

import csv
import contextlib
import io
import json
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

import numpy as np
import pandas as pd


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from compare_observations import normalize_pamela, normalize_poes
from analyze_dynamics import (
    analysis_availability_products, cutoff_map_change_products,
    fit_two_mlt_harmonics,
)
from make_figures import (
    _continental_outline_segments, _datetime_plot_values, _global_field_grid,
    _numeric_plot_values, accessible_area_figure,
    altitude_response_figure, cutoff_change_figures,
    cutoff_degradation_figures, epoch_cutoff_rigidity_maps,
    lag_hysteresis_figure, mlt_evolution_figure,
)
from run_morphology import (
    derive_cutoff_rigidity_map, load_c10_module, snapshot_suffix,
    split_multishell_access,
)
from run_study import execute, independent_validation_remains
import run_study
from run_global_cutoff_maps import apply_profile_override, deep_merge, workload_estimate
from study_common import read_driver


class PipelineTests(unittest.TestCase):
    def test_global_plot_grid_is_cyclic_and_expands_canonical_poles(self):
        """Filled world maps must close at the date line without fake pole cells."""

        rows = []
        for longitude in (0.0, 90.0, 180.0, 270.0):
            rows.append({
                "longitude_geo_deg": longitude,
                "latitude_geo_deg": 0.0,
                "cutoff": 5.0 + longitude / 90.0,
            })
        # Canonical postprocessing retains one physical record per pole. The
        # visual grid may expand it, but the archived table must stay canonical.
        rows.extend((
            {"longitude_geo_deg": 0.0, "latitude_geo_deg": -90.0,
             "cutoff": 0.05},
            {"longitude_geo_deg": 0.0, "latitude_geo_deg": 90.0,
             "cutoff": 0.05},
        ))
        longitude, latitude, values = _global_field_grid(
            pd.DataFrame(rows), "cutoff"
        )
        self.assertTrue(np.allclose(longitude, [-180, -90, 0, 90, 180]))
        self.assertTrue(np.allclose(latitude, [-90, 0, 90]))
        self.assertEqual(values.shape, (3, 5))
        self.assertTrue(np.allclose(values[0], 0.05))
        self.assertTrue(np.allclose(values[-1], 0.05))
        self.assertAlmostEqual(float(values[1, 0]), float(values[1, -1]))
        self.assertGreaterEqual(len(_continental_outline_segments()), 7)

    def test_second_mlt_harmonic_recovers_known_shape(self):
        """The semidiurnal coefficient must retain amplitude and phase."""

        mlt = np.arange(0.0, 24.0, 3.0)
        phase = 2.25
        angle = 4.0 * np.pi * (mlt - phase) / 24.0
        latitude = 61.0 + 1.75 * np.cos(angle)
        fit = fit_two_mlt_harmonics(mlt, latitude)
        self.assertAlmostEqual(fit["two_harmonic_mean_latitude_deg"], 61.0, 12)
        self.assertAlmostEqual(fit["second_harmonic_amplitude_deg"], 1.75, 12)
        self.assertAlmostEqual(fit["second_harmonic_phase_mlt_hour"], phase, 12)
        self.assertLess(fit["two_harmonic_fit_rms_deg"], 1.0e-12)

    def test_smoke_availability_prevents_temporal_overinterpretation(self):
        """Four epochs support spatial QA but not lag/recovery inference."""

        boundary = []
        for epoch in range(4):
            for altitude in (475.0, 850.0):
                for rigidity in (0.4, 0.7):
                    for mlt in range(0, 24, 3):
                        boundary.append({
                            "epoch_utc": f"2006-12-{14 + epoch:02d}T00:00:00Z",
                            "altitude_km": str(altitude),
                            "rigidity_gv": str(rigidity), "hemisphere": "N",
                            "mlt_hour": str(mlt),
                        })
        status = analysis_availability_products(
            boundary, [{"epoch_utc": "x"}], [{"epoch_utc": "x"}], [], [],
            [], Path("/nonexistent/study/output"),
        )
        mapping = {row["analysis"]: row["status"] for row in status}
        self.assertEqual(mapping["mlt_morphology"], "AVAILABLE")
        self.assertEqual(mapping["altitude_dependence"], "AVAILABLE")
        self.assertEqual(mapping["driver_lag"], "DIAGNOSTIC_ONLY")
        self.assertEqual(mapping["recovery_timescale"], "DIAGNOSTIC_ONLY")
        self.assertEqual(mapping["ts05_driver_attribution"], "NOT_AVAILABLE")

    def test_cutoff_map_inversion_preserves_censoring_and_penumbra(self):
        """Only a resolved 50% bracket may become a numerical map cutoff."""

        rigidities = (0.5, 1.0, 2.0, 5.0)
        rows = []
        for longitude, states in (
            (0.0, (0, 0, 1, 1)),
            (15.0, (1, 1, 1, 1)),
            (30.0, (0, 0, 0, 0)),
            (45.0, (0, 2, 1, 1)),
        ):
            for rigidity, state in zip(rigidities, states):
                rows.append(SimpleNamespace(
                    longitude_deg=longitude, latitude_deg=55.0,
                    rigidity_gv=rigidity, access_state=state,
                    aacgm_latitude_deg=56.0, mlt_hour=longitude / 15.0,
                ))
        result = derive_cutoff_rigidity_map(rows, rigidities)
        by_lon = {row["longitude_geo_deg"]: row for row in result}
        self.assertEqual(by_lon[0.0]["cutoff_status"], "BRACKETED")
        self.assertAlmostEqual(by_lon[0.0]["cutoff_rigidity_r50_gv"], 1.5)
        self.assertEqual(by_lon[15.0]["cutoff_status"], "BELOW_RANGE")
        self.assertIsNone(by_lon[15.0]["cutoff_rigidity_r50_gv"])
        self.assertEqual(by_lon[30.0]["cutoff_status"], "ABOVE_RANGE")
        self.assertEqual(by_lon[45.0]["cutoff_status"], "BRACKETED")
        self.assertEqual(by_lon[45.0]["n_unresolved_rigidities"], 1)

    def test_snapshot_suffix_matches_native_mode3d_filename_contract(self):
        """The postprocessor must address one exact file per batch epoch."""

        from datetime import datetime, timezone
        epoch = datetime(2006, 12, 15, 0, 50, tzinfo=timezone.utc)
        self.assertEqual(
            snapshot_suffix(2, epoch),
            "_snapshot_000002_2006_12_15T00_50_00",
        )

    def test_independent_observation_validation_is_not_suppressed(self):
        """A PAMELA failure must not prevent the independent POES check."""

        self.assertTrue(independent_validation_remains(
            "pamela", ["poes", "morphology", "compare"]
        ))
        self.assertFalse(independent_validation_remains(
            "poes", ["morphology", "compare"]
        ))
        self.assertFalse(independent_validation_remains(
            "validate", ["pamela", "poes"]
        ))

    def test_shared_morphology_then_pamela_failure_still_allows_poes(self):
        """Shared production runs first; both independent comparisons are collected."""

        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "study"

            def fake_execute(command, cwd, log_path, dry_run, **keywords):
                return 1 if keywords["stage"] == "pamela" else 0

            arguments = [
                "run_study.py", "--prepare-only", "--output-root", str(output),
                "--stage", "validate", "--stage", "pamela",
                "--stage", "poes", "--stage", "morphology",
            ]
            with mock.patch.object(sys, "argv", arguments), \
                    mock.patch.object(run_study, "execute", side_effect=fake_execute), \
                    contextlib.redirect_stdout(io.StringIO()), \
                    contextlib.redirect_stderr(io.StringIO()):
                return_code = run_study.main()

            self.assertEqual(return_code, 1)
            manifest = json.loads((output / "study_run_manifest.json").read_text())
            self.assertEqual(manifest["return_codes"]["validate"], 0)
            self.assertEqual(manifest["return_codes"]["morphology"], 0)
            self.assertEqual(manifest["return_codes"]["pamela"], 1)
            self.assertEqual(manifest["return_codes"]["poes"], 0)

    def test_stage_execution_tees_live_output_and_records_status(self):
        """The orchestrator must show child output without sacrificing logs."""

        with tempfile.TemporaryDirectory() as temporary:
            log_path = Path(temporary) / "logs" / "diagnostic.log"
            terminal = io.StringIO()
            command = [
                sys.executable, "-c",
                "print('synthetic AMPS progress', flush=True)",
            ]
            with contextlib.redirect_stdout(terminal):
                return_code = execute(
                    command, ROOT, log_path, False,
                    stage="pamela", stage_index=2, stage_count=7,
                )

            self.assertEqual(return_code, 0)
            screen = terminal.getvalue()
            self.assertIn("[2/7] START PAMELA", screen)
            self.assertIn("[PAMELA] synthetic AMPS progress", screen)
            self.assertIn("[2/7] PASS PAMELA", screen)
            saved = log_path.read_text()
            self.assertIn("Stage: pamela", saved)
            self.assertIn("synthetic AMPS progress", saved)

    def test_normalized_comparison_schema(self):
        pamela = normalize_pamela([{
            "interval_midpoint_utc": "2006-12-14T00:00:00Z",
            "rigidity_center_gv": "0.5", "pamela_cutoff_aacgm_deg": "60",
            "amps_cutoff_aacgm_deg": "59", "pamela_sigma_plus_deg": "0.5",
            "pamela_sigma_minus_deg": "0.6",
        }])[0]
        self.assertEqual(pamela["model_minus_observation_deg"], -1.0)
        self.assertTrue(pamela["used_for_primary_metrics"])
        poes = normalize_poes([{
            "interval_midpoint_utc": "2006-12-14T00:00:00Z",
            "rigidity_gv": "0.174013525", "channel": "P6", "hemisphere": "N",
            "mlt_hour": "3", "observed_boundary_aacgm_deg": "62",
            "modeled_boundary_aacgm_deg": "63", "used_for_acceptance": "True",
            "validation_role": "PRIMARY", "sigma_deg": "1",
        }])[0]
        self.assertEqual(poes["model_minus_observation_deg"], 1.0)
        self.assertTrue(poes["used_for_primary_metrics"])

    def test_smoke_prepare_renders_one_native_multi_epoch_batch(self):
        """BATCHED must mean one process/mesh, not directory-only grouping."""

        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "morphology"
            completed = subprocess.run([
                sys.executable, str(ROOT / "scripts" / "run_morphology.py"),
                "--profile", "SMOKE", "--prepare-only", "--output-root", str(output),
            ], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(completed.returncode, 0, completed.stdout)
            result = json.loads((output / "morphology_result.json").read_text())
            self.assertEqual(result["n_cases"], 8)
            self.assertEqual(result["n_amps_launches"], 1)
            self.assertEqual(result["mesh_layout"], "BATCHED")
            self.assertTrue(result["mesh_reused_across_shells"])
            self.assertTrue(result["mesh_reused_across_epochs"])
            inputs = list(output.rglob("AMPS_PARAM_C10.in"))
            self.assertEqual(len(inputs), 1)
            for path in inputs:
                text = path.read_text()
                self.assertIn("CUTOFF_SAMPLING         VERTICAL", text)
                self.assertIn("CUTOFF_SEARCH_ALGORITHM RIGIDITY_LIST", text)
                self.assertIn("TEMPORAL_MODE          SNAPSHOT_LIST", text)
                self.assertIn("SNAPSHOT_LIST_FILE     snapshot_epochs.txt", text)
                self.assertIn("SHELL_COUNT 2", text)
                self.assertIn("SHELL_ALTS_KM 475 850", text)
                self.assertNotIn("CUTOFF_UNRESOLVED_EXTENSION_PASSES", text)
                epochs = (path.parent / "snapshot_epochs.txt").read_text()
                self.assertEqual(len([
                    line for line in epochs.splitlines()
                    if line and not line.startswith("#")
                ]), 4)
            inventory = json.loads((output / "command_inventory.json").read_text())
            self.assertEqual(len(inventory), 1)
            for item in inventory:
                self.assertNotIn("--epoch", item["command"])
                self.assertTrue(item["reuses_mesh_across_epochs"])
                self.assertEqual(item["command"][item["command"].index("-mover") + 1],
                                 "RK4")

    def test_global_runner_prepares_manageable_smoke_mesh_reuse_batch(self):
        """SMOKE must remain global and batched without using the production grid."""

        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "global"
            completed = subprocess.run([
                sys.executable,
                str(ROOT / "scripts" / "run_global_cutoff_maps.py"),
                "--profile", "SMOKE", "--stage", "model", "--prepare-only",
                "--output-root", str(output),
            ], cwd=ROOT, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT)
            self.assertEqual(completed.returncode, 0, completed.stdout)
            inputs = list((output / "morphology").rglob("AMPS_PARAM_C10.in"))
            self.assertEqual(len(inputs), 1)
            text = inputs[0].read_text(encoding="utf-8")
            self.assertIn("TEMPORAL_MODE          SNAPSHOT_LIST", text)
            self.assertIn("SHELL_COUNT 2", text)
            self.assertIn("SHELL_ALTS_KM 475 850", text)
            self.assertIn("SHELL_LON_RES_DEG 30", text)
            self.assertIn("SHELL_LAT_RES_DEG 10", text)
            self.assertIn("CUTOFF_ACCESS_ABS_LAT_MIN 0", text)
            self.assertIn("CUTOFF_ACCESS_ABS_LAT_MAX 90", text)
            self.assertIn("20", text.split("CUTOFF_RIGIDITY_LIST_GV", 1)[1].splitlines()[0])
            manifest = json.loads(
                (output / "global_cutoff_map_run_manifest.json").read_text()
            )
            self.assertEqual(manifest["mesh_layout"], "BATCHED")
            self.assertEqual(manifest["return_codes"]["model"], 0)
            self.assertEqual(manifest["estimated_workload"]["tasks_per_epoch"], 7752)
            self.assertEqual(manifest["estimated_workload"]["tasks_total"], 15504)
            epochs = (inputs[0].parent / "snapshot_epochs.txt").read_text()
            self.assertEqual(len([
                line for line in epochs.splitlines()
                if line and not line.startswith("#")
            ]), 2)

    def test_global_smoke_override_does_not_change_full_grid(self):
        """Profile reduction must be isolated to SMOKE and leave FULL publishable."""

        base = json.loads((ROOT / "config" / "study.json").read_text())
        overlay = json.loads(
            (ROOT / "config" / "global_cutoff_maps.json").read_text()
        )
        merged = deep_merge(base, overlay)
        smoke = apply_profile_override(json.loads(json.dumps(merged)), "SMOKE")
        full = apply_profile_override(json.loads(json.dumps(merged)), "FULL")
        self.assertEqual(smoke["model"]["shell_longitude_step_deg"], 30.0)
        self.assertEqual(smoke["model"]["shell_latitude_step_deg"], 10.0)
        self.assertEqual(len(smoke["model"]["rigidities_gv"]), 17)
        self.assertEqual(workload_estimate(smoke, "SMOKE")["tasks_total"], 15504)
        self.assertEqual(full["model"]["shell_longitude_step_deg"], 10.0)
        self.assertEqual(full["model"]["shell_latitude_step_deg"], 2.0)
        self.assertEqual(len(full["model"]["rigidities_gv"]), 53)

    def test_global_map_postprocessor_collapses_poles_and_derives_erosion(self):
        """Canonical maps retain one pole and quantify quiet-relative change."""

        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            config = json.loads((ROOT / "config" / "study.json").read_text())
            config["study_id"] = "SYNTHETIC_GLOBAL_MAP"
            config["model"]["latitude_band_abs_deg"] = [0.0, 90.0]
            config["model"]["shell_longitude_step_deg"] = 90.0
            config["model"]["shell_latitude_step_deg"] = 90.0
            config["model"]["rigidities_gv"] = [0.05, 1.0, 5.0, 20.0]
            config["global_map_analysis"] = {
                "minimum_expected_rigidity_max_gv": 15.0,
                "maximum_expected_rigidity_min_gv": 0.1,
                "minimum_exact_map_fraction": 0.2,
                "large_decrease_threshold_gv": 0.5,
            }
            config_path = work / "config.json"
            config_path.write_text(json.dumps(config), encoding="utf-8")
            morphology = work / "morphology"
            morphology.mkdir()
            (morphology / "event_landmarks.json").write_text(json.dumps({
                "quiet": "2006-12-14T00:00:00Z",
                "compression": "2006-12-14T14:10:00Z",
                "main_phase": "2006-12-15T00:50:00Z",
            }))
            manifest_rows = []
            longitudes = (0.0, 90.0, 180.0, 270.0)
            latitudes = (-90.0, 0.0, 90.0)
            for epoch, token, cutoff in (
                ("2006-12-14T00:00:00Z", "quiet", 5.0),
                ("2006-12-14T14:10:00Z", "event", 4.0),
            ):
                for altitude in (475.0, 850.0):
                    directory = morphology / f"alt_{altitude:g}km" / token
                    directory.mkdir(parents=True)
                    path = directory / "cutoff_rigidity_map.csv"
                    rows = []
                    for latitude in latitudes:
                        for longitude in longitudes:
                            rows.append({
                                "longitude_geo_deg": longitude,
                                "latitude_geo_deg": latitude,
                                "aacgm_latitude_deg": latitude,
                                "mlt_hour": longitude / 15.0,
                                "cutoff_rigidity_r50_gv": cutoff,
                                "cutoff_status": "BRACKETED",
                            })
                    with path.open("w", newline="", encoding="utf-8") as stream:
                        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
                        writer.writeheader()
                        writer.writerows(rows)
                    manifest_rows.append({
                        "epoch_utc": epoch, "altitude_km": altitude,
                        "map_path": path.relative_to(morphology).as_posix(),
                        "n_spatial_cells": len(rows),
                        "sampled_rigidity_min_gv": 0.05,
                        "sampled_rigidity_max_gv": 20.0,
                    })
            with (morphology / "cutoff_rigidity_map_manifest.csv").open(
                    "w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=manifest_rows[0].keys())
                writer.writeheader()
                writer.writerows(manifest_rows)

            output = work / "postprocessing"
            completed = subprocess.run([
                sys.executable,
                str(ROOT / "scripts" / "postprocess_global_cutoff_maps.py"),
                "--config", str(config_path), "--map-root", str(morphology),
                "--output-root", str(output),
            ], cwd=ROOT, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT)
            self.assertEqual(completed.returncode, 0, completed.stdout)
            result = json.loads(
                (output / "global_map_postprocessing_result.json").read_text()
            )
            self.assertTrue(result["passed"])
            self.assertEqual(result["expected_cells_per_map_after_pole_collapse"], 6)
            canonical_manifest = pd.read_csv(
                output / "canonical_maps" / "cutoff_rigidity_map_manifest.csv"
            )
            first_map = output / "canonical_maps" / canonical_manifest.iloc[0].map_path
            self.assertEqual(len(pd.read_csv(first_map)), 6)
            change = pd.read_csv(output / "global_cutoff_event_change.csv")
            self.assertEqual(len(change), 12)
            self.assertTrue(np.allclose(change.maximum_cutoff_decrease_gv, 1.0))

            figure_root = work / "figures"
            completed = subprocess.run([
                sys.executable,
                str(ROOT / "scripts" / "make_global_cutoff_map_figures.py"),
                "--postprocessing-root", str(output),
                "--output-root", str(figure_root),
            ], cwd=ROOT, text=True, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT)
            self.assertEqual(completed.returncode, 0, completed.stdout)
            figure_result = json.loads(
                (figure_root / "global_cutoff_figure_result.json").read_text()
            )
            self.assertTrue(figure_result["passed"])
            self.assertEqual(figure_result["n_epoch_map_png"], 2)
            self.assertEqual(figure_result["n_rendered_shell_epoch_panels"], 4)

    def test_native_mode3d_supports_snapshot_list_shells_with_one_mesh(self):
        """Guard new SHELLS support and the pre-existing dispatch boundaries."""

        source_path = ROOT.parents[1] / "3d" / "Mode3D.cpp"
        source = source_path.read_text(encoding="utf-8")
        self.assertGreaterEqual(source.count('if (outputMode=="SHELLS") return snap;'), 1)
        self.assertGreaterEqual(source.count('if (outputMode=="SHELLS") return;'), 1)

        # The earliest return is the compatibility firewall: every ordinary
        # SNAPSHOT/TIME_SERIES calculation exits before the new SHELLS branch.
        # Within SNAPSHOT_LIST, TRAJECTORY must retain its historical timestamp
        # filtering, flattened-point rebuild, and aperture-index remapping.
        build_start = source.index("EarthUtil::AmpsParam Mode3DBuildSnapshotWorkParam(")
        build_end = source.index("void Mode3DValidateSnapshotListCoverage(", build_start)
        build = source[build_start:build_end]
        self.assertLess(
            build.index("if (!Mode3DSnapshotListRequested(snap)) return snap;"),
            build.index('if (outputMode=="SHELLS") return snap;'),
        )
        for legacy_marker in (
            "globalToLocal", "RebuildFlattenedPointsFromTrajectories",
            "remapped.locationIndex=found->second",
        ):
            self.assertIn(legacy_marker, build)

        validate_start = build_end
        validate_end = source.index("EarthUtil::AmpsParam Mode3DBuildSnapshotParam(",
                                    validate_start)
        validation = source[validate_start:validate_end]
        self.assertLess(
            validation.index("if (!Mode3DSnapshotListRequested(prm)) return;"),
            validation.index('if (outputMode=="SHELLS") return;'),
        )
        self.assertIn("every location must match exactly one epoch", validation)

        mesh_position = source.index("  amps_init_mesh();   // build")
        snapshot_loop_position = source.index(
            "for (std::size_t iSnapshot=0; iSnapshot<snapshotEpochs.size();"
        )
        self.assertLess(mesh_position, snapshot_loop_position)

    def test_multishell_split_requires_altitude_labeled_zones(self):
        """The optimizer must never infer shell identity from row order."""

        c10 = load_c10_module(ROOT)
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            source = work / "combined.dat"
            source.write_text(
                'TITLE="fixture"\n'
                'VARIABLES="lon_deg" "lat_deg" "rigidity_gv" '
                '"access_state" "allowed" "unresolved"\n'
                'ZONE T="Alt_km=475"\n0 40 0.5 1 1 0\n'
                'ZONE T="Alt_km=850"\n0 50 0.5 0 0 0\n'
            )
            destinations = {475.0: work / "475.dat", 850.0: work / "850.dat"}
            counts = split_multishell_access(
                source, (475.0, 850.0), destinations, c10
            )
            self.assertEqual(counts, {475.0: 1, 850.0: 1})
            self.assertEqual(
                c10.parse_tecplot_shell_access(destinations[475.0])[0].access_state,
                1,
            )

    def test_multishell_split_accepts_explicit_zero_based_shell_indices(self):
        """AMPS Shell_0/Shell_1 zone labels follow configured altitude order."""

        c10 = load_c10_module(ROOT)
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            source = work / "combined.dat"
            source.write_text(
                'TITLE="fixture"\n'
                'VARIABLES="lon_deg" "lat_deg" "rigidity_gv" '
                '"access_state" "allowed" "unresolved"\n'
                'ZONE T="Shell_0"\n0 40 0.5 1 1 0\n'
                'ZONE T="Shell_1"\n0 50 0.5 0 0 0\n'
            )
            destinations = {475.0: work / "475.dat", 850.0: work / "850.dat"}
            counts = split_multishell_access(
                source, (475.0, 850.0), destinations, c10
            )
            self.assertEqual(counts, {475.0: 1, 850.0: 1})

    def test_multishell_split_accepts_mode3d_shell_index_column(self):
        """Current DIRECT_ACCESS uses one generic zone plus shell_index rows.

        This fixture reproduces the schema that exposed the production failure:
        line four is the first row of ``ZONE T=\"fixed_rigidity_access\"`` and
        the zone itself contains no altitude.  The row-level shell index is the
        authoritative, scheduler-independent identity written by Mode3D.
        """

        c10 = load_c10_module(ROOT)
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            source = work / "combined.dat"
            source.write_text(
                'TITLE="fixture"\n'
                'VARIABLES="shell_index" "lon_deg" "lat_deg" '
                '"rigidity_gv" "access_state" "allowed" "unresolved"\n'
                'ZONE T="fixed_rigidity_access" I=4 F=POINT\n'
                '0 0 40 0.5 1 1 0\n'
                '1 0 50 0.5 0 0 0\n'
                '0 15 40 0.5 0 0 0\n'
                '1 15 50 0.5 1 1 0\n'
            )
            destinations = {475.0: work / "475.dat", 850.0: work / "850.dat"}
            counts = split_multishell_access(
                source, (475.0, 850.0), destinations, c10
            )
            self.assertEqual(counts, {475.0: 2, 850.0: 2})
            self.assertEqual(
                [row.access_state for row in
                 c10.parse_tecplot_shell_access(destinations[850.0])],
                [0, 1],
            )

    def test_multishell_split_rejects_invalid_shell_index_column(self):
        """A malformed row must fail instead of being assigned by position."""

        c10 = load_c10_module(ROOT)
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            source = work / "combined.dat"
            source.write_text(
                'VARIABLES="shell_index" "lon_deg" "lat_deg" '
                '"rigidity_gv" "access_state" "allowed" "unresolved"\n'
                'ZONE T="fixed_rigidity_access" I=1 F=POINT\n'
                '0.5 0 40 0.5 1 1 0\n'
            )
            destinations = {475.0: work / "475.dat", 850.0: work / "850.dat"}
            with self.assertRaisesRegex(ValueError, "not a valid zero-based index"):
                split_multishell_access(
                    source, (475.0, 850.0), destinations, c10
                )

    def test_publication_degradation_figures_include_png_and_eps(self):
        """The required science visualization must include raster and vector files."""

        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            source = work / "cutoff_dynamics_timeseries.csv"
            with source.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=(
                    "epoch_utc", "altitude_km", "rigidity_gv", "hemisphere",
                    "cutoff_erosion_deg",
                ))
                writer.writeheader()
                for altitude in (475.0, 850.0):
                    for rigidity in (0.4, 0.7):
                        for hemisphere in ("N", "S"):
                            for epoch, erosion in (
                                ("2006-12-14T00:00:00Z", 0.0),
                                ("2006-12-15T00:00:00Z", -2.0),
                            ):
                                writer.writerow({
                                    "epoch_utc": epoch, "altitude_km": altitude,
                                    "rigidity_gv": rigidity,
                                    "hemisphere": hemisphere,
                                    "cutoff_erosion_deg": erosion,
                                })
            products = cutoff_degradation_figures(source, work / "figures")
            self.assertEqual(len(products), 6)
            for stem in ("figure_cutoff_degradation",
                         "figure_peak_cutoff_degradation"):
                self.assertTrue((work / "figures" / f"{stem}.png").is_file())
                self.assertTrue((work / "figures" / f"{stem}.eps").is_file())

    def test_enhanced_spatial_figures_run_on_smoke_sized_products(self):
        """Four epochs are sufficient to verify all spatial publication panels."""

        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            cells = work / "boundary_cell_dynamics.csv"
            with cells.open("w", newline="", encoding="utf-8") as stream:
                fields = (
                    "epoch_utc", "altitude_km", "rigidity_gv", "hemisphere",
                    "mlt_hour", "boundary_aacgm_abs_lat_deg",
                    "quiet_reference_boundary_deg", "event_phase",
                )
                writer = csv.DictWriter(stream, fieldnames=fields)
                writer.writeheader()
                for index, epoch in enumerate((
                    "2006-12-14T00:00:00Z", "2006-12-14T14:10:00Z",
                    "2006-12-15T00:50:00Z", "2006-12-16T00:00:00Z",
                )):
                    for hemisphere, sign in (("N", 1.0), ("S", -1.0)):
                        for mlt in range(0, 24, 3):
                            writer.writerow({
                                "epoch_utc": epoch, "altitude_km": 850.0,
                                "rigidity_gv": 0.423556372,
                                "hemisphere": hemisphere, "mlt_hour": mlt,
                                "boundary_aacgm_abs_lat_deg": (
                                    60.0 - index + sign * np.cos(2*np.pi*mlt/24)
                                ),
                                "quiet_reference_boundary_deg": 60.0,
                                "event_phase": ("PRECOMPRESSION" if index == 0
                                                else "MAIN_PHASE" if index < 3
                                                else "RECOVERY"),
                            })
            altitude = work / "altitude_response.csv"
            with altitude.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=(
                    "epoch_utc", "rigidity_gv", "hemisphere",
                    "high_minus_low_erosion",
                ))
                writer.writeheader()
                for index, epoch in enumerate((
                    "2006-12-14T00:00:00Z", "2006-12-15T00:00:00Z",
                )):
                    for rigidity in (0.4, 0.7):
                        for hemisphere in ("N", "S"):
                            writer.writerow({
                                "epoch_utc": epoch, "rigidity_gv": rigidity,
                                "hemisphere": hemisphere,
                                "high_minus_low_erosion": (index + 1) * rigidity,
                            })
            dynamics = work / "cutoff_dynamics_timeseries.csv"
            with dynamics.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=(
                    "epoch_utc", "altitude_km", "rigidity_gv", "hemisphere",
                    "accessible_area_fraction_in_analyzed_band",
                ))
                writer.writeheader()
                for altitude_km in (475.0, 850.0):
                    for rigidity in (0.174013525, 0.423556372, 0.692820323,
                                     1.131017241):
                        for hemisphere in ("N", "S"):
                            for index, epoch in enumerate((
                                "2006-12-14T00:00:00Z", "2006-12-15T00:00:00Z",
                            )):
                                writer.writerow({
                                    "epoch_utc": epoch, "altitude_km": altitude_km,
                                    "rigidity_gv": rigidity, "hemisphere": hemisphere,
                                    "accessible_area_fraction_in_analyzed_band":
                                        0.2 + 0.1 * index,
                                })
            products = []
            products += mlt_evolution_figure(cells, work / "figures")
            products += altitude_response_figure(altitude, work / "figures")
            products += accessible_area_figure(dynamics, work / "figures")
            self.assertEqual(len(products), 9)
            for stem in ("figure_mlt_cutoff_evolution",
                         "figure_altitude_response", "figure_accessible_area"):
                self.assertTrue((work / "figures" / f"{stem}.png").is_file())
                self.assertTrue((work / "figures" / f"{stem}.eps").is_file())

    def test_cutoff_map_event_change_and_epoch_visualization(self):
        """Quiet-to-event R50 decrease must remain located and traceable."""

        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            morphology = work / "morphology"
            morphology.mkdir()
            manifest_rows = []
            for epoch, token, event_delta in (
                ("2006-12-14T00:00:00Z", "quiet", 0.0),
                ("2006-12-14T14:10:00Z", "event", -0.3),
            ):
                for altitude, base in ((475.0, 1.0), (850.0, 1.2)):
                    directory = morphology / f"alt_{altitude:g}km" / token
                    directory.mkdir(parents=True)
                    map_path = directory / "cutoff_rigidity_map.csv"
                    with map_path.open("w", newline="", encoding="utf-8") as stream:
                        writer = csv.DictWriter(stream, fieldnames=(
                            "longitude_geo_deg", "latitude_geo_deg",
                            "aacgm_latitude_deg", "mlt_hour",
                            "cutoff_rigidity_r50_gv", "cutoff_status",
                        ))
                        writer.writeheader()
                        writer.writerow({
                            "longitude_geo_deg": 30.0,
                            "latitude_geo_deg": 55.0,
                            "aacgm_latitude_deg": 57.0,
                            "mlt_hour": 2.0,
                            "cutoff_rigidity_r50_gv": base + event_delta,
                            "cutoff_status": "BRACKETED",
                        })
                    manifest_rows.append({
                        "epoch_utc": epoch, "altitude_km": altitude,
                        "map_path": map_path.relative_to(morphology).as_posix(),
                        "n_spatial_cells": 1,
                        "sampled_rigidity_min_gv": 0.15,
                        "sampled_rigidity_max_gv": 1.25,
                    })
            with (morphology / "cutoff_rigidity_map_manifest.csv").open(
                    "w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=manifest_rows[0].keys())
                writer.writeheader()
                writer.writerows(manifest_rows)

            spatial, evolution, summary = cutoff_map_change_products(
                morphology,
                datetime(2006, 12, 14, 12, tzinfo=timezone.utc),
                datetime(2006, 12, 14, 14, 10, tzinfo=timezone.utc),
            )
            self.assertEqual(len(spatial), 2)
            self.assertEqual(len(evolution), 4)
            self.assertEqual(summary["status"], "AVAILABLE")
            self.assertTrue(all(abs(row["maximum_cutoff_decrease_gv"] - 0.3) < 1e-12
                                for row in spatial))

            spatial_path = work / "spatial.csv"
            evolution_path = work / "evolution.csv"
            with spatial_path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=spatial[0].keys())
                writer.writeheader()
                writer.writerows(spatial)
            with evolution_path.open("w", newline="", encoding="utf-8") as stream:
                writer = csv.DictWriter(stream, fieldnames=evolution[0].keys())
                writer.writeheader()
                writer.writerows(evolution)
            epoch_paths, epoch_records = epoch_cutoff_rigidity_maps(
                morphology, work / "figures"
            )
            change_paths = cutoff_change_figures(
                spatial_path, evolution_path, work / "figures"
            )
            self.assertEqual(len(epoch_paths), 2)
            self.assertEqual(len(epoch_records), 4)
            self.assertEqual(len(change_paths), 6)

    def test_figure_module_avoids_twoslope_norm_version_dependency(self):
        """System Matplotlib on production hosts may predate TwoSlopeNorm."""

        source = (ROOT / "scripts" / "make_figures.py").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("from matplotlib.colors import TwoSlopeNorm", source)
        self.assertNotIn("norm=TwoSlopeNorm(", source)
        self.assertIn("Normalize(vmin=-limit, vmax=limit)", source)

    def test_figure_inputs_are_detached_from_pandas_series(self):
        """Old Matplotlib must receive arrays, never pandas Series objects."""

        numeric = pd.Series([1.0, 2.0])
        epochs = pd.to_datetime(pd.Series([
            "2006-12-14T00:00:00Z", "2006-12-15T00:00:00Z",
        ]), utc=True)
        self.assertIsInstance(_numeric_plot_values(numeric), np.ndarray)
        self.assertEqual(_numeric_plot_values(numeric).ndim, 1)
        converted_epochs = _datetime_plot_values(epochs)
        self.assertIsInstance(converted_epochs, list)
        self.assertFalse(any(isinstance(value, pd.Timestamp)
                             for value in converted_epochs))

    def test_optional_hysteresis_figure_accepts_empty_smoke_product(self):
        """No matched SMOKE pairs is a valid result, not a plotting failure."""

        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            lag = work / "lag.csv"
            hysteresis = work / "hysteresis.csv"
            lag.write_text(
                "altitude_km,rigidity_gv,hemisphere,driver_variable,"
                "lag_minutes,correlation\n",
                encoding="utf-8",
            )
            hysteresis.write_text("", encoding="utf-8")
            lag_hysteresis_figure(lag, hysteresis, work / "figures")
            self.assertFalse(
                (work / "figures" / "figure_lag_hysteresis.png").exists()
            )

    def test_sensitivity_driver_generation_preserves_cadence(self):
        with tempfile.TemporaryDirectory() as temporary:
            completed = subprocess.run([
                sys.executable,
                str(ROOT / "scripts" / "make_ts05_sensitivity_drivers.py"),
                "--output-root", temporary,
            ], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            self.assertEqual(completed.returncode, 0, completed.stdout)
            for name in ("history_frozen", "instantaneous_frozen"):
                rows = read_driver(Path(temporary) / f"ts05_dec2006_{name}.txt")
                self.assertEqual(len(rows), 865)
                self.assertEqual((rows[1].epoch - rows[0].epoch).total_seconds(), 300.0)


if __name__ == "__main__":
    unittest.main()
