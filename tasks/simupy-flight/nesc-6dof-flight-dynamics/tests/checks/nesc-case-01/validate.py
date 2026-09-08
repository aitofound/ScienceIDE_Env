"""Physical comparison; reads check policy and oracle/candidate results only."""
import argparse
import json
from pathlib import Path
import numpy as np


def angle(a, b):
    an, bn = np.linalg.norm(a, axis=1), np.linalg.norm(b, axis=1)
    if np.any(an == 0) or np.any(bn == 0):
        raise ValueError("Zero quaternion cannot represent a rotation")
    a = a / an[:, None]
    b = b / bn[:, None]
    delta = np.minimum(np.linalg.norm(a-b, axis=1), np.linalg.norm(a+b, axis=1))
    return 4*np.arcsin(np.clip(delta/2, 0, 1))


def momentum(q, h):
    # Scalar-first active body-to-inertial quaternion, independently evaluated.
    v, w = q[:, 1:], q[:, :1]
    return h + 2*np.cross(v, np.cross(v, h) + w*h)


def validate(reference, candidate, rubric, check):
    failures, details = [], {}
    def bound(name, error, limit):
        error = np.asarray(error)
        maximum = float(np.max(np.abs(error)))
        fraction = float(np.max(np.abs(error)/np.asarray(limit)))
        details[name] = dict(max_abs_error=maximum, rms_error=float(np.sqrt(np.mean(error**2))), final_max_abs_error=float(np.max(np.abs(error[-1]))) if error.ndim else maximum, bound_fraction=fraction)
        if not np.isfinite(fraction) or fraction > 1:
            failures.append(name + ': outside physical bound')
    try:
        with np.load(reference/'physical.npz', allow_pickle=False) as f:
            r = {k:f[k] for k in f.files}
        with np.load(candidate/'physical.npz', allow_pickle=False) as f:
            c = {k:f[k] for k in f.files}
        if set(r) != set(c):
            raise ValueError('Physical output field set differs from oracle')
        for key in r:
            if r[key].shape != c[key].shape or not r[key].size or not np.all(np.isfinite(r[key])) or not np.all(np.isfinite(c[key])):
                raise ValueError(key + ': shape, emptiness or finiteness failure')
        comp = rubric['comparison']
        if rubric['kind'] == 'model':
            expected = np.asarray(json.loads((check/'official-model.json').read_text())['outputs'])
            if c['values'].shape != expected.shape:
                raise ValueError('Official model output shape mismatch')
            bound('candidate_vs_oracle', c['values']-r['values'], comp['model_atol']+comp['model_rtol']*np.abs(r['values']))
            bound('official_model', c['values']-expected, comp['official_atol']+comp['official_rtol']*np.abs(expected))
        else:
            n = len(r['time'])
            if r['state'].shape != (n,13) or c['local_q'].shape != (n,4) or c['environment'].shape != (n,4):
                raise ValueError('Wrong physical state layout')
            if np.any(np.diff(c['time']) <= 0) or not np.allclose(c['time'],r['time'],rtol=0,atol=1e-12):
                raise ValueError('Fixed physical sampling times differ')
            # Unit-specific comparisons, no relative tolerance on Earth-radius position.
            for name, sl in [('position_m',slice(0,3)),('velocity_mps',slice(7,10)),('rate_radps',slice(10,13))]:
                bound(name,c['state'][:,sl]-r['state'][:,sl],comp[name])
            bound('inertial_attitude_rad',angle(c['state'][:,3:7],r['state'][:,3:7]),comp['attitude_rad'])
            bound('local_attitude_rad',angle(c['local_q'],r['local_q']),comp['attitude_rad'])
            for key, sl in [('state',slice(3,7)),('local_q',slice(None))]:
                bound(key+'_unit_norm',np.linalg.norm(c[key][:,sl],axis=1)-1,comp['quaternion_norm'])
            bound('raw_quaternion_norm',c['quaternion_norm_error'],comp['quaternion_norm'])
            bound('environment',c['environment']-r['environment'],np.asarray(comp['environment_atol'])+comp['environment_rtol']*np.abs(r['environment']))
            if 'trim' in c:
                bound('trim',c['trim']-r['trim'],np.asarray(comp['trim_atol']))
            if rubric['case']=='02':
                inertia=np.asarray(rubric['inertia_diagonal_kg_m2'])
                omega=c['state'][:,10:13]
                hb=omega*inertia
                energy=.5*np.sum(omega*hb,axis=1)
                hi=momentum(c['state'][:,3:7],hb)
                bound('rotational_energy_drift',(energy-energy[0])/abs(energy[0]),comp['energy_relative'])
                bound('inertial_momentum_drift',np.linalg.norm(hi-hi[0],axis=1)/np.linalg.norm(hi[0]),comp['momentum_relative'])
            # Anchor the pristine nominal oracle to frozen NESC data, never candidate source.
            # Candidate equivalence and independent invariants are mandatory above.
            # A numerical-noise variant is never treated as the published nominal case.
            with np.load(check/'nesc-reference.npz',allow_pickle=False) as f:
                external={k:f[k] for k in f.files}
            target = external['time']
            mask=(target>=r['time'][0])&(target<=r['time'][-1])
            target=target[mask]
            if len(target)<2 or abs(target[-1]-r['time'][-1])>0.100001:
                raise ValueError('Insufficient external reference coverage')
            # Runner exports the NASA observation grid (100 Hz, or 10 Hz for Case 11); use physical time identity explicitly.
            indices=np.searchsorted(r['time'],target)
            indices=np.clip(indices,0,n-1)
            left=np.maximum(indices-1,0)
            indices=np.where(abs(r['time'][left]-target)<abs(r['time'][indices]-target),left,indices)
            if np.max(abs(r['time'][indices]-target))>1e-8:
                raise ValueError('Reference timestamps do not coincide with fixed output grid')
            for name,sl in [('position_m',slice(0,3)),('velocity_mps',slice(7,10)),('rate_radps',slice(10,13))]:
                bound('NESC_'+name,r['state'][indices,sl]-external[name][mask],rubric['nesc_bounds'][name])
            bound('NESC_attitude_rad',angle(r['local_q'][indices],external['local_q'][mask]),rubric['nesc_bounds']['attitude_rad'])
    except (ValueError, OSError, KeyError, IndexError, TypeError) as exc:
        failures.append(str(exc))
    worst=max((d['bound_fraction'] for d in details.values()),default=0.)
    comparison_names={'candidate_vs_oracle','position_m','velocity_mps','rate_radps','inertial_attitude_rad','local_attitude_rad','environment','trim'}
    spread=max((d['max_abs_error'] for k,d in details.items() if k in comparison_names),default=0.)
    # Keep the overall gate conservative; expose the separate scientific roles.
    groups = {
        'port_equivalence': comparison_names,
        'independent_physics': {'state_unit_norm','local_q_unit_norm','raw_quaternion_norm','rotational_energy_drift','inertial_momentum_drift'},
        'published_anchor': {k for k in details if k.startswith('NESC_') or k == 'official_model'},
    }
    families = {}
    for family, names in groups.items():
        streams = {k:v for k,v in details.items() if k in names}
        if streams:
            worst_stream = max(streams, key=lambda k: streams[k]['bound_fraction'])
            fraction = streams[worst_stream]['bound_fraction']
            families[family] = dict(bound_fraction=fraction, worst_stream=worst_stream, margin=(1/fraction if fraction else None), streams=list(streams))
    return dict(passed=not failures,policy='pointwise',distance=spread,bound_fraction=worst,comparison_families=families,files=details,reason='all physical comparisons within approved bounds' if not failures else '; '.join(failures))


def main():
    ap=argparse.ArgumentParser()
    for flag in ['reference','candidate','rubric','out']:
        ap.add_argument('--'+flag,required=True)
    a=ap.parse_args()
    rp=Path(a.rubric)
    result=validate(Path(a.reference),Path(a.candidate),json.loads(rp.read_text()),rp.parent)
    Path(a.out).write_text(json.dumps(result,indent=2,allow_nan=False)+'\n')
    print(result['reason'])


if __name__=='__main__':
    main()
