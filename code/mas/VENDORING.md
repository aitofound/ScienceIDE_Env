# Vendoring note: MAS (Magnetohydrodynamic Algorithm outside a Sphere)

Upstream: https://github.com/predsci/MAS, branch main, at commit 3641eb2a5b22e1c3030baa8b3566fe25ebfa01b3 (2026-09-05, "Cleaned up Fortran with suggestions from Fortitude").

Entry point that led here: the CCMC model page https://ccmc.gsfc.nasa.gov/models/CORHEL-CME~1/ . CORHEL-CME is a Predictive Science Inc. framework hosted at NASA's Community Coordinated Modeling Center whose web layer (PHP, bash, tcsh, the RBSL flux-rope interface, the diagnostic plotting) is not published. MAS is the MHD engine of both CORHEL and CORHEL-CME and is the part of that framework that is publicly released; it is vendored here on its own.

This tree is `git archive` of the pinned commit, unchanged: the 238 tracked files (src/, bin/, conf/, doc/, examples/, testsuite/, build.sh, load_mas_env.sh, LICENSE, README.md, .gitignore). No submodules, no LFS objects, no generated files; upstream's .gitignore already excludes the build products (src/Makefile, src/expmac, src/mas_cpp.f90, src/build.log, src/build.err, bin/mas).

Licence: Apache-2.0 (LICENSE; src/LICENSE_PCHIP covers the bundled PCHIP interpolation module). The upstream README asks anyone using the code for research to contact support@predsci.com; that is a request, not a licence term.
