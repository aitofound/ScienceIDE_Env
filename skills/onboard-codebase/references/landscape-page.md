# The landscape page

Phase 3. One self-contained HTML file that explains a codebase and its tests to
someone who has never seen either.

It has two jobs, and the second is the real one:

1. It is what the human reviewer reads before Phase 5, so the quality of every
   approval decision depends on it.
2. **Writing it is how you find out whether you actually understand the
   codebase.** A section you cannot fill in is a gap in the work, not a gap in
   the document. Do not paper over one; go back to Phase 1 or 2 and close it.

---

## Serve it locally. Do not publish it.

```sh
python3 -m http.server 8000 --directory tasks/<codebase>/authoring/
```

Then hand over `http://localhost:8000/landscape.html`.

This is a standing constraint of this repository. The page carries unreviewed
claims about software someone else wrote and maintains, sometimes including
"this test has not passed since 2019" or "there is no oracle here at all".
Those belong in an internal working document, not on an external host. Do not
call the Artifact tool for it.

The craft guidance in the `artifact-design` skill still applies to how the page
looks and reads — it is a deliverable a human has to work from, not a data
dump. Only the publishing step is off.

---

## Required sections

Each one has a completion test. If you cannot answer the test, the section is
not done.

### 1 · What this code computes

Two or three paragraphs in the language of the field, not of the source tree.
What problem, what method, roughly what scale of run.

*Test:* someone from an adjacent field can say what the program is for.

### 2 · The pin

Upstream URL, version, commit or tag, checksum. The build system. The
dependencies the container has to satisfy.

*Test:* a reader can obtain exactly these bytes.

### 3 · Structure, with counts

The grouping levels the codebase actually uses — module, problem, configuration,
or whatever it calls them — with **real numbers at each level**, and the
vocabulary mapped onto this skill's words.

*Test:* every count is a number you obtained by enumerating, not an estimate.

### 4 · The test inventory

One row per test: its name, its **oracle class** (`oracle-classes.md`), the
evidence for that class, whether it builds, whether it runs, how long, and
whether it currently passes.

*Test:* every test directory in the tree appears, including the ones that fail
and the ones that turned out not to be tests.

### 5 · Determinism

Does the code reproduce itself run to run in the same image? If not, why not,
with a citation. Then the **measured floor**: the number, the two builds it came
from, and the evidence that those two builds genuinely differ.

*Test:* the floor is a number with units, not an adjective.

### 6 · The triage

ADMIT / MEASURED / DEFER counts, rolled up by whatever grouping level the
codebase uses, as a table. Then the hazards that drive the demotions, each with
a `file:line`.

*Test:* the three counts sum to the total from section 3.

### 7 · What is not gradeable, and why

The single most valuable section, and the one most often left out.

Every excluded group, its size, and its reason. "Iteration count is
data-dependent at `solver.c:212`, so an elementwise difference measures
nothing" is a reason. "Too hard" is not.

*Test:* excluded plus included equals the total, and every exclusion names a
mechanism.

### 8 · What is being proposed

How many checks, at what cost, and what the human is about to be asked to
approve in Phase 5.

*Test:* a reader can decide whether the plan is worth their time before opening
a single proposal.

---

## Practical notes

- **Self-contained.** Inline the CSS and any script. The page gets moved around,
  attached to messages, and opened from a filesystem.
- **Tables, with real numbers.** This page's value is that it is quantitative.
  A prose paragraph where a table belongs hides exactly the counts a reviewer
  needs.
- **Cite the source.** Every mechanical claim carries a `file:line` against the
  pin. A claim without one is an opinion, and the reviewer cannot check it.
- **Show the failures.** Tests that do not build, references that no longer
  reproduce, configurations that were dropped. A landscape page that reports
  only successes is not a landscape.
- **Theme-aware and responsive** if it costs you nothing — it will be read on
  someone else's machine, in someone else's terminal-adjacent browser.
