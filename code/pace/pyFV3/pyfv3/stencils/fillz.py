import typing

from ndsl import NDSLRuntime, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import BACKWARD, FORWARD, PARALLEL, computation, interval, max, min
from ndsl.dsl.typing import FloatField, FloatFieldIJ, IntFieldIJ
from pyfv3.tracers import FVTracers


@typing.no_type_check
def fix_tracer(q: FloatField, dp: FloatField) -> None:
    """
    Args:
        q (inout): tracer to fix negative masses in
        dp (in): pressure thickness of atmospheric layer
    """
    # Reset 2D fields
    with computation(FORWARD), interval(0, 1):
        zfix: IntFieldIJ = 0
        sum0: FloatFieldIJ = 0.0
        sum1: FloatFieldIJ = 0.0
    # Reset 3D fields
    with computation(PARALLEL), interval(...):
        lower_fix = 0.0
        upper_fix = 0.0
    # fix_top:
    with computation(BACKWARD):
        with interval(1, 2):
            if q[0, 0, -1] < 0.0:
                q = (
                    q + q[0, 0, -1] * dp[0, 0, -1] / dp
                )  # move enough mass up so that the top layer isn't negative
        with interval(0, 1):
            if q < 0:
                q = 0
            dm = q * dp
    # fix_interior:
    with computation(FORWARD), interval(1, -1):
        # if a higher layer borrowed from this one, account for that here
        if lower_fix[0, 0, -1] != 0.0:
            q = q - (lower_fix[0, 0, -1] / dp)
        if q < 0.0:
            zfix += 1
            if q[0, 0, -1] > 0.0:
                # Borrow from the layer above
                dq = min(q[0, 0, -1] * dp[0, 0, -1], -(q * dp))
                q = q + dq / dp
                upper_fix = dq
            if (q < 0.0) and (q[0, 0, 1] > 0.0):
                # borrow from the layer below
                dq = min(q[0, 0, 1] * dp[0, 0, 1], -(q * dp))
                q = q + dq / dp
                lower_fix = dq
    with computation(PARALLEL), interval(0, -1):
        if upper_fix[0, 0, 1] != 0.0:
            # If a lower layer borrowed from this one, account for that here
            q = q - upper_fix[0, 0, 1] / dp
        dm = q * dp
        dm_pos = max(dm, 0.0)
    # fix_bottom:
    with computation(FORWARD), interval(-1, None):
        # the 2nd-to-last layer borrowed from this one, account for that here
        if lower_fix[0, 0, -1] != 0.0:
            q = q - (lower_fix[0, 0, -1] / dp)
        qup = q[0, 0, -1] * dp[0, 0, -1]
        qly = -q * dp
        dup = min(qup, qly)
        if (q < 0.0) and (q[0, 0, -1] > 0.0):
            zfix += 1
            q = q + (dup / dp)
            upper_fix = dup
        dm = q * dp
        dm_pos = max(dm, 0.0)
    with computation(PARALLEL), interval(-2, -1):
        # if the bottom layer borrowed from this one, adjust
        if upper_fix[0, 0, 1] != 0.0:
            q = q - (upper_fix[0, 0, 1] / dp)
            dm = q * dp
            dm_pos = max(dm, 0.0)  # now we gotta update these too
    with computation(FORWARD), interval(1, None):
        sum0 += dm
        sum1 += dm_pos
    # final_check
    with computation(PARALLEL), interval(1, None):
        fac = sum0 / sum1 if sum0 > 0.0 else 0.0
        if zfix > 0 and fac > 0.0:
            q = max(fac * dm / dp, 0.0)


class FillNegativeTracerValues(NDSLRuntime):
    """
    Fix tracer values to prevent negative masses.

    Fortran name is `fillz`
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        nq: int,
    ) -> None:
        super().__init__(stencil_factory)

        self._nq = int(nq)
        self._fix_tracer_stencil = stencil_factory.from_dims_halo(
            fix_tracer,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )

    def __call__(
        self,
        dp2: FloatField,
        tracers: FVTracers,
    ) -> None:
        """
        Args:
            tracers (inout): tracers to fix negative masses in
            dp2 (in): pressure thickness of atmospheric layer
        """
        for i_tracer in range(0, self._nq):
            self._fix_tracer_stencil(
                tracers[:, :, :, i_tracer],
                dp2,
            )
