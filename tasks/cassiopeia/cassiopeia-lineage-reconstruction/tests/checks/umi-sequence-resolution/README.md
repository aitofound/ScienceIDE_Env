# umi-sequence-resolution

本 check 检查 `pipeline.resolve_umi_sequence` 的独立科学目标：**为每个 cellBC/UMI 选择支持数最高的现有 DNA 序列，再整体过滤低覆盖 cell**。它不是 UMI barcode 纠错，不调用 `error_correct_umis`，也不把两方法并入其他 suite。

## 官方来源与配置

来源为 `code/cassiopeia/test/preprocess_tests/resolve_umi_sequence_test.py::TestResolveUMISequence`，保留原始七行 collapsed table。

| 官方方法 | 生产调用行号 | 参数 | 科学输出 |
|---|---|---|---|
| `test_resolve_umi` | 67–69 | min_umi_per_cell=1；默认 avg cutoff=2.0；plot=False | `resolve-default.npz` |
| `test_filter_by_reads` | 94–100 | min_umi_per_cell=1；avg cutoff=30.0；plot=True | `filter-by-reads.npz` |

两个完整 nodeid、source anchor、原始数据与参数均在 `ic/nominal/inputs.json`。默认 2.0 显式物化，第二个 cutoff 用等值浮点数表示；原始 readCount、seq、qual、grpFlag、readName 均不改变。原 plot=False/True 调用保留，图像只写 scratch，不进入科学 grade。

## 选择与支持数语义

`pipeline.py:382–425` 按 `(cellBC,UMI)` 分组。多条候选序列时按原 readCount 降序，保留最高支持的**那一行**；singleton 直接保留。

**输出 readCount 不是竞争候选的支持总和。** `total_numReads`、`top_reads` 等中间统计只用于 diagnostic plots，源函数没有把竞争序列的支持数加回赢家行。不能套用 UMI error correction 的合并/守恒规则，也不能为了“保留总 reads”而抬高所选序列的支持数。

`utilities.py:111–140` 随后计算每个 cell 的所选 UMI 行数与所选 readCount 均值，使用 `>= min_umi_per_cell` 和 `>= min_avg_reads_per_umi` 的交集，**整 cell 保留或删除**。这不是逐条 UMI 的 readCount cutoff。当前输入的 UMI 是字符串，此调用走行数计数分支；不声称覆盖该 helper 的 numeric-UMI-count 模式。

## 身份、tie 与未评分边界

- 科学身份是 `(cellBC,UMI)`，不是单独 UMI、readName 或表的行号。同名 UMI 可以在不同 cell 出现。
- 原始七个 readName 当前全部唯一，六个分组均有唯一最大 readCount；因此所选 seq 没有任意 tie-break。
- validator 重验这一前提。新输入若出现最大支持 tie 或 readName 碰撞，不擅自固定任意代表，而是明确拒绝套用当前唯一赢家合同。这里不宣称一般 tie 情形已解决。
- `readName` 在源路径内部用于 mask，但输出的名字格式与 DataFrame 索引/行序不评分。改变输出名字不应改变科学结果。
- `qual` 不参与此函数的选择或 cell 过滤。原 fixture 的两个七碱基 seq 配有六字符 qual，这个原样不一致被保留，没有修补输入；也不声称这些 quality 值或长度关系已验证。
- `grpFlag` 只用于诊断计数，PNG 内容、绘图格式、日志、临时路径与 pass bits 均不评分。

**seq 本身必须评分。** 换成低支持候选、将不同 molecule 的 seq 错配，或按 qual 长度截断 seq，即使 cell 总 reads 完全不变，也属于科学错误。

## 输出合同

两个文件均为标准 NumPy NPZ，不使用 pickle。每个文件必须且只能包含以下三个数组；`M` 是该场景定义的完整保留分子数。

| 数组 | dtype | shape | 内容 |
|---|---|---|---|
| `molecules` | Unicode，足够的字符串宽度 | `(M,2)` | cellBC、UMI |
| `sequences` | Unicode | `(M,)` | 真实所选 DNA 序列，完整保留碱基顺序与长度 |
| `read_count` | signed int64，任一 byte order | `(M,)` | 所选序列原行的 readCount |

允许任意分子行重排，但三个数组必须同步移动；不按 seq 或 counts 各自排序后比较。Unicode 宽度、byte order、健康的 ZIP 压缩方式不构成科学差异。不能 flatten 不同 shape、用 float/bool/object/unsigned 代替 signed int64 支持数，或输出遗漏/额外/重复/跨 cell 的 molecule。

比较保持完整保留集合，并逐身份检查 seq 和所选支持数。未知/错选 DNA 不能靠正确的 counts 掩盖。完整矩阵或空表也不能自动通过：空表只有在 IC 的选择与过滤确实没有保留分子时才有意义，当前两个官方场景均非空。

## Provisional policy 与变体

采用 provisional pointwise：DNA 序列精确匹配，整数支持数 `atol=rtol=0`。唯一最大支持的离散选择没有可接受的“少一个碱基”或“多一个 read”舍入误差；最终规则仍待人工 review。

`ic/variant/inputs.json` 只把两个真实浮点 coverage cutoff 各向正无穷移动 binary64 两 ULP，其他参数和七行数据完全不变。实际所选均值远离 cutoffs，六条保留 seq 和六个所选支持数全部相同，两 NPZ 文件逐字节相同。这是**graded-zero**，不能把日志、输入字节或图像变化称为非零 noise 证据，也不能人为扰动整数 readCount 或 DNA 字符制造差异。

覆盖限制明确保留：当前每个 cell 解析后有两个 UMI，min_umi_per_cell=1 并未实际筛除 cell；所选平均值也不在 cutoff=2/30 边界上。因此这两个场景不验证全部 UMI-count 阈值、`>=` 对比 `>` 的边界，或所有能产生相同输出的内部筛选顺序。自测中的合成边界案例不算额外 benchmark 场景。

不声明 altbuild：当前执行的是 Python/Pandas 选择与 `filter_cells`，没有调用本包自定义 Cython/Numba 数值核心。重编译未执行的扩展或关闭 JIT 不能冒充这条路径的独立 build floor；也未验证不同 runtime/dependency build。

## 运行与隔离

```bash
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh nominal
SOURCE_DIR=<pinned-source> CHECK_DIR=<this-check> OUT_DIR=<empty-output> bash run.sh variant
bash run.sh --help
python selftest_validate.py
```

只有 `SAB_PYTHON=python3` 解释器选择。原七行问题没有可缩减的科学分辨率/时间窗，不通过复制样本、重复计算或合并其他 tests 制造 workload，也没有 acceleration 标签。

脚本使用 strict mode，复制只读 source 到临时目录，离线执行真实 wheel 构建与 `pip install --target` 独立安装。镜像只需预装依赖，不要求 Cassiopeia 已预装；不修改 source/venv、不共享其他 check 模块、不缓存 graded 结果。完整 package import 需要构建 `build.py` 的三个扩展，包括 C++17 扩展，但它们不是此科学步骤的计算核心。

`SAB_BUILD_SECONDS` 报实际复制、构建与安装时间。正式入口没有固定调查 timeout；本地取证从外部施加 180 秒保护。绘图使用 headless Agg，原 source 创建的 PNG 留在临时目录并清理，不放进 OUT_DIR。跨 check build reuse 留待统一 suite 集成。

## 最终失败协议

正常科学判分和预期错误分类保持独立。最终安全边界对未预期的普通 Exception、严格 JSON 序列化及 UTF-8 编码失败，重新创建 `passed=false`、`distance=null`、`bound_fraction=null`、`files={}`，保留 qualified exception type、context 与 traceback，不复用含 NaN 或 passed=true 的半成品，不捕获 BaseException。

先在保护区内完成 `json.dumps(ensure_ascii=True, allow_nan=False).encode('utf-8')`，再于区外 `write_bytes`；错误 Unicode 不会先触碰结果文件。真正无法写文件时返回非零，不伪装成已写出的 verdict。健康 DEFLATE/LZMA 接受，损坏流、加密、CRC、截断及缺失归档均被拒绝。CLI exit 0 仅说明 verdict 已写出，实际 grade 看 `passed`。

通过时 distance/fraction 为 0。只有支持差异时报告整数误差；DNA 或身份无法建立一致比较时 distance 可为 null，不能把它解释为零科学误差。

## 已执行证据

2026-09-10，Linux x86_64、CPython 3.12、NumPy 1.26.4、pandas 2.3.3、Cython 3.3.0、system gcc/g++，仅原生运行，未 Docker/GPU。

| 入口 | 构建秒数 | 非构建 wall 秒数 | 结果 |
|---|---:|---:|---|
| nominal | 14.943408 | 3.996022 | 两场景完整返回六个分子记录 |
| variant | 14.943598 | 3.949439 | seq/support 全通过，distance=0，2/2 文件逐字节相同 |

- 完整官方 suite 原生 **2 passed，2.91 秒**；plot=False/True 对应 scratch PNG 数为 0/3。
- 四轮实际 RED→GREEN；最终 **16 方法、89 次 CLI 比较探针通过**，包括健康/坏 LZMA、双方普通意外 Exception、NaN 结果与坏 Unicode 写入前拦截。
- 实际输出三个数组同步重排通过。只错配 seq 而保留全部正确 counts，六条 seq 均被拒绝；支持数加一被拒绝。跨 cell、缺失/额外/重复 molecule 也被拒绝。
- 真实 API 输入行倒序、输出 readName 重新命名，科学结果均通过。
- 真实生产故障探针选择最低支持 seq 被拒绝；累计竞争支持被拒绝，最大支持误差为 9；错误逐 read 过滤或降低 coverage cutoff 被拒绝。
- 按 qual 长度截断返回 seq 的受控故障被拒绝：四条 DNA 序列错误，支持数仍全部正确。这不是原 source 的行为，也没有修改原 fixture 来回避它。

CLI `floor`、`self_validation_spread`、`self_validation_bound_fraction` 保持 null。完整 suite、全 API/quality/绘图接口、正式 Docker/selfcheck、GPU 及最终人工 policy 确认均未完成；不以本 check 的原生结果替代它们。
