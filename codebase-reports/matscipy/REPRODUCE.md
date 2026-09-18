# Native reproduction

The source pin is `b9530066f61223b3e4879aeb7424642932baeaab` (matscipy 1.2.0). Run in Linux with Python 3.12, a C/C++ compiler and Git available. Build and test in a temporary copy so the vendored payload stays unchanged.

The commands below are portable equivalents of the recorded local runs. Original run records retain the measured times, outcomes and environment differences. A successful command does not replace the reported scientific failures or optional-dependency skips.

```bash
repo_root="$(git rev-parse --show-toplevel)"
run_dir="$(mktemp -d)"
cp -a "$repo_root/code/matscipy" "$run_dir/source"
python3.12 -m venv "$run_dir/venv"
export PATH="$run_dir/venv/bin:$PATH"
export OMP_NUM_THREADS=2 OPENBLAS_NUM_THREADS=2 MPLBACKEND=Agg
python -m pip install numpy==2.2.6 scipy==1.16.3 ase==3.26.0 \
  pytest pytest-subtests pytest-timeout sympy
cd "$run_dir/source"
printf 'Metadata-Version: 2.1\nName: matscipy\nVersion: 1.2.0\n' > PKG-INFO
python -m pip wheel . --no-deps --no-cache-dir -Ccompile-args=-j2 --wheel-dir ../wheels
python -m pip install --no-deps ../wheels/matscipy-1.2.0-*.whl
cd tests
python -m pytest --collect-only -q
timeout 180 python -m pytest test_atomic_strain.py test_elastic_moduli.py test_eam_calculator.py -q --timeout=170
```

The generated `PKG-INFO` supplies the release version after Git metadata has been removed. It belongs only in the temporary build copy. Source files and scientific algorithms are unchanged.

Run additional files separately with a 180-second process limit. Split `test_dislocation.py` into its five collected classes to keep each invocation below this limit. Keep the virtual environment first in `PATH`: the electrochemistry tests launch child Python scripts.

## Recorded limitations

- The original Birch-coefficient case in `tests/manybody/test_newmb.py` fails at its shipped finite-difference step of `1e-8`. Diagnostic steps from `1e-7` to `1e-5` pass the unchanged tolerance. These diagnostics do not replace the original result.
- Atomistica and QUIP are required for conditional test classes in `test_crack.py` and `test_fit_elastic_constants.py`. Without those dependencies, the files collect no tests.
- FEniCS, LAMMPS and visualization dependencies are needed by some optional paths. Preserve skipped-item reasons.
- Some parameter decks use legacy Python or external potential files. The example inventory records them as supplied input decks, not as successful runs.
- SciPy 1.18 removed the `sqrtm` argument used by the Cauchy–Born code. NumPy 2.5 rejects array-to-scalar conversions used by the EAM writer. The versions above resolved these measured interface failures.

Documentation notebooks can require interactive visualization. The four `docs/tools` notebook runs omitted only `%matplotlib inline`, used the Agg backend, and executed every computational cell. Their retained numeric arrays were finite; no separate numerical-reference comparison was performed.
