# Ranger PR Identity Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate five open Quantum Harness PRs from Rager to Ranger and place the requested Chinese quotation directly below the standardized English quotation.

**Architecture:** Snapshot the current externally edited PR bodies, publish the existing five image bytes under Ranger-only media paths, and deterministically rebuild each opening while leaving its current technical suffix untouched except for the rename. Apply updates through the GitHub API and compare the returned bodies against locally reconstructed expectations.

**Tech Stack:** GitHub CLI/API, Git, GitHub Markdown, jq, SHA-256 snapshots.

## Global Constraints

- Modify only PRs #214, #215, #216, #217, and #220.
- Do not modify PR #210.
- Preserve current `🌌 Ranger` title wording and all external body edits.
- Remove both visible `Rager` and lowercase `rager` URL/path occurrences.
- Reuse the existing approved image bytes without regeneration.
- Keep the old media branch for auditability; do not delete it.

---

### Task 1: Snapshot Current State

**Files:**
- Create: `/tmp/qh-pr-before-ranger-210.json`
- Create: `/tmp/qh-pr-before-ranger-214.json`
- Create: `/tmp/qh-pr-before-ranger-215.json`
- Create: `/tmp/qh-pr-before-ranger-216.json`
- Create: `/tmp/qh-pr-before-ranger-217.json`
- Create: `/tmp/qh-pr-before-ranger-220.json`

**Interfaces:**
- Consumes: the latest GitHub title/body state after external edits.
- Produces: immutable pre-mutation snapshots used for transformation and #210 comparison.

- [x] **Step 1: Retrieve all six PR snapshots**

Run `gh pr view` with `--json number,title,body,url,state` for #210, #214,
#215, #216, #217, and #220.

Expected: five open in-scope snapshots and one excluded #210 snapshot.

- [x] **Step 2: Assert the current opening shape**

For every in-scope body, verify that the first four lines are the existing
English blockquote. Record whether a Rager image line is currently present;
#215 and #220 are expected to lack one.

Expected: the parser can remove exactly four leading quote lines and zero or
one immediately following Rager image.

### Task 2: Publish Ranger Media Paths

**Files:**
- Rename on new branch: `assets/rager/` to `assets/ranger/`

**Interfaces:**
- Consumes: commit `df38f21` on `media/rager-pr-banners`.
- Produces: branch `media/ranger-pr-banners` and five Ranger-only raw URLs.

- [x] **Step 1: Clone the user's public fork**

Run:

```bash
gh repo clone JunkaiWang-TheoPhy/quantum.harness /tmp/ranger-pr-media
git switch -c media/ranger-pr-banners origin/media/rager-pr-banners
```

Expected: a new local branch containing the five approved images.

- [x] **Step 2: Rename the media directory**

Run:

```bash
git mv assets/rager assets/ranger
git commit -m "assets: migrate Rager banners to Ranger paths"
git push -u origin media/ranger-pr-banners
```

Expected: image bytes remain unchanged and only their tree paths change.

- [x] **Step 3: Verify all new URLs**

Request the five URLs below and require HTTP 200 plus `image/png`:

```text
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/ranger-pr-banners/assets/ranger/pr-214-tenferro-event-horizon.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/ranger-pr-banners/assets/ranger/pr-215-occam-ice-circuit.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/ranger-pr-banners/assets/ranger/pr-216-certified-tensor-cathedral.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/ranger-pr-banners/assets/ranger/pr-217-ed-fci-accretion-states.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/ranger-pr-banners/assets/ranger/pr-220-qcs-ringed-slingshot.png
```

Compare every new file's SHA-256 with the matching old URL.

Expected: all five byte pairs are identical.

### Task 3: Build and Apply Bilingual PR Bodies

**Files:**
- Read: `/tmp/qh-pr-before-ranger-214.json`
- Read: `/tmp/qh-pr-before-ranger-215.json`
- Read: `/tmp/qh-pr-before-ranger-216.json`
- Read: `/tmp/qh-pr-before-ranger-217.json`
- Read: `/tmp/qh-pr-before-ranger-220.json`

**Interfaces:**
- Consumes: latest snapshots and five verified Ranger image URLs.
- Produces: five updated PR title/body pairs.

- [x] **Step 1: Define the exact bilingual quote**

Use this exact Markdown:

```markdown
> Do not go gentle into that good night,<br>
> Old age should burn and rave at close of day;<br>
> Rage, rage against the dying of the light.<br>
> —— [Dylan Thomas](https://www.poetryfoundation.org/poets/dylan-thomas), 「**Do Not Go Gentle into That Good Night**」

> 不要温和地走进那良夜，<br>
> 老年应当在日暮时燃烧咆哮；<br>
> 怒斥，怒斥光明的消逝。<br>
```

- [x] **Step 2: Extract each current technical suffix**

From each current body:

1. remove exactly the first four contiguous blockquote lines;
2. remove the following blank line;
3. remove one immediately following Markdown image when present;
4. remove the image's following blank line;
5. replace `Rager` with `Ranger` and `rager` with `ranger` in the retained
   suffix.

Expected: #215 and #220 begin their retained suffix at
`## Team and challenge`; the other three retain their existing headings.

- [x] **Step 3: Prepend each unique Ranger image**

After the bilingual quote, add the PR-specific image:

```text
#214 pr-214-tenferro-event-horizon.png
#215 pr-215-occam-ice-circuit.png
#216 pr-216-certified-tensor-cathedral.png
#217 pr-217-ed-fci-accretion-states.png
#220 pr-220-qcs-ringed-slingshot.png
```

Use alt text beginning with `Ranger —`.

- [x] **Step 4: Validate every proposed update locally**

Assert:

- title contains `Ranger`;
- title and body contain neither `Rager` nor `rager`;
- body begins with the exact bilingual quote;
- exactly one unique `assets/ranger/` image is present;
- technical suffix equals the transformed current snapshot exactly.

Expected: all five proposals pass before any mutation.

- [x] **Step 5: Patch the five PRs**

Send each validated title/body JSON object to:

```text
PATCH /repos/QuantumBFS/quantum.harness/pulls/214
PATCH /repos/QuantumBFS/quantum.harness/pulls/215
PATCH /repos/QuantumBFS/quantum.harness/pulls/216
PATCH /repos/QuantumBFS/quantum.harness/pulls/217
PATCH /repos/QuantumBFS/quantum.harness/pulls/220
```

Expected: five successful GitHub API responses.

### Task 4: Verify and Record

**Files:**
- Modify: `docs/submission-pr-body.md`
- Modify: `docs/superpowers/plans/2026-07-28-ranger-pr-identity.md`

**Interfaces:**
- Consumes: GitHub's final PR state.
- Produces: exact read-back verification and synchronized private documentation.

- [x] **Step 1: Read back all six PRs**

Retrieve #210 and all five updated PRs.

Expected: every in-scope PR is open; #210 remains excluded.

- [x] **Step 2: Compare final state**

Rebuild each expected body from its pre-update snapshot and require exact
equality with GitHub's returned body. Require five unique image URLs, zero
`Rager`/`rager` occurrences, and exact title preservation.

Expected: all assertions pass.

- [x] **Step 3: Confirm #210 is unchanged**

Compare #210's title and body to its pre-update snapshot.

Expected: byte-for-byte equality.

- [x] **Step 4: Synchronize the #217 body source**

Update `docs/submission-pr-body.md` to exactly match PR #217's final body
apart from the optional final newline.

Expected: no substantive diff between local source and GitHub.

- [x] **Step 5: Run gates, commit, and push**

Run:

```bash
git diff --check
scripts/verify-submission.sh
git add docs/plans/2026-07-28-ranger-pr-identity-design.md \
  docs/superpowers/plans/2026-07-28-ranger-pr-identity.md \
  docs/submission-pr-body.md
git commit -m "docs: complete Ranger PR identity migration"
git push origin main
```

Expected: local verification passes, the private workbench is synchronized,
and its GitHub CI completes successfully.

## Execution Record

- Ranger media commit: `a8e2d2b` on `media/ranger-pr-banners`.
- All five Ranger raw URLs returned HTTP 200 with `Content-Type: image/png`.
- Every Ranger image SHA-256 matched its corresponding Rager-path source.
- PRs #214, #215, #216, #217, and #220 matched their reconstructed expected
  title and body exactly on read-back.
- The five PRs contain five unique `assets/ranger/` image URLs.
- No in-scope title or body contains `Rager` or lowercase `rager`.
- PR #210 remained byte-for-byte unchanged.
