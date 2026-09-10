# C10 implementation validation

## Completed offline checks

The delivered source was checked with:

```bash
python3 -m compileall -q .
python3 -m unittest discover -s tests -v
python3 download_poes_sem2.py --help
python3 build_poes_reference.py --help
python3 run_C10.py --help
```

The unit-test suite verifies:

1. relativistic conversion of nominal channel thresholds to proton rigidity;
2. parsing of the documented historical 16-second ASCII columns;
3. rejection of negative fill values and the historical unphysical P8/P9 value;
4. separation of inbound and outbound pass legs;
5. interpolation of the 50%-of-polar-plateau crossing;
6. aggregation into explicit window/channel/hemisphere/MLT cells;
7. parsing and validation of exact-rigidity access-state products;
8. reconstruction of the common `ACCESS_T50` boundary;
9. selection of `FULL_SCAN` versus GRIDDED-only `DIRECT_ACCESS` commands;
10. raw-state consistency comparison between the two products; and
11. creation of `C10_comparison.png`, `C10_scatter.png`, and
    `C10_mlt_comparison.png`.

All delivered offline tests pass.

## Independent validation with the NOAA archive

The bundled compressed reference has now been independently regenerated with
the delivered builder from 14 nonempty official NOAA/NCEI Level-2 16-second
daily files for MetOp-02 and NOAA-15 through NOAA-18 over the event interval.
The rebuild read 64,599 observations, extracted 1,869 pass-leg crossings, and
produced the same 3,904 reference-cell keys. All cell fields matched except two
AACGM boundary values that differed by `0.00001 deg`, consistent with numerical
rounding or an AACGM library-version change. The numerical reference is
therefore confirmed as archive-derived rather than synthetic.

The compact package still does not contain the historical download and
reference manifests identified by the compressed file's provenance digest.
That is a packaging/provenance limitation, not a discrepancy in the numerical
boundary table. A publication archive must regenerate and retain the manifests
and pass-level crossing table with the exact source hashes and software version.

For every new publication freeze, repeat these checks:

```bash
python3 build_poes_reference.py --input-dir data/reference_source --validate-only
python3 build_poes_reference.py --input-dir data/reference_source
python3 run_C10.py --validate-references --reference reference_C10_poes_meped_boundary.csv.gz
```

Then inspect:

- the pass-level crossing count by satellite/channel/hemisphere;
- the number and location of missing MLT cells;
- P8/P9 alternating-readout flags;
- several individual passes plotted directly from the source count rates;
- agreement of extracted storm-time morphology with Dmitriev et al. (2010);
- sensitivity to plateau latitude and spacecraft subsets; and
- GRIDLESS/GRIDDED mesh and scan convergence.

A scientific regression baseline should be frozen only after those checks and
should include the two generated manifests and all source-file SHA-256 values.

## FULL_SCAN/DIRECT_ACCESS runtime validation

The offline tests verify command construction and postprocessing, but a built
AMPS executable must still be used to validate the complete data path. Run the
fast product first and then the complete product with a consistency root:

```bash
python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable ACCESS_T50 --profile SMOKE --interval-samples 1 --output-root test_output/C10_direct_smoke --amps /absolute/path/to/amps -np 4 -nt 8
```

```bash
python3 run_C10.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable ACCESS_T50 --profile SMOKE --interval-samples 1 --output-root test_output/C10_full_smoke --access-consistency-root test_output/C10_direct_smoke --amps /absolute/path/to/amps -np 4 -nt 8
```

The full branch is expected to report an exact-state agreement fraction of at
least 0.999 and an unresolved fraction no larger than 0.01.  Both branches must
produce the same `ACCESS_T50` observable within postprocessing precision.  The
complete scan remains necessary for Rc-boundary sensitivity and for diagnosing
penumbra topology; it is not required for every routine development run.
