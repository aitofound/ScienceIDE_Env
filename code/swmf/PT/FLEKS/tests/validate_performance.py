#!/usr/bin/env python3
"""Performance regression runner for standalone FLEKS.

Runs the full-PIC beam and hybrid-PIC whistler benchmarks on 1 and 2 MPI
processes (3 runs each for statistical robustness), parses AMReX
TinyProfiler output to extract particle-mover and field-solver timings, and
reports particle-step rates (μs/part-step) and parallel speedup against
baseline targets.
"""
import os
import shutil
import subprocess
import re
import sys
import time
import glob

MOVERS = [
    "Pts::charged_particle_mover",
    "Pts::charged_particle_mover_cell_centered",
]

FULLPIC_SOLVERS = [
    "Pic::update_E_impl",
    "Pic::E_iterate",
    "Pic::E_matvec",
]

HYBRID_SOLVERS = [
    "Pic::update_B_hybrid",
    "Pic::assemble_ohm_E",
]

_REQUIRED_MOVER = {
    "fullpic": "Pts::charged_particle_mover",
    "hybrid":  "Pts::charged_particle_mover_cell_centered",
}

# Baseline targets (μs/part-step) and 2-core speedup floor, one set per solver.
BASELINES = {
    "fullpic": {
        "total_pps": 10.0,   # total wall-clock (GHA VM)
        "mover_pps": 0.2,    # isolated particle mover
        "solver_pps": 5.0,   # isolated implicit field solver (E_iterate)
        "speedup": 1.5,      # 2-core scaling floor
    },
    "hybrid": {
        "total_pps": 0.4,    # total wall-clock
        "mover_pps": 0.1,    # isolated particle mover
        "solver_pps": 0.1,   # explicit hybrid field advance is cheaper
        "speedup": 1.5,      # 2-core scaling floor
    },
}

def safe_symlink(src, dst):
    if os.path.lexists(dst):
        if os.path.islink(dst) or os.path.isfile(dst):
            os.remove(dst)
        elif os.path.isdir(dst):
            shutil.rmtree(dst)
    os.symlink(src, dst)

def prepare_run_dir(run_dir):
    os.makedirs(run_dir, exist_ok=True)
    safe_symlink(os.path.abspath("../bin/FLEKS.exe"), os.path.join(run_dir, "FLEKS.exe"))
    
    # Component plots and restart directories
    pc_dir = os.path.join(run_dir, "PC")
    os.makedirs(pc_dir, exist_ok=True)
    os.makedirs(os.path.join(pc_dir, "plots"), exist_ok=True)
    os.makedirs(os.path.join(pc_dir, "restartOUT"), exist_ok=True)

def _param_float(block, idx):
    """Read the idx-th numeric token of a PARAM block (list of stripped lines)."""
    try:
        return float(block[idx].split()[0])
    except (IndexError, ValueError):
        return None


def _read_param_block(param_file, command):
    """Return the (stripped) lines belonging to a #COMMAND block of a PARAM file."""
    block = []
    capture = False
    with open(param_file) as f:
        for line in f:
            s = line.strip()
            if not s:
                continue
            if s.startswith("#"):
                capture = (s.split()[0].upper() == command.upper())
                continue
            if capture:
                block.append(s)
    return block


def count_particles_from_param(param_file):
    """Total seeded macroparticles = prod(nCell_d * nPartPerCell_d) from the
    #NCELL and #PARTICLES blocks.  FLEKS seeds exactly nPartPerCell particles
    per cell, so this is exact for both benchmarks.  Returns None if the blocks
    are missing/malformed."""
    ncell = _read_param_block(param_file, "#NCELL")
    ppc = _read_param_block(param_file, "#PARTICLES")
    if len(ncell) < 3 or len(ppc) < 3:
        return None
    total = 1
    for d in range(3):
        nc = _param_float(ncell, d)
        np = _param_float(ppc, d)
        if nc is None or np is None:
            return None
        total *= int(nc) * int(np)
    return total


def cycles_from_param(param_file):
    """Number of timesteps for a fixed-dt run, from #TIMESTEPPING dt and
    #STOP (MaxIter if > 0, else round(TimeMax / dt)).  Returns None if the
    values cannot be determined."""
    dt_block = _read_param_block(param_file, "#TIMESTEPPING")
    stop_block = _read_param_block(param_file, "#STOP")
    use_fixed = dt_block[0].split()[0] if len(dt_block) >= 1 else None
    dt = _param_float(dt_block, 1) if len(dt_block) >= 2 else None
    max_iter = _param_float(stop_block, 0) if len(stop_block) >= 1 else None
    time_max = _param_float(stop_block, 1) if len(stop_block) >= 2 else None
    if max_iter is not None and int(max_iter) > 0:
        return int(max_iter)
    if use_fixed and use_fixed.upper() == "T" and dt and dt > 0 and time_max:
        return int(round(time_max / dt))
    return None

def parse_tiny_profiler(stdout):
    """Parse AMReX TinyProfiler total time, exclusive and inclusive timing
    tables from FLEKS stdout into nested dicts."""
    profiler_data = {}

    total_time_match = re.search(r"TinyProfiler total time across processes\s+\[min\.\.\.avg\.\.\.max\]:\s+([\d.]+)", stdout)
    if total_time_match:
        profiler_data["total_time"] = float(total_time_match.group(1))
    else:
        total_time_match2 = re.search(r"TinyProfiler total time across processes.*?:\s+([\d.]+)", stdout)
        if total_time_match2:
            profiler_data["total_time"] = float(total_time_match2.group(1))

    excl_section_match = re.search(
        r"Name\s+NCalls\s+Excl\.\s+Min\s+Excl\.\s+Avg\s+Excl\.\s+Max\s+Max\s+%\n-+([\s\S]+?)-+",
        stdout
    )
    if excl_section_match:
        excl_text = excl_section_match.group(1)
        excl_entries = {}
        for line in excl_text.splitlines():
            line = line.strip()
            if not line:
                continue
            match = re.match(r"^(.+?)\s+(\d+)\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)%", line)
            if match:
                name = match.group(1).strip()
                excl_entries[name] = {
                    "calls": int(match.group(2)),
                    "min": float(match.group(3)),
                    "avg": float(match.group(4)),
                    "max": float(match.group(5)),
                    "percent": float(match.group(6))
                }
        profiler_data["exclusive"] = excl_entries

    incl_section_match = re.search(
        r"Name\s+NCalls\s+Incl\.\s+Min\s+Incl\.\s+Avg\s+Incl\.\s+Max\s+Max\s+%\n-+([\s\S]+?)-+",
        stdout
    )
    if incl_section_match:
        incl_text = incl_section_match.group(1)
        incl_entries = {}
        for line in incl_text.splitlines():
            line = line.strip()
            if not line:
                continue
            match = re.match(r"^(.+?)\s+(\d+)\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)\s+([\d.e+-]+)%", line)
            if match:
                name = match.group(1).strip()
                incl_entries[name] = {
                    "calls": int(match.group(2)),
                    "min": float(match.group(3)),
                    "avg": float(match.group(4)),
                    "max": float(match.group(5)),
                    "percent": float(match.group(6))
                }
        profiler_data["inclusive"] = incl_entries

    return profiler_data

def run_benchmark_suite(n_proc, run_dir, param_file, solver_kind="fullpic",
                        count=3):
    """Run `count` benchmark runs on `n_proc` MPI processes and collect timing
    stats.  `solver_kind` selects the field-solver and mover TinyProfiler
    region lists (FULLPIC_SOLVERS / HYBRID_SOLVERS; _REQUIRED_MOVER).
    Returns (stats, 0) or (None, code) on failure."""
    solver_names = HYBRID_SOLVERS if solver_kind == "hybrid" else FULLPIC_SOLVERS

    print(f"Running benchmark in {run_dir} with {n_proc} MPI process(es) "
          f"({count} runs, solver={solver_kind})...")
    runs = []

    particles = count_particles_from_param(param_file)
    cycles = cycles_from_param(param_file)
    if particles is None or cycles is None:
        print(f"Error: could not derive particle/cycle counts from {param_file}.")
        return None, 1

    for i in range(count):
        print(f"  Run {i+1}/{count}...")
        prepare_run_dir(run_dir)

        shutil.copy(param_file, os.path.join(run_dir, "PARAM.in"))

        cmd = ["mpirun", "-n", str(n_proc), "./FLEKS.exe"]

        start_time = time.perf_counter()
        result = subprocess.run(cmd, cwd=run_dir, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, text=True)
        end_time = time.perf_counter()

        duration = end_time - start_time

        if result.returncode != 0:
            print(f"Error executing benchmark in run {i+1}:")
            print(result.stderr)
            return None, result.returncode

        prof = parse_tiny_profiler(result.stdout)

        # Mover time (exclusive); warn if the required mover region is absent.
        required_mover = _REQUIRED_MOVER[solver_kind]
        mover_excl_time = 0.0
        mover_missing = True
        for name in MOVERS:
            if "exclusive" in prof and name in prof["exclusive"]:
                mover_excl_time = prof["exclusive"][name]["avg"]
                if name == required_mover:
                    mover_missing = False
                    break

        if mover_missing and mover_excl_time == 0.0:
            print(f"  ⚠ WARNING: Required mover region '{required_mover}' "
                  f"NOT FOUND in TinyProfiler exclusive table. "
                  f"Available regions: {sorted(prof.get('exclusive',{}).keys())}")

        # Solver time (inclusive); warn if no solver region was found.
        solver_incl_time = 0.0
        solver_missing = True
        for name in solver_names:
            if "inclusive" in prof and name in prof["inclusive"]:
                solver_incl_time = prof["inclusive"][name]["avg"]
                solver_missing = False
                break

        if solver_missing:
            print(f"  ⚠ WARNING: No solver region in {solver_names} "
                  f"found in TinyProfiler inclusive table. "
                  f"Available inclusive regions: "
                  f"{sorted(prof.get('inclusive',{}).keys())}")

        runs.append({
            "duration": duration,
            "cycles": cycles,
            "particles": particles,
            "total_profile_time": prof.get("total_time", duration),
            "mover_time": mover_excl_time,
            "solver_time": solver_incl_time,
        })

    def median(lst):
        sorted_lst = sorted(lst)
        n = len(sorted_lst)
        if n % 2 == 1:
            return sorted_lst[n//2]
        else:
            return (sorted_lst[n//2 - 1] + sorted_lst[n//2]) / 2.0

    durations = [r["duration"] for r in runs]
    movers = [r["mover_time"] for r in runs]
    solvers = [r["solver_time"] for r in runs]

    stats = {
        "runs": runs,
        "cycles": runs[-1]["cycles"],
        "particles": runs[-1]["particles"],
        "duration_median": median(durations),
        "duration_min": min(durations),
        "mover_median": median(movers),
        "mover_min": min(movers),
        "solver_median": median(solvers),
        "solver_min": min(solvers),
    }

    return stats, 0

def _evaluate_solver(solver_kind, param_file, serial_dir, parallel_dir, count=3):
    """Run the 1- and 2-process benchmark suites for one solver kind and return
    (stats, pass_flags, label)."""
    label = "FULL PIC" if solver_kind == "fullpic" else "HYBRID PIC"

    serial_stats, code = run_benchmark_suite(
        1, serial_dir, param_file, solver_kind=solver_kind, count=count)
    if code != 0 or serial_stats is None:
        print(f"FAIL: {label} serial benchmark runs failed.")
        return None, None, label

    parallel_stats, code = run_benchmark_suite(
        2, parallel_dir, param_file, solver_kind=solver_kind, count=count)
    if code != 0 or parallel_stats is None:
        print(f"FAIL: {label} parallel benchmark runs failed.")
        return None, None, label

    t_serial_median = serial_stats["duration_median"]
    t_parallel_median = parallel_stats["duration_median"]
    p_count = serial_stats["particles"]
    cycles = serial_stats["cycles"]
    total_steps = p_count * cycles

    # μs/part-step = (T * 1e6) / (Particles * Cycles)
    pps_total = (t_serial_median * 1e6) / total_steps
    pps_mover = (serial_stats["mover_median"] * 1e6) / total_steps
    pps_solver = (serial_stats["solver_median"] * 1e6) / total_steps
    speedup = t_serial_median / t_parallel_median

    base = BASELINES[solver_kind]
    passed = {
        "total": pps_total <= base["total_pps"],
        "mover": pps_mover <= base["mover_pps"],
        "solver": pps_solver <= base["solver_pps"],
        "speedup": speedup >= base["speedup"],
    }

    stats = {
        "serial_stats": serial_stats,
        "parallel_stats": parallel_stats,
        "total_steps": total_steps,
        "pps_total": pps_total,
        "pps_mover": pps_mover,
        "pps_solver": pps_solver,
        "speedup": speedup,
    }

    return stats, passed, label


def _status_markdown(flag):
    return "🟢 **PASSED**" if flag else "🔴 **FAILED**"


def _status_console(flag):
    return "PASSED" if flag else "FAILED"


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    os.chdir(script_dir)

    if not os.path.exists("../bin/FLEKS.exe"):
        print("Error: Standalone FLEKS.exe not found. Please compile it first.")
        sys.exit(1)

    print("================================================================================")
    print("                 FLEKS ROBUST PERFORMANCE REGRESSION TEST")
    print("   (full-PIC beam benchmark + hybrid-PIC whistler benchmark)")
    print("================================================================================")

    fullpic_param = os.path.join("performance", "PARAM.in")
    hybrid_param = os.path.join("performance", "PARAM.in.hybrid")
    if not os.path.exists(hybrid_param):
        print(f"Error: Hybrid benchmark config {hybrid_param} not found.")
        sys.exit(1)

    # Benchmark suites: (solver_kind, param_file)
    suites = [
        ("fullpic", fullpic_param),
        ("hybrid", hybrid_param),
    ]

    results = {}
    all_passed = True
    for solver_kind, param_file in suites:
        print("\n" + "#" * 85)
        print(f"#  SOLVER: {'FULL PIC' if solver_kind == 'fullpic' else 'HYBRID PIC'}")
        print("#" * 85)
        stats, passed, label = _evaluate_solver(
            solver_kind, param_file, "run_perf_serial", "run_perf_parallel")
        if stats is None:
            all_passed = False
            continue
        results[solver_kind] = (stats, passed, label)

        base = BASELINES[solver_kind]
        print(f"\nSerial Median Wall-Clock: "
              f"{stats['serial_stats']['duration_median']:.3f} s "
              f"(Rate: {stats['pps_total']:.3f} μs/part-step)")
        print(f"Serial Median Mover:      "
              f"{stats['serial_stats']['mover_median']:.3f} s "
              f"(Rate: {stats['pps_mover']:.3f} μs/part-step)")
        print(f"Serial Median Solver:     "
              f"{stats['serial_stats']['solver_median']:.3f} s "
              f"(Rate: {stats['pps_solver']:.3f} μs/part-step)")
        print(f"Parallel Median Wall-Clock: "
              f"{stats['parallel_stats']['duration_median']:.3f} s "
              f"(Rate: "
              f"{(stats['parallel_stats']['duration_median']*1e6)/stats['total_steps']:.3f} "
              f"μs/part-step)")
        print(f"Median Speedup (2 Cores):   {stats['speedup']:.2f}x")

        # Console comparison profile for this solver
        print("\n" + "=" * 85)
        print(" " * 26 + f"PERFORMANCE PROFILE ({label})")
        print("=" * 85)
        print(f" {'Metric / Component':<30} | {'Measured':<18} | "
              f"{'Baseline':<18} | {'Status':<8}")
        print("-" * 85)
        rows = [
            ("Total Runtime Rate (PPS)", stats["pps_total"], base["total_pps"],
             "μs", passed["total"]),
            ("Particle Mover Rate (PPS)", stats["pps_mover"], base["mover_pps"],
             "μs", passed["mover"]),
            ("Field Solver Rate (PPS)", stats["pps_solver"], base["solver_pps"],
             "μs", passed["solver"]),
            ("Parallel Speedup (2 Cores)", stats["speedup"], base["speedup"],
             "x", passed["speedup"]),
        ]
        for name, val, base_val, unit, flag in rows:
            st = _status_console(flag)
            if sys.stdout.isatty():
                st = (f"\033[92;1m{st}\033[0m" if flag
                      else f"\033[91;1m{st}\033[0m")
            if unit == "x":
                print(f" {name:<30} | {val:<16.2f} {unit}  | "
                      f">={base_val:<14.2f} {unit} | {st}")
            else:
                print(f" {name:<30} | {val:<13.3f} {unit}/pt | "
                      f"<={base_val:<14.1f} {unit} | {st}")
        print("=" * 85)

        all_passed = all_passed and all(passed.values())

    # --- Generate markdown report ---
    summary_path = "performance_summary.md"
    try:
        with open(summary_path, "w") as f:
            f.write("### ⚡ Standalone FLEKS Performance Report\n\n")
            f.write("A robust statistical check was executed on the runner to "
                    "filter out virtualization noise (3 runs per benchmark):\n\n")

            for solver_kind in ("fullpic", "hybrid"):
                if solver_kind not in results:
                    continue
                stats, passed, label = results[solver_kind]
                base = BASELINES[solver_kind]
                f.write(f"\n#### {label}\n\n")
                f.write("| Performance Metric | Measured (Median) | Target "
                        "Baseline | Status |\n")
                f.write("| :--- | :--- | :--- | :--- |\n")
                f.write(f"| Total Wall-Clock Rate | {stats['pps_total']:.3f} "
                        f"μs/pt | <= {base['total_pps']:.1f} μs/pt | "
                        f"{_status_markdown(passed['total'])} |\n")
                f.write(f"| Particle Mover Rate | {stats['pps_mover']:.3f} "
                        f"μs/pt | <= {base['mover_pps']:.1f} μs/pt | "
                        f"{_status_markdown(passed['mover'])} |\n")
                f.write(f"| Field Solver Rate | {stats['pps_solver']:.3f} "
                        f"μs/pt | <= {base['solver_pps']:.1f} μs/pt | "
                        f"{_status_markdown(passed['solver'])} |\n")
                f.write(f"| Parallel Speedup (2 Cores) | {stats['speedup']:.2f}"
                        f"x | >= {base['speedup']:.2f}x | "
                        f"{_status_markdown(passed['speedup'])} |\n")
                f.write(f"*Macroparticles: {stats['serial_stats']['particles']}, "
                        f"cycles: {stats['serial_stats']['cycles']} "
                        f"(total steps: {stats['total_steps']}).*\n")
                f.write("\n**Detailed Runs (Wall-Clock Runtime)**\n")
                f.write("| Process Count | Run 1 | Run 2 | Run 3 | Median | "
                        "Minimum |\n")
                f.write("| :--- | :--- | :--- | :--- | :--- | :--- |\n")
                s_runs = [f"{r['duration']:.3f}s"
                          for r in stats["serial_stats"]["runs"]]
                p_runs = [f"{r['duration']:.3f}s"
                          for r in stats["parallel_stats"]["runs"]]
                f.write(f"| 1 MPI Process (Serial) | {s_runs[0]} | {s_runs[1]} "
                        f"| {s_runs[2]} | "
                        f"{stats['serial_stats']['duration_median']:.3f}s | "
                        f"{stats['serial_stats']['duration_min']:.3f}s |\n")
                f.write(f"| 2 MPI Processes (Parallel) | {p_runs[0]} | "
                        f"{p_runs[1]} | {p_runs[2]} | "
                        f"{stats['parallel_stats']['duration_median']:.3f}s | "
                        f"{stats['parallel_stats']['duration_min']:.3f}s |\n")
            f.write("\n*Note: Benchmark ran on virtualized runner.*\n")
    except Exception as e:
        print(f"Warning: Could not write performance_summary.md: {e}")

    # Clean up temporary run directories
    for d in ("run_perf_serial", "run_perf_parallel"):
        if os.path.exists(d):
            shutil.rmtree(d)
            print(f"Cleaned up {d}")

    if all_passed:
        if sys.stdout.isatty():
            print("\033[92;1m\nALL PERFORMANCE TESTS PASSED SUCCESSFULLY!\033[0m\n")
        else:
            print("\nALL PERFORMANCE TESTS PASSED SUCCESSFULLY!\n")
        sys.exit(0)
    else:
        if sys.stdout.isatty():
            print("\033[91;1m\nPERFORMANCE REGRESSION DETECTED.\033[0m\n")
        else:
            print("\nPERFORMANCE REGRESSION DETECTED.\n")
        sys.exit(1)

if __name__ == "__main__":
    main()
