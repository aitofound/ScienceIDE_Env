use std::collections::HashMap;

use thiserror::Error;

use crate::combinadic::rank_occupation;
use crate::determinant::{apply_annihilation, apply_creation, occupation_strings};

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OneBodyLink {
    pub source: usize,
    pub target: usize,
    pub created: usize,
    pub annihilated: usize,
    pub sign: i8,
}

#[derive(Debug, Clone)]
pub struct StringSpace {
    pub norb: usize,
    pub nelec: usize,
    pub strings: Vec<u64>,
    addresses: HashMap<u64, usize>,
    outgoing: Vec<Vec<OneBodyLink>>,
}

#[derive(Debug, Error)]
pub enum StringSpaceError {
    #[error("failed to enumerate strings: {0}")]
    Enumeration(String),
}

impl StringSpace {
    pub fn new(norb: usize, nelec: usize) -> Result<Self, StringSpaceError> {
        let strings = occupation_strings(norb, nelec)
            .map_err(|error| StringSpaceError::Enumeration(error.to_string()))?;
        let addresses: HashMap<_, _> = strings
            .iter()
            .enumerate()
            .map(|(index, &bits)| (bits, index))
            .collect();
        let mut outgoing = vec![Vec::new(); strings.len()];
        for (source, &bits) in strings.iter().enumerate() {
            for annihilated in 0..norb {
                let Some((after_annihilation, sign_a)) = apply_annihilation(bits, annihilated)
                else {
                    continue;
                };
                for created in 0..norb {
                    let Some((target_bits, sign_c)) = apply_creation(after_annihilation, created)
                    else {
                        continue;
                    };
                    let target = addresses[&target_bits];
                    let sign = (sign_a * sign_c) as i8;
                    outgoing[source].push(OneBodyLink {
                        source,
                        target,
                        created,
                        annihilated,
                        sign,
                    });
                }
            }
            outgoing[source]
                .sort_by_key(|link| (link.created, link.annihilated, link.target, link.sign));
        }
        Ok(Self {
            norb,
            nelec,
            strings,
            addresses,
            outgoing,
        })
    }

    pub fn address(&self, bits: u64) -> Option<usize> {
        self.addresses.get(&bits).copied()
    }

    pub fn rank(&self, bits: u64) -> Option<usize> {
        rank_occupation(bits, self.norb, self.nelec)
            .ok()
            .and_then(|rank| usize::try_from(rank).ok())
    }

    pub fn unrank(&self, rank: usize) -> Option<u64> {
        self.strings.get(rank).copied()
    }

    pub fn outgoing(&self, source: usize) -> &[OneBodyLink] {
        &self.outgoing[source]
    }

    pub fn len(&self) -> usize {
        self.strings.len()
    }

    pub fn link_count(&self) -> usize {
        self.outgoing.iter().map(Vec::len).sum()
    }

    pub fn is_empty(&self) -> bool {
        self.strings.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn links_include_diagonal_number_operators() {
        let space = StringSpace::new(3, 1).unwrap();
        let source = space.address(0b010).unwrap();
        let diagonal = space
            .outgoing(source)
            .iter()
            .find(|link| link.created == 1 && link.annihilated == 1)
            .unwrap();
        assert_eq!(diagonal.target, source);
        assert_eq!(diagonal.sign, 1);
    }

    #[test]
    fn link_signs_match_fermionic_bit_operators() {
        for nelec in 0..=3 {
            let space = StringSpace::new(4, nelec).unwrap();
            for (source, &bits) in space.strings.iter().enumerate() {
                for link in space.outgoing(source) {
                    let (after, sign_a) = apply_annihilation(bits, link.annihilated).unwrap();
                    let (target, sign_c) = apply_creation(after, link.created).unwrap();
                    assert_eq!(space.address(target), Some(link.target));
                    assert_eq!((sign_a * sign_c) as i8, link.sign);
                }
            }
        }
    }

    #[test]
    fn combinadic_rank_and_unrank_match_lexical_storage() {
        for norb in 1..=6 {
            for nelec in 0..=norb {
                let space = StringSpace::new(norb, nelec).unwrap();
                for (expected_rank, &bits) in space.strings.iter().enumerate() {
                    assert_eq!(space.rank(bits), Some(expected_rank));
                    assert_eq!(space.unrank(expected_rank), Some(bits));
                }
                assert_eq!(space.unrank(space.len()), None);
            }
        }
    }
}
