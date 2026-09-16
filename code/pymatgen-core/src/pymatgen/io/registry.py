"""Plugin registry for `Structure` and `Molecule` file-format handlers.

`IStructure.from_file`/`from_str`/`to` (and the `IMolecule` counterparts)
dispatch through this registry instead of carrying a hard-coded if/elif chain.
Each `pymatgen.io.<format>` module owns its own read/write glue and registers
it here at module-import time:

    from pymatgen.io.registry import StructureFormat, register_structure_format

    def _read_str(s, *, primitive=False, **kwargs):
        ...

    register_structure_format(StructureFormat(
        name="cif",
        patterns=("*.cif*", "*.mcif*"),
        read_str=_read_str,
        write_str=_write_str,
        write_file=_write_file,
    ))

Built-in formats are discovered lazily via `_BUILTIN_STRUCTURE_MODULES` and
`_BUILTIN_MOLECULE_MODULES`: the first lookup of a format name (or matching
filename) triggers `importlib.import_module` of the io module, which performs
its registration as a side effect. External packages can additionally declare
the entry-point groups `pymatgen.io.structure_formats` and
`pymatgen.io.molecule_formats` in their `pyproject.toml`.
"""

from __future__ import annotations

import importlib
import inspect
import os
import warnings
from dataclasses import dataclass, field
from fnmatch import fnmatch
from typing import TYPE_CHECKING

from monty.io import zopen

if TYPE_CHECKING:
    from collections.abc import Callable

    from pymatgen.core.structure import IMolecule, IStructure


__all__ = (
    "MoleculeFormat",
    "StructureFormat",
    "filter_kwargs",
    "get_molecule_format",
    "get_structure_format",
    "list_molecule_formats",
    "list_structure_formats",
    "register_molecule_format",
    "register_structure_format",
    "unregister_molecule_format",
    "unregister_structure_format",
)


# ---------------------------------------------------------------------------
# Dataclasses
# ---------------------------------------------------------------------------


@dataclass(frozen=True)
class _IOFormat:
    """Common base for `StructureFormat` and `MoleculeFormat`.

    A handler describes how a single I/O format reads and writes its target
    object. All callables receive the user's `**kwargs` after the registry has
    optionally filtered them against the callable's own signature (see
    `filter_kwargs`); plugins typically take `**kwargs` and forward to the
    underlying parser they wrap.

    Attributes:
        name: Lowercase format identifier used as the `fmt` argument.
        patterns: Filename glob patterns used to infer the format when only a
            filename is given. Matching is case-insensitive against the
            basename when `case_insensitive` is True.
        read_str: `(input_string, **kwargs) -> Structure/Molecule`. Optional;
            if absent, the format can only be read from a file.
        read_file: `(filename, **kwargs) -> Structure/Molecule`. Optional; if
            absent, the registry reads the file as text and delegates to
            `read_str`.
        write_str: `(obj, **kwargs) -> str`. Optional; if absent, the format
            cannot be serialized to a string and `write_file` must be present.
        write_file: `(obj, filename, **kwargs) -> None`. Optional; if absent,
            the registry writes `write_str(obj, **kwargs)` to the filename.
        binary: Set True for binary-only formats (e.g. NetCDF). Disables the
            text-mode fallback that delegates `read_file` → `read_str`.
        case_insensitive: If True (the default), filename glob matching uses
            the lowercased basename.
    """

    name: str
    patterns: tuple[str, ...] = ()
    read_str: Callable[..., object] | None = None
    read_file: Callable[..., object] | None = None
    write_str: Callable[..., str] | None = None
    write_file: Callable[..., None] | None = None
    binary: bool = False
    case_insensitive: bool = True
    extra: dict = field(default_factory=dict)


@dataclass(frozen=True)
class StructureFormat(_IOFormat):
    """Plugin descriptor for a `Structure` (periodic) I/O format."""


@dataclass(frozen=True)
class MoleculeFormat(_IOFormat):
    """Plugin descriptor for a `Molecule` (non-periodic) I/O format."""


# ---------------------------------------------------------------------------
# Built-in module map and pattern priority
# ---------------------------------------------------------------------------

# Format name -> dotted module path. The module's import triggers registration
# of its format(s) as a side effect. This is just the discovery layer; the
# canonical handler lives in the imported module.
_BUILTIN_STRUCTURE_MODULES: dict[str, str] = {
    "cif": "pymatgen.io.cif",
    "mcif": "pymatgen.io.cif",
    "poscar": "pymatgen.io.vasp",
    "chgcar": "pymatgen.io.vasp",
    "locpot": "pymatgen.io.vasp",
    "vasprun": "pymatgen.io.vasp",
    "cssr": "pymatgen.io.cssr",
    "xsf": "pymatgen.io.xcrysden",
    "mcsqs": "pymatgen.io.atat",
    "prismatic": "pymatgen.io.prismatic",
    "res": "pymatgen.io.res",
    "pwmat": "pymatgen.io.pwmat",
    "exciting": "pymatgen.io.exciting.inputs",
    "lmto": "pymatgen.io.lmto",
    "abinit-nc": "pymatgen.io.abinit.netcdf",
    # External namespace packages — their pymatgen.io.<name> modules register
    # themselves if installed. We list them here so the lookup triggers the
    # import; if not installed, ImportError surfaces to the caller.
    "aims": "pymatgen.io.aims.inputs",
    "fleur": "pymatgen.io.fleur",
    "fleur-inpgen": "pymatgen.io.fleur",
}

_BUILTIN_MOLECULE_MODULES: dict[str, str] = {
    "xyz": "pymatgen.io.xyz",
    "gaussian": "pymatgen.io.gaussian",
    "gaussian-out": "pymatgen.io.gaussian",
    # Common short-form aliases registered by pymatgen.io.gaussian.
    "gjf": "pymatgen.io.gaussian",
    "g03": "pymatgen.io.gaussian",
    "g09": "pymatgen.io.gaussian",
    "com": "pymatgen.io.gaussian",
    "inp": "pymatgen.io.gaussian",
    "babel": "pymatgen.io.babel",
    # Babel-supported molecule formats.
    "pdb": "pymatgen.io.babel",
    "mol": "pymatgen.io.babel",
    "mdl": "pymatgen.io.babel",
    "sdf": "pymatgen.io.babel",
    "sd": "pymatgen.io.babel",
    "ml2": "pymatgen.io.babel",
    "sy2": "pymatgen.io.babel",
    "mol2": "pymatgen.io.babel",
    "cml": "pymatgen.io.babel",
    "mrv": "pymatgen.io.babel",
}

# Ordered (format_name, glob) pairs used when only a filename is given. Order
# preserves the resolution priority of the legacy if/elif chain — earlier
# entries win.
_BUILTIN_STRUCTURE_FILENAME_PRIORITY: tuple[tuple[str, str], ...] = (
    ("cif", "*.cif*"),
    ("cif", "*.mcif*"),
    ("poscar", "*POSCAR*"),
    ("poscar", "*CONTCAR*"),
    ("poscar", "*.vasp"),
    ("chgcar", "CHGCAR*"),
    ("chgcar", "LOCPOT*"),
    ("vasprun", "vasprun*.xml*"),
    ("cssr", "*.cssr*"),
    ("json", "*.json*"),
    ("json", "*.mson*"),
    ("yaml", "*.yaml*"),
    ("yaml", "*.yml*"),
    ("xsf", "*.xsf*"),
    ("exciting", "input*.xml"),
    ("mcsqs", "*rndstr.in*"),
    ("mcsqs", "*lat.in*"),
    ("mcsqs", "*bestsqs*"),
    ("lmto", "CTRL*"),
    ("aims", "geometry.in*"),
    # Fleur's "*.in*" is intentionally generic and listed last so that the
    # narrower aims / mcsqs patterns get first dibs.
    ("fleur-inpgen", "inp*.xml"),
    ("fleur-inpgen", "*.in*"),
    ("fleur-inpgen", "inp_*"),
    ("res", "*.res"),
    ("pwmat", "*.config*"),
    ("pwmat", "*.pwmat*"),
    ("abinit-nc", "*.nc"),
    # For `to`: "prismatic" is dispatched by fmt= today and would match here
    # only if a user names their file with "prismatic" in it.
    ("prismatic", "*prismatic*"),
)

_BUILTIN_MOLECULE_FILENAME_PRIORITY: tuple[tuple[str, str], ...] = (
    ("xyz", "*.xyz*"),
    ("gaussian", "*.gjf*"),
    ("gaussian", "*.g03*"),
    ("gaussian", "*.g09*"),
    ("gaussian", "*.com*"),
    ("gaussian", "*.inp*"),
    ("gaussian-out", "*.out*"),
    ("gaussian-out", "*.lis*"),
    ("gaussian-out", "*.log*"),
    ("json", "*.json*"),
    ("json", "*.mson*"),
    ("yaml", "*.yaml*"),
    ("yaml", "*.yml*"),
)


# ---------------------------------------------------------------------------
# Registry state
# ---------------------------------------------------------------------------

_STRUCTURE_REGISTRY: dict[str, StructureFormat] = {}
_MOLECULE_REGISTRY: dict[str, MoleculeFormat] = {}
_LOADED_ENTRY_POINT_GROUPS: set[str] = set()


def register_structure_format(fmt: StructureFormat) -> None:
    """Register a `StructureFormat` handler.

    Re-registering an existing name silently replaces the previous handler.
    """
    _STRUCTURE_REGISTRY[fmt.name.lower()] = fmt


def register_molecule_format(fmt: MoleculeFormat) -> None:
    """Register a `MoleculeFormat` handler."""
    _MOLECULE_REGISTRY[fmt.name.lower()] = fmt


def unregister_structure_format(name: str) -> None:
    """Remove a `StructureFormat` handler. No-op if not registered."""
    _STRUCTURE_REGISTRY.pop(name.lower(), None)


def unregister_molecule_format(name: str) -> None:
    """Remove a `MoleculeFormat` handler. No-op if not registered."""
    _MOLECULE_REGISTRY.pop(name.lower(), None)


def list_structure_formats() -> list[StructureFormat]:
    """Return all currently registered structure formats (does not trigger lazy imports)."""
    return list(_STRUCTURE_REGISTRY.values())


def list_molecule_formats() -> list[MoleculeFormat]:
    """Return all currently registered molecule formats (does not trigger lazy imports)."""
    return list(_MOLECULE_REGISTRY.values())


# ---------------------------------------------------------------------------
# kwargs filtering
# ---------------------------------------------------------------------------


def filter_kwargs(func: Callable, kwargs: dict, *, stacklevel: int = 3) -> dict:
    """Filter `kwargs` to those accepted by `func`, warning about any dropped.

    Mirrors the legacy `IStructure._filter_kwargs` behavior:
    - Functions that declare `**kwargs` get the full dict unchanged.
    - Otherwise only keys whose names match the function's parameters are
      kept; the rest are dropped with a `UserWarning`.

    Plugin authors typically call this inside their adapter to forward only
    the relevant kwargs to the underlying parser class, e.g.::

        def _cif_read_str(s, *, primitive=False, **kwargs):
            kwargs = filter_kwargs(CifParser.from_str, kwargs)
            return CifParser.from_str(s, **kwargs).parse_structures(primitive=primitive)[0]
    """
    params = inspect.signature(func).parameters
    if any(p.kind == inspect.Parameter.VAR_KEYWORD for p in params.values()):
        return kwargs
    supported = {k: v for k, v in kwargs.items() if k in params}
    unsupported = kwargs.keys() - supported.keys()
    if unsupported:
        warnings.warn(
            f"The following kwargs are not supported by {func.__qualname__} and will be ignored: {unsupported}",
            stacklevel=stacklevel,
        )
    return supported


# ---------------------------------------------------------------------------
# Lookup
# ---------------------------------------------------------------------------


def _ensure_loaded_structure(name: str) -> None:
    name = name.lower()
    if name in _STRUCTURE_REGISTRY:
        return
    module = _BUILTIN_STRUCTURE_MODULES.get(name)
    if module:
        importlib.import_module(module)
        return
    _load_entry_points("pymatgen.io.structure_formats")


def _ensure_loaded_molecule(name: str) -> None:
    name = name.lower()
    if name in _MOLECULE_REGISTRY:
        return
    module = _BUILTIN_MOLECULE_MODULES.get(name)
    if module:
        importlib.import_module(module)
        return
    _load_entry_points("pymatgen.io.molecule_formats")


def _load_entry_points(group: str) -> None:
    """Import every entry-point in `group` once. Cached by group name."""
    if group in _LOADED_ENTRY_POINT_GROUPS:
        return
    _LOADED_ENTRY_POINT_GROUPS.add(group)
    try:
        from importlib.metadata import entry_points

        for ep in entry_points(group=group):
            # The convention is that the entry-point value points at a module
            # whose import side-effect registers the format(s).
            try:
                importlib.import_module(ep.value)
            except ImportError as exc:
                warnings.warn(
                    f"Failed to load pymatgen I/O plugin {ep.name!r} from {ep.value!r}: {exc}",
                    stacklevel=2,
                )
    except Exception as exc:  # pragma: no cover — defensive
        warnings.warn(f"Entry-point discovery for {group!r} failed: {exc}", stacklevel=2)


def _match_filename(filename: str, pattern: str, *, case_insensitive: bool) -> bool:
    fname = os.path.basename(filename)
    if case_insensitive:
        return fnmatch(fname.lower(), pattern.lower())
    return fnmatch(fname, pattern)


_BUILTIN_STRUCTURE_NAMES = frozenset(name for name, _ in _BUILTIN_STRUCTURE_FILENAME_PRIORITY)
_BUILTIN_MOLECULE_NAMES = frozenset(name for name, _ in _BUILTIN_MOLECULE_FILENAME_PRIORITY)


def _resolve_structure_by_filename(filename: str) -> str | None:
    """Return the format name that matches the filename, or None.

    Resolution order:

    1. The ordered `_BUILTIN_STRUCTURE_FILENAME_PRIORITY` list (preserves the
       priority of the legacy if/elif chain).
    2. Patterns declared by additionally-registered handlers — i.e. plugins
       whose format name is *not* a built-in. Built-in names still go through
       the priority list above, so a plugin registering `name="cif"` overrides
       the built-in CIF read/write code but doesn't change pattern priority.
    """
    for name, pat in _BUILTIN_STRUCTURE_FILENAME_PRIORITY:
        if _match_filename(filename, pat, case_insensitive=True):
            return name
    for fmt in _STRUCTURE_REGISTRY.values():
        if fmt.name in _BUILTIN_STRUCTURE_NAMES:
            continue
        for pat in fmt.patterns:
            if _match_filename(filename, pat, case_insensitive=fmt.case_insensitive):
                return fmt.name
    return None


def _resolve_molecule_by_filename(filename: str) -> str | None:
    for name, pat in _BUILTIN_MOLECULE_FILENAME_PRIORITY:
        if _match_filename(filename, pat, case_insensitive=True):
            return name
    for fmt in _MOLECULE_REGISTRY.values():
        if fmt.name in _BUILTIN_MOLECULE_NAMES:
            continue
        for pat in fmt.patterns:
            if _match_filename(filename, pat, case_insensitive=fmt.case_insensitive):
                return fmt.name
    return None


def get_structure_format(*, name: str = "", filename: str = "") -> StructureFormat:
    """Resolve a `StructureFormat` handler by explicit name or filename.

    Args:
        name: Format identifier (case-insensitive). Takes precedence over
            `filename` when both are given.
        filename: Path or filename used to infer the format from its glob.

    Raises:
        ValueError: If neither argument resolves to a known format. The error
            message mirrors the legacy `Structure.from_file`/`to` wording so
            existing user-facing assertions keep passing.
    """
    name = (name or "").lower()
    filename = filename or ""

    if name:
        _ensure_loaded_structure(name)
        if name in _STRUCTURE_REGISTRY:
            return _STRUCTURE_REGISTRY[name]
        raise ValueError(f"Invalid fmt={name!r}")

    if not filename:
        raise ValueError("Either fmt or filename must be provided.")

    resolved = _resolve_structure_by_filename(filename)
    if resolved is None:
        # Try entry-points once before giving up.
        _load_entry_points("pymatgen.io.structure_formats")
        resolved = _resolve_structure_by_filename(filename)

    if resolved is None:
        raise ValueError(f"Unrecognized extension in filename={filename!r}")

    _ensure_loaded_structure(resolved)
    if resolved in _STRUCTURE_REGISTRY:
        return _STRUCTURE_REGISTRY[resolved]

    raise ValueError(f"Unrecognized extension in filename={filename!r}")


def get_molecule_format(*, name: str = "", filename: str = "") -> MoleculeFormat:
    """Resolve a `MoleculeFormat` handler by explicit name or filename."""
    name = (name or "").lower()
    filename = filename or ""

    if name:
        _ensure_loaded_molecule(name)
        if name in _MOLECULE_REGISTRY:
            return _MOLECULE_REGISTRY[name]
        raise ValueError(f"Invalid fmt={name!r}")

    if not filename:
        raise ValueError("Either fmt or filename must be provided.")

    resolved = _resolve_molecule_by_filename(filename)
    if resolved is None:
        _load_entry_points("pymatgen.io.molecule_formats")
        resolved = _resolve_molecule_by_filename(filename)

    if resolved is None:
        raise ValueError("Cannot determine file type.")

    _ensure_loaded_molecule(resolved)
    if resolved in _MOLECULE_REGISTRY:
        return _MOLECULE_REGISTRY[resolved]

    raise ValueError("Cannot determine file type.")


# ---------------------------------------------------------------------------
# High-level dispatch helpers
# ---------------------------------------------------------------------------


def dispatch_read_str(handler: _IOFormat, input_string: str, /, **kwargs):
    """Read an object from a string via `handler`.

    The return type is `IStructure | Molecule` in practice, depending on
    whether `handler` is a `StructureFormat` or `MoleculeFormat`. Typed as
    `Any` so callers don't need format-aware narrowing.
    """
    if handler.read_str is None:
        raise ValueError(f"Format {handler.name!r} cannot be read from a string.")
    return handler.read_str(input_string, **kwargs)


def dispatch_read_file(handler: _IOFormat, filename: str, /, **kwargs):
    """Read an object from a file via `handler`."""
    if handler.read_file is not None:
        return handler.read_file(filename, **kwargs)
    if handler.read_str is None or handler.binary:
        raise ValueError(f"Format {handler.name!r} cannot be read from a file.")
    with zopen(filename, mode="rt", errors="replace", encoding="utf-8") as file:
        contents = file.read()
    return handler.read_str(contents, **kwargs)


def dispatch_write(handler: _IOFormat, obj: object, filename: str = "", /, **kwargs) -> str | None:
    """Write `obj` via `handler`; returns the string representation when available."""
    out_str: str | None = None
    if handler.write_str is not None:
        out_str = handler.write_str(obj, **kwargs)
    if filename:
        if handler.write_file is not None:
            handler.write_file(obj, filename, **kwargs)
        elif out_str is not None:
            mode = "wb" if handler.binary else "wt"
            encoding = None if handler.binary else "utf-8"
            with zopen(filename, mode=mode, encoding=encoding) as file:
                file.write(out_str if not handler.binary else out_str.encode())  # type:ignore[arg-type]
        else:
            raise ValueError(f"Format {handler.name!r} cannot be written.")
    return out_str


# ---------------------------------------------------------------------------
# Built-in JSON / YAML handlers (no separate io module)
# ---------------------------------------------------------------------------


def _structure_from_dict(dct: dict) -> IStructure:
    from pymatgen.core.structure import Structure

    return Structure.from_dict(dct)


def _molecule_from_dict(dct: dict) -> IMolecule:
    from pymatgen.core.structure import Molecule

    return Molecule.from_dict(dct)


def _json_read_struct(s: str, **kwargs) -> IStructure:
    import orjson

    return _structure_from_dict(orjson.loads(s))


def _json_read_mol(s: str, **kwargs) -> IMolecule:
    import orjson

    return _molecule_from_dict(orjson.loads(s))


def _json_write(obj: object, **kwargs) -> str:
    import json as _json

    import orjson

    as_dict = obj.as_dict()  # type:ignore[attr-defined]
    if kwargs:
        return _json.dumps(as_dict, **kwargs)
    return orjson.dumps(as_dict, option=orjson.OPT_SERIALIZE_NUMPY).decode()


def _yaml_read_struct(s: str, **kwargs) -> IStructure:
    from ruamel.yaml import YAML

    return _structure_from_dict(YAML().load(s))


def _yaml_read_mol(s: str, **kwargs) -> IMolecule:
    from ruamel.yaml import YAML

    return _molecule_from_dict(YAML().load(s))


def _yaml_write(obj: object, **kwargs) -> str:
    import io as _io

    from ruamel.yaml import YAML

    buf = _io.StringIO()
    YAML().dump(obj.as_dict(), buf)  # type:ignore[attr-defined]
    return buf.getvalue()


def _register_builtin_json_yaml() -> None:
    register_structure_format(
        StructureFormat(
            name="json",
            patterns=("*.json*", "*.mson*"),
            read_str=_json_read_struct,
            write_str=_json_write,
        )
    )
    register_structure_format(
        StructureFormat(
            name="yaml",
            patterns=("*.yaml*", "*.yml*"),
            read_str=_yaml_read_struct,
            write_str=_yaml_write,
        )
    )
    # "yml" is a common alias for yaml.
    register_structure_format(
        StructureFormat(
            name="yml",
            patterns=(),
            read_str=_yaml_read_struct,
            write_str=_yaml_write,
        )
    )
    register_molecule_format(
        MoleculeFormat(
            name="json",
            patterns=("*.json*", "*.mson*"),
            read_str=_json_read_mol,
            write_str=_json_write,
        )
    )
    register_molecule_format(
        MoleculeFormat(
            name="yaml",
            patterns=("*.yaml*", "*.yml*"),
            read_str=_yaml_read_mol,
            write_str=_yaml_write,
        )
    )
    register_molecule_format(
        MoleculeFormat(
            name="yml",
            patterns=(),
            read_str=_yaml_read_mol,
            write_str=_yaml_write,
        )
    )


_register_builtin_json_yaml()


# ---------------------------------------------------------------------------
# Compatibility shims for external namespace packages
# ---------------------------------------------------------------------------
#
# `pymatgen-io-fleur` and `pymatgen-io-aims` live outside pymatgen-core. Until
# those packages publish their own `register_structure_format(...)` calls,
# these shims preserve the behavior of the legacy if/elif dispatch in
# `core/structure.py` for users who have either package installed.


def _aims_read_str(input_string: str, **kwargs):
    from pymatgen.io.aims.inputs import AimsGeometryIn

    kwargs.pop("primitive", None)
    return AimsGeometryIn.from_str(input_string, **filter_kwargs(AimsGeometryIn.from_str, kwargs)).structure


def _aims_write_str(structure, **kwargs) -> str:
    from pymatgen.io.aims.inputs import AimsGeometryIn

    return AimsGeometryIn.from_structure(structure).content


def _aims_write_file(structure, filename, **kwargs) -> None:
    from pymatgen.io.aims.inputs import AimsGeometryIn

    geom_in = AimsGeometryIn.from_structure(structure)
    with zopen(filename, mode="wt", encoding="utf-8") as file:
        file.write(geom_in.get_header(filename))  # type:ignore[arg-type]
        file.write(geom_in.content)  # type:ignore[arg-type]
        file.write("\n")  # type:ignore[arg-type]


def _fleur_read_str(input_string: str, *, inpgen_input: bool = True, **kwargs):
    from pymatgen.io.fleur import FleurInput

    kwargs.pop("primitive", None)
    if kwargs:
        warnings.warn(
            f"kwargs {set(kwargs)} cannot be validated for fleur and will be passed through as-is.",
            stacklevel=2,
        )
    return FleurInput.from_string(input_string, inpgen_input=inpgen_input, **kwargs).structure


def _fleur_read_file(filename: str, **kwargs):
    from pymatgen.io.fleur import FleurInput

    kwargs.pop("primitive", None)
    return FleurInput.from_file(filename, **kwargs).structure


def _fleur_inpgen_write_str(structure, **kwargs) -> str:
    from pymatgen.io.fleur import FleurInput

    return str(FleurInput(structure, **kwargs))


def _fleur_inpgen_write_file(structure, filename, **kwargs) -> None:
    from pymatgen.io.fleur import FleurInput

    FleurInput(structure, **kwargs).write_file(filename)


def _try_register_external_shims() -> None:
    """Register fall-back handlers for formats that live in external namespace pkgs.

    These shims only become reachable when the user actually invokes them
    (the underlying `from pymatgen.io.aims.inputs import ...` happens inside
    the adapter), so they cost nothing for users without the optional pkg.
    Once the external package publishes its own registration, that takes
    precedence at import time.
    """
    register_structure_format(
        StructureFormat(
            name="aims",
            patterns=("geometry.in", "geometry.in*"),
            read_str=_aims_read_str,
            write_str=_aims_write_str,
            write_file=_aims_write_file,
        )
    )
    register_structure_format(
        StructureFormat(
            name="fleur-inpgen",
            patterns=("inp*.xml", "*.in*", "inp_*"),
            read_str=lambda s, **kw: _fleur_read_str(s, inpgen_input=True, **kw),
            read_file=_fleur_read_file,
            write_str=_fleur_inpgen_write_str,
            write_file=_fleur_inpgen_write_file,
        )
    )
    register_structure_format(
        StructureFormat(
            name="fleur",
            patterns=(),
            read_str=lambda s, **kw: _fleur_read_str(s, inpgen_input=False, **kw),
            read_file=_fleur_read_file,
        )
    )


_try_register_external_shims()
