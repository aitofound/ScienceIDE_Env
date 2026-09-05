#!/usr/bin/env python3
import argparse,json
from pathlib import Path
import numpy as np
from pyamg.gallery import poisson
from pyamg.relaxation.utils import relaxation_as_linear_operator

def main():
 p=argparse.ArgumentParser(); p.add_argument('--input',required=True); p.add_argument('--out',required=True); p.add_argument('--size',type=int,required=True); a=p.parse_args()
 cfg=json.loads(Path(a.input).read_text()); np.random.seed(int(cfg['seed'])); rng=np.random.RandomState(int(cfg['seed'])); values=[]
 for complex_case in (False,True):
  A=poisson((a.size,a.size),format='csr').astype(np.float64)
  if complex_case:A=(1.0+0.25j)*A
  b=rng.rand(A.shape[0]); x=rng.rand(A.shape[0])
  if complex_case:b=b+1j*rng.rand(A.shape[0]); x=x+1j*rng.rand(A.shape[0])
  b[0]*=float(cfg['rhs_scale'])
  for method in ('gauss_seidel','jacobi','block_gauss_seidel','block_jacobi'):
   y=relaxation_as_linear_operator((method,{'iterations':2}),A,b) @ x
   values.extend((np.asarray(y.real,dtype=np.float64),np.asarray(y.imag,dtype=np.float64)))
 np.save(a.out,np.concatenate(values),allow_pickle=False)
if __name__=='__main__': main()
