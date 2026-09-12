#!/usr/bin/env python3
import argparse,json
from pathlib import Path
def rows(p): return [x.strip() for x in p.read_text(encoding="utf-8",errors="replace").splitlines() if x.strip() and not x.lstrip().startswith("#")]
def main():
 a=argparse.ArgumentParser(); a.add_argument("--reference",required=True); a.add_argument("--candidate",required=True); a.add_argument("--rubric",required=True); a.add_argument("--out",required=True); x=a.parse_args(); r=json.loads(Path(x.rubric).read_text()); fail=[]; details={}
 for s in r["comparison"]["files"]:
  rel=s["path"]; rp=Path(x.reference)/rel; cp=Path(x.candidate)/rel
  if not rp.is_file() or not cp.is_file(): fail.append(rel+": missing"); continue
  rr,cc=rows(rp),rows(cp); ok=rr==cc; details[rel]={"rows":len(rr),"matching":ok}
  if not ok: fail.append(rel+": normalized scientific rows differ")
 passed=not fail; out={"passed":passed,"policy":"pointwise","distance":0.0 if passed else 1.0,"bound_fraction":0.0 if passed else 1.0,"files":details,"reason":"all normalized scientific rows match" if passed else "; ".join(fail)}
 Path(x.out).write_text(json.dumps(out,indent=2,sort_keys=True)+"\n"); print(out["reason"])
if __name__=="__main__": main()

