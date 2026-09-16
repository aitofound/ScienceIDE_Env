# munoz21-relative-velocities

Upstream example: `code/21cmfast/docs/tutorials/relative_velocities.ipynb`. Policy: `invariants`.

## Scope: configuration regression, not full Muñoz21 reproduction

This check runs only the official Muñoz21 tutorial's initial-condition configuration at seed 911, HII_DIM=64, DIM=172 and BOX_LEN=200 Mpc. It does not run or claim acceptance of the template's later star-formation, thermal, ionisation or brightness-temperature physics.

Within this approved module, Muñoz21's distinctive active matter choice is `USE_RELATIVE_VELOCITIES=True`, which forces CLASS. `SOURCE_MODEL` and the template's astrophysical options affect downstream modules, not a distinct initial density/v_cb algorithm. This check therefore protects official-example wiring and a second documented physical scale; it is not presented as independent Muñoz21 physics beyond the main relative-velocity check.

It grades density and scalar-v_cb summaries, mode-count-weighted dimensional mean-subtracted power in low/mid/high-k bands with explicit k ranges and mode counts, the official \(20<v_{cb,\mathrm{rms}}<40\) km/s and Maxwell-like mean/RMS bounds, and the production relative-velocity power input at fixed physical k.

## Initial conditions and finalized policy

The variant keeps the grid, BOX_LEN and bins fixed and moves SIGMA_8 upward by two float32 ulps. Its spectrum changes are about \(3\times10^{-7}\). Changing from four threads to one changes the legal random realization: v_cb low/mid/high band power moves by 29.1%, 1.78% and 0.19%, RMS by 8.30%, and density band power by at most 2.29%. The finalized tolerances are set above those observed realization floors, while the official absolute v_cb bounds and deterministic transfer-input check remain much tighter scientific anchors. Turning relative velocities off must fail; changing only downstream template options is correctly expected not to alter this task leaf.

The fault probes confirm the narrowed claim. Disabling relative velocities produces a hard missing-`v_cb` failure. Replacing the Muñoz21 template with ordinary CLASS plus relative velocities passes with only \(1.8\times10^{-9}\) normalized distance, showing that this initial-condition leaf is intentionally configuration/example coverage rather than unique downstream Muñoz21 science.
