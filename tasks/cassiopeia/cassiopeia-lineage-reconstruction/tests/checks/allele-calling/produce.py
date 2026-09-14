#!/usr/bin/env python3
"""直接运行固定 CIGAR/allele API；只解码返回注释，不重做 alignment 或 calling。"""
from __future__ import annotations

import argparse
import json
from numbers import Integral
from pathlib import Path
import re
import time

import numpy as np

ATOM = r'(?:None|[0-9]+\s*:\s*[0-9]+\s*[ID])'
CONTEXT = re.compile(r'([ACGTN]*)\[\s*(' + ATOM + r')\s*\]([ACGTN]*)', re.IGNORECASE)
PLAIN = re.compile(ATOM, re.IGNORECASE)


def decode_atom(value, left='', right=''):
    value = re.sub(r'\s+', '', value).upper()
    if value == 'NONE':
        return ('WT', -1, 0, left.upper(), right.upper())
    match = re.fullmatch(r'([0-9]+):([0-9]+)([ID])', value)
    if match is None:
        raise ValueError('非法生产 indel 注释')
    position, length, operation = match.groups()
    if int(position) < 1 or int(length) < 1:
        raise ValueError('生产 indel 坐标或长度非法')
    return (operation, int(position) - 1, int(length), left.upper(), right.upper())


def decode_call(value):
    if not isinstance(value, str):
        raise TypeError('生产 allele 必须是字符串，不把 NaN 当作 missing')
    value = value.strip()
    if value == '':
        return [('MISSING', -1, 0, '', '')]
    if '[' in value or ']' in value:
        match = CONTEXT.fullmatch(value)
        if match is None:
            raise ValueError('非法或复合 context 注释；这11个固定输入只产生单事件 context')
        left, atom, right = match.groups()
        return [decode_atom(atom, left, right)]
    result, end = [], 0
    for match in PLAIN.finditer(value):
        if value[end:match.start()].strip():
            raise ValueError('生产 allele 含未知注释内容')
        result.append(decode_atom(match.group()))
        end = match.end()
    if not result or value[end:].strip():
        raise ValueError('生产 allele 不能完整解码')
    return result


def text(values):
    values = list(values)
    if any(not isinstance(value, str) for value in values):
        raise TypeError('生产身份或 barcode 不是字符串')
    return np.asarray(values, dtype=str)


def integer(value):
    if isinstance(value, (bool, np.bool_)) or not isinstance(value, Integral):
        raise TypeError('生产计数不是整数')
    return int(value)


def pack_calls(calls):
    groups = [decode_call(value) for value in calls]
    flat = [item for group in groups for item in group]
    offsets = [0]
    for group in groups:
        offsets.append(offsets[-1] + len(group))
    return {'offsets': np.asarray(offsets, dtype=np.int64),
            'operations': text(item[0] for item in flat),
            'positions': np.asarray([item[1] for item in flat], dtype=np.int64),
            'lengths': np.asarray([item[2] for item in flat], dtype=np.int64),
            'left_context': text(item[3] for item in flat),
            'right_context': text(item[4] for item in flat)}


def pack_result(result, stage):
    if stage['kind'] == 'cigar':
        intbc, calls = result
        sites = stage['inputs']['cutsites']
        if len(calls) != len(sites):
            raise ValueError('生产 allele 数目不等于请求 cutsite 数目')
        output = {'intbcs': text([intbc]), 'sites': np.asarray(sites, dtype=np.int64)}
        output.update(pack_calls(calls))
        return output
    sites = stage['kwargs']['cutsite_locations']
    columns = ['r' + str(i + 1) for i in range(len(sites))]
    calls = [row[column] for _, row in result.iterrows() for column in columns]
    output = {'cells': text(result['cellBC']), 'umis': text(result['UMI']),
              'read_counts': np.asarray([integer(x) for x in result['readCount']], dtype=np.int64),
              'intbcs': text(result['intBC']), 'sites': np.asarray(sites, dtype=np.int64)}
    output.update(pack_calls(calls))
    output.update({'aggregate.' + name: value for name, value in pack_calls(result['allele']).items()})
    return output


def main():
    import pandas as pd
    import cassiopeia as cas
    from cassiopeia.preprocess import alignment_utilities

    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    inputs = json.loads(args.inputs.read_text())
    arrays, timings = {}, []
    for stage in inputs['stages']:
        start = time.perf_counter()
        if stage['kind'] == 'cigar':
            result = alignment_utilities.parse_cigar(**stage['inputs'])
        else:
            result = cas.pp.call_alleles(pd.DataFrame(stage['alignments']), **stage['kwargs'])
        elapsed = time.perf_counter() - start
        arrays.update({stage['id'] + '.' + name: value for name, value in pack_result(result, stage).items()})
        timings.append({'id': stage['id'], 'selector': stage['selector'], 'elapsed_seconds': elapsed})
    args.out.mkdir(parents=True, exist_ok=True)
    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(json.dumps({'ungraded': True, 'stages': timings}, indent=2) + '\n')
    print(f'完成 {len(timings)} 个固定官方生产调用；未重新运行 alignment')


if __name__ == '__main__':
    main()
