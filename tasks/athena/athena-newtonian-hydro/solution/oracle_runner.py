#!/usr/bin/env python3
"""Run each distinct pinned upstream Athena++ hydro regression script once."""
from __future__ import annotations
import hashlib,json,os,shutil,socket,subprocess,sys,tempfile,time
from pathlib import Path

def tree_hash(root):
 rows=[]
 for p in sorted(x for x in root.rglob('*') if x.is_file() and not x.is_symlink()):
  rows.append(f'{p.relative_to(root)}\0{hashlib.sha256(p.read_bytes()).hexdigest()}\n')
 return hashlib.sha256(''.join(rows).encode()).hexdigest(),len(rows),sum(p.stat().st_size for p in root.rglob('*') if p.is_file() and not p.is_symlink())

def numeric_rows(path):
 out=[]
 for line in path.read_text(errors='replace').splitlines():
  w=line.split()
  if not w or w[0].startswith('#'): continue
  try: out.append([float(x) for x in w])
  except ValueError: continue
 return [r for r in out if r]

def main():
 leaf,source,checks_root,out,extractor,manifest,jobs,cap,evidence=sys.argv[1:]
 leaf,source,out=Path(leaf),Path(source),Path(out)
 ident=json.loads((leaf/'tests/lib/source_identity.json').read_text()); tree,count,total=tree_hash(source)
 if (tree,count,total)!=(ident['tree_sha256'],ident['file_count'],ident['byte_count']): raise SystemExit('pinned source tree identity mismatch')
 catalog=json.loads(Path(manifest).read_text()); checks=catalog['checks']
 if len(checks)!=5 or len({c['official_test'] for c in checks})!=5: raise SystemExit('catalog must contain five distinct official scripts')
 out.mkdir(parents=True,exist_ok=True); build=Path(tempfile.mkdtemp(prefix='athena-build-',dir=str(out))); shutil.copytree(source,build,symlinks=False,dirs_exist_ok=True)
 nonce=f'{time.time_ns():x}-{os.getpid()}'; started=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()); records=[]
 outputs={'hydro/hydro_carbuncle.py':'carbuncle-diff.dat','hydro/hydro_linwave.py':'linearwave-errors.dat','hydro/sod_shock.py':'shock-errors.dat','hydro4/hydro_linwave_2d.py':'linearwave-errors.dat','hydro4/hydro_linwave_3d.py':'linearwave-errors.dat'}
 for spec in checks:
  test=spec['official_test']; cwd=build/'tst/regression'; d=out/'.runs'/spec['id']; d.mkdir(parents=True,exist_ok=True); argv=['python3','run_tests.py',test]
  t0=time.monotonic(); proc=subprocess.run(argv,cwd=cwd,text=True,capture_output=True,timeout=float(cap),check=False); (d/'stdout.log').write_text(proc.stdout); (d/'stderr.log').write_text(proc.stderr)
  native=cwd/'bin'/outputs[test]
  if not native.is_file(): raise SystemExit(f'official runner did not produce {native}')
  raw=native.read_bytes(); raw_rel=f'.runs/{spec["id"]}/{native.name}'; (out/raw_rel).write_bytes(raw); rows=numeric_rows(native)
  dest=out/spec['folder']/'official-case'; dest.mkdir(parents=True,exist_ok=True); (dest/spec['observable_file']).write_bytes(raw); digest=hashlib.sha256(raw).hexdigest()
  obs={'schema':'athena-newtonian-hydro-native-observable/v2','check_id':spec['id'],'case':spec['case'],'source_artifact':spec['observable_file'],'official_test':test,'rows':rows,'raw_artifact':raw_rel,'raw_sha256':digest}; (dest/'observable.json').write_text(json.dumps(obs,indent=2,sort_keys=True)+'\n')
  result={'schema':'athena-newtonian-hydro-official-result/v2','check_id':spec['id'],'case':spec['case'],'official_test':test,'runner':spec['runner'],'runner_command':spec['runner_command'],'deck':spec['deck'],'source_script':spec['source_script'],'source_commit':catalog['source_commit'],'configuration':spec['config'],'run_count':1,'official_lines':spec['official_lines'],'raw_stdout':f'.runs/{spec["id"]}/stdout.log','raw_stderr':f'.runs/{spec["id"]}/stderr.log','raw_artifact':raw_rel,'raw_sha256':digest,'observable':'official-case/observable.json'}; (dest/'result.json').write_text(json.dumps(result,indent=2,sort_keys=True)+'\n')
  records.append({'check_id':spec['id'],'folder':spec['folder'],'official_test':test,'status':'complete' if proc.returncode==0 else 'runner-nonzero','run_count':1,'artifact':f'{spec["folder"]}/official-case/{spec["observable_file"]}','stdout':f'.runs/{spec["id"]}/stdout.log','stderr':f'.runs/{spec["id"]}/stderr.log','seconds':time.monotonic()-t0})
 finished=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()); receipt={'schema':'athena-newtonian-hydro-execution/v2','status':'complete' if all(r['status']=='complete' for r in records) else 'failed','role':'reference-oracle','evidence_class':evidence,'source_commit':catalog['source_commit'],'source_tree_sha256':tree,'source_file_count':count,'source_byte_count':total,'run_nonce':nonce,'container_hostname':socket.gethostname(),'container_id':os.environ.get('HOSTNAME',socket.gethostname()),'started_at':started,'finished_at':finished,'check_count':5,'records':records,'build_tree':str(build)}; (out/'execution_manifest.json').write_text(json.dumps(receipt,indent=2,sort_keys=True)+'\n')
 if receipt['status']!='complete': raise SystemExit('official runner returned nonzero')
if __name__=='__main__': main()
