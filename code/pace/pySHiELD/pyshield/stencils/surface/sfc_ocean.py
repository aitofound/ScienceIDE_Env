import ndsl.constants as constants
import pyshield.constants as physcons
from ndsl import StencilFactory
from ndsl.dsl.gt4py import FORWARD, computation, interval, max, min, sqrt
from ndsl.dsl.typing import BoolFieldIJ, FloatFieldIJ, IntFieldIJ
from pyshield.functions.physics_functions import fpvs


def sfc_ocean(
    ps: FloatFieldIJ,
    u1: FloatFieldIJ,
    v1: FloatFieldIJ,
    t1: FloatFieldIJ,
    qvapor: FloatFieldIJ,
    tskin: FloatFieldIJ,
    cm: FloatFieldIJ,
    ch: FloatFieldIJ,
    prsl1: FloatFieldIJ,
    prslki: FloatFieldIJ,
    ddvel: FloatFieldIJ,
    qsurf: FloatFieldIJ,
    cmm: FloatFieldIJ,
    chh: FloatFieldIJ,
    gflux: FloatFieldIJ,
    evap: FloatFieldIJ,
    hflx: FloatFieldIJ,
    ep: FloatFieldIJ,
    islimsk: IntFieldIJ,
    flag_iter: BoolFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        if (islimsk == 0) and (flag_iter):
            wind = max(sqrt(u1**2 + v1**2) + max(0.0, min(ddvel, 30)), 1.0)
            q0 = max(qvapor[0, 0], 1.0e-8)
            rho = prsl1 / (constants.RDGAS * t1 * (1.0 + constants.ZVIR * q0))

            qss = fpvs(tskin)
            qss = constants.EPS * qss / (ps + constants.EPSM1 * qss)

            evap = 0.0
            hflx = 0.0
            ep = 0.0
            gflux = 0.0

            # rcp  = rho cp ch v
            rch = rho * constants.CP_AIR * ch * wind
            cmm = cm * wind
            chh = rho * ch * wind

            # sensible and latent heat flux over open water:
            hflx = rch * (tskin - t1 * prslki)

            evap = physcons.HOCP * rch * (qss - q0)
            qsurf = qss

            tem = 1.0 / rho
            hflx = hflx * tem / constants.CP_AIR
            evap = evap * tem / constants.HLV


class SurfaceOcean:
    def __init__(
        self,
        stencil_factory: StencilFactory,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._sfc_ocean = stencil_factory.from_origin_domain(
            sfc_ocean,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )

    def __call__(
        self,
        ps: FloatFieldIJ,
        u1: FloatFieldIJ,
        v1: FloatFieldIJ,
        t1: FloatFieldIJ,
        qvapor: FloatFieldIJ,
        tskin: FloatFieldIJ,
        cm: FloatFieldIJ,
        ch: FloatFieldIJ,
        prsl1: FloatFieldIJ,
        prslki: FloatFieldIJ,
        ddvel: FloatFieldIJ,
        qsurf: FloatFieldIJ,
        cmm: FloatFieldIJ,
        chh: FloatFieldIJ,
        gflux: FloatFieldIJ,
        evap: FloatFieldIJ,
        hflx: FloatFieldIJ,
        ep: FloatFieldIJ,
        islimsk: IntFieldIJ,
        flag_iter: BoolFieldIJ,
    ):
        """
        Original Fortran docstring:
        ! ===================================================================== !
        !  description:                                                         !
        !                                                                       !
        !  usage:                                                               !
        !                                                                       !
        !    call sfc_ocean                                                     !
        !       inputs:                                                         !
        !          ( im, ps, u1, v1, t1, q1, tskin, cm, ch,                     !
        !            prsl1, prslki, islimsk, ddvel, flag_iter,                  !
        !       outputs:                                                        !
        !            qsurf, cmm, chh, gflux, evap, hflx, ep )                   !
        !                                                                       !
        !                                                                       !
        !  subprograms/functions called: fpvs                                   !
        !                                                                       !
        !                                                                       !
        !  program history log:                                                 !
        !         2005  -- created from the original progtm to account for      !
        !                  ocean only                                           !
        !    oct  2006  -- h. wei      added cmm and chh to the output          !
        !    apr  2009  -- y.-t. hou   modified to match the modified gbphys.f  !
        !                  reformatted the code and added program documentation !
        !    sep  2009  -- s. moorthi removed rcl and made pa as pressure unit  !
        !                  and furthur reformatted the code                     !
        !                                                                       !
        !                                                                       !
        !  ====================  defination of variables  ====================  !
        !                                                                       !
        !  inputs:                                                       size   !
        !     im       - integer, horizontal dimension                     1    !
        !     ps       - real, surface pressure                            im   !
        !     u1, v1   - real, u/v component of surface layer wind         im   !
        !     t1       - real, surface layer mean temperature ( k )        im   !
        !     q1       - real, surface layer mean specific humidity        im   !
        !     tskin    - real, ground surface skin temperature ( k )       im   !
        !     cm       - real, surface exchange coeff for momentum (m/s)   im   !
        !     ch       - real, surface exchange coeff heat & moisture(m/s) im   !
        !     prsl1    - real, surface layer mean pressure                 im   !
        !     prslki   - real,                                             im   !
        !     islimsk  - integer, sea/land/ice mask (=0/1/2)               im   !
        !     ddvel    - real, wind enhancement due to convection (m/s)    im   !
        !     flag_iter- logical,                                          im   !
        !                                                                       !
        !  outputs:                                                             !
        !     qsurf    - real, specific humidity at sfc                    im   !
        !     cmm      - real,                                             im   !
        !     chh      - real,                                             im   !
        !     gflux    - real, ground heat flux (zero for ocean)           im   !
        !     evap     - real, evaporation from latent heat flux           im   !
        !     hflx     - real, sensible heat flux                          im   !
        !     ep       - real, potential evaporation                       im   !
        !                                                                       !
        ! ===================================================================== !
        """
        self._sfc_ocean(
            ps,
            u1,
            v1,
            t1,
            qvapor,
            tskin,
            cm,
            ch,
            prsl1,
            prslki,
            ddvel,
            qsurf,
            cmm,
            chh,
            gflux,
            evap,
            hflx,
            ep,
            islimsk,
            flag_iter,
        )
