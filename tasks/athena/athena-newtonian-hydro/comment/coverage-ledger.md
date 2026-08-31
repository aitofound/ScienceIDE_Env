# Official coverage ledger

| Direct ID | Official script | Official deck | Native observable |
|---|---|---|---|
| AH-CARBUNCLE | `tst/regression/scripts/tests/hydro/hydro_carbuncle.py` | `inputs/hydro/athinput.quirk` | `carbuncle-diff.dat` |
| AH-LINWAVE | `tst/regression/scripts/tests/hydro/hydro_linwave.py` | `inputs/hydro/athinput.linear_wave3d` | `linearwave-errors.dat` |
| AH-SOD | `tst/regression/scripts/tests/hydro/sod_shock.py` | `inputs/hydro/athinput.sod` | `shock-errors.dat` |
| AH-LINWAVE-2D | `tst/regression/scripts/tests/hydro4/hydro_linwave_2d.py` | `inputs/hydro/athinput.linear_wave2d` | `linearwave-errors.dat` |
| AH-LINWAVE-3D | `tst/regression/scripts/tests/hydro4/hydro_linwave_3d.py` | `inputs/hydro/athinput.linear_wave3d` | `linearwave-errors.dat` |

Exactly five direct scripts are run once each per solve. Their full internal
solver/axis/resolution loops remain inside the corresponding upstream script
and are not promoted to separate checks. The twenty case-split legacy payload
directories remain content-preserved under
`tests/metadata/superseded-nonofficial/`.
