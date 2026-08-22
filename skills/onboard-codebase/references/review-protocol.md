# The review protocol

Phase 5. Every check gets a written proposal and an explicit human decision
before it ships. This is the gate the whole pipeline exists to reach.

**Why a human, when CI already checks the validator discriminates.** CI proves
the policy is self-consistent — that it accepts what it says it accepts. It
cannot prove the policy asks the *right question*, that a bound is defensible,
or that a warrant's citations say what the prose claims. Those are judgement,
and judgement is the thing that must not be automated here. A benchmark whose
tolerances were approved by the same process that wrote them measures nothing.

---

## The proposal

One page per check, or per group of checks that share an argument. Six
questions, in this order, no preamble.

```markdown
# <check name>

## What it runs
<configuration; window; cost in wall time and in CI minutes>

## What it observes
<files, fields, and the comparison rule — which shape from pass-policy-shapes.md>

## The bound
<the number, with units, and what it is relative to>

## The warrant
<asserted: the branches ruled out, each with file:line and the flag that
 keeps it out of the build>
<measured: the number, the two builds, and the evidence they genuinely differ>

## The discrimination
<which fixtures a wrong answer fails, and by how much — the margin, not just
 "it fails". A reject that fails by 1.2x the bound is a different claim from
 one that fails by 1e8x.>

## What it does not claim
<the failure modes this check is blind to>
```

The last section is not modesty. A reviewer approving a check needs to know
what it will *not* catch, because that is what determines whether the set as a
whole has coverage. A proposal without it is asking for approval of an
unbounded claim.

---

## Respect the reviewer's attention

**Do not hand over forty near-identical pages.** A reviewer given forty copies
of one document approves them all without reading, and the gate becomes
theatre — worse than no gate, because it produces a signature.

Group by shared argument: one full proposal for the group, then a diff table.

```markdown
# Group: <problem>, configurations 01–09

<one full proposal, for the representative>

## Members

| check | differs from representative in | bound | warrant |
|---|---|---|---|
| ...-01 | (representative) | 1e-10 | asserted |
| ...-03 | 2-D instead of 1-D; reaches <hazard> at file:line | 1e-8 | measured, 3.1e-12 |
| ...-07 | isothermal — no pressure field; 4 fields not 5 | 1e-10 | asserted |
```

The reviewer's eye goes to the "differs in" column, which is exactly where the
risk is. Everything identical across the group was argued once, properly.

If a member's difference cannot be stated in one line, it does not belong in
the group — give it its own proposal.

---

## The decision

The human returns one of three per check. Record it **in the check**, not in a
chat log, so it travels with the artifact.

**APPROVE** — ships. Records the reviewer and the date.

**REVISE** — with what must change. Back to Phase 4 or 5. Record the round so
the history is visible; a bound that moved three times under review is a signal
about the check, not noise.

**DEFER** — not honestly gradeable yet. It stays out of the package and the
reason is written down. A deferred check is a finding, not a failure. Deferring
one that cannot be argued for is the pipeline working.

```jsonc
"review": {
  "decision": "approved",
  "reviewer": "<name>",
  "date":     "2026-08-22",
  "round":    2,
  "notes":    "bound moved 1e-9 -> 1e-8 after the measured floor came in at 3.1e-12"
}
```

---

## What the reviewer should push back on

Give them this list. A reviewer who knows the failure modes reviews faster and
catches more.

| smell | why it matters |
|---|---|
| a bound with no `file:line` and no measurement | it was guessed — the one thing rule 1 forbids |
| the same bound across every check "for consistency" | the bound is a per-check property; uniformity is a symptom of a house default, not of an argument |
| a warrant that says "no branches" | not a warrant. Which branches, ruled out how, and by which flag? |
| a discrimination margin near 1× | the reject barely fails. The floor and the signal are close and the check is fragile |
| a rubric quoting a value the solver must derive | that value is part of the answer and has just been given away |
| a rubric asking how large an error is acceptable | wrong question. The check asks about fidelity to the incumbent, not about physics |
| no `accept-*` fixture that is correct-but-different | nothing guards against a false FAIL, the worst error a grader can make |
| a window justified only by cost | cheap is necessary, not sufficient. Does a fault still show up inside it? |
| "measured floor: 0.0" | almost always a null experiment — the two builds produced the same binary |

---

## After approval

Approval is of a specific proposal against a specific pin. It does not survive:

- a change to the bound, the window, or the observable
- a change to the upstream pin
- a change to a frozen file that affects what the check runs

Any of those returns the check to REVISE. Changes that do not touch what is
graded — prose, formatting, a comment — do not.
