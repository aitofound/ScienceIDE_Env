#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args()
n=a.cases; containers=np.zeros(n,strax.interval_dtype); containers["time"]=np.arange(n)*100; containers["length"]=80; containers["dt"]=1
things=np.zeros(3*n,strax.interval_dtype); things["dt"]=1
for i in range(n):
    things[3*i:3*i+3]["time"]=containers[i]["time"]+np.array([5,35,75])
    things[3*i:3*i+3]["length"]=[10,20,10]
inside=strax.fully_contained_in(things,containers)
windows=strax.touching_windows(things,containers,window=2)
np.save(a.out,np.concatenate([inside.astype(np.float64),windows.astype(np.float64).ravel()]))

