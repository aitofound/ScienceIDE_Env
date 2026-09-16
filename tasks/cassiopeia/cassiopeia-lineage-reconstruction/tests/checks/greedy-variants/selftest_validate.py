#!/usr/bin/env python3
"""独立人工 greedy-variants 自测；不读取 HOME、生产 source 或真实 nominal 结果。

常数策略（结构 test-first、常数探查回填）：
  * EXPECTED_SPLIT / EXPECTED_TOPOLOGY / EXPECTED_ERROR 先留空，
    再由**一次性探查**调用官方 SpectralGreedySolver.perform_split /
    MaxCutGreedySolver.perform_split / solve / CassiopeiaTree.get_tree_topology 回填。
  * 三元组结构的期望值由本文件里的 triplet_structure() 就地算出——它是官方 test 顶部
    那个 find_triplet_structure 的直译，不构成循环论证。

与本 leaf 其它 check 的两处**故意不同**，都必须写明：
  1. 本 check 的字符矩阵 schema **接受 ambiguous state**（int 或 int 列表），
     因为官方 test_raises_error_on_ambiguous 要求在 ambiguous 输入上抛 GreedySolverError。
     maxcut 那个 check 在 schema 层拒绝 ambiguous（只收纯 int），**那层保护在这里不存在**。
  2. 因此本 check 的独立复算**必须复现** unravel_ambiguous_states，不能像 maxcut 那样省略。

**评分身份是「重复组」而不是「样本名」。** 逐位相同的字符向量在这个算法里不可区分，
所以 drop_duplicates 之后「哪一个代表活下来」是 bookkeeping，不是科学答案。
[measured] 扫全排列：按样本名，maxcut 两个配置的 left 有 2 种；按重复组，五个配置全部唯一。
按样本名评分会拒掉一个只是按不同顺序遍历行的合法实现，而它在科学上完全正确。

同理官方用 assertListEqual 断言的**列表顺序**也不是不变量。两处都是「约束得比科学更多」，
而第二处更难认——**因为「样本名」看起来天经地义就是身份**。

**topology 只有两条腿。** partition 有三条（候选↔参考、各侧↔独立复算），
topology **没有独立复算腿**：独立复现整条贪心递归要复现九层，每层都需单独的保真审计。
详见 test_topology_has_no_independent_leg_so_both_sides_wrong_passes。
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

# --- 合成 fixture（官方 fixture 只出现在 ic/ 里）------------------------------

# 两个变体共用贪心框架，只有划分准则不同，所以同一张矩阵两个 solver 各跑一遍。
SYN = {'columns': ['a', 'b', 'c', 'd'],
       'order': ['s1', 's2', 's3', 's4', 's5'],
       'rows': {'s1': [7, 4, 0, 0], 's2': [7, 0, 5, 1], 's3': [0, 4, 5, 1],
                's4': [7, 4, 0, 0], 's5': [-1, 4, 0, 0]}}
# s5 的 char2 由 5 改成 0：第一版 char1/state4 与 char2/state5 的加权频率并列，
# 而 validator 的 argmax 并列展开证明那**两种划分不同** —— 合成 fixture 自己不可评。
# 那是自保护抓到了我自己的 fixture（当时 validator 还没展开 argmax 并列，所以先前 GREEN 过）。
# 含 ambiguous state（元组在 JSON 里写成列表）——maxcut 的 schema 写不出这一行。
AMBIGUOUS = {'columns': ['a', 'b', 'c', 'd'],
             'order': ['s1', 's2', 's3'],
             'rows': {'s1': [7, [0, 4], 5, 1], 's2': [0, 4, 5, 1], 's3': [7, 4, 0, 0]}}
# 阴性对照：与 AMBIGUOUS **只差一处**——s1 的第 1 位由 [0, 4] 换成普通整数 4，其余全同。
# 阴性对照是承重的不是配平的：只评异常类型的话，一个「永远抛」的实现也会通过。
# 而对照差得越远，它排除的假设越少——拿一个任意正常矩阵只能排除「永远抛」，
# 只差一处才能排除「抛的条件过宽」。
NEAR_MISS = {'columns': ['a', 'b', 'c', 'd'],
             'order': ['s1', 's2', 's3'],
             'rows': {'s1': [7, 4, 5, 1], 's2': [0, 4, 5, 1], 's3': [7, 4, 0, 0]}}
PRIORS = {'0': {'7': 0.5}, '1': {'4': 0.25}, '2': {'5': 0.5}, '3': {'1': 0.2}}   # 0.125 会让 char3 的加权频率与 char1 并列（都是 4.159）

CONFIGS = [
    {'id': 'spectral_plain', 'solver': 'spectral', 'matrix': 'syn', 'priors': None},
    {'id': 'spectral_priors', 'solver': 'spectral', 'matrix': 'syn', 'priors': 'p'},
    {'id': 'maxcut_plain', 'solver': 'maxcut', 'matrix': 'syn', 'priors': None},
    {'id': 'maxcut_priors', 'solver': 'maxcut', 'matrix': 'syn', 'priors': 'p'},
]
# 负向断言转成一对正向断言：阳性探针必须抛出**声明的那个类型**，阴性对照必须不抛。
ERROR_PROBES = [
    {'id': 'ambiguous_raises', 'solver': 'spectral', 'matrix': 'ambiguous', 'priors': None},
    {'id': 'near_miss_does_not_raise', 'solver': 'spectral', 'matrix': 'near_miss',
     'priors': None},
]

# ===== 一次性探查回填的常数（探查前留空）=====================================
# EXPECTED_SPLIT[config] = {'left': [...], 'right': [...]}
#   —— 产物里写的是**样本名**（producer 只能拿到名字），但判分把每个名字映到它的
#      **重复组**（逐位相同的字符向量）之后再比较集合。见 test_split_representative_swap_passes。
EXPECTED_SPLIT = {
    "spectral_plain": {
        "left": [
            "s1",
            "s3",
            "s5"
        ],
        "right": [
            "s2"
        ]
    },
    "spectral_priors": {
        "left": [
            "s1",
            "s5"
        ],
        "right": [
            "s2",
            "s3"
        ]
    },
    "maxcut_plain": {
        "left": [
            "s1",
            "s5"
        ],
        "right": [
            "s2",
            "s3"
        ]
    },
    "maxcut_priors": {
        "left": [
            "s1",
            "s5"
        ],
        "right": [
            "s2",
            "s3"
        ]
    }
}
# [measured] 本合成 fixture 上扫全排列（120 序）：按**样本名** spectral_plain/spectral_priors/
# maxcut_priors 各有 2 种 left，按**重复组**四个配置全部唯一——这个面真被压到了，
# 不是「fixture 里恰好没有重复行」那种空覆盖。
# EXPECTED_TOPOLOGY[config] = {'<a|b|c>': 'ab' | 'ac' | 'bc' | '-'}
EXPECTED_TOPOLOGY = {
    "spectral_plain": {
        "s1|s2|s3": "ac",
        "s1|s2|s4": "ac",
        "s1|s2|s5": "ac",
        "s1|s3|s4": "ac",
        "s1|s3|s5": "-",
        "s1|s4|s5": "ab",
        "s2|s3|s4": "bc",
        "s2|s3|s5": "bc",
        "s2|s4|s5": "bc",
        "s3|s4|s5": "-"
    },
    "spectral_priors": {
        "s1|s2|s3": "bc",
        "s1|s2|s4": "ac",
        "s1|s2|s5": "ac",
        "s1|s3|s4": "ac",
        "s1|s3|s5": "ac",
        "s1|s4|s5": "-",
        "s2|s3|s4": "ab",
        "s2|s3|s5": "ab",
        "s2|s4|s5": "bc",
        "s3|s4|s5": "bc"
    },
    "maxcut_plain": {
        "s1|s2|s3": "bc",
        "s1|s2|s4": "ac",
        "s1|s2|s5": "ac",
        "s1|s3|s4": "ac",
        "s1|s3|s5": "ac",
        "s1|s4|s5": "-",
        "s2|s3|s4": "ab",
        "s2|s3|s5": "ab",
        "s2|s4|s5": "bc",
        "s3|s4|s5": "bc"
    },
    "maxcut_priors": {
        "s1|s2|s3": "bc",
        "s1|s2|s4": "ac",
        "s1|s2|s5": "ac",
        "s1|s3|s4": "ac",
        "s1|s3|s5": "ac",
        "s1|s4|s5": "-",
        "s2|s3|s4": "ab",
        "s2|s3|s5": "ab",
        "s2|s4|s5": "bc",
        "s3|s4|s5": "bc"
    }
}
# EXPECTED_ERROR[probe] = 异常的完整限定类名，或 None 表示不抛
EXPECTED_ERROR = {
    "ambiguous_raises": "cassiopeia.mixins.errors.GreedySolverError",
    "near_miss_does_not_raise": None
}
# ============================================================================


def triplet_structure(ancestors, a, b, c):
    """官方 find_triplet_structure 的直译：谁与谁共享的祖先更多。"""
    ab = len(ancestors[a] & ancestors[b])
    ac = len(ancestors[a] & ancestors[c])
    bc = len(ancestors[b] & ancestors[c])
    if ab > bc and ab > ac:
        return 'ab'
    if ac > bc and ac > ab:
        return 'ac'
    if bc > ab and bc > ac:
        return 'bc'
    return '-'


def rubric(configs=None, probes=None, matrices=None, **override):
    comparison = {'atol': 0, 'rtol': 0, 'missing_state_indicator': MISSING,
                  'prior_transformation': 'negative_log',
                  'prior_tables': {'p': PRIORS},
                  'character_matrices': matrices or {'syn': SYN, 'ambiguous': AMBIGUOUS,
                                                    'near_miss': NEAR_MISS},
                  'configs': CONFIGS if configs is None else configs,
                  'error_probes': ERROR_PROBES if probes is None else probes}
    comparison.update(override)
    return {'comparison': comparison}


def artifact():
    return {'schema_version': 1,
            'split': [{'config': c['id'], **copy.deepcopy(EXPECTED_SPLIT[c['id']])}
                      for c in CONFIGS],
            'topology': [{'config': c['id'],
                          'triplets': copy.deepcopy(EXPECTED_TOPOLOGY[c['id']])}
                         for c in CONFIGS],
            'error_behaviour': [{'probe': p['id'], 'raised': EXPECTED_ERROR[p['id']]}
                                for p in ERROR_PROBES]}


class GreedyVariantsContract(unittest.TestCase):
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

    def mutate(self, change, side='candidate'):
        reference, candidate = artifact(), artifact()
        change(candidate if side == 'candidate' else reference)
        return self.verdict(reference, candidate, expected=False)

    def contract_failure(self, result):
        self.assertIs(result['passed'], False, result)
        self.assertIn('error_type', result)
        self.assertIsNone(result['distance'], result)

    def scientific_rejection(self, result):
        self.assertIs(result['passed'], False, result)
        self.assertNotIn('error_type', result)
        self.assertIn('measurements', result)

    # --- GREEN ---------------------------------------------------------------

    def test_green_matches_independent_recomputation(self):
        result = self.verdict(artifact(), artifact(), expected=True)
        self.assertEqual(result['policy'], 'pointwise')
        measured = result['measurements']
        self.assertEqual(measured['split_configs'], 4)
        self.assertEqual(measured['topology_configs'], 4)
        self.assertEqual(measured['error_probes'], 2)
        self.assertEqual(measured['categorical_mismatches'], 0)

    def test_ambiguous_probe_raises_and_control_does_not(self):
        """负向断言转成一对正向断言：阳性抛出声明的类型，阴性对照不抛。"""
        self.assertIsNotNone(EXPECTED_ERROR['ambiguous_raises'])
        self.assertIn('GreedySolverError', EXPECTED_ERROR['ambiguous_raises'])
        self.assertIsNone(EXPECTED_ERROR['near_miss_does_not_raise'])

    def test_duplicate_row_is_dropped_before_the_split(self):
        """s4 与 s1 逐位相同，drop_duplicates 只留首次出现的那一行。"""
        for config in CONFIGS:
            members = set(EXPECTED_SPLIT[config['id']]['left']) | \
                      set(EXPECTED_SPLIT[config['id']]['right'])
            self.assertNotIn('s4', members)

    # --- 划分（按集合）--------------------------------------------------------

    def test_split_reordered_side_passes(self):
        """按集合评分：官方虽用 assertListEqual，但那个顺序不是不变量。"""
        reference, candidate = artifact(), artifact()
        for row in candidate['split']:
            row['left'] = list(reversed(row['left']))
            row['right'] = list(reversed(row['right']))
        self.verdict(reference, candidate, expected=True)

    def test_split_representative_swap_passes(self):
        """换一个重复组代表**必须通过**——那是 drop_duplicates 的 bookkeeping，不是科学差别。

        [measured] 扫全排列：按样本名，maxcut 两个配置的 left 有 2 种；按重复组全部唯一。
        按样本名评分会拒掉一个只是按不同顺序遍历行的合法实现。
        """
        groups = {}
        for cell in SYN['order']:
            groups.setdefault(tuple(SYN['rows'][cell]), []).append(cell)
        swappable = {members[0]: members[1] for members in groups.values() if len(members) > 1}
        self.assertTrue(swappable, '合成矩阵里必须有重复行，否则这条测试是空的')
        reference, candidate = artifact(), artifact()
        for row in candidate['split']:
            for side in ('left', 'right'):
                row[side] = [swappable.get(name, name) for name in row[side]]
        self.verdict(reference, candidate, expected=True)

    def test_grouping_does_not_depend_on_what_the_reference_submitted(self):
        """**独立复算必须独立于被判对象，不只是独立于另一个 validator。**

        重复组划分是从**可信字符矩阵**独立算出的，不是「看参考交了哪些名字、
        把没出现的归到出现的那个组里」——后者是拿被判对象定义判据，最省事的写法恰好是循环的。
        判据（reviewer 给的）：**参考交上来一份错的存活代表，重复组划分必须不变。**
        """
        groups = {}
        for cell in SYN['order']:
            groups.setdefault(tuple(SYN['rows'][cell]), []).append(cell)
        swappable = {members[0]: members[1] for members in groups.values() if len(members) > 1}
        self.assertTrue(swappable)
        # 参考侧换代表、候选侧不换：若判据是从参考推的，这里会「跟着变」从而通过得莫名其妙；
        # 若判据独立，两侧映到同一组，仍然通过——且与两侧都不换时**判决完全相同**。
        baseline = self.verdict(artifact(), artifact(), expected=True)
        reference, candidate = artifact(), artifact()
        for row in reference['split']:
            for side in ('left', 'right'):
                row[side] = [swappable.get(name, name) for name in row[side]]
        swapped = self.verdict(reference, candidate, expected=True)
        self.assertEqual(baseline['measurements']['split_mismatches'],
                         swapped['measurements']['split_mismatches'])
        # 而参考换成**不等价**的成员，判决必须改变——证明它不是「什么都放过」
        reference, candidate = artifact(), artifact()
        row = next(r for r in reference['split'] if r['left'] and r['right'])
        a, b = row['left'][0], row['right'][0]
        row['left'] = [b] + row['left'][1:]
        row['right'] = [a] + row['right'][1:]
        self.scientific_rejection(self.verdict(reference, candidate, expected=False))

    def test_widened_representation_gate_rejects_illegal_ambiguous_shapes(self):
        """放宽表示门要双向测：合法输入不被拒、非法输入不被放进来。

        本 check 的 schema 比姊妹 check 宽（接受 int 或 int 列表），而**放宽一个表示门**
        正是最容易顺手放过非法输入的地方。合法方向由 test_rubric_accepts_ambiguous_states
        覆盖；这一条打非法方向。
        """
        for label, bad in [('non_integer_member', [0, 'x']), ('empty', []),
                           ('nested', [[0], 4]), ('bool_member', [0, True]),
                           ('float_member', [0, 1.5]), ('duplicate_member', [4, 4])]:
            matrices = copy.deepcopy({'syn': SYN, 'ambiguous': AMBIGUOUS, 'near_miss': NEAR_MISS})
            matrices['ambiguous']['rows']['s1'][1] = bad
            with self.subTest(shape=label):
                self.contract_failure(self.verdict(artifact(), artifact(),
                                                   policy=rubric(matrices=matrices)))

    def test_split_swapping_two_non_equivalent_members_is_rejected(self):
        """把两侧各一个**不等价**成员对调必须拒绝——重复组身份不是「随便换个名字都行」。

        注意不能只做「替换」：那会让两侧交叠，走的是合同失败路径（内部自相矛盾），
        测不到科学判定。要测科学判定就得保持划分仍然是一个合法划分。
        """
        def change(document):
            row = next(r for r in document['split'] if r['left'] and r['right'])
            a, b = row['left'][0], row['right'][0]
            self.assertNotEqual(SYN['rows'][a], SYN['rows'][b], '这两个必须不等价')
            row['left'] = [b] + row['left'][1:]
            row['right'] = [a] + row['right'][1:]
        self.scientific_rejection(self.mutate(change))

    def test_topology_has_no_independent_leg_so_both_sides_wrong_passes(self):
        """**已知限制，写成断言而不是散文。**

        topology 只有两条腿（候选↔参考），**没有独立复算腿**——独立复现整条贪心递归
        要复现九层（unravel 频率 → 带权 argmax → 劈分 → assign_missing_average →
        建图 ×2 → 爬山 ×2 → 递归 → 边坍缩），每层都需单独的保真审计。
        本 leaf 已有两个先例说明一条错的第三条腿比没有更糟：maxcut 的 frequencies 复算
        独立写成却省略了 unravel；以及本 check 自己那个受限的行顺序扫描扫错了空间。

        **后果：本量对 producer bug 与共享依赖 bug 无防护。** 双侧同错会通过——
        这条测试就是断言那个事实，好让它不会被误以为已覆盖。
        """
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            triplets = document['topology'][0]['triplets']
            key = sorted(triplets)[0]
            triplets[key] = 'bc' if triplets[key] != 'bc' else 'ac'
        self.verdict(reference, candidate, expected=True)

    def test_split_moving_one_sample_across_is_rejected(self):
        def change(document):
            row = document['split'][0]
            if row['right']:
                row['left'] = row['left'] + [row['right'][0]]
                row['right'] = row['right'][1:]
            else:
                row['right'] = [row['left'][0]]
                row['left'] = row['left'][1:]
        self.scientific_rejection(self.mutate(change))

    def test_split_swapping_the_two_sides_is_rejected(self):
        """left 与 right 不是可互换的——left 是由被选 (character, state) 定义的那一侧。"""
        def change(document):
            row = document['split'][0]
            row['left'], row['right'] = row['right'], row['left']
        self.scientific_rejection(self.mutate(change))

    def test_split_dropping_a_sample_is_rejected(self):
        def change(document):
            document['split'][0]['left'] = document['split'][0]['left'][:-1]
        self.scientific_rejection(self.mutate(change))

    def test_split_duplicate_member_is_contract_failure(self):
        def change(document):
            row = document['split'][0]
            row['left'] = row['left'] + row['left'][:1]
        self.contract_failure(self.mutate(change))

    def test_split_unknown_sample_is_contract_failure(self):
        def change(document):
            document['split'][0]['left'] = document['split'][0]['left'] + ['nope']
        self.contract_failure(self.mutate(change))

    def test_split_missing_config_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d['split'].pop()))

    def test_empty_right_side_is_legal(self):
        """chosen_state == 0 与爬山退化都会给出空 right，是合法结局不是缺失。"""
        reference, candidate = artifact(), artifact()
        self.verdict(reference, candidate, expected=True)

    # --- 拓扑 -----------------------------------------------------------------

    def test_topology_wrong_triplet_is_rejected(self):
        def change(document):
            triplets = document['topology'][0]['triplets']
            key = sorted(triplets)[0]
            triplets[key] = 'bc' if triplets[key] != 'bc' else 'ac'
        self.scientific_rejection(self.mutate(change))

    def test_topology_collapsing_everything_to_unresolved_is_rejected(self):
        def change(document):
            for row in document['topology']:
                for key in row['triplets']:
                    row['triplets'][key] = '-'
        self.scientific_rejection(self.mutate(change))

    def test_topology_missing_triplet_is_contract_failure(self):
        def change(document):
            triplets = document['topology'][0]['triplets']
            triplets.pop(sorted(triplets)[0])
        self.contract_failure(self.mutate(change))

    def test_topology_unknown_triplet_key_is_contract_failure(self):
        def change(document):
            document['topology'][0]['triplets']['zz|yy|xx'] = 'ab'
        self.contract_failure(self.mutate(change))

    def test_topology_illegal_structure_label_is_contract_failure(self):
        def change(document):
            triplets = document['topology'][0]['triplets']
            triplets[sorted(triplets)[0]] = 'ba'
        self.contract_failure(self.mutate(change))

    # --- 异常行为 -------------------------------------------------------------

    def test_wrong_exception_type_is_rejected(self):
        """抛了但抛错类型——这正是「有没有抛」这种负向断言分辨不出的。"""
        def change(document):
            row = next(r for r in document['error_behaviour']
                       if r['probe'] == 'ambiguous_raises')
            row['raised'] = 'builtins.KeyError'
        self.scientific_rejection(self.mutate(change))

    def test_not_raising_on_the_ambiguous_probe_is_rejected(self):
        def change(document):
            row = next(r for r in document['error_behaviour']
                       if r['probe'] == 'ambiguous_raises')
            row['raised'] = None
        self.scientific_rejection(self.mutate(change))

    def test_raising_on_the_near_miss_control_is_rejected(self):
        """阴性对照与触发输入**只差一处**（那个 ambiguous 单元格换成普通整数）。

        差得越远排除的假设越少：任意正常矩阵只能排除「永远抛」，
        只差一处才能排除「抛的条件过宽」。
        """
        def change(document):
            row = next(r for r in document['error_behaviour']
                       if r['probe'] == 'near_miss_does_not_raise')
            row['raised'] = 'cassiopeia.mixins.errors.GreedySolverError'
        self.scientific_rejection(self.mutate(change))

    def test_missing_error_probe_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d['error_behaviour'].pop()))

    # --- ambiguous state 的 schema（与 maxcut 故意不同）------------------------

    def test_rubric_accepts_ambiguous_states(self):
        """本 check 的 schema **必须**能表达 ambiguous state，否则覆盖不了官方那条 test。"""
        self.verdict(artifact(), artifact(), expected=True)
        self.assertIsInstance(AMBIGUOUS['rows']['s1'][1], list)

    def test_ambiguous_state_with_a_non_integer_member_is_contract_failure(self):
        matrices = copy.deepcopy({'syn': SYN, 'ambiguous': AMBIGUOUS, 'near_miss': NEAR_MISS})
        matrices['ambiguous']['rows']['s1'][1] = [0, 'x']
        self.contract_failure(self.verdict(artifact(), artifact(),
                                           policy=rubric(matrices=matrices)))

    def test_empty_ambiguous_state_is_contract_failure(self):
        matrices = copy.deepcopy({'syn': SYN, 'ambiguous': AMBIGUOUS, 'near_miss': NEAR_MISS})
        matrices['ambiguous']['rows']['s1'][1] = []
        self.contract_failure(self.verdict(artifact(), artifact(),
                                           policy=rubric(matrices=matrices)))

    # --- rubric 与产物合同 ----------------------------------------------------

    def test_nonzero_atol_is_refused(self):
        """本 check 的 graded 量全是离散的，容差没有意义。"""
        self.contract_failure(self.verdict(artifact(), artifact(), policy=rubric(atol=1e-9)))

    def test_unknown_prior_transformation_is_refused(self):
        self.contract_failure(self.verdict(artifact(), artifact(),
                                           policy=rubric(prior_transformation='inverse')))

    def test_missing_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), None))

    def test_missing_section_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d.pop('topology')))

    def test_extra_section_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d.update(extra=[])))

    def test_wrong_schema_version_is_contract_failure(self):
        self.contract_failure(self.mutate(lambda d: d.update(schema_version=2)))

    def test_duplicate_json_key_is_contract_failure(self):
        raw = json.dumps(artifact()).encode('utf-8')
        broken = raw.replace(b'"schema_version": 1', b'"schema_version": 1, "schema_version": 1', 1)
        self.contract_failure(self.verdict(artifact(), broken))

    def test_oversize_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), b' ' * 5_000_000 + b'{}'))

    def test_non_utf8_artifact_is_contract_failure(self):
        self.contract_failure(self.verdict(artifact(), b'{"schema_version": 1, "s": "\xff\xfe"}'))

    def test_row_order_is_not_graded(self):
        reference, candidate = artifact(), artifact()
        for key in ('split', 'topology', 'error_behaviour'):
            candidate[key].reverse()
        self.verdict(reference, candidate, expected=True)

    def test_both_sides_wrong_on_the_split_is_still_rejected(self):
        """三向比较的关键一格——**只对 partition 成立**，topology 没有第三条腿。"""
        reference, candidate = artifact(), artifact()
        for document in (reference, candidate):
            row = document['split'][0]
            moved = row['left'][-1]
            row['left'] = row['left'][:-1]
            row['right'] = row['right'] + [moved]
        self.scientific_rejection(self.verdict(reference, candidate, expected=False))

    def test_reference_side_is_graded_too(self):
        def change(document):
            document['split'][0]['left'] = document['split'][0]['left'][:-1]
        self.scientific_rejection(self.mutate(change, side='reference'))


if __name__ == '__main__':
    unittest.main()
