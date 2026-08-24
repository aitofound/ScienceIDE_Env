# Determinism triage

Read this when the incumbent will not reproduce itself, or before measuring
any floor. The question is not "is this code correct" — assume it is. The
question is: **if someone reimplements this faithfully on different hardware,
how far apart can the two answers legitimately be?**

That distance is the **floor**. Whatever shape the check's pass policy takes,
no bound below its floor is reachable — a correct port would fail it.

---

## The three verdicts

Assign one per configuration.

### ADMIT — the floor is rounding

No discrete choice on this path depends on a floating-point comparison, and the
number of operations is fixed by the inputs. Two correct builds differ only by
rounding, reassociation and FMA contraction. The floor is `O(eps * sqrt(N))`
and can be **asserted from source** without measuring.

For integer or exact codes the same verdict is stronger: the floor is zero and
the policy is `diff`.

### MEASURED — a branch is reachable

Somewhere on this path the code compares a float and takes a different route
depending on the answer. At the flip point the two routes disagree by a
truncation-scale amount, not a rounding-scale one, so the floor is no longer
assertable — but it is still *bounded*, and it can be measured.

This is a demotion, not a disqualification. The check still ships; its warrant
changes from asserted to measured and its bound comes from the measurement.

### DEFER — the computation's shape can differ

The **number of operations itself** depends on floating-point comparisons: an
iteration count, a substep count, an adaptive step size, a convergence test, a
tie-break in a sort. Two correct builds then do genuinely different amounts of
work, and an elementwise difference between their outputs measures nothing.

DEFER does not mean "ungradeable". It means **this observable is wrong for this
configuration.** The repair is a different observable — an invariant, a
statistic, a spectrum — not a looser tolerance.

---

## Building the hazard register

Grep is the start, not the method. Search the source for the patterns below,
then **open every hit and read it**, because most of them will be dead.

```
1e-6  1.e-6  1e-8  1e-10  1e-12  1e-14   # threshold literals
EPS  eps  tol  TOL  SMALL  TINY  HUGE    # named thresholds
> 0.0   < 0.0   fabs(   abs(             # sign and magnitude tests
while (  do {                            # iteration to convergence
(int)   floor(   ceil(   round(          # float to integer
atomicAdd  omp critical  reduction       # order-dependent accumulation
rand  random  srand  drand48  mt19937    # RNG
time(  clock(  getenv(  hostname         # ambient state in the output
```

Classify each surviving hit three ways.

**LIVE** — reachable, and the two sides disagree at the flip point. Demotes any
configuration whose flags reach it. Record the flag that reaches it, not just
the line.

**SELF-LIMITING** — the branch flips, but both formulas *agree at the flip
point*, so the result is continuous across it and the difference stays at
rounding scale. Slope limiters and entropy fixes are usually this. Record it,
explain why, and do **not** demote. Left unrecorded, the next reader finds the
threshold, panics, and demotes a clean check.

**NOT LIVE** — inside an `#if` that is off by default, inside a comment, in a
file no build includes. Name these explicitly, with the reason. A register that
lists only live hazards gets re-derived from scratch every time.

## Reachability is not the same as existence

The most common triage error is checking whether a branch exists in the file
and stopping there. Three ways that goes wrong, all observed:

**Compiled out.** A branch inside `#if DIMENSIONS > 1` is not a hazard for a
1-D configuration. Read the flags, not just the code.

**Gated differently in two places.** The same construct can be guarded by a
feature flag in one solver and unguarded in another. Do not generalise from one
file to its neighbour; check both.

**Reachable by the flow, not by the flags.** A threshold on a quantity that
starts at zero and grows will be *crossed*, no matter how far the final state
sits from the threshold. "The value is far from the cutoff" is only an argument
if you also checked the initial condition and the path between them. This is
the subtlest of the three and the easiest to get wrong while sounding rigorous.

---

## Measuring the floor

Build the same source two genuinely different ways and compare the outputs over
the intended window.

```
floor = max over frames, over fields, of | a - b |     # absolute, elementwise
```

Absolute and elementwise. Not RMS, not per-field-normalised: an RMS hides a
single catastrophic cell among a million good ones, which is exactly the fault
you are trying to catch.

**Confirm the two builds are actually different.** Changing a flag the target
cannot act on yields an identical binary and a measured difference of exactly
`0.0` — which looks like a triumph and is a null experiment. Check `sha256sum`
differs and that the intended instructions changed in the disassembly before
believing any floor, and be especially suspicious of a floor of exactly zero.

Vary something with real arithmetic consequences: optimisation level, FP
contraction, vector width, libm, or the summation order in a reduction.

---

## Sources of floor that are not branches

These raise the floor without any discrete choice, and belong in a warrant even
for an ADMIT verdict.

| source | scale | notes |
|---|---|---|
| **FMA contraction** | eps per op | GPU compilers default to fusing; the reference build usually pins it off. A fused multiply-add is *more* accurate, and different. |
| **Reduction order** | eps · sqrt(N) | tree vs sequential summation. Worst where the result feeds back into every subsequent step — a timestep from a global minimum is the classic case. |
| **Transcendentals** | last ulp | `exp`, `log`, `pow`, `sin` differ between libm implementations and between host and device. |
| **Atomic accumulation** | grows with contention | `atomicAdd` order is scheduling order, so the result is **not reproducible run-to-run on the same device**. Not stochastic physics — stochastic implementation. |
| **Fixed-point accumulation** | the quantum | the deliberate fix for the previous row: accumulate in integers so addition is associative. Reproducible, but quantised — and that quantum is a floor present **at step 1**, which a shorter window does not shrink. |
| **RNG** | total | a seeded PRNG is reproducible only if the algorithm is also reproduced. A `time(NULL)` seed makes the run unrepeatable even against itself. |

The last two rows carry a distinction worth stating explicitly in any warrant:

- **floor that shrinks with a shorter window** — accumulated drift, divergence
  of iteration counts, the probability of hitting a branch at all
- **floor that does not** — a quantisation or a seed, present at the first step

A short window is the cheapest way to buy headroom against the first kind and
does nothing at all against the second.

---

## Choosing the window

The window is part of the pass policy. It exists to keep the floor small.

```
floor grows roughly like sqrt(steps)
```

so run the shortest window in which a fault is still guaranteed to appear.
Practical defaults, adjust to the codebase:

- **around ten recorded frames** — enough for a fault to develop, few enough to
  stay cheap
- **an operation count in the hundreds, not the millions** — the floor scales
  with it and so does the CI bill
- **never past the configuration's own natural endpoint** — its author chose
  that value for a reason
- **never grade the initial state** — it was produced by setup code, not by the
  computation under test

A fault in a numerical kernel — a mis-signed term, an index off by one, a
boundary applied in the wrong order — exceeds a `1e-10` bound within a handful
of steps. No implementation fault sits at rounding scale for many steps and
then suddenly appears. That asymmetry is why a short window is not a weaker
test; it is a cheaper one with the same discriminating power.
