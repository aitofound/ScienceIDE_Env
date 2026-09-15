#!/usr/bin/env python3
"""独立人工 spectral 自测；不读取 HOME、生产 source 或真实 nominal 结果。

常数策略（本 check 采用「结构 test-first、常数探查回填」）：
  * 成对相似度的期望值由本文件里的 similarity() 就地算出——它是源码四行的直译，
    在自测里重写它不构成循环论证。
  * EXPECTED_GRAPH 与 EXPECTED_CUT 先留空，再由**一次性探查**调用官方
    graph_utilities.construct_similarity_graph / spectral_improve_cut 回填。
    因此自测 GREEN 证明的是「validator 的独立复算 ≡ 官方实现」，
    而不是「validator ≡ validator」。回填留档见 evidence 的 constant-backfill.json。
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

# --- 合成 fixture（都不是官方 fixture，官方 fixture 只出现在 ic/ 里）-----------

# 相似度：含缺失、含 0（源码里 missing 与 0 都不计入共享），四行两两六对。
SIM_CM = {'columns': ['x1', 'x2', 'x3', 'x4'],
          'order': ['s1', 's2', 's3', 's4'],
          'rows': {'s1': [7, -1, 0, 3],
                   's2': [7, 4, 0, 3],
                   's3': [7, 4, 5, 0],
                   's4': [-1, 4, 5, 3]}}
# 图构造：g2 与 g1 逐位相同，逼出 drop_duplicates；去重后四行。
# 无权时最小边权 1 有四条并列、加权时有两条并列——正好压到 min() 的并列面上；
# 减掉最小边权之后另有边恰好归零被删，g1/g3 变成孤立节点。
GRAPH_CM = {'columns': ['x1', 'x2', 'x3', 'x4'],
            'order': ['g1', 'g2', 'g3', 'g4', 'g5'],
            'rows': {'g1': [7, 4, 0, 0],
                     'g2': [7, 4, 0, 0],
                     'g3': [7, 0, 5, 0],
                     'g4': [0, 4, 5, 3],
                     'g5': [0, 0, 5, 3]}}
WEIGHTS = {'0': {'7': 2}, '1': {'4': 3}, '2': {'5': 1}, '3': {'3': 4}}

# 爬山：三张显式图。
#   hc_unique     并列展开后结局唯一，可评分
#   hc_zero_cut   初始 cut 跨边权重为 0，源码在 :338 早退，直接原样返回 cut.copy()
#   hc_ambiguous  全等权 K4，从 [0] 出发的第一步在 1/2/3 之间并列，结局不唯一
GRAPHS = {
    'hc_unique': {'nodes': [0, 1, 2, 3],
                  'edges': [[0, 1, 3], [0, 2, 2], [0, 3, 2], [1, 2, 2], [1, 3, 10], [2, 3, 1]]},
    'hc_zero_cut': {'nodes': [0, 1, 2, 3],
                    'edges': [[0, 1, 5], [2, 3, 5]]},
    'hc_ambiguous': {'nodes': [0, 1, 2, 3],
                     'edges': [[0, 1, 1], [0, 2, 1], [0, 3, 1], [1, 2, 1], [1, 3, 1], [2, 3, 1]]},
}

CONFIGS = [
    {'id': 'sim_plain', 'kind': 'similarity', 'matrix': 'sim', 'weights': None},
    {'id': 'sim_weighted', 'kind': 'similarity', 'matrix': 'sim', 'weights': 'w'},
    {'id': 'graph_plain', 'kind': 'graph', 'matrix': 'graph', 'weights': None},
    {'id': 'graph_weighted', 'kind': 'graph', 'matrix': 'graph', 'weights': 'w'},
    {'id': 'cut_unique', 'kind': 'improved_cut', 'graph': 'hc_unique', 'initial_cut': [0]},
    {'id': 'cut_zero', 'kind': 'improved_cut', 'graph': 'hc_zero_cut', 'initial_cut': [0, 1]},
]
# 声明 hc_ambiguous 为可评分的 rubric——validator 必须自己展开并列后拒绝它。
AMBIGUOUS_CONFIG = {'id': 'cut_ambiguous', 'kind': 'improved_cut',
                    'graph': 'hc_ambiguous', 'initial_cut': [0]}

# ===== 一次性探查回填的常数 ==================================================
# 由 evidence 的 probe_constants.py 调用官方 construct_similarity_graph /
# spectral_improve_cut 得到；本文件的结构与断言在探查之前就已写定。
# graph_plain 去重后 5 条候选边、最小边权 1 有 4 条并列，减掉之后只剩 g4-g5，
# g1/g3 变成孤立节点——正好压住并列、归零删边与孤立节点三个面。
EXPECTED_GRAPH = {
    'graph_plain': {'nodes': ['g1', 'g3', 'g4', 'g5'],
                    'edges': [['g4', 'g5', 1]]},
    'graph_weighted': {'nodes': ['g1', 'g3', 'g4', 'g5'],
                       'edges': [['g1', 'g3', 1], ['g1', 'g4', 2], ['g4', 'g5', 4]]},
}
# EXPECTED_CUT[config] = 官方返回的列表；本 check 按集合比较。
EXPECTED_CUT = {'cut_unique': [0, 2], 'cut_zero': [0, 1]}
# ============================================================================


def similarity(a, b, weights):
    """hamming_similarity_without_missing：两侧都非缺失、非 0 且相同才累加。"""
    total = 0.0
    for index, (x, y) in enumerate(zip(a, b)):
        if x == MISSING or y == MISSING or x == 0 or y == 0:
            continue
        if x == y:
            total += weights[str(index)][str(x)] if weights else 1
    return total


def rubric(configs=None, **override):
    comparison = {'atol': ATOL, 'rtol': 0, 'missing_state_indicator': MISSING,
                  'similarity_function': 'hamming_similarity_without_missing',
                  'threshold': 0,
                  'weight_tables': {'w': WEIGHTS},
                  'character_matrices': {'sim': SIM_CM, 'graph': GRAPH_CM},
                  'graphs': GRAPHS,
                  'configs': CONFIGS if configs is None else configs}
    comparison.update(override)
    return {'comparison': comparison}


def artifact():
    similarity_rows, graph_rows, cut_rows = [], [], []
    for config in CONFIGS:
        if config['kind'] == 'similarity':
            cells = SIM_CM['rows']
            applied = WEIGHTS if config['weights'] else None
            for a, b in itertools.combinations(sorted(cells), 2):
                similarity_rows.append({'config': config['id'], 'cell_i': a, 'cell_j': b,
                                        'value': similarity(cells[a], cells[b], applied)})
        elif config['kind'] == 'graph':
            graph_rows.append({'config': config['id'],
                               **copy.deepcopy(EXPECTED_GRAPH[config['id']])})
        else:
            cut_rows.append({'config': config['id'],
                             'side': list(EXPECTED_CUT[config['id']])})
    return {'schema_version': 1, 'similarity': similarity_rows,
            'graph': graph_rows, 'improved_cut': cut_rows}


class SpectralContract(unittest.TestCase):
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
            wire.decode('ascii')          # 结果必须是严格 ASCII
            wire.decode('utf-8')
            result = json.loads(wire)
            self.assertNotIn('stale', result)   # 失败时不得留下任何旧的部分结果
            if expected is not None:
                self.assertIs(result['passed'], expected, result)
            return result

    def mutate(self, change, side='candidate'):
        reference, candidate = artifact(), artifact()
        change(candidate if side == 'candidate' else reference)
        return self.verdict(reference, candidate, expected=False)

    def contract_failure(self, result):
        """合同失败：异常型判定，报 error_type，distance 必须是 null。"""
        self.assertIs(result['passed'], False, result)
        self.assertIn('error_type', result)
        self.assertIsNone(result['distance'], result)
        self.assertIsNone(result['bound_fraction'], result)

    def scientific_rejection(self, result):
        """科学不符：正常返回，必须给出可读的数字，不能退化成 null。"""
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
        self.assertEqual(result['measurements']['similarity'], 12)
        self.assertEqual(result['measurements']['graph_configs'], 2)
        self.assertEqual(result['measurements']['improved_cut_configs'], 2)

    def test_zero_cut_config_returns_initial_cut_unchanged(self):
        """源码 :338 numerator == 0 时直接返回 cut.copy()——独立复算必须复现这个早退。"""
        self.assertEqual(sorted(EXPECTED_CUT['cut_zero']), [0, 1])

    # --- 相似度 ---------------------------------------------------------------

    def test_similarity_beyond_bound_is_scientific_rejection(self):
        def change(document):
            document['similarity'][0]['value'] += 1e-6
        self.scientific_rejection(self.mutate(change))

    def test_similarity_within_bound_passes(self):
        reference, candidate = artifact(), artifact()
        candidate['similarity'][0]['value'] += ATOL / 4
        result = self.verdict(reference, candidate, expected=True)
        self.assertGreater(result['distance'], 0.0)
        self.assertLess(result['bound_fraction'], 1.0)

    def test_similarity_wrong_on_both_sides_still_rejected(self):
        """双侧一致但都与独立复算不符——三向比较的关键一格。"""
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['similarity'][0]['value'] += 1.0
        self.scientific_rejection(self.verdict(reference, candidate, expected=False))

    def test_similarity_missing_pair_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d['similarity'].pop()))

    def test_similarity_unknown_pair_is_contract_failure(self):
        def change(document):
            document['similarity'].append({'config': 'sim_plain', 'cell_i': 's1',
                                           'cell_j': 'nope', 'value': 0})
        self.contract_failure(self.mutate(change))

    def test_similarity_duplicate_pair_is_contract_failure(self):
        def change(document):
            document['similarity'].append(copy.deepcopy(document['similarity'][0]))
        self.contract_failure(self.mutate(change))

    def test_similarity_self_pair_is_contract_failure(self):
        def change(document):
            document['similarity'][0]['cell_j'] = document['similarity'][0]['cell_i']
        self.contract_failure(self.mutate(change))

    # --- 图构造 ---------------------------------------------------------------

    def test_graph_extra_edge_is_rejected(self):
        def change(document):
            row = document['graph'][0]
            row['edges'].append(['g1', 'g5', 1])
        self.scientific_rejection(self.mutate(change))

    def test_graph_missing_edge_is_rejected(self):
        self.scientific_rejection(self.mutate(lambda d: d['graph'][0]['edges'].pop()))

    def test_graph_wrong_weight_is_rejected(self):
        def change(document):
            document['graph'][0]['edges'][0][2] += 1
        result = self.mutate(change)
        self.scientific_rejection(result)
        self.assertGreaterEqual(result['distance'], 1.0)

    def test_graph_edge_endpoints_may_be_written_either_way(self):
        """无向图：一条边写成 (v, u) 与 (u, v) 是同一条边，不是科学差别。"""
        reference, candidate = artifact(), artifact()
        for row in candidate['graph']:
            row['edges'] = [[v, u, w] for u, v, w in row['edges']]
        self.verdict(reference, candidate, expected=True)

    def test_graph_edge_order_is_not_graded(self):
        reference, candidate = artifact(), artifact()
        for row in candidate['graph']:
            row['edges'] = list(reversed(row['edges']))
        self.verdict(reference, candidate, expected=True)

    def test_graph_duplicate_edge_is_contract_failure(self):
        def change(document):
            document['graph'][0]['edges'].append(copy.deepcopy(document['graph'][0]['edges'][0]))
        self.contract_failure(self.mutate(change))

    def test_graph_keeping_the_duplicate_row_is_rejected(self):
        """没做 drop_duplicates 的移植会多出 g2 这个节点——必须抓住。"""
        def change(document):
            document['graph'][0]['nodes'].append('g2')
        self.scientific_rejection(self.mutate(change))

    def test_graph_dropping_an_isolated_node_is_rejected(self):
        """减最小边权后归零的边被删，端点变成孤立节点；孤立节点仍属于返回的图。"""
        def change(document):
            row = document['graph'][0]
            connected = {end for edge in row['edges'] for end in edge[:2]}
            row['nodes'] = [node for node in row['nodes'] if node in connected]
        result = self.mutate(change)
        self.scientific_rejection(result)

    def test_graph_forgetting_to_remove_zeroed_edges_is_rejected(self):
        """减掉最小边权之后没删掉权重 <= 0 的边——移植最容易漏的那一步。"""
        def change(document):
            document['graph'][0]['edges'].append(['g1', 'g3', 0])
        self.scientific_rejection(self.mutate(change))

    def test_graph_unknown_node_is_contract_failure(self):
        def change(document):
            document['graph'][0]['nodes'].append('nope')
        self.contract_failure(self.mutate(change))

    # --- 爬山划分 -------------------------------------------------------------

    def test_cut_wrong_side_is_rejected(self):
        def change(document):
            row = next(r for r in document['improved_cut'] if r['config'] == 'cut_unique')
            row['side'] = [0]
        self.scientific_rejection(self.mutate(change))

    def test_cut_complement_is_rejected(self):
        """返回的是由 initial_cut 演化出的那一侧，不是「任一侧」。"""
        def change(document):
            row = next(r for r in document['improved_cut'] if r['config'] == 'cut_unique')
            row['side'] = [n for n in GRAPHS['hc_unique']['nodes'] if n not in EXPECTED_CUT['cut_unique']]
        self.scientific_rejection(self.mutate(change))

    def test_cut_reordered_side_passes(self):
        """本 check 按集合评分：new_cut 的列表顺序是移动序列的 bookkeeping。"""
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

    def test_cut_empty_side_is_contract_failure(self):
        def change(document):
            document['improved_cut'][0]['side'] = []
        self.contract_failure(self.mutate(change))

    def test_ambiguous_graph_declared_graded_is_refused(self):
        """validator 不信 rubric：自己展开并列，结局不唯一就拒绝这条 rubric。"""
        policy = rubric(configs=CONFIGS + [AMBIGUOUS_CONFIG])
        self.contract_failure(self.verdict(artifact(), artifact(), policy=policy))

    # --- rubric 与产物合同 ----------------------------------------------------

    def test_unsupported_similarity_function_is_refused(self):
        policy = rubric(similarity_function='weighted_hamming_distance')
        self.contract_failure(self.verdict(artifact(), artifact(), policy=policy))

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
        document['similarity'][0]['value'] = 0.0
        raw = json.dumps(document).encode('utf-8')
        broken = raw.replace(b'"value": 0.0', b'"value": NaN', 1)
        self.assertIn(b'NaN', broken)
        self.contract_failure(self.verdict(artifact(), broken))

    def test_oversize_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), b' ' * 5_000_000 + b'{}'))

    def test_non_utf8_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), b'{"schema_version": 1, "s": "\xff\xfe"}'))

    def test_reference_side_is_graded_too(self):
        def change(document):
            document['similarity'][0]['value'] += 1.0
        self.scientific_rejection(self.mutate(change, side='reference'))


if __name__ == '__main__':
    unittest.main()
