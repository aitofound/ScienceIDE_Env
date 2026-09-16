# bam-quality-filtering

官方来源为 `code/cassiopeia/test/preprocess_tests/filter_bam_test.py` 的 `TestFilterBam.test_filter`，其中两个 `pipeline.filter_bam` 调用分别使用固定阈值 **10** 与 **20**。本检查验证每个阈值下**完整的保留 read 集合**，以及每条保留 read 的 barcode / UMI / 序列 / 逐碱基质量是否仍与该 read 绑定。

**policy、字段 scope 与精确相等要求均为 provisional，等待人工最终定案。** 原生测试不是 Docker、selfcheck 或 GPU 验证。

## 两个固定科学配置

`ic/nominal/inputs.json` 只声明官方 BAM 的 source 相对路径与两个阈值配置：

| case ID | `quality_threshold` | 官方位置 |
|---|---|---|
| `quality10` | 10 | `filter_bam_test.py:22` |
| `quality20` | 20 | `filter_bam_test.py:27` |

输入是 `test/preprocess_tests/test_files/10xv3_unmapped.bam`，2 条 unpaired / unmapped 的 10xv3 记录，每条 274 个碱基、16 碱基 `CR` 与 12 碱基 `UR`。没有增加官方以外的 read、配置或数据。每个配置像原 test 一样使用一个全新的输出目录；过滤后的 BAM 写在临时目录里，不落进 `OUT_DIR`。

## 被评分的观测

源码 `cassiopeia/preprocess/pipeline.py:129–144` 的谓词是：用 `pysam.qualitystring_to_array` 把 `CY` 与 `UY` 解码成 PHRED33 整数，**两个向量的每一个碱基**都必须 `>= quality_threshold`，二者取 AND。`ngs_tools/bam.py:337–367` 把 `False` 映为 `None`，其余把同一个 `AlignedSegment` 原样写出。

因此产物 `results.json` 对每个配置给出完整记录表，每条记录九个字段：

| 字段 | 含义 |
|---|---|
| `qname` | read 身份。与 `mate_role` 共同构成对齐 key |
| `mate_role` | 当前 fixture 只有 `unpaired` 这一类型边界，不逐 bit 比较 SAM FLAG 整数 |
| `alignment_status` | 当前 fixture 只有 `unmapped`；质量过滤不能假称产生 mapping |
| `sequence` | 完整 274 碱基观测序列，位置顺序有科学意义 |
| `sequence_phred` | 与每个碱基同位绑定的完整 PHRED 整数向量 |
| `CR` / `UR` | 原始 cell barcode 与 UMI |
| `CY_phred` / `UY_phred` | 与 `CR` / `UR` 逐碱基绑定的完整 PHRED 整数向量 |

`quality20` 这样的空结果**必须显式给出** `records: []`；缺块、缺文件或坏归档仍然失败。只交条数、只交 min/mean 质量、或把 records 换成计数，都不满足 schema。

**不评分但保留原始审计**：`RG` 字面值与 header `RG.ID`（NGS 可随机生成句柄，且此 header 没有 `SM`/`LB` 等样本属性）、BGZF 压缩字节、`HD.VN`/`SO`、`PG`/`CL`、tag 存储顺序、文件名与计时；以及这些 unmapped/unpaired 记录上没有有效科学量的 `reference_id`/`position`/`MAPQ`/`CIGAR`/mate 位置/`TLEN`，和当前全为 false 的 QC-fail / duplicate / secondary / supplementary 位。是否把 read-group 划分关系纳入评分属于待人工决定的 scope。

## 暂拟等价规则

- 对齐 key 是 `(qname, mate_role)`；case 块顺序、记录顺序与记录内字段顺序**不评分**，但所有字段必须按同一个 key 同步对齐。PHRED 向量**内部位置不可重排**。
- DNA 大小写是存储表示，`sequence` / `CR` / `UR` 统一折叠为大写；PHRED 只解码为等价整数，不做任何折叠。
- 质量分数是小整数编码，改动一个单位不是舍入噪声而是另一个观测，所以 `atol = rtol = 0`，精确相等。
- validator 不只做双侧比较：它用 `rubric.json` 里与你手上同一个官方 BAM 同源的**可信输入表**，独立按上述谓词复算每个阈值应保留的集合。参考与候选双方同错也会被拒绝。
- 同一个 `(qname, mate_role)` 出现两次不会被静默去重，必须拒绝。

## 覆盖限制（已实测，如实公开）

本 fixture 只有 2 条 read：两条的 `CY` 最小质量同为 **16**、`UY` 最小质量同为 **15**、`SEQ` 最小质量同为 **12**，且**没有任何碱基质量恰好等于 10 或 20**（这些都是 `rubric.json` 里公开的输入事实）。因此在 10 与 20 两个阈值下，`CY` 与 `UY` 两个谓词恒同真同假。已用 in-memory 源码体 AST 代理实测确认，下列实现错误**不能**由本 check 区分：

- `AND` 写成 `OR`
- 只看 `CY`（或只看 `UY`）
- 误用 `SEQ` 质量代替 barcode/UMI 质量
- `>=` 写成 `>`

实测**能**区分的：`all` 写成 `any`；PHRED64 误解码（在解码阶段直接崩溃，check fail-closed）。

未观察到、因而完全不在本 check 覆盖内的：missing / empty tag、重复 QNAME、paired 或 mapped 记录、reverse 存储方向、QC/duplicate 标记、数值或数组型 optional tag、多 read-group 的样本关系。若必须区分上述盲点，需要另行批准额外的官方 fixture 或 custom 输入——本 check 没有创建新数据，也不以“加数据补洞”的方式掩盖它们。

## 判决书的形状

`validate.py` 把两类失败分开:

- **合同失败**(文件缺失、JSON 坏、schema 不符、身份/覆盖缺失或多余)走异常路径,判决书里有 `error_type`,`distance` 为 `null`——这确实是「你的产物读不出来」。
- **科学不一致**(值与独立复算或与对侧不符)走正常路径,判决书里**没有** `error_type`,`distance` 与 `measurements` 都填好。

本合同没有连续量(`atol = rtol = 0`),所以 `distance` 报的是**不一致的 graded 值个数**,`bound_fraction` 在 `atol = 0` 下无法定义分数:通过时 `0.0`,失败时 `null`。`measurements` 区分「双侧之间不同」与「某一侧与独立真值不同」,后者能把双侧同错的情形单独指出来。

本 check 的失败一律是离散量不符,所以 `bound_fraction` 从不表示「余量充足」;失败的原因在 `distance`(不一致的 graded 值个数)与 `measurements` 里。

## 失败协议

输入、解码、比较或 JSON 编码的普通 `Exception` 生成全新的 `passed=false`、ASCII 可编码且 UTF-8 有效的 JSON，覆盖旧的通过结果，不保留 partial pass；中断等 `BaseException` 不转换成普通科学判分，结果写盘失败暴露为非零退出。JSON 拒绝重复对象键、`NaN`/`Infinity`、超过 1 MiB 的产物、控制字符与非核苷酸符号。

## variant

整条路径是离散的整数 PHRED 比较与 record 复制，**没有活跃浮点**：把阈值 10 改成 20 是换配置，不是数值噪声。因此 `ic/variant/inputs.json` 是 nominal 的**逐字节相同副本**，明确不提供 ULP / noise 校准证据，也没有已验证的 altbuild；`run.sh altbuild` 明确退出 2。

## 运行与可移植自测

```bash
SOURCE_DIR=/path/to/source CHECK_DIR="$PWD" OUT_DIR=/path/to/output bash run.sh nominal
python3 selftest_validate.py
```

`SAB_PYTHON=python3` 选择已预装依赖的解释器。`run.sh` 自包含地复制只读 source，在 scratch 离线构建 wheel，用 `pip --target` 安装到私有目录，不修改 source 或预装环境，线程固定为 1。没有删减覆盖或人为重复的科学运行旋钮。

自测只需标准库，通过 `__file__` 找到 validator、通过 `sys.executable` 运行正式 CLI；人工 fixture 不读取 HOME、生产 source、真实 BAM 或真实 nominal 输出。自测使用一条只过 10、一条两档都过、一条两档都不过的**人工三条 read**，因此能覆盖真实 fixture 给不出的混合选择。
