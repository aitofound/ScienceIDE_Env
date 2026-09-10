# Installing C10 in an AMPS checkout

Place this directory at:

```text
<AMPS>/srcEarth/test/C10
```

The runner expects the AMPS executable at `<AMPS>/amps` unless `--amps` is
provided. It searches for the verified December-2006 TS05 driver in either:

```text
srcEarth/test/C10/data/ts05_driving.txt
srcEarth/test/C9/data/ts05_driving.txt
```

## One-line installation checks

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && python3 -m venv .venv && source .venv/bin/activate && python3 -m pip install --upgrade pip && python3 -m pip install -r requirements.txt
```

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && python3 -m compileall -q . && python3 -m unittest discover -s tests -v
```

## One-line data download and reference generation

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && source .venv/bin/activate && python3 download_poes_sem2.py --start 2006-12-05 --end 2006-12-16 --satellites n15,n16,n17,n18,m02 --format txt --output-dir data/reference_source
```

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && source .venv/bin/activate && python3 build_poes_reference.py --input-dir data/reference_source --event-start 2006-12-14T00:00:00Z --event-end 2006-12-16T12:00:00Z --crossings-output C10_poes_boundary_crossings.csv --reference-output reference_C10_poes_meped_boundary.csv.gz --manifest-output C10_reference_manifest.json --summary-output C10_reference_summary.json
```

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && source .venv/bin/activate && python3 run_C10.py --validate-references --reference reference_C10_poes_meped_boundary.csv.gz && python3 run_C10.py --validate-driver
```

## One-line C10 runs

Fast GRIDDED `DIRECT_ACCESS` routine run:

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && source .venv/bin/activate && python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable ACCESS_T50 --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --output-root test_output/C10_direct --amps /home/vtenishe/T11/AMPS/amps --shell-lon-res-deg 15 --shell-lat-res-deg 2 --access-abs-lat-min-deg 45 --access-abs-lat-max-deg 85 --t50-grid-step-deg 0.25 --t50-min-resolved-profile-fraction 0.66 --t50-min-edge-margin-deg 1.0 --dynamic-chunk 32 -np 8 -nt 16
```

Complete GRIDDED `FULL_SCAN`, verified against the direct run:

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && source .venv/bin/activate && python3 run_C10.py --solver GRIDDED --cutoff-evaluation FULL_SCAN --comparison-observable ACCESS_T50 --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --output-root test_output/C10_full --access-consistency-root test_output/C10_direct --amps /home/vtenishe/T11/AMPS/amps --cutoff-scan-n 120 --shell-lon-res-deg 15 --shell-lat-res-deg 2 --access-abs-lat-min-deg 45 --access-abs-lat-max-deg 85 --t50-grid-step-deg 0.25 --t50-min-resolved-profile-fraction 0.66 --t50-min-edge-margin-deg 1.0 --dynamic-chunk 32 -np 8 -nt 16
```

Full two-solver diagnostic run with all Rc products:

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && source .venv/bin/activate && python3 run_C10.py --solver BOTH --cutoff-evaluation FULL_SCAN --comparison-observable ALL --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --output-root test_output/C10_full_both --amps /home/vtenishe/T11/AMPS/amps --cutoff-scan-n 120 --dynamic-chunk 32 -np 8 -nt 16
```

Reprocess an existing direct run:

```bash
cd /home/vtenishe/T11/AMPS/srcEarth/test/C10 && source .venv/bin/activate && python3 run_C10.py --solver GRIDDED --cutoff-evaluation DIRECT_ACCESS --comparison-observable ACCESS_T50 --profile ROUTINE --interval-samples 1 --reference reference_C10_poes_meped_boundary.csv.gz --skip-run --keep --output-root test_output/C10_direct --amps /home/vtenishe/T11/AMPS/amps -np 8 -nt 16
```
