# perturb-linear

Upstream test: `code/21cmfast/tests/test_integration_features.py::test_perturb_field_data[linear]`. Policy: `invariants`.

## Scientific purpose

This check selects the direct LINEAR path at redshift 10. In `PerturbedField.c`, the density is first formed as `dicke(z) * lowres_density`; the shared post-processing then clips the unphysical tail below delta=-1+1e-7. The check explicitly reproduces both operations rather than misclassifying the clipped tail as numerical error.

It also reconstructs from the evolved density the Fourier template proportional to `i*k_z/k^2`. The fitted amplitude, normalized residual, phase correlation, k_z=0-plane power and zero-mode amplitude test the velocity operator directly. Density--velocity-divergence coherence/sign and the three-component curl/divergence ratio provide independent vector checks.

## Realization-safe statistics and variant

Absolute random phases are reduced to mode-count-weighted low/mid/high-k density and total-vector velocity power. Hard PDFs are replaced by continuous moments and quantiles. BOX_LEN, grids, k bands, mode counts and seed stay fixed; variant moves SIGMA_8 by two float32 ulps.

A one-thread same-seed probe moved low-k density/velocity power by 15.6%/19.1% and summaries by at most 16.9%. A further five-seed ensemble found a 6.11% maximum shift in mid-k total velocity power, motivating its 8% envelope. In contrast, the clipped growth-law residual remained about 2.6e-7, initial--final coherence exceeded 0.99999, the fitted velocity amplitude moved below 0.04%, the velocity-template residual stayed below 0.0026 and phase correlation exceeded 0.999995. Amplitude and structure are graded separately so that realization-dependent residual size cannot loosen the velocity normalization.

The completed two-ulp selfcheck used only 0.0000123 of the nearest bound. Replacing the source's growth factor by its square failed at about 25400 times the bound; reversing the Fourier velocity sign failed at 163 times the bound through the phase and density--divergence signs. Evidence is currently limited to one arm64 host; no x86/A100 or alternative FFT/compiler floor is claimed.
