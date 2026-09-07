# A gitignored cache under the leaf changes the fingerprint while git status is clean

**Symptom.** `sab.py status --task ... --ci-freshness` says the
self-validation record is fresh on the packager's machine and on the worker
that produced it, and CI says the same record is stale on the pushed tree.
`git status` is clean; `git status --ignored` shows the cause.

**What breaks.** Before skill 5.11.2, `contract_fingerprint`
(`scripts/_vendor/sciaccel_pipeline/util.py`) hashed every file under
`tests/`, `solution/`, `environment/` and `target/`, skipping only
`__pycache__`:

    files += [p for p in (leaf / d).rglob("*") if p.is_file() and "__pycache__" not in p.parts]

`Path.rglob("*")` does not skip dot-directories, so anything a tool drops in
the leaf is hashed too. Running a check's `official_runner.py` natively in its
own directory, the obvious way to time a gate before a container run, makes
pytest write `.pytest_cache/` beside `official_test.py`. The repository
`.gitignore` hides it from `git status`, and `rsync -a` copies it to the
worker, so the packager's tree, the worker's tree and the record agree with
each other while the committed tree, which is what CI checks out, does not.
The record is not wrong; it was taken on a tree with files the commit does not
carry.

**Measured** (`tasks/pyamg/relaxation-smoothing`, PR #437, head `078a15ddf`):

| tree | extra files under `tests/` | fingerprint |
|---|---|---|
| packager's worktree and the worker's sync dir | 12 (`.pytest_cache/` in 3 checks: `.gitignore`, `CACHEDIR.TAG`, `README.md`, `v/cache/nodeids`) | `4f768da954798b87` |
| the pushed commit, as CI checks it out | 0 | `8c1d39865898fe2e` |

`git status --porcelain` on the leaf was empty in both. Cost: one full
selfcheck rerun, about 60 minutes of worker time for three solves.

**How to detect it.** Before the final selfcheck, run
`git status --ignored --porcelain tasks/<id>/<slug>` and expect nothing;
`git ls-files tasks/<id>/<slug>/tests | wc -l` against
`find tasks/<id>/<slug>/tests -type f | wc -l` is the same check in two
numbers. Compute the fingerprint on a detached checkout of the commit you are
about to push and compare it with `comment/pipeline/self-validation.json`'s
`contract_fingerprint`; that is exactly what CI does. The usual producers are
`.pytest_cache/`, `.ruff_cache/`, `.hypothesis/`, `.ipynb_checkpoints/`,
`build/` and `*.egg-info/`.

**What to do in the check.** Clean the leaf before the rerun that produces the
record you will ship, and sync to the worker with `rsync -a --delete` from a
cleaned tree so the worker cannot hold a stale copy. Do not hand-edit
`contract_fingerprint`: `selfcheck` is its only writer, and a record edited to
match a tree it was not taken on is a false statement about what ran. Since skill
5.11.2 the fingerprint skips a fixed list of generated names
(`.pytest_cache/`, `__pycache__/`, `.ruff_cache/`, `.mypy_cache/`,
`.hypothesis/`, `.ipynb_checkpoints/`, `*.egg-info/`, `.DS_Store`),
`selfcheck` refuses to run while one is present and `status` lists them. The
list is fixed rather than "every dot-path" because git tracks legitimate
dotfiles, and a tracked dotfile under a contract directory is contract and
must move the fingerprint. Anything not on the list is still hashed, so the
detached-commit fingerprint stays the authority.

**Where measured.** aitofound/ScienceAccelBench issue #515, PR #437
(`tasks/pyamg/relaxation-smoothing`), 2026-09-06.
