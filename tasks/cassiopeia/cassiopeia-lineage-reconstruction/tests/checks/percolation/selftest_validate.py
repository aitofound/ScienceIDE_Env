#!/usr/bin/env python3
"""独立人工 percolation 自测；不读取HOME、生产source或真实nominal结果。"""
import copy
import itertools
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
VALIDATOR = Path(__file__).with_name('validate.py')
MISSING = -1
ATOL = 1e-9

# 立刻就是两个分量：pq 与 rs 各自相连、跨组相似度 0，不进 joining solver。
PAIRS = {'p': [1, 1, 0], 'q': [1, 1, 0], 'r': [2, 2, 0], 's': [2, 2, 0]}
# 四个单点分量：weighted-hamming 的 NJ 合并唯一（{a,b,c} | {d}），
# 同一 fixture 交给 VanillaGreedy 却有 6 种分组——一个 fixture 同时覆盖两条路径。
FOUR = {'a': [1, -1, 0, 0, 9], 'b': [2, 2, 0, 0, 9], 'c': [3, 3, 3, 0, 9], 'd': [4, 0, 0, 0, 9]}
# 三个单点分量、完全对称：任何 joiner 都给出 3 种分组。
SYMMETRIC = {'a': [1, 0, 7], 'b': [2, 2, 7], 'c': [3, 3, 7]}
# 一条边都没有：percolate 走 (samples, []) 那个退化分支。
ISOLATED = {'a': [1, 0, 0], 'b': [0, 2, 0], 'c': [0, 0, 3]}
PRIORS = {'0': {'1': 0.5, '2': 0.4, '3': 0.3, '4': 0.2, '5': 0.6},
          '1': {'1': 0.2, '2': 0.8, '3': 0.3, '4': 0.7},
          '2': {'3': 0.3, '7': 0.9},
          '3': {'5': 0.5},
          '4': {'9': 0.9}}
MATRICES = {'pairs': {'columns': ['x1', 'x2', 'x3'], 'rows': PAIRS},
            'four': {'columns': ['x1', 'x2', 'x3', 'x4', 'x5'], 'rows': FOUR},
            'symmetric': {'columns': ['x1', 'x2', 'x3'], 'rows': SYMMETRIC},
            'isolated': {'columns': ['x1', 'x2', 'x3'], 'rows': ISOLATED}}
NJ = 'neighbor_joining:weighted_hamming'
CONFIGS = [
    {'id': 'pairs_nojoin', 'matrix': 'pairs', 'joiner': NJ,
     'use_priors': False, 'partition_graded': True},
    {'id': 'pairs_weighted', 'matrix': 'pairs', 'joiner': NJ,
     'use_priors': True, 'partition_graded': True},
    {'id': 'four_nj', 'matrix': 'four', 'joiner': NJ,
     'use_priors': False, 'partition_graded': True},
    {'id': 'symmetric_ungraded', 'matrix': 'symmetric', 'joiner': NJ,
     'use_priors': False, 'partition_graded': False},
]
# 这两个是本自测已知的事实（由一次性探查确定并锁死在这里）：
EXPECTED_PARTITIONS = {'pairs_nojoin': [['p', 'q'], ['r', 's']],
                       'pairs_weighted': [['p', 'q'], ['r', 's']],
                       'four_nj': [['a', 'b', 'c'], ['d']]}


def weights_of(priors):
    return {int(c): {int(s): -math.log(p) for s, p in table.items()} for c, table in priors.items()}


def similarity(a, b, weights):
    total = 0.0
    for index, (x, y) in enumerate(zip(a, b)):
        if x == MISSING or y == MISSING or x == 0 or y == 0:
            continue
        if x == y:
            total += weights[index][x] if weights else 1
    return total


def rubric(configs=None, priors=None, threshold=0, similarity_function=None):
    return {'comparison': {
        'atol': ATOL, 'rtol': 0, 'missing_state_indicator': MISSING, 'threshold': threshold,
        'similarity_function': similarity_function or 'hamming_similarity_without_missing',
        'prior_transformation': 'negative_log',
        'character_matrices': MATRICES, 'priors': PRIORS if priors is None else priors,
        'configs': CONFIGS if configs is None else configs}}


def artifact():
    weights = weights_of(PRIORS)
    similarity_rows, partition_rows = [], []
    for config in CONFIGS:
        cells = MATRICES[config['matrix']]['rows']
        applied = weights if config['use_priors'] else None
        for a, b in itertools.combinations(sorted(cells), 2):
            similarity_rows.append({'config': config['id'], 'cell_i': a, 'cell_j': b,
                                    'value': similarity(cells[a], cells[b], applied)})
        if config['partition_graded']:
            partition_rows.append({'config': config['id'],
                                   'sides': copy.deepcopy(EXPECTED_PARTITIONS[config['id']])})
    return {'schema_version': 1, 'similarity': similarity_rows, 'partition': partition_rows}


class PercolationContract(unittest.TestCase):
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

    def mutate(self, change, expected=False, side='candidate'):
        reference, candidate = artifact(), artifact()
        change(candidate if side == 'candidate' else reference)
        return self.verdict(reference, candidate, expected=expected)

    # --- 合法表示等价 ---

    def test_identical_complete_artifact_passes(self):
        result = self.verdict(artifact(), artifact(), expected=True)
        self.assertEqual(result['distance'], 0)
        self.assertEqual(result['measurements']['partition'], 3)

    def test_row_order_is_not_graded(self):
        def reorder(document):
            document['similarity'].reverse()
            document['partition'].reverse()
        self.mutate(reorder, expected=True)

    def test_unordered_pair_and_side_membership_are_symmetric(self):
        def flip(document):
            for row in document['similarity']:
                row['cell_i'], row['cell_j'] = row['cell_j'], row['cell_i']
            for row in document['partition']:
                row['sides'] = list(reversed([list(reversed(s)) for s in row['sides']]))
        self.mutate(flip, expected=True)

    def test_difference_inside_the_bound_passes(self):
        self.mutate(lambda d: d['similarity'][0].__setitem__(
            'value', d['similarity'][0]['value'] + ATOL / 4), expected=True)

    # --- 相似度 ---

    def test_difference_outside_the_bound_reports_a_number(self):
        result = self.mutate(lambda d: d['similarity'][0].__setitem__(
            'value', d['similarity'][0]['value'] + ATOL * 100))
        self.assertNotIn('error_type', result)
        self.assertGreater(result['distance'], ATOL)
        self.assertGreater(result['bound_fraction'], 1.0)
        self.assertGreater(result['values_over_bound'], 0)
        self.assertEqual(result['categorical_mismatches'], 0)

    def test_unweighted_value_in_the_weighted_config_is_rejected(self):
        def unweighted(document):
            for row in document['similarity']:
                if row['config'] == 'pairs_weighted':
                    row['value'] = similarity(PAIRS[row['cell_i']], PAIRS[row['cell_j']], None)
        self.mutate(unweighted)

    def test_counting_shared_zeros_is_rejected(self):
        def wrong(document):
            for row in document['similarity']:
                if row['config'] == 'pairs_nojoin' and {row['cell_i'], row['cell_j']} == {'p', 'r'}:
                    row['value'] += 1
                    return
            self.fail('人工 fixture 必须含有共享 0 的一对')
        self.mutate(wrong)

    def test_counting_shared_missing_is_rejected(self):
        def wrong(document):
            for row in document['similarity']:
                if row['config'] == 'four_nj' and {row['cell_i'], row['cell_j']} == {'a', 'b'}:
                    row['value'] += 1
                    return
            self.fail('人工 fixture 必须含有带缺失的一对')
        self.mutate(wrong)

    # --- 划分 ---

    def test_wrong_partition_is_rejected(self):
        result = self.mutate(lambda d: d['partition'][0].__setitem__('sides', [['p', 'r'], ['q', 's']]))
        self.assertNotIn('error_type', result)
        self.assertEqual(result['values_over_bound'], 0)
        self.assertGreater(result['categorical_mismatches'], 0)
        self.assertIn('划分', result['reason'])

    def test_wrong_joined_partition_is_rejected(self):
        def wrong(document):
            for row in document['partition']:
                if row['config'] == 'four_nj':
                    row['sides'] = [['a', 'b'], ['c', 'd']]
                    return
        self.mutate(wrong)

    def test_partition_of_an_ungraded_config_must_not_be_delivered(self):
        def leak(document):
            row = copy.deepcopy(document['partition'][0])
            row['config'] = 'symmetric_ungraded'
            row['sides'] = [['a'], ['b', 'c']]
            document['partition'].append(row)
        self.mutate(leak)

    def test_missing_partition_for_a_graded_config_is_rejected(self):
        self.mutate(lambda d: d['partition'].pop())

    # --- validator 自己重做前置排查 ---

    def test_declaring_a_tie_ambiguous_config_as_graded_is_rejected(self):
        configs = copy.deepcopy(CONFIGS)
        for config in configs:
            if config['id'] == 'symmetric_ungraded':
                config['partition_graded'] = True
        self.verdict(artifact(), artifact(), policy=rubric(configs=configs), expected=False)

    def test_same_fixture_under_a_tie_ambiguous_joiner_is_rejected(self):
        """four 这张矩阵在 weighted-hamming 下唯一，换成 VanillaGreedy 就有多种分组。"""
        configs = copy.deepcopy(CONFIGS)
        for config in configs:
            if config['id'] == 'four_nj':
                config['joiner'] = 'vanilla_greedy'
        self.verdict(artifact(), artifact(), policy=rubric(configs=configs), expected=False)

    def test_degenerate_no_edge_config_declared_graded_is_rejected(self):
        configs = copy.deepcopy(CONFIGS) + [
            {'id': 'isolated', 'matrix': 'isolated', 'joiner': NJ,
             'use_priors': False, 'partition_graded': True}]
        self.verdict(artifact(), artifact(), policy=rubric(configs=configs), expected=False)

    # --- 完整覆盖与 schema ---

    def test_dropping_a_similarity_pair_is_rejected(self):
        self.mutate(lambda d: d['similarity'].pop())

    def test_duplicate_identity_is_not_silently_deduplicated(self):
        self.mutate(lambda d: d['similarity'].append(copy.deepcopy(d['similarity'][0])))

    def test_row_under_the_wrong_config_is_rejected(self):
        self.mutate(lambda d: d['similarity'][0].__setitem__('config', 'four_nj'))

    def test_missing_section_is_rejected(self):
        self.mutate(lambda d: d.pop('partition'))

    def test_missing_row_field_is_rejected(self):
        self.mutate(lambda d: d['similarity'][0].pop('value'))

    def test_extra_row_field_is_rejected(self):
        self.mutate(lambda d: d['partition'][0].__setitem__('extra', 1))

    def test_summary_instead_of_rows_is_rejected(self):
        self.mutate(lambda d: d.__setitem__('similarity', len(d['similarity'])))

    def test_unsupported_schema_version_is_rejected(self):
        self.mutate(lambda d: d.__setitem__('schema_version', 2))

    def test_negative_or_non_numeric_similarity_is_rejected(self):
        for bad in (-1.0, True, '1.0', 1e400):
            with self.subTest(bad=bad):
                self.mutate(lambda d, bad=bad: d['similarity'][0].__setitem__('value', bad))

    def test_partition_side_with_an_unknown_cell_is_rejected(self):
        self.mutate(lambda d: d['partition'][0]['sides'][0].append('zz'))

    def test_partition_that_does_not_cover_every_cell_is_rejected(self):
        self.mutate(lambda d: d['partition'][0].__setitem__('sides', [['p'], ['q']]))

    def test_partition_with_overlapping_sides_is_rejected(self):
        self.mutate(lambda d: d['partition'][0].__setitem__('sides', [['p', 'q', 'r'], ['r', 's']]))

    def test_partition_with_three_sides_is_rejected(self):
        self.mutate(lambda d: d['partition'][0].__setitem__('sides', [['p'], ['q'], ['r', 's']]))

    # --- 双侧同错与不可信 rubric ---

    def test_both_sides_agreeing_on_a_wrong_value_is_still_rejected(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['similarity'][0]['value'] += 1.0
        result = self.verdict(reference, candidate, expected=False)
        self.assertNotIn('error_type', result)
        self.assertGreater(result['distance'], 0.5)

    def test_both_sides_agreeing_on_a_wrong_partition_is_still_rejected(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['partition'][0]['sides'] = [['p', 'r'], ['q', 's']]
        self.verdict(reference, candidate, expected=False)

    def test_wrong_reference_alone_is_rejected(self):
        self.mutate(lambda d: d['partition'][0].__setitem__('sides', [['p', 'r'], ['q', 's']]),
                    side='reference')

    def test_config_referring_to_an_unknown_matrix_is_rejected(self):
        configs = copy.deepcopy(CONFIGS)
        configs[0]['matrix'] = 'nowhere'
        self.verdict(artifact(), artifact(), policy=rubric(configs=configs), expected=False)

    def test_unknown_joiner_is_rejected(self):
        configs = copy.deepcopy(CONFIGS)
        configs[0]['joiner'] = 'something_else'
        self.verdict(artifact(), artifact(), policy=rubric(configs=configs), expected=False)

    def test_unknown_similarity_function_is_rejected(self):
        self.verdict(artifact(), artifact(),
                     policy=rubric(similarity_function='hamming_distance'), expected=False)

    def test_non_zero_threshold_is_rejected(self):
        self.verdict(artifact(), artifact(), policy=rubric(threshold=1), expected=False)

    def test_prior_outside_the_unit_interval_is_rejected(self):
        broken = copy.deepcopy(PRIORS)
        broken['1']['1'] = 0.0
        self.verdict(artifact(), artifact(), policy=rubric(priors=broken), expected=False)

    def test_unknown_prior_transformation_is_rejected(self):
        policy = rubric()
        policy['comparison']['prior_transformation'] = 'inverse'
        self.verdict(artifact(), artifact(), policy=policy, expected=False)

    def test_negative_tolerance_is_rejected(self):
        policy = rubric()
        policy['comparison']['atol'] = -1.0
        self.verdict(artifact(), artifact(), policy=policy, expected=False)

    # --- 最终失败协议 ---

    def test_missing_artifact_is_rejected_on_either_side(self):
        for side in ('reference', 'candidate'):
            with self.subTest(side=side):
                pair = {'reference': artifact(), 'candidate': artifact(), side: None}
                self.verdict(pair['reference'], pair['candidate'], expected=False)

    def test_malformed_json_is_rejected(self):
        for raw in (b'{', b'[]', b'null', b'\xff\xfe not utf8'):
            with self.subTest(raw=raw):
                self.verdict(artifact(), raw, expected=False)

    def test_duplicate_json_object_key_is_rejected(self):
        raw = b'{"schema_version": 1, "schema_version": 1, "similarity": []}'
        self.verdict(artifact(), raw, expected=False)

    def test_nonfinite_json_literal_is_rejected(self):
        raw = json.dumps(artifact()).replace('"schema_version": 1', '"schema_version": NaN').encode('utf-8')
        self.verdict(artifact(), raw, expected=False)

    def test_oversized_artifact_is_rejected(self):
        raw = b'{"pad": "' + b'a' * 4_300_000 + b'"}'
        self.verdict(artifact(), raw, expected=False)

    def test_failed_verdict_is_a_fresh_ascii_record_without_partial_pass(self):
        result = self.verdict(artifact(), b'{', expected=False)
        self.assertIsNone(result['distance'])
        self.assertEqual(result['policy'], 'pointwise')
        self.assertIn('error_type', result)
        self.assertNotIn('measurements', result)


if __name__ == '__main__':
    unittest.main(verbosity=2)
