# pynndescent-ann-engine: authoring notes

> ## ⚠ 状态更正（2026-09-13）——下文若干陈述已被取代
>
> 本文件正文写于只有四个 check 的阶段。**以下三条陈述现已不成立**，请以本节为准：
>
> | 正文原话 | 现状（实测） |
> | --- | --- |
> | "Four representative checks…" / "Current checks are `dense-euclidean`, `dense-manhattan`, `dense-chebyshev` and `prepared-euclidean-query`" | **34 个 check**，全部已实现并跑通 |
> | "no Docker build has run" | Docker 镜像**已构建** |
> | "No Docker selfcheck … exists" | **selfcheck 已运行**，见下 |
>
> ### selfcheck 记录（CLI 写入，`comment/pipeline/self-validation.json`）
>
> * 完成时间 `2026-09-12T19:44:10Z`，主机 `a0164.ten.osc.edu`（x86_64），
>   `where = SLURM nextgen partition, account pcon0080 (OSC)`，`docker 5.4.0`
>   （**这是 podman 经 shim 报出的版本，不是 Docker 5**）。
> * **reward = 1.0，34/34 通过**，`problems` 为空，budget 900 s。
> * `build_seconds_nominal = 0.0`；受判段单 check 运行 12.8–111.0 s，
>   最慢三个是 `update-with-changed-data` 111.0 s、`degenerate-index-query-accuracy` 89.0 s、
>   `hub-tree-query-accuracy` 88.4 s。
>
> ### ⚠ 读 `bound_fraction` 前必须知道：它在本 leaf 里是**两个不同的量**
>
> * `policy=pointwise`（18 个）：`|cand−ref| / (atol + rtol·|ref|)`，即**跨侧扩散占容差的比例**。
>   本线最高 0.1604（`alternative-metric-corrections`）。
> * `policy=invariants`（16 个）：`max(reference 侧, candidate 侧)` 的**各自质量余量占用率**，
>   **不是跨侧扩散**。所以会出现 `distance=0.0` 而 `bound_fraction=0.51`（`index-determinism`）。
>   本组最高 0.5254（`prepared-euclidean-query`）。
>
> **不要把这两组放在一起排名。** 那四个 ~0.5 **不**表示「界快破了」，
> 而是「一次合法运行各自用掉约一半的质量余量」。
>
> ### ⚠ 16 个 `invariants` check 的判别力全部落在阈值上
>
> 逐个查过 `failures` 的写入点：16 个全是
> `for name, path in [('reference', …), ('candidate', …)]: failures.extend(assess(path)…)`，
> **跨侧比较不参与 `passed` 判定**。这是**有意设计且已写明的**
> （`index-determinism/validate.py:4-11`：NN-descent 是近似算法，要求复现参考的邻居 id
> 会否掉每一个正确移植）。设计站得住，但后果需要 reviewer 知道：
> **一个邻居图明显更差、只要越过 `min_recall` 的移植会通过。**
> 那两个负的 `distance`（`degenerate-index-query-accuracy` −0.0020、
> `index-update-query-accuracy` −0.00050）是有向的 recall 差
> （`reference.worst_recall − candidate.worst_recall`），负值表示候选反而更好——不是错，
> 但字段名会误导横向阅读。
>
> ### 第三条腿：**34 / 34 全部具备**
>
> 本线每个 validator 都从 `ic/nominal/inputs.npz` 独立复算精确几何。
> 对照：同期 liana 23 个、squidpy 27 个**均为两条腿**（与已 merge 的 mitgcm 一致）。
> reviewer 横向比较时不应把后者读成回退，也不应把本线的覆盖率当作全舰队水平。

Four representative checks have accepted implementation and native evidence. This is not a completed task, a finalized scientific policy, or Docker self-validation. This directory is hidden at runtime; pipeline records remain CLI-written only.

## Module and coverage status

The approved boundary remains the complete pinned PyNNDescent tree. Current checks are `dense-euclidean`, `dense-manhattan`, `dense-chebyshev` and `prepared-euclidean-query`. Each distance check retains all144 ordered pair distances from the complete12x20 float32 official fixture, including two distinct zero-vector sample IDs; the query check retains200 queries and ten ID-bound neighbors per query.

The official inventory remains four pytest files,70 source definitions,69 runtime-unique functions and162 collected nodes. The earlier `test_update_w_prepare_query_accuracy` at line543 is shadowed by line572 and receives no execution credit. Six notebooks comprise four executable data-dependent tutorials and two narrative-only notebooks. The current four checks do not exclude the remaining six dense metric nodes, other query representations, sparse/binary/rank tests, hub splits, updates, serialization or any executable tutorial. Final granularity, proposed exclusions and external-example data/permission questions remain unresolved.

Canonical scaffold wrote an empty current-module `test-survey.json` because the preserved local survey names the withdrawn `distance-metric-library` module. Neither that obsolete survey nor the empty projection is a completed formal survey. No manual approval or self-validation record replaces the CLI records.

## Build and resources

All four run scripts copy source into disposable scratch and verify the import location. The native environment is reused read-only. No standalone source build occurs: `SAB_BUILD_SECONDS=0`; import and lazy Numba compilation remain part of runtime, not an estimated build cost subtracted from it. Cross-check JIT reuse has not been established.

`SAB_THREADS` controls Numba settings and the ANN constructor's `n_jobs` argument where honored, not every joblib pool. `rp_trees.py:2909-2920` still hard-codes `Parallel(n_jobs=-1, require="sharedmem")` for leaf extraction. BLAS/OpenMP environment settings are1. Recorded native runs used an external single-CPU `taskset` affinity as the CPU boundary. Direct `run.sh` imposes no universal pool/affinity/cgroup guarantee; formal resources must establish and verify an external boundary separately. The task's8-CPU/16-GB resource values remain CLI fallback placeholders, not measurements or consent. Both Docker dependency recipes remain unfinished and no Docker build has run.

## Native evidence and timing scope

The first pair's revised artificial/protocol suite contains87 parameterized test cases, not87 upstream source functions. The Manhattan/Chebyshev pair has110 parameterized artificial/protocol cases, including legal float32/float64 byte orders and C/F layouts, malformed archives and extreme finite values. Parent verification and independent review accepted their implementation/native evidence; these are not complete upstream pytest-suite runs or final tolerance approval.

The second pair's nominal producers were measured in one combined command. Its full wall time and each producer's internal import and matrix/output timings are retained, but individual full nominal wall times were not separately collected. Never present the internal timers' sum as the missing full wall. The separately measured complete variant walls were about12.9145s and12.7128s. The recorded cold processes include import/JIT and output overhead, and do not establish the entire module's budget.

Fresh complete producer outputs confirmed18 active Manhattan values and6 active Chebyshev values under their two-ULP variants. Their actual nominal and variant NPZ hashes differ, while IDs and both zero rows remain unchanged. These are per-check files, not the planning container holding both arrays. Mathematical/permutation positives and output-contract negative proxies were classified explicitly; no source mutation or accelerator implementation was claimed.

## Provisional tolerances and quality

Euclidean and Manhattan each use `atol=1e-6, rtol=2e-6`; Chebyshev uses `atol=5e-7, rtol=1e-6`. Both axes follow sample identity, and both outputs are also checked against the corresponding independently computed fixed-input geometry. Manhattan's float32-operand short sum can differ from the same-source interpreted path even when JIT output is float64. Chebyshev's max avoids accumulated-sum error but not float32 subtraction rounding. A planning `py_func` comparison is not an altbuild or final device floor.

ANN checks all reported edges with `distance_atol=2e-6, distance_rtol=2e-6`. Its provisional tie band is `2e-7+2e-6*r10`; strict numerical core slots are reserved and cutoff ties receive only the remaining `10-|C|` slots. Mean recall must meet the upstream0.95 floor, without demanding each query reach0.95 or reproduce the CPU neighbor set. At positive scales, provisional mean-distance factor2 and farthest-distance factor3 limit local degradation; exact zero scales use absolute `distance_atol`, not division by zero or a skipped query. Better quality is allowed. These geometric factors, tie band and all distance tolerances still require human scientific finalization.

The earlier inactive ANN perturbation at query[0,0] remains negative calibration evidence; query[0,3] later produced real distance changes. Changed input bytes, random streams, timing or diagnostic sidecars alone never count as numerical-noise calibration. No Docker selfcheck, alternative-build floor or human tolerance finalization exists.

## Verification boundary and remaining work

Validators use NumPy and the standard library. Complete identities, every graded value/edge, archive schemas, sizes and finite values are validated. Decode/comparison/strict-ASCII JSON/UTF8 encoding share an ordinary-Exception guard; failures create fresh records, cancellation propagates and output writing remains outside the guard. Check-local tests generate independent artificial operands and can run with only validator/test files under an empty HOME, without reading actual graded nominal data.

Remaining work is the full official survey/check set, scientific finalization, acceleration selection, validated Docker dependency recipes, the external resource plan and consent, Docker calibration and final selfcheck. Having four implemented checks does not establish full-module coverage or waive these stops.

---

## variant「测出来的零」的记录（2026-09-13）

`self-validation.json` 里有 warning 指出本线若干 check 的 nominal 与 variant 输出逐字节相同、
「the perturbation never took effect」。**这些是准确的**：`ic/variant` 与 `ic/nominal` 确实
不同，扰动施加了，只是没越过任何判定边界。相关 rubric 的 `variant` 字段已补记这是
**测出来的零**而非声明的 identical，并明说不应通过把字段改写成 `identical` 开头来消掉警告。

## STOP 4 定稿与 2026-09-14 的变更（人工拍板后补录）

### 容差：原样定稿
用户 2026-09-14 当面决定「原样定稿」。依据：本 leaf 全部 check 的
`evidence.self_validation_spread` / `bound_fraction` 都已是**实测数**，null 归零；
每条 warrant 都是从读源码推出的物理论证，没有无出处的房屋默认值。
SPEC 的原则是界代表**真实的跨平台科学等价**而不是实测到的那点扩散，
所以「界比实测松」是设计要求，不是余量浪费。

### 六条 `the perturbation never took effect` 告警：留着，不改写 variant
harness 的判据是 `rubric["variant"]` 是否以 `identical` 开头。把它改写成 identical
会让告警消失，但那是**假话**——扰动确实施加了，`ic/variant` 与 `ic/nominal` 并不相同，
只是没有越过任何判定边界。告警本身是准确的，各 rubric 里逐条写明了实测依据。
这个二元模型（identical / differing）无法表达「确实扰动了但结构上到不了受判量」这第三种状态。

### produce 循环改为并发执行
`tests/test.sh` 的 produce 循环换成了有界并发池（`SAB_JOBS` / `cpu_quota()` / `STATUS_DIR` /
`wait -n`），**形状逐字取自已 merge 的 `tasks/pluto/*`**，与它的 diff 只有并发脚手架，
`run.sh` 的调用与一切科学部分未变。

**已实测证明并发不改变任何算术**：同一份输入在串行轮与并发轮之间，
每条 check 的 `distance` 逐位相同（liana 23/23、squidpy 27/27、cassiopeia 38/38）。
PyNNDescent 有 4 条不同，但**两次串行运行之间就有 5 条不同**（差异集是超集），
说明那是 NN-descent 固有的运行间随机性，与并发无关。

⚠ **读数口径变了**：`sab.py` 报的 "suite run time" 是**各 check elapsed 之和**，不是墙钟。
并发下单条 check 因争抢而变长、整套墙钟大幅缩短，所以这个数字上升而实际等待下降。
本 leaf 的墙钟实测见下表；把它当 CPU 秒读，不要当等待时间读。

### test-survey 补齐
此前 `comment/pipeline/test-survey.json` 是空的，导致 review brief 打出
`THIN (0 suitable official tests)`。**那不是误报，是 survey 确实没做。**
现已逐个文件过完整个官方测试面并写入判断（suitable 与否、理由、是否已实现为 check）。
判断依据是每个文件**实际有无数值断言**，不按目录名或基类一刀切。

本 leaf 的墙钟：34 条 check 从串行 1182 s 降到 **216 s**（5.5×）。
survey 34 条，**4 个测试文件的 69 个 `def test_*` 全部被 check 引用，零遗漏**。
⚠ 本 leaf 有 5 条 check 实测存在运行间随机性（`degenerate-index-query-accuracy`、`index-serialization-roundtrip`、`index-update-query-accuracy`、`query-accuracy-metrics`、`unseeded-neighbor-graph-accuracy`）：两次串行运行之间它们的 distance 即不同。**这几条的 `self_validation_spread` 是一个样本，不是定值**，读标定表时要按这个口径。
