> Historical milestone; current status is batch4: nine checks, saved lint 9/0, inventory 141/75 suitable/3 exclusions/2 dedup/61 pending. See TASK_IMPLEMENTATION_BATCH4.md; no Docker calibration.

# Implementation batch 2: partial task, not complete

Six checks are now authored and native-verified. Survey remains 141 rows: 59 suitable, one grounded exclusion, one grounded deduplication, 80 pending. Ten-file static reviews were checked against byte-identical merged source and canonical official node names; original fixture/node inventories and limitations remain. The proposed cache dedup was rejected: its three-to-one duplicate/degenerate triangle regression is not present in test_base or existing drivers. No static review runtime was invented; unmeasured schema placeholder times remain marked runtime_measured=false.

Camera: 640x480 rays, all pixel identities and directions, K, ten framing poses and every projected corner. Analytic pinhole oracle strengthens the original index-extrema check. Original K/setter guards retained. Two binary64 ULP FOV perturbation changes directions/intrinsics/projections (not poses); wrong ray component rejected.

Boolean: explicit existing manifold3d 3.5.3; analytic overlapping cube A-B/B-A/intersection/union, each physical property and each signed-distance query; original ballA/ballB volume and bounds truths, three-sphere multioperation truths, and empty guards retained. The official adapter casts input vertices to float32: variant is two binary32 ULPs, not a vacuous binary64 perturbation. Properties and distances actually change smoothly; wrong individual distance rejected. Surface queries prevent volume-only aggregation from concealing boundary errors. Missing 100/101-sphere cascade and five-sphere wrapper cases remain outstanding.

No complete-survey claim: existing scaffold was created before complete scientific review. Neither file-level suitable decisions nor six checks establish exhaustive official coverage. See individual check READMEs for narrowed drivers and uncovered fixtures.

Native-only milestone: both stock validators unchanged. All tolerance values provisional; no Docker selfcheck, alternate-build floor, final human tolerance approval or charter. Existing Docker recipes do not provision manifold3d and MUST be resolved before Boolean can run in the container. No packages installed or new project requirements added here. CPU numeric threads=1, each real shell execution capped at 180 seconds. No source edits, Git commit/push/merge, GPU, SSH or host configuration changes.

Final verification: canonical saved checks.json has exactly six names and real LINT PASS reports six checks / zero warnings. Generated survey equals canonical state, 141 rows / 59 suitable. Public text scan found no private host paths or token-like markers; pinned tracked source remains unchanged; existing untracked material.mtl, material_0.png, shape and sphere.obj artifacts were preserved.

CLI lint uses the already-existing absolute-Git-Bash launcher tools/misc/20260917_run_canonical_lint_gitbash.py with PYTHONPATH pointing to pinned pipeline src. This launcher changes executable resolution only, not CLI/validator logic or subprocess results.
