# hmmsearch-profile-search: authoring notes

This directory is hidden at Harbor runtime and is not part of the contract.
`comment/pipeline/` is written only by the CLI (module entry, the Step 1.2 build-and-run record, test survey,
self-validation and runtime records). This file is the human-readable story.

## Module

HMMER 3.4's complete single-node protein `hmmsearch`: the MSV, bias, Viterbi and Forward filters, the
Forward/Backward scoring, posterior domain definition with its stochastic region resolution, null2
correction, thresholding and the tabular reporting of targets and domains. The module owns
`src/hmmsearch.c`, the `p7_pipeline`/`p7_domaindef`/`p7_tophits` orchestration and the SSE, NEON and VMX
SIMD kernels under `src/impl_*`; the Easel miniapps (`esl-shuffle`, `esl-reformat`) and `hmmemit` of the
same pinned tree are used only to build inputs at fixed seeds.

Twenty-seven checks, one per distinct official exercise: 17 of the 32 `hmmsearch` lines of
`testsuite/testsuite.sqc`, the 9 integration regression scripts of `testsuite/` that run `hmmsearch`
(i2, i4, i8, i9, i10, i13, i17, i21, i23), and the 2 `hmmsearch` walkthroughs of the user guide. The
survey (`comment/pipeline/test-survey.json`, 60 rows) records every line with its verdict:

- **Covered by another check (15 sqc lines, each measured byte-identical in its graded tables).** Ten
  lines change only the human-readable report or restate a default (`-o`, `--tblout`, `--domtblout`,
  `--pfamtblout`, `--acc`, `--noali`, `--notextw`, `--textw`, `--seed 42`, `--tformat fasta`) and are
  covered by `search-default`; `-E 0.01` and `--domE 0.01` remove no row because a hit that passes the
  default filters has P below 1e-5 and, at Z = 293, E below 0.003 (the reporting thresholds are exercised
  by `-T 20` and `--domT 20` instead); `--incE 0.01` and `--incdomE 0.01` restate the default inclusion
  E-values and are covered by `search-alignment`; `--incdomT 20` selects the same included domains as
  `--incT 20` on this database and is covered by `search-include-score`.
- **Excluded (18).** The 16 C unit drivers (`*_utest`, assertion-only on private structures), `-h`, and
  the fixed-bug regression `h39` (a one-node model against two one-residue sequences: no hit can exist,
  and an empty table is bookkeeping, not an observable). Their `why` fields carry the reason.

**What the sqc lines actually exercise, and how the checks make them grade something.** The upstream
option lines all run `globins4.hmm` against `RNDDB`, two random 100-residue sequences with no hit, so
upstream proves only that each option parses. The checks keep the option and the query and search the
*mixed database*, built by `run.sh` from the pinned tree at fixed seeds: the 45 tutorial globins, HBB_HUMAN
and 7LESS_DROME (true hits), the same 45 globins regionally shuffled in 4-residue windows with
`esl-shuffle --seed 42 -w 4` (local composition kept, alignment destroyed: 27 borderline hits whose
scores and E-values sit around the 20-bit and filter thresholds) and 200 random decoys. On that database
every kept option changes the graded rows in a measured way (row counts in each README). The three
cutoff lines use `testsuite/Caudal_act.hmm`, whose GA, TC and NC differ (25.0, 46.2, -6.3 bits), against
100 sequences sampled from it, because `fn3.hmm`'s near-equal cutoffs select identical rows on every
database in the tree. The regression scripts are reproduced with their own inputs (the i8, i10 and i4
sequences, the i13 `hmmemit` sampling, the i21 two-copy query, the i17 standard-input read, the i2 three
seeds); i23's rejection of a gapped FASTA is recorded as a discrete outcome next to a real search, since
the upstream test has no scientific output.

Deliberately outside the module: MPI and `hmmpgmd` daemon modes, nucleotide search (`nhmmer`, FM-index),
`hmmscan`/`phmmer`/`jackhmmer`, and any external Pfam or UniProt database.

## Build

Every check compiles the pinned source at solve time; nothing is prebuilt in either image. The recipe is
upstream's own `./configure --disable-mpi && make -j$SAB_THREADS` (default `-O3`, SIMD kernels selected by
configure); the altbuild adds `CFLAGS=-O0` and nothing else. **Reuse within a run:** the first `run.sh` of a
container builds into `$SAB_BUILD_ROOT/nominal` (default `/tmp/sciaccel-hmmer-build/nominal`, moved into
place only after `make` succeeds) and every later check of the same container finds the binaries there
and reports `SAB_BUILD_SECONDS=0`; the altbuild has its own `altbuild/` tree. `tests/test.sh produce` runs
its checks sequentially inside one container, so no cross-check race protocol is needed; the resource-aware
`solve.sh` of skill 5.17 may run several containers at once, sharded by the rubric's `configuration`
text, and each shard pays the compile once. `SAB_THREADS` (default 4, the declared cpus) is both the
`make -j` and the `hmmsearch --cpu` count, fixed and never read from the host; the worker-thread count
does not change any graded number (hits are merged and sorted after the search), only the wall time.
`SAB_DECOYS` (200) and `SAB_MAX_SEQS` (20000) are the runtime knobs. Natively (arm64, 4 threads, 2026-09-16) every check's search takes under 3 s; the x86_64 record's
build and run seconds are in `comment/pipeline/runtime-metadata.json`.

## Tolerances

The bounds are the print precision of `p7_tophits.c`, not packager choices: bit scores and biases print
`%6.1f`/`%5.1f` (lines 1670 and 1755), so `atol` 0.11 is one printed unit; E-values print `%9.2g`, so
`rtol` 0.11 is one unit in the second significant figure; the mean posterior accuracy prints `%4.2f`, so
its bound is 0.011; coordinates, domain counts, identities, accessions, descriptions and aligned residues
are exact, and rows are keyed by target, query and domain number plus an occurrence index (duplicate
names in i10 and i21 grade row for row). The previous revision applied `atol` 0.11 to the E-value
columns as well, which cannot fail for any E-value below 0.1; that is the one tolerance change of this
revision. Floors: the `-O0` altbuild graded identical to nominal in every value on every check natively
(arm64) and in the shipped record (`evidence.floor` per rubric); the nominal-versus-variant spread of the
shipped record is in `evidence.self_validation_spread`. Variant: every check perturbs every match-emission
value of the profile (+0.06 nats on ten residues, -0.02 on the other ten), because the previous
single-field 0.005 perturbation stayed below the 0.1-bit print resolution on 12 of 43 checks
(`references/pitfalls/output-precision-floors-the-bound.md`); natively the variant moves every graded
stream of every check (2 to 9 bounds where the row set is unchanged, a different row set on the
threshold checks).

## Blind spots

The altbuild floor is zero on this codebase (the Forward sums are done in single precision with the
same association order at `-O0` and `-O3`, and the print precision hides anything below 0.05 bits), so
the record shows the bound is achievable but not how much of it a different SIMD kernel would consume;
the two architectures of the records (the author's arm64 NEON build, the curator's x86_64 SSE build)
agree value for value, which is the stronger evidence. The stochastic domain-region resolution is graded
at three seeds on one check only (i2). The human-readable report (`output.txt`) is produced but not
graded; every scientific quantity in it is also in the two tables or the `-A` alignment. Nucleotide
search, MPI and the daemon are outside the module.
