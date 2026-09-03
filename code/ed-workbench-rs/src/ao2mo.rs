use nalgebra::DMatrix;
use thiserror::Error;

use crate::libcint_frontend::AoIntegrals;
use crate::problem::{ElectronicProblem, ProblemError};
use crate::rhf::RhfResult;

#[derive(Debug, Error)]
pub enum Ao2MoError {
    #[error("coefficient matrix is {rows}x{columns}, expected {expected}x{expected}")]
    CoefficientShape {
        rows: usize,
        columns: usize,
        expected: usize,
    },
    #[error(transparent)]
    Problem(#[from] ProblemError),
}

pub fn transform_to_mo(
    integrals: &AoIntegrals,
    rhf: &RhfResult,
) -> Result<ElectronicProblem, Ao2MoError> {
    let n = integrals.nao;
    let coefficients = &rhf.coefficients;
    if coefficients.nrows() != n || coefficients.ncols() != n {
        return Err(Ao2MoError::CoefficientShape {
            rows: coefficients.nrows(),
            columns: coefficients.ncols(),
            expected: n,
        });
    }
    let hcore = DMatrix::from_row_slice(n, n, &integrals.hcore);
    let mo_h1 = coefficients.transpose() * hcore * coefficients;

    let mut stage1 = vec![0.0; n.pow(4)];
    let mut stage2 = vec![0.0; n.pow(4)];
    let mut stage3 = vec![0.0; n.pow(4)];
    let mut mo_eri = vec![0.0; n.pow(4)];
    for p in 0..n {
        for nu in 0..n {
            for kappa in 0..n {
                for lambda in 0..n {
                    stage1[index4(n, p, nu, kappa, lambda)] = (0..n)
                        .map(|mu| coefficients[(mu, p)] * integrals.eri(mu, nu, kappa, lambda))
                        .sum();
                }
            }
        }
    }
    for p in 0..n {
        for q in 0..n {
            for kappa in 0..n {
                for lambda in 0..n {
                    stage2[index4(n, p, q, kappa, lambda)] = (0..n)
                        .map(|nu| coefficients[(nu, q)] * stage1[index4(n, p, nu, kappa, lambda)])
                        .sum();
                }
            }
        }
    }
    for p in 0..n {
        for q in 0..n {
            for r in 0..n {
                for lambda in 0..n {
                    stage3[index4(n, p, q, r, lambda)] = (0..n)
                        .map(|kappa| {
                            coefficients[(kappa, r)] * stage2[index4(n, p, q, kappa, lambda)]
                        })
                        .sum();
                }
            }
        }
    }
    for p in 0..n {
        for q in 0..n {
            for r in 0..n {
                for s in 0..n {
                    mo_eri[index4(n, p, q, r, s)] = (0..n)
                        .map(|lambda| {
                            coefficients[(lambda, s)] * stage3[index4(n, p, q, r, lambda)]
                        })
                        .sum();
                }
            }
        }
    }
    let mut problem = ElectronicProblem::new(
        n,
        integrals.nelec,
        0,
        integrals.nuclear_repulsion,
        mo_h1.as_slice().to_vec(),
        mo_eri,
    )?;
    problem.orbital_energies = Some(rhf.orbital_energies.clone());
    Ok(problem)
}

fn index4(n: usize, p: usize, q: usize, r: usize, s: usize) -> usize {
    ((p * n + q) * n + r) * n + s
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::molecule::CoordinateUnit;
    use crate::rhf::RhfResult;

    #[test]
    fn staged_four_index_transform_matches_direct_eight_index_sum() {
        let n = 2;
        let integrals = AoIntegrals {
            nao: n,
            nelec: 2,
            coordinate_unit: CoordinateUnit::Angstrom,
            basis_provenance: "synthetic unit test".to_string(),
            nuclear_repulsion: 0.7,
            overlap: vec![1.0, 0.0, 0.0, 1.0],
            hcore: vec![-1.0, 0.2, 0.2, -0.4],
            eri: (0..n.pow(4))
                .map(|index| (index as f64 + 1.0) / 19.0)
                .collect(),
        };
        let angle = 0.37_f64;
        let coefficients =
            DMatrix::from_row_slice(n, n, &[angle.cos(), -angle.sin(), angle.sin(), angle.cos()]);
        let rhf = RhfResult {
            total_energy: 0.0,
            electronic_energy: 0.0,
            orbital_energies: vec![-0.5, 0.3],
            coefficients: coefficients.clone(),
            density: DMatrix::zeros(n, n),
            iterations: 1,
            density_rms: 0.0,
            converged: true,
        };
        let transformed = transform_to_mo(&integrals, &rhf).unwrap();
        for p in 0..n {
            for q in 0..n {
                let expected_h1: f64 = (0..n)
                    .flat_map(|mu| (0..n).map(move |nu| (mu, nu)))
                    .map(|(mu, nu)| {
                        coefficients[(mu, p)] * integrals.hcore[mu * n + nu] * coefficients[(nu, q)]
                    })
                    .sum();
                assert!((transformed.h1(p, q) - expected_h1).abs() < 1e-12);
                for r in 0..n {
                    for s in 0..n {
                        let mut expected_eri = 0.0;
                        for mu in 0..n {
                            for nu in 0..n {
                                for kappa in 0..n {
                                    for lambda in 0..n {
                                        expected_eri += coefficients[(mu, p)]
                                            * coefficients[(nu, q)]
                                            * coefficients[(kappa, r)]
                                            * coefficients[(lambda, s)]
                                            * integrals.eri(mu, nu, kappa, lambda);
                                    }
                                }
                            }
                        }
                        assert!((transformed.eri(p, q, r, s) - expected_eri).abs() < 1e-12);
                    }
                }
            }
        }
    }
}
