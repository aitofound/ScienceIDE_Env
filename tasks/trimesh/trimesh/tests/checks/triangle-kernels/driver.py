#!/usr/bin/env python3
import argparse,json
from pathlib import Path
import numpy as np
from trimesh import triangles as tt

def main():
 p=argparse.ArgumentParser();p.add_argument('--input',required=True);p.add_argument('--output',required=True);p.add_argument('--triangles',type=int,default=200000);a=p.parse_args()
 scale=float(json.loads(Path(a.input).read_text())['scale']); n=a.triangles
 i=np.arange(n,dtype=np.float64); base=np.column_stack((np.sin(i*.013),np.cos(i*.017),i/n))*scale
 u=np.column_stack((1.0+.1*np.sin(i*.019),.2+.05*np.cos(i*.023),.3+.02*np.sin(i*.029)))*scale
 v=np.column_stack((.1+.03*np.cos(i*.031),1.1+.1*np.sin(i*.037),.4+.02*np.cos(i*.041)))*scale
 tris=np.stack((base,base+u,base+v),axis=1)
 bary=np.column_stack((.2+.01*np.sin(i*.007),.3+.01*np.cos(i*.011),np.zeros(n)))
 bary[:,2]=1.0-bary[:,:2].sum(axis=1)
 points=tt.barycentric_to_points(tris,bary); recovered=tt.points_to_barycentric(tris,points,method='cramer')
 areas=tt.area(tris); normals,valid=tt.normals(tris)
 if not valid.all() or np.max(np.abs(recovered-bary))>1e-10: raise RuntimeError('triangle kernel consistency failed')
 out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
 np.save(out/'points.npy',points);np.save(out/'barycentric.npy',recovered);np.save(out/'area.npy',areas);np.save(out/'normal.npy',normals)
if __name__=='__main__':main()
