#!/usr/bin/env python3
"""Convert calculator-export CSV rows to the strict C7 reference schema.

This utility deliberately does *not* scrape or submit either external website.
GeoMagSphere currently returns calculator results asynchronously by email, and
IZMIRAN exposes an interactive service rather than a documented bulk API.  The
safe and reproducible workflow is therefore:

1. Export or transcribe the numerical calculator result into a small CSV.
2. Run this converter with the exact model, epoch, solar-wind/geomagnetic
   drivers, and calculator version used for that result.
3. Validate the resulting strict CSV with ``run_C7.py --validate-references``.

The input CSV may use common aliases for coordinates and cutoff columns. The
output always uses the fixed C7 schema, including independent W1..W6 columns so
that the complete GeoMagSphere T05 state is archived explicitly. IZMIRAN is
accepted only for the secondary T96 comparison.
"""

from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Dict, Iterable, Mapping, Optional, Sequence


ALIASES = {
    "latitude_deg": (
        "latitude_deg", "latitude", "lat", "geo_lat", "geographic_latitude"
    ),
    "longitude_deg_east": (
        "longitude_deg_east", "longitude", "lon", "long", "geo_lon",
        "geographic_longitude",
    ),
    "altitude_km": ("altitude_km", "altitude", "alt_km", "height_km"),
    "rc_lower_gv": (
        "rc_lower_gv", "rc_lower", "lower", "lower_cutoff", "r_lower"
    ),
    "rc_upper_gv": (
        "rc_upper_gv", "rc_upper", "upper", "upper_cutoff", "r_upper"
    ),
    "rc_effective_gv": (
        "rc_effective_gv", "rc_effective", "effective", "effective_cutoff",
        "r_eff",
    ),
    "request_id": ("request_id", "id", "case_id"),
}

OUT_FIELDS = [
    "request_id", "reference_source", "model", "epoch_utc", "latitude_deg",
    "longitude_deg_east", "altitude_km", "rc_lower_gv", "rc_upper_gv",
    "rc_effective_gv", "effective_definition", "rigidity_step_gv", "dst_nT",
    "pdyn_nPa", "imf_bx_nT", "imf_by_nT", "imf_bz_nT", "sw_vx_kms",
    "sw_n_cm3", "w1", "w2", "w3", "w4", "w5", "w6",
    "reference_version", "citation", "notes",
]


def find_value(
    raw: Mapping[str, object], canonical: str, *, required: bool = True
) -> str:
    """Find one canonical value through the accepted case-insensitive aliases."""

    lower = {str(key).strip().lower(): value for key, value in raw.items()}
    for alias in ALIASES[canonical]:
        value = lower.get(alias.lower())
        if value is not None and str(value).strip() != "":
            return str(value).strip()
    if required:
        raise ValueError(
            "missing %s (accepted aliases: %s)"
            % (canonical, ", ".join(ALIASES[canonical]))
        )
    return ""


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter
    )
    parser.add_argument("input", help="calculator-export or hand-transcribed CSV")
    parser.add_argument("output", help="strict C7 reference CSV to create")
    parser.add_argument("--source", choices=("GEOMAGSPHERE", "IZMIRAN"), required=True)
    parser.add_argument("--model", choices=("T96", "T05"), required=True)
    parser.add_argument("--epoch", required=True, help="UTC timestamp used by the calculator")
    parser.add_argument(
        "--effective-definition",
        choices=("MIDPOINT", "PENUMBRA_INTEGRAL", "REPORTED_UNKNOWN", "NONE"),
        default=None,
        help=(
            "meaning of the exported effective cutoff; GeoMagSphere is forced "
            "to MIDPOINT, while IZMIRAN must be declared explicitly"
        ),
    )
    parser.add_argument("--rigidity-step-gv", type=float, default=0.1)
    parser.add_argument("--dst", type=float, required=True, help="Dst/SYM-H in nT")
    parser.add_argument("--pdyn", type=float, required=True, help="solar-wind dynamic pressure in nPa")
    parser.add_argument("--bx", type=float, default=0.0, help="IMF Bx in nT")
    parser.add_argument("--by", type=float, required=True, help="IMF By in nT")
    parser.add_argument("--bz", type=float, required=True, help="IMF Bz in nT")
    parser.add_argument("--vx", type=float, default=-450.0, help="solar-wind Vx in km/s")
    parser.add_argument("--density", type=float, default=5.0, help="solar-wind density in cm^-3")
    for index in range(1, 7):
        parser.add_argument(
            "--w%d" % index,
            type=float,
            default=0.0,
            help="T05 storm-history integral W%d" % index,
        )
    parser.add_argument("--reference-version", default="")
    parser.add_argument("--citation", default="")
    parser.add_argument("--notes", default="")
    return parser.parse_args(argv)


def convert_rows(args: argparse.Namespace) -> Iterable[Dict[str, object]]:
    source = args.source
    if source == "IZMIRAN" and args.model != "T96":
        raise ValueError(
            "IZMIRAN is supported only as the C7 secondary T96 reference; "
            "use GeoMagSphere for the controlled T05 table"
        )
    definition = args.effective_definition
    if source == "GEOMAGSPHERE":
        if definition not in (None, "MIDPOINT"):
            raise ValueError("GeoMagSphere effective_definition must be MIDPOINT")
        definition = "MIDPOINT"
    elif definition is None:
        raise ValueError("--effective-definition is required for IZMIRAN")

    input_path = Path(args.input)
    with input_path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        for line_number, raw in enumerate(reader, 2):
            try:
                latitude = float(find_value(raw, "latitude_deg"))
                longitude_input = float(find_value(raw, "longitude_deg_east"))
                if not -90.0 <= latitude <= 90.0:
                    raise ValueError("latitude must be within [-90,90] degrees")
                if source == "IZMIRAN" and not -180.0 <= longitude_input <= 180.0:
                    raise ValueError(
                        "IZMIRAN longitude must be within [-180,180] degrees; "
                        "use -90 instead of 270"
                    )
                longitude = longitude_input % 360.0
                altitude = float(find_value(raw, "altitude_km"))
                lower = float(find_value(raw, "rc_lower_gv"))
                upper = float(find_value(raw, "rc_upper_gv"))
                effective_text = find_value(raw, "rc_effective_gv", required=False)
                effective: object = float(effective_text) if effective_text else ""
                request_id = find_value(raw, "request_id", required=False)
                if not request_id:
                    request_id = "%s_%s_lat%+g_lon%03g" % (
                        source.lower(), args.model.lower(), latitude, longitude
                    )
            except Exception as exc:
                raise ValueError("%s:%d: %s" % (input_path, line_number, exc)) from exc

            if upper < lower:
                raise ValueError("%s:%d upper cutoff is below lower cutoff" % (input_path, line_number))

            if source == "GEOMAGSPHERE":
                if effective == "":
                    raise ValueError(
                        "%s:%d GeoMagSphere export is missing effective cutoff"
                        % (input_path, line_number)
                    )
                midpoint = 0.5 * (lower + upper)
                tolerance = max(0.051, 0.6 * args.rigidity_step_gv)
                if abs(float(effective) - midpoint) > tolerance:
                    raise ValueError(
                        "%s:%d effective %.6g is inconsistent with documented "
                        "lower/upper midpoint %.6g"
                        % (input_path, line_number, float(effective), midpoint)
                    )

            row: Dict[str, object] = {
                "request_id": request_id,
                "reference_source": source,
                "model": args.model,
                "epoch_utc": args.epoch,
                "latitude_deg": latitude,
                "longitude_deg_east": longitude,
                "altitude_km": altitude,
                "rc_lower_gv": lower,
                "rc_upper_gv": upper,
                "rc_effective_gv": effective,
                "effective_definition": definition,
                "rigidity_step_gv": args.rigidity_step_gv,
                "dst_nT": args.dst,
                "pdyn_nPa": args.pdyn,
                "imf_bx_nT": args.bx,
                "imf_by_nT": args.by,
                "imf_bz_nT": args.bz,
                "sw_vx_kms": args.vx,
                "sw_n_cm3": args.density,
                "reference_version": args.reference_version,
                "citation": args.citation,
                "notes": args.notes,
            }
            for index in range(1, 7):
                row["w%d" % index] = getattr(args, "w%d" % index)
            yield {field: row[field] for field in OUT_FIELDS}


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser_args = parse_args(argv)
    try:
        rows = list(convert_rows(parser_args))
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    if not rows:
        raise SystemExit("input contains no rows")

    output_path = Path(parser_args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=OUT_FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    print("Wrote %d C7 reference rows to %s" % (len(rows), output_path))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
