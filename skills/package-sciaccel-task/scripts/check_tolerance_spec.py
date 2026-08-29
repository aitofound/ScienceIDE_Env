#!/usr/bin/env python3
"""容差证据门:「什么叫正确」必须带实测证据,不许拍脑袋。

契约:tests/checks/<name>/rubric.json 必须含 "comparison" 对象:

  {"comparison": {
     "kind": "exact" | "abs" | "rel" | "statistical",
     "tolerance": <number>,              # exact 可省;statistical 用 alpha 代替
     "alpha": <number 0~1>,              # 仅 statistical
     "metric": "…",                      # 比的是什么量(自由文本,必填)
     "evidence": {
        "oracle_repeats": <int ≥ 2>,     # 重复跑参考解的次数(推荐 ≥3)
        "observed_spread": <number ≥ 0>, # 重复运行间该 metric 的实测极差/波动
        "note": "…"                      # 怎么测的(命令/环境),自由文本
     },
     "rationale": "…"}}                  # 科学依据一句话,必填(给人批时看)

判定规则(全部来自实测教训,不是审美):
  - tolerance < observed_spread → 红:判据比 oracle 自身噪声还紧,必然假红
  - kind=exact 而 observed_spread ≠ 0 → 红:声明确定性但证据说有噪声
  - 缺 evidence / oracle_repeats < 2 → 红:没有重复测量就没有噪声概念
  - tolerance / max(observed_spread, tiny) > 1e3 → ⚠️ warn(太松嫌疑,给人批时看,不阻断)

退出码:0 全绿;1 有红。JSON 报告打到 stdout。
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

LOOSE_RATIO = 1e3
TINY = 1e-300


def check_one(name: str, rb: dict, fails: list[str], warns: list[str]) -> None:
    cmp_ = rb.get("comparison")
    if not isinstance(cmp_, dict):
        fails.append(f"{name}: rubric.json 缺 comparison 对象")
        return
    kind = cmp_.get("kind")
    if kind not in ("exact", "abs", "rel", "statistical"):
        fails.append(f"{name}: comparison.kind 必须是 exact|abs|rel|statistical(拿到 {kind!r})")
        return
    if not (cmp_.get("metric") or "").strip():
        fails.append(f"{name}: comparison.metric 必填(比的是什么量)")
    if not (cmp_.get("rationale") or "").strip():
        fails.append(f"{name}: comparison.rationale 必填(科学依据,给人批时看)")
    ev = cmp_.get("evidence")
    if not isinstance(ev, dict):
        fails.append(f"{name}: 缺 evidence —— 容差没有实测证据就是拍脑袋")
        return
    rep = ev.get("oracle_repeats")
    spread = ev.get("observed_spread")
    if not isinstance(rep, int) or rep < 2:
        fails.append(f"{name}: evidence.oracle_repeats 必须是 ≥2 的整数(拿到 {rep!r});"
                     f"没有重复测量就没有噪声概念")
        return
    if not isinstance(spread, (int, float)) or spread < 0:
        fails.append(f"{name}: evidence.observed_spread 必须是 ≥0 的数(拿到 {spread!r})")
        return

    if kind == "exact":
        if spread != 0:
            fails.append(f"{name}: 声明 exact 但实测 spread={spread} ≠ 0 —— "
                         f"要么改成容差判据,要么先解决不确定性来源")
        return
    if kind == "statistical":
        alpha = cmp_.get("alpha")
        if not isinstance(alpha, (int, float)) or not (0 < alpha < 1):
            fails.append(f"{name}: statistical 判据必须给 0<alpha<1(拿到 {alpha!r})")
        return
    tol = cmp_.get("tolerance")
    if not isinstance(tol, (int, float)) or tol <= 0:
        fails.append(f"{name}: kind={kind} 必须给正数 tolerance(拿到 {tol!r})")
        return
    if tol < spread:
        fails.append(f"{name}: tolerance={tol} < 实测噪声 spread={spread} —— "
                     f"判据比 oracle 自身噪声还紧,正确实现也会假红")
        return
    if tol / max(spread, TINY) > LOOSE_RATIO:
        warns.append(f"{name}: tolerance={tol} 比实测噪声 {spread} 松 >1000 倍 —— "
                     f"太松嫌疑(送分维度?),请人批容差时重点看")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--leaf-dir", required=True)
    a = ap.parse_args()
    leaf = Path(a.leaf_dir).resolve()
    checks_dir = leaf / "tests" / "checks"
    checks = sorted(p for p in checks_dir.iterdir() if p.is_dir()) \
        if checks_dir.is_dir() else []
    if not checks:
        print("RED: 没有 check 可审容差")
        return 1
    fails: list[str] = []
    warns: list[str] = []
    for c in checks:
        rj = c / "rubric.json"
        if not rj.is_file():
            fails.append(f"{c.name}: 缺 rubric.json(每个 check 必须定义「什么叫正确」)")
            continue
        try:
            rb = json.loads(rj.read_text())
        except json.JSONDecodeError as e:
            fails.append(f"{c.name}: rubric.json 解析失败:{e}")
            continue
        check_one(c.name, rb, fails, warns)
    print(json.dumps({"checks": len(checks), "fails": fails, "warns": warns},
                     ensure_ascii=False, indent=1))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
