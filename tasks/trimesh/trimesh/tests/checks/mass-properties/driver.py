#!/usr/bin/env python3
import argparse,json
from pathlib import Path
import numpy as np
import trimesh

def main():
 p=argparse.ArgumentParser();p.add_argument('--input',required=True);p.add_argument('--output',required=True);p.add_argument('--copies',type=int,default=32);a=p.parse_args()
 radius=float(json.loads(Path(a.input).read_text())['radius']); base=trimesh.creation.icosphere(subdivisions=5,radius=radius)
 rows=[]
 for j in range(a.copies):
  mesh=base.copy(); angle=(j+1)*.03125
  T=trimesh.transformations.rotation_matrix(angle,[1.0,2.0,3.0]); T[:3,3]=[j*.01,-j*.005,j*.002]
  mesh.apply_transform(T); mp=mesh.mass_properties
  rows.append(np.r_[mp.volume,mp.mass,mp.center_mass,mp.inertia.reshape(-1),mesh.area])
 values=np.asarray(rows,dtype=np.float64)
 if not np.all(np.isfinite(values)) or np.any(values[:,0]<=0):raise RuntimeError('invalid mass properties')
 out=Path(a.output);out.mkdir(parents=True,exist_ok=True);np.save(out/'mass_properties.npy',values)
if __name__=='__main__':main()
