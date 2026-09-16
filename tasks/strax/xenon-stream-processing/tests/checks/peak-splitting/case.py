#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
splitter=strax.processing.peak_splitting.LocalMinimumSplitter(); rows=[]
for i in range(a.cases):
    w=np.array([0,2,5,3,1,3,6,2,0,1,4,1,0],dtype=np.float64); w[6]*=scale
    raw=np.array(list(splitter.find_split_points(w,dt=None,peak_i=None,min_height=1,min_ratio=0)),dtype=np.float64)
    valid=raw[:,0][raw[:,0]>=0]
    rows.append([len(valid),valid.sum(),w.sum()])
np.savetxt(a.out,np.asarray(rows,dtype=np.float64),fmt="%.17g")
