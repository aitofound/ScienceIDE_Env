#!/usr/bin/env python3
"""Measure the temporal order of accuracy of the hybrid-PIC solver.

Uses tests/iaw (ion acoustic wave): a single-mode density perturbation that
oscillates at the acoustic frequency, so the ion moments vary in time and the
time-interpolation (hstep) of the generalized Ohm's law is exercised. The grid
is kept fixed while dt is halved; the measured acoustic frequency omega(dt) is
fit to a Richardson series  omega(dt) = omega* + C dt^p, and the effective
temporal order p is reported (second order -> p ~ 2).

Usage:
    python3 tests/order_convergence.py            # run the dt refinement sequence
    python3 tests/order_convergence.py --dt 0.02  # single dt (diagnostics)
    python3 tests/order_convergence.py --nproc 2  # run under mpirun -n 2
"""
import argparse
import glob
import math
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUN_DIR = os.path.join(REPO, "run_order")
TEST_DIR = os.path.join(REPO, "tests", "iaw")
EXE = os.path.join(REPO, "bin", "FLEKS.exe")


def _rewrite_param(text, block, fields):
    """Rewrite numeric values within a named #BLOCK. `fields` maps keyword ->
    new value. Returns patched text."""
    out = []
    in_block = False
    replaced = set()
    for line in text.splitlines():
        s = line.strip()
        if not s:
            out.append(line)
            continue
        tok = s.split()[0]
        if tok.startswith("#"):
            in_block = (tok == block)
            out.append(line)
            continue
        if in_block:
            for kw, val in fields.items():
                if kw in s and kw not in replaced:
                    indent = line[: len(line) - len(line.lstrip())]
                    out.append(f"{indent}{val}                     {kw}")
                    replaced.add(kw)
                    break
            else:
                out.append(line)
            continue
        out.append(line)
    missing = set(fields) - replaced
    if missing:
        raise RuntimeError(f"Could not patch {missing} in {block}")
    return "\n".join(out) + "\n"


def run_case(dt, nsub, nprocs=1):
    if os.path.isdir(RUN_DIR):
        shutil.rmtree(RUN_DIR)
    os.makedirs(os.path.join(RUN_DIR, "PC", "plots"), exist_ok=True)
    os.makedirs(os.path.join(RUN_DIR, "PC", "restartOUT"), exist_ok=True)

    with open(os.path.join(TEST_DIR, "PARAM.in")) as f:
        text = f.read()
    # Time step + subcycling (smaller dt -> weaker Hall CFL, fewer sub-steps).
    text = _rewrite_param(text, "#TIMESTEPPING", {"dt": dt})
    text = _rewrite_param(text, "#BSUBCYCLE", {"nBSubcycle": nsub})
    # Disable the EMA B-averaging: it is a time filter that would mask the
    # pure temporal convergence order.
    text = _rewrite_param(text, "#AVGFIELDB", {"useAvgFieldB": "F"})
    # Save plots more often so the acoustic phase is well resolved.
    text = _rewrite_param(text, "#SAVEPLOT", {"dn": "5"})
    # High PPC: push the PIC shot-noise floor well below the temporal error so
    # the Richardson fit is not noise-limited.
    text = _rewrite_param(text, "#PARTICLES",
                          {"nParticle per cell in X": 20000})
    with open(os.path.join(RUN_DIR, "PARAM.in"), "w") as f:
        f.write(text)

    for link, target in (("FLEKS.exe", EXE),
                         ("PostProc.pl", os.path.join(REPO, "share", "Scripts", "PostProc.pl")),
                         ("PC/PostIDL.exe", os.path.join(REPO, "bin", "PostIDL.exe")),
                         ("PC/pIDL", os.path.join(REPO, "share", "Scripts", "pIDL"))):
        p = os.path.join(RUN_DIR, link)
        if not os.path.lexists(p):
            os.symlink(target, p)

    cmd = (["./FLEKS.exe"] if nprocs <= 1
           else ["mpirun", "-n", str(nprocs), "./FLEKS.exe"])
    r = subprocess.run(cmd, cwd=RUN_DIR, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True)
    if r.returncode != 0:
        sys.stderr.write("FLEKS failed (dt=%s):\n%s\n%s\n" % (dt, r.stdout, r.stderr))
        return None
    pp = subprocess.run(["./PostProc.pl", "-v"], cwd=RUN_DIR, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, text=True)
    if pp.returncode != 0:
        sys.stderr.write("PostProc.pl failed (dt=%s):\n%s\n%s\n" % (dt, pp.stdout, pp.stderr))
        return None
    return measure_acoustic_freq()


def _load_rho(path):
    """Return (x_list, rho_list) for the ion density column of an .out file."""
    with open(path, "r", encoding="latin-1") as f:
        lines = f.readlines()
    if len(lines) < 6:
        return None
    var_names = lines[4].split()
    vidx = {v.upper(): i for i, v in enumerate(var_names)}
    ridx = None
    for target in ("RHOS0", "RHOS1", "RHO"):
        if target in vidx:
            ridx = vidx[target]
            break
    if ridx is None:
        return None
    xs, rhos = [], []
    for line in lines[5:]:
        cols = line.split()
        if len(cols) <= ridx:
            continue
        try:
            xs.append(float(cols[0]))
            rhos.append(float(cols[ridx]))
        except (ValueError, IndexError):
            continue
    return (xs, rhos) if xs else None


def _frame_time(path):
    try:
        with open(path, "r", encoding="latin-1") as f:
            f.readline()
            nxt = f.readline()
        return float(nxt.split()[1])
    except (OSError, IndexError, ValueError):
        return None


def measure_acoustic_freq():
    """Track the phase of the n=1 density mode vs time; return its angular
    frequency in code units."""
    import cmath
    plots = os.path.join(RUN_DIR, "PC", "plots")
    out_files = sorted(glob.glob(os.path.join(plots, "*.out")))
    if len(out_files) < 5:
        sys.stderr.write("  need >=5 .out frames, got %d\n" % len(out_files))
        return None

    kdom = 1  # IonAcousticWave seeds waveMode = 1.
    samples = []
    for f in out_files:
        data = _load_rho(f)
        if data is None:
            continue
        xs, rhos = data
        n = len(xs)
        if n < 4:
            continue
        t = _frame_time(f)
        if t is None:
            continue
        # Phase of the k=1 Fourier mode of the (mean-subtracted) density.
        mean = sum(rhos) / n
        c = complex(0.0, 0.0)
        for i in range(n):
            c += (rhos[i] - mean) * cmath.exp(-2j * math.pi * kdom * i / n)
        C1 = c / n
        if abs(C1) < 1e-12:
            continue
        samples.append((t, cmath.phase(C1)))
    if len(samples) < 5:
        return None
    samples.sort(key=lambda s: s[0])
    unwrapped = [samples[0][1]]
    for i in range(1, len(samples)):
        dphi = samples[i][1] - samples[i - 1][1]
        dphi -= 2.0 * math.pi * round(dphi / (2.0 * math.pi))
        unwrapped.append(unwrapped[-1] + dphi)

    ts = [s[0] for s in samples]
    nt = len(ts)
    mean_t = sum(ts) / nt
    mean_p = sum(unwrapped) / nt
    cov = sum((ts[i] - mean_t) * (unwrapped[i] - mean_p) for i in range(nt))
    var_t = sum((ts[i] - mean_t) ** 2 for i in range(nt))
    if var_t <= 0:
        return None
    omega_si = cov / var_t  # SI rad/s

    # Convert to code units: tNorm = lNormSI / uNormSI.
    try:
        with open(os.path.join(TEST_DIR, "PARAM.in")) as f:
            ptext = f.read()
        norms = _rewrite_param  # noqa -- keep import simple
    except Exception:
        ptext = ""
    import re as _re
    m = _re.search(r"#NORMALIZATION(.*?)#", ptext, _re.S)
    toks = []
    if m:
        for ln in m.group(1).splitlines():
            s = ln.strip()
            if s and s[0].isdigit() or (s and s[0] == "-"):
                try:
                    toks.append(float(s.split()[0]))
                except ValueError:
                    pass
    tNorm = (toks[0] / toks[1]) if len(toks) >= 2 and toks[1] > 0 else 2.0
    return {"omega_si": omega_si, "omega_code": omega_si * tNorm}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dt", type=float, default=None)
    ap.add_argument("--nproc", type=int, default=1)
    args = ap.parse_args()

    if args.dt is not None:
        # For a single dt, scale the subcycle count to keep the absolute
        # sub-step size dt_sub = dt/nsub at the verified-stable minimum
        # (nBSubcycle = 2 at dt = 0.02, i.e. dt_sub ~ 0.01).
        nsub = max(1, round(2 * 0.02 / args.dt))
        res = run_case(args.dt, nsub, args.nproc)
        if res is None:
            sys.exit("no frequency measured")
        print("dt=%-8s omega_code = %.6f" % (args.dt, res["omega_code"]))
        return

    # dt refinement sequence (halving), subcycling scaled to keep the absolute
    # sub-step size (Hall CFL) at the verified-stable minimum for the IAW test
    # (nBSubcycle = 2 at dt = 0.02; see tests/iaw/PARAM.in). Grid is fixed
    # (64 cells) so the temporal order is isolated.
    base_dt = 0.02
    base_nsub = 2
    dts = [base_dt / 2**r for r in range(4)]
    omegas = []
    print("%-10s %-12s %-16s" % ("dt", "nBSubcycle", "omega_code"))
    for dt in dts:
        nsub = max(1, round(base_nsub * base_dt / dt))
        res = run_case(dt, nsub, args.nproc)
        if res is None:
            print("%-10.5f %-12d (failed)" % (dt, nsub))
            continue
        print("%-10.5f %-12d %-16.6f" % (dt, nsub, res["omega_code"]))
        omegas.append((dt, res["omega_code"]))

    if len(omegas) >= 3:
        # Richardson: with omega(dt) = omega* + C dt^p, the ratio
        #   r_i = (w_{i+1}-w_i)/(w_i - w_{i-1}) -> 2^{-p}  as dt -> 0.
        diffs = [omegas[i + 1][1] - omegas[i][1] for i in range(len(omegas) - 1)]
        print("\nomega differences:", ["%.3e" % d for d in diffs])
        orders = []
        for i in range(len(diffs) - 1):
            if abs(diffs[i]) < 1e-12:
                continue
            ratio = diffs[i + 1] / diffs[i]
            p = -math.log(abs(ratio)) / math.log(2.0)
            orders.append(p)
            print("  Richardson ratio r=%.3f  ->  effective order p=%.2f" % (ratio, p))
        if orders:
            p = sum(orders) / len(orders)
            print("\nMean effective order p = %.2f  (second order -> p ~ 2)" % p)
            if p >= 1.7:
                print("RESULT: CONFIRMED second-order (p = %.2f)" % p)
            else:
                print("RESULT: order %.2f < 1.7, NOT second-order" % p)
        else:
            print("\nCould not estimate the order (differences too small).")
    else:
        print("\nInsufficient successful runs to fit the order.")


if __name__ == "__main__":
    main()
