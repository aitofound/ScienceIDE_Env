"""产出 umi-collapse 的受判产物。

覆盖 collapse_umi_test.py 的全部七个 test：六个 BAM 阶段（两次 sort_bam、四次
form_collapsed_clusters）外加一次 `utilities.convert_bam_to_df`。

输入是随 pinned 源码一起 vendored 的三个小 BAM 的逐字节副本，随 `ic/` 一起提交并记录
了来源路径与 sha256，**不新增任何数据**。`sort_bam` 与 `form_collapsed_clusters` 都要
写文件，所以先把它们复制到临时目录。

**受判文件内顺序**，不做规范化重排：`sort_bam` 的全部意义就是定序，上游也按位置断言
（`cellBCs[10]`、`quals[2][0]`）。这与同 leaf 其它 check 里「storage order 不受判」的
处理不同，因为这里顺序就是观测量本身。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import logging
import tempfile
import time
import warnings
from pathlib import Path

import numpy as np
import pysam

from cassiopeia.preprocess import UMI_utils, utilities

# ZR（读数）是整数；ZC（cluster id）不是——它形如 '0+'，带链方向后缀，按字符串评。
INT_TAGS = {'ZR'}


def make_sort_key(spec):
    if spec == 'default':
        return None
    tags = list(spec)
    return lambda al: tuple(al.get_tag(t) for t in tags)


def make_filter(spec):
    if spec == 'default':
        return None
    return lambda al: al.has_tag(spec['has_tag'])


def run_sort(stage, paths, work):
    out = work / f'{stage["id"]}.bam'
    kwargs = {}
    key = make_sort_key(stage['sort_key'])
    if key is not None:
        kwargs['sort_key'] = key
    filt = make_filter(stage['filter'])
    if filt is not None:
        kwargs['filter_func'] = filt
    UMI_utils.sort_bam(str(paths[stage['input']]), str(out), **kwargs)
    return out


def run_collapse(stage, paths, work):
    out = work / f'{stage["id"]}.bam'
    kwargs = dict(stage['params'])
    if 'cell_key_tag' in stage:
        tag = stage['cell_key_tag']
        kwargs['cell_key'] = lambda al, tag=tag: al.get_tag(tag)
    UMI_utils.form_collapsed_clusters(
        str(paths[stage['input']]), str(out), **kwargs)
    return out


def observe_bam(path, stage):
    names, tags, seqs, quals = [], {t: [] for t in stage['tags']}, [], []
    with pysam.AlignmentFile(str(path), 'rb', check_sq=False) as handle:
        for al in handle.fetch(until_eof=True):
            names.append(al.query_name)
            for tag in stage['tags']:
                if not al.has_tag(tag):
                    raise ValueError(
                        f'{stage["id"]}: 记录 {al.query_name} 缺标签 {tag}')
                tags[tag].append(al.get_tag(tag))
            seqs.append(al.query_sequence or '')
            quality = al.query_qualities
            quals.append('' if quality is None
                         else ''.join(chr(q + 33) for q in quality))
    sid = stage['id']
    arrays = {f'{sid}.record_count': np.asarray(len(names), dtype=np.int64),
              f'{sid}.query_names': np.asarray(names, dtype=str)}
    for tag, values in tags.items():
        arrays[f'{sid}.tag_{tag}'] = np.asarray(
            values, dtype=np.int64 if tag in INT_TAGS else str)
    if stage.get('grade_sequence'):
        arrays[f'{sid}.sequences'] = np.asarray(seqs, dtype=str)
    if stage.get('grade_qualities'):
        arrays[f'{sid}.qualities'] = np.asarray(quals, dtype=str)
    return arrays


def observe_frame(frame):
    return {
        'bam2df.shape': np.asarray(list(frame.shape), dtype=np.int64),
        'bam2df.columns': np.asarray(list(frame.columns), dtype=str),
        'bam2df.values': np.asarray(
            [str(v) for row in frame.itertuples(index=False) for v in row],
            dtype=str),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()

    logging.disable(logging.CRITICAL)
    warnings.filterwarnings('ignore')
    config = json.loads(args.inputs.read_text())
    args.out.mkdir(parents=True, exist_ok=True)
    if any(p.name not in {'run.ok', 'run.failed', 'run.skipped', 'run.log'}
           for p in args.out.iterdir()):
        raise ValueError('OUT_DIR 必须为空')

    arrays, timings = {}, {}
    with tempfile.TemporaryDirectory() as scratch:
        work = Path(scratch)
        paths = {}
        for name, spec in config['inputs'].items():
            source = args.inputs.parent / spec['file']
            data = source.read_bytes()
            if hashlib.sha256(data).hexdigest() != spec['sha256']:
                raise ValueError(f'{spec["file"]} 的 sha256 与 IC 记录不符')
            paths[name] = work / f'{name}.bam'
            paths[name].write_bytes(data)

        for stage in config['stages']:
            start = time.perf_counter()
            if stage['op'] == 'sort_bam':
                produced = run_sort(stage, paths, work)
            elif stage['op'] == 'form_collapsed_clusters':
                produced = run_collapse(stage, paths, work)
            else:
                raise ValueError(f'未知 op: {stage["op"]}')
            paths[stage['id']] = produced
            arrays.update(observe_bam(produced, stage))
            timings[stage['id']] = time.perf_counter() - start

        start = time.perf_counter()
        frame = utilities.convert_bam_to_df(str(paths[config['bam2df_stage']]))
        arrays.update(observe_frame(frame))
        timings['bam2df'] = time.perf_counter() - start

    np.savez(args.out / 'results.npz', **arrays)
    (args.out / 'diagnostics.json').write_text(
        json.dumps({'ungraded': True, 'timings': timings}, indent=2) + '\n')
    print(f'已导出 {len(arrays)} 个数组，覆盖 {len(config["stages"])} 个 BAM 阶段'
          f'与一次 bam2df')


if __name__ == '__main__':
    main()
