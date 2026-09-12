"""Unit tests for the archive-independent parts of the C10 reference pipeline.

These tests deliberately construct small in-memory observations.  They verify
scientific bookkeeping and interpolation without downloading NOAA data or
requiring AACGM coefficient files.  Real archive validation remains a separate,
explicit step documented in README.md.
"""
from __future__ import annotations

import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

# The tests are run from the C10 directory by the README command.  Explicitly
# importing the local modules makes it obvious which implementation is tested.
from poes_sem2 import (
    CHANNEL_THRESHOLDS_MEV,
    Observation,
    aggregate_crossings,
    extract_boundary_crossings,
    proton_rigidity_gv_from_kinetic_energy_mev,
    read_ascii_level2,
)


UTC = timezone.utc


class RigidityMappingTests(unittest.TestCase):
    """Verify the transparent channel-threshold-to-rigidity conversion."""

    def test_mapping_is_positive_and_monotonic(self) -> None:
        values = [
            proton_rigidity_gv_from_kinetic_energy_mev(energy)
            for energy in CHANNEL_THRESHOLDS_MEV.values()
        ]
        self.assertTrue(all(value > 0.0 for value in values))
        self.assertEqual(values, sorted(values))

    def test_16_mev_value(self) -> None:
        # Relativistic p = sqrt(T(T+2mc^2)); this is a numerical regression,
        # not an instrument-response assertion.
        self.assertAlmostEqual(
            proton_rigidity_gv_from_kinetic_energy_mev(16.0),
            0.174013525,
            places=8,
        )


class HistoricalAsciiReaderTests(unittest.TestCase):
    """Exercise the documented Level-2 16-second ASCII column layout."""

    def test_reads_documented_columns_and_rejects_fill(self) -> None:
        header = (
            "year mo dy hr mi second dayofyear sslat sslon lval mlt "
            "mepomp6 mepomp7 mepomp8 mepomp9\n"
        )
        rows = (
            "2006 12 14 00 00 08 348.000093 65.0 10.0 5.0 180.0 10 20 30 40\n"
            # -999 and the historical unphysical unpacker maximum must become
            # missing values rather than physical channel fluxes.
            "2006 12 14 00 00 24 348.000278 66.0 11.0 5.1 181.0 -999 21 19988488 41\n"
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "poes_n15_20061214.txt"
            path.write_text("# synthetic parser fixture\n" + header + rows)
            parsed = read_ascii_level2(path)
        self.assertEqual(len(parsed), 2)
        self.assertEqual(parsed[0]["time_utc"], datetime(2006, 12, 14, 0, 0, 8, tzinfo=UTC))
        self.assertEqual(parsed[0]["flux_by_channel"]["P8"], 30.0)
        self.assertIsNone(parsed[1]["flux_by_channel"]["P6"])
        self.assertIsNone(parsed[1]["flux_by_channel"]["P8"])


class BoundaryExtractionTests(unittest.TestCase):
    """Verify pass splitting, half-plateau interpolation, and aggregation."""

    @staticmethod
    def make_pass() -> list[Observation]:
        start = datetime(2006, 12, 14, 12, 0, tzinfo=UTC)
        # A complete northern pass.  Flux increases toward the polar cap and
        # then decreases symmetrically, producing one crossing on each leg.
        latitudes = [45, 50, 55, 60, 65, 70, 75, 80, 82, 80, 75, 70, 65, 60, 55, 50, 45]
        flux_by_abs_lat = {45: 1, 50: 1, 55: 1, 60: 2, 65: 4, 70: 6, 75: 8, 80: 10, 82: 10}
        result: list[Observation] = []
        for index, latitude in enumerate(latitudes):
            flux = float(flux_by_abs_lat[abs(latitude)])
            result.append(Observation(
                time_utc=start + timedelta(seconds=16 * index),
                satellite="NOAA-15",
                geographic_lat_deg=float(latitude),
                geographic_lon_deg=float(index),
                altitude_km=850.0,
                aacgm_lat_deg=float(latitude),
                mlt_hour=(6.0 + index / 20.0) % 24.0,
                l_value=None,
                flux_by_channel={channel: flux for channel in CHANNEL_THRESHOLDS_MEV},
                source_file="poes_n15_20061214.txt",
                source_sha256="fixture",
            ))
        return result

    def test_two_pass_leg_crossings_are_found(self) -> None:
        crossings = extract_boundary_crossings(
            self.make_pass(),
            minimum_polar_samples=4,
            minimum_leg_samples=4,
            minimum_background_samples=2,
        )
        # Four channels times inbound/outbound.
        self.assertEqual(len(crossings), 8)
        p6 = [row for row in crossings if row.channel == "P6"]
        self.assertEqual({row.leg for row in p6}, {"INBOUND", "OUTBOUND"})
        # The low-latitude background is 1 and the polar plateau is 10, so the
        # background-normalized T50 flux is 5.5.  Interpolation between 4 at
        # 65 degrees and 6 at 70 degrees gives 68.75 degrees.
        self.assertTrue(all(abs(abs(row.aacgm_lat_deg) - 68.75) < 1.0e-9 for row in p6))
        self.assertTrue(all(row.crossing_method == "BACKGROUND_NORMALIZED_ISOTONIC" for row in p6))
        self.assertTrue(all(row.normalized_t25_aacgm_lat_deg is not None for row in p6))
        self.assertTrue(all(row.transition_width_deg and row.transition_width_deg > 0.0 for row in p6))

    def test_crossings_aggregate_into_real_reference_cells(self) -> None:
        crossings = extract_boundary_crossings(
            self.make_pass(),
            minimum_polar_samples=4,
            minimum_leg_samples=4,
            minimum_background_samples=2,
        )
        cells = aggregate_crossings(
            crossings,
            event_start=datetime(2006, 12, 14, 11, 0, tzinfo=UTC),
            event_end=datetime(2006, 12, 14, 13, 0, tzinfo=UTC),
            window_hours=2.0,
            step_hours=1.0,
            mlt_bin_centers=(0.0, 6.0, 12.0, 18.0),
        )
        nonmissing = [cell for cell in cells if not cell.missing]
        self.assertTrue(nonmissing)
        self.assertTrue(all(cell.source == "POES_NCEI_LEVEL2_16SEC" for cell in nonmissing))
        self.assertTrue(all(cell.n_crossings >= 1 for cell in nonmissing))
        primary = [cell for cell in nonmissing if cell.validation_role == "PRIMARY"]
        diagnostic = [cell for cell in nonmissing if cell.validation_role == "DIAGNOSTIC"]
        self.assertTrue(primary)
        self.assertTrue(diagnostic)
        self.assertTrue(all(cell.background_corrected for cell in nonmissing))


if __name__ == "__main__":
    unittest.main()
