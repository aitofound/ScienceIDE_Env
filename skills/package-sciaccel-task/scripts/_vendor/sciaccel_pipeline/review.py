"""STOP 5: the review presentation and the review brief, the body of the task PR.

The presentation (the six-line header and the one table with a row per check)
is built by presentation(), shared with the reviewer's audit (reviewer.py) so
both sides of a review read the same numbers from the same records.
"""
from __future__ import annotations

from pathlib import Path

from . import config
from .lint import lint
from .runplan import review_dir
from .util import contract_fingerprint, graded_identical, leaf_of, next_line, now, read_json, rel, task_codebase, task_meta, write_json


def num(x, fmt=".3g"):
    return format(x, fmt) if isinstance(x, (int, float)) and not isinstance(x, bool) else "-"


def rubric_tolerance(comp: dict) -> str:
    """The bound as the validator applies it, in one cell."""
    if isinstance(comp.get("invariants"), list):
        return "; ".join(f"{q.get('name')}: " + ", ".join(f"{k}={q[k]:g}" for k in ("rtol", "atol", "max_relative_drift") if isinstance(q.get(k), (int, float))) for q in comp["invariants"]) or "see rubric"
    tol = ", ".join(f"{k}={comp[k]:g}" for k in ("atol", "rtol") if isinstance(comp.get(k), (int, float))) or "see rubric"
    groups = [f for f in (comp.get("files") or []) if isinstance(f, dict) and ("atol" in f or "rtol" in f)]
    if groups:
        tol += "; " + "; ".join(f"{g.get('label') or g.get('path')}: " + ", ".join(f"{k}={g[k]:g}" for k in ("atol", "rtol") if isinstance(g.get(k), (int, float))) for g in groups)
    return tol


def check_row(leaf: Path, info: dict, rows: dict, times: dict, run_times: dict, builds: dict) -> dict:
    """One check's presentation row as data: every number from the rubric or the record."""
    name = info["name"]
    rp = leaf / "tests" / "checks" / name / "rubric.json"
    rb = read_json(rp) if rp.is_file() else {}
    comp = rb.get("comparison") if isinstance(rb.get("comparison"), dict) else {}
    ev = rb.get("evidence") if isinstance(rb.get("evidence"), dict) else {}
    spread = ev.get("self_validation_spread")
    spread_v = spread if isinstance(spread, (int, float)) else (spread.get("distance") if isinstance(spread, dict) else None)
    # margin: the bound over the worst graded value's error, from the validator's bound_fraction (selfcheck records it as
    # evidence.self_validation_bound_fraction); None where the validator predates 5.10.0 and reports none, inf when the runs are identical
    bf = ev.get("self_validation_bound_fraction")
    bf_v = bf if isinstance(bf, (int, float)) and not isinstance(bf, bool) else (bf.get("value") if isinstance(bf, dict) else None)
    margin = None
    if isinstance(bf_v, (int, float)) and not isinstance(bf_v, bool):
        margin = (1.0 / bf_v) if bf_v > 0 else float("inf")
    relbound = False
    floor = ev.get("floor") if ev.get("floor") is not None else ev.get("spread")
    try:
        floor = float(floor) if floor is not None and not isinstance(floor, dict) else floor
    except (TypeError, ValueError):
        pass
    r = rows.get(name) or {}
    labels = [x for x in (info.get("labels") or [])]
    return {"name": name, "upstream_test": rb.get("upstream_test") or "", "policy": rb.get("policy"), "chaotic": bool(rb.get("chaotic")),
            "labels": labels, "observable": rb.get("observable") or "-", "tolerance": rubric_tolerance(comp), "spread": spread_v,
            "rubric_spread": spread, "margin": margin, "relbound": bool(relbound), "floor": floor,
            "variant": rb.get("variant") or "", "default_vs_upstream": rb.get("default_vs_upstream") or "-",
            "run_s": run_times.get(name, times.get(name, 0)), "build_s": builds.get(name, 0), "identical": bool(r.get("identical")),
            "graded_identical": graded_identical(r),
            "record_distance": r.get("distance"), "passed": r.get("passed"), "expected_runtime_s": info.get("expected_runtime_s"),
            "altbuild": ev.get("altbuild") if isinstance(ev.get("altbuild"), dict) else None,
            "altbuild_declared": rb.get("altbuild") if isinstance(rb.get("altbuild"), str) else None}


def format_row(row: dict) -> str:
    pol = f"{row['policy']}" + ("; chaotic" if row["chaotic"] else "") + ("; " + ", ".join(row["labels"]) if row["labels"] else "")
    variant = row["variant"].split(";")[0].split(". ")[0][:90]
    obs = row["observable"][:100]
    margin = row["margin"]
    return (f"| {row['name']} ({row['upstream_test'].split('/')[-1]}) | {pol} | {obs} | {row['tolerance']} | {num(row['spread'])} | "
            f"{('identical' if margin == float('inf') else num(margin, '.0f') + 'x') if margin is not None else 'not reported'} | {num(row['floor'])} | {variant} | {row['default_vs_upstream']} | "
            f"{row['run_s']:.0f} | {row['build_s']:.0f} | {identical_word(row['identical'], row['graded_identical'])} |")


def identical_word(byte_identical: bool, graded: bool) -> str:
    """The identical column: YES when every file is byte-identical, 'graded' when every graded value is identical
    (validator distance 0) while an ungraded file differs, otherwise no."""
    return "YES" if byte_identical else ("graded" if graded else "no")


TABLE_HEAD = ["| check | policy | observable | tolerance | spread | margin | floor | variant | default vs upstream | run s | build s | identical |",
              "|---|---|---|---|---|---|---|---|---|---|---|---|"]
READING_ORDER = ("Read first: the rows this table flags (margin under 50 or over 10,000, chaotic, custom, identical, run time far from its "
                 "declared value or above 300 s); then the catalogue, the warrants, comment/README.md, the records. identical YES means every output file "
                 "is byte-identical; 'graded' means every graded value is identical (the validator's distance is 0) while an ungraded file "
                 "differs, which reads the same way. The margin is the bound divided by the "
                 "worst graded value's error in the nominal-versus-variant run, from the validator's bound_fraction; 'not reported' means the "
                 "check's validator predates 5.10.0 and the headroom is read in the warrant. The floor column is the CLI's measurement where the "
                 "check declares an altbuild (evidence.altbuild), otherwise the author's.")


def presentation(leaf: Path, allow_custom_drivers: bool) -> tuple[list[str], dict]:
    """The review presentation lines and everything they were computed from."""
    errs, warns, infos = lint(leaf, allow_custom_drivers)
    meta = task_meta(leaf)
    pipeline = leaf / "comment" / "pipeline"
    sv = read_json(pipeline / "self-validation.json") if (pipeline / "self-validation.json").is_file() else None
    rows = ((sv or {}).get("reward") or {}).get("checks") or {}
    times = ((sv or {}).get("solves") or [{}])[0].get("check_seconds") or {}
    run_times = (sv or {}).get("check_run_seconds_nominal") or {}
    builds = ((sv or {}).get("solves") or [{}])[0].get("build_seconds") or {}
    fp = contract_fingerprint(leaf)
    fresh = bool(sv) and sv.get("contract_fingerprint") == fp
    flags = []
    suitable = None
    ts_path = pipeline / "test-survey.json"
    if ts_path.is_file():
        suitable = sum(1 for x in (read_json(ts_path).get("tests") or []) if x.get("suitable"))
    customs = [i["name"] for i in infos if "custom" in (i.get("labels") or [])]
    if customs:
        flags.append("custom: " + ", ".join(customs))
    longs = [i["name"] for i in infos if run_times.get(i["name"], times.get(i["name"], 0)) > config.CHECK_RUNTIME_ADVISED_S]
    if longs:
        flags.append(f"run time above {config.CHECK_RUNTIME_ADVISED_S} s: " + ", ".join(longs) + " (the rubric's runtime_note says why)")
    rw = (sv or {}).get("reward") or {}
    cons = (sv or {}).get("consent") or {}
    host = (sv or {}).get("host") or {}
    budget = float(((meta.get("resources") or {}).get("suite_budget_s")) or 900.0)
    cpus = (meta.get("resources") or {}).get("cpus")
    prev = review_dir(leaf) / f"{leaf.name}.json"
    prev_fp = read_json(prev).get("contract_fingerprint") if prev.is_file() else None
    changed = ("first presentation" if not prev_fp else
               ("unchanged contract since the previous presentation" if prev_fp == fp else
                "REVISED since the previous presentation: contract fingerprint changed (the agent states what changed below this header)"))
    alt_rows = ((sv or {}).get("altbuild") or {}).get("checks") or {}
    alt_note = (f"altbuild measured on {len(alt_rows)} of {len(infos)} checks ({sum(1 for v in alt_rows.values() if v.get('passed'))} pass, "
                f"{sum(1 for v in alt_rows.values() if v.get('identical'))} bit-identical, "
                f"{sum(1 for v in alt_rows.values() if graded_identical(v))} identical in every graded value while an ungraded file differs)"
                if alt_rows else "altbuild: none declared (optional)")
    header = [
        f"**Result.** {(sv or {}).get('result') or 'no record'}; reward {rw.get('reward')}; {rw.get('passed')}/{rw.get('total')} checks; identical {rw.get('identical_checks') if sv else '-'}; {alt_note}.",
        f"**Suite.** run time {(sv or {}).get('suite_seconds_nominal') if sv else '-'} s, builds {str((sv or {}).get('build_seconds_nominal')) + ' s' if isinstance((sv or {}).get('build_seconds_nominal'), (int, float)) else 'not reported'}, against {budget:.0f} s (guidance) on {cpus} declared cpus; {(sv or {}).get('budget') or '-'}.",
        f"**Host and consent.** {host.get('hostname') or '-'} ({host.get('arch') or '-'}, {host.get('docker_cpus') or '-'} docker cpus) under consent where={cons.get('where') or '-'} at {cons.get('at') or '-'}.",
        f"**Lint and record.** lint {len(errs)} error(s), {len(warns)} warning(s); record {'fresh' if fresh else 'STALE'}; freshness gate {'ok' if fresh and sv and sv.get('result') == 'passed' else 'not ok'}; CI: see the PR checks.",
        f"**Flags.** {'; '.join(flags) if flags else 'none (no custom checks, every check under 300 s)'}.",
        f"**Since the previous round.** {changed}.",
    ]
    check_rows = [check_row(leaf, i, rows, times, run_times, builds) for i in infos]
    table = TABLE_HEAD + [format_row(r) for r in check_rows]
    present = [f"# Review presentation: {rel(leaf)}", "",
               f"Task `{meta.get('slug')}` of codebase `{meta.get('source')}` ({meta.get('repo_url')} @ {(meta.get('repo_commit') or '')[:12]}); {len(infos)} checks.", ""] + header + [""] + table + ["", READING_ORDER, ""]
    ctx = {"errs": errs, "warns": warns, "infos": infos, "meta": meta, "sv": sv, "rows": rows, "times": times, "run_times": run_times,
           "builds": builds, "fresh": fresh, "fingerprint": fp, "flags": flags, "suitable": suitable, "customs": customs,
           "budget": budget, "cpus": cpus, "check_rows": check_rows, "header": header, "table": table}
    return present, ctx


def cmd_task_review(a) -> None:
    leaf = leaf_of(a.task)
    present, ctx = presentation(leaf, a.allow_custom_drivers)
    errs, warns, infos, meta, sv = ctx["errs"], ctx["warns"], ctx["infos"], ctx["meta"], ctx["sv"]
    rows, times, run_times, builds, fresh = ctx["rows"], ctx["times"], ctx["run_times"], ctx["builds"], ctx["fresh"]
    pipeline = leaf / "comment" / "pipeline"
    if getattr(a, "present", False):
        print("\n".join(present))
        return
    lines = present + [f"# Review brief: {rel(leaf)}", "",
             f"Task `{meta.get('slug')}` of codebase `{meta.get('source')}` ({meta.get('repo_url')} @ {(meta.get('repo_commit') or '')[:12]}). "
             f"{len(infos)} checks; lint {len(errs)} error(s), {len(warns)} warning(s); self-validation "
             + (f"{sv.get('result')} at {sv.get('finished_at')}, {'fresh' if fresh else 'STALE against the current contract'}" if sv else "none"), "",
             "## 1. Summary table", "",
             "| check | policy | labels | tolerance | spread (nominal vs variant) | floor | expected s | run s | build s | identical |",
             "|---|---|---|---|---|---|---|---|---|---|"]
    for i in infos:
        rb = read_json(leaf / "tests" / "checks" / i["name"] / "rubric.json") if (leaf / "tests" / "checks" / i["name"] / "rubric.json").is_file() else {}
        comp = rb.get("comparison") if isinstance(rb.get("comparison"), dict) else {}
        tol = ", ".join(f"{k}={comp[k]:g}" for k in ("atol", "rtol") if isinstance(comp.get(k), (int, float))) or "see rubric"
        ev = rb.get("evidence") if isinstance(rb.get("evidence"), dict) else {}
        spread = ev.get("self_validation_spread")
        spread_s = f"{spread:.3g}" if isinstance(spread, (int, float)) else ("(overrides)" if isinstance(spread, dict) else "none")
        floor = ev.get("floor")
        floor_s = f"{floor:.3g}" if isinstance(floor, (int, float)) else "none"
        r = rows.get(i["name"]) or {}
        lines.append(f"| {i['name']} | {rb.get('policy')}{' (chaotic)' if rb.get('chaotic') else ''} | {' '.join(i.get('labels') or []) or '-'} | {tol} | {spread_s} | {floor_s} | "
                     f"{i.get('expected_runtime_s') or '?'} | {run_times.get(i['name'], times.get(i['name'], 0)):.0f} | {builds.get(i['name'], 0):.0f} | {identical_word(bool(r.get('identical')), graded_identical(r))} |")
    ts = pipeline / "test-survey.json"
    if ts.is_file():
        rows_t = (read_json(ts).get("tests") or [])
        suitable = sum(1 for t in rows_t if t.get("suitable"))
        left_out = [t for t in rows_t if not t.get("suitable")]
        unreasoned = [t.get("id") for t in left_out if not str(t.get("why") or "").strip()]
        customs = [i["name"] for i in infos if "custom" in (i.get("labels") or [])]
        lines += ["", f"Survey: {len(rows_t)} distinct official test(s)/example(s) listed, {suitable} suitable (the exhaustive "
                  f"default is one check each), {len(left_out)} left out"
                  + (f", {len(unreasoned)} of them WITHOUT a reason: {', '.join(map(str, unreasoned))}" if unreasoned else " with a reason each")
                  + f"; custom checks: {customs or 'none'}."]
    lines += ["", "## 2. The catalogue (task.toml equivalence_explanation) against the rubrics", "", (meta.get("equivalence_explanation") or "").strip(), "",
              "## 3. Warrants and variants, per check", ""]
    for i in infos:
        rp = leaf / "tests" / "checks" / i["name"] / "rubric.json"
        rb = read_json(rp) if rp.is_file() else {}
        lines += [f"### {i['name']}", "", f"Variant: {rb.get('variant', '')}", "", f"Warrant: {rb.get('warrant', '')}", ""]
    readme = leaf / "comment" / "README.md"
    lines += ["## 4. comment/README.md: module boundary, tolerance story, blind spots", "",
              readme.read_text(encoding="utf-8").strip() if readme.is_file() else "(comment/README.md is missing)", "",
              "## 5. Self-validation record", ""]
    if sv:
        rw = sv.get("reward") or {}
        cons = sv.get("consent") or {}
        host = sv.get("host") or {}
        where = cons.get("where")
        lines += [f"Result {sv.get('result')}, reward {rw.get('reward')}, {rw.get('passed')}/{rw.get('total')} checks, identical checks {rw.get('identical_checks')}. "
                  f"Suite run time {sv.get('suite_seconds_nominal')} s nominal ({'builds ' + str(sv.get('build_seconds_nominal')) + ' s excluded' if isinstance(sv.get('build_seconds_nominal'), (int, float)) else 'builds not reported by the checks, counted as run time'}) against the guidance budget {sv.get('budget_s')} s ({sv.get('budget')}). "
                  f"Host: {host.get('hostname')} ({host.get('arch')}, {host.get('ncpu')} cpus, docker {host.get('docker')}, {host.get('docker_cpus')} docker cpus). "
                  f"Consent: where={where} at {cons.get('at')}: {consent_agreement(sv)}. Warnings: {sv.get('warnings')}.", ""]
    else:
        lines += ["No self-validation record.", ""]
    lines += ["## 6. Module and source records", ""]
    mj = pipeline / "module.json"
    if mj.is_file():
        md = read_json(mj)
        mm = md.get("module") if isinstance(md.get("module"), dict) else md
        appr = md.get("approval") or {}
        lines.append(f"Module `{mm.get('slug')}` approved {appr.get('at')}: \"{appr.get('human_ref')}\"; owns {mm.get('paths')}.")
    cbj = config.PIPE / task_codebase(leaf) / "codebase.json"
    if cbj.is_file():
        sp = read_json(cbj).get("source_pr") or {}
        lines.append(f"Source PR {sp.get('pr') or '?'} merged at {(sp.get('merge_commit') or '?')[:12]}: \"{sp.get('human_ref')}\".")
    lines += ["", "Reviewers: the review phase is extensive by design; reproduce with `sab.py task selfcheck` on your machine, request changes, "
              "or redesign the checks with this PR as a priori information. CI runs the structural validator and the freshness gate."]
    text = "\n".join(lines) + "\n"
    print(text)
    rd = review_dir(leaf)
    rd.mkdir(parents=True, exist_ok=True)
    (rd / f"{leaf.name}.md").write_text(text, encoding="utf-8")
    write_json(rd / f"{leaf.name}.json", {"task": rel(leaf), "at": now(), "contract_fingerprint": contract_fingerprint(leaf)})
    print(f"(kept in {rd / (leaf.name + '.md')}; this is the body of the task PR)")
    if not sv or not fresh or sv.get("result") != "passed":
        print("note: not PR-ready: a passing, fresh self-validation is required first")
    next_line("show the brief to the human (STOP 5); on their go, open the task PR with it as the body")


def consent_agreement(sv: dict) -> str:
    """Whether the recorded host agrees with the consent it ran under, in words."""
    cons = sv.get("consent") or {}
    host = sv.get("host") or {}
    where = cons.get("where")
    if where == "local":
        return "the run happened on the consenting machine" if host.get("hostname") == cons.get("consented_on") else "the hostname differs from the consenting machine"
    if where:
        return f"the run happened on {host.get('hostname')} under a consent for {where}; a reviewer checks they are the same host"
    return "no consent recorded with this run"
