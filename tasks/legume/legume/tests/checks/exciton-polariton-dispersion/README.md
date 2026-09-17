# exciton-polariton-dispersion

The **exciton-polariton dispersion** in the strong-coupling regime: the same quantum-well stack as the
anchored polariton check, at a higher oscillator strength where the light-matter splitting is large.

Derived from `docs/examples/15_excitons_and_polaritons.ipynb`.

## Output files

Raw little-endian float64, C order:

| file | meaning |
|---|---|
| `eners.f64` | polariton energies, eV |
| `eners_im.f64` | polariton linewidths |
| `fractions_ex.f64` | excitonic fraction of each branch |

## Inputs

`ic/nominal/params.json` and `ic/variant/params.json` carry the whole configuration. Nothing else is read.

## Knobs

`run.sh --help` lists them. `SAB_GMAX` sets the cutoff; `SAB_NPTS` sets wavevectors. The defaults are the graded values.
