# stabilizer-simulation: authoring notes

Hidden at Harbor runtime, not part of the contract. `comment/pipeline/` is
written only by the CLI; this file is the human-readable story.

## Module

Owns Stim's bit-parallel core: `src/stim/mem/` (the packed-word primitives and
the three `bitword` backends), the frame and tableau simulators that consume
them, and `src/stim/stabilizers/` (Pauli-string and tableau algebra).

Separate from `error-analysis` **on measurement, not taxonomy**. Upstream's own
`stim_perf` puts this module's cost in dense packed-word propagation
(`FrameSimulator` 3.9–8.1 ms, `tableau_iter_all_3q` 140 ms, `tableau_random_1000`
61 ms) and `error-analysis`'s in sparse graph work (`ErrorAnalyzer` 370–490 ms,
`find_graphlike_logical_error` up to 410 ms). A port that vectorised the frame
simulator would do nothing for those paths.

## Two check families, deliberately different shapes

| check | policy | bound | measured spread | margin |
|---|---|---|---|---|
| `frame-simulator-shot-batch` **[accel]** | invariants | 6 own bounds, `1e-5`–`5e-3` | 2.67e-6–1.53e-3 | 3.3–4.6 |
| `detection-event-sampling` | invariants | 6 own bounds, `2e-4`–`4e-3` | 4.97e-5–1.26e-3 | 3.2–4.0 |
| `tableau-algebra-composition` | pointwise | **0** | **0** | — |
| `pauli-string-multiplication` | pointwise | **0** | **0** | — |
| `simd-word-primitives` | pointwise | **0** | **0** | — |
| `bit-table-transpose` | pointwise | **0** | **0** | — |

**Neither family fits the 50–10,000 margin heuristic, and both are correct.**
For a sampled observable the floor *is* statistical error, so each bound is a 5σ
band and a margin of ~3–4 is right — a margin in the hundreds would admit
hundreds of times the Monte Carlo error and catch nothing (MrBayes is the
precedent). For exact GF(2) algebra there is no round-off, so there is no
tolerance and no margin to report; the meaningful statement is floor 0,
allowance 0, and the four exact checks passed with 0 of up to 1,048,576 values
over bound.

**Every invariant carries its own bound**, because the Monte Carlo noise of
these observables spans a factor of ~500. A single shared absolute tolerance is
necessarily set by the noisiest quantity, which would leave the sharpest ones
ungraded in practice: sharing one bound cost the mean detector rate a factor of
200 in discrimination and forced the observable parity out of the check
altogether. Self-validation landed 3.8×–125× inside the bounds, tightest at
`mean-detector-rate` (2.67e-6 against 1e-5) and `any-event-fraction`
(6.25e-4 against 2.5e-3).

The exact checks carry the fine-grained correctness burden; the sampling checks
earn their place as the only ones exercising the bit-packed shot-batch path the
module exists to accelerate.

## The `invariants` policy is forced by upstream

Stim's `--seed` documentation says results are only "PARTIALLY deterministic"
and warns they "MAY NOT be consistent across machines", giving as its example
"using the same seed on a machine that supports AVX instructions and one that
only supports SSE instructions may produce different simulation results."
Changing the vector word width is exactly what an accelerator port does, so
grading sampled bits pointwise would reject a correct port by the codebase's own
contract.

## What the first calibration caught

The first calibration run **failed** (reward 0.667) and was worth more than a
pass would have been. Five of six checks had passed *produce*, so four of these
defects were invisible until `VERIFY` ran. Recorded here because four of them
are grading defects, not crashes, and a passing run would have shipped them.

1. **The acceleration check could not run at the graded configuration.** It
   unpacked the whole 1e6 × 12,000-bit sample — 11.2 GiB of uint8 — and was
   killed in the 4 GB container. It now consumes the sample from a **pipe** in
   10,000-shot blocks and never writes it at all. Column sums, the shot count
   and the first two moments of the per-shot event count are additive, so the
   reduction is exact, not approximate.
2. **A graded scalar was a duplicate.** `summary[2]` was "overall event
   density", which equals `rates.mean()` identically — both are total events
   over (shots × bits) — and was recorded bit-identical to `summary[0]`
   (0.0225918 in both). Both invariants checks graded 3 quantities while
   claiming 4. Replaced with the coefficient of variation of the per-shot event
   count, which is sensitive to detector correlations the mean cannot see.
3. **Seven graded bits were structurally zero.** The graded width was taken as
   `ceil(n/8)*8` from the b8 file size, which appends up to 7 byte-padding bits
   that are zero in every run and movable by no port. Small as a fraction, but
   it **pinned the `min-flip-rate` invariant at exactly 0.0**, making it inert.
   The width is now probed exactly from stim with a one-shot ASCII `01` sample
   (12000 and 481 bits), which also stays correct if the distance/rounds knobs
   change the geometry.
4. **The recorded floor could not have come from the shipped code.** It cited
   runtimes of 42–47.6 s at a configuration where `run.sh` OOMs. All evidence
   was re-measured from scratch, and re-measured again after every subsequent
   change rather than sliced from older arrays.
5. **The comparison schema did not match the policy.** The rubrics used the
   pointwise `files` block while shipping the invariants `validate.py`, which
   reads `comparison.invariants`. Note for the maintainers: **`sab.py lint`
   does not catch this** — it reads `comp.get("invariants", [])`, so a missing
   block is an empty list and lints clean with 0 warnings, while `validate.py`
   dies with `KeyError: 'invariants'`. A lint rule asserting the comparison
   block matches the declared policy would have saved a 25-minute run.

## The observable parity, and why it is graded loosely

`stim detect --obs_out` reports the **raw, undecoded** parity of the logical
observable — no decoder runs. Over 100 rounds it therefore accumulates
measurement noise and sits near one half (0.48174 over four seeds). It is *not*
the sub-threshold logical error rate a QEC paper reports, and an earlier draft
of the warrant wrongly called it "the quantity a QEC paper actually reports",
which would have read to a QEC-literate reviewer as a distance-11 code failing
completely.

It is the noisiest quantity here and, being near maximum entropy, also the least
discriminating: any fault moves it by at most 0.018, against detector rates that
move from 0.0132 to ~0.5. Under one shared bound it had to be dropped. With its
own 5e-3 it costs nothing and catches the one fault class no other invariant
sees — a port that stops tracking observables returns 0, which is 0.482 away,
~96× its band.

Its bound is also the one case where theory was not enough: Bernoulli(0.48) over
1e6 shots predicts sd 5.0e-4, but the measured seed sd is 6.61e-4, so a bound
sized from theory (4e-3) would have sat *below* its own 5σ band. Every bound
here is set from the measurement.

## Things verified rather than assumed

- **`stim.Tableau.random(num_qubits)` takes no seed** and two unseeded calls
  compare unequal. Using it would have made every run a different instance, so
  with identical initial conditions the two solves would disagree at order one
  and three checks could never have passed. The instance is instead composed
  from a fixed sequence of named Clifford gates chosen by a seeded numpy RNG,
  confirmed reproducible for a given seed and distinct across seeds.
- **Both solves confirmed byte-identical** across every graded file of the four
  exact checks, with file counts asserted non-zero so an empty-directory
  comparison cannot pass silently.
- **No check is inert.** Self-validation reports `identical: False` for all six,
  so every variant genuinely moves its graded output.
- **The pipe refactor is behaviour-preserving.** All four seeds reproduce the
  file-based values to every digit (mean rate 0.013216688, dispersion
  0.133389032, parity 0.481843000 at seed 12345).
- **pybind11 must be `2.11.1`**, per `code/stim/pyproject.toml:2`
  (`pybind11~=2.11.1`). Bookworm's `pybind11-dev` is 2.10.4, *below* the floor,
  and pybind11 3.x is a hard compile error in `tableau_simulator.pybind.cc`.
  Pinned via pip.
- **The Python module is required, not a convenience.** Four checks grade
  tableau algebra, Pauli multiplication and the bit-table primitives, and none
  has a CLI surface — stim's CLI exposes only
  gen/sample/detect/m2d/analyze_errors/diagram/repl.

## Runtime, and why wall time is mostly compilation

Declared `expected_runtime_s` totals **37 s per solve** (23 s of it the
acceleration check), measured with builds excluded. Actual solves were 1441 s
and 1199 s, because **each check rebuilds stim from source**: the four algebra
checks build `stim_python_bindings` with pybind11 and `-flto` at ~3–4 min each,
the two sampling checks build only the `stim` CLI at ~1–1.5 min. Every check
prints `SAB_BUILD_SECONDS` so the two can be separated.

## Blind spots and open questions

- **The incumbent's instruction set is unresolved.** `SIMD_WIDTH` (256/128/64)
  is a documented CMake option but every flag is x86-only and there is no NEON
  path; `CMakeLists.txt:25` matches `ARM64` case-sensitively against macOS's
  `arm64`, so this machine builds the portable `bitword_64` with no machine flag
  at all. Upstream's own `stim_perf` shows what that costs — 410 ms against a
  100 ms reference for `find_graphlike_logical_error_d11_r1000`, 810 ns against
  250 ns for `biased_random_1024_1percent`, so **1.5–4x slower than Stim at
  speed**. The curator was asked to rule on this in source PR #415 and merged
  without answering, so this package declares what it measured and every check
  writes `word_backend.txt` beside its output. A port scored against an arm64
  baseline would post an inflated speedup, and that needs settling before any
  official run.
- **Three surveyed checks were deferred** to keep the first leaf reviewable:
  `tableau-simulator-evolution`, `tableau-enumeration` and
  `stabilizer-flow-verification`. Their tests are in the survey and adding them
  later is cheap.
- **`simd-word-primitives` grades an effect, not a call.** The word primitives
  have no API surface; the check drives them at dimension 1024 and grades the
  resulting blocks plus per-row popcounts. Stated so its coverage is not
  overread.
- **`simd_bits_not_zero_100K` is unusable as a workload.** Upstream's own
  benchmark reports 310 ps against a 32 ns reference — a hundredfold speedup on
  a 100k-bit scan is not credible and suggests the compiler elides the loop. No
  check is built on it.
- **The base image is Debian 13 trixie with Python 3.13.5**, not the bookworm
  the template's Dockerfile comment claims, though the pinned digest is the
  template's own. Worth a note to the maintainers: every leaf inherits it.
- **The invariants policy cannot localise a small elementwise fault.** Its
  `validate.py` reduces each file to one statistic, so a single detector shifted
  by a few percent is invisible to the mean over 12,000. The `max` and `min`
  invariants are what recover the localisation that matters: one detector driven
  to 0.5 moves the max by 0.48 against a 5e-4 band, one silenced to zero moves
  the min by 0.002 against 2.5e-4, while the mean shifts by only 4e-5.
