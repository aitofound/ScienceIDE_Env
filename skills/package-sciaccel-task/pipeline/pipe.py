#!/usr/bin/env python3
"""pipe.py —— 老命令的兼容转发壳(引擎已并入两个 advisory CLI,这里不再有工作流)。

    老命令(本壳仍接受)                          现在的等价命令
    ------------------------------------------  ------------------------------------------------------
    pipe.py status                              codebase_cli.py status
    pipe.py status --repo R                     codebase_cli.py status --codebase R
    pipe.py status --leaf L                     task_cli.py status --manifest <该 leaf 绑的 manifest>
    pipe.py advance --repo R                    codebase_cli.py decompose --codebase R
    pipe.py advance --leaf L [--stage X …]      task_cli.py advance --manifest <m> [--stage X …]
    pipe.py approve --repo R --what cut         codebase_cli.py approve-cut --codebase R
    pipe.py approve --leaf L --what tests|      task_cli.py approve --manifest <m> --what …
                    tolerance|custom-check
    pipe.py approve --leaf L --what ship        task_cli.py approve-mergeable --manifest <m>
    pipe.py reject  --leaf L --what W           task_cli.py reject --manifest <m> --what W
    pipe.py baseline [--update]                 task_cli.py baseline [--update]
    pipe.py verdict --leaf L                    task_cli.py status --manifest <m>(status 已含 verdict 信息)
    pipe.py run                                 (已移除 —— 见下)

为什么:PR319 复审(Jason Telegram6162/6164/6168)裁定两个 CLI 必须自己拥有工作流,
不许再包着第三层引擎。工序/机械门/⛔ 人批/override 纪律现在都在 scripts/task_cli.py
与 scripts/codebase_cli.py 里;两 CLI 共享的数据原语(manifest 契约、路径、四域指纹、
append-only journal、状态推导)在 scripts/_shared.py。journal 记录形状不变,旧
journal 原样有效。`run` 调度器(自动扇出派活、资源熔断、单实例守卫)随第三层一并
移除:两-CLI 契约是操作员按 next_action 一步步推进单个任务,不是无人值守批量驱动。
要并行就开多个操作员会话,每个盯一个 manifest。

本壳只做两件事:把老参数翻译成新命令(--leaf 借 _shared 解析成绑定的 manifest 路径),
然后原样转发并透传退出码。没有状态机、不写 journal、不派 AI。

pipeline/ 目录本身现在只剩三样东西:`intake/<repo>.toml`(派工单,人写,git 管;
codebase_cli.py locate 写出的形状是 `code_path = "<pinned 快照路径>"`,可选
`pin = "<commit-sha 或快照说明>"`、`notes = "…"`)、
`manifests/<codebase>/<task>.manifest.json`(人批 task manifest,codebase_cli.py
emit-manifest 写,git 管 —— 两个 CLI 之间唯一的正式接口)、本文件(兼容壳)。

控制面不在 repo 里,在 `~/.sciaccel_pipeline/`(env `SAB_PIPE_DIR`;AI 工序不被指
到这里):

    journal.jsonl   append-only 事实记录:每次门/工序/审批/驳回/override/PR + 每条 CLI 命令
    inbox/          AI 产物投递处(分解提案)—— 是数据不是控制状态,一律当不可信输入
    logs/           每个 AI 会话的完整 transcript + prompt 快照
    running/        在飞工序标记(PID 存活判据)
    skill_baseline.sha

纪律(不变):(1) 改判据后 —— 现在即改 scripts/task_cli.py 里的
`_provenance_report`/`_active_report`、对应的内置 gate 函数及 `_self_calibrate` ——
下一次 `advance` 会自动先跑 `_self_calibrate()`,任何门先跑校准,失败拒绝出数;
写完新守卫第一件事是给它加正负
用例证明它真的会报警。(2) 改 skill/pipeline 代码后:`task_cli.py baseline --update`
(否则 advance 拒绝工作)。(3) 新流程先在 1–3 个 leaf 上串行走完全程,再扇出。
(4) 闸门变红是正确结果:溯源红=测试来源没锚,容差红=证据缺口——是工单不是故障,
绝不允许为了变绿去删检查、放松容差、或给 custom 测试伪造 upstream 声明。
(5) 存量 tasks/*(athena-*、pluto-*)不进状态机(2026-08-29 拍板);要纳管时给它们
补 provenance/rubric 再经正常门。
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parents[1]
SCRIPTS = SKILL / "scripts"
sys.path.insert(0, str(SCRIPTS))
import _shared as shared  # noqa: E402


def _manifest_arg(leaf: str) -> str:
    mp = shared.manifest_path_for_leaf(leaf)
    if mp is None:
        raise SystemExit(
            f"{leaf} 没有绑定的人批 manifest —— 两-CLI 契约里 manifest 是唯一交接:"
            f"先 codebase_cli.py emit-manifest --task {leaf},再用 task_cli.py 操作该任务"
            f"(存量无 manifest 的 leaf 不进新契约,行为维持原状,无需本壳)")
    return str(mp)


def _forward(cli: str, args: list[str]) -> None:
    print(f"# pipe.py 已是兼容壳 —— 转发等价命令:{cli} {' '.join(args)}", file=sys.stderr)
    r = subprocess.run([sys.executable, str(SCRIPTS / cli), *args])
    sys.exit(r.returncode)


def _passthru(a, *flags: str) -> list[str]:
    out: list[str] = []
    for f in flags:
        v = getattr(a, f.replace("-", "_"), None)
        if v is True:
            out.append(f"--{f}")
        elif v:
            out += [f"--{f}", str(v)]
    return out


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("status")
    p.add_argument("--leaf")
    p.add_argument("--repo")
    p = sub.add_parser("advance")
    p.add_argument("--leaf")
    p.add_argument("--repo")
    p.add_argument("--stage")
    p.add_argument("--ai")
    p.add_argument("--override-reason")
    p.add_argument("--human-ref")
    p = sub.add_parser("approve")
    p.add_argument("--leaf")
    p.add_argument("--repo")
    p.add_argument("--what", required=True,
                   choices=["cut", "tests", "tolerance", "ship", "custom-check"])
    p.add_argument("--leaves")
    p.add_argument("--check")
    p.add_argument("--note")
    p.add_argument("--human-ref")
    p = sub.add_parser("reject")
    p.add_argument("--leaf", required=True)
    p.add_argument("--what", required=True, choices=["tests", "tolerance", "ship"])
    p.add_argument("--reason", required=True)
    p.add_argument("--human-ref")
    p = sub.add_parser("baseline")
    p.add_argument("--update", action="store_true")
    p = sub.add_parser("verdict")
    p.add_argument("--leaf", required=True)
    sub.add_parser("run")
    a = ap.parse_args()

    if a.cmd == "status":
        if a.leaf:
            _forward("task_cli.py", ["status", "--manifest", _manifest_arg(a.leaf)])
        _forward("codebase_cli.py", ["status"] + (["--codebase", a.repo] if a.repo else []))
    if a.cmd == "advance":
        if a.repo:
            _forward("codebase_cli.py", ["decompose", "--codebase", a.repo]
                     + _passthru(a, "ai"))
        if not a.leaf:
            raise SystemExit("advance 需要 --leaf 或 --repo")
        _forward("task_cli.py", ["advance", "--manifest", _manifest_arg(a.leaf)]
                 + _passthru(a, "stage", "ai", "override-reason", "human-ref"))
    if a.cmd == "approve":
        if a.what == "cut":
            if not a.repo:
                raise SystemExit("approve cut 需要 --repo")
            _forward("codebase_cli.py", ["approve-cut", "--codebase", a.repo]
                     + _passthru(a, "leaves", "note", "human-ref"))
        if not a.leaf:
            raise SystemExit("需要 --leaf")
        if a.what == "ship":
            _forward("task_cli.py", ["approve-mergeable", "--manifest", _manifest_arg(a.leaf)]
                     + _passthru(a, "note", "human-ref"))
        _forward("task_cli.py", ["approve", "--manifest", _manifest_arg(a.leaf), "--what", a.what]
                 + _passthru(a, "check", "note", "human-ref"))
    if a.cmd == "reject":
        _forward("task_cli.py", ["reject", "--manifest", _manifest_arg(a.leaf), "--what", a.what,
                                 "--reason", a.reason] + _passthru(a, "human-ref"))
    if a.cmd == "baseline":
        _forward("task_cli.py", ["baseline"] + _passthru(a, "update"))
    if a.cmd == "verdict":
        _forward("task_cli.py", ["status", "--manifest", _manifest_arg(a.leaf)])
    if a.cmd == "run":
        raise SystemExit(
            "pipe.py run(无人值守调度器)已随第三层引擎移除:两-CLI 契约是操作员按 "
            "next_action 一步步推进 —— 对每个任务循环 `task_cli.py advance --manifest <m>`,"
            "在 ⛔ 处停下来等人。")


if __name__ == "__main__":
    main()
