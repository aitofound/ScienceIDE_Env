#!/usr/bin/env node
/* tasks/sa-*.yaml -> pass/fail.
 *
 * The registry has no database behind it — this file IS the acceptance rule
 * for shape. It runs in CI on every pull request that touches tasks/** and
 * locally as `npm run check`.
 *
 * Every violation is collected and printed — a submitter should see the whole
 * bill at once, not one item per push.
 *
 *   node scripts/validate.mjs
 */

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import YAML from 'yaml';

const ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const DIR = path.join(ROOT, 'tasks');

/* Everything the template declares, plus the three registry-metadata keys the
   site reads (short_title for the chart, resource_class for the runtime band,
   record for the standing speedup — written by us, never by a submitting PR).
   Anything outside this list is almost always a typo like `licence:`, and a
   typo'd key would silently render as an absent one. */
const ALLOWED_KEYS = new Set([
  'slug', 'title', 'short_title', 'status',
  'repo_url', 'repo_commit', 'license', 'language_from', 'language_to',
  'loc_in_scope', 'domain',
  'science_summary', 'owner',
  'instruction',
  'public_inputs', 'hidden_inputs_note', 'equivalence_criteria', 'precision_policy',
  'incumbent_description', 'incumbent_runtime', 'incumbent_hardware', 'incumbent_source',
  'resource_class', 'record',
  'verifier_command', 'verifier_repo',
  'exploit_description', 'exploit_score',
  'references', 'notes',
]);

const STATUSES = new Set(['draft', 'review', 'published', 'retired']);

/* A task beyond draft is a claim that the package is complete enough to
   verify. A task without equivalence criteria has no verifier, and a task
   without an owner has nobody who may author them. */
const REQUIRED_BEYOND_DRAFT = [
  'title', 'domain', 'repo_url', 'instruction', 'equivalence_criteria', 'owner',
];

/* Source of truth: the fields table of aitonomy.org's open-problems registry
   (the `public.fields` seed in the site's migrations). The list is static by
   design — a ninth science is a change there first, and then a line here. */
const FIELD_CODES = new Set([
  'mathematics', 'computer-systems', 'physics-astronomy', 'chemistry',
  'biology-biomedicine', 'medicine-clinical', 'earth-climate',
  'materials-engineering',
]);

const RESOURCE_CLASSES = new Set(['R0', 'R1', 'R2', 'R3', 'R4', 'R5']);

if (!fs.existsSync(DIR)) {
  console.error(`no tasks/ directory at ${DIR}`);
  process.exit(1);
}

/* TEMPLATE.yaml lives at the repo root, outside the scanned directory; the
   sa- prefix in the glob keeps any stray copy out of the scan regardless. */
const files = fs.readdirSync(DIR)
  .filter((f) => /^sa-.*\.ya?ml$/.test(f))
  .sort();

if (!files.length) {
  console.error('tasks/ has no sa-*.yaml files');
  process.exit(1);
}

const errors = [];
const seen = new Map(); // slug -> first filename that used it
let count = 0;

const blank = (v) => v === undefined || v === null || String(v).trim() === '';

for (const file of files) {
  const full = path.join(DIR, file);
  let doc;
  try {
    doc = YAML.parse(fs.readFileSync(full, 'utf8'));
  } catch (e) {
    errors.push(`${file}: unparseable YAML — ${e.message}`);
    continue;
  }
  if (!doc || typeof doc !== 'object' || Array.isArray(doc)) {
    errors.push(`${file}: not a YAML mapping`);
    continue;
  }
  count += 1;

  const slug = doc.slug;
  if (blank(slug)) {
    errors.push(`${file}: no slug`);
  } else {
    if (!/^sa-[0-9]{4}$/.test(String(slug))) {
      errors.push(`${file}: slug '${slug}' does not match sa-NNNN`);
    }
    if (`${slug}.yaml` !== file && `${slug}.yml` !== file) {
      errors.push(`${file}: filename does not match slug '${slug}'`);
    }
    if (seen.has(slug)) {
      errors.push(`${file}: slug '${slug}' already used by ${seen.get(slug)}`);
    } else {
      seen.set(slug, file);
    }
  }

  const unknown = Object.keys(doc).filter((k) => !ALLOWED_KEYS.has(k));
  if (unknown.length) {
    errors.push(`${file}: unknown key(s) ${unknown.map((k) => `'${k}'`).join(', ')} — typo, or a key the template does not declare`);
  }

  const status = doc.status;
  if (blank(status) || !STATUSES.has(String(status))) {
    errors.push(`${file}: status '${status ?? ''}' is not one of draft | review | published | retired`);
  } else if (status !== 'draft') {
    for (const req of REQUIRED_BEYOND_DRAFT) {
      if (blank(doc[req])) {
        errors.push(`${file}: status '${status}' requires a non-blank '${req}'`);
      }
    }
  }

  /* Draft or not, a domain that IS written must be a real field code — a
     wrong code would file the leaf under no field at all. */
  if (!blank(doc.domain) && !FIELD_CODES.has(String(doc.domain))) {
    errors.push(`${file}: domain '${doc.domain}' is not one of the eight field codes`);
  }

  if (!blank(doc.record) && !(typeof doc.record === 'number' && doc.record >= 1)) {
    errors.push(`${file}: record '${doc.record}' must be a number >= 1 (a speedup below 1x is not a record)`);
  }
  if (!blank(doc.exploit_score) && typeof doc.exploit_score !== 'number') {
    errors.push(`${file}: exploit_score '${doc.exploit_score}' must be a number`);
  }
  if (!blank(doc.repo_commit) && !/^[0-9a-f]{40}$/.test(String(doc.repo_commit))) {
    errors.push(`${file}: repo_commit '${doc.repo_commit}' is not a full 40-hex SHA — "latest" is not an answer`);
  }
  if (!blank(doc.resource_class) && !RESOURCE_CLASSES.has(String(doc.resource_class))) {
    errors.push(`${file}: resource_class '${doc.resource_class}' is not one of R0–R5`);
  }
}

if (errors.length) {
  console.error(`${errors.length} violation(s) across ${count} task file(s):\n`);
  for (const e of errors) console.error(`  ${e}`);
  process.exit(1);
}

console.log(`ok — ${count} tasks, no violations`);
