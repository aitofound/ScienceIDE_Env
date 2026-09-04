# detection-event-sampling

**Policy:** `invariants`

## What this check runs

A rotated surface-code memory-Z experiment at distance 5 over 20 rounds.
200,000 shots are sampled to a measurement record with `stim sample`, then
converted to detection events and observable flips with
`stim m2d --append_observables`. 488 graded bits per shot.

Graded: `detflip_rates.npy` (per-bit flip rates) and `summary.npy` (four rates).

## The path under test is exact; only its input is sampled

`m2d` does **no sampling** — it is a pure function of circuit and measurement
record. The policy is `invariants` only because the record it consumes comes
from a seeded sample whose stream a port will not reproduce, per stim's
cross-architecture disclaimer. The reduction itself is deterministic, which is
what distinguishes this check from `frame-simulator-shot-batch`.

Distance and rounds are kept small deliberately: this check owns the `m2d`
reduction, while `frame-simulator-shot-batch` owns the workload and carries the
acceleration label.

## Bound

Four seeds at the graded configuration give a per-element sd of `1.203e-3` on
the flip rates; 5σ is `6.0e-3`, which is the bound. Margin **2.3** against the
largest observed pairwise spread — small by construction, for the same reason as
the sibling sampling check.

**All four summaries are normalised to rates in [0, 1] on purpose.** An earlier
version graded mean-events-per-shot (about 11) alongside rates of about 0.02;
one absolute bound could only admit that spread at roughly 24% of a detector
rate. Normalising dropped the summary spread from 1.36e-2 to 5.2e-4.

## What it catches

The reduction decides which measurement parities form each detector, so a port
that mis-indexed the record, dropped the observable append, or shifted a
measurement round moves individual rates from ~0.02 to 0 or 0.5 — two orders of
magnitude beyond the band — and the fraction of shots carrying any event, at
0.99 here, collapses or saturates.

Grading the per-bit rates alongside the summaries is what localises a fault: a
single mis-indexed detector moves one element of the 488 while leaving every
summary inside its band.
