# Vendoring note: SPHEREPACK 3.2

Upstream: https://github.com/NCAR/NCAR-Classic-Libraries-for-Geophysics, directory SpherePack/, at commit ab983f0b16170b018770ec3bf70102c55f3fa3d8.

Upstream ships this library as the tarball SpherePack/spherepack3.2.tar.gz (SHA-256 7f5497e77101a4423cee887294f873048f6ff6bc8d0e908c8a89ece677ee19ea) beside a LICENSE, an OVERVIEW and PDF manuals. This tree is the tarball's top-level directory spherepack3.2/ extracted here unchanged (tar xzf, nothing modified), plus the upstream LICENSE and OVERVIEW files. The tarball itself and the PDF manuals are not vendored; the HTML manual under doc/ is.

PDF manuals left out (bytes, upstream path):
    113252  SpherePack/SPHEREPACK 3.2 Abstract.pdf
    5078040  SpherePack/SPHEREPACK Documentation.pdf
    733471  SpherePack/SPHEREPACK Tutorial.pdf

Git does not track empty directories, so the tarball's empty lib/ and objs/ directories are absent here; the Makefile recreates them (mkdir -p) before building.
