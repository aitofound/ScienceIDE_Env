# autograd-eigh-primitive

Reverse-mode derivative of a **dense Hermitian eigenproblem**, the vector-Jacobian product legume writes by hand
because autograd does not provide one.

A Hermitian matrix is built from a seeded random draw; the objective sums the absolute eigenvalues and eigenvector
entries; the gradient is taken both analytically and by finite differences.

Derived from `tests/test_primitives.py`.

## Output files

Raw little-endian float64, C order:

| file | meaning |
|---|---|
| `grad_analytic.f64` | reverse-mode gradient, one value per matrix entry |
| `grad_numeric.f64` | finite-difference gradient, same shape |

## Inputs

`ic/nominal/params.json` and `ic/variant/params.json` carry the whole configuration. Nothing else is read.

## Knobs

`run.sh --help` lists them. `SAB_N` sets the matrix side, whose cost grows as its cube. The defaults are the graded values.
