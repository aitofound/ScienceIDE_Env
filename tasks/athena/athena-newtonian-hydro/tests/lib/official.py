#!/usr/bin/env python3
"""Independent acceptance for the distinct pinned Athena++ hydro scripts."""
from __future__ import annotations
import hashlib, json, math
from pathlib import Path
SCHEMA='athena-newtonian-hydro-official-result/v2'
MANIFEST_SCHEMA='athena-newtonian-hydro-official-regressions/v2'
SOURCE_COMMIT='823614c90b594472747a0ac2a699e4a454f300d2'
SOURCE_TREE='857c56fdca02ea53cf3839736791e0267a9e0a31460fa4d589023031888dafad'
RUNNER='tst/regression/run_tests.py'

# These values are copied from the pinned upstream hydro4 scripts.  They are
# script-level acceptance contracts; the internal solver loops remain one
# direct check and are deliberately not promoted to separate checks.
_H2_SOLVERS=(('vl2','2c'),('vl2','3'),('rk2','3c'),('rk3','3f'),('rk3','4'),('rk4','4c'),('ssprk5_4','4'))
_H2_ERROR_TOLS=(
    ((1.4e-7,4.6e-8,1.1e-8,2.5e-9),(1.1e-7,3.7e-8,9.3e-9,2.2e-9)),
    ((9.6e-8,2.4e-8,5.8e-9,1.5e-9),(4.5e-8,1.1e-8,2.6e-9,6.4e-10)),
    ((3.7e-8,1.1e-8,2.7e-9,6.7e-10),(4.8e-9,2.0e-9,5.3e-10,1.4e-10)),
    ((6.7e-8,1.5e-8,3.6e-9,7.9e-10),(5.0e-8,1.2e-8,2.9e-9,6.7e-10)),
    ((5.5e-9,4.0e-10,3.6e-11,6.2e-12),(3.7e-9,2.5e-10,1.6e-11,1.1e-12)),
    ((5.2e-9,3.4e-10,2.2e-11,5.6e-12),(3.8e-9,2.4e-10,1.6e-11,1.7e-12)),
    ((5.2e-9,3.4e-10,2.1e-11,5.6e-12),(3.8e-9,2.4e-10,1.6e-11,1.1e-12)),
)
_H2_RATE_TOLS=((2.0,1.9),(2.0,2.0),(1.95,1.85),(2.0,2.0),(3.4,3.95),(3.95,3.95),(3.95,3.95))
_H3_SOLVERS=(('rk4','4c'),('ssprk5_4','4'))
_H3_ERROR_TOLS=(((5.6e-9,3.6e-10),(4.05e-9,2.65e-10)),((5.6e-9,3.65e-10),(4.05e-9,2.65e-10)))
_H3_RATE_TOLS=((3.95,3.94),(3.95,3.94))

def load_manifest(root:Path):
 d=json.loads((root/'tests/coverage_manifest.json').read_text())
 if d.get('schema')!=MANIFEST_SCHEMA or d.get('direct_check_directory_count')!=5 or len(d.get('checks',[]))!=5: raise ValueError('official manifest must contain exactly five script checks')
 if len({c.get('official_test') for c in d['checks']}) != 5: raise ValueError('checks must be distinct official scripts')
 return d

def numeric_rows(path:Path):
 out=[]
 for line in path.read_text(errors='replace').splitlines():
  w=line.split()
  if not w or w[0].startswith('#'): continue
  try: row=[float(x) for x in w]
  except ValueError: continue
  if row: out.append(row)
 return out

def finite(x): return isinstance(x,(int,float)) and not isinstance(x,bool) and math.isfinite(float(x))

def _allclose(a,b,atol=5e-16,rtol=1e-5):
 return abs(a-b) <= atol + rtol*abs(b)

def _rate(previous,current,previous_nx,current_nx):
 if current==0 or previous_nx<=0 or current_nx<=0 or previous_nx==current_nx: raise ValueError('invalid convergence denominator')
 return math.log(previous/current)/math.log(current_nx/previous_nx)

def _linwave(rows):
 if len(rows)!=18 or any(len(r)<5 or any(not finite(x) for x in r[:5]) for r in rows): return False,'linear-wave native table must contain 18 finite rows'
 for i,f in enumerate(('hlle','hllc','roe')):
  d=rows[i*6:(i+1)*6]
  sl,sr=(4.3e-8,.33) if f=='hlle' else (3.7e-8,.325)
  el,er=(3.1e-8,.34) if f=='hlle' else (2.7e-8,.33)
  if d[0][4]==0 or d[2][4]==0: return False,'linear-wave convergence denominator is zero'
  if d[1][4]>sl or d[1][4]/d[0][4]>sr: return False,f'{f} sound acceptance failed'
  if d[3][4]>el or d[3][4]/d[2][4]>er: return False,f'{f} entropy acceptance failed'
  if d[4][4]!=d[5][4]: return False,f'{f} direction equality failed'
 return True,'hydro_linwave.py full analyze acceptance passed'

def _sod(rows):
 if len(rows)!=18 or any(len(r)<5 or not finite(r[3]) or not finite(r[4]) for r in rows): return False,'Sod native table must contain 18 rows with finite cycles/errors'
 for j,f in enumerate(('hlle','hllc','roe')):
  d=rows[j*6:(j+1)*6]
  for axis in range(3):
   low,high=d[axis*2],d[axis*2+1]
   if low[4]==0 or low[4]>.011 or high[4]/low[4]>.6: return False,f'{f} x{axis+1} Sod acceptance failed'
  if not (d[0][3]==d[2][3]==d[4][3]): return False,f'{f} Sod cycle invariant failed'
 return True,'sod_shock.py full analyze acceptance passed'

def _linwave4_2d(rows):
 # hydro4/hydro_linwave_2d.py: seven solver tuples, five resolutions,
 # two wave families, and two no-SMR directional rows per tuple.
 if len(rows)!=84 or any(len(r)<5 or any(not finite(x) for x in r[:5]) for r in rows): return False,'hydro4 2D native table must contain 84 finite rows'
 resolutions=(16,32,64,128,256)
 for j,((torder,xorder),err_tol,rate_tol) in enumerate(zip(_H2_SOLVERS,_H2_ERROR_TOLS,_H2_RATE_TOLS)):
  d=rows[j*12:(j+1)*12]; sound=d[:5]; entropy=d[5:10]
  for wave,errs,tols in (('sound',sound,err_tol[0]),('entropy',entropy,err_tol[1])):
   for i in range(1,len(resolutions)):
    if errs[i][4]>tols[i-1]: return False,f'{torder}+{xorder} 2D {wave} error acceptance failed at nx1={resolutions[i]}'
    if resolutions[i]==128:
     try: rate=_rate(errs[i-1][4],errs[i][4],resolutions[i-1],resolutions[i])
     except ValueError: return False,f'{torder}+{xorder} 2D {wave} convergence denominator invalid'
     if rate<rate_tol[0 if wave=='sound' else 1]: return False,f'{torder}+{xorder} 2D {wave} convergence-rate acceptance failed'
  if xorder!='3c' and not _allclose(d[-2][4],d[-1][4]): return False,f'{torder}+{xorder} 2D direction equality acceptance failed'
 return True,'hydro4/hydro_linwave_2d.py full analyze acceptance passed'

def _linwave4_3d(rows):
 # hydro4/hydro_linwave_3d.py: two solver tuples, three resolutions,
 # two wave families, and two no-SMR directional rows per tuple.
 if len(rows)!=16 or any(len(r)<5 or any(not finite(x) for x in r[:5]) for r in rows): return False,'hydro4 3D native table must contain 16 finite rows'
 resolutions=(16,32,64)
 for j,((torder,xorder),err_tol,rate_tol) in enumerate(zip(_H3_SOLVERS,_H3_ERROR_TOLS,_H3_RATE_TOLS)):
  d=rows[j*8:(j+1)*8]; sound=d[:3]; entropy=d[3:6]
  for wave,errs,tols in (('sound',sound,err_tol[0]),('entropy',entropy,err_tol[1])):
   for i in range(1,len(resolutions)):
    if errs[i][4]>tols[i-1]: return False,f'{torder}+{xorder} 3D {wave} error acceptance failed at nx1={resolutions[i]}'
    if resolutions[i]==64:
     try: rate=_rate(errs[i-1][4],errs[i][4],resolutions[i-1],resolutions[i])
     except ValueError: return False,f'{torder}+{xorder} 3D {wave} convergence denominator invalid'
     if rate<rate_tol[0 if wave=='sound' else 1]: return False,f'{torder}+{xorder} 3D {wave} convergence-rate acceptance failed'
  if not _allclose(d[-2][4],d[-1][4]): return False,f'{torder}+{xorder} 3D direction equality acceptance failed'
 return True,'hydro4/hydro_linwave_3d.py full analyze acceptance passed'

def acceptance(spec,rows):
 if spec['official_test']=='hydro/hydro_carbuncle.py':
  if len(rows)!=5 or any(len(r)!=1 or not finite(r[0]) for r in rows): return False,'carbuncle native table must contain five finite scalar rows'
  for f,row in zip(('hlle','roe','llf','lhllc','hllc'),rows):
   if f not in ('hllc','roe') and row[0]>.05: return False,f'{f} carbuncle difference exceeds .05'
  return True,'hydro_carbuncle.py full analyze acceptance passed'
 if spec['official_test']=='hydro/hydro_linwave.py': return _linwave(rows)
 if spec['official_test']=='hydro/sod_shock.py': return _sod(rows)
 if spec['official_test']=='hydro4/hydro_linwave_2d.py': return _linwave4_2d(rows)
 if spec['official_test']=='hydro4/hydro_linwave_3d.py': return _linwave4_3d(rows)
 return False,'unknown official script'

def verify_result(root:Path,spec:dict):
 folder=root/spec['folder']; result_path=folder/'official-case'/'result.json'; obs_path=folder/'official-case'/'observable.json'; raw_path=folder/'official-case'/spec['observable_file']
 try: result=json.loads(result_path.read_text()); obs=json.loads(obs_path.read_text())
 except Exception as exc: return False,f'unreadable official result: {exc}',{}
 required={'schema':SCHEMA,'check_id':spec['id'],'case':spec['case'],'official_test':spec['official_test'],'runner':RUNNER,'runner_command':spec['runner_command'],'deck':spec['deck'],'source_script':spec['source_script'],'run_count':1}
 for k,v in required.items():
  if result.get(k)!=v: return False,f'result metadata mismatch: {k}',result
 if result.get('source_commit')!=SOURCE_COMMIT or result.get('configuration')!=spec.get('config'): return False,'source/configuration identity mismatch',result
 if not raw_path.is_file() or obs.get('schema')!='athena-newtonian-hydro-native-observable/v2': return False,'native observable missing/schema mismatch',result
 raw=raw_path.read_bytes(); digest=hashlib.sha256(raw).hexdigest()
 if result.get('raw_sha256')!=digest or obs.get('raw_sha256')!=digest: return False,'native digest mismatch',result
 if not isinstance(result.get('raw_artifact'),str) or not (root/result['raw_artifact']).is_file() or hashlib.sha256((root/result['raw_artifact']).read_bytes()).hexdigest()!=digest: return False,'raw run artifact mismatch',result
 if any(not isinstance(result.get(k),str) or not (root/result[k]).is_file() for k in ('raw_stdout','raw_stderr')): return False,'raw runner logs missing',result
 rows=numeric_rows(raw)
 if rows!=obs.get('rows') or obs.get('check_id')!=spec['id'] or obs.get('source_artifact')!=spec['observable_file']: return False,'observable is not derived from native bytes',result
 try: receipt=json.loads((root/'execution_manifest.json').read_text())
 except Exception as exc: return False,f'execution receipt missing: {exc}',result
 if receipt.get('schema')!='athena-newtonian-hydro-execution/v2' or receipt.get('status')!='complete' or receipt.get('source_tree_sha256')!=SOURCE_TREE: return False,'execution receipt identity mismatch',result
 records=[r for r in receipt.get('records',[]) if r.get('check_id')==spec['id'] and r.get('status')=='complete']
 if len(records)!=1 or records[0].get('artifact')!=f"{spec['folder']}/official-case/{spec['observable_file']}": return False,'execution receipt does not bind check artifact',result
 ok,detail=acceptance(spec,rows); return ok,detail,result
