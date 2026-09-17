#!/usr/bin/env python3
"""直接运行生产估计 API；没有 assertion recorder，也不读取优化器私有状态。"""
import argparse
import json
from pathlib import Path

import networkx as nx
import numpy as np

from cassiopeia.data import CassiopeiaTree
from cassiopeia.tools import IIDExponentialMLE


def produce(inputs,out,backend='upstream'):
    cases=json.loads(Path(inputs).read_text())['cases']
    out=Path(out);out.mkdir(parents=True,exist_ok=True)
    for case in cases:
        graph=nx.DiGraph()
        graph.add_nodes_from(case['states'])
        graph.add_edges_from(case['edges'])
        tree=CassiopeiaTree(tree=graph)
        tree.set_all_character_states(case['states'])
        model=IIDExponentialMLE(minimum_branch_length=case['minimum_branch_length'],
                               relative_mutation_rates=case['relative_rates'],
                               solver=case['solver'] if backend=='upstream' else backend)
        model.estimate_branch_lengths(tree)
        nodes=list(tree.nodes);edges=list(tree.edges)
        rates=model.mutation_rate
        if np.isscalar(rates): rates=[rates]*len(case['states'][nodes[0]])
        np.savez(out/(case['id']+'.npz'),node_ids=np.asarray(nodes,dtype=str),
                 times=np.array([tree.get_time(n) for n in nodes],dtype=np.float64),
                 edge_ids=np.asarray(edges,dtype=str),
                 branch_lengths=np.array([tree.get_branch_length(p,c) for p,c in edges],dtype=np.float64),
                 site_ids=np.arange(len(rates),dtype=np.int64),mutation_rates=np.asarray(rates,dtype=np.float64),
                 log_likelihood=np.array([model.log_likelihood],dtype=np.float64))
        print(case['id'],flush=True)


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--inputs',required=True);parser.add_argument('--out',required=True)
    parser.add_argument('--backend',choices=['upstream','ECOS','SCS'],default='upstream')
    args=parser.parse_args()
    produce(args.inputs,args.out,args.backend)

if __name__=='__main__':
    main()
