#!/usr/bin/env python3
"""人工两叶单字符完整分布自测，独立于Cassiopeia/source/HOME与真实nominal输出。"""
import copy
import importlib.util
import io
import itertools
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile
import zlib
import numpy as np

VALIDATOR=Path(__file__).with_name('validate.py')


def identity(model,states):
    return json.dumps({'model':model,'assignment':[['a',0,states[0]],['b',0,states[1]]]},sort_keys=True)


def fixture():
    comparison={'atol':1e-10,'rtol':0.0,'normalization_atol':1e-10,'leaf_ids':['a','b'],'character_ids':[0],
                'alphabet':[0,1],'models':['discrete','continuous'],
                'support_witness':{'parameters':{'mutation_rate':.5,'heritable_missing_rate':.25,'stochastic_missing_probability':.25},
                                   'priors':{'0':{'1':1.0}},'edge_lengths':[1.0,1.0,1.0]}}
    ids=[];values=[]
    for model,probabilities in [('discrete',[.1,.2,.3,.4]),('continuous',[.4,.3,.2,.1])]:
        for states,p in zip(itertools.product([0,1],repeat=2),probabilities):ids.append(identity(model,states));values.append(np.log(p))
    data={'log_ids':np.asarray(ids),'is_zero':np.zeros(8,dtype=bool),'positive_ids':np.asarray(ids),'log_values':np.asarray(values,dtype=np.float64)}
    return comparison,data


class DistributionContract(unittest.TestCase):
    def run_pair(self,change=None,expected=True,side='candidate',damage=None):
        comparison,data=fixture()
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);ref=root/'ref';cand=root/'cand';ref.mkdir();cand.mkdir()
            np.savez(ref/'results.npz',**data);np.savez(cand/'results.npz',**data)
            modified=copy.deepcopy(data)
            if change:change(modified)
            target=ref if side=='reference' else cand;np.savez(target/'results.npz',**modified)
            if damage:damage(target/'results.npz')
            rubric=root/'rubric.json';rubric.write_text(json.dumps({'comparison':comparison}))
            out=root/'result.json';out.write_text('{"passed":true,"stale":true}')
            command=[sys.executable,str(VALIDATOR),'--reference',str(ref),'--candidate',str(cand),'--rubric',str(rubric),'--out',str(out)]
            process=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(process.returncode,0,process.stderr)
            wire=out.read_bytes();wire.decode('ascii');wire.decode('utf-8');result=json.loads(wire)
            self.assertIs(result['passed'],expected,result);self.assertNotIn('stale',result)
            return result

    def test_fixed_positive_support_requires_its_input_witness(self):
        comparison,data=fixture();comparison['support_witness']['priors']['0']['1']=0.0
        with patch(__name__+'.fixture',return_value=(comparison,data)):
            self.run_pair(expected=False)

    def test_valid_complete_distribution(self):self.run_pair()
    def test_complete_identity_payload_and_assignment_reorder(self):
        def change(d):
            for key in ['log_ids','positive_ids']:
                altered=[]
                for raw in d[key]:
                    x=json.loads(raw);x['assignment']=x['assignment'][::-1];altered.append(json.dumps(x,separators=(',',':')))
                d[key]=np.asarray(altered)
            p=[5,2,1,7,0,4,3,6];q=p[::-1]
            d['log_ids']=d['log_ids'][p];d['is_zero']=d['is_zero'][p]
            d['positive_ids']=d['positive_ids'][q];d['log_values']=d['log_values'][q]
        self.run_pair(change)
    def test_model_exchange_preserves_normalization(self):self.run_pair(lambda d:d.update(log_values=np.roll(d['log_values'],4)),False)
    def test_pattern_exchange_preserves_normalization(self):
        def change(d):d['log_values'][0],d['log_values'][1]=d['log_values'][1],d['log_values'][0]
        self.run_pair(change,False)
    def test_uniform_distribution_is_not_enough(self):self.run_pair(lambda d:d.update(log_values=np.full(8,-np.log(4))),False)
    def test_fake_zero_both_sides(self):
        def change(d):d['is_zero'][0]=True;d['positive_ids']=d['positive_ids'][1:];d['log_values']=d['log_values'][1:]
        for side in ['reference','candidate']:
            with self.subTest(side=side):self.run_pair(change,False,side)
    def test_missing_duplicate_extra_positive_identity(self):
        changes=[lambda d:d.update(positive_ids=d['positive_ids'][:-1],log_values=d['log_values'][:-1]),
                 lambda d:d['positive_ids'].__setitem__(0,d['positive_ids'][1]),
                 lambda d:d.update(positive_ids=np.append(d['positive_ids'],identity('unknown',[0,0])),log_values=np.append(d['log_values'],-1.))]
        for change in changes:
            with self.subTest(change=repr(change)):self.run_pair(change,False)
    def test_assignment_missing_duplicate_extra_slot(self):
        for mode in ['missing','duplicate','extra','unknown-state']:
            def change(d):
                record=json.loads(d['log_ids'][0])
                if mode=='missing':record['assignment'].pop()
                elif mode=='duplicate':record['assignment'][1]=record['assignment'][0]
                elif mode=='extra':record['assignment'].append(['c',0,0])
                else:record['assignment'][0][2]=99
                values=d['log_ids'].tolist();values[0]=json.dumps(record);d['log_ids']=np.asarray(values)
            with self.subTest(mode=mode):self.run_pair(change,False)
    def test_nonfinite_log_values_both_sides(self):
        for value in [np.nan,np.inf,-np.inf]:
            for side in ['reference','candidate']:
                with self.subTest(value=value,side=side):self.run_pair(lambda d:d['log_values'].__setitem__(0,value),False,side)
    def test_dtype_shape_fields(self):
        changes=[lambda d:d.update(is_zero=d['is_zero'].astype(int)),lambda d:d.update(log_values=d['log_values'].astype(np.float32)),
                 lambda d:d.update(log_ids=d['log_ids'].astype(object)),lambda d:d.update(log_values=d['log_values'].reshape(2,-1)),
                 lambda d:d.pop('positive_ids'),lambda d:d.update(extra=np.array([1]))]
        for change in changes:
            with self.subTest(change=repr(change)):self.run_pair(change,False)
    def test_noncanonical_json_encoding_is_not_physics(self):
        def change(d):
            for key in ['log_ids','positive_ids']:d[key]=np.asarray([json.dumps(json.loads(x),indent=2) for x in d[key]])
        self.run_pair(change)
    def test_threshold_boundary(self):
        self.run_pair(lambda d:d['log_values'].__setitem__(0,d['log_values'][0]+.5e-10))
        self.run_pair(lambda d:d['log_values'].__setitem__(0,d['log_values'][0]+2e-10),False)

    def test_decode_boundaries_both_sides(self):
        def damage(path,kind):
            if kind=='empty':path.write_bytes(b'');return
            if kind=='bad':path.write_bytes(b'PK\x03\x04bad');return
            if kind=='empty-zip':
                with zipfile.ZipFile(path,'w'):pass
                return
            with zipfile.ZipFile(path) as z:payload={k:z.read(k) for k in z.namelist()}
            if kind=='truncated-npy':payload['log_ids.npy']=payload['log_ids.npy'][:-1]
            with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
                for key,value in payload.items():z.writestr(key,value)
            if kind=='deflate':
                with zipfile.ZipFile(path) as z:info=z.getinfo('log_ids.npy');sizes={i.filename:i.file_size for i in z.infolist()}
                body=bytearray(path.read_bytes());n,e=struct.unpack_from('<HH',body,info.header_offset+26);offset=info.header_offset+30+n+e
                body[offset]=(body[offset]&~6)|6;path.write_bytes(body)
                with zipfile.ZipFile(path) as z:
                    self.assertEqual(sizes,{i.filename:i.file_size for i in z.infolist()})
                    with self.assertRaises(zlib.error):z.read('log_ids.npy')
        for kind in ['empty','bad','empty-zip','truncated-npy','deflate']:
            for side in ['reference','candidate']:
                with self.subTest(kind=kind,side=side):self.run_pair(expected=False,side=side,damage=lambda p:damage(p,kind))

    def test_exception_wire_baseexception_and_io(self):
        spec=importlib.util.spec_from_file_location('validator_under_test',VALIDATOR);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);out=root/'result.json';argv=['--reference',str(root),'--candidate',str(root),'--rubric',str(root/'rubric.json'),'--out',str(out)]
            class BadStringError(Exception):
                def __str__(self):raise RuntimeError('do not format this exception')
            for error in [ValueError('surrogate\ud800'),BadStringError()]:
                out.write_text('{"passed":true,"stale":true}')
                with patch.object(module,'evaluate',side_effect=error),patch('sys.stderr',new=io.StringIO()):self.assertEqual(module.main(argv),0)
                wire=out.read_bytes();wire.decode('ascii');wire.decode('utf-8');self.assertFalse(json.loads(wire)['passed']);self.assertNotIn('stale',json.loads(wire))
            with patch.object(module,'evaluate',return_value={'passed':True,'distance':float('nan')}),patch('sys.stderr',new=io.StringIO()):module.main(argv)
            self.assertFalse(json.loads(out.read_bytes())['passed'])
            with patch.object(module,'evaluate',side_effect=KeyboardInterrupt()):
                with self.assertRaises(KeyboardInterrupt):module.main(argv)
            with patch.object(module,'evaluate',return_value={'passed':True,'reason':'ok'}):
                with self.assertRaises(OSError):module.main(argv[:-1]+[str(root)])


if __name__=='__main__':unittest.main(verbosity=2)
