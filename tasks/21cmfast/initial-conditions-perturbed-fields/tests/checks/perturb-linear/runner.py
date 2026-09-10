#!/usr/bin/env python3
"""Realization-safe invariants for 21cmFAST's direct LINEAR branch."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from powerbox import get_power
import py21cmfast as p21c
from py21cmfast.wrapper import cfuncs as cf


def array(box, name):
    value = box.get(name)
    return np.asarray(value.value if hasattr(value, "value") else value)


def save(out, name, values):
    np.save(out / name, np.asarray(values, dtype=np.float64), allow_pickle=False)


def sigma8(cfg):
    value = np.float32(cfg.get("sigma8", 0.8102))
    ulps = int(cfg.get("sigma8_perturb_ulps", 0))
    for _ in range(abs(ulps)):
        value = np.nextafter(value, np.float32(np.inf if ulps >= 0 else -np.inf))
    return float(value)


def make_inputs(cfg):
    return p21c.InputParameters(random_seed=int(cfg["seed"]), node_redshifts=()).evolve_input_structs(
        HII_DIM=int(cfg["hii_dim"]), DIM=int(cfg["dim"]), BOX_LEN=float(cfg["box_len"]),
        N_THREADS=int(cfg["threads"]), SIGMA_8=sigma8(cfg), SOURCE_MODEL="E-INTEGRAL",
        USE_EXP_FILTER=False, CELL_RECOMB=False, USE_TS_FLUCT=False,
        USE_UPPER_STELLAR_TURNOVER=False, PERTURB_ALGORITHM="LINEAR",
        PERTURB_ON_HIGH_RES=False, KEEP_3D_VELOCITIES=True,
    )


def radial_power(values, box_len):
    values = np.asarray(values, dtype=np.float64)
    power, k, _, counts = get_power(
        values-values.mean(), boxlength=box_len, bins=18, log_bins=True,
        bins_upto_boxlen=True, dimensionless=False, ignore_zero_mode=True,
        remove_shotnoise=False, return_sumweights=True, nthreads=False,
    )
    return tuple(np.asarray(x, dtype=np.float64) for x in (power, k, counts))


def scale_bands(power, k, counts, n, box_len):
    nyquist = np.pi*n/box_len
    usable = np.isfinite(power) & np.isfinite(k) & (counts >= 32)
    result = {"low": usable&(k<=.20*nyquist), "mid": usable&(k>.20*nyquist)&(k<=.65*nyquist), "high": usable&(k>.65*nyquist)}
    if any(not mask.any() for mask in result.values()): raise RuntimeError("empty scale band")
    return result


def save_spectrum(out, stem, values, box_len):
    power,k,counts = radial_power(values,box_len)
    use = scale_bands(power,k,counts,values.shape[0],box_len)
    metadata=[]
    for band in ("low","mid","high"):
        mask=use[band]
        save(out,f"power-{stem}-{band}.npy",[np.average(power[mask],weights=counts[mask])])
        metadata.append([k[mask].min(),k[mask].max(),counts[mask].sum()])
    save(out,f"spectrum-{stem}-bands.npy",metadata)


def save_vector_spectrum(out,vectors,box_len):
    spectra=[radial_power(v,box_len) for v in vectors]
    power=sum(x[0] for x in spectra); k,counts=spectra[0][1:]
    use=scale_bands(power,k,counts,vectors[0].shape[0],box_len); metadata=[]
    for band in ("low","mid","high"):
        mask=use[band]
        save(out,f"power-velocity-total-{band}.npy",[np.average(power[mask],weights=counts[mask])])
        metadata.append([k[mask].min(),k[mask].max(),counts[mask].sum()])
    save(out,"spectrum-velocity-total-bands.npy",metadata)


def moments(values):
    values=np.asarray(values,dtype=np.float64)
    return [values.mean(),values.std(),np.sqrt(np.mean(values**2)),*np.quantile(values,[.01,.1,.5,.9,.99])]


def mode_relation(first,second,box_len):
    n=first.shape[0]; k=2*np.pi*np.fft.fftfreq(n,d=box_len/n)
    kmag=np.sqrt(k[:,None,None]**2+k[None,:,None]**2+k[None,None,:]**2); nyquist=np.pi*n/box_len
    first_k=np.fft.fftn(np.asarray(first,dtype=np.float64)-np.mean(first)); second_k=np.fft.fftn(np.asarray(second,dtype=np.float64)-np.mean(second)); result=[]
    for low,high in ((0.,.20),(.20,.65)):
        mask=(kmag>low*nyquist)&(kmag<=high*nyquist)
        aa=np.sum(abs(first_k[mask])**2); bb=np.sum(abs(second_k[mask])**2); cross=np.vdot(first_k[mask],second_k[mask])
        result.extend([abs(cross)/np.sqrt(aa*bb),cross.real/aa])
    return result


def divergence(vectors,box_len):
    n=vectors[0].shape[0]; k=2*np.pi*np.fft.fftfreq(n,d=box_len/n)
    vx,vy,vz=[np.fft.fftn(np.asarray(v,dtype=np.float64)) for v in vectors]
    return np.fft.ifftn(1j*(k[:,None,None]*vx+k[None,:,None]*vy+k[None,None,:]*vz)).real


def longitudinal_ratio(vectors,box_len):
    n=vectors[0].shape[0]; k=2*np.pi*np.fft.fftfreq(n,d=box_len/n)
    vx,vy,vz=[np.fft.fftn(np.asarray(v,dtype=np.float64)) for v in vectors]; kx,ky,kz=k[:,None,None],k[None,:,None],k[None,None,:]
    div=kx*vx+ky*vy+kz*vz; curl=abs(ky*vz-kz*vy)**2+abs(kz*vx-kx*vz)**2+abs(kx*vy-ky*vx)**2
    return np.sqrt(np.sum(curl)/np.sum(abs(div)**2))


def run(cfg,out):
    inputs=make_inputs(cfg)
    initial=p21c.compute_initial_conditions(inputs=inputs,write=False)
    perturbed=p21c.perturb_field(redshift=float(cfg["redshift"]),initial_conditions=initial,write=False)
    density=array(perturbed,"density"); velocity=tuple(array(perturbed,f"velocity_{axis}")*1e16 for axis in "xyz"); box_len=float(cfg["box_len"])
    save_spectrum(out,"density",density,box_len); save_vector_spectrum(out,velocity,box_len)
    save(out,"density-summary.npy",moments(density)); save(out,"velocity-magnitude-summary.npy",moments(np.sqrt(sum(v**2 for v in velocity))))
    save(out,"density-mass-summary.npy",[density.mean(),np.mean(1+density)])
    save(out,"density-velocity-relation.npy",mode_relation(density,divergence(velocity,box_len),box_len))
    save(out,"velocity-longitudinal-ratio.npy",[longitudinal_ratio(velocity,box_len)])

    initial_density=array(initial,"lowres_density")
    growth=float(cf.get_growth_factor(inputs=inputs,redshift=float(cfg["redshift"])))
    predicted=growth*initial_density
    clipped_fraction=np.mean(predicted < -1.0 + 1.0e-7)
    predicted=np.maximum(predicted,-1.0+1.0e-7)
    density_residual=np.linalg.norm(density-predicted)/np.linalg.norm(density)
    relation=mode_relation(initial_density,density,box_len)
    save(out,"linear-density-relation.npy",[
        growth,density_residual,clipped_fraction,relation[0],
        abs(relation[1]/growth-1.0),relation[2],abs(relation[3]/growth-1.0)])

    n=density.shape[0]; k=2*np.pi*np.fft.fftfreq(n,d=box_len/n)
    kz=np.broadcast_to(k[None,None,:],density.shape); ksq=k[:,None,None]**2+k[None,:,None]**2+k[None,None,:]**2
    density_k=np.fft.fftn(density); template_k=np.zeros_like(density_k); active=ksq>0
    template_k[active]=1j*kz[active]/ksq[active]*density_k[active]
    template=np.fft.ifftn(template_k).real; vz=velocity[2]
    coefficient=np.vdot(template,vz).real/np.vdot(template,template).real
    residual=np.linalg.norm(vz-coefficient*template)/np.linalg.norm(vz)
    correlation=np.vdot(template,vz).real/(np.linalg.norm(template)*np.linalg.norm(vz))
    vz_k=np.fft.fftn(vz); zero_plane=np.sum(abs(vz_k[:,:,0])**2)/np.sum(abs(vz_k)**2)
    save(out,"linear-velocity-amplitude.npy",[coefficient])
    save(out,"linear-velocity-structure.npy",[residual,correlation,zero_plane,abs(vz.mean())/np.sqrt(np.mean(vz**2))])


def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--input",required=True); parser.add_argument("--out",required=True); parser.add_argument("--dim",type=int); parser.add_argument("--hii-dim",type=int); parser.add_argument("--threads",type=int); args=parser.parse_args()
    cfg=json.loads(Path(args.input).read_text(encoding="utf-8"))
    for key,value in (("dim",args.dim),("hii_dim",args.hii_dim),("threads",args.threads)):
        if value is not None: cfg[key]=value
    if cfg["mode"]!="integration" or cfg["algorithm"]!="LINEAR": raise ValueError("this check requires LINEAR integration")
    out=Path(args.out); out.mkdir(parents=True,exist_ok=True); run(cfg,out)


if __name__=="__main__": main()
