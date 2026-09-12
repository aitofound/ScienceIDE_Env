#!/usr/bin/env python3
"""Render the informational task-evidence backfill; --check detects stale views."""
import argparse
import html
import json
from pathlib import Path


def value(item):
    if item is None:
        return "unknown"
    if isinstance(item, (dict, list)):
        return json.dumps(item, ensure_ascii=False)
    return str(item)


def render(doc):
    md = ["<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->"]
    body = []

    def heading(text, level=2):
        md.extend(["#" * level + " " + text, ""])
        body.append(f"<h{level}>{html.escape(text)}</h{level}>")

    def paragraph(text):
        md.extend([text, ""])
        body.append("<p>" + html.escape(text) + "</p>")

    def table(headers, rows):
        def cell(x):
            return value(x).replace("|", "\\|").replace("\n", " ")
        md.extend(["| " + " | ".join(headers) + " |", "|" + "---|" * len(headers)])
        md.extend("| " + " | ".join(cell(x) for x in row) + " |" for row in rows)
        md.append("")
        body.append('<div class="scroll"><table><thead><tr>' + "".join(
            "<th>" + html.escape(h) + "</th>" for h in headers) + "</tr></thead><tbody>" + "".join(
            "<tr>" + "".join("<td>" + html.escape(value(x)) + "</td>" for x in row) + "</tr>"
            for row in rows) + "</tbody></table></div>")

    def bullets(items):
        md.extend("- " + text for text in items)
        md.append("")
        body.append("<ul>" + "".join("<li>" + html.escape(text) + "</li>" for text in items) + "</ul>")

    heading("Codebase metadata: Stim (informational, non-blocking)")
    paragraph(doc["notes"])
    paragraph("Generated: " + doc["generated_at"] + "; benchmark revision: " + doc["measurement"]["benchmark_commit"] + ".")
    c = doc["codebase"]
    table(["Identity", "Value", "Evidence ownership"], [
        ["Codebase", c["id"], "shipped task metadata"],
        ["Source root", c["source_root"], "shipped task metadata"],
        ["Upstream", c["upstream_url"], "shipped task metadata"],
        ["Pin / tag", c["upstream_pin"] + " / " + c["upstream_tag"], "task metadata; upstream tag resolution"],
        ["License", c["license"], "task metadata; pinned CITATION.cff"],
        ["Languages", c["languages"], "shipped task metadata"],
        ["Domain / owner", c["domain"] + " / " + c["owner"], "shipped task metadata"],
        ["Source fingerprint", c["source_tree_fingerprint"], "not measured in this backfill"],
    ])
    paragraph(c["agent_authored"]["description"])
    heading("Modules and shipped checks", 3)
    paragraph("Archived module approval: " + ", ".join(doc["approval"]["approved_modules"]) + ". Evidence: " + doc["approval"]["evidence"] + ". Only the shipped module is described below; approval is not a claim that both modules shipped.")
    for m in doc["modules"]:
        paragraph(m["slug"] + " — " + m["purpose"] + " " + m["differences"])
        paragraph("Task: " + m["task_path"] + "; manifest status: " + m["task_status"] + "; shipped checks: " + str(m["shipped_check_count"]) + ".")
        table(["Check", "Policy", "Workload / scope", "Citation keys"], [
            [x["slug"], x["policy"], x["description"], ", ".join(x["citation_keys"])] for x in m["checks"]])
        table(["Check", "Shipped evidence", "Upstream provenance recorded by task"], [
            [x["slug"], x["task_evidence"], x["upstream_evidence"]] for x in m["checks"]])
        paragraph("Owned paths (from shipped module.json):")
        bullets(m["owned_paths"])
    heading("Shared code and unknown measurements", 3)
    table(["Shared component", "Used by", "Files", "Text lines"], [
        [s["id"], ", ".join(s["used_by"]), s["metrics"]["regular_files"], s["metrics"]["text_physical_lines"]]
        for s in doc["shared_components"]])
    table(["Source size measure", "Value"], [[k, v] for k, v in doc["size"].items()])
    table(["Source accounting bucket", "Files", "Bytes", "Text lines"], [
        [k, v["regular_files"], v["bytes"], v["text_physical_lines"]]
        for k, v in doc["classification_and_gaps"]["buckets"].items()])
    table(["Official-test count", "Value", "Unit"], [
        [k, v["value"], v["unit"]] for k, v in doc["official_tests"]["counts"].items()])
    paragraph(doc["official_tests"]["note"])
    heading("Bibliography and verification", 3)
    paragraph(str(doc["bibliography"]["entry_count"]) + " entries in references.bib, primary upstream citation first. Verified " + doc["bibliography"]["verified_at"] + ".")
    for e in doc["bibliography"]["entries"]:
        paragraph(e["key"] + " — " + e["role"] + " DOI: " + e["doi"] + "; arXiv: " + e["arxiv"] + ".")
        paragraph(e["verified_fields"])
        bullets(e["verification_urls"])
    paragraph(doc["bibliography"]["verification_note"])
    heading("Shipped and pending-PR coverage", 3)
    bullets(doc["coverage"]["shipped_tasks"])
    paragraph(doc["coverage"]["inventory_note"])
    query = doc["coverage"]["github_check"]
    paragraph("GitHub snapshot " + query["checked_at"] + ": " + str(query["open_pr_count"]) + " open PRs inspected; Stim matches: " + value(query["stim_open_prs"]) + ". " + query["method"])
    heading("Gaps and warnings", 3)
    bullets(doc["warnings"])
    paragraph("Artifacts: codebase-metadata.json (canonical), codebase-metadata.md, codebase-metadata.html (self-contained detail), references.bib. Regenerate with python3 codebase-reports/stim/render_report.py; verify with the same command plus --check.")
    md.extend(["<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->", ""])
    body.append('<details><summary>Full canonical JSON</summary><pre id="canonical-json">' + html.escape(json.dumps(doc, ensure_ascii=False, indent=2)) + "</pre></details>")
    page = '<!doctype html>\n<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Codebase metadata: stim</title>\n<style>body{font:16px/1.5 system-ui,sans-serif;max-width:1200px;margin:auto;padding:24px;color:#17202a;background:#fafafa}h2,h3{border-bottom:1px solid #ddd}table{border-collapse:collapse;width:100%;font-size:14px}td,th{padding:6px;border:1px solid #ddd;text-align:left;vertical-align:top;overflow-wrap:anywhere}th{background:#eaf0f5}.scroll{overflow-x:auto}pre{white-space:pre-wrap;overflow-wrap:anywhere}li,p{overflow-wrap:anywhere}</style></head><body><main>\n'
    page += "\n".join(body) + "\n</main></body></html>\n"
    return {"codebase-metadata.md": "\n".join(md), "codebase-metadata.html": page}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    doc = json.loads((root / "codebase-metadata.json").read_text(encoding="utf-8"))
    for name, content in render(doc).items():
        path = root / name
        if args.check:
            if not path.is_file() or path.read_text(encoding="utf-8") != content:
                parser.exit(1, f"stale or missing generated view: {name}\n")
            print(f"consistent: {name}")
        else:
            path.write_text(content, encoding="utf-8")
            print(f"generated: {name}")


if __name__ == "__main__":
    main()
