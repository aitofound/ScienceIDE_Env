# Oracle classes

Phase 1. Classify **each test**, not the repository — mixed codebases are the
norm, and the mix is usually the most useful thing you learn.

The question is narrow and it is not "is this a good test". It is: **when this
test runs, what decides that the answer was right?**

---

## The four classes

### A — self-judging

The codebase ships a test that asserts and exits nonzero on failure. pytest,
ctest, googletest, a shell script ending in `diff && echo PASS`.

*What it gives you:* the oracle already exists and someone maintained it. The
check can wrap it.

*What you still have to supply:* almost always a **determinism and performance
envelope**, because the existing assertion answers "is this answer acceptable",
not "is this the same computation". A ported implementation can pass a loose
unit test while computing something structurally different.

*Trap:* assertions with tolerances chosen for a different purpose. `assert_close(rtol=1e-3)`
was written to keep CI green across compilers, not to detect a mis-signed flux.
Read the tolerance's history before adopting it. Reusing it uncritically imports
someone else's judgement about a question they were not asking.

### B — referenced

The codebase ships inputs plus committed expected outputs — `.ref`, `.gold`,
`expected/`, a `baseline/` directory — and usually a comparison script.

*What it gives you:* an oracle for the *values*, produced by the authors on
hardware they trusted.

*What you still have to supply:* the comparison rule and the tolerance, because
the reference files alone do not say how close is close enough, and any script
that ships with them encodes an answer to that question that you must read
rather than inherit.

*Trap:* reference files generated once, years ago, on one machine, never
regenerated. Check whether the incumbent still reproduces them **today, in your
container**. If it does not, you have learned something important and the
reference is not an oracle.

### C — demonstrative

The codebase ships example problems that run and produce output, and
correctness is judged by a human — comparing a plot to a figure in a paper, or
recognising a familiar structure in the result.

*What it gives you:* a runnable, meaningful configuration, and usually a
published figure that tells you the run is physically sensible.

*What you have to supply:* **the entire pass policy.** The window, the
observable, the bound, and the warrant. There is no oracle to inherit, so every
number must be read out of the source or measured.

*This is the hardest and the most common class in scientific software.* It is
also where this benchmark adds the most, because the gap it fills is real: the
codebase genuinely cannot tell you whether a port is faithful.

*Trap:* mistaking a demonstration for a regression test because it lives in a
directory called `tests/`.

### D — none

No tests, no examples, or only a build smoke test.

*What it gives you:* nothing.

*What you have to supply:* a configuration invented from scratch, which means
you are now authoring the science as well as the check. Rule 1 says you may not.

*Before proceeding, ask whether this codebase should be onboarded at all.* If
nobody can produce a configuration whose correct answer is known, a check over
it grades nothing. The honest outcome is sometimes to stop.

---

## Recording the classification

One row per test in `MAP.md`. The evidence column is what makes the
classification checkable by someone else:

| test | class | evidence |
|---|---|---|
| `tests/test_grid.py::test_spacing` | A | `assert np.allclose(...)` at `tests/test_grid.py:44` |
| `regression/shock/` | B | `regression/shock/expected.dat`, compared by `regression/run.sh:19` |
| `Test_Problems/HD/Sod` cfg 01–09 | C | no `.ref` in tree; `README` cites Figure 3 of the method paper |
| `benchmarks/scaling.sh` | — | performance only, not a correctness test |

"None found, searched `tests/ test/ ci/ .github/`" is a legitimate and valuable
evidence string. Write it rather than leaving the cell empty — an empty cell
reads as "not yet looked at", which is a different and much weaker statement.

---

## What the class changes downstream

| | A | B | C | D |
|---|---|---|---|---|
| oracle exists | yes | for values | no | no |
| Phase 5 must invent | envelope | rule + bound | everything | everything, incl. the configuration |
| effort per check | low | medium | high | prohibitive |
| risk of inheriting a bad number | **high** | **high** | low | n/a |

The inversion in the last row is the one to remember. Classes A and B look
easy, and their real danger is adopting a tolerance the authors chose for a
question you are not asking. Class C is laborious and its danger is different:
you have to be the one who reads the source, and if you skip that you are back
to guessing.
