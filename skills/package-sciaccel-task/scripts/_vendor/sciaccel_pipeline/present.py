"""The codebase page: one fixed screen a human reads in a minute, computed only from the Step 1.5 report.

It is the body of the source PR (Markdown rendering) and the first thing the reviewer's agent prints
(text rendering) before any exploration. Numbers come from codebase-reports/<id>/codebase-metadata.json
and nothing else; what the report does not carry is shown as unknown, never estimated.
"""
from __future__ import annotations

import textwrap
from pathlib import Path

from . import config
from .util import die, next_line, read_json


def _n(x) -> str:
    return f"{int(x):,}" if isinstance(x, (int, float)) and not isinstance(x, bool) else "unknown"


def _k(x) -> str:
    if not isinstance(x, (int, float)) or isinstance(x, bool):
        return "?"
    return f"{x / 1000:.0f}k" if x >= 10_000 else (f"{x / 1000:.1f}k" if x >= 1000 else str(int(x)))


def _mb(b) -> str:
    return f"{b / 1e6:.1f} MB" if isinstance(b, (int, float)) and not isinstance(b, bool) else "unknown"


def _s(v, limit: int = 90) -> str:
    t = " ".join(str(v if v is not None else "").split())
    return t if len(t) <= limit else t[: limit - 1] + "…"


def load_report(cb_id: str, root: Path | None = None) -> dict:
    p = (root or config.ROOT) / "codebase-reports" / cb_id / "codebase-metadata.json"
    if not p.is_file():
        die(f"no report at {p}: run `sab.py codebase report --codebase {cb_id}` first (the page is computed from it, never typed)")
    return read_json(p)


def build_page(rep: dict) -> dict:
    """The page as data, every value read from the report."""
    cb = rep.get("codebase") or {}
    size = rep.get("size") or {}
    bd = size.get("breakdown") or {}
    bar = rep.get("build_and_run") or {}
    appr = rep.get("approval") or {}
    tests = rep.get("official_tests") or {}
    by_module = tests.get("by_module") or {}
    gaps = rep.get("classification_and_gaps") or {}
    authored = cb.get("agent_authored") or {}
    mods = []
    for m in rep.get("modules") or []:
        owned = (m.get("metrics") or {}).get("owned") or {}
        mods.append({"slug": m.get("slug"), "status": m.get("approval_status"), "owned_lines": owned.get("text_physical_lines"),
                     "expensive_path": m.get("expensive_path") or "", "purpose": m.get("purpose") or "",
                     "suites": [str(s) for s in ((by_module.get(m.get("slug")) or {}).get("sources") or [])]})
    shared = [{"id": s.get("id"), "lines": ((s.get("metrics") or {}).get("text_physical_lines")), "purpose": s.get("purpose") or ""}
              for s in rep.get("shared_components") or []]
    runs = [r for r in (bar.get("runs") or []) if isinstance(r, dict)]
    ran = [r for r in runs if r.get("ran")]
    reproduced = []
    for r in ran:
        v = _s(r.get("reproduced"), 60)
        if v and v not in reproduced:
            reproduced.append(v)
    return {
        "id": cb.get("id"), "title": cb.get("title"), "upstream": cb.get("upstream_url"), "pin": cb.get("upstream_pin"),
        "license": cb.get("license"), "domain": cb.get("domain"), "languages": cb.get("languages") or [],
        "description": authored.get("description") if isinstance(authored, dict) else None,
        "code": {"production": bd.get("production") or {}, "tests": bd.get("tests") or {}, "examples": bd.get("examples") or {},
                 "third_party": bd.get("third_party") or {}, "other": bd.get("other") or {},
                 "total_lines": size.get("text_physical_lines"), "files": size.get("regular_files"),
                 "bytes": size.get("payload_bytes"), "binary_bytes": size.get("binary_bytes"),
                 "test_files": ((tests.get("counts") or {}).get("test_files") or {}).get("value"),
                 "known": bool(bd)},
        "build_and_run": {"recorded": bool(bar.get("recorded")), "build": bar.get("build") or {},
                          "landscape": [f for f in (bar.get("landscape") or []) if isinstance(f, dict)],
                          "attempted": len(runs), "ran": len(ran), "reproduced": reproduced,
                          "pitfalls": [p for p in (bar.get("pitfalls") or []) if isinstance(p, dict)],
                          "not_run": [n for n in (bar.get("not_run") or []) if isinstance(n, dict)]},
        "cut": {"approved": appr.get("approved_modules") or [], "proposed": appr.get("proposed_modules") or [],
                "human_ref": appr.get("human_ref"), "at": appr.get("approved_at"), "modules": mods, "shared": shared},
        "left_out": [g for g in (gaps.get("not_packaged") or []) if isinstance(g, dict)],
        "warnings": list(rep.get("warnings") or []),
        "review_gaps": [g for g in (gaps.get("known_review_gaps") or [])],
    }


def _lang_line(bucket: dict, n: int = 4) -> str:
    langs = [l for l in (bucket.get("by_language") or []) if isinstance(l, dict)]
    langs = sorted(langs, key=lambda l: -(l.get("text_physical_lines") or 0))[:n]
    return ", ".join(f"{l.get('language')} {_k(l.get('text_physical_lines'))}" for l in langs)


def _single_module(page: dict) -> bool:
    ref = str(page["cut"].get("human_ref") or "")
    return len(page["cut"]["modules"]) == 1 and ref.startswith("single-module default")


def render_page_text(page: dict, decide: str) -> str:
    W = 14
    def row(label: str, text: str) -> list[str]:
        lines = textwrap.wrap(text, 96 - W) or [""]
        return [f"{label:<{W}}{lines[0]}"] + [f"{'':<{W}}{ln}" for ln in lines[1:]]
    L = [f"CODEBASE  {page['id']} · {page['title']} · {page['upstream'] or 'no upstream URL'} @ {str(page['pin'] or '?')[:9]} · "
         f"{page['license'] or 'licence unknown'} · {page['domain'] or 'domain unknown'}",
         "=" * 96]
    L += row("WHAT IT DOES", (page["description"] or "unknown: the report carries no description (codebase.agent_authored.description)")
             + (f"  Languages: {', '.join(page['languages'])}." if page["languages"] else ""))
    c = page["code"]
    if c["known"]:
        L += row("CODE", f"production   {_n(c['production'].get('text_physical_lines')):>10} lines   {_lang_line(c['production'])}   <- what gets ported")
        L += row("", f"tests        {_n(c['tests'].get('text_physical_lines')):>10} lines   {_n(c['tests'].get('files'))} files" + (f", {_n(c['test_files'])} official test files" if c["test_files"] is not None else ""))
        L += row("", f"examples     {_n(c['examples'].get('text_physical_lines')):>10} lines   {_n(c['examples'].get('files'))} files (decks, inputs, tutorials)")
        tp = c["third_party"]
        if (tp.get("text_physical_lines") or 0) > 0:
            L += row("", f"third-party  {_n(tp.get('text_physical_lines')):>10} lines   {', '.join(tp.get('paths') or [])} (bundled, not ported)")
        L += row("", f"other        {_n(c['other'].get('text_physical_lines')):>10} lines   docs, build, data")
    else:
        L += row("CODE", "production lines unknown: the report predates the size breakdown or names no source extensions")
    L += row("", f"total        {_n(c['total_lines']):>10} text lines in {_n(c['files'])} files, {_mb(c['bytes'])} ({_mb(c['binary_bytes'])} binary)")
    L += row("", "(approximate: source extensions minus test and example paths minus declared third-party; the report has the rules)")
    b = page["build_and_run"]
    if b["recorded"]:
        bl = b["build"]
        L += row("BUILD & RUN", f"{bl.get('system')}: {'builds' if bl.get('ok') else 'BUILD FAILED'} in {_n(bl.get('wall_s'))} s; build pitfalls: {len(bl.get('pitfalls') or [])}")
        fam = ", ".join(f"{_s(f.get('family'), 30)} ({f.get('kind')}, {_n(f.get('count'))} decks, refs {f.get('reference_outputs')})" for f in b["landscape"][:6])
        L += row("", f"landscape: {len(b['landscape'])} families: {fam or 'none listed'}")
        L += row("", f"ran {b['ran']} of {b['attempted']} attempted tests/examples; reproduced: {'; '.join(b['reproduced']) or 'nothing compared'}")
        pits = "; ".join(f"[{p.get('where')}] {_s(p.get('symptom'), 50)}" for p in b["pitfalls"][:4])
        L += row("", f"pitfalls ({len(b['pitfalls'])}): {pits or 'none recorded'}")
        if b["not_run"]:
            L += row("", "not run: " + "; ".join(f"{_s(n.get('what'), 30)} ({_s(n.get('why'), 50)})" for n in b["not_run"][:3]))
    else:
        L += row("BUILD & RUN", "NOT RECORDED: Step 1.2 was skipped; nothing here says the codebase builds or that any test ran. Ask for it.")
    cut = page["cut"]
    if _single_module(page):
        L += row("MODULE CUT", f"one module, the whole codebase ({cut['modules'][0]['slug']}); the single-module default, recorded by propose-modules")
    else:
        L += row("MODULE CUT", f"{len(cut['modules'])} modules, {len(cut['approved'])} approved; human: \"{_s(cut['human_ref'], 60)}\" ({str(cut['at'] or '?')[:10]})")
    L.append(f"{'':<{W}}{'slug':30} {'owned lines':>11}  {'expensive path':40}  suites / status")
    for m in cut["modules"]:
        L.append(f"{'':<{W}}{_s(m['slug'], 30):30} {_n(m['owned_lines']):>11}  {_s(m['expensive_path'], 40):40}  "
                 f"{_s(', '.join(Path(s).name for s in m['suites']), 30) or '-'}{'' if m['status'] == 'approved' else '  (proposed-only)'}")
    if cut["shared"]:
        L += row("", "shared: " + "; ".join(f"{s['id']} {_n(s['lines'])} lines" for s in cut["shared"][:4]))
    L += row("LEFT OUT", "; ".join(f"{_s(g.get('what'), 40)} ({_s(g.get('why'), 60)})" for g in page["left_out"][:4]) or "nothing recorded as left out")
    warns = page["warnings"] + [str(g) for g in page["review_gaps"]]
    L += row("WARNINGS", f"{len(warns)}: " + "; ".join(_s(w, 70) for w in warns[:4]) if warns else "none")
    L += row("DECIDE", decide)
    return "\n".join(L) + "\n"


def render_page_markdown(page: dict) -> str:
    c = page["code"]
    md = [f"## {page['title']} (`{page['id']}`)", "",
          page["description"] or "_no description in the report_", "",
          "| field | value |", "|---|---|",
          f"| upstream | {page['upstream'] or 'unknown'} |", f"| pin | `{page['pin'] or 'unknown'}` |",
          f"| licence | {page['license'] or 'unknown'} |", f"| domain | {page['domain'] or 'unknown'} |",
          f"| languages | {', '.join(page['languages']) or 'unknown'} |", "",
          "### Code (approximate: source extensions minus test and example paths minus declared third-party)", "",
          "| bucket | text lines | files | detail |", "|---|---:|---:|---|"]
    if c["known"]:
        md.append(f"| **production** | **{_n(c['production'].get('text_physical_lines'))}** | {_n(c['production'].get('files'))} | {_lang_line(c['production'], 6)}; what gets ported |")
        md.append(f"| tests | {_n(c['tests'].get('text_physical_lines'))} | {_n(c['tests'].get('files'))} | {_n(c['test_files']) + ' official test files' if c['test_files'] is not None else ''} |")
        md.append(f"| examples | {_n(c['examples'].get('text_physical_lines'))} | {_n(c['examples'].get('files'))} | decks, inputs, tutorials |")
        tp = c["third_party"]
        if (tp.get("text_physical_lines") or 0) > 0:
            md.append(f"| third-party | {_n(tp.get('text_physical_lines'))} | {_n(tp.get('files'))} | {', '.join('`' + p + '`' for p in tp.get('paths') or [])}; bundled, not ported |")
        md.append(f"| other | {_n(c['other'].get('text_physical_lines'))} | {_n(c['other'].get('files'))} | docs, build, data |")
    else:
        md.append("| production | unknown | | the report predates the size breakdown or names no source extensions |")
    md.append(f"| total | {_n(c['total_lines'])} | {_n(c['files'])} | {_mb(c['bytes'])}, {_mb(c['binary_bytes'])} binary |")
    b = page["build_and_run"]
    md += ["", "### Build and run (Step 1.2)", ""]
    if b["recorded"]:
        bl = b["build"]
        md.append(f"Build: `{bl.get('system')}`, {'ok' if bl.get('ok') else 'FAILED'} in {_n(bl.get('wall_s'))} s; build pitfalls: {len(bl.get('pitfalls') or [])}.")
        md += ["", "| family | kind | decks | how to run | references |", "|---|---|---:|---|---|"]
        md += [f"| {_s(f.get('family'), 60)} | {f.get('kind')} | {_n(f.get('count'))} | `{_s(f.get('how_to_run'), 80)}` | {f.get('reference_outputs')} |" for f in b["landscape"]]
        md += ["", f"Actually run: **{b['ran']} of {b['attempted']}** attempted tests/examples. Reproduced: {'; '.join(b['reproduced']) or 'nothing compared'}.", ""]
        md += [f"- pitfall [{p.get('where')}]: {_s(p.get('symptom'), 200)} → {_s(p.get('workaround'), 200)}" for p in b["pitfalls"]]
        md += [f"- not run: {_s(n.get('what'), 80)}: {_s(n.get('why'), 200)}" for n in b["not_run"]]
    else:
        md.append("**NOT RECORDED.** Step 1.2 was skipped: nothing here says the codebase builds or that any test or example ran.")
    cut = page["cut"]
    md += ["", "### Module cut", ""]
    if _single_module(page):
        md.append(f"One module, the whole codebase (`{cut['modules'][0]['slug']}`): the single-module default, recorded by `propose-modules`.")
    else:
        md.append(f"{len(cut['modules'])} modules, {len(cut['approved'])} approved. Human approval: \"{cut['human_ref'] or 'none recorded'}\" ({cut['at'] or 'undated'}).")
    md += ["", "| module | owned lines | expensive path | official suites | status |", "|---|---:|---|---|---|"]
    md += [f"| `{m['slug']}` | {_n(m['owned_lines'])} | {_s(m['expensive_path'], 120)} | {', '.join('`' + s + '`' for s in m['suites']) or '-'} | {m['status']} |" for m in cut["modules"]]
    if cut["shared"]:
        md += ["", "Shared: " + "; ".join(f"`{s['id']}` {_n(s['lines'])} lines" for s in cut["shared"]) + "."]
    md += ["", "### Left out", ""]
    md += [f"- {_s(g.get('what'), 100)}: {_s(g.get('why'), 300)}" for g in page["left_out"]] or ["- nothing recorded as left out"]
    warns = page["warnings"] + [str(g) for g in page["review_gaps"]]
    md += ["", "### Warnings", ""] + ([f"- {_s(w, 300)}" for w in warns] or ["- none"])
    md += ["", f"_Page generated by the pipeline (revision {config.REVISION}) from `codebase-reports/{page['id']}/codebase-metadata.json`; the full report follows below the rule for information only._", ""]
    return "\n".join(md)


def cmd_codebase_present(a) -> None:
    root = Path(a.root).resolve() if getattr(a, "root", None) else config.ROOT
    page = build_page(load_report(a.codebase, root))
    if a.markdown:
        print(render_page_markdown(page))
        return
    print(render_page_text(page, "this page is the source PR body (--markdown); the human reads it at STOP 2: merge, send back, or change the cut"))
    next_line(f"sab.py codebase present --codebase {a.codebase} --markdown  > the PR body; then the approval words, a rule, and codebase-metadata.md")
