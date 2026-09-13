import assert from 'node:assert/strict';
import { test } from 'node:test';
import path from 'node:path';
import fs from 'node:fs';
import os from 'node:os';
import { checkCodebaseReports, REQUIRED_REPORT_FILES } from '../scripts/check-codebase-reports.mjs';

// Build tiny fixtures at test time so Git need not preserve empty directories.
// Retain the run directory (no teardown/deletion), including on failure.
const FIXTURES = fs.mkdtempSync(path.join(process.env.REPORT_GATE_FIXTURE_ROOT || os.tmpdir(), 'report-gate-'));
function fixture(name, { source = 'source', missing, empty, directory } = {}) {
  const root = path.join(FIXTURES, name);
  fs.mkdirSync(path.join(root, 'code', source), { recursive: true });
  fs.writeFileSync(path.join(root, 'code', source, 'module.c'), 'source');
  for (const file of REQUIRED_REPORT_FILES) {
    if (file === missing) continue;
    const target = path.join(root, 'codebase-reports', source, file);
    fs.mkdirSync(path.dirname(target), { recursive: true });
    if (file === directory) fs.mkdirSync(target);
    else fs.writeFileSync(target, file === empty ? '' : 'fixture');
  }
}
fixture('valid');
fixture('unchanged');
for (const [i, suffix] of ['json', 'html', 'md', 'bib'].entries()) {
  fixture(`missing-${suffix}`, { missing: REQUIRED_REPORT_FILES[i] });
  fixture(`empty-${suffix}`, { empty: REQUIRED_REPORT_FILES[i] });
}
fixture('nonregular', { directory: 'codebase-metadata.json' });
// The unrelated source deliberately has no report bundle.
fs.mkdirSync(path.join(FIXTURES, 'unrelated', 'code', 'source'), { recursive: true });
fs.mkdirSync(path.join(FIXTURES, 'root-file', 'code'), { recursive: true });
fs.writeFileSync(path.join(FIXTURES, 'root-file', 'code', 'README.md'), 'source index');
fs.mkdirSync(path.join(FIXTURES, 'removed'), { recursive: true });
fixture('renamed', { source: 'new-source' });
const diff = (text) => checkCodebaseReports({
  root: path.join(FIXTURES, text.root),
  diffText: text.diff,
  baselineSources: text.baselineSources || [],
});

const sourceChange = 'M\tcode/source/module.c\n';

test('accepts a complete bundle for a changed source', () => {
  const result = diff({ root: 'valid', diff: sourceChange });
  assert.deepEqual(result.sources, ['source']);
  assert.deepEqual(result.errors, []);
});

test('each required file is checked when missing', () => {
  const missingRoots = ['json', 'html', 'md', 'bib'];
  for (const [index, suffix] of missingRoots.entries()) {
    const result = diff({ root: `missing-${suffix}`, diff: sourceChange });
    assert.equal(result.errors.length, 1, `${suffix} should produce one error`);
    assert.match(result.errors[0], new RegExp(REQUIRED_REPORT_FILES[index].replace('.', '\\.') + '.*missing'));
  }
});

test('each required file is checked when empty', () => {
  const emptyRoots = ['json', 'html', 'md', 'bib'];
  for (const [index, suffix] of emptyRoots.entries()) {
    const result = diff({ root: `empty-${suffix}`, diff: sourceChange });
    assert.equal(result.errors.length, 1, `${suffix} should produce one error`);
    assert.match(result.errors[0], new RegExp(REQUIRED_REPORT_FILES[index].replace('.', '\\.') + '.*empty'));
  }
});

test('rejects a directory where a regular report file is required', () => {
  const result = diff({ root: 'nonregular', diff: sourceChange });
  assert.equal(result.errors.length, 1);
  assert.match(result.errors[0], /codebase-metadata\.json.*not a regular file/);
});

test('counts an unchanged existing bundle for a source-only diff', () => {
  const result = diff({ root: 'unchanged', diff: 'M\tcode/source/module.c\n' });
  assert.deepEqual(result.errors, []);
});

test('does not require retroactive reports for an existing codebase update', () => {
  const result = diff({
    root: 'unrelated',
    diff: 'M\tcode/source/module.c\n',
    baselineSources: ['source'],
  });
  assert.deepEqual(result.sources, []);
  assert.deepEqual(result.errors, []);
});

test('does not sweep unrelated or task-only changes', () => {
  const unrelated = diff({ root: 'unrelated', diff: 'M\ttasks/example/task.toml\n' });
  assert.deepEqual(unrelated.errors, []);
  assert.deepEqual(unrelated.sources, []);

  const rootFile = diff({ root: 'root-file', diff: 'M\tcode/README.md\n' });
  assert.deepEqual(rootFile.errors, []);
  assert.deepEqual(rootFile.sources, []);
});

test('reports removal or emptying of a bundle for a source still present', () => {
  const result = diff({ root: 'unrelated', diff: 'D\tcodebase-reports/source/codebase-metadata.json\nM\tcode/source/module.c\n' });
  assert.equal(result.errors.length, REQUIRED_REPORT_FILES.length);
  assert.match(result.errors[0], /codebase-reports\/source\/codebase-metadata\.json/);
});

test('does not fabricate an obligation for a completely removed source', () => {
  const result = diff({ root: 'removed', diff: 'D\tcode/removed/module.c\n' });
  assert.deepEqual(result.sources, []);
  assert.deepEqual(result.errors, []);
});

test('checks the destination of a source rename in the final tree', () => {
  const result = diff({ root: 'renamed', diff: 'R100\tcode/old-source/module.c\tcode/new-source/module.c\n' });
  assert.deepEqual(result.sources, ['new-source']);
  assert.deepEqual(result.errors, []);
});

test('fails explicitly when BASE_REF is absent or cannot be looked up', () => {
  const unset = checkCodebaseReports({ root: path.join(FIXTURES, 'valid'), baseRef: '' });
  assert.equal(unset.errors.length, 1);
  assert.match(unset.errors[0], /BASE_REF is unset or empty/);

  const missingRef = checkCodebaseReports({ root: path.join(FIXTURES, 'valid'), baseRef: 'not-a-real-baseline' });
  assert.equal(missingRef.errors.length, 1);
  assert.match(missingRef.errors[0], /git diff against BASE_REF 'not-a-real-baseline' failed/);
});
