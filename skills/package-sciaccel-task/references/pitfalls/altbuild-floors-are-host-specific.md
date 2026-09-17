# An altbuild floor measured on one host says nothing about another

**Symptom.** A leaf calibrated on an arm64 laptop records altbuild floors of
zero, or a bound with comfortable headroom, and the same check fails or
changes floor when the selfcheck is rerun on the x86 worker, or the reverse.

**What breaks.** The `-O0` altbuild measures whatever the optimizer changes in
floating-point evaluation. On baseline x86_64 gcc without `-ffast-math` that
is nothing: no FMA instructions exist without `-march`, and gcc never
reassociates. On arm64, FMA is baseline and gcc contracts at `-O2`. So the
same altbuild definition measures a real floor on one host and a zero floor
on the other. A zero floor is not evidence of stability; it is evidence that
the alternative build computed the same thing.

**Measured.**

| leaf | host | altbuild | result |
|---|---|---|---|
| meep fdtd-timestepping (#422, #498) | x86 worker | `-O0` | 29 of 29, later 33 of 33 checks bit-identical, every floor 0 |
| s4 rcwa-eigenmode-smatrix (#451) | x86 worker | clang for gcc | 7 of 7 bit-identical |
| s4 fmm-fourier-factorization (#504) | arm64 laptop | `-O0` | two checks ungradeable, see [s4-gvector-selection-fma](s4-gvector-selection-fma.md) |
| meep conductivity-attenuation (#422) | arm64 then x86 | variant | arm64-set bound (atol 1e-12, rtol 5e-12) failed by 84x on x86, see [meep-mpb-eigensolver-two-state](meep-mpb-eigensolver-two-state.md) |

**How to detect it.** Read `comment/pipeline/runtime-metadata.json`: the
`host.arch` field says where the record was made. Any floor of exactly zero
from a `-O0` altbuild on x86 should be read as "not measured", not "stable".

**What to do in the check.** Define the altbuild so that it changes
something on the grading host. Where `-O0` is a no-op, a second compiler in
the image, an IEEE mode switch, or `-O2 -mfma -ffp-contract=fast` against
the pinned `-O2` are candidates; say which in `run.sh --help`. Rerun the
selfcheck on the worker before trusting a sub-1e-11 bound calibrated
elsewhere. Record the host in the leaf README when the floors differ.

**Where measured.** PRs #422, #451, #498, #504 and issue #505 on
aitofound/ScienceAccelBench, 2026-09-05 and 2026-09-06.
