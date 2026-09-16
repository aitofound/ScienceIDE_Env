# umi-error-correction

此 check 只检查已经带有 alignment/allele 注释的 molecule table 的 **UMI barcode 纠错阶段**。调用真实 `pipeline.error_correct_umis`，不包含 `resolve_umi_sequence`，也不来自 `collapse_umi_test.py`。完整分区、代表资格、allele/cut-site 状态与整数 read support 构成 **provisional pointwise** 合同；不以每个 cell 的总 reads 代替逐簇检查。

## 官方来源与四个场景

来源为 `code/cassiopeia/test/preprocess_tests/error_correct_umi_test.py::TestErrorCorrectUMISequence`。`ic/nominal/inputs.json` 完整物化原始 `multi_case`、`ambiguous` 两个 fixture，包括原来的 `readName` 与 alignment 字段；没有修改 UMI、支持数或 allele 来迎合判据。

| 官方方法 | 生产调用行号 | 输入 fixture | 参数 | 输出文件 |
|---|---|---|---|---|
| `test_format` | 139–141 | multi_case，11 行 | distance=1；默认 cellBC/intBC 分组 | `format-distance-one.npz` |
| `test_zero_dist` | 161–163 | multi_case，11 行 | distance=0 | `zero-distance.npz` |
| `test_error_correct_two_dist` | 172–174 | multi_case，11 行 | distance=2；默认 cellBC/intBC 分组 | `two-distance.npz` |
| `test_error_correct_allow_conflicts` | 198–203 | ambiguous，5 行 | distance=2，`allow_allele_conflicts=True`，2 workers | `allele-separated.npz` |

全部 nodeid、参数与 source anchor 保存在 IC 的 `scenarios` 中。`test_format` 原本只断言列存在性；本 check 保留它实际执行的 distance=1 数值阶段，但不声称评分其完整 DataFrame 列接口。其他方法通过 `readName` 定位行的字符串方式不评分，改用下面的科学身份。

实际代码在 `allow_allele_conflicts=True` 时**额外按 allele 分组**；它并非允许跨 allele 合并。这里严格保持源实现的含义。

## 为什么当前 fixture 可以消除任意 tie-break

首先按场景的 cellBC/intBC/可选 allele 分组。在每个组内，以原始 UMI 为顶点；当整数 Hamming distance **小于或等于**该场景阈值时连边。

对这四个固定官方场景，已逐一验证：

1. 每个连通 component 都是 clique，即其中任意两个 UMI 都直接相邻；不同 component 没有边。
2. 每个 clique 内的 `allele`、`r1`、`r2`、`r3` 一致。
3. 原始 `(cellBC,intBC,UMI)` 身份唯一，每组 UMI 等长，readCount 为正整数。

这使**完整成员 UMI 集合**成为唯一、与代表选择无关的分簇身份。一个输出行声明的是该代表所属输入 clique 的**全部成员**，不是任意未知 subset；因此身份可以从固定输入和该代表唯一解码。validator 要求每个 clique 恰好一行，不能漏簇、重复一个 clique 的两个 tie 代表，或只输出 cell 总支持数。

源实现 `pipeline.py:756–758` 先按原始 readCount 降序排序；`collapse_cython.pyx:39–60` 对后续 UMI 扫描所有先前邻居并传播 correction。在 clique 内，每个非首成员都与首成员直接相邻，因此最终代表是某个**最大原始 readCount**的 UMI。多个成员并列最大时，可以任取其中一个；不能以已聚合的输出 readCount 反过来为低丰度代表辩护。

validator 将代表规范化为 `(group, sorted complete member UMIs)`，再检查全部关联 allele 与 read support。即使合法 tie 替换改变了代表的字典序或输出表顺序，科学身份仍不变。producer 本身不计算 clique、不实现 correction mapping 或答案，只导出真实生产表。

**这不是一般 UMI 纠错算法结论。** validator 会重验 IC 的上述前提；遇到非 clique 或 clique 内冲突 allele，明确报不适用，而不退化为比较总和。不能把本合同直接套到任意 UMI 网络。

## 科学字段与未评分字段

| 字段 | 本阶段作用 |
|---|---|
| `cellBC`、`intBC` | 必须准确保持的分组身份；同一 UMI 在不同组是不同 molecule |
| `UMI` | 输出代表；必须来自该 clique 的原始最大支持代表集合 |
| `allele`、`r1`、`r2`、`r3` | 精确的 called allele/cut-site 状态；同时约束可选 allele 分组及输出的生物学关联 |
| `readCount` | clique 内所有原始 molecule 的完整整数支持和，必须精确守恒 |
| `readName` | 不评分。由其他字段派生，原 fixture 已有跨 intBC 重复，不能当唯一身份 |
| `Seq` | 不评分。它是有生物学意义的上游 read/alignment 载荷，但此纠错阶段不重算；合法 tie 可以携带不同 Seq |
| `CIGAR`、`AlignmentScore` | 不评分。它们是此前 alignment 阶段的结果，本 check 不检查其重算或逐字段保留接口 |

`Seq`/alignment 字段的排除不表示它们在整个 pipeline 中没有科学意义，也不表示完整 API 已验收。UMI 纠错使用的是 UMI 序列，不是 `Seq` 的距离。原始重复输入触发的 `PreprocessError`、任意非 clique 数据、sequence resolution 与 alignment/collapse 算法均不在本 check 的覆盖声明中。

## 输出合同

四个文件均是标准 NumPy NPZ，可使用 `np.savez` 或 `np.savez_compressed`，不使用 pickle。每个文件必须且只能包含以下三个数组；`M` 是该场景输入所定义的完整 clique 数，不是任意截取的行数。

| 数组 | dtype | shape | 列定义 |
|---|---|---|---|
| `molecules` | Unicode，任意足够的字符串宽度 | `(M,3)` | cellBC、intBC、UMI representative |
| `alleles` | Unicode | `(M,4)` | allele、r1、r2、r3 |
| `read_count` | signed int64，任一 byte order | `(M,)` | 该 clique 的完整 read support |

允许任意行重排，但三个数组必须同步移动。允许合法最高支持 tie 代表替换；其分簇身份和所有科学 payload 必须仍正确。不能把二维数组 flatten 后冒充规定 shape，也不能以 float、bool、unsigned/object 数组冒充 signed int64 支持数。不存在/低丰度代表、cross-group、错误 allele、缺失/额外/重复 molecule 或 clique 均失败。

`validate.py` 对两侧输出同样检查。支持数使用 Python 整数求差，避免整数溢出与浮点舍入；`atol=rtol=0`。通过时 `distance=0`、`bound_fraction=0`；一个 read 的差异会失败，并以 JSON `null` 表示零 bound 下无界的 fraction，不进行 `0/0`。身份/shape 无法建立时 distance 也可为 `null`，不能把这种失败解释成零误差。损坏 deflate、CRC、截断、加密或缺失 NPZ 会写失败 JSON，不以未捕获异常代替 verdict；CLI exit 0 只表示结果已写出，实际结果由 `passed` 字段决定。

## IC、阈值盲区与编译校准

`ic/variant/inputs.json` 与 nominal 逐字节相同。有效纠错输入是离散 barcode、UMI、called states、整数支持数及阈值，没有可以合法施加两 ULP 噪声的浮点输入，故此 variant **不提供 numerical-noise 证据**。

当前 multi_case 在阈值 1 与 2 下得到相同科学分区，不能检出只把 2 误作 1、却产生相同输出的实现。阈值 0 或 3 则会改变分区；本地故障探针验证了这些可观察差异。不修改原 fixture 来伪造缺失的 threshold=2 覆盖。

`run.sh altbuild` 保留现有 CFLAGS；未设置时取 Python 默认 CFLAGS，然后在末尾追加 `-O0`，重新编译同一 pinned source 的核心 `collapse_cython`。真实原生 compiler trace 已确认最后优化选项从 `-O2` 切换为 `-O0`，核心 `.so` hash 不同，且四个完整场景通过。它不是 Numba 开关，也没有换 source。其他扩展仍遵循 `build.py` 自己的额外 flags，例如 C++17 扩展的配置。

最初本地探针曾把 CFLAGS 显式设为空，使 nominal 实际已经是 gcc 默认 O0，与 `-O0` binary 完全相同；该尝试被识别为 **no-op** 并保留原日志，没有作为 altbuild 证据。最终测量已修复这个问题。

## 运行与构建

```bash
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh nominal
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh variant
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh altbuild
bash run.sh --help
python selftest_validate.py
```

`SAB_THREADS=2` 只控制官方 allow-conflicts 场景的 worker 数，其余场景仍为单 worker；`SAB_PYTHON=python3` 选择预装依赖与构建工具的解释器。candidate 不必使用相同线程或语言实现。没有人为增加样本、重复相同计算或加 acceleration 标签。

脚本在 strict mode 下复制只读 SOURCE_DIR 到临时目录，执行真实离线 wheel 构建，再用 `pip install --target` 安装到独立工作目录；不假设 Cassiopeia 已预装，不修改 source/宿主 venv，也不依赖其他 check 的代码或缓存。构建调用 pinned `build.py` 的三个扩展，包括 C++17 扩展。所有缓存及临时文件在结束时清理，不缓存 graded 输出；跨 check build reuse 留待 suite 集成。

`SAB_BUILD_SECONDS` 报实际复制、编译与安装时间。正式入口没有固定调查 timeout；本地取证 runner 从外部施加 180 秒限制。gcc/g++ 的参数 trace wrappers 与本地动态链接配置只存在于 state 取证目录，不写入 repo。

## 已执行证据与局限

2026-09-10 的 Linux x86_64 原生环境为 CPython 3.12、NumPy 1.26.4、pandas 2.3.3、Cython 3.3.0，实际使用系统 gcc/g++；没有 Docker 或 GPU。

| 入口 | 构建秒数 | 非构建 wall 秒数 | 结果 |
|---|---:|---:|---|
| nominal | 18.764932 | 6.039114 | 四场景完整运行 |
| variant | 18.763991 | 5.837828 | 29 个逐簇支持值通过，distance=0 |
| altbuild | 13.962856 | 6.229191 | 真实 O0 核心通过，distance=0 |

variant 与 altbuild 各四个文件均逐字节相同。编译二进制确实不同，但科学结果仍完全一致；它证明了本次离散合同下的等价，不能宣称获得浮点 noise 尺度或 GPU 结果。

- 完整官方文件原生 **4 passed，4.38 秒**；仅说明未修改的官方测试在该环境通过。
- 三轮实际 RED→GREEN；最终 **15 个方法、87 次 CLI 比较探针通过**。自测使用临时合成 fixture，不把它们加入 benchmark IC；其中合法 tie 会改变代表的字典序相对位置，避免仅按代表排序产生的假通过。
- 全部四个真实输出的三数组同步重排通过；四个合法 tie 代表替换通过。低丰度/不存在代表、错 support/allele、cross-intBC、缺失/额外/重复 molecule 全被拒绝。
- 另在隔离进程中仅反转生产排序的并列 UMI 字典序，四次真实 pipeline 调用产生不同代表身份，科学比较仍通过；没有在 adapter 内计算替代答案。
- 真实生产故障探针分别改为低丰度优先、阈值 0、阈值 3、忽略 allele 分组。这四种错误**都保持每个 cell 的总 reads**，仍被本 check 拒绝，证明判据没有弱化为总和。
- 将真实生产表支持数增加一个 read 被拒绝，最大整数误差为 1。

已规避 `phantom-particle-reordering`、`assertion-recorder-grades-candidate-internals` 与 `ungraded-sidecars-mask-identical-graded-output` 类问题；计时、compiler hash、进度及 readName 不进入科学 grade。

所有 tie 等价规则及 exact support bound 仍为 **provisional**，等待最终人工 review。CLI `floor` 与 `self_validation_*` 保持 `null`。完整 suite、正式 Docker/selfcheck、GPU 和全 API 覆盖均未完成，不以本 check 的原生通过替代它们。
