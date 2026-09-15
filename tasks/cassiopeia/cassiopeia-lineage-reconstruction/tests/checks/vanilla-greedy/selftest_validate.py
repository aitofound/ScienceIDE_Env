#!/usr/bin/env python3
"""独立人工VanillaGreedy自测；不读取HOME、生产source或真实nominal结果。"""
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
ATOL = 0

FREQ_CELLS = {'c1': [5, {'ambiguous': [0, 1]}, 1], 'c2': [0, 0, 3], 'c3': [-1, 4, 0], 'c4': [4, 4, 1]}
MISS_CELLS = {'c1': [-1, 4, 0], 'c2': [4, 4, 1], 'c3': [4, 0, 3], 'c4': [5, 0, 1], 'c5': [5, 0, 1]}
# 无并列：char0 的 state1 计数唯一最高，之后每一层的最优也唯一。
CLEAN_CELLS = {'c1': [1, 1, 0], 'c2': [1, 0, 0], 'c3': [0, 0, 1]}
# 顶层 split 唯一（char0/1 计数 3），但下一层三个字符计数都是 1，三路并列且给出不同的树。
# 这与真实 fixture 的形状一致：官方 case_2 的顶层也是唯一的，并列出现在更深处。
TIED_CELLS = {'c1': [1, 1, 0, 0], 'c2': [1, 0, 1, 0], 'c3': [1, 0, 0, 1],
              'c4': [0, 0, 0, 0], 'c5': [0, 0, 0, 0]}
PRIORS = {'0': {'4': 0.5, '5': 0.5}, '1': {'4': 1.0}, '2': {'1': 1.0, '3': 1.0}}


def unravel(state):
    return list(state['ambiguous']) if isinstance(state, dict) else [state]


def frequencies(cells, samples, width):
    table = {}
    for character in range(width):
        counts = {}
        for cell in samples:
            for state in unravel(cells[cell][character]):
                counts[state] = counts.get(state, 0) + 1
        ordered = {state: counts[state] for state in sorted(counts)}
        ordered.setdefault(MISSING, 0)
        table[character] = ordered
    return table


def score_side(cells, side, query, weights, width):
    score = 0.0
    for character in range(width):
        states = [s for cell in side for s in unravel(cells[cell][character])]
        for q in unravel(query[character]):
            if q in (0, MISSING):
                continue
            score += (weights[character][q] if weights else 1) * states.count(q)
    return score


def assign_missing(cells, left, right, missing, weights, width):
    left, right = list(left), list(right)
    for cell in missing:
        a = score_side(cells, left, cells[cell], weights, width) / len(left)
        b = score_side(cells, right, cells[cell], weights, width) / len(right)
        (left if a > b else right).append(cell)
    return left, right


def perform_split(cells, samples, weights, width):
    table = frequencies(cells, samples, width)
    best, chosen = 0.0, None
    for character in table:
        for state in table[character]:
            if state in (0, MISSING):
                continue
            if table[character][state] >= len(samples) - table[character][MISSING]:
                continue
            value = table[character][state] * (weights[character][state] if weights else 1)
            if value > best:
                best, chosen = value, (character, state)
    if chosen is None:
        return list(samples), []
    character, state = chosen
    left, right, missing = [], [], []
    for cell in samples:
        observed = cells[cell][character]
        if state in unravel(observed) if isinstance(observed, dict) else observed == state:
            left.append(cell)
        elif observed == MISSING:
            missing.append(cell)
        else:
            right.append(cell)
    return assign_missing(cells, left, right, missing, weights, width)


def unique_rows(cells):
    seen, keep = set(), []
    for cell, row in cells.items():
        key = tuple(frozenset(unravel(s)) for s in row)
        if key not in seen:
            seen.add(key)
            keep.append(cell)
    return keep


def solve(cells, weights, width):
    edges, counter = [], [0]

    def recurse(samples):
        if len(samples) == 1:
            return samples[0]
        clades = [c for c in perform_split(cells, samples, weights, width) if c]
        root = f'node{counter[0]}'
        counter[0] += 1
        if len(clades) == 1:
            edges.extend((root, c) for c in clades[0])
            return root
        for clade in clades:
            edges.append((root, recurse(clade)))
        return root
    recurse(unique_rows(cells))
    groups = {}
    for cell, row in cells.items():
        groups.setdefault(tuple(frozenset(unravel(s)) for s in row), []).append(cell)
    for group in groups.values():
        if len(group) < 2:
            continue
        keeper, parent = group[0], f'dup{group[0]}'
        edges[:] = [(a, parent if b == keeper else b) for a, b in edges]
        edges.extend((parent, member) for member in group)
    return edges


def triplets(edges, leaves):
    parent = {child: node for node, child in edges}
    ancestors = {}
    for node in set(parent) | {e[0] for e in edges}:
        chain, walk = set(), node
        while walk in parent:
            walk = parent[walk]
            chain.add(walk)
        ancestors[node] = chain
    result = {}
    for a, b, c in itertools.combinations(sorted(leaves), 3):
        ab, ac, bc = (len(ancestors[a] & ancestors[b]), len(ancestors[a] & ancestors[c]),
                      len(ancestors[b] & ancestors[c]))
        structure = '-'
        if ab > bc and ab > ac:
            structure = 'ab'
        elif ac > bc and ac > ab:
            structure = 'ac'
        elif bc > ab and bc > ac:
            structure = 'bc'
        result[(a, b, c)] = structure
    return result


FIXTURES = {'freq': {'columns': ['x1', 'x2', 'x3'], 'rows': FREQ_CELLS},
            'miss': {'columns': ['x1', 'x2', 'x3'], 'rows': MISS_CELLS},
            'clean': {'columns': ['x1', 'x2', 'x3'], 'rows': CLEAN_CELLS},
            'tied': {'columns': ['x1', 'x2', 'x3', 'x4'], 'rows': TIED_CELLS}}
FREQ_PROBES = [{'id': 'freq', 'fixture': 'freq', 'samples': ['c1', 'c2', 'c3', 'c4']}]
MISS_PROBES = [{'id': 'miss', 'fixture': 'miss', 'left': ['c1', 'c2'], 'right': ['c4', 'c5'],
                'missing': ['c3'], 'use_priors': False}]
SOLVE_PROBES = [{'id': 'clean', 'fixture': 'clean', 'use_priors': False, 'topology_graded': True},
                {'id': 'tied', 'fixture': 'tied', 'use_priors': False, 'topology_graded': False}]


def rubric(solve_probes=None, priors=None, freq_probes=None):
    return {'comparison': {
        'atol': ATOL, 'rtol': 0, 'missing_state_indicator': MISSING,
        'prior_transformation': 'negative_log', 'fixtures': FIXTURES,
        'priors': PRIORS if priors is None else priors,
        'frequency_probes': FREQ_PROBES if freq_probes is None else freq_probes,
        'missing_probes': MISS_PROBES,
        'solve_probes': SOLVE_PROBES if solve_probes is None else solve_probes}}


def artifact():
    rows = []
    for probe in FREQ_PROBES:
        cells = FIXTURES[probe['fixture']]['rows']
        width = len(FIXTURES[probe['fixture']]['columns'])
        table = frequencies(cells, probe['samples'], width)
        for character in table:
            for state, count in table[character].items():
                rows.append({'probe': probe['id'], 'character': character, 'state': state, 'count': count})
    assignments = []
    for probe in MISS_PROBES:
        cells = FIXTURES[probe['fixture']]['rows']
        width = len(FIXTURES[probe['fixture']]['columns'])
        left, right = assign_missing(cells, probe['left'], probe['right'], probe['missing'], None, width)
        assignments.append({'probe': probe['id'], 'left': left, 'right': right})
    splits, structures = [], []
    for probe in SOLVE_PROBES:
        cells = FIXTURES[probe['fixture']]['rows']
        width = len(FIXTURES[probe['fixture']]['columns'])
        keep = unique_rows(cells)
        left, right = perform_split(cells, keep, None, width)
        splits.append({'probe': probe['id'], 'left': left, 'right': right})
        if not probe['topology_graded']:
            continue
        for key, value in triplets(solve(cells, None, width), sorted(cells)).items():
            structures.append({'probe': probe['id'], 'triplet': list(key), 'structure': value})
    return {'schema_version': 1, 'frequencies': rows, 'missing_assignment': assignments,
            'splits': splits, 'topology_triplets': structures}


class GreedyContract(unittest.TestCase):
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
            for key in ('frequencies', 'missing_assignment', 'splits', 'topology_triplets'):
                document[key].reverse()
        self.mutate(reorder, expected=True)

    def test_integral_counts_may_arrive_as_json_floats(self):
        def widen(document):
            for row in document['frequencies']:
                row['count'] = float(row['count'])
        self.mutate(widen, expected=True)

    # --- 频率表 ---

    def test_wrong_frequency_count_is_rejected(self):
        self.mutate(lambda d: d['frequencies'][0].__setitem__('count', 99))

    def test_ambiguous_state_not_unravelled_is_rejected(self):
        def collapse(document):
            for row in document['frequencies']:
                if row['character'] == 1 and row['state'] == 1:
                    row['count'] = 0
                    return
            self.fail('人工 fixture 必须含有被展开的 ambiguous 状态')
        self.mutate(collapse)

    def test_dropping_the_zero_missing_entry_is_rejected(self):
        def drop(document):
            for index, row in enumerate(document['frequencies']):
                if row['state'] == MISSING and row['count'] == 0:
                    document['frequencies'].pop(index)
                    return
            self.fail('人工 fixture 必须含有补零的缺失项')
        self.mutate(drop)

    def test_extra_frequency_entry_is_rejected(self):
        def add(document):
            row = copy.deepcopy(document['frequencies'][0])
            row['state'] = 99
            document['frequencies'].append(row)
        self.mutate(add)

    def test_duplicate_frequency_identity_is_rejected(self):
        self.mutate(lambda d: d['frequencies'].append(copy.deepcopy(d['frequencies'][0])))

    # --- 缺失数据分配 ---

    def test_wrong_missing_assignment_side_is_rejected(self):
        def flip(document):
            row = document['missing_assignment'][0]
            row['left'] = [c for c in row['left'] if c != 'c3']
            row['right'] = row['right'] + ['c3']
        self.mutate(flip)

    def test_missing_assignment_order_is_graded(self):
        self.mutate(lambda d: d['missing_assignment'][0]['left'].reverse())

    def test_missing_sample_dropped_entirely_is_rejected(self):
        def drop(document):
            row = document['missing_assignment'][0]
            row['left'] = [c for c in row['left'] if c != 'c3']
        self.mutate(drop)

    # --- 顶层 split ---

    def test_wrong_split_partition_is_rejected(self):
        self.mutate(lambda d: d['splits'][0].__setitem__('right', []))

    def test_split_order_is_graded(self):
        self.mutate(lambda d: d['splits'][0]['left'].reverse())

    def test_swapping_the_two_split_sides_is_rejected(self):
        def swap(document):
            row = document['splits'][0]
            row['left'], row['right'] = row['right'], row['left']
        self.mutate(swap)

    # --- 拓扑 ---

    def test_wrong_triplet_structure_is_rejected(self):
        def flip(document):
            for row in document['topology_triplets']:
                if row['structure'] != '-':
                    row['structure'] = 'bc' if row['structure'] == 'ab' else 'ab'
                    return
            self.fail('人工 fixture 必须含有可解 triplet')
        self.mutate(flip)

    def test_topology_of_a_tie_ambiguous_probe_must_not_be_delivered(self):
        def leak(document):
            row = copy.deepcopy(document['topology_triplets'][0])
            row['probe'] = 'tied'
            document['topology_triplets'].append(row)
        self.mutate(leak)

    def test_declaring_a_tie_ambiguous_probe_as_topology_graded_is_rejected(self):
        probes = copy.deepcopy(SOLVE_PROBES)
        for probe in probes:
            if probe['id'] == 'tied':
                probe['topology_graded'] = True
        self.verdict(artifact(), artifact(), policy=rubric(solve_probes=probes), expected=False)

    def test_dropping_a_triplet_is_rejected(self):
        self.mutate(lambda d: d['topology_triplets'].pop())

    # --- schema 完整性 ---

    def test_missing_section_is_rejected(self):
        self.mutate(lambda d: d.pop('splits'))

    def test_missing_row_field_is_rejected(self):
        self.mutate(lambda d: d['frequencies'][0].pop('count'))

    def test_extra_row_field_is_rejected(self):
        self.mutate(lambda d: d['splits'][0].__setitem__('extra', 1))

    def test_summary_instead_of_rows_is_rejected(self):
        self.mutate(lambda d: d.__setitem__('frequencies', len(d['frequencies'])))

    def test_unsupported_schema_version_is_rejected(self):
        self.mutate(lambda d: d.__setitem__('schema_version', 2))

    def test_unknown_probe_identity_is_rejected(self):
        self.mutate(lambda d: d['frequencies'][0].__setitem__('probe', 'nowhere'))

    def test_illegal_count_values_are_rejected(self):
        for bad in (True, 1.5, -1, '3'):
            with self.subTest(bad=bad):
                self.mutate(lambda d, bad=bad: d['frequencies'][0].__setitem__('count', bad))

    def test_illegal_structure_label_is_rejected(self):
        self.mutate(lambda d: d['topology_triplets'][0].__setitem__('structure', 'xy'))

    def test_duplicate_cell_in_a_partition_is_rejected(self):
        self.mutate(lambda d: d['splits'][0]['left'].append(d['splits'][0]['left'][0]))

    # --- 双侧同错与不可信 rubric ---

    def test_both_sides_agreeing_on_a_wrong_count_is_still_rejected(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['frequencies'][0]['count'] += 1
        self.verdict(reference, candidate, expected=False)

    def test_wrong_reference_alone_is_rejected(self):
        self.mutate(lambda d: d['splits'][0].__setitem__('right', []), side='reference')

    def test_probe_referring_to_an_unknown_fixture_is_rejected(self):
        probes = copy.deepcopy(FREQ_PROBES)
        probes[0]['fixture'] = 'nowhere'
        self.verdict(artifact(), artifact(), policy=rubric(freq_probes=probes), expected=False)

    def test_probe_sampling_an_unknown_cell_is_rejected(self):
        probes = copy.deepcopy(FREQ_PROBES)
        probes[0]['samples'] = ['c1', 'zz']
        self.verdict(artifact(), artifact(), policy=rubric(freq_probes=probes), expected=False)

    def test_prior_outside_the_unit_interval_is_rejected(self):
        broken = copy.deepcopy(PRIORS)
        broken['1']['4'] = 0.0
        self.verdict(artifact(), artifact(), policy=rubric(priors=broken), expected=False)

    def test_unknown_prior_transformation_is_rejected(self):
        policy = rubric()
        policy['comparison']['prior_transformation'] = 'inverse'
        self.verdict(artifact(), artifact(), policy=policy, expected=False)

    def test_non_zero_tolerance_is_rejected_for_a_discrete_contract(self):
        policy = rubric()
        policy['comparison']['atol'] = 1e-9
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
        raw = b'{"schema_version": 1, "schema_version": 1, "frequencies": []}'
        self.verdict(artifact(), raw, expected=False)

    def test_nonfinite_json_literal_is_rejected(self):
        raw = json.dumps(artifact()).replace('"schema_version": 1', '"schema_version": NaN').encode('utf-8')
        self.verdict(artifact(), raw, expected=False)

    def test_oversized_artifact_is_rejected(self):
        raw = b'{"pad": "' + b'a' * 4_300_000 + b'"}'
        self.verdict(artifact(), raw, expected=False)

    def test_scientific_rejection_reports_measurements_not_a_decode_error(self):
        result = self.mutate(lambda d: d['frequencies'][0].__setitem__('count', 99))
        self.assertNotIn('error_type', result)
        self.assertIsNotNone(result['distance'])
        self.assertGreater(result['distance'], 0)
        self.assertGreater(result['measurements']['frequencies_candidate_vs_truth'], 0)

    def test_both_sides_wrong_is_reported_against_the_independent_truth(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['frequencies'][0]['count'] += 1
        result = self.verdict(reference, candidate, expected=False)
        self.assertNotIn('error_type', result)
        self.assertEqual(result['measurements']['frequencies_changed_between_sides'], 0)
        self.assertGreater(result['measurements']['frequencies_reference_vs_truth'], 0)

    def test_failed_verdict_is_a_fresh_ascii_record_without_partial_pass(self):
        result = self.verdict(artifact(), b'{', expected=False)
        self.assertIsNone(result['distance'])
        self.assertEqual(result['policy'], 'pointwise')
        self.assertIn('error_type', result)
        self.assertNotIn('measurements', result)


if __name__ == '__main__':
    unittest.main(verbosity=2)
