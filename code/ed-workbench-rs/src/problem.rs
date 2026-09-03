use thiserror::Error;

use crate::fcidump::Fcidump;

#[derive(Debug, Clone)]
pub struct ElectronicProblem {
    pub norb: usize,
    pub nelec: usize,
    pub ms2: isize,
    /// Molpro-style one-based Abelian irrep labels for the spatial orbitals.
    pub orbsym: Vec<usize>,
    /// Molpro-style one-based target irrep for the many-electron state.
    pub isym: usize,
    pub ecore: f64,
    h1: Vec<f64>,
    eri: Vec<f64>,
    pub orbital_energies: Option<Vec<f64>>,
}

#[derive(Debug, Error)]
pub enum ProblemError {
    #[error("NORB must be positive")]
    NoOrbitals,
    #[error("NELEC={nelec} does not fit in {norb} spatial orbitals")]
    ElectronOverflow { norb: usize, nelec: usize },
    #[error("NELEC={nelec} and MS2={ms2} have inconsistent parity")]
    SpinParity { nelec: usize, ms2: isize },
    #[error("one-electron integral length is {actual}, expected {expected}")]
    OneBodyLength { actual: usize, expected: usize },
    #[error("two-electron integral length is {actual}, expected {expected}")]
    TwoBodyLength { actual: usize, expected: usize },
    #[error("ORBSYM contains {actual} labels, expected NORB={expected}")]
    OrbitalSymmetryLength { actual: usize, expected: usize },
    #[error("ORBSYM[{orbital}]={irrep} is outside the Molpro range 1..=8")]
    InvalidOrbitalSymmetry { orbital: usize, irrep: usize },
    #[error("ISYM={0} is outside the Molpro range 1..=8")]
    InvalidWavefunctionSymmetry(usize),
    #[error("integrals and core energy must be finite")]
    NonFinite,
}

impl ElectronicProblem {
    pub fn new(
        norb: usize,
        nelec: usize,
        ms2: isize,
        ecore: f64,
        h1: Vec<f64>,
        eri: Vec<f64>,
    ) -> Result<Self, ProblemError> {
        if norb == 0 {
            return Err(ProblemError::NoOrbitals);
        }
        if nelec > 2 * norb {
            return Err(ProblemError::ElectronOverflow { norb, nelec });
        }
        if (nelec as isize + ms2) % 2 != 0 || nelec as isize + ms2 < 0 || nelec as isize - ms2 < 0 {
            return Err(ProblemError::SpinParity { nelec, ms2 });
        }
        if h1.len() != norb * norb {
            return Err(ProblemError::OneBodyLength {
                actual: h1.len(),
                expected: norb * norb,
            });
        }
        let expected_eri = norb.pow(4);
        if eri.len() != expected_eri {
            return Err(ProblemError::TwoBodyLength {
                actual: eri.len(),
                expected: expected_eri,
            });
        }
        if !ecore.is_finite()
            || h1.iter().any(|value| !value.is_finite())
            || eri.iter().any(|value| !value.is_finite())
        {
            return Err(ProblemError::NonFinite);
        }
        Ok(Self {
            norb,
            nelec,
            ms2,
            orbsym: vec![1; norb],
            isym: 1,
            ecore,
            h1,
            eri,
            orbital_energies: None,
        })
    }

    pub fn from_fcidump(dump: &Fcidump) -> Result<Self, ProblemError> {
        let mut h1 = vec![0.0; dump.norb * dump.norb];
        let mut eri = vec![0.0; dump.norb.pow(4)];
        for p in 0..dump.norb {
            for q in 0..dump.norb {
                h1[p * dump.norb + q] = dump.h1(p, q);
                for r in 0..dump.norb {
                    for s in 0..dump.norb {
                        eri[index4(dump.norb, p, q, r, s)] = dump.eri(p, q, r, s);
                    }
                }
            }
        }
        Self::new(dump.norb, dump.nelec, dump.ms2, dump.ecore, h1, eri)?
            .with_symmetry(dump.orbsym.clone(), dump.isym)
    }

    pub fn h1(&self, p: usize, q: usize) -> f64 {
        self.h1[p * self.norb + q]
    }

    pub fn eri(&self, p: usize, q: usize, r: usize, s: usize) -> f64 {
        self.eri[index4(self.norb, p, q, r, s)]
    }

    pub fn h1_data(&self) -> &[f64] {
        &self.h1
    }

    pub fn eri_data(&self) -> &[f64] {
        &self.eri
    }

    pub fn with_orbital_energies(mut self, energies: Vec<f64>) -> Self {
        self.orbital_energies = Some(energies);
        self
    }

    pub fn with_symmetry(mut self, orbsym: Vec<usize>, isym: usize) -> Result<Self, ProblemError> {
        if orbsym.len() != self.norb {
            return Err(ProblemError::OrbitalSymmetryLength {
                actual: orbsym.len(),
                expected: self.norb,
            });
        }
        if let Some((orbital, &irrep)) = orbsym
            .iter()
            .enumerate()
            .find(|(_, irrep)| !(1..=8).contains(*irrep))
        {
            return Err(ProblemError::InvalidOrbitalSymmetry { orbital, irrep });
        }
        if !(1..=8).contains(&isym) {
            return Err(ProblemError::InvalidWavefunctionSymmetry(isym));
        }
        self.orbsym = orbsym;
        self.isym = isym;
        Ok(self)
    }
}

pub(crate) fn index4(n: usize, p: usize, q: usize, r: usize, s: usize) -> usize {
    ((p * n + q) * n + r) * n + s
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn validates_dimensions_and_spin_parity() {
        assert!(matches!(
            ElectronicProblem::new(2, 2, 1, 0.0, vec![0.0; 4], vec![0.0; 16]),
            Err(ProblemError::SpinParity { .. })
        ));
        assert!(matches!(
            ElectronicProblem::new(2, 2, 0, 0.0, vec![0.0; 3], vec![0.0; 16]),
            Err(ProblemError::OneBodyLength { .. })
        ));
    }

    #[test]
    fn validates_molpro_symmetry_metadata() {
        let problem = ElectronicProblem::new(2, 2, 0, 0.0, vec![0.0; 4], vec![0.0; 16])
            .unwrap()
            .with_symmetry(vec![1, 4], 1)
            .unwrap();
        assert_eq!(problem.orbsym, vec![1, 4]);
        assert_eq!(problem.isym, 1);
        assert!(matches!(
            problem.clone().with_symmetry(vec![1], 1),
            Err(ProblemError::OrbitalSymmetryLength { .. })
        ));
        assert!(matches!(
            problem.clone().with_symmetry(vec![1, 9], 1),
            Err(ProblemError::InvalidOrbitalSymmetry { .. })
        ));
        assert!(matches!(
            problem.with_symmetry(vec![1, 4], 0),
            Err(ProblemError::InvalidWavefunctionSymmetry(0))
        ));
    }
}
