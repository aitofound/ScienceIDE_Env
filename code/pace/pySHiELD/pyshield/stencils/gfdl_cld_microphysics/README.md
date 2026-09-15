# gfdl_cld_mp

Python implementation of the cloud microphysics scheme in SHiELD built using the NDSL domain-specific language middleware in Python.

This module reproduces gfdl_cld_mp_mod.F90 from the 202411 release of the SHiELD physics on December 18, 2024: https://github.com/NOAA-GFDL/SHiELD_physics/commit/7ea294bbfdd7d37708fe78d3c8ba881ae6615a32

## Notes for future cleanup:
loops over stencil calls (`for n in range(num_tracers):`) will need to be decorated with `dace.nounroll` for orchestration.

`k_mask` will soon be obsolete and we can replace it with the `THIS_K` keyword.

`GFDLCloudMicrophysicsState` can inherit from `ndsl.state`
