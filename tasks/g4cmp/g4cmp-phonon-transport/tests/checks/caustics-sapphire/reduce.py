#!/usr/bin/env python3
"""Reduce the caustics hits file(s) to the graded invariants (stdlib + numpy only).
examples/caustics writes tab-separated rows (event, track, particle name, x, y, z in metres) for every
phonon absorbed in the bolometer, one file per worker thread (phonon_hits_G4WT0.txt) or phonon_hits.txt.
Writes summary.txt: one comment line naming the columns, one row of values."""
import glob, sys
import numpy as np
srcdir, dst = sys.argv[1], sys.argv[2]
names, xs, ys = [], [], []
for p in sorted(glob.glob(srcdir + "/phonon_hits*.txt")):
    for line in open(p):
        t = line.split()
        if len(t) < 6: continue
        names.append(t[2]); xs.append(float(t[3])); ys.append(float(t[4]))
name = np.array(names); x = np.array(xs) * 1e3; y = np.array(ys) * 1e3      # mm
n = len(name); r = np.sqrt(x**2 + y**2); phi = np.arctan2(y, x)
def f(mask): return float(mask.sum() / n) if n else 0.0
cols = ["n_absorbed", "frac_TF", "mean_r_mm", "rms_r_mm", "frac_r_lt_0p5mm", "frac_r_0p5_1p0mm", "frac_r_1p0_1p5mm", "cos4phi_mean", "cos4phi_r_gt_1mm"]
vals = [n, f(name == "phononTF"), r.mean() if n else 0.0, np.sqrt((r**2).mean()) if n else 0.0,
        f(r < 0.5), f((r >= 0.5) & (r < 1.0)), f((r >= 1.0) & (r < 1.5)),
        float(np.cos(4 * phi).mean()) if n else 0.0, float(np.cos(4 * phi[r > 1.0]).mean()) if (r > 1.0).any() else 0.0]
with open(dst, "w") as g:
    g.write("# " + " ".join(cols) + "\n"); g.write(" ".join(repr(float(v)) for v in vals) + "\n")
