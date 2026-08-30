_module_name = "sdf_helper"
_sdf_version = "2.2.0"

try:
    from ._version import __version__ as v
    from ._version import __commit_id__ as c
    from ._version import __commit_date__ as d
    __version__ = v
    __commit_id__ = c
    __commit_date__ = d
    del v, c, d
except ImportError:
    __version__ = "UNKNOWN"
    __commit_id__ = "UNKNOWN"
    __commit_date__ = "UNKNOWN"

try:
    import sdf
    got_sdf = True
except ImportError:
    got_sdf = False


def _error_message():
    if not got_sdf:
        raise ImportError(
            r"This module relies on the sdf python module \n"
            "which we were unable to load\n")
    raise ImportError(
        r"Your sdf python module is too old for this version "
        "of " + _module_name + ".\n"
        "Either upgrade to sdf python " + _sdf_version + " or newer, or "
        "downgrade " + _module_name)


def _check_validity():
    if not got_sdf or not hasattr(sdf, "__version__"):
        return _error_message()
    our_version = list(map(int, _sdf_version.split(".")))
    lib_version = list(map(int, sdf.__version__.split(".")))
    # Check that major version number matches, and minor version is at least
    # as big as that specified
    if our_version[0] != lib_version[0]:
        return _error_message()
    if our_version[1] > lib_version[1]:
        return _error_message()


_check_validity()

from .sdf_helper import *
from .read_nameval import *
from .visit_cmap import *
