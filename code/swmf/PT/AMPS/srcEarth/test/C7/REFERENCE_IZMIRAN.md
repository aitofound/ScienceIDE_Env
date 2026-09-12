# IZMIRAN reference provenance

The file `reference_C7_izmiran_t96.csv` contains 28 manually transcribed results
from the IZMIRAN Cutoff2050 online calculator. Each row identifies its source
screenshot in the `notes` column. The corresponding images are stored in
`reference_evidence/IZMIRAN/`.

Common calculator settings:

```text
IGRF+T96; date 2012-03-08; altitude 500 km; SWDP 2 nPa; Dst -50 nT;
IMF By 0 nT; IMF Bz -5 nT; vertical 0 deg; azimuth 0 deg;
rigidity 0.01-20 GV in 0.1-GV increments; maximum flight time 20 s.
```

The date field did not show a time. The strict table uses midnight UTC and
records this assumption explicitly. The solar-wind velocity, density, IMF Bx,
and T05 W1-W6 columns are retained to satisfy the general C7 schema; they were
not independent controls on the displayed IZMIRAN T96 form.

The screenshots `image(160).png`, `image(161).png`, and `image(162).png` are
repeated captures of the same latitude 45 deg, longitude 90 deg result and are
retained as confirmation rather than separate reference rows.
