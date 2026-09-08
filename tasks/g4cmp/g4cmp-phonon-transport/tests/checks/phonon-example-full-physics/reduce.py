#!/usr/bin/env python3
"""Reduce the electrode hits CSV of examples/phonon to the graded invariants (stdlib + numpy only).
Columns of phonon_hits.txt: Run ID, Event ID, Track ID, Particle Name, Start Energy [eV], Start X/Y/Z [m],
Start Time [ns], Energy Deposited [eV], Track Weight, End X/Y/Z [m], Final Time [ns]. The file is opened in
append mode by the example and carries one header line per run; run.sh starts from an empty directory.
Writes summary.txt: one comment line naming the columns, one row of values."""
import csv, sys
import numpy as np
src, dst = sys.argv[1], sys.argv[2]
rows = [r for r in csv.reader(open(src)) if r and not r[0].startswith("Run")]
name = np.array([r[3] for r in rows]); edep = np.array([float(r[9]) for r in rows])
ex, ey, ez = (np.array([float(r[i]) for r in rows]) for i in (11, 12, 13)); tf = np.array([float(r[14]) for r in rows])
n = len(rows); etot = edep.sum()
def frac(mask): return float(edep[mask].sum() / etot) if etot > 0 else 0.0
cols = ["n_hits", "e_absorbed_eV", "frac_e_top_cap", "frac_e_TS", "frac_e_TF", "frac_e_L", "mean_final_time_ns", "rms_final_time_ns", "mean_end_radius_m", "mean_hit_energy_eV"]
vals = [n, etot, frac(ez > 0), frac(name == "phononTS"), frac(name == "phononTF"), frac(name == "phononL"),
        tf.mean() if n else 0.0, tf.std() if n else 0.0, np.sqrt(ex**2 + ey**2).mean() if n else 0.0, edep.mean() if n else 0.0]
with open(dst, "w") as f:
    f.write("# " + " ".join(cols) + "\n"); f.write(" ".join(repr(float(v)) for v in vals) + "\n")
