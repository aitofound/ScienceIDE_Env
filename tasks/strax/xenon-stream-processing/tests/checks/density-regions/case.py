#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
rows=[]
for i in range(a.cases):
    x=np.linspace(-5,5,1001); d=np.exp(-0.5*x*x); d[500]*=scale
    intervals,heights=strax.highest_density_region(d,np.array([0.5,0.6827,0.9]),only_upper_part=True)
    rows.extend(intervals.astype(np.float64).ravel()); rows.extend(heights.astype(np.float64).ravel())
np.save(a.out,np.asarray(rows,dtype=np.float64))

