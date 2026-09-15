#!/usr/bin/env python3
"""执行固定官方输入生命周期，只导出四个公共score、三元参数及真实log结果。"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import networkx as nx
import numpy as np
import pandas as pd
import cassiopeia as cas
from cassiopeia.tools import tree_metrics

PARAMETERS=('mutation_rate','heritable_missing_rate','stochastic_missing_probability')
APIS={'calculate_parsimony','get_lineage_tracing_parameters','log_transition_probability',
      'log_likelihood_of_character','calculate_likelihood_discrete','calculate_likelihood_continuous'}


def make_tree(fixture):
    graph=nx.DiGraph();graph.add_nodes_from(fixture['nodes']);graph.add_edges_from((p,c) for p,c,t in fixture['edges'])
    cm=fixture['character_matrix']
    matrix=None if cm is None else pd.DataFrame(cm['values'],index=cm['leaf_ids'],columns=cm['character_ids'])
    priors=None if fixture['priors'] is None else {int(c):{int(s):p for s,p in entries.items()} for c,entries in fixture['priors'].items()}
    tree=cas.data.CassiopeiaTree(tree=graph,character_matrix=matrix,priors=priors,missing_state_indicator=fixture['missing_state_indicator'])
    for parent,child,length in fixture['edges']:tree.set_branch_length(parent,child,length)
    for node,states in fixture['internal_states'].items():tree.set_character_states(node,states)
    tree.parameters.update(fixture['parameters'])
    return tree


def callback(spec):
    value=spec['value']
    if spec['kind']=='constant':return lambda t:value
    if spec['kind']=='linear':return lambda t:value*t
    raise ValueError('未知固定callback')


def structural_transition(arguments):
    source,target=arguments['s'],arguments['s_']
    if target==-1:return False
    if target=='&':return source==-1
    if target==0:return source!=0
    return source==-1 or source not in (0,target)


def decode_log(value,allow_structural_sentinel):
    array=np.asarray(value)
    if array.ndim!=0 or array.dtype.kind not in 'if':raise ValueError('log API必须返回实数标量')
    number=float(array)
    if np.isnan(number) or np.isposinf(number):raise ValueError('生产log API返回NaN/+Inf')
    # 读取实际返回值，不按期望mask伪造标签；unexpected zero由validator固定支持约束拒绝。
    zero=bool(np.isneginf(number) or allow_structural_sentinel and number==-1e16)
    return zero,number


def produce(inputs):
    data={key:[] for key in ['count_ids','counts','parameter_ids','parameters','log_ids','is_zero','positive_ids','log_values']}
    for session in inputs['sessions']:
        tree=make_tree(session['initial_tree'])
        for event in session['events']:
            operation=event['op']
            if operation=='new_tree':tree=make_tree(event['tree'])
            elif operation=='reset_parameters':tree.reset_parameters()
            elif operation=='pop_parameter':tree.parameters.pop(event['key'])
            elif operation=='set_parameter':tree.parameters[event['key']]=event['value']
            elif operation=='set_character_states':tree.set_character_states(event['node'],event['states'])
            elif operation=='reconstruct_ancestral_characters':tree.reconstruct_ancestral_characters()
            elif operation=='call':
                if event['api'] not in APIS:raise ValueError('未知固定生产API')
                kwargs={key:callback(value) if isinstance(value,dict) and 'kind' in value else value for key,value in event['arguments'].items()}
                result=getattr(tree_metrics,event['api'])(tree,**kwargs)
                if event['kind']=='count':
                    data['count_ids'].append(event['id']+'::parsimony');data['counts'].append(result)
                elif event['kind']=='parameters':
                    if not isinstance(result,tuple) or len(result)!=3:raise ValueError('参数API必须完整返回三元tuple')
                    for name,value in zip(PARAMETERS,result):
                        data['parameter_ids'].append(event['id']+'::'+name);data['parameters'].append(value)
                else:
                    identifier=event['id']+'::log_probability'
                    allow_sentinel=event['api']=='log_transition_probability' and structural_transition(kwargs)
                    zero,value=decode_log(result,allow_sentinel)
                    data['log_ids'].append(identifier);data['is_zero'].append(zero)
                    if not zero:data['positive_ids'].append(identifier);data['log_values'].append(value)
            else:raise ValueError('未知生命周期操作')
    result={key:np.asarray(value,dtype=np.str_) for key,value in data.items() if key.endswith('_ids')}
    result.update(counts=np.asarray(data['counts']),parameters=np.asarray(data['parameters'],dtype=np.float64),
                  is_zero=np.asarray(data['is_zero'],dtype=bool),log_values=np.asarray(data['log_values'],dtype=np.float64))
    return result


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--inputs',required=True);parser.add_argument('--out',required=True)
    args=parser.parse_args();output=Path(args.out);output.mkdir(parents=True,exist_ok=True)
    if (output/'results.npz').exists():raise ValueError('拒绝覆盖既有科学结果')
    inputs=json.loads(Path(args.inputs).read_text(encoding='utf-8'))
    values=produce(inputs)
    np.savez(output/'results.npz',**values)
    print('wrote real public parsimony scores, complete parameter triples and typed log probabilities')


if __name__=='__main__':main()
