#!/usr/bin/env python3
"""codebase_cli / task_cli 共用的私有数据原语 —— 不是第三个 CLI,也不是引擎。

两个 advisory CLI 之间只允许共享五类原语(references/two-cli-architecture.md):
  manifest —— 人批 task manifest 的结构/校验/scope 指纹(两 CLI 唯一的正式接口);
  path     —— 控制面/intake/manifests/leaf 落盘位置的唯一权威解析;
  指纹     —— 四域内容指纹(fp_env/fp_tests/fp_tol/fp_all)与 custom-check 目录哈希,
             审批/门记录绑它们,改一个字节自动作废;
  JSONL    —— append-only journal 的读写与统一 cli_action 日志形状;
  状态     —— 从 journal+磁盘推导 repo/leaf 状态与建议 next_action(纯读,无副作用)。

工作流本身(AI 工序派发、机械门执行、PR 开/跟、人批命令)不在这里:
codebase_cli.py 拥有 Step1–3,task_cli.py 拥有 Step4–5。本模块没有 main、
不解析参数、不派 AI、不跑门 —— 谁 import 它谁负责动作。
"""
from __future__ import annotations

import hashlib
import json
import os
import time
from pathlib import Path
from typing import Any

SKILL = Path(__file__).resolve().parents[1]           # skills/package-sciaccel-task
ROOT = Path(os.environ.get("SAB_ROOT", str(SKILL.parents[1])))   # ScienceAccelBench
TASKS = ROOT / "tasks"
SCRIPTS = SKILL / "scripts"
PROMPTS = SKILL / "prompts"
CTRL = Path(os.environ.get("SAB_PIPE_DIR", str(Path.home() / ".sciaccel_pipeline")))
JOURNAL = CTRL / "journal.jsonl"
INBOX = CTRL / "inbox"        # AI 产物投递处:是数据不是控制状态,内容一律当不可信输入
LOGS = CTRL / "logs"
RUNMARK = CTRL / "running"
INTAKE = Path(os.environ.get("SAB_INTAKE_DIR", str(SKILL / "pipeline" / "intake")))          # 派工单(人写,git 管)
MANIFESTS = Path(os.environ.get("SAB_MANIFEST_DIR", str(SKILL / "pipeline" / "manifests")))    # 人批 task manifest(codebase_cli 写,git 管)

GATES_VER = 1        # 门的判定逻辑修实质 bug 时 +1:旧版本的**失败**记录整体作废


# ---------------------------------------------------------------- JSONL journal

def append_journal(rec: dict) -> None:
    JOURNAL.parent.mkdir(parents=True, exist_ok=True)
    rec = {"ts": time.strftime("%Y-%m-%dT%H:%M:%S"), **rec}
    with JOURNAL.open("a") as f:
        f.write(json.dumps(rec, ensure_ascii=False) + "\n")


def read_journal(leaf: str | None = None, repo: str | None = None) -> list[dict]:
    if not JOURNAL.is_file():
        return []
    out = []
    for ln in JOURNAL.read_text().splitlines():
        try:
            r = json.loads(ln)
        except json.JSONDecodeError:
            continue
        if leaf is not None and r.get("leaf") != leaf:
            continue
        if repo is not None and r.get("repo") != repo:
            continue
        out.append(r)
    return out


def latest(recs: list[dict], kind: str, **match) -> dict | None:
    """最新一条 kind 记录,且给定字段全部匹配(典型:fp=当前域指纹)。
    指纹不匹配的记录一概视而不见 —— 这就是作废机制。"""
    hit = None
    for r in recs:
        if r.get("kind") != kind:
            continue
        if all(r.get(k) == v for k, v in match.items()):
            hit = r
    return hit


def log_action(*, cli: str, command: str, codebase: str | None = None,
               task: str | None = None, leaf: str | None = None,
               prev_state: str | None = None, action: str | None = None,
               target: str | None = None, result: str, fp: str | None = None,
               head: str | None = None, next_action: str | None = None,
               human_ref: str | None = None, reason: str | None = None,
               error: str | None = None) -> None:
    """两个 CLI 的统一日志形状(见 references/two-cli-architecture.md 的字段表):
    timestamp(journal 自动加)、codebase/task id、CLI/命令、前置状态、动作/目标、
    结果(ok/failed/skipped/overridden)、相关指纹/head、下一步建议、人类引用/理由
    (如适用)、简短错误摘要。绝不写 secrets 或大段 stdout —— error 只留摘要,
    证据一律用路径/哈希。这层记录叠加在既有的细粒度 journal kind
    (gate_*/approval/rejection/cut/override/pr 等)之上,后者形状保持不变。"""
    rec: dict = {"kind": "cli_action", "cli": cli, "command": command, "result": result}
    extra = {"codebase": codebase, "task": task, "leaf": leaf, "prev_state": prev_state,
             "action": action, "target": target, "fp": fp, "head": head,
             "next": next_action, "human_ref": human_ref, "reason": reason,
             "error": ((error or "")[:400] or None)}
    rec.update({k: v for k, v in extra.items() if v is not None})
    append_journal(rec)


# ---------------------------------------------------------------- manifest 契约
# (原 pipeline/manifest.py;pipeline/manifest.py 现在只是指到这里的兼容 re-export。)

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


def manifest_validate(manifest: dict[str, Any]) -> list[str]:
    """返回问题列表;空列表 = 这份 manifest 内部自洽且来源透明。

    这里只检查 manifest 自身的结构与诚实性(分母是否对得上行数、custom 是否
    透明披露、path 是否等于唯一允许的形状),不检查它是否与磁盘上某个具体 leaf
    的当前内容一致——那是 gate-active(task_cli.py 的 `_active_report`)的职责。
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


def manifest_load(path: str | Path) -> dict[str, Any]:
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
    problems = manifest_validate(d)
    if problems:
        raise ManifestError(f"manifest 未通过校验 {p}:\n" + "\n".join(f"  - {x}" for x in problems))
    return d


def scope_fingerprint(manifest: dict[str, Any]) -> str:
    """只对「定义任务边界」的字段取指纹:module_cut / path / checks / denominator。
    人批 manifest 之后这些字段任何一处变化,都必须让下游的旧审批/旧证据显式作废——
    这个指纹就是 task_cli 判断「过时」的依据(不判断磁盘上 leaf 现状,只判断
    manifest 本身相对上一次绑定时是否变了)。"""
    core = {"module_cut": manifest.get("module_cut"), "path": manifest.get("path"),
            "checks": manifest.get("checks"),
            "expected_denominator": manifest.get("expected_denominator")}
    blob = json.dumps(core, sort_keys=True, ensure_ascii=False).encode()
    return hashlib.sha256(blob).hexdigest()[:16]


def official_source_by_id(manifest: dict[str, Any]) -> dict[str, dict[str, Any]]:
    """id -> check 行,给 task_cli.py 的 `_active_report` 之类只关心「这一行长什么样」的调用者用。"""
    return {c["id"]: c for c in manifest.get("checks", []) if isinstance(c, dict) and c.get("id")}


def manifest_path_for_leaf(leaf: str) -> Path | None:
    """人批 task manifest 的落盘位置约定:pipeline/manifests/<codebase>/<leaf>.manifest.json
    (codebase_cli.py emit-manifest 写;task_id 必须等于 leaf slug)。没有 manifest 的
    leaf(存量包、或尚未走两-CLI 契约的旧式 cut)返回 None —— 全部下游调用点都要把
    None 当「此门/此约束对这个 leaf 不适用」处理,绝不能因为没有 manifest 就报错。"""
    m = planned_leaves().get(leaf)
    repo = m.get("repo") if m else None
    cands = ([MANIFESTS / repo / f"{leaf}.manifest.json"] if repo else [])
    cands += sorted(MANIFESTS.glob(f"*/{leaf}.manifest.json"))
    for p in cands:
        if p.is_file():
            return p
    return None


def _manifest_doc(leaf: str) -> dict | None:
    mp = manifest_path_for_leaf(leaf)
    if mp is None:
        return None
    try:
        d = json.loads(mp.read_text())
        return d if isinstance(d, dict) else None
    except (OSError, json.JSONDecodeError):
        return None


def manifest_scope_fp(leaf: str) -> str:
    """绑了 manifest 的 leaf:manifest 的 scope 指纹(module_cut/path/checks/denominator);
    没绑返回 ""。"""
    d = _manifest_doc(leaf)
    return scope_fingerprint(d) if d else ""


def manifest_check_ids(leaf: str) -> list[str]:
    """manifest 承诺的 check id(排序);没绑 manifest → []。"""
    d = _manifest_doc(leaf)
    return sorted(official_source_by_id(d)) if d else []


# ---------------------------------------------------------------- 内容指纹

# 生成物目录不进指纹:solve/test 的运行产物(oracle 输出、缓存)每次自证都会变,
# 进了指纹就会「门刚绿就被自己的产物作废」(演练实测的死循环)。
# 约定:oracle 输出必须落在 solution/oracle_out/;评分沙盒产物落 /tmp。
GENERATED_DIRS = {"oracle_out", "outputs", "__pycache__", ".pytest_cache", ".cache"}

# 后写域的文件不算测试选择域:rubric.json 是 tolerance 工序写的,check.json(labels)
# 与 test.sh/harness 是 finalize 工序写的 —— 它们落盘不该作废「用哪些测试」的人类审批
# (演练实测:不排除的话 tolerance/finalize 一动笔,tests 审批必然连带作废,流程死锁)。
LATER_STAGE_FILES = {"rubric.json", "check.json"}


def hash_paths(files: list[Path], base: Path) -> str:
    """稳定内容哈希。>16MB 的文件(大 fixture)只掺入 名字+大小,避免指纹计算本身变成 IO 风暴。"""
    h = hashlib.sha256()
    for f in sorted(files):
        if not f.is_file() or ".bak" in f.name:
            continue
        if GENERATED_DIRS & set(f.relative_to(base).parts[:-1]):
            continue
        rel = str(f.relative_to(base)).encode()
        h.update(rel)
        st = f.stat()
        if st.st_size > 16 * 2**20:
            h.update(str(st.st_size).encode())
        else:
            h.update(f.read_bytes())
    return h.hexdigest()[:16]


def leaf_dir(leaf: str) -> Path:
    """leaf 落盘位置。绑了人批 manifest 的 leaf 用 manifest 固定的 tasks/{codebase}/{task}/
    (两-CLI 契约唯一允许的形状);没绑 manifest 的存量 leaf 仍是 tasks/<slug>/。"""
    mp = manifest_path_for_leaf(leaf)
    if mp is not None:
        try:
            rel = json.loads(mp.read_text()).get("path")
            if isinstance(rel, str) and rel.strip("/"):
                return ROOT / rel.strip("/")
        except (OSError, json.JSONDecodeError):
            pass
    return TASKS / leaf


def _glob(d: Path, *pats: str) -> list[Path]:
    out: list[Path] = []
    for p in pats:
        out += [f for f in d.glob(p) if f.is_file()]
    return out


def _with_scope(leaf: str, fp: str) -> str:
    """把 manifest 的 scope 指纹掺进域指纹:manifest 改了(哪怕 leaf 一个字节没动),该域的门与
    人类审批一样作废 —— 「scope 变了旧审批/旧证据不再适用」直接复用现有指纹作废机制,不另起
    状态。没绑 manifest 的存量 leaf 指纹原样不变(旧 journal 记录继续有效)。"""
    s = manifest_scope_fp(leaf)
    return fp if not s else hashlib.sha256(f"{fp}:{s}".encode()).hexdigest()[:16]


def fp_env(leaf: str) -> str:
    """环境+oracle 域:solver 镜像、隐藏测试镜像、oracle 入口。变了 → docker 门重跑。"""
    d = leaf_dir(leaf)
    return hash_paths(_glob(d, "environment/**/*", "tests/Dockerfile",
                            "solution/**/*", "comment/pipeline.toml"), d)


def fp_tests(leaf: str) -> str:
    """测试选择域:有哪些 check、验证逻辑、fixture、溯源声明。"""
    d = leaf_dir(leaf)
    files = [f for f in _glob(d, "tests/checks/**/*")
             if f.name not in LATER_STAGE_FILES]
    return _with_scope(leaf, hash_paths(files, d))


def fp_tol(leaf: str) -> str:
    d = leaf_dir(leaf)
    return hash_paths(_glob(d, "tests/checks/*/rubric.json"), d)


def fp_all(leaf: str) -> str:
    d = leaf_dir(leaf)
    return _with_scope(leaf, hash_paths(_glob(d, "task.toml", "instruction.md", "environment/**/*",
                                              "tests/**/*", "solution/**/*", "target/*.json"), d))


def check_dir_sha(leaf: str, check: str) -> str:
    """custom-check 审批绑的目录哈希。同样排除后写域文件:批的是「这个测试本身」,
    容差与标签另有各自的门和审批。"""
    d = leaf_dir(leaf) / "tests" / "checks" / check
    if not d.is_dir():
        return ""
    return hash_paths([f for f in d.rglob("*")
                       if f.is_file() and f.name not in LATER_STAGE_FILES], d)


def skill_baseline(update: bool = False) -> tuple[bool, str]:
    """skill 自身完整性(§32):驱动器/判据/提示词被 agent 改过 = 测「能不能改分」。"""
    files = sorted(list(SCRIPTS.glob("*.py")) + list((SKILL / "pipeline").glob("*.py"))
                   + list(PROMPTS.glob("*.txt")) + list((SKILL / "tests").glob("*.py")))
    h = hashlib.sha256()
    for f in files:
        h.update(f.name.encode())
        h.update(f.read_bytes())
    cur = h.hexdigest()
    bf = CTRL / "skill_baseline.sha"
    if update or not bf.is_file():
        bf.parent.mkdir(parents=True, exist_ok=True)
        bf.write_text(cur + "\n")
        return True, "baseline recorded"
    ok = bf.read_text().strip() == cur
    return ok, ("ok" if ok else
                "skill 文件与基线不符 —— 若是你自己改的,跑 `task_cli.py baseline --update`;"
                "若不是,先查是谁改了验收代码(§32)")


# ---------------------------------------------------------------- 审批/驳回记录

APPROVABLE = {"cut", "tests", "tolerance", "ship", "custom-check"}
SCOPE_FP = {"tests": fp_tests, "tolerance": fp_tol, "ship": fp_all}


def approval(leaf: str, what: str) -> dict | None:
    """当前域指纹下的有效审批。任何改动 → 指纹变 → 审批自动失效(退回等人重批)。
    同指纹下 rejection 晚于 approval 则以 rejection 为准。"""
    fp = SCOPE_FP[what](leaf)
    recs = read_journal(leaf=leaf)
    ap = latest(recs, "approval", what=what, fp=fp)
    rj = latest(recs, "rejection", what=what, fp=fp)
    if ap and rj and recs.index(rj) > recs.index(ap):
        return None
    return ap


def rejection(leaf: str, what: str) -> dict | None:
    fp = SCOPE_FP[what](leaf)
    recs = read_journal(leaf=leaf)
    ap = latest(recs, "approval", what=what, fp=fp)
    rj = latest(recs, "rejection", what=what, fp=fp)
    if rj and not (ap and recs.index(ap) > recs.index(rj)):
        return rj
    return None


def approved_customs(leaf: str) -> list[str]:
    """人类逐个批过、且 check 目录内容未变的 custom 检查名单。"""
    out = []
    for r in read_journal(leaf=leaf):
        if r.get("kind") == "approval" and r.get("what") == "custom-check":
            if check_dir_sha(leaf, r.get("check", "")) == r.get("sha"):
                out.append(r["check"])
    return sorted(set(out))


# ---------------------------------------------------------------- 状态推导

def planned_leaves() -> dict[str, dict]:
    """cut 审批注册的 leaf → module 描述(建包工单)。后批的覆盖先批的。"""
    out: dict[str, dict] = {}
    for r in read_journal():
        if r.get("kind") == "cut":
            for m in r.get("modules", []):
                out[m["slug"]] = {**m, "repo": r.get("repo")}
    return out


def state_repo(repo: str) -> dict:
    info = {"repo": repo}
    if not (INTAKE / f"{repo}.toml").is_file():
        info["state"] = "NO_INTAKE"
        return info
    recs = read_journal(repo=repo)
    if latest(recs, "cut"):
        info["state"] = "CUT_APPROVED"
        info["leaves"] = latest(recs, "cut").get("leaves")
        return info
    if (INBOX / f"{repo}.decomposition.json").is_file():
        info["state"] = "PROPOSED"
        info["next"] = "⛔ 人:审提案后 codebase_cli.py approve-cut --codebase %s --human-ref …" % repo
        return info
    info["state"] = "INTAKE"
    info["next"] = "decompose"
    return info


def state_leaf(leaf: str) -> dict:
    d = leaf_dir(leaf)
    info: dict = {"leaf": leaf}
    recs = read_journal(leaf=leaf)

    if not d.is_dir() or not (d / "environment" / "Dockerfile").is_file():
        info["state"] = "MISSING"
        info["next"] = "scaffold" if leaf in planned_leaves() else None
        if info["next"] is None:
            info["note"] = "不在任何已批 cut 里,也无現成包 —— 先走 intake/decompose/approve-cut"
        return info

    # —— 环境域:docker 门
    fe = fp_env(leaf)
    info["fp_env"] = fe
    g = latest(recs, "gate_env", fp=fe)
    if g and g.get("ver") != GATES_VER and not g.get("ok"):
        g = None
    if not g:
        info["state"] = "SCAFFOLDED"
        info["next"] = "gate-env"
        return info
    if not g.get("ok"):
        info["state"] = "SCAFFOLDED"
        info["next"] = "fix"
        info["gate_fail"] = ("gate_env", g.get("detail", "")[:300])
        return info

    # —— 测试选择域:溯源门 + 人批
    checks = sorted(p.name for p in (d / "tests" / "checks").glob("*") if p.is_dir()) \
        if (d / "tests" / "checks").is_dir() else []
    info["checks"] = len(checks)
    if not checks:
        info["state"] = "ENV_OK"
        info["next"] = "curate"
        return info
    ft = fp_tests(leaf)
    info["fp_tests"] = ft
    g = latest(recs, "gate_tests", fp=ft)
    if g and g.get("ver") != GATES_VER and not g.get("ok"):
        g = None
    if not g:
        info["state"] = "ENV_OK"
        info["next"] = "gate-tests"
        return info
    if not g.get("ok"):
        info["state"] = "ENV_OK"
        info["next"] = "fix"
        info["gate_fail"] = ("gate_tests", g.get("detail", "")[:300])
        return info
    if rejection(leaf, "tests"):
        info["state"] = "TESTS_OK"
        info["next"] = "fix"
        info["rejected"] = ("tests", rejection(leaf, "tests").get("reason", ""))
        return info
    if not approval(leaf, "tests"):
        info["state"] = "TESTS_OK"
        info["next"] = f"⛔ 人:审测试选择后 task_cli.py approve --what tests --human-ref …(leaf {leaf})"
        return info

    # —— 容差域:证据门 + 人批
    have_rubric = bool(list((d / "tests" / "checks").glob("*/rubric.json")))
    if not have_rubric:
        info["state"] = "TESTS_APPROVED"
        info["next"] = "tolerance"
        return info
    fl = fp_tol(leaf)
    info["fp_tol"] = fl
    g = latest(recs, "gate_tol", fp=fl)
    if g and g.get("ver") != GATES_VER and not g.get("ok"):
        g = None
    if not g:
        info["state"] = "TESTS_APPROVED"
        info["next"] = "gate-tol"
        return info
    if not g.get("ok"):
        info["state"] = "TESTS_APPROVED"
        info["next"] = "fix"
        info["gate_fail"] = ("gate_tol", g.get("detail", "")[:300])
        return info
    if rejection(leaf, "tolerance"):
        info["state"] = "TOL_OK"
        info["next"] = "fix"
        info["rejected"] = ("tolerance", rejection(leaf, "tolerance").get("reason", ""))
        return info
    if not approval(leaf, "tolerance"):
        info["state"] = "TOL_OK"
        info["next"] = f"⛔ 人:审容差后 task_cli.py approve --what tolerance --human-ref …(leaf {leaf})"
        return info

    # —— 收尾域:结构 validator + 容器自证 + 人批
    has_targets = (d / "target").is_dir() and bool(list((d / "target").glob("*.json")))
    if not ((d / "task.toml").is_file() and (d / "instruction.md").is_file()
            and has_targets):
        info["state"] = "TOL_APPROVED"
        info["next"] = "finalize"
        return info
    fa = fp_all(leaf)
    info["fp_all"] = fa
    g = latest(recs, "gate_final", fp=fa)
    if g and g.get("ver") != GATES_VER and not g.get("ok"):
        g = None
    if not g:
        info["state"] = "TOL_APPROVED"
        info["next"] = "gate-final"
        return info
    if not g.get("ok"):
        info["state"] = "TOL_APPROVED"
        info["next"] = "fix"
        info["gate_fail"] = ("gate_final", g.get("detail", "")[:300])
        return info
    if rejection(leaf, "ship"):
        info["state"] = "PACKAGED"
        info["next"] = "fix"
        info["rejected"] = ("ship", rejection(leaf, "ship").get("reason", ""))
        return info
    if not approval(leaf, "ship"):
        info["state"] = "PACKAGED"
        info["next"] = "⛔ 人:PR 上终审后 task_cli.py approve-mergeable --human-ref …"
        return info
    info["state"] = "READY"
    return info
