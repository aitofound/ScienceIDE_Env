# pipeline/ —— 数据目录 + 兼容壳(引擎不在这里)

> 工作流的执行入口是两个 advisory CLI:`scripts/codebase_cli.py`(代码库 → 人批 manifest)
> 与 `scripts/task_cli.py`(manifest → 人批可合并的 PR)。状态机、机械门、⛔ 人批、
> override 纪律都在那两个文件里;架构与契约见 `references/two-cli-architecture.md`,
> 操作说明见 `SKILL.md`。本目录只剩三样东西:

| 内容 | 是什么 |
|---|---|
| `intake/<repo>.toml` | 派工单(人写,git 管):`code_path` / `pin` / `notes`。样例见 `intake/laps.toml.example` |
| `manifests/<codebase>/<task>.manifest.json` | 人批 task manifest(`codebase_cli.py emit-manifest` 写,git 管)—— 两个 CLI 之间唯一的正式接口 |
| `pipe.py` / `manifest.py` | **兼容壳**:老命令/老 import 的转发层,没有任何工作流逻辑 |

## 老命令 → 新命令

`pipe.py` 仍接受老参数,翻译后原样转发(转发时会打印等价命令;`--leaf` 自动解析成该
leaf 绑定的 manifest 路径):

```bash
pipe.py status                        → codebase_cli.py status
pipe.py status --leaf <slug>          → task_cli.py status --manifest <m>
pipe.py advance --repo <r>            → codebase_cli.py decompose --codebase <r>
pipe.py advance --leaf <slug> [...]   → task_cli.py advance --manifest <m> [...]
pipe.py approve --repo <r> --what cut → codebase_cli.py approve-cut --codebase <r>
pipe.py approve --leaf <slug> --what tests|tolerance|custom-check
                                      → task_cli.py approve --manifest <m> --what …
pipe.py approve --leaf <slug> --what ship
                                      → task_cli.py approve-mergeable --manifest <m>
pipe.py reject / baseline / verdict   → task_cli.py reject / baseline / status
```

`pipe.py run`(无人值守调度器:自动扇出派活、资源熔断、单实例守卫)已随第三层引擎
移除:两-CLI 契约是操作员按 `next_action` 一步步推进单个任务、在 ⛔ 处停下来等人,
不是批量无人驱动。要并行就开多个操作员会话,每个盯一个 manifest。

## 控制面在哪(不变)

**不在 repo 里**,在 `~/.sciaccel_pipeline/`(env `SAB_PIPE_DIR`;AI 工序不被指到这里):

```
journal.jsonl   append-only 事实记录:每次门/工序/审批/驳回/override/PR + 每条 CLI 命令
inbox/          AI 产物投递处(分解提案)—— 是数据不是控制状态,一律当不可信输入
logs/           每个 AI 会话的完整 transcript + prompt 快照
running/        在飞工序标记(PID 存活判据)
skill_baseline.sha
```

journal 的记录形状(`gate_*` / `approval` / `rejection` / `cut` / `override` / `pr` /
`cli_action` …)与旧引擎完全一致 —— 旧 journal 原样有效,状态永远从 journal + 磁盘
推导,没有需要手工维护的状态文件。

## 纪律(不变)

1. 改任何判据脚本后:`python3 tests/checker_calibration.py` 必须全绿。
   **写完新守卫,第一件事是给它加正负用例证明它真的会报警。**
2. 改 skill/pipeline 代码后:`task_cli.py baseline --update`(否则 advance 拒绝工作)。
3. 新流程先在 1–3 个 leaf 上串行走完全程,再扇出。
4. **闸门变红是正确结果**:溯源红=测试来源没锚,容差红=证据缺口 —— 是工单不是故障。
   绝不允许为了变绿去删检查、放松容差、或给 custom 测试伪造 upstream 声明。
5. 存量 tasks/*(athena-*、pluto-*)不进状态机(2026-08-29 拍板);
   要纳管时给它们补 provenance/rubric 再经正常门。
