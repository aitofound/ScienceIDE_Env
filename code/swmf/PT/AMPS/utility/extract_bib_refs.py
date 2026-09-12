#!/usr/bin/env python3
r"""
extract_bib_refs.py

Scan a set of LaTeX (.tex) source files for citation keys, then pull just
those entries out of a large BibDesk/BibTeX (.bib) library into a new,
smaller .bib file -- with each reference included exactly once.

Handles:
  - natbib commands:  \cite \citep \citet \citeauthor \citeyear \citealt
                       \citealp \citenum \Citep \Citet ... (any \...cite...)
  - biblatex commands: \parencite \textcite \autocite \footcite \fullcite
                        \smartcite \citeyearpar ...
  - biblatex *multicite* commands with several {key} groups in one call:
    \cites{key1}{key2}{key3}, \parencites[see][]{key1}[cf.][]{key2}, etc.
  - \nocite{key}  (and warns on \nocite{*})
  - multiple comma-separated keys per \cite{...}
  - optional [..] arguments, e.g. \citep[see][chap. 2]{key1,key2}
  - LaTeX line comments (%...), so commented-out \cite calls are ignored
  - BibDesk/BibTeX entries with nested braces in field values
    (parsed by brace-counting, not naive regex)
  - duplicate keys, in both the .tex citations and the .bib file
  - crossref fields: if a pulled entry has crossref = {otherkey}, that
    entry is pulled in too (recursively), since BibTeX needs it to compile

Usage:
    python3 extract_bib_refs.py --bib library.bib --tex chapters/*.tex -o refs.bib
    python3 extract_bib_refs.py --bib library.bib --tex-dir ./paper -o refs.bib
    python3 extract_bib_refs.py --bib library.bib --tex-dir . --case-insensitive -o refs.bib

No third-party dependencies -- standard library only (Python 3.7+).
"""

import argparse
import glob
import re
import sys
from pathlib import Path


# Matches the *name* of any command containing "cite" (case-insensitive on
# the C), plus an optional trailing star. This is a heuristic -- it will
# also match a hypothetical \myfancycite{} style macro, which is what you
# want for custom cite wrappers, but could in rare cases over-match a
# non-citation command with "cite" in its name.
CITE_CMD_RE = re.compile(r'\\([A-Za-z]*[Cc]ite[A-Za-z]*)(\*?)')

CROSSREF_RE = re.compile(r'crossref\s*=\s*[{"]([^}"]+)[}"]', re.IGNORECASE)

COMMENT_RE = re.compile(r'(?<!\\)%.*')


def read_text_robust(path):
    """Read a file trying a few common encodings before giving up."""
    data = Path(path).read_bytes()
    for enc in ('utf-8', 'utf-8-sig', 'latin-1'):
        try:
            return data.decode(enc)
        except UnicodeDecodeError:
            continue
    return data.decode('utf-8', errors='replace')


def strip_comments(line):
    return COMMENT_RE.sub('', line)


def find_tex_files(tex_patterns, tex_dirs):
    files = []
    for pattern in tex_patterns:
        matched = glob.glob(pattern, recursive=True)
        if not matched and Path(pattern).is_file():
            matched = [pattern]
        files.extend(matched)
    for d in tex_dirs:
        files.extend(str(p) for p in Path(d).rglob('*.tex'))

    seen, unique_files = set(), []
    for f in files:
        rp = str(Path(f).resolve())
        if rp not in seen:
            seen.add(rp)
            unique_files.append(f)
    return unique_files


def scan_cite_groups(text, start, allow_multi):
    r"""
    Starting right after a matched \\citecommand* name, walk forward and
    consume any run of [prenote]/[postnote] groups and {key} groups.

    Plain commands (\cite, \citep, \parencite, \autocite, ...) only ever
    take ONE {key,key,...} group, so we stop right after it -- this avoids
    accidentally swallowing an unrelated {...} group that happens to follow
    (e.g. "\cite{key1} {\it emphasis}" should not eat the second group).

    biblatex's *multicite* family (\cites, \parencites, \textcites,
    \autocites, \footcites, \smartcites, \supercites -- anything ending in
    "cites") can chain several [pre][post]{key} groups in one call, e.g.
        \cites{key1}{key2}{key3}
        \parencites[see][]{key1}[cf.][]{key2}
    so for those we keep consuming groups. We stop at a blank line either
    way, so we never wander into unrelated later content.

    Returns (list_of_raw_group_strings, end_index).
    """
    n = len(text)
    i = start
    key_groups = []
    while i < n:
        j = i
        newlines = 0
        while j < n and text[j] in ' \t\r\n':
            if text[j] == '\n':
                newlines += 1
                if newlines >= 2:
                    return key_groups, i  # blank line: citation args ended
            j += 1
        i = j

        if i < n and text[i] == '[':
            depth, j = 0, i
            while j < n:
                if text[j] == '[':
                    depth += 1
                elif text[j] == ']':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if j >= n:
                break
            i = j + 1
            continue

        if i < n and text[i] == '{':
            depth, j = 0, i
            while j < n:
                if text[j] == '{':
                    depth += 1
                elif text[j] == '}':
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            if j >= n:
                break
            key_groups.append(text[i + 1:j])
            i = j + 1
            if not allow_multi:
                break
            continue

        break  # next char isn't [ or { -- citation args are done
    return key_groups, i


def extract_keys_from_tex(path):
    """Return citation keys in first-seen order (duplicates included;
    caller dedupes)."""
    text = read_text_robust(path)
    clean_text = '\n'.join(strip_comments(line) for line in text.splitlines())

    keys = []
    for m in CITE_CMD_RE.finditer(clean_text):
        cmd_name = m.group(1)
        allow_multi = cmd_name.lower().endswith('cites')  # \cites, \parencites, \autocites, ...
        groups, _ = scan_cite_groups(clean_text, m.end(), allow_multi)
        for group_text in groups:
            for key in group_text.split(','):
                key = key.strip()
                if key == '*':
                    print(f"warning: \\nocite{{*}} found in {path} "
                          f"(means 'cite everything in the .bib'); ignoring",
                          file=sys.stderr)
                elif key:
                    keys.append(key)
    return keys


def split_bib_entries(bib_text):
    """
    Walk the .bib text and return {key: full_entry_text} for every
    @entry. Uses brace-counting (not regex) to find each entry's end,
    so nested braces inside field values don't break parsing.
    """
    entries = {}
    entry_start_re = re.compile(r'@(\w+)\s*\{', re.IGNORECASE)
    n = len(bib_text)
    i = 0

    while True:
        m = entry_start_re.search(bib_text, i)
        if not m:
            break
        entry_type = m.group(1).lower()
        brace_open = m.end() - 1

        depth = 0
        j = brace_open
        while j < n:
            if bib_text[j] == '{':
                depth += 1
            elif bib_text[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        entry_text = bib_text[m.start():j + 1]

        if entry_type not in ('comment', 'string', 'preamble'):
            inner = bib_text[brace_open + 1:j]
            key_match = re.match(r'\s*([^,\s]+)\s*,', inner)
            if key_match:
                key = key_match.group(1)
                if key in entries:
                    print(f"warning: duplicate key '{key}' in .bib file; "
                          f"keeping the first occurrence", file=sys.stderr)
                else:
                    entries[key] = entry_text

        i = j + 1 if j < n else n
    return entries


def resolve_keys(seed_keys, bib_entries, case_insensitive):
    """Pull entries for seed_keys, following crossref fields. Returns
    (resolved_dict, missing_list). Dedup is automatic via the dict/set."""
    lower_map = {}
    if case_insensitive:
        for k in bib_entries:
            lower_map.setdefault(k.lower(), k)

    def lookup(k):
        if k in bib_entries:
            return k
        if case_insensitive and k.lower() in lower_map:
            return lower_map[k.lower()]
        return None

    queue = list(seed_keys)
    seen = set()
    resolved = {}
    missing = []

    while queue:
        k = queue.pop(0)
        if k in seen:
            continue
        seen.add(k)
        real_key = lookup(k)
        if real_key is None:
            missing.append(k)
            continue
        resolved[real_key] = bib_entries[real_key]
        for m in CROSSREF_RE.finditer(bib_entries[real_key]):
            cref = m.group(1).strip()
            if cref not in seen:
                queue.append(cref)

    return resolved, missing


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--tex', nargs='+', default=[],
                     help='.tex files or glob patterns, e.g. chapters/*.tex')
    ap.add_argument('--tex-dir', nargs='*', default=[],
                     help='director(y/ies) to search recursively for *.tex')
    ap.add_argument('--bib', required=True,
                     help='large source .bib / BibDesk library file')
    ap.add_argument('-o', '--out', required=True,
                     help='output .bib file to write')
    ap.add_argument('--case-insensitive', action='store_true',
                     help='fall back to case-insensitive key matching')
    args = ap.parse_args()

    tex_files = find_tex_files(args.tex, args.tex_dir)
    if not tex_files:
        sys.exit('error: no .tex files found (check --tex / --tex-dir)')

    print(f"Scanning {len(tex_files)} .tex file(s) for citations...",
          file=sys.stderr)
    all_keys = []
    for f in tex_files:
        all_keys.extend(extract_keys_from_tex(f))

    seen, ordered_keys = set(), []
    for k in all_keys:
        if k not in seen:
            seen.add(k)
            ordered_keys.append(k)
    print(f"Found {len(ordered_keys)} unique citation key(s) across "
          f"{len(tex_files)} file(s).", file=sys.stderr)

    bib_text = read_text_robust(args.bib)
    bib_entries = split_bib_entries(bib_text)
    print(f"Parsed {len(bib_entries)} entries from {args.bib}.",
          file=sys.stderr)

    resolved, missing = resolve_keys(ordered_keys, bib_entries,
                                      args.case_insensitive)

    ordered_out = sorted(resolved, key=str.lower)
    header = (f"% Extracted {len(ordered_out)} entries from {args.bib}\n"
              f"% by extract_bib_refs.py -- do not edit citation keys "
              f"without updating the .tex source\n\n")
    body = '\n\n'.join(resolved[k] for k in ordered_out)

    out_path = Path(args.out)
    out_path.write_text(header + body + '\n', encoding='utf-8')
    print(f"Wrote {len(ordered_out)} entries to {out_path}", file=sys.stderr)

    if missing:
        print(f"\nWARNING: {len(missing)} cited key(s) not found in "
              f"{args.bib}:", file=sys.stderr)
        for k in missing:
            print(f"  - {k}", file=sys.stderr)


if __name__ == '__main__':
    main()
