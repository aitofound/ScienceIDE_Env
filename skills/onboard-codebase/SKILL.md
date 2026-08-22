---
name: onboard-codebase
description: Use when bringing a scientific codebase into SciAccelBench as a set of graded checks — exploring its structure, finding and running its own test suite, publishing a landscape page that explains it, writing a codebase-specific authoring skill, and putting each pass policy in front of a human before it ships. Triggers on onboarding a codebase, "turn this repo into checks", finding official tests, test landscape, pass policy proposal, tolerance review, ADMIT/MEASURED/DEFER triage, or building the per-check Dockerfiles for a new tasks/<codebase>/ package.
---

# Onboarding a codebase

You are given a scientific codebase and asked to turn it into graded checks.
This is the pipeline. It ends with a human approving every pass policy, and it
does not end any earlier.

**This skill is codebase-agnostic on purpose.** It never assumes a language, a
build system, a physics, or a file format. Everything specific to a codebase is
*produced* by Phase 4 — as its own skill — rather than written here.

> **Sibling, not duplicate.** `skill/SKILL.md` at the repository root is the
> *submission* path: a scientist arrives with their own code and a rough idea.
> This skill runs the other direction: a benchmark author arrives at a codebase
> that already exists and has to work out what it can honestly be graded on.

---

## Three rules that override everything below

**1. You do not invent the science.** A benchmark whose tolerances were chosen
by a language model measures nothing. Every number a check enforces must come
from one of exactly two places:

- **read**, out of the codebase's source, with a `file:line` citation, or
- **measured**, by running the incumbent two legitimately different ways.

A plausible number is worse than a missing one. When you cannot get either,
say so and stop — do not fill the gap to make the package look complete.

**2. You do not invent rules.** `TEMPLATE/checks/CONTRACT.md`,
`CONTRIBUTING.md` and `scripts/validate.mjs` are the specification. This skill
navigates them. When a field list or a signature is needed, read the source
file — do not recall it. When the validator disagrees with you, it is right.

**3. Nothing ships without a human approval line.** Phase 5 is a gate, not a
formality. A check whose pass policy has not been approved does not get
committed, however green its CI is.

---

## Vocabulary

Fixed across every codebase. The codebase-specific skill you write in Phase 4
inherits these words and adds only what its domain genuinely needs.

| word | meaning |
|---|---|
| **codebase** | one scientific program, packaged as one task directory |
| **check** | the graded unit. One configuration, one Dockerfile, runs with no arguments, passes or fails on its own. A codebase scores `k/n` over its checks. |
| **oracle** | whatever decides that a run was correct. May be an assertion in the codebase, a committed reference output, a human looking at a plot, or nothing. |
| **window** | how much of the computation the check actually runs, and how often it records. **Part of the pass policy, never of the build.** |
| **floor** | the difference two *correct* builds produce. Rounding, reassociation, FMA, thread order. |
| **bound** | the tolerance the validator enforces. |
| **signal** | the difference a *faulty* port produces. |
| **hazard** | a named place where the code makes a discrete choice from a floating-point comparison, or where the number of operations is data-dependent. Cited `file:line`. Hazards raise the floor. |
| **warrant** | the written argument that the floor really is below the bound — *asserted* from source, or *measured* from a number. |

**The validity condition:**

```
floor  <<  bound  <<  signal
```

A check is worth shipping only when both gaps are wide. Everything in Phases 1
through 4 exists to find out whether they are, and Phase 5 exists so a human
agrees they are before anyone is graded on it.

---

## The five phases

Each phase has one deliverable and one exit gate. Do not start the next phase
until the gate is green. If a gate cannot go green, report it — never lower it.

Phases 1–3 are reconnaissance and are safe to run broadly. Phase 4 commits you
to a design. Phase 5 costs a human's attention, so arrive prepared.

---

### Phase 1 · MAP — structure, and where the tests are

Shallow-clone at a pinned commit or tag. Everything downstream must be
reproducible against that exact pin, so record it before you read anything.

Find out, yourself, before asking anyone:

- **The build system and its age.** autoconf, hand-written Makefiles per
  platform, CMake, a Fortran tree, a Python package with compiled extensions.
  This is what the Dockerfile has to reproduce and it is usually the hardest
  part of the whole job.
- **The computational core**, and its boundary. What calls it, what it calls,
  what state it touches.
- **Where the codebase keeps its own tests**, under whatever name it uses:
  `tests/`, `test/`, `Test_Problems/`, `examples/`, `regression/`, `benchmarks/`,
  `validation/`, `verification/`, `Examples/`, or a CI config that names files
  nothing else references.
- **What each test is *for*.** A unit test of a helper function, a regression
  test with a committed output, a demonstration that reproduces a figure from
  a paper, and a performance benchmark are four different things and only some
  of them can become checks.
- **How correctness is currently decided.** This is the most important question
  in the phase and it is often uncomfortable to answer. Look for assertions,
  `.ref`/`.gold`/expected-output files, a comparison script, a CI workflow that
  actually runs something. Their *absence* is a finding, not a failure to
  search: a great deal of published scientific software has no automated
  oracle at all, and knowing that early changes the whole plan.

Classify every test you found by **oracle class** — see
`references/oracle-classes.md`. Classify per test, not per repository; mixed
codebases are normal.

> **Deliverable** — `tasks/<codebase>/authoring/MAP.md`: the pin, the build
> system, the test inventory with one row per test, and each row's oracle class.
>
> **Gate** — every test directory in the tree appears in the inventory, and
> every row has an oracle class with the evidence that put it there (the
> assertion, the reference file, the CI line, or "none found, searched X").

---

### Phase 2 · RUN — does any of it actually work?

A test that has never been run is a rumour. Build the codebase in a container
and run its own tests, unmodified, before designing anything.

**Announce the cost first.** Compiling a large scientific codebase is minutes
per build, and a full test suite can be hours. Say what you are about to spend
before you spend it, and clean up images and output directories immediately
afterwards.

Record per test: does it build, does it run, how long, what does it emit, and —
if it has an oracle — does it pass. Expect to find some that do not, and treat
that as data about the codebase rather than as your bug to fix.

Then the two questions that decide everything in Phase 5:

- **Is it deterministic?** Run the same test twice in the same image. Byte
  identical or not? If not, find out why before going further: an RNG without a
  fixed seed, a timestamp in the output, a thread-order reduction, a hash
  iteration order.
- **What does a legitimate difference cost?** Build the same source a second,
  genuinely different way — a different optimisation level, a different FP
  contraction mode, a different libm — and compare. That number is the
  **floor**, and without it no bound is defensible.

Beware the null experiment. Changing a flag that the target hardware cannot act
on produces a byte-identical binary and a measured difference of exactly zero,
which reads like a wonderful result and means nothing. Confirm the two binaries
differ (`sha256sum`) and that the intended instructions actually changed before
believing a floor of `0.0`.

> **Deliverable** — `tasks/<codebase>/authoring/RUNS.md`: per test, build/run
> status, wall time, outputs produced, oracle verdict, determinism verdict, and
> the measured floor with how it was produced.
>
> **Gate** — at least one test builds and runs reproducibly in a container, and
> the floor has been measured at least once against two genuinely different
> binaries.

---

### Phase 3 · EXPLAIN — the landscape page

Write a single self-contained HTML page that lets someone who has never seen
this codebase understand what it is, what it ships as tests, and what can
honestly be graded. This is not decoration: it is the artifact the human
reviewer in Phase 5 reads first, and writing it is how you find out whether you
actually understand the codebase yet.

Required content is in `references/landscape-page.md`. In short: what the code
computes, the module/problem/config structure with real counts, the test
inventory by oracle class, the determinism findings, the measured floor, and an
explicit list of what is **not** gradeable and why.

**Serve it on localhost. Do not publish it.**

```sh
python3 -m http.server 8000 --directory tasks/<codebase>/authoring/
```

Then give the human the URL. This is a standing constraint of this repository:
onboarding pages are internal working documents, they contain unreviewed claims
about someone else's software, and they are not for an external host.

> **Deliverable** — `tasks/<codebase>/authoring/landscape.html`, served locally.
>
> **Gate** — the page states, in numbers, how many candidate checks exist and
> how many are ruled out, and every ruled-out group names its reason.

---

### Phase 4 · CURATE — write the codebase's own authoring skill

Now you know enough to write down how *this* codebase gets turned into checks,
and you write it as a skill so it can be handed to other agents and run in
parallel.

This is the phase that makes the work scale. One agent authoring 400 checks
serially is a queue; twenty agents each authoring one problem is a morning —
but only if the procedure is written down and the work is genuinely disjoint.

Produce two artifacts:

**A. The hazard register** — `tasks/<codebase>/authoring/HAZARDS.md`. Every
place in the source that makes a discrete choice by comparing floats, or whose
operation count is data-dependent, split three ways:

- **LIVE** — reachable; disqualifies an asserted bound whenever its flags are set
- **SELF-LIMITING** — the branch flips but both sides agree at the flip point, so
  the difference stays at rounding scale. Note it; do not demote for it.
- **NOT LIVE** — inside a disabled `#if`, or inside a comment. Name these
  explicitly so nobody rediscovers them and demotes a clean check.

Every entry cites `file:line` against the pin. See
`references/determinism-triage.md`.

**B. The authoring skill** — `tasks/<codebase>/authoring/SKILL.md`. It sits
with the provenance it depends on, travels with the package it serves, and is
not tied to any one agent runtime — keep it out of a vendor's dot-directory.
If a runtime needs it discoverable somewhere specific, put a pointer there,
never the content.
It must contain, in this order:

1. **Scope** — the unit of one agent's work. Pick the level at which the
   *argument* is shared: usually the directory that groups configurations of one
   problem, not the individual configuration and not the whole module.
2. **Isolation** — the exact paths an agent may write, and an explicit
   never-touch list. Shared files must be read-only to agents; a discovery goes
   in the agent's own note and a human merges it. This is not tidiness — a
   violation silently corrupts another agent's work.
3. **Stages with exit gates** — one command per gate, in order, cheapest first.
4. **A debug table** — symptom, cause, fix. Seed it with every failure you hit
   in Phases 1–3, verbatim. This table is the single highest-value part of the
   generated skill and it is the part that only you can write, because you are
   the one who hit them.
5. **A fixed report-back format** — so twenty parallel results are comparable
   and none of them is a transcript.

Split the check folder into **FROZEN** and **AUTHORED**:

- **FROZEN** — everything byte-identical across all checks of this codebase: the
  Dockerfile, the run script, the build recipe. Written by one vendoring script,
  never by an authoring agent, hashed by CI.
- **AUTHORED** — the pass policy, the validator, the discrimination fixtures.
  That is the agent's entire deliverable.

**Push every per-check parameter into the pass policy, and have the build read
it from there.** If the window is set in the Dockerfile *and* stated in the
policy, they will drift, and nothing will notice. One source, read by both the
build and the validator, makes disagreement structurally impossible — and it is
what lets the Dockerfile be identical everywhere, which is what makes parallel
authoring safe.

> **Deliverable** — the hazard register, the authoring skill, and the
> vendoring script.
>
> **Gate** — hand the generated skill, plus one check you authored with it, to
> a fresh agent with no context from this session. If it cannot produce a second
> check from those two things alone, the skill is not finished.

---

### Phase 5 · PROPOSE — one pass policy, one human, one decision

Every check gets a written proposal and an explicit human decision before it
ships. Full format and approval record in `references/review-protocol.md`.

A proposal is one page and answers six questions:

| | |
|---|---|
| **what it runs** | the configuration, the window, and the cost in wall time |
| **what it observes** | the files, the fields, and the comparison rule |
| **the bound** | the number, and its units |
| **the warrant** | asserted from source with citations, or measured with the number |
| **the discrimination** | which fixtures a wrong answer fails, and by how much |
| **what it does not claim** | the failure modes this check is blind to |

Two constraints on the prose, both learned by shipping them wrong:

- **Ask about fidelity, never about physics.** The check asks whether the
  submission faithfully implements the incumbent. It must never ask how large
  an error is still a scientifically acceptable answer — that question has no
  general answer and is not yours to settle on a scientist's behalf.
- **State rules, never results.** The rubric is visible to the solver, so
  anything in it is given away. Input parameters may appear; the solver already
  has the inputs. Anything a correct port must *derive* may not.

**Respect the reviewer's attention.** Do not hand over forty near-identical
pages. Group by shared argument: one full proposal for the group, then a short
diff table for the members — what changes and what stays. A reviewer who can
see the differences reviews forty checks properly; a reviewer given forty
copies of one page approves them all without reading.

The human returns one of three decisions per check, and it is recorded in the
check itself:

- **APPROVE** — ships, with the reviewer and the date
- **REVISE** — with what must change; back to Phase 4 or 5
- **DEFER** — the check is not honestly gradeable yet; it stays out of the
  package and the reason is written down

> **Deliverable** — `tasks/<codebase>/authoring/proposals/` and an approval
> record per check.
>
> **Gate** — every shipped check carries an APPROVE line. Any check without one
> is not in the package.

---

## References

| file | when |
|---|---|
| `references/oracle-classes.md` | Phase 1 — classifying what the codebase's own tests actually prove |
| `references/determinism-triage.md` | Phase 2 and 4 — ADMIT / MEASURED / DEFER, and how to build a hazard register |
| `references/pass-policy-shapes.md` | Phase 5 — the palette of policies, and why there is no universal one |
| `references/landscape-page.md` | Phase 3 — what the HTML must contain |
| `references/review-protocol.md` | Phase 5 — the proposal format and the approval record |

---

## Failure modes of this pipeline

Ordered by how much damage they do.

**A guessed tolerance.** Invisible, permanent, and it silently redefines what
the benchmark measures. Rule 1 exists for this one alone.

**A false FAIL.** A validator that rejects a correct port is the worst error a
grader can make: it is invisible in the transcript and it inflates the apparent
difficulty of the task. Every discrimination fixture set needs at least one
*correct-but-different* case — flushed denormals, a different summation order,
a legitimately different compiler — that must pass.

**A validator that accepts its own near-miss.** Right on the checked statistic,
wrong everywhere else. That is what an agent optimising the statistic rather
than porting the code hands in. Fixtures catch this; prose cannot.

**A fixture that cannot fail.** A near-miss that perturbs a field this
configuration does not have is a no-op, and the "reject" tree is byte-identical
to the reference. It looks like a passing gate and it is testing nothing. Assert
that every perturbation actually changed bytes.

**Skipping Phase 2.** Designing a pass policy for a test nobody has run
produces a confident document about a program that does not build.

**A shared mutable file during fan-out.** Two agents, one table, silent
corruption. Phase 4's isolation list is what prevents it.
