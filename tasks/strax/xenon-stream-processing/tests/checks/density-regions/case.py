#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
rows=[]
for i in range(a.cases):
    x=np.linspace(-5,5,1001); d=np.exp(-0.5*x*x); d*=scale
    intervals,heights=strax.highest_density_region(d,np.array([0.5,0.6827,0.9]),only_upper_part=True)
    # Convert the interval buffer to physical-bin membership. Buffer slot order
    # and unused sentinels are implementation details, not scientific output.
    for fraction_intervals,height in zip(intervals,heights):
        mask=np.zeros(len(d),dtype=np.float64); n_regions=0
        for left,right in fraction_intervals.T:
            if left>=0 and right>left:
                mask[left:right]=1; n_regions+=1
        rows.extend(mask); rows.extend([float(n_regions),float(height)])
np.save(a.out,np.asarray(rows,dtype=np.float64))
