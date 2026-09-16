# K-mimic tolerance discriminator: approval plan

This is the next diagnostic only. It uses the untouched pinned oracle source;
it does not edit `code/eftcamb`, task checks, rubrics, or CLI records. The
earlier scratch background-repair probe is historical and is not mounted.

## Static result before the run

`IntTolBoost` is declared in `fortran/model.f90:77` and used in the ODE
tolerance expressions in `cmbmain.f90`, but the pinned executable has zero
`Ini%Read(...IntTolBoost...)` calls. `camb.f90` reads only
`accuracy_boost` and `l_accuracy_boost`. Therefore this task cannot perform a
clean ini-controlled `IntTolBoost` experiment. The fallback changes only the
copied `accuracy_boost` value from 1 to 2, but the source also uses that value
for time-step density, source and k sampling, hierarchy cutoffs, neutrino
approximations and lensing-related choices. Any reduction in spread will be
reported as evidence consistent with tighter numerical settings, not as an
isolated ODE-tolerance attribution.

## Exact approved run

The script is
`jobs/eftcamb-kmimic-tolerance-probe.py` (SHA-256
`46bd998e673e91ea504802f28d5388e4499e9db22f521b1a0796891bc1c9cdd7`). Its
static selftest has passed without a solver run. The exact command is:

```bash
python3 -u -B /Users/apple/ScienceAccelBench/jobs/eftcamb-kmimic-tolerance-probe.py --run --out /Users/apple/ScienceAccelBench/jobs/eftcamb-kmimic-tolerance-probe-01
```

The command runs one clean build of the pinned source and eight executions:
K-mimic 1 and 2, nominal and variant, at `accuracy_boost=1` and `2`. In every
copy, `l_max_scalar=3500`, `transfer_kmax=2`, `OMP_NUM_THREADS=8`, all physical
deck parameters and stability flags remain unchanged. Within each level,
nominal is compared with variant. Default-versus-tight nominal outputs are
also paired where their printed coordinates agree, to expose changes caused by
the broader `accuracy_boost` setting.

The runner expands to this exact offline container invocation:

```bash
docker run --rm --pull=never --network=none --cpus=8 --memory=4g --memory-swap=4g --pids-limit=256 --mount type=bind,source=/Users/apple/ScienceAccelBench/jobs/eftcamb-kmimic-tolerance-probe.py,target=/probe.py,readonly --mount type=bind,source=/Users/apple/ScienceAccelBench/tasks/eftcamb/eftcamb-linear-perturbations/tests/checks/kmimic,target=/probe-check,readonly --mount type=bind,source=/Users/apple/ScienceAccelBench/jobs/eftcamb-kmimic-tolerance-probe-01,target=/probe-output --entrypoint /usr/bin/timeout sha256:b9ba018da26774a0b0b396deb9db71bbb1008a143ef20700b816ea2221832f2a --signal=TERM --kill-after=15s 900s python3 -u -B /probe.py --inside
```

Docker payload and limits are fixed:

| Item | Value |
| --- | --- |
| Image | `sha256:b9ba018da26774a0b0b396deb9db71bbb1008a143ef20700b816ea2221832f2a` (already local) |
| Source | original `/workspace/code`, copied to one disposable scratch tree |
| Inputs | read-only `kmimic` check manifest; K-mimic 1/2 only |
| CPUs / memory | 8 / 4 GiB |
| Swap / network / GPU | 0 / none / none |
| Build / executions | 1 / 8 |
| Timeout | 900 s plus 15 s kill grace |
| Expected wall time | 3–6 minutes (estimate) |
| Retained output | under 80 MB (estimate); temporary build under 1 GB |
| Paid compute | $0; local CPU, battery and disk only |

No image pull, image build, push, merge, comment or external message occurs.
The output directory must not already exist. The script records resource limits,
source hashes, input manifest, build status, all eight exit codes and raw
outputs. A separate report command reads those files without running Docker.

## Conditional bound and window plan (not finalized)

The current contract is one combined check with `rtol=1e-4` and these unchanged
per-file absolute terms: `scalCls=100`, `lensedCls`, `lensedtotCls`,
`lenspotentialCls`, `totCls`, and `scalarCovCls` `=0.1`, `tensCls=0.01`,
`matterpower=1`, `transfer_out=1000`.

From the retained original-source arm64 run, the largest K-mimic required
relative term after the existing `atol` is `5.71173e-4` (matterpower, K-mimic
2). Other maxima are `2.76334e-4` (lens-potential/total/covariance, K-mimic
1), `1.999e-4` (lens-potential/total, K-mimic 2), `1.659e-4` (lensed files,
K-mimic 2), and zero for scalar and transfer files after their absolute terms.
These are measured requirements, not proposed final bounds. A full-window
pointwise candidate would therefore be a single K-mimic `rtol` just above
`5.71173e-4` (for example `7e-4`, subject to the discriminator and human
review), while K-mouflage stays at `1e-4`. That is a measured widening, not a
silent relaxation; the old/new values will be shown file by file.

The explicit old-versus-candidate comparison is:

| K-mimic output file | Existing `atol` | Existing `rtol` | Full-window candidate (provisional) | Window candidate | Largest retained arm64 required `rtol` after `atol` |
| --- | ---: | ---: | ---: | --- | ---: |
| `scalCls.dat` | 100 | 1e-4 | 7e-4* | 1e-4; `l_max_scalar=2000` | 0 |
| `lensedCls.dat` | 0.1 | 1e-4 | 7e-4* | 1e-4; `l_max_scalar=2000` | 1.65937e-4 (K-mimic 2) |
| `lensedtotCls.dat` | 0.1 | 1e-4 | 7e-4* | 1e-4; `l_max_scalar=2000` | 1.65477e-4 (K-mimic 2) |
| `lenspotentialCls.dat` | 0.1 | 1e-4 | 7e-4* | 1e-4; `l_max_scalar=2000` | 2.76334e-4 (K-mimic 1) |
| `totCls.dat` | 0.1 | 1e-4 | 7e-4* | 1e-4; `l_max_scalar=2000` | 2.76334e-4 (K-mimic 1) |
| `tensCls.dat` | 0.01 | 1e-4 | 7e-4* | 1e-4; `l_max_scalar=2000` | 0 |
| `scalarCovCls.dat` | 0.1 | 1e-4 | 7e-4* | 1e-4; `l_max_scalar=2000` | 2.76334e-4 (K-mimic 1) |
| `matterpower.dat` | 1 | 1e-4 | 7e-4* | 1e-4; `transfer_kmax=0.2` | 5.71173e-4 (K-mimic 2) |
| `transfer_out.dat` | 1000 | 1e-4 | 7e-4* | 1e-4; `transfer_kmax=0.2` | 0 |

The full-window column is one common `rtol` candidate for the current rubric
schema (the asterisk marks that it would also cover files whose measured
requirement is zero); it is an example only. The discriminator run and the
human’s later STOP-4 decision are required before any rubric is changed.
The window column shows the independent cuts: lowering `l_max_scalar` affects
the CMB/lensing tables, while lowering `transfer_kmax` affects matter-power and
transfer output. Neither cut is being treated as a substitute for the other.

If the fallback experiment shows the spread is not reduced in a way that can
be separated from the broader accuracy changes, the no-widening candidate is a
windowed K-mimic check with `SAB_LMAX=2000` and `SAB_KMAX=0.2`, retaining
`rtol=1e-4` and all existing atols. In the retained baseline data these cuts
remove the high-ell/high-k points that require a larger relative term. The
defaults would be recorded explicitly as changed from upstream; setting
`SAB_LMAX=3500 SAB_KMAX=2` restores the upstream window. Because these knobs
are independent, the K-mimic rubric must state the ell cut separately from the
k cut and must not claim that lowering `l_max_scalar` controls matterpower or
transfer output. A fresh run with those exact defaults is required before
adopting this candidate.

The discriminator result chooses between these two scientific descriptions:

| Result | Warrant language | Next bound/window action |
| --- | --- | --- |
| Spread falls with `accuracy_boost=2`, with modest nominal shifts | numerical settings are a contributor; attribution is limited because the fallback also changes sampling/hierarchy/lensing | propose measured full-window `rtol` with explicit margin, or independently verify the cut window |
| Spread persists, or nominal shifts are large and spread change is inseparable | K-mimic model is genuinely sensitive to roundoff over the tested range; no clean tolerance attribution | propose the physically retained `SAB_LMAX`/`SAB_KMAX` windows at old `rtol`, or a measured full-window bound if it still rejects faults |

Only after this result will the old-versus-new per-file table be finalized and
the provisional bounds/window recorded in the already-prepared separate
`kmouflage` and `kmimic` checks. The subsequent split-check selfcheck gets its
own complete plan and fresh `APPROVE RUN`; this approval does not authorize it.

## Ask

Approve exactly the eight-execution original-source fallback experiment above
with the phrase `APPROVE RUN`. No source correction, tolerance change, window
change, split, selfcheck record, push or external communication is included.
