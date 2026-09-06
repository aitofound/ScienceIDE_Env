# srcInterface/ — SWMF Coupling Layer

This directory contains the interface between FLEKS and the SWMF framework.
The coupling follows a three-layer pattern:

```
SWMF Coupler (Fortran)
  → PC_wrapper.f90 / PT_wrapper.f90  (Fortran wrappers)
    → FleksInterface.cpp              (C++ entry points)
      → Domain / Pic / Particles      (Core FLEKS C++ code)
```

## Files

| File                  | Language | Purpose                                              |
|-----------------------|----------|------------------------------------------------------|
| `FleksInterface.cpp`  | C++      | C-linkage entry points called by Fortran wrappers. Manages the global `fleksDomains` object. |
| `PC_wrapper.f90`      | Fortran  | SWMF wrapper for the **PC** (Particle-in-Cell) component. Implements standard SWMF component interface subroutines. |
| `PT_wrapper.f90`      | Fortran  | SWMF wrapper for the **PT** (Particle Tracker) component. Similar structure to PC_wrapper. |
| `Makefile`            | Make     | Builds the interface into `libPC.a`.                 |

## Key Interface Functions (`FleksInterface.cpp`)

| Function                 | Called By           | Purpose                                      |
|--------------------------|---------------------|----------------------------------------------|
| `fleks_init_mpi_`        | `PC_set_param`      | Initialize MPI communicator                  |
| `fleks_init_`            | `PC_init_session`   | Initialize FLEKS with simulation time        |
| `fleks_read_param_`      | `PC_set_param`      | Pass PARAM.in content to FLEKS               |
| `fleks_from_gm_init_`    | `PC_put_from_gm_init` | Receive grid/normalization info from GM   |
| `fleks_finalize_init_`   | `PC_put_from_gm`    | Finalize initialization after first IC set   |
| `fleks_run_`             | `PC_run`            | Advance simulation to target time            |
| `fleks_set_state_var_`   | `PC_put_from_gm`    | Receive state variables from GM              |
| `fleks_get_state_var_`   | `PC_get_for_gm`     | Provide state variables to GM                |
| `fleks_find_points_`     | `PC_find_points`    | Find MPI ranks owning given coordinates      |
| `fleks_set_dt_`          | `PC_put_from_gm_dt` | Set time step from coupler                   |
| `fleks_save_restart_`    | `PC_save_restart`   | Save restart files                           |
| `fleks_get_grid_`        | `PC_get_grid`       | Return grid node positions                   |
| `fleks_set_grid_info_`   | `PC_put_from_gm_grid_info` | Receive AMR grid structure from GM    |
| `fleks_end_`             | `PC_finalize`       | Clean up and finalize                        |

## SWMF Wrapper Subroutines (PC_wrapper.f90)

Standard SWMF component interface:

| Subroutine                | SWMF Action    | Description                    |
|---------------------------|----------------|--------------------------------|
| `PC_set_param`            | READ/CHECK     | Read parameters, initialize MPI|
| `PC_init_session`         | INIT           | Initialize simulation session  |
| `PC_run`                  | RUN            | Advance to `TimeSimulationLimit` |
| `PC_save_restart`         | SAVE           | Save restart data              |
| `PC_finalize`             | FINALIZE       | Clean up resources             |
| `PC_put_from_gm`          | COUPLE GM→PC   | Receive MHD state from GM      |
| `PC_get_for_gm`           | COUPLE PC→GM   | Send moments back to GM        |
| `PC_put_from_gm_init`     | INIT COUPLE    | Receive initial grid params    |
| `PC_put_from_gm_grid_info`| GRID COUPLE    | Receive AMR grid updates       |
| `PC_put_from_gm_dt`       | DT COUPLE      | Receive time step              |
| `PC_find_points`          | POINT QUERY    | Find owning processors         |
| `PC_get_grid_info`        | GRID QUERY     | Return grid dimension info     |

## Common Pitfalls & Considerations

When dealing with the Fortran/C++ interface in SWMF (such as `PC_wrapper.f90`), keep an eye out for:
1. **Array indexing & Slicing:** C++ is 0-indexed and row-major; Fortran is 1-indexed and column-major. Be careful when passing multi-dimensional arrays `Data_VI` or grid sizes.
2. **Type Mismatches:** Watch out for "Possible change of value in conversion from REAL(8) to REAL(4)" warnings. Always use compatible types (e.g., `double` in C++ matching `real(8)` in Fortran).
3. **Array Rank Remapping:** Passing multidimensional arrays to routines expecting 1D/contiguous memory requires explicit contiguity or rank-1 array slicing, otherwise gfortran/ifort will throw runtime or compile-time rank mismatch errors.
4. **Name Mangling:** C functions intended for Fortran should use `extern "C"` and end with a trailing underscore (e.g., `fleks_run_`) unless `bind(C, name="...")` is used explicitly.
5. **ISO_C_BINDING:** Prefer `use iso_c_binding` in Fortran interface blocks when adding new C bindings to keep types explicit.

## Adding a New Interface Function

1. Add the C++ function in `FleksInterface.cpp` with `extern "C"` linkage
   (trailing underscore convention for Fortran interop):
   ```cpp
   extern "C" int fleks_new_function_(int *param) {
     // implementation
     return 0;
   }
   ```
2. Declare in `FleksInterface.h`
3. Add Fortran interface block and wrapper subroutine in `PC_wrapper.f90`:
   ```fortran
   use iso_c_binding, only: c_int
   interface
     integer(c_int) function fleks_new_function(param) bind(C)
       integer(c_int), intent(in) :: param
     end function
   end interface
   ```

## Validation

- Build interface-linked library from repo root: `make LIB -j8`
- If interface signatures changed, check both declaration and call sites in `include/FleksInterface.h`, `srcInterface/FleksInterface.cpp`, and `srcInterface/PC_wrapper.f90`.
