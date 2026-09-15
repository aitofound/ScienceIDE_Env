# bayesian-branch-length

本 check 研究固定 CRISPR 字符状态和细胞谱系拓扑下的分裂时间后验。生产入口是 `IIDExponentialBayesian.estimate_branch_lengths`，通过 Cython 调用 C++17 动态规划，估计归一化实验时间 `[0,1]` 上的节点时间分布和分支长度。策略为暂定 `pointwise`，还未由人工校准定稿。

## 官方来源与实际覆盖

官方文件为 `code/cassiopeia/test/tools_tests/branch_length_estimator_tests/iid_exponential_bayesian_test.py`，类为 `TestIIDExponentialBayesian`。它有 **5 个源码 test 定义，参数化后 10 个 collected items**。完整 selector 是以下共同前缀加上表内方法名。

```text
test/tools_tests/branch_length_estimator_tests/iid_exponential_bayesian_test.py::TestIIDExponentialBayesian::
```

| 方法名 | 本 check 的科学场景 / 范围 |
|---|---|
| `test_against_closed_form_solution_small_0_1` | `small_0_1`，单字符，T=200 |
| `test_against_closed_form_solution_small_1_2` | `small_1_2`，9 字符，T=200 |
| `test_against_closed_form_solution_small_2_3` | `small_2_3`，单字符，T=200 |
| `test_against_closed_form_solution_small_3_4` | `small_3_4`，9 字符，T=200 |
| `test_against_closed_form_solution_small_4_5` | `small_4_5`，单字符，T=200 |
| `test_against_closed_form_solution_small_5_6` | `small_5_6`，9 字符，T=200 |
| `test_against_closed_form_solution_medium` | `medium`，2 个非根内部节点，T=100；官方 pytest 需要 `--runslow` |
| `test_small_discretization_level_raises_error` | 未作为评分阶段覆盖 |
| `test_invalid_tree_topology_raises_error` | 未作为评分阶段覆盖；定义内有两种错误拓扑 |
| `test_invalid_sampling_probability_raises_error` | 未作为评分阶段覆盖；定义内有两个非法概率 |

`ic/nominal/inputs.json` 固定上述七个科学场景的全部拓扑、输入节点身份、字符状态、突变率、出生率和采样概率，均取自该官方文件，不使用 candidate 的随机生成器决定问题。`produce.py` 直接调用生产接口，而不是导入测试、替换断言或记录 pass bits。七个场景保留官方完整时间网格，不执行 SciPy 连续多重积分，不比较上游测试辅助函数输出，也不比较 `log_joints` API 的未归一化数组。因此它是该文件的科学阶段适配，**不是原封不动执行全部 10 项，也不是全部行为覆盖**。

## 运行与构建

```bash
SOURCE_DIR=/path/to/pinned/source CHECK_DIR="$PWD" OUT_DIR=/path/to/empty/output bash run.sh nominal
bash run.sh --help
```

- `SAB_GRID_MULTIPLIER=1`：默认保留官方 T=200/100。正整数倍数扩大离散网格，动态规划工作量近似随 T 线性增长；非默认只用于研究，默认 validator 仍要求官方网格形状。
- `SAB_PYTHON=python3`：已预装依赖和构建工具的解释器。编译器遵循环境 `CC`、`CXX`。
- `run.sh` 不包含研究阶段的 180 秒超时，也不重复同一计算制造工作量。原生研究使用外部 timeout，构建时间单独记录。
- 脚本离线复制只读源码到独立 `WORK`，通过 `pip wheel --no-deps --no-build-isolation --no-index` 及 `pip install --target` 构建、隔离加载。不向预装环境安装包，不修改源码。
- 上游 `build.py` 编译 `collapse_cython`、`ilp_solver_utilities` 和 Bayesian 三个扩展，Bayesian 使用 `-std=c++17 -O3`。当前 check 不共享兄弟目录或预编译安装；每次独立构建，`SAB_BUILD_SECONDS` 报告实际复制、构建及隔离安装耗时。没有跨 check 缓存复用。
- `altbuild` 用相同源码、相同 nominal 输入重新编译全部扩展；临时 CC/CXX wrapper 在实际命令末尾追加 `-O0` 覆盖上游 `-O3`，保留 C++17。不使用 `NUMBA_DISABLE_JIT` 冒充 C++ 不同构建。

正式输出合同不要求 candidate 使用本编译方式、线程数、内部节点名、遍历顺序或缓存布局。原生取证固定 BLAS、OpenMP、Numba 各 1 线程，属于测量环境，不是 candidate 的实现限制。

## 初始条件与数值扰动

每个场景只改变一个活跃 binary64 参数，连续 `nextafter` 两次，其余输入完全不变。

| 场景 | 改变的参数 | 方向 |
|---|---|---|
| `small_0_1` | `sampling_probability` | 从 1 向负无穷移动两次，留在合法概率域内 |
| `small_1_2` | `mutation_rate` | 向正无穷移动两次 |
| `small_2_3`、`small_3_4`、`small_4_5`、`small_5_6`、`medium` | `birth_rate` | 向正无穷移动两次 |

采样概率从 1 向域内扰动会进入 C++ 的未采样概率递推分支，但对应连续模型的变化只有两 ULP。拓扑、字符状态、离散网格都不变，不通过改 seed、改数组顺序或加入时间戳制造差异。初版只扰动 mutation_rate，在四个场景被浮点舍入吞没；选择上述每场景最小单参数扰动后，原生探针中七个场景的 posterior、times 和 branch_lengths 均有改变。部分 log_likelihood 仍可逐位相同，必须如实记录，不能要求每个标量都移动。

## 输出合同

输出恰好七个科学 NPZ 文件，名称为 `small_0_1.npz`、`small_1_2.npz`、`small_2_3.npz`、`small_3_4.npz`、`small_4_5.npz`、`small_5_6.npz`、`medium.npz`。每个 NPZ 恰好包含以下字段，不允许 pickle/object arrays、缺失字段或额外字段。

| 字段 | dtype / shape | 含义 |
|---|---|---|
| `node_ids` | Unicode `(N,)` | 全部固定输入节点的身份 |
| `times` | binary64 `(N,)` | 对应节点估计时间；包含根及全部叶子 |
| `posterior_ids` | Unicode `(M,)` | 非根内部节点的固定输入身份 |
| `posterior` | binary64 `(M,T+1)` | 对应节点在每个离散时间点的概率质量，而非连续概率密度 |
| `grid` | binary64 `(T+1,)` | 按 `np.arange(T+1, dtype=np.float64)/T` 表示的物理坐标；不是计算迭代次数 |
| `edges` | Unicode `(E,2)` | 固定输入有向边的父、子身份 |
| `branch_lengths` | binary64 `(E,)` | 对应生产分支长度 |
| `log_likelihood` | binary64 `(1,)` | 模型对观测字符及拓扑给出的 log probability |

small 场景 `N=4,M=1,E=3,T=200`；medium 场景 `N=6,M=2,E=5,T=100`。具体身份和有向边来自输入文件。根的 out-degree 为 1，根和它的唯一子节点即使具有相同 descendant leafset，仍是两个时间含义不同的输入节点；不能按 leafset 合并，也不能删除根边。根和叶子没有生产 posterior API 输出，不人为构造 delta posterior。

节点行、posterior 行以及边行都可独立重排，只需各自身份与全部对应 payload 同步移动。validator 根据输入身份对齐所有数据后再比较，不把原始存储位置或内部生成的名字当作科学身份。时间网格列为固定物理时间，不能任意重排。输出端和 validator 都先检查 dtype，再处理数值，不能把 float32 静默转成 float64 伪装符合精度合同。

## 暂定等价策略

`times`、`posterior`、`branch_lengths`、`log_likelihood` 每个值满足 `abs(candidate-reference) <= 1e-8`，`rtol=0`。输入身份和有向拓扑必须完整、唯一、严格匹配。双方都必须是有限 binary64；概率质量在 `[0,1]`，每行归一化 absolute bound 为 `1e-10`；非根内部节点时间须与自身 posterior 的网格加权均值一致，边长须与对应子父时间差一致，这两项 absolute bound 为 `1e-8`。根时间为 0，叶时间为 1，所有实际边长严格为正。

依据生产源码 `_iid_exponential_bayesian_cpp.cpp:88-103,124-273,349-379`，up/down 动态规划使用 double 精度的 logsumexp、exp、log，以及后验归一化和加权求和；`IIDExponentialBayesian.py:105-130` 先 impute 可推断的缺失状态，再把后验均值写回原树。此处没有采样噪声，但合法实现可以改变浮点归约次序。暂定 `1e-8` 留出重排和跨实现余量，仍远低于 0.005/0.01 的离散时间间隔，不采用上游与连续积分比较的 1% 离散化误差作移植容差。错误系数、概率质量移错节点和遗失有时间意义的根边都应被拒绝。

## 取证与限制

原生环境为 Linux x86_64、Python 3.12、系统 GCC/G++，科学进程由外部 `timeout 180s` 限时，构建另计。官方 10 selectors 已逐项完整执行且 exit 0；medium 9.677 秒，其余每项 2.868–3.219 秒（含 Python/pytest 启动）。medium 官方积分报告慢收敛及 log(0) 警告，测试仍通过；不将这些测试辅助函数中的非有限值作为本 check 的输出。

最终原生测量使用 Python 3.12.9、NumPy 1.26.4、SciPy 1.13.1、GCC/G++ 11.5.0。三个模式的实际时间如下，nonbuild 为总 wall 减去脚本报告的构建时间，包含 Python 导入及驱动开销。

| 模式 | exit | 总 wall 秒 | build 秒 | nonbuild 秒 |
|---|---:|---:|---:|---:|
| nominal | 0 | 21.097171 | 17.696221 | 3.400950 |
| variant | 0 | 21.043837 | 17.713526 | 3.330310 |
| altbuild | 0 | 14.889731 | 11.424461 | 3.465270 |

最终 variant 的七个 NPZ 都有评分值及字节变化，最大 absolute 差异 `4.440892098500626e-15`，最大 bound fraction 为 `4.440892098500626e-7`。`small_0_1` 和 `small_3_4` 的 log_likelihood 没有变化，其余科学字段组均有变化。另一个独立 scratch 输入代理探针把 mutation_rate 减半，保持源码不变，模拟参数系数错误；七个场景都被拒绝，最大 absolute 差异 `2.394892062188016`，bound fraction `239489206.21880162`。这不是改写生产源码的故障注入实验。

同源 O0 的完整原生构建与七个场景已成功执行，编译日志明确记录 Bayesian C++ 两个翻译单元末尾 `-O3 -O0`；其全部评分数值与 O3 逐位相同。这是本机测得的零差异，不证明所有平台都相同，也不是 CLI selfcheck 的 floor 记录。完整命令、退出码、wall/build 分离时间及输入/输出比较保存在外部 evidence 目录，由主审接收；本文件不携带原生环境私有路径。

`test_validate.py` 包含全部 payload 同步重排正例，以及错配身份、重复/缺失/多余身份、shape/dtype、双方 NaN/Inf、float32 转换保护、概率质量未归一化、全零、质量移错节点、保均值质量偏移、错误拓扑及极端有限数值等负例。另将七个真实生产 NPZ 的全部节点、posterior 和边 payload 同步重排，validator 实跑通过且 distance=0；`test_produce.py` 专门检查混合 Python float/np.float32 列表也不能静默提升。测量只支持暂定 policy；没有运行 Docker、GPU、CLI selfcheck，也没有人工最终容差批准。

相关已知风险：`phantom-particle-reordering` 要求对齐每个身份相关 payload；`output-precision-floors-the-bound` 要求确认两 ULP 实际到达输出；`ungraded-sidecars-mask-identical-graded-output` 要求区分编译或日志差异与真正评分值差异。本 check 不向评分目录输出时间戳或构建诊断 sidecar。
