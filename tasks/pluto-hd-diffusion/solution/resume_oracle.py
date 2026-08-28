#!/usr/bin/env python3
"""Fail-closed continuation importer for the retained C01-C20 CPU campaign.

The importer only copies artifacts after checking the exact source/configuration
contract, solver completion, native files, and output contract.  It never
creates a completion marker from a directory name or stale status alone.
"""
from __future__ import annotations
import argparse, hashlib, json, shutil, subprocess, sys
from pathlib import Path

LEGACY = [
 'c01-hd-sod-08','c02-hd-riemann-2d-03','c03-hd-isentropic-vortex-03','c04-hd-disk-planet-03',
 'c05-hd-viscosity-flow-past-cylinder-02','c06-hd-sedov-01','c07-hd-jet-01','c08-hd-underexpanded-jet-01',
 'c09-hd-underexpanded-jet-02','c10-hd-sedov-04','c11-hd-blast-02','c12-hd-riemann-2d-05',
 'c13-hd-sedov-02','c14-hd-sedov-03','c15-hd-stellar-wind-04','c16-hd-stellar-wind-06',
 'c17-hd-disk-planet-08-fargo','c18-hd-viscosity-taylor-couette-05',
 'c19-hd-viscosity-flow-past-cylinder-01','c20-hd-wind-tunnel-02']
COMPILER='GCC C17; PARALLEL=FALSE; USE_HDF5=FALSE; USE_PNG=FALSE; -ffp-contract=off'

def source_hash(path: Path) -> str:
 d=hashlib.sha256()
 for p in sorted(path.rglob('*')):
  if p.is_symlink(): raise ValueError('source contains symlink: '+str(p))
  if p.is_file(): d.update(p.relative_to(path).as_posix().encode()); d.update(b'\0'); d.update(hashlib.sha256(p.read_bytes()).digest())
 return d.hexdigest()

def digest(p: Path) -> str:
 return hashlib.sha256(p.read_bytes()).hexdigest()

def regular(p: Path) -> bool:
 return p.is_file() and not p.is_symlink()

def validate(root: Path, source: Path) -> dict[str, object]:
 if source.is_symlink() or not source.is_dir(): raise ValueError('resume root is not a real directory')
 oldm=source/'oracle_manifest.json'
 if not regular(oldm): raise ValueError('resume manifest missing or non-regular')
 old=json.loads(oldm.read_text(encoding='utf-8'))
 if old.get('schema')!=1 or old.get('compiler')!=COMPILER or old.get('status')!='complete': raise ValueError('resume manifest is not complete under current compiler contract')
 if old.get('check_ids')!=LEGACY: raise ValueError('resume manifest is not exactly C01-C20')
 if old.get('source_hash')!=source_hash(root/'code'/'pluto'): raise ValueError('resume source digest mismatch')
 entries={x.get('check'):x for x in old.get('checks',[]) if isinstance(x,dict)}
 if sorted(entries)!=sorted(LEGACY) or any(entries[x].get('status')!='complete' for x in LEGACY): raise ValueError('resume manifest has missing/noncomplete row')
 for name in LEGACY:
  row=source/name
  if row.is_symlink() or not row.is_dir(): raise ValueError(f'{name}: missing real artifact directory')
  done=row/'solver_completion.txt'
  if not regular(done) or 'solver_exit_status=0' not in done.read_text(encoding='utf-8'): raise ValueError(f'{name}: solver completion is not zero')
  contract=row/'output_contract.json'
  if not regular(contract): raise ValueError(f'{name}: output contract missing')
  try: json.loads(contract.read_text(encoding='utf-8'))
  except Exception as exc: raise ValueError(f'{name}: output contract JSON invalid: {exc}')
  if not regular(row/'dbl.out') or not list(row.glob('data.*.dbl')): raise ValueError(f'{name}: native PLUTO outputs missing')
  dm=row/'deck_manifest.json'
  if not regular(dm): raise ValueError(f'{name}: deck manifest missing')
  deck=json.loads(dm.read_text(encoding='utf-8'))
  if deck.get('check') != f'C{LEGACY.index(name)+1:02d}': raise ValueError(f'{name}: deck check identity mismatch')
  files=deck.get('source_files')
  if not isinstance(files,dict): raise ValueError(f'{name}: deck source digest map missing')
  current=root/'tests'/'checks'/name/'build'/'deck'
  for fn,h in files.items():
   cur=current/fn
   # deck manifests may include deterministic generated runtime inputs (for
   # example C11 grid0.out/rho0.dbl) that are preserved evidence, not source
   # configuration files.  Compare every matching checked-in deck file; reject
   # missing/mismatched files that the current deck claims as configuration.
   if cur.exists() and (not regular(cur) or digest(cur)!=h):
    raise ValueError(f'{name}: current config/source digest mismatch for {fn}')
  for fn in ('init.c','definitions.h','pluto.ini'):
   cur=current/fn
   if not regular(cur) or fn not in files or digest(cur)!=files[fn]: raise ValueError(f'{name}: required current config digest missing for {fn}')
  # Re-run the checked-in native parser; this rejects stale/corrupt payloads.
  proc=subprocess.run([sys.executable,str(root/'solution'/'output_contract.py'),str(row)],capture_output=True,text=True)
  if proc.returncode!=0: raise ValueError(f'{name}: native output contract revalidation failed: {proc.stderr.strip()}')
 return old

def copy_rows(source: Path, scratch: Path) -> None:
 for name in LEGACY:
  dst=scratch/name
  if dst.exists(): raise ValueError(f'refusing to overwrite scratch row: {dst}')
  shutil.copytree(source/name,dst,symlinks=False)

def main(argv=None):
 ap=argparse.ArgumentParser(); ap.add_argument('--root',required=True); ap.add_argument('--source',required=True); ap.add_argument('--scratch',required=True); ap.add_argument('--manifest',required=True)
 ns=ap.parse_args(argv); root=Path(ns.root).resolve(); source=Path(ns.source).resolve(); scratch=Path(ns.scratch).resolve(); manifest=Path(ns.manifest).resolve()
 old=validate(root,source); copy_rows(source,scratch)
 doc=json.loads(manifest.read_text(encoding='utf-8')); doc['resume_source']=str(source); doc['resume_validation']='source-config-output-contract-digests-and-native-parser'; doc['checks']=[{'check':n,'image':'retained-cpu-campaign','build_exit':0,'run_exit':0,'copy_exit':0,'contract_exit':0,'status':'complete','resumed':True} for n in LEGACY]; doc['complete_count']=len(LEGACY); doc['status']='incomplete'; manifest.write_text(json.dumps(doc,sort_keys=True,indent=2)+'\n')
 print('validated and imported retained C01-C20:',source)
if __name__=='__main__': main()
