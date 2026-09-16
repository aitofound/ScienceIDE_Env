"""Review mode: the reviewer's side of the two review stops, one brief each.

    sab.py review codebase --codebase <id> [--root <checkout>] [--modules <modules.json>] [--base <ref>] [--upstream <checkout>]
    sab.py review task     --task <leaf>   [--root <checkout>] [--base <ref>] [--allow-custom-drivers]
    sab.py review codebase|task ... --done --human-ref "<the human's words>" [--presented <file.md>] [--rerun-ref "<their words on the proposed rerun>"]
    sab.py review status

One command per human stop (STOP 2, the source PR; STOP 6, the task PR).
Each prints what the CLI owns (the checkout, the change set, the tree, and
for a task the presentation table, lint, validate-harbor and the record's
freshness), then the brief: what to gather, the fixed shape to present it
in, what to ask. The agent gathers and presents; the human decides; the
words are recorded with --done. Two states, open and decided. The CLI reads
no GitHub state, posts nothing, runs no Docker, and renders no judgement.
"""
from __future__ import annotations

import hashlib
import re
import shutil
import subprocess
from pathlib import Path

from . import config, harbor_validate
from .metadata import _metadata_owned_files, _metadata_walk
from .privacy import _METADATA_SECRET_VALUE
from .review import presentation
from .util import die, leaf_of, next_line, now, read_json, rel, state_dir, task_codebase, write_json

LICENCE_NAMES = ("LICENSE", "LICENCE", "COPYING", "COPYRIGHT")
SCAN_MAX_BYTES = 2_000_000
MARGIN_LOW, MARGIN_HIGH = 50, 10_000   # the presentation's reading-order flags (SPEC §4.3), not a pass rule


# ---------------------------------------------------------------- shared

def anchor(a) -> None:
    """--root points the current CLI at a detached checkout of the PR head."""
    root = getattr(a, "root", None)
    if root:
        config.ROOT = Path(root).resolve()
        if not config.ROOT.is_dir():
            die(f"--root is not a directory: {config.ROOT}")


def git(*args: str) -> str | None:
    try:
        proc = subprocess.run(["git", "-C", str(config.ROOT), *args], capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.TimeoutExpired):
        return None
    return proc.stdout.strip() if proc.returncode == 0 else None


def checkout_facts(paths: list[str], base: str) -> dict:
    """Head, base and the change set of the checkout, from git alone; None where git cannot say."""
    head = git("rev-parse", "HEAD")
    facts = {"root": str(config.ROOT), "head": head, "base_ref": base, "base": None,
             "changed": None, "changed_inside": None, "changed_outside": None, "dirty": None, "untracked": None}
    if head is None:
        return facts
    facts["base"] = git("rev-parse", base)
    if facts["base"]:
        diff = git("diff", "--name-only", f"{base}...HEAD")
        if diff is not None:
            changed = [ln for ln in diff.splitlines() if ln]
            inside = [p for p in changed if any(p == q or p.startswith(q.rstrip("/") + "/") for q in paths)]
            facts.update(changed=changed, changed_inside=inside, changed_outside=[p for p in changed if p not in inside])
    status = git("status", "--porcelain", "--", *paths)
    if status is not None:
        lines = [ln for ln in status.splitlines() if ln]
        facts["dirty"] = [ln[3:] for ln in lines if not ln.startswith("??")]
        facts["untracked"] = [ln[3:] for ln in lines if ln.startswith("??")]
    return facts


def checkout_line(co: dict, inside: str) -> str:
    if co["head"] is None:
        return f"**Checkout.** {co['root']} is not a git checkout: head, base and the change set are unknown."
    base = f"{co['base_ref']} `{co['base'][:12]}`" if co["base"] else f"{co['base_ref']} (not found: pass --base)"
    clean = "yes" if co["dirty"] == [] and co["untracked"] == [] else ("unknown" if co["dirty"] is None else "NO")
    line = (f"**Checkout.** head `{co['head'][:12]}`, base {base}; tree unchanged since the head: {clean}"
            + (f" (modified {listing(co['dirty'])}; untracked {listing(co['untracked'])})" if clean == "NO" else "") + ".")
    if co["changed"] is not None:
        line += (f"\n**Change.** {len(co['changed'])} file(s) against the base: {len(co['changed_inside'])} under `{inside}`, "
                 f"{len(co['changed_outside'])} outside" + (": " + listing(co["changed_outside"]) if co["changed_outside"] else "") + ".")
    return line


def listing(items: list[str], limit: int = 20) -> str:
    if not items:
        return "none"
    shown = ", ".join(f"`{p}`" for p in items[:limit])
    return shown + (f" and {len(items) - limit} more" if len(items) > limit else "")


def fmt_bytes(n: int) -> str:
    for unit, div in (("GB", 1e9), ("MB", 1e6), ("kB", 1e3)):
        if n >= div:
            return f"{n / div:.1f} {unit}"
    return f"{n} B"


def skill_version_in_tree() -> str | None:
    p = config.ROOT / "skills" / "package-sciaccel-task" / "SKILL.md"
    if not p.is_file():
        return None
    m = re.search(r"^version:\s*([0-9][^\s]*)", p.read_text(encoding="utf-8", errors="replace"), re.M)
    return m.group(1) if m else None


def brief_text(name: str, **tokens) -> str:
    return (config.TEMPLATES / f"review-{name}.md").read_text(encoding="utf-8").format(revision=config.REVISION, **tokens)


def record_path(kind: str, cb_id: str, name: str) -> Path:
    return state_dir(cb_id) / "reviewer" / f"{kind}-{name}.json"


def open_record(path: Path, doc: dict) -> dict:
    """The review record: opened on the first run, kept across runs, closed by --done."""
    rec = read_json(path) if path.is_file() else {"opened_at": now(), "runs": []}
    rec.update(doc)
    rec["runs"] = (rec.get("runs") or [])[-9:] + [{"at": now(), "head": doc.get("head")}]
    write_json(path, rec)
    return rec


def record_decision(a, path: Path, doc: dict) -> None:
    if not (a.human_ref or "").strip():
        die("--done needs --human-ref: the human's words, verbatim")
    rec = read_json(path) if path.is_file() else {"opened_at": now(), "runs": []}
    rec.update(doc)
    presented = None
    if a.presented:
        src = Path(a.presented)
        if not src.is_file():
            die(f"--presented file not found: {src}")
        presented = path.with_suffix(".presented.md")
        shutil.copyfile(src, presented)
    rerun_ref = (getattr(a, "rerun_ref", None) or "").strip() or None
    rec["decision"] = {"human_ref": a.human_ref, "at": now(), "head": doc.get("head"), "presented": str(presented) if presented else None,
                       "rerun": {"human_ref": rerun_ref, "at": now()} if rerun_ref else None}
    write_json(path, rec)
    print(f"decision recorded for {doc.get('review')} at head {(doc.get('head') or '?')[:12]}: \"{a.human_ref}\"" + (f"; presentation kept at {presented}" if presented else ""))
    print(f"rerun: {'approved in the human\'s words: ' + chr(34) + rerun_ref + chr(34) if rerun_ref else 'none approved (no --rerun-ref); nothing runs'}")
    print(f"(record: {path}; the curator posts the words and the presentation on the PR, verbatim)")
    next_line("sab.py review status")


# ---------------------------------------------------------------- codebase

def content_hash(path: Path) -> str:
    h = hashlib.sha256()
    try:
        with path.open("rb") as stream:
            while block := stream.read(1024 * 1024):
                h.update(block)
    except OSError:
        return "<unreadable>"
    return h.hexdigest()


def tree_facts(tree: Path) -> dict:
    """The vendored tree as the filesystem shows it; every number measured."""
    files, directories, symlinks, warnings = _metadata_walk(tree)
    by_ext: dict[str, dict] = {}
    for relative, _, size, lines in files:
        ext = Path(relative).suffix.lower() or "(none)"
        e = by_ext.setdefault(ext, {"files": 0, "bytes": 0, "lines": 0})
        e["files"] += 1
        e["bytes"] += size
        e["lines"] += lines or 0
    nontext = [(r, s) for r, _, s, lines in files if lines is None]
    licences = sorted(r for r, _, _, _ in files if Path(r).name.upper().split(".")[0] in LICENCE_NAMES)
    secret_hits = []
    for relative, path, size, lines in files:
        if lines is None or size > SCAN_MAX_BYTES:
            continue
        try:
            if _METADATA_SECRET_VALUE.search(path.read_text(encoding="utf-8", errors="replace")):
                secret_hits.append(relative)
        except OSError:
            pass
    hashes = {r: content_hash(p) for r, p, _, _ in files}
    h = hashlib.sha256()
    for relative in sorted(hashes):
        h.update(b"F\0" + relative.encode("utf-8") + b"\0" + hashes[relative].encode())
    return {"exists": tree.is_dir(), "files": len(files), "directories": directories, "bytes": sum(s for _, _, s, _ in files),
            "lines": sum(l or 0 for _, _, _, l in files), "nontext_files": len(nontext), "nontext_bytes": sum(s for _, s in nontext),
            "largest_nontext": sorted(nontext, key=lambda t: -t[1])[:5],
            "symlinks": symlinks, "nested_repositories": [w.split(": ", 1)[1] for w in warnings if w.startswith("repository/cache")],
            "warnings": [w for w in warnings if not w.startswith("repository/cache")],
            "languages": sorted(((e, v) for e, v in by_ext.items() if v["lines"]), key=lambda kv: -kv[1]["lines"])[:6],
            "licence_files": licences, "root_licence": [r for r in licences if "/" not in r],
            "secret_like_files": secret_hits, "fingerprint": h.hexdigest(),
            "_files": {r: (p, s, l) for r, p, s, l in files}, "_hashes": hashes}


def upstream_diff(vendored: dict, upstream: Path) -> dict:
    files, _, _, _ = _metadata_walk(upstream)
    theirs = {r: content_hash(p) for r, p, _, _ in files}
    ours = vendored["_hashes"]
    added = sorted(set(ours) - set(theirs))
    removed = sorted(set(theirs) - set(ours))
    modified = sorted(r for r in set(ours) & set(theirs) if ours[r] != theirs[r])
    return {"checkout": str(upstream), "upstream_files": len(theirs), "added": added, "removed": removed, "modified": modified,
            "identical": not (added or removed or modified)}


def bucket(names: set[str], file_map: dict) -> dict:
    rows = [file_map[n] for n in names]
    return {"files": len(rows), "bytes": sum(r[1] for r in rows), "lines": sum(r[2] or 0 for r in rows)}


def cut_facts(tree_dir: Path, tree: dict, mdoc: dict) -> dict:
    """The module cut measured against the vendored tree: lines owned, shared, overlapping and unowned."""
    file_map = tree["_files"]
    warnings: list[str] = []
    shared = _metadata_owned_files(tree_dir, list(mdoc.get("shared_infrastructure") or []), file_map, warnings, "shared_infrastructure")
    modules, owned_by = [], {}
    for m in mdoc.get("modules") or []:
        names = _metadata_owned_files(tree_dir, list(m.get("paths") or []), file_map, warnings, m.get("slug") or "?")
        for n in names:
            owned_by.setdefault(n, []).append(m.get("slug") or "?")
        modules.append({"slug": m.get("slug"), "title": m.get("title") or "", "paths": list(m.get("paths") or []), **bucket(names, file_map)})
    overlaps: dict[str, set[str]] = {}
    for n, owners in owned_by.items():
        if len(owners) > 1:
            overlaps.setdefault(" + ".join(sorted(owners)), set()).add(n)
    unowned = set(file_map) - set(owned_by) - shared
    by_top: dict[str, set[str]] = {}
    for n in unowned:
        by_top.setdefault(n.split("/", 1)[0] if "/" in n else ".", set()).add(n)
    approval = mdoc.get("approval") or {}
    return {"modules": modules, "shared": bucket(shared, file_map), "shared_paths": list(mdoc.get("shared_infrastructure") or []),
            "owned": bucket(set(owned_by), file_map), "unowned": bucket(unowned, file_map),
            "unowned_by_top": sorted(((d, bucket(s, file_map)) for d, s in by_top.items()), key=lambda kv: -kv[1]["lines"])[:10],
            "overlaps": {k: sorted(v) for k, v in overlaps.items()}, "not_packaged": list(mdoc.get("not_packaged") or []),
            "approval": {"modules": list(approval.get("modules") or []), "at": approval.get("at"), "human_ref": approval.get("human_ref")},
            "warnings": warnings}


def codebase_page(cb_id: str, source: str, co: dict, tree: dict, cut: dict | None, cut_from: str, up: dict | None) -> list[str]:
    head12 = (co["head"] or "unknown")[:12]
    L = [f"# Codebase review: {cb_id} at {head12}  (STOP 2, the source PR)", "",
         f"Vendored under `code/{source}/`; pipeline revision {config.REVISION}; {now()}.", "", checkout_line(co, f"code/{source}/")]
    if not tree["exists"]:
        return L + [f"**Tree.** `code/{source}/` does not exist in this checkout.", ""]
    langs = ", ".join(f"{e} {v['lines']:,}" for e, v in tree["languages"])
    L.append(f"**Tree.** {tree['files']} files, {tree['lines']:,} lines, {fmt_bytes(tree['bytes'])}; lines by extension: {langs}; "
             f"{tree['nontext_files']} non-text file(s) ({fmt_bytes(tree['nontext_bytes'])})"
             + (": " + ", ".join(f"`{r}` {fmt_bytes(s)}" for r, s in tree["largest_nontext"]) if tree["largest_nontext"] else "") + ".")
    L.append(f"**Licence and hygiene.** licence at the root: {listing(tree['root_licence'])}; licence files in the tree: {len(tree['licence_files'])}; "
             f"nested repositories: {listing(tree['nested_repositories'])}; symlinks: {len(tree['symlinks'])}; secret-like text files: "
             f"{len(tree['secret_like_files'])}" + (" (" + listing(tree["secret_like_files"]) + ")" if tree["secret_like_files"] else "")
             + f"; content fingerprint `{tree['fingerprint'][:16]}`.")
    if up is None:
        L.append("**Upstream.** not compared (pass --upstream <checkout of the upstream repository at the pin>).")
    else:
        L.append(f"**Upstream.** {up['upstream_files']} files at the pin; vendored tree {'identical' if up['identical'] else 'differs'}: "
                 f"{len(up['added'])} added, {len(up['removed'])} removed, {len(up['modified'])} modified.")
        if not up["identical"]:
            L += [f"  added {listing(up['added'])}", f"  removed {listing(up['removed'])}", f"  modified {listing(up['modified'])}"]
    if cut is None:
        L += ["**Cut.** no module cut on this machine: write modules.json from the PR body and pass --modules; the lines per module are then measured.", ""]
        return L
    ap = cut["approval"]
    appr = f"approved {ap['modules']} at {ap['at']}: \"{ap['human_ref']}\"" if ap["modules"] else "no approval recorded in this cut"
    pct = (100.0 * cut["owned"]["lines"] / tree["lines"]) if tree["lines"] else 0.0
    L.append(f"**Cut.** {len(cut['modules'])} module(s) from {cut_from}; {appr}. Owned {cut['owned']['lines']:,} lines ({pct:.0f}% of the tree) "
             f"in {cut['owned']['files']} files; shared infrastructure {cut['shared']['lines']:,} lines in {cut['shared']['files']} files; "
             f"unowned {cut['unowned']['lines']:,} lines in {cut['unowned']['files']} files; overlaps between modules: "
             + ("; ".join(f"{k}: {len(v)} file(s)" for k, v in cut["overlaps"].items()) if cut["overlaps"] else "none") + ".")
    L += ["", "| module | title | owned paths | files | lines |", "|---|---|---|---:|---:|"]
    for m in sorted(cut["modules"], key=lambda m: -m["lines"]):
        L.append(f"| `{m['slug']}` | {m['title']} | {listing(m['paths'], 6)} | {m['files']} | {m['lines']:,} |")
    L.append(f"| shared infrastructure | | {listing(cut['shared_paths'], 6)} | {cut['shared']['files']} | {cut['shared']['lines']:,} |")
    for d, b in cut["unowned_by_top"]:
        L.append(f"| unowned | | `{d}` | {b['files']} | {b['lines']:,} |")
    if cut["not_packaged"]:
        L += ["", "| not packaged | why |", "|---|---|"] + [f"| {n.get('what', '')} | {n.get('why', '')} |" for n in cut["not_packaged"]]
    for k, v in cut["overlaps"].items():
        L.append(f"warn  overlap {k}: {listing(v)}")
    for w in cut["warnings"] + tree["warnings"]:
        L.append(f"warn  {w}")
    return L + [""]


def cmd_review_codebase(a) -> None:
    anchor(a)
    cb_id = a.codebase
    if config.KEBAB.fullmatch(cb_id) is None:
        die("--codebase must be lower-kebab-case")
    state = read_json(config.PIPE / cb_id / "codebase.json") if (config.PIPE / cb_id / "codebase.json").is_file() else {}
    source = a.source or state.get("source") or cb_id
    co = checkout_facts([f"code/{source}"], a.base)
    rec_path = record_path("codebase", cb_id, source)
    doc = {"review": f"codebase {cb_id}", "kind": "codebase", "codebase": cb_id, "source": source, "head": co["head"], "root": co["root"]}
    if a.done:
        record_decision(a, rec_path, doc)
        return
    tree = tree_facts(config.ROOT / "code" / source)
    mdoc, cut_from = None, ""
    if a.modules:
        p = Path(a.modules)
        if not p.is_file():
            die(f"--modules file not found: {p}")
        mdoc, cut_from = read_json(p), f"`{p}`"
    elif (config.PIPE / cb_id / "modules.json").is_file():
        mdoc, cut_from = read_json(config.PIPE / cb_id / "modules.json"), "the local codebase state"
    cut = cut_facts(config.ROOT / "code" / source, tree, mdoc) if (mdoc and tree["exists"]) else None
    up = upstream_diff(tree, Path(a.upstream).resolve()) if a.upstream else None
    page = codebase_page(cb_id, source, co, tree, cut, cut_from, up)
    text = "\n".join(page) + "\n"
    from .present import build_page, render_page_text
    report_path = config.ROOT / "codebase-reports" / cb_id / "codebase-metadata.json"
    if report_path.is_file():
        page_text = render_page_text(build_page(read_json(report_path)),
                                     "merge, send back, or change the cut; show the human this page first, then the block below")
    else:
        page_text = (f"NO CODEBASE PAGE: this checkout has no codebase-reports/{cb_id}/codebase-metadata.json, so the PR carries no report "
                     "and nothing here says what the code does, how big it is, or whether it was built and run. Ask for the report "
                     "(skill 5.15.0 or later) before reading the tree.\n")
    text = page_text + "\n" + text
    print(brief_text("preamble", what=f"{cb_id}  (STOP 2, the source PR)", decisions="merge, send back, or change the cut"))
    print(text)
    print(brief_text("codebase", codebase=cb_id, source=source))
    rec = open_record(rec_path, doc)
    rec_path.with_suffix(".md").write_text(text, encoding="utf-8")
    write_json(rec_path.with_suffix(".page.json"), {"at": now(), "checkout": co, "tree": {k: v for k, v in tree.items() if not k.startswith("_")},
                                                    "cut": cut, "upstream": up})
    print(f"(page kept at {rec_path.with_suffix('.md')}; review opened {rec['opened_at']})")
    next_line(f"present as above, then sab.py review codebase --codebase {cb_id} --done --human-ref \"<their words>\"")


# ---------------------------------------------------------------- task

def flagged_rows(ctx: dict) -> list[str]:
    """The rows the presentation flags, with the reason, from the same data the table was built from."""
    out = []
    for r in ctx["check_rows"]:
        why = []
        m = r["margin"]
        if isinstance(m, (int, float)) and m != float("inf"):
            if m < MARGIN_LOW:
                why.append(f"margin {m:.0f}x under {MARGIN_LOW}")
            elif m > MARGIN_HIGH:
                why.append(f"margin {m:.0f}x over {MARGIN_HIGH:,}")
        if r["chaotic"]:
            why.append("chaotic")
        for lab in r["labels"]:
            why.append(lab)
        if r["identical"] or r.get("graded_identical"):
            how = "byte-identical" if r["identical"] else "every graded value identical while an ungraded file differs"
            why.append(f"{how}: variant inactive" if not r["variant"].strip().lower().startswith("identical") else f"{how}, as the rubric declares")
        exp, got = r["expected_runtime_s"], r["run_s"]
        # selfcheck's own rule (measured above twice the declared value), and its mirror for a stale
        # over-declaration, which is read only when the measured run time is at least a second.
        if isinstance(exp, (int, float)) and isinstance(got, (int, float)) and (got > 2 * exp or (got >= 1 and exp > 2 * got)):
            why.append(f"run time {got:.0f} s against declared {exp:.0f} s")
        rd, rs = r["record_distance"], r["rubric_spread"]
        if isinstance(rd, (int, float)) and isinstance(rs, (int, float)) and rd != rs:
            why.append(f"rubric spread {rs:.3g} differs from the record's {rd:.3g}")
        if why:
            out.append(f"  {r['name']}: " + "; ".join(why))
    return out


def coverage_facts(leaf: Path, ctx: dict) -> list[str]:
    """Provenance and coverage, from the rubrics and the survey copy in the leaf: never typed."""
    infos = ctx["infos"]
    names = {i["name"] for i in infos}
    upstream, custom = [], []
    for r in ctx["check_rows"]:
        (custom if "custom" in r["labels"] or not r["upstream_test"] else upstream).append(r["name"])
    ts = leaf / "comment" / "pipeline" / "test-survey.json"
    rows = (read_json(ts).get("tests") or []) if ts.is_file() else None
    L = [f"**Coverage.** {len(infos)} checks: {len(upstream)} from an official test or example, {len(custom)} custom"
         + (f" ({', '.join(custom)})" if custom else "") + "; read against the exhaustive default: every distinct official test and example, each omission with its reason; not a count target."]
    if rows is None:
        L.append("  survey: comment/pipeline/test-survey.json is absent; coverage against the official tests cannot be read here.")
        return L
    suitable = [t for t in rows if t.get("suitable")]
    proposed = {t.get("proposed_check") for t in suitable if t.get("proposed_check")}
    uncovered = [t for t in suitable if t.get("proposed_check") and t.get("proposed_check") not in names]
    unnamed = [t for t in suitable if not t.get("proposed_check")]
    extra = sorted(names - proposed)
    L.append(f"  survey: {len(rows)} official tests recorded, {len(suitable)} suitable, {len(rows) - len(suitable)} not; "
             f"{len(uncovered)} suitable test(s) whose proposed check is absent from the leaf"
             + ("" if not uncovered else ": " + ", ".join(f"{t.get('id') or '?'} -> {t.get('proposed_check')}" for t in uncovered[:20]))
             + (f"; {len(unnamed)} suitable test(s) with no proposed check" if unnamed else "")
             + (f"; {len(extra)} check(s) the survey did not propose: {', '.join(extra[:20])}" if extra else "") + ".")
    return L


def task_page(leaf: Path, co: dict, present: list[str], ctx: dict, harbor: str) -> list[str]:
    sv = ctx["sv"]
    head12 = (co["head"] or "unknown")[:12]
    tree_skill, mine = skill_version_in_tree(), config.REVISION
    L = [f"# Task review: {rel(leaf)} at {head12}  (STOP 6, the task PR)", "", checkout_line(co, rel(leaf) + "/"),
         f"**Skill.** the tree's copy is {tree_skill or 'absent'}; this CLI is {mine}" + ("" if tree_skill == mine else "; review against this CLI and say what differs") + ".", ""]
    L += present
    L += ["**Lint.** " + (f"{len(ctx['errs'])} error(s), {len(ctx['warns'])} warning(s)" if (ctx["errs"] or ctx["warns"]) else "PASS")]
    L += [f"  error {e}" for e in ctx["errs"]] + [f"  warn  {w}" for w in ctx["warns"]]
    L.append(f"**validate-harbor.** {harbor}")
    if sv:
        L.append(f"**Record.** {sv.get('result')} at {sv.get('finished_at')}, {'fresh' if ctx['fresh'] else 'STALE'} against the contract fingerprint "
                 f"`{ctx['fingerprint'][:12]}`; run window {sv.get('started_at')} to {sv.get('finished_at')}; warnings {len(sv.get('warnings') or [])}, problems {len(sv.get('problems') or [])}.")
        L += [f"  warn  {w}" for w in (sv.get("warnings") or [])] + [f"  problem {p}" for p in (sv.get("problems") or [])]
    else:
        L.append("**Record.** none: comment/pipeline/self-validation.json is absent.")
    flags = flagged_rows(ctx)
    L += ["**Flagged rows.** " + ("" if flags else "none.")] + flags
    L += coverage_facts(leaf, ctx)
    return L + [""]


def cmd_review_task(a) -> None:
    anchor(a)
    leaf = leaf_of(a.task)
    cb_id = task_codebase(leaf)
    co = checkout_facts([rel(leaf)], a.base)
    rec_path = record_path("task", cb_id, leaf.name)
    doc = {"review": f"task {rel(leaf)}", "kind": "task", "codebase": cb_id, "task": rel(leaf), "head": co["head"], "root": co["root"]}
    if a.done:
        record_decision(a, rec_path, doc)
        return
    present, ctx = presentation(leaf, a.allow_custom_drivers)
    _, harbor = harbor_validate.validate_report(leaf)
    page = task_page(leaf, co, present, ctx, harbor)
    text = "\n".join(page) + "\n"
    print(brief_text("preamble", what=f"{rel(leaf)}  (STOP 6, the task PR)",
                     decisions="approve, request changes, or redesign the checks with the PR as a priori information"))
    print(text)
    print(brief_text("task", task=rel(leaf)))
    rec = open_record(rec_path, doc)
    rec_path.with_suffix(".md").write_text(text, encoding="utf-8")
    print(f"(page kept at {rec_path.with_suffix('.md')}; review opened {rec['opened_at']})")
    next_line(f"present as above, then sab.py review task --task {rel(leaf)} --done --human-ref \"<their words>\"")


# ---------------------------------------------------------------- status

def cmd_review_status(a) -> None:
    recs = sorted(p for p in config.PIPE.glob("*/reviewer/*.json") if not p.name.endswith(".page.json"))
    if not recs:
        print(f"no reviews under {config.PIPE}; start one with sab.py review codebase --codebase <id> or sab.py review task --task <leaf>")
        return
    print("| review | head | opened | state | the human's words |")
    print("|---|---|---|---|---|")
    for p in recs:
        r = read_json(p)
        d = r.get("decision")
        state = f"decided {d['at']}" if d else "open"
        words = (d or {}).get("human_ref") or ""
        if d and (d.get("rerun") or {}).get("human_ref"):
            words += f" | rerun: {d['rerun']['human_ref']}"
        print(f"| {r.get('review')} | {(r.get('head') or '?')[:12]} | {r.get('opened_at')} | {state} | {words[:80]} |")
