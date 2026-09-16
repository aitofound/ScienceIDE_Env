#!/usr/bin/env python3
import argparse, json
from pathlib import Path
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args()
n=a.cases; records=np.zeros(n,strax.record_dtype(32)); records["time"]=np.arange(n)*64; records["dt"]=1; records["length"]=32; records["channel"]=np.arange(n)%4
for i,r in enumerate(records): r["data"][:32]=np.array(([0]*4+[2+i%3]*5+[0]*8+[3]*4+[0]*11),dtype=np.int16)
hits=strax.find_hits(records,min_amplitude=1)
records["data"][:,:32]=1
out=strax.cut_outside_hits(records,hits,left_extension=2,right_extension=3)
np.save(a.out,out["data"].astype(np.float64))

