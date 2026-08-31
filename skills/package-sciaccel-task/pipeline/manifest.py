#!/usr/bin/env python3
"""human-approved task manifest —— codebase_cli 与 task_cli 之间唯一的正式接口。

背景见 references/two-cli-architecture.md。这份契约只做一件事:把「人已经批过
什么」变成一份两个 CLI 都能读、都不能悄悄改的结构化事实——

  · codebase_cli.py emit-manifest 产出它(codebase/task id、模块切分、
    tasks/{codebase}/{task}/ 路径、expected check 清单/分母、每行的
    source_type/official_source、透明披露过的 custom check、human_approval_ref);
  · task_cli.py 只消费它——生成受限的 worker brief、跑 gate-active、判断
    manifest 是否与已建的 leaf 走样(scope 指纹)。

没有第三份格式,也没有另一层「manifest 的 manifest」。字段一旦确定,改动
module_cut / checks / expected_denominator / path 中任何一项都会改变
scope_fingerprint(),task_cli 用它判断「这份人类批准是否还适用于当前的 leaf」——
这是契约文字里「Stale/changed scope invalidates prior approval/evidence」的落地。

本模块只做静态/结构校验(纯函数,不摸磁盘上的 leaf 状态),不判断某个具体 leaf
现在是不是新鲜——那是 task_cli.py 结合 pipe.py 的 journal 才能回答的问题。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

MANIFEST_VERSION = 1
SOURCE_TYPES = {"official", "custom"}
REQUIRED_TOP = {"manifest_version", "codebase_id", "task_id", "path", "module_cut",
                "checks", "expected_denominator", "human_approval_ref"}
REQUIRED_CHECK = {"id", "source_type"}


class ManifestError(ValueError):
    """manifest 未通过schema/一致性校验,或压根读不到/解析不了。"""


def manifest_path_for(codebase_id: str, task_id: str) -> str:
    """契约里唯一允许的任务路径形状:tasks/{codebase-name}/{task-name}/。"""
    return f"tasks/{codebase_id}/{task_id}/"


def validate(manifest: dict[str, Any]) -> list[str]:
    """返回问题列表;空列表 = 这份 manifest 内部自洽且来源透明。

    这里只检查 manifest 自身的结构与诚实性(分母是否对得上行数、custom 是否
    透明披露、path 是否等于唯一允许的形状),不检查它是否与磁盘上某个具体 leaf
    的当前内容一致——那是 gate-active(scripts/gate_active.py)的职责。
    """
    problems: list[str] = []
    missing_top = REQUIRED_TOP - set(manifest)
    if missing_top:
        problems.append(f"manifest 缺顶层字段:{sorted(missing_top)}")
        return problems  # 基本字段都不全,再往下检查只会是噪声

    if manifest.get("manifest_version") != MANIFEST_VERSION:
        problems.append(f"manifest_version 必须是 {MANIFEST_VERSION}"
                        f"(拿到 {manifest.get('manifest_version')!r})")

    codebase_id, task_id = manifest.get("codebase_id"), manifest.get("task_id")
    if not (isinstance(codebase_id, str) and codebase_id):
        problems.append("codebase_id 必须是非空字符串")
    if not (isinstance(task_id, str) and task_id):
        problems.append("task_id 必须是非空字符串")
    if isinstance(codebase_id, str) and isinstance(task_id, str):
        want_path = manifest_path_for(codebase_id, task_id)
        if manifest.get("path") != want_path:
            problems.append(f"path 必须是 {want_path!r}(拿到 {manifest.get('path')!r})"
                            f"—— tasks/{{codebase-name}}/{{task-name}}/ 是唯一允许的形状")

    if not str(manifest.get("human_approval_ref", "")).strip():
        problems.append("human_approval_ref 不能为空 —— 每份 manifest 必须能追溯到"
                        "人类批准的原话/消息引用,不接受「看起来同意了」")

    if not str(manifest.get("module_cut", "")).strip():
        problems.append("module_cut 不能为空 —— 必须写清这份 manifest 对应哪个模块边界")

    checks = manifest.get("checks")
    if not isinstance(checks, list) or not checks:
        problems.append("checks 必须是非空数组(至少 1 行)")
        checks = []

    seen_ids: set[str] = set()
    dup_ids: set[str] = set()
    for i, c in enumerate(checks):
        loc = f"checks[{i}]"
        if not isinstance(c, dict):
            problems.append(f"{loc} 必须是对象")
            continue
        missing = REQUIRED_CHECK - set(c)
        if missing:
            problems.append(f"{loc} 缺字段:{sorted(missing)}")
            continue
        cid = c["id"]
        if not (isinstance(cid, str) and cid):
            problems.append(f"{loc}.id 必须是非空字符串")
        elif cid in seen_ids:
            dup_ids.add(cid)
            problems.append(f"{loc}: 重复的 check id {cid!r}"
                            f"—— 每行必须在 manifest 里恰好出现一次")
        else:
            seen_ids.add(cid)
        st = c.get("source_type")
        if st not in SOURCE_TYPES:
            problems.append(f"{loc}.source_type 必须是 {sorted(SOURCE_TYPES)}(拿到 {st!r})")
        elif st == "official" and not str(c.get("official_source", "")).strip():
            problems.append(f"{loc}: source_type=official 必须给 official_source"
                            f"(官方测试的相对路径,门会按这条核对来源一致性)")
        elif st == "custom":
            if not str(c.get("justification", "")).strip():
                problems.append(f"{loc}: source_type=custom 必须写 justification"
                                f"(官方测试为什么覆盖不到这一行)")
            if not c.get("human_disclosed"):
                problems.append(f"{loc}: custom check 必须显式标 human_disclosed=true"
                                f"—— 未透明披露并单独批准的 custom check 不允许进 manifest,"
                                f"这正是「undisclosed/agent-created checks must fail or HOLD」")

    denom = manifest.get("expected_denominator")
    if not isinstance(denom, int) or isinstance(denom, bool) or denom != len(checks):
        problems.append(f"expected_denominator 必须等于 checks 行数"
                        f"(声明 {denom!r},实际 {len(checks)} 行)")

    return problems


def load(path: str | Path) -> dict[str, Any]:
    """读并校验一份 manifest;不通过就抛 ManifestError(把问题列全,不猜、不将就)。"""
    p = Path(path)
    try:
        text = p.read_text()
    except OSError as e:
        raise ManifestError(f"manifest 读不到:{p}({e})") from e
    try:
        d = json.loads(text)
    except json.JSONDecodeError as e:
        raise ManifestError(f"manifest 不是合法 JSON:{p}({e})") from e
    if not isinstance(d, dict):
        raise ManifestError(f"manifest 顶层必须是 JSON 对象:{p}")
    problems = validate(d)
    if problems:
        raise ManifestError(f"manifest 未通过校验 {p}:\n" + "\n".join(f"  - {x}" for x in problems))
    return d


def scope_fingerprint(manifest: dict[str, Any]) -> str:
    """只对「定义任务边界」的字段取指纹:module_cut / path / checks / denominator。
    人批 manifest 之后这些字段任何一处变化,都必须让下游的旧审批/旧证据显式作废——
    这个指纹就是 task_cli.py 判断「过时」的依据(不判断磁盘上 leaf 现状,只判断
    manifest 本身相对上一次绑定时是否变了)。"""
    core = {"module_cut": manifest.get("module_cut"), "path": manifest.get("path"),
            "checks": manifest.get("checks"),
            "expected_denominator": manifest.get("expected_denominator")}
    blob = json.dumps(core, sort_keys=True, ensure_ascii=False).encode()
    return hashlib.sha256(blob).hexdigest()[:16]


def official_source_by_id(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """id -> check 行,给 gate_active.py 之类只关心「这一行长什么样」的调用者用。"""
    return {c["id"]: c for c in manifest.get("checks", []) if isinstance(c, dict) and c.get("id")}
