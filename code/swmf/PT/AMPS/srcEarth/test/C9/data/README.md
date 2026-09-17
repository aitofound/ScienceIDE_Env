# C9 TS05 event driver

`ts05_driving.txt` is the default five-minute driver for the C9 PAMELA
public-data global-shell validation.  It contains:

```text
UTC Bx By Bz Vx Vy Vz Np Temp SYM-H IMFflag SWflag Tilt Pdyn W1 W2 W3 W4 W5 W6
```

Coverage:

```text
2006-12-14T00:00:00 through 2006-12-17T00:00:00
```

Cadence:

```text
5 minutes, 865 numerical records
```

SHA-256:

```text
cb3f3f1959763660beb1e26e5a49489b132708944fb91c4e1ee37cfc3a6c4317
```

`run_C9.py` uses this file by default and verifies the checksum, cadence,
monotonic timestamps, field count, gap size, and requested-time coverage before
creating any AMPS inputs.  The file is copied into the output directory so each
run retains the exact driver used.

An alternate driver can be supplied with `--driver` or `C9_TS05_DRIVER`.
`tools/prepare_official_ts05_driver.py` remains available for preparing a
provenance-tagged driver from the official yearly Tsyganenko product.
