#!/usr/bin/env python3
"""Read-only GEO order-5 invariant collector.

This is deliberately separate from the suite runner.  It consumes the already
retrieved nominal/variant/altbuild outputs and emits the measured NVA70/200
(and, when present, NVA7/20) envelopes used to review rubric.json.  It never
runs SWMF and never changes a source or shared script.

The rich ionosphere endpoint is the 15-variable ``it...000200.idl`` schema.
The t=0 steady files are a six-variable schema and are reported separately;
those files are never silently combined with the 15-variable endpoint.
"""
from __future__ import annotations

import argparse
import csv
import datetime
import json
import math
import re
from pathlib import Path
from typing import Iterable

import numpy as np

NO_E = re.compile(r"^([+-]?(?:\d+\.?\d*|\.\d+))([+-]\d{2,3})$")
BEGIN = re.compile(r"^BEGIN\s+(\S+)\s+HEMISPHERE\s*$")
DATE0 = (2014, 4, 10, 0, 0, 0, 0)
DATE120 = (2014, 4, 10, 0, 2, 0, 0)
RADIUS_M = 6_378_000.0

ION_FIELDS = [
    "Theta", "Psi", "SigmaH", "SigmaP", "Jr", "Phi", "E-Flux", "Ave-E",
    "RT 1/B", "RT Rho", "RT P", "JouleHeat", "IonNumFlux",
    "conjugate dLat", "conjugate dLon",
]
ION_UNITS = {
    "Theta": "deg", "Psi": "deg", "SigmaH": "mhos", "SigmaP": "mhos",
    "Jr": "microA/m^2", "Phi": "kV", "E-Flux": "W/m2", "Ave-E": "keV",
    "RT 1/B": "1/T", "RT Rho": "kg/m^3", "RT P": "Pa",
    "JouleHeat": "mW/m2", "IonNumFlux": "/cm2/s", "conjugate dLat": "deg",
    "conjugate dLon": "deg",
}
ION_AGG_FIELDS = ["SigmaH", "SigmaP", "Jr", "Phi", "E-Flux", "Ave-E", "JouleHeat", "IonNumFlux"]
ION_STABLE_FIELDS = ["RT 1/B", "RT Rho", "RT P", "conjugate dLat", "conjugate dLon"]

FILE_MAP = {
    "log.log": ("GM_IO2", "IO2", "log_e20140410-000200.log"),
    "magnetometers.mag": ("GM_IO2", "IO2", "magnetometers_e20140410-000200.mag"),
    "geoindex.log": ("GM_IO2", "IO2", "geoindex_e20140410-000200.log"),
    "mag_grid_global.out": ("GM_IO2", "IO2", "mag_grid_global_e20140410-000200.out"),
    "ie.log": ("IE_ionosphere", "ionosphere", "IE_t140410_000200.log"),
    "ionosphere.idl": ("IE_ionosphere", "ionosphere", "it140410_000200_000.idl"),
}


def real(token: str) -> float | None:
    token = token.replace("D", "E").replace("d", "e")
    try:
        value = float(token)
    except ValueError:
        m = NO_E.match(token)
        if not m:
            return None
        try:
            value = float(m.group(1) + "E" + m.group(2))
        except ValueError:
            return None
    return value if math.isfinite(value) else value


def row(line: str):
    values = [real(t) for t in line.split()]
    if not values or any(v is None for v in values):
        return None
    return values


def table(path: Path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    rows, started, header = [], False, None
    for lineno, line in enumerate(lines, 1):
        s = line.strip()
        if not s:
            continue
        values = row(s)
        if values is None:
            if started:
                raise ValueError(f"{path}: nonnumeric line {lineno} after table start")
            header = s.split()
            continue
        started = True
        rows.append(values)
    if not rows or header is None:
        raise ValueError(f"{path}: missing header or numeric rows")
    widths = {len(x) for x in rows}
    if len(widths) != 1:
        raise ValueError(f"{path}: ragged rows {sorted(widths)}")
    a = np.asarray(rows, dtype=np.float64)
    if not np.all(np.isfinite(a)):
        raise ValueError(f"{path}: nonfinite table values")
    return header, a


def endpoint_rows(header: list[str], values: np.ndarray, when: tuple[int, ...]):
    first = header[0].lower()
    if first == "t":
        # IE uses seconds as the physical key.  The last t=0 row is the final
        # steady-state solve record, not an intermediate row to intersect.
        sec = (datetime.datetime(*when[:6]) - datetime.datetime(*DATE0[:6])).total_seconds()
        hit = values[np.isclose(values[:, 0], sec, rtol=0.0, atol=1e-10)]
        if hit.size == 0:
            raise ValueError(f"table has no exact t={sec:g} endpoint")
        return hit[-1:]
    if first not in {"it", "nstep"}:
        raise ValueError(f"unexpected table key {header[0]!r}")
    # physical date/time follows adaptive bookkeeping.  station, if present,
    # follows the seven time columns and is retained in the selected rows.
    key = np.asarray(when, dtype=float)
    hit = values[np.all(values[:, 1:8] == key, axis=1)]
    if hit.size == 0:
        raise ValueError(f"table has no exact endpoint {when}")
    return hit


def _header_and_shape(lines: list[str], path: Path):
    begins = [i for i, x in enumerate(lines) if BEGIN.match(x.strip())]
    if len(begins) != 2:
        raise ValueError(f"{path}: expected two hemisphere blocks, got {len(begins)}")
    section = None
    fields, units, shape = [], {}, {}
    time_sim = None
    for lineno, line in enumerate(lines[:begins[0]], 1):
        s = line.strip()
        if not s:
            continue
        if s in {"NUMERICAL VALUES", "TIME", "SIMULATION", "DIPOLE TILT", "TITLE", "VARIABLE LIST"}:
            section = s
            continue
        if section == "VARIABLE LIST":
            m = re.match(r"^\s*\d+\s+(.+?)\s+\[([^\]]+)\]\s*$", s)
            if m:
                fields.append(m.group(1)); units[m.group(1)] = m.group(2)
            continue
        if section in {"NUMERICAL VALUES", "TIME", "SIMULATION", "DIPOLE TILT"}:
            toks = s.split(); value = real(toks[0])
            if value is None:
                raise ValueError(f"{path}: bad header line {lineno}: {s!r}")
            if section == "NUMERICAL VALUES" and len(toks) > 1:
                shape[toks[1]] = int(round(value))
            if section == "SIMULATION" and len(toks) > 1 and toks[1] == "Time_Simulation":
                time_sim = float(value)
    for n in ("nvars", "nTheta", "nPhi"):
        if n not in shape: raise ValueError(f"{path}: no {n}")
    if time_sim is None:
        raise ValueError(f"{path}: no Time_Simulation")
    return begins, fields, units, shape, time_sim


def ionosphere(path: Path):
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    begins, fields, units, shape, time_sim = _header_and_shape(lines, path)
    nt, np_, nv = shape["nTheta"], shape["nPhi"], shape["nvars"]
    blocks, names = [], []
    for bi, start in enumerate(begins):
        stop = begins[bi + 1] if bi + 1 < len(begins) else len(lines)
        m = BEGIN.match(lines[start].strip()); names.append(m.group(1).upper())
        vals = []
        for lineno, line in enumerate(lines[start + 1:stop], start + 2):
            s = line.strip()
            if not s: continue
            x = row(s)
            if x is None: raise ValueError(f"{path}: nonnumeric block line {lineno}")
            vals.append(x)
        if len(vals) != nt * np_ or {len(x) for x in vals} != {nv}:
            raise ValueError(f"{path}: block shape {len(vals)}x{sorted({len(x) for x in vals})}")
        # SWMF records are phi-major; use [theta,phi,var] for area weights.
        blocks.append(np.asarray(vals, dtype=np.float64).reshape(np_, nt, nv).transpose(1, 0, 2))
    data = np.stack(blocks)
    if not np.all(np.isfinite(data)):
        raise ValueError(f"{path}: nonfinite ionosphere data")
    return {"data": data, "fields": fields, "units": units, "shape": [2, nt, np_, nv], "time": time_sim, "blocks": names}


def grid(path: Path):
    lines = [x.strip() for x in path.read_text(encoding="utf-8", errors="replace").splitlines() if x.strip()]
    if len(lines) < 5: raise ValueError(f"{path}: truncated grid")
    head = [real(x) for x in lines[1].split()]
    if len(head) < 5 or any(x is None for x in head): raise ValueError(f"{path}: bad IDL header")
    nstep, ts, ndim, nparam, nvar = head[:5]
    dims = [real(x) for x in lines[2].split()]
    if any(x is None for x in dims) or len(dims) != int(abs(ndim)): raise ValueError(f"{path}: bad dimensions")
    pos = 3
    if int(nparam) > 0: pos += 1
    names = lines[pos].split(); pos += 1
    nrow = math.prod(int(round(x)) for x in dims)
    vals = [row(x) for x in lines[pos:pos+nrow]]
    if len(vals) != nrow or any(x is None for x in vals): raise ValueError(f"{path}: bad grid point rows")
    data = np.asarray(vals, dtype=np.float64)
    if data.shape[1] != int(abs(ndim)) + int(nvar): raise ValueError(f"{path}: width {data.shape[1]}")
    if not np.all(np.isfinite(data)): raise ValueError(f"{path}: nonfinite grid")
    return {"time": float(ts), "shape": [int(round(x)) for x in dims], "names": names, "data": data, "ndim": int(abs(ndim)), "nvar": int(nvar)}


def case_path(raw: Path, case: str, rel: str) -> Path:
    sub = FILE_MAP[rel]
    return raw / f"{case}-{sub[0]}" / sub[1] / sub[2]


def load_case(raw: Path, case: str):
    out = {}
    for rel in ("log.log", "magnetometers.mag", "geoindex.log", "ie.log"):
        p = case_path(raw, case, rel); h, a = table(p); out[rel] = {"header": h, "data": a}
    out["ionosphere.idl"] = ionosphere(case_path(raw, case, "ionosphere.idl"))
    out["mag_grid_global.out"] = grid(case_path(raw, case, "mag_grid_global.out"))
    return out


def area_weights(theta: np.ndarray, south: bool, cap: bool):
    # Theta are cell centres (the first polar value is a tiny pole-safe centre,
    # then 0.5-degree centres).  Midpoint edges and the unique Psi=0..359
    # columns avoid double counting the 360-degree duplicate.
    t = np.radians(theta)
    edges = np.empty(theta.size + 1)
    edges[1:-1] = (t[:-1] + t[1:]) / 2.0
    edges[0] = 0.0 if not south else np.pi / 2.0
    edges[-1] = np.pi / 2.0 if not south else np.pi
    if cap:
        lo, hi = ((0.0, 30.0) if not south else (150.0, 180.0))
        edges = np.clip(edges, np.radians(lo), np.radians(hi))
    domega = np.abs(np.cos(edges[:-1]) - np.cos(edges[1:])) * (2.0 * np.pi / 360.0)
    return domega * (RADIUS_M ** 2)


def weighted_stats(values: np.ndarray, weights: np.ndarray):
    x = values[:, :360].reshape(-1)
    w = np.broadcast_to(weights[:, None], values[:, :360].shape).reshape(-1)
    keep = w > 0
    x, w = x[keep], w[keep]
    total = float(w.sum())
    mean = float(np.sum(x*w) / total)
    std = float(np.sqrt(np.sum((x-mean)**2*w) / total))
    order = np.argsort(x); xs, ws = x[order], w[order]
    cdf = np.cumsum(ws) / total
    def q(p): return float(xs[np.searchsorted(cdf, p, side="left")])
    return {"integral": float(np.sum(x*w)), "mean": mean, "std": std,
            "p05": q(.05), "p50": q(.50), "p95": q(.95),
            "min": float(x.min()), "max": float(x.max()), "area_m2": total}


def ion_metrics(parsed):
    if parsed["fields"] != ION_FIELDS or parsed["shape"] != [2,181,361,15]:
        raise ValueError(f"rich endpoint schema differs: {parsed['fields']!r} {parsed['shape']}")
    for f,u in ION_UNITS.items():
        if parsed["units"].get(f) != u: raise ValueError(f"unit {f}: {parsed['units'].get(f)!r}, expected {u!r}")
    data = parsed["data"]; out = {}
    for hi, hemi, south in ((0,"north",False),(1,"south",True)):
        theta = data[hi,:,0,0]
        for domain, cap in (("hemisphere",False),("polar_cap",True)):
            w = area_weights(theta,south,cap)
            for field in ION_AGG_FIELDS:
                fi = ION_FIELDS.index(field); st = weighted_stats(data[hi,:,:,fi],w)
                for metric in ("integral","mean","std","p05","p50","p95"):
                    out[f"{hemi}|{domain}|{field}|{metric}"] = {"value":st[metric], "units":ION_UNITS[field] + (" m^2" if metric=="integral" else ""), "area_m2":st["area_m2"]}
            fi = ION_FIELDS.index("Phi"); st = weighted_stats(data[hi,:,:,fi],w)
            if domain == "polar_cap":
                out[f"{hemi}|{domain}|Phi|range"] = {"value":st["max"]-st["min"], "units":"kV", "area_m2":st["area_m2"]}
    return out


def endpoint_stat(values: np.ndarray):
    x = np.asarray(values, dtype=np.float64).reshape(-1)
    if x.size == 0 or not np.all(np.isfinite(x)): raise ValueError("empty/nonfinite endpoint")
    return {"mean":float(x.mean()), "min":float(x.min()), "max":float(x.max())}


def failed_table_stats(case: dict, when):
    out = {}
    bookkeeping={"it","nstep","year","mo","dy","hr","mn","sc","msc"}
    for rel, stable in {
        "log.log": {"mx","my","mz"},
        "geoindex.log": {"Kp","K_12","K_13","K_14","K_15","K_16","K_17","K_18","K_19","K_20","K_21","K_22","K_23","K_00","K_01","K_02","K_03","K_04","K_05","K_06","K_07","K_08","K_09","K_10","K_11"},
        "magnetometers.mag": {"station","X","Y","Z"},
    }.items():
        h,a = case[rel]["header"], case[rel]["data"]; rows = endpoint_rows(h,a,when)
        offset = 1
        ignored={x.lower() for x in (bookkeeping | stable)}
        for j,name in enumerate(h[offset:]):
            if name.lower() in ignored: continue
            out[f"{rel}|{name}"] = endpoint_stat(rows[:,offset+j])
    return out


def ie_stats(case: dict, when):
    h,a = case["ie.log"]["header"], case["ie.log"]["data"]
    rows = endpoint_rows(h,a,when)
    ignored={"t","year","mo","dy","hr","mn","sc","msc"}
    return {name:endpoint_stat(rows[:,j]) for j,name in enumerate(h) if name.lower() not in ignored}


def grid_stats(case: dict, when):
    g=case["mag_grid_global.out"]
    if not math.isclose(g["time"],120.0,abs_tol=1e-12): raise ValueError("grid endpoint is not t=120")
    out={}
    for j,name in enumerate(g["names"][g["ndim"]:],g["ndim"]): out[name]=endpoint_stat(g["data"][:,j])
    return out


def stable_screen(cases):
    specs={
      "log.log":({"mx","my","mz"},2e-8,1e-5),
      "magnetometers.mag":({"station","X","Y","Z"},4e-5,2e-4),
      "geoindex.log":({"Kp","K_12","K_13","K_14","K_15","K_16","K_17","K_18","K_19","K_20","K_21","K_22","K_23","K_00","K_01","K_02","K_03","K_04","K_05","K_06","K_07","K_08","K_09","K_10","K_11"},1e-8,1e-5),
    }
    out=[]
    for rel,(wanted,atol,rtol) in specs.items():
      hn,an=cases["nominal"][rel]["header"],cases["nominal"][rel]["data"]
      for c in ("variant","altbuild"):
       hc,ac=cases[c][rel]["header"],cases[c][rel]["data"]
       for when in (DATE120,):
        rn,rc=endpoint_rows(hn,an,when),endpoint_rows(hc,ac,when)
        for j,name in enumerate(hn[1:],1):
         if name not in wanted: continue
         err=np.abs(rc[:,j]-rn[:,j]); bound=atol+rtol*np.abs(rn[:,j])
         out.append({"file":rel,"field":name,"case":c,"time":when[5],"max_abs":float(err.max()),"max_scaled":float((err/bound).max()) if np.any(bound) else 0.0,"count":int(err.size)})
    # Rich ionosphere coordinates and stable fields at 18.
    n=cases["nominal"]["ionosphere.idl"]; nd=cases["variant"]["ionosphere.idl"]; na=cases["altbuild"]["ionosphere.idl"]
    for c,d in (("variant",nd),("altbuild",na)):
      for f in ["Theta","Psi"]+ION_STABLE_FIELDS:
       j=ION_FIELDS.index(f); base=n["data"][:,:,:,j]; cur=d["data"][:,:,:,j]
       err=np.abs(cur-base); at,rt=(7e-5,4e-4)
       out.append({"file":"ionosphere.idl","field":f,"case":c,"time":120,"max_abs":float(err.max()),"max_scaled":float((err/(at+rt*np.abs(base))).max()),"count":int(err.size)})
    gn=cases["nominal"]["mag_grid_global.out"]
    for c in ("variant","altbuild"):
      gc=cases[c]["mag_grid_global.out"]
      for j,name in enumerate(gn["names"][:gn["ndim"]]):
       base=gn["data"][:,j]; cur=gc["data"][:,j]; err=np.abs(cur-base); at,rt=2e-4,2e-4
       out.append({"file":"mag_grid_global.out","field":name,"case":c,"time":120,"max_abs":float(err.max()),"max_scaled":float((err/(at+rt*np.abs(base))).max()),"count":int(err.size)})
    return out


def metric_envelope(records):
    # records: case -> metric key -> {value,units}; generate the measured
    # larger NV/NA separation and an asymmetry-derived factor per metric.
    n,v,a=records["nominal"],records["variant"],records["altbuild"]
    rows=[]
    for key in sorted(n):
      nv=abs(v[key]["value"]-n[key]["value"]); na=abs(a[key]["value"]-n[key]["value"]); larger=max(nv,na); asym=abs(nv-na)
      factor=(1.0+asym/larger) if larger else 1.0
      rows.append({"key":key,"units":n[key]["units"],"nominal":n[key]["value"],"variant":v[key]["value"],"altbuild":a[key]["value"],"nv_separation":nv,"na_separation":na,"larger_separation":larger,"asymmetry":asym,"factor":factor,"bound":larger*factor,"area_m2":n[key]["area_m2"]})
    return rows


def stat_envelope(allstats):
    # allstats case -> (kind,key) -> endpoint/time -> statistic dict
    n,v,a=allstats["nominal"],allstats["variant"],allstats["altbuild"]
    rows=[]
    for key in sorted(n):
      for metric in ("mean","min","max"):
       nv=abs(v[key][metric]-n[key][metric]); na=abs(a[key][metric]-n[key][metric]); larger=max(nv,na); asym=abs(nv-na); factor=1+asym/larger if larger else 1.0
       rows.append({"key":key,"metric":metric,"nominal":n[key][metric],"variant":v[key][metric],"altbuild":a[key][metric],"nv_separation":nv,"na_separation":na,"larger_separation":larger,"asymmetry":asym,"factor":factor,"bound":larger*factor})
    return rows


def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--raw-root",required=True); ap.add_argument("--out",required=True)
    a=ap.parse_args(); raw=Path(a.raw_root); out=Path(a.out); out.parent.mkdir(parents=True,exist_ok=True)
    runs={"nominal":load_case(raw,"nominal"),"variant":load_case(raw,"variant"),"altbuild":load_case(raw,"altbuild")}
    # Endpoint key/finite coverage evidence for every existing NVA output.
    coverage={}
    for c,d in runs.items():
      coverage[c]={}
      for rel in ("log.log","magnetometers.mag","geoindex.log","ie.log"):
       h,x=d[rel]["header"],d[rel]["data"]
       endpoints={}
       for label,when in (("0",DATE0),("18",DATE120)):
        try: endpoints[label]=int(len(endpoint_rows(h,x,when)))
        except ValueError: endpoints[label]=0
       coverage[c][rel]={"rows":int(len(x)),"finite":bool(np.all(np.isfinite(x))),"endpoints":endpoints}
      q=d["ionosphere.idl"]; coverage[c]["ionosphere.idl"]={"shape":q["shape"],"finite":bool(np.all(np.isfinite(q["data"]))),"time":q["time"],"fields":q["fields"],"units":q["units"]}
      q=d["mag_grid_global.out"]; coverage[c]["mag_grid_global.out"]={"shape":q["shape"],"finite":bool(np.all(np.isfinite(q["data"]))),"time":q["time"],"names":q["names"]}
    im={c:ion_metrics(d["ionosphere.idl"]) for c,d in runs.items()}
    aggregate=metric_envelope(im)
    # Failed endpoint statistics, with one shared bound per field/statistic
    # applied independently at both physical endpoints.
    ep={}
    for c,d in runs.items():
      ep[c]={}
      for when,label in ((DATE0,"0"),(DATE120,"18")):
       for k,v in failed_table_stats(d,when).items(): ep[c][f"{k}|t={label}"] = v
       # IE_t... is an integrated summary written through 15 s in the
       # retrieved run.  Keep its exact available t=0 endpoint; do not
       # relabel its t=15 row as the rich ionosphere's exact t=120 frame.
       try:
        for k,v in ie_stats(d,when).items(): ep[c][f"ie.log|{k}|t={label}"] = v
       except ValueError:
        pass
      for k,v in grid_stats(d,DATE120).items(): ep[c][f"mag_grid_global.out|{k}|t=120"] = v
    endpoint_bounds=stat_envelope(ep)
    result={"schema":"swpc-order5-physical-invariants-v1","radius_m":RADIUS_M,"area_definition":"dA=R^2*|cos(theta_left)-cos(theta_right)|*(2*pi/360), midpoint theta edges, unique Psi=0..359; source integrals retain source-unit*m^2","source_units":ION_UNITS,"coverage":coverage,"stable_screen":stable_screen(runs),"aggregate_envelopes":aggregate,"endpoint_envelopes":endpoint_bounds,"aggregate_nominal_t18":{r["key"]:r["nominal"] for r in aggregate},"note":"NVA means nominal/variant/altbuild; bounds use larger measured separation times factor=1+abs(NV-NA)/larger, independently per observable, not a universal multiplier. Rich ionosphere is exact 120 s 15-variable schema; non-endpoint steady schema is six-variable and is not conflated."}
    out.write_text(json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    # Compact CSV is useful for review and avoids asking a reader to inspect JSON.
    with out.with_suffix(".csv").open("w",newline="",encoding="utf-8") as f:
      w=csv.DictWriter(f,fieldnames=["key","units","nominal","variant","altbuild","nv_separation","na_separation","larger_separation","asymmetry","factor","bound","area_m2"]); w.writeheader(); w.writerows(aggregate)
    print(json.dumps({"output":str(out),"aggregate_rows":len(aggregate),"endpoint_rows":len(endpoint_bounds),"all_finite":all(x.get("finite",True) for c in coverage.values() for x in c.values()),"rich_shape":coverage["nominal"]["ionosphere.idl"]["shape"]},sort_keys=True))

if __name__ == "__main__": main()
