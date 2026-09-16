# Vendoring note: FFTPACK 5.1

Upstream: https://github.com/NCAR/NCAR-Classic-Libraries-for-Geophysics, directory FFTPack/, at commit ab983f0b16170b018770ec3bf70102c55f3fa3d8.

Upstream ships this library as the tarball FFTPack/fftpack5.1.tar.gz (SHA-256 6049d8f7242b1f41f2cd1dd38fe7db802acf14186dbb0479a28792545cd49ea5) beside a LICENSE, an OVERVIEW and PDF manuals. This tree is the tarball's top-level directory fftpack5.1/ extracted here unchanged (tar xzf, nothing modified), plus the upstream LICENSE and OVERVIEW files. The tarball itself and the PDF manuals are not vendored; the HTML manual under doc/ is.

PDF manuals left out (bytes, upstream path):
    115903  FFTPack/FFTPACK Abstract.pdf
    973603  FFTPack/FFTPACK5 DOCUMENTATION.pdf
    194254  FFTPack/FFTPACK5 Tutorial.pdf
    194254  FFTPack/FFTPack_Tutorial.pdf

Git does not track empty directories, so the tarball's empty lib/ and objs/ directories are absent here; the Makefile recreates them (mkdir -p) before building.
