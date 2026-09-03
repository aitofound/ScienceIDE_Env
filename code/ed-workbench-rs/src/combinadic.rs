use thiserror::Error;

#[derive(Debug, Error, Clone, PartialEq, Eq)]
pub enum CombinadicError {
    #[error("population {nelec} exceeds orbital count {norb}")]
    PopulationOverflow { norb: usize, nelec: usize },
    #[error("u64 occupation strings support at most 64 orbitals, got {0}")]
    TooManyOrbitals(usize),
    #[error("occupation string contains bits outside {norb} orbitals")]
    BitsOutsideSpace { norb: usize },
    #[error("occupation string has {actual} electrons, expected {expected}")]
    PopulationMismatch { actual: usize, expected: usize },
    #[error("binomial coefficient C({n}, {k}) exceeds u128")]
    CountOverflow { n: usize, k: usize },
    #[error("rank {rank} is outside a space of {count} strings")]
    RankOutOfRange { rank: u128, count: u128 },
}

pub fn combination_count(n: usize, k: usize) -> Result<u128, CombinadicError> {
    if k > n {
        return Ok(0);
    }
    let reduced_k = k.min(n - k);
    let mut result = 1_u128;
    for step in 1..=reduced_k {
        let mut numerator = (n - reduced_k + step) as u128;
        let mut denominator = step as u128;

        let common_with_result = gcd(result, denominator);
        result /= common_with_result;
        denominator /= common_with_result;

        let common_with_numerator = gcd(numerator, denominator);
        numerator /= common_with_numerator;
        denominator /= common_with_numerator;
        debug_assert_eq!(denominator, 1);

        result = result
            .checked_mul(numerator)
            .ok_or(CombinadicError::CountOverflow { n, k })?;
    }
    Ok(result)
}

pub fn rank_occupation(bits: u64, norb: usize, nelec: usize) -> Result<u128, CombinadicError> {
    validate_space(norb, nelec)?;
    if norb < 64 && bits >> norb != 0 {
        return Err(CombinadicError::BitsOutsideSpace { norb });
    }
    let actual = bits.count_ones() as usize;
    if actual != nelec {
        return Err(CombinadicError::PopulationMismatch {
            actual,
            expected: nelec,
        });
    }

    let mut rank = 0_u128;
    let mut occupied_index = 1_usize;
    for orbital in 0..norb {
        if bits & (1_u64 << orbital) != 0 {
            rank = rank
                .checked_add(combination_count(orbital, occupied_index)?)
                .ok_or(CombinadicError::CountOverflow { n: norb, k: nelec })?;
            occupied_index += 1;
        }
    }
    Ok(rank)
}

pub fn unrank_occupation(rank: u128, norb: usize, nelec: usize) -> Result<u64, CombinadicError> {
    validate_space(norb, nelec)?;
    let count = combination_count(norb, nelec)?;
    if rank >= count {
        return Err(CombinadicError::RankOutOfRange { rank, count });
    }
    if nelec == 0 {
        return Ok(0);
    }

    let mut remainder = rank;
    let mut bits = 0_u64;
    let mut upper = norb;
    for occupied_index in (1..=nelec).rev() {
        let mut selected = None;
        for orbital in ((occupied_index - 1)..upper).rev() {
            let block = combination_count(orbital, occupied_index)?;
            if block <= remainder {
                selected = Some((orbital, block));
                break;
            }
        }
        let (orbital, block) =
            selected.expect("a valid combinadic rank always has a selectable orbital");
        bits |= 1_u64 << orbital;
        remainder -= block;
        upper = orbital;
    }
    debug_assert_eq!(remainder, 0);
    Ok(bits)
}

fn validate_space(norb: usize, nelec: usize) -> Result<(), CombinadicError> {
    if norb > 64 {
        return Err(CombinadicError::TooManyOrbitals(norb));
    }
    if nelec > norb {
        return Err(CombinadicError::PopulationOverflow { norb, nelec });
    }
    Ok(())
}

fn gcd(mut left: u128, mut right: u128) -> u128 {
    while right != 0 {
        let remainder = left % right;
        left = right;
        right = remainder;
    }
    left
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn exhaustive_small_spaces_match_numeric_lexical_order() {
        for norb in 0..=12 {
            for nelec in 0..=norb {
                let strings: Vec<_> = if norb == 0 {
                    vec![0]
                } else {
                    (0..(1_u64 << norb))
                        .filter(|bits| bits.count_ones() as usize == nelec)
                        .collect()
                };
                assert_eq!(
                    combination_count(norb, nelec).unwrap(),
                    strings.len() as u128
                );
                for (expected, &bits) in strings.iter().enumerate() {
                    assert_eq!(
                        rank_occupation(bits, norb, nelec).unwrap(),
                        expected as u128
                    );
                    assert_eq!(
                        unrank_occupation(expected as u128, norb, nelec).unwrap(),
                        bits
                    );
                }
            }
        }
    }

    #[test]
    fn handles_the_largest_balanced_u64_space() {
        assert_eq!(
            combination_count(64, 32).unwrap(),
            1_832_624_140_942_590_534
        );
        let last_rank = combination_count(64, 32).unwrap() - 1;
        let last = unrank_occupation(last_rank, 64, 32).unwrap();
        assert_eq!(last.count_ones(), 32);
        assert_eq!(rank_occupation(last, 64, 32).unwrap(), last_rank);
    }

    #[test]
    fn rejects_invalid_strings_and_ranks() {
        assert!(matches!(
            rank_occupation(0b1000, 3, 1),
            Err(CombinadicError::BitsOutsideSpace { norb: 3 })
        ));
        assert!(matches!(
            rank_occupation(0b0011, 3, 1),
            Err(CombinadicError::PopulationMismatch {
                actual: 2,
                expected: 1
            })
        ));
        assert!(matches!(
            unrank_occupation(3, 3, 1),
            Err(CombinadicError::RankOutOfRange { rank: 3, count: 3 })
        ));
        assert!(matches!(
            unrank_occupation(0, 65, 1),
            Err(CombinadicError::TooManyOrbitals(65))
        ));
    }

    #[test]
    fn reports_counts_that_exceed_u128() {
        assert!(matches!(
            combination_count(256, 128),
            Err(CombinadicError::CountOverflow { n: 256, k: 128 })
        ));
    }
}
