from typing import Optional

from ndsl import NDSLRuntime, QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import FORWARD, PARALLEL, computation, interval
from ndsl.dsl.typing import Float, FloatField, FloatFieldIJ, Int, IntFieldIJ
from ndsl.stencils import copy
from pyfv3.stencils.remap_profile import RemapProfile


def set_dp(dp1: FloatField, pe1: FloatField, lev: IntFieldIJ):
    with computation(PARALLEL), interval(...):
        dp1 = pe1[0, 0, 1] - pe1
    with computation(FORWARD), interval(0, 1):
        lev = 0


def lagrangian_contributions(
    q: FloatField,
    pe1: FloatField,
    pe2: FloatField,
    q4_1: FloatField,
    q4_2: FloatField,
    q4_3: FloatField,
    q4_4: FloatField,
    dp1: FloatField,
    lev: IntFieldIJ,
):
    """
    Args:
        q (out):
        pe1 (in):
        pe2 (in):
        q4_1 (in):
        q4_2 (in):
        q4_3 (in):
        q4_4 (in):
        dp1 (in):
        lev (inout):
    """
    # TODO: Can we make lev a 2D temporary?
    with computation(FORWARD), interval(...):
        pl = (pe2 - pe1[0, 0, lev]) / dp1[0, 0, lev]
        if pe2[0, 0, 1] <= pe1[0, 0, lev + 1]:
            pr = (pe2[0, 0, 1] - pe1[0, 0, lev]) / dp1[0, 0, lev]
            q = (
                q4_2[0, 0, lev]
                + 0.5
                * (q4_4[0, 0, lev] + q4_3[0, 0, lev] - q4_2[0, 0, lev])
                * (pr + pl)
                - q4_4[0, 0, lev] * 1.0 / 3.0 * (pr * (pr + pl) + pl * pl)
            )
        else:
            qsum = (pe1[0, 0, lev + 1] - pe2) * (
                q4_2[0, 0, lev]
                + 0.5
                * (q4_4[0, 0, lev] + q4_3[0, 0, lev] - q4_2[0, 0, lev])
                * (1.0 + pl)
                - q4_4[0, 0, lev] * 1.0 / 3.0 * (1.0 + pl * (1.0 + pl))
            )
            lev = lev + 1
            while pe1[0, 0, lev + 1] < pe2[0, 0, 1]:
                qsum += dp1[0, 0, lev] * q4_1[0, 0, lev]
                lev = lev + 1
            dp = pe2[0, 0, 1] - pe1[0, 0, lev]
            esl = dp / dp1[0, 0, lev]
            qsum += dp * (
                q4_2[0, 0, lev]
                + 0.5
                * esl
                * (
                    q4_3[0, 0, lev]
                    - q4_2[0, 0, lev]
                    + q4_4[0, 0, lev] * (1.0 - (2.0 / 3.0) * esl)
                )
            )
            q = qsum / (pe2[0, 0, 1] - pe2)
        lev = lev - 1


class MapSingle(NDSLRuntime):
    """
    Fortran name is map_single, test classes are Map1_PPM_2d, Map_Scalar_2d
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        kord: int,
        mode: int,
        dims: list[str] | tuple[str],
    ) -> None:
        super().__init__(stencil_factory)

        self._dp1 = self.make_local(quantity_factory, [I_DIM, J_DIM, K_DIM])
        self._q4_1 = self.make_local(quantity_factory, [I_DIM, J_DIM, K_DIM])
        self._q4_2 = self.make_local(quantity_factory, [I_DIM, J_DIM, K_DIM])
        self._q4_3 = self.make_local(quantity_factory, [I_DIM, J_DIM, K_DIM])
        self._q4_4 = self.make_local(quantity_factory, [I_DIM, J_DIM, K_DIM])
        self._lev = self.make_local(quantity_factory, [I_DIM, J_DIM], dtype=Int)

        # If the boundary condition is not given as an input, we use use a zero-reference
        # Therefore we CAN'T use make_local
        self._zero_qs = quantity_factory.zeros([I_DIM, J_DIM], "")

        self._copy_stencil = stencil_factory.from_dims_halo(
            copy,
            compute_dims=dims,
        )

        self._set_dp = stencil_factory.from_dims_halo(
            set_dp,
            compute_dims=dims,
        )

        self._remap_profile = RemapProfile(
            stencil_factory,
            quantity_factory,
            kord,
            mode,
            dims=dims,
        )

        self._lagrangian_contributions = stencil_factory.from_dims_halo(
            lagrangian_contributions,
            compute_dims=dims,
        )

    def __call__(
        self,
        q1: FloatField,
        pe1: FloatField,
        pe2: FloatField,
        qs: Optional[FloatFieldIJ] = None,
        qmin: Float = 0.0,
    ) -> None:
        """
        Compute x-flux using the PPM method.

        Args:
            q1 (out): Remapped field on Eulerian grid
            pe1 (in): Lagrangian pressure levels
            pe2 (in): Eulerian pressure levels
            qs (in): Bottom boundary condition
            qmin (in): Minimum allowed value of the remapped field
        """

        self._copy_stencil(q1, self._q4_1)
        self._set_dp(self._dp1, pe1, self._lev)

        if qs is None:
            self._remap_profile(
                self._zero_qs,
                self._q4_1,
                self._q4_2,
                self._q4_3,
                self._q4_4,
                self._dp1,
                qmin,
            )
        else:
            self._remap_profile(
                qs,
                self._q4_1,
                self._q4_2,
                self._q4_3,
                self._q4_4,
                self._dp1,
                qmin,
            )
        self._lagrangian_contributions(
            q1,
            pe1,
            pe2,
            self._q4_1,
            self._q4_2,
            self._q4_3,
            self._q4_4,
            self._dp1,
            self._lev,
        )
