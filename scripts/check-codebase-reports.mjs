#!/usr/bin/env node
/**
 * Mechanical new-source gate for the four-file codebase report bundle.
 *
 * A first-time source/vendor introduction is scoped from the final tree against
 * BASE_REF. Existing codebases may be maintained without retroactive report
 * paperwork; a fully removed source creates no obligation.  The gate deliberately checks
 * only regular-file/non-empty presence, not report contents.
 */

import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';

export const REQUIRED_REPORT_FILES = Object.freeze([
  'codebase-metadata.json',
  'codebase-metadata.html',
  'codebase-metadata.md',
  'references.bib',
]);

function readBaselineDiff(root, baseRef) {
  if (typeof baseRef !== 'string' || baseRef.trim() === '') {
    throw new Error('BASE_REF is unset or empty');
  }
  try {
    return execFileSync(
      'git',
      ['diff', '--name-status', '-M', baseRef, '--', 'code/', 'codebase-reports/'],
      { cwd: root, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024, stdio: ['ignore', 'pipe', 'pipe'] },
    );
  } catch (error) {
    const detail = String(error.stderr ?? error.message ?? error).trim().split('\n')[0];
    throw new Error(`git diff against BASE_REF '${baseRef}' failed${detail ? `: ${detail}` : ''}`);
  }
}

function parseNameStatus(diffText) {
  const changes = [];
  for (const line of String(diffText).split(/\r?\n/)) {
    if (!line.trim()) continue;
    const columns = line.split('\t');
    const status = columns[0] ?? '';
    if (!status) continue;
    const paths = /^[RC]/.test(status) ? columns.slice(-2) : [columns[1]];
    for (const changedPath of paths) {
      if (changedPath) changes.push({ status, path: changedPath });
    }
  }
  return changes;
}

function sourceNameForPath(changedPath, root) {
  const parts = String(changedPath).split('/');
  if (parts[0] !== 'code' || !parts[1] || parts[1].startsWith('.')) return null;
  // Git normally reports files below the source root.  The second case also
  // handles a source represented as a submodule/gitlink at code/<source>.
  if (parts.length >= 3) return parts[1];
  if (parts.length === 2) {
    try {
      if (fs.statSync(path.join(root, 'code', parts[1])).isDirectory()) return parts[1];
    } catch {
      // A deleted source root is intentionally not an obligation.
    }
  }
  return null;
}

function sourceExistsAtBase(root, baseRef, source) {
  try {
    execFileSync('git', ['cat-file', '-e', `${baseRef}:code/${source}`], {
      cwd: root,
      stdio: ['ignore', 'ignore', 'ignore'],
    });
    return true;
  } catch {
    // readBaselineDiff already proved BASE_REF itself exists. A failed lookup
    // therefore means this source path was absent at the baseline.
    return false;
  }
}

function newlyIntroducedSourceDirs(root, changes, baseRef, baselineSources) {
  const sourceNames = new Set();
  for (const change of changes) {
    const source = sourceNameForPath(change.path, root);
    if (!source) continue;
    const sourceDir = path.join(root, 'code', source);
    try {
      if (!fs.statSync(sourceDir).isDirectory()) continue;
    } catch {
      // Missing source root: complete deletion/rename-away, not a new vendor.
      continue;
    }
    const existed = baselineSources === undefined
      ? sourceExistsAtBase(root, baseRef, source)
      : baselineSources.includes(source);
    if (!existed) sourceNames.add(source);
  }
  return [...sourceNames].sort();
}

function reportFileProblem(file) {
  let info;
  try {
    // lstat rejects symlinks: the contract asks for four regular files.
    info = fs.lstatSync(file);
  } catch (error) {
    if (error?.code === 'ENOENT') return 'missing';
    return `cannot be inspected (${error.message})`;
  }
  if (!info.isFile()) return 'not a regular file';
  if (info.size === 0) return 'empty';
  return null;
}

/**
 * Return { errors, sources } for the first-time source/vendor report gate.
 *
 * `diffText` is intentionally injectable for hermetic unit tests. Production
 * callers omit it, which requires BASE_REF and performs the real git lookup;
 * baseline failures are errors and never silently treated as "no changes".
 */
export function checkCodebaseReports({
  root,
  baseRef = process.env.BASE_REF,
  diffText,
  baselineSources,
} = {}) {
  const repoRoot = path.resolve(root ?? process.cwd());
  let diff;
  try {
    diff = diffText === undefined ? readBaselineDiff(repoRoot, baseRef) : String(diffText);
  } catch (error) {
    return {
      errors: [`codebase report gate could not determine changed sources: ${error.message}`],
      sources: [],
    };
  }

  const sources = newlyIntroducedSourceDirs(
    repoRoot,
    parseNameStatus(diff),
    baseRef,
    baselineSources,
  );
  const errors = [];
  for (const source of sources) {
    const reportId = source;
    const reportDir = path.join(repoRoot, 'codebase-reports', reportId);
    for (const filename of REQUIRED_REPORT_FILES) {
      const file = path.join(reportDir, filename);
      const problem = reportFileProblem(file);
      if (problem) {
        errors.push(
          `codebase-reports/${reportId}/${filename}: required for changed source ` +
          `code/${source}/ and must be a regular non-empty file (${problem})`,
        );
      }
    }
  }
  return { errors, sources };
}

if (import.meta.url === `file://${process.argv[1]}`) {
  const result = checkCodebaseReports();
  for (const error of result.errors) console.error(error);
  if (result.errors.length) process.exitCode = 1;
}
