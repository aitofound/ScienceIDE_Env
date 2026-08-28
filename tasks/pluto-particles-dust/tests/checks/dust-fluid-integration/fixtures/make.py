#!/usr/bin/env python3
from pathlib import Path
import json,sys
if len(sys.argv)!=2: raise SystemExit('usage: make.py OUTDIR')
out=Path(sys.argv[1]); out.mkdir(parents=True,exist_ok=True)
for label in ('reference','accept-placeholder','reject-near-miss'):
 d=out/label; d.mkdir(exist_ok=True); (d/'fixture.json').write_text(json.dumps({'check':'dust-fluid-integration','status':'blocked','expected':'prerequisite is not satisfied'},sort_keys=True)+'\n',encoding='utf-8')
print('blocked fixture metadata written to',out)
