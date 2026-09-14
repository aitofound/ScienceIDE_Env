"""仅 stdlib + NumPy 的人工 fixture；不读取 HOME、state 或生产包。"""
import copy
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch
import zlib

import numpy as np

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('mle_validate', HERE / 'validate.py')
validator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(validator)

CASE = {'id':'fixture','edges':[['root','middle'],['middle','leaf']],
        'states':{'root':[0,0,0],'middle':[1,0,0],'leaf':[1,1,0]},
        'minimum_branch_length':0.01,'relative_rates':[1.0,2.0,3.0], 'solver':'SCS'}
COMPARISON = {'atol':0.003,'rtol':0.001,'feasibility_atol':0.0001,
              'consistency_atol':0.000001,'likelihood_atol':0.0001}

def fixture():
    rates = np.array([0.7,1.4,2.1])
    likelihood = np.log(-np.expm1(-0.4*rates[0]-1e-5)) - 0.4*(rates[1]+rates[2])
    likelihood += np.log(-np.expm1(-0.6*rates[1]-1e-5)) - 0.6*rates[2]
    return {'node_ids':np.array(['root','middle','leaf']), 'times':np.array([0.0,0.4,1.0]),
            'edge_ids':np.array([['root','middle'],['middle','leaf']]),
            'branch_lengths':np.array([0.4,0.6]), 'site_ids':np.arange(3,dtype=np.int64),
            'mutation_rates':rates,'log_likelihood':np.array([likelihood])}

class ValidatorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.reference = self.root / 'reference'; self.reference.mkdir()
        self.candidate = self.root / 'candidate'; self.candidate.mkdir()
        self.arrays = fixture()
        self.save(self.reference, self.arrays); self.save(self.candidate, self.arrays)

    def tearDown(self):
        self.temp.cleanup()

    def save(self, root, arrays):
        np.savez(root / 'fixture.npz', **arrays)

    def compare(self, comparison=None):
        return validator.compare(self.reference,self.candidate,comparison or COMPARISON,[CASE])

    def rejects(self, key, value):
        arrays = fixture(); arrays[key] = value
        self.save(self.candidate, arrays)
        with self.assertRaises(ValueError):
            self.compare()

    def test_identical(self):
        result = self.compare()
        self.assertTrue(result['passed']); self.assertEqual(result['distance'],0.0)

    def test_all_payloads_permuted(self):
        arrays=fixture()
        for ids,payload,order in [('node_ids','times',[2,0,1]),('edge_ids','branch_lengths',[1,0]),('site_ids','mutation_rates',[2,0,1])]:
            arrays[ids]=arrays[ids][order]; arrays[payload]=arrays[payload][order]
        self.save(self.candidate,arrays)
        self.assertTrue(self.compare()['passed'])
        self.assertEqual(self.compare()['distance'],0.0)

    def test_ids_without_payload_permutation(self):
        self.rejects('node_ids',np.array(['middle','root','leaf']))

    def test_edge_ids_without_payload_permutation(self):
        self.rejects('edge_ids',np.array([['middle','leaf'],['root','middle']]))

    def test_site_ids_without_payload_permutation(self):
        self.rejects('site_ids',np.array([2,0,1]))

    def test_node_missing_extra_duplicate(self):
        for ids in [np.array(['root','middle']),np.array(['root','middle','leaf','extra']),np.array(['root','middle','middle']),np.array(['root','other','leaf'])]:
            with self.subTest(ids=ids): self.rejects('node_ids',ids)

    def test_wrong_direction(self):
        self.rejects('edge_ids',np.array([['middle','root'],['middle','leaf']]))

    def test_wrong_shape_dtype_finite(self):
        for key,value in [('times',np.array([[0.,.4,1.]])),('times',np.array([0,0,1])),('times',np.array([0.,np.nan,1.])),('times',np.array([0.,np.inf,1.])),('times',np.array([0.,.4,1.],dtype=np.float32)),('mutation_rates',np.array([.7,1.4,np.inf])),('site_ids',np.array([0.,1.,2.])),('node_ids',np.array([0,1,2])),('log_likelihood',np.array([-1+1j]))]:
            with self.subTest(key=key,value=value): self.rejects(key,value)

    def test_branch_consistency(self):
        self.rejects('branch_lengths',np.array([.401,.6]))

    def test_root_time(self):
        self.rejects('times',np.array([.01,.4,1.]))

    def test_leaf_time(self):
        self.rejects('times',np.array([0.,.4,.99]))

    def test_minimum_branch_length(self):
        self.rejects('times',np.array([0.,.001,1.]))

    def test_negative_rate(self):
        self.rejects('mutation_rates',np.array([-.7,-1.4,-2.1]))

    def test_rate_scaling(self):
        self.rejects('mutation_rates',np.array([.7,1.4,2.11]))

    def test_likelihood_consistency(self):
        self.rejects('log_likelihood',self.arrays['log_likelihood']+.01)

    def test_feasible_wrong_science(self):
        arrays=fixture(); arrays['mutation_rates'] *= 1.1
        r=arrays['mutation_rates']
        arrays['log_likelihood']=np.array([np.log(-np.expm1(-.4*r[0]-1e-5))-.4*(r[1]+r[2])+np.log(-np.expm1(-.6*r[1]-1e-5))-.6*r[2]])
        self.save(self.candidate,arrays)
        self.assertFalse(self.compare()['passed'])

    def test_archive_schema(self):
        for arrays in [{k:v for k,v in fixture().items() if k != 'times'},dict(fixture(),extra=np.array([1.]))]:
            with self.subTest(keys=list(arrays)):
                self.save(self.candidate,arrays)
                with self.assertRaises(ValueError): self.compare()

    def test_bad_parameters(self):
        for key,value in [('atol',-1),('rtol',float('nan')),('feasibility_atol','x'),('consistency_atol',True),('likelihood_atol',float('inf'))]:
            c=dict(COMPARISON);c[key]=value
            with self.subTest(key=key), self.assertRaises(ValueError): self.compare(c)

    def test_invalid_input_rates(self):
        for rates in [[1.,-1.,2.],[1.,0.,2.],[1.,2.],[],[1.,2.,float('nan')]]:
            c=copy.deepcopy(CASE);c['relative_rates']=rates
            with self.subTest(rates=rates),self.assertRaises(ValueError):
                validator.compare(self.reference,self.candidate,COMPARISON,[c])

    def test_decoder_boundary_context(self):
        for exc in [EOFError('truncated'),zipfile.BadZipFile('CRC'),zlib.error('deflate'),RuntimeError('encrypted; password required')]:
            with self.subTest(exc=type(exc).__name__),patch.object(validator.np,'load',side_effect=exc):
                result=validator.guarded_compare(self.reference,self.candidate,COMPARISON,[CASE])
                self.assertFalse(result['passed']);self.assertIn(type(exc).__name__,result['reason'])
                self.assertIn(str(exc),result['reason'])

    def test_bad_npz_both_sides_failed_json(self):
        rubric=self.root/'rubric.json';rubric.write_text(json.dumps({'comparison':COMPARISON}))
        inputs=self.root/'inputs.json';inputs.write_text(json.dumps({'cases':[CASE]}))
        stream=io.BytesIO()
        with zipfile.ZipFile(stream,'w') as z:
            z.writestr('times.npy',b'\x93NUMPYbroken')
        complete=io.BytesIO();np.savez_compressed(complete,**fixture())
        payload=complete.getvalue()
        with zipfile.ZipFile(io.BytesIO(payload)) as archive:
            info=archive.getinfo('times.npy')
            start=info.header_offset
            name_length,extra_length=struct.unpack_from('<HH',payload,start+26)
            compressed_start=start+30+name_length+extra_length
        broken_deflate=bytearray(payload);broken_deflate[compressed_start]=7
        encrypted=bytearray(payload)
        struct.pack_into('<H',encrypted,start+6,struct.unpack_from('<H',encrypted,start+6)[0]|1)
        position=0
        while True:
            position=encrypted.find(b'PK\x01\x02',position)
            if position<0:break
            name_size=struct.unpack_from('<H',encrypted,position+28)[0]
            if encrypted[position+46:position+46+name_size]==b'times.npy':
                struct.pack_into('<H',encrypted,position+8,struct.unpack_from('<H',encrypted,position+8)[0]|1)
            position+=4
        crc=bytearray(payload)
        position=0
        while True:
            position=crc.find(b'PK\x01\x02',position)
            if position<0:break
            name_size=struct.unpack_from('<H',crc,position+28)[0]
            if crc[position+46:position+46+name_size]==b'times.npy':
                struct.pack_into('<I',crc,position+16,0)
            position+=4
        corruptions=[b'',b'not an archive',b'PK\x03\x04',stream.getvalue(),payload[:-20],bytes(broken_deflate),bytes(encrypted),bytes(crc)]
        for side in [self.reference,self.candidate]:
            for data in corruptions:
                with self.subTest(side=side.name,data=data[:12]):
                    self.save(self.reference,fixture());self.save(self.candidate,fixture())
                    (side/'fixture.npz').write_bytes(data)
                    out=self.root/'result.json'
                    proc=subprocess.run([sys.executable,str(HERE/'validate.py'),'--reference',str(self.reference),'--candidate',str(self.candidate),'--rubric',str(rubric),'--inputs',str(inputs),'--out',str(out)],capture_output=True,text=True)
                    self.assertEqual(proc.returncode,0,proc.stderr)
                    self.assertFalse(json.loads(out.read_text())['passed'])

if __name__ == '__main__':
    unittest.main()
