"""Headless adaptation of the pinned TSID biped example for native investigation."""
import argparse
import ast
import json
import sys
import time
import types
from pathlib import Path
import numpy as np
import pinocchio as pin
import pinocchio.visualize
import tsid


def setup(source,urdf,srdf):
    # Keep numerical defaults from the actual editable upstream configuration.
    # Resolve only model paths outside its POSIX absolute-path join bug.
    path=source/'exercizes/talos_conf.py'
    tree=ast.parse(path.read_text())
    tree.body=[node for node in tree.body if not
        (isinstance(node,ast.ImportFrom) and node.module=='example_robot_data.robots_loader')
        and not (isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id in ('urdf','path','srdf') for t in node.targets))]
    ns={}; exec(compile(tree,str(path),'exec'),ns)
    conf=types.SimpleNamespace(**{k:v for k,v in ns.items() if not k.startswith('__')})
    conf.urdf=str(urdf); conf.srdf=str(srdf); conf.path=str(urdf.parents[2])
    import importlib.util
    module_spec=importlib.util.spec_from_file_location("tsid_biped",source/"exercizes/tsid_biped.py")
    module=importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    return conf,module.TsidBiped


def rollout(source,urdf,srdf,steps,case,noise=False,initial_q=None):
    conf,Biped=setup(source,urdf,srdf)
    # Low-priority posture must allow the hand to reach rather than hold the arm.
    conf.w_posture=1e-3; conf.kp_com=40.; conf.kp_contact=100.
    controller=Biped(conf,viewer=False)
    model=controller.model; data=model.createData()
    candidates=[f.name for f in model.frames if 'arm_right' in f.name or 'torso' in f.name or 'gripper_right' in f.name]
    hand='gripper_right_joint'; torso='torso_2_joint'
    if not model.existFrame(hand) or not model.existFrame(torso):
        raise RuntimeError('Discover frames: '+str(candidates))
    q=controller.q.copy() if initial_q is None else np.asarray(initial_q,dtype=float).copy(); v=np.zeros(model.nv)
    names=list(model.names)[2:]
    if noise:
        i=7+names.index('arm_right_2_joint')
        q[i]=np.nextafter(np.nextafter(q[i],np.inf),np.inf)
    pin.framesForwardKinematics(model,data,q)
    initial_hand=data.oMf[model.getFrameId(hand)].copy()
    initial_torso=data.oMf[model.getFrameId(torso)].copy()
    shifts=[np.array([.025,0.,.015]),np.array([.015,-.015,.01]),np.array([.02,.01,-.005])]
    comshifts=[np.array([.005,0.,0.]),np.array([0.,-.003,0.]),np.array([.003,.003,0.])]
    target_hand=initial_hand.copy(); target_hand.translation+=shifts[case] if case<3 else np.zeros(3)
    com0=pin.centerOfMass(model,data,q).copy(); comtarget=com0+(comshifts[case] if case<3 else np.zeros(3))
    handtask=tsid.TaskSE3Equality('task-hand',controller.robot,hand)
    handtask.setKp(100.*np.ones(6)); handtask.setKd(20.*np.ones(6)); handtask.setMask(np.array([1.,1.,1.,0.,0.,0.]))
    handtraj=tsid.TrajectorySE3Constant('hand-target',target_hand)
    handtask.setReference(handtraj.computeNext()); controller.formulation.addMotionTask(handtask,1.,1,0.)
    torsotask=tsid.TaskSE3Equality('task-torso',controller.robot,torso)
    torsotask.setKp(100.*np.ones(6)); torsotask.setKd(20.*np.ones(6)); torsotask.setMask(np.array([0.,0.,0.,1.,1.,1.]))
    torsotraj=tsid.TrajectorySE3Constant('torso-target',initial_torso)
    torsotask.setReference(torsotraj.computeNext()); controller.formulation.addMotionTask(torsotask,1.,1,0.)
    ctraj=tsid.TrajectoryEuclidianConstant('com-target',comtarget)
    controller.comTask.setReference(ctraj.computeNext())
    controller.leftFootTask.setReference(controller.sampleLF); controller.rightFootTask.setReference(controller.sampleRF)
    controller.solver.resize(controller.formulation.nVar,controller.formulation.nEq,controller.formulation.nIn)
    qs=[]; vs=[]; accelerations=[]; torques=[]; forces=[]; status=[]
    start=time.perf_counter()
    for step in range(steps):
        if case==3:
            t=step*conf.dt; omega=np.pi; amplitude=np.array([0.,.005,0.])
            sample=ctraj.computeNext()
            sample.value(com0+amplitude*np.sin(omega*t))
            sample.derivative(amplitude*omega*np.cos(omega*t))
            sample.second_derivative(-amplitude*omega**2*np.sin(omega*t))
            controller.comTask.setReference(sample)
        hqp=controller.formulation.computeProblemData(step*conf.dt,q,v)
        sol=controller.solver.solve(hqp)
        if int(sol.status)!=0: raise RuntimeError(f'Nonoptimal status {sol.status} at {step}')
        dv=np.asarray(controller.formulation.getAccelerations(sol)).copy()
        tau=np.asarray(controller.formulation.getActuatorForces(sol)).copy()
        force=np.array([controller.formulation.getContactForce('contact_lfoot',sol),controller.formulation.getContactForce('contact_rfoot',sol)])
        qs.append(q.copy()); vs.append(v.copy()); accelerations.append(dv); torques.append(tau); forces.append(force); status.append(int(sol.status))
        q,v=controller.integrate_dv(q,v,dv,conf.dt)
    qs.append(q.copy()); vs.append(v.copy())
    elapsed=time.perf_counter()-start
    pin.framesForwardKinematics(model,data,q)
    hp=data.oMf[model.getFrameId(hand)]
    if case==3: comtarget=com0+np.array([0.,.005,0.])*np.sin(np.pi*steps*conf.dt)
    metrics={'case':case,'steps':steps,'runtime_s':elapsed,'nq':model.nq,'nv':model.nv,'hand_error':float(np.linalg.norm(hp.translation-target_hand.translation)),'com_error':float(np.linalg.norm(pin.centerOfMass(model,data,q)-comtarget)),'torso_angle':float(np.linalg.norm(pin.log3(initial_torso.rotation.T@data.oMf[model.getFrameId(torso)].rotation)))}
    payload=dict(q=np.array(qs),v=np.array(vs),dv=np.array(accelerations),tau=np.array(torques),forces=np.array(forces),status=np.array(status),hand_target=target_hand.translation,torso_target=initial_torso.rotation,com_target=comtarget,dt=np.array(conf.dt),joint_names=np.array(names))
    return metrics,payload


