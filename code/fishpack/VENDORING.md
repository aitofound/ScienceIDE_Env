# Vendoring note: FISHPACK 4.1

Upstream: https://github.com/NCAR/NCAR-Classic-Libraries-for-Geophysics, directory FishPack/, at commit ab983f0b16170b018770ec3bf70102c55f3fa3d8.

Upstream ships this library as the tarball FishPack/fishpack4.1.tar.gz (SHA-256 42222ed389b18336345867c7fa62f5ff03660950f527d76e9b9e8285d9874367) beside a LICENSE, an OVERVIEW and PDF manuals. This tree is the tarball's top-level directory fishpack4.1/ extracted here unchanged (tar xzf, nothing modified), plus the upstream LICENSE and OVERVIEW files. The tarball itself and the PDF manuals are not vendored; the HTML manual under doc/ is.

PDF manuals left out (bytes, upstream path):
    75859  FishPack/FISHPACK Abstract.pdf
    1961701  FishPack/FISHPACK Documentation.pdf
    4567775  FishPack/Technical Note.pdf

Git does not track empty directories, so the tarball's empty lib/ and objs/ directories are absent here; the Makefile recreates them (mkdir -p) before building.
