#!/usr/bin/env python3
"""独立人工共享突变合并自测；不读取HOME、生产source或真实nominal结果。"""
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

COLUMNS = ['x1', 'x2', 'x3']
PLAIN = {'p': [1, 2, 2], 'q': [1, 2, 1], 'r': [1, 2, 0], 's': [2, 0, 0]}
GAPPY = {'p': [1, -1, 0], 'q': [2, -1, 2], 'r': [2, 0, 2], 's': [2, 0, -1]}
PRIORS = {'0': {'1': 0.5, '2': 0.5}, '1': {'1': 0.2, '2': 0.8}, '2': {'1': 0.9, '2': 0.1}}
# 显式相似度表刻意做成有并列最大值，和官方 basic fixture 一样。
EXPLICIT = {'p': {'q': 2.0, 'r': 1.0, 's': 1.0},
            'q': {'r': 1.0, 's': 2.0},
            'r': {'s': 0.0}}


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


def lca(a, b):
    out = []
    for x, y in zip(a, b):
        if x == MISSING and y == MISSING:
            out.append(MISSING)
            continue
        present = [state for state in (x, y) if state != MISSING]
        out.append(present[0] if len(set(present)) == 1 else 0)
    return out


def explicit_pairs():
    return {frozenset((a, b)): value for a, row in EXPLICIT.items() for b, value in row.items()}


MATRICES = {'plain': {'columns': COLUMNS, 'rows': PLAIN}, 'gappy': {'columns': COLUMNS, 'rows': GAPPY}}
SIM_CONFIGS = [{'id': 'plain', 'matrix': 'plain', 'use_priors': False},
               {'id': 'weighted', 'matrix': 'plain', 'use_priors': True},
               {'id': 'gappy', 'matrix': 'gappy', 'use_priors': False}]
MAX_PROBES = [{'id': 'explicit', 'similarity_map': 'explicit'}]
LCA_PROBES = [{'id': 'plain_pq', 'matrix': 'plain', 'cell_i': 'p', 'cell_j': 'q'},
              {'id': 'gappy_pq', 'matrix': 'gappy', 'cell_i': 'p', 'cell_j': 'q'},
              {'id': 'gappy_qs', 'matrix': 'gappy', 'cell_i': 'q', 'cell_j': 's'}]
UPDATE_PROBES = [{'id': 'merge_pq', 'matrix': 'plain', 'similarity_map': 'explicit',
                  'cherry': ['p', 'q'], 'new_node': 'pq', 'use_priors': False}]


def rubric(configs=None, priors=None, update_probes=None, lca_probes=None):
    return {'comparison': {
        'atol': ATOL, 'rtol': 0, 'missing_state_indicator': MISSING,
        'prior_transformation': 'negative_log',
        'character_matrices': MATRICES,
        'explicit_similarity_maps': {'explicit': {a: dict(row) for a, row in EXPLICIT.items()}},
        'priors': PRIORS if priors is None else priors,
        'similarity_configs': SIM_CONFIGS if configs is None else configs,
        'maximal_pair_probes': MAX_PROBES,
        'lca_probes': LCA_PROBES if lca_probes is None else lca_probes,
        'update_probes': UPDATE_PROBES if update_probes is None else update_probes}}


def artifact():
    weights = weights_of(PRIORS)
    similarity_rows = []
    for config in SIM_CONFIGS:
        cells = MATRICES[config['matrix']]['rows']
        applied = weights if config['use_priors'] else None
        for a, b in itertools.combinations(sorted(cells), 2):
            similarity_rows.append({'config': config['id'], 'cell_i': a, 'cell_j': b,
                                    'value': similarity(cells[a], cells[b], applied)})
    pairs = explicit_pairs()
    best = max(pairs.values())
    maximal = [{'probe': 'explicit', 'max_similarity': best,
                'pairs': sorted(sorted(pair) for pair, value in pairs.items() if value == best)}]
    lca_rows = [{'probe': probe['id'], 'cell_i': probe['cell_i'], 'cell_j': probe['cell_j'],
                 'states': lca(MATRICES[probe['matrix']]['rows'][probe['cell_i']],
                               MATRICES[probe['matrix']]['rows'][probe['cell_j']])}
                for probe in LCA_PROBES]
    updates = []
    for probe in UPDATE_PROBES:
        cells = MATRICES[probe['matrix']]['rows']
        a, b = probe['cherry']
        merged = lca(cells[a], cells[b])
        names = sorted({name for pair in pairs for name in pair})
        rest = sorted(name for name in names if name not in (a, b))
        table = []
        for x, y in itertools.combinations(sorted(rest + [probe['new_node']]), 2):
            if probe['new_node'] in (x, y):
                other = y if x == probe['new_node'] else x
                value = similarity(merged, cells[other], None)
            else:
                value = pairs[frozenset((x, y))]
            table.append({'node_i': x, 'node_j': y, 'value': value})
        updates.append({'probe': probe['id'], 'cherry': sorted(probe['cherry']),
                        'new_node_states': merged, 'map': table})
    return {'schema_version': 1, 'similarity': similarity_rows, 'maximal_pairs': maximal,
            'lca': lca_rows, 'update_steps': updates}


class SharedMutationContract(unittest.TestCase):
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
        self.assertEqual(self.verdict(artifact(), artifact(), expected=True)['distance'], 0)

    def test_row_order_is_not_graded(self):
        def reorder(document):
            for key in ('similarity', 'lca', 'update_steps'):
                document[key].reverse()
            document['update_steps'][0]['map'].reverse()
        self.mutate(reorder, expected=True)

    def test_unordered_pair_identity_is_symmetric(self):
        def flip(document):
            for row in document['similarity']:
                row['cell_i'], row['cell_j'] = row['cell_j'], row['cell_i']
            document['update_steps'][0]['cherry'].reverse()
        self.mutate(flip, expected=True)

    def test_maximal_pair_listing_order_is_not_graded(self):
        self.mutate(lambda d: d['maximal_pairs'][0]['pairs'].reverse(), expected=True)

    def test_difference_inside_the_bound_passes(self):
        self.mutate(lambda d: d['similarity'][0].__setitem__('value', d['similarity'][0]['value'] + ATOL / 4),
                    expected=True)

    # --- 相似度 ---

    def test_difference_outside_the_bound_is_rejected(self):
        self.mutate(lambda d: d['similarity'][0].__setitem__('value', d['similarity'][0]['value'] + ATOL * 100))

    def test_counting_shared_zeros_is_rejected(self):
        def wrong(document):
            for row in document['similarity']:
                if row['config'] == 'plain' and {row['cell_i'], row['cell_j']} == {'r', 's'}:
                    row['value'] += 2
                    return
            self.fail('人工 fixture 必须含有共享 0 的一对')
        self.mutate(wrong)

    def test_counting_shared_missing_is_rejected(self):
        def wrong(document):
            for row in document['similarity']:
                if row['config'] == 'gappy' and {row['cell_i'], row['cell_j']} == {'p', 'q'}:
                    row['value'] += 1
                    return
            self.fail('人工 fixture 必须含有共享缺失的一对')
        self.mutate(wrong)

    def test_unweighted_value_in_the_weighted_config_is_rejected(self):
        def unweighted(document):
            for row in document['similarity']:
                if row['config'] == 'weighted':
                    row['value'] = similarity(PLAIN[row['cell_i']], PLAIN[row['cell_j']], None)
        self.mutate(unweighted)

    # --- 最大相似对集合 ---

    def test_reporting_only_one_of_the_tied_maximal_pairs_is_rejected(self):
        self.mutate(lambda d: d['maximal_pairs'][0].__setitem__('pairs', d['maximal_pairs'][0]['pairs'][:1]))

    def test_wrong_maximal_similarity_is_rejected(self):
        self.mutate(lambda d: d['maximal_pairs'][0].__setitem__('max_similarity', 1.0))

    def test_extra_pair_in_the_maximal_set_is_rejected(self):
        self.mutate(lambda d: d['maximal_pairs'][0]['pairs'].append(['r', 's']))

    # --- LCA ---

    def test_wrong_lca_state_is_rejected(self):
        self.mutate(lambda d: d['lca'][0]['states'].__setitem__(0, 9))

    def test_lca_imputing_missing_instead_of_the_observed_state_is_rejected(self):
        def wrong(document):
            for row in document['lca']:
                if row['probe'] == 'gappy_qs':
                    row['states'] = [MISSING] * len(row['states'])
                    return
        self.mutate(wrong)

    def test_lca_keeping_a_disagreeing_state_instead_of_zero_is_rejected(self):
        def wrong(document):
            for row in document['lca']:
                if row['probe'] == 'gappy_pq':
                    row['states'][0] = 1
                    return
        self.mutate(wrong)

    # --- 合并更新 ---

    def test_wrong_updated_similarity_is_rejected(self):
        self.mutate(lambda d: d['update_steps'][0]['map'][0].__setitem__('value',
                                                                         d['update_steps'][0]['map'][0]['value'] + 1))

    def test_updated_map_still_containing_the_joined_cells_is_rejected(self):
        """合并之后没删掉被合并的节点——这是**算法错误**，必须出数字判决书。

        「一次合并之后哪些标签还活着」正是 SMJ 要算的答案，不是本 check 定义的固定清单，
        所以它不能退化成解码异常：那样一个教科书级的 bug 与坏文件就分不开了。
        """
        def keep(document):
            document['update_steps'][0]['map'].append({'node_i': 'p', 'node_j': 'q', 'value': 2.0})
        result = self.mutate(keep)
        self.assertNotIn('error_type', result)
        self.assertIsNotNone(result['distance'])
        self.assertGreater(result['measurements']['update_steps_surviving_label_set_mismatches'], 0)

    def test_updated_map_dropping_a_surviving_pair_is_rejected(self):
        """反方向：少一个本该活着的节点对，同样是科学拒绝而不是合同失败。"""
        def drop(document):
            document['update_steps'][0]['map'].pop()
        result = self.mutate(drop)
        self.assertNotIn('error_type', result)
        self.assertIsNotNone(result['distance'])
        self.assertGreater(result['measurements']['update_steps_surviving_label_set_mismatches'], 0)

    def test_missing_update_probe_is_still_a_contract_failure(self):
        """对照：probe 清单是本 check 写死的固定单元，缺一个说明那次调用根本没跑——合同失败。"""
        result = self.mutate(lambda d: d['update_steps'].pop())
        self.assertIn('error_type', result)
        self.assertIsNone(result['distance'])

    def test_wrong_new_node_states_are_rejected(self):
        self.mutate(lambda d: d['update_steps'][0]['new_node_states'].__setitem__(0, 0))

    def test_wrong_cherry_is_rejected(self):
        self.mutate(lambda d: d['update_steps'][0].__setitem__('cherry', ['r', 's']))

    # --- 完整覆盖与 schema ---

    def test_dropping_a_similarity_pair_is_rejected(self):
        self.mutate(lambda d: d['similarity'].pop())

    def test_duplicate_identity_is_not_silently_deduplicated(self):
        self.mutate(lambda d: d['similarity'].append(copy.deepcopy(d['similarity'][0])))

    def test_row_under_the_wrong_config_is_rejected(self):
        self.mutate(lambda d: d['similarity'][0].__setitem__('config', 'gappy'))

    def test_missing_section_is_rejected(self):
        self.mutate(lambda d: d.pop('lca'))

    def test_missing_row_field_is_rejected(self):
        self.mutate(lambda d: d['similarity'][0].pop('value'))

    def test_extra_row_field_is_rejected(self):
        self.mutate(lambda d: d['lca'][0].__setitem__('extra', 1))

    def test_summary_instead_of_rows_is_rejected(self):
        self.mutate(lambda d: d.__setitem__('similarity', len(d['similarity'])))

    def test_unsupported_schema_version_is_rejected(self):
        self.mutate(lambda d: d.__setitem__('schema_version', 2))

    def test_negative_or_non_numeric_similarity_is_rejected(self):
        for bad in (-1.0, True, '1.0', 1e400):
            with self.subTest(bad=bad):
                self.mutate(lambda d, bad=bad: d['similarity'][0].__setitem__('value', bad))

    def test_non_integer_lca_state_is_rejected(self):
        self.mutate(lambda d: d['lca'][0]['states'].__setitem__(0, 1.5))

    def test_truncated_lca_vector_is_rejected(self):
        self.mutate(lambda d: d['lca'][0]['states'].pop())

    # --- 双侧同错与不可信 rubric ---

    def test_both_sides_agreeing_on_a_wrong_value_is_still_rejected(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['similarity'][0]['value'] += 1.0
        self.verdict(reference, candidate, expected=False)

    def test_wrong_reference_alone_is_rejected(self):
        self.mutate(lambda d: d['lca'][0]['states'].__setitem__(0, 9), side='reference')

    def test_config_referring_to_an_unknown_matrix_is_rejected(self):
        configs = copy.deepcopy(SIM_CONFIGS)
        configs[0]['matrix'] = 'nowhere'
        self.verdict(artifact(), artifact(), policy=rubric(configs=configs), expected=False)

    def test_asymmetric_explicit_similarity_map_is_rejected(self):
        policy = rubric()
        policy['comparison']['explicit_similarity_maps']['explicit']['q']['p'] = 9.0
        self.verdict(artifact(), artifact(), policy=policy, expected=False)

    def test_lca_probe_on_the_same_cell_is_rejected(self):
        probes = copy.deepcopy(LCA_PROBES)
        probes[0]['cell_j'] = probes[0]['cell_i']
        self.verdict(artifact(), artifact(), policy=rubric(lca_probes=probes), expected=False)

    def test_update_probe_whose_new_node_collides_with_a_cell_is_rejected(self):
        probes = copy.deepcopy(UPDATE_PROBES)
        probes[0]['new_node'] = 'r'
        self.verdict(artifact(), artifact(), policy=rubric(update_probes=probes), expected=False)

    def test_prior_outside_the_unit_interval_is_rejected(self):
        broken = copy.deepcopy(PRIORS)
        broken['1']['2'] = 0.0
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

    def test_numeric_rejection_reports_a_number_not_a_decode_error(self):
        result = self.mutate(
            lambda d: d['similarity'][0].__setitem__('value', d['similarity'][0]['value'] + ATOL * 100))
        self.assertNotIn('error_type', result)
        self.assertGreater(result['distance'], ATOL)
        self.assertGreater(result['bound_fraction'], 1.0)
        self.assertGreater(result['values_over_bound'], 0)
        self.assertEqual(result['categorical_mismatches'], 0)
        self.assertIn('超出暂拟界限', result['reason'])

    def test_discrete_rejection_is_named_as_categorical(self):
        result = self.mutate(lambda d: d['lca'][0]['states'].__setitem__(0, 9))
        self.assertNotIn('error_type', result)
        self.assertEqual(result['values_over_bound'], 0)
        self.assertGreater(result['categorical_mismatches'], 0)
        self.assertIn('离散量', result['reason'])

    def test_both_sides_wrong_is_reported_against_the_independent_truth(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['similarity'][0]['value'] += 1.0
        result = self.verdict(reference, candidate, expected=False)
        self.assertNotIn('error_type', result)
        self.assertGreater(result['distance'], 0.5)
        self.assertGreater(result['measurements']['similarity_values_over_bound'], 0)

    def test_failed_verdict_is_a_fresh_ascii_record_without_partial_pass(self):
        result = self.verdict(artifact(), b'{', expected=False)
        self.assertIsNone(result['distance'])
        self.assertEqual(result['policy'], 'pointwise')
        self.assertIn('error_type', result)
        self.assertNotIn('measurements', result)


if __name__ == '__main__':
    unittest.main(verbosity=2)
