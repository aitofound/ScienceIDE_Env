use crate::excitation::ExcitationSpace;

#[derive(Debug, Clone)]
pub struct Amplitudes {
    pub values: Vec<f64>,
}

impl Amplitudes {
    pub fn zeros(space: &ExcitationSpace) -> Self {
        Self {
            values: vec![0.0; space.excitations.len()],
        }
    }

    pub fn norm(&self) -> f64 {
        self.values
            .iter()
            .map(|value| value * value)
            .sum::<f64>()
            .sqrt()
    }
}

pub fn orbital_denominators(
    space: &ExcitationSpace,
    orbital_energies: &[f64],
    norb: usize,
) -> Vec<f64> {
    space
        .excitations
        .iter()
        .map(|excitation| {
            let holes: f64 = excitation
                .holes
                .iter()
                .map(|&orbital| orbital_energies[orbital % norb])
                .sum();
            let particles: f64 = excitation
                .particles
                .iter()
                .map(|&orbital| orbital_energies[orbital % norb])
                .sum();
            holes - particles
        })
        .collect()
}
