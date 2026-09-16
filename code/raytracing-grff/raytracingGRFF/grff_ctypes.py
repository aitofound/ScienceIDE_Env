"""ctypes loader for GRFF ``PyGET_MW`` (``GRFF_DEM_Transfer.so``)."""

from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path

from numpy.ctypeslib import ndpointer

try:
    from GRFFcodes import initGET_MW as _init_from_grffcodes
except ImportError:
    _init_from_grffcodes = None

_libgomp_preloaded = False


def _preload_libgomp() -> None:
    """Load libgomp with RTLD_GLOBAL so GRFF .so can resolve OpenMP symbols."""
    global _libgomp_preloaded
    if _libgomp_preloaded or sys.platform != "linux":
        return

    candidates: list[Path] = []
    conda = os.environ.get("CONDA_PREFIX")
    if conda:
        candidates.append(Path(conda) / "lib" / "libgomp.so.1")
    candidates.extend(
        [
            Path("/usr/lib/x86_64-linux-gnu/libgomp.so.1"),
            Path("/usr/lib64/libgomp.so.1"),
        ]
    )
    mode = getattr(ctypes, "RTLD_GLOBAL", 0)
    for path in candidates:
        if path.is_file():
            ctypes.CDLL(str(path), mode=mode)
            _libgomp_preloaded = True
            return


def initGET_MW(libname: str | Path):
    """Return GRFF ``PyGET_MW`` function bound to ``libname``."""
    if _init_from_grffcodes is not None:
        return _init_from_grffcodes(str(libname))

    _preload_libgomp()
    _intp = ndpointer(dtype=ctypes.c_int32, flags="F")
    _doublep = ndpointer(dtype=ctypes.c_double, flags="F")
    libc_mw = ctypes.CDLL(str(libname))
    mwfunc = libc_mw.PyGET_MW
    mwfunc.argtypes = [_intp, _doublep, _doublep, _doublep, _doublep, _doublep, _doublep]
    mwfunc.restype = ctypes.c_int
    return mwfunc


def default_grff_lib_path(repo_root: Path | None = None) -> Path:
    """``GRFF/binaries/GRFF_DEM_Transfer.so`` relative to the dev tree (parent of GRFFradioSun)."""
    root = repo_root or Path(__file__).resolve().parents[1]
    return root.parent / "GRFF" / "binaries" / "GRFF_DEM_Transfer.so"
