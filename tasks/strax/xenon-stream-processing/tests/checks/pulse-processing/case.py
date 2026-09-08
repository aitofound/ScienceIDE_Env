#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
n=a.cases; records=np.zeros(n,strax.record_dtype(32)); records["time"]=np.arange(n)*40; records["dt"]=1; records["length"]=32; records["channel"]=np.arange(n)%4; records["pulse_length"]=32
records["data"][:,5:10]=np.array([1,3,7,3,1]); records["data"][:,20:24]=np.array([2,5,5,2]); hits=strax.find_hits(records,min_amplitude=1)
x=np.tile(np.sin(np.linspace(0,3*np.pi,32)),(n,1)); ir=np.array([0.2,0.6,0.2])*scale; filtered=strax.filter_waveforms(x,ir,prev_r=np.full(n,strax.NO_RECORD_LINK),next_r=np.full(n,strax.NO_RECORD_LINK))
hits=hits[np.lexsort((hits["channel"],hits["time"]))]
meta=np.column_stack([hits["time"],hits["length"],hits["area"]]).astype(np.float64).ravel()
np.save(a.out,np.concatenate([meta,filtered.astype(np.float64).ravel()]))
