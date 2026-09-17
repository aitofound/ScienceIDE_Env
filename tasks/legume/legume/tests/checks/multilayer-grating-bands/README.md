# multilayer-grating-bands

Guided-mode expansion of a **three-layer one-dimensional grating**: dielectric rods with thin added
layers above and below, in a ficticious supercell one period wide. Only the guided bands whose magnetic
field lies in the plane are kept.

Derived from `docs/examples/03_Guided_mode_expansion_multi_layer_grating.ipynb`.

## Output files

Raw little-endian float64, C order:

| file | meaning |
|---|---|
| `freqs.f64` | band frequencies, shape (wavevectors, bands), units c/a |
| `freqs_im.f64` | radiative linewidths, same shape |

## Inputs

`ic/nominal/params.json` and `ic/variant/params.json` carry the whole configuration. Nothing else is read.

## Knobs

`run.sh --help` lists them. `SAB_GMAX` sets the cutoff and dominates cost; `SAB_NPTS` sets wavevectors; `SAB_NUMEIG` sets bands kept. The defaults are the graded values.
