#!/usr/bin/env python3
"""独立人工 maxcut 自测；不读取 HOME、生产 source 或真实 nominal 结果。

常数策略（结构 test-first、常数探查回填）：
  * check_if_cut 的期望值由本文件里的一行谓词就地算出——它是源码一行的直译。
  * EXPECTED_GRAPH / EXPECTED_CUT_WEIGHT / EXPECTED_IMPROVED_CUT
    与 HC_AMBIGUOUS 先留空，再由**一次性探查**调用官方
    graph_utilities.construct_connectivity_graph /
    MaxCutSolver.evaluate_cut / graph_utilities.max_cut_improve_cut 回填。
    所以自测 GREEN 证明的是「validator 的独立复算 ≡ 官方实现」。
  * HC_AMBIGUOUS 是探查**搜出来**的（小图穷举），不是手工凑的——见 constant-backfill.json。
"""
import copy
import itertools
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
VALIDATOR = Path(__file__).with_name('validate.py')
MISSING = -1
ATOL = 1e-9

# --- 合成 fixture（官方 fixture 只出现在 ic/ 里）------------------------------

# s3 与 s2 逐位相同 → 逼出 drop_duplicates。
# (s0, s6) 这一对的 score 恰好是 0：char0 只有 x==0 那一支贡献 freq[9]-1 = 0，
# 另外两位两侧都是 0 被 `(x != 0 or y != 0)` 跳过——所以这条边**被省略**。
# 官方 fixture 一条零 score 都没有，这个面只有合成矩阵压得到。
SYN = {'columns': ['a', 'b', 'c'],
       'order': ['s0', 's1', 's2', 's3', 's4', 's5', 's6'],
       'rows': {'s0': [0, 0, 0], 's1': [0, 4, 0], 's2': [7, 4, 0], 's3': [7, 4, 0],
                's4': [0, 0, 5], 's5': [-1, 4, 5], 's6': [9, 0, 0]}}
WEIGHTS = {'0': {'7': 2, '9': 3}, '1': {'4': 2}, '2': {'5': 1}}

# 爬山图一律**有向**——官方 test_hill_climb 传的就是 nx.DiGraph，
# 而 max_cut_improve_cut 内部的 G.neighbors 在 DiGraph 上只给后继。
GRAPHS = {
    'hc_unique': {'directed': True, 'nodes': [0, 1, 2, 3],
                  'edges': [[0, 1, 3], [0, 2, 2], [0, 3, 2], [1, 2, 2], [1, 3, 10], [2, 3, 1]]},
    # 所有节点的改进势一开始就 <= 0，源码不动 cut、原样返回 cut.copy()。
    # 做法：让每条边都跨 cut（全部从 0 出发），非 cut 侧的节点没有后继因而势为 0。
    # 第一版我写成 [[0,1,5],[2,3,5]]，手算错了——2 与 3 同在 cut 外，那条边不跨 cut、
    # 势是 +5，反而会移动。探查回填时才发现，fixture 已改，过程留在 constant-backfill.json。
    'hc_no_move': {'directed': True, 'nodes': [0, 1, 2, 3],
                   'edges': [[0, 1, 5], [0, 2, 3], [0, 3, 2]]},
}
# 并列展开后结局不唯一的有向图——由探查**穷举搜出**（284 次命中），不是手工凑的。
# 爬山相当汇合，手工很难造出结局不唯一的例子；这个数字本身也是信息。
# 声明成可评分时 validator 必须拒绝它。
HC_AMBIGUOUS = {
    "graph": {
        "directed": True,
        "nodes": [
            0,
            1,
            2,
            3
        ],
        "edges": [
            [
                0,
                2,
                4
            ],
            [
                1,
                0,
                5
            ],
            [
                0,
                3,
                2
            ],
            [
                2,
                1,
                3
            ],
            [
                1,
                3,
                1
            ]
        ]
    },
    "initial_cut": [
        3
    ]
}
# 展开后的两种结局：[[1], [1, 2, 3]]

CUT_PROBES = [
    {'id': 'int_nodes', 'u': 2, 'v': 4, 'cut': [0, 1, 2]},
    {'id': 'same_side', 'u': 7, 'v': 8, 'cut': [7, 8, 9]},
    {'id': 'neither_side', 'u': 5, 'v': 6, 'cut': [1, 2]},
]
# compute_mutation_frequencies **不评分**：它由 GreedySolver 继承，上游
# vanillagreedy_test.py:50/85/118/166 直接断言它的值（:61 断 freq_dict[0][5] == 1），
# 而本 leaf 的 vanilla-greedy check 已经在评它、且带独立复算。再评一次是同一性质占两格。
CONFIGS = [
    {'id': 'graph_plain', 'kind': 'graph', 'matrix': 'syn', 'weights': None},
    {'id': 'graph_weighted', 'kind': 'graph', 'matrix': 'syn', 'weights': 'w'},
    {'id': 'weight_of_cut', 'kind': 'cut_weight', 'matrix': 'syn', 'weights': None,
     'cut': ['s1', 's4']},
    {'id': 'cut_unique', 'kind': 'improved_cut', 'graph': 'hc_unique', 'initial_cut': [0, 2]},
    {'id': 'cut_no_move', 'kind': 'improved_cut', 'graph': 'hc_no_move', 'initial_cut': [0]},
]

# ===== 一次性探查回填的常数 ==================================================
# 由 evidence 的 probe_constants.py 调用官方 compute_mutation_frequencies /
# construct_connectivity_graph / evaluate_cut / max_cut_improve_cut 得到；
# 本文件的结构与断言在探查之前就已写定（回填前 sha 597b978bae946cf0ae220e7484702865）。
EXPECTED_GRAPH = {
    "graph_plain": {
        "nodes": [
            "s0",
            "s1",
            "s2",
            "s4",
            "s5",
            "s6"
        ],
        "edges": [
            [
                "s0",
                "s1",
                2.0
            ],
            [
                "s0",
                "s2",
                2.0
            ],
            [
                "s0",
                "s4",
                1.0
            ],
            [
                "s0",
                "s5",
                3.0
            ],
            [
                "s1",
                "s2",
                -9.0
            ],
            [
                "s1",
                "s4",
                3.0
            ],
            [
                "s1",
                "s5",
                -8.0
            ],
            [
                "s1",
                "s6",
                2.0
            ],
            [
                "s2",
                "s4",
                3.0
            ],
            [
                "s2",
                "s5",
                -8.0
            ],
            [
                "s2",
                "s6",
                2.0
            ],
            [
                "s4",
                "s5",
                -10.0
            ],
            [
                "s4",
                "s6",
                1.0
            ],
            [
                "s5",
                "s6",
                3.0
            ]
        ]
    },
    "graph_weighted": {
        "nodes": [
            "s0",
            "s1",
            "s2",
            "s4",
            "s5",
            "s6"
        ],
        "edges": [
            [
                "s0",
                "s1",
                4.0
            ],
            [
                "s0",
                "s2",
                4.0
            ],
            [
                "s0",
                "s4",
                1.0
            ],
            [
                "s0",
                "s5",
                5.0
            ],
            [
                "s1",
                "s2",
                -18.0
            ],
            [
                "s1",
                "s4",
                5.0
            ],
            [
                "s1",
                "s5",
                -17.0
            ],
            [
                "s1",
                "s6",
                4.0
            ],
            [
                "s2",
                "s4",
                5.0
            ],
            [
                "s2",
                "s5",
                -17.0
            ],
            [
                "s2",
                "s6",
                4.0
            ],
            [
                "s4",
                "s5",
                -8.0
            ],
            [
                "s4",
                "s6",
                1.0
            ],
            [
                "s5",
                "s6",
                5.0
            ]
        ]
    }
}
EXPECTED_CUT_WEIGHT = {"weight_of_cut": -18.0}
# 官方返回的列表；本自测按集合比较（顺序是移动序列的 bookkeeping）。
EXPECTED_IMPROVED_CUT = {"cut_unique": [0, 1], "cut_no_move": [0]}
# ============================================================================


def is_cut(u, v, cut):
    """check_if_cut 的一行直译。"""
    return ((u in cut) and (v not in cut)) or ((v in cut) and (u not in cut))


def rubric(configs=None, graphs=None, **override):
    comparison = {'atol': ATOL, 'rtol': 0, 'missing_state_indicator': MISSING,
                  'weight_tables': {'w': WEIGHTS},
                  'character_matrices': {'syn': SYN},
                  'graphs': GRAPHS if graphs is None else graphs,
                  'cut_probes': CUT_PROBES,
                  'configs': CONFIGS if configs is None else configs}
    comparison.update(override)
    return {'comparison': comparison}


def artifact():
    document = {'schema_version': 1, 'cut_checks': [],
                'graph': [], 'cut_weight': [], 'improved_cut': []}
    for probe in CUT_PROBES:
        document['cut_checks'].append(
            {'probe': probe['id'], 'value': is_cut(probe['u'], probe['v'], probe['cut'])})
    for config in CONFIGS:
        kind, name = config['kind'], config['id']
        if kind == 'graph':
            document['graph'].append({'config': name, **copy.deepcopy(EXPECTED_GRAPH[name])})
        elif kind == 'cut_weight':
            document['cut_weight'].append({'config': name, 'value': EXPECTED_CUT_WEIGHT[name]})
        else:
            document['improved_cut'].append(
                {'config': name, 'side': list(EXPECTED_IMPROVED_CUT[name])})
    return document


class MaxCutContract(unittest.TestCase):
    def verdict(self, reference, candidate, policy=None, expected=None):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for side, data in [('reference', reference), ('candidate', candidate)]:
                (root / side).mkdir()
                if data is None:
                    continue
                raw = data if isinstance(data, bytes) else json.dumps(data).encode('utf-8')
                (root / side / 'results.json').write_bytes(raw)
            (root / 'rubric.json').write_text(json.dumps(policy or rubric(), allow_nan=False))
            out = root / 'result.json'
            out.write_text('{"passed": true, "stale": true}')
            done = subprocess.run(
                [sys.executable, str(VALIDATOR), '--reference', str(root / 'reference'),
                 '--candidate', str(root / 'candidate'), '--rubric', str(root / 'rubric.json'),
                 '--out', str(out)], capture_output=True, text=True)
            self.assertEqual(done.returncode, 0, done.stderr)
            wire = out.read_bytes()
            wire.decode('ascii')
            wire.decode('utf-8')
            result = json.loads(wire)
            self.assertNotIn('stale', result)
            if expected is not None:
                self.assertIs(result['passed'], expected, result)
            return result

    def mutate(self, change, side='candidate'):
        reference, candidate = artifact(), artifact()
        change(candidate if side == 'candidate' else reference)
        return self.verdict(reference, candidate, expected=False)

    def contract_failure(self, result):
        self.assertIs(result['passed'], False, result)
        self.assertIn('error_type', result)
        self.assertIsNone(result['distance'], result)
        self.assertIsNone(result['bound_fraction'], result)

    def scientific_rejection(self, result):
        self.assertIs(result['passed'], False, result)
        self.assertNotIn('error_type', result)
        self.assertIsNotNone(result['distance'], result)
        self.assertIn('measurements', result)

    # --- GREEN ---------------------------------------------------------------

    def test_green_matches_independent_recomputation(self):
        result = self.verdict(artifact(), artifact(), expected=True)
        self.assertEqual(result['distance'], 0.0)
        self.assertEqual(result['bound_fraction'], 0.0)
        self.assertEqual(result['policy'], 'pointwise')
        measured = result['measurements']
        self.assertEqual(measured['cut_checks'], 3)
        self.assertEqual(measured['graph_configs'], 2)
        self.assertEqual(measured['cut_weight_configs'], 1)
        self.assertEqual(measured['improved_cut_configs'], 2)

    def test_zero_score_pair_is_absent_from_the_graph(self):
        """(s0, s6) 的 score 恰好为 0，源码 `if score != 0` 因此**不连这条边**。

        官方 fixture 一条零 score 都没有，这个分支只有合成矩阵压得到。
        """
        edges = {tuple(sorted(e[:2])) for e in EXPECTED_GRAPH['graph_plain']['edges']}
        self.assertNotIn(('s0', 's6'), edges)
        self.assertIn('s0', EXPECTED_GRAPH['graph_plain']['nodes'])
        self.assertIn('s6', EXPECTED_GRAPH['graph_plain']['nodes'])

    def test_duplicate_row_is_dropped_before_the_graph(self):
        self.assertNotIn('s3', EXPECTED_GRAPH['graph_plain']['nodes'])

    def test_no_move_config_returns_the_initial_cut(self):
        self.assertEqual(sorted(EXPECTED_IMPROVED_CUT['cut_no_move']), [0])

    # --- check_if_cut ---------------------------------------------------------

    def test_flipped_cut_check_is_rejected(self):
        def change(document):
            document['cut_checks'][0]['value'] = not document['cut_checks'][0]['value']
        self.scientific_rejection(self.mutate(change))

    def test_missing_cut_probe_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d['cut_checks'].pop()))

    def test_non_boolean_cut_check_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d['cut_checks'][0].update(value=1)))

    # frequencies 的故障由本 leaf 的 vanilla-greedy check 评分（同一个继承来的方法）；
    # 这里只保留「多出一节」的通用 schema 收紧测试（见 test_extra_section_is_contract_failure）。

    # --- 连通图 ---------------------------------------------------------------

    def test_graph_extra_edge_is_rejected(self):
        def change(document):
            document['graph'][0]['edges'].append(['s0', 's6', 1])
        self.scientific_rejection(self.mutate(change))

    def test_graph_keeping_the_zero_score_edge_is_rejected(self):
        """把 score == 0 的那条边也连上——`if score != 0` 这一支的直接故障。"""
        def change(document):
            document['graph'][0]['edges'].append(['s0', 's6', 0])
        self.scientific_rejection(self.mutate(change))

    def test_graph_missing_edge_is_rejected(self):
        self.scientific_rejection(self.mutate(lambda d: d['graph'][0]['edges'].pop()))

    def test_graph_wrong_weight_is_rejected(self):
        def change(document):
            document['graph'][0]['edges'][0][2] += 1
        result = self.mutate(change)
        self.scientific_rejection(result)
        self.assertGreaterEqual(result['distance'], 1.0)

    def test_graph_sign_flip_is_rejected(self):
        """共享突变是**强负**连接、差异是正连接；符号弄反是移植的经典错误。"""
        def change(document):
            for row in document['graph']:
                row['edges'] = [[u, v, -w] for u, v, w in row['edges']]
        self.scientific_rejection(self.mutate(change))

    def test_graph_keeping_the_duplicate_row_is_rejected(self):
        def change(document):
            document['graph'][0]['nodes'].append('s3')
        self.scientific_rejection(self.mutate(change))

    def test_graph_edge_endpoints_may_be_written_either_way(self):
        reference, candidate = artifact(), artifact()
        for row in candidate['graph']:
            row['edges'] = [[v, u, w] for u, v, w in row['edges']]
        self.verdict(reference, candidate, expected=True)

    def test_graph_duplicate_edge_is_contract_failure(self):
        def change(document):
            document['graph'][0]['edges'].append(copy.deepcopy(document['graph'][0]['edges'][0]))
        self.contract_failure(self.mutate(change))

    # --- evaluate_cut ---------------------------------------------------------

    def test_wrong_cut_weight_is_rejected(self):
        def change(document):
            document['cut_weight'][0]['value'] += 1.0
        result = self.mutate(change)
        self.scientific_rejection(result)
        self.assertGreaterEqual(result['distance'], 1.0)

    def test_cut_weight_within_bound_passes(self):
        reference, candidate = artifact(), artifact()
        candidate['cut_weight'][0]['value'] += ATOL / 4
        result = self.verdict(reference, candidate, expected=True)
        self.assertGreater(result['distance'], 0.0)
        self.assertLess(result['bound_fraction'], 1.0)

    # --- 爬山划分 -------------------------------------------------------------

    def test_cut_wrong_side_is_rejected(self):
        def change(document):
            row = next(r for r in document['improved_cut'] if r['config'] == 'cut_unique')
            row['side'] = [0]
        self.scientific_rejection(self.mutate(change))

    def test_cut_reordered_side_passes(self):
        reference, candidate = artifact(), artifact()
        for row in candidate['improved_cut']:
            row['side'] = list(reversed(row['side']))
        self.verdict(reference, candidate, expected=True)

    def test_cut_duplicate_member_is_contract_failure(self):
        def change(document):
            row = document['improved_cut'][0]
            row['side'] = row['side'] + row['side'][:1]
        self.contract_failure(self.mutate(change))

    def test_cut_unknown_node_is_contract_failure(self):
        def change(document):
            document['improved_cut'][0]['side'] = document['improved_cut'][0]['side'] + [99]
        self.contract_failure(self.mutate(change))

    def test_undirected_graph_is_refused(self):
        """官方 test 传的是 DiGraph；无向语义给出不同答案且并列展开不唯一，不接受。"""
        graphs = copy.deepcopy(GRAPHS)
        graphs['hc_unique']['directed'] = False
        self.contract_failure(self.verdict(artifact(), artifact(),
                                           policy=rubric(graphs=graphs)))

    def test_ambiguous_graph_declared_graded_is_refused(self):
        """validator 不信 rubric：自己展开并列，结局不唯一就拒绝这条 rubric。"""
        graphs = copy.deepcopy(GRAPHS)
        graphs['hc_ambiguous'] = copy.deepcopy(HC_AMBIGUOUS['graph'])
        configs = CONFIGS + [{'id': 'cut_ambiguous', 'kind': 'improved_cut',
                              'graph': 'hc_ambiguous',
                              'initial_cut': list(HC_AMBIGUOUS['initial_cut'])}]
        self.contract_failure(self.verdict(artifact(), artifact(),
                                           policy=rubric(configs=configs, graphs=graphs)))

    # --- rubric 与产物合同 ----------------------------------------------------

    def test_nonzero_rtol_is_refused(self):
        self.contract_failure(self.verdict(artifact(), artifact(), policy=rubric(rtol=1e-9)))

    def test_negative_atol_is_refused(self):
        self.contract_failure(self.verdict(artifact(), artifact(), policy=rubric(atol=-1.0)))

    def test_missing_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), None))

    def test_missing_section_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d.pop('graph')))

    def test_extra_section_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d.update(extra=[])))

    def test_wrong_schema_version_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d.update(schema_version=2)))

    def test_duplicate_json_key_is_contract_failure(self):
        raw = json.dumps(artifact()).encode('utf-8')
        broken = raw.replace(b'"schema_version": 1', b'"schema_version": 1, "schema_version": 1', 1)
        self.contract_failure(self.verdict(artifact(), broken))

    def test_nan_literal_is_contract_failure(self):
        document = artifact()
        document['cut_weight'][0]['value'] = 0.0
        raw = json.dumps(document).encode('utf-8')
        broken = raw.replace(b'"value": 0.0', b'"value": NaN', 1)
        self.assertIn(b'NaN', broken)
        self.contract_failure(self.verdict(artifact(), broken))

    def test_oversize_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), b' ' * 5_000_000 + b'{}'))

    def test_non_utf8_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), b'{"schema_version": 1, "s": "\xff\xfe"}'))

    def test_both_sides_wrong_is_still_rejected(self):
        """三向比较的关键一格：双侧一致但都与独立复算不符。"""
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['cut_weight'][0]['value'] += 1.0
        self.scientific_rejection(self.verdict(reference, candidate, expected=False))

    def test_reference_side_is_graded_too(self):
        def change(document):
            document['cut_weight'][0]['value'] += 1.0
        self.scientific_rejection(self.mutate(change, side='reference'))


if __name__ == '__main__':
    unittest.main()
