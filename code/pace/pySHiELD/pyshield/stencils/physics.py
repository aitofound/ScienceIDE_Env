import datetime

import numpy as np

import ndsl.constants as constants
import pyshield.constants as physcons
from ndsl import QuantityFactory, StencilFactory, orchestrate
from ndsl.constants import I_DIM, J_DIM, K_DIM, K_INTERFACE_DIM
from ndsl.dsl.gt4py import BACKWARD, FORWARD, PARALLEL, computation, cos, exp
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval, log, sin
from ndsl.dsl.typing import (
    Bool,
    BoolFieldIJ,
    Float,
    FloatField,
    FloatFieldIJ,
    Int,
    IntFieldIJ,
    IntFieldK,
)
from ndsl.grid import GridData
from ndsl.logging import ndsl_log
from ndsl.stencils import copy
from pyshield._config import (
    PHYSICS_PACKAGES,
    TRACER_DIM,
    FloatFieldTracer,
    PhysicsConfig,
)
from pyshield.physics_state import PhysicsState
from pyshield.radiation import RTE_RRTMGPConfig, RTE_RRTMGPDriver, RTE_RRTMGPState
from pyshield.stencils.get_phi_fv3 import get_phi_fv3
from pyshield.stencils.get_prs_fv3 import get_prs_fv3
from pyshield.stencils.gfdl_cld_microphysics import (
    GFDLCloudMicrophysics,
    GFDLCloudMicrophysicsState,
    GFDLCloudMPConfig,
)
from pyshield.stencils.gfs_microphysics import GFSMicrophysics
from pyshield.stencils.pbl import PBLConfig, SATMEDMFVDiffState, ScaleAwareTKEMoistEDMF
from pyshield.stencils.shallow_convection import (
    SAMFShalConvState,
    ScaleAwareMassFluxShallowConvection,
    ShallowConvectionConfig,
)
from pyshield.stencils.surface import SurfaceConfig, SurfaceLayer, SurfaceState


def calc_sigma(ak: np.ndarray, bk: np.ndarray, k_toa: int):
    return (ak + bk * physcons.P_REF - ak[k_toa]) / (physcons.P_REF - ak[k_toa])


def set_sst(tsea, gridlat):
    from __externals__ import tmax, tmin

    with computation(FORWARD), interval(0, 1):
        tsea = tmax - ((tmax - tmin) * sin(gridlat) ** 2)


def calc_p_lay_hydro(
    p_level: FloatField,
    p_layer: FloatField,
):
    """
    stencil to calculate hydrostatic layer mean pressure
    from level (interface) pressure
    """
    with computation(PARALLEL), interval(...):
        p_layer = (p_level[0, 0, 1] - p_level) / log(p_level[0, 0, 1] / p_level)


def calc_p_lay_nonhydro(
    delp: FloatField,
    delz: FloatField,
    t_layer: FloatField,
    qvapor: FloatField,
    p_layer: FloatField,
):
    """
    stencil to calculate nonhydrostatic layer mean pressure
    Assumes delp has condensates subtracted out
    """
    with computation(PARALLEL), interval(0, -1):
        tmp = constants.RDGAS * t_layer * (1 + constants.ZVIR * qvapor)
        p_layer = delp / (constants.GRAV * delz) * tmp


def atmos_phys_driver_statein(
    prsik: FloatField,
    phii: FloatField,
    prsi: FloatField,
    delz: FloatField,
    delp: FloatField,
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qo3mr: FloatField,
    qsgs_tke: FloatField,
    qcld: FloatField,
    pt: FloatField,
    dm: FloatField,
    pgr: FloatFieldIJ,
):
    from __externals__ import nwat, pk0inv, pktop, ptop

    with computation(BACKWARD), interval(...):
        phii = 0.0

    with computation(BACKWARD), interval(0, -1):
        phii = phii[0, 0, 1] - delz * constants.GRAV

    with computation(PARALLEL), interval(...):
        prsik = 1.0e25
        qvapor = qvapor * delp
        qliquid = qliquid * delp
        qrain = qrain * delp
        qice = qice * delp
        qsnow = qsnow * delp
        qgraupel = qgraupel * delp
        qo3mr = qo3mr * delp
    # The following needs to execute after the above (TODO)
    with computation(PARALLEL), interval(...):
        if nwat == 6:
            delp = delp - qliquid - qrain - qice - qsnow - qgraupel

    with computation(PARALLEL), interval(0, 1):
        prsi = ptop

    with computation(FORWARD), interval(1, None):
        prsi = prsi[0, 0, -1] + delp[0, 0, -1]

    with computation(PARALLEL), interval(0, -1):
        prsik = log(prsi)
        qvapor = qvapor / delp
        qliquid = qliquid / delp
        qrain = qrain / delp
        qice = qice / delp
        qsnow = qsnow / delp
        qgraupel = qgraupel / delp
        qo3mr = qo3mr / delp
        qsgs_tke = qsgs_tke / delp

    with computation(FORWARD), interval(-1, None):
        prsik = log(prsi)
        pgr = prsi

    with computation(PARALLEL), interval(0, 1):
        prsik = log(ptop)

    with computation(PARALLEL), interval(0, -1):
        qgrs_rad = max(physcons.QMIN, qvapor)
        rTv = constants.RDGAS * pt * (1.0 + constants.ZVIR * qgrs_rad)
        dm = delp[0, 0, 0]
        delp = dm * rTv / (phii[0, 0, 0] - phii[0, 0, 1])
        delp = min(delp, prsi[0, 0, 1] - 0.01 * dm)
        delp = max(delp, prsi + 0.01 * dm)

    with computation(PARALLEL), interval(-1, None):
        prsik = exp(constants.KAPPA * prsik) * pk0inv

    with computation(PARALLEL), interval(0, 1):
        prsik = pktop


def flip_fields(
    u: FloatField,
    v: FloatField,
    w: FloatField,
    t: FloatField,
    q: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qo3mr: FloatField,
    qsgs_tke: FloatField,
    qcld: FloatField,
    prsl: FloatField,
    delp: FloatField,
    prsi: FloatField,
    prsik: FloatField,
    prslk: FloatField,
    phil: FloatField,
    phii: FloatField,
    u1: FloatField,
    v1: FloatField,
    w1: FloatField,
    t1: FloatField,
    q1: FloatField,
    qliquid1: FloatField,
    qrain1: FloatField,
    qice1: FloatField,
    qsnow1: FloatField,
    qgraupel1: FloatField,
    qo3mr1: FloatField,
    qsgs_tke1: FloatField,
    qcld1: FloatField,
    prsl1: FloatField,
    delp1: FloatField,
    prsi1: FloatField,
    prsik1: FloatField,
    prslk1: FloatField,
    phil1: FloatField,
    phii1: FloatField,
    level_flip: IntFieldK,
    layer_flip: IntFieldK,
):
    with computation(PARALLEL), interval(...):
        u1 = u[0, 0, layer_flip]
        v1 = v[0, 0, layer_flip]
        w1 = w[0, 0, layer_flip]
        t1 = t[0, 0, layer_flip]
        q1 = q[0, 0, layer_flip]
        qliquid1 = qliquid[0, 0, layer_flip]
        qrain1 = qrain[0, 0, layer_flip]
        qice1 = qice[0, 0, layer_flip]
        qsnow1 = qsnow[0, 0, layer_flip]
        qgraupel1 = qgraupel[0, 0, layer_flip]
        qo3mr1 = qo3mr[0, 0, layer_flip]
        qsgs_tke1 = qsgs_tke[0, 0, layer_flip]
        qcld1 = qcld[0, 0, layer_flip]
        prsl1 = prsl[0, 0, layer_flip]
        delp1 = delp[0, 0, layer_flip]
        prsi1 = prsi[0, 0, level_flip]
        prslk1 = prslk[0, 0, layer_flip]
        prsik1 = prsik[0, 0, level_flip]
        phil1 = phil[0, 0, layer_flip]
        phii1 = phii[0, 0, level_flip]


def start_physics(
    dvdt: FloatField,
    dudt: FloatField,
    dtdt: FloatField,
    dqdt: FloatFieldTracer,
    dusfc: FloatFieldIJ,
    dvsfc: FloatFieldIJ,
    dtsfc: FloatFieldIJ,
    dqsfc: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        dusfc = 0.0
        dvsfc = 0.0
        dtsfc = 0.0
        dqsfc = 0.0
    with computation(FORWARD), interval(...):
        dvdt = 0.0
        dudt = 0.0
        dtdt = 0.0
        dqdt = 0.0


def pack_tracers(
    qgrs: FloatFieldTracer,
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qo3mr: FloatField,
    qsgs_tke: FloatField,
    qcld: FloatField,
):
    from __externals__ import (
        ntcw,
        ntgraupel,
        ntiw,
        ntke,
        ntliquid,
        nto3,
        ntrain,
        ntsnow,
        ntvap,
    )

    with computation(PARALLEL), interval(...):
        qgrs[0, 0, 0][ntvap] = qvapor
        qgrs[0, 0, 0][ntliquid] = qliquid
        qgrs[0, 0, 0][ntrain] = qrain
        qgrs[0, 0, 0][ntiw] = qice
        qgrs[0, 0, 0][ntsnow] = qsnow
        qgrs[0, 0, 0][ntgraupel] = qgraupel
        qgrs[0, 0, 0][nto3] = qo3mr
        qgrs[0, 0, 0][ntke] = qsgs_tke
        qgrs[0, 0, 0][ntcw] = qcld


def copy_to_radiation(
    prsi: FloatField,
    prsl: FloatField,
    pt: FloatField,
    tsfc: FloatFieldIJ,
    qvapor: FloatField,
    qliquid: FloatField,
    qice: FloatField,
    qo3mr: FloatField,
    qcld: FloatField,
    rad_prsi: FloatField,
    rad_prsl: FloatField,
    rad_tlyr: FloatField,
    rad_tsfc: FloatFieldIJ,
    rad_qvapor: FloatField,
    rad_qliquid: FloatField,
    rad_qice: FloatField,
    rad_qo3mr: FloatField,
    rad_qcld: FloatField,
):
    with computation(FORWARD):
        with interval(0, 1):
            rad_tsfc = tsfc
            rad_prsi = prsi
            rad_prsl = prsl
            rad_tlyr = pt
            # Convert gases to molar mixing ratio
            rad_qvapor = (qvapor / (1.0 - qvapor)) * (physcons.MMDRY / physcons.MMVAP)
            rad_qo3mr = qo3mr * physcons.MMDRY / physcons.MMO3
            rad_qliquid = qliquid
            rad_qice = qice

            rad_qcld = qcld
        with interval(1, None):
            rad_prsi = prsi
            rad_prsl = prsl
            rad_tlyr = pt
            # Convert gases to molar mixing ratio
            rad_qvapor = (qvapor / (1.0 - qvapor)) * (physcons.MMDRY / physcons.MMVAP)
            rad_qo3mr = qo3mr * physcons.MMDRY / physcons.MMO3
            rad_qliquid = qliquid
            rad_qice = qice
            rad_qcld = qcld


# TODO: combine with interpolate_radiation stencil
def copy_from_radiation(
    rad_htrsw: FloatField,
    rad_htrlw: FloatField,
    rad_swflux_up: FloatField,
    rad_swflux_down: FloatField,
    rad_lwflux_up: FloatField,
    rad_lwflux_down: FloatField,
    htrsw: FloatField,
    htrlw: FloatField,
    htrsw_noflip: FloatField,
    htrlw_noflip: FloatField,
    swflux_up: FloatField,
    swflux_down: FloatField,
    lwflux_up: FloatField,
    lwflux_down: FloatField,
    layer_flip: IntFieldK,
    level_flip: IntFieldK,
):
    with computation(PARALLEL), interval(...):
        htrsw = rad_htrsw[0, 0, layer_flip]
        htrlw = rad_htrlw[0, 0, layer_flip]
        swflux_up = rad_swflux_up[0, 0, level_flip]
        swflux_down = rad_swflux_down[0, 0, level_flip]
        lwflux_up = rad_lwflux_up[0, 0, level_flip]
        lwflux_down = rad_lwflux_down[0, 0, level_flip]
        # TODO: once radiation state is required these become unnecessary
        htrsw_noflip = rad_htrsw
        htrlw_noflip = rad_htrlw


def interpolate_radiation(
    latitude: FloatFieldIJ,
    xlon: FloatFieldIJ,
    coszen: FloatFieldIJ,
    t_sea: FloatFieldIJ,
    t_surface: FloatField,
    t_surface_longwave: FloatField,
    sfcemis: FloatFieldIJ,
    sfcdlw: FloatField,
    sfcnsw: FloatFieldIJ,
    sfcdsw: FloatField,
    sfcnirbmu: FloatFieldIJ,
    sfcnirdfu: FloatFieldIJ,
    sfcvisbmu: FloatFieldIJ,
    sfcvisdfu: FloatFieldIJ,
    sfcnirbmd: FloatFieldIJ,
    sfcnirdfd: FloatFieldIJ,
    sfcvisbmd: FloatFieldIJ,
    sfcvisdfd: FloatFieldIJ,
    swh: FloatField,
    swhc: FloatField,
    hlw: FloatField,
    hlwc: FloatField,
    dtdt: FloatField,
    dtdtc: FloatField,
    adjsfcdlw: FloatFieldIJ,
    adjsfculw: FloatFieldIJ,
    adjsfcnsw: FloatFieldIJ,
    adjsfcdsw: FloatFieldIJ,
    adjnirbmu: FloatFieldIJ,
    adjnirdfu: FloatFieldIJ,
    adjvisbmu: FloatFieldIJ,
    adjvisdfu: FloatFieldIJ,
    adjnirbmd: FloatFieldIJ,
    adjnirdfd: FloatFieldIJ,
    adjvisbmd: FloatFieldIJ,
    adjvisdfd: FloatFieldIJ,
    xcosz: FloatFieldIJ,
    xmu: FloatFieldIJ,
    solhr: Float,
    slag: Float,
    sdec: Float,
    cdec: Float,
):
    """
        fits radiative fluxes and heating rates from a coarse radiation
        calc time interval into model's more frequent time steps.
        Assumes k=0 is the surface
        Fortran name is dcyc2t3
        TODO: t_surface, t_surface_longwave, and fluxes should all be true
        2D variables ideally
    !  ====================  defination of variables  ====================  !
    !                                                                       !
    !  inputs:                                                              !
    !     solhr        - real, forecast time in 24-hour form (hr)           !
    !     slag         - real, equation of time in radians                  !
    !     sdec, cdec   - real, sin and cos of the solar declination angle   !
    !     sinlat(im), coslat(im):                                           !
    !                  - real, sin and cos of latitude                      !
    !     xlon   (im)  - real, longitude in radians                         !
    !     coszen (im)  - real, avg of cosz over daytime sw call interval    !
    !     tsea   (im)  - real, ground surface temperature (k)               !
    !     tf     (im)  - real, surface air (layer 1) temperature (k)        !
    !     sfcemis(im)  - real, surface emissivity (fraction)                !
    !     tsflw  (im)  - real, sfc air (layer 1) temp in k saved in lw call !
    !     sfcdsw (im)  - real, total sky sfc downward sw flux ( w/m**2 )    !
    !     sfcnsw (im)  - real, total sky sfc net sw into ground (w/m**2)    !
    !     sfcdlw (im)  - real, total sky sfc downward lw flux ( w/m**2 )    !
    !     swh(ix,levs) - real, total sky sw heating rates ( k/s )           !
    !     swhc(ix,levs) - real, clear sky sw heating rates ( k/s )          !
    !     hlw(ix,levs) - real, total sky lw heating rates ( k/s )           !
    !     hlwc(ix,levs) - real, clear sky lw heating rates ( k/s )          !
    !     sfcnirbmu(im)- real, tot sky sfc nir-beam sw upward flux (w/m2)   !
    !     sfcnirdfu(im)- real, tot sky sfc nir-diff sw upward flux (w/m2)   !
    !     sfcvisbmu(im)- real, tot sky sfc uv+vis-beam sw upward flux (w/m2)!
    !     sfcvisdfu(im)- real, tot sky sfc uv+vis-diff sw upward flux (w/m2)!
    !     sfcnirbmd(im)- real, tot sky sfc nir-beam sw downward flux (w/m2) !
    !     sfcnirdfd(im)- real, tot sky sfc nir-diff sw downward flux (w/m2) !
    !     sfcvisbmd(im)- real, tot sky sfc uv+vis-beam sw dnward flux (w/m2)!
    !     sfcvisdfd(im)- real, tot sky sfc uv+vis-diff sw dnward flux (w/m2)!
    !     ix, im       - integer, horiz. dimention and num of used points   !
    !     levs         - integer, vertical layer dimension                  !
    !                                                                       !
    !  input/output:                                                        !
    !     dtdt(im,levs)- real, model time step adjusted total radiation     !
    !                          heating rates ( k/s )                        !
    !     dtdtc(im,levs)- real, model time step adjusted clear sky radiation!
    !                          heating rates ( k/s )                        !
    !                                                                       !
    !  outputs:                                                             !
    !     adjsfcdsw(im)- real, time step adjusted sfc dn sw flux (w/m**2)   !
    !     adjsfcnsw(im)- real, time step adj sfc net sw into ground (w/m**2)!
    !     adjsfcdlw(im)- real, time step adjusted sfc dn lw flux (w/m**2)   !
    !     adjsfculw(im)- real, sfc upward lw flux at current time (w/m**2)  !
    !     adjnirbmu(im)- real, t adj sfc nir-beam sw upward flux (w/m2)     !
    !     adjnirdfu(im)- real, t adj sfc nir-diff sw upward flux (w/m2)     !
    !     adjvisbmu(im)- real, t adj sfc uv+vis-beam sw upward flux (w/m2)  !
    !     adjvisdfu(im)- real, t adj sfc uv+vis-diff sw upward flux (w/m2)  !
    !     adjnirbmd(im)- real, t adj sfc nir-beam sw downward flux (w/m2)   !
    !     adjnirdfd(im)- real, t adj sfc nir-diff sw downward flux (w/m2)   !
    !     adjvisbmd(im)- real, t adj sfc uv+vis-beam sw dnward flux (w/m2)  !
    !     adjvisdfd(im)- real, t adj sfc uv+vis-diff sw dnward flux (w/m2)  !
    !     xmu   (im)   - real, time step zenith angle adjust factor for sw  !
    !     xcosz (im)   - real, cosine of zenith angle at current time step  !
    !                                                                       !
    !  ====================    end of description    =====================  !
    """
    from __externals__ import daily_mean

    with computation(FORWARD), interval(0, 1):
        sinlat = sin(latitude)
        coslat = cos(latitude)
        cns = constants.PI * (solhr - 12.0) / 12.0 + slag

        # adjust sfc downward lw flux to account for t changes in layer 1
        tem1 = (t_surface / t_surface_longwave) ** 2
        adjsfcdlw = sfcdlw * tem1 * tem1

        # compute sfc upward lw flux from current sfc temp
        adjsfculw = sfcemis * constants.SBC * (t_sea) ** 4 + (1.0 - sfcemis) * adjsfcdlw

        # sw time-step adjustment
        ss = sinlat * sdec
        cc = coslat * cdec
        ch = cc * cos(xlon + cns)
        xcosz = ch + ss

        if daily_mean:
            # replace cosz with daily mean value
            xcosz = coszen

        if (xcosz > physcons.F_EPS) and (coszen > physcons.F_EPS):
            xmu = xcosz / coszen
        else:
            xmu = 0.0

        # adjust sfc net and downward sw fluxes for zenith angle changes
        adjsfcnsw = sfcnsw * xmu
        adjsfcdsw = sfcdsw * xmu
        adjnirbmu = sfcnirbmu * xmu
        adjnirdfu = sfcnirdfu * xmu
        adjvisbmu = sfcvisbmu * xmu
        adjvisdfu = sfcvisdfu * xmu
        adjnirbmd = sfcnirbmd * xmu
        adjnirdfd = sfcnirdfd * xmu
        adjvisbmd = sfcvisbmd * xmu
        adjvisdfd = sfcvisdfd * xmu

    with computation(FORWARD), interval(...):
        # adjust sw heating rates with zenith angle change and add with
        # lw heating to temperature tendency
        dtdt = dtdt + swh * xmu + hlw
        dtdtc = dtdtc + swhc * xmu + hlwc


def prepare_sfc(
    u1: FloatFieldIJ,
    v1: FloatFieldIJ,
    t1: FloatFieldIJ,
    prsl1: FloatFieldIJ,
    prsik1: FloatFieldIJ,
    prslk1: FloatFieldIJ,
    qvapor1: FloatFieldIJ,
    phil1: FloatFieldIJ,
    ps: FloatFieldIJ,
    sfcdlw: FloatFieldIJ,
    sfcdsw: FloatFieldIJ,
    sfcnsw: FloatFieldIJ,
    physics_u: FloatField,
    physics_v: FloatField,
    physics_t: FloatField,
    physics_prsl: FloatField,
    physics_prsik: FloatField,
    physics_prslk: FloatField,
    physics_qvapor: FloatField,
    physics_phil: FloatField,
    physics_pgr: FloatFieldIJ,
    physics_sfcdlw: FloatFieldIJ,
    physics_sfcdsw: FloatFieldIJ,
    physics_sfcnsw: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        u1 = physics_u
        v1 = physics_v
        t1 = physics_t
        prsl1 = physics_prsl
        prsik1 = physics_prsik
        prslk1 = physics_prslk
        qvapor1 = physics_qvapor
        phil1 = physics_phil
        ps = physics_pgr
        sfcdlw = physics_sfcdlw
        sfcdsw = physics_sfcdsw
        sfcnsw = physics_sfcnsw


# TODO: once surface_state is a required argument we can remove this
def update_from_sfc(
    sfc_wind: FloatFieldIJ,
    sfc_uustar: FloatFieldIJ,
    sfc_ffmm: FloatFieldIJ,
    sfc_ffhh: FloatFieldIJ,
    sfc_f10m: FloatFieldIJ,
    sfc_emis: FloatFieldIJ,
    sfc_srflg: BoolFieldIJ,
    sfc_hice: FloatFieldIJ,
    sfc_fice: FloatFieldIJ,
    sfc_tisfc: FloatFieldIJ,
    sfc_weasd: FloatFieldIJ,
    sfc_tprcp: FloatFieldIJ,
    sfc_rb: FloatFieldIJ,
    sfc_stress: FloatFieldIJ,
    sfc_hflx: FloatFieldIJ,
    sfc_evap: FloatFieldIJ,
    phys_wind: FloatFieldIJ,
    phys_uustar: FloatFieldIJ,
    phys_ffmm: FloatFieldIJ,
    phys_ffhh: FloatFieldIJ,
    phys_f10m: FloatFieldIJ,
    phys_emis: FloatFieldIJ,
    phys_srflg: BoolFieldIJ,
    phys_hice: FloatFieldIJ,
    phys_fice: FloatFieldIJ,
    phys_tisfc: FloatFieldIJ,
    phys_weasd: FloatFieldIJ,
    phys_tprcp: FloatFieldIJ,
    phys_rb: FloatFieldIJ,
    phys_stress: FloatFieldIJ,
    phys_hflx: FloatFieldIJ,
    phys_evap: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        phys_wind = sfc_wind
        phys_uustar = sfc_uustar
        phys_ffmm = sfc_ffmm
        phys_ffhh = sfc_ffhh
        phys_f10m = sfc_f10m
        phys_emis = sfc_emis
        phys_srflg = sfc_srflg
        phys_hice = sfc_hice
        phys_fice = sfc_fice
        phys_tisfc = sfc_tisfc
        phys_weasd = sfc_weasd
        phys_tprcp = sfc_tprcp
        phys_rb = sfc_rb
        phys_stress = sfc_stress
        phys_hflx = sfc_hflx
        phys_evap = sfc_evap


def fill_pbl_state(
    pbl_u: FloatField,
    pbl_v: FloatField,
    pbl_t: FloatField,
    pbl_q: FloatFieldTracer,
    pbl_prsl: FloatField,
    pbl_delta: FloatField,
    pbl_phii: FloatField,
    pbl_phil: FloatField,
    pbl_prsi: FloatField,
    pbl_prslk: FloatField,
    pbl_hsw: FloatField,
    pbl_hlw: FloatField,
    pbl_islimsk: IntFieldIJ,
    pbl_kinver: IntFieldIJ,
    pbl_xmu: FloatFieldIJ,
    pbl_psik: FloatFieldIJ,
    pbl_rbsoil: FloatFieldIJ,
    pbl_zorl: FloatFieldIJ,
    pbl_tsea: FloatFieldIJ,
    pbl_ffmm: FloatFieldIJ,
    pbl_ffhh: FloatFieldIJ,
    pbl_hflx: FloatFieldIJ,
    pbl_evap: FloatFieldIJ,
    pbl_stress: FloatFieldIJ,
    pbl_wind: FloatFieldIJ,
    physics_u: FloatField,
    physics_v: FloatField,
    physics_t: FloatField,
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qo3mr: FloatField,
    qsgs_tke: FloatField,
    qcld: FloatField,
    delp: FloatField,
    phii: FloatField,
    phil: FloatField,
    prsl: FloatField,
    prsi: FloatField,
    prsik: FloatField,
    prslk: FloatField,
    hsw: FloatField,
    hlw: FloatField,
    rbsoil: FloatFieldIJ,
    tsea: FloatFieldIJ,
    ffmm: FloatFieldIJ,
    ffhh: FloatFieldIJ,
    hflx: FloatFieldIJ,
    evap: FloatFieldIJ,
    wind: FloatFieldIJ,
    stress: FloatFieldIJ,
    xmu: FloatFieldIJ,
):
    from __externals__ import levs

    with computation(FORWARD), interval(0, 1):
        pbl_psik = prsik
        pbl_kinver = levs
        pbl_rbsoil = rbsoil
        pbl_tsea = tsea
        pbl_ffmm = ffmm
        pbl_ffhh = ffhh
        pbl_hflx = hflx
        pbl_evap = evap
        pbl_wind = wind
        pbl_stress = stress
        pbl_xmu = xmu
        # These will be filled in as they become available from prior schemes
        pbl_islimsk = 0  # sea-only for now
        pbl_zorl = 0.0
    with computation(PARALLEL), interval(...):
        pbl_u = physics_u
        pbl_v = physics_v
        pbl_t = physics_t
        # TODO: these should not be hardcoded
        pbl_q[0, 0, 0][0] = qvapor
        pbl_q[0, 0, 0][1] = qliquid
        pbl_q[0, 0, 0][2] = qrain
        pbl_q[0, 0, 0][3] = qice
        pbl_q[0, 0, 0][4] = qsnow
        pbl_q[0, 0, 0][5] = qgraupel
        pbl_q[0, 0, 0][6] = qo3mr
        pbl_q[0, 0, 0][7] = qsgs_tke
        pbl_q[0, 0, 0][8] = qcld
        pbl_delta = delp
        pbl_phii = phii
        pbl_phil = phil
        pbl_prsi = prsi
        pbl_prslk = prslk
        pbl_prsl = prsl
        pbl_hsw = hsw
        pbl_hlw = hlw


@gtfunction
def forward_euler(q_t0, q_dt, dt):
    return q_t0 + q_dt * dt


def results_from_pbl(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qo3mr: FloatField,
    qsgs_tke: FloatField,
    qcld: FloatField,
    pt: FloatField,
    ua: FloatField,
    va: FloatField,
    hpbl: FloatFieldIJ,
    pbl_rtg: FloatFieldTracer,
    pbl_dtdt: FloatField,
    pbl_du: FloatField,
    pbl_dv: FloatField,
    pbl_hpbl: FloatFieldIJ,
    dt: Float,
):
    with computation(FORWARD), interval(0, 1):
        hpbl = pbl_hpbl
    # TODO: Eventually we'll want the tracers to be in 4D fields for the physics
    # TODO: Use config variables instead of hardcoding indices, and in one pass
    with computation(PARALLEL), interval(...):
        qvapor = forward_euler(qvapor, pbl_rtg[0, 0, 0][0], dt)
        qliquid = forward_euler(qliquid, pbl_rtg[0, 0, 0][1], dt)
        qrain = forward_euler(qrain, pbl_rtg[0, 0, 0][2], dt)
        qice = forward_euler(qice, pbl_rtg[0, 0, 0][3], dt)
        qsnow = forward_euler(qsnow, pbl_rtg[0, 0, 0][4], dt)
        qgraupel = forward_euler(qgraupel, pbl_rtg[0, 0, 0][5], dt)
        qo3mr = forward_euler(qo3mr, pbl_rtg[0, 0, 0][6], dt)
        qsgs_tke = forward_euler(qsgs_tke, pbl_rtg[0, 0, 0][7], dt)
        qcld = forward_euler(qcld, pbl_rtg[0, 0, 0][8], dt)
        pt = forward_euler(pt, pbl_dtdt[0, 0, 0], dt)
        ua = forward_euler(ua, pbl_du[0, 0, 0], dt)
        va = forward_euler(va, pbl_dv[0, 0, 0], dt)


def fill_shalconv_state(
    shalconv_t1: FloatField,
    shalconv_u1: FloatField,
    shalconv_v1: FloatField,
    shalconv_qtr: FloatFieldTracer,
    shalconv_dot: FloatField,
    shalconv_hpbl: FloatFieldIJ,
    shalconv_prslp: FloatField,
    shalconv_phil: FloatField,
    shalconv_delp: FloatField,
    shalconv_psp: FloatFieldIJ,
    shalconv_kcnv: IntFieldIJ,
    shalconv_garea: FloatFieldIJ,
    shalconv_islimsk: IntFieldIJ,
    physics_q1: FloatField,
    physics_t1: FloatField,
    physics_u1: FloatField,
    physics_v1: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qo3mr: FloatField,
    qsgs_tke: FloatField,
    qcld: FloatField,
    physics_dot: FloatField,
    physics_hpbl: FloatFieldIJ,
    physics_prslp: FloatField,
    physics_phil: FloatField,
    physics_delp: FloatField,
    physics_psp: FloatFieldIJ,
    physics_garea: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        shalconv_hpbl = physics_hpbl
        shalconv_psp = physics_psp
        shalconv_garea = physics_garea
        # These will be filled in as they're enabled by previous schemes
        shalconv_kcnv = 0  # no deep convection
        shalconv_islimsk = 0  # sea-only for now
    with computation(PARALLEL), interval(...):
        shalconv_t1 = physics_t1
        shalconv_u1 = physics_u1
        shalconv_v1 = physics_v1
        shalconv_dot = physics_dot
        shalconv_prslp = physics_prslp
        shalconv_phil = physics_phil
        shalconv_delp = physics_delp

        # TODO: These should not be hardcoded
        shalconv_qtr[0, 0, 0][0] = physics_q1
        shalconv_qtr[0, 0, 0][1] = qliquid
        shalconv_qtr[0, 0, 0][2] = qrain
        shalconv_qtr[0, 0, 0][3] = qice
        shalconv_qtr[0, 0, 0][4] = qsnow
        shalconv_qtr[0, 0, 0][5] = qgraupel
        shalconv_qtr[0, 0, 0][6] = qo3mr
        shalconv_qtr[0, 0, 0][7] = qsgs_tke
        shalconv_qtr[0, 0, 0][8] = qsgs_tke
        shalconv_qtr[0, 0, 0][9] = qcld


def results_from_shalconv(
    physics_t: FloatField,
    physics_u: FloatField,
    physics_v: FloatField,
    physics_qvapor: FloatField,
    physics_qliquid: FloatField,
    physics_qrain: FloatField,
    physics_qice: FloatField,
    physics_qsnow: FloatField,
    physics_qgraupel: FloatField,
    physics_qo3mr: FloatField,
    physics_qsgs_tke: FloatField,
    phtsics_qcld: FloatField,
    shalconv_t1: FloatField,
    shalconv_u1: FloatField,
    shalconv_v1: FloatField,
    shalconv_qtr: FloatFieldTracer,
):
    with computation(PARALLEL), interval(...):
        physics_t = shalconv_t1
        physics_u = shalconv_u1
        physics_v = shalconv_v1
        physics_qvapor = shalconv_qtr[0, 0, 0][0]
        physics_qliquid = shalconv_qtr[0, 0, 0][1]
        physics_qrain = shalconv_qtr[0, 0, 0][2]
        physics_qice = shalconv_qtr[0, 0, 0][3]
        physics_qsnow = shalconv_qtr[0, 0, 0][4]
        physics_qgraupel = shalconv_qtr[0, 0, 0][5]
        physics_qo3mr = shalconv_qtr[0, 0, 0][6]
        physics_qsgs_tke = shalconv_qtr[0, 0, 0][7]
        phtsics_qcld = shalconv_qtr[0, 0, 0][8]


def prepare_gfs_microphysics(
    dz: FloatField,
    phii: FloatField,
    wmp: FloatField,
    omga: FloatField,
    qvapor: FloatField,
    pt: FloatField,
    delp: FloatField,
    u_dt: FloatField,
    v_dt: FloatField,
    pt_dt: FloatField,
    qv_dt: FloatField,
    ql_dt: FloatField,
    qr_dt: FloatField,
    qi_dt: FloatField,
    qs_dt: FloatField,
    qg_dt: FloatField,
    qa_dt: FloatField,
):
    with computation(BACKWARD), interval(...):
        dz = (phii[0, 0, 1] - phii[0, 0, 0]) * constants.RGRAV
        wmp = (
            -omga
            * (1.0 + constants.ZVIR * qvapor)
            * pt
            / delp
            * (constants.RDGAS * constants.RGRAV)
        )
    with computation(PARALLEL), interval(...):
        u_dt = 0.0
        v_dt = 0.0
        pt_dt = 0.0
        qv_dt = 0.0
        ql_dt = 0.0
        qr_dt = 0.0
        qi_dt = 0.0
        qs_dt = 0.0
        qg_dt = 0.0
        qa_dt = 0.0


def update_physics_state_with_tendencies(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qcld: FloatField,
    pt: FloatField,
    ua: FloatField,
    va: FloatField,
    qv_dt: FloatField,
    ql_dt: FloatField,
    qr_dt: FloatField,
    qi_dt: FloatField,
    qs_dt: FloatField,
    qg_dt: FloatField,
    qa_dt: FloatField,
    pt_dt: FloatField,
    udt: FloatField,
    vdt: FloatField,
    physics_updated_specific_humidity: FloatField,
    physics_updated_qliquid: FloatField,
    physics_updated_qrain: FloatField,
    physics_updated_qice: FloatField,
    physics_updated_qsnow: FloatField,
    physics_updated_qgraupel: FloatField,
    physics_updated_cloud_fraction: FloatField,
    physics_updated_pt: FloatField,
    physics_updated_ua: FloatField,
    physics_updated_va: FloatField,
    dt: Float,
):
    # TODO: Change back to parallel once self-assigns are allowed with 0 offset
    with computation(FORWARD), interval(...):
        physics_updated_specific_humidity = forward_euler(qvapor, qv_dt, dt)
        physics_updated_qliquid = forward_euler(qliquid, ql_dt, dt)
        physics_updated_qrain = forward_euler(qrain, qr_dt, dt)
        physics_updated_qice = forward_euler(qice, qi_dt, dt)
        physics_updated_qsnow = forward_euler(qsnow, qs_dt, dt)
        physics_updated_qgraupel = forward_euler(qgraupel, qg_dt, dt)
        physics_updated_cloud_fraction = forward_euler(qcld, qa_dt, dt)
        physics_updated_pt = forward_euler(pt, pt_dt, dt)
        physics_updated_ua = forward_euler(ua, udt, dt)
        physics_updated_va = forward_euler(va, vdt, dt)


def prepare_gfdl_cld_microphysics(
    delp: FloatField,
    delz: FloatField,
    ua: FloatField,
    va: FloatField,
    wa: FloatField,
    pt: FloatField,
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qcld: FloatField,
    physics_delp: FloatField,
    physics_delz: FloatField,
    physics_ua: FloatField,
    physics_va: FloatField,
    physics_wa: FloatField,
    physics_pt: FloatField,
    physics_qvapor: FloatField,
    physics_qliquid: FloatField,
    physics_qrain: FloatField,
    physics_qice: FloatField,
    physics_qsnow: FloatField,
    physics_qgraupel: FloatField,
    physics_qcld: FloatField,
    water: FloatFieldIJ,
    rain: FloatFieldIJ,
    ice: FloatFieldIJ,
    snow: FloatFieldIJ,
    graupel: FloatFieldIJ,
    prefluxw: FloatField,
    prefluxr: FloatField,
    prefluxi: FloatField,
    prefluxs: FloatField,
    prefluxg: FloatField,
    qnl1: FloatField,
    qni1: FloatField,
):
    with computation(FORWARD), interval(0, 1):
        water = 0.0
        rain = 0.0
        ice = 0.0
        snow = 0.0
        graupel = 0.0
    with computation(PARALLEL), interval(...):
        delp = physics_delp
        delz = physics_delz
        ua = physics_ua
        va = physics_va
        wa = physics_wa
        pt = physics_pt
        qvapor = physics_qvapor
        qliquid = physics_qliquid
        qrain = physics_qrain
        qice = physics_qice
        qsnow = physics_qsnow
        qgraupel = physics_qgraupel
        qcld = physics_qcld
        prefluxw = 0.0
        prefluxr = 0.0
        prefluxi = 0.0
        prefluxs = 0.0
        prefluxg = 0.0
        qnl1 = 0.0
        qni1 = 0.0


def post_gfdl_cld_mp(
    delp: FloatField,
    delz: FloatField,
    ua: FloatField,
    va: FloatField,
    wa: FloatField,
    pt: FloatField,
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qice: FloatField,
    qsnow: FloatField,
    qgraupel: FloatField,
    qcloud: FloatField,
    physics_delp: FloatField,
    physics_delz: FloatField,
    physics_ua: FloatField,
    physics_va: FloatField,
    physics_wa: FloatField,
    physics_pt: FloatField,
    physics_qvapor: FloatField,
    physics_qliquid: FloatField,
    physics_qrain: FloatField,
    physics_qice: FloatField,
    physics_qsnow: FloatField,
    physics_qgraupel: FloatField,
    physics_qcloud: FloatField,
    water: FloatFieldIJ,
    rain: FloatFieldIJ,
    ice: FloatFieldIJ,
    snow: FloatFieldIJ,
    graupel: FloatFieldIJ,
    rain1: FloatFieldIJ,
    physics_updated_qo3mr: FloatField,
    physics_updated_qtke: FloatField,
    qo3mr: FloatField,
    qsgs_tke: FloatField,
):
    from __externals__ import dtp

    with computation(FORWARD), interval(0, 1):
        tem = dtp * physcons.CON_P001 * physcons.CON_DAY
        water = water * tem
        rain = rain * tem
        ice = ice * tem
        snow = snow * tem
        graupel = graupel * tem
        rain1 = water + rain + ice + snow + graupel
    with computation(PARALLEL), interval(...):
        physics_delp = delp
        physics_delz = delz
        physics_ua = ua
        physics_va = va
        physics_wa = wa
        physics_pt = pt
        physics_qvapor = qvapor
        physics_qliquid = qliquid
        physics_qrain = qrain
        physics_qice = qice
        physics_qsnow = qsnow
        physics_qgraupel = qgraupel
        physics_qcloud = qcloud
        physics_updated_qo3mr = qo3mr
        physics_updated_qtke = qsgs_tke


class Physics:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        grid_data: GridData,
        namelist: PhysicsConfig,
        rad_config: RTE_RRTMGPConfig = None,
        pre_radiation=False,
        sfc_config: SurfaceConfig = None,
        pbl_config: PBLConfig = None,
        sc_config: ShallowConvectionConfig = None,
        gfdl_cld_mp_config: GFDLCloudMPConfig = None,
    ):
        schemes = [scheme.value for scheme in namelist.schemes]
        for scheme in schemes:
            if scheme not in PHYSICS_PACKAGES:  # type: ignore
                raise NotImplementedError(
                    f"{scheme} is not an implemented physics parameterization"
                )
            ndsl_log.info(f"{scheme} enabled")
        orchestrate(
            obj=self,
            config=stencil_factory.config.dace_config,
            dace_compiletime_args=["physics_state"],
        )
        self._ntracers = namelist.ntracers
        if self._ntracers != 9:
            raise NotImplementedError(
                f"ntracers != 9 has not been implemented, got {self._ntracers}"
            )
        self.TRACER_DIM = TRACER_DIM
        self.quantity_factory = quantity_factory
        self.quantity_factory.add_data_dimensions(
            {
                self.TRACER_DIM: self._ntracers,
            }
        )

        grid_indexing = stencil_factory.grid_indexing
        nz = grid_indexing.domain[2]
        npz = nz + 1
        self._setup_statein()
        self._ptop = grid_data.ptop
        self._area = grid_data.area
        self._pktop = (self._ptop / self._p00) ** constants.KAPPA
        self._pk0inv = (1.0 / self._p00) ** constants.KAPPA
        if self._ntracers == 9:  # TODO: implement actual tracer handling eventually
            self._ntliquid = 1
            self._ntrain = 2
            self._ntsnow = 4
            self._ntgraupel = 5
            self._nto3 = 6
            self._ntvap = 0
        self._pre_radiation = pre_radiation
        self._dt_phys = namelist.dt_atmos
        self._prescribe_sst = namelist.prescribe_sst
        self._gridlon = grid_data.lon_agrid
        self._gridlat = grid_data.lat_agrid
        self._nsteps = 0
        self._nsswr = namelist.nsswr
        self._nslwr = namelist.nslwr
        self._hydro_delp = namelist.hydro_delp

        self._level_flip = self.quantity_factory.zeros(
            dims=[K_INTERFACE_DIM], units="", dtype=Int
        )
        self._layer_flip = self.quantity_factory.zeros(
            dims=[K_INTERFACE_DIM], units="", dtype=Int
        )
        for k in range(npz):
            self._level_flip[k] = npz - 1 - 2 * k
            if k < nz:
                self._layer_flip[k] = nz - 1 - 2 * k

        def make_quantity():
            return self.quantity_factory.zeros(
                dims=[I_DIM, J_DIM, K_DIM], units="unknown"
            )

        def make_quantity_2d():
            return quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")

        self._rain1 = quantity_factory.zeros(dims=[I_DIM, J_DIM], units="unknown")
        self._dm3d = make_quantity()
        self._del_gz = make_quantity()
        self._u1 = make_quantity()
        self._v1 = make_quantity()
        self._w1 = make_quantity()
        self._t1 = make_quantity()
        self._prsl1 = make_quantity()
        self._delp1 = make_quantity()
        self._prsi1 = quantity_factory.zeros(
            dims=[I_DIM, J_DIM, K_INTERFACE_DIM], units="unknown"
        )
        self._prsik1 = quantity_factory.zeros(
            dims=[I_DIM, J_DIM, K_INTERFACE_DIM], units="unknown"
        )
        self._prslk1 = make_quantity()
        self._qvapor1 = make_quantity()
        self._qliquid1 = make_quantity()
        self._qrain1 = make_quantity()
        self._qice1 = make_quantity()
        self._qsnow1 = make_quantity()
        self._qgraupel1 = make_quantity()
        self._qo3mr1 = make_quantity()
        self._qsgs_tke1 = make_quantity()
        self._qcld1 = make_quantity()
        self._phil1 = make_quantity()
        self._phii1 = quantity_factory.zeros(
            dims=[I_DIM, J_DIM, K_INTERFACE_DIM], units="unknown"
        )
        # TODO: once surface state is a required argument we don't need these copies
        self._sfcwind = make_quantity_2d()
        self._uustar = make_quantity_2d()
        self._ffmm = make_quantity_2d()
        self._ffhh = make_quantity_2d()
        self._f10m = make_quantity_2d()
        self._emis = make_quantity_2d()
        self._srflg = self.quantity_factory.zeros(
            dims=[I_DIM, J_DIM], units="unknown", dtype=Bool
        )
        self._hice = make_quantity_2d()
        self._fice = make_quantity_2d()
        self._tisfc = make_quantity_2d()
        self._weasd = make_quantity_2d()
        self._tprcp = make_quantity_2d()
        self._rb = make_quantity_2d()
        self._stress = make_quantity_2d()
        self._hflx = make_quantity_2d()
        self._evap = make_quantity_2d()

        self._hrtsw1 = make_quantity()
        self._hrtlw1 = make_quantity()

        self._adjsfcdlw = make_quantity_2d()
        self._adjsfculw = make_quantity_2d()
        self._adjsfcdsw = make_quantity_2d()
        self._adjsfcnsw = make_quantity_2d()
        self._adjnirbmu = make_quantity_2d()
        self._adjnirdfu = make_quantity_2d()
        self._adjvisbmu = make_quantity_2d()
        self._adjvisdfu = make_quantity_2d()
        self._adjnirbmd = make_quantity_2d()
        self._adjnirdfd = make_quantity_2d()
        self._adjvisbmd = make_quantity_2d()
        self._adjvisdfd = make_quantity_2d()
        self._xcosz = make_quantity_2d()
        self._xmu = make_quantity_2d()

        # TODO: Eventually these should come from radiation or be stripped
        self._sfcnirbmu = make_quantity_2d()
        self._sfcnirdfu = make_quantity_2d()
        self._sfcvisbmu = make_quantity_2d()
        self._sfcvisdfu = make_quantity_2d()
        self._sfcnirbmd = make_quantity_2d()
        self._sfcnirdfd = make_quantity_2d()
        self._sfcvisbmd = make_quantity_2d()
        self._sfcvisdfd = make_quantity_2d()

        self._copy_stencil = stencil_factory.from_origin_domain(
            func=copy,
            origin=grid_indexing.origin_full(),
            domain=grid_indexing.domain_full(add=(0, 0, 1)),
        )
        self._start_physics = stencil_factory.from_origin_domain(
            func=start_physics,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )
        self._pack_tracers = stencil_factory.from_origin_domain(
            func=pack_tracers,
            externals={
                "ntiw": namelist.ntiw,
                "ntcw": namelist.ntcw,
                "ntke": namelist.ntke,
                "ntliquid": self._ntliquid,
                "ntrain": self._ntrain,
                "ntsnow": self._ntsnow,
                "ntgraupel": self._ntgraupel,
                "nto3": self._nto3,
                "ntvap": self._ntvap,
            },
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )
        self._get_prs_fv3 = stencil_factory.from_origin_domain(
            func=get_prs_fv3,
            origin=grid_indexing.origin_full(),
            domain=grid_indexing.domain_full(add=(0, 0, 1)),
        )
        self._get_phi_fv3 = stencil_factory.from_origin_domain(
            func=get_phi_fv3,
            origin=grid_indexing.origin_full(),
            domain=grid_indexing.domain_full(add=(0, 0, 1)),
        )
        self._atmos_phys_driver_statein = stencil_factory.from_origin_domain(
            func=atmos_phys_driver_statein,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(add=(0, 0, 1)),
            externals={
                "nwat": self._nwat,
                "ptop": self._ptop,
                "pk0inv": self._pk0inv,
                "pktop": self._pktop,
            },
        )
        self._flip_fields = stencil_factory.from_origin_domain(
            func=flip_fields,
            origin=grid_indexing.origin_full(),
            domain=grid_indexing.domain_full(),
        )

        if self._hydro_delp:
            self._calc_p_lay_hydro = stencil_factory.from_origin_domain(
                func=calc_p_lay_hydro,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )

        if self._prescribe_sst:
            self._set_sst = stencil_factory.from_origin_domain(
                func=set_sst,
                externals={
                    "tmax": namelist.max_sst,
                    "tmin": namelist.min_sst,
                },
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )

        if not self._pre_radiation:
            self._interpolate_radiation = stencil_factory.from_origin_domain(
                func=interpolate_radiation,
                externals={
                    "daily_mean": namelist.daily_mean,
                },
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )

        # Setup radiation
        if "RTE_RRTMGP" in schemes:
            if not rad_config:
                raise ValueError(
                    "You must specify a radiation configuration to use RTE-RRTMGP"
                )
            self._rterrtmgp = True
            sigma = calc_sigma(grid_data.ak[:], grid_data.bk[:], 0)
            self._copy_to_radiation = stencil_factory.from_origin_domain(
                func=copy_to_radiation,
                origin=grid_indexing.origin_full(),
                domain=grid_indexing.domain_full(),
            )
            self._copy_from_radiation = stencil_factory.from_origin_domain(
                func=copy_from_radiation,
                origin=grid_indexing.origin_full(),
                domain=grid_indexing.domain_full(),
            )
            self._radiation = RTE_RRTMGPDriver(
                rad_config,
                self._gridlon,
                self._gridlat,
                sigma[::-1],
                quantity_factory,
                stencil_factory,
            )
        else:
            self._rterrtmgp = False

        # Setup surface schemes
        if "SFC_layer" in schemes:
            if sfc_config is None:
                raise ValueError(
                    "Specify a surface configuration to use surface parameterizations"
                )
            self._sfc_layer = True
            self._prepare_sfc = stencil_factory.from_origin_domain(
                func=prepare_sfc,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
            self._sfc = SurfaceLayer(
                stencil_factory,
                quantity_factory,
                sfc_config,
            )
            self._update_from_sfc = stencil_factory.from_origin_domain(
                func=update_from_sfc,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
        else:
            self._sfc_layer = False

        self._dudt = make_quantity()
        self._dvdt = make_quantity()
        self._dtdt = make_quantity()
        self._dtdtc = make_quantity()
        self._dqdt = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )
        self._qgrs = self.quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._dusfc = make_quantity_2d()
        self._dvsfc = make_quantity_2d()
        self._dtsfc = make_quantity_2d()
        self._dqsfc = make_quantity_2d()

        # Setup PBL scheme
        if "SATM_EDMF" in schemes:
            ndsl_log.info("SATM EDMF PBL scheme selected")
            if pbl_config is None:
                raise ValueError("Specify a PBL configuration to use SATM_EDMF scheme")
            self.pbl_state = SATMEDMFVDiffState.init_zeros(self.quantity_factory)
            self._satm_edmf = True
            self._fill_pbl_state = stencil_factory.from_origin_domain(
                func=fill_pbl_state,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
                externals={"levs": nz},
            )
            self._pbl = ScaleAwareTKEMoistEDMF(
                stencil_factory,
                self.quantity_factory,
                grid_data.area,
                pbl_config,
            )
            self._results_from_pbl = stencil_factory.from_origin_domain(
                func=results_from_pbl,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
        else:
            self._satm_edmf = False

        # Setup shallow convection
        if "SAMF_SHALCONV" in schemes:
            if sc_config is None:
                raise ValueError("Shallow convection enabled but no config specified")
            self._samf_shalconv = True
            self._fill_shalconv_state = stencil_factory.from_origin_domain(
                func=fill_shalconv_state,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
            self._samf_shallow_convection = ScaleAwareMassFluxShallowConvection(
                stencil_factory=stencil_factory,
                quantity_factory=quantity_factory,
                config=sc_config,
            )
            self.shalconv_state = SAMFShalConvState.init_zeros(quantity_factory)
            self._results_from_shalconv = stencil_factory.from_origin_domain(
                func=results_from_shalconv,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
        else:
            self._samf_shalconv = False

        # Setup microphysics
        if "GFS_microphysics" in schemes:
            if "GFDL_cloud_microphysics" in schemes:
                raise ValueError(
                    f"Multiple microphysics schemes: {schemes}"
                )  # TODO: We should consider a ConfigurationError exception for this
            self._microphysics = "GFS"
            ndsl_log.info("GFS microphysics selected")
            self._prepare_gfs_microphysics = stencil_factory.from_origin_domain(
                func=prepare_gfs_microphysics,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
            self._update_physics_state_with_tendencies = (
                stencil_factory.from_origin_domain(
                    func=update_physics_state_with_tendencies,
                    origin=grid_indexing.origin_compute(),
                    domain=grid_indexing.domain_compute(),
                )
            )
            self._gfs_microphysics = GFSMicrophysics(
                stencil_factory, quantity_factory, grid_data, namelist=namelist
            )
        elif "GFDL_cloud_microphysics" in schemes:
            ndsl_log.info("GFDL Cloud microphysics selected")
            if gfdl_cld_mp_config is None:
                raise ValueError(
                    "Specify a configuration to use the GFDL Cloud Microphysics"
                )
            self._microphysics = "GFDL_CLoud"
            self.microphyics_state = GFDLCloudMicrophysicsState.init_zeros(
                quantity_factory
            )
            self._prepare_gfdl_cld_microphysics = stencil_factory.from_origin_domain(
                func=prepare_gfdl_cld_microphysics,
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
            self._gfdl_cld_microphysics = GFDLCloudMicrophysics(
                stencil_factory, quantity_factory, grid_data, gfdl_cld_mp_config
            )
            self._post_gfdl_cld_microphysics = stencil_factory.from_origin_domain(
                func=post_gfdl_cld_mp,
                externals={
                    "dtp": self._dt_phys,
                },
                origin=grid_indexing.origin_compute(),
                domain=grid_indexing.domain_compute(),
            )
        else:
            ndsl_log.info("No microphysics selected")
            self._microphysics = None

    def _setup_statein(self):
        self._NQ = 8  # state.nq_tot - spec.config.dnats
        self._dnats = 1  # spec.config.dnats
        self._nwat = 6  # spec.config.nwat
        self._p00 = 1.0e5

    def __call__(
        self,
        physics_state: PhysicsState,
        timestep: float = 0.0,
        radiation_state: RTE_RRTMGPState = None,
        surface_state: SurfaceState = None,
        date: datetime.datetime = None,
    ):
        if timestep == 0.0:
            timestep = self._dt_phys
        do_radiation = (self._nsteps % self._nsswr == 0) or (
            self._nsteps % self._nslwr == 0
        )
        self._atmos_phys_driver_statein(
            physics_state.prsik,
            physics_state.phii,
            physics_state.prsi,
            physics_state.delz,
            physics_state.delp,
            physics_state.qvapor,
            physics_state.qliquid,
            physics_state.qrain,
            physics_state.qice,
            physics_state.qsnow,
            physics_state.qgraupel,
            physics_state.qo3mr,
            physics_state.qsgs_tke,
            physics_state.qcld,
            physics_state.pt,
            self._dm3d,
            physics_state.pgr,
        )

        if self._hydro_delp:
            self._calc_p_lay_hydro(physics_state.prsi, physics_state.delp)

        self._start_physics(
            self._dvdt,
            self._dudt,
            self._dtdt,
            self._dqdt,  # FloatField with extra data dimension
            self._dusfc,
            self._dvsfc,
            self._dtsfc,
            self._dqsfc,
        )
        if self._ntracers == 9:
            self._pack_tracers(
                self._qgrs,
                physics_state.qvapor,
                physics_state.qliquid,
                physics_state.qrain,
                physics_state.qice,
                physics_state.qsnow,
                physics_state.qgraupel,
                physics_state.qo3mr,
                physics_state.qsgs_tke,
                physics_state.qcld,
            )
        self._get_prs_fv3(
            physics_state.phii,
            physics_state.prsi,
            physics_state.pt,
            physics_state.qvapor,
            physics_state.delprsi,
            self._del_gz,
        )
        if self._prescribe_sst:
            self._set_sst(physics_state.tsfc, self._gridlat)
        # If PBL scheme is present, physics_state should be updated here
        self._get_phi_fv3(
            physics_state.pt,
            physics_state.qvapor,
            self._del_gz,
            physics_state.phii,
            physics_state.phil,
        )
        self._flip_fields(
            physics_state.ua,
            physics_state.va,
            physics_state.omga,
            physics_state.pt,
            physics_state.qvapor,
            physics_state.qliquid,
            physics_state.qrain,
            physics_state.qice,
            physics_state.qsnow,
            physics_state.qgraupel,
            physics_state.qo3mr,
            physics_state.qsgs_tke,
            physics_state.qcld,
            physics_state.delp,
            physics_state.delprsi,
            physics_state.prsi,
            physics_state.prsik,
            physics_state.prslk,
            physics_state.phil,
            physics_state.phii,
            self._u1,
            self._v1,
            self._w1,
            self._t1,
            self._qvapor1,
            self._qliquid1,
            self._qrain1,
            self._qice1,
            self._qsnow1,
            self._qgraupel1,
            self._qo3mr1,
            self._qsgs_tke1,
            self._qcld1,
            self._prsl1,
            self._delp1,
            self._prsi1,
            self._prsik1,
            self._prslk1,
            self._phil1,
            self._phii1,
            self._level_flip,
            self._layer_flip,
        )

        # Call radiation if timestep is right
        if self._rterrtmgp:
            if do_radiation:
                if not surface_state:
                    raise ValueError("You must pass a surface state to run radiation")
                if not radiation_state:
                    raise ValueError("You must pass a radiation state to run radiation")
                if not date:
                    raise ValueError("You must pass a date to run radiation")
                self._copy_to_radiation(
                    self._prsi1,
                    self._prsl1,
                    self._t1,
                    physics_state.tsfc,
                    self._qvapor1,
                    self._qliquid1,
                    self._qice1,
                    self._qo3mr1,
                    self._qcld1,
                    radiation_state.prsi,
                    radiation_state.prsl,
                    radiation_state.tlyr,
                    radiation_state.tsfc,
                    radiation_state.qvapor,
                    radiation_state.qliquid,
                    radiation_state.qice,
                    radiation_state.qo3mr,
                    radiation_state.qcld,
                )
                ndsl_log.info("Entering radiation")
                self._radiation.step_radiation(radiation_state, surface_state, date)
                self._copy_from_radiation(
                    radiation_state.hrtsw,
                    radiation_state.hrtlw,
                    radiation_state.fswu,
                    radiation_state.fswd,
                    radiation_state.flwu,
                    radiation_state.flwd,
                    physics_state.hrtsw,
                    physics_state.hrtlw,
                    self._hrtsw1,
                    self._hrtlw1,
                    physics_state.fswu,
                    physics_state.fswd,
                    physics_state.flwu,
                    physics_state.flwd,
                    self._layer_flip,
                    self._level_flip,
                )

            self._interpolate_radiation(
                self._gridlat,
                self._gridlon,
                radiation_state.mu0,
                surface_state.tsfc,
                radiation_state.tlyr,  # Should be lowest physics state temp.
                radiation_state.tlyr,
                surface_state.sfcemis,
                radiation_state.flwd,
                radiation_state.fswn,
                radiation_state.fswd,
                self._sfcnirbmu,
                self._sfcnirdfu,
                self._sfcvisbmu,
                self._sfcvisdfu,
                self._sfcnirbmd,
                self._sfcnirdfd,
                self._sfcvisbmd,
                self._sfcvisdfd,
                radiation_state.hrtsw,
                radiation_state.hrtsw_clr,
                radiation_state.hrtlw,
                radiation_state.hrtlw_clr,
                self._dtdt,
                self._dtdtc,
                self._adjsfcdlw,
                self._adjsfculw,
                self._adjsfcnsw,
                self._adjsfcdsw,
                self._adjnirbmu,
                self._adjnirdfu,
                self._adjvisbmu,
                self._adjvisdfu,
                self._adjnirbmd,
                self._adjnirdfd,
                self._adjvisbmd,
                self._adjvisdfd,
                self._xcosz,
                self._xmu,
                self._radiation.solhr,
                self._radiation.slag,
                self._radiation.sdec,
                self._radiation.cdec,
            )
        # Do physics schemes here.
        # First the surface parameterizations:
        if self._sfc_layer:
            if not surface_state:
                raise ValueError("You must pass a surface state to run surface schemes")
            self._prepare_sfc(
                surface_state.u1,
                surface_state.v1,
                surface_state.t1,
                surface_state.prsl1,
                surface_state.prsik,
                surface_state.prslk,
                surface_state.qvapor,
                surface_state.phil,
                surface_state.ps,
                surface_state.sfcdlw,
                surface_state.sfcdsw,
                surface_state.sfcnsw,
                self._u1,
                self._v1,
                self._t1,
                self._prsl1,
                self._prsik1,
                self._prslk1,
                self._qvapor1,
                self._phil1,
                physics_state.pgr,
                self._adjsfcdlw,
                self._adjsfcdsw,
                self._adjsfcnsw,
            )
            self._sfc(surface_state)
            # TODO: once surface state is a required argument we don't need this copy
            self._update_from_sfc(
                surface_state.wind,
                surface_state.uustar,
                surface_state.ffmm,
                surface_state.ffhh,
                surface_state.f10m,
                surface_state.sfcemis,
                surface_state.srflag,
                surface_state.hice,
                surface_state.fice,
                surface_state.tisfc,
                surface_state.weasd,
                surface_state.tprcp,
                surface_state.rb,
                surface_state.stress,
                surface_state.hflx,
                surface_state.evap,
                self._sfcwind,
                self._uustar,
                self._ffmm,
                self._ffhh,
                self._f10m,
                self._emis,
                self._srflg,
                self._hice,
                self._fice,
                self._tisfc,
                self._weasd,
                self._tprcp,
                self._rb,
                self._stress,
                self._hflx,
                self._evap,
            )

        # Run the PBL scheme:
        if self._satm_edmf:
            self._fill_pbl_state(
                self.pbl_state.u1,
                self.pbl_state.v1,
                self.pbl_state.t1,
                self.pbl_state.q1,
                self.pbl_state.prsl,
                self.pbl_state.delta,
                self.pbl_state.phii,
                self.pbl_state.phil,
                self.pbl_state.prsi,
                self.pbl_state.prslk,
                self.pbl_state.hsw,
                self.pbl_state.hlw,
                self.pbl_state.islimsk,
                self.pbl_state.kinver,
                self.pbl_state.xmu,
                self.pbl_state.psk,
                self.pbl_state.rbsoil,
                self.pbl_state.zorl,
                self.pbl_state.tsea,
                self.pbl_state.fm,
                self.pbl_state.fh,
                self.pbl_state.heat,
                self.pbl_state.evap,
                self.pbl_state.stress,
                self.pbl_state.spd1,
                self._u1,
                self._v1,
                self._t1,
                self._qvapor1,
                self._qliquid1,
                self._qrain1,
                self._qice1,
                self._qsnow1,
                self._qgraupel1,
                self._qo3mr1,
                self._qsgs_tke1,
                self._qcld1,
                self._delp1,
                self._phil1,
                self._phii1,
                self._prsl1,
                self._prsi1,
                self._prsik1,
                self._prslk1,
                self._hrtsw1,
                self._hrtlw1,
                self._rb,
                self._tisfc,
                self._ffmm,
                self._ffhh,
                self._hflx,
                self._evap,
                self._sfcwind,
                self._stress,
                self._xmu,
            )

            self._pbl(self.pbl_state)

            self._results_from_pbl(
                self._qvapor1,
                self._qliquid1,
                self._qrain1,
                self._qice1,
                self._qsnow1,
                self._qgraupel1,
                self._qo3mr1,
                self._qsgs_tke1,
                self._qcld1,
                self._t1,
                self._u1,
                self._v1,
                physics_state.hpbl,
                self.pbl_state.rtg,
                self.pbl_state.dtdt,
                self.pbl_state.du,
                self.pbl_state.dv,
                self.pbl_state.hpbl,
                timestep,
            )

        # Shallow convection:
        if self._samf_shalconv:
            self._fill_shalconv_state(
                self.shalconv_state.t1,
                self.shalconv_state.u1,
                self.shalconv_state.v1,
                self.shalconv_state.qtr,
                self.shalconv_state.dot,
                self.shalconv_state.hpbl,
                self.shalconv_state.prslp,
                self.shalconv_state.phil,
                self.shalconv_state.delp,
                self.shalconv_state.psp,
                self.shalconv_state.kcnv,
                self.shalconv_state.garea,
                self.shalconv_state.islimsk,
                self._qvapor1,
                self._t1,
                self._u1,
                self._v1,
                self._qliquid1,
                self._qrain1,
                self._qice1,
                self._qsnow1,
                self._qgraupel1,
                self._qo3mr1,
                self._qsgs_tke1,
                self._qcld1,
                self._w1,
                physics_state.hpbl,
                self._prsl1,
                self._phil1,
                self._delp1,
                physics_state.pgr,
                self._area,
            )

            self._samf_shallow_convection(self.shalconv_state)

            self._results_from_shalconv(
                self._t1,
                self._u1,
                self._v1,
                self._qvapor1,
                self._qliquid1,
                self._qrain1,
                self._qice1,
                self._qsnow1,
                self._qgraupel1,
                self._qo3mr1,
                self._qsgs_tke1,
                self._qcld1,
                self.shalconv_state.t1,
                self.shalconv_state.u1,
                self.shalconv_state.v1,
                self.shalconv_state.qtr,
            )

        self._flip_fields(
            self._u1,
            self._v1,
            self._w1,
            self._t1,
            self._qvapor1,
            self._qliquid1,
            self._qrain1,
            self._qice1,
            self._qsnow1,
            self._qgraupel1,
            self._qo3mr1,
            self._qsgs_tke1,
            self._qcld1,
            self._prsl1,
            self._delp1,
            self._prsi1,
            self._prsik1,
            self._prslk1,
            self._phil1,
            self._phii1,
            physics_state.ua,
            physics_state.va,
            physics_state.omga,
            physics_state.pt,
            physics_state.qvapor,
            physics_state.qliquid,
            physics_state.qrain,
            physics_state.qice,
            physics_state.qsnow,
            physics_state.qgraupel,
            physics_state.qo3mr,
            physics_state.qsgs_tke,
            physics_state.qcld,
            physics_state.delp,
            physics_state.delprsi,
            physics_state.prsi,
            physics_state.prsik,
            physics_state.prslk,
            physics_state.phil,
            physics_state.phii,
            self._level_flip,
            self._layer_flip,
        )

        # Microphysics:
        if self._microphysics:
            if self._microphysics == "GFS":
                self._prepare_gfs_microphysics(
                    physics_state.dz,
                    physics_state.phii,
                    physics_state.wmp,
                    physics_state.omga,
                    physics_state.qvapor,
                    physics_state.pt,
                    physics_state.delp,
                    physics_state.gfs_microphysics.udt,
                    physics_state.gfs_microphysics.vdt,
                    physics_state.gfs_microphysics.pt_dt,
                    physics_state.gfs_microphysics.qv_dt,
                    physics_state.gfs_microphysics.ql_dt,
                    physics_state.gfs_microphysics.qr_dt,
                    physics_state.gfs_microphysics.qi_dt,
                    physics_state.gfs_microphysics.qs_dt,
                    physics_state.gfs_microphysics.qg_dt,
                    physics_state.gfs_microphysics.qa_dt,
                )
                self._gfs_microphysics(
                    physics_state.gfs_microphysics, timestep=timestep
                )
                # Fortran uses IPD interface, here we use physics_updated_<var>
                # to denote the updated field

                # TODO: This is cumbersome and should be refactored to just have
                # state.var instead of state.var and state.updated_var
                self._update_physics_state_with_tendencies(
                    physics_state.qvapor,
                    physics_state.qliquid,
                    physics_state.qrain,
                    physics_state.qice,
                    physics_state.qsnow,
                    physics_state.qgraupel,
                    physics_state.qcld,
                    physics_state.pt,
                    physics_state.ua,
                    physics_state.va,
                    physics_state.gfs_microphysics.qv_dt,
                    physics_state.gfs_microphysics.ql_dt,
                    physics_state.gfs_microphysics.qr_dt,
                    physics_state.gfs_microphysics.qi_dt,
                    physics_state.gfs_microphysics.qs_dt,
                    physics_state.gfs_microphysics.qg_dt,
                    physics_state.gfs_microphysics.qa_dt,
                    physics_state.gfs_microphysics.pt_dt,
                    physics_state.gfs_microphysics.udt,
                    physics_state.gfs_microphysics.vdt,
                    physics_state.physics_updated_specific_humidity,
                    physics_state.physics_updated_qliquid,
                    physics_state.physics_updated_qrain,
                    physics_state.physics_updated_qice,
                    physics_state.physics_updated_qsnow,
                    physics_state.physics_updated_qgraupel,
                    physics_state.physics_updated_cloud_fraction,
                    physics_state.physics_updated_pt,
                    physics_state.physics_updated_ua,
                    physics_state.physics_updated_va,
                    timestep,
                )
            elif self._microphysics == "GFDL_CLoud":
                self._prepare_gfdl_cld_microphysics(
                    self.microphyics_state.delp,
                    self.microphyics_state.delz,
                    self.microphyics_state.ua,
                    self.microphyics_state.va,
                    self.microphyics_state.wa,
                    self.microphyics_state.pt,
                    self.microphyics_state.qvapor,
                    self.microphyics_state.qliquid,
                    self.microphyics_state.qrain,
                    self.microphyics_state.qice,
                    self.microphyics_state.qsnow,
                    self.microphyics_state.qgraupel,
                    self.microphyics_state.qcld,
                    physics_state.delp,
                    physics_state.delz,
                    physics_state.ua,
                    physics_state.va,
                    physics_state.w,
                    physics_state.pt,
                    physics_state.qvapor,
                    physics_state.qliquid,
                    physics_state.qrain,
                    physics_state.qice,
                    physics_state.qsnow,
                    physics_state.qgraupel,
                    physics_state.qcld,
                    self.microphyics_state.column_water,
                    self.microphyics_state.column_rain,
                    self.microphyics_state.column_ice,
                    self.microphyics_state.column_snow,
                    self.microphyics_state.column_graupel,
                    self.microphyics_state.preflux_water,
                    self.microphyics_state.preflux_rain,
                    self.microphyics_state.preflux_ice,
                    self.microphyics_state.preflux_snow,
                    self.microphyics_state.preflux_graupel,
                    self.microphyics_state.qcloud_cond_nuclei,
                    self.microphyics_state.qcloud_ice_nuclei,
                )
                self._gfdl_cld_microphysics(
                    self.microphyics_state,
                    last_step=True,
                )
                # TODO: Refactor to ditch the physics_updated variables
                self._post_gfdl_cld_microphysics(
                    self.microphyics_state.delp,
                    self.microphyics_state.delz,
                    self.microphyics_state.ua,
                    self.microphyics_state.va,
                    self.microphyics_state.wa,
                    self.microphyics_state.pt,
                    self.microphyics_state.qvapor,
                    self.microphyics_state.qliquid,
                    self.microphyics_state.qrain,
                    self.microphyics_state.qice,
                    self.microphyics_state.qsnow,
                    self.microphyics_state.qgraupel,
                    self.microphyics_state.qcld,
                    physics_state.delp,
                    physics_state.delz,
                    physics_state.physics_updated_ua,
                    physics_state.physics_updated_va,
                    physics_state.w,
                    physics_state.physics_updated_pt,
                    physics_state.physics_updated_specific_humidity,
                    physics_state.physics_updated_qliquid,
                    physics_state.physics_updated_qrain,
                    physics_state.physics_updated_qice,
                    physics_state.physics_updated_qsnow,
                    physics_state.physics_updated_qgraupel,
                    physics_state.physics_updated_cloud_fraction,
                    self.microphyics_state.column_water,
                    self.microphyics_state.column_rain,
                    self.microphyics_state.column_ice,
                    self.microphyics_state.column_snow,
                    self.microphyics_state.column_graupel,
                    self._rain1,
                    physics_state.physics_updated_qo3mr,
                    physics_state.physics_updated_qtke,
                    physics_state.qo3mr,
                    physics_state.qsgs_tke,
                )

            else:
                raise NotImplementedError(
                    f"{self._microphysics} scheme not implemented"
                )
        else:
            ndsl_log.info("No microphysics selected, skipping...")
