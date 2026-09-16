#!/usr/bin/env python3
import argparse
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args()
rows=[]
for k in range(a.cases):
    x=np.zeros(4,strax.interval_dtype); x["time"]=k*100+np.array([0,10,20,30]); x["length"]=5; x["dt"]=1
    m=np.zeros(1,strax.interval_dtype); m["time"]=k*100; m["length"]=15; m["dt"]=1
    y=strax.replace_merged(x,m); y=y[np.argsort(y["time"],kind="stable")]; rows.extend(np.column_stack([y["time"],y["length"],y["dt"]]))
np.save(a.out,np.asarray(rows,dtype=np.float64))
