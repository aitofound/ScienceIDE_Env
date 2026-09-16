from typing import Any

from ndsl import QuantityFactory, StencilFactory
from ndsl.constants import I_INTERFACE_DIM, J_INTERFACE_DIM, K_DIM
from pyfv3.stencils.map_single import MapSingle


class MapSingleFactory:
    _object_pool: dict[tuple[int, int, tuple[str, ...]], MapSingle] = {}
    """Pool of MapSingle objects."""

    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
    ) -> None:
        self.stencil_factory = stencil_factory
        self.quantity_factory = quantity_factory

    def __call__(
        self,
        kord: int,
        mode: int,
        *args: Any,
        **kwargs: dict,
    ) -> None:
        key_tuple = (kord, mode, (I_INTERFACE_DIM, J_INTERFACE_DIM, K_DIM))
        if key_tuple not in self._object_pool:
            self._object_pool[key_tuple] = MapSingle(
                self.stencil_factory,
                self.quantity_factory,
                key_tuple[0],
                key_tuple[1],
                list(key_tuple[2]),
            )
        return self._object_pool[key_tuple](*args, **kwargs)
