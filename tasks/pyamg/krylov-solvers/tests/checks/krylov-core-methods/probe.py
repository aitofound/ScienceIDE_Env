#!/usr/bin/env python3
import argparse,json
from pathlib import Path
import numpy as np
from pyamg.gallery import poisson
from pyamg.krylov import bicgstab,cg,cr,fgmres,gmres

def main():
 q=argparse.ArgumentParser(); q.add_argument('--input',required=True); q.add_argument('--out',required=True); q.add_argument('--size',required=True,type=int); q.add_argument('--iterations',required=True,type=int); q.add_argument('--bicgstab-iterations',required=True,type=int); a=q.parse_args()
 scale=float(json.loads(Path(a.input).read_text())['rhs_scale']); A=poisson((a.size,),format='csr').astype(np.float64); A.setdiag(A.diagonal()+0.1); rhs=np.linspace(.5,1.5,A.shape[0]); rhs[0]*=scale; values=[]
 for method in (cg,cr,gmres,fgmres,bicgstab):
  residuals=[]; method_iterations=a.bicgstab_iterations if method is bicgstab else a.iterations; solution,flag=method(A,rhs,x0=np.zeros_like(rhs),tol=0.0,maxiter=method_iterations,residuals=residuals); padded=np.zeros(a.iterations+2,dtype=np.float64); take=min(len(residuals),padded.size); padded[:take]=np.asarray(residuals[:take],dtype=np.float64); values.extend((solution,padded,np.asarray([len(residuals),flag],dtype=np.float64)))
 np.save(a.out,np.concatenate(values),allow_pickle=False)
if __name__=='__main__':main()
