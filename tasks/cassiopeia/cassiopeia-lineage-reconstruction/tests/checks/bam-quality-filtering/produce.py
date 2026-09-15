#!/usr/bin/env python3
"""跑官方两个固定阈值配置，导出每个保留read的完整科学记录；不改写source或BAM。"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import tempfile
import pysam
from cassiopeia.preprocess import pipeline

CASE_THRESHOLDS = {'quality10': 10, 'quality20': 20}


def record(aln):
    if aln.is_paired or not aln.is_unmapped:
        raise ValueError('当前合同范围只覆盖官方fixture的unpaired/unmapped记录')
    return {'qname': aln.query_name, 'mate_role': 'unpaired', 'alignment_status': 'unmapped',
            'sequence': aln.query_sequence,
            'sequence_phred': [int(q) for q in aln.query_qualities],
            'CR': aln.get_tag('CR'), 'UR': aln.get_tag('UR'),
            'CY_phred': [int(q) for q in pysam.qualitystring_to_array(aln.get_tag('CY'))],
            'UY_phred': [int(q) for q in pysam.qualitystring_to_array(aln.get_tag('UY'))]}


def produce(inputs, source):
    bam = source / inputs['bam_relative_path']
    if not bam.is_file():
        raise ValueError('官方BAM输入缺失')
    cases = {}
    for case in inputs['cases']:
        threshold = case['quality_threshold']
        if CASE_THRESHOLDS.get(case['id']) != threshold:
            raise ValueError('配置身份或阈值与固定官方配置不符')
        # 每个配置一个全新输出目录，和原test一样；filtered BAM不落在OUT_DIR里。
        filtered = pipeline.filter_bam(str(bam), tempfile.mkdtemp(), threshold)
        with pysam.AlignmentFile(filtered, 'rb', check_sq=False) as handle:
            records = [record(aln) for aln in handle.fetch(until_eof=True)]
        cases[case['id']] = {'threshold': threshold, 'records': records}
    if set(cases) != set(CASE_THRESHOLDS):
        raise ValueError('必须显式交付两个官方配置，空结果也要显式给出')
    return {'schema_version': 1, 'cases': cases}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    if (out / 'results.json').exists():
        raise ValueError('拒绝覆盖旧结果')
    inputs = json.loads(Path(args.inputs).read_text(encoding='utf-8'))
    document = produce(inputs, Path(args.source))
    wire = json.dumps(document, ensure_ascii=True, allow_nan=False, indent=2).encode('utf-8')
    (out / 'results.json').write_bytes(wire + b'\n')
    print('wrote complete retained-read records for both official thresholds')


if __name__ == '__main__':
    main()
