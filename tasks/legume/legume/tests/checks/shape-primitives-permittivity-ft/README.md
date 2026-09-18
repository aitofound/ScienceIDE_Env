# shape-primitives-permittivity-ft

The **analytic Fourier transform of the patterned permittivity** for each shape primitive: a circle, a
polygon and a square, evaluated on a fixed reciprocal-lattice grid, plus the assembled layer transform.

These coefficients are the input to every matrix element in both expansions.

Derived from `docs/examples/01_Shapes_layers_and_photonic_crystals.ipynb`.

## Output files

Raw little-endian float64, C order:

| file | meaning |
|---|---|
| `ft_circle_re.f64 / ft_circle_im.f64` | circle form factor, real and imaginary parts |
| `ft_poly_re.f64 / ft_poly_im.f64` | polygon form factor |
| `ft_square_re.f64 / ft_square_im.f64` | square form factor |
| `layer_eps_ft_re.f64 / layer_eps_ft_im.f64` | the layer's assembled permittivity transform |

## Inputs

`ic/nominal/params.json` and `ic/variant/params.json` carry the whole configuration. Nothing else is read.

## Knobs

`run.sh --help` lists them. `SAB_NG` sets the half-width of the reciprocal grid; the arrays hold (2*SAB_NG+1)^2 values. The defaults are the graded values.
