#!/usr/bin/env python3
"""Extract coordinate-indexed global physical observables from EPOCH SDF.

Each graded plain-variable array is indexed using the SDF ``Grid/Grid`` plain
mesh edges and the variable's physical stagger metadata. Values are explicitly
canonicalized in physical-coordinate order with x varying fastest, then y,
then z, rather than accepted as an unexplained flattened byte order. The emitted contract.json
contains exact coordinate signatures; validators compare those signatures and
fail closed if the physical coordinate index is absent or differs.
"""
from __future__ import annotations
import hashlib,json,struct,sys
from pathlib import Path
import numpy as np
MAGIC=b"SDF1"; ENDIANNESS_LE=16911887
PLAIN_MESH,PLAIN,CONSTANT,ARRAY,CPU_SPLIT=1,3,5,6,20
IDLEN=32
DT={1:("<i4",4),2:("<i8",8),3:("<f4",4),4:("<f8",8),7:("<u1",1)}
OBS=[('Electric Field/Ex', 'ex'), ('Current/Jx', 'jx'), ('Derived/Charge_Density', 'charge_density'), ('Derived/Number_Density', 'number_density'), ('Derived/Number_Density/Background', 'number_density_Background'), ('Derived/Number_Density/Beam', 'number_density_Beam')]
SPECIES=['Background', 'Beam']
DUMPS=(1, 3, 5)
LAYOUT=False
UNITS={'ex': 'V/m', 'ey': 'V/m', 'ez': 'V/m', 'bx': 'T', 'by': 'T', 'bz': 'T', 'jx': 'A/m^2', 'jy': 'A/m^2', 'jz': 'A/m^2', 'charge_density': 'C/m^3', 'number_density': '1/m^3', 'number_density_electrons': '1/m^3', 'number_density_protons': '1/m^3', 'number_density_Right': '1/m^3', 'number_density_Left': '1/m^3', 'number_density_Background': '1/m^3', 'number_density_Beam': '1/m^3'}
COORDINATE_ORDER='physical coordinates (x fastest, then y, then z)'
def _text(buf,offset,length): return buf[offset:offset+length].split(b"\0",1)[0].decode("ascii","replace").strip()
def _payload(buf,off,code,dims):
    typ,size=DT[code]; n=1
    for d in dims: n*=int(d)
    raw=buf[off:off+n*size]
    if len(raw)!=n*size: raise ValueError(f"short SDF payload: expected {n*size}, got {len(raw)}")
    return np.array(np.frombuffer(raw,dtype=typ,count=n),copy=True),tuple(int(d) for d in dims),code
def read_sdf(path):
    buf=path.read_bytes()
    if buf[:4]!=MAGIC or struct.unpack_from("<i",buf,4)[0]!=ENDIANNESS_LE: raise ValueError(f"{path}: invalid SDF1")
    first=struct.unpack_from("<q",buf,48)[0]; nblocks=struct.unpack_from("<i",buf,68)[0]; header=struct.unpack_from("<i",buf,72)[0]; slen=struct.unpack_from("<i",buf,96)[0]
    if nblocks<=0: raise ValueError(f"{path}: incomplete dump")
    out={}; ids={}; loc=first
    for _ in range(nblocks):
        if loc<0 or loc+68+slen>len(buf): raise ValueError(f"{path}: invalid block offset")
        nxt,data=struct.unpack_from("<qq",buf,loc); blocktype,code,ndims=struct.unpack_from("<iii",buf,loc+56); block_id=_text(buf,loc+16,IDLEN); name=_text(buf,loc+68,slen); meta=loc+header; value=None; info={"kind":"other"}
        if code in DT:
            if blocktype==PLAIN_MESH:
                n=ndims; dims=struct.unpack_from(f"<{n}i",buf,meta+88*n+4); mesh_values,_,_=_payload(buf,data,code,(sum(dims),)); value=(mesh_values,tuple(int(x) for x in dims),code); info={"kind":"mesh","dims":tuple(int(x) for x in dims),"geometry":struct.unpack_from("<i",buf,meta+72*n)[0]}
            elif blocktype==PLAIN:
                dims=struct.unpack_from(f"<{ndims}i",buf,meta+72); mesh_id=_text(buf,meta+8+IDLEN,IDLEN); stagger=struct.unpack_from("<i",buf,meta+72+4*ndims)[0]; value=_payload(buf,data,code,dims); info={"kind":"variable","mesh_id":mesh_id,"stagger":int(stagger),"dims":tuple(int(x) for x in dims)}
            elif blocktype==CONSTANT:
                fmt={1:"<i",2:"<q",3:"<f",4:"<d",7:"<B"}[code]; value=(np.array([struct.unpack_from(fmt,buf,meta)[0]]),(),code); info={"kind":"constant"}
            elif blocktype==ARRAY:
                dims=struct.unpack_from(f"<{ndims}i",buf,meta); value=_payload(buf,data,code,dims)
            elif blocktype==CPU_SPLIT:
                dims=struct.unpack_from(f"<{ndims}i",buf,meta+4); value=_payload(buf,data,code,(sum(dims),))
        if value is not None: out[name]=(value[0],value[1],value[2],info); ids[block_id]=name
        if nxt<=loc: break
        loc=nxt
    return {"blocks":out,"ids":ids}
def required(snap,name,source):
    if name not in snap["blocks"]: raise ValueError(f"{source}: required block absent: {name!r}")
    return snap["blocks"][name]
def coordinate_index(snap,name,source):
    arr,dims,code,info=required(snap,name,source)
    if info.get("kind")!="variable": raise ValueError(f"{source}: {name!r} is not a plain physical variable")
    mesh_name=snap["ids"].get(info.get("mesh_id","grid"))
    if mesh_name is None: raise ValueError(f"{source}: variable {name!r} references missing mesh id {info.get('mesh_id')!r}")
    edges,dims_mesh,_,mesh_info=required(snap,mesh_name,source)
    if mesh_info.get("kind")!="mesh" or mesh_info.get("geometry")!=1: raise ValueError(f"{source}: mesh id {mesh_name!r} is not a Cartesian Grid/Grid plain mesh")
    nd=len(dims_mesh); axes=[]; pieces=[]; pos=0
    for n in dims_mesh:
        edge=np.asarray(edges[pos:pos+n],dtype=np.float64); pos+=n
        if edge.size<2 or not np.all(np.isfinite(edge)) or np.any(np.diff(edge)<=0): raise ValueError(f"{source}: non-monotonic physical mesh axis")
        axes.append(edge)
    if pos!=edges.size: raise ValueError(f"{source}: Grid/Grid payload has {edges.size} values, expected {pos}")
    if len(dims)!=nd: raise ValueError(f"{source}: {name!r} dimensions {dims} disagree with mesh {dims_mesh}")
    physical=[]
    for axis,n in enumerate(dims):
        edge=axes[axis]; bit=1<<axis
        center=0.5*(edge[:-1]+edge[1:])
        if int(info.get("stagger",0)) & bit:
            # These EPOCH plain variables omit the upper face while
            # Grid/Grid retains both domain-edge coordinates.
            if len(edge)==n: selected=edge
            elif len(edge)==n+1: selected=edge[:-1]
            else: raise ValueError(f"{source}: {name!r} staggered axis {axis} has {len(edge)} mesh coordinates for data dimension {n}")
        else:
            selected=center
        if len(selected)!=n:
            raise ValueError(f"{source}: {name!r} physical axis {axis} has {len(selected)} coordinates for data dimension {n}")
        physical.append(selected)
    grids=np.meshgrid(*physical,indexing="ij"); flat=[g.ravel(order="F") for g in grids]; order=np.lexsort(tuple(flat))
    canonical=np.asarray(arr,dtype=np.float64).ravel()[order]
    h=hashlib.sha256(); h.update(np.asarray(dims,dtype="<i8").tobytes()); h.update(struct.pack("<i",int(info.get("stagger",0))))
    for axis in physical: h.update(np.asarray(axis,dtype="<f8").tobytes())
    return canonical,{"path":None,"shape":list(dims),"mesh_id":info.get("mesh_id","grid"),"stagger":int(info.get("stagger",0)),"coordinate_hash":h.hexdigest(),"index_order":COORDINATE_ORDER}
def write_f64(value,path):
    arr=np.ascontiguousarray(value,dtype=np.float64)
    if arr.size==0 or not np.all(np.isfinite(arr)): raise ValueError(f"{path}: empty or non-finite observable")
    arr.tofile(path)
def write_count(snap,species,source,path):
    arr=required(snap,"CPU split/"+species,source)[0]
    if arr.size==0 or not np.all(np.isfinite(arr)) or not np.all(arr==np.rint(arr)): raise ValueError(f"{source}: invalid exact count for {species}")
    np.array([int(np.rint(arr).sum())],dtype="<i8").tofile(path)
def write_manifest(out,mode,entries):
    (out/"contract.json").write_text(json.dumps({"coordinate_order":COORDINATE_ORDER,"coordinate_source":"SDF Grid/Grid plain mesh edge coordinates plus each variable's mesh_id/stagger metadata","mode":mode,"graded":"global physical coordinates; rank metadata and ownership are ungraded","coordinate_index":entries,"observables":OBS,"species":SPECIES},indent=2,sort_keys=True)+"\n",encoding="utf-8")
def extract_one(run,out):
    out.mkdir(parents=True,exist_ok=True); entries={}
    for dump in DUMPS:
        source=run/f"{dump:04d}.sdf"; snap=read_sdf(source)
        for block,key in OBS:
            arr,entry=coordinate_index(snap,block,source); path=f"{key}_{dump:04d}.f64"; write_f64(arr,out/path); entry["path"]=path; entry["units"]=UNITS[key]; entries[path]=entry
        write_f64(required(snap,"Total Field Energy in Simulation (J)",source)[0],out/f"field_energy_{dump:04d}.f64")
        for species in SPECIES:
            write_f64(required(snap,f"Total Particle Energy/{species} (J)",source)[0],out/f"particle_energy_{species}_{dump:04d}.f64"); write_count(snap,species,source,out/f"global_count_{species}_{dump:04d}.i8")
        if SPECIES: write_f64(required(snap,"Total Particle Energy in Simulation (J)",source)[0],out/f"particle_energy_total_{dump:04d}.f64")
    write_manifest(out,"single-run",entries)
def extract_layout(serial,ranks,out):
    out.mkdir(parents=True,exist_ok=True); entries={}
    for dump in DUMPS:
        sa=serial/f"{dump:04d}.sdf"; sb=ranks/f"{dump:04d}.sdf"; a,b=read_sdf(sa),read_sdf(sb)
        for block,key in OBS:
            av,ea=coordinate_index(a,block,sa); bv,eb=coordinate_index(b,block,sb)
            if ea["shape"]!=eb["shape"] or ea["coordinate_hash"]!=eb["coordinate_hash"]: raise ValueError(f"{block}: serial/decomposed physical coordinate index differs")
            for label,value in (("serial",av),("ranks",bv),("delta",bv-av)):
                path=f"{label}_{key}_{dump:04d}.f64"; write_f64(value,out/path); entry=dict(ea); entry["path"]=path; entry["units"]=UNITS[key]; entries[path]=entry
        write_f64(required(a,"Total Field Energy in Simulation (J)",sa)[0],out/f"serial_field_energy_{dump:04d}.f64"); write_f64(required(b,"Total Field Energy in Simulation (J)",sb)[0],out/f"ranks_field_energy_{dump:04d}.f64")
    write_manifest(out,"serial-versus-decomposed global physical grids",entries)
def main():
    try:
        if LAYOUT:
            if len(sys.argv)!=4: raise ValueError("usage: extract.py SERIAL_DIR RANKS_DIR OUT_DIR")
            extract_layout(Path(sys.argv[1]),Path(sys.argv[2]),Path(sys.argv[3]))
        else:
            if len(sys.argv)!=3: raise ValueError("usage: extract.py RUN_DIR OUT_DIR")
            extract_one(Path(sys.argv[1]),Path(sys.argv[2]))
    except (OSError,ValueError,struct.error) as exc:
        print(f"extract.py: {exc}",file=sys.stderr); return 1
    return 0
if __name__=="__main__": raise SystemExit(main())
