#!/usr/bin/env python3
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
d=json.loads((root/'coverage_manifest.json').read_text())
expected={
    'hydro/hydro_carbuncle.py',
    'hydro/hydro_linwave.py',
    'hydro/sod_shock.py',
    'hydro4/hydro_linwave_2d.py',
    'hydro4/hydro_linwave_3d.py',
}
assert d['direct_check_directory_count']==len(d['checks'])==5
assert {c['official_test'] for c in d['checks']}==expected
assert all((root/'checks'/c['folder']/'check.json').is_file() for c in d['checks'])
print('official script-level catalog self-check passed: 5/5')
