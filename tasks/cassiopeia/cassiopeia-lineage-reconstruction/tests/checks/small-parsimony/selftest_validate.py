#!/usr/bin/env python3
"""可移植 validator 自测：只需标准库与 NumPy。

人工两叶树 p -> x,y 的观测为 C,A；直接按边定义构造数学证书。
不读取官方 nominal 输出、生产 source、作者 HOME 或外部 evidence。
运行：python3 selftest_validate.py
"""
import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zipfile
import zlib
import struct
import numpy as np

VALIDATOR = Path(__file__).resolve().with_name('validate.py')


def specimen(kind):
    arrays = dict(node_ids=np.array(['p','x','y']), leaf_ids=np.array(['','x','y']), edges=np.array([['p','x'],['p','y']]), assignment=np.array(['A','C','A']), score=np.array([1], dtype=np.int64), states=np.array(['A','C','G']))
    if kind == 'sets':
        arrays['root_state_membership'] = np.array([[1,1,0],[0,1,0],[1,0,0]], dtype=np.bool_)
    if kind == 'count':
        arrays.update(row_states=np.array(['A','C','G']), column_states=np.array(['A','C','G']), transitions=np.array([[1,1,0],[1,1,0],[0,0,0]], dtype=np.float64))
    return arrays


class Contract(unittest.TestCase):
    def check(self, transform=None, expected=True, bad_archive=None, broken_reference=False,
              archive_side='candidate', examples=None):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); reference=root/'reference'; candidate=root/'candidate'
            reference.mkdir(); candidate.mkdir()
            files=[]
            if examples is None:
                examples=[]
                for kind in ['assignment','sets','count']:
                    spec=dict(path=kind+'.npz',kind=kind,edges=[['p','x'],['p','y']],leaf_states={'x':'C','y':'A'},states=['A','C','G'])
                    examples.append((spec,specimen(kind)))
            for spec,original in examples:
                files.append(spec)
                changed=copy.deepcopy(original)
                if transform: transform(changed,spec['kind'])
                np.savez(reference/spec['path'],**(changed if broken_reference else original))
                np.savez(candidate/spec['path'],**changed)
            if bad_archive:
                target=reference if archive_side=='reference' else candidate
                bad_archive(target/files[0]['path'])
            rubric=root/'rubric.json'; rubric.write_text(json.dumps({'policy':'invariants','comparison':{'atol':0,'rtol':0,'files':files}}))
            out=root/'result.json'
            process=subprocess.run([sys.executable,str(VALIDATOR),'--reference',str(reference),'--candidate',str(candidate),'--rubric',str(rubric),'--out',str(out)], capture_output=True, text=True)
            self.assertEqual(process.returncode,0,process.stderr)
            self.assertTrue(out.is_file(),'validator must write result JSON')
            result=json.loads(out.read_text())
            self.assertIs(result['passed'],expected,result)
            if expected:
                self.assertEqual(result['distance'],0,result)
                self.assertEqual(result['bound_fraction'],0,result)
            return result

    def test_identical_valid(self): self.check()

    def test_another_optimum(self):
        self.check(lambda d,k:d['assignment'].__setitem__(0,'C'))

    def test_all_identity_payload_axes_permuted(self):
        def change(d,k):
            d['node_ids']=np.array(['renamed-root','a','b'])
            d['edges']=np.array([['renamed-root','b'],['renamed-root','a']])
            p=[2,0,1]
            for key in ['node_ids','leaf_ids','assignment']: d[key]=d[key][p]
            d['states']=d['states'][p]
            if k=='sets': d['root_state_membership']=d['root_state_membership'][p][:,p]
            if k=='count':
                r=[1,2,0]; c=[2,0,1]
                d['row_states']=d['row_states'][r]; d['column_states']=d['column_states'][c]
                d['transitions']=d['transitions'][r][:,c]
        self.check(change)

    def test_big_endian_numeric(self):
        def change(d,k):
            d['score']=d['score'].astype('>i8')
            if k=='count': d['transitions']=d['transitions'].astype('>f8')
        self.check(change)

    def test_nonoptimal_forged_scalar(self): self.check(lambda d,k:d['assignment'].__setitem__(0,'G'),False)
    def test_nonoptimal_truthful_scalar(self):
        def change(d,k): d['assignment'][0]='G'; d['score'][0]=2
        self.check(change,False)
    def test_false_score(self): self.check(lambda d,k:d['score'].__setitem__(0,0),False)
    def test_all_same_state(self): self.check(lambda d,k:d['assignment'].__setitem__(slice(None),'A'),False)
    def test_wrong_leaf_state(self): self.check(lambda d,k:d['assignment'].__setitem__(1,'A'),False)
    def test_unknown_state(self): self.check(lambda d,k:d['assignment'].__setitem__(0,'N'),False)
    def test_missing_assignment(self): self.check(lambda d,k:d.pop('assignment'),False)
    def test_missing_tree(self): self.check(lambda d,k:d.pop('edges'),False)
    def test_extra_array(self): self.check(lambda d,k:d.update(extra=np.array([1])),False)
    def test_duplicate_node_identity(self): self.check(lambda d,k:d['node_ids'].__setitem__(0,'x'),False)
    def test_duplicate_leaf_identity(self): self.check(lambda d,k:d['leaf_ids'].__setitem__(2,'x'),False)
    def test_extra_leaf_identity(self): self.check(lambda d,k:d['leaf_ids'].__setitem__(2,'88'),False)
    def test_missing_leaf_identity(self): self.check(lambda d,k:d['leaf_ids'].__setitem__(2,''),False)
    def test_false_internal_leaf(self): self.check(lambda d,k:d['leaf_ids'].__setitem__(0,'55'),False)
    def test_duplicate_edge(self): self.check(lambda d,k:d['edges'].__setitem__(1,d['edges'][0]),False)
    def test_unknown_edge_endpoint(self): self.check(lambda d,k:d['edges'].__setitem__((0,1),'88'),False)
    def test_cycle(self): self.check(lambda d,k:d['edges'].__setitem__(1,['x','p']),False)
    def test_wrong_topology(self): self.check(lambda d,k:d['edges'].__setitem__(1,['x','y']),False)
    def test_disconnected_extra_node(self):
        def change(d,k):
            for key,val in [('node_ids','xx'),('leaf_ids',''),('assignment','A')]: d[key]=np.append(d[key],val)
            if k=='sets': d['root_state_membership']=np.vstack([d['root_state_membership'],[1,0,0]])
        self.check(change,False)
    def test_missing_node_row(self):
        def change(d,k):
            for key in ['node_ids','leaf_ids','assignment']: d[key]=d[key][1:]
            if k=='sets': d['root_state_membership']=d['root_state_membership'][1:]
        self.check(change,False)
    def test_identity_only_permutation(self): self.check(lambda d,k:d.update(node_ids=d['node_ids'][::-1]),False)
    def test_payload_only_permutation(self): self.check(lambda d,k:d.update(assignment=d['assignment'][[1,2,0]]),False)
    def test_duplicate_state_axis(self): self.check(lambda d,k:d['states'].__setitem__(0,'C'),False)
    def test_extra_state_axis(self): self.check(lambda d,k:d.update(states=np.append(d['states'],'N')),False)
    def test_invalid_sets(self):
        self.check(lambda d,k:d['root_state_membership'].__setitem__((0,2),True) if k=='sets' else None,False)
    def test_set_axes_not_synchronized(self):
        self.check(lambda d,k:d.update(root_state_membership=d['root_state_membership'][:,::-1]) if k=='sets' else None,False)
    def test_wrong_transition(self): self.check(lambda d,k:d['transitions'].__setitem__((0,0),2) if k=='count' else None,False)
    def test_transition_axes_not_synchronized(self): self.check(lambda d,k:d.update(row_states=d['row_states'][::-1]) if k=='count' else None,False)
    def test_duplicate_count_identity(self): self.check(lambda d,k:d['row_states'].__setitem__(0,'C') if k=='count' else None,False)
    def test_nan_inf_counts(self):
        for value in [float('nan'),float('inf'),float('-inf')]:
            with self.subTest(value=value): self.check(lambda d,k:d['transitions'].__setitem__((0,0),value) if k=='count' else None,False)
    def test_invalid_assignment_dtype(self): self.check(lambda d,k:d.update(assignment=np.array([np.nan,np.inf,-np.inf])),False)
    def test_invalid_dtypes(self):
        for key,dtype in [('node_ids','S4'),('leaf_ids',object),('score',np.float64),('score',np.int32),('assignment',object),('edges',np.int64),('states',object)]:
            with self.subTest(key=key,dtype=dtype):
                def change(d,k):
                    d[key]=np.zeros(d[key].shape,dtype=dtype) if dtype==np.int64 and key=='edges' else d[key].astype(dtype)
                self.check(change,False)
    def test_invalid_count_dtype_shape(self):
        for value in [np.ones((3,3),dtype=np.int64),np.ones((3,3),dtype=np.float32),np.ones((9,),dtype=np.float64)]:
            with self.subTest(dtype=str(value.dtype),shape=value.shape): self.check(lambda d,k:d.update(transitions=value) if k=='count' else None,False)
    def test_invalid_sets_dtype_shape(self):
        for value in [np.ones((3,3),dtype=np.int64),np.ones((9,),dtype=bool)]:
            with self.subTest(dtype=str(value.dtype)): self.check(lambda d,k:d.update(root_state_membership=value) if k=='sets' else None,False)
    def test_bad_archive(self): self.check(expected=False,bad_archive=lambda p:p.write_bytes(b'not an npz'))
    def test_truncated_archive(self): self.check(expected=False,bad_archive=lambda p:p.write_bytes(p.read_bytes()[:100]))
    def test_npy_instead_of_npz(self):
        def change(p):
            with p.open('wb') as f: np.save(f,np.array([1]))
        self.check(expected=False,bad_archive=change)
    def test_duplicate_archive_member(self):
        def change(p):
            b=io.BytesIO(); np.save(b,np.array([1],dtype=np.int64))
            with self.assertWarns(UserWarning):
                with zipfile.ZipFile(p,'a') as z:
                    z.writestr('score.npy',b.getvalue())
        self.check(expected=False,bad_archive=change)
    def test_invalid_reference_not_trusted(self): self.check(lambda d,k:d['assignment'].__setitem__(0,'G'),False,broken_reference=True)

    def test_both_sides_zero_bytes(self):
        for side in ['reference','candidate']:
            with self.subTest(side=side):
                self.check(expected=False,bad_archive=lambda p:p.write_bytes(b''),archive_side=side)

    def test_both_sides_bad_zip(self):
        for side in ['reference','candidate']:
            with self.subTest(side=side):
                self.check(expected=False,bad_archive=lambda p:p.write_bytes(b'PK\x03\x04invalid ZIP'),archive_side=side)

    def test_both_sides_empty_zip(self):
        def damage(path):
            with zipfile.ZipFile(path,'w'):
                pass
        for side in ['reference','candidate']:
            with self.subTest(side=side):
                self.check(expected=False,bad_archive=damage,archive_side=side)

    def test_both_sides_truncated_npy_inside_valid_zip(self):
        def damage(path):
            with zipfile.ZipFile(path) as archive:
                payload={name:archive.read(name) for name in archive.namelist()}
            payload['node_ids.npy']=payload['node_ids.npy'][:-1]
            # CRC和ZIP结构合法，NPY header的shape仍声明完整数据。
            with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_STORED) as archive:
                for name,value in payload.items():
                    archive.writestr(name,value)
        for side in ['reference','candidate']:
            with self.subTest(side=side):
                self.check(expected=False,bad_archive=damage,archive_side=side)

    def test_both_sides_invalid_deflate_with_valid_schema(self):
        def damage(path):
            with zipfile.ZipFile(path) as archive:
                payload={name:archive.read(name) for name in archive.namelist()}
            with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as archive:
                for name,value in payload.items():
                    archive.writestr(name,value)
            with zipfile.ZipFile(path) as archive:
                sizes={info.filename:info.file_size for info in archive.infolist()}
                info=archive.getinfo('node_ids.npy')
                self.assertEqual(info.compress_type,zipfile.ZIP_DEFLATED)
            content=bytearray(path.read_bytes())
            offset=info.header_offset
            self.assertEqual(content[offset:offset+4],b'PK\x03\x04')
            name_length,extra_length=struct.unpack_from('<HH',content,offset+26)
            block=offset+30+name_length+extra_length
            content[block]=(content[block] & ~6) | 6
            path.write_bytes(content)
            with zipfile.ZipFile(path) as archive:
                self.assertEqual({info.filename:info.file_size for info in archive.infolist()},sizes)
                self.assertEqual(set(archive.namelist()),set(payload))
                # 必须真的到达DEFLATE解压器，而非成员预检查已经失败。
                with self.assertRaises(zlib.error):
                    archive.read('node_ids.npy')
        for side in ['reference','candidate']:
            with self.subTest(side=side):
                result=self.check(expected=False,bad_archive=damage,archive_side=side)
                self.assertIn('invalid block type',result['reason'])

    def test_directed_counts_reject_asymmetric_transpose(self):
        # 人工星形树两个A叶、一个C叶：全局唯一最优根为A，矩阵方向不对称。
        spec=dict(path='directed.npz',kind='count',edges=[['p','x'],['p','y'],['p','z']],
                  leaf_states={'x':'A','y':'A','z':'C'},states=['A','C'])
        payload=dict(node_ids=np.array(['p','x','y','z']),leaf_ids=np.array(['','x','y','z']),
                     edges=np.array(spec['edges']),assignment=np.array(['A','A','A','C']),
                     score=np.array([1],dtype=np.int64),states=np.array(['A','C']),
                     row_states=np.array(['A','C']),column_states=np.array(['A','C']),
                     transitions=np.array([[2,1],[0,0]],dtype=np.float64))
        self.assertFalse(np.array_equal(payload['transitions'],payload['transitions'].T))
        self.check(examples=[(spec,payload)])
        self.check(lambda d,k:d.update(transitions=d['transitions'].T),False,examples=[(spec,payload)])

    def test_global_optimum_can_use_locally_suboptimal_root(self):
        # u局部根B多一次改变，但省掉父边变化；不能将局部根集合强加给全局赋值。
        spec=dict(path='independent.npz',kind='sets',edges=[['r','u'],['r','z'],['r','w'],['u','x'],['u','y']],
                  leaf_states={'x':'A','y':'C','z':'B','w':'B'},states=['A','B','C'])
        payload=dict(node_ids=np.array(['r','u','x','y','z','w']),leaf_ids=np.array(['','','x','y','z','w']),
                     edges=np.array(spec['edges']),states=np.array(spec['states']),
                     assignment=np.array(['B','A','A','C','B','B']),score=np.array([2],dtype=np.int64),
                     root_state_membership=np.array([[0,1,0],[1,0,1],[1,0,0],[0,0,1],[0,1,0],[0,1,0]],dtype=bool))
        self.assertFalse(payload['root_state_membership'][1,1])
        self.check(lambda d,k:d['assignment'].__setitem__(1,'B'),examples=[(spec,payload)])


if __name__=='__main__':
    unittest.main(verbosity=2)
