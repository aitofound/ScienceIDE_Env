from typing import Optional

import dace

from ndsl import NDSLRuntime, Quantity, QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, I_INTERFACE_DIM, J_DIM, J_INTERFACE_DIM, K_DIM
from ndsl.dsl.gt4py import PARALLEL, computation
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import horizontal, interval, region
from ndsl.dsl.stencil import get_stencils_with_varied_bounds
from ndsl.dsl.typing import Float, FloatField, FloatFieldIJ, FloatFieldK
from ndsl.grid import DampingCoefficients
from pyfv3.stencils.copy_corners import CopyCornersX, CopyCornersY


def calc_damp(damp_c: Quantity, da_min: Float, nord: Quantity) -> Quantity:
    if damp_c.dims != nord.dims or damp_c.shape != nord.shape:
        raise NotImplementedError(
            "Current implementation requires damp_c and nord to have identical data shape and dims."
        )
    data = (damp_c[:] * da_min) ** (nord[:] + 1)
    return Quantity(
        data=data,
        dims=damp_c.dims,
        # TODO: find and document units
        units="unknown",
        origin=damp_c.origin,
        extent=damp_c.extent,
        backend=damp_c.backend,
    )


def fx_calc_stencil_nord(
    q: FloatField, del6_v: FloatFieldIJ, fx: FloatField, nord: FloatFieldK
):
    """
    Args:
        q (in):
        del6_v (in):
        fx (out):
    """
    from __externals__ import local_ie, local_is, local_je, local_js

    with computation(PARALLEL), interval(...):
        if nord == 0:
            with horizontal(region[local_is : local_ie + 2, local_js : local_je + 1]):
                fx = fx_calculation(q, del6_v)
        else:
            fx = fx_calculation(q, del6_v)


def fy_calc_stencil_nord(
    q: FloatField, del6_u: FloatFieldIJ, fy: FloatField, nord: FloatFieldK
):
    """
    Args:
        q (in):
        del6_u (in):
        fy (out):
    """
    from __externals__ import local_ie, local_is, local_je, local_js

    with computation(PARALLEL), interval(...):
        if nord == 0:
            with horizontal(region[local_is : local_ie + 1, local_js : local_je + 2]):
                fy = fy_calculation(q, del6_u)
        else:
            fy = fy_calculation(q, del6_u)


def fx_calc_stencil_column(
    q: FloatField,
    del6_v: FloatFieldIJ,
    fx: FloatField,
    nord: FloatFieldK,
    current_nord: int,
):
    with computation(PARALLEL), interval(...):
        if nord > current_nord:
            fx = fx_calculation_neg(q, del6_v)


def fy_calc_stencil_column(
    q: FloatField,
    del6_u: FloatFieldIJ,
    fy: FloatField,
    nord: FloatFieldK,
    current_nord: int,
):
    with computation(PARALLEL), interval(...):
        if nord > current_nord:
            fy = fy_calculation_neg(q, del6_u)


@gtfunction
def fx_calculation(q: FloatField, del6_v: FloatField):
    return del6_v * (q[-1, 0, 0] - q)


@gtfunction
def fx_calculation_neg(q: FloatField, del6_v: FloatField):
    return -del6_v * (q[-1, 0, 0] - q)


@gtfunction
def fy_calculation(q: FloatField, del6_u: FloatField):
    return del6_u * (q[0, -1, 0] - q)


@gtfunction
def fy_calculation_neg(q: FloatField, del6_u: FloatField):
    return -del6_u * (q[0, -1, 0] - q)


def d2_highorder_stencil(
    fx: FloatField,
    fy: FloatField,
    rarea: FloatFieldIJ,
    nord: FloatFieldK,
    d2: FloatField,
    current_nord: int,
):
    with computation(PARALLEL), interval(...):
        if nord > current_nord:
            d2 = (fx - fx[1, 0, 0] + fy - fy[0, 1, 0]) * rarea


def d2_damp_interval(
    q: FloatField, d2: FloatField, damp: FloatFieldK, nord: FloatFieldK
):
    """
    q (in):
    d2 (out):
    damp (in):
    """
    from __externals__ import local_ie, local_is, local_je, local_js

    with computation(PARALLEL), interval(...):
        if nord == 0:
            with horizontal(
                region[local_is - 1 : local_ie + 2, local_js - 1 : local_je + 2]
            ):
                d2 = damp * q
        else:
            d2 = damp * q


def copy_stencil_interval(q_in: FloatField, q_out: FloatField, nord: FloatFieldK):
    """
    Args:
        q_in (in):
        q_out (out):
    """
    from __externals__ import local_ie, local_is, local_je, local_js

    with computation(PARALLEL), interval(...):
        if nord == 0:
            with horizontal(
                region[local_is - 1 : local_ie + 2, local_js - 1 : local_je + 2]
            ):
                q_out = q_in
        else:
            q_out = q_in


def add_diffusive_component(
    fx: FloatField, fx2: FloatField, fy: FloatField, fy2: FloatField
):
    with computation(PARALLEL), interval(...):
        fx = fx + fx2
        fy = fy + fy2


def diffusive_damp(
    fx: FloatField,
    fx2: FloatField,
    fy: FloatField,
    fy2: FloatField,
    mass: FloatField,
    damp: FloatFieldK,
):
    with computation(PARALLEL), interval(...):
        fx = fx + 0.5 * damp * (mass[-1, 0, 0] + mass) * fx2
        fy = fy + 0.5 * damp * (mass[0, -1, 0] + mass) * fy2


class DelnFlux(NDSLRuntime):
    """
    Fortran name is deln_flux
    The test class is DelnFlux

    This class computes the fluxes for damping and also applies them.
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        damping_coefficients: DampingCoefficients,
        rarea: Quantity,
        nord_col: Quantity,
        damp_c: Quantity,
    ):
        """
        nord sets the order of damping to apply:
        nord = 0:   del-2
        nord = 1:   del-4
        nord = 2:   del-6

        nord and damp_c define the damping coefficient used in DelnFluxNoSG
        """
        super().__init__(stencil_factory)
        self._no_compute = False
        if (damp_c.view[:] <= 1e-4).all():
            self._no_compute = True
        elif (damp_c.view[:-1] <= 1e-4).any():
            raise NotImplementedError(
                "damp_c currently must be always greater than 10^-4 for delnflux"
            )
        grid_indexing = stencil_factory.grid_indexing
        nk = grid_indexing.domain[2]
        self._origin = grid_indexing.origin_full()

        self._fx2 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="undefined",
            dtype=Float,
        )
        self._fy2 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="undefined",
            dtype=Float,
        )
        self._d2 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="undefined",
            dtype=Float,
        )

        self._add_diffusive_stencil = stencil_factory.from_dims_halo(
            func=add_diffusive_component,
            compute_dims=[I_INTERFACE_DIM, J_INTERFACE_DIM, K_DIM],
        )
        self._diffusive_damp_stencil = stencil_factory.from_dims_halo(
            func=diffusive_damp, compute_dims=[I_INTERFACE_DIM, J_INTERFACE_DIM, K_DIM]
        )

        self._damp = calc_damp(
            damp_c=damp_c, da_min=damping_coefficients.da_min, nord=nord_col
        )

        self.delnflux_nosg = DelnFluxNoSG(
            stencil_factory, damping_coefficients, rarea, nord_col, nk=nk
        )

    def __call__(
        self,
        q: FloatField,
        fx: FloatField,
        fy: FloatField,
        d2: Optional["FloatField"] = None,
        mass: Optional["FloatField"] = None,
    ):
        """
        Del-n damping for fluxes, where n = 2 * nord + 2
        Args:
            q: Field for which to calculate damped fluxes (in)
            fx: x-flux on A-grid (inout)
            fy: y-flux on A-grid (inout)
            d2: A damped copy of the q field (in)
            mass: Mass to weight the diffusive flux by (in)
        """
        if self._no_compute:
            return fx, fy

        # [DaCe] Optional d2 gets reduced to subset 0 in DaCe parsing leading to a
        # parsing error
        # Original code:
        # if d2 is None:
        #     d2 = self._d2
        # fx2 and fy2 are local variables containing the diffusive flux, which
        # gets added to the base flux below
        if d2 is None:
            self.delnflux_nosg(q, self._fx2, self._fy2, self._damp, self._d2, mass)
        else:
            self.delnflux_nosg(q, self._fx2, self._fy2, self._damp, d2, mass)

        if mass is None:
            self._add_diffusive_stencil(fx, self._fx2, fy, self._fy2)
        else:
            # TODO: To join these stencils you need to overcompute, making the edges
            # 'wrong', but not actually used, separating now for comparison sanity.

            # diffusive_damp(fx, fx2, fy, fy2, mass, damp, origin=diffuse_origin,
            # domain=(grid.nic + 1, grid.njc + 1, nk))
            self._diffusive_damp_stencil(fx, self._fx2, fy, self._fy2, mass, self._damp)

        return fx, fy


class DelnFluxNoSG(NDSLRuntime):
    """
    This contains the mechanics of del6_vt and some of deln_flux from
    the Fortran code, since they are very similar routines. The test class
    is Del6VtFlux

    SG stands for signsg

    This class only computes damping fluxes, and does not apply them.
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        damping_coefficients: DampingCoefficients,
        rarea: Quantity,
        nord: Quantity,
        nk: int | None = None,
    ):
        """
        nord sets the order of damping to apply:
        nord = 0:   del-2
        nord = 1:   del-4
        nord = 2:   del-6
        """
        super().__init__(stencil_factory)
        grid_indexing = stencil_factory.grid_indexing
        self._del6_u = damping_coefficients.del6_u
        self._del6_v = damping_coefficients.del6_v
        self._rarea = rarea
        nord[:] = nord[:].round().astype(int)
        self._nmax = int(max(nord.view[:]))
        if self._nmax > 3:
            raise ValueError("nord must be less than 3")
        if not all(n in [0, 2, 3] for n in nord.view[:]):
            raise NotImplementedError("nord must have values 0, 2, or 3")
        i1 = grid_indexing.isc - 1 - self._nmax
        i2 = grid_indexing.iec + 1 + self._nmax
        j1 = grid_indexing.jsc - 1 - self._nmax
        j2 = grid_indexing.jec + 1 + self._nmax
        if nk is None:
            nk = grid_indexing.domain[2]
        nk = nk
        origin_d2 = (i1, j1, 0)
        domain_d2 = (i2 - i1 + 1, j2 - j1 + 1, nk)
        f1_ny = grid_indexing.jec - grid_indexing.jsc + 1 + 2 * self._nmax
        f1_nx = grid_indexing.iec - grid_indexing.isc + 2 + 2 * self._nmax
        fx_origin = (grid_indexing.isc - self._nmax, grid_indexing.jsc - self._nmax, 0)
        self._nord = nord

        if nk <= 3:
            raise NotImplementedError("nk must be more than 3 for DelnFluxNoSG")

        preamble_ax_offsets = grid_indexing.axis_offsets(origin_d2, domain_d2)
        fx_ax_offsets = grid_indexing.axis_offsets(fx_origin, (f1_nx, f1_ny, nk))
        fy_ax_offsets = grid_indexing.axis_offsets(
            fx_origin, (f1_nx - 1, f1_ny + 1, nk)
        )

        origins_d2 = []
        domains_d2 = []
        origins_flux = []
        domains_fx = []
        domains_fy = []

        for n in range(self._nmax):
            nt = self._nmax - 1 - n
            nt_ny = grid_indexing.jec - grid_indexing.jsc + 3 + 2 * nt
            nt_nx = grid_indexing.iec - grid_indexing.isc + 3 + 2 * nt
            origins_d2.append(
                (grid_indexing.isc - nt - 1, grid_indexing.jsc - nt - 1, 0)
            )
            domains_d2.append((nt_nx, nt_ny, nk))
            origins_flux.append((grid_indexing.isc - nt, grid_indexing.jsc - nt, 0))
            domains_fx.append((nt_nx - 1, nt_ny - 2, nk))
            domains_fy.append((nt_nx - 2, nt_ny - 1, nk))

        self._d2_damp = stencil_factory.from_origin_domain(
            d2_damp_interval,
            origin=origin_d2,
            domain=domain_d2,
            externals={**preamble_ax_offsets},
        )

        self._copy_stencil_interval = stencil_factory.from_origin_domain(
            copy_stencil_interval,
            origin=origin_d2,
            domain=domain_d2,
            externals={**preamble_ax_offsets},
        )

        self._d2_stencil = get_stencils_with_varied_bounds(
            d2_highorder_stencil,
            origins_d2,
            domains_d2,
            stencil_factory=stencil_factory,
        )
        self._column_conditional_fx_calculation = get_stencils_with_varied_bounds(
            fx_calc_stencil_column,
            origins_flux,
            domains_fx,
            stencil_factory=stencil_factory,
        )
        self._column_conditional_fy_calculation = get_stencils_with_varied_bounds(
            fy_calc_stencil_column,
            origins_flux,
            domains_fy,
            stencil_factory=stencil_factory,
        )
        self._fx_calc_stencil = stencil_factory.from_origin_domain(
            fx_calc_stencil_nord,
            externals={**fx_ax_offsets},
            origin=fx_origin,
            domain=(f1_nx, f1_ny, nk),
        )
        self._fy_calc_stencil = stencil_factory.from_origin_domain(
            fy_calc_stencil_nord,
            externals={**fy_ax_offsets},
            origin=fx_origin,
            domain=(f1_nx - 1, f1_ny + 1, nk),
        )

        self.copy_corners_x = CopyCornersX(stencil_factory)
        self.copy_corners_y = CopyCornersY(stencil_factory)

    def __call__(self, q, fx2, fy2, damp_c, d2, mass=None):
        """
        Computes flux fields which would apply del-n damping to q,
        where n is set by nord.

        Can compute diffusion at 2nd, 4th, 6th-order but expresses it as a flux
        so that it's conservative. Doesn't apply those fluxes in this object.

        Args:
            q (in): Field for which to calculate damping fluxes
            fx2 (out): x-flux on A grid to apply damping to q
            fy2 (out): y-flux on A grid to apply damping to q
            damp_c (in): damping coefficient for q
            d2 (out): higher-order damped version of q
            mass (unused): if given, apply d2 damping (does not use this as input)
        """

        if mass is None:
            self._d2_damp(q=q, d2=d2, damp=damp_c, nord=self._nord)
        else:
            self._copy_stencil_interval(q_in=q, q_out=d2, nord=self._nord)

        self.copy_corners_x.nord(d2, self._nord)

        self._fx_calc_stencil(q=d2, del6_v=self._del6_v, fx=fx2, nord=self._nord)

        self.copy_corners_y.nord(d2, self._nord)

        self._fy_calc_stencil(q=d2, del6_u=self._del6_u, fy=fy2, nord=self._nord)

        # Force unroll of the loop because list of object do not parse
        # when unrolled
        # -> https://github.com/spcl/dace/issues/2332
        for n in dace.unroll(range(self._nmax)):
            self._d2_stencil[n](
                fx=fx2,
                fy=fy2,
                rarea=self._rarea,
                nord=self._nord,
                d2=d2,
                current_nord=n,
            )

            self.copy_corners_x.nord(d2, self._nord)

            self._column_conditional_fx_calculation[n](
                q=d2, del6_v=self._del6_v, fx=fx2, nord=self._nord, current_nord=n
            )

            self.copy_corners_y.nord(d2, self._nord)

            self._column_conditional_fy_calculation[n](
                q=d2, del6_u=self._del6_u, fy=fy2, nord=self._nord, current_nord=n
            )
