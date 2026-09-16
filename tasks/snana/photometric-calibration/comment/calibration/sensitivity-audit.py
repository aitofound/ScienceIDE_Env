"""Compare researched and previous candidate bounds on meaningful faults."""
import argparse,copy,importlib.util,json,tempfile
from pathlib import Path
import numpy as np

p=argparse.ArgumentParser()
for name in ('task','reference','source-probes','out'):p.add_argument('--'+name,type=Path,required=True)
args=p.parse_args()
s=importlib.util.spec_from_file_location('validator',args.task/'tests/checks/official-sdss/validate.py')
v=importlib.util.module_from_spec(s);s.loader.exec_module(v)
report={'source_probes':[],'near_zero_probes':[]}
def rules(check):
    new=json.loads((args.task/'tests/checks'/check/'rubric.json').read_text())
    old=copy.deepcopy(new)
    for spec in old['comparison']['files']:
        if spec['name'] in ('zeropoints','snmag_summary'):spec['atol']=5e-4
        spec.pop('peak_axis',None)
    return old,new
for row in json.loads((args.source_probes/'report.json').read_text()):
    assert row['returncode'] == 0, 'Source probe did not produce a successful calculation'
    check=row['check'];old,new=rules(check)
    ref=args.reference/check;cand=args.source_probes/row['probe']/check
    expected=row['probe']=='primary-endpoint-diagnostic'
    result={**row,'expected_new_pass':expected,
            'previous_bound_result':v.compare(ref,cand,old),'recommended_bound_result':v.compare(ref,cand,new)}
    report['source_probes'].append(result)
for check in sorted((args.task/'tests/checks').iterdir()):
    ref=args.reference/check.name;a=v.load(ref/'result.npz');old,new=rules(check.name)
    for probe in ('erase_day_minus_20','bd17_six_ppm_error'):
        c={k:x.copy() for k,x in a.items()}
        if probe=='erase_day_minus_20':c['sn_sed'][a['phase']==-20]=0
        else:
            if 'BD17' not in a['primary_names']:continue
            j=a['primary_names'].tolist().index('BD17')
            positive=np.flatnonzero(a['primary_sed'][:,j]>0)
            i=positive[np.argmin(a['primary_sed'][positive,j])]
            c['primary_sed'][i,j] *= 1+6e-6
        with tempfile.TemporaryDirectory() as tmp:
            cand=Path(tmp);np.savez_compressed(cand/'result.npz',**c)
            report['near_zero_probes'].append(dict(check=check.name,probe=probe,expected_new_pass=False,
                previous_bound_result=v.compare(ref,cand,old),recommended_bound_result=v.compare(ref,cand,new)))
report['all_expected_results']=all(row['recommended_bound_result']['passed']==row['expected_new_pass'] for group in report.values() if isinstance(group,list) for row in group)
args.out.write_text(json.dumps(report,indent=2,allow_nan=False)+'\n')
for group in ('source_probes','near_zero_probes'):
    for row in report[group]:
        print(row['check'],row['probe'],'previous',row['previous_bound_result']['passed'],'new',row['recommended_bound_result']['passed'],'fraction',row['recommended_bound_result']['bound_fraction'])
assert report['all_expected_results']
