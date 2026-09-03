use std::collections::HashMap;
use std::num::{ParseFloatError, ParseIntError};

use thiserror::Error;

#[derive(Debug, Clone)]
pub struct Fcidump {
    pub norb: usize,
    pub nelec: usize,
    pub ms2: isize,
    pub orbsym: Vec<usize>,
    pub isym: usize,
    pub ecore: f64,
    one_body: HashMap<(usize, usize), f64>,
    two_body: HashMap<(usize, usize, usize, usize), f64>,
}

#[derive(Debug, Error)]
pub enum FcidumpError {
    #[error("FCIDUMP header is missing &FCI or &FCIDUMP")]
    MissingHeader,
    #[error("FCIDUMP header terminator (&END or /) is missing")]
    MissingHeaderEnd,
    #[error("required header field {0} is missing")]
    MissingField(&'static str),
    #[error("invalid integer in {field}: {source}")]
    InvalidInteger {
        field: &'static str,
        #[source]
        source: ParseIntError,
    },
    #[error("invalid floating-point value {value}: {source}")]
    InvalidFloat {
        value: String,
        #[source]
        source: ParseFloatError,
    },
    #[error("malformed integral record on line {line}: expected value and four indices")]
    MalformedRecord { line: usize },
    #[error("orbital index {index} on line {line} is outside 1..={norb}")]
    InvalidOrbitalIndex {
        line: usize,
        index: usize,
        norb: usize,
    },
    #[error("integral on line {line} is not finite")]
    NonFiniteIntegral { line: usize },
    #[error("ORBSYM contains {actual} labels, expected NORB={expected}")]
    OrbitalSymmetryLength { actual: usize, expected: usize },
    #[error("ORBSYM[{orbital}]={irrep} is outside the Molpro range 1..=8")]
    InvalidOrbitalSymmetry { orbital: usize, irrep: usize },
    #[error("ISYM={0} is outside the Molpro range 1..=8")]
    InvalidWavefunctionSymmetry(usize),
}

impl Fcidump {
    pub fn parse(input: &str) -> Result<Self, FcidumpError> {
        let upper = input.to_ascii_uppercase();
        let header_start = upper
            .find("&FCI")
            .or_else(|| upper.find("&FCIDUMP"))
            .ok_or(FcidumpError::MissingHeader)?;
        let after_start = &upper[header_start..];
        let relative_end = after_start
            .find("&END")
            .map(|index| index + 4)
            .or_else(|| after_start.find('/').map(|index| index + 1))
            .ok_or(FcidumpError::MissingHeaderEnd)?;
        let header_end = header_start + relative_end;
        let header = &input[header_start..header_end];

        let fields = parse_header_fields(header);
        let norb = parse_required_usize(&fields, "NORB")?;
        let nelec = parse_required_usize(&fields, "NELEC")?;
        let ms2 = parse_optional_isize(&fields, "MS2", 0)?;
        let isym = parse_optional_usize(&fields, "ISYM", 1)?;
        let orbsym = fields
            .get("ORBSYM")
            .map(|value| {
                value
                    .split_whitespace()
                    .filter(|part| !part.is_empty())
                    .map(|part| {
                        part.parse::<usize>()
                            .map_err(|source| FcidumpError::InvalidInteger {
                                field: "ORBSYM",
                                source,
                            })
                    })
                    .collect::<Result<Vec<_>, _>>()
            })
            .transpose()?
            .unwrap_or_else(|| vec![1; norb]);
        if orbsym.len() != norb {
            return Err(FcidumpError::OrbitalSymmetryLength {
                actual: orbsym.len(),
                expected: norb,
            });
        }
        if let Some((orbital, &irrep)) = orbsym
            .iter()
            .enumerate()
            .find(|(_, irrep)| !(1..=8).contains(*irrep))
        {
            return Err(FcidumpError::InvalidOrbitalSymmetry { orbital, irrep });
        }
        if !(1..=8).contains(&isym) {
            return Err(FcidumpError::InvalidWavefunctionSymmetry(isym));
        }

        let mut result = Self {
            norb,
            nelec,
            ms2,
            orbsym,
            isym,
            ecore: 0.0,
            one_body: HashMap::new(),
            two_body: HashMap::new(),
        };

        let data = &input[header_end..];
        let header_line_count = input[..header_end].lines().count();
        for (offset, raw_line) in data.lines().enumerate() {
            let line_number = header_line_count + offset + 1;
            let line = raw_line.trim();
            if line.is_empty() || line.starts_with('!') || line.starts_with('#') {
                continue;
            }
            let parts: Vec<_> = line.split_whitespace().collect();
            if parts.len() != 5 {
                return Err(FcidumpError::MalformedRecord { line: line_number });
            }
            let value_text = parts[0].replace(['D', 'd'], "E");
            let value = value_text
                .parse::<f64>()
                .map_err(|source| FcidumpError::InvalidFloat {
                    value: parts[0].to_string(),
                    source,
                })?;
            if !value.is_finite() {
                return Err(FcidumpError::NonFiniteIntegral { line: line_number });
            }
            let mut index = [0_usize; 4];
            for (slot, text) in index.iter_mut().zip(&parts[1..]) {
                *slot = text
                    .parse::<usize>()
                    .map_err(|source| FcidumpError::InvalidInteger {
                        field: "integral index",
                        source,
                    })?;
            }
            for &orbital in &index {
                if orbital > norb {
                    return Err(FcidumpError::InvalidOrbitalIndex {
                        line: line_number,
                        index: orbital,
                        norb,
                    });
                }
            }
            let [i, j, k, l] = index;
            if k != 0 {
                if i == 0 || j == 0 || l == 0 {
                    return Err(FcidumpError::MalformedRecord { line: line_number });
                }
                result
                    .two_body
                    .insert(canonical_eri(i - 1, j - 1, k - 1, l - 1), value);
            } else if j != 0 {
                if i == 0 || l != 0 {
                    return Err(FcidumpError::MalformedRecord { line: line_number });
                }
                result.one_body.insert(canonical_pair(i - 1, j - 1), value);
            } else {
                if i != 0 || l != 0 {
                    return Err(FcidumpError::MalformedRecord { line: line_number });
                }
                result.ecore = value;
            }
        }
        Ok(result)
    }

    pub fn h1(&self, p: usize, q: usize) -> f64 {
        self.one_body
            .get(&canonical_pair(p, q))
            .copied()
            .unwrap_or(0.0)
    }

    pub fn eri(&self, p: usize, q: usize, r: usize, s: usize) -> f64 {
        self.two_body
            .get(&canonical_eri(p, q, r, s))
            .copied()
            .unwrap_or(0.0)
    }

    pub fn one_body_record_count(&self) -> usize {
        self.one_body.len()
    }

    pub fn two_body_record_count(&self) -> usize {
        self.two_body.len()
    }
}

fn parse_header_fields(header: &str) -> HashMap<String, String> {
    let normalized = header
        // A Fortran namelist may use a newline instead of a comma between
        // fields. Treating it as another separator also preserves wrapped
        // ORBSYM lists because value-only segments extend the active field.
        .replace(['\n', '\r'], ",")
        .replace("&FCIDUMP", "")
        .replace("&fcidump", "")
        .replace("&FCI", "")
        .replace("&fci", "")
        .replace("&END", "")
        .replace("&end", "")
        .replace('/', "");
    let mut result = HashMap::new();
    let mut active_key: Option<String> = None;
    for segment in normalized.split(',') {
        let segment = segment.trim();
        if segment.is_empty() {
            continue;
        }
        if let Some((key, value)) = segment.split_once('=') {
            let key = key.trim().to_ascii_uppercase();
            result.insert(key.clone(), value.trim().to_string());
            active_key = Some(key);
        } else if let Some(key) = &active_key {
            let entry = result.entry(key.clone()).or_default();
            entry.push(' ');
            entry.push_str(segment);
        }
    }
    result
}

fn parse_required_usize(
    fields: &HashMap<String, String>,
    key: &'static str,
) -> Result<usize, FcidumpError> {
    fields
        .get(key)
        .ok_or(FcidumpError::MissingField(key))?
        .split_whitespace()
        .next()
        .ok_or(FcidumpError::MissingField(key))?
        .parse()
        .map_err(|source| FcidumpError::InvalidInteger { field: key, source })
}

fn parse_optional_usize(
    fields: &HashMap<String, String>,
    key: &'static str,
    default: usize,
) -> Result<usize, FcidumpError> {
    match fields.get(key) {
        Some(value) => value
            .split_whitespace()
            .next()
            .unwrap_or_default()
            .parse()
            .map_err(|source| FcidumpError::InvalidInteger { field: key, source }),
        None => Ok(default),
    }
}

fn parse_optional_isize(
    fields: &HashMap<String, String>,
    key: &'static str,
    default: isize,
) -> Result<isize, FcidumpError> {
    match fields.get(key) {
        Some(value) => value
            .split_whitespace()
            .next()
            .unwrap_or_default()
            .parse()
            .map_err(|source| FcidumpError::InvalidInteger { field: key, source }),
        None => Ok(default),
    }
}

fn canonical_pair(a: usize, b: usize) -> (usize, usize) {
    if a >= b { (a, b) } else { (b, a) }
}

fn canonical_eri(p: usize, q: usize, r: usize, s: usize) -> (usize, usize, usize, usize) {
    let left = canonical_pair(p, q);
    let right = canonical_pair(r, s);
    if left >= right {
        (left.0, left.1, right.0, right.1)
    } else {
        (right.0, right.1, left.0, left.1)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SAMPLE: &str = r#"&FCI NORB=2,NELEC=2,MS2=0,
 ORBSYM=1,1,
 ISYM=1,
 &END
 0.7D+00 1 1 1 1
 0.2 2 1 2 1
 -1.0 1 1 0 0
 0.5 0 0 0 0
"#;

    #[test]
    fn parses_header_records_and_fortran_exponents() {
        let dump = Fcidump::parse(SAMPLE).unwrap();
        assert_eq!(dump.norb, 2);
        assert_eq!(dump.nelec, 2);
        assert_eq!(dump.ms2, 0);
        assert_eq!(dump.orbsym, vec![1, 1]);
        assert_eq!(dump.h1(0, 0), -1.0);
        assert_eq!(dump.eri(0, 0, 0, 0), 0.7);
        assert_eq!(dump.ecore, 0.5);
    }

    #[test]
    fn restores_eightfold_eri_symmetry() {
        let dump = Fcidump::parse(SAMPLE).unwrap();
        let expected = dump.eri(1, 0, 1, 0);
        assert_eq!(dump.eri(0, 1, 1, 0), expected);
        assert_eq!(dump.eri(1, 0, 0, 1), expected);
        assert_eq!(dump.eri(0, 1, 0, 1), expected);
    }

    #[test]
    fn rejects_out_of_range_indices() {
        let input = "&FCI NORB=2,NELEC=2,MS2=0,&END\n1.0 3 1 0 0\n";
        assert!(matches!(
            Fcidump::parse(input),
            Err(FcidumpError::InvalidOrbitalIndex { .. })
        ));
    }

    #[test]
    fn parses_pyscf_header_without_a_comma_before_isym() {
        let input = "&FCI NORB=7,NELEC=10,MS2=0,\n\
                     ORBSYM=1,1,3,1,2,1,3\n\
                     ISYM=1,\n\
                     &END\n\
                     0.0 0 0 0 0\n";
        let dump = Fcidump::parse(input).unwrap();
        assert_eq!(dump.orbsym, vec![1, 1, 3, 1, 2, 1, 3]);
        assert_eq!(dump.isym, 1);
    }
}
