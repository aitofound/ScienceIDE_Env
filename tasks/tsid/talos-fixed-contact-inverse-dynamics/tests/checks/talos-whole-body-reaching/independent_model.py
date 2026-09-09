"""URDF rigid-body calculations using only the standard library and NumPy.

World-coordinate Newton-Euler recursion; free-flyer velocity is body-local,
linear before angular, followed by the explicitly named revolute joints.
This module never imports TSID or Pinocchio.
"""
import xml.etree.ElementTree as ET
from pathlib import Path
import numpy as np


def vec(text, default=(0., 0., 0.)):
    return np.array(default if text is None else [float(x) for x in text.split()])


def skew(x):
    a,b,c=x
    return np.array([[0.,-c,b],[c,0.,-a],[-b,a,0.]])


def exp3(x):
    t=float(np.linalg.norm(x)); k=skew(x)
    a=np.sin(t)/t if t>1e-8 else 1-t*t/6
    b=(1-np.cos(t))/(t*t) if t>1e-8 else .5-t*t/24
    return np.eye(3)+a*k+b*k@k


def quaternion(q):
    xyz=np.asarray(q[:3]); w=q[3]
    return np.eye(3)+2*w*skew(xyz)+2*skew(xyz)@skew(xyz)


def origin(node):
    if node is None: return np.eye(3),np.zeros(3)
    r,p,y=vec(node.get('rpy'))
    return exp3([0,0,y])@exp3([0,p,0])@exp3([r,0,0]),vec(node.get('xyz'))


class Model:
    def __init__(self, urdf, joint_names):
        doc=ET.parse(Path(urdf)).getroot()
        self.names=list(joint_names); self.nv=6+len(self.names)
        self.links={}
        for link in doc.findall('link'):
            inertial=link.find('inertial')
            mass=0.; com=np.zeros(3); inertia=np.zeros((3,3))
            if inertial is not None:
                mass=float(inertial.find('mass').get('value'))
                r,com=origin(inertial.find('origin'))
                s=inertial.find('inertia').attrib
                inertia=r@np.array([[float(s['ixx']),float(s['ixy']),float(s['ixz'])],[float(s['ixy']),float(s['iyy']),float(s['iyz'])],[float(s['ixz']),float(s['iyz']),float(s['izz'])]])@r.T
            self.links[link.get('name')]=(mass,com,inertia)
        self.joints={}; children={}; incoming={}
        for node in doc.findall('joint'):
            name=node.get('name'); kind=node.get('type')
            if kind not in ('fixed','revolute','continuous'): raise ValueError('Unsupported joint '+kind)
            parent=node.find('parent').get('link'); child=node.find('child').get('link')
            r,p=origin(node.find('origin')); axis_node=node.find('axis')
            axis=vec(None if axis_node is None else axis_node.get('xyz'),(1.,0.,0.))
            idx=None if kind=='fixed' else self.names.index(name)
            lim=node.find('limit')
            self.joints[name]=dict(parent=parent,child=child,R=r,p=p,axis=axis,index=idx,effort=None if lim is None else float(lim.get('effort','0')))
            children.setdefault(parent,[]).append(name); incoming[child]=name
        roots=set(self.links)-set(incoming)
        if len(roots)!=1: raise ValueError('Expected one root link')
        self.root=roots.pop(); self.order=[]
        def walk(link):
            for name in sorted(children.get(link,[])):
                self.order.append(name); walk(self.joints[name]['child'])
        walk(self.root)
        self.effort=np.array([self.joints[n]['effort'] for n in self.names])
        self.mass=sum(v[0] for v in self.links.values())

    def evaluate(self,q,v=None,a=None,external=None):
        q=np.asarray(q); v=np.zeros(self.nv) if v is None else np.asarray(v); a=np.zeros(self.nv) if a is None else np.asarray(a)
        r=quaternion(q[3:7]); w=r@v[3:6]
        states={self.root:(r,q[:3],w,r@v[:3],r@a[3:6],r@(a[:3]+np.cross(v[3:6],v[:3])))}
        for name in self.order:
            j=self.joints[name]; rp,pp,wp,vp,alp,ap=states[j['parent']]
            offset=rp@j['p']; rj=rp@j['R']; pj=pp+offset
            wj=wp.copy(); alj=alp.copy()
            idx=j['index']
            if idx is not None:
                axis=rj@j['axis']; wj+=axis*v[6+idx]
                alj+=axis*a[6+idx]+np.cross(wp,axis*v[6+idx])
                rj=rj@exp3(j['axis']*q[7+idx])
            states[j['child']]=(rj,pj,wj,vp+np.cross(wp,offset),alj,ap+np.cross(alp,offset)+np.cross(wp,np.cross(wp,offset)))
        forces={}; moments={}; center=np.zeros(3)
        for link,(mass,local_com,inertia) in self.links.items():
            r,p,w,vel,alpha,acc=states[link]; c=r@local_com; iw=r@inertia@r.T
            f=mass*(acc+np.cross(alpha,c)+np.cross(w,np.cross(w,c))-np.array([0.,0.,-9.81]))
            forces[link]=f; moments[link]=iw@alpha+np.cross(w,iw@w)+np.cross(c,f)
            center+=mass*(p+c)
        for frame,(force,moment) in (external or {}).items():
            link=self.frame_link(frame); r=states[link][0]
            forces[link]-=r@force; moments[link]-=r@moment
        generalized=np.zeros(self.nv)
        for name in reversed(self.order):
            j=self.joints[name]; child,parent=j['child'],j['parent']; idx=j['index']
            if idx is not None:
                generalized[6+idx]=(states[parent][0]@j['R']@j['axis'])@moments[child]
            offset=states[child][1]-states[parent][1]
            forces[parent]+=forces[child]; moments[parent]+=moments[child]+np.cross(offset,forces[child])
        generalized[:3]=states[self.root][0].T@forces[self.root]
        generalized[3:6]=states[self.root][0].T@moments[self.root]
        return generalized,states,center/self.mass

    def frame_link(self,name):
        return self.joints[name]['child'] if name in self.joints else name

    def pose(self,states,name):
        value=states[self.frame_link(name)]
        return value[0],value[1]


def integrate(q,delta):
    q=np.asarray(q); rho=delta[:3]; phi=delta[3:6]; t=np.linalg.norm(phi); k=skew(phi)
    b=(1-np.cos(t))/(t*t) if t>1e-8 else .5-t*t/24
    c=(t-np.sin(t))/(t**3) if t>1e-8 else 1/6-t*t/120
    out=q.copy(); out[:3]+=quaternion(q[3:7])@(np.eye(3)+b*k+c*k@k)@rho
    s=np.sin(t/2)/t if t>1e-8 else .5-t*t/48
    xyz,w=q[3:6],q[6]; dxyz=phi*s; dw=np.cos(t/2)
    out[3:6]=w*dxyz+dw*xyz+np.cross(xyz,dxyz); out[6]=w*dw-xyz@dxyz
    out[3:7]/=np.linalg.norm(out[3:7]); out[7:]+=delta[6:]
    return out
