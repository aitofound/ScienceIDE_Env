# PySCF Level 0 Oracle

Python is used only to generate independent fixtures. The Rust executable and
test suite do not import PySCF.

From the repository root:

```bash
uv sync --locked
uv run --frozen python scripts/oracle/generate.py
```

The root `.python-version`, `pyproject.toml`, and `uv.lock` pin CPython
3.12.11, uv 0.11.32, PySCF 2.14.0, and all transitive dependencies. The
`scripts/oracle/requirements.txt` file remains only as a minimal compatibility
input for tools that cannot consume a uv project lock.

The generator writes `FCIDUMP`, `reference.json`, and `ao_reference.json` into
the selected fixture directories. Generate only the equilibrium H2 fixture
without touching existing fixtures with:

```bash
uv run --frozen python scripts/oracle/generate.py h2-equilibrium-sto3g
```

The equilibrium fixture uses an H-H distance of 0.7414 Å. The original
`h2-sto3g` regression fixture is retained separately at 1.4 Å. Re-running a
fixture should preserve its numerical fields and FCIDUMP SHA-256 checksum when
the pinned PySCF version and platform math stack are unchanged.

All atom strings in `generate.py` are interpreted as Angstrom because every
system carries `coordinate_unit="angstrom"` and the generator passes that
value explicitly to PySCF. PySCF/libcint converts coordinates internally to
Bohr. Total energies, orbital energies, nuclear repulsion, and FCIDUMP
integrals are in Hartree; overlap, orbital coefficients, CI coefficients, and
CC amplitudes are dimensionless.

Symmetry-enabled FCIDUMPs use one-based Molpro `ORBSYM` labels in the range
1–8 and a one-based `ISYM` target. When calling PySCF's
`pyscf.tools.fcidump.from_scf`, pass `molpro_orbsym=True`; PySCF's default
zero-based internal labels are a different convention. The Rust determinant
basis retains only alpha/beta string pairs whose direct-product irrep equals
`ISYM`.

The primary challenge fixture can be regenerated in isolation with:

```bash
uv run --frozen python scripts/oracle/generate.py h2o-631g-fc
```

It uses H2O/6-31G, `R(O-H)=0.967 Å`, `angle(H-O-H)=107.6°`, and freezes the
oxygen 1s orbital after RHF. Regeneration is an oracle audit, not a prerequisite
for the Rust calculations. Before replacing any committed primary fixture,
require the generated FCIDUMP checksum and every numerical reference field to
match or document and review the platform-dependent difference.

The stretched primary Hamiltonians can be regenerated together with:

```bash
uv run --frozen python scripts/oracle/generate.py \
  h2o-631g-fc-r1p5 h2o-631g-fc-r2p0
```

They uniformly scale both equilibrium O–H vectors by 1.5 and 2.0 while
preserving the 107.6° H–O–H angle. Their O–H distances are 1.4505 Å and
1.934 Å. PySCF CCSD is allowed up to 200 iterations because stretched-bond
references need more than the default 50 iterations to satisfy the
1 × 10⁻¹² Hartree convergence threshold.

The extended all-electron H2O/DZ fixture can be regenerated with:

```bash
uv run --frozen python scripts/oracle/generate.py h2o-dz-ae
```

This system uses the exact Bohr coordinates and printed O `(9s5p)/[4s2p]` and
H `(4s)/[2s]` contractions from Bauschlicher and Taylor 1986, not a similarly
named modern basis. Spatial symmetry is enabled and exported with
`molpro_orbsym=True`.

The frozen-core H2O/DZP input can be regenerated safely with:

```bash
uv run --frozen python scripts/oracle/generate.py h2o-dzp-fc
```

It adds the printed oxygen d polarization exponent 1.2 and hydrogen p
polarization exponent 0.8, then freezes the oxygen 1s orbital. The resulting
C₂ᵥ block contains 28,233,466 determinants. Fixture generation computes RHF,
MP2, CCSD, integrals, and checksums, but deliberately does not start PySCF FCI.
The size guard is explicit in `generation_metadata.json`; the literature FCI
anchor is stored separately and is not represented as a high-precision PySCF
result.

The review benchmark reference is deliberately RHF-only:

```bash
.venv/bin/python scripts/oracle/generate.py h2o-ccpvdz-ae
```

It uses all 10 electrons, 24 cc-pVDZ spatial orbitals, `symmetry=False`, and
records the fixed-`Nalpha=Nbeta=5` determinant dimension. It does not create
FCIDUMP, run FCI, CCSD, or MP2, or allocate a full CI vector.

The later symmetry-adapted production calculation has a separate, explicit
cross-check:

```bash
uv run --frozen python scripts/oracle/validate_ccpvdz_fci.py \
  --threads 1 \
  --output /tmp/pyscf-crosscheck.json \
  --fcidump-output /tmp/FCIDUMP.c2v

shasum -a 256 /tmp/FCIDUMP.c2v
```

The expected FCIDUMP SHA-256 is
`b55d1bcb04f6889e5b5dff1336412c5f7118b5bdb8461d504764f2a704cd6255`.
The script uses the exact 0.967 Å, 107.6° geometry, all ten electrons,
cc-pVDZ spherical functions, C₂ᵥ symmetry, and one-based Molpro orbital
labels. It also computes high-precision RHF, MP2, CISD, CCSD, and CCSD(T)
values for an independent energy-hierarchy check. It intentionally does not
start PySCF FCI.
