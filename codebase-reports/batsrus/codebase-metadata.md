<!-- SCIACCEL_CODEBASE_METADATA_REPORT:BEGIN -->
## Codebase metadata (informational, non-blocking)
Generated from the canonical JSON. Unknown values are visible; this report never gates source-PR merge or downstream steps.

| field | value | ownership |
|---|---|---|
| codebase | `batsrus` | CLI |
| source payload | `code/batsrus/` | CLI |
| upstream pin | `9dfe746d48aa650b5209c3039f1a8676bc624899` | human/state |
| license | `Apache-2.0` | human/state |
| source fingerprint | `0d87a9076dc5c0b6e3901554d6a2edaf0532049548f8de548aac51061d0a459a` | CLI |
| size | 2029 files / 79184407 bytes / 1179595 text lines | CLI |

### Modules, differences, and official tests

| module | approval | purpose / difference | owned files | owned text lines | collected tests | shared components |
|---|---|---|---:|---:|---:|---|
| `batsrus-ideal-mhd-solver` | approved | one module owns the single-fluid ideal MHD and hydrodynamic core that every other configuration builds on: the Riemann solvers, the reconstruction and limiters, the three divergen… | 72 | 32024 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `build-and-test-harness`, `postproc-tools` |
| `batsrus-nonideal-closures` | approved | the dissipative and closure physics beyond ideal MHD shares one numerical backbone, the implicit and semi-implicit machinery that removes its stiff time-step limit; the whistler-w… | 27 | 18899 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `build-and-test-harness`, `postproc-tools` |
| `batsrus-multifluid-fivemoment` | approved | all configurations that carry more than one momentum equation (ion plus neutral fluids, several ion fluids with a common or separate electron pressure, the five- and six-moment io… | 30 | 5940 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `build-and-test-harness`, `postproc-tools` |
| `batsrus-solar-corona-awsom` | approved | the corona configuration is a self-contained physical model (magnetogram-driven potential field, Alfven-wave turbulence heating, electron heat conduction, radiative losses, chromo… | 98 | 255975 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `util-libraries`, `build-and-test-harness`, `postproc-tools` |
| `batsrus-outer-heliosphere` | approved | the heliospheric termination shock, heliopause and heliotail model is one application with its own user module, its own family of equation sets (one or two ion fluids, pickup ions… | 56 | 56969 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `util-libraries`, `build-and-test-harness`, `postproc-tools` |
| `batsrus-geospace-magnetosphere` | approved | the Earth magnetosphere is the code's original and most used configuration; its official tests are the ones that exercise planet dipoles, IMF-driven upstream boundaries, ionospher… | 48 | 32625 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `util-libraries`, `build-and-test-harness`, `postproc-tools` |
| `batsrus-planetary-ionospheres` | approved | each planet has its own user module and equation set, but they are one family: unmagnetised or weakly magnetised bodies with a chemically active ionosphere as the inner boundary, … | 84 | 35823 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `util-libraries`, `build-and-test-harness`, `postproc-tools` |
| `batsrus-cometary-plasma` | approved | comets are the mass-loading limit of the code: the plasma is created from the neutral gas rather than flowing in, and the Rosetta cases add an irregular body; the four open tests … | 23 | 12706 | unknown | `batl-grid`, `mhd-core-driver`, `swmf-share-library`, `build-and-test-harness`, `postproc-tools` |

### Shared code

| component | purpose | used by | files | text lines |
|---|---|---|---:|---:|
| `batl-grid` | block tree, geometry, ghost-cell message passing, AMR refinement and coarsening, particles and interpolation for every configuration | ["batsrus-ideal-mhd-solver", "batsrus-nonideal-closures", "batsrus-multifluid-fivemoment", "batsrus-solar-corona-awsom", "batsrus-outer-heliosphere", "batsrus-geospace-magnetosphe… | 29 | 31102 |
| `mhd-core-driver` | main program, ModMain, ModSetParameters (PARAM.in commands), ModPhysics units, ModAdvance and ModAdvanceExplicit stage loop, ModFaceFlux and ModFaceValue numerics, ModUpdateState,… | ["batsrus-ideal-mhd-solver", "batsrus-nonideal-closures", "batsrus-multifluid-fivemoment", "batsrus-solar-corona-awsom", "batsrus-outer-heliosphere", "batsrus-geospace-magnetosphe… | 59 | 53079 |
| `swmf-share-library` | MPI wrappers, ModUtilities, ModReadParam, ModPlotFile, ModPlanetConst and coordinate transforms, ModIoUnit, ModNumConst, the build templates in share/build, and the Perl tools Con… | ["batsrus-ideal-mhd-solver", "batsrus-nonideal-closures", "batsrus-multifluid-fivemoment", "batsrus-solar-corona-awsom", "batsrus-outer-heliosphere", "batsrus-geospace-magnetosphe… | 245 | 85693 |
| `util-libraries` | timing tree used in every run, the NOMPI stub for serial builds, magnetogram and index readers and empirical models used by the corona, heliosphere and geospace configurations | ["batsrus-solar-corona-awsom", "batsrus-outer-heliosphere", "batsrus-geospace-magnetosphere", "batsrus-planetary-ionospheres"] | 803 | 277601 |
| `build-and-test-harness` | compile-time selection of equation set, user module, block size, ghost layers and optimisation; Makefile.test with 61 test targets in the compile, rundir, run, check pattern; Test… | ["batsrus-ideal-mhd-solver", "batsrus-nonideal-closures", "batsrus-multifluid-fivemoment", "batsrus-solar-corona-awsom", "batsrus-outer-heliosphere", "batsrus-geospace-magnetosphe… | 26 | 17728 |
| `postproc-tools` | PostIDL merges per-processor .idl pieces into one .out file after every run (make PIDL); interpolate_output, ConvertRestart and the SPECTRUM synthetic-spectra code | ["batsrus-ideal-mhd-solver", "batsrus-nonideal-closures", "batsrus-multifluid-fivemoment", "batsrus-solar-corona-awsom", "batsrus-outer-heliosphere", "batsrus-geospace-magnetosphe… | 19 | 7018 |
| `docs-licences-visualisation` | upstream manuals and release notes, licence and copyright texts, and the IDL, Python, Julia and MATLAB readers and plotting tools; no official test runs any of it | [] | 179 | 152544 |
| `crash-radhydro-unused` | equation-of-state and opacity tables, laser package and the FISHPAK Poisson solver used only by the CRASH configurations whose user modules live in srcUserExtra | [] | 165 | 80231 |
| `unexercised-applications` | application code shipped upstream that no test in the open suite builds or runs (flux emergence, Europa, Ganymede, GEM reconnection, cylindrical outflow, rotating frame, and the r… | [] | 96 | 28564 |

### Source accounting

| bucket | files | bytes | text lines |
|---|---:|---:|---:|
| shared | 1571 | 38049719 | 721873 |
| owned | 438 | 40902676 | 450961 |
| overlapping_owned | 6 | 38789 | 1557 |
| unclassified | 14 | 193223 | 5204 |

### Total official-test counts (units are not interchangeable)

| count | value | unit |
|---|---:|---|
| `test_files` | 61 | files |
| `test_definitions` | 61 | source-level test definitions |
| `collected_items` | unknown | framework-collected items |
| `inner_cases` | unknown | inner cases |

### Gaps and warnings
- 20 of 59 open leaf tests (plus restart halves) measured natively so far; the rest are listed as not yet measured and the report is refreshed when they finish
- test_bx0 (isosurface plot) and test_partsteady differ from the stored upstream references on this platform beyond upstream's own tolerance (1e-3 relative and 1e-11 absolute respectively); recorded, not resolved
- the module approval was given in chat as the single word 'approve' after the table; recorded by approve-modules --human-ref
- runtimes were measured on a 20-core Apple Silicon machine with two runners in parallel, so compile times include contention
- Should the *_gpu test variants (identical CPU code with -noacc and Config.pl -opt) count as separate checks from their CPU siblings?
- Is a THIN module (four tests: nonideal-closures, cometary-plasma) acceptable or should they be merged into neighbours?
- CLI: 14 regular file(s) are unclassified; this is visible but non-blocking

Artifacts: `codebase-metadata.json` (canonical) · `codebase-metadata.html` (self-contained detail)
<!-- SCIACCEL_CODEBASE_METADATA_REPORT:END -->
