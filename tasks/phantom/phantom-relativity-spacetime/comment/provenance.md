# Provenance

- Repository: https://github.com/danieljprice/phantom
- Shared source: `code/phantom/` at `e53ea16758d2a261680506852a528f21270dca1c`
- License: GPL-3.0-or-later with LICENSE Section 7(c) naming condition
- Setup authority: `build/Makefile_setups`
- Documentation: `docs/user-guide/setups-list.rst`, `setups-best.rst`, `docs/examples/star.rst`, `phantomNR.rst`
- GR test: `src/tests/test_gr.f90`; upstream invocation `phantomtest gr ptmass`
- Binary-BH exclusion: pinned `data/binarybh/README` requires `cbwaves.txt`, but commit `e53ea16758d2a261680506852a528f21270dca1c` contains no such file; no task-owned substitute is used
- Staging: `scripts/stage-task-source.py --source phantom`; no task-local `code/`.
