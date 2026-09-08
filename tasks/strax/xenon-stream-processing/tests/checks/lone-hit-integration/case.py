#!/usr/bin/env python3
import argparse, json
import numpy as np, strax
p=argparse.ArgumentParser(); p.add_argument("--config"); p.add_argument("--out"); p.add_argument("--cases",type=int); a=p.parse_args(); scale=json.load(open(a.config))["scale"]
n=a.cases; records=np.zeros(n,strax.record_dtype(24)); records["time"]=np.arange(n)*40; records["dt"]=1; records["length"]=24; records["channel"]=np.arange(n)%4; records["pulse_length"]=24
records["data"][:,4:8]=3; records["data"][:,14:17]=5
hits=strax.find_hits(records,np.ones(4)*scale); strax.find_hit_integration_bounds(hits,np.zeros(0,dtype=strax.time_dt_fields),records,(2,3),4,allow_bounds_beyond_records=False)
order=np.lexsort((hits["channel"],hits["time"])); h=hits[order]
np.save(a.out,np.column_stack([h["time"],h["channel"],h["left"],h["right"],h["left_integration"],h["right_integration"],h["area"]]).astype(np.float64))

