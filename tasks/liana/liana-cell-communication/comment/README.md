# LIANA 整库任务阶段记录

此目录在运行时隐藏，属于非规范性的作者说明。`comment/pipeline/` 只由canonical CLI写入；本文件不能替代每项检查的输入、输出、validator或rubric，也不是运行consent或人工最终policy批准。

## Module

批准的模块边界为 `paths=[.]`，即pin `f45f7efeb89fdb652dd13f6b303514348dadbc8b` 的完整LIANA+。它包括单细胞配体—受体评分、空间双变量指标、LRIC、inflow、多视图变换、模型/网络推断及相关资源与绘图基础设施。八项代表实现不构成缩减后的模块边界，也不因已有GPU实现或第三方kernel而排除其它子系统。

已枚举的官方测试面包含231个collected pytest items、54个独立doctest items及14个notebooks；notebook共有414个code cells，其中410个非空。定义数、collected items、cells、科学阶段与最终checks是不同单位。当前下列八项分别对应八个真实官方nodeid，并完成阶段性代码/科学复核；其余223个pytest items及doctest/notebook科学阶段的逐项适用性、stage拆分、资源与时长尚未完整定稿或实现。下一候选的有限native调查不等于已实现check或通过最终验收。CLI当前保留空test-survey，是正式survey未完成状态，不是“没有其它官方测试”的证明。

## 已实现的八项独立入口

| 检查 | 官方nodeid | 完整observable与身份 |
|---|---|---|
| `lric-pairwise` | `tests/method/sp/test_LRIC.py::test_lric_pairwise` | 2250行九列表，按有向source/target/ligand/receptor/radius对齐g、g_expr、g_pcf及各自NaN定义域 |
| `weighted-pearson` | `tests/method/sp/test_bivariate_funs.py::test_pc_vectorized` | 全20×5相关系数，人工固定center/pair身份 |
| `weighted-spearman` | `tests/method/sp/test_bivariate_funs.py::test_sp_vectorized` | 全20×5全spot平均秩加权相关系数，人工固定center/pair身份 |
| `weighted-cosine` | `tests/method/sp/test_bivariate_funs.py::test_costine_vectorized` | 全20×5有符号Cosine similarity，人工固定center/pair身份；保留上游costine拼写 |
| `weighted-jaccard` | `tests/method/sp/test_bivariate_funs.py::test_vectorized_jaccard` | 全20×5正值presence的weighted交集/并集比，人工固定center/pair身份 |
| `local-morans-primitive` | `tests/method/sp/test_bivariate_funs.py::test_morans` | 全20×5有符号两项乘积和x*(W@y)+y*(W@x)，人工固定center/pair身份，不套归一化相关系数域 |
| `weighted-product` | `tests/method/sp/test_bivariate_funs.py::test_product` | 全20×5 dense (W@x)*(W@y)，人工center/pair身份，保留符号及无界幅值 |
| `normalized-weighted-product` | `tests/method/sp/test_bivariate_funs.py::test_norm_product` | 全20×5 dense平滑后按全列max(abs)归一化乘积；保留完整spot总体，不用winner行位置作身份 |

共享官方mats不意味着复用科学执行credit；各项有自己的IC、producer、validator和人工自测，没有跨check运行helper。输入重排同步作用所有共享轴；不把CSV行号、CSR存储slot或任意输出位置当物理身份。

## Build

每个 `run.sh` 从只读 `SOURCE_DIR` 复制到独立临时目录，使用 `pip --no-index --no-deps --no-build-isolation --target` 构建/安装纯Python LIANA；运行依赖必须预先存在。当前没有跨check构建缓存，重复安装成本分别记录为 `SAB_BUILD_SECONDS`，不修改既有venv或原source。

各次原生执行的wall、package build/install、非build及核心production call时间分开保存在本地不可变证据中。rubric的 `expected_runtime_s` 取各自完整脚本wall减build/install，仍含import/JIT/I/O；不是微秒级核心调用，也不能用组时长分摊成逐check测量。当前线程参数默认1；task.toml中8 CPUs、16GB、900秒仍是scaffold默认，不是已测资源或获批计划。

两个Dockerfile的依赖行尚未定稿；没有运行Docker build、CLI selfcheck或GPU，没有consent、自验证或跨平台floor。后续先完整准备依赖/资源方案，再展示run plan并等待人工决定。

## Tolerances

八项均为provisional `pointwise`：LRIC暂用atol=1e-6、rtol=1e-5；其它七项暂用atol=1e-5、rtol=1e-5。此次metadata整合未改变八项的任何rubric比较字段、IC、producer、validator或公开说明，也没有重跑这些已验收科学；下一候选的有限调查单独记录。

这些量级依据已读源码中的float32乘法/归约、float64累加或输出buffer、方差阈值、clip及上游断言，而非机械复制两ULP小扰动距离。LRIC和Spearman合法输入重排的误差明显高于各自单值variant；Spearman一次重排用掉约0.1842的暂定bound，是需继续审视的有限余量，不是宽裕跨平台保障。最终policy、bounds、variant覆盖与跨平台等价性仍待人工正式校准。

LRIC三数值具有不同的NaN定义域，不能统一mask或把NaN抹成0。Pearson/Spearman有方差归零和clip；Cosine不中心化、不ranking，epsilon在平方根内部，也没有末尾clip，不能机械套用其它函数的硬边界。

## 证据与计数边界

四批已验收原生科学执行各自保留不可变命令、exit、source patch/hash和生产文件：第一批LRIC/Pearson，第二批Spearman/Cosine，第三批Jaccard/local Moran primitive，第四批dense product/normalized product。此次metadata freeze引用四批既有证据，没有重跑这八个producer，也不伪造CLI self-validation；下一候选的调查不混入这四批验收计数。

- 第一批有53项纯validator/protocol自测及29份完整产物复判。
- 第二批有51项纯validator/protocol自测及32份完整产物复判；另有5项依赖LIANA/NumPy/SciPy的人工数学自测，不能称为56项portable测试，也不增加graded coverage。
- 第三批有52项纯validator/protocol自测及32份完整产物复判；另有6项依赖LIANA/NumPy/SciPy的人工数学开发测试，不能称58项portable测试或增加graded覆盖。
- 第四批有52项纯validator/protocol自测及32份完整产物复判；另有7项依赖LIANA/NumPy的人工数学开发测试，不能称59项portable测试或额外graded覆盖。
- 四批各有六个真实source故障配置，均先成功产生完整输出，再由validator判定。第一批包含五个有限数值故障和一个独立NaN定义域变化；第二批中的两个abs、第三批中的Moran clip、第四批中的raw product abs明确是source-return-transform，不混作输出文件代理。第三批Jaccard的presence!=0与错误intersection变换在该fixture落到同类坏结果，不夸称两个独立精度证据。
- 完整payload的schema/key/domain/局部数值绑定测试另作分类，不能把header拒绝或wrapper崩溃称为真实科学数值故障检出。

## Blind spots 与未决事项

- 整库survey和绝大多数官方阶段未完成，不将八项通过称为整库最终验收。没有因数量或默认时间预算删除其它适合的官方检查。
- LRIC单表达variant不校准几何float32环带分箱，`g_pcf`不动；半开边界fault尚未单独执行。数据中的合法NaN语义按各列科学定义处理。
- Pearson原fixture未专门覆盖零分母；Spearman的400个weight全正、X/Y每列无ties，因此非零邻域与全spot相同，不能凭本项数据检出全部masked/local-rank替代。源码实际仍全spot average-rank；人工tie病例不补充graded覆盖。
- Cosine原fixture未覆盖zero-vector或epsilon占主导的近零区间。人工小算例只验证开发语义，不擅自变为custom graded数据。
- Jaccard的weight-only两ULP不校准presence跨零，原fixture未专门覆盖empty union；Moran primitive不覆盖LocalFunction的前处理、local/global p-value或置换null。相关人工小算例仅开发自测。
- 第四批必须保留官方dense W路径：pair块1对块5的实际差异，raw为2.86102294921875e-6、normalized为1.7881393432617188e-7；全轴输入重排的最大bound_fraction分别约0.1318与0.01486。不能宣称dense分块bit-identical或已有宽裕跨平台floor。
- normalized product局部variant虽触达最终值，却未改变列normalizer；原fixture无zero norm或max ties，人工边界算例不代表这些分支的graded校准。spot不能拆块独立归一化，不能把winning-max行当物理身份。
- masked-Spearman、analytic与permutation p-values、其它global指标、单细胞方法、LRIC配置、模型、资源和official examples仍留在全量覆盖义务中，不因八项通过而删除、合并或视为已覆盖。确定性的analytic方法不能仅因名字含p-values就预设随机/invariants；各自源码与统计输入定义必须先调查。
- LRIC使用的Scanpy package-local PBMC派生输入已记录版本/来源/hash且没有新下载，但数据再分发许可仍pending；代码许可证不自动代表数据授权，不在人工决定前外发数据。
- 其它官方资源面仍有MetaLinksDB、HCOP、PROGENy及notebook数据的冷态闭包、版本与许可决定；历史缓存存在或历史摘要不能代替完整执行证据。
- 尚未选择或标记acceleration workload，Docker依赖未定，资源计划/consent与最终calibration未进行。静态门留下的这些错误必须如实保留，而非为了变绿临时贴标签或伪造记录。

## 从 solver 可见文件迁出的诚实披露（面向 reviewer 与人工，不进任何镜像）

`environment/Dockerfile` 是 `COPY tests/ /workspace/tests/` **整目录**，所以 `rubric.json` 与
`README.md` 的每个字节 solver 都看得见；`comment/` 两个镜像都不拷贝，是这类披露的正确归宿。

**scseqcomm-interaction-score：上游钉的回归值余量只剩 8%。** 官方 `test_scseqcomm` 钉的
`0.6819619345` 与 pinned source 现在算出来的 `0.6819757298342821` 差 `1.38e-05`，卡在 numpy
`decimal=5` 判据（`1.5e-05`）里面，余量约 8%。上游测试目前仍过，但该回归值相对代码已略陈旧。
本 check 评的是**实际输出**而不是那个期望值，所以不受影响。

这三个数原本写在该 check 的 `README.md` 与 `rubric.json` 里，现已迁出：计算值本身就是一个
graded 单元（`scores.csv` 的 `receptor_cdf`，参考产物中出现 14 次），而钉值（solver 经
`COPY code/liana/` 本来就有）与差值、余量百分比中的任意一个组合都能把它反推出来。
solver 可见侧只保留定性披露与"为什么不记"。属 A4 上游报告候选。

---

## selfcheck 记录与读法（2026-09-13 补录）

此前本文件未记录 selfcheck 的结果。补上，并附一条读 `bound_fraction` 的注意事项。

**CLI 写入的记录**（`comment/pipeline/self-validation.json`）：完成于 `2026-09-12T18:58:22Z`，
主机 `a0154.ten.osc.edu`（x86_64），`where = SLURM nextgen partition, account pcon0080 (OSC)`，
`docker 5.4.0`——**那是 podman 经 shim 报出的版本，不是 Docker 5**。
**reward = 1.0，23/23 通过**，`problems` 为空，budget 900 s；
`build_seconds_nominal = 41.9`，受判段单 check 运行 13.6–20.8 s，
最慢三个是 `bivariate-wrapper-analytic` 20.8 s、`rank-aggregate-consensus` 17.2 s、`global-moran-analytic-pvalues` 16.7 s。

**计分跑的方向**：`reward.reference_dir = oracle-nominal/results`、
`reward.candidate_dir = oracle-variant/results`。即**参考=nominal、候选=variant**，
`bound_fraction` 量的是两-ULP 扰动造成的跨侧扩散占容差的比例，不是同侧自比。

**本线的 `bound_fraction` 分布**：全部为 `policy=pointwise`，
即 `|cand−ref| / (atol + rtol·|ref|)`。最高 0.08815（`lric-curve-summaries`），恰为 0 的有 2 个。
**没有一条界接近吃满。** 另注：同期 pynndescent 线有 16 个 `invariants` check，
其 `bound_fraction` 是「每侧各自的质量余量占用率」而非跨侧扩散，
**两组数字不可混排**（那边会出现 `distance=0.0` 而 `bound_fraction=0.51`）。

**第三条腿**：本线 23 个 validator **均为两条腿**（参考↔候选 + schema/身份校验），
与已 merge 的 mitgcm 一致，各 rubric 也未宣称具备第三条腿。对照：同期 pynndescent 线
34/34 都从 `ic/` 独立复算。这是证据强度上的不对称，据实写明供 reviewer 权衡，
不应被读成本线有回退。

---

## variant「测出来的零」的记录（2026-09-13）

`self-validation.json` 里有 warning 指出本线若干 check 的 nominal 与 variant 输出逐字节相同、
「the perturbation never took effect」。**这些是准确的**：`ic/variant` 与 `ic/nominal` 确实
不同，扰动施加了，只是没越过任何判定边界。相关 rubric 的 `variant` 字段已补记这是
**测出来的零**而非声明的 identical，并明说不应通过把字段改写成 `identical` 开头来消掉警告。

## 2026-09-13：五条原语 check 补上第三条腿（此前全 leaf 为零）

### 先说发现

复查时量了一遍：**本 leaf 23 条 check 的判分器，没有一个做独立复算**——全部是
reference 与 candidate 的两侧比较。按「两侧同污染」的道理，参考侧本身错了就发现不了；
一个错误的 oracle 会和一个同样错误的候选互相通过。

23 条里有一批是**数学原语**（weighted cosine / jaccard / product / 归一化乘积 /
局部双变量 Moran），它们的实现在 `_local_functions.py` 里各只有两到六行，
是最该有第三条腿、也最容易加的。本轮先做这五条。

### 做法

判分器里新增 `recompute(ic_dir)`：从 `ic/<名>/inputs.npz` 读 `x`、`y`、`weight`、
`spots`、`pairs`，**用纯 Python 的显式三重循环**做矩阵乘——**不经 BLAS、不调 liana**，
numpy 只用来解析 .npz。

selfcheck 的计分是跨 IC 的（reference=nominal、candidate=variant），所以对两个已提交
IC 各算一份期望、任一份通过即可。实测两份期望本身只差 **2.07e-09**（atol 的 2e-4），
这条「任一」不会放松判分。

### 实测余量

| check | 重写的函数 | 与真实产物最大绝对差 | 占 atol=1e-5 |
| --- | --- | --- | --- |
| weighted-cosine | `_vectorized_cosine` | 7.51e-08 | 0.75% |
| weighted-jaccard | `_vectorized_jaccard` | 7.32e-08 | 0.73% |
| normalized-weighted-product | `_norm_product` | 2.16e-07 | 2.2% |
| local-morans-primitive | `_local_morans` | 1.67e-06 | **16.7%** |
| weighted-product | `_product` | 2.08e-06 | **20.8%** |

后两条占掉约五分之一容差——它们的值量级更大、float32 累加的绝对误差随之变大。
仍在容差内，但这个余量请人工在定最终 bound 时一并看。

### 鉴别力是验过的，不是声称的

五条各做了一次**两侧同污染**：把同一个值在两边同样改掉 1e-2，判决里
`over_bound` 仍为 **0**（逐项比较结构上拒不掉），而第三条腿把两侧都拒下。
自检里有一条用例把这两件事一起断言。

### 顺带修掉的一处缺陷

这五条的 `test_validate.py` 原先都用一张**编造的 2×2 表**做基线。那种基线只能测比较
逻辑自己跟自己，连「基线是否正确」都验不了——接上第三条腿后，每条立刻失败 4 条用例。
已全部改为从 `ic/nominal` 独立重算出真实的 20×5 表做基线。少数**协议**用例
（略大于 1 的合法浮点、无界乘积不被截断到 [-1,1]）的期望相应改为
「走正常比较路径被判不一致」而非「通过」——它们验的是**不得硬拒**，这一点仍被断言。

五条的完整测试套件分别 31 / 29 / 29 / 30 / 29 通过；
`sab.py task lint` 仍为 23 check / 0 error / 0 warning，`validate-harbor` PASS。

### 仍未覆盖

余下 18 条仍无第三条腿。下一批的自然目标是 `weighted-pearson`、`weighted-spearman`、
`masked-spearman`——它们共用 `_vectorized_correlations`，比上面五条长，且含一处
`denominator <= 1e-6 * ss` 的不稳定性守卫与一次 `np.clip(-1, 1)`，重写时必须照抄。

## 2026-09-13（续）：相关系数一族补齐，8 / 23 有第三条腿

新增 `weighted-pearson`、`weighted-spearman`、`masked-spearman`，各 100/100。

| check | 重写的函数 | 与真实产物最大差 | 占 atol=1e-5 |
| --- | --- | --- | --- |
| weighted-pearson | `_vectorized_correlations(pearson)` | 5.93e-08 | 0.6% |
| masked-spearman | `_masked_spearman` + `_wcorr` | 1.91e-07 | 1.9% |
| weighted-spearman | `_vectorized_correlations(spearman)` | 1.33e-06 | **13.3%** |

### 两支相关系数实现不是同一套算法，重写时逐处照抄

`_vectorized_correlations` 与 `_masked_spearman`+`_wcorr` **有三处实质差异**，
照着其中一支写另一支就会误拒：

| | 向量化支 | masked 支 |
| --- | --- | --- |
| 排秩 | `rankdata(axis=0)`，1 起、并列取**平均**秩 | `argsort().argsort()`，0 起、并列按**原位置** |
| 不稳定性守卫 | `denominator <= 1e-6 * ss` 置零 | **没有** |
| 零判据 | `np.divide(where=denominator!=0)` | `denominator == 0 or numerator == 0` |

两支末尾都 `clip(-1, 1)`。`masked` 支每行只在 `w > 0` 的列上算，`wsum` 也只累加这些列。

### ⚠ 一处我自己引入又抓出来的缺陷

给七条 check 批量追加两条新用例时，追加落到了文件末尾 `if __name__ == '__main__'`
**之后**——语法合法、pytest 照常报「18 passed」，但那两条**根本没运行**，是死代码。

第一次检查时我用了 `pytest -q ... -v`，`-q` 把逐条输出压掉，于是我量出「0 条运行」，
连 `weighted-cosine`（实际是好的）也被误判。**两次测量都错，方向相反。**
改用 `pytest -v` 单独跑一次后拿到准确结果：只有手工改的 `weighted-cosine` 是对的，
其余七条全是死代码。已全部移回类内，现八条均 2/2 运行且通过
（20/20/20/20/20/19/19/19）。

教训：**「N passed」不能证明你新加的用例跑了**，要按名字确认；而确认用的命令本身
也要先验证它对已知为真的情形能给出正确答案。

`sab.py task lint` 仍为 23 check / 0 error / 0 warning。仍有 15 条无第三条腿。

## 2026-09-13（三）：expand-coordinates-grid 补上第三条腿，9 / 23

`utils/expand_coordinates.py` 是**纯几何**，判分器用纯 Python 逐句照抄即可：
按样本平移到各自原点、取各轴最大 extent 乘 `(1 + margin)` 作格宽高、再按
`col = i % n_cols`、`row = i // n_cols` 平移；`n_cols` 缺省 `ceil(sqrt(样本数))`；
样本类别取自 `pd.Categorical`，categories 是**排序**的。

**六个 graded 文件、1800 个分量与真实产物逐位相同（最大绝对差 0.0）。** float64
纯平移没有归约次序歧义，所以这条腿是**精确**的，不是近似的——这也是目前唯一一条
gap 为零的第三条腿。两侧同污染已实证能拒（`over_bound=0`，只有第三条腿响）。

### 自检改造：与前八条不同的做法

前八条 check 的自检只要把基线换成真实 fixture 的重算表就行。这条不行——它的用例
硬编码了 `spot-0` 这类身份名和 `100.25` 这类数值，换成真实 150 点 fixture 会失败 18 条。

改成：**自检自己造一个 3 点的小 IC**，用 `inputs_root` 指过去，基线取判分器对这个小 IC
的重算结果。用例仍跑在小数据上（快、自足），第三条腿也被真正演练到。硬编码的数值一并
改为从真实行派生（`rows[2][1] + 1e-10` 而不是 `100.25 + 1e-10`），计数断言改为
`2 * SPOTS * len(FILES)`。现 29/29 通过。

两条新用例已**按名字**确认确实被收集运行——上一格刚栽在这上面。

`sab.py task lint` 仍为 23 check / 0 error / 0 warning。仍有 14 条无第三条腿。

## 2026-09-13（四）：Moran 解析 p 值两条，11 / 23

`global-moran-analytic-pvalues`（10/10）与 `local-moran-analytic-pvalues`（100/100），
**与真实产物最大绝对差均为 1.11e-16，即机器精度**（atol=1e-10，余量六个数量级）。

两者都是纯解析式，判分器逐句照抄即可，`norm.sf` 用 `math.erfc(z/√2)/2` 等价实现，
**不调 liana 也不用 scipy**：

- 全局（`_global_functions.py:_zscore_pvals`）：
  `z = global_stat / sqrt((n²·Σ(W∘W) − 2n·Σ(W@W) + (ΣW)²) / (n²(n−1)²))`
- 局部（`_local_functions.py:_zscore_pvals` + `_get_local_var`）：
  `sigma = norm.fit(列)[1] · n/(n−1)`（`norm.fit` 给的是 MLE，即 **ddof=0** 的总体标准差），
  `core = 2(n−1)²/n² · sigma_x · sigma_y`，`var = rowsum(W²) ⊗ core + core`，
  `p = norm.sf(local_truth / sqrt(var))`

两条均已实证能拒两侧同污染。

### 自检改造中两处自找的麻烦

1. 我写的新用例用了 `self.reference`，而这两个文件的属性名是 **`self.ref`**——
   于是「污染 reference」那一步静默失效，判决里只有 candidate 被拒。是断言
   `third_leg_failures == ['reference','candidate']` 把它抓出来的；若只断言
   `passed is False`，这个错会一直藏着。
2. 原有的 roundoff 用例硬编码了 `0.2 + 1e-11`，而真实 p 值是 0.1817——换成真实基线后
   那就成了 0.018 的大改动。已改为 `ROWS[0][1] + 1e-11`。

现 22/22 与 28/28 通过，新用例均**按名确认**被收集运行。

### 一条评估结论：spatial-neighbor-kernels 本轮不做

`utils/spatial_neighbors.py:147` 用 `sklearn.neighbors.NearestNeighbors`
以 `n_neighbors = max_neighbours + 1` 做 k-NN 截断。纯 Python 重算 700×700 距离可行，
但**第 100 名处存在并列风险**：一旦有并列，取哪一个取决于实现的排序稳定性，
照抄不来。要做的话得先实测这份 fixture 上第 100/101 名的距离间隔，确认没有并列，
并把这个前提写进 rubric。留给下一轮。

`sab.py task lint` 仍为 23 check / 0 error / 0 warning。仍有 12 条无第三条腿。

## 2026-09-13（五）：spatial-neighbor-kernels，12 / 23

上一格我把这条**暂缓**了，理由是 sklearn 的 k 近邻截断可能有并列、照抄不来。
本格先把那个前提**量出来**再动手：

> 这份 fixture 的 700 行里，第 101 名与第 102 名的距离间隔**最小 9.42e-03，无一行为 0**。

没有并列，于是暴力精确排序与 ball_tree 选出的邻居集合必然相同。**判分器把这个前提
写成了运行时检查**——`recompute` 对每一行重新比较第 k / 第 k+1 名，一旦并列就直接报错，
而不是给出一个可疑的判决。

**51936 / 51936 全部有第三条腿，七个配置的边集与真实产物完全一致、最大绝对差
1.11e-16**，整次判分约 1.8 秒。纯 Python 重写 `spatial_neighbors:147-166` 的整条流程，
不调 liana 也不用 sklearn。几处容易写错的地方：

- k 近邻是 `max_neighbours + 1`，**含自身**（自身距离 0）；
- `cutoff` 是 `data * (data > cutoff)`——**乘零而不是删除**，严格不大于 cutoff 的项变 0
  后才被 `nonzero()` 丢掉；
- `standardize` 是按行 **L1** 归一；
- `spot_n > 1000` 才转 float32，本 fixture 700 个点，全程 float64。

### 自检：第三次遇到合成基线，第二次用「自造小 IC」

真实 fixture 700 点 5 万多条边，拿它当自检基线每条用例都慢。仍用自造小 IC（4 点）+
`inputs_root`。另修正四处硬编码，其中一处值得记：

`test_direction_is_part_of_the_identity` 交换 `(a, b)` 后期望被拒——但**高斯核作用在
对称距离上本身就是对称的**，交换后完全一样，这条用例在新基线下根本测不到方向性。
改用按行 L1 归一的那个配置（它非对称）才真正验到。现 30/30，新用例按名确认运行。

`sab.py task lint` 仍为 23 check / 0 error / 0 warning。仍有 11 条无第三条腿。

## 2026-09-14：lric-primitives，LIANA+ 13 / 23

**135 / 135（128 个数值 + 7 个字符串输出），最大绝对差 0.0，atol=1e-12 下 0 超界。**

八个 helper 全部用 numpy 重写，**不 import liana，也不 import scipy**——
`_support_edge_list` 不建 cKDTree，3 个点直接算两两距离。两处按源码钉死：
距离先转 **float32**（`spdm.data.astype(np.float32)`），`max_distance` 是**闭**区间。

字符串输出（`_to_dense` 的 dtype、`_index_resource` 的 interaction 名）也有独立期望，
不是只跟参考比——所以两侧同改 dtype 也拒得掉。

### 自检基线换掉了

原来那份手编的 4 数值 / 2 标签小表在判分器带第三条腿之后就用不了了：`recompute`
无论初值是什么都给出同一套 19 个 case 的身份集合，手编小表永远对不上，于是
**每一条「合法情形应通过」的用例都会变成假阳性**。基线改成判分器自己对 `ic/nominal`
的独立重算，破坏点改成按 `(case, key)` 锚点定位而不是行号（行号会随重算内容漂，
锚点找不到时直接 `AssertionError` 提示更新，不会静默跑偏）。

31 条用例全过（含三条新增），`test_math.py` + `test_protocol.py` 另 18 条全过；
lint 23 check / 0 error / 0 warning。

两条负控确认没变空转：`test_representation_roundoff_passes`（+1e-14 仍通过）证明第三条腿
不是在无差别拒绝；`test_value_change_beyond_the_bound_fails` 断言 `bound_fraction > 1`，
那个量在第三条腿之前算出，证明它走的仍是逐值比较那条路。

## 2026-09-14：scseqcomm-interaction-score，LIANA+ 14 / 23

**29400 / 29400（4200 个 LR 身份 × 7 列），0 超界，最坏占界 10.3%。**
**不 import liana、不 import scipy**，只用 numpy 与 `math.erfc`。判分器耗时 1.26 s。

| 列 | 最大绝对差 | 占界 |
|---|---|---|
| `ligand_props` / `receptor_props` | 0.0（逐位相同） | 0% |
| `ligand_means` / `receptor_means` | 8.93e-07 | 3.1% |
| `ligand_cdf` / `inter_score` | 3.22e-07 | 10.3% |
| `receptor_cdf` | 1.85e-07 | 7.9% |

### 刻意不调 scipy 的稀疏 `.mean`

实测 scipy 稀疏 `.mean(axis=0)` 能让 `*_means` **逐位相同（0.0）**——但那正是被测
流水线调用的**同一个原语**，调它就不算独立验证了。改用纯 numpy 的 float64 累加，
接受 8.93e-07 的求和次序差（占界 3.1%）。

### ⚠ 一处上游缺陷，报给人工

`_reduce_complexes` 的文档与代码都说复合体归约到「**最小**表达 subunit」，
但 `return_all_lrs=True` 时 `_filter_reassemble_complexes` 会在归约**之前**执行
`lr_res.drop_duplicates(subset=_key_cols, inplace=True)`，每个 key 只剩行序上的第一行，
`_reduce_complexes` 随后变成**空操作**。

实测这份 fixture 的 200 个复合体条目：**「第一个 subunit」200/200 全中，
「最小 subunit」只中 80/200**，两者在 `*_means` 上最大差 **4.27**。

判分器必须复现**实际**行为，不能复现文档写的那个。更值得注意的是：留下哪一行取决于
pandas 的 merge/groupby **行序**——实现细节而非文档保证的次序。与之前报的
`np.argpartition(coord_sum, 2)[:2]` 和 k 近邻并列是同一类问题。
自检 `test_complex_uses_the_first_subunit_not_the_smallest` 把它钉住，并带一条
「R 与 Z 均值相同就是空转」的自我保护断言。

自检 28 条全过（含三条新增，按名确认），`test_math.py` + `test_protocol.py` 另 13 条全过；
lint 23 check / 0 error / 0 warning。

## 2026-09-14：lric-pairwise，LIANA+ 15 / 23

**5012 / 5012 有限值全覆盖，0 超界，最坏占界 1.2%**；另外 **1738 个 NaN 的位置也独立推导**，
不是只跟参考比。三条曲线按 `_LRIC.py:860-930` 的定义用 numpy 重写，
**不 import liana、不建 cKDTree**——700 个点直接算两两距离，落在 120 px 内的边只有 882 条。

| 列 | 最大绝对差 | 占界 |
|---|---|---|
| `g` | 3.05e-05 | 1.2% |
| `g_expr` | 1.91e-06 | 1.2% |
| `g_pcf` | 8.88e-16 | ~0 |

`g` 的绝对差看着大是因为它的量级大——**占界才是该看的量**。判分器耗时 1.22 s。

重现的链条：`fine tiles → roll → T / T_SR / Num_SR → exp_T → 三条曲线`，
包括 `extend_first_annulus` 把第一环并到 0 起、分母为 0 处置 NaN、末尾统一收 float32。
权重 `W = _linear_transform(_to_dense(X[:, 唯一基因]))[:, idx]`——**先转 float32
再按列均值归一**，次序按源码。

### 自检的合成初值是挑过的

8 细胞 / 2 基因 / 2 个 cell type，坐标特意安排成让三种定义域都出现：全有限、整行 NaN、
以及**只有 `g_expr` 是 NaN** 的混合行（环 [60,80) 里只有一条 A-A 边、没有 A-B 边，
于是 `T_SR = 0 < T`）。三个用途固定的行**按性质找**而不是写死行号，找不到就直接
`AssertionError`，不会静默空转。

新增的两条污染用例中，第二条是这条腿的独有价值：**把一个 NaN 的 `g_expr` 两侧同改成 0.0，
NaN 位置两侧一致、逐值比较放行**，仍由第三条腿拒下——因为定义域本身也是独立推导的。

自检 23 条全过（含三条新增，按名确认），`test_protocol.py` 另 8 条全过；
lint 23 check / 0 error / 0 warning。

## 2026-09-14：lric-parameter-branches，LIANA+ 16 / 23

**416 / 416（405 个 `g` + 11 个分支行数），0 超界。** 不 import liana、不建 cKDTree。
五个 `cross_pcf` 分支最大绝对差 ≤ 8.88e-16；六个 agnostic `lric` 分支 ≤ 1.19e-07
（atol=1e-6，占界 < 12%）。**NaN 定义域与行数也独立推导**。判分器 1.48 s。

### 三处按实测钉死、没有按惯例假定

1. **`cell_types` 与 `groupby_pairs` 会把支撑集本身子集化**——只用涉及到的类型的细胞
   重算 N 与 T。用全部细胞算 `pcf-groupby-pairs` 实测差 **0.66**；只用那三种类型算
   差 **1.1e-16**。初版我按「支撑集是全体细胞」写，就是被这个数字证伪的。
2. 输出的类型对是**无序**的、按 level 排序归一——请求 `(B, A)` 会以 `(A, B)` 出现。
3. `extend_first_annulus=False` 时第一环不并到 0 起，半径从 20 开始。

行数是独立推的：`cross_pcf` 给 `C(存活类型数, 2) × 环数`（`min_cells=None` →
`floor(0.01 N) + 1`），`groupby_pairs` 给 2 对，agnostic 给 `LR 对数 × 环数`。

### 自检的合成初值也是挑过的

40 细胞 / 2 基因 / 3 个 cell type。规模特意选成**没有任何类型达到 200 个细胞**，
于是 `pcf-min-cells-200` 仍是空分支，原来那条「空分支只能靠 support 的 0 钉住」的
用例得以保留；`lric-expr-prop-high` 提供 NaN 定义域。用途固定的三行按性质找而非写死行号。

新增的第二条污染用例是这条腿的独有价值：**把一个分支的行数在两侧同样 +5 并补上对应的行**，
`support.csv` 两侧一致、逐值比较放行，仍被第三条腿拒下——行数本身也是独立推导的。

自检 33 条全过（含三条新增，按名确认），`test_math.py` + `test_protocol.py` 另 21 条全过；
lint 23 check / 0 error / 0 warning。

## 2026-09-14：lric-curve-summaries，LIANA+ 17 / 23

**842 / 842 全部逐位相同（0.0）**——410 行 AUC 的 score 与 peak_radius、6 个支持计数、
4 个散度 case 的三个数与 direction 标签。本 check 的输入**本身就是曲线**，
所以两个 helper 是它们的纯函数，用 numpy/pandas 重写即可，**不 import liana**。
判分器 0.45 s。

重现要点：`transform_fn` 默认 `log2(max(g, 0.05))`；梯形积分**只在相邻两格都 keep 的
区间上**累加；`span = 最大 keep 半径 − 最小 keep 半径`；`keep 计数 < min_bins` 或
`span == 0` 的 interaction 整条丢弃——`support.csv` 数的就是活下来的条数。
两处按源码钉死：`g` 先以 **float32** 读入再升 float64；`zeroed()` 与 `conditions`
两个派生表按**字典序**而不是存储顺序挑 interaction。

### 三条污染用例，后两条是这条腿的独有价值

1. AUC `score` 两侧同 +0.01 → 逐值比较 0 超界，第三条腿拒下。
2. **`direction` 是字符串标签**——比较只看两侧是否相同，两侧同样翻转就放行；
   只有独立期望能拒。
3. 支持计数两侧同 +1，同理。

自检 36 条全过（含四条新增，按名确认），`test_math.py` + `test_protocol.py` 另 19 条全过；
lint 23 check / 0 error / 0 warning。

**踩到一次并修掉**：`load_support` / `load_divergence` 的键是 `(case,)` **元组**，
我的重算一开始用裸字符串，第一次跑就被自己的「支持计数与独立重算不符」拒下。
这正是那条腿该有的样子——它先抓住了我自己。

## 2026-09-14：cross-pcf-and-agnostic-lric，LIANA+ 18 / 23

**480 / 480 全部命中，0 超界。** 三个 `cross_pcf` case 逐位相同（0.0），
`lric-agnostic` 最大绝对差 5.96e-08（atol=1e-6）。不 import liana、不建 cKDTree。
NaN 定义域同样独立推导。判分器 1.40 s。

**唯一新增的一块是 `annulus_steps=2`**：输出环**重叠**（每环覆盖两个 fine tile），
`roll` 的前缀和窗口宽度随之变成 2，但输出半径仍是 `[0, 40, 60, 80, 100]`。
另两处沿用前两格的同源结论：`cell_types` 会把支撑集本身子集化；类型对无序、按 level
排序归一。

自检合成初值坐标做成两簇（近簇 0–30、远簇 200 附近），好让中间的环空掉、产生 NaN
定义域；计数断言改成从 `ROWS` 导出，并显式断言「两类都有」，防止定义域那几条用例空转。

自检 30 条全过（含三条新增，按名确认），`test_math.py` + `test_protocol.py` 另 16 条全过；
lint 23 check / 0 error / 0 warning。

## 2026-09-14：inflow-parameter-branches，LIANA+ 19 / 23

**22442 / 22446（99.98%），0 超界，最坏占界 2.1%。** 18463 个非零 inflow 值、
3975 个每列摘要、4 个 case 的非零计数全部独立重算，不 import liana。判分器 1.51 s。

三处按源码钉死：**复合体列是各 subunit 的逐细胞元素最小值**（`CD74_CXCR4` 不是原始
基因，直接查名字会 KeyError，这是原型第一次跑挂的地方）；`transform-clip` 一支用
`use_raw=False`；`zi_minmax` 的列 min/max **含隐式零**、只缩放非零项、缩放后 < 0.5 置 0。

### ⚠ 这条腿是部分的，原因是结构性的

`consensus` resource 表**不在 `ic/` 里**（`ic/` 只有 `expression.h5ad`），所以
「哪些 LR 对存在」无法独立枚举，列集合只能取自受判身份轴。对它只做**单向**检查
（两侧 `nz_prop` 过阈、列方差 > 0）——**抓得住多出来的列，抓不住漏掉的列**。
4 个 case 的列数因此不计入覆盖（22446 − 4）。要补全需把 resource 冻进 `ic/`，那是改 IC。

自检 38 + 20 条全过；lint 23 check / 0 error / 0 warning。

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

本 leaf 的墙钟：23 条 check 从串行 ~370 s 降到 **81 s**（4.6×）。
survey 50 条 / 40 suitable / 23 已实现。⚠ 记一条易错点：**`plotting/` 下并非都不适合**——`test_circle_plot.py` 与 `test_connectivity_plot.py` 判的是绘图前算出的邻接均值与连通度数值，已标 suitable。按目录名一刀切会判错。
