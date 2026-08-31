"""Canonical distinct-official-script catalog projection."""
from pathlib import Path
from official import load_manifest

EXPECTED = {
    'hydro/hydro_carbuncle.py',
    'hydro/hydro_linwave.py',
    'hydro/sod_shock.py',
    'hydro4/hydro_linwave_2d.py',
    'hydro4/hydro_linwave_3d.py',
}

def load(root:Path): return load_manifest(root)
def validate_projections(root:Path,catalog:dict):
 checks=catalog.get('checks',[])
 if catalog.get('direct_check_directory_count')!=5 or len(checks)!=5: raise ValueError('catalog must contain exactly five checks')
 scripts={c.get('official_test') for c in checks}
 if scripts!=EXPECTED: raise ValueError('catalog must contain exactly the pinned five official scripts')
 folders={c.get('folder') for c in checks}
 if len(folders)!=5 or any(not (root/'tests'/'checks'/f/'check.json').is_file() for f in folders): raise ValueError('catalog folders do not project to direct checks')
 return True
