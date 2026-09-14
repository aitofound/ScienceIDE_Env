# dissimilarity-functions

此 check 检查独立距离/相似性函数、prior transformation 与 PHYLIP 数值导出三个阶段，采用 **provisional pointwise** policy。它不是单一 kernel，也不是整个 lineage-reconstruction suite 的验收结果。

## 官方来源与逐项边界

来源是 `code/cassiopeia/test/solver_tests/dissimilarity_functions_test.py::TestDissimilarityFunctions`。`ic/nominal/inputs.json` 的 `official_methods` 完整列出全部 **27 个方法**的 nodeid、源码行号、科学 observation 与未评分行为；`scalar_cases` 另外绑定每次实际生产调用。共 **26 个方法提供数值**，其中两个方法各调用两次，最终形成 69 个科学值。

| 官方方法组 | 方法数 | 本 check 的科学阶段 |
|---|---:|---|
| `test_negative_log_prior_transformations`、`test_inverse_prior_transformations`、`test_sq_inverse_prior_transformations` | 3 | 13 个 `(character,state)` 的三种变换，共 39 个 weights |
| `test_weighted_hamming_distance_*` | 6 | identical、无 priors、三种 prior transformation、all-missing |
| `test_hamming_similarity_without_missing_*` | 4 | identical、无 priors、有 priors、all-missing |
| `test_hamming_similarity_normalized_*` | 4 | identical、无 priors、有 priors、all-missing |
| `test_weighted_hamming_similarity_*` | 4 | identical、无 priors、有 priors、all-missing |
| `test_cluster_dissimilarity` | 1 | weighted Hamming + mean linkage，默认 `normalize=True` |
| `test_cluster_dissimilarity_weighted_hamming_distance_min_linkage` | 1 | ambiguous_no_missing 与 ambiguous 两次 min-linkage 调用，均无 weights |
| `test_hamming_distance`、`test_hamming_distance_ignore_missing` | 2 | 默认包含 missing 差异，以及 ignore-missing 的普通/all-missing 两次调用 |
| `test_save_dissimilarity_as_phylip` | 1 | 实际写出三 cell 下三角，含 diagonal，共 6 个 pair 距离 |
| `test_bad_prior_transformations` | 1 | **不评分**：只验证错误输入抛出异常，没有科学数值输出 |

数值方法保留全部实际科学调用。`test_bad_prior_transformations` 的 invalid priors 仍记录在 IC 与 inventory，但 producer 不把异常成功标志转换成分数。PHYLIP 方法的 mock `open` 路径/模式、write 调用次数与特定文本行序不评分；这里调用真实 writer 并解析数值与 cell 身份。没有全局 assertion wrapper、`numpy.testing` hook 或 success-bit 输出。

不声称覆盖这份官方文件未调用的 `exponential_negative_hamming_distance`、加权 min-linkage、其他 linkage/非法 transformation 名称等分支，也不把独立函数调用误称为距离矩阵构建、树求解或完整方法接口覆盖。

## 输入与源函数语义

固定输入直接来自官方 `setUp:16–44`：五个长度为 6 的序列（含 all-missing 和两种 ambiguous 序列）、13 个有效 state priors，以及 PHYLIP 方法中的 3×3 输入矩阵。运行时不调用随机树或 sampler，不要求 candidate 复刻随机流。ambiguous JSON 内层数组重建为 tuple；tuple 元素及其重数是输入，不可当作无序集合去重。

- `solver_utilities.py:53–103`：对每个 prior 分别计算 `-log(p)`、`1/p`、`sqrt(1/p)`；检查的定义域是每个 `p` 在 `(0,1]`，不自动归一化整组 priors。
- `dissimilarity_functions.py:12–72`：weighted Hamming 跳过任一端 missing 的位置，不同 indel 与 uncut/indel 使用不同代价，再除以两端都 present 的字符数；没有 present 位点时返回 0。
- `:75–157`：相似性只在相同非零、非 missing state 上累加；normalized 版本的分母包含所有共同 present 位点，而不是只有相同 indel 的位点。
- `:197–238`：weighted similarity 对相同非零 state 加双倍权重；相同 uncut state 仅在无 weights 时加 1，再按 present 数归一化。
- `:160–194`：普通 Hamming 在 `ignore_missing_state=False` 时仍统计涉及 `-1` 的差异；不能把所有函数都改成忽略 missing。
- `:370–399`、`:447–496`：ambiguous 状态逐字符枚举组合，再执行 mean/min linkage，并以两端非 missing 的比例作为有效位点贡献。mean 与 min 不是可交换的实现选择。
- `solver_utilities.py:139–146`：PHYLIP writer 输出包含 diagonal 的下三角，按四位小数格式化。

producer 将真正的 `transform_priors` 返回值继续传给使用 weights 的函数；同时单独导出每个变换值，不能因下游未使用某一 state 而漏检该项变换。

## 输出合同

### 四个 NPZ 文件

每个 NPZ 必须且只能包含 `keys` 与 `values`；不使用 pickle，允许 compressed/uncompressed NPZ 与任一 byte order。

| 文件 | `keys` dtype / shape | `values` dtype / shape | 科学身份 |
|---|---|---|---|
| `negative_log.npz` | signed int64，`(13,2)` | float64，`(13,)` | `(character,state)` |
| `inverse.npz` | signed int64，`(13,2)` | float64，`(13,)` | `(character,state)` |
| `square_root_inverse.npz` | signed int64，`(13,2)` | float64，`(13,)` | `(character,state)` |
| `scores.npz` | Unicode，`(24,3)` | float64，`(24,)` | `(case_id, sequence_1_id, sequence_2_id)` |

所有固定 keys 见 IC 和 rubric。行可以任意重排，但 `values` 必须同步移动。`(character,state)` 两列不可互换：它们是不同物理角色，不是无序 pair。此处测试的序列距离/相似性函数是对称的，因此 scores 的两个 sequence IDs 可以互换，`case_id` 必须保持对应；它绑定固定测试输入与函数语义，不是候选内部的断言/执行序号。

不能用 object、float32、整数 values 或 reshape 后的数组替代规定类型/科学 shape。双方输出都必须有限、非空，身份集合完整且无重复、额外或缺失记录。

### `distances.phy`

UTF-8 文本第一行只有 cell 数 `3`；之后恰好三行，每行先写一个 cell ID，第 `i` 行（从 0 开始）跟随 `i+1` 个数值，表示对本行以及之前各行 cell 的距离。所有 cell IDs 必须恰好各出现一次。输出数值必须可解析为有限实数；source writer 打印四位小数，但科学比较不检查空白风格或数字拼写。

PHYLIP 行顺序可以变化，相关下三角数值必须同步重排。validator 从文件携带的 cell IDs 恢复无序 cell-pair 身份再比较全部六项，不按行号直接比较。完整矩阵、缺项下三角、额外行、重复 cell 或 NaN/Inf 均失败。

## 两种 IC 与 alternative build

`ic/variant/inputs.json` 仅将全部 13 个有效 prior 各向零移动 binary64 **两 ULP**，其他字段逐项相同，仍满足源函数的有效域。这是输入浮点舍入噪声，不是改变状态编码、重新采样或改变归一化模型。

实测 69 个科学值中有 36 个变化：negative-log 13/13、inverse 13/13、square-root-inverse 5/13、scores 5/24。sqrt 舍入及部分短和会消去扰动；纯离散、all-missing 提前返回与未扰动的 PHYLIP 阶段不能据此声称获得非零 noise 证据。

`run.sh altbuild` 对同一源码重新构建，在 nominal 输入上设置 `NUMBA_DISABLE_JIT=1`；常规入口固定为 `0`。完整路径已实测可运行。**只有 `hamming_distance` 从 JIT 切换为 Python，其余浮点路径不变**；所有 69 值及五个文件均相同，因此这不是 log/sqrt、PHYLIP 或 GPU 的独立精度 floor 测量。

## 运行与隔离构建

```bash
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh nominal
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh variant
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh altbuild
bash run.sh --help
python selftest_validate.py
```

只有 `SAB_PYTHON=python3` 解释器选择。官方六字符场景没有可缩减的科学分辨率/时间窗，也没有有用的并行参数；不通过重复计算同一结果、添加虚假规模或删减方法制造可调负载。candidate 不必使用 Python、Numba 或同一实现方式。

脚本使用 strict mode，将 `SOURCE_DIR` 复制进临时目录，离线执行 wheel 构建和 `pip install --target` 隔离安装；镜像只需预装依赖与编译器，不要求已安装 Cassiopeia。构建执行 pinned `build.py` 的三个扩展，包括 C++17 扩展。没有网络依赖下载、不修改 source/宿主 venv、不依赖其他 check 的模块或缓存。当前每次独立构建，不缓存 graded 输出；跨 check build reuse 留待 suite 集成。

`SAB_BUILD_SECONDS` 报实际复制、wheel 构建及独立安装时间。正式入口没有固定调查 timeout；原生取证的 180 秒限制由 check 外的本地 runner 施加。临时构建、安装与缓存随运行清理。

## Provisional pass policy

四个 NPZ 的 absolute bound 为 `1e-5`、`rtol=0`；PHYLIP 单独使用 `5e-5`、`rtol=0`，对应四位小数打印的半单位舍入。短和、log/sqrt 与不同合法浮点中间精度需要余量；missing 分母错误、丢弃有效 weights 或把 mean 改成 min 的实际故障均明显超出这些提案 bound。所有数字仍待人工校准确认，不能把 bound 机械压到两 ULP 的微小 spread。

validator 用同一 identity permutation 对每行全部数值重排，并对两侧同时检查 dtype、shape、完整性和有限值。`distance` 为最大绝对误差，`bound_fraction` 为误差占对应文件 bound 的最大比例；exact 零 bound 且误差零时比例为 0，有非零误差时失败并用 JSON `null` 表示无界比例，不写 NaN 或执行 `0/0`。

## 已执行的原生证据

2026-09-10，Linux x86_64、CPython 3.12、NumPy 1.26.4、pandas 2.3.3、Numba 0.67.0、Cython 3.3.0、system gcc/g++；本地取证固定线程与动态链接设置，没有 Docker 或 GPU。

| 入口 | 构建秒数 | 非构建 wall 秒数 | 结果 |
|---|---:|---:|---|
| nominal | 17.820894 | 3.625743 | 全部三类科学阶段完成 |
| variant | 17.797624 | 3.547363 | 69 值通过，最大差 `3.552713678800501e-15` |
| altbuild | 17.655524 | 3.184651 | 69 值通过，最大差 0 |

- 完整官方文件原生 **27 passed，2.53 秒**；这包含其异常/mock 断言的官方测试佐证，但不等于本 check 对这些接口行为评分。
- validator 五轮实际 RED→GREEN；最终 **11 方法、96 次 CLI 比较探针通过**。
- 全部五个真实输出同步重排身份与 payload 后通过；不移动关联 payload 的错配样本有 60 个值被拒；全零科学模型也有 60 个值被拒。
- 在生产 weighted-Hamming 函数加入私有无害 assertion，实际调用 14 次，五个输出文件与科学 schema 均不变；没有把该 assertion 采集进结果。
- 真实 API 故障探针：忽略 weights 被拒，最大差约 `4.0353`；错误归一化分母被拒，最大差约 `0.1`；mean 改成 min 被拒，最大差约 `0.0770164`。这些探针只在本地隔离进程替换调用，未改 vendored source。

避免的已知问题包括 `assertion-recorder-grades-candidate-internals`、`phantom-particle-reordering`、`mink-candidate-sampler-sets-the-inputs` 与 `ungraded-sidecars-mask-identical-graded-output`。计时及探针调用次数只进入 stdout，不作为科学输出或评分维度。

CLI `floor`、`self_validation_spread`、`self_validation_bound_fraction` 保持 `null`。完整 suite、正式 Docker/selfcheck、GPU、跨 check build reuse 与最终人工容差确认尚未完成；不以本 check 的原生通过替代它们。
