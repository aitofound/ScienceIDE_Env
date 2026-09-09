# CMTSOLUTION Time-Domain SAC Synthesis

`./synthesize_cmtsolution_sac.py` builds final CMTSOLUTION synthetics by linearly combining DSM moment-component SAC files.

Run it from a DSM case directory that contains moment-component folders such as `Mzz`, `Mrr`, `Mtt`, `Mzr`, `Mzt`, and `Mrt`.

Default usage from a case directory:

```bash
/path/to/DSM/auxiliary/CMTSOLUTION_TimeSac/synthesize_cmtsolution_sac.py --clean
```

Important options:

```bash
--cmt PATH                 CMTSOLUTION file
--sem-par-file PATH        SPECFEM3D Par_file containing SINGLE_FORCE_ENZ
--input-subdir NAME        component SAC subdirectory, default disp_solid_time_sac
--output-dir PATH          output directory, default OUTPUT_FILES/CMTSOLUTION_time_sac
--basis-moment-dyn-cm VAL  basis moment used by DSM component runs, default 1.e7
--clean                    remove existing output directory first
```

The script supports `SINGLE_FORCE_ENZ=-1` explosion and `SINGLE_FORCE_ENZ=0` full moment tensor modes.
