# H₂O/cc-pVDZ All-Electron C₂ᵥ Full-CI Result

Date: 2026-07-30

## Result

| Quantity | Value |
|---|---:|
| Spatial orbitals | 24 |
| Correlated electrons | 10; no frozen core |
| Spin sector | `Nalpha = Nbeta = 5`, `MS2 = 0` |
| Point group and target irrep | C₂ᵥ, A1 (`ISYM=1`) |
| Determinants without point-group symmetry | 1,806,590,016 |
| Determinants in the A1 block | 451,681,246 |
| Rust Davidson FCI energy | **−76.243218589558566 Hartree** |
| Residual norm | **6.602 × 10⁻⁸** |
| Davidson iterations | 21 |
| Converged | `true` |
| Slurm job | `23008083`, `COMPLETED`, exit `0:0` |
| Wall time | 3:55:43 |
| Final Slurm step MaxRSS | 233,052,988 KiB = 222.257 GiB |

This is a converged full-configuration-interaction result for the finite
Hamiltonian stored in `FCIDUMP.c2v`. "Full" means that all determinants with
the required electron number, spin projection, and A1 spatial symmetry are
included. No active orbital or electron was removed.

Compared with the earlier reviewer-requested benchmark, the only intentional
change made to make the full solve feasible is use of the exact C₂ᵥ/A1
symmetry block. The geometry, spherical cc-pVDZ basis, all-electron treatment,
24 spatial orbitals, ten electrons, singlet `Nalpha=Nbeta=5` sector,
Hamiltonian convention, and residual acceptance threshold remain unchanged.

## Exact Hamiltonian and provenance

The molecular specification is:

- geometry in Angstrom:
  `O 0 0 0; H 0.967 0 0;`
  `H -0.2923916843556798 0.9217353757557798 0`;
- `R(O-H)=0.967 Å` and `angle(H-O-H)=107.6°`;
- Dunning cc-pVDZ basis with spherical functions;
- restricted-Hartree–Fock canonical molecular orbitals;
- all ten electrons in 24 spatial orbitals;
- C₂ᵥ orbital labels written in the one-based Molpro convention;
- A1 target sector, represented by `ISYM=1`.

PySCF 2.14.0 generated the RHF orbitals and FCIDUMP. The exact production
input has SHA-256
`b55d1bcb04f6889e5b5dff1336412c5f7118b5bdb8461d504764f2a704cd6255`.
Running `scripts/oracle/validate_ccpvdz_fci.py` with
`--fcidump-output` reproduces that file byte for byte. The script makes the
RHF tolerance used for the input orbitals explicit; its separate
high-precision RHF/MP2/CISD/CCSD/CCSD(T) calculation is an independent
cross-check rather than part of the Rust production solve.

Dunning's cc-pVDZ definition is from J. Chem. Phys. 90, 1007 (1989),
[DOI 10.1063/1.456153](https://doi.org/10.1063/1.456153). The water geometry
is the challenge's equilibrium convention. It must not be silently replaced
by another paper's equilibrium geometry when comparing total energies.

## Why using symmetry is scientifically valid

For a symmetry-preserving Hamiltonian, determinants belonging to different
irreducible representations do not couple. Diagonalizing the A1 block is
therefore not a truncation of the A1 wave function; it is the exact
block-diagonal form of the same finite-basis Hamiltonian. The water ground
state is the lowest singlet A1 state.

The reduction is nevertheless only fourfold:

```text
1,806,590,016 determinants without point-group symmetry
  451,681,246 determinants in the C2v A1 block
```

One A1 `f64` vector is still about 3.365 GiB, so matrix-free sigma application
and bounded Davidson storage remain necessary.

## Validation

### 1. Solver acceptance

The Slurm job finished normally with exit code `0:0`. The Rust solver reported
`converged=true`, and `6.602e-8` is below the requested residual tolerance
`1e-7`. The final stdout and stderr are committed without editing.

### 2. Independent same-input calculations

PySCF 2.14.0 independently gives:

| Method | Total energy (Hartree) | Relation to Rust FCI |
|---|---:|---:|
| RHF | −76.02579259490489 | FCI correlation = −0.217425994653681 |
| MP2 | −76.23013897797726 | above FCI |
| CISD | −76.23129965292942 | above FCI, as required variationally |
| CCSD | −76.23950008550794 | above FCI |
| CCSD(T) | −76.24257144581735 | 0.647144 mHartree above FCI |
| Rust FCI | **−76.243218589558566** | reference result |

The RHF value also agrees with the previously committed symmetry-free PySCF
RHF reference to approximately `1.1e-13` Hartree. Spatial symmetry changes
the representation of the Hamiltonian, not its RHF or ground-state energy.

### 3. Literature scale check and precision boundary

Al-Saidi, Zhang, and Krakauer report all-electron water/cc-pVDZ values
`E[CCSD(T)]=-76.241201` and `E[FCI]=-76.241860` Hartree in their Table I
([J. Chem. Phys. 124, 224101 (2006)](https://doi.org/10.1063/1.2200885)).
Their FCI value is sourced to Olsen *et al.*,
[J. Chem. Phys. 104, 8007 (1996)](https://doi.org/10.1063/1.471518).

Those absolute energies are **not** a direct oracle for this run because the
literature calculation uses a different equilibrium geometry. The useful
cross-check is the method gap: their FCI lies `0.659 mHartree` below
CCSD(T), while this calculation gives `0.647144 mHartree`. The difference
between the gaps is only `0.011856 mHartree`. Together with the exact
same-input RHF match, the expected method ordering, and the converged residual,
this supports accepting the result without misattributing extra digits to the
paper.

## HPC execution

The production calculation ran on SCNet North China Region 1 [Xiongheng],
partition `xhacnormalb`, node `a02r03n08`.

- Slurm allocated an exclusive 128-CPU node and 384 GiB.
- The solver used one task with 64 Rayon workers and 64 fixed sigma blocks.
- The deterministic parallel sigma workspace estimate was 215.378402 GiB.
- The final Slurm step MaxRSS was 222.257 GiB.
- No out-of-memory condition or nonzero exit occurred.

The `Maximum resident set size` printed by `/usr/bin/time -v` in stderr
belongs to the lightweight `srun` launcher. Slurm's step-level `MaxRSS` is the
authoritative memory measurement for the remote worker process.

## Scientific meaning

This calculation closes the gap between the earlier bounded cc-pVDZ kernel
benchmark and an actual converged all-electron FCI solve. It demonstrates
that:

1. the Rust FCIDUMP, symmetry, determinant, sigma, and Davidson paths remain
   coherent at a 451-million-dimensional scale;
2. deterministic parallel reduction preserves the energy scale expected from
   independent correlated methods;
3. C₂ᵥ symmetry changes feasibility by reducing vector storage fourfold
   without changing the finite-basis A1 ground-state problem;
4. the result is a reusable finite-basis benchmark, not a complete-basis,
   relativistic, or experimental energy.

## Archived evidence and reproduction

- `fixtures/h2o-ccpvdz-ae/FCIDUMP.c2v`: exact input.
- `fixtures/h2o-ccpvdz-ae/fci-c2v-xh5-result.json`: complete machine-readable
  result, checksums, resource record, and acceptance flags.
- `fixtures/h2o-ccpvdz-ae/pyscf-crosscheck.json`: independent RHF through
  CCSD(T) calculation.
- `fixtures/h2o-ccpvdz-ae/xh5/production-23008083.out`: unedited solver output.
- `fixtures/h2o-ccpvdz-ae/xh5/production-23008083.err`: unedited launcher and
  resource output.
- `hpc/xh5/production.slurm`: submitted configuration.

Regenerate the input and independent cross-check:

```bash
uv run --frozen python scripts/oracle/validate_ccpvdz_fci.py \
  --threads 1 \
  --output /tmp/pyscf-crosscheck.json \
  --fcidump-output /tmp/FCIDUMP.c2v

shasum -a 256 /tmp/FCIDUMP.c2v
```

Inspect the symmetry space:

```bash
cargo run --release --locked -- inspect \
  fixtures/h2o-ccpvdz-ae/FCIDUMP.c2v
```

The full production command and cluster requirements are recorded in
`hpc/xh5/production.slurm`.
