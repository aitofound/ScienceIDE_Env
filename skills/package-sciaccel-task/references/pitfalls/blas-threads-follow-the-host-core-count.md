# A threaded BLAS sizes its pool from the host, not the container quota

**Symptom.** One check in an otherwise uniform suite takes two orders of
magnitude longer than its neighbours for no reason visible in the probe; the
same probe run natively outside the container finishes in a second. The check
is doing the same work with the host's core count of BLAS threads inside a
one-core container.

**What breaks.** `docker run --cpus 1` sets a CFS quota; it does not change
the container's affinity mask or what `sysconf(_SC_NPROCESSORS_ONLN)` reports.
The OpenBLAS inside the numpy and scipy wheels can size its thread pool from
that count (it depends on the wheel, the OpenBLAS version and any thread
environment already set), so on an 88-core host every BLAS call in a `--cpus 1`
container started 88 threads that spun against one core's quota. Code that
makes many small BLAS calls in a loop pays it repeatedly: here PyAMG's
restarted Arnoldi in `approximate_spectral_radius` (`pyamg/util/linalg.py:255`,
`maxiter=15`, `restart=5`), called once per matrix per smoother setup.

The quieter consequence: a threaded reduction splits the vector across the
pool, so the summation order of `np.linalg.norm` or `np.dot` depends on how
many threads the host gave it, and a graded residual norm carries the host's
core count in its last bits.

**Measured** (`tasks/pyamg/relaxation-smoothing`, check
`relaxation-linear-operator`, x86_64 worker with 88 cores, `docker run --cpus 1
--memory 2g`, numpy 2.2.6 / scipy 1.15.3 wheels, identical probe and inputs):

| run | thread pool | build s | run s (build excluded) |
|---|---|---|---|
| selfcheck run2 | OpenBLAS default (88) | 155 | 290.1 |
| same image, `OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 NUMEXPR_NUM_THREADS=1` | 1 | 81 | 1.35 |

215x on that check's run time; the suite's declared run time fell from 372 s
to 78 s per solve. The graded observable was unchanged to the last bit: the
pinned x86 run and an unpinned native arm64 run both give
`max|observable| = 1.3975599014401034` over 307200 values. These numbers are
for that worker and image, not a constant of OpenBLAS.

**How to detect it.** Compare each check's run time (build
excluded) with its neighbours in the first selfcheck record; a single outlier
the probe's arithmetic does not explain is the signature. Confirm by running
the probe natively, and once in the container with the thread variables set
to 1. `python -c "import numpy; numpy.show_config()"` names the BLAS,
`threadpoolctl.threadpool_info()` shows the live pools, and `nproc` inside a
`--cpus 1` container still prints the host's core count, which is the whole
mechanism in one line. Any library that autosizes a pool (OpenBLAS, MKL,
OpenMP, TBB) behaves the same way under a quota.

**What to do in the check.** Export the thread-count variables from `run.sh`,
before the interpreter that loads the BLAS starts, matched to the resources
the task declares:
`export OMP_NUM_THREADS=1 OPENBLAS_NUM_THREADS=1 MKL_NUM_THREADS=1 NUMEXPR_NUM_THREADS=1`
for `cpus = 1`, and say so in the leaf README. Pick the variables for the
backend `numpy.show_config()` names; these four cover the Debian
numpy/scipy OpenBLAS image. A fixed constant does not make graded behaviour
depend on the host; it takes the host's core count out of the graded
reductions. Do not raise `suite_budget_s` or shorten the window to absorb the
cost, and do not call a probe too big before the pool is pinned: the
measurement that counts is the one taken with the declared resources in force.

**Where measured.** aitofound/ScienceAccelBench issue #514, PR #437
(`tasks/pyamg/relaxation-smoothing`), 2026-09-06.
