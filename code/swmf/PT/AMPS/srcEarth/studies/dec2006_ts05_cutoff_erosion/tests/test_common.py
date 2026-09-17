"""Unit tests for numerical conventions that do not require AMPS."""

from __future__ import annotations

import math
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from study_common import (
    STUDY_RUN_NAME, default_output_root, fit_first_harmonic, pearson,
    read_driver, residual_metrics,
)
from run_morphology import event_landmarks
from analyze_dynamics import morphology_products


class CommonTests(unittest.TestCase):
    def test_default_output_uses_stable_study_name(self):
        """Every runner must share the requested repository output label."""
        output = default_output_root()
        self.assertEqual(output.name, STUDY_RUN_NAME)
        self.assertEqual(output.parent.name, "test_output")

    def test_driver_and_objective_landmarks(self):
        import json
        config = json.loads((ROOT / "config" / "study.json").read_text())
        rows = read_driver(ROOT / config["data"]["driver"])
        self.assertEqual(len(rows), 865)
        landmarks = event_landmarks(rows, config)
        self.assertEqual(landmarks["compression"].isoformat(), "2006-12-14T14:10:00+00:00")
        self.assertEqual(landmarks["main_phase"].isoformat(), "2006-12-15T00:50:00+00:00")

    def test_first_harmonic_recovers_known_coefficients(self):
        mlt = [0.0, 3.0, 6.0, 9.0, 12.0, 15.0, 18.0, 21.0]
        mean, amplitude, phase = 61.0, 2.5, 7.0
        latitude = [
            mean + amplitude * math.cos(2.0 * math.pi * (hour - phase) / 24.0)
            for hour in mlt
        ]
        result = fit_first_harmonic(mlt, latitude)
        self.assertAlmostEqual(result["mean_latitude_deg"], mean, places=10)
        self.assertAlmostEqual(result["amplitude_deg"], amplitude, places=10)
        self.assertAlmostEqual(result["phase_mlt_hour"], phase, places=10)
        self.assertLess(result["fit_rms_deg"], 1.0e-10)

    def test_residual_sign_is_model_minus_observation(self):
        result = residual_metrics([10.0, 20.0], [12.0, 18.0])
        self.assertEqual(result["n"], 2)
        self.assertAlmostEqual(result["bias_deg"], 0.0)
        self.assertAlmostEqual(result["mae_deg"], 2.0)
        self.assertAlmostEqual(result["rmse_deg"], 2.0)
        self.assertAlmostEqual(pearson([1.0, 2.0], [2.0, 4.0]), 1.0)

    def test_morphology_products_preserve_epoch_and_area_bounds(self):
        import json
        config = json.loads((ROOT / "config" / "study.json").read_text())
        rows = []
        for hour in (0, 3, 6, 9, 12, 15, 18, 21):
            latitude = 60.0 + 2.0 * math.cos(2.0 * math.pi * (hour - 6.0) / 24.0)
            rows.append({
                "epoch_utc": "2006-12-14T00:00:00Z", "altitude_km": "850",
                "rigidity_gv": "0.5", "hemisphere": "N", "mlt_hour": str(hour),
                "boundary_aacgm_lat_deg": str(latitude),
            })
        harmonics, time_series = morphology_products(rows, config)
        self.assertEqual(len(harmonics), 1)
        self.assertEqual(len(time_series), 1)
        self.assertAlmostEqual(harmonics[0]["mean_latitude_deg"], 60.0)
        self.assertAlmostEqual(harmonics[0]["amplitude_deg"], 2.0)
        fraction = harmonics[0]["accessible_area_fraction_in_analyzed_band"]
        self.assertGreaterEqual(fraction, 0.0)
        self.assertLessEqual(fraction, 1.0)


if __name__ == "__main__":
    unittest.main()
