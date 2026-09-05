#!/usr/bin/env python3
import argparse,json
from pathlib import Path
import numpy as np
from pyamg.gallery import poisson
from pyamg.relaxation.relaxation import gauss_seidel,jacobi

def main():
 q=argparse.ArgumentParser(); q.add_argument('--input',required=True); q.add_argument('--out',required=True); q.add_argument('--size',required=True,type=int); q.add_argument('--jacobi-sweeps',required=True,type=int); q.add_argument('--gs-sweeps',required=True,type=int); a=q.parse_args()
 scale=float(json.loads(Path(a.input).read_text())['rhs_scale']); A=poisson((a.size,a.size),format='csr').astype(np.float64); rhs=np.linspace(.5,1.5,A.shape[0]); rhs[0]*=scale
 xj=np.zeros_like(rhs); xg=np.zeros_like(rhs); jacobi(A,xj,rhs,iterations=a.jacobi_sweeps,omega=2/3); gauss_seidel(A,xg,rhs,iterations=a.gs_sweeps,sweep='symmetric')
 residuals=np.asarray([np.linalg.norm(rhs-A@xj),np.linalg.norm(rhs-A@xg)],dtype=np.float64); np.save(a.out,np.concatenate((xj,xg,residuals)),allow_pickle=False)
if __name__=='__main__':main()
