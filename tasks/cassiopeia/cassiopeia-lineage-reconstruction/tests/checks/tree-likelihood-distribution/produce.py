#!/usr/bin/env python3
"""完整官方256种叶×字符模式，每种直接计算discrete和continuous likelihood。"""
from __future__ import annotations
import argparse
import itertools
import json
from pathlib import Path
import networkx as nx
import numpy as np
import pandas as pd
import cassiopeia as cas
from cassiopeia.tools import tree_metrics


def make_tree(inputs,assignment):
    graph=nx.DiGraph();graph.add_nodes_from(inputs['nodes']);graph.add_edges_from((p,c) for p,c,t in inputs['edges'])
    matrix=pd.DataFrame([[assignment[(leaf,char)] for char in inputs['character_ids']] for leaf in inputs['leaf_ids']],
                        index=inputs['leaf_ids'],columns=inputs['character_ids'])
    priors={int(c):{int(s):p for s,p in entries.items()} for c,entries in inputs['priors'].items()}
    tree=cas.data.CassiopeiaTree(tree=graph,character_matrix=matrix,priors=priors,missing_state_indicator=inputs['missing_state_indicator'])
    for parent,child,length in inputs['edges']:tree.set_branch_length(parent,child,length)
    tree.parameters.update(inputs['parameters'])
    return tree


def identifier(model,assignment):
    return json.dumps({'model':model,'assignment':[[leaf,char,state] for (leaf,char),state in sorted(assignment.items())]},
                      sort_keys=True,separators=(',',':'))


def produce(inputs):
    all_ids=[];zero_mask=[];positive_ids=[];logs=[]
    coordinates=[tuple(x) for x in inputs['iteration_coordinates']]
    expected=set(itertools.product(inputs['leaf_ids'],inputs['character_ids']))
    if set(coordinates)!=expected or len(coordinates)!=len(expected):raise ValueError('固定枚举坐标缺失或重复')
    if set(inputs['models'])!={'discrete','continuous'} or len(inputs['models'])!=2:raise ValueError('必须计算两个模型')
    for states in itertools.product(inputs['alphabet'],repeat=len(coordinates)):
        assignment=dict(zip(coordinates,states));tree=make_tree(inputs,assignment)
        for model in inputs['models']:
            if model=='discrete':value=tree_metrics.calculate_likelihood_discrete(tree,use_internal_character_states=False)
            else:value=tree_metrics.calculate_likelihood_continuous(tree,use_internal_character_states=False)
            scalar=np.asarray(value)
            if scalar.ndim!=0 or scalar.dtype.kind not in 'if':raise ValueError('likelihood必须为实数标量')
            log=float(scalar)
            if np.isnan(log) or np.isposinf(log):raise ValueError('生产likelihood返回NaN/+Inf')
            # 全树finite值始终保留，包括极负值；只有真实-inf编码成zero，随后受固定支持约束核验。
            is_zero=bool(np.isneginf(log));identity=identifier(model,assignment)
            all_ids.append(identity);zero_mask.append(is_zero)
            if not is_zero:positive_ids.append(identity);logs.append(log)
    return {'log_ids':np.asarray(all_ids,dtype=np.str_),'is_zero':np.asarray(zero_mask,dtype=bool),
            'positive_ids':np.asarray(positive_ids,dtype=np.str_),'log_values':np.asarray(logs,dtype=np.float64)}


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--inputs',required=True);parser.add_argument('--out',required=True)
    args=parser.parse_args();output=Path(args.out);output.mkdir(parents=True,exist_ok=True)
    if (output/'results.npz').exists():raise ValueError('拒绝覆盖既有科学结果')
    inputs=json.loads(Path(args.inputs).read_text(encoding='utf-8'))
    np.savez(output/'results.npz',**produce(inputs))
    print('wrote complete pattern/model typed likelihood distribution')


if __name__=='__main__':main()
