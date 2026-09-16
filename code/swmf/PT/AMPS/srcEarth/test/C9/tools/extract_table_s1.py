#!/usr/bin/env python3
"""Recreate ``reference_C9_pamela_table_s1.csv`` from the AGU supplement PDF.

The checked-in CSV is the runtime reference and does not require Camelot.  This
utility documents the acquisition/transcription path and is intended for audit
or future regeneration.  It requires ``camelot-py`` and a PDF renderer supported
by Camelot.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import math
import re
from datetime import datetime, timedelta, timezone
from pathlib import Path

import camelot

CELL = re.compile(
    r"^\s*(\d+\.\d+)\+(\d+):(\d+)\s*\n\(cid:0\)(\d+):(\d+)\s*$"
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pdf", help="swe20314-sup-0001-supinfo.pdf")
    parser.add_argument("output", help="Output CSV")
    args = parser.parse_args()
    pdf = Path(args.pdf).resolve()
    table = camelot.read_pdf(str(pdf), pages="2", flavor="stream")[0].df

    bins = []
    for raw in table.iloc[1, 1:].tolist():
        lo, hi = [float(value) for value in raw.replace("{", "-").split("-")]
        bins.append((lo, hi, math.sqrt(lo * hi)))

    source_sha = hashlib.sha256(pdf.read_bytes()).hexdigest()
    rows = []
    for row_index in range(3, 40):
        token = table.iloc[row_index, 0].strip()
        midpoint = datetime(
            2006, 12, int(token[:2]), int(token[3:5]), int(token[5:7]),
            tzinfo=timezone.utc,
        )
        for column, (lo, hi, center) in enumerate(bins, start=1):
            raw = table.iloc[row_index, column].strip()
            missing = raw in ("", "{", "-", "–")
            value = plus = minus = ""
            if not missing:
                match = CELL.match(raw)
                if not match:
                    raise RuntimeError("unparsed cell %d,%d: %r" %
                                       (row_index, column, raw))
                value = "%.2f" % float(match.group(1))
                plus = "%.2f" % float(match.group(2) + "." + match.group(3))
                minus = "%.2f" % float(match.group(4) + "." + match.group(5))
            rows.append({
                "interval_midpoint_utc": midpoint.strftime("%Y-%m-%dT%H:%M:%SZ"),
                "interval_start_utc": (midpoint - timedelta(minutes=47)).strftime(
                    "%Y-%m-%dT%H:%M:%SZ"),
                "interval_end_utc": (midpoint + timedelta(minutes=47)).strftime(
                    "%Y-%m-%dT%H:%M:%SZ"),
                "rigidity_min_gv": "%.2f" % lo,
                "rigidity_max_gv": "%.2f" % hi,
                "rigidity_geometric_center_gv": "%.9f" % center,
                "pamela_cutoff_aacgm_deg": value,
                "sigma_plus_deg": plus,
                "sigma_minus_deg": minus,
                "missing": str(missing).upper(),
                "source": ("Adriani et al. (2016), Space Weather, "
                           "doi:10.1002/2016SW001364, Supporting Information Table S1"),
                "source_pdf_sha256": source_sha,
                "notes": ("Dates are midpoints of 94-minute intervals; cutoff "
                          "latitudes are AACGM latitude magnitudes."),
            })

    output = Path(args.output).resolve()
    with output.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader(); writer.writerows(rows)
    print("Wrote %d rows (%d missing measurements)" %
          (len(rows), sum(row["missing"] == "TRUE" for row in rows)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
