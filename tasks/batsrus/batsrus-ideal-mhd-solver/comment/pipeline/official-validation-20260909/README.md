# PR466 official validation evidence (original run)

This directory publishes evidence for the already-completed official PR466 run at executed head `18fa1b04898c9ee375abb7df142c6141fa2c7d75` (`batsrus/ideal-mhd-solver`, task `batsrus-ideal-mhd-solver`). It is additive publication only: no source, solution, task input, Dockerfile, test, rubric, tolerance, acceptance bound, or scientific result was changed; no run/build/verifier/selfcheck was launched here.

## Official record

- Receipt status `done`; build, self-check, and wrapper exits `0`; collector reached `max_turns` only after valid reports, not a science failure.
- Official interval: `2026-09-09T21:01:51Z`–`2026-09-09T22:28:21Z`; allocation 8 CPU / 6 GB; suite 346.4 s, nominal build 1561.0 s, guidance budget 900 s, recorded `budget=within`.
- Self-validation result `passed`; reward `1.0`; 24/24 expected rows; 72/72 physical `run.ok` and `run.log` markers; 24/24 verifier PASS rows; 244/244 selected modest SHA matches.
- Runtime metadata SHA-256 (new active record): `4be02258cb4837550f63f9bdb0edf9c96925eef98e050b67e9b284ce657c2869`. Self-validation SHA-256 (new active record): `c78939f051cdaf9dd2b069caa12cf6cd76ee38c40c3894c973b69c91f3833f04`. Their exact source copies remain under `collection/remote/comment/pipeline/`.
- Immutable official executed fingerprint: `f9616377bc52ed9a2f8d80df890fe5c5e9ce842277a7f96fa60741fc57c79307`; preparation fingerprint: `095a1950eb6a2a147b5a2e17327b48e2870793e6f40d6640807e724a3977889f`.
- Source pin/tree/manifest: `9dfe746d48aa650b5209c3039f1a8676bc624899` / `2b7ceffe897cc21572847d7aed65d46eeddb4c40` / `452b3eb778286cd87a7fd100eeee3ccf46975c5dd52304c52d23145107837347`; packaging CLI skill `5.11.10`, current-main pin `25823f1add2a1fbd9e14bab4550a1a2428095721`.

The three official commands were `SAB_IC=nominal|variant|altbuild ./solution/solve.sh` and `./tests/test.sh`. Cache audit is exactly 72 `miss` + 72 `published`, 0 `hit`, 0 `disabled`; no speedup/reuse claim is made. The 96 large output payloads (278,985,852 bytes) remain remote inventory-only and are not copied here.

## Input equivalence and rubric boundary

The preserved official stage and the public PR tree have `156/156` non-rubric, non-`comment/pipeline` execution-input files matching byte-for-byte: zero missing files and zero content mismatches. Deterministic public inventory SHA-256 is `4ab68d1e5d4af0b5b097948373feff334d012f88219a48c481061324bdf9c1fe`. Docker roles are direct and unchanged: stage `environment/Dockerfile` -> public `environment/Dockerfile` (d23fa59f8ae005e811ce0ce4fc9ecdf9644292dfa5774afc17008885dca8722f), and stage `tests/Dockerfile` -> public `tests/Dockerfile` (97d0daf547346ce4c0b48f0283779666b9f7678bbb359839fbdad385f514be6a); no role swap. All 24 stage/public rubric files are byte-identical, so no generated rubric field was promoted and no scoring field was touched. See `current-inputs.json`.

The actual validators read only `rubric["comparison"]` (`atol`, `rtol`, `files`) for grading; the 24 validator hashes and `tests/test.sh` hash are in `reader-audit.json`. The pinned current-main writer is object `1d0c74983e7a191be4e1a8abafc9632d665d8a4`, writer SHA `8061a66664382215ad42069487284cc49082a3c81d708a3944b15fbdd699a4b2`, with role staging at lines 152-155, grading at 190-210, generated evidence writes at 303-364, and final fingerprint/record writes at 387-415. Thus this publication does not reinterpret evidence as criteria.

## Preservation and collection contents

Before active records were replaced, all four pre-publication canonical pipeline bytes were preserved under `comment/pipeline/history/pre-2026-09-09/`; old runtime SHA `684e32f00852fc67d36158a992fceb2208aa51fca911de31f483cddb34615749` and old self-validation SHA `e2204228d32eeec9efca087e04def5327ba5cf6039b8c078449b1223d218b710` remain there. `collection/` contains the immutable collection reports, identity-inputs mapping, audits, exact 244-file modest remote inventory, marker/log/manifest receipts, and a hash manifest. No raw GB output is present.

This evidence records the parent-accepted original official result only; it does not claim measured acceleration, alter scientific bounds, relabel executed timestamps/source, or replace human review.
