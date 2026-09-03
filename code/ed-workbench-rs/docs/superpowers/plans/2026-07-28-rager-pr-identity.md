# Rager PR Identity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rename the team to Rager in five open Quantum Harness PRs and give each PR the requested Dylan Thomas quotation plus a unique GPT Image widescreen banner.

**Architecture:** Generate and visually inspect five original 21:9 banners locally, then publish them to a dedicated media branch in the user's public Quantum Harness fork so no implementation PR gains binary files. Transform body snapshots deterministically, submit each title/body pair through the GitHub API, and read every PR back to verify exact copy, unique image URLs, preserved technical content, and the exclusion of PR #210.

**Tech Stack:** Codex built-in GPT Image, GitHub Markdown, GitHub CLI/API, Git, SHA-256 body snapshots, local image inspection.

## Global Constraints

- Modify only `QuantumBFS/quantum.harness` PRs #214, #215, #216, #217, and #220.
- Do not modify closed PR #210.
- Replace team-name variants `Rewrite It In Rust!`, `Rewrite It in Rust`, and standalone `RIIR` with `Rager`.
- Start every body with the exact four-line Dylan Thomas block quote from the approved design.
- Use five different original 21:9 GPT Image images with no text, actors, characters, logos, or copied movie shots.
- Preserve every existing technical paragraph, table, link, completion date, and validation record after the new header.
- Host images on `JunkaiWang-TheoPhy/quantum.harness` branch `media/rager-pr-banners`.

---

### Task 1: Generate and Inspect Five Banners

**Files:**
- Create: `output/imagegen/rager-pr-banners/pr-214-tenferro-event-horizon.png`
- Create: `output/imagegen/rager-pr-banners/pr-215-occam-ice-circuit.png`
- Create: `output/imagegen/rager-pr-banners/pr-216-certified-tensor-cathedral.png`
- Create: `output/imagegen/rager-pr-banners/pr-217-ed-fci-accretion-states.png`
- Create: `output/imagegen/rager-pr-banners/pr-220-qcs-ringed-slingshot.png`

**Interfaces:**
- Consumes: the five visual concepts in `docs/plans/2026-07-28-rager-pr-identity-design.md`.
- Produces: five original image files with distinct compositions for the media branch.

- [x] **Step 1: Create the project output directory**

Run:

```bash
mkdir -p output/imagegen/rager-pr-banners
```

Expected: one task-specific image output directory in the private workbench.

- [x] **Step 2: Generate each banner with Codex built-in GPT Image**

Use one generation call for each of these exact prompts:

```text
#214: Original ultra-widescreen 21:9 cinematic space banner. A precise luminous
tensor lattice made of fine amber and cyan lines bends gravitationally around
a vast black hole and its thin accretion disk. Scientific, austere, high
contrast, deep black negative space, no people, no text, no letters, no logos,
no recognizable film spacecraft, and no reproduction of an identifiable
movie shot.

#215: Original ultra-widescreen 21:9 cinematic space banner. A frozen alien
plain under a dark indigo sky, crossed by one elegant minimal circuit pattern
glowing beneath translucent ice; a tiny original geometric probe in the far
distance for scale. Cold blue and white palette, solitary and rigorous, no
people, no text, no letters, no logos, no recognizable film spacecraft, and
no reproduction of an identifiable movie shot.

#216: Original ultra-widescreen 21:9 cinematic space banner. Monumental cosmic
architecture opens onto a star field, containing a crystalline tensor lattice
whose connected geometric cells suggest formal proofs and certified
transformations. Violet, silver, and warm gold palette, symmetrical but not
frontal, no people, no text, no letters, no logos, no recognizable film
spacecraft, and no reproduction of an identifiable movie shot.

#217: Original ultra-widescreen 21:9 cinematic scientific space banner. A
radiant accretion disk surrounds a massive black hole while hundreds of small
determinant-like stellar states form discrete orbital paths and matrix
connections around it. Copper, white, and midnight-blue palette, dynamic
diagonal composition, no people, no text, no letters, no logos, no
recognizable film spacecraft, and no reproduction of an identifiable movie
shot.

#220: Original ultra-widescreen 21:9 cinematic space banner. A small wholly
original spacecraft performs a gravity slingshot above the rings of a majestic
ringed planet, with sparse constellation-like arithmetic nodes trailing along
its curved trajectory. Pale gold, charcoal, and muted teal palette, immense
scale and motion, no people, no text, no letters, no logos, no recognizable
film spacecraft, and no reproduction of an identifiable movie shot.
```

The explicit `gpt-image-2` CLI dry-run was valid, but live generation returned
404 because `OPENAI_BASE_URL` pointed to `api.deepseek.com`, which exposed no
image models. After the user explicitly authorized the built-in route, generate
one image per prompt with Codex built-in GPT Image. The user rejected the first
generic science-fiction batch; regenerate with grounded 70mm/IMAX
cinematography, practical realism, restrained color, and no neon UI overlays.

Expected: five visually different near-21:9 images matching the
PR-to-concept table.

- [x] **Step 3: Normalize generated files**

Convert the five outputs to RGB PNG if necessary and name them exactly:

```text
pr-214-tenferro-event-horizon.png
pr-215-occam-ice-circuit.png
pr-216-certified-tensor-cathedral.png
pr-217-ed-fci-accretion-states.png
pr-220-qcs-ringed-slingshot.png
```

Expected: five readable, non-empty PNG files.

- [x] **Step 4: Inspect every image**

Open all five files at original detail and verify:

- the aspect ratio is widescreen;
- the concepts are visibly different;
- no text or logo appears;
- no real actor or recognizable film frame appears;
- no image has obvious generation defects.

Expected: all five images pass or the failing image is regenerated.

### Task 2: Publish the Media Branch

**Files:**
- Create on `media/rager-pr-banners`: `assets/rager/pr-214-tenferro-event-horizon.png`
- Create on `media/rager-pr-banners`: `assets/rager/pr-215-occam-ice-circuit.png`
- Create on `media/rager-pr-banners`: `assets/rager/pr-216-certified-tensor-cathedral.png`
- Create on `media/rager-pr-banners`: `assets/rager/pr-217-ed-fci-accretion-states.png`
- Create on `media/rager-pr-banners`: `assets/rager/pr-220-qcs-ringed-slingshot.png`

**Interfaces:**
- Consumes: the inspected PNG files from Task 1.
- Produces: five stable public raw GitHub URLs used by Task 3.

- [x] **Step 1: Clone the user's fork into a task-specific temporary directory**

Run:

```bash
gh repo clone JunkaiWang-TheoPhy/quantum.harness /tmp/rager-pr-media
```

Expected: a clone whose `origin` is the user's public fork.

- [x] **Step 2: Create the media branch from the fork's main branch**

Run:

```bash
git switch -c media/rager-pr-banners origin/main
```

Expected: the new branch has no relationship to the five implementation PR
heads beyond their shared fork.

- [x] **Step 3: Add the five inspected images**

Copy only the named image files into `assets/rager/`, then run:

```bash
git add assets/rager
git diff --cached --stat
git commit -m "assets: add Rager PR banners"
git push -u origin media/rager-pr-banners
```

Expected: one media-only commit containing exactly five PNG files.

- [x] **Step 4: Verify the public image URLs**

Request these five exact URLs:

```text
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-214-tenferro-event-horizon.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-215-occam-ice-circuit.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-216-certified-tensor-cathedral.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-217-ed-fci-accretion-states.png
https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-220-qcs-ringed-slingshot.png
```

Expected: HTTP 200 and `Content-Type: image/png`.

### Task 3: Transform the Five PRs

**Files:**
- Read: `/tmp/qh-pr-214.json`
- Read: `/tmp/qh-pr-215.json`
- Read: `/tmp/qh-pr-216.json`
- Read: `/tmp/qh-pr-217.json`
- Read: `/tmp/qh-pr-220.json`
- Create: `/tmp/qh-pr-214-body-rager.md`
- Create: `/tmp/qh-pr-215-body-rager.md`
- Create: `/tmp/qh-pr-216-body-rager.md`
- Create: `/tmp/qh-pr-217-body-rager.md`
- Create: `/tmp/qh-pr-220-body-rager.md`

**Interfaces:**
- Consumes: exact original body snapshots and the five public image URLs.
- Produces: five updated GitHub PR title/body pairs.

- [x] **Step 1: Build each body from its snapshot**

Prepend the exact quote followed by the matching image line from this table:

| PR | Exact image line |
|---|---|
| #214 | `![Rager — tensor waves at the event horizon](https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-214-tenferro-event-horizon.png)` |
| #215 | `![Rager — a circuit hidden beneath an alien ice field](https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-215-occam-ice-circuit.png)` |
| #216 | `![Rager — a certified tensor structure in five-dimensional space](https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-216-certified-tensor-cathedral.png)` |
| #217 | `![Rager — determinant states around a gravitationally lensed accretion disk](https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-217-ed-fci-accretion-states.png)` |
| #220 | `![Rager — a gravity slingshot past a ringed world](https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-220-qcs-ringed-slingshot.png)` |

The full header shape is:

```markdown
> Do not go gentle into that good night,<br>
> Old age should burn and rave at close of day;<br>
> Rage, rage against the dying of the light.<br>
> —— [Dylan Thomas](https://www.poetryfoundation.org/poets/dylan-thomas), 「**Do Not Go Gentle into That Good Night**」

![Rager — tensor waves at the event horizon](https://raw.githubusercontent.com/JunkaiWang-TheoPhy/quantum.harness/refs/heads/media/rager-pr-banners/assets/rager/pr-214-tenferro-event-horizon.png)

```

In the retained original body, replace only team-name occurrences. Do not
summarize or reflow the technical content.

- [x] **Step 2: Set these exact titles**

```text
#214 [agent-kb] Rager: tenferro-rs verification from ED/FCI workloads
#215 [agent-kb] Rager: port Occam's Circuit verification to Rust (Finished, July 28th)
#216 [agent-kb] Rager: design a certified tensor DSL
#217 [e.d.] Rager: rust ED/FCI workbench for electronic-structure methods (Finished, July 27th)
#220 [qcs] Rager: recover hidden arithmetic functions with compact circuits (Finished, July 28th)
```

- [x] **Step 3: Check each transformed body before mutation**

For every body, assert:

- it begins with the exact quote;
- it contains the matching unique image URL once;
- it contains `Rager`;
- it contains none of `Rewrite It In Rust`, `Rewrite It in Rust`, or `RIIR`;
- removing the new quote/image header and reversing the team-name substitution
  yields the original snapshot, except for PR #217's redundant RIIR label.

Expected: all assertions pass locally.

- [x] **Step 4: Update the five PRs**

Run `gh pr edit` against `QuantumBFS/quantum.harness` with the exact title and
the matching `--body-file` for each PR.

Expected: all five API mutations succeed.

### Task 4: Read Back and Verify

**Files:**
- Create: `/tmp/qh-pr-rager-verification.json`
- Modify: `docs/superpowers/plans/2026-07-28-rager-pr-identity.md`

**Interfaces:**
- Consumes: GitHub's post-update PR state.
- Produces: an auditable verification record and completed implementation plan.

- [x] **Step 1: Retrieve all five updated PRs and excluded PR #210**

Run `gh pr view` for #210, #214, #215, #216, #217, and #220 with
`--json number,title,body,url`.

Expected: six current snapshots.

- [x] **Step 2: Validate the five in-scope PRs**

Assert for each in-scope PR:

- exact expected title;
- exact quote prefix;
- exactly one `assets/rager/` image;
- five image URLs are globally unique;
- no old team-name variants remain;
- all pre-update Markdown links outside the inserted header remain present.

Expected: no validation failures.

- [x] **Step 3: Validate the excluded PR**

Compare PR #210's post-run title and body to the pre-run snapshot.

Expected: byte-for-byte equality.

- [x] **Step 4: Complete and commit the plan record**

Mark every completed checkbox in this file, then run:

```bash
git add docs/superpowers/plans/2026-07-28-rager-pr-identity.md
git commit -m "docs: record Rager PR identity rollout"
git push origin main
```

Expected: the private workbench records the external PR identity migration.

## Execution Record

- Built-in output dimensions: `1915 × 821` pixels for all five images
  (`2.3325:1`, effectively 21:9).
- Media commit: `df38f21` on `media/rager-pr-banners`.
- Every raw image URL returned HTTP 200 with `Content-Type: image/png`.
- PRs #214, #215, #216, #217, and #220 matched their exact expected
  title/body transformations on read-back.
- Five unique Rager image URLs were present.
- No in-scope title or body retained `Rewrite It In Rust`, capitalization
  variants, or `RIIR`.
- PR #210's title and body remained byte-for-byte unchanged.
