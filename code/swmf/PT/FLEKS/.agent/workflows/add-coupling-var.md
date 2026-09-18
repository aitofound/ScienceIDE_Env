---
description: How to add a new GM↔PC exchange variable for SWMF coupling
---

# Add a New Coupling Variable

This workflow adds a new variable to the data exchanged between
GM (BATS-R-US) and PC (FLEKS) during SWMF coupling.

## Overview

Data flows in two directions:
- **GM → PC**: MHD state (B, E, rho, u, p) via `PC_put_from_gm`
- **PC → GM**: PIC moments (rho, u, p, J) via `PC_get_for_gm`

## Steps for GM → PC (adding a new variable from MHD to PIC)

1. **FLEKS side — `FluidInterface.h`:**
   Add an index constant and storage for the new variable:
   ```cpp
   // In the existing variable index section
   static constexpr int iNewVar_ = <next_index>;
   ```

2. **FLEKS side — `FluidInterface.cpp`:**
   In `set_state_var()`, add code to receive and store the variable
   from the `Data_VI` array passed by SWMF.

3. **FLEKS side — `FleksInterface.cpp`:**
   If the variable requires a new interface function, add it with
   `extern "C"` linkage.

4. **Fortran side — `srcInterface/PC_wrapper.f90`:**
   Modify `PC_put_from_gm` to include the new variable in the
   `Data_VI` array that is passed to FLEKS:
   ```fortran
   Data_VI(iNewVar, iPoint) = State_V(NewVar_)
   ```

5. **BATS-R-US side — `GM/BATSRUS/srcInterface/ModGridDescriptor.f90`:**
   Ensure the variable is available in `State_V` for the coupling.

## Steps for PC → GM (adding a new moment from PIC to MHD)

1. **FLEKS side — `Pic.h` / `Pic.cpp`:**
   Compute the moment during the moment deposition step.

2. **FLEKS side — `FleksInterface.cpp`:**
   In `fleks_get_state_var_()`, include the new moment in the
   output `Data_VI` array.

3. **Fortran side — `srcInterface/PC_wrapper.f90`:**
   Modify `PC_get_for_gm` to extract the new moment.

## Testing

- Run `make test16_3d` from SWMF root to ensure coupling still works.
- Check `run_test/RESULTS/3d/PC/` output for the new variable.
- If reference results change, bless them: `make test16_3d_check BLESS=YES`

## Common Pitfalls

- **Array indexing:** Fortran is 1-indexed, C++ is 0-indexed. Double-check
  `Data_VI(iVar, iPoint)` offsets.
- **Units:** FLEKS uses CGS internally; BATS-R-US may use SI or normalized
  units. Apply correct conversion factors in `FluidInterface`.
- **nVar counts:** Update `nVar` in both the Fortran wrapper and C++
  interface to reflect the new variable count.
