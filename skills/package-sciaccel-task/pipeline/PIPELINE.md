# PIPELINE.md —— sciaccel 打包流水线的唯一执行入口

> 本文件是操作规程;`pipeline/pipe.py` 是执行本规程的代码。
> **两者冲突时,以代码为准,并把冲突当 bug 修。**
> SKILL.md 是 AI 操作员入口(扔给 Claude Code 即可代为操作本管线);
> references/(含原 SKILL.md 散文 authoring-doctrine.md)是知识库,不是操作入口。
> 形制照抄 `~/ale/design_pipe_skill`:确定性状态机驱动窄工序 AI,
> 完成判定由代码做,改动靠内容指纹自动作废下游。

## 一张图

```
              (AI 提案)                 ⛔人批 cut
  派工单 ──decompose──▶ PROPOSED ──approve──▶ CUT_APPROVED ──┐ 每模块一个 leaf,可并行
                                                            ▼
        (AI)             docker真build          (AI遴选官方测试)   溯源门(确定性)
  MISSING ─scaffold─▶ SCAFFOLDED ─gate_env─▶ ENV_OK ─curate─▶ ─gate_tests─▶ TESTS_OK
                                                                              │⛔人批 tests
        (AI实测噪声)      证据门(确定性)      ⛔人批 tolerance                    ▼
  ◀────tolerance────  TESTS_APPROVED ◀───────────────────────────────────────┘
      ─▶ ─gate_tol─▶ TOL_OK ──approve──▶ TOL_APPROVED ─finalize(AI)─▶
      ─gate_final(validator+溯源+容差+真实自证)─▶ PACKAGED ──⛔人批 ship──▶ READY

  任何门红/人驳回 ──▶ fix(AI,只修点名条目) ──改动→域指纹变→该域门+审批自动作废
```

对应五步流程:①拆分=decompose+cut;②docker=scaffold+gate_env;
③复用官方测试=curate+gate_tests+人批;④容差=tolerance+gate_tol+人批;
⑤提任务=finalize+gate_final+人批 ship。

## 命令

```bash
cd ~/ScienceAccelBench/skills/package-sciaccel-task/pipeline

python3 pipe.py status                        # 全库状态(从磁盘+journal 推导,无缓存可骗)
python3 pipe.py run --max-ai 3 --max-gate 2   # 调度器:自动派活到只剩人类门
python3 pipe.py advance --repo laps           # 手工:仓库级 decompose
python3 pipe.py advance --leaf <slug>         # 手工:推进一个 leaf(该干什么它自己知道)
python3 pipe.py approve --repo laps --what cut [--leaves a,b]      # ⛔人跑
python3 pipe.py approve --leaf <slug> --what tests|tolerance|ship  # ⛔人跑
python3 pipe.py approve --leaf <slug> --what custom-check --check <name>
python3 pipe.py reject  --leaf <slug> --what tests --reason "…"    # 驳回→fix 吃理由
python3 pipe.py verdict --leaf <slug>
python3 pipe.py baseline --update             # 自己改了 skill 代码之后
```

派工单:`pipeline/intake/<repo>.toml`(人写),字段 `code_path` / `pin` / `notes`。

## 六个 AI 节点(其余一切都是确定性代码)

| 节点 | 输入 | 产出 | 完成判定(由代码做) |
|---|---|---|---|
| decompose | pinned 代码库 + 派工单 | inbox 里的分解提案 JSON | schema 齐 + ⛔人批 cut |
| scaffold | 已批模块工单 | leaf 骨架 + 两张 Dockerfile + solve.sh | docker 真 build 通过 |
| curate | 模块工单 + code/ | checks + provenance.json | 溯源门:sha256 对上官方测试 + ⛔人批 |
| tolerance | oracle 重复实测 | 每 check 的 rubric.json | 证据门:tolerance≥实测噪声 + ⛔人批 |
| finalize | 全包 | 题面/manifest/target/reward 接线 | validator+自证(solve→test=1.0)+ ⛔人批 |
| fix | 门失败详情+驳回理由 | 点名条目的修复 | 指纹变→该域门/审批自动重走 |

默认后端 codex `gpt-5.6-sol@high`(`--ai claude` 走 sonnet 无头;
env:`SAB_CODEX_MODEL` / `SAB_CLAUDE_MODEL`)。

## 机制对照表(痛点 → 机制 → 代码位置)

| 痛点/教训 | 机制 | 在哪 |
|---|---|---|
| AI 不 follow 大而全的 skill 散文 | 流程收进状态机,AI 每次只拿一份窄任务书 | pipe.py + prompts/ |
| **AI 总喜欢自己加 test** | 溯源门:每 check 带 provenance.json,upstream 按 sha256 对到 code/ 内官方测试;custom 必须人逐个批(绑 check 目录哈希) | scripts/check_test_provenance.py |
| 容差拍脑袋 | 证据门:tolerance ≥ 实测 oracle 噪声;exact 要零噪声证据;repeats≥2;松 1e3 倍打 warn | scripts/check_tolerance_spec.py |
| 人类审批被「顺手」绕过 | approve 只能人跑;记录绑**域指纹**,AI 改一个字节审批自动失效 | pipe.py approval/fp_* |
| 改容差不该作废测试选择的审批 | 指纹分四域:fp_env / fp_tests / fp_tol / fp_all | pipe.py fp_* |
| 「跑过了」是嘴上说的 | gate_env 真 build;gate_final 真跑 solve.sh→test.sh 读 reward 文件 | scripts/docker_env_gate.py, selfpass_gate.py |
| 门内工序污染指纹域(演练实测:oracle 产物写进 solution/ → 门刚绿就被自己作废) | 生成物目录(solution/oracle_out/ 等)不进指纹 + 门出口做 fingerprint-drift 对照 | pipe.py _GENERATED_DIRS/_gate |
| tolerance/finalize 动笔就作废 tests 审批(演练实测的死锁) | 后写域文件(rubric.json/check.json)不进 fp_tests 与 custom-check 目录哈希 | pipe.py _LATER_STAGE_FILES |
| 检查器自己坏了没人知道 | 15 用例正负对照,任何门先跑校准,失败拒绝出数 | tests/checker_calibration.py |
| agent 改验收代码 | skill SHA 基线,advance/run 前必验 | pipe.py skill_baseline |
| mtime 对账出错 | 记录带内容指纹,指纹不符自动作废 | pipe.py _latest |
| 盲改循环烧配额 | (leaf,action) 派发上限 + 连败 2 次停派待人 | pipe.py cmd_run |
| 配额失败误计连败 | 零工作量(rc≠0+指纹未变+日志小)退 75 不计派发 | pipe.py stage_fix/_dispatch_ai |
| 双调度器/资源雪崩 | 单实例守卫(只认 python 进程防自匹配)+ 内存/磁盘熔断 | pipe.py cmd_run/_resources_ok |

## 控制面在哪

**不在 repo 里**,在 `~/.sciaccel_pipeline/`(env `SAB_PIPE_DIR`;AI 工序不被指到这里):

```
journal.jsonl   append-only 事实记录:每次门/工序/审批/驳回
inbox/          AI 产物投递处(分解提案)—— 是数据不是控制状态,一律当不可信输入
logs/           每个 AI 会话的完整 transcript + prompt 快照
running/        在飞工序标记(PID 存活判据)
skill_baseline.sha
```

状态永远从 journal + 磁盘推导,没有需要手工维护的状态文件。

## 纪律

1. 改任何判据脚本后:`python3 tests/checker_calibration.py` 必须全绿。
   **写完新守卫,第一件事是给它加正负用例证明它真的会报警。**
2. 改 skill/pipeline 代码后:`pipe.py baseline --update`(否则 advance 拒绝工作)。
3. 新流程先在 1–3 个 leaf 上串行走完全程,再扇出(照 ALE 的 tosolve #4 纪律)。
4. **闸门变红是正确结果**:溯源红=测试来源没锚,容差红=证据缺口 —— 是工单不是故障。
   绝不允许为了变绿去删检查、放松容差、或给 custom 测试伪造 upstream 声明。
5. 存量 tasks/*(athena-*、pluto-*)不进本状态机(2026-08-29 拍板);
   要纳管时给它们补 provenance/rubric 再经正常门。
6. ⚠️未验证假说(承认,别当结论用):容差「松 1e3 倍」的 warn 阈值未标定;
   docker 门未做镜像体积/网络纪律;调度并发默认值(3 AI + 2 gate)未在本机满载实测。
