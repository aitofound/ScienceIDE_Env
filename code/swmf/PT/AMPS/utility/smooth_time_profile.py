#!/usr/bin/env python3
"""
smooth_dat.py  --  smooth and inspect time-series .dat files with energy-band columns.

Usage examples:
  python smooth_dat.py -h
  python smooth_dat.py -f data.dat -stat
  python smooth_dat.py -f data.dat -smooth 5 -plot
  python smooth_dat.py -f data.dat -smooth 7 -fout smoothed.dat
  python smooth_dat.py -f data.dat -smooth 5 -plot -x-scale log -y-scale log
"""

import sys
import os
import re
import math


# ---------------------------------------------------------------------------
# Help text
# ---------------------------------------------------------------------------
HELP_TEXT = """
smooth_dat.py  --  smooth and inspect time-series .dat files
=============================================================

OPTIONS
  -h | --h              Print this help and exit.

  -f  <name>            Path to the input .dat file (required for all
                        operations except -h).

  -stat                 Print statistics: time range, number of points,
                        time step info, per-channel min/max/mean/peak.

  -smooth <N>           Smooth all data channels by a running average over
                        N consecutive time points (N must be a positive
                        odd integer; if even, N+1 is used). The smoothed
                        data are written to the output file.

  -plot                 Plot the data (original, or smoothed when -smooth is
                        also given) using matplotlib.

  -x-scale <linear|log> Scale for the time (x) axis in the plot.
                        Default: linear.

  -y-scale <linear|log> Scale for the flux (y) axis in the plot.
                        Default: linear.

  -fout <name>          Output file name for the smoothed data.
                        Default: <input-stem>.smoothed.dat

NOTES
  * The file must have a header line starting with VARIABLES= that lists
    quoted column names separated by commas.
  * All remaining lines are treated as whitespace-separated numerical data.
  * Smoothing uses a centred moving average; edge points use a smaller
    symmetric window so no data are dropped.
"""


# ---------------------------------------------------------------------------
# Argument parsing (no argparse to keep full control over -h / --h)
# ---------------------------------------------------------------------------
def parse_args(argv):
    args = {
        "file": None,
        "stat": False,
        "smooth": None,
        "plot": False,
        "x_scale": "linear",
        "y_scale": "linear",
        "fout": None,
    }
    i = 1
    while i < len(argv):
        tok = argv[i]
        if tok in ("-h", "--h", "--help", "-help"):
            print(HELP_TEXT)
            sys.exit(0)
        elif tok == "-f":
            i += 1
            if i >= len(argv):
                sys.exit("ERROR: -f requires a filename argument.")
            args["file"] = argv[i]
        elif tok == "-stat":
            args["stat"] = True
        elif tok == "-smooth":
            i += 1
            if i >= len(argv):
                sys.exit("ERROR: -smooth requires an integer argument.")
            try:
                args["smooth"] = int(argv[i])
            except ValueError:
                sys.exit(f"ERROR: -smooth argument must be an integer, got '{argv[i]}'.")
        elif tok == "-plot":
            args["plot"] = True
        elif tok == "-x-scale":
            i += 1
            if i >= len(argv):
                sys.exit("ERROR: -x-scale requires 'linear' or 'log'.")
            val = argv[i].lower()
            if val not in ("linear", "log"):
                sys.exit(f"ERROR: -x-scale must be 'linear' or 'log', got '{argv[i]}'.")
            args["x_scale"] = val
        elif tok == "-y-scale":
            i += 1
            if i >= len(argv):
                sys.exit("ERROR: -y-scale requires 'linear' or 'log'.")
            val = argv[i].lower()
            if val not in ("linear", "log"):
                sys.exit(f"ERROR: -y-scale must be 'linear' or 'log', got '{argv[i]}'.")
            args["y_scale"] = val
        elif tok == "-fout":
            i += 1
            if i >= len(argv):
                sys.exit("ERROR: -fout requires a filename argument.")
            args["fout"] = argv[i]
        else:
            sys.exit(f"ERROR: Unknown option '{tok}'. Use -h for help.")
        i += 1
    return args


# ---------------------------------------------------------------------------
# File I/O
# ---------------------------------------------------------------------------
def read_dat(path):
    """
    Returns (header_line, col_names, time_arr, data_matrix).
    data_matrix shape: (n_times, n_channels).
    """
    if not os.path.isfile(path):
        sys.exit(f"ERROR: File not found: {path}")

    with open(path, "r") as fh:
        lines = fh.readlines()

    if not lines:
        sys.exit("ERROR: File is empty.")

    header_line = lines[0].rstrip("\n")
    col_names = _parse_header(header_line)
    if len(col_names) < 2:
        sys.exit("ERROR: Header must list at least 2 columns (time + one data channel).")

    time_list = []
    data_list = []
    for lineno, raw in enumerate(lines[1:], start=2):
        raw = raw.strip()
        if not raw or raw.startswith("#"):
            continue
        parts = raw.split()
        if len(parts) != len(col_names):
            sys.exit(
                f"ERROR: Line {lineno} has {len(parts)} values but header declares "
                f"{len(col_names)} columns."
            )
        try:
            nums = [float(p) for p in parts]
        except ValueError as exc:
            sys.exit(f"ERROR: Non-numeric value on line {lineno}: {exc}")
        time_list.append(nums[0])
        data_list.append(nums[1:])

    if not time_list:
        sys.exit("ERROR: No data rows found in file.")

    return header_line, col_names, time_list, data_list


def _parse_header(header_line):
    """Extract column names from a VARIABLES=\"...\", \"...\" header."""
    # Find everything inside double-quotes
    names = re.findall(r'"([^"]+)"', header_line)
    if names:
        return names
    # Fallback: treat space-separated tokens after VARIABLES= as names
    m = re.search(r'VARIABLES\s*=\s*(.*)', header_line, re.IGNORECASE)
    if m:
        return [t.strip() for t in m.group(1).split(",")]
    sys.exit("ERROR: Could not parse VARIABLES header line.")


def write_dat(path, header_line, col_names, time_list, data_smoothed):
    """Write smoothed data preserving the original header."""
    with open(path, "w") as fh:
        fh.write(header_line + "\n")
        for t, row in zip(time_list, data_smoothed):
            vals = [f"{t:14.6e}"] + [f"  {v:14.6e}" for v in row]
            fh.write("".join(vals) + "\n")
    print(f"Smoothed data written to: {path}")


def default_output_name(input_path):
    base = os.path.basename(input_path)
    stem, ext = os.path.splitext(base)
    return stem + ".smoothed" + ext


# ---------------------------------------------------------------------------
# Statistics
# ---------------------------------------------------------------------------
def print_stats(col_names, time_list, data_list):
    n = len(time_list)
    t_min = time_list[0]
    t_max = time_list[-1]

    # Time-step analysis
    dts = [time_list[i+1] - time_list[i] for i in range(n - 1)]
    dt_min = min(dts) if dts else float("nan")
    dt_max = max(dts) if dts else float("nan")
    dt_mean = sum(dts) / len(dts) if dts else float("nan")
    uniform = (dt_max - dt_min) < 1e-6 * abs(dt_mean) if dt_mean else False

    print("=" * 62)
    print("FILE STATISTICS")
    print("=" * 62)
    print(f"  Number of time points : {n}")
    print(f"  Time range            : {t_min:.6e}  -->  {t_max:.6e}")
    print(f"  Total time span       : {t_max - t_min:.6e}")
    print(f"  Time step  (mean)     : {dt_mean:.6e}")
    print(f"  Time step  (min/max)  : {dt_min:.6e} / {dt_max:.6e}")
    print(f"  Uniform spacing       : {'yes' if uniform else 'no (irregular)'}")
    print()

    n_ch = len(col_names) - 1
    print(f"  Data channels ({n_ch}):")
    print(f"  {'Channel':<45}  {'min':>12}  {'max':>12}  {'mean':>12}  {'t_peak':>12}")
    print("  " + "-" * 97)
    for ch in range(n_ch):
        vals = [row[ch] for row in data_list]
        ch_min = min(vals)
        ch_max = max(vals)
        ch_mean = sum(vals) / n
        idx_peak = vals.index(ch_max)
        t_peak = time_list[idx_peak]
        label = col_names[ch + 1]
        # truncate long labels
        if len(label) > 44:
            label = label[:41] + "..."
        print(f"  {label:<45}  {ch_min:>12.4e}  {ch_max:>12.4e}  {ch_mean:>12.4e}  {t_peak:>12.4e}")
    print("=" * 62)


# ---------------------------------------------------------------------------
# Smoothing
# ---------------------------------------------------------------------------
def smooth_data(data_list, n_window):
    """
    Centred moving average with half-width = n_window // 2.
    Edge points use a symmetric window that shrinks to avoid going out of bounds.
    Returns a new list of rows (same structure as data_list).
    """
    n = len(data_list)
    n_ch = len(data_list[0])
    half = n_window // 2

    smoothed = []
    for i in range(n):
        lo = max(0, i - half)
        hi = min(n - 1, i + half)
        # Keep window symmetric around i
        sym_half = min(i - lo, hi - i)
        lo = i - sym_half
        hi = i + sym_half
        count = hi - lo + 1
        row_avg = []
        for ch in range(n_ch):
            total = sum(data_list[j][ch] for j in range(lo, hi + 1))
            row_avg.append(total / count)
        smoothed.append(row_avg)
    return smoothed


# ---------------------------------------------------------------------------
# Plotting
# ---------------------------------------------------------------------------
def plot_data(col_names, time_list, data_list, data_smoothed, x_scale, y_scale):
    try:
        import matplotlib.pyplot as plt
        import matplotlib.colors as mcolors
    except ImportError:
        sys.exit("ERROR: matplotlib is not installed. Install with: pip install matplotlib")

    n_ch = len(col_names) - 1
    channel_labels = col_names[1:]

    # Build a colour cycle
    prop_cycle = plt.rcParams["axes.prop_cycle"].by_key()["color"]
    colours = [prop_cycle[i % len(prop_cycle)] for i in range(n_ch)]

    fig, ax = plt.subplots(figsize=(12, 6))

    for ch in range(n_ch):
        orig_vals = [row[ch] for row in data_list]
        colour = colours[ch]
        label = channel_labels[ch]

        if data_smoothed is not None:
            # Faint original, solid smoothed
            ax.plot(time_list, orig_vals, color=colour, alpha=0.25, linewidth=0.8)
            sm_vals = [row[ch] for row in data_smoothed]
            ax.plot(time_list, sm_vals, color=colour, linewidth=1.4, label=label)
        else:
            ax.plot(time_list, orig_vals, color=colour, linewidth=1.2, label=label)

    ax.set_xscale(x_scale)
    ax.set_yscale(y_scale)
    ax.set_xlabel("Time", fontsize=12)
    ax.set_ylabel("Flux", fontsize=12)
    title = "Particle flux vs time"
    if data_smoothed is not None:
        title += "  (faint = original, solid = smoothed)"
    ax.set_title(title, fontsize=13)
    ax.legend(loc="upper left", fontsize=7, ncol=2, framealpha=0.7)
    ax.grid(True, linestyle="--", alpha=0.4)
    plt.tight_layout()
    plt.show()


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    args = parse_args(sys.argv)

    # Require -f for all real operations
    any_op = args["stat"] or args["smooth"] is not None or args["plot"]
    if args["file"] is None:
        if any_op:
            sys.exit("ERROR: -f <filename> is required. Use -h for help.")
        else:
            print(HELP_TEXT)
            sys.exit(0)

    header_line, col_names, time_list, data_list = read_dat(args["file"])

    # --- Statistics ----------------------------------------------------------
    if args["stat"]:
        print_stats(col_names, time_list, data_list)

    # --- Smoothing -----------------------------------------------------------
    data_smoothed = None
    if args["smooth"] is not None:
        n_win = args["smooth"]
        if n_win < 1:
            sys.exit("ERROR: -smooth value must be >= 1.")
        if n_win % 2 == 0:
            n_win += 1
            print(f"INFO: Even window size adjusted to {n_win} (nearest odd).")
        if n_win > len(time_list):
            sys.exit(
                f"ERROR: Smoothing window ({n_win}) exceeds number of data points ({len(time_list)})."
            )
        data_smoothed = smooth_data(data_list, n_win)
        print(f"Smoothing applied: {n_win}-point centred moving average.")

        # Write output
        fout = args["fout"] if args["fout"] else default_output_name(args["file"])
        write_dat(fout, header_line, col_names, time_list, data_smoothed)

    # --- Plot ----------------------------------------------------------------
    if args["plot"]:
        plot_data(
            col_names,
            time_list,
            data_list,
            data_smoothed,
            args["x_scale"],
            args["y_scale"],
        )

    if not args["stat"] and not args["smooth"] and not args["plot"]:
        print("Nothing to do. Specify -stat, -smooth N, or -plot. Use -h for help.")


if __name__ == "__main__":
    main()
