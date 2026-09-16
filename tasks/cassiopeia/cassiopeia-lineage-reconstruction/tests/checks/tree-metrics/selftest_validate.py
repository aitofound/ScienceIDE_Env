#!/usr/bin/env python3
"""人工typed-zero/tuple契约回归，只需stdlib与NumPy，不读取生产source或原生输出。"""
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

VALIDATOR=Path(__file__).with_name('validate.py')


import importlib.util as _ilu
_SPEC=_ilu.spec_from_file_location('tm_validator_under_test',VALIDATOR)
_V=_ilu.module_from_spec(_SPEC);_SPEC.loader.exec_module(_V)
_HERE=Path(__file__).resolve().parent
# 夹具取自**真实合同**：counts 的身份与数值来自 rubric.json 与
# `validate.expected_counts(ic/nominal)` 的独立重算。原来这里是合成的两个
# 假 id（score-explicit / score-reconstructed，值 9/8），加上简约性第三条腿之后
# 被正确拒掉——那两个 id 在 ic/ 里根本不存在。parameters 与 logs 两组仍用合成值，
# 因为那两组本就不在第三条腿的覆盖内（见 validate 的 third_leg_note）。
_REAL=json.loads((_HERE/'rubric.json').read_text(encoding='utf-8'))['comparison']
_COUNTS=_V.expected_counts(_V.read_ic(_HERE/'ic'/'nominal'))


def fixture():
    count_ids=list(_REAL['counts'])
    assert all(i in _COUNTS for i in count_ids),'rubric 的 counts 身份必须都能被独立重算覆盖'
    comparison={'atol':1e-10,'rtol':0.0,'counts':count_ids,
                'parameters':[{'id':'p::mutation','domain':'probability'},{'id':'p::heritable','domain':'rate'},{'id':'p::stochastic','domain':'probability'}],
                'logs':[{'id':'transition:-1->0:character0','rule':'transition','source_state':-1,'target_state':0},
                        {'id':'transition:0->1:character0','rule':'transition','source_state':0,'target_state':1},
                        {'id':'character-log','rule':'positive'}]}
    data={'count_ids':np.array(count_ids),
          'counts':np.array([_COUNTS[i] for i in count_ids],dtype=np.int64),
          'parameter_ids':np.array([x['id'] for x in comparison['parameters']]),'parameters':np.array([.2,.3,.4],dtype=np.float64),
          'log_ids':np.array([x['id'] for x in comparison['logs']]),'is_zero':np.array([True,False,False],dtype=bool),
          'positive_ids':np.array(['transition:0->1:character0','character-log']),'log_values':np.array([-2.,-3.],dtype=np.float64)}
    return comparison,data


class TypedContract(unittest.TestCase):
    def run_pair(self,change=None,expected=True,side='candidate',damage=None):
        comparison,data=fixture()
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);ref=root/'ref';cand=root/'cand';ref.mkdir();cand.mkdir()
            np.savez(ref/'results.npz',**data);np.savez(cand/'results.npz',**data)
            changed=copy.deepcopy(data)
            if change:change(changed)
            target=ref if side=='reference' else cand
            np.savez(target/'results.npz',**changed)
            if damage:damage(target/'results.npz')
            rubric=root/'rubric.json';rubric.write_text(json.dumps({'comparison':comparison}))
            output=root/'result.json';output.write_text('{"passed":true,"stale":true}')
            command=[sys.executable,str(VALIDATOR),'--reference',str(ref),'--candidate',str(cand),'--rubric',str(rubric),'--out',str(output)]
            process=subprocess.run(command,capture_output=True,text=True)
            self.assertEqual(process.returncode,0,process.stderr)
            wire=output.read_bytes();wire.decode('ascii');wire.decode('utf-8')
            result=json.loads(wire)
            self.assertIs(result['passed'],expected,result)
            self.assertNotIn('stale',result)
            return result

    def test_finite_extremely_negative_log_remains_positive_support(self):
        comparison,data=fixture();data['log_values'][1]=-1e17
        with patch(__name__+'.fixture',return_value=(comparison,data)):self.run_pair()

    def test_zero_rate_one_witness_cannot_ignore_explicit_mutation(self):
        comparison,data=fixture()
        comparison['logs'].append({'id':'fixed-zero','rule':'rate_one_uncut_edge','witness':{'leaf_characters':[[1],[1]],
                                  'parent_state':0,'child_state':0,'mean_depth':1,'parameters':{'mutation_rate':.5},
                                  'model':'discrete','use_internal_character_states':True}})
        data['log_ids']=np.append(data['log_ids'],'fixed-zero');data['is_zero']=np.append(data['is_zero'],True)
        with patch(__name__+'.fixture',return_value=(comparison,data)):self.run_pair(expected=False)

    def test_valid(self):self.run_pair()
    def test_full_identity_payload_reordering(self):
        def change(d):
            for ids,values in [('count_ids','counts'),('parameter_ids','parameters'),('log_ids','is_zero'),('positive_ids','log_values')]:
                d[ids]=d[ids][::-1];d[values]=d[values][::-1]
        self.run_pair(change)
    def test_tuple_field_swap(self):self.run_pair(lambda d:d.update(parameters=d['parameters'][::-1]),False)
    def test_wrong_parsimony_not_forced_to_optimum(self):
        """错的简约性不得被悄悄当成最优解接受。

        原来这条写的是 `counts[0] = 8`。夹具换成真实重算值之后它成了**空操作**
        （真实的 counts[0] 恰好就是 8），用例当场失效。改成「在真实值上 +1」，
        并加一条自守断言把这种空操作挡住。
        """
        _, data = fixture()
        original = int(data['counts'][0])

        def wrong(d):
            d['counts'][0] = original + 1
            assert int(d['counts'][0]) != original, '扰动必须真的改变了数值'

        self.run_pair(wrong, False)
    def test_declared_zero_as_finite(self):
        def change(d):d['is_zero'][0]=False;d['positive_ids']=np.append(d['positive_ids'],d['log_ids'][0]);d['log_values']=np.append(d['log_values'],-1e16)
        for side in ['reference','candidate']:
            with self.subTest(side=side):self.run_pair(change,False,side)
    def test_declared_positive_as_zero(self):
        def change(d):d['is_zero'][1]=True;d['positive_ids']=d['positive_ids'][1:];d['log_values']=d['log_values'][1:]
        for side in ['reference','candidate']:
            with self.subTest(side=side):self.run_pair(change,False,side)
    def test_both_sides_same_forged_support(self):
        comparison,data=fixture();data['is_zero'][:]=True;data['positive_ids']=np.array([],dtype='U1');data['log_values']=np.array([],dtype=np.float64)
        with tempfile.TemporaryDirectory() as name:
            root=Path(name);np.savez(root/'results.npz',**data)
            rubric=root/'rubric.json';rubric.write_text(json.dumps({'comparison':comparison}))
            process=subprocess.run([sys.executable,str(VALIDATOR),'--reference',str(root),'--candidate',str(root),'--rubric',str(rubric),'--out',str(root/'result.json')],capture_output=True,text=True)
            self.assertEqual(process.returncode,0,process.stderr);self.assertFalse(json.loads((root/'result.json').read_text())['passed'])
    def test_missing_duplicate_extra_positive_ids(self):
        changes=[lambda d:d.update(positive_ids=d['positive_ids'][:-1],log_values=d['log_values'][:-1]),
                 lambda d:d['positive_ids'].__setitem__(1,d['positive_ids'][0]),
                 lambda d:d.update(positive_ids=np.append(d['positive_ids'],'extra'),log_values=np.append(d['log_values'],-1.))]
        for change in changes:
            with self.subTest(change=repr(change)):self.run_pair(change,False)
    def test_payload_only_log_reorder(self):self.run_pair(lambda d:d.update(log_values=d['log_values'][::-1]),False)
    def test_wrong_counts_identity(self):self.run_pair(lambda d:d['count_ids'].__setitem__(1,d['count_ids'][0]),False)
    def test_nonfinite_values_both_sides(self):
        for field in ['parameters','log_values']:
            for value in [np.nan,np.inf,-np.inf]:
                for side in ['reference','candidate']:
                    with self.subTest(field=field,value=value,side=side):self.run_pair(lambda d:d[field].__setitem__(0,value),False,side)
    def test_shape_dtype_and_fields(self):
        changes=[lambda d:d.update(is_zero=d['is_zero'].astype(np.int64)),lambda d:d.update(log_values=d['log_values'].astype(np.float32)),
                 lambda d:d.update(counts=d['counts'].astype(np.float64)),lambda d:d.update(parameter_ids=d['parameter_ids'].astype(object)),
                 lambda d:d.update(log_ids=d['log_ids'].reshape(1,-1)),lambda d:d.pop('positive_ids'),lambda d:d.update(extra=np.array([1]))]
        for change in changes:
            with self.subTest(change=repr(change)):self.run_pair(change,False)
    def test_tolerance_boundary(self):
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
