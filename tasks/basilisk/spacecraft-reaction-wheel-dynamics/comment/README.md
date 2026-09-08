# spacecraft-reaction-wheel-dynamics: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

Traditional hub-centric spacecraft 6-DoF dynamics coupled to balanced
reaction-wheel actuation: orbital translation, torque-free attitude rotation
and the back-substitution coupling that exchanges momentum between the hub
and its wheels. Owns `src/simulation/dynamics/spacecraft/`,
`src/simulation/dynamics/reactionWheels/`, and the hub/gravity effector
files under `src/simulation/dynamics/_GeneralModuleFiles/`. This session
re-derived the module boundary from scratch (own native investigation, own
official-test survey) but reproduces exactly the one-module cut already
approved and merged by a different contributor in PR #537; nothing about
the cut itself changed. Flight software (`fswAlgorithms/`), sensors,
MuJoCo, docking/contact, multi-spacecraft and optical navigation are
excluded, per that approval.

**This is the first successful native build ever recorded for this
codebase.** The prior investigation (Windows, Conan 2.28.1) failed before
compilation with no C++ compiler detected, so the codebase report's Gaps
and warnings section flagged the build, every test and every example as
entirely unmeasured. This session's native build (Linux x86_64, gcc
13.3.0, cmake 4.3.2, conan 2.28.1, swig 4.5.0) succeeded in 7 minutes 36
seconds, and all 30 of the module's unit tests plus both of its official
example scenarios ran and passed.

## Checks and their provenance

Of 25 official tests directly touching this module's production code (30
collected pytest items across 6 unit-test files, some sharing a
parametrized upstream function, plus 2 official example scripts), 17 are
suitable checks. Excluded, recorded in `comment/pipeline/test-survey.json`:
`test_spacecraftHubResetChecks.py` (7 items, all exception/validation-only,
no graded physical output); the reaction-wheel jitter and friction
parametrizations of `test_reactionWheelIntegratedTest` (4 items:
JitterSimple, JitterFullyCoupled, FrictionSpinDown, FrictionSpinUp) and
`test_mixed_rw_models_theta_output_indexing` (1 item), all out of the
approved module's scope (only balanced wheels are owned);
`test_rw_memory_leak` (a software resource-usage regression test, not
physics); and `scenarioBasicOrbit.py` (an official example, ran cleanly,
but its physics -- central point-mass translation -- is already covered
with a stronger analytic-reference bound by `sc-translation` and
`sc-trans-boe`, so it was deferred as redundant rather than built as a
tenth translation-only check).

**Design choice, applied uniformly to 16 of the 17 checks:** each upstream
unit-test function already performs a rigorous physical comparison
internally (an analytic two-body or torque-free-rotation solution,
momentum/energy conservation, a physical actuator limit) and returns a
scalar fail count. Rather than re-deriving that same physics independently
in `run.sh` (which would duplicate, not strengthen, an already
peer-reviewed comparison, and would have meant hand-rederiving 16 distinct
scenarios), each check imports the pinned test module directly (bypassing
pytest, which needs only the trivial `show_plots` fixture) and calls the
same function upstream's own pytest node calls, grading the returned fail
count exactly (`atol=0`). The seventeenth check,
`scenario-attitude-feedback-rw` (the module's flagship closed-loop
example), has no built-in pass/fail assertion, so it grades the logged
attitude-error/wheel-speed/rate-error trajectories directly, captured via a
`sys.settrace` return-event hook on the unmodified script's own `run()`
frame (see the check's README) rather than by editing the pinned source.
It is the acceleration check: the longest, most representative coupled
hub+wheel workload (10 simulated minutes, 3 wheels, a real feedback
controller) of anything available.

## Build

**The solve runs with networking disabled**, the same constraint found
while packaging 21cmFAST. Basilisk's build (Conan resolving Eigen, CSPICE,
CFITSIO, protobuf, cppzmq, googletest, then CMake, then SWIG for the Python
bindings) takes about 7.5 minutes and needs network access to resolve
Conan packages -- repeating it inside every check's `run.sh` was never
considered, both because of the network restriction and because 17
checks x 2 initial conditions x 7.5 minutes would dominate the suite. The
fix: `python3 conanfile.py -o mujoco=False -o opNav=False` runs once in
**both** Dockerfiles, at image build time, producing `dist3/` baked into
the image at `$BSK_DIST`. Every check's `run.sh` reports
`SAB_BUILD_SECONDS=0` unconditionally and only imports the already-built
package and calls one Python function from the pinned source; each script
still copies the relevant `_UnitTest/`/`examples/` directory to a writable
scratch location first, so a test that writes a TeX snippet or similar
artifact never touches `SOURCE_DIR`.

## Tolerances

16 of the 17 checks grade a discrete pass/fail count computed by upstream's
own internal analytic/conservation assertions; the bound is exact equality
(`atol=0, rtol=0`), so "calibration" for these is not a floor measurement in
the usual sense -- there is no continuous quantity to have a floor. Their
`variant` is explicitly identical: every one of these scenarios hardcodes
its initial conditions as Python literals inside the upstream function
(not passed in from `ic/`), and each integrates a deterministic rigid-body
ODE (RK4) with no random element, so nominal and variant runs are
byte-identical (verified directly for representative cases). The
seventeenth check, `scenario-attitude-feedback-rw`, grades continuous
trajectories (`atol=1e-6, rtol=1e-4`, a packager hypothesis); its variant
is also identical for the same hardcoded-literal reason, confirmed by
perturbing the hardcoded spacecraft mass by two ULPs of float64 (the
graded output's own precision) and observing a byte-identical result --
the MRP feedback controller is a stable, converging tracker, so a
perturbation at that scale damps out well below double-precision
visibility inside the logged window. **Calibration run (STOP 4, 2026-09-08T06:32:34Z):** the in-Docker `selfcheck`
confirmed every native pre-Docker measurement exactly: reward `1.0`, all 17
checks, every one flagged identical as declared, spread `0` throughout. The
human accepted every check's design and tolerance as calibrated, with no
changes, including the pass/fail-count design's diagnostic tradeoff (see
Blind spots), at STOP 4.

## Blind spots

- No GPU/accelerator baseline exists yet for any check.
- No `altbuild` is declared for any check: the image links one
  Conan-resolved GCC/CMake configuration, and no second compiler or
  numeric mode was available to build a legitimately different reference
  from in this environment.
- The 16 pass/fail-count checks are weaker diagnostically than a raw-array
  comparison would be: on failure, they report only that upstream's
  internal assertion failed and its message text, not which specific
  quantity diverged or by how much. This was a deliberate scope/effort
  trade-off (see "Checks and their provenance"); the message text is
  preserved in each check's `test_message.txt` output for a reviewer or
  solver to read directly.
- `scenarioBasicOrbit.py` was surveyed as an official example but not built
  as a check (redundant with `sc-translation`/`sc-trans-boe`); see the
  survey entry for `scenario-basic-orbit` in
  `comment/pipeline/test-survey.json`.
- Multi-threaded execution was not exercised; Basilisk's dynamics core is
  effectively single-threaded per simulation process.
