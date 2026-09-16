"""Status across codebases and tasks, the CI freshness gate, and validate-harbor."""
from __future__ import annotations

import json
from pathlib import Path

from . import config, harbor_validate
from .lint import lint
from .runplan import compute_plan, consent_matches, consent_path, review_dir
from .util import approved_modules, contract_fingerprint, die, generated_paths, leaf_of, read_json, rel, task_codebase


def task_status(leaf: Path, allow_custom: bool) -> dict:
    errs, warns, infos = lint(leaf, allow_custom)
    sv = leaf / "comment" / "pipeline" / "self-validation.json"
    state = {"task": rel(leaf), "checks": len(infos), "lint_errors": len(errs), "lint_warnings": len(warns), "self_validation": None}
    state["generated_files"] = [rel(p) for p in generated_paths(leaf)]
    fp = contract_fingerprint(leaf)
    if sv.is_file():
        doc = read_json(sv)
        state["self_validation"] = {"result": doc.get("result"), "at": doc.get("finished_at"), "budget": doc.get("budget"),
                                    "fresh": doc.get("contract_fingerprint") == fp, "consent": doc.get("consent")}
    state_known = (config.PIPE / task_codebase(leaf) / "codebase.json").is_file()
    state["consent"] = None
    c = consent_path(leaf)
    if c.is_file() and not errs:
        rec = read_json(c)
        ok, why = consent_matches(rec, compute_plan(leaf, infos))
        state["consent"] = {"where": rec.get("where"), "at": rec.get("at"), "valid": ok, "why": why}
    rj = review_dir(leaf) / f"{leaf.name}.json"
    state["review_brief"] = None
    if rj.is_file():
        rd = read_json(rj)
        state["review_brief"] = {"at": rd.get("at"), "fresh": rd.get("contract_fingerprint") == fp}
    if not infos:
        nxt = f"sab.py task add-check --task {rel(leaf)} ... (one per suitable test)"
    elif errs:
        nxt = f"sab.py task lint --task {rel(leaf)}  (fix the {len(errs)} error(s))"
    elif state["generated_files"]:
        nxt = f"remove the {len(state['generated_files'])} generated file(s) listed under generated_files (selfcheck refuses them; a commit never carries them)"
    elif state["self_validation"] is None or not state["self_validation"]["fresh"]:
        # A run is needed: consent comes first. A consent given here for another host reads as invalid on this
        # machine by design (the run happens there), so the stop is reported only when no run has been made under it.
        if state_known and not (state["consent"] and state["consent"]["valid"]):
            nxt = f"STOP 3 (consent): sab.py task plan --task {rel(leaf)}; show the run plan to the human, then sab.py task consent ..."
        else:
            nxt = f"sab.py task build --task {rel(leaf)}; then sab.py task selfcheck --task {rel(leaf)}  (self-validation missing or stale)"
    elif state["self_validation"]["result"] != "passed":
        nxt = f"STOP 4 (finalisation): revise policy/tolerance/window/variant with the human, then sab.py task selfcheck --task {rel(leaf)}  (last run was calibration)"
    elif state_known and not (state["review_brief"] and state["review_brief"]["fresh"]):
        nxt = f"write comment/README.md, then sab.py task review --task {rel(leaf)}  (the review brief, STOP 5)"
    else:
        nxt = "PR-ready: on the human's go, open the task PR with the review brief as its body; the review phase follows"
    state["next"] = nxt
    return state


def cmd_status(a) -> None:
    if a.task:
        st = task_status(leaf_of(a.task), a.allow_custom_drivers)
        print(json.dumps(st, indent=2))
        if a.ci_freshness:
            sv = st.get("self_validation")
            if sv is None:
                die(f"freshness gate: {st['task']} has no self-validation record")
            if not sv.get("fresh"):
                die(f"freshness gate: the self-validation record of {st['task']} ({sv.get('at')}) is stale against the contract files in this tree; rerun selfcheck")
            if sv.get("result") != "passed":
                print(f"::warning::{st['task']}: the self-validation record is a calibration run, not a pass")
            print(f"freshness gate: {st['task']} ok")
        return
    ids = [a.codebase] if a.codebase else sorted(p.name for p in config.PIPE.glob("*") if (p / "codebase.json").is_file())
    if not ids:
        print(f"no codebases under {config.PIPE}; start with: sab.py codebase init --codebase <id> --code-path <checkout> ...")
        return
    for cb_id in ids:
        d = config.PIPE / cb_id
        cb = read_json(d / "codebase.json")
        mdoc = read_json(d / "modules.json") if (d / "modules.json").is_file() else None
        approved = approved_modules(mdoc)
        report_dir = config.ROOT / "codebase-reports" / cb_id
        report_files = {name: (report_dir / name).is_file() for name in
                        ("codebase-metadata.json", "codebase-metadata.html", "codebase-metadata.md")}
        line = {"codebase": cb_id, "source": f"code/{cb['source']}", "vendored": (config.ROOT / "code" / cb["source"]).is_dir(),
                "source_merged": (cb.get("source_pr") or {}).get("merge_commit"),
                "overview": (d / "overview.md").is_file(), "build_and_run": (d / "runs.json").is_file(),
                "modules_proposed": len(mdoc["modules"]) if mdoc else 0,
                "modules_approved": approved,
                "metadata_report": {"informational": True, "non_blocking": True, "files": report_files,
                                    "complete": all(report_files.values())},
                "survey": (d / "tests.json").is_file(), "tasks": {}}
        for m in approved:
            leaf = config.ROOT / "tasks" / cb_id / m
            line["tasks"][m] = task_status(leaf, True)["next"] if (leaf / "task.toml").is_file() else "not scaffolded"
        if not line["overview"] or mdoc is None:
            nxt = (f"Step 1: investigate; Step 1.2: build natively, actually run tests and examples, write runs.json, sab.py codebase build-and-run --codebase {cb_id}"
                   f"{' (DONE)' if line['build_and_run'] else ' (NOT DONE, strongly advised against skipping)'}; then sab.py codebase propose-modules --codebase {cb_id}")
        elif not line["build_and_run"] and not (cb.get("source_pr") or {}).get("human_ref"):
            nxt = (f"Step 1.2 (strongly advised, before the report and the source PR): build natively, actually run tests and examples, "
                   f"record the landscape and pitfalls in {d / 'runs.json'}, then sab.py codebase build-and-run --codebase {cb_id}")
        elif not approved:
            nxt = f"STOP 1: a multi-module cut awaits the human's approval (sab.py codebase approve-modules --codebase {cb_id} --human-ref ...); a single-module cut is recorded by propose-modules"
        elif not (cb.get("source_pr") or {}).get("human_ref") and not line["metadata_report"]["complete"]:
            nxt = (f"Step 1.5 informational (non-blocking): recommended fill {d / 'codebase-metadata.json'}, run "
                   f"sab.py codebase report --codebase {cb_id}, and present its HTML plus bounded Markdown to the human; "
                   f"or explicitly report the omission and proceed directly to the source PR for code/{cb['source']}/ now. "
                   f"In either case STOP for human merge, then record sab.py codebase source-merged --codebase {cb_id} --human-ref ...")
        elif not (cb.get("source_pr") or {}).get("human_ref"):
            nxt = (f"Step 1.5 presentation: show the human codebase-reports/{cb_id}/codebase-metadata.html and the bounded Markdown if not already shown. "
                   f"Then STOP (source gate): open the source PR that adds code/{cb['source']}/ and codebase-reports/{cb_id}/, "
                   f"wait for the human to merge it, then sab.py codebase source-merged --codebase {cb_id} --human-ref ...")
        elif not line["survey"]:
            nxt = f"Step 2: sab.py codebase survey-tests --codebase {cb_id}"
        else:
            pending = [m for m, s in line["tasks"].items() if not s.startswith("PR-ready")]
            nxt = f"Step 3 per module: {pending}" if pending else "all approved modules are PR-ready or in review"
        line["next"] = nxt
        print(json.dumps(line, indent=2))


def cmd_validate_harbor(a) -> None:
    argv = list(a.task or [])
    if a.tasks_dir:
        argv += ["--all", a.tasks_dir]
    raise SystemExit(harbor_validate.main(argv))
