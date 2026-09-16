from functools import singledispatch

import dace
import numpy as np

from ndsl import NDSLRuntime, Quantity, StencilFactory
from ndsl.dsl.typing import FloatField
from ndsl.optional_imports import cupy as cp


@singledispatch
def corner_copy_x(field_to_copy):
    raise NotImplementedError(f"No CopyCorners for type {type(field_to_copy)}")


if cp is not None:

    @corner_copy_x.register(cp.ndarray)
    def _corner_copy_x_cupy(field_to_copy: cp.ndarray):
        _blind_copy_corners_x(field_to_copy)


@corner_copy_x.register(np.ndarray)
def _corner_copy_x_numpy(field_to_copy: np.ndarray):
    _blind_copy_corners_x(field_to_copy)


@corner_copy_x.register(Quantity)
def _corner_copy_x_quantity(field_to_copy: Quantity):
    _blind_copy_corners_x(field_to_copy._data)


def _blind_copy_corners_x(field_to_copy):
    """Equivalent to the copy_corners_x functions in fortran.

    This is written to operate on plain ndarrarys and not use the GT4Py framework.
    This choice was made because we've seen a lot of performance left on the table using
    orchestration without explicitly describing the operations but rather have full 3d-
    sweeps with conditionals.
    Since DaCe can handle (simple) operations on ndarrays directly this gives us a more
    explicit entrypoint to the language and more optimization-potential.

    Args:
        field_to_copy (ndarray): field to apply the corner copy on.
            This is explicitly not type-hinted for orchestration
    """
    field_to_copy[0, 0] = field_to_copy[0, 5]
    field_to_copy[0, 1] = field_to_copy[1, 5]
    field_to_copy[0, 2] = field_to_copy[2, 5]

    field_to_copy[1, 0] = field_to_copy[0, 4]
    field_to_copy[1, 1] = field_to_copy[1, 4]
    field_to_copy[1, 2] = field_to_copy[2, 4]

    field_to_copy[2, 0] = field_to_copy[0, 3]
    field_to_copy[2, 1] = field_to_copy[1, 3]
    field_to_copy[2, 2] = field_to_copy[2, 3]

    field_to_copy[0, -4] = field_to_copy[2, -7]
    field_to_copy[0, -3] = field_to_copy[1, -7]
    field_to_copy[0, -2] = field_to_copy[0, -7]

    field_to_copy[1, -4] = field_to_copy[2, -6]
    field_to_copy[1, -3] = field_to_copy[1, -6]
    field_to_copy[1, -2] = field_to_copy[0, -6]

    field_to_copy[2, -4] = field_to_copy[2, -5]
    field_to_copy[2, -3] = field_to_copy[1, -5]
    field_to_copy[2, -2] = field_to_copy[0, -5]

    field_to_copy[-4, 0] = field_to_copy[-2, 3]
    field_to_copy[-4, 1] = field_to_copy[-3, 3]
    field_to_copy[-4, 2] = field_to_copy[-4, 3]

    field_to_copy[-3, 0] = field_to_copy[-2, 4]
    field_to_copy[-3, 1] = field_to_copy[-3, 4]
    field_to_copy[-3, 2] = field_to_copy[-4, 4]

    field_to_copy[-2, 0] = field_to_copy[-2, 5]
    field_to_copy[-2, 1] = field_to_copy[-3, 5]
    field_to_copy[-2, 2] = field_to_copy[-4, 5]

    field_to_copy[-4, -2] = field_to_copy[-2, -5]
    field_to_copy[-4, -3] = field_to_copy[-3, -5]
    field_to_copy[-4, -4] = field_to_copy[-4, -5]

    field_to_copy[-3, -2] = field_to_copy[-2, -6]
    field_to_copy[-3, -3] = field_to_copy[-3, -6]
    field_to_copy[-3, -4] = field_to_copy[-4, -6]

    field_to_copy[-2, -2] = field_to_copy[-2, -7]
    field_to_copy[-2, -3] = field_to_copy[-3, -7]
    field_to_copy[-2, -4] = field_to_copy[-4, -7]


@singledispatch
def corner_copy_y(field_to_copy):
    raise NotImplementedError(f"No CopyCorners for type {type(field_to_copy)}")


if cp is not None:

    @corner_copy_y.register(cp.ndarray)
    def _corner_copy_y_cupy(field_to_copy: cp.ndarray):
        _blind_copy_corners_y(field_to_copy)


@corner_copy_y.register(np.ndarray)
def _corner_copy_y_nupy(field_to_copy: np.ndarray):
    _blind_copy_corners_y(field_to_copy)


@corner_copy_y.register(Quantity)
def _corner_copy_y_quantity(field_to_copy: Quantity):
    _blind_copy_corners_y(field_to_copy._data)


def _blind_copy_corners_y(field_to_copy):
    """Equivalent to the copy_corners_y functions in fortran.

    This is written to operate on plain ndarrarys and not use the GT4Py framework.
    This choice was made because we've seen a lot of performance left on the table using
    orchestration without explicitly describing the operations but rather have full 3d-
    sweeps with conditionals.
    Since DaCe can handle (simple) operations on ndarrays directly this gives us a more
    explicit entrypoint to the language and more optimization-potential.

    Args:
        field_to_copy (ndarray): field to apply the corner copy on.
            This is explicitly not type-hinted for orchestration
    """
    field_to_copy[0, 0] = field_to_copy[5, 0]
    field_to_copy[1, 0] = field_to_copy[5, 1]
    field_to_copy[2, 0] = field_to_copy[5, 2]

    field_to_copy[0, 1] = field_to_copy[4, 0]
    field_to_copy[1, 1] = field_to_copy[4, 1]
    field_to_copy[2, 1] = field_to_copy[4, 2]

    field_to_copy[0, 2] = field_to_copy[3, 0]
    field_to_copy[1, 2] = field_to_copy[3, 1]
    field_to_copy[2, 2] = field_to_copy[3, 2]

    field_to_copy[-4, 0] = field_to_copy[-7, 2]
    field_to_copy[-3, 0] = field_to_copy[-7, 1]
    field_to_copy[-2, 0] = field_to_copy[-7, 0]

    field_to_copy[-4, 1] = field_to_copy[-6, 2]
    field_to_copy[-3, 1] = field_to_copy[-6, 1]
    field_to_copy[-2, 1] = field_to_copy[-6, 0]

    field_to_copy[-4, 2] = field_to_copy[-5, 2]
    field_to_copy[-3, 2] = field_to_copy[-5, 1]
    field_to_copy[-2, 2] = field_to_copy[-5, 0]

    field_to_copy[0, -2] = field_to_copy[5, -2]
    field_to_copy[0, -3] = field_to_copy[4, -2]
    field_to_copy[0, -4] = field_to_copy[3, -2]

    field_to_copy[1, -2] = field_to_copy[5, -3]
    field_to_copy[1, -3] = field_to_copy[4, -3]
    field_to_copy[1, -4] = field_to_copy[3, -3]

    field_to_copy[2, -2] = field_to_copy[5, -4]
    field_to_copy[2, -3] = field_to_copy[4, -4]
    field_to_copy[2, -4] = field_to_copy[3, -4]

    field_to_copy[-2, -4] = field_to_copy[-5, -2]
    field_to_copy[-2, -3] = field_to_copy[-6, -2]
    field_to_copy[-2, -2] = field_to_copy[-7, -2]

    field_to_copy[-3, -4] = field_to_copy[-5, -3]
    field_to_copy[-3, -3] = field_to_copy[-6, -3]
    field_to_copy[-3, -2] = field_to_copy[-7, -3]

    field_to_copy[-4, -4] = field_to_copy[-5, -4]
    field_to_copy[-4, -3] = field_to_copy[-6, -4]
    field_to_copy[-4, -2] = field_to_copy[-7, -4]


class CopyCornersX(NDSLRuntime):
    """
    Helper-class to copy corners corresponding to the fortran function copy_corners_x
    """

    def __init__(self, stencil_factory: StencilFactory) -> None:
        super().__init__(stencil_factory)

        if stencil_factory.grid_indexing.n_halo != 3:
            raise NotImplementedError(
                "Corner-Copy only implemented for exactly 3 Halo-Points"
            )

        self._is_orch = stencil_factory.backend.is_orchestrated()

    def _internal_corners_copy(self, field: FloatField):
        _blind_copy_corners_x(field) if self._is_orch else corner_copy_x(field)

    def __call__(self, field: FloatField):
        self._internal_corners_copy(field)

    def nord(self, field: FloatField, nord: Quantity):
        for k in dace.map[0 : nord.shape[0]]:
            if nord[k] > 0:
                self._internal_corners_copy(field[:, :, k])


class CopyCornersY(NDSLRuntime):
    """
    Helper-class to copy corners corresponding to the fortran function
    copy_corners_y
    """

    def __init__(self, stencil_factory: StencilFactory) -> None:
        super().__init__(stencil_factory)

        if stencil_factory.grid_indexing.n_halo != 3:
            raise NotImplementedError(
                "Corner-Copy only implemented for exactly 3 Halo-Points"
            )

        self._is_orch = stencil_factory.backend.is_orchestrated()

    def _internal_corners_copy(self, field: FloatField):
        _blind_copy_corners_y(field) if self._is_orch else corner_copy_y(field)

    def __call__(self, field: FloatField):
        self._internal_corners_copy(field)

    def nord(self, field: FloatField, nord: Quantity):
        for k in dace.map[0 : nord.shape[0]]:
            if nord[k] > 0:
                self._internal_corners_copy(field[:, :, k])
