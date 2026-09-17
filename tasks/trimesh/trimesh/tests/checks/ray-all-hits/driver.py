#!/usr/bin/env python3
"""Complete physical ray events, not arbitrary triangle ownership at ties."""
import argparse,json,hashlib
from pathlib import Path
import numpy as np
import trimesh
from trimesh.ray.ray_triangle import RayMeshIntersector
from trimesh.ray.ray_util import contains_points


def events(mesh, origins, directions):
    engine=RayMeshIntersector(mesh)
    loc,ray,_=engine.intersects_location(origins,directions,multiple_hits=True)
    # No dedup/truncation here: production's complete location events are graded.
    order=np.lexsort((loc[:,2],loc[:,1],loc[:,0],ray))
    pairs=np.column_stack((ray[order],loc[order]))
    counts=np.bincount(ray,minlength=len(origins))
    any_hit=engine.intersects_any(origins,directions)
    if not np.array_equal(any_hit,counts>0): raise RuntimeError('inconsistent any/all hits')
    return pairs,counts


def slab_events(origins,directions,half):
    rows=[];counts=[]
    for i,(o,d) in enumerate(zip(origins,directions)):
        near=-np.inf;far=np.inf;valid=True
        for j in range(3):
            if d[j]==0:
                if abs(o[j])>half: valid=False
            else:
                ts=sorted(((-half-o[j])/d[j],(half-o[j])/d[j]))
                near=max(near,ts[0]);far=min(far,ts[1])
        times=sorted(set(t for t in [near,far] if t>=0)) if valid and far>=near else []
        counts.append(len(times))
        for t in times: rows.append([i,*(o+t*d)])
    rows=np.asarray(rows,dtype=float).reshape(-1,4)
    return rows[np.lexsort((rows[:,3],rows[:,2],rows[:,1],rows[:,0]))],np.array(counts)


def main():
    p=argparse.ArgumentParser();p.add_argument('--input',required=True);p.add_argument('--output',required=True);p.add_argument('--samples',type=int,default=32);a=p.parse_args()
    if not 8<=a.samples<=128: raise ValueError('samples must be 8..128')
    factor=float(json.loads(Path(a.input).read_text())['factor']);out=Path(a.output);out.mkdir(parents=True,exist_ok=True)
    # Nonthreshold generic rays plus exact shared-edge/vertex hits and misses.
    v=np.linspace(-1.37,1.37,a.samples);x,y=np.meshgrid(v,v,indexing='ij')
    origins=np.column_stack((x.ravel(),y.ravel(),np.full(x.size,-3.)))
    directions=np.tile([0.,0.,1.],(len(origins),1))
    tie_dirs=np.array([[x,y,z] for x in [-1.,1.] for y in [-1.,1.] for z in [-1.,1.]]+[[1.,1.,0.],[1.,0.,1.],[0.,1.,1.]])
    origins=np.vstack((origins,np.zeros_like(tie_dirs),[[0,0,3],[3,0,0]]));directions=np.vstack((directions,tie_dirs,[[0,0,1],[0,1,0]]))
    box=trimesh.creation.box(extents=np.full(3,2*factor))
    pairs,counts=events(box,origins,directions);truth,tc=slab_events(origins,directions,factor)
    np.testing.assert_array_equal(counts,tc);np.testing.assert_allclose(pairs,truth,atol=2e-13,rtol=2e-13)
    np.save(out/'box_events.npy',pairs);np.save(out/'box_counts.npy',counts)
    # Preserve BOTH exact original example configurations, including their misses.
    for name,mesh,o in [('script',trimesh.primitives.Sphere(radius=factor),[[0,0,-5],[2,2,-10]]),('notebook',trimesh.creation.icosphere(radius=factor),[[0,0,-3],[2,2,-3]])]:
        ep,ec=events(mesh,np.array(o,dtype=float),np.array([[0,0,1],[0,0,1]],dtype=float))
        np.testing.assert_array_equal(ec,[2,0]);np.testing.assert_allclose(ep,[[0,0,0,-factor],[0,0,0,factor]],atol=2e-13,rtol=2e-13)
        np.save(out/(name+'_events.npy'),ep);np.save(out/(name+'_counts.npy'),ec)
    points=np.array([[0,0,0],[.25,.3,.4],[-.7,.2,.1],[1.2,0,0],[10,10,10],[.01,.02,.03],[.8,.8,.8]])
    # Concentric oppositely oriented shells express a cavity without a Boolean dependency.
    inner=trimesh.creation.box(extents=np.full(3,.2*factor));inner.invert();cavity=trimesh.util.concatenate([box,inner])
    contained=[]
    for m,expected in [(box,np.max(abs(points),axis=1)<factor),(cavity,(np.max(abs(points),axis=1)<factor)&(np.max(abs(points),axis=1)>.1*factor))]:
        engine=RayMeshIntersector(m)
        # Explicit direction disables randomized recovery. Forward/reverse parity must agree.
        d=np.tile([.4395064455,.617598629942,.652231566745],(len(points),1))
        _,forward=events(m,points,d);_,backward=events(m,points,-d)
        np.testing.assert_array_equal(forward%2,backward%2)
        actual=contains_points(engine,points,check_direction=d[0]);np.testing.assert_array_equal(actual,expected);contained.append(actual)
    np.save(out/'containment.npy',np.array(contained,dtype=np.int64))
    # Undefined surface-point containment is intentionally NOT asserted or graded.
    (out/'diagnostics.json').write_text(json.dumps({'ray_count':len(origins),'events':len(pairs),'edge_vertex_rays':len(tie_dirs),'counts_sha256':hashlib.sha256(counts.tobytes()).hexdigest(),'random_recovery':False},indent=2))
if __name__=='__main__': main()
