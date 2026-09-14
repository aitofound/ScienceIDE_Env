#!/usr/bin/env python3
"""独立人工helper语义/完整tuple自测；不读取HOME、生产source或真实nominal结果。"""
import copy
import importlib.util
import io
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
sys.dont_write_bytecode=True
VALIDATOR=Path(__file__).with_name('validate.py')


def fixture():
    data={'cell_ids':['p','a','b','c','d'],'character_ids':[0],
          'character_matrix':[[9],[{'tuple':[2,2]}],[1],[-1],[1]],
          'coordinate_cell_ids':['p','a','b','c','d'],'coordinates':[[0.,0.],[15.,0.],[5.,0.],[8.,0.],[20.,0.]],
          'graph_nodes':['p','a','b','c','d'],'edges':[['p','a'],['p','b'],['p','c'],['a','d']]}
    cases=[{'id':'basic','query_cell':'p','character_id':0,'number_of_hops':1,'max_neighbor_distance':'unbounded','use_coordinates':False},
           {'id':'distance','query_cell':'p','character_id':0,'number_of_hops':1,'max_neighbor_distance':15,'use_coordinates':True},
           {'id':'other','query_cell':'b','character_id':0,'number_of_hops':1,'max_neighbor_distance':'unbounded','use_coordinates':False}]
    comparison={'frequency_atol':1e-12,'fixture':data,'cases':cases}
    output={'case_ids':np.array(['basic','distance','other']),'query_cells':np.array(['p','p','b']),
            'character_ids':np.array([0,0,0],dtype=np.int64),'states':np.array([2,2,9],dtype=np.int64),
            'frequencies':np.array([2/3,2/3,1.0],dtype=np.float64),'counts':np.array([2,2,1],dtype=np.int64)}
    return comparison,output


def load_module():
    spec=importlib.util.spec_from_file_location('spatial_validator',VALIDATOR);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module


class CompleteTuple(unittest.TestCase):
    def pair(self,change=None,expected=True,side='candidate',damage=None):
        comparison,original=fixture()
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);ref=root/'ref';cand=root/'cand';ref.mkdir();cand.mkdir()
            np.savez(ref/'results.npz',**original);np.savez(cand/'results.npz',**original)
            value=copy.deepcopy(original)
            if change:change(value)
            target=ref if side=='reference' else cand;np.savez(target/'results.npz',**value)
            if damage:damage(target/'results.npz')
            rubric=root/'rubric.json';rubric.write_text(json.dumps({'comparison':comparison},allow_nan=False))
            out=root/'result.json';out.write_text('{"passed":true,"stale":true}')
            process=subprocess.run([sys.executable,str(VALIDATOR),'--reference',str(ref),'--candidate',str(cand),'--rubric',str(rubric),'--out',str(out)],capture_output=True,text=True)
            self.assertEqual(process.returncode,0,process.stderr)
            wire=out.read_bytes();wire.decode('ascii');wire.decode('utf-8');result=json.loads(wire)
            self.assertIs(result['passed'],expected,result);self.assertNotIn('stale',result)
            return result

    def test_valid_complete_tuple(self):self.pair()
    def test_all_payloads_reordered(self):self.pair(lambda d:d.update({k:v[::-1] for k,v in d.items()}))
    def test_state_only_wrong(self):self.pair(lambda d:d['states'].__setitem__(0,1),False)
    def test_count_is_not_neighbor_count(self):self.pair(lambda d:d['counts'].__setitem__(0,3),False)
    def test_frequency_only_wrong(self):self.pair(lambda d:d['frequencies'].__setitem__(0,.5),False)
    def test_frequency_tolerance(self):
        self.pair(lambda d:d['frequencies'].__setitem__(0,d['frequencies'][0]+.5e-12))
        self.pair(lambda d:d['frequencies'].__setitem__(0,d['frequencies'][0]+2e-12),False)
    def test_payload_only_reorder(self):self.pair(lambda d:d.update(states=d['states'][::-1],counts=d['counts'][::-1],frequencies=d['frequencies'][::-1]),False)
    def test_bad_id_sets(self):
        changes=[lambda d:d['case_ids'].__setitem__(0,d['case_ids'][1]),lambda d:d['query_cells'].__setitem__(0,'x'),
                 lambda d:d['character_ids'].__setitem__(0,1),lambda d:d.update({k:v[:-1] for k,v in d.items()}),
                 lambda d:d.update({k:np.append(v,v[0]) for k,v in d.items()})]
        for change in changes:
            with self.subTest(change=repr(change)):self.pair(change,False)
    def test_dtypes_shapes_fields(self):
        changes=[lambda d:d.update(states=d['states'].astype(float)),lambda d:d.update(counts=d['counts'].astype(np.int32)),
                 lambda d:d.update(frequencies=d['frequencies'].astype(np.float32)),lambda d:d.update(query_cells=d['query_cells'].astype(object)),
                 lambda d:d.update(character_ids=d['character_ids'].reshape(1,-1)),lambda d:d.pop('counts'),lambda d:d.update(extra=np.array([1]))]
        for change in changes:
            with self.subTest(change=repr(change)):self.pair(change,False)
    def test_either_byte_order(self):
        self.pair(lambda d:d.update(states=d['states'].astype('>i8'),counts=d['counts'].astype('>i8'),character_ids=d['character_ids'].astype('>i8'),frequencies=d['frequencies'].astype('>f8')))
    def test_nonfinite_both_sides(self):
        for value in [np.nan,np.inf,-np.inf]:
            for side in ['reference','candidate']:
                with self.subTest(value=value,side=side):self.pair(lambda d:d['frequencies'].__setitem__(0,value),False,side)
    def test_wrong_reference_also_rejected(self):self.pair(lambda d:d['counts'].__setitem__(0,3),False,'reference')

    def test_development_tuple_missing_and_zero(self):
        module=load_module();comparison,_=fixture();f=comparison['fixture'];case=comparison['cases'][0]
        profile=module.vote_profile(f,case)
        self.assertEqual(profile['winning_count'],2);self.assertEqual(profile['total_votes'],3);self.assertEqual(profile['winners'],[2])
        f['character_matrix'][1]=[{'tuple':[-1,-1,2]}];f['character_matrix'][2]=[-1]
        profile=module.vote_profile(f,case)
        self.assertEqual(profile['winners'],[-1]);self.assertEqual(profile['winning_count'],2);self.assertEqual(profile['total_votes'],3)
        f['character_matrix'][1]=[{'tuple':[0,0]}];f['character_matrix'][2]=[1]
        self.assertEqual(module.vote_profile(f,case)['winners'],[0])
    def test_development_bfs_and_tie(self):
        module=load_module();comparison,_=fixture();f=comparison['fixture'];case=comparison['cases'][0]
        case['number_of_hops']=2
        profile=module.vote_profile(f,case)
        self.assertEqual(profile['winners'],[1,2]);self.assertEqual(profile['total_votes'],4)
        with self.assertRaises(ValueError):module.catalog(comparison)
        f['character_matrix'][4]=[2]
        self.assertEqual(module.vote_profile(f,case)['winning_count'],3)
    def test_development_inclusive_boundary(self):
        module=load_module();comparison,_=fixture();f=comparison['fixture'];case=comparison['cases'][1]
        self.assertEqual(module.vote_profile(f,case)['total_votes'],3)
        case['max_neighbor_distance']=float(np.nextafter(np.nextafter(15.,-np.inf),-np.inf))
        self.assertEqual(module.vote_profile(f,case)['total_votes'],1)
    def test_development_no_votes(self):
        module=load_module();comparison,_=fixture();f=comparison['fixture'];case=comparison['cases'][0]
        f['character_matrix'][1]=[-1];f['character_matrix'][2]=[-1]
        profile=module.vote_profile(f,case)
        self.assertEqual(profile['winners'],[-1]);self.assertEqual(profile['winning_count'],0);self.assertEqual(profile['frequency'],0)

    def test_bad_archives_both_sides(self):
        def damage(path,kind):
            if kind=='zero':path.write_bytes(b'');return
            if kind=='bad':path.write_bytes(b'PK\x03\x04bad');return
            if kind=='empty-zip':
                with zipfile.ZipFile(path,'w'):pass
                return
            with zipfile.ZipFile(path) as archive:payload={key:archive.read(key) for key in archive.namelist()}
            if kind=='truncated-npy':payload['states.npy']=payload['states.npy'][:-1]
            with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as archive:
                for key,value in payload.items():archive.writestr(key,value)
            if kind=='deflate':
                with zipfile.ZipFile(path) as archive:info=archive.getinfo('states.npy');sizes={x.filename:x.file_size for x in archive.infolist()}
                data=bytearray(path.read_bytes());n,e=struct.unpack_from('<HH',data,info.header_offset+26);start=info.header_offset+30+n+e
                data[start]=(data[start]&~6)|6;path.write_bytes(data)
                with zipfile.ZipFile(path) as archive:
                    self.assertEqual(sizes,{x.filename:x.file_size for x in archive.infolist()})
                    with self.assertRaises(zlib.error):archive.read('states.npy')
        for kind in ['zero','bad','empty-zip','truncated-npy','deflate']:
            for side in ['reference','candidate']:
                with self.subTest(kind=kind,side=side):self.pair(expected=False,side=side,damage=lambda path:damage(path,kind))

    def test_numeric_rejection_reports_a_number_not_a_decode_error(self):
        """纯数值越界必须拿到填好的 distance/bound_fraction，而不是「解码失败」。"""
        result = self.pair(lambda d: d['frequencies'].__setitem__(0, d['frequencies'][0] + 1e-6), False)
        self.assertNotIn('error_type', result)
        self.assertIsNotNone(result['distance'])
        self.assertGreater(result['distance'], 1e-9)
        self.assertIsNotNone(result['bound_fraction'])
        self.assertGreater(result['bound_fraction'], 1.0)
        self.assertTrue(result['measurements'])

    def test_discrete_rejection_is_named_as_categorical(self):
        """离散字段不符时判决书要说明是类别不符，而不是数值越界。"""
        result = self.pair(lambda d: d['counts'].__setitem__(0, 3), False)
        self.assertNotIn('error_type', result)
        self.assertGreaterEqual(result['mismatched_discrete_fields'], 1)
        self.assertIn('winning vote count', result['reason'])

    def test_final_failure_protocol(self):
        module=load_module()
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);out=root/'result.json';argv=['--reference',str(root),'--candidate',str(root),'--rubric',str(root/'rubric.json'),'--out',str(out)]
            class BrokenString(Exception):
                def __str__(self):raise RuntimeError('broken exception string')
            for exc in [ValueError('nonASCII\ud800'),BrokenString()]:
                out.write_text('{"passed":true,"stale":true}')
                with patch.object(module,'evaluate',side_effect=exc),patch('sys.stderr',new=io.StringIO()):self.assertEqual(module.main(argv),0)
                wire=out.read_bytes();wire.decode('ascii');wire.decode('utf-8');self.assertFalse(json.loads(wire)['passed']);self.assertNotIn('stale',json.loads(wire))
            with patch.object(module,'evaluate',return_value={'passed':True,'distance':float('nan')}),patch('sys.stderr',new=io.StringIO()):module.main(argv)
            self.assertFalse(json.loads(out.read_text())['passed'])
            with patch.object(module,'evaluate',side_effect=KeyboardInterrupt()):
                with self.assertRaises(KeyboardInterrupt):module.main(argv)
            with patch.object(module,'evaluate',return_value={'passed':True,'reason':'ok'}):
                with self.assertRaises(OSError):module.main(argv[:-1]+[str(root)])


if __name__=='__main__':unittest.main(verbosity=2)
