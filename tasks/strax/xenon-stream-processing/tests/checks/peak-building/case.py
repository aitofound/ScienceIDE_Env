#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
n=a.cases; records=np.zeros(2*n,strax.record_dtype(16)); records["time"]=np.repeat(np.arange(n)*80,2); records["time"][1::2]+=3; records["dt"]=1; records["length"]=16; records["channel"]=np.tile([0,1],n); records["pulse_length"]=16; records["data"][:,2:8]=np.tile(np.array([2,4,7,5,3,1],dtype=np.int16),(2*n,1))
hits=strax.find_hits(records,np.ones(2)); hits=strax.sort_by_time(hits)
peaks=strax.find_peaks(hits,np.ones(2)*scale,gap_threshold=20,left_extension=2,right_extension=3,min_area=0,min_channels=1,max_duration=1000)
peaks=peaks[np.argsort(peaks["time"],kind="stable")]
np.save(a.out,np.column_stack([peaks["time"],peaks["length"],peaks["dt"],peaks["area"],peaks["n_hits"],peaks["max_gap"]]).astype(np.float64))
