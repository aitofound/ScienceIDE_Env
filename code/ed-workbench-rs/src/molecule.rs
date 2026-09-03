use std::fmt;

use thiserror::Error;

const H2O_GEOMETRY_ANGSTROM: &str =
    "O 0 0 0; H 0.967 0 0; H -0.2923916843556798 0.9217353757557798 0";

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum CoordinateUnit {
    Angstrom,
    Bohr,
}

impl CoordinateUnit {
    pub const fn libcint_name(self) -> &'static str {
        match self {
            Self::Angstrom => "angstrom",
            Self::Bohr => "bohr",
        }
    }
}

impl fmt::Display for CoordinateUnit {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        formatter.write_str(self.libcint_name())
    }
}

#[derive(Debug, Clone)]
pub struct Molecule {
    pub atom: String,
    pub basis: String,
    pub charge: isize,
    pub coordinate_unit: CoordinateUnit,
}

#[derive(Debug, Error)]
pub enum MoleculeError {
    #[error("atom specification must not be empty")]
    EmptyAtoms,
    #[error("basis must not be empty")]
    EmptyBasis,
    #[error("atom or basis strings must not contain double quotes")]
    InvalidText,
}

impl Molecule {
    pub fn new(
        atom: impl Into<String>,
        basis: impl Into<String>,
        charge: isize,
        coordinate_unit: CoordinateUnit,
    ) -> Result<Self, MoleculeError> {
        let atom = atom.into();
        let basis = basis.into();
        if atom.trim().is_empty() {
            return Err(MoleculeError::EmptyAtoms);
        }
        if basis.trim().is_empty() {
            return Err(MoleculeError::EmptyBasis);
        }
        if atom.contains('"') || basis.contains('"') {
            return Err(MoleculeError::InvalidText);
        }
        Ok(Self {
            atom,
            basis,
            charge,
            coordinate_unit,
        })
    }

    pub fn h2_sto3g() -> Self {
        // Cartesian Angstrom coordinates: R(H-H) = 1.4 Angstrom.
        Self::new(
            "H 0 0 -0.7; H 0 0 0.7",
            "STO-3G",
            0,
            CoordinateUnit::Angstrom,
        )
        .expect("built-in molecule is valid")
    }

    pub fn h2o_sto3g() -> Self {
        // Cartesian Angstrom coordinates: R(O-H) = 0.967 Angstrom,
        // angle(H-O-H) = 107.6 degree.
        Self::new(H2O_GEOMETRY_ANGSTROM, "STO-3G", 0, CoordinateUnit::Angstrom)
            .expect("built-in molecule is valid")
    }

    pub fn h2o_cc_pvdz() -> Self {
        // Same challenge geometry, all-electron cc-pVDZ benchmark input.
        Self::new(
            H2O_GEOMETRY_ANGSTROM,
            "cc-pVDZ",
            0,
            CoordinateUnit::Angstrom,
        )
        .expect("built-in molecule is valid")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rejects_empty_or_unsafe_text_fields() {
        assert!(matches!(
            Molecule::new("", "STO-3G", 0, CoordinateUnit::Angstrom),
            Err(MoleculeError::EmptyAtoms)
        ));
        assert!(matches!(
            Molecule::new("H 0 0 0", "", 0, CoordinateUnit::Angstrom),
            Err(MoleculeError::EmptyBasis)
        ));
        assert!(matches!(
            Molecule::new("H \"0\" 0 0", "STO-3G", 0, CoordinateUnit::Angstrom),
            Err(MoleculeError::InvalidText)
        ));
    }

    #[test]
    fn built_in_systems_have_expected_charge_and_basis() {
        for molecule in [Molecule::h2_sto3g(), Molecule::h2o_sto3g()] {
            assert_eq!(molecule.charge, 0);
            assert_eq!(molecule.basis, "STO-3G");
            assert_eq!(molecule.coordinate_unit, CoordinateUnit::Angstrom);
            assert!(!molecule.atom.is_empty());
        }
    }

    #[test]
    fn h2o_cc_pvdz_is_all_electron_benchmark_geometry() {
        let molecule = Molecule::h2o_cc_pvdz();
        assert_eq!(molecule.charge, 0);
        assert_eq!(molecule.basis, "cc-pVDZ");
        assert_eq!(molecule.coordinate_unit, CoordinateUnit::Angstrom);
        assert_eq!(molecule.atom, Molecule::h2o_sto3g().atom);
    }

    #[test]
    fn built_in_cartesian_geometries_have_documented_distances_and_angle() {
        let h2 = coordinates(&Molecule::h2_sto3g().atom);
        assert!((distance(h2[0], h2[1]) - 1.4).abs() < 1e-12);

        let water = coordinates(&Molecule::h2o_sto3g().atom);
        let first_bond = subtract(water[1], water[0]);
        let second_bond = subtract(water[2], water[0]);
        let first_length = norm(first_bond);
        let second_length = norm(second_bond);
        let cosine = dot(first_bond, second_bond) / (first_length * second_length);
        let angle = cosine.acos().to_degrees();
        assert!((first_length - 0.967).abs() < 1e-12);
        assert!((second_length - 0.967).abs() < 1e-12);
        assert!((angle - 107.6).abs() < 1e-12);
    }

    #[test]
    fn coordinate_units_have_explicit_display_and_libcint_names() {
        assert_eq!(CoordinateUnit::Angstrom.to_string(), "angstrom");
        assert_eq!(CoordinateUnit::Angstrom.libcint_name(), "angstrom");
        assert_eq!(CoordinateUnit::Bohr.to_string(), "bohr");
        assert_eq!(CoordinateUnit::Bohr.libcint_name(), "bohr");
    }

    fn coordinates(atom: &str) -> Vec<[f64; 3]> {
        atom.split(';')
            .map(|record| {
                let fields: Vec<&str> = record.split_whitespace().collect();
                [
                    fields[1].parse().unwrap(),
                    fields[2].parse().unwrap(),
                    fields[3].parse().unwrap(),
                ]
            })
            .collect()
    }

    fn subtract(left: [f64; 3], right: [f64; 3]) -> [f64; 3] {
        [left[0] - right[0], left[1] - right[1], left[2] - right[2]]
    }

    fn dot(left: [f64; 3], right: [f64; 3]) -> f64 {
        left.iter().zip(right).map(|(a, b)| a * b).sum()
    }

    fn norm(vector: [f64; 3]) -> f64 {
        dot(vector, vector).sqrt()
    }

    fn distance(left: [f64; 3], right: [f64; 3]) -> f64 {
        norm(subtract(left, right))
    }
}
