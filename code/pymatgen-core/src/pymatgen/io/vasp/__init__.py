"""
This package implements modules for input and output to and from VASP. It
imports the key classes form both vasp_input and vasp_output to allow most
classes to be simply called as pymatgen.io.vasp.Incar for example, to retain
backwards compatibility.
"""

from __future__ import annotations

from .inputs import Incar, Kpoints, Poscar, Potcar, PotcarSingle, VaspInput
from .outputs import (
    BSVasprun,
    Chgcar,
    Dynmat,
    Elfcar,
    Locpot,
    Oszicar,
    Outcar,
    Procar,
    Vaspout,
    Vasprun,
    Vaspwave,
    VolumetricData,
    Wavecar,
    Waveder,
    Xdatcar,
)

# ----------------------------------------------------------------------------
# pymatgen.io.registry plugin: Structure <-> POSCAR / CHGCAR / vasprun
# ----------------------------------------------------------------------------


def _poscar_read_str(input_string: str, **kwargs):
    from pymatgen.io.registry import filter_kwargs

    kwargs.pop("primitive", None)  # not honored by POSCAR
    return Poscar.from_str(
        input_string,
        default_names=None,
        read_velocities=False,
        **filter_kwargs(Poscar.from_str, kwargs),
    ).structure


def _poscar_write_str(structure, **kwargs) -> str:
    return str(Poscar(structure, **kwargs))


def _poscar_write_file(structure, filename, **kwargs) -> None:
    Poscar(structure, **kwargs).write_file(filename)


def _chgcar_read_file(filename: str, **kwargs):
    from pymatgen.io.registry import filter_kwargs

    kwargs.pop("primitive", None)
    return Chgcar.from_file(filename, **filter_kwargs(Chgcar.from_file, kwargs)).structure


def _vasprun_read_file(filename: str, **kwargs):
    from pymatgen.io.registry import filter_kwargs

    kwargs.pop("primitive", None)
    return Vasprun(filename, **filter_kwargs(Vasprun.__init__, kwargs)).final_structure


def _register_formats() -> None:
    from pymatgen.io.registry import StructureFormat, register_structure_format

    register_structure_format(
        StructureFormat(
            name="poscar",
            patterns=("*POSCAR*", "*CONTCAR*", "*.vasp"),
            read_str=_poscar_read_str,
            write_str=_poscar_write_str,
            write_file=_poscar_write_file,
        )
    )
    register_structure_format(
        StructureFormat(
            name="chgcar",
            patterns=("CHGCAR*", "LOCPOT*"),
            read_file=_chgcar_read_file,
        )
    )
    # Alias "locpot" -> same handler (file pattern is the discriminator).
    register_structure_format(
        StructureFormat(
            name="locpot",
            patterns=("LOCPOT*",),
            read_file=_chgcar_read_file,
        )
    )
    register_structure_format(
        StructureFormat(
            name="vasprun",
            patterns=("vasprun*.xml*",),
            read_file=_vasprun_read_file,
        )
    )


_register_formats()
