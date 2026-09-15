#!/usr/bin/env python3
"""独立人工BAM核心记录自测；不读取HOME、生产source、真实BAM或真实nominal结果。"""
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
VALIDATOR = Path(__file__).with_name('validate.py')


def canonical(record):
    """与 validate.py 同一口径：摘要前做 DNA 大小写归一化。"""
    out = dict(record)
    for field in ('sequence', 'CR', 'UR'):
        if isinstance(out.get(field), str):
            out[field] = out[field].upper()
    return out


def summarise(reads):
    """把可信输入表压成摘要——与 validate.py 里同一口径。

    rubric 不再携带逐值向量：那些向量正是 graded 成员，而 solver 看得见 rubric。
    摘要足以做「两侧 vs 已知真值」的比对，且不可反演。
    """
    def digest(value):
        return hashlib.sha256(json.dumps(value, sort_keys=True, ensure_ascii=True,
                                         separators=(',', ':')).encode()).hexdigest()
    return [{'qname_sha256': digest(r['qname']), 'mate_role': r['mate_role'],
             'alignment_status': r['alignment_status'], 'record_sha256': digest(canonical(r)),
             'sequence_length': len(r['sequence']), 'CR_length': len(r['CR']),
             'UR_length': len(r['UR']), 'CY_phred_min': min(r['CY_phred']),
             'UY_phred_min': min(r['UY_phred'])} for r in reads]


def input_reads():
    """人工三条read：ReadA只过10、ReadB两档都过、ReadC两档都不过。真实fixture没有这种混合选择。"""
    return [
        {'qname': 'ReadA', 'mate_role': 'unpaired', 'alignment_status': 'unmapped',
         'sequence': 'ACGT', 'sequence_phred': [21, 22, 23, 24],
         'CR': 'AC', 'UR': 'TG', 'CY_phred': [16, 17], 'UY_phred': [15, 18]},
        {'qname': 'ReadB', 'mate_role': 'unpaired', 'alignment_status': 'unmapped',
         'sequence': 'TTAA', 'sequence_phred': [30, 31, 32, 33],
         'CR': 'GG', 'UR': 'CA', 'CY_phred': [25, 30], 'UY_phred': [22, 21]},
        {'qname': 'ReadC', 'mate_role': 'unpaired', 'alignment_status': 'unmapped',
         'sequence': 'GGCC', 'sequence_phred': [11, 12, 13, 14],
         'CR': 'TT', 'UR': 'AA', 'CY_phred': [9, 30], 'UY_phred': [30, 30]},
    ]


def rubric(reads=None):
    return {'comparison': {'atol': 0, 'rtol': 0,
                           'cases': [{'id': 'quality10', 'threshold': 10},
                                     {'id': 'quality20', 'threshold': 20}],
                           'input_read_summaries': summarise(
                               input_reads() if reads is None else reads)}}


def artifact():
    """与人工输入表一致的完整正确产物：quality10保留A/B，quality20只保留B。"""
    keep = {read['qname']: read for read in input_reads()}
    return {'schema_version': 1,
            'cases': {'quality10': {'threshold': 10,
                                    'records': [copy.deepcopy(keep['ReadA']), copy.deepcopy(keep['ReadB'])]},
                      'quality20': {'threshold': 20,
                                    'records': [copy.deepcopy(keep['ReadB'])]}}}


class CoreRecordContract(unittest.TestCase):
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
            completed = subprocess.run(
                [sys.executable, str(VALIDATOR), '--reference', str(root / 'reference'),
                 '--candidate', str(root / 'candidate'), '--rubric', str(root / 'rubric.json'),
                 '--out', str(out)], capture_output=True, text=True)
            self.assertEqual(completed.returncode, 0, completed.stderr)
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
        self.assertEqual(result['cases']['quality20']['reference_records'], 1)

    def test_case_block_and_record_order_are_not_graded(self):
        def reorder(document):
            document['cases'] = dict(reversed(list(document['cases'].items())))
            document['cases']['quality10']['records'].reverse()
        self.mutate(reorder, expected=True)

    def test_field_order_inside_a_record_is_not_graded(self):
        def reorder(document):
            document['cases']['quality10']['records'] = [
                dict(reversed(list(record.items()))) for record in document['cases']['quality10']['records']]
        self.mutate(reorder, expected=True)

    def test_dna_case_normalisation_is_legal_for_sequence_and_barcodes(self):
        def lower(document):
            for case in document['cases'].values():
                for record in case['records']:
                    for field in ('sequence', 'CR', 'UR'):
                        record[field] = record[field].lower()
        self.mutate(lower, expected=True)

    # --- 完整记录集合 ---

    def test_duplicating_one_read_to_replace_another_is_rejected(self):
        def duplicate(document):
            records = document['cases']['quality10']['records']
            records[1] = copy.deepcopy(records[0])
        self.mutate(duplicate)

    def test_repeated_identity_is_not_silently_deduplicated(self):
        def repeat(document):
            records = document['cases']['quality10']['records']
            records.append(copy.deepcopy(records[0]))
        self.mutate(repeat)

    def test_dropping_a_retained_read_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'].pop())

    def test_adding_a_read_to_the_empty_side_of_a_case_is_rejected(self):
        def add(document):
            document['cases']['quality20']['records'].append(copy.deepcopy(input_reads()[2]))
        self.mutate(add)

    def test_emptying_a_non_empty_case_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10'].__setitem__('records', []))

    def test_keeping_a_read_that_fails_the_threshold_is_rejected(self):
        def keep_failing(document):
            document['cases']['quality20']['records'].append(copy.deepcopy(input_reads()[0]))
        self.mutate(keep_failing)

    # --- payload 绑定 ---

    def test_swapping_barcodes_between_reads_is_rejected(self):
        def swap(document):
            records = document['cases']['quality10']['records']
            records[0]['CR'], records[1]['CR'] = records[1]['CR'], records[0]['CR']
        self.mutate(swap)

    def test_swapping_quality_vectors_between_reads_is_rejected(self):
        def swap(document):
            records = document['cases']['quality10']['records']
            records[0]['CY_phred'], records[1]['CY_phred'] = records[1]['CY_phred'], records[0]['CY_phred']
        self.mutate(swap)

    def test_changing_one_sequence_base_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'][0].__setitem__('sequence', 'ACGA'))

    def test_changing_one_phred_by_one_unit_is_rejected(self):
        def bump(document):
            document['cases']['quality10']['records'][0]['sequence_phred'][2] += 1
        self.mutate(bump)

    def test_truncated_quality_vector_is_rejected(self):
        def truncate(document):
            document['cases']['quality10']['records'][0]['sequence_phred'].pop()
        self.mutate(truncate)

    def test_reordering_inside_a_phred_vector_is_rejected(self):
        def shuffle(document):
            document['cases']['quality10']['records'][0]['CY_phred'].reverse()
        self.mutate(shuffle)

    # --- schema 完整性 ---

    def test_count_only_summary_instead_of_records_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10'].__setitem__('records', 2))

    def test_missing_record_field_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'][0].pop('UY_phred'))

    def test_extra_record_field_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'][0].__setitem__('RG', 'test'))

    def test_missing_case_block_is_rejected(self):
        self.mutate(lambda d: d['cases'].pop('quality20'))

    def test_renegotiated_threshold_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality20'].__setitem__('threshold', 10))

    def test_unsupported_schema_version_is_rejected(self):
        self.mutate(lambda d: d.__setitem__('schema_version', 2))

    def test_out_of_scope_record_kind_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'][0].__setitem__('alignment_status', 'mapped'))

    def test_illegal_phred_values_are_rejected(self):
        for bad in (True, 3.0, -1, 94, '30'):
            with self.subTest(bad=bad):
                self.mutate(lambda d, bad=bad: d['cases']['quality10']['records'][0]['CY_phred'].__setitem__(0, bad))

    def test_non_nucleotide_sequence_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'][0].__setitem__('sequence', 'ACGZ'))

    def test_control_character_in_identifier_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'][0].__setitem__('qname', 'Read\x00A'))

    # --- 双侧同错与不可信 rubric ---

    def test_both_sides_agreeing_on_a_wrong_set_is_still_rejected(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['cases']['quality20']['records'].append(copy.deepcopy(input_reads()[0]))
        self.verdict(reference, candidate, expected=False)

    def test_wrong_reference_alone_is_rejected(self):
        self.mutate(lambda d: d['cases']['quality10']['records'].pop(), side='reference')

    def test_inconsistent_input_table_in_rubric_is_rejected(self):
        broken = copy.deepcopy(input_reads())
        broken[0]['CY_phred'] = [16]
        self.verdict(artifact(), artifact(), policy=rubric(broken), expected=False)

    def test_duplicate_identity_in_input_table_is_rejected(self):
        broken = copy.deepcopy(input_reads())
        broken[1]['qname'] = 'ReadA'
        self.verdict(artifact(), artifact(), policy=rubric(broken), expected=False)

    def test_non_zero_tolerance_is_rejected_for_a_discrete_contract(self):
        loose = rubric()
        loose['comparison']['atol'] = 1e-9
        self.verdict(artifact(), artifact(), policy=loose, expected=False)

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
        raw = b'{"schema_version": 1, "schema_version": 1, "cases": {}}'
        self.verdict(artifact(), raw, expected=False)

    def test_nonfinite_json_literal_is_rejected(self):
        raw = json.dumps(artifact()).replace('[21', '[NaN').encode('utf-8')
        self.verdict(artifact(), raw, expected=False)

    def test_oversized_artifact_is_rejected(self):
        raw = b'{"pad": "' + b'a' * 1_100_000 + b'"}'
        self.verdict(artifact(), raw, expected=False)

    def test_scientific_rejection_reports_measurements_not_a_decode_error(self):
        """科学不一致必须拿到填好 distance/cases 的判决书，而不是「解码失败」。"""
        candidate = artifact()
        candidate['cases']['quality10']['records'][0]['CR'] = 'GG'
        result = self.verdict(artifact(), candidate, expected=False)
        self.assertNotIn('error_type', result)
        self.assertIsNotNone(result['distance'])
        self.assertGreater(result['distance'], 0)
        self.assertIn('cases', result)
        self.assertGreater(result['cases']['quality10']['changed_records'], 0)
        self.assertGreater(
            result['cases']['quality10']['against_independent_truth']['candidate']['changed_records'], 0)

    def test_both_sides_wrong_is_reported_against_the_independent_truth(self):
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            document['cases']['quality20']['records'].append(copy.deepcopy(input_reads()[0]))
        result = self.verdict(reference, candidate, expected=False)
        self.assertNotIn('error_type', result)
        self.assertEqual(result['cases']['quality20']['extra_records'], 0)
        self.assertGreater(
            result['cases']['quality20']['against_independent_truth']['reference']['extra_records'], 0)

    def test_failed_verdict_is_a_fresh_ascii_record_without_partial_pass(self):
        result = self.verdict(artifact(), b'{', expected=False)
        self.assertIsNone(result['distance'])
        self.assertEqual(result['policy'], 'pointwise')
        self.assertIn('error_type', result)
        self.assertNotIn('cases', result)


if __name__ == '__main__':
    unittest.main(verbosity=2)
