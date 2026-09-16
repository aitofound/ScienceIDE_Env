"""Author-only fixed-window QP sensitivity and exclusive-time attribution."""
import argparse
import cProfile
import json
import os
from pathlib import Path
import pstats
import runpy
import sys
import time
import numpy as np
import qpsolvers
import mink
import mujoco
import functools

ap=argparse.ArgumentParser()
ap.add_argument('--repository',type=Path,required=True)
ap.add_argument('--component',choices=('quickstart','panda-circle','panda-bank'),required=True)
ap.add_argument('--mode',default='nominal',choices=('nominal','variant'))
ap.add_argument('--out',type=Path,required=True)
ap.add_argument('--profile',action='store_true')
ap.add_argument('--capture-qp',action='store_true')
ap.add_argument('--spans',action='store_true')
a=ap.parse_args()
repo=a.repository.resolve(); leaf=repo/'tasks/mink/mink-constrained-differential-ik'
os.environ['SOURCE_DIR']=str(repo/'code/mink'); sys.dont_write_bytecode=True
a.out.mkdir(parents=True,exist_ok=True)
check=leaf/'tests/checks'/('examples-docs-quickstart' if a.component=='quickstart' else 'examples-arm-panda')
script=check/({'quickstart':'producer.py','panda-circle':'circle/producer.py','panda-bank':'run_bank.py'}[a.component])
argv=['--mode',a.mode] if a.component=='panda-bank' else ['--ic',str(check/'ic'/a.mode)]
sys.argv=[str(script),*argv,'--out',str(a.out/'output')]
captures=[]
original=qpsolvers.solve_problem
def captured(problem,*args,**kwargs):
    result=original(problem,*args,**kwargs)
    if a.capture_qp:
        captures.append({key:None if value is None else np.asarray(value).copy() for key,value in
            {'P':problem.P,'q':problem.q,'G':problem.G,'h':problem.h,'x':result.x,'z':result.z}.items()})
    return result
qpsolvers.solve_problem=captured
span_totals={}
span_stack=[]
def timed(category, function):
    @functools.wraps(function)
    def wrapper(*args, **kwargs):
        frame=[time.perf_counter(),time.process_time(),0.,0.]
        span_stack.append(frame)
        try:
            return function(*args, **kwargs)
        finally:
            wall=time.perf_counter()-frame[0]
            cpu=time.process_time()-frame[1]
            assert span_stack.pop() is frame
            row=span_totals.setdefault(category,{'calls':0,'exclusive_wall_seconds':0.,'exclusive_cpu_seconds':0.})
            row['calls']+=1
            row['exclusive_wall_seconds']+=wall-frame[2]
            row['exclusive_cpu_seconds']+=cpu-frame[3]
            if span_stack:
                span_stack[-1][2]+=wall
                span_stack[-1][3]+=cpu
    return wrapper
if a.spans:
    assert not a.profile
    mink.solve_ik=timed('mink_solve_and_configuration',mink.solve_ik)
    for name in ('update','integrate','integrate_inplace','get_frame_jacobian',
                 'get_transform_frame_to_world','get_transform'):
        if hasattr(mink.Configuration,name):
            setattr(mink.Configuration,name,timed('mink_solve_and_configuration',getattr(mink.Configuration,name)))
    qpsolvers.solve_problem=timed('qp_backend',qpsolvers.solve_problem)
    for name in dir(mujoco):
        function=getattr(mujoco,name)
        if name.startswith(('mj_','mju_')) and callable(function):
            setattr(mujoco,name,timed('mujoco_function_calls',function))
profile=cProfile.Profile(timer=time.process_time)
start=time.perf_counter()
cpu_start=time.process_time()
if a.profile: profile.enable()
runpy.run_path(str(script),run_name='__main__')
if a.profile: profile.disable()
elapsed=time.perf_counter()-start
cpu_elapsed=time.process_time()-cpu_start
summary={'component':a.component,'mode':a.mode,'elapsed_seconds':elapsed,'profiled':a.profile,
         'process_cpu_seconds':cpu_elapsed, 'profile_timer':'time.process_time',
         'kind':'author investigation; timing is never a graded observable','qp_count':len(captures)}
if a.spans:
    assert not span_stack
    wall_used=sum(v['exclusive_wall_seconds'] for v in span_totals.values())
    cpu_used=sum(v['exclusive_cpu_seconds'] for v in span_totals.values())
    assert wall_used<=elapsed and cpu_used<=cpu_elapsed
    summary['exclusive_spans']=span_totals
    summary['unattributed_wall_seconds']=elapsed-wall_used
    summary['unattributed_cpu_seconds']=cpu_elapsed-cpu_used
    summary['attribution_scope']='Non-overlapping nested timers around Mink solve_ik and listed Configuration methods, qpsolvers.solve_problem, and MuJoCo mj_/mju_ function calls. Nested QP/MuJoCo cost is subtracted from Mink. Model loading, other Mink APIs and adapter/import/I/O overhead remain unattributed. Timers add overhead; these are attribution probes, not speedup measurements.'
if captures:
    payload={k:np.stack([row[k] for row in captures]) for k in ('P','q','G','h','x','z') if captures[0][k] is not None}
    np.savez_compressed(a.out/'qp-trace.npz',**payload)
    eigen=np.linalg.eigvalsh(payload['P'])
    summary['hessian_eigenvalue_min']=float(eigen.min())
    summary['hessian_eigenvalue_max']=float(eigen.max())
    summary['condition_number_max']=float(np.max(eigen[:,-1]/eigen[:,0]))
    summary['damping']='source-default 1e-12 for quickstart; source 1e-3 for circle/bank'
if a.profile:
    profile.dump_stats(str(a.out/'profile.pstats'))
    categories={}
    for (file,line,name),(cc,nc,tt,ct,callers) in pstats.Stats(profile).stats.items():
        key=(file+' '+name).replace('\\','/').lower()
        if str(check).replace('\\','/').lower() in key: category='trusted_adapter'
        elif str(Path(mink.__file__).parent).replace('\\','/').lower()+'/' in key or '/src/mink/' in key or '_lie_ops_c' in key: category='mink_python_and_native'
        elif 'mujoco' in key: category='mujoco'
        elif 'qpsolvers' in key or 'daqp' in key: category='qp_backend'
        elif 'numpy' in key or 'scipy' in key: category='numpy_scipy'
        else: category='other_python_and_io'
        categories[category]=categories.get(category,0.)+tt
    summary['exclusive_seconds']=categories
    summary['exclusive_time_sum']=sum(categories.values())
(a.out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf-8')
print(json.dumps(summary,indent=2))
