# Level 2 Arbitrary-Order CC Report

Date: 2026-07-27

Energy unit: Hartree

## Primary Acceptance Result

The determinant-based Rust solver completes the full CC(1)-CC(8) series for
the challenge's primary system and matches every equilibrium CC entry printed
in Hirata and Bartlett, Chemical Physics Letters 321, 216-224 (2000),
[Table 2](https://doi.org/10.1016/S0009-2614(00)00387-0).

Calculation settings:

- H2O/6-31G, oxygen 1s frozen;
- `R(O-H)=0.967 Å`, `angle(H-O-H)=107.6°`;
- 12 active spatial orbitals, 8 active electrons;
- 245,025 alpha/beta product determinants;
- restricted Hartree-Fock canonical orbitals;
- committed FCI energy `-76.121174204141970`;
- CC residual tolerance `1e-6`, energy-change tolerance `1e-8`;
- CC(n-1) amplitudes warm-start CC(n).

`Delta` below is `E_CC(n) - E_FCI`. The paper prints only six decimals, so
published acceptance means the computed `Delta` rounds to the same
six-decimal value; it does not claim that the paper supplied more digits.

| Method | Amplitudes | Rust total energy | Rust Delta | Table 2 Delta | Match | Iterations | Final residual | Time (s) |
|---|---:|---:|---:|---:|:---:|---:|---:|---:|
| CC(1) | 64 | -75.984502842520712 | 0.136671361621254 | 0.136671 | yes | 2 | 1.080e-9 | 3.013 |
| CC(2) | 1,424 | -76.119629519205702 | 0.001544684936263 | 0.001545 | yes | 12 | 7.892e-8 | 38.667 |
| CC(3) | 12,624 | -76.120725652588177 | 0.000448551553788 | 0.000449 | yes | 11 | 2.040e-7 | 34.868 |
| CC(4) | 55,324 | -76.121162423556896 | 0.000011780585069 | 0.000012 | yes | 10 | 4.067e-7 | 31.918 |
| CC(5) | 135,068 | -76.121170991020350 | 0.000003213121616 | 0.000003 | yes | 9 | 1.756e-7 | 29.653 |
| CC(6) | 208,764 | -76.121174144494702 | 0.000000059647263 | 0.000000 | yes | 8 | 1.142e-7 | 23.993 |
| CC(7) | 240,124 | -76.121174198217162 | 0.000000005924804 | 0.000000 | yes | 6 | 9.512e-8 | 15.883 |
| CC(8) | 245,024 | -76.121174196144139 | 0.000000007997826 | 0.000000 | yes | 3 | 8.650e-7 | 8.357 |

CC(2), which is CCSD in this challenge, differs from the independently
generated PySCF CCSD value `-76.119629518903210` by
`3.025e-10`. CC(8) is the full-rank cluster operator for the eight active
electrons and differs from the committed FCI energy by `7.998e-9`.

The full series took 186.94 seconds with ten Rayon workers on an Apple M4 with
16 GiB memory. `/usr/bin/time -lp` reported a maximum resident set of
155,680,768 bytes. Machine-readable values, provenance, command, timings, and
hardware are committed in
`fixtures/h2o-631g-fc/cc_series_results.json`.

## Algorithm

Each amplitude corresponds to one determinant connected to the Hartree-Fock
reference, with a phase-normalized excitation operator satisfying:

```text
tau_mu |HF> = |mu>
```

The projected equations remain:

```text
E = <HF|H exp(T)|HF>
R_mu = <mu|(H-E)exp(T)|HF>
```

Rather than scanning every retained amplitude for every source determinant,
the production CC path evaluates `exp(T)|HF>` through an exact graded
subset-convolution recurrence. A target determinant can only be composed from
spin-preserving subsets of its reference holes and particles:

```text
rank(mu) C_mu =
    sum_{nu subset mu} rank(nu) t_nu C_(mu\nu) phase(nu, mu\nu)
```

Alpha and beta subset partitions and normalized signs are precomputed.
Targets at the same excitation rank are independent and run in parallel;
their individual sums retain a deterministic order. Unit tests compare every
coefficient with the independent Taylor-series implementation through full
rank on H2 and H4, and exhaustively compare factored signs with direct
fermionic operator application.

Amplitudes use orbital-denominator Jacobi updates and DIIS. Lower-rank
converged amplitudes initialize the next rank, following the recommendation in
Hirata 2000.

## Small-System Regressions

| System | Method | Rust energy | Reference | Absolute error | Final residual |
|---|---|---:|---:|---:|---:|
| equilibrium H2/STO-3G, 0.7414 Å | CC(2) | -1.137270174665275 | PySCF CCSD -1.137270174666663 | 1.388e-12 | 3.899e-11 |
| stretched H2/STO-3G, 1.4 Å | CC(2) | -1.0154682493 | PySCF CCSD/FCI | < 1e-9 | < 1e-9 |
| linear H4/STO-3G | CC(2) | -2.166379520346392 | PySCF CCSD -2.166379520332999 | 1.339e-11 | 6.389e-10 |
| linear H4/STO-3G | CC(4) | -2.166387448640237 | PySCF FCI -2.166387448634763 | 5.474e-12 | 6.440e-11 |
| H2O/STO-3G | CC(2) | -75.012790405014059 | PySCF CCSD -75.012790405040 | < 3e-11 | 2.499e-9 |

## Reproduction

```bash
cargo build --release

RAYON_NUM_THREADS=10 target/release/ed_workbench_rs cc-series \
  fixtures/h2o-631g-fc/FCIDUMP \
  fixtures/h2o-631g-fc/reference.json \
  --published-reference fixtures/h2o-631g-fc/hirata2000-table2.json \
  --max-rank 8 \
  --residual-tolerance 1e-6 \
  --max-iterations 100

cargo test --test cc_series
cargo test --test level2
```

The live three-minute calculation is also available as the ignored
`live_primary_cc_series_matches_hirata_table2` integration test. The normal
test suite validates the committed full-series artifact without repeating the
long calculation.
