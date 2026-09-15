# samfshalconv

Performance portable implementation of the scale-aware mass flux scheme shallow convection in SHiELD built using the NDSL domain-specific language middleware in Python.

This module reproduces samfshalcnv.f from the 202411 release of the SHiELD physics on December 18, 2024: https://github.com/NOAA-GFDL/SHiELD_physics/commit/7ea294bbfdd7d37708fe78d3c8ba881ae6615a32

## Notes for future cleanup:
loops over stencil calls (`for n in range(num_tracers):`) will need to be decorated with `dace.nounroll` for orchestration.

`k_mask` will soon be obsolete and we can replace it with the `THIS_K` keyword.

`SAMFShalConvState` can inherit from `ndsl.state`
