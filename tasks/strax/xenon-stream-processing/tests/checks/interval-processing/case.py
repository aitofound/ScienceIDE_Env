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
# Grade physical identities and extents, not array offsets returned by the API.
inside_time=np.full(len(inside),-1,dtype=np.int64); matched=inside>=0
inside_time[matched]=containers["time"][inside[matched]]
window_time=np.full((len(windows),2),-1,dtype=np.int64)
for i,(left,right) in enumerate(windows):
    if right>left:
        window_time[i]=[things["time"][left],strax.endtime(things[right-1:right])[0]]
np.save(a.out,np.concatenate([inside_time.astype(np.float64),window_time.astype(np.float64).ravel()]))
