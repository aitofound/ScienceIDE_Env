# Official-check ownership decision

The five physical direct folders under `tests/checks/` each correspond to one
distinct official upstream regression script and contribute equally. The pinned
set is `hydro/hydro_carbuncle.py`, `hydro/hydro_linwave.py`,
`hydro/sod_shock.py`, `hydro4/hydro_linwave_2d.py`, and
`hydro4/hydro_linwave_3d.py`. Internal solver, flux, wave, axis, and resolution
loops in those scripts are not separate checks.

The twenty case-split legacy payload directories (seventeen former direct
folders and three nested payloads) are retained byte-for-byte under
`tests/metadata/superseded-nonofficial/` and are not discovered or scored.
