"""This package implements modules for input and output to and from PWmat."""

from __future__ import annotations

from .inputs import AtomConfig
from .outputs import DosSpin, Movement, OutFermi, Report

# ----------------------------------------------------------------------------
# pymatgen.io.registry plugin: Structure <-> PWmat atom.config
# ----------------------------------------------------------------------------


def _pwmat_read_str(input_string: str, **kwargs):
    from pymatgen.io.registry import filter_kwargs

    kwargs.pop("primitive", None)
    return AtomConfig.from_str(input_string, **filter_kwargs(AtomConfig.from_str, kwargs)).structure


def _pwmat_read_file(filename: str, **kwargs):
    from pymatgen.io.registry import filter_kwargs

    kwargs.pop("primitive", None)
    return AtomConfig.from_file(filename, **filter_kwargs(AtomConfig.from_file, kwargs)).structure


def _pwmat_write_str(structure, **kwargs) -> str:
    return str(AtomConfig(structure, **kwargs))


def _pwmat_write_file(structure, filename, **kwargs) -> None:
    AtomConfig(structure, **kwargs).write_file(filename)


def _register_formats() -> None:
    from pymatgen.io.registry import StructureFormat, register_structure_format

    register_structure_format(
        StructureFormat(
            name="pwmat",
            patterns=("*.pwmat*", "*.config*"),
            read_str=_pwmat_read_str,
            read_file=_pwmat_read_file,
            write_str=_pwmat_write_str,
            write_file=_pwmat_write_file,
        )
    )


_register_formats()
