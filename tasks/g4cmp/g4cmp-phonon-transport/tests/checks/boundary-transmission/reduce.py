#!/usr/bin/env python3
"""Reduce the validation step file to the boundary-transmission invariants (stdlib + numpy only).
Each line of the step file has 18 fields: run event track particle preX preY preZ preT preE preKE
postX postY postZ postT postE postKE nReflections process (mm, ns, eV). Following
validation/AnalysisTools/ValidationAnalysis.cc (ValidationAnalysis_BoundaryTransmission): a step whose
post-step point lies on the Si/Ge interface plane z = -0.5 mm inside r < 9.9 mm is surface-incident;
it is transmitted when the next step of the same track continues in the same z direction and reflected
when it turns back. The file is streamed line by line (it is hundreds of MB) and not copied.
Writes summary.txt: one comment line naming the columns, one row of values."""
import sys
import numpy as np
src, dst = sys.argv[1], sys.argv[2]
Z_IF, R_MAX = -0.5, 9.9
prev = None  # (track key, preZ, postZ, energy)
incident = reflected = transmitted = from_above = 0; e_inc = []
with open(src) as f:
    for line in f:
        t = line.split()
        if len(t) < 18: continue
        key = (t[0], t[1], t[2]); pre_z, post_z = float(t[6]), float(t[12])
        if prev is not None and prev[0] == key and prev[4]:
            d1 = prev[2] - prev[1]; d2 = post_z - pre_z
            if d1 * d2 > 0: transmitted += 1
            else: reflected += 1
            prev = None
        x1, y1 = float(t[10]), float(t[11])
        on_if = abs(post_z - Z_IF) < 1e-6 and (x1 * x1 + y1 * y1) ** 0.5 < R_MAX
        if on_if:
            incident += 1; e_inc.append(float(t[14]))
            if pre_z > Z_IF: from_above += 1
        prev = (key, pre_z, post_z, float(t[14]), on_if)
classified = reflected + transmitted
cols = ["n_incident", "frac_transmitted", "frac_from_above", "mean_incident_energy_eV", "n_classified"]
vals = [incident, transmitted / classified if classified else 0.0, from_above / incident if incident else 0.0,
        float(np.mean(e_inc)) if e_inc else 0.0, classified]
with open(dst, "w") as g:
    g.write("# " + " ".join(cols) + "\n"); g.write(" ".join(repr(float(v)) for v in vals) + "\n")
