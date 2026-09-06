# Known pitfalls

Failure modes that packagers have measured on earlier leaves, one per file.
Read this index at Step 2 (the official-test survey) and again before you
propose a policy at STOP 4 (calibration); open an entry only when its symptom
matches what you see. Reviewers read it at the task review stop for the same
reason. Every entry carries measured numbers and the PR or issue where they
were taken; none carries an estimate.

| entry | symptom | codebase |
|---|---|---|
| [s4-gvector-selection-fma](s4-gvector-selection-fma.md) | two builds retain a different number of Fourier basis vectors; rows keyed by G index permute | S4 |
| [altbuild-floors-are-host-specific](altbuild-floors-are-host-specific.md) | `-O0` altbuild floors are zero on x86 and nonzero on arm64, or a bound set on one host fails on the other | any |
| [altbuild-crashes-record-none](altbuild-crashes-record-none.md) | the alternative build compiles but traps in `MPI_Init`, segfaults on two ranks or NaNs at step 1 | EPOCH, Athena++, PLUTO |
| [meep-mpb-eigensolver-two-state](meep-mpb-eigensolver-two-state.md) | the variant's distance does not scale with the perturbation; the solver lands on one of two states | Meep |
| [meep-harminv-fitted-outputs](meep-harminv-fitted-outputs.md) | a fitted resonance moves a hundred times more than the history it was fitted from | Meep |
| [mitgcm-snapshot-diagnostics-never-final](mitgcm-snapshot-diagnostics-never-final.md) | a snapshot diagnostic is written under the previous iteration's suffix | MITgcm |
| [athena-turbulence-rng-per-rank](athena-turbulence-rng-per-rank.md) | a stochastic driver reseeds per rank; the run is reproducible only with a global seed and a cycle-count window | Athena++ |
| [phantom-particle-reordering](phantom-particle-reordering.md) | a validator permutes block 1 by particle id and compares later blocks by storage position | Phantom |
| [residual-below-one-ulp](residual-below-one-ulp.md) | conservation residuals below one ulp of the total fail an altbuild by 80x; float32 dumps graded below one float32 ulp | Phantom |
| [text-precision-caps-the-variant](text-precision-caps-the-variant.md) | a two-ulp perturbation leaves a six-figure text stream byte-identical; the wrong knob was perturbed | S4 |
| [gkeyll-eqdsk-cwd](gkeyll-eqdsk-cwd.md) | an input loader resolves its file against the working directory and asserts | Gkeyll |

## Adding one

File a **Known pitfall** issue on `aitofound/ScienceAccelBench`, the benchmark
repository, using the issue template there (the skill's canonical source lives
in `aitofound/sciaccelbench-pipeline`, but pitfalls are found on leaves and are
discussed where the leaves are). Give the symptom, what
breaks and why, how it was found, how the next packager detects it in a new
codebase, what to do in the check, and the measurement with the leaf or PR
it came from. Do not propose a change to vendored source; a pitfall is a
constraint on how checks are written, not a fix to the codebase. The curator
turns an accepted issue into a file here in the next skill revision.

## Entry form

Each file is short and uses the same headings: **Symptom**, **What breaks**,
**Measured** (a table where there are several numbers), **How to detect it in
a new codebase**, **What to do in the check**, **Where measured**. Slugs are
`<codebase>-<what>` for a codebase-specific pitfall and a plain phrase for a
general one.
