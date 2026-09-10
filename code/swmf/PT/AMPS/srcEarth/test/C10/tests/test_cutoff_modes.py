"""Unit tests for the C10 FULL_SCAN/DIRECT_ACCESS implementation."""
from __future__ import annotations

import contextlib
import io
import tempfile
import unittest
from datetime import datetime, timezone
from pathlib import Path

from run_C10 import (
    AccessRow,
    command_for,
    compare_access_states,
    estimate_access_t50_boundaries,
    parse_args,
    parse_tecplot_shell_access,
)


class AccessProductTests(unittest.TestCase):
    def test_access_parser_and_state_contract(self) -> None:
        text = (
            'TITLE="access"\n'
            'VARIABLES="lon_deg" "lat_deg" "rigidity_gv" "access_state" "allowed" "unresolved"\n'
            'ZONE T="shell" F=POINT\n'
            '0 50 0.2 0 0 0\n'
            '0 70 0.2 1 1 0\n'
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "access.dat"
            path.write_text(text)
            rows = parse_tecplot_shell_access(path)
        self.assertEqual([row.access_state for row in rows], [0, 1])

    def test_common_t50_is_recovered_from_longitude_profiles(self) -> None:
        rows = []
        for longitude in (0.0, 15.0, 30.0):
            for latitude, state in ((50.0, 0), (60.0, 0), (70.0, 1), (80.0, 1)):
                rows.append(AccessRow(
                    longitude_deg=longitude,
                    latitude_deg=latitude,
                    rigidity_gv=0.2,
                    access_state=state,
                    allowed=state,
                    unresolved=0,
                    aacgm_latitude_deg=latitude,
                    mlt_hour=0.0,
                ))
        estimates, profiles = estimate_access_t50_boundaries(
            rows, [0.2], [0.0], ["N"], 8,
            latitude_step_deg=1.0,
            min_resolved_fraction=0.66,
            minimum_edge_margin_deg=1.0,
        )
        self.assertEqual(len(estimates), 1)
        self.assertAlmostEqual(estimates[0].boundary_by_mlt[0.0], 65.0, places=8)
        self.assertTrue(profiles)

    def test_identical_products_pass_consistency_gate(self) -> None:
        rows = [AccessRow(0.0, 50.0, 0.2, 0, 0, 0),
                AccessRow(0.0, 70.0, 0.2, 1, 1, 0)]
        summary, details = compare_access_states(rows, list(rows), 0.999, 0.01)
        self.assertTrue(summary["passed"])
        self.assertEqual(details, [])


class CommandSelectionTests(unittest.TestCase):
    def test_direct_command_uses_only_rigidity_list(self) -> None:
        args = parse_args([
            "--solver", "GRIDDED", "--cutoff-evaluation", "DIRECT_ACCESS",
            "--comparison-observable", "ACCESS_T50",
        ])
        args.rigidities_gv = [0.2, 0.3]
        command = command_for(
            args, Path("/tmp/amps"), "GRIDDED",
            datetime(2006, 12, 14, tzinfo=timezone.utc),
        )
        self.assertIn("RIGIDITY_LIST", command)
        self.assertIn("-cutoff-rigidity-list-gv", command)
        self.assertNotIn("-cutoff-upper-scan-n", command)

    def test_full_command_requests_penumbra_and_companion_states(self) -> None:
        args = parse_args([
            "--solver", "GRIDDED", "--cutoff-evaluation", "FULL_SCAN",
            "--comparison-observable", "ACCESS_T50",
        ])
        args.rigidities_gv = [0.2, 0.3]
        command = command_for(
            args, Path("/tmp/amps"), "GRIDDED",
            datetime(2006, 12, 14, tzinfo=timezone.utc),
        )
        self.assertIn("PENUMBRA_SCAN", command)
        self.assertIn("-cutoff-upper-scan-n", command)
        self.assertIn("-cutoff-rigidity-list-gv", command)

    def test_direct_access_rejects_non_gridded_selection(self) -> None:
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit):
                parse_args(["--solver", "BOTH", "--cutoff-evaluation", "DIRECT_ACCESS"])


if __name__ == "__main__":
    unittest.main()
