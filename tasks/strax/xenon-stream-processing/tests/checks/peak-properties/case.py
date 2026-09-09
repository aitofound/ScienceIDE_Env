#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
peaks=np.zeros(a.cases,strax.peak_dtype()); peaks["time"]=np.arange(a.cases)*100; peaks["dt"]=2; peaks["length"]=32
base=np.exp(-0.5*((np.arange(32)-13.0)/4.0)**2).astype(np.float32); peaks["data"][:,:32]=base*scale; peaks["area"]=peaks["data"].sum(axis=1); strax.compute_properties(peaks)
peaks=peaks[np.argsort(peaks["time"],kind="stable")]
np.save(a.out,np.column_stack([peaks["time"],peaks["area"],peaks["center_time"],peaks["median_time"],peaks["width"][:,1],peaks["width"][:,5],peaks["area_fraction_top"]]).astype(np.float64))
