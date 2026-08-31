#!/usr/bin/env python3
"""两-CLI 契约的最小生命周期测试(确定性、无 docker、不碰真实控制面;每个用例自带临时 ROOT/journal)。

覆盖:LOCAL_VALIDATED → PR_OPEN 直达(无多余人类门)→ 修复后证据过期可见 → 重验 → track-pr →
approve-mergeable 终态;人类越阶必须留痕且被跳过的审批保持缺失;manifest scope 变化作废审批/证据;
gate_final 对未披露 check 红;codebase_cli 按已批 cut 产 manifest、拒绝未披露 custom。

    python3 tests/test_two_cli.py      # 或 pytest tests/test_two_cli.py
"""
from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SKILL = Path(__file__).resolve().parents[1]
PIPE = SKILL / "pipeline" / "pipe.py"
TASK_CLI = SKILL / "scripts" / "task_cli.py"
CODEBASE_CLI = SKILL / "scripts" / "codebase_cli.py"
GOOD_CMP = {"comparison": {"kind": "abs", "tolerance": 1e-6, "metric": "L2 error", "rationale": "机器精度累积上界",
                           "evidence": {"oracle_repeats": 3, "observed_spread": 1e-8, "note": "3 次重复"}}}
CB, TASK = "cb", "core"


class Env:
    def __init__(self, tmp: Path):
        self.root, self.ctrl = tmp / "repo", tmp / "ctrl"
        (self.root / "tasks").mkdir(parents=True)
        self.manifests, self.intake = tmp / "manifests", tmp / "intake"
        self.env = {**os.environ, "SAB_ROOT": str(self.root), "SAB_PIPE_DIR": str(self.ctrl),
                    "SAB_MANIFEST_DIR": str(self.manifests), "SAB_INTAKE_DIR": str(self.intake)}

    def run(self, script: Path, *args: str, check: bool = False) -> subprocess.CompletedProcess:
        r = subprocess.run([sys.executable, str(script), *args], env=self.env, cwd=str(self.root),
                           capture_output=True, text=True)
        if check:
            assert r.returncode == 0, f"{script.name} {args} rc={r.returncode}\n{r.stdout}\n{r.stderr}"
        return r

    def journal(self) -> list[dict]:
        f = self.ctrl / "journal.jsonl"
        return [json.loads(ln) for ln in f.read_text().splitlines() if ln.strip()] if f.is_file() else []

    def append(self, rec: dict) -> None:
        self.ctrl.mkdir(parents=True, exist_ok=True)
        with (self.ctrl / "journal.jsonl").open("a") as f:
            f.write(json.dumps({"ts": time.strftime("%Y-%m-%dT%H:%M:%S"), **rec}, ensure_ascii=False) + "\n")

    def task(self, mp: Path) -> dict:
        return json.loads(self.run(TASK_CLI, "status", "--manifest", str(mp), check=True).stdout)

    def leaf(self, leaf: str) -> dict:
        return json.loads(self.run(PIPE, "status", "--leaf", leaf, check=True).stdout)

    def approve(self, what: str, ref: str) -> None:
        self.run(PIPE, "approve", "--leaf", TASK, "--what", what, "--human-ref", ref, check=True)

    def simulate_docker_gate(self) -> None:
        """docker 门真跑 docker build,测试里不跑:按当前 fp_env 直接写一条绿记录(journal 是状态源)。"""
        self.append({"kind": "gate_env", "leaf": TASK, "fp": self.leaf(TASK)["fp_env"], "ok": True, "ver": 1})


def write_manifest(env: Env, ids: list[str], ref: str = "tg#100") -> Path:
    m = {"manifest_version": 1, "codebase_id": CB, "task_id": TASK, "path": f"tasks/{CB}/{TASK}/",
         "module_cut": "solver core", "expected_denominator": len(ids), "human_approval_ref": ref,
         "checks": [{"id": i, "source_type": "official", "official_source": "tests/test_a.py"} for i in ids]}
    p = env.manifests / CB / f"{TASK}.manifest.json"
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(m, ensure_ascii=False, indent=1))
    return p


def cut(env: Env, official=("tests/test_a.py",)) -> None:
    env.append({"kind": "cut", "repo": CB, "leaves": [TASK], "note": "", "human_ref": "tg#99",
                "modules": [{"slug": TASK, "paths": ["src/"], "official_tests": list(official), "rationale": "core"}]})


def build_leaf(env: Env, ids: list[str]) -> Path:
    d = env.root / "tasks" / CB / TASK
    src = d / "code" / CB / "tests" / "test_a.py"
    src.parent.mkdir(parents=True)
    src.write_bytes(b"def test(): pass\n")
    sha = hashlib.sha256(src.read_bytes()).hexdigest()
    for sub in ("environment", "tests", "solution", "target"):
        (d / sub).mkdir()
    (d / "environment" / "Dockerfile").write_text("FROM scratch\n")
    (d / "tests" / "Dockerfile").write_text("FROM scratch\n")
    reward = json.dumps({"reward": 1.0, "checks": {i: {"reward": 1.0} for i in ids}})
    (d / "tests" / "test.sh").write_text("#!/bin/bash\ncat > \"$HARBOR_REWARD_FILE\" <<'JSON'\n" + reward + "\nJSON\n")
    (d / "solution" / "solve.sh").write_text("#!/bin/bash\nexit 0\n")
    (d / "target" / "cpu.json").write_text('{"device": "cpu"}\n')
    (d / "task.toml").write_text(f'[metadata.sciaccel]\nslug = "{TASK}"\n')
    (d / "instruction.md").write_text("# port the core\n")
    for n, i in enumerate(ids):
        add_check(d, i, sha, acceleration=(n == 0))
    return d


def add_check(d: Path, cid: str, sha: str, acceleration: bool = False) -> None:
    c = d / "tests" / "checks" / cid
    c.mkdir(parents=True, exist_ok=True)
    (c / "provenance.json").write_text(json.dumps({"origin": "upstream",
                                                   "sources": [{"path": f"code/{CB}/tests/test_a.py", "sha256": sha}]}))
    (c / "rubric.json").write_text(json.dumps(GOOD_CMP))
    if acceleration:
        (c / "check.json").write_text('{"labels": ["acceleration"]}')


def to_local_validated(env: Env, mp: Path) -> None:
    env.simulate_docker_gate()
    env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True)          # gate-tests(真跑溯源门)
    env.approve("tests", "tg#101")
    env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True)          # gate-tol(真跑证据门)
    env.approve("tolerance", "tg#102")
    env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True)          # gate-final(all-active+validator+溯源+容差+逐行 selfpass)


# ---------------------------------------------------------------- 用例

def test_local_validated_opens_pr_directly_then_iterates_to_mergeable():
    with tempfile.TemporaryDirectory(prefix="sab_2cli_") as s:
        env = Env(Path(s)); ids = ["energy", "mass"]
        mp = write_manifest(env, ids); build_leaf(env, ids); cut(env)
        st = env.task(mp)
        assert st["lifecycle"] == "BUILDING" and st["pipe_next"] == "gate-env", st
        assert st["leaf_dir"].endswith(f"tasks/{CB}/{TASK}"), st["leaf_dir"]        # manifest 固定的嵌套路径
        to_local_validated(env, mp)
        st = env.task(mp)
        assert st["lifecycle"] == "LOCAL_VALIDATED" and "open-pr" in st["next_action"], st
        assert st["gates"]["gate_active"].startswith("✅") and st["gates"]["gate_final"].startswith("✅"), st["gates"]
        assert st["approvals"]["ship"].startswith("无"), "PR 之前不该要求任何 ship/HUMAN_PR_APPROVED 人类门"
        # LOCAL_VALIDATED → PR_OPEN 直达
        env.run(TASK_CLI, "open-pr", "--manifest", str(mp), "--url", "https://github.com/x/y/pull/1", check=True)
        st = env.task(mp)
        assert st["lifecycle"] == "PR_OPEN" and st["pr"]["evidence_current"] is True and st["completion"] == "normal", st
        assert not [r for r in env.journal() if r.get("kind") == "override"]
        body = (env.ctrl / "pr" / f"{TASK}.pr-body.md").read_text()
        assert "expected denominator = 2" in body and "never merges" in body and "| energy | official |" in body
        # PR 上修复(改容差)→ 指纹变 → 证据/审批过期可见
        rub = env.root / "tasks" / CB / TASK / "tests" / "checks" / "energy" / "rubric.json"
        d = json.loads(rub.read_text()); d["comparison"]["tolerance"] = 2e-6; rub.write_text(json.dumps(d))
        st = env.task(mp)
        assert st["lifecycle"] == "PR_ITERATING" and st["pr"]["evidence_current"] is False, st
        assert "过期" in st["gates"]["gate_final"] and st["pipe_state"] == "TESTS_APPROVED", st
        assert env.run(TASK_CLI, "track-pr", "--manifest", str(mp)).returncode == 1   # 未重验不许重新绑定
        env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True)               # gate-tol
        env.approve("tolerance", "tg#103")
        env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True)               # gate-final
        st = env.task(mp)
        assert st["lifecycle"] == "PR_ITERATING" and "track-pr" in st["next_action"], st
        env.run(TASK_CLI, "track-pr", "--manifest", str(mp), check=True)
        assert env.task(mp)["lifecycle"] == "PR_OPEN"
        # ⛔ 人:可合并
        env.run(TASK_CLI, "approve-mergeable", "--manifest", str(mp), "--human-ref", "tg#110", check=True)
        st = env.task(mp)
        assert st["lifecycle"] == "HUMAN_APPROVED_MERGEABLE" and st["completion"] == "normal", st
        assert env.run(TASK_CLI, "merge", "--manifest", str(mp)).returncode != 0      # 没有 merge 命令
        acts = [r for r in env.journal() if r.get("kind") == "cli_action" and r.get("cli") == "task_cli"]
        assert acts and all({"ts", "cli", "command", "result", "prev_state", "next", "task"} <= set(r) for r in acts)
        assert {r["result"] for r in acts} <= {"ok", "failed", "skipped", "overridden"}


def test_override_requires_human_ref_and_keeps_skipped_approval_visible():
    with tempfile.TemporaryDirectory(prefix="sab_2cli_") as s:
        env = Env(Path(s)); ids = ["energy"]
        mp = write_manifest(env, ids); build_leaf(env, ids); cut(env)
        env.simulate_docker_gate()
        env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True)      # gate-tests → TESTS_OK ⛔
        assert env.task(mp)["pipe_next"].startswith("⛔")
        r = env.run(TASK_CLI, "advance", "--manifest", str(mp), "--stage", "gate-tol")
        assert r.returncode != 0 and not [x for x in env.journal() if x.get("kind") == "override"], "空手 --stage 必须被拒"
        env.run(TASK_CLI, "advance", "--manifest", str(mp), "--stage", "gate-tol",
                "--override-reason", "人说先看容差", "--human-ref", "tg#120", check=True)
        ov = [x for x in env.journal() if x.get("kind") == "override"]
        assert len(ov) == 1 and ov[0]["target"] == "gate-tol" and ov[0]["human_ref"] == "tg#120", ov
        st = env.task(mp)
        assert st["completion"] == "human-override" and st["pipe_state"] == "TESTS_OK", st
        assert st["approvals"]["tests"].startswith("无") and st["pipe_next"].startswith("⛔"), "被跳过的审批必须保持缺失"
        assert env.run(TASK_CLI, "gate-active", "--manifest", str(mp)).returncode == 0   # 只读诊断,不算越阶
        assert not [x for x in env.journal() if x.get("kind") == "override" and x.get("target") == "gate-active"]
        results = [x["result"] for x in env.journal() if x.get("kind") == "cli_action" and x.get("command") == "advance"]
        assert results == ["ok", "failed", "overridden"], results


def test_manifest_scope_change_invalidates_tests_approval_and_gate_active():
    with tempfile.TemporaryDirectory(prefix="sab_2cli_") as s:
        env = Env(Path(s)); ids = ["energy"]
        mp = write_manifest(env, ids); build_leaf(env, ids); cut(env)
        env.simulate_docker_gate()
        env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True)
        env.approve("tests", "tg#101")
        st = env.task(mp)
        assert st["pipe_state"] == "TESTS_APPROVED" and st["approvals"]["tests"].startswith("✅"), st
        fp0 = st["manifest_scope_fp"]
        write_manifest(env, ["energy", "mass"], ref="tg#130")     # 人改了 scope:多一行期望 check
        st = env.task(mp)
        assert st["manifest_scope_fp"] != fp0 and st["pipe_state"] == "ENV_OK" and st["pipe_next"] == "gate-tests", st
        assert st["approvals"]["tests"].startswith("无"), "scope 变了,旧的 tests 审批必须作废"
        r = env.run(TASK_CLI, "gate-active", "--manifest", str(mp))
        assert r.returncode == 1 and "mass" in r.stdout, r.stdout


def test_gate_final_rejects_undisclosed_check():
    with tempfile.TemporaryDirectory(prefix="sab_2cli_") as s:
        env = Env(Path(s)); ids = ["energy"]
        mp = write_manifest(env, ids); d = build_leaf(env, ids); cut(env)
        add_check(d, "invented", hashlib.sha256(b"def test(): pass\n").hexdigest())   # 溯源合法,但 manifest 没批
        env.simulate_docker_gate()
        env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True); env.approve("tests", "tg#101")
        env.run(TASK_CLI, "advance", "--manifest", str(mp), check=True); env.approve("tolerance", "tg#102")
        assert env.run(TASK_CLI, "advance", "--manifest", str(mp)).returncode != 0
        g = [x for x in env.journal() if x.get("kind") == "gate_final"][-1]
        assert not g["ok"] and "all-active" in g["fails"] and "invented" in g["detail"], g
        assert env.task(mp)["lifecycle"] == "BUILDING"


def test_codebase_cli_emits_manifest_from_approved_cut():
    with tempfile.TemporaryDirectory(prefix="sab_2cli_") as s:
        env = Env(Path(s)); cut(env, official=("tests/test_a.py", "tests/test_b.py"))
        env.run(CODEBASE_CLI, "emit-manifest", "--codebase", CB, "--task", TASK, "--human-ref", "tg#140",
                "--from-official-tests", "--check", "id=edge,source=custom,justification=官方没测边界,human_disclosed=true",
                check=True)
        m = json.loads((env.manifests / CB / f"{TASK}.manifest.json").read_text())
        assert m["expected_denominator"] == 3 and m["path"] == f"tasks/{CB}/{TASK}/" and m["human_approval_ref"] == "tg#140"
        assert [c["id"] for c in m["checks"]] == ["edge", "test-a", "test-b"]
        r = env.run(CODEBASE_CLI, "emit-manifest", "--codebase", CB, "--task", TASK, "--human-ref", "tg#141",
                    "--check", "id=sneaky,source=custom,justification=x")
        assert r.returncode != 0 and "human_disclosed" in (r.stdout + r.stderr), "未披露的 custom 不许进 manifest"
        out = env.run(CODEBASE_CLI, "status", "--codebase", CB, check=True).stdout
        assert TASK in out and "task_cli" in out


if __name__ == "__main__":
    bad = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_"):
            try:
                fn(); print(f"  ✓ {name}")
            except Exception as e:                # noqa: BLE001  (测试运行器:任何异常都算失败)
                bad += 1; print(f"  ✗ {name}: {type(e).__name__}: {e}")
    sys.exit(1 if bad else 0)
