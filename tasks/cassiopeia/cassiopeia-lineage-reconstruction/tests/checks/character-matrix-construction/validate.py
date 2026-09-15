#!/usr/bin/env python3
"""完整科学对象等价：有身份的表对齐身份，匿名字符按完整联合列匹配。"""
from __future__ import annotations

import argparse
from collections import defaultdict
import json
from pathlib import Path
import sys
import traceback
import zipfile
import zlib

import numpy as np

FIELDS = {
    'empirical': ('alleles', 'counts', 'frequencies'),
    'alleletable': ('cells', 'loci', 'alleles', 'r1', 'umi'),
    'profile': ('cells', 'loci', 'offsets', 'alleles', 'missing'),
    'matrix': ('cells', 'characters', 'offsets', 'states', 'map_characters',
               'map_states', 'map_alleles', 'prior_characters', 'prior_states', 'prior_values'),
}


def array(data, name, kind, size=None):
    value = data[name]
    if value.ndim != 1 or value.dtype.kind not in kind:
        raise ValueError(f'{name}: 非法 shape 或 dtype {value.shape}/{value.dtype}')
    if size is not None and value.size != size:
        raise ValueError(f'{name}: 长度 {value.size} 应为 {size}')
    if value.dtype.kind in 'f' and (value.dtype.itemsize != 8 or not np.isfinite(value).all()):
        raise ValueError(f'{name}: 需要有限 binary64 数值')
    return value


def identities(data, name):
    values = array(data, name, 'U').tolist()
    if not values or any(not x for x in values) or len(set(values)) != len(values):
        raise ValueError(f'{name}: 身份为空或重复')
    return values


def ragged(data, prefix, total, length):
    offsets = array(data, prefix + '.offsets', 'iu', total + 1)
    if offsets[0] != 0 or offsets[-1] != length or np.any(offsets[1:] <= offsets[:-1]):
        raise ValueError(f'{prefix}: 非法 ragged offsets 或空状态')
    return offsets


def matrix(data, stage):
    p = stage['id']
    cells = identities(data, p + '.cells')
    characters = identities(data, p + '.characters')
    states = array(data, p + '.states', 'iu')
    offsets = ragged(data, p, len(cells) * len(characters), len(states))
    missing = stage['missing']
    if missing >= 0 or isinstance(missing, bool):
        raise ValueError('missing 状态必须为负整数')
    mc = array(data, p + '.map_characters', 'U')
    ms = array(data, p + '.map_states', 'iu', len(mc))
    ma = array(data, p + '.map_alleles', 'U', len(mc))
    mappings = {c: {} for c in characters}
    for character, state, allele in zip(mc, ms, ma):
        if character not in mappings or state <= 0 or not allele or state in mappings[character]:
            raise ValueError(f'{p}: 非法或重复 state→allele 映射')
        mappings[character][int(state)] = str(allele)
    for mapping in mappings.values():
        if len(set(mapping.values())) != len(mapping):
            raise ValueError(f'{p}: 同字符的 allele 映射非单射')
    pc = array(data, p + '.prior_characters', 'U')
    ps = array(data, p + '.prior_states', 'iu', len(pc))
    pv = array(data, p + '.prior_values', 'f', len(pc))
    priors = {c: {} for c in characters}
    for character, state, value in zip(pc, ps, pv):
        if character not in priors or state not in mappings[character] or state in priors[character]:
            raise ValueError(f'{p}: prior 未绑定唯一已映射突变')
        if not 0 < value <= 1:
            raise ValueError(f'{p}: prior 不在 (0,1]')
        priors[character][int(state)] = float(value)
    groups = defaultdict(list)
    for j, character in enumerate(characters):
        mapping, prior = mappings[character], priors[character]
        if set(prior) != (set(mapping) if stage['priors'] else set()):
            raise ValueError(f'{p}: prior 覆盖与映射不一致')
        column, used = [], set()
        for i in sorted(range(len(cells)), key=lambda x: cells[x]):
            index = i * len(characters) + j
            decoded = []
            for state in states[int(offsets[index]):int(offsets[index + 1])]:
                if state == missing:
                    decoded.append(('missing', ''))
                elif state == 0:
                    decoded.append(('wildtype', ''))
                elif state in mapping:
                    used.add(int(state))
                    decoded.append(('allele', mapping[state]))
                else:
                    raise ValueError(f'{p}: state 缺少映射或特殊状态错误')
            column.append((cells[i], tuple(sorted(decoded))))
        if used != set(mapping):
            raise ValueError(f'{p}: 映射包含矩阵中不存在的状态')
        alleles = tuple(sorted(mapping.values()))
        # 整列全部 cell/allele 联合关系和映射是离散结构，不以 prior 浮点作身份。
        signature = (tuple(column), alleles)
        by_allele = {mapping[state]: value for state, value in prior.items()}
        groups[signature].append(np.array([by_allele[a] for a in alleles] if stage['priors'] else [], dtype=np.float64))
    return groups


def profile(data, stage):
    p = stage['id']
    cells = identities(data, p + '.cells')
    loci = identities(data, p + '.loci')
    alleles = array(data, p + '.alleles', 'U')
    missing = array(data, p + '.missing', 'iu', len(alleles))
    if not np.isin(missing, [0, 1]).all() or np.any((missing == 1) != (alleles == '')):
        raise ValueError(f'{p}: 非法 missing 标签或空 allele')
    offsets = ragged(data, p, len(cells) * len(loci), len(alleles))
    result = {}
    for i, cell in enumerate(cells):
        for j, locus in enumerate(loci):
            index = i * len(loci) + j
            start, end = int(offsets[index]), int(offsets[index + 1])
            signature = (cell, locus, tuple(sorted(zip(missing[start:end].tolist(), alleles[start:end].tolist()))))
            result[signature] = [np.empty(0, dtype=np.float64)]
    return result


def empirical(data, stage):
    p = stage['id']
    alleles = identities(data, p + '.alleles')
    counts = array(data, p + '.counts', 'f', len(alleles))
    frequencies = array(data, p + '.frequencies', 'f', len(alleles))
    if np.any(counts < 1) or np.any(counts != np.floor(counts)) or np.any((frequencies <= 0) | (frequencies > 1)):
        raise ValueError(f'{p}: 非法 group count 或经验频率')
    # count 是离散科学观测，不是 identity；放入精确比较元组而非浮点容差。
    return {(allele, int(count)): [np.array([freq], dtype=np.float64)]
            for allele, count, freq in zip(alleles, counts, frequencies)}


def alleletable(data, stage):
    p = stage['id']
    cells = array(data, p + '.cells', 'U')
    loci = array(data, p + '.loci', 'U', len(cells))
    alleles = array(data, p + '.alleles', 'U', len(cells))
    r1 = array(data, p + '.r1', 'U', len(cells))
    umi = array(data, p + '.umi', 'iu', len(cells))
    keys = list(zip(cells.tolist(), loci.tolist()))
    if not keys or len(set(keys)) != len(keys) or any(not c or not l for c, l in keys):
        raise ValueError(f'{p}: allele table 身份为空或重复')
    if np.any(umi < 1) or any(not a for a in alleles) or any(not a for a in r1):
        raise ValueError(f'{p}: allele table 非法 payload')
    return {(cell, locus, allele, cutsite, int(count)): [np.empty(0, dtype=np.float64)]
            for (cell, locus), allele, cutsite, count in zip(keys, alleles, r1, umi)}


def edge_error(reference, candidate, atol, rtol):
    error = np.abs(candidate - reference)
    bound = atol + rtol * np.abs(reference)
    fraction = np.divide(error, bound, out=np.full_like(error, np.inf), where=bound > 0)
    fraction[(error == 0) & (bound == 0)] = 0
    return float(error.max(initial=0)), float(fraction.max(initial=0))


def match_columns(reference, candidate, atol, rtol):
    if len(reference) != len(candidate):
        raise ValueError('匿名字符的重复数量改变')
    n = len(reference)
    errors = [[edge_error(r, c, atol, rtol) for c in candidate] for r in reference]

    def match(threshold):
        owner = [-1] * n

        def augment(i, visited):
            for j in range(n):
                if j in visited or errors[i][j][1] > threshold:
                    continue
                visited.add(j)
                if owner[j] == -1 or augment(owner[j], visited):
                    owner[j] = i
                    return True
            return False

        return owner if all(augment(i, set()) for i in range(n)) else None

    # 最小瓶颈二分匹配只匹配 payload；不会用被比较的 float 建立或排序身份。
    thresholds = sorted({pair[1] for row in errors for pair in row})
    low, high = 0, len(thresholds) - 1
    while low < high:
        middle = (low + high) // 2
        if match(thresholds[middle]) is None:
            low = middle + 1
        else:
            high = middle
    owner = match(thresholds[low])
    if owner is None:
        raise ValueError('找不到完整列匹配')
    selected = [errors[i][j] for j, i in enumerate(owner)]
    return max(x[0] for x in selected), max(x[1] for x in selected)


def compare(data_r, data_c, stages, atol, rtol):
    worst, fraction = 0.0, 0.0
    details = {}
    for stage in stages:
        p = stage['id']
        reader = {'matrix': matrix, 'profile': profile, 'empirical': empirical,
                  'alleletable': alleletable}[stage['kind']]
        r, c = reader(data_r, stage), reader(data_c, stage)
        if set(r) != set(c):
            raise ValueError(f'{p}: 完整 cell/allele 列或映射改变')
        distance, bound_fraction = 0.0, 0.0
        for key in r:
            d, f = match_columns(r[key], c[key], atol, rtol)
            distance, bound_fraction = max(distance, d), max(bound_fraction, f)
        details[p] = {'distance': distance,
                      'bound_fraction': bound_fraction if np.isfinite(bound_fraction) else None}
        worst, fraction = max(worst, distance), max(fraction, bound_fraction)
    return worst, fraction, details


def load(path, expected):
    with np.load(path, allow_pickle=False) as archive:
        if len(archive.files) != len(set(archive.files)) or set(archive.files) != expected:
            raise ValueError(f'{path.name}: NPZ 字段缺失、额外或重复')
        return {key: archive[key] for key in archive.files}


def failed_result(exc, context):
    error_type = f'{type(exc).__module__}.{type(exc).__qualname__}'
    return {'passed': False, 'policy': 'invariants', 'distance': None, 'bound_fraction': None,
            'error_type': error_type, 'context': context,
            'reason': f'无法比较科学产物 ({context}): {error_type}'}


def main():
    parser = argparse.ArgumentParser()
    for name in ('reference', 'candidate', 'rubric', 'out'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args()
    result = {'passed': False, 'policy': 'invariants', 'distance': None, 'bound_fraction': None}
    context = '读取 rubric 与阶段配置'
    try:
        try:
            rubric = json.loads(Path(args.rubric).read_text())
            comparison = rubric['comparison']
            atol, rtol = float(comparison['atol']), float(comparison['rtol'])
            if not np.isfinite([atol, rtol]).all() or min(atol, rtol) < 0:
                raise ValueError('容差必须有限且非负')
            stages = comparison['stages']
            if not stages or len({s['id'] for s in stages}) != len(stages):
                raise ValueError('stage inventory 为空或重复')
            expected = {s['id'] + '.' + field for s in stages for field in FIELDS[s['kind']]}
            context = '解码 reference results.npz'
            r = load(Path(args.reference) / 'results.npz', expected)
            context = '解码 candidate results.npz'
            c = load(Path(args.candidate) / 'results.npz', expected)
            context = '比较完整科学对象'
            distance, fraction, details = compare(r, c, stages, atol, rtol)
            result.update(passed=fraction <= 1, distance=distance,
                          bound_fraction=fraction if np.isfinite(fraction) else None, stages=details,
                          reason='完整科学对象等价' if fraction <= 1 else 'prior 或频率超出科学容差')
        except (OSError, ValueError, TypeError, KeyError, IndexError, EOFError,
                zipfile.BadZipFile, zlib.error, RuntimeError, OverflowError) as exc:
            result = failed_result(exc, context)
            result['reason'] += f': {exc}'
        context = '严格 JSON 序列化结果'
        serialized = json.dumps(result, ensure_ascii=True, allow_nan=False, indent=2)
    except Exception as exc:
        # 最后协议边界：丢弃可能已含 passed=True 或非法 JSON 值的部分结果。
        # 不枚举压缩 codec，也不吞掉 KeyboardInterrupt/SystemExit 等 BaseException。
        result = failed_result(exc, context)
        traceback.print_exc(file=sys.stderr)
        serialized = json.dumps(result, ensure_ascii=True, allow_nan=False, indent=2)
    # 完整序列化后才触碰目标；目标路径不可写仍是独立的环境 I/O 失败。
    Path(args.out).write_text(serialized + '\n')
    print(result['reason'], file=sys.stderr)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
