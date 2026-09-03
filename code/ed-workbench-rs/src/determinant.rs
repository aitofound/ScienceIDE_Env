use crate::combinadic::{CombinadicError, combination_count, unrank_occupation};
use crate::problem::ElectronicProblem;
use thiserror::Error;

#[derive(Debug, Error)]
pub enum DeterminantError {
    #[error("the Level 0 u64 representation supports at most 32 spatial orbitals")]
    TooManyOrbitals,
    #[error("electron count is inconsistent with NELEC={nelec} and MS2={ms2}")]
    InvalidElectronCount { nelec: usize, ms2: isize },
    #[error("requested {electrons} electrons in only {orbitals} orbitals")]
    TooManyElectrons { electrons: usize, orbitals: usize },
    #[error("ORBSYM contains {actual} labels, expected NORB={expected}")]
    OrbitalSymmetryLength { actual: usize, expected: usize },
    #[error("ORBSYM[{orbital}]={irrep} is outside the Molpro range 1..=8")]
    InvalidOrbitalSymmetry { orbital: usize, irrep: usize },
    #[error("ISYM={0} is outside the Molpro range 1..=8")]
    InvalidWavefunctionSymmetry(usize),
    #[error("the requested ISYM={0} determinant sector is empty")]
    EmptySymmetrySector(usize),
    #[error("determinant space contains {count} entries and does not fit this platform")]
    SpaceTooLarge { count: u128 },
    #[error("failed to allocate determinant space with {count} entries")]
    AllocationFailed { count: usize },
    #[error(transparent)]
    Combinadic(#[from] CombinadicError),
}

#[derive(Debug, Clone)]
pub struct DeterminantBasis {
    pub norb: usize,
    pub nalpha: usize,
    pub nbeta: usize,
    pub alpha_strings: Vec<u64>,
    pub beta_strings: Vec<u64>,
    alpha_irreps: Vec<usize>,
    beta_irreps: Vec<usize>,
    beta_indices_by_irrep: Vec<Vec<usize>>,
    beta_ranks_within_irrep: Vec<usize>,
    alpha_offsets: Vec<usize>,
    target_irrep: usize,
}

impl DeterminantBasis {
    pub fn new(norb: usize, nelec: usize, ms2: isize) -> Result<Self, DeterminantError> {
        Self::build(norb, nelec, ms2, None)
    }

    pub fn with_symmetry(
        norb: usize,
        nelec: usize,
        ms2: isize,
        orbsym: &[usize],
        isym: usize,
    ) -> Result<Self, DeterminantError> {
        if orbsym.len() != norb {
            return Err(DeterminantError::OrbitalSymmetryLength {
                actual: orbsym.len(),
                expected: norb,
            });
        }
        if let Some((orbital, &irrep)) = orbsym
            .iter()
            .enumerate()
            .find(|(_, irrep)| !(1..=8).contains(*irrep))
        {
            return Err(DeterminantError::InvalidOrbitalSymmetry { orbital, irrep });
        }
        if !(1..=8).contains(&isym) {
            return Err(DeterminantError::InvalidWavefunctionSymmetry(isym));
        }
        Self::build(norb, nelec, ms2, Some((orbsym, isym)))
    }

    pub fn from_problem(problem: &ElectronicProblem) -> Result<Self, DeterminantError> {
        Self::with_symmetry(
            problem.norb,
            problem.nelec,
            problem.ms2,
            &problem.orbsym,
            problem.isym,
        )
    }

    fn build(
        norb: usize,
        nelec: usize,
        ms2: isize,
        symmetry: Option<(&[usize], usize)>,
    ) -> Result<Self, DeterminantError> {
        if norb > 32 {
            return Err(DeterminantError::TooManyOrbitals);
        }
        let nalpha_twice = nelec as isize + ms2;
        let nbeta_twice = nelec as isize - ms2;
        if nalpha_twice < 0 || nbeta_twice < 0 || nalpha_twice % 2 != 0 || nbeta_twice % 2 != 0 {
            return Err(DeterminantError::InvalidElectronCount { nelec, ms2 });
        }
        let nalpha = (nalpha_twice / 2) as usize;
        let nbeta = (nbeta_twice / 2) as usize;
        let alpha_strings = occupation_strings(norb, nalpha)?;
        let beta_strings = occupation_strings(norb, nbeta)?;
        let target_irrep = symmetry.map_or(1, |(_, isym)| isym);
        let alpha_irreps = alpha_strings
            .iter()
            .map(|&bits| symmetry.map_or(1, |(orbsym, _)| string_irrep(bits, orbsym)))
            .collect::<Vec<_>>();
        let beta_irreps = beta_strings
            .iter()
            .map(|&bits| symmetry.map_or(1, |(orbsym, _)| string_irrep(bits, orbsym)))
            .collect::<Vec<_>>();
        let mut beta_indices_by_irrep = vec![Vec::new(); 8];
        let mut beta_ranks_within_irrep = vec![0; beta_strings.len()];
        for (beta_index, &irrep) in beta_irreps.iter().enumerate() {
            beta_ranks_within_irrep[beta_index] = beta_indices_by_irrep[irrep - 1].len();
            beta_indices_by_irrep[irrep - 1].push(beta_index);
        }
        let mut alpha_offsets = Vec::with_capacity(alpha_strings.len() + 1);
        alpha_offsets.push(0);
        for &alpha_irrep in &alpha_irreps {
            let required_beta_irrep = molpro_irrep_product(alpha_irrep, target_irrep);
            let next = alpha_offsets.last().copied().unwrap()
                + beta_indices_by_irrep[required_beta_irrep - 1].len();
            alpha_offsets.push(next);
        }
        if alpha_offsets.last() == Some(&0) {
            return Err(DeterminantError::EmptySymmetrySector(target_irrep));
        }
        Ok(Self {
            norb,
            nalpha,
            nbeta,
            alpha_strings,
            beta_strings,
            alpha_irreps,
            beta_irreps,
            beta_indices_by_irrep,
            beta_ranks_within_irrep,
            alpha_offsets,
            target_irrep,
        })
    }

    pub fn address(&self, determinant: u64) -> Option<usize> {
        let orbital_mask = (1_u64 << self.norb) - 1;
        let alpha = determinant & orbital_mask;
        let beta = determinant >> self.norb;
        if beta >> self.norb != 0 {
            return None;
        }
        let alpha_index = self.alpha_strings.binary_search(&alpha).ok()?;
        let beta_index = self.beta_strings.binary_search(&beta).ok()?;
        self.pair_address(alpha_index, beta_index)
    }

    pub fn string_pair(&self, address: usize) -> Option<(usize, usize)> {
        if address >= self.len() {
            return None;
        }
        let alpha = self
            .alpha_offsets
            .partition_point(|&offset| offset <= address)
            - 1;
        let beta_irrep = molpro_irrep_product(self.alpha_irreps[alpha], self.target_irrep);
        let rank = address - self.alpha_offsets[alpha];
        let beta = self.beta_indices_by_irrep[beta_irrep - 1][rank];
        Some((alpha, beta))
    }

    pub fn pair_address(&self, alpha: usize, beta: usize) -> Option<usize> {
        if alpha >= self.alpha_strings.len() || beta >= self.beta_strings.len() {
            return None;
        }
        if molpro_irrep_product(self.alpha_irreps[alpha], self.beta_irreps[beta])
            != self.target_irrep
        {
            return None;
        }
        Some(self.alpha_offsets[alpha] + self.beta_ranks_within_irrep[beta])
    }

    pub fn string_pairs(&self) -> impl Iterator<Item = (usize, usize)> + '_ {
        self.alpha_irreps
            .iter()
            .enumerate()
            .flat_map(move |(alpha, &alpha_irrep)| {
                let beta_irrep = molpro_irrep_product(alpha_irrep, self.target_irrep);
                self.beta_indices_by_irrep[beta_irrep - 1]
                    .iter()
                    .copied()
                    .map(move |beta| (alpha, beta))
            })
    }

    pub fn determinant(&self, address: usize) -> Option<u64> {
        let (alpha, beta) = self.string_pair(address)?;
        Some(self.alpha_strings[alpha] | (self.beta_strings[beta] << self.norb))
    }

    pub fn determinants(&self) -> impl Iterator<Item = u64> + '_ {
        self.string_pairs()
            .map(|(alpha, beta)| self.alpha_strings[alpha] | (self.beta_strings[beta] << self.norb))
    }

    pub fn len(&self) -> usize {
        self.alpha_offsets.last().copied().unwrap_or(0)
    }

    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }
}

/// Direct product for one-based Molpro irreps in D2h and its Abelian subgroups.
fn molpro_irrep_product(left: usize, right: usize) -> usize {
    ((left - 1) ^ (right - 1)) + 1
}

fn string_irrep(bits: u64, orbsym: &[usize]) -> usize {
    let mut product = 1;
    for (orbital, &irrep) in orbsym.iter().enumerate() {
        if bits & (1_u64 << orbital) != 0 {
            product = molpro_irrep_product(product, irrep);
        }
    }
    product
}

pub fn occupation_strings(orbitals: usize, electrons: usize) -> Result<Vec<u64>, DeterminantError> {
    if electrons > orbitals {
        return Err(DeterminantError::TooManyElectrons {
            electrons,
            orbitals,
        });
    }
    if orbitals > 64 {
        return Err(DeterminantError::TooManyOrbitals);
    }
    let count_u128 = combination_count(orbitals, electrons)?;
    let count = usize::try_from(count_u128)
        .map_err(|_| DeterminantError::SpaceTooLarge { count: count_u128 })?;
    let mut strings = Vec::new();
    strings
        .try_reserve_exact(count)
        .map_err(|_| DeterminantError::AllocationFailed { count })?;
    for rank in 0..count {
        strings.push(unrank_occupation(rank as u128, orbitals, electrons)?);
    }
    Ok(strings)
}

pub fn apply_annihilation(determinant: u64, orbital: usize) -> Option<(u64, f64)> {
    let mask = 1_u64 << orbital;
    if determinant & mask == 0 {
        return None;
    }
    let occupied_below = (determinant & (mask - 1)).count_ones();
    let sign = if occupied_below.is_multiple_of(2) {
        1.0
    } else {
        -1.0
    };
    Some((determinant ^ mask, sign))
}

pub fn apply_creation(determinant: u64, orbital: usize) -> Option<(u64, f64)> {
    let mask = 1_u64 << orbital;
    if determinant & mask != 0 {
        return None;
    }
    let occupied_below = (determinant & (mask - 1)).count_ones();
    let sign = if occupied_below.is_multiple_of(2) {
        1.0
    } else {
        -1.0
    };
    Some((determinant | mask, sign))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn enumerates_fixed_population_in_numeric_lexical_order() {
        assert_eq!(
            occupation_strings(4, 2).unwrap(),
            vec![0b0011, 0b0101, 0b0110, 0b1001, 0b1010, 0b1100]
        );
    }

    #[test]
    fn builds_alpha_beta_product_basis() {
        let basis = DeterminantBasis::new(2, 2, 0).unwrap();
        assert_eq!(basis.nalpha, 1);
        assert_eq!(basis.nbeta, 1);
        assert_eq!(basis.len(), 4);
        for det in basis.determinants() {
            assert_eq!((det & 0b11).count_ones(), 1);
            assert_eq!((det >> 2).count_ones(), 1);
            assert!(basis.address(det).is_some());
        }
    }

    #[test]
    fn filters_to_the_requested_molpro_symmetry_sector() {
        let basis = DeterminantBasis::with_symmetry(2, 2, 0, &[1, 2], 1).unwrap();
        assert_eq!(basis.len(), 2);
        assert_eq!(basis.string_pairs().collect::<Vec<_>>(), [(0, 0), (1, 1)]);
        assert_eq!(basis.pair_address(0, 0), Some(0));
        assert_eq!(basis.pair_address(0, 1), None);
        assert_eq!(basis.pair_address(1, 1), Some(1));
        assert_eq!(basis.address(0b0101), Some(0));
        assert_eq!(basis.address(0b1010), Some(1));
    }

    #[test]
    fn dzp_symmetry_index_is_compact_and_has_the_expected_dimension() {
        let orbsym = [
            1, 3, 1, 2, 1, 3, 1, 2, 3, 1, 3, 1, 4, 1, 2, 3, 3, 1, 2, 4, 1, 1, 3, 1,
        ];
        let basis = DeterminantBasis::with_symmetry(24, 8, 0, &orbsym, 1).unwrap();
        assert_eq!(basis.alpha_strings.len(), 10_626);
        assert_eq!(basis.beta_strings.len(), 10_626);
        assert_eq!(basis.len(), 28_233_466);

        let indexing_bytes = (basis.alpha_strings.len()
            + basis.beta_strings.len()
            + basis.alpha_irreps.len()
            + basis.beta_irreps.len()
            + basis.beta_ranks_within_irrep.len()
            + basis.alpha_offsets.len()
            + basis
                .beta_indices_by_irrep
                .iter()
                .map(Vec::len)
                .sum::<usize>())
            * std::mem::size_of::<usize>();
        assert!(indexing_bytes < 1_000_000);
    }

    #[test]
    fn molpro_irrep_labels_form_the_expected_xor_product_table() {
        assert_eq!(molpro_irrep_product(1, 7), 7);
        assert_eq!(molpro_irrep_product(4, 6), 7);
        assert_eq!(molpro_irrep_product(8, 8), 1);
    }

    #[test]
    fn fermionic_operators_return_expected_sign() {
        let det = 0b1011;
        assert_eq!(apply_annihilation(det, 3), Some((0b0011, 1.0)));
        assert_eq!(apply_annihilation(det, 2), None);
        assert_eq!(apply_creation(det, 2), Some((0b1111, 1.0)));
        assert_eq!(apply_annihilation(0b1010, 3), Some((0b0010, -1.0)));
    }
}
