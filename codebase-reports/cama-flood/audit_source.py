#!/usr/bin/env python3
"""Verify the vendored source against the GitHub tree at the declared pin.

Run from any directory. Requires gh unless --upstream-tree names a saved response
from the GitHub recursive Git Trees API. Emits JSON; returns nonzero on deviations
other than the data omissions declared in data-policy.json.
"""
import argparse
import base64
import hashlib
import io
import json
from pathlib import Path
import subprocess
import tarfile
import zipfile

PIN = '25b9caab93dc809d2d5580781c6bff31f185ebf8'
TREE = 'acd9f2985865347131e2e2180bbf9e15a3d83d2a'
REPO = 'global-hydrodynamics/CaMa-Flood_v4'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--upstream-tree', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    report = Path(__file__).resolve().parent
    root = report.parents[1] / 'code/cama-flood'
    policy = json.loads((report / 'data-policy.json').read_text())
    declared = {p for row in policy['groups'] if row['disposition'] == 'omit' for p in row['paths']}
    raw = args.upstream_tree.read_text() if args.upstream_tree else subprocess.check_output([
        'gh', 'api', f'repos/{REPO}/git/trees/{PIN}?recursive=1'], text=True)
    remote = json.loads(raw)
    if remote.get('truncated'):
        raise ValueError('The upstream recursive tree was truncated')
    # GitHub accepts a commit in the tree endpoint; verify its resolved tree separately.
    if not args.upstream_tree:
        resolved = subprocess.check_output(['gh', 'api', f'repos/{REPO}/git/commits/{PIN}',
                                           '--jq', '.tree.sha'], text=True).strip()
        if resolved != TREE:
            raise ValueError('Unexpected upstream tree identity')
    upstream = {x['path']: x for x in remote['tree'] if x['type'] != 'tree'}
    actual = {p.relative_to(root).as_posix(): p for p in root.rglob('*') if p.is_file() or p.is_symlink()}
    removed = sorted(set(upstream) - set(actual))
    added = sorted(set(actual) - set(upstream))
    modified = []
    modes = []
    assets = []
    for name, path in sorted(actual.items()):
        if path.is_symlink():
            modes.append(name)
            continue
        contents = path.read_bytes()
        blob = hashlib.sha1(b'blob ' + str(len(contents)).encode() + b'\0' + contents).hexdigest()
        if name in upstream:
            if blob != upstream[name]['sha']:
                modified.append(name)
            mode = '100755' if path.stat().st_mode & 0o111 else '100644'
            if mode != upstream[name]['mode']:
                modes.append(name)
        # Include all binary assets, text data/configuration files and the notebook.
        try:
            contents.decode('utf-8')
            binary = b'\0' in contents
        except UnicodeDecodeError:
            binary = True
        if binary or path.suffix.lower() in {'.txt', '.csv', '.nml', '.ctl', '.ipynb'}:
            item = {'path': name, 'bytes': len(contents), 'sha256': hashlib.sha256(contents).hexdigest(),
                    'git_blob': blob, 'kind': 'binary' if binary else 'text-data-or-configuration'}
            if path.name.endswith('.tar.gz'):
                with tarfile.open(fileobj=io.BytesIO(contents)) as archive:
                    item['members'] = [{'path': m.name, 'bytes': m.size, 'type': 'file' if m.isfile() else 'directory' if m.isdir() else 'other'}
                                       for m in archive]
            elif path.suffix.lower() == '.docx':
                with zipfile.ZipFile(io.BytesIO(contents)) as doc:
                    item['embedded_objects'] = [n for n in doc.namelist() if '/embeddings/' in n]
            elif path.suffix.lower() == '.ipynb':
                notebook = json.loads(contents)
                item['code_cells'] = sum(c['cell_type'] == 'code' for c in notebook['cells'])
                item['stored_output_types'] = sorted({o.get('output_type', 'unknown') for c in notebook['cells'] for o in c.get('outputs', [])})
            assets.append(item)
    notebook_checks = []
    for entry in policy.get('cleared_notebook_outputs', []):
        name = entry['path']
        encoded = subprocess.check_output(['gh', 'api',
            f"repos/{REPO}/git/blobs/{upstream[name]['sha']}", '--jq', '.content'], text=True)
        original_raw = base64.b64decode(encoded)
        original = json.loads(original_raw)
        for cell in original['cells']:
            if cell['cell_type'] == 'code':
                cell['outputs'] = []
                cell['execution_count'] = None
        candidate = json.loads(actual[name].read_text())
        notebook_checks.append({'path': name, 'only_outputs_and_execution_counts_cleared': original == candidate,
                                'original_blob_verified': hashlib.sha1(b'blob ' + str(len(original_raw)).encode() + b'\0' + original_raw).hexdigest() == upstream[name]['sha']})
    ok = (set(removed) == declared and set(added) == set(policy['added_packaging_files'])
          and set(modified) == {e['path'] for e in policy.get('cleared_notebook_outputs', [])}
          and all(r['only_outputs_and_execution_counts_cleared'] and r['original_blob_verified'] for r in notebook_checks)
          and not modes)
    result = {'upstream': REPO, 'pin': PIN, 'upstream_tree': TREE, 'upstream_files': len(upstream),
              'vendored_files': len(actual), 'vendored_bytes': sum(p.stat().st_size for p in actual.values()),
              'added': added, 'removed': removed, 'modified': modified, 'mode_changes': modes,
              'omitted_files': [{k: upstream[name][k] for k in ('path', 'sha', 'size', 'mode')} for name in removed],
              'retained_assets': assets, 'notebook_checks': notebook_checks, 'matches_declared_policy': ok}
    text = json.dumps(result, indent=2) + '\n'
    if args.output:
        args.output.write_text(text)
    else:
        print(text, end='')
    raise SystemExit(0 if ok else 1)


if __name__ == '__main__':
    main()
