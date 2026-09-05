#!/usr/bin/env python3
"""Deterministic, graded execution of the README Example Usage."""
import argparse,json
from pathlib import Path
import numpy as np
import pyamg

def main():
 p=argparse.ArgumentParser(); p.add_argument('--input',required=True); p.add_argument('--out',required=True); p.add_argument('--size',type=int,required=True); a=p.parse_args()
 cfg=json.loads(Path(a.input).read_text()); rng=np.random.RandomState(int(cfg['seed']))
 A=pyamg.gallery.poisson((a.size,a.size),format='csr')
 ml=pyamg.ruge_stuben_solver(A)
 b=rng.rand(A.shape[0]); b[0]*=float(cfg['rhs_scale'])
 residuals=[]; x=ml.solve(b,tol=1e-10,residuals=residuals)
 sizes=np.asarray([level.A.shape[0] for level in ml.levels],dtype=np.float64)
 nnz=np.asarray([level.A.nnz for level in ml.levels],dtype=np.float64)
 diagnostics=np.asarray([np.linalg.norm(b-A@x),ml.operator_complexity(),ml.grid_complexity(),len(ml.levels)],dtype=np.float64)
 np.save(a.out,np.concatenate((x,np.asarray(residuals,dtype=np.float64),sizes,nnz,diagnostics)),allow_pickle=False)
if __name__=='__main__': main()
