"""Tests for the gzip-compressed C10 production reference.

The scientific reference can contain many window/channel/hemisphere/MLT cells.
Keeping it as ``.csv.gz`` reduces repository and transfer size without changing
its CSV schema.  These tests verify both deterministic creation and transparent
loading by the runner.
"""
from __future__ import annotations

import contextlib
import io
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from build_poes_reference import parse_args as parse_builder_args
from poes_sem2 import ReferenceCell, write_reference_csv
from run_C10 import load_reference


UTC = timezone.utc


def one_cell() -> ReferenceCell:
    midpoint = datetime(2006, 12, 14, 6, 0, tzinfo=UTC)
    return ReferenceCell(
        event_id="C10_200612_STORM",
        interval_midpoint_utc=midpoint,
        interval_start_utc=midpoint - timedelta(hours=1),
        interval_end_utc=midpoint + timedelta(hours=1),
        rigidity_gv=0.174013525,
        energy_threshold_mev=16.0,
        channel="P6",
        validation_role="PRIMARY",
        acceptance_eligible=True,
        hemisphere="N",
        mlt_hour=0.0,
        boundary_aacgm_lat_deg=65.25,
        sigma_deg=0.5,
        altitude_km=850.0,
        n_crossings=2,
        n_distinct_pass_legs=2,
        n_distinct_satellites=2,
        median_transition_width_deg=4.0,
        background_corrected=True,
        satellites="NOAA-15;NOAA-18",
        missing=False,
        source="POES_NCEI_LEVEL2_16SEC",
        source_ref="fixture",
        notes="gzip regression fixture",
    )


class GzipReferenceTests(unittest.TestCase):
    def test_builder_defaults_to_csv_gz(self) -> None:
        args = parse_builder_args([])
        self.assertEqual(args.reference_output.name, "reference_C10_poes_meped_boundary.csv.gz")

    def test_builder_rejects_uncompressed_production_output(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                parse_builder_args(["--reference-output", "reference.csv"])

    def test_missing_gzip_reports_existing_legacy_csv(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            legacy = Path(directory) / "reference.csv"
            legacy.write_text("placeholder\n")
            with self.assertRaisesRegex(FileNotFoundError, "gzip -n -9"):
                load_reference(Path(directory) / "reference.csv.gz")

    def test_gzip_output_is_deterministic_and_runner_reads_it(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "first.csv.gz"
            second = Path(directory) / "second.csv.gz"
            write_reference_csv([one_cell()], first, "manifest-sha")
            write_reference_csv([one_cell()], second, "manifest-sha")

            # Gzip magic number and byte-for-byte equality confirm that the
            # product is compressed and that mtime/filename metadata do not
            # make identical reference builds produce different checksums.
            self.assertEqual(first.read_bytes()[:2], b"\x1f\x8b")
            self.assertEqual(first.read_bytes(), second.read_bytes())

            rows = load_reference(first)
            self.assertEqual(len(rows), 1)
            self.assertEqual(rows[0].channel, "P6")
            self.assertAlmostEqual(rows[0].boundary_lat_deg or 0.0, 65.25)
            self.assertEqual(rows[0].validation_role, "PRIMARY")
            self.assertTrue(rows[0].acceptance_eligible)
            self.assertTrue(rows[0].background_corrected)


if __name__ == "__main__":
    unittest.main()
