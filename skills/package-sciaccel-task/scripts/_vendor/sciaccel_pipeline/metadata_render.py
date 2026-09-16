"""Bounded Markdown and self-contained HTML renderings of the metadata report."""
from __future__ import annotations

import html
import json

from .metadata import _METADATA_COUNT_SPECS, _metadata_list


def _metadata_value(value) -> str:
    if value is None or value == "":
        return "unknown"
    if isinstance(value, bool):
        return "yes" if value else "no"
    if isinstance(value, (dict, list)):
        return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(", ", ": "))
    return str(value).replace(chr(10), " ").strip()


def _metadata_short(value, limit: int = 180) -> str:
    text = _metadata_value(value)
    return text if len(text) <= limit else text[: limit - 1] + "…"


def _markdown_build_and_run(bar: dict) -> list[str]:
    """The Step 1.2 section: what was built and actually run, and the pitfalls; loud when it is missing."""
    lines = ["### Build and run (Step 1.2: what was actually built and run natively)", ""]
    if not bar.get("recorded"):
        lines.append("**NOT RECORDED.** Step 1.2 build-and-run was not done: nothing here says the codebase builds or that any test or example ran. Skipping it is strongly advised against.")
        return lines
    b = bar.get("build") or {}
    lines.append(f"Build: `{_metadata_value(b.get('system'))}`, {'ok' if b.get('ok') else 'FAILED'}, {_metadata_value(b.get('wall_s'))} s; commands: "
                 + "; ".join(f"`{_metadata_short(c, 120)}`" for c in _metadata_list(b.get("commands"))) + ".")
    for p in _metadata_list(b.get("pitfalls"))[:10]:
        lines.append(f"- build pitfall: {_metadata_short(p, 400)}")
    lines.extend(["", "| family | kind | decks | how to run | references |", "|---|---|---:|---|---|"])
    for f in _metadata_list(bar.get("landscape"))[:30]:
        lines.append(f"| {_metadata_short(f.get('family'), 60)} | {_metadata_value(f.get('kind'))} | {_metadata_value(f.get('count'))} | `{_metadata_short(f.get('how_to_run'), 100)}` | {_metadata_value(f.get('reference_outputs'))} |")
    runs = _metadata_list(bar.get("runs"))
    ran = [r for r in runs if isinstance(r, dict) and r.get("ran")]
    lines.extend(["", f"Actually run: {len(ran)} of {len(runs)} attempted.", "",
                  "| run | ran | wall s | reproduced / reason | pitfalls |", "|---|---|---:|---|---|"])
    for r in runs[:40]:
        if not isinstance(r, dict):
            continue
        lines.append(f"| `{_metadata_value(r.get('id'))}` | {'yes' if r.get('ran') else 'no'} | {_metadata_value(r.get('wall_s')) if r.get('ran') else '-'} | "
                     f"{_metadata_short(r.get('reproduced') if r.get('ran') else r.get('reason_not_run'), 120).replace('|', chr(92) + '|')} | "
                     f"{'; '.join(_metadata_short(p, 160) for p in _metadata_list(r.get('pitfalls'))).replace('|', chr(92) + '|') or '-'} |")
    pits = _metadata_list(bar.get("pitfalls"))
    lines.extend(["", f"Pitfalls of running the codebase: {len(pits)}"])
    for p in pits[:30]:
        if isinstance(p, dict):
            lines.append(f"- [{_metadata_value(p.get('where'))}] {_metadata_short(p.get('symptom'), 300)} -> {_metadata_short(p.get('workaround'), 300)}")
    nr = _metadata_list(bar.get("not_run"))
    if nr:
        lines.append("")
        lines.append("Not run:")
        for n in nr[:20]:
            if isinstance(n, dict):
                lines.append(f"- {_metadata_short(n.get('what'), 120)}: {_metadata_short(n.get('why'), 300)}")
    return lines


def render_metadata_markdown(doc: dict, limit: int = 12_000) -> str:
    codebase, size = doc["codebase"], doc["size"]
    account = doc["classification_and_gaps"]["buckets"]
    test_by_module = doc["official_tests"].get("by_module", {})
    lines = ["<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->", "## Codebase metadata (informational, non-blocking)",
             "Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.", "",
             "| field | value | ownership |", "|---|---|---|",
             f"| codebase | `{_metadata_value(codebase.get('id'))}` | CLI |",
             f"| source payload | `{_metadata_value(codebase.get('source_root'))}` | CLI |",
             f"| upstream pin | `{_metadata_value(codebase.get('upstream_pin'))}` | human/state |",
             f"| license | `{_metadata_value(codebase.get('license'))}` | human/state |",
             f"| source fingerprint | `{_metadata_value(codebase.get('source_tree_fingerprint'))}` | CLI |",
             f"| size | {_metadata_value(size.get('regular_files'))} files / {_metadata_value(size.get('payload_bytes'))} bytes / {_metadata_value(size.get('text_physical_lines'))} text lines | CLI |", ""]
    lines.extend(_markdown_build_and_run(doc.get("build_and_run") or {}))
    lines.extend(["", "### Modules, differences, and official tests", "",
             "| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |",
             "|---|---|---|---:|---:|---:|---|"])
    for module in doc.get("modules", []):
        owned = module.get("metrics", {}).get("owned", {})
        tests = test_by_module.get(module.get("slug"), {}).get("counts", {}).get("collected_items", {})
        desc = _metadata_short(module.get("differences") or module.get("purpose")).replace("|", "\\|")
        shared = ", ".join(f"`{value}`" for value in module.get("shared_component_ids", [])) or "unknown"
        lines.append(f"| `{_metadata_value(module.get('slug'))}` | {_metadata_value(module.get('approval_status'))} | {desc} | {_metadata_value(owned.get('regular_files'))} | {_metadata_value(owned.get('text_physical_lines'))} | {_metadata_value(tests.get('value'))} | {shared} |")
    lines.extend(["", "### Shared code", "", "| component | purpose | used by | files | text lines |", "|---|---|---|---:|---:|"])
    for item in doc.get("shared_components", []):
        metrics = item.get("metrics", {})
        purpose = _metadata_short(item.get("purpose")).replace("|", "\\|")
        used_by = _metadata_short(item.get("used_by")).replace("|", "\\|")
        lines.append(f"| `{_metadata_value(item.get('id'))}` | {purpose} | {used_by} | {_metadata_value(metrics.get('regular_files'))} | {_metadata_value(metrics.get('text_physical_lines'))} |")
    lines.extend(["", "### Source accounting", "", "| bucket | files | bytes | text lines |", "|---|---:|---:|---:|"])
    for key in ("shared", "owned", "overlapping_owned", "unclassified"):
        row = account.get(key, {})
        lines.append(f"| {key} | {_metadata_value(row.get('regular_files'))} | {_metadata_value(row.get('bytes'))} | {_metadata_value(row.get('text_physical_lines'))} |")
    lines.extend(["", "### Total official-test counts (units are not interchangeable)", "", "| count | value | unit |", "|---|---:|---|"])
    for key, _, _ in _METADATA_COUNT_SPECS:
        row = doc["official_tests"].get("counts", {}).get(key, {})
        lines.append(f"| `{key}` | {_metadata_value(row.get('value'))} | {_metadata_value(row.get('unit'))} |")
    gaps = doc["classification_and_gaps"]
    gap_rows = _metadata_list(gaps.get("known_review_gaps")) + _metadata_list(gaps.get("open_questions"))
    if gap_rows or doc.get("warnings"):
        lines.extend(["", "### Gaps and warnings"])
        lines.extend(f"- {_metadata_short(item, 500)}" for item in gap_rows[:20])
        lines.extend(f"- CLI: {item}" for item in doc.get("warnings", [])[:30])
    lines.extend(["", "Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)",
                  "<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->", ""])
    text = chr(10).join(lines)
    if len(text) <= limit:
        return text
    closing = "\n\n[PR section truncated at 12,000 characters; canonical JSON and HTML retain the full report.]\n<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->\n"
    return text[: max(0, limit - len(closing))] + closing


def render_metadata_html(doc: dict) -> str:
    esc = lambda value: html.escape(_metadata_value(value))
    codebase, size = doc["codebase"], doc["size"]
    account = doc["classification_and_gaps"]["buckets"]
    tests_by_module = doc["official_tests"].get("by_module", {})
    module_rows = []
    for module in doc.get("modules", []):
        owned = module.get("metrics", {}).get("owned", {})
        tests = tests_by_module.get(module.get("slug"), {}).get("counts", {}).get("collected_items", {})
        module_rows.append(f"<tr><td><code>{esc(module.get('slug'))}</code><br>{esc(module.get('title'))}</td><td>{esc(module.get('approval_status'))}</td><td>{esc(module.get('purpose'))}</td><td>{esc(module.get('differences'))}</td><td>{esc(owned.get('regular_files'))}</td><td>{esc(owned.get('text_physical_lines'))}</td><td>{esc(tests.get('value'))}</td><td>{esc(module.get('shared_component_ids'))}</td></tr>")
    shared_rows = [f"<tr><td><code>{esc(item.get('id'))}</code><br>{esc(item.get('title'))}</td><td>{esc(item.get('purpose'))}</td><td>{esc(item.get('paths'))}</td><td>{esc(item.get('used_by'))}</td><td>{esc(item.get('metrics', {}).get('regular_files'))}</td><td>{esc(item.get('metrics', {}).get('text_physical_lines'))}</td></tr>" for item in doc.get("shared_components", [])]
    count_rows = [f"<tr><td>{esc(key)}</td><td>{esc(doc['official_tests'].get('counts', {}).get(key, {}).get('value'))}</td><td>{esc(unit)}</td></tr>" for key, unit, _ in _METADATA_COUNT_SPECS]
    per_module_rows = []
    for slug, section in tests_by_module.items():
        c = section.get("counts", {})
        per_module_rows.append(f"<tr><td><code>{esc(slug)}</code></td><td>{esc(c.get('test_files', {}).get('value'))}</td><td>{esc(c.get('test_definitions', {}).get('value'))}</td><td>{esc(c.get('collected_items', {}).get('value'))}</td><td>{esc(c.get('inner_cases', {}).get('value'))}</td><td>{esc(section.get('covers'))}</td><td>{esc(section.get('known_gaps'))}</td></tr>")
    bucket_rows = [f"<tr><td>{esc(key)}</td><td>{esc(account.get(key, {}).get('regular_files'))}</td><td>{esc(account.get(key, {}).get('bytes'))}</td><td>{esc(account.get(key, {}).get('text_physical_lines'))}</td></tr>" for key in ("shared", "owned", "overlapping_owned", "unclassified")]
    gaps = doc["classification_and_gaps"]
    gap_items = _metadata_list(gaps.get("not_packaged")) + _metadata_list(gaps.get("known_review_gaps")) + _metadata_list(gaps.get("open_questions"))
    gap_html = "".join(f"<li>{esc(item)}</li>" for item in gap_items) or "<li>none recorded</li>"
    warning_html = "".join(f"<li>{esc(item)}</li>" for item in doc.get("warnings", [])) or "<li>none</li>"
    module_details = "".join(f"<details><summary><code>{esc(item.get('slug'))}</code> full module card</summary><pre>{esc(item)}</pre></details>" for item in doc.get("modules", [])) or "<p>unknown</p>"
    shared_details = "".join(f"<details><summary><code>{esc(item.get('id'))}</code> evidence and relationship</summary><pre>{esc(item)}</pre></details>" for item in doc.get("shared_components", [])) or "<p>unknown</p>"
    test_details = f"<details><summary>Framework, commands, selectors, coverage, execution, and evidence</summary><pre>{esc(doc.get('official_tests'))}</pre></details>"
    bar = doc.get("build_and_run") or {}
    if bar.get("recorded"):
        b = bar.get("build") or {}
        run_rows = "".join(f"<tr><td><code>{esc(r.get('id'))}</code></td><td>{'yes' if r.get('ran') else 'no'}</td><td>{esc(r.get('wall_s')) if r.get('ran') else '-'}</td><td>{esc(r.get('reproduced') if r.get('ran') else r.get('reason_not_run'))}</td><td>{esc(r.get('outputs'))}</td><td>{esc(r.get('pitfalls'))}</td></tr>"
                           for r in _metadata_list(bar.get('runs')) if isinstance(r, dict)) or '<tr><td colspan="6">none</td></tr>'
        land_rows = "".join(f"<tr><td>{esc(f.get('family'))}</td><td>{esc(f.get('kind'))}</td><td>{esc(f.get('count'))}</td><td><code>{esc(f.get('how_to_run'))}</code></td><td>{esc(f.get('reference_outputs'))}</td><td>{esc(f.get('notes'))}</td></tr>"
                            for f in _metadata_list(bar.get('landscape')) if isinstance(f, dict)) or '<tr><td colspan="6">none</td></tr>'
        pit_items = "".join(f"<li><strong>{esc(p.get('where'))}</strong>: {esc(p.get('symptom'))} &rarr; {esc(p.get('workaround'))}</li>" for p in _metadata_list(bar.get('pitfalls')) if isinstance(p, dict)) or "<li>none recorded</li>"
        nr_items = "".join(f"<li>{esc(n.get('what'))}: {esc(n.get('why'))}</li>" for n in _metadata_list(bar.get('not_run')) if isinstance(n, dict)) or "<li>none</li>"
        bar_html = (f"<p>Build: <code>{esc(b.get('system'))}</code>, <strong>{'ok' if b.get('ok') else 'FAILED'}</strong> in {esc(b.get('wall_s'))} s; commands: <code>{esc(b.get('commands'))}</code>; build pitfalls: {esc(b.get('pitfalls'))}</p>"
                    f"<h3>Landscape</h3><div class=\"scroll\"><table><tr><th>family</th><th>kind</th><th>decks</th><th>how to run</th><th>references</th><th>notes</th></tr>{land_rows}</table></div>"
                    f"<h3>Actually run</h3><div class=\"scroll\"><table><tr><th>run</th><th>ran</th><th>wall s</th><th>reproduced / reason</th><th>outputs</th><th>pitfalls</th></tr>{run_rows}</table></div>"
                    f"<h3>Pitfalls of running the codebase</h3><ul>{pit_items}</ul><h3>Not run</h3><ul>{nr_items}</ul>")
    else:
        bar_html = "<p class=\"note\"><strong>NOT RECORDED.</strong> Step 1.2 build-and-run was not done: nothing here says the codebase builds or that any test or example ran. Skipping it is strongly advised against.</p>"
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Codebase metadata: {esc(codebase.get('id'))}</title>
<style>:root{{color-scheme:light dark;--bg:#f6f7f5;--card:#fff;--ink:#1b2430;--muted:#5b6672;--line:#d7dcda;--note:#e9f0f9}}@media(prefers-color-scheme:dark){{:root{{--bg:#0f1418;--card:#161c22;--ink:#e6eaee;--muted:#98a3ae;--line:#2a333c;--note:#17273a}}}}body{{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif}}main{{max-width:1180px;margin:auto;padding:32px 22px 70px}}h1,h2{{font-family:Georgia,serif}}h1{{font-size:34px;margin:0 0 6px}}h2{{margin-top:34px;border-bottom:1px solid var(--line);padding-bottom:6px}}.note{{background:var(--note);border-left:3px solid #1f5fa8;padding:10px 14px}}.scroll{{overflow:auto}}table{{border-collapse:collapse;width:100%;background:var(--card);font-size:13px}}th,td{{text-align:left;padding:7px 9px;border-bottom:1px solid var(--line);vertical-align:top}}th{{color:var(--muted);text-transform:uppercase;font-size:10px;letter-spacing:.07em}}code{{font-family:ui-monospace,monospace}}details{{margin:10px 0;padding:9px 11px;background:var(--card);border:1px solid var(--line)}}summary{{cursor:pointer;font-weight:600}}pre{{white-space:pre-wrap;overflow:auto;font:12px/1.45 ui-monospace,monospace}}.muted{{color:var(--muted)}}</style></head><body><main>
<h1>{esc(codebase.get('title'))}</h1><p class="muted">Generated {esc(doc.get('generated_at'))} · schema {esc(doc.get('schema_version'))}</p>
<p class="note"><strong>Informational and non-blocking.</strong> Best-effort context only. Unknowns remain visible and never gate the source PR or later pipeline.</p>
<h2>Identity and source provenance</h2><div class="scroll"><table><tr><th>id</th><th>source</th><th>upstream</th><th>pin</th><th>license</th><th>fingerprint</th><th>measured from</th></tr><tr><td><code>{esc(codebase.get('id'))}</code></td><td><code>{esc(codebase.get('source_root'))}</code></td><td>{esc(codebase.get('upstream_url'))}</td><td><code>{esc(codebase.get('upstream_pin'))}</code></td><td>{esc(codebase.get('license'))}</td><td><code>{esc(codebase.get('source_tree_fingerprint'))}</code></td><td>{esc(codebase.get('measurement_source'))}</td></tr></table></div>
<h2>Whole-codebase size</h2><div class="scroll"><table><tr><th>entries</th><th>files</th><th>directories</th><th>symlinks</th><th>bytes</th><th>binary bytes</th><th>text lines</th><th>source lines</th><th>implementation lines</th><th>test lines</th></tr><tr><td>{esc(size.get('tree_entries'))}</td><td>{esc(size.get('regular_files'))}</td><td>{esc(size.get('directories'))}</td><td>{esc(size.get('symlinks'))}</td><td>{esc(size.get('payload_bytes'))}</td><td>{esc(size.get('binary_bytes'))}</td><td>{esc(size.get('text_physical_lines'))}</td><td>{esc(size.get('source_physical_lines'))}</td><td>{esc(size.get('implementation_source_physical_lines'))}</td><td>{esc(size.get('test_source_physical_lines'))}</td></tr></table></div>
<h2>Measurement method and file-type rollup</h2><details><summary>Included/excluded paths, counting rules, source extensions, and file extensions</summary><pre>{esc({'measurement': doc.get('measurement'), 'files_by_extension': size.get('files_by_extension')})}</pre></details>
<h2>Build and run (Step 1.2: what was actually built and run natively)</h2>{bar_html}
<h2>Approved cut</h2><p>Proposed: {esc(doc['approval'].get('proposed_modules'))}<br>Approved: <strong>{esc(doc['approval'].get('approved_modules'))}</strong><br>Human reference: {esc(doc['approval'].get('human_ref'))}</p>
<h2>Modules: size, responsibility, and difference</h2><div class="scroll"><table><tr><th>module</th><th>approval status</th><th>purpose</th><th>how it differs</th><th>owned files</th><th>owned text lines</th><th>collected tests</th><th>shared components</th></tr>{''.join(module_rows) or '<tr><td colspan="8">unknown</td></tr>'}</table></div>{module_details}
<h2>Shared components</h2><div class="scroll"><table><tr><th>component</th><th>purpose</th><th>paths</th><th>used by</th><th>files</th><th>text lines</th></tr>{''.join(shared_rows) or '<tr><td colspan="6">unknown</td></tr>'}</table></div>{shared_details}
<h2>Official tests (count units are distinct)</h2><div class="scroll"><table><tr><th>total count</th><th>value</th><th>unit</th></tr>{''.join(count_rows)}</table></div><div class="scroll"><table><tr><th>module</th><th>files</th><th>definitions</th><th>collected items</th><th>inner cases</th><th>covers</th><th>gaps</th></tr>{''.join(per_module_rows) or '<tr><td colspan="7">unknown</td></tr>'}</table></div>{test_details}
<h2>Shared / owned / overlap / unclassified</h2><div class="scroll"><table><tr><th>bucket</th><th>files</th><th>bytes</th><th>text lines</th></tr>{''.join(bucket_rows)}</table></div><p>Reconciliation: <strong>{esc(gaps.get('reconciliation', {}).get('reconciliation_ok'))}</strong>.</p>
<h2>Gaps and open questions</h2><ul>{gap_html}</ul><h2>CLI warnings</h2><ul>{warning_html}</ul>
<footer class="muted">Canonical JSON is the sole source for this HTML and the generated PR section. Report files sit outside the vendored payload.</footer></main></body></html>"""
