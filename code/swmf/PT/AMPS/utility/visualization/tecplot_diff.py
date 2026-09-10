#!/usr/bin/env python3
"""
tecplot_diff.py

Compute numerical differences between Tecplot ASCII output files.

The first file in the list is always treated as the base file.

Example:

    python3 tecplot_diff.py base.dat run001.dat run002.dat run003.dat

This computes:

    run001.dat - base.dat
    run002.dat - base.dat
    run003.dat - base.dat

and writes:

    run001_minus_base.dat
    run002_minus_base.dat
    run003_minus_base.dat

Use -d/--output-dir to place all output files in a selected directory:

    python3 tecplot_diff.py base.dat run001.dat run002.dat -d DIFF_OUTPUT

Default signed difference:

    diff = file - base

Relative difference:

    diff = 2 * (file - base) / (file + base)

Optional absolute value:

    diff = abs(diff)
"""

import argparse
import math
import re
import sys
from pathlib import Path


def is_title_line(line):
    return re.match(r"^\s*TITLE\b", line, re.IGNORECASE) is not None


def is_variables_line(line):
    return re.match(r"^\s*VARIABLES\b", line, re.IGNORECASE) is not None


def is_zone_line(line):
    return re.match(r"^\s*ZONE\b", line, re.IGNORECASE) is not None


def parse_float_tokens(line):
    line = line.replace(",", " ")
    parts = line.split()

    values = []

    for part in parts:
        values.append(float(part))

    return values


def read_tecplot_ascii(filename):
    filename = Path(filename)

    if not filename.exists():
        raise FileNotFoundError("File does not exist: {}".format(filename))

    title_line = None
    variable_lines = []
    zones_raw = []

    current_zone = None
    in_variables = False

    with filename.open("r") as f:
        for line_number, line in enumerate(f, start=1):
            raw_line = line.rstrip("\n")
            stripped = raw_line.strip()

            if not stripped:
                continue

            if stripped.startswith("#"):
                continue

            if is_zone_line(stripped):
                in_variables = False

                current_zone = {
                    "header": raw_line,
                    "tokens": [],
                    "start_line": line_number,
                }

                zones_raw.append(current_zone)
                continue

            if current_zone is None:
                if is_title_line(stripped):
                    title_line = raw_line
                    continue

                if is_variables_line(stripped):
                    in_variables = True
                    variable_lines.append(raw_line)
                    continue

                if in_variables:
                    variable_lines.append(raw_line)
                    continue

                continue

            try:
                values = parse_float_tokens(stripped)
            except ValueError:
                raise ValueError(
                    "Non-numeric line inside data section in {} at line {}:\n{}"
                    .format(filename, line_number, raw_line)
                )

            current_zone["tokens"].extend(values)

    if not variable_lines:
        raise ValueError("Could not find VARIABLES line in {}".format(filename))

    variable_text = "\n".join(variable_lines)
    variable_names = re.findall(r'"([^"]*)"', variable_text)

    if not variable_names:
        raise ValueError(
            "Could not parse variable names from VARIABLES section in {}"
            .format(filename)
        )

    nvar = len(variable_names)

    if not zones_raw:
        raise ValueError("Could not find any ZONE sections in {}".format(filename))

    zones = []

    for izone, zone in enumerate(zones_raw, start=1):
        tokens = zone["tokens"]

        if len(tokens) % nvar != 0:
            raise ValueError(
                "Zone {} in {} has {} numeric values, which is not divisible "
                "by the number of variables ({})"
                .format(izone, filename, len(tokens), nvar)
            )

        npoints = len(tokens) // nvar
        data = []

        for i in range(npoints):
            i0 = i * nvar
            i1 = i0 + nvar
            data.append(tokens[i0:i1])

        zones.append({
            "header": zone["header"],
            "data": data,
        })

    return title_line, variable_names, zones


def check_compatible(file1, vars1, zones1, file2, vars2, zones2):
    if len(vars1) != len(vars2):
        raise ValueError(
            "Different number of variables: {} has {}, {} has {}"
            .format(file1, len(vars1), file2, len(vars2))
        )

    if len(zones1) != len(zones2):
        raise ValueError(
            "Different number of zones: {} has {}, {} has {}"
            .format(file1, len(zones1), file2, len(zones2))
        )

    for i, item in enumerate(zip(vars1, vars2), start=1):
        v1, v2 = item

        if v1 != v2:
            print(
                "WARNING: variable {} has different names: '{}' vs '{}'"
                .format(i, v1, v2),
                file=sys.stderr,
            )

    for izone, item in enumerate(zip(zones1, zones2), start=1):
        z1, z2 = item

        n1 = len(z1["data"])
        n2 = len(z2["data"])

        if n1 != n2:
            raise ValueError(
                "Different number of data points in zone {}: "
                "{} has {}, {} has {}"
                .format(izone, file1, n1, file2, n2)
            )


def compute_difference(value, base_value, relative, zero_denom_value):
    if relative:
        denom = value + base_value

        if denom == 0.0:
            if value == 0.0 and base_value == 0.0:
                return 0.0

            return zero_denom_value

        return 2.0 * (value - base_value) / denom

    return value - base_value


def make_output_name(input_file, base_file, output_dir):
    input_file = Path(input_file)
    base_file = Path(base_file)

    output_name = "{}_minus_{}{}".format(
        input_file.stem,
        base_file.stem,
        input_file.suffix,
    )

    if output_dir is None:
        return input_file.with_name(output_name)

    return Path(output_dir) / output_name


def resolve_output_file(output_name, output_dir):
    """
    Resolve explicit -o/--output name.

    If output_dir is given and output_name is relative, place it inside output_dir.
    If output_name is absolute, use it exactly as given.
    """

    output_path = Path(output_name)

    if output_path.is_absolute():
        return output_path

    if output_dir is not None:
        return Path(output_dir) / output_path

    return output_path


def write_difference_file(
    output_file,
    input_file,
    base_file,
    variable_names,
    input_zones,
    base_zones,
    relative,
    abs_value,
    copy_cols,
    zero_denom_value,
):
    output_file = Path(output_file)
    nvar = len(variable_names)

    if copy_cols < 0:
        raise ValueError("--copy-cols cannot be negative")

    if copy_cols > nvar:
        raise ValueError(
            "--copy-cols={} is larger than the number of variables ({})"
            .format(copy_cols, nvar)
        )

    count = 0
    max_abs_diff = 0.0
    max_abs_location = None
    sum_sq = 0.0
    n_nan = 0

    with output_file.open("w") as out:
        if relative:
            diff_type = "relative difference"
        else:
            diff_type = "difference"

        if abs_value:
            diff_type = "absolute value of " + diff_type

        out.write(
            'TITLE="{}: {} minus {}"\n'
            .format(diff_type, Path(input_file).name, Path(base_file).name)
        )

        out.write("VARIABLES=")
        out.write(",".join('"{}"'.format(name) for name in variable_names))
        out.write("\n")

        for izone, item in enumerate(zip(input_zones, base_zones), start=1):
            input_zone, base_zone = item

            out.write(input_zone["header"] + "\n")

            for ipoint, rows in enumerate(
                zip(input_zone["data"], base_zone["data"]),
                start=1,
            ):
                input_row, base_row = rows
                out_values = []

                for ivar in range(nvar):
                    if ivar < copy_cols:
                        value = input_row[ivar]
                    else:
                        value = compute_difference(
                            input_row[ivar],
                            base_row[ivar],
                            relative=relative,
                            zero_denom_value=zero_denom_value,
                        )

                        if abs_value:
                            value = abs(value)

                        if math.isnan(value):
                            n_nan += 1
                        else:
                            adiff = abs(value)
                            sum_sq += value * value
                            count += 1

                            if adiff > max_abs_diff:
                                max_abs_diff = adiff
                                max_abs_location = (izone, ipoint, ivar + 1)

                    out_values.append(value)

                out.write(" ".join("{:.16e}".format(v) for v in out_values))
                out.write("\n")

    if count > 0:
        rms = math.sqrt(sum_sq / count)
    else:
        rms = float("nan")

    print("Created output file: {}".format(output_file))
    print("  input file:                 {}".format(input_file))
    print("  base file:                  {}".format(base_file))
    print("  compared variables/point:   {}".format(nvar - copy_cols))
    print("  copied variables/point:     {}".format(copy_cols))
    print("  number of compared values:  {}".format(count))
    print("  number of NaN values:       {}".format(n_nan))
    print("  RMS difference:             {:.16e}".format(rms))
    print("  maximum absolute difference:{: .16e}".format(max_abs_diff))

    if max_abs_location is not None:
        izone, ipoint, ivar = max_abs_location
        print(
            "  maximum location:           zone {}, point {}, variable {} ({})"
            .format(izone, ipoint, ivar, variable_names[ivar - 1])
        )


def compare_to_base(
    input_file,
    base_file,
    base_vars,
    base_zones,
    output_file,
    relative,
    abs_value,
    copy_cols,
    zero_denom_value,
):
    title, input_vars, input_zones = read_tecplot_ascii(input_file)

    check_compatible(
        file1=input_file,
        vars1=input_vars,
        zones1=input_zones,
        file2=base_file,
        vars2=base_vars,
        zones2=base_zones,
    )

    write_difference_file(
        output_file=output_file,
        input_file=input_file,
        base_file=base_file,
        variable_names=input_vars,
        input_zones=input_zones,
        base_zones=base_zones,
        relative=relative,
        abs_value=abs_value,
        copy_cols=copy_cols,
        zero_denom_value=zero_denom_value,
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "Compute numerical differences between Tecplot ASCII files. "
            "The first file in the list is always the base file. "
            "Each following file is compared as: file - base."
        )
    )

    parser.add_argument(
        "files",
        nargs="+",
        help=(
            "Input files. The first file is the base file. "
            "All following files are compared against the base."
        ),
    )

    parser.add_argument(
        "-o",
        "--output",
        default=None,
        help=(
            "Optional output file name. This can only be used when exactly "
            "two files are provided: base file and one input file. "
            "If -d/--output-dir is also given and this path is relative, "
            "the output file is placed in that directory."
        ),
    )

    parser.add_argument(
        "-d",
        "--output-dir",
        default=None,
        help=(
            "Directory where output files will be written. "
            "The directory is created if it does not exist. "
            "Default: write each output next to the corresponding input file."
        ),
    )

    parser.add_argument(
        "--relative",
        action="store_true",
        help="Use relative difference: 2 * (file - base) / (file + base).",
    )

    parser.add_argument(
        "--abs-value",
        "--abs",
        action="store_true",
        help=(
            "Output the absolute value of the difference. "
            "This applies to either ordinary difference or relative difference."
        ),
    )

    parser.add_argument(
        "--copy-cols",
        type=int,
        default=0,
        help=(
            "Copy the first N variables from the input file without differencing. "
            "Useful for coordinate columns. For lon/lat/Rc/Emin use --copy-cols 2. "
            "Default: 0."
        ),
    )

    parser.add_argument(
        "--zero-denom-value",
        type=float,
        default=float("nan"),
        help=(
            "Value to write for relative difference when file + base = 0 "
            "and the numerator is nonzero. Default: NaN."
        ),
    )

    args = parser.parse_args()

    try:
        if len(args.files) < 2:
            raise ValueError(
                "At least two files are required:\n"
                "    python3 tecplot_diff.py base.dat run001.dat\n"
                "or:\n"
                "    python3 tecplot_diff.py base.dat run001.dat run002.dat"
            )

        if args.output is not None and len(args.files) != 2:
            raise ValueError(
                "-o/--output can only be used when exactly two files are given: "
                "base.dat run001.dat"
            )

        if args.output_dir is not None:
            output_dir = Path(args.output_dir)
            output_dir.mkdir(parents=True, exist_ok=True)

            if not output_dir.is_dir():
                raise ValueError(
                    "-d/--output-dir is not a directory: {}".format(output_dir)
                )
        else:
            output_dir = None

        base_file = args.files[0]
        input_files = args.files[1:]

        print("Base file: {}".format(base_file))

        base_title, base_vars, base_zones = read_tecplot_ascii(base_file)

        for input_file in input_files:
            if args.output is not None:
                output_file = resolve_output_file(
                    output_name=args.output,
                    output_dir=output_dir,
                )
            else:
                output_file = make_output_name(
                    input_file=input_file,
                    base_file=base_file,
                    output_dir=output_dir,
                )

            compare_to_base(
                input_file=input_file,
                base_file=base_file,
                base_vars=base_vars,
                base_zones=base_zones,
                output_file=output_file,
                relative=args.relative,
                abs_value=args.abs_value,
                copy_cols=args.copy_cols,
                zero_denom_value=args.zero_denom_value,
            )

    except Exception as err:
        print("ERROR: {}".format(err), file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
