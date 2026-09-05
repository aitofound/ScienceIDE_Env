# normalization-contract

Status: acceptance policy and auxiliary gates finalized by the curator after
calibration, with their existing values retained. Packaging review remains pending.

## Upstream origin and scope

EDKit v0.5.0, commit `538fce882ab73e3af447f4bc6a1704d290c88aba`,
`test/timeevolve_tests.jl`: Adaptive Krylov Time Evolution / normalize_output default does not mask error.
The solver and original tests are upstream work. This contribution packages
the environment, freezes reproducible inputs and adds independent checks.

Original Julia 1.10 shared Random.seed!(11) draw order preserved across all direct selectors; the wide-spectrum selector resets to 42 as upstream. Inputs frozen in TOML; no runtime random generation. Added controls use copies of existing states and consume no draws.

## Run and output contract

`run.sh nominal` and `run.sh variant` copy read-only `SOURCE_DIR` into an
isolated scratch package with Julia 1.12.5 and the unchanged upstream
Project/Manifest at that copied source root.
They run offline, with one Julia and one BLAS thread, and write `result.toml`.
The input file specifies all cases, complex initial states, Hamiltonians,
requested times, solver settings, required API outcomes and branch conditions.
Every output column and complex phase is retained; no phase alignment or
post-normalization is applied except the explicitly tested normalize option.

`run.sh --help` documents `SAB_TIME_SCALE=1.0`. Smaller positive values shorten
the physical horizon for diagnostics only; the verifier uses the trusted
environment setting, never a candidate-supplied time scale. A short window
can legitimately fail an original restart/extension requirement. Default
grading uses the original complete windows. The discrete API case cannot
meaningfully be shortened. No alternate build is declared in this revision.

## Independent validation and finalized bounds

The bit-action oracle uses physical spin S=sigma/2, periodic bonds, big-endian
tensor coordinates and an independently constructed momentum-zero orbit basis
where applicable. Matrix-input cases use the frozen explicit matrix. The
independent calculation uses full Hermitian diagonalization or analytic
diagonal phases. No generated exact states are included in this package.

The run gate checks the same-input complex state and its L2 error, norm and
normalized energy expectation. The density integration additionally checks
trace and the original pure-state/density relation. Declared magnetization is
checked independently. API outcomes and the upstream broad reuse/restart/
extension conditions remain mandatory; full integer diagnostic traces are
not required to match across implementations.

| Case | Pointwise / L2 absolute caps | Norm/trace cap | Normalized energy cap |
| --- | --- | --- | --- |
| default-normalized-input | 1e-10 / 1e-10 | 1e-10 | 1.62e-09 |
| opt-in-normalized-input | 1e-10 / 1e-10 | 1e-12 | 1.62e-09 |
| nonunit-default | 2.5e-10 / 2.5e-10 | 1e-10 | 1.62e-09 |
| nonunit-opt-in | 1e-10 / 1e-10 | 1e-12 | 6.48e-10 |
| analytic-diagonal | 1e-10 / 1e-10 | 1e-10 | 6.40184e-10 |

All relative terms are zero in this policy. Cross-case L2 comparisons use
`1e-10`; declared magnetization uses `1e-9`. These bounds and auxiliary gates
were retained unchanged at human finalization. Source assertions are the
starting constraints, not evidence of measured roundoff. A coarser density restart retains its own
source bound; it does not relax the other cases.

The SAB pair validator reports only complex-state pair distance and the worst
fraction of that pair bound. The independent algorithm-error gate is separate
and logs `SAB_SCIENCE_JSON`. Norm and energy never substitute for state error.
Build/load duration is printed as `SAB_BUILD_SECONDS`; run time still includes
first-use Julia JIT and fixed independent-oracle overhead, so it is not a pure
kernel benchmark. Detailed local measurements, when available, are reviewer
notes; only genuine container selfcheck can produce the pipeline record.

## Explicit additions to the official selector

- `opt-in-normalized-input`: Full complex state comparison supplements upstream norm-only check for normalize_output=true.
- `nonunit-default`: Nonunit complex input exposes accidental normalization with the default option.
- `nonunit-opt-in`: Explicit normalize_output=true is checked against normalized independent output.
- `analytic-diagonal`: Two-level analytic componentwise phase control with unequal amplitudes and nonunit norm; no numerical eigensolver is needed for the oracle.

## Physical reference

Anders W. Sandvik, *Computational Studies of Quantum Spin Systems*,
AIP Conf. Proc. 1297, 135 (2010), doi:10.1063/1.3518900, Sections 4.1-4.2:
spin-model matrix construction and finite-precision Lanczos reliability.
The lecture is a model/reliability reference, not a new ground-state task or
an error-bound prescription for this real-time solver.
