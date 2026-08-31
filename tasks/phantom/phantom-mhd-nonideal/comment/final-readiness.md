# Final receipt finisher report

Status: **PASS**

Fresh verifier: `em-46c8`

Created: `2026-08-31T05:30:06.480820Z`

## Verdict

Every assigned final-receipt acceptance item is green. This verification was mechanical and read-only: it did **not** rerun Docker, either bare solve, the two-root verifier, or the separate self-test verifier. The only mutation is this unique additive report directory.

## Gate-3 evidence

- Both `gate3-run1-reference` and `gate3-run2-candidate` have `solve_exit=0` and `tee_exit=0` in their bare-solve metadata.
- Each solve/container transcript has exactly nine per-check `RUN [` lines, nine matching `OK [` lines in the exact `checks-order.json` order, and one terminal `hidden reference production completed for all 9 official sibling checks.` line.
- Each root has exactly nine expected result directories, nine `result.json` receipts, nine `canonical.log` files, and one `oracle-manifest.json`; every result says process exit 0, zero failures, positive equal test/pass counts, the exact source pin, and a transcript SHA-256 matching its canonical log.
- Run IDs, container IDs, requested image tags, and roots are distinct. The roots neither overlap nor contain one another. Each has 21 retained files with no symlinks or intra-root inode aliases; the two roots share zero artifact inodes.
- Both manifests pin `e53ea16758d2a261680506852a528f21270dca1c` and image `sha256:7c9fbd0c30a4be6bb4bdfb309b0a56db8bef3f4095355826e8bed2e22d7e00e2`. Retained image inspect lists both requested tags. Retained run-1 container inspect agrees with its manifest and exit 0.
- `code/phantom` is clean and exactly Git tree `ae40f54661feb12f0550092fd2188e5738b7b955`.

## Separate verifier receipt

The em-aab3 artifacts are byte-coherent: log JSON equals reward JSON; metadata records verifier and tee exit 0; all nine check verdicts are passed; aggregate fields are `passed=9`, `total=9`, `reward=1.0`, `status=passed`, `self_test_mode=true`, and `self_test_ok=true`.

## Package and projections

The live check scope is exactly the final nine rows, with 27 direct check JSON files, 29 active JSON files, three active Python files, and three shell entrances. All 89 existing task-local JSON documents parsed strictly, all five task-local Python sources parsed as AST, and all three shell files passed `bash -n`.

The current generator contract was read rather than guessed: `registry/index.yaml` projects every filesystem task, while `registry.json` excludes `draft` and `retired`. Thus the coherent result is exactly one Phantom index row (`draft`, 9 checks, 1 target, 9 cells, LOC 19000) and **zero** Phantom runnable rows. Draft CPU readiness remains task-local and correctly adds no official `registry/runs.yaml` or operator-overlay row.

All real static gates exited 0: leaf Harbor validation, all-leaf Harbor validation, `node scripts/gen-index.mjs --check`, and documented `BASE_REF=origin/main npm run check` (`ok — 16 tasks, no violations`). `git diff --check` also passed.

Tracked changes are exactly the two generated projections. All untracked regular content is under this Phantom task. Root `node_modules` is the pre-existing dependency symlink; the one ignored task cache was born during em-aab3's prior self-test and was not created or modified by this verifier.

## Artifacts

- `commands.log` — commands, exit codes, concise outputs, and two transparently recorded corrected diagnostic assumptions.
- `receipt.json` — machine-readable acceptance receipt.
- `evidence-hashes.sha256` — SHA-256 for 62 exact predecessor/package/contract evidence files.
- `final-report.md` — this report.
