# sciaccel-canary GUID epoch-mdpc-1a7c9e42
"""Independent re-implementation of EPOCH's split_domain auto-search.

Each function here is a line-for-line translation of the pinned Fortran
algorithm (not a generic factorization heuristic), so that an "auto" row can
be checked against the *exact* decomposition split_domain would choose,
rather than against a hand-picked expected answer. See
epoch<N>d/src/housekeeping/mpi_routines.F90 for the originals cited in
tests/contract.json's source_paths.
"""
from __future__ import annotations

NCELL_MIN = 3  # (png + 1) / 2 + 1 with the default png = 3 (constants.F90:552-575)


def uneven_split(nglobal: int, nproc: int) -> list[int]:
    """Replicate mpi_initialise's cell_x_max ladder for one axis.

    nx0 = nglobal // nproc; the first (nglobal % nproc) ranks get nx0 + 1
    cells, the rest get nx0 cells (mpi_routines.F90 mpi_initialise, the
    ``nxp`` branch). Returns the cumulative cell_?_max boundary for every
    rank, length == nproc.
    """
    nx0, nxp = divmod(nglobal, nproc)
    widths = [nx0 + 1] * nxp + [nx0] * (nproc - nxp)
    boundaries = []
    total = 0
    for w in widths:
        total += w
        boundaries.append(total)
    assert boundaries[-1] == nglobal
    return boundaries


def auto_split_1d(nglobal: int, nproc: int) -> int:
    """1-D split_domain: nprocx == nproc as long as the local width clears ncell_min."""
    n = nproc
    while n > 1:
        if nglobal // n >= NCELL_MIN:
            return n
        n -= 1
    return 1


def auto_split_2d(nx_global: int, ny_global: int, nproc: int) -> tuple[int, int]:
    """Exact translation of epoch2d split_domain's area=nxsplit+nysplit search."""
    n = nproc
    while n > 1:
        best = None
        minarea = nx_global + ny_global
        for ix in range(1, n + 1):
            if n % ix:
                continue
            iy = n // ix
            nxsplit = nx_global // ix
            nysplit = ny_global // iy
            if nxsplit < NCELL_MIN or nysplit < NCELL_MIN:
                continue
            area = nxsplit + nysplit
            if area < minarea:
                best = (ix, iy)
                minarea = area
        if best is not None:
            return best
        n -= 1
    return (1, 1)


def auto_split_3d(nx_global: int, ny_global: int, nz_global: int, nproc: int) -> tuple[int, int, int]:
    """Exact translation of epoch3d split_domain's true-surface-area search."""
    n = nproc
    while n > 1:
        best = None
        minarea = nx_global * ny_global + ny_global * nz_global + nz_global * nx_global
        for ix in range(1, n + 1):
            if n % ix:
                continue
            nprocyz = n // ix
            nxsplit = nx_global // ix
            if nxsplit < NCELL_MIN:
                continue
            for iy in range(1, nprocyz + 1):
                if nprocyz % iy:
                    continue
                iz = nprocyz // iy
                nysplit = ny_global // iy
                nzsplit = nz_global // iz
                if nysplit < NCELL_MIN or nzsplit < NCELL_MIN:
                    continue
                area = nxsplit * nysplit + nysplit * nzsplit + nzsplit * nxsplit
                if area < minarea:
                    best = (ix, iy, iz)
                    minarea = area
        if best is not None:
            return best
        n -= 1
    return (1, 1, 1)
