# Vendoring note: MUDPACK 5.0.1

Upstream: https://github.com/NCAR/NCAR-Classic-Libraries-for-Geophysics, directory MudPack/, at commit ab983f0b16170b018770ec3bf70102c55f3fa3d8.

Upstream ships this library as the tarball MudPack/mudpack5.0.1.tar.gz (SHA-256 ecb0cecea1486d2428255059c9835cd9855b3a0be1c4bb0b0f5de2ce6a230def) beside a LICENSE, an OVERVIEW and PDF manuals. This tree is the tarball's top-level directory mudpack5.0.1/ extracted here unchanged (tar xzf, nothing modified), plus the upstream LICENSE and OVERVIEW files. The tarball itself and the PDF manuals are not vendored; the HTML manual under doc/ is.

PDF manuals left out (bytes, upstream path):
    75709  MudPack/MUDPACK Abstract.pdf
    6564121  MudPack/MUDPACK Documentation.pdf

Git does not track empty directories, so the tarball's empty lib/ and objs/ directories are absent here; the Makefile recreates them (mkdir -p) before building.
