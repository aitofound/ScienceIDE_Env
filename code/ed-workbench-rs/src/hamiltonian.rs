use nalgebra::DMatrix;

use crate::determinant::{DeterminantBasis, apply_annihilation, apply_creation};
use crate::fcidump::Fcidump;

pub fn build_dense_hamiltonian(dump: &Fcidump, basis: &DeterminantBasis) -> DMatrix<f64> {
    let dimension = basis.len();
    let mut matrix = DMatrix::zeros(dimension, dimension);

    for (column, ket) in basis.determinants().enumerate() {
        matrix[(column, column)] += dump.ecore;

        for p in 0..(2 * dump.norb) {
            let p_spatial = p % dump.norb;
            let p_spin = p / dump.norb;
            for q in 0..(2 * dump.norb) {
                if p_spin != q / dump.norb {
                    continue;
                }
                let integral = dump.h1(p_spatial, q % dump.norb);
                if integral == 0.0 {
                    continue;
                }
                if let Some((after_q, sign_q)) = apply_annihilation(ket, q)
                    && let Some((bra_det, sign_p)) = apply_creation(after_q, p)
                    && let Some(row) = basis.address(bra_det)
                {
                    matrix[(row, column)] += integral * sign_q * sign_p;
                }
            }
        }

        for p in 0..(2 * dump.norb) {
            let p_spatial = p % dump.norb;
            let p_spin = p / dump.norb;
            for q in 0..(2 * dump.norb) {
                if p_spin != q / dump.norb {
                    continue;
                }
                let q_spatial = q % dump.norb;
                for r in 0..(2 * dump.norb) {
                    let r_spatial = r % dump.norb;
                    let r_spin = r / dump.norb;
                    for s in 0..(2 * dump.norb) {
                        if r_spin != s / dump.norb {
                            continue;
                        }
                        let integral = dump.eri(p_spatial, q_spatial, r_spatial, s % dump.norb);
                        if integral == 0.0 {
                            continue;
                        }
                        if let Some((after_q, sign_q)) = apply_annihilation(ket, q)
                            && let Some((after_s, sign_s)) = apply_annihilation(after_q, s)
                            && let Some((after_r, sign_r)) = apply_creation(after_s, r)
                            && let Some((bra_det, sign_p)) = apply_creation(after_r, p)
                            && let Some(row) = basis.address(bra_det)
                        {
                            matrix[(row, column)] +=
                                0.5 * integral * sign_q * sign_s * sign_r * sign_p;
                        }
                    }
                }
            }
        }
    }
    matrix
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::determinant::DeterminantBasis;
    use crate::fcidump::Fcidump;

    #[test]
    fn adds_core_energy_to_every_diagonal() {
        let dump = Fcidump::parse("&FCI NORB=1,NELEC=1,MS2=1,&END\n0.25 0 0 0 0\n").unwrap();
        let basis = DeterminantBasis::new(1, 1, 1).unwrap();
        let matrix = build_dense_hamiltonian(&dump, &basis);
        assert_eq!(matrix[(0, 0)], 0.25);
    }

    #[test]
    fn produces_a_symmetric_matrix() {
        let dump = Fcidump::parse(
            "&FCI NORB=2,NELEC=2,MS2=0,&END\n\
             0.7 1 1 1 1\n\
             0.2 2 1 2 1\n\
             -1.0 1 1 0 0\n\
             -0.5 2 2 0 0\n",
        )
        .unwrap();
        let basis = DeterminantBasis::new(2, 2, 0).unwrap();
        let matrix = build_dense_hamiltonian(&dump, &basis);
        assert!((&matrix - matrix.transpose()).amax() < 1e-12);
    }
}
