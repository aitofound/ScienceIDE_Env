#!/usr/bin/env python3
"""Canonicalize one real Phantom dump and complete numeric event history."""
import glob,importlib.util,json,os,re,sys
import numpy as np
PIN='e53ea16758d2a261680506852a528f21270dca1c'
reader,dumpfile,prefix,out,check,setup,metric=sys.argv[1:]
spec=importlib.util.spec_from_file_location('phantom_reader',reader);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);d=m.read_dump(dumpfile)
a={};schema=[]
for ib,b in enumerate(d['blocks']):
 data=b.get('data',{});order=None
 if 'iorig' in data:
  rawids=np.asarray(data['iorig']);ids=np.asarray(rawids,dtype='<i8')
  if ids.size and np.unique(ids).size!=ids.size and rawids.dtype.kind=='f' and rawids.dtype.itemsize==8:
   viewed=np.ascontiguousarray(rawids).view('<i8')
   if np.unique(viewed).size==viewed.size:ids=viewed
  if ids.size==0 or np.unique(ids).size!=ids.size:raise RuntimeError('invalid iorig')
  order=np.argsort(ids,kind='stable')
 for name in sorted(data):
  v=ids if name=='iorig' and order is not None else np.asarray(data[name])
  if v.ndim!=1:raise RuntimeError('non-vector field '+name)
  if order is not None:
   n=order.size
   if name=='itype' and v.size!=n and v.nbytes==n:v=np.ascontiguousarray(v).view('u1')
   if name=='u' and v.dtype.kind=='f' and v.dtype.itemsize==4 and v.size==2*n:
    viewed=np.ascontiguousarray(v).view('<f8')
    if np.isfinite(viewed).all():v=viewed
   if v.size==n:v=v[order]
  if v.dtype.kind not in 'iuf':continue
  if v.dtype.kind=='f' and not np.isfinite(v).all():raise RuntimeError('non-finite '+name)
  key='block%02d__%s'%(ib,re.sub(r'[^A-Za-z0-9_.-]+','_',name).strip('_'))
  v=np.asarray(v,dtype='<i8' if v.dtype.kind in 'iu' else '<f8');a[key]=v
  schema.append({'key':key,'block':ib,'field':name,'shape':list(v.shape),'dtype':v.dtype.str})
if not a:raise RuntimeError('empty dump state')
os.makedirs(out,exist_ok=False);np.savez(os.path.join(out,'state.npz'),**a)
rows=[]
for ep in sorted(glob.glob(prefix+'*.ev')):
 for line in open(ep,encoding='utf-8'):
  if line.strip() and not line.lstrip().startswith('#'):
   vals=[float(x) for x in line.split()]
   if not np.isfinite(vals).all():raise RuntimeError('non-finite event row')
   rows.append(vals)
if len({len(x) for x in rows})>1:raise RuntimeError('inconsistent event widths')
diag=np.asarray(rows,dtype='<f8') if rows else np.empty((0,0),dtype='<f8')
np.save(os.path.join(out,'diagnostics.npy'),diag,allow_pickle=False)
q={}
for k,v in d.get('quantities',{}).items():
 z=np.asarray(v)
 if z.size==1 and z.dtype.kind in 'iuf' and np.isfinite(z).all():q[str(k)]=float(z.reshape(-1)[0])
meta={'schema':'phantom-relativity-artifact/v1','check':check,'setup':setup,'metric':metric,'source_commit':PIN,'dump_file':os.path.basename(dumpfile),'quantities':q,'fields':schema,'diagnostics_shape':list(diag.shape)}
with open(os.path.join(out,'meta.json'),'w',encoding='utf-8') as f:json.dump(meta,f,indent=2,sort_keys=True);f.write('\n')
