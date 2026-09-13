from ndsl import NDSLRuntime, QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.typing import FloatField
from pyfv3.stencils.fillz import FillNegativeTracerValues
from pyfv3.stencils.map_single import MapSingle
from pyfv3.tracers import FVTracers


class MapNTracer(NDSLRuntime):
    """
    Fortran code is mapn_tracer, test class is MapN_Tracer_2d
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        kord: int,
        nq: int,
        fill: bool,
    ):
        super().__init__(stencil_factory)
        self._nq = int(nq)

        self._map_single_parametrized_kord = MapSingle(
            stencil_factory,
            quantity_factory,
            kord,
            0,
            dims=[I_DIM, J_DIM, K_DIM],
        )

        self._map_single_kord9 = MapSingle(
            stencil_factory,
            quantity_factory,
            9,
            0,
            dims=[I_DIM, J_DIM, K_DIM],
        )

        if fill:
            self._fill_negative_tracers = True
            self._fillz = FillNegativeTracerValues(stencil_factory, self._nq)
        else:
            self._fill_negative_tracers = False

        self._graupel = FVTracers.index("graupel")

    def __call__(
        self,
        pe1: FloatField,
        pe2: FloatField,
        dp2: FloatField,
        tracers: FVTracers,
    ):
        """
        Remaps the tracer species onto the Eulerian grid
        and optionally fills negative values in the tracer fields
        Assumes the minimum value is 0 for each tracer

        Args:
            pe1 (in): Lagrangian pressure levels
            pe2 (in): Eulerian pressure levels
            dp2 (in): Difference in pressure between Eulerian levels
            tracers (inout): tracers to be remapped
        """
        for i_tracer in range(0, self._nq):
            if i_tracer == self._graupel:
                self._map_single_kord9(tracers[:, :, :, i_tracer], pe1, pe2)
            else:
                self._map_single_parametrized_kord(tracers[:, :, :, i_tracer], pe1, pe2)

        if self._fill_negative_tracers:
            self._fillz(dp2, tracers)
