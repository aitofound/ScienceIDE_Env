#!/usr/bin/env python3
import argparse,json
from pathlib import Path
import numpy as np
import trimesh

def directions(n):
 i=np.arange(n,dtype=np.float64)+0.5; z=1.0-2.0*i/n
 phi=i*(np.pi*(3.0-np.sqrt(5.0)))+0.2718281828459045
 r=np.sqrt(np.maximum(0.0,1.0-z*z))
 return np.column_stack((r*np.cos(phi),r*np.sin(phi),z))

def main():
 p=argparse.ArgumentParser();p.add_argument('--input',required=True);p.add_argument('--output',required=True);p.add_argument('--rays',type=int,default=16384);a=p.parse_args()
 s=json.loads(Path(a.input).read_text()); radius=float(s['radius'])
 mesh=trimesh.creation.icosphere(subdivisions=4,radius=radius)
 unit=directions(a.rays); origins=unit*3.0; vectors=-unit
 # A deterministic subset points away from the sphere, exercising no-hit output.
 miss=np.arange(a.rays)%11==0; vectors[miss]=unit[miss]
 loc,idx,_=mesh.ray.intersects_location(origins,vectors,multiple_hits=False)
 hit=np.zeros(a.rays,dtype=np.float64); hit[idx]=1.0
 first=np.zeros((a.rays,3),dtype=np.float64); first[idx]=loc
 distance=np.zeros(a.rays,dtype=np.float64); distance[idx]=np.einsum('ij,ij->i',loc-origins[idx],vectors[idx])
 if not np.array_equal(hit==0,miss): raise RuntimeError('unexpected ray hit classification')
 if np.any(distance[idx]<=0) or not np.all(np.isfinite(first)): raise RuntimeError('invalid first-hit result')
 out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
 np.save(out/'hit.npy',hit);np.save(out/'location.npy',first);np.save(out/'distance.npy',distance)
if __name__=='__main__':main()
