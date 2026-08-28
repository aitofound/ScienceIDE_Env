# CPU build decision (implementation staging)

**Status: STAGED; no PLUTO build or solver run was performed.**

The pinned archive contains the complete C source, official RHD inputs, the
Taub EOS, root radiation, and the shared build machinery. The implementation
must use copied per-check workspaces and must never edit `code/pluto/` in place.
The intended serial path is the archive's `Tools/Python/make_problem.py` with a
pre-existing `ARCH` line and `--auto-update`; this path still needs a permitted
CPU build measurement. Any Chombo/AMR section, custom C++ tagger, and auxiliary
`irradiation.c` path must be audited before it can be called runnable.

Rows staged in this tree are only the eight RHD rows listed in
`comment/staging.md`. The solution script verifies the archive, official input
presence, and copied-workspace manifest, but this staging change does not claim
that a compiler, generated makefile, executable, or solver output exists.

Required follow-up before a runtime claim:

1. establish the pinned Debian CPU image/toolchain and serial/AMR decision;
2. build each selected official configuration from a copied deck and record the
   exact command, compiler, source hash, and auxiliary files;
3. run two trusted CPU repeats per row and retain raw `data.%04d.dbl`,
   `dbl.out`, `grid.out`, and manifests;
4. measure output shape, runtime/storage, refinement signal, nonzero scales, and
   arithmetic variation before filling any active target or numeric rubric
   bound.

No historical PLUTO timing, tolerance, target, or legacy branch result is
copied into this package.

## Row-level config audit (continuation session)

Cross-checking each staged `rubric.json` against the real vendored deck found
one class of defect: `physics.gamma` must reflect what the official deck
actually runs with, not a generic ideal-gas default. PLUTO's compiled-in
default is `g_gamma = 5./3.` (`Src/globals.h:118`), but `Test_Problems/RHD/
Shock_Tubes/init.c:63` overrides it at runtime with
`g_gamma = g_inputParam[GAMMA_EOS]`, and both `pluto_01.ini` and `pluto_05.ini`
set that parameter to `1.333333333333` (4/3). `rhd-shocktube-01` and
`rhd-shocktube-05-entropy` had inherited the generic 5/3 default instead;
both `rubric.json` files are corrected to `gamma: 1.3333333333333333` with a
new `gamma_source` field recording the evidence. The other two ideal-EOS rows
(`rhd-sedov2d-01-weno`, `rhd-riemann2d-01`) have no `GAMMA_EOS` user parameter
in their decks, so PLUTO's compiled-in 5/3 default is confirmed correct there
and was left unchanged. The four Taub-EOS rows carry a `gamma` field that
`tests/lib/taub.py`'s Taub-branch reconstruction does not currently consume
(the Taub-Mathews closure used there is gamma-free), so no equivalent defect
was possible for those rows; their `gamma: 1.3333333333333333` is retained as
informational/reserved only.

A synthetic fixture-build-and-validate smoke test (`fixture_builder.build()`
then `generic_validator.validate()`, all 8 checks × 9 accept/reject fixture
variants = 72 assertions) was run directly in Python against a scratch temp
directory outside the task tree, with no Docker and no PLUTO compilation, and
passed before and after this correction.

## Gamma and output-contract correction (continuation session, PR #303 repair)

The row-level config audit above is **superseded** for 6 of the 8 rows. It
correctly found the two Shock_Tubes rows' `GAMMA_EOS` deck parameter, but
mis-stated its precision, and it never checked whether `Jet/init.c:55`'s
`g_gamma = 5.0/3.0;` actually compiles for the TAUB rows that reference it.
Re-derived directly from `code/pluto/Src/` and `code/pluto/Test_Problems/RHD/`
(grep + full-file reads, not inference):

- **`rhd-shocktube-01` / `rhd-shocktube-05-entropy`**: the official
  `pluto_01.ini:57`/`pluto_05.ini:57` literally read
  `GAMMA_EOS   1.333333333333` -- 12 threes, not the normalized 4/3 double
  `1.3333333333333333` this file's prior entry (and the rubric it produced)
  used. `float("1.333333333333")` and `4/3` are different IEEE-754 doubles;
  since the validator uses exact equality, the rubric now stores the literal
  ini value, `1.333333333333`.
- **`rhd-sedov2d-05-taub`, `rhd-jet-01-taub`, `rhd-jet-04-taub-entropy`,
  `rhd-blast3d-03-taub`**: all four are `EOS == TAUB`
  (`definitions_NN.h:14`), under which `g_gamma` is not even a declared,
  linkable symbol -- `Src/globals.h:117-121` and `Src/pluto.h:925-929` both
  guard its declaration to `#if EOS == IDEAL`. There is no compiled-in 5/3
  fallback for a TAUB row; the "Taub rows inherit the 5/3 default" reasoning
  used elsewhere for this class of row does not apply here. Row by row:
  - Sedov2D-05 and Blast3D-03's `init.c` never reference gamma at all.
  - Blast3D-03's `init.c:60,93` does set a variable literally named `gamma`
    to `4./3.`, but it is a local, non-EOS variable used only to convert the
    initial blast energy deposit to a pressure profile
    (`init.c:95: us[PRS] = (gamma-1)*ENRG0/vol`) -- never assigned to
    `g_gamma`. This is almost certainly why a prior contract (twice: 5/3
    then 4/3) guessed wrong for this row.
  - Both Jet rows' `init.c:55` does read `g_gamma = 5.0/3.0;`, but it sits
    inside `#if EOS == IDEAL` (`init.c:54..56`), and both `definitions_01.h`
    and `definitions_04.h` set `EOS TAUB` -- that line is dead code for both
    rows, never compiled, not evidence of a real gamma.
  - All four rows' `rubric.json.physics.gamma` is now `null`, with a
    `gamma_source` field citing the exact evidence above. `tests/lib/taub.py`
    's `reconstruct()` never reads `gamma` on the `eos == "taub"` branch, so
    this costs nothing there. `tests/lib/generic_validator.py` retains an
    EOS-aware entropy-recovery predicate for any future rubric that explicitly
    emits an entropy field, but the official Shock_Tubes/05 and Jet/04 decks
    emit only `rho`, `vx1`, `vx2`, `vx3`, and `prs`; their entropy switches are
    internal and are not directly graded by these active output contracts.
    PLUTO compiles two different internal entropy formulas
    (`Src/RHD/rhd_energy_solve.c:193-201`): ideal EOS is
    `p*lor/rho^(gamma-1)` (uses gamma); TAUB EOS is the gamma-free
    Taub-Mathews form `p*lor/rho^(2/3)*(1.5*theta+sqrt(2.25*theta^2+1))`,
    `theta=p/rho`. No gamma is needed for the TAUB internal branch.

`rhd-sedov2d-01-weno` and `rhd-riemann2d-01` (ideal EOS, no `GAMMA_EOS`
override, compiled default 5/3) were already correct and are unchanged.

The fabricated PLUTO output contract flagged by that later audit is also
fixed in this session: `tests/lib/pluto_io.py`'s `read_grid`/`read_index` now
parse PLUTO's actual `#`-header `grid.out` (`Src/set_grid.c:154-195`) and the
literal `little`/`big` endian token (`Src/write_data.c:441-442`), and
`tests/lib/generic_validator.py`'s exact-file-set rule now tolerates
`pluto.ini`/`restart.out`/`pluto.0.log` (see
`tests/lib/pluto_io.TOLERATED_SIDE_FILES` for the per-file evidence) without
weakening required-file rejection. `tests/lib/fixture_builder.py` was updated
to emit fixtures in this same real shape, plus new `accept-with-side-files`,
`reject-wrong-name`, `reject-malformed-grid`, and `reject-wrong-endian`
fixtures. The same synthetic fixture-build-and-validate smoke test described
above (now 8 checks x 13 accept/reject variants) was re-run after every
change in this session, entirely in Python against fresh temporary
directories, no Docker/PLUTO/compiler -- see `comment/staging.md`'s matching
addendum for the run record.
