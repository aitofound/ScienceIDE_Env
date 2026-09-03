use std::fs::{self, File};
use std::io::{BufReader, BufWriter, Read, Write};
use std::path::{Path, PathBuf};

use super::DavidsonError;

pub(crate) trait VectorStore {
    fn dimension(&self) -> usize;
    fn len(&self) -> usize;
    fn generation(&self) -> u64;
    fn push(&mut self, vector: &[f64]) -> Result<(), DavidsonError>;
    fn load(&self, index: usize, output: &mut [f64]) -> Result<(), DavidsonError>;
    fn replace_all(&mut self, vectors: &[Vec<f64>]) -> Result<(), DavidsonError>;
}

#[derive(Debug)]
pub(crate) struct MemoryVectorStore {
    dimension: usize,
    vectors: Vec<Vec<f64>>,
    generation: u64,
}

impl MemoryVectorStore {
    pub(crate) fn new(dimension: usize) -> Self {
        Self {
            dimension,
            vectors: Vec::new(),
            generation: 0,
        }
    }
}

impl VectorStore for MemoryVectorStore {
    fn dimension(&self) -> usize {
        self.dimension
    }

    fn len(&self) -> usize {
        self.vectors.len()
    }

    fn generation(&self) -> u64 {
        self.generation
    }

    fn push(&mut self, vector: &[f64]) -> Result<(), DavidsonError> {
        validate_vector_slice(vector, self.dimension)?;
        self.vectors.push(vector.to_vec());
        Ok(())
    }

    fn load(&self, index: usize, output: &mut [f64]) -> Result<(), DavidsonError> {
        validate_vector_slice(output, self.dimension)?;
        let vector = self.vectors.get(index).ok_or(DavidsonError::VectorIndex {
            index,
            count: self.vectors.len(),
        })?;
        output.copy_from_slice(vector);
        Ok(())
    }

    fn replace_all(&mut self, vectors: &[Vec<f64>]) -> Result<(), DavidsonError> {
        for vector in vectors {
            validate_vector_slice(vector, self.dimension)?;
        }
        self.vectors = vectors.to_vec();
        self.generation += 1;
        Ok(())
    }
}

#[derive(Debug)]
pub(crate) struct DiskVectorStore {
    root: PathBuf,
    dimension: usize,
    len: usize,
    generation: u64,
}

impl DiskVectorStore {
    pub(crate) fn create(root: PathBuf, dimension: usize) -> Result<Self, DavidsonError> {
        create_directory(&root)?;
        let generation = 0;
        create_directory(&generation_path(&root, generation))?;
        Ok(Self {
            root,
            dimension,
            len: 0,
            generation,
        })
    }

    pub(crate) fn open(
        root: PathBuf,
        dimension: usize,
        generation: u64,
        len: usize,
    ) -> Result<Self, DavidsonError> {
        let store = Self {
            root,
            dimension,
            len,
            generation,
        };
        for index in 0..len {
            let mut scratch = vec![0.0; dimension];
            store.load(index, &mut scratch)?;
        }
        Ok(store)
    }

    fn vector_path(&self, index: usize) -> PathBuf {
        generation_path(&self.root, self.generation).join(format!("vector-{index:06}.bin"))
    }
}

impl VectorStore for DiskVectorStore {
    fn dimension(&self) -> usize {
        self.dimension
    }

    fn len(&self) -> usize {
        self.len
    }

    fn generation(&self) -> u64 {
        self.generation
    }

    fn push(&mut self, vector: &[f64]) -> Result<(), DavidsonError> {
        validate_vector_slice(vector, self.dimension)?;
        write_vector_atomic(&self.vector_path(self.len), vector)?;
        self.len += 1;
        Ok(())
    }

    fn load(&self, index: usize, output: &mut [f64]) -> Result<(), DavidsonError> {
        if index >= self.len {
            return Err(DavidsonError::VectorIndex {
                index,
                count: self.len,
            });
        }
        read_vector(&self.vector_path(index), self.dimension, output)
    }

    fn replace_all(&mut self, vectors: &[Vec<f64>]) -> Result<(), DavidsonError> {
        for vector in vectors {
            validate_vector_slice(vector, self.dimension)?;
        }
        let generation = self.generation + 1;
        let directory = generation_path(&self.root, generation);
        create_directory(&directory)?;
        for (index, vector) in vectors.iter().enumerate() {
            write_vector_atomic(&directory.join(format!("vector-{index:06}.bin")), vector)?;
        }
        self.generation = generation;
        self.len = vectors.len();
        Ok(())
    }
}

pub(crate) fn write_vector_atomic(path: &Path, vector: &[f64]) -> Result<(), DavidsonError> {
    if vector.iter().any(|value| !value.is_finite()) {
        return Err(DavidsonError::InvalidVectorFile {
            path: path.to_path_buf(),
            reason: "refusing to write non-finite values".to_string(),
        });
    }
    let temporary = path.with_extension("bin.tmp");
    let file = File::create(&temporary).map_err(|source| io_error(&temporary, source))?;
    let mut writer = BufWriter::new(file);
    for value in vector {
        writer
            .write_all(&value.to_le_bytes())
            .map_err(|source| io_error(&temporary, source))?;
    }
    writer
        .flush()
        .map_err(|source| io_error(&temporary, source))?;
    writer
        .get_ref()
        .sync_all()
        .map_err(|source| io_error(&temporary, source))?;
    fs::rename(&temporary, path).map_err(|source| io_error(path, source))?;
    Ok(())
}

pub(crate) fn read_vector(
    path: &Path,
    dimension: usize,
    output: &mut [f64],
) -> Result<(), DavidsonError> {
    validate_vector_slice(output, dimension)?;
    let expected_bytes = dimension.checked_mul(size_of::<f64>()).ok_or_else(|| {
        DavidsonError::InvalidVectorFile {
            path: path.to_path_buf(),
            reason: "expected byte length overflow".to_string(),
        }
    })?;
    let metadata = fs::metadata(path).map_err(|source| io_error(path, source))?;
    if metadata.len() != expected_bytes as u64 {
        return Err(DavidsonError::InvalidVectorFile {
            path: path.to_path_buf(),
            reason: format!(
                "file has {} bytes, expected {expected_bytes}",
                metadata.len()
            ),
        });
    }
    let file = File::open(path).map_err(|source| io_error(path, source))?;
    let mut reader = BufReader::new(file);
    let mut bytes = [0_u8; size_of::<f64>()];
    for value in output {
        reader
            .read_exact(&mut bytes)
            .map_err(|source| io_error(path, source))?;
        *value = f64::from_le_bytes(bytes);
        if !value.is_finite() {
            return Err(DavidsonError::InvalidVectorFile {
                path: path.to_path_buf(),
                reason: "file contains a non-finite value".to_string(),
            });
        }
    }
    Ok(())
}

fn validate_vector_slice(vector: &[f64], dimension: usize) -> Result<(), DavidsonError> {
    if vector.len() != dimension {
        return Err(DavidsonError::StoredVectorLength {
            actual: vector.len(),
            expected: dimension,
        });
    }
    if vector.iter().any(|value| !value.is_finite()) {
        return Err(DavidsonError::InvalidStoredVector);
    }
    Ok(())
}

fn generation_path(root: &Path, generation: u64) -> PathBuf {
    root.join(format!("generation-{generation:06}"))
}

fn create_directory(path: &Path) -> Result<(), DavidsonError> {
    fs::create_dir_all(path).map_err(|source| io_error(path, source))
}

pub(crate) fn io_error(path: &Path, source: std::io::Error) -> DavidsonError {
    DavidsonError::WorkspaceIo {
        path: path.to_path_buf(),
        source,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn unique_workspace(label: &str) -> PathBuf {
        std::env::temp_dir().join(format!(
            "ed-workbench-storage-{label}-{}",
            std::process::id()
        ))
    }

    #[test]
    fn memory_and_disk_stores_share_the_same_contract() {
        let workspace = unique_workspace("contract");
        if workspace.exists() {
            fs::remove_dir_all(&workspace).unwrap();
        }
        for mut store in [
            Box::new(MemoryVectorStore::new(3)) as Box<dyn VectorStore>,
            Box::new(DiskVectorStore::create(workspace.clone(), 3).unwrap())
                as Box<dyn VectorStore>,
        ] {
            store.push(&[1.0, 2.0, 3.0]).unwrap();
            store.push(&[4.0, 5.0, 6.0]).unwrap();
            let mut loaded = vec![0.0; 3];
            store.load(1, &mut loaded).unwrap();
            assert_eq!(loaded, vec![4.0, 5.0, 6.0]);
            store.replace_all(&[vec![7.0, 8.0, 9.0]]).unwrap();
            assert_eq!(store.len(), 1);
            store.load(0, &mut loaded).unwrap();
            assert_eq!(loaded, vec![7.0, 8.0, 9.0]);
        }
        fs::remove_dir_all(workspace).unwrap();
    }
}
