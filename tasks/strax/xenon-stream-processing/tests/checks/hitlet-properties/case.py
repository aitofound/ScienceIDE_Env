#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
n=a.cases; records=np.zeros(n,strax.record_dtype(16)); records["time"]=100+np.arange(n)*32; records["dt"]=1; records["length"]=16; records["channel"]=np.arange(n)%2; records["pulse_length"]=16
wave=np.array([0,1,3,6,4,2,1,0,0,2,5,2,0,0,0,0],dtype=np.int16); records["data"][:,:]=wave
hits=strax.find_hits(records,min_amplitude=1); hitlets=strax.create_hitlets_from_hits(hits,(1,1),(0,1),0,float("inf")); hitlets=strax.get_hitlets_data(hitlets,records,np.ones(2)*scale); strax.hitlet_properties(hitlets)
order=np.lexsort((hitlets["channel"],hitlets["time"])); h=hitlets[order]
np.save(a.out,np.column_stack([h["time"],h["channel"],h["length"],h["area"],h["amplitude"],h["time_amplitude"]]).astype(np.float64))

