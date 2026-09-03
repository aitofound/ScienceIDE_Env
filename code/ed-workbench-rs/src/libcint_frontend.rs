use libcint::prelude::*;
use thiserror::Error;

use crate::molecule::{CoordinateUnit, Molecule};

pub const ENERGY_UNIT: &str = "hartree";

#[derive(Debug, Clone)]
pub struct AoIntegrals {
    pub nao: usize,
    pub nelec: usize,
    pub coordinate_unit: CoordinateUnit,
    pub basis_provenance: String,
    pub nuclear_repulsion: f64,
    pub overlap: Vec<f64>,
    pub hcore: Vec<f64>,
    pub eri: Vec<f64>,
}

impl AoIntegrals {
    pub fn matrix(&self, data: &[f64], row: usize, column: usize) -> f64 {
        data[row * self.nao + column]
    }

    pub fn eri(&self, p: usize, q: usize, r: usize, s: usize) -> f64 {
        self.eri[((p * self.nao + q) * self.nao + r) * self.nao + s]
    }
}

#[derive(Debug, Error)]
pub enum IntegralError {
    #[error("failed to build libcint molecule: {0}")]
    Build(String),
    #[error("integral {name} returned no data")]
    MissingData { name: String },
    #[error("integral {name} has shape {actual:?}, expected {expected:?}")]
    Shape {
        name: String,
        actual: Vec<usize>,
        expected: Vec<usize>,
    },
    #[error("molecular charge gives invalid electron count")]
    InvalidElectronCount,
}

pub fn compute_ao_integrals(molecule: &Molecule) -> Result<AoIntegrals, IntegralError> {
    let embedded_basis = if molecule.basis.eq_ignore_ascii_case("sto-3g") {
        Some((STO3G_H, STO3G_O, "STO-3G"))
    } else if molecule.basis.eq_ignore_ascii_case("cc-pvdz") {
        Some((CCPVDZ_H, CCPVDZ_O, "cc-pVDZ"))
    } else {
        None
    };
    let basis_input = if let Some((hydrogen, oxygen, _)) = embedded_basis {
        format!(
            "basis = \"custom\"\n[basis-custom]\nH = '''{}'''\nO = '''{}'''\n",
            hydrogen, oxygen
        )
    } else {
        format!("basis = \"{}\"\n", molecule.basis)
    };
    let input = format!(
        "atom = \"{}\"\nunit = \"{}\"\n{}",
        molecule.atom,
        molecule.coordinate_unit.libcint_name(),
        basis_input
    );
    let built =
        CIntMol::from_toml_f(&input).map_err(|error| IntegralError::Build(format!("{error:?}")))?;
    let cint = &built.cint;
    let nao = cint.nao();
    let overlap = integral(cint, "int1e_ovlp", &[nao, nao])?;
    let kinetic = integral(cint, "int1e_kin", &[nao, nao])?;
    let nuclear_attraction = integral(cint, "int1e_nuc", &[nao, nao])?;
    let eri = integral(cint, "int2e", &[nao, nao, nao, nao])?;
    let hcore = kinetic
        .iter()
        .zip(&nuclear_attraction)
        .map(|(kinetic, attraction)| kinetic + attraction)
        .collect();
    let charges = cint.atom_charges();
    let coordinates = cint.atom_coords();
    let total_charge: f64 = charges.iter().sum();
    let nelec_signed = total_charge.round() as isize - molecule.charge;
    if nelec_signed < 0 {
        return Err(IntegralError::InvalidElectronCount);
    }
    let mut nuclear_repulsion = 0.0;
    for i in 0..charges.len() {
        for j in 0..i {
            let distance = coordinates[i]
                .iter()
                .zip(coordinates[j])
                .map(|(a, b)| (a - b).powi(2))
                .sum::<f64>()
                .sqrt();
            nuclear_repulsion += charges[i] * charges[j] / distance;
        }
    }
    Ok(AoIntegrals {
        nao,
        nelec: nelec_signed as usize,
        coordinate_unit: molecule.coordinate_unit,
        basis_provenance: if let Some((_, _, basis_name)) = embedded_basis {
            format!("PySCF 2.14.0 {basis_name} values embedded as NWChem text")
        } else {
            format!("libcint named-basis resolver: {}", molecule.basis)
        },
        nuclear_repulsion,
        overlap,
        hcore,
        eri,
    })
}

const STO3G_H: &str = r#"BASIS "ao basis" SPHERICAL PRINT
H S
  3.42525091  0.15432897
  0.62391373  0.53532814
  0.16885540  0.44463454
END"#;

const STO3G_O: &str = r#"BASIS "ao basis" SPHERICAL PRINT
O S
  130.70932  0.15432897
  23.808861  0.53532814
  6.4436083  0.44463454
O SP
  5.0331513 -0.09996723  0.15591627
  1.1695961  0.39951283  0.60768372
  0.3803890  0.70011547  0.39195739
END"#;

const CCPVDZ_H: &str = r#"BASIS "ao basis" SPHERICAL PRINT
H S
  13.010000000  0.019685000
   1.962000000  0.137977000
   0.444600000  0.478148000
H S
   0.122000000  1.000000000
H P
   0.727000000  1.000000000
END"#;

const CCPVDZ_O: &str = r#"BASIS "ao basis" SPHERICAL PRINT
O S
  11720.000000000  0.000710000 -0.000160000
   1759.000000000  0.005470000 -0.001263000
    400.800000000  0.027837000 -0.006267000
    113.700000000  0.104800000 -0.025716000
     37.030000000  0.283062000 -0.070924000
     13.270000000  0.448719000 -0.165411000
      5.025000000  0.270952000 -0.116955000
      1.013000000  0.015458000  0.557368000
O S
      0.302300000  1.000000000
O P
     17.700000000  0.043018000
      3.854000000  0.228913000
      1.046000000  0.508728000
O P
      0.275300000  1.000000000
O D
      1.185000000  1.000000000
END"#;

fn integral(cint: &CInt, name: &str, expected: &[usize]) -> Result<Vec<f64>, IntegralError> {
    let output = cint.integrate_row_major(name, None, None);
    if output.shape != expected {
        return Err(IntegralError::Shape {
            name: name.to_string(),
            actual: output.shape,
            expected: expected.to_vec(),
        });
    }
    output.out.ok_or_else(|| IntegralError::MissingData {
        name: name.to_string(),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn embedded_cc_pvdz_water_basis_has_24_spherical_functions() {
        let integrals = compute_ao_integrals(&Molecule::h2o_cc_pvdz()).unwrap();
        assert_eq!(integrals.nao, 24);
        assert_eq!(integrals.nelec, 10);
        assert!(integrals.basis_provenance.contains("PySCF 2.14.0 cc-pVDZ"));
    }
}
