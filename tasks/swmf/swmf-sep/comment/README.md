# SWMF SEP implementation handoff

## Scope and source

This leaf is pinned to SWMF commit `127a73cb13951351d60e7936583f69f39bd0272e`
and owns `SP/MFLAMPA`, `PT/MITTENS`, `PT/AMPS`, and the field-line coupling
interfaces used by SC/IH/OH. Authoring used the candidate's actual
`skills/package-sciaccel-task/SKILL.md` and adjacent `SPEC.html`, both from
package-sciaccel-task v5.11.10; private CLI state is kept outside the runtime
leaf under `coordination/cli-state/swmf/`. It has three executable/build classes:

* **MFLAMPA** standalone transport, including upwind, MPI decomposition,
  spectra, steady-state and the three independent Poisson-bracket outputs;
* **MITTENS** standalone shock transport and coupled PT/MITTENS wiring;
* **AMPS** self-contained ParkerEq and Parker3D/ParkerIMF adapters, with
  `-spice-path=nospice` and Linux suitability explicitly left for measurement.

The task has no SPICE dependency. Shared SC+IH and SC+IH+OH builds should be
reused where the source/configuration permits. Any accelerator work should
first measure the baseline and retain the physical MFLAMPA field-line
quantities; the preferred future MFLAMPA strategy is independent many-fieldline
parallelism. AMPS acceleration is deferred until a measured case exists.

## Baseline coverage

The 16 historical PR517 checks are implemented as real check directories and
are not presented as current passes:

| group | checks |
| --- | --- |
| MFLAMPA transport | `mflampa-upwind`, `mflampa-mpi`, `mflampa-poisson`, `mflampa-spectra`, `mflampa-steady`, `mflampa-steady-init`, `mflampa-steady-state` |
| Poisson-bracket physics | `poisson-bracket-1d`, `poisson-bracket-2d`, `poisson-bracket-dsa` |
| MITTENS | `mittens-shock` |
| coupled field lines | `sp-bline-scih-init`, `sp-bline-scih-restart`, `sp-bline-scihoh-init`, `sp-bline-scihoh-restart` |
| MHD-data input | `sp-mhdata` |

The three Poisson-bracket leaves intentionally split the single upstream
executable's 1-D, 2-D and DSA outputs (`test_poisson.out`,
`test_poisson2d.out`, and `test_dsa_sa_mhd.out`) into separate physical
observables. `test_show` is an informational Makefile target, not a graded
physics check. Official coupled test14 is recorded as source-blocked because
its pinned PT/MITTENS interface references a legacy `libPARMISAN.a` artifact
that is not present; adding a nominal check would be misleading.

Coupled checks retain MHD startup and endpoint state even when startup
 dominates cost. The existing reduced windows are part of the proposed check
contract only when their physical endpoint is preserved; a future calibration
must document any change rather than silently shortening the window.

## AMPS status

The separate AMPS survey reports exactly 30 SEP table entries and identifies
two repository-self-contained candidates implemented here:

* `SEP--Parker_spiral--ParkerEq`, application
  `test/sep_parker_spiral__field_line`;
* `SEP--Parker3D--ParkerIMF`, application
  `test/sep_3d_cut-domain_parkerimf_parker3d`.

Their adapters configure `PT/AMPS/Config.pl`, build `make amps`, run the
configured `amps` executable with MPI, and collect finite physical files under
`PT/plots`. Their validators require the expected physical output family,
reject empty/malformed/non-finite output, and compare order-insensitively by
field-line file name where needed. They remain `measurement_needed`: the
corrected native batch ended at 2026-09-10T09:35:05Z, but its output collection
belongs to a separate collection worker and no collected reference or current
validation is asserted here. The other 28 table entries remain explicitly
unmeasured pending survey/calibration; they are not automatically unsuitable.

The source survey `inventory.json`, `ready.json`, and `REPORT.md` are copied
under candidate-owned `coordination/amps-survey/` for provenance only. The
approved module and official-test handoff is copied from the isolated private
CLI state under `coordination/cli-state/swmf/` into
`comment/pipeline/{module,test-survey}.json`; these records are not self-check
receipts and are not runtime inputs. No reference output, timing, fingerprint,
or current pass is claimed.

## Validation and handoff

Run only cheap checks in this workspace: task metadata lint, shell syntax,
Python AST parsing, and validator fixtures. Do not build SWMF natively or run
Docker in the coding workspace. Parent-owned compute/publication must perform
reference generation and record source/environment fingerprints. Before
publication, answer:

1. Which MFLAMPA field-line decomposition gives a measured speedup while
   preserving the endpoint flux and physical order-independent output?
2. Are the four coupled checks accepted with their current physical windows,
   or does calibration prove an endpoint-preserving reduction?
3. Do ParkerEq and Parker3D/ParkerIMF build and produce references on Linux
   without SPICE, and what MPI rank count is stable?
4. Which of the other 28 AMPS entries has a self-contained input and a
   repeatable Linux output family?
