"""Reproduce immutable model facts from pinned XML; never run a solver."""
import argparse
from pathlib import Path
import mujoco
import numpy as np

parser=argparse.ArgumentParser()
parser.add_argument('--source-dir',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
free=Path(__file__).resolve().parent.name=='unit-free-joint-velocity-limit'
path=args.source_dir/'third_party/mujoco_menagerie'/('unitree_g1/g1.xml' if free else 'universal_robots_ur5e/ur5e.xml')
model=mujoco.MjModel.from_xml_path(str(path))
fields=('jnt_type','jnt_qposadr','jnt_dofadr','jnt_range','jnt_limited','qpos0') if free else (
    'body_parentid','body_pos','body_quat','body_mocapid','body_jntadr','body_jntnum',
    'jnt_type','jnt_pos','jnt_axis','jnt_qposadr','jnt_dofadr','jnt_range','jnt_limited',
    'qpos0','site_bodyid','site_pos','site_quat')
data={key:np.array(getattr(model,key),copy=True) for key in fields}
data['initial_q']=model.key('stand' if free else 'home').qpos.copy()
if free:
    assert model.nq==36 and model.nv==35 and data['jnt_type'][0]==0 and np.all(data['jnt_type'][1:]==3)
else:
    assert model.nq==model.nv==6 and np.all(data['jnt_type']==3)
    data.update(frame_kind=np.array([2],dtype=np.int64),frame_id=np.array([model.site('attachment_site').id],dtype=np.int64))
np.savez(args.out,**data)
