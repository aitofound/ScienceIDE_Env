#!/usr/bin/env python3
"""固定官方输入直接调用生产 API，仅序列化返回对象，不重算转换答案。"""
from __future__ import annotations

import argparse
import json
from numbers import Integral, Real
from pathlib import Path
import time

import numpy as np
import pandas as pd
import cassiopeia as cas


def text(values):
    values = list(values)
    if any(not isinstance(value, str) for value in values):
        raise TypeError('身份与 allele 必须是字符串')
    return np.asarray(values, dtype=str)


def integer(value):
    if isinstance(value, (bool, np.bool_)) or not isinstance(value, Integral):
        raise TypeError(f'非整数生产状态: {value!r}')
    return int(value)


def floats(values):
    values = list(values)
    if any(isinstance(x, (bool, np.bool_)) or not isinstance(x, Real) for x in values):
        raise TypeError('生产概率或计数不是实数')
    values = np.asarray(values, dtype=np.float64)
    if not np.isfinite(values).all():
        raise ValueError('生产概率或计数含 NaN/Inf')
    return values


def pack_matrix(result):
    matrix, priors, mapping = result
    cells, characters = text(matrix.index), text(matrix.columns)
    states, offsets = [], [0]
    for value in matrix.to_numpy().ravel():
        items = value if isinstance(value, tuple) else (value,)
        states.extend(integer(item) for item in items)
        offsets.append(len(states))
    output = {'cells': cells, 'characters': characters,
              'offsets': np.asarray(offsets, dtype=np.int64),
              'states': np.asarray(states, dtype=np.int64)}
    for name, dictionaries in [('map', mapping), ('prior', priors)]:
        columns, codes, values = [], [], []
        for character, values_by_state in dictionaries.items():
            position = integer(character)
            if not 0 <= position < len(characters):
                raise ValueError('生产映射引用不存在的字符')
            for state, value in values_by_state.items():
                columns.append(characters[position])
                codes.append(integer(state))
                values.append(value)
        output[name + '_characters'] = text(columns)
        output[name + '_states'] = np.asarray(codes, dtype=np.int64)
        output[name + ('_alleles' if name == 'map' else '_values')] = text(values) if name == 'map' else floats(values)
    return output


def pack_profile(profile):
    alleles, missing, offsets = [], [], [0]
    for value in profile.to_numpy().ravel():
        items = value if isinstance(value, tuple) else (value,)
        for item in items:
            if isinstance(item, str):
                alleles.append(item)
                missing.append(0)
            elif item is None or isinstance(item, Real) and np.isnan(item):
                alleles.append('')
                missing.append(1)
            else:
                raise TypeError(f'非法生产 allele: {item!r}')
        offsets.append(len(alleles))
    return {'cells': text(profile.index), 'loci': text(profile.columns),
            'offsets': np.asarray(offsets, dtype=np.int64), 'alleles': text(alleles),
            'missing': np.asarray(missing, dtype=np.int8)}


def pack_empirical(table):
    return {'alleles': text(table.index), 'counts': floats(table['count']),
            'frequencies': floats(table['freq'])}


def allele_token(value):
    if isinstance(value, str):
        if value == 'None':
            return 'wildtype'
        raise ValueError('官方逆转换固定输入不含其他字符串 allele')
    return 'state:' + str(integer(value))


def pack_alleletable(table):
    return {'cells': text(table['cellBC']), 'loci': text(table['intBC']),
            'alleles': text(allele_token(x) for x in table['allele']),
            'r1': text(allele_token(x) for x in table['r1']),
            'umi': np.asarray([integer(x) for x in table['UMI']], dtype=np.int64)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    inputs = json.loads(args.inputs.read_text())
    mutation_priors = pd.DataFrame.from_dict(inputs['mutation_priors'], orient='index', columns=['freq'])
    apis = {name: getattr(cas.pp, name) for name in (
        'convert_alleletable_to_character_matrix', 'convert_alleletable_to_lineage_profile',
        'convert_lineage_profile_to_character_matrix', 'compute_empirical_indel_priors',
        'convert_character_matrix_to_allele_table')}
    packers = {'matrix': pack_matrix, 'profile': pack_profile,
               'empirical': pack_empirical, 'alleletable': pack_alleletable}
    results, arrays, timings = {}, {}, []
    for stage in inputs['stages']:
        if 'source_stage' in stage:
            frame = results[stage['source_stage']].copy(deep=True)
        else:
            fixture = inputs['fixtures'][stage['fixture']]
            frame = pd.DataFrame(**fixture) if stage['kind'] == 'alleletable' else pd.DataFrame(fixture)
        if 'fillna' in stage:
            frame.fillna(stage['fillna'], inplace=True)
        kwargs = {key: mutation_priors.copy() if isinstance(value, str) and value == '$mutation_priors' else value
                  for key, value in stage['kwargs'].items()}
        start = time.perf_counter()
        result = apis[stage['api']](frame, **kwargs)
        elapsed = time.perf_counter() - start
        results[stage['id']] = result
        for name, values in packers[stage['kind']](result).items():
            arrays[stage['id'] + '.' + name] = values
        timings.append({'id': stage['id'], 'selector': inputs['upstream'] + '::' + stage['selector'],
                        'api': stage['api'], 'elapsed_seconds': elapsed})
    args.out.mkdir(parents=True, exist_ok=True)
    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(json.dumps({'ungraded': True, 'stages': timings}, indent=2) + '\n')
    print(f'已输出 {len(timings)} 个生产阶段；{len(set(s["selector"] for s in inputs["stages"]))} 个官方方法')


if __name__ == '__main__':
    main()
