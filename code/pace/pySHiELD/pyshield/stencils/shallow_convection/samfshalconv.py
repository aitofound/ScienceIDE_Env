import numpy as np

import ndsl.constants as constants
import pyshield.constants as physcons
import pyshield.stencils.shallow_convection.constants as sccons
from ndsl import QuantityFactory, StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import (
    BACKWARD,
    FORWARD,
    PARALLEL,
    computation,
    exp,
    interval,
    log,
    sqrt,
)
from ndsl.dsl.typing import (
    Bool,
    BoolFieldIJ,
    Float,
    FloatField,
    FloatFieldIJ,
    Int,
    IntField,
    IntFieldIJ,
)
from pyshield._config import TRACER_DIM, FloatFieldTracer
from pyshield.functions.physics_functions import fpvs
from pyshield.stencils.shallow_convection._config import ShallowConvectionConfig
from pyshield.stencils.shallow_convection.samf_shalconv_state import SAMFShalConvState


def exit_routine(cnvflg):
    return cnvflg.sum() == 0


def col_diffs(conv1, conv2):
    cols = []
    for i, j in np.ndindex(conv1.shape):
        if np.logical_xor(conv1[i, j], conv2[i, j]):
            cols.append((i, j))
    return cols


def pa_to_cb(
    psp: FloatFieldIJ,
    prslp: FloatField,
    delp: FloatField,
    ps: FloatFieldIJ,
    prsl: FloatField,
    del0: FloatField,
):
    with computation(FORWARD):
        # Convert input Pa terms to Cb terms
        with interval(0, 1):
            ps = psp * 0.001
            prsl = prslp * 0.001
            del0 = delp * 0.001
        with interval(1, None):
            prsl = prslp * 0.001
            del0 = delp * 0.001


def init_col_arr(
    kcnv: IntFieldIJ,
    cnvflg: BoolFieldIJ,
    kbot: IntFieldIJ,
    ktop: IntFieldIJ,
    kbcon: IntFieldIJ,
    kb: IntFieldIJ,
    ktcon: IntFieldIJ,
    ktconn: IntFieldIJ,
    pdot: FloatFieldIJ,
    rn: FloatFieldIJ,
    qlko_ktcon: FloatFieldIJ,
    edt: FloatFieldIJ,
    aa1: FloatFieldIJ,
    cina: FloatFieldIJ,
    vshear: FloatFieldIJ,
    gdx: FloatFieldIJ,
    garea: FloatFieldIJ,
):
    from __externals__ import km

    with computation(FORWARD), interval(0, 1):
        # Initialize column-integrated and other single-value-per-column
        # variable arrays
        cnvflg = True
        # If there is deep convection turn off shallow convection
        if kcnv == 1:
            cnvflg = False

        if cnvflg:
            kbot = km
            ktop = -1

        rn = 0.0
        kbcon = km - 1
        ktcon = 0
        ktconn = 0
        kb = km - 1
        pdot = 0.0
        qlko_ktcon = 0.0
        edt = 0.0
        aa1 = 0.0
        cina = 0.0
        vshear = 0.0
        gdx = sqrt(garea)


def init_par_and_arr(
    islimsk: IntFieldIJ,
    c0: FloatFieldIJ,
    t1: FloatField,
    c0t: FloatField,
    cnvw: FloatField,
    cnvc: FloatField,
    ud_mf: FloatField,
    dt_mf: FloatField,
):
    from __externals__ import asolfac, c0s

    with computation(FORWARD), interval(0, 1):
        # Determine aerosol-aware rain conversion parameter over land
        if islimsk == 1:
            c0 = c0s * asolfac
        else:
            c0 = c0s

    with computation(FORWARD), interval(...):
        # Determine rain conversion parameter above the freezing level
        # which exponentially decreases with decreasing temperature
        # from Han et al.'s (2017) \cite han_et_al_2017 equation 8
        if t1 > 273.16:
            c0t = c0
        else:
            tem = exp(sccons.D0_SHAL * (t1 - 273.16))
            c0t = c0 * tem

        # Initialize convective cloud water and cloud cover to zero
        cnvw = 0.0
        cnvc = 0.0

        # Initialize updraft mass fluxes to zero
        ud_mf = 0.0
        dt_mf = 0.0


def init_kbm_kmax(
    kbm: IntFieldIJ,
    kmax: IntFieldIJ,
    tx1: FloatFieldIJ,
    ps: FloatFieldIJ,
    prsl: FloatField,
    k_mask: IntField,
):
    from __externals__ import km

    # Determine maximum indices for the parcel starting point (kbm)
    # and cloud top (kmax)
    with computation(FORWARD), interval(0, 1):
        kbm = km - 1
        kmax = km - 1
        tx1 = 1.0 / ps
    with computation(FORWARD), interval(...):
        if prsl * tx1 > 0.7:
            kbm = k_mask + 1
        if prsl * tx1 > 0.6:
            kmax = k_mask + 1
    with computation(FORWARD), interval(-1, None):
        kbm = min(kbm, kmax)


def init_final(
    kbm: IntFieldIJ,
    kmax: IntFieldIJ,
    flg: BoolFieldIJ,
    cnvflg: BoolFieldIJ,
    kpbl: IntFieldIJ,
    prsl: FloatField,
    zo: FloatField,
    phil: FloatField,
    zi: FloatField,
    pfld: FloatField,
    eta: FloatField,
    hcko: FloatField,
    qcko: FloatField,
    qrcko: FloatField,
    ucko: FloatField,
    vcko: FloatField,
    dbyo: FloatField,
    pwo: FloatField,
    dellal: FloatField,
    to: FloatField,
    qo: FloatField,
    uo: FloatField,
    vo: FloatField,
    wu2: FloatField,
    buo: FloatField,
    drag: FloatField,
    cnvwt: FloatField,
    qeso: FloatField,
    heo: FloatField,
    heso: FloatField,
    hpbl: FloatFieldIJ,
    t1: FloatField,
    qtr: FloatFieldTracer,
    u1: FloatField,
    v1: FloatField,
    k_mask: IntField,
):
    from __externals__ import ntvap

    with computation(PARALLEL), interval(...):
        # Calculate hydrostatic height at layer centers assuming a flat
        # surface (no terrain) from the geopotential
        zo = phil / constants.GRAV

    with computation(PARALLEL), interval(0, -1):
        # Calculate interface height
        zi = 0.5 * (zo[0, 0, 0] + zo[0, 0, +1])

    with computation(FORWARD), interval(0, 1):
        flg = cnvflg
        kpbl = 0

    with computation(FORWARD), interval(1, -1):
        # Find the index for the PBL top using the PBL height; enforce
        # that it is lower than the maximum parcel starting level
        if flg and (zo <= hpbl):
            kpbl = k_mask
        else:
            flg = False

    with computation(FORWARD), interval(-1, None):
        kpbl = min(kpbl, kbm)

    with computation(PARALLEL), interval(...):
        val1 = 0.0
        val2 = 0.0
        tem = 0.0

        if cnvflg and k_mask <= kmax:
            # Convert prsl from centibar to millibar, set normalized mass
            # flux to 1, cloud properties to 0, and save model state
            # variables (after advection/turbulence)
            pfld = prsl * 10.0
            eta = 1.0
            hcko = 0.0
            qcko = 0.0
            qrcko = 0.0
            ucko = 0.0
            vcko = 0.0
            dbyo = 0.0
            pwo = 0.0
            dellal = 0.0
            to = t1
            qo = qtr[0, 0, 0][ntvap]
            uo = u1
            vo = v1
            wu2 = 0.0
            buo = 0.0
            drag = 0.0
            cnvwt = 0.0

            # Calculate saturation specific humidity and enforce minimum
            # moisture values
            qeso = 0.01 * fpvs(to)
            qeso = (constants.EPS * qeso) / (pfld + (constants.EPS - 1) * qeso)
            val1 = 1.0e-8
            val2 = 1.0e-10
            qeso = max(qeso, val1)
            qo = max(qo, val2)

            # Calculate moist static energy (heo) and saturation moist
            # static energy (heso)
            tem = phil + constants.CP_AIR * to
            heo = tem + constants.HLV * qo
            heso = tem + constants.HLV * qeso


def init_tracers(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kmax: IntFieldIJ,
    ctr: FloatFieldTracer,
    ctro: FloatFieldTracer,
    ecko: FloatFieldTracer,
    qtr: FloatFieldTracer,
    n_tracer: int,
):
    with computation(PARALLEL), interval(...):
        # Initialize tracer variables
        if cnvflg and k_mask <= kmax:
            ctr[0, 0, 0][n_tracer] = qtr[0, 0, 0][n_tracer]
            ctro[0, 0, 0][n_tracer] = qtr[0, 0, 0][n_tracer]
            ecko[0, 0, 0][n_tracer] = 0.0


def stencil_static0(
    cnvflg: BoolFieldIJ,
    hmax: FloatFieldIJ,
    heo: FloatField,
    kb: IntFieldIJ,
    k_mask: IntField,
    kpbl: IntFieldIJ,
    kmax: IntFieldIJ,
    zo: FloatField,
    to: FloatField,
    qeso: FloatField,
    qo: FloatField,
    po: FloatField,
    uo: FloatField,
    vo: FloatField,
    heso: FloatField,
    pfld: FloatField,
):
    """
    Scale-Aware Mass-Flux Shallow Convection
    """
    # Search in the PBL for the level of maximum moist
    # static energy to start the ascending parcel.
    with computation(FORWARD), interval(0, 1):
        if cnvflg:
            hmax = heo
            kb = 0

    with computation(FORWARD), interval(1, None):
        if (cnvflg) and (k_mask <= kpbl):
            if heo > hmax:
                kb = k_mask
                hmax = heo

    # Calculate the temperature, water vapor mixing ratio,
    # and pressure at interface levels.
    with computation(FORWARD), interval(0, -1):
        tmp = fpvs(to[0, 0, 1])
        dz = 1.0
        dp = 1.0
        es = 1.0
        pprime = 1.0
        qs = 1.0
        dqsdp = 1.0
        desdt = 1.0
        dqsdt = 1.0
        gamma = 1.0
        dt = 1.0
        dq = 1.0

        if cnvflg and (k_mask <= kmax - 1):
            dz = 0.5 * (zo[0, 0, 1] - zo[0, 0, 0])
            dp = 0.5 * (pfld[0, 0, 1] - pfld[0, 0, 0])
            es = 0.01 * tmp  # fpvs is in pa
            pprime = pfld[0, 0, 1] + (constants.EPS - 1) * es
            qs = constants.EPS * es / pprime
            dqsdp = -qs / pprime
            desdt = es * (
                physcons.FACT1 / to[0, 0, 1]
                + physcons.FACT2 / (to[0, 0, 1] * to[0, 0, 1])
            )
            dqsdt = qs * pfld[0, 0, 1] * desdt / (es * pprime)
            gamma = physcons.EL2ORC * qeso[0, 0, 1] / (to[0, 0, 1] * to[0, 0, 1])
            dt = (constants.GRAV * dz + constants.HLV * dqsdp * dp) / (
                constants.CP_AIR * (1.0 + gamma)
            )
            dq = dqsdt * dt + dqsdp * dp
            to = to[0, 0, 1] + dt
            qo = qo[0, 0, 1] + dq
            po = 0.5 * (pfld[0, 0, 0] + pfld[0, 0, 1])

    with computation(FORWARD), interval(0, -1):
        # Recalculate saturation specific humidity, moist static energy,
        # saturation moist static energy, and horizontal momentum on
        # interface levels. Enforce minimum specific humidity.
        tmp = fpvs(to)

        if cnvflg and k_mask <= kmax - 1:
            qeso = 0.01 * tmp  # fpvs is in pa
            qeso = constants.EPS * qeso / (po + (constants.EPS - 1) * qeso)
            val1 = 1.0e-8
            qeso = max(qeso, val1)
            val2 = 1.0e-10
            qo = max(qo, val2)
            # qo   = min(qo[0,0,0],qeso[0,0,0])
            heo = (
                0.5 * constants.GRAV * (zo[0, 0, 0] + zo[0, 0, 1])
                + constants.CP_AIR * to
                + constants.HLV * qo
            )
            heso = (
                0.5 * constants.GRAV * (zo[0, 0, 0] + zo[0, 0, 1])
                + constants.CP_AIR * to
                + constants.HLV * qeso
            )
            uo = 0.5 * (uo[0, 0, 0] + uo[0, 0, 1])
            vo = 0.5 * (vo[0, 0, 0] + vo[0, 0, 1])


# ntr stencil put at last
def stencil_ntrstatic0(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kmax: IntFieldIJ,
    ctro: FloatFieldTracer,
    n_tracer: int,
):
    with computation(FORWARD), interval(0, -1):
        if (cnvflg) and (k_mask <= (kmax - 1)):
            ctro[0, 0, 0][n_tracer] = 0.5 * (
                ctro[0, 0, 0][n_tracer] + ctro[0, 0, 1][n_tracer]
            )


def stencil_static1(
    cnvflg: BoolFieldIJ,
    flg: BoolFieldIJ,
    kbcon: IntFieldIJ,
    kmax: IntFieldIJ,
    k_mask: IntField,
    kbm: IntFieldIJ,
    kb: IntFieldIJ,
    heo_kb: FloatFieldIJ,
    heo: FloatField,
    heso: FloatField,
):
    # Search below the index "kbm" for the level of free convection (LFC)
    # where the condition \f$h_b > h^*\f$ is first met,
    # where \f$h_b, h^*\f$ are the state moist static energy at the parcel's
    # starting level and saturation moist static energy, respectively.
    # Set "kbcon" to the index of the LFC.
    with computation(FORWARD), interval(...):
        if k_mask == kb:
            heo_kb[0, 0] = heo[0, 0, 0]
    with computation(FORWARD):
        with interval(0, 1):
            flg = cnvflg
            if flg:
                kbcon = kmax

        with interval(1, -1):
            if flg and k_mask < kbm:
                # To use heo_kb to represent heo(i,kb(i))
                if k_mask > kb and heo_kb > heso:
                    kbcon = k_mask
                    flg = False

        with interval(-1, None):
            if cnvflg:
                if kbcon == kmax:
                    cnvflg = False


def stencil_static2(
    cnvflg: BoolFieldIJ,
    pdot: FloatFieldIJ,
    dot: FloatField,
    islimsk: IntFieldIJ,
    k_mask: IntField,
    kbcon: IntFieldIJ,
    kb: IntFieldIJ,
    pfld: FloatField,
    pfld_kb: FloatFieldIJ,
    pfld_kbcon: FloatFieldIJ,
):
    # Determine the vertical pressure velocity at the LFC.
    # After Han and Pan (2011) \cite han_and_pan_2011 , determine
    # the maximum pressure thickness between a parcel's starting
    # level and the LFC. If a parcel doesn't reach the LFC within
    # the critical thickness, then the convective inhibition is
    # deemed too great for convection to be triggered, and the
    # subroutine returns to the calling routine without modifying
    # the state variables.
    with computation(FORWARD), interval(...):
        if cnvflg:
            if k_mask == kbcon:
                # pdot = 10. * dot
                pdot = 0.01 * dot  # Now dot is in Pa/s
                pfld_kbcon = pfld
            if k_mask == kb:
                pfld_kb = pfld

    with computation(FORWARD), interval(0, 1):
        # turn off convection if pressure depth between parcel source level
        # and cloud base is larger than a critical value, cinpcr
        tem = 0.0
        tem1 = 0.0
        ptem = 0.0
        ptem1 = 0.0
        cinpcr = 0.0
        if cnvflg:
            if islimsk == 1:
                w1 = sccons.W1L
                w2 = sccons.W2L
                w3 = sccons.W3L
                w4 = sccons.W4L
            else:
                w1 = sccons.W1S
                w2 = sccons.W2S
                w3 = sccons.W3S
                w4 = sccons.W4S

            if pdot <= w4:
                tem = (pdot - w4) / (w3 - w4)
            elif pdot >= -w4:
                tem = -(pdot + w4) / (w4 - w3)
            else:
                tem = 0.0
            val1 = -1.0
            val2 = 1.0
            tem = max(tem, val1)
            tem = min(tem, val2)
            ptem = 1.0 - tem
            ptem1 = 0.5 * (sccons.CINPCRMX - sccons.CINPCRMN)
            cinpcr = sccons.CINPCRMX - ptem * ptem1
            tem1 = pfld_kb - pfld_kbcon
            if tem1 > cinpcr:
                cnvflg = False


def stencil_static3(
    sumx: FloatFieldIJ,
    tkemean: FloatFieldIJ,
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kb: IntFieldIJ,
    kbcon: IntFieldIJ,
    zo: FloatField,
    qtr: FloatFieldTracer,
    clamt: FloatFieldIJ,
):
    # turbulent entrainment rate assumed to be proportional
    # to subcloud mean TKE
    from __externals__ import clam, ntk

    with computation(FORWARD), interval(0, 1):
        if ntk > -1:
            if cnvflg:
                sumx = 0.0
                tkemean = 0.0

    with computation(FORWARD), interval(0, -1):
        dz = 0.0
        tem = 0.0
        if ntk > -1:
            if cnvflg:
                if (k_mask >= kb) and (k_mask < kbcon):
                    dz = zo[0, 0, 1] - zo
                    tem = 0.5 * (qtr[0, 0, 0][ntk] + qtr[0, 0, 1][ntk])
                    tkemean = tkemean + tem * dz
                    sumx = sumx + dz

    with computation(FORWARD), interval(-1, None):
        tem1 = 0.0
        if ntk > -1:
            if cnvflg:
                tkemean = tkemean / sumx
                if tkemean > sccons.TKEMX:
                    clamt = clam + sccons.CLAMD
                elif tkemean < sccons.TKEMN:
                    clamt = clam - sccons.CLAMD
                else:
                    tem1 = 1.0 - 2.0 * (sccons.TKEMX - tkemean) / sccons.DTKE
                    clamt = clam + sccons.CLAMD * tem1
        else:
            if cnvflg:
                clamt = clam


def stencil_static5(
    cnvflg: BoolFieldIJ,
    xlamue: FloatField,
    clamt: FloatFieldIJ,
    zi: FloatField,
    xlamud: FloatFieldIJ,
    k_mask: IntField,
    kbcon: IntFieldIJ,
    kb: IntFieldIJ,
    eta: FloatField,
    ktconn: IntFieldIJ,
    kmax: IntFieldIJ,
    kbm: IntFieldIJ,
    hcko: FloatField,
    ucko: FloatField,
    vcko: FloatField,
    heo: FloatField,
    uo: FloatField,
    vo: FloatField,
    ptem: FloatFieldIJ,
    flg: BoolFieldIJ,
):
    # Start updraft entrainment rate.
    # assume updraft entrainment rate
    # is an inverse function of height
    with computation(FORWARD), interval(0, -1):
        if cnvflg:
            xlamue = clamt / zi

    with computation(FORWARD), interval(-1, None):
        if cnvflg:
            xlamue[0, 0, 0] = xlamue[0, 0, -1]

    # specify the detrainment rate for the updrafts
    # (The updraft detrainment rate was set constant and equal to
    # the entrainment rate at cloud base.)
    # The updraft detrainment rate is vertically constant and proportional to clamt
    with computation(FORWARD), interval(0, 1):
        if cnvflg:
            # xlamud(i) = xlamue(i,kbcon(i))
            # xlamud(i) = crtlamd
            xlamud = 0.001 * clamt

    # determine updraft mass flux for the subcloud layers
    # Calculate the normalized mass flux for subcloud and in-cloud layers according
    # to Pan and Wu (1995) \cite pan_and_wu_1995 equation 1:
    # \f[
    # \frac{1}{\eta}\frac{\partial \eta}{\partial z} = \lambda_e - \lambda_d
    # \f]
    # where \f$\eta\f$ is the normalized mass flux,
    # \f$\lambda_e\f$ is the entrainment rate
    # and \f$\lambda_d\f$ is the detrainment rate.
    # The normalized mass flux increases upward below the cloud base and decreases
    # upward above.
    with computation(BACKWARD), interval(0, -1):
        dz = 0.0
        ptem = 0.0
        if cnvflg:
            if (k_mask < kbcon) and (k_mask >= kb):
                dz = zi[0, 0, 1] - zi
                ptem = 0.5 * (xlamue + xlamue[0, 0, 1]) - xlamud
                eta = eta[0, 0, 1] / (1.0 + ptem * dz)

    # compute mass flux above cloud base
    with computation(FORWARD), interval(0, 1):
        flg = cnvflg

    with computation(FORWARD), interval(1, -1):
        dz = 0.0
        ptem = 0.0
        if flg:
            if (k_mask > kbcon) and (k_mask < kmax):
                dz = zi - zi[0, 0, -1]
                ptem = 0.5 * (xlamue + xlamue[0, 0, -1]) - xlamud
                eta = eta[0, 0, -1] * (1 + ptem * dz)

                if eta <= 0.0:
                    kmax = k_mask
                    ktconn = k_mask
                    kbm = min(kbm, kmax)
                    flg = False

    # compute updraft cloud property
    # Set cloud properties equal to the state variables
    # at updraft starting level (kb).
    with computation(PARALLEL), interval(...):
        if cnvflg:
            if k_mask == kb:
                hcko = heo
                ucko = uo
                vcko = vo


def stencil_ntrstatic1(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kb: IntFieldIJ,
    ecko: FloatFieldTracer,
    ctro: FloatFieldTracer,
    n_tracer: Int,
):
    with computation(PARALLEL), interval(...):
        if (cnvflg) and (k_mask == kb):
            ecko[0, 0, 0][n_tracer] = ctro[0, 0, 0][n_tracer]


def stencil_static7(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kb: IntFieldIJ,
    kmax: IntFieldIJ,
    zi: FloatField,
    xlamue: FloatField,
    xlamud: FloatFieldIJ,
    hcko: FloatField,
    heo: FloatField,
    dbyo: FloatField,
    heso: FloatField,
    ucko: FloatField,
    uo: FloatField,
    vcko: FloatField,
    vo: FloatField,
):
    # cm is an enhancement factor in entrainment rates for momentum.
    # Calculate the cloud properties as a parcel ascends, modified by entrainment and
    # detrainment. Discretization follows Appendix B of Grell (1993) \cite grell_1993.
    # Following Han and Pan (2006) \cite han_and_pan_2006, the convective momentum
    # transport is reduced by the convection-induced pressure gradient force by the
    # constant "pgcon", currently set to 0.55 after Zhang and Wu (2003)
    # \cite zhang_and_wu_2003.
    # pass
    from __externals__ import pgcon

    with computation(FORWARD), interval(1, -1):
        dz = 0.0
        tem = 0.0
        tem1 = 0.0
        ptem = 0.0
        ptem1 = 0.0
        factor = 0.0

        if cnvflg:
            if k_mask > kb and k_mask < kmax:
                dz = zi[0, 0, 0] - zi[0, 0, -1]
                tem = 0.5 * (xlamue[0, 0, 0] + xlamue[0, 0, -1]) * dz
                tem1 = 0.5 * xlamud * dz
                factor = 1.0 + tem - tem1
                hcko = (
                    (1.0 - tem1) * hcko[0, 0, -1] + tem * 0.5 * (heo + heo[0, 0, -1])
                ) / factor
                dbyo = hcko - heso

                tem = 0.5 * sccons.CM * tem
                factor = 1.0 + tem
                ptem = tem + pgcon
                ptem1 = tem - pgcon
                ucko = (
                    (1.0 - tem) * ucko[0, 0, -1] + ptem * uo + ptem1 * uo[0, 0, -1]
                ) / factor
                vcko = (
                    (1.0 - tem) * vcko[0, 0, -1] + ptem * vo + ptem1 * vo[0, 0, -1]
                ) / factor


# for n = 1, ntr:
# pass
def stencil_ntrstatic2(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kb: IntFieldIJ,
    kmax: IntFieldIJ,
    zi: FloatField,
    xlamue: FloatField,
    ecko: FloatFieldTracer,
    ctro: FloatFieldTracer,
    n_tracer: Int,
):
    with computation(FORWARD), interval(1, -1):
        tem = 0.0
        dz = 0.0
        factor = 0.0

        if cnvflg:
            if k_mask > kb and k_mask < kmax:
                dz = zi - zi[0, 0, -1]
                tem = 0.25 * (xlamue + xlamue[0, 0, -1]) * dz
                factor = 1.0 + tem
                ecko[0, 0, 0][n_tracer] = (
                    (1.0 - tem) * ecko[0, 0, -1][n_tracer]
                    + tem * (ctro[0, 0, 0][n_tracer] + ctro[0, 0, -1][n_tracer])
                ) / factor


def stencil_update_kbcon1_cnvflg(
    dbyo: FloatField,
    cnvflg: BoolFieldIJ,
    kmax: IntFieldIJ,
    kbm: IntFieldIJ,
    kbcon: IntFieldIJ,
    kbcon1: IntFieldIJ,
    flg: BoolFieldIJ,
    k_mask: IntField,
):
    # Taking account into convection inhibition due to existence of
    # dry layers below cloud base
    # With entrainment, recalculate the LFC as the first level where buoyancy
    # is positive. The difference in pressure levels between LFCs calculated
    # with/without entrainment must be less than a threshold (currently 25 hPa).
    # Otherwise, convection is inhibited and the scheme returns to the calling
    # routine without modifying the state variables. This is the subcloud dryness
    # trigger modification discussed in Han and Pan (2011) \cite han_and_pan_2011.
    with computation(FORWARD), interval(0, 1):
        flg = cnvflg
        kbcon1 = kmax

    with computation(FORWARD), interval(1, -1):
        if flg and (k_mask < kbm):
            if (k_mask >= kbcon) and (dbyo > 0.0):
                kbcon1 = k_mask
                flg = False

    with computation(FORWARD), interval(-1, None):
        if cnvflg:
            if kbcon1 == kmax:
                cnvflg = False


def stencil_static9(
    cnvflg: BoolFieldIJ,
    pfld: FloatField,
    pfld_kbcon: FloatFieldIJ,
    pfld_kbcon1: FloatFieldIJ,
    k_mask: IntField,
    kbcon1: IntFieldIJ,
):
    with computation(FORWARD), interval(...):
        if k_mask == kbcon1:
            pfld_kbcon1 = pfld

    with computation(FORWARD), interval(0, 1):
        tem = 0.0

        if cnvflg:
            # Use pfld_kbcon and pfld_kbcon1 to represent
            # tem = pfld(i,kbcon(i)) - pfld(i,kbcon1(i))
            tem = pfld_kbcon - pfld_kbcon1
            if tem > sccons.DTHK:
                cnvflg = False


def stencil_static10(
    cina: FloatFieldIJ,
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kb: IntFieldIJ,
    kbcon1: IntFieldIJ,
    zo: FloatField,
    qeso: FloatField,
    to: FloatField,
    dbyo: FloatField,
    qo: FloatField,
    pdot: FloatFieldIJ,
    islimsk: IntFieldIJ,
):
    # calculate convective inhibition
    # Calculate additional trigger condition of the convective inhibition (CIN)
    # according to Han et al.'s (2017) \cite han_et_al_2017 equation 13.
    with computation(FORWARD), interval(1, -1):
        dz1 = 0.0
        gamma = 0.0
        rfact = 0.0

        if cnvflg:
            if k_mask > kb and k_mask < kbcon1:
                dz1 = zo[0, 0, 1] - zo
                gamma = physcons.EL2ORC * qeso / (to * to)
                rfact = 1.0 + physcons.DELTA * constants.CP_AIR * (
                    gamma * to / constants.HLV
                )
                cina = (
                    cina
                    + dz1
                    * (constants.GRAV / (constants.CP_AIR * to))
                    * dbyo
                    / (1.0 + gamma)
                    * rfact
                )
                # val   = 0.
                cina = cina + dz1 * constants.GRAV * physcons.DELTA * max(
                    0.0, (qeso - qo)
                )

    with computation(FORWARD), interval(-1, None):
        # Turn off convection if the CIN is less than a critical value (cinacr)
        # which is inversely proportional to the large-scale vertical velocity.
        w1 = sccons.W1S
        w2 = sccons.W2S
        w3 = sccons.W3S
        w4 = sccons.W4S
        tem = 0.0
        tem1 = 0.0
        cinacr = 0.0

        if cnvflg:
            if islimsk == 1:
                w1 = sccons.W1L
                w2 = sccons.W2L
                w3 = sccons.W3L
                w4 = sccons.W4L

            if pdot <= w4:
                tem = (pdot - w4) / (w3 - w4)
            elif pdot >= -w4:
                tem = -(pdot + w4) / (w4 - w3)
            else:
                tem = 0.0

            tem = max(tem, -1.0)
            tem = min(tem, 1.0)
            tem = 1.0 - tem
            tem1 = 0.5 * (sccons.CINACRMX - sccons.CINACRMN)
            cinacr = sccons.CINACRMX - tem * tem1
            # cinacr = cinacrmx
            if cina < cinacr:
                cnvflg = False


def stencil_static11(
    flg: BoolFieldIJ,
    cnvflg: BoolFieldIJ,
    ktcon: IntFieldIJ,
    kbm: IntFieldIJ,
    kbcon1: IntFieldIJ,
    dbyo: FloatField,
    kbcon: IntFieldIJ,
    del0: FloatField,
    xmbmax: FloatFieldIJ,
    aa1: FloatFieldIJ,
    kb: IntFieldIJ,
    tx1: FloatFieldIJ,
    qcko: FloatField,
    qo: FloatField,
    qrcko: FloatField,
    zi: FloatField,
    qeso: FloatField,
    to: FloatField,
    xlamue: FloatField,
    xlamud: FloatFieldIJ,
    eta: FloatField,
    c0t: FloatField,
    dellal: FloatField,
    buo: FloatField,
    drag: FloatField,
    zo: FloatField,
    k_mask: IntField,
    pwo: FloatField,
    cnvwt: FloatField,
    pfld: FloatField,
    prsl: FloatField,
    pfld_kbcon: FloatFieldIJ,
    pfld_ktcon: FloatFieldIJ,
    prsl_ktcon: FloatFieldIJ,
):
    # Determine first guess cloud top as the level of zero buoyancy
    # limited to the level of P/Ps=0.7
    # Calculate the cloud top as the first level where parcel buoyancy
    # becomes negative; the maximum possible value is at \f$p=0.7p_{sfc}\f$.
    from __externals__ import c1, cthk, dt2, limit_shal_conv, ncloud, top_shal

    with computation(FORWARD), interval(0, 1):
        flg = cnvflg
        if flg:
            ktcon = kbm

    with computation(FORWARD), interval(1, -1):
        if flg and k_mask < kbm:
            if k_mask > kbcon1 and dbyo < 0.0:
                ktcon = k_mask
                flg = False
                pfld_ktcon = pfld
                prsl_ktcon = prsl

    with computation(FORWARD), interval(-1, None):
        # KG change: turn off shal conv based on diagnosed cloud depth or top
        # The idea here is that if the cloud is too deep or too high, it should not be
        # handled by shal conv

        if cnvflg and limit_shal_conv:
            # a) cloud depth criterion as in deep conv
            tem = pfld_kbcon - pfld_ktcon
            if tem >= cthk:
                cnvflg = False
            # b) cloud top criterion
            if prsl_ktcon * tx1 < top_shal:
                cnvflg = False
            # if(ktcon > kmax) cnvflg = .false.

    # Specify upper limit of mass flux at cloud base
    # Calculate the maximum value of the cloud base mass flux using
    # the CFL-criterion-based formula of Han and Pan (2011)
    # \cite han_and_pan_2011, equation 7.
    with computation(FORWARD), interval(...):
        dp = 0.0

        if cnvflg:
            if k_mask == kbcon:
                dp = 1000.0 * del0

                xmbmax = dp / (2.0 * constants.GRAV * dt2)

    # Compute cloud moisture property and precipitation
    # Set cloud moisture property equal to the enviromental
    # moisture at updraft starting level (kb).
    with computation(FORWARD), interval(...):
        if cnvflg:
            aa1 = 0.0
            if k_mask == kb:
                qcko = qo
                qrcko = qo

    # Calculate the moisture content of the entraining/detraining parcel (qcko)
    # and the value it would have if just saturated (qrch), according to equation A.14
    # in Grell (1993) \cite grell_1993 . Their difference is the amount of convective
    # cloud water (qlk = rain + condensate). Determine the portion of convective cloud
    # water that remains suspended and the portion that is converted into convective
    # precipitation (pwo). Calculate and save the negative cloud work function (aa1)
    # due to water loading. Above the level of minimum moist static energy, some of the
    # cloud water is detrained into the grid-scale cloud water from every cloud layer
    # with a rate of 0.0005 \f$m^{-1}\f$ (dellal).
    with computation(FORWARD), interval(1, -1):
        dz = 0.0
        gamma = 0.0
        qrch = 0.0
        tem = 0.0
        tem1 = 0.0
        factor = 0.0
        dq = 0.0
        etah = 0.0
        dp = 0.0
        ptem = 0.0
        qlk = 0.0
        rfact = 0.0

        if cnvflg:
            if k_mask > kb and k_mask < ktcon:
                dz = zi - zi[0, 0, -1]
                gamma = physcons.EL2ORC * qeso / (to * to)
                qrch = qeso + gamma * dbyo / (constants.HLV * (1.0 + gamma))
                tem = 0.5 * (xlamue + xlamue[0, 0, -1]) * dz
                tem1 = 0.5 * xlamud * dz
                factor = 1.0 + tem - tem1
                qcko = (
                    (1.0 - tem1) * qcko[0, 0, -1] + tem * 0.5 * (qo + qo[0, 0, -1])
                ) / factor
                qrcko = qcko
                dq = eta * (qcko - qrch)

                # rhbar(i) = rhbar(i) + qo(i,k_mask) / qeso(i,k_mask)

                # Below lfc check if there is excess moisture to release
                # latent heat
                if k_mask >= kbcon and dq > 0.0:
                    etah = 0.5 * (eta + eta[0, 0, -1])
                    dp = 1000.0 * del0

                    if ncloud > 0:
                        ptem = c0t + c1
                        qlk = dq / (eta + etah * ptem * dz)
                        dellal = etah * c1 * dz * qlk * constants.GRAV / dp
                    else:
                        qlk = dq / (eta + etah * c0t * dz)

                    buo = buo - constants.GRAV * qlk
                    qcko = qlk + qrch
                    pwo = etah * c0t * dz * qlk
                    cnvwt = etah * qlk * constants.GRAV / dp

                if k_mask >= kbcon:
                    rfact = 1.0 + physcons.DELTA * constants.CP_AIR * gamma * (
                        to / constants.HLV
                    )
                    buo = (
                        buo
                        + (constants.GRAV / (constants.CP_AIR * to))
                        * dbyo
                        / (1.0 + gamma)
                        * rfact
                    )

                    buo = buo + (
                        constants.GRAV * physcons.DELTA * max(0.0, (qeso - qo))
                    )
                    drag = max(xlamue, xlamud)

    # L1064: Calculate the cloud work function according to Pan and Wu (1995)
    # \cite pan_and_wu_1995 equation 4:
    # \f[
    # A_u=\int_{z_0}^{z_t}\frac{g}{c_pT(z)}\frac{\eta}{1 + \gamma}[h(z)-h^*(z)]dz
    # \f]
    # (discretized according to Grell (1993) \cite grell_1993 equation B.10 using
    # B.2 and B.3 of Arakawa and Schubert (1974) \cite arakawa_and_schubert_1974
    # and assuming \f$\eta=1\f$) where \f$A_u\f$ is the updraft cloud work function,
    # \f$z_0\f$ and \f$z_t\f$ are cloud base and cloud top, respectively,
    # \f$\gamma=\frac{L}{c_p}\left(\frac{\partial\overline{q_s}}{\partial T}\right)_p\f$
    # and other quantities are previously defined.
    with computation(FORWARD), interval(0, 1):
        if cnvflg:
            aa1 = 0.0

    with computation(FORWARD), interval(1, -1):
        dz1 = 0.0
        if cnvflg:
            if k_mask >= kbcon and k_mask < ktcon:
                dz1 = zo[0, 0, 1] - zo
                aa1 = aa1 + buo * dz1

    # To make all slices like final slice
    with computation(FORWARD), interval(-1, None):
        if cnvflg and aa1 <= 0.0:
            cnvflg = False


def stencil_static12(
    cnvflg: BoolFieldIJ,
    aa1: FloatFieldIJ,
    flg: BoolFieldIJ,
    ktcon1: IntFieldIJ,
    kbm: IntFieldIJ,
    k_mask: IntField,
    ktcon: IntFieldIJ,
    zo: FloatField,
    qeso: FloatField,
    to: FloatField,
    dbyo: FloatField,
    zi: FloatField,
    xlamue: FloatField,
    xlamud: FloatFieldIJ,
    qcko: FloatField,
    qrcko: FloatField,
    qo: FloatField,
    eta: FloatField,
    del0: FloatField,
    c0t: FloatField,
    pwo: FloatField,
    cnvwt: FloatField,
    buo: FloatField,
    wu2: FloatField,
    wc: FloatFieldIJ,
    sumx: FloatFieldIJ,
    kbcon1: IntFieldIJ,
    drag: FloatField,
    dellal: FloatField,
):
    # Estimate the convective overshooting as the level
    #   where the [aafac * cloud work function] becomes zero,
    #   which is the final cloud top
    #   limited to the level of P/Ps=0.7

    # Continue calculating the cloud work function past the point of neutral buoyancy to
    # represent overshooting according to Han and Pan (2011) \cite han_and_pan_2011.
    # Convective overshooting stops when \f$ cA_u < 0\f$ where \f$c\f$ is currently 10%,
    # or when 10% of the updraft cloud work function has been consumed by the stable
    # buoyancy force.
    # Overshooting is also limited to the level where \f$p=0.7p_{sfc}\f$.
    from __externals__ import c1, ncloud

    with computation(FORWARD):
        with interval(0, 1):
            if cnvflg:
                aa1 = sccons.AAFAC * aa1

            flg = cnvflg
            ktcon1 = kbm

        with interval(1, -1):
            dz1 = 0.0
            gamma = 0.0
            rfact = 0.0

            if flg:
                if k_mask >= ktcon and k_mask < kbm:
                    dz1 = zo[0, 0, 1] - zo
                    gamma = physcons.EL2ORC * qeso / (to * to)
                    rfact = 1.0 + (
                        physcons.DELTA * constants.CP_AIR * gamma * to / constants.HLV
                    )
                    aa1 = (
                        aa1
                        + dz1
                        * (constants.GRAV / (constants.CP_AIR * to))
                        * dbyo
                        / (1.0 + gamma)
                        * rfact
                    )

                    # val = 0.
                    # aa1(i) = aa1(i) +
                    #         dz1 * eta(i,k_mask) * g * delta *
                    #         dz1 * g * delta *
                    #         max(val,(qeso(i,k_mask) - qo(i,k_mask)))

                    if aa1 < 0.0:
                        ktcon1 = k_mask
                        flg = False

    # Compute cloud moisture property, detraining cloud water
    # and precipitation in overshooting layers

    with computation(FORWARD), interval(1, -1):
        dz = 0.0
        gamma = 0.0
        qrch = 0.0
        tem = 0.0
        tem1 = 0.0
        factor = 0.0
        dq = 0.0
        etah = 0.0
        ptem = 0.0
        qlk = 0.0
        dp = 0.0

        if cnvflg:
            if k_mask >= ktcon and k_mask < ktcon1:
                # For the overshooting convection, calculate the moisture content of
                # the entraining/detraining parcel as before. Partition convective
                # cloud water and precipitation and detrain convective cloud water in
                # the overshooting layers.
                dz = zi - zi[0, 0, -1]
                gamma = physcons.EL2ORC * qeso / (to * to)
                qrch = qeso + gamma * dbyo / (constants.HLV * (1.0 + gamma))
                tem = 0.5 * (xlamue + xlamue[0, 0, -1]) * dz
                tem1 = 0.5 * xlamud * dz
                factor = 1.0 + tem - tem1
                qcko = (
                    (1.0 - tem1) * qcko[0, 0, -1] + tem * 0.5 * (qo + qo[0, 0, -1])
                ) / factor
                qrcko = qcko
                dq = eta * (qcko - qrch)

                # Check if there is excess moisture to release latent heat
                if dq > 0.0:
                    etah = 0.5 * (eta + eta[0, 0, -1])
                    dp = 1000.0 * del0
                    if ncloud > 0:
                        ptem = c0t + c1
                        qlk = dq / (eta + etah * ptem * dz)
                        dellal = etah * c1 * dz * qlk * constants.GRAV / dp
                    else:
                        qlk = dq / (eta + etah * c0t * dz)

                    qcko = qlk + qrch
                    pwo = etah * c0t * dz * qlk
                    cnvwt = etah * qlk * constants.GRAV / dp

    # Compute updraft velocity square(wu2)
    # Calculate updraft velocity square(wu2) according to Han et al.'s
    # (2017) \cite han_et_al_2017 equation 7.
    with computation(FORWARD), interval(1, -1):
        dz = 0.0
        tem = 0.0
        tem1 = 0.0
        ptem = 0.0
        ptem1 = 0.0

        # bb1 = 2. * (1.+bet1*cd1)
        # bb2 = 2. / (f1*(1.+gam1))

        # bb1 = 3.9
        # bb2 = 0.67

        # bb1 = 2.0
        # bb2 = 4.0

        bb1 = 4.0
        bb2 = 0.8
        if cnvflg:
            if k_mask > kbcon1 and k_mask < ktcon:
                dz = zi - zi[0, 0, -1]
                tem = 0.25 * bb1 * (drag + drag[0, 0, -1]) * dz
                tem1 = 0.5 * bb2 * (buo + buo[0, 0, -1]) * dz
                ptem = (1.0 - tem) * wu2[0, 0, -1]
                ptem1 = 1.0 + tem
                wu2 = (ptem + tem1) / ptem1
                wu2 = max(wu2, 0.0)

    # Compute updraft velocity averaged over the whole cumulus
    # Calculate the mean updraft velocity within the cloud (wc).
    with computation(FORWARD):
        with interval(0, 1):
            wc = 0.0
            sumx = 0.0

        with interval(1, -1):
            dz = 0.0
            tem = 0.0

            if cnvflg:
                if k_mask > kbcon1 and k_mask < ktcon:
                    dz = zi - zi[0, 0, -1]
                    tem = 0.5 * (sqrt(wu2) + sqrt(wu2[0, 0, -1]))
                    wc = wc + tem * dz
                    sumx = sumx + dz

        with interval(-1, None):
            if cnvflg:
                if sumx == 0.0:
                    cnvflg = False
                else:
                    wc = wc / sumx

                # val = 1.e-4
                if wc < 1.0e-4:
                    cnvflg = False

    # Exchange ktcon with ktcon1
    with computation(FORWARD), interval(-1, None):
        kk = 0
        if cnvflg:
            kk = ktcon
            ktcon = ktcon1
            ktcon1 = kk


# if(ncloud > 0):
def stencil_static13(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    ktcon: IntFieldIJ,
    qeso: FloatField,
    to: FloatField,
    dbyo: FloatField,
    qcko: FloatField,
    qlko_ktcon: FloatFieldIJ,
):
    # This section is ready for cloud water
    # compute liquid and vapor separation at cloud top
    # Separate the total updraft cloud water at cloud top into vapor and condensate.
    with computation(FORWARD), interval(...):
        gamma = 0.0
        qrch = 0.0
        dq = 0.0

        if cnvflg:
            if k_mask == ktcon - 1:
                gamma = physcons.EL2ORC * qeso / (to * to)
                qrch = qeso + gamma * dbyo / (constants.HLV * (1.0 + gamma))
                dq = qcko - qrch
                # Check if there is excess moisture to release latent heat
                if dq > 0.0:
                    qlko_ktcon = dq
                    qcko = qrch


# endif


def stencil_static14(
    cnvflg: BoolFieldIJ,
    vshear: FloatFieldIJ,
    k_mask: IntField,
    kb: IntFieldIJ,
    ktcon: IntFieldIJ,
    uo: FloatField,
    vo: FloatField,
    zi: FloatField,
    zi_kb: FloatFieldIJ,
    zi_ktcon: FloatFieldIJ,
    edt: FloatFieldIJ,
):
    # Compute precipitation efficiency in terms of windshear
    # Calculate the wind shear and precipitation efficiency according to equation 58
    # in Fritsch and Chappell (1980) \cite fritsch_and_chappell_1980 :
    # \f[
    # E = 1.591 - 0.639\frac{\Delta V}{\Delta z} +
    # 0.0953\left(\frac{\Delta V}{\Delta z}\right)^2
    # - 0.00496\left(\frac{\Delta V}{\Delta z}\right)^3
    # \f]
    # where \f$\Delta V\f$ is the integrated horizontal shear over the cloud depth,
    # \f$\Delta z\f$, (the ratio is converted to units of \f$10^{-3} s^{-1}\f$).
    # The variable "edt" is \f$1-E\f$ and is constrained to the range \f$[0,0.9]\f$.
    with computation(FORWARD), interval(0, 1):
        zi_kb = 0.0
        zi_ktcon = 0.0
        if cnvflg:
            vshear = 0.0

        if k_mask == kb:
            zi_kb = zi
        if k_mask == ktcon:
            zi_ktcon = zi

    with computation(FORWARD), interval(1, None):
        if cnvflg:
            if k_mask > kb and k_mask <= ktcon:
                # shear = ((uo-uo[0,0,-1]) ** 2 \
                #      + (vo-vo[0,0,-1]) ** 2)**0.5
                vshear = vshear + sqrt(
                    (uo - uo[0, 0, -1]) ** 2 + (vo - vo[0, 0, -1]) ** 2
                )

        if k_mask == kb:
            zi_kb = zi
        if k_mask == ktcon:
            zi_ktcon = zi

    with computation(FORWARD), interval(0, 1):
        if cnvflg:
            # Use ziktcon and zikb to represent zi(ktcon) and zi(kb)
            vshear = 1.0e3 * vshear / (zi_ktcon - zi_kb)

            e1 = 1.591 - 0.639 * vshear + 0.0953 * (vshear**2) - 0.00496 * (vshear**3)

            edt = 1.0 - e1
            # val = .9
            edt = min(edt, 0.9)
            # val = .0
            edt = max(edt, 0.0)


def comp_tendencies(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kmax: IntFieldIJ,
    kb: IntFieldIJ,
    ktcon: IntFieldIJ,
    ktcon1: IntFieldIJ,
    kbcon1: IntFieldIJ,
    kbcon: IntFieldIJ,
    dellah: FloatField,
    dellaq: FloatField,
    dellau: FloatField,
    dellav: FloatField,
    del0: FloatField,
    zi: FloatField,
    zi_ktcon: FloatFieldIJ,
    zi_kbcon: FloatFieldIJ,
    heo: FloatField,
    qo: FloatField,
    xlamue: FloatField,
    xlamud: FloatFieldIJ,
    eta: FloatField,
    hcko: FloatField,
    qrcko: FloatField,
    uo: FloatField,
    ucko: FloatField,
    vo: FloatField,
    vcko: FloatField,
    qcko: FloatField,
    dellal: FloatField,
    qlko_ktcon: FloatFieldIJ,
    wc: FloatFieldIJ,
    gdx: FloatFieldIJ,
    dtconv: FloatFieldIJ,
    u1: FloatField,
    v1: FloatField,
    po: FloatField,
    to: FloatField,
    tauadv: FloatFieldIJ,
    xmb: FloatFieldIJ,
    sigmagfm: FloatFieldIJ,
    garea: FloatFieldIJ,
    scaldfunc: FloatFieldIJ,
    xmbmax: FloatFieldIJ,
    sumx: FloatFieldIJ,
    umean: FloatFieldIJ,
):
    # what would the change be, that a cloud with unit mass
    # will do to the environment?
    # Calculate the change in moist static energy, moisture
    # mixing ratio, and horizontal winds per unit cloud base mass
    # flux for all layers below cloud top from equations B.14
    # and B.15 from Grell (1993) \cite grell_1993, and for the
    # cloud top from B.16 and B.17
    from __externals__ import dt2

    # Initialize zi_ktcon and zi_kbcon fields (propagate forward)
    with computation(FORWARD), interval(0, 1):
        zi_ktcon = 0.0
        zi_kbcon = 0.0

    with computation(FORWARD), interval(...):
        if k_mask == ktcon1:
            zi_ktcon = zi
        if k_mask == kbcon1:
            zi_kbcon = zi

    with computation(PARALLEL), interval(...):
        if cnvflg and k_mask <= kmax:
            dellah = 0.0
            dellaq = 0.0
            dellau = 0.0
            dellav = 0.0

    with computation(PARALLEL), interval(1, -1):
        # changed due to subsidence and entrainment

        dp = 0.0
        dz = 0.0
        gdp = 0.0

        dv1h = 0.0
        dv3h = 0.0
        dv2h = 0.0

        dv1q = 0.0
        dv3q = 0.0
        dv2q = 0.0

        tem = 0.0
        tem1 = 0.0
        tem2 = 0.0

        # Changes due to subsidence and entrainment
        if cnvflg and k_mask > kb and k_mask < ktcon:
            dp = 1000.0 * del0
            dz = zi[0, 0, 0] - zi[0, 0, -1]
            gdp = constants.GRAV / dp

            dv1h = heo[0, 0, 0]
            dv3h = heo[0, 0, -1]
            dv2h = 0.5 * (dv1h + dv3h)

            dv1q = qo[0, 0, 0]
            dv3q = qo[0, 0, -1]
            dv2q = 0.5 * (dv1q + dv3q)

            tem = 0.5 * (xlamue[0, 0, 0] + xlamue[0, 0, -1])
            tem1 = xlamud

            dellah = (
                dellah
                + (
                    eta * dv1h
                    - eta[0, 0, -1] * dv3h
                    - tem * eta[0, 0, -1] * dv2h * dz
                    + tem1 * eta[0, 0, -1] * 0.5 * (hcko + hcko[0, 0, -1]) * dz
                )
                * constants.GRAV
                / dp
            )

            dellaq = (
                dellaq
                + (
                    eta * dv1q
                    - eta[0, 0, -1] * dv3q
                    - tem * eta[0, 0, -1] * dv2q * dz
                    + tem1 * eta[0, 0, -1] * 0.5 * (qrcko + qcko[0, 0, -1]) * dz
                )
                * constants.GRAV
                / dp
            )

            tem1 = eta * (uo - ucko)
            tem2 = eta[0, 0, -1] * (uo[0, 0, -1] - ucko[0, 0, -1])
            dellau = dellau + (tem1 - tem2) * constants.GRAV / dp

            tem1 = eta * (vo - vcko)
            tem2 = eta[0, 0, -1] * (vo[0, 0, -1] - vcko[0, 0, -1])
            dellav = dellav + (tem1 - tem2) * constants.GRAV / dp

    with computation(PARALLEL), interval(1, None):
        tfac = 0.0

        # Cloud top
        if cnvflg:
            if k_mask == ktcon:
                dp = 1000.0 * del0

                dv1h = heo[0, 0, -1]
                dellah = eta[0, 0, -1] * (hcko[0, 0, -1] - dv1h) * constants.GRAV / dp

                dv1q = qo[0, 0, -1]
                dellaq = eta[0, 0, -1] * (qcko[0, 0, -1] - dv1q) * constants.GRAV / dp

                dellau = (
                    eta[0, 0, -1]
                    * (ucko[0, 0, -1] - uo[0, 0, -1])
                    * constants.GRAV
                    / dp
                )
                dellav = (
                    eta[0, 0, -1]
                    * (vcko[0, 0, -1] - vo[0, 0, -1])
                    * constants.GRAV
                    / dp
                )

                # Cloud water
                dellal = eta[0, 0, -1] * qlko_ktcon * constants.GRAV / dp

    with computation(FORWARD), interval(0, 1):
        # compute convective turn-over time
        # Following Bechtold et al. (2008) \cite
        # bechtold_et_al_2008, calculate the convective turnover
        # time using the mean updraft velocity (wc) and the cloud
        # depth. It is also proportional to the grid size (gdx).
        if cnvflg:
            tauadv = 0.0
            tem = zi_ktcon - zi_kbcon
            tfac = 1.0 + gdx / 75000.0
            dtconv = tem / wc
            dtconv = tfac * dtconv
            dtconv = max(dtconv, sccons.DTMIN)
            dtconv = max(dtconv, dt2)
            dtconv = min(dtconv, sccons.DTMAX)

            # Initialize field for advective time scale computation
            sumx = 0.0
            umean = 0.0

    # Calculate advective time scale (tauadv) using a mean cloud layer
    # wind speed
    with computation(FORWARD), interval(1, -1):
        if cnvflg:
            if k_mask >= kbcon1 and k_mask < ktcon1:
                dz = zi[0, 0, 0] - zi[0, 0, -1]
                tem = sqrt(u1 * u1 + v1 * v1)
                umean = umean + tem * dz
                sumx = sumx + dz
    with computation(FORWARD), interval(-1, None):
        if cnvflg:
            umean = umean / sumx
            umean = max(umean, 1.0)
            tauadv = gdx / umean

    with computation(FORWARD), interval(...):
        # compute cloud base mass flux as a function of the mean
        # updraft velcoity
        # From Han et al.'s (2017) \cite han_et_al_2017 equation
        # 6, calculate cloud base mass flux as a function of the
        # mean updraft velocity
        if cnvflg and k_mask == kbcon:
            rho = po * 100.0 / (constants.RDGAS * to)
            tfac = tauadv / dtconv
            tfac = min(tfac, 1.0)
            xmb = tfac * sccons.BETAW * rho * wc

            # For scale-aware parameterization, the updraft fraction
            # (sigmagfm) is first computed as a function of the
            # lateral entrainment rate at cloud base (see Han et
            # al.'s (2017) \cite han_et_al_2017 equation 4 and 5),
            # following the study by Grell and Freitas (2014) \cite
            # grell_and_freitus_2014
            tem = max(xlamue, 2.0e-4)
            tem = min(tem, 6.0e-4)
            tem = 0.2 / tem
            tem1 = 3.14 * tem * tem

            sigmagfm = tem1 / garea
            sigmagfm = max(sigmagfm, 0.001)
            sigmagfm = min(sigmagfm, 0.999)

    with computation(FORWARD), interval(0, 1):
        # Then, calculate the reduction factor (scaldfunc) of the
        # vertical convective eddy transport of mass flux as a
        # function of updraft fraction from the studies by Arakawa
        # and Wu (2013) \cite arakawa_and_wu_2013 (also see Han et
        # al.'s (2017) \cite han_et_al_2017 equation 1 and 2). The
        # final cloud base mass flux with scale-aware
        # parameterization is obtained from the mass flux when
        # sigmagfm << 1, multiplied by the reduction factor (Han et
        # al.'s (2017) \cite han_et_al_2017 equation 2).
        if cnvflg:
            if gdx < sccons.DXCRT:
                scaldfunc = (1.0 - sigmagfm) * (1.0 - sigmagfm)
                scaldfunc = min(scaldfunc, 1.0)
                scaldfunc = max(scaldfunc, 0.0)
            else:
                scaldfunc = 1.0

            xmb = xmb * scaldfunc
            xmb = min(xmb, xmbmax)


def comp_tendencies_tr(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kmax: IntFieldIJ,
    kb: IntFieldIJ,
    ktcon: IntFieldIJ,
    dellae: FloatFieldTracer,
    del0: FloatField,
    eta: FloatField,
    ctro: FloatFieldTracer,
    ecko: FloatFieldTracer,
    n_tracer: Int,
):
    with computation(PARALLEL), interval(...):
        if cnvflg and k_mask <= kmax:
            dellae[0, 0, 0][n_tracer] = 0.0

    with computation(PARALLEL), interval(1, -1):
        tem1 = 0.0
        tem2 = 0.0
        dp = 0.0

        if cnvflg and k_mask > kb and k_mask < ktcon:
            # Changes due to subsidence and entrainment
            dp = 1000.0 * del0

            tem1 = eta[0, 0, 0] * (ctro[0, 0, 0][n_tracer] - ecko[0, 0, 0][n_tracer])
            tem2 = eta[0, 0, -1] * (ctro[0, 0, -1][n_tracer] - ecko[0, 0, -1][n_tracer])

            dellae[0, 0, 0][n_tracer] = (
                dellae[0, 0, 0][n_tracer] + (tem1 - tem2) * constants.GRAV / dp
            )

    with computation(PARALLEL), interval(1, None):
        # Cloud top
        if cnvflg and ktcon == k_mask:
            dp = 1000.0 * del0
            dellae[0, 0, 0][n_tracer] = (
                eta[0, 0, -1]
                * (ecko[0, 0, -1][n_tracer] - ctro[0, 0, -1][n_tracer])
                * constants.GRAV
                / dp
            )


def feedback_control_update_mass_flux(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kmax: IntFieldIJ,
    kb: IntFieldIJ,
    ktcon: IntFieldIJ,
    flg: BoolFieldIJ,
    islimsk: IntFieldIJ,
    ktop: IntFieldIJ,
    kbot: IntFieldIJ,
    kbcon: IntFieldIJ,
    kcnv: IntFieldIJ,
    qeso: FloatField,
    pfld: FloatField,
    delhbar: FloatFieldIJ,
    delqbar: FloatFieldIJ,
    deltbar: FloatFieldIJ,
    delubar: FloatFieldIJ,
    delvbar: FloatFieldIJ,
    qcond: FloatFieldIJ,
    dellah: FloatField,
    dellaq: FloatField,
    t1: FloatField,
    xmb: FloatFieldIJ,
    qtr: FloatFieldTracer,
    u1: FloatField,
    dellau: FloatField,
    v1: FloatField,
    dellav: FloatField,
    del0: FloatField,
    rntot: FloatFieldIJ,
    delqev: FloatFieldIJ,
    delq2: FloatFieldIJ,
    pwo: FloatField,
    deltv: FloatFieldIJ,
    delq: FloatFieldIJ,
    qevap: FloatFieldIJ,
    rn: FloatFieldIJ,
    edt: FloatFieldIJ,
    cnvw: FloatField,
    cnvwt: FloatField,
    cnvc: FloatField,
    ud_mf: FloatField,
    dt_mf: FloatField,
    eta: FloatField,
):
    # For the "feedback control", calculate updated values of
    # the state variables by multiplying the cloud base mass
    # flux and the tendencies calculated per unit cloud base
    # mass flux from the static control.
    # Recalculate saturation specific humidity.
    from __externals__ import dt2, ntvap

    with computation(FORWARD), interval(0, 1):
        # Initialize flg
        flg = cnvflg
        rntot = 0.0
        delqev = 0.0
        delq2 = 0.0
        delhbar = 0.0
        delqbar = 0.0
        deltbar = 0.0
        delubar = 0.0
        delvbar = 0.0
        qcond = 0.0

    with computation(FORWARD), interval(...):
        dellat = 0.0
        fpvst1 = 0.0
        if cnvflg and (k_mask <= kmax):
            qeso = 0.01 * fpvs(t1)  # fpvs is in Pa
            qeso = constants.EPS * qeso / (pfld + (constants.EPS - 1) * qeso)
            val = 1.0e-8
            qeso = max(qeso, val)

        # - Calculate the temperature tendency from the moist
        #   static energy and specific humidity tendencies
        # - Update the temperature, specific humidity, and
        #   horizontal wind state variables by multiplying the
        #   cloud base mass flux-normalized tendencies by the
        #   cloud base mass flux
        # Accumulate column-integrated tendencies:
        if cnvflg:
            if k_mask > kb and k_mask <= ktcon:
                dellat = (dellah - constants.HLV * dellaq) / constants.CP_AIR
                t1 = t1 + dellat * xmb * dt2
                qtr[0, 0, 0][ntvap] = qtr[0, 0, 0][ntvap] + dellaq * xmb * dt2
                u1 = u1 + dellau * xmb * dt2
                v1 = v1 + dellav * xmb * dt2

                dp = 1000.0 * del0
                delhbar = delhbar + dellah * xmb * dp / constants.GRAV
                delqbar = delqbar + dellaq * xmb * dp / constants.GRAV
                deltbar = deltbar + dellat * xmb * dp / constants.GRAV
                delubar = delubar + dellau * xmb * dp / constants.GRAV
                delvbar = delvbar + dellav * xmb * dp / constants.GRAV

    with computation(FORWARD), interval(...):
        fpvst1 = 0.01 * fpvs(t1)  # fpvs is in Pa
        if cnvflg:
            if k_mask > kb and k_mask <= ktcon:
                # Recalculate saturation specific humidity using the
                # updated temperature
                qeso = fpvst1
                qeso = constants.EPS * qeso / (pfld + (constants.EPS - 1) * qeso)
                val = 1.0e-8
                qeso = max(qeso, val)

    with computation(FORWARD), interval(1, -1):
        # Add up column-integrated convective precipitation by
        # multiplying the normalized value by the cloud base
        # mass flux (propagate forward)

        if cnvflg:
            if (k_mask < ktcon) and (k_mask > kb):
                rntot = rntot + pwo * xmb * 0.001 * dt2

    # evaporating rain
    # Determine the evaporation of the convective precipitation
    # and update the integrated convective precipitation
    # Update state temperature and moisture to account for
    # evaporation of convective precipitation
    # Update column-integrated tendencies to account for
    # evaporation of convective precipitation
    with computation(BACKWARD):
        with interval(...):
            evef = 0.0
            dp = 0.0
            tem = 0.0
            tem1 = 0.0

            if k_mask <= kmax:
                deltv = 0.0
                delq = 0.0
                qevap = 0.0

                if cnvflg:
                    if k_mask > kb and k_mask < ktcon:
                        rn = rn + pwo * xmb * 0.001 * dt2

                if flg and k_mask < ktcon:
                    if islimsk == 1:
                        evef = edt * sccons.EVFACTL
                    else:
                        evef = edt * sccons.EVFACT
                    qcond = (
                        evef
                        * (qtr[0, 0, 0][ntvap] - qeso)
                        / (1.0 + physcons.EL2ORC * qeso / (t1 * t1))
                    )

                    dp = 1000.0 * del0
                    if rn > 0.0 and qcond < 0.0:
                        tem = -0.32 * sqrt(dt2 * rn)
                        tem = exp(tem)
                        qevap = -qcond * (1.0 - tem)
                        tem = rn * 1000.0 * constants.GRAV / dp
                        qevap = min(qevap, tem)
                        delq2 = delqev + 0.001 * qevap * dp / constants.GRAV

                    if rn > 0.0 and qcond < 0.0 and delq2 > rntot:
                        qevap = 1000.0 * constants.GRAV * (rntot - delqev) / dp
                        flg = False

                    if rn > 0.0 and qevap > 0.0:
                        tem = 0.001 * dp / constants.GRAV
                        tem1 = qevap * tem
                        if tem1 > rn:
                            qevap = rn / tem
                            rn = 0.0
                        else:
                            rn = rn - tem1

                        qtr[0, 0, 0][ntvap] = qtr[0, 0, 0][ntvap] + qevap
                        t1 = t1 - physcons.ELOCP * qevap
                        deltv = -physcons.ELOCP * qevap / dt2
                        delq = qevap / dt2

                        delqev = delqev + 0.001 * dp * qevap / constants.GRAV

                    delqbar = delqbar + delq * dp / constants.GRAV
                    deltbar = deltbar + deltv * dp / constants.GRAV

    with computation(BACKWARD), interval(0, 1):
        if cnvflg:
            if (rn < 0.0) or (not flg):
                rn = 0.0
            ktop = ktcon
            kbot = kbcon
            kcnv = 2

    with computation(FORWARD), interval(...):
        # convective cloud water
        val1 = 0.0
        if cnvflg and k_mask >= kbcon and k_mask < ktcon:
            # Calculate shallow convective cloud water
            cnvw = cnvwt * xmb * dt2
            # convective cloud cover
            # Calculate convective cloud cover, which is used when pdf-based
            # cloud fraction is used (i.e., pdfcld=.true.).
            cnvc = 0.04 * log(1.0 + 675.0 * eta * xmb)
            cnvc = min(cnvc, 0.2)
            cnvc = max(cnvc, 0.0)

        # hchuang code change
        # Calculate and retain the updraft mass flux for dust transport
        # by cumulus convection.
        # Calculate the updraft convective mass flux.
        if cnvflg:
            # Calculate the updraft convective mass flux
            if k_mask >= kb and k_mask < ktop:
                ud_mf = eta * xmb * dt2

            # Save the updraft convective mass flux at cloud top
            if k_mask == ktop - 1:
                dt_mf = ud_mf


def feedback_control_upd_trr(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kmax: IntFieldIJ,
    ktcon: IntFieldIJ,
    del0: FloatField,
    delebar: FloatFieldTracer,
    ctr: FloatFieldTracer,
    dellae: FloatFieldTracer,
    xmb: FloatFieldIJ,
    qtr: FloatFieldTracer,
    n_tracer: Int,
):
    from __externals__ import dt2

    with computation(FORWARD), interval(0, 1):
        delebar[0, 0, 0][n_tracer] = 0.0  # Should be an [i, j, n_tracer] field
        dp = 1000.0 * del0
        if cnvflg and k_mask <= kmax:
            if k_mask <= ktcon:
                ctr[0, 0, 0][n_tracer] = ctr[0, 0, 0][n_tracer] + (
                    dellae[0, 0, 0][n_tracer] * xmb * dt2
                )
                delebar[0, 0, 0][n_tracer] = delebar[0, 0, 0][n_tracer] + (
                    dellae[0, 0, 0][n_tracer] * xmb * dp / constants.GRAV
                )
                qtr[0, 0, 0][n_tracer] = ctr[0, 0, 0][n_tracer]
    with computation(FORWARD), interval(1, None):
        delebar[0, 0, 0][n_tracer] = delebar[0, 0, -1][n_tracer]
        dp = 1000.0 * del0

        if cnvflg and k_mask <= kmax:
            if k_mask <= ktcon:
                ctr[0, 0, 0][n_tracer] = ctr[0, 0, 0][n_tracer] + (
                    dellae[0, 0, 0][n_tracer] * xmb * dt2
                )
                delebar[0, 0, 0][n_tracer] = delebar[0, 0, 0][n_tracer] + (
                    dellae[0, 0, 0][n_tracer] * xmb * dp / constants.GRAV
                )
                qtr[0, 0, 0][n_tracer] = ctr[0, 0, 0][n_tracer]

    with computation(BACKWARD), interval(...):
        # Propagate backward delebar values
        delebar = delebar[0, 0, 1]


def store_aero_conc(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kmax: IntFieldIJ,
    rn: FloatFieldIJ,
    qtr: FloatFieldTracer,
    qaero: FloatField,
    n_tracer: Int,
    k_aerosol: Int,
):
    with computation(PARALLEL), interval(...):
        # Store aerosol concentrations if present
        if cnvflg and rn > 0.0 and k_mask <= kmax:
            qtr[0, 0, 0][n_tracer] = qaero[0, 0, 0][k_aerosol]


def separate_detrained_cw(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kbcon: IntFieldIJ,
    ktcon: IntFieldIJ,
    dellal: FloatField,
    xmb: FloatFieldIJ,
    t1: FloatField,
    qtr: FloatFieldTracer,
):
    from __externals__ import dt2, ntcw, ntiw

    with computation(FORWARD), interval(0, -1):
        # cloud water
        # Separate detrained cloud water into liquid and ice species as
        # a function of temperature only

        tem = 0.0
        tem1 = 0.0

        if cnvflg and k_mask >= kbcon and k_mask <= ktcon:
            tem = dellal * xmb * dt2
            tem1 = (sccons.SHAL_TCR - t1) * sccons.SHAL_TCRF
            tem1 = min(1.0, tem1)
            tem1 = max(0.0, tem1)

            if qtr[0, 0, 0][ntcw] > -999.0:
                qtr[0, 0, 0][ntiw] = qtr[0, 0, 0][ntiw] + tem * tem1  # ice
                qtr[0, 0, 0][ntcw] = qtr[0, 0, 0][ntcw] + tem * (1.0 - tem1)  # water
            else:
                qtr[0, 0, 0][ntiw] = qtr[0, 0, 0][ntiw] + tem


def tke_contribution(
    cnvflg: BoolFieldIJ,
    k_mask: IntField,
    kb: IntFieldIJ,
    ktop: IntFieldIJ,
    eta: FloatField,
    xmb: FloatFieldIJ,
    pfld: FloatField,
    t1: FloatField,
    sigmagfm: FloatFieldIJ,
    qtr: FloatFieldTracer,
):
    # Include TKE contribution from shallow convection
    from __externals__ import ntk

    with computation(FORWARD), interval(1, -1):
        tem = 0.0
        tem1 = 0.0
        ptem = 0.0

        if cnvflg and k_mask > kb and k_mask < ktop:
            tem = 0.5 * (eta[0, 0, -1] + eta[0, 0, 0]) * xmb
            tem1 = pfld * 100.0 / (constants.RDGAS * t1)
            sigmagfm = max(sigmagfm, sccons.BETAW)
            ptem = tem / (sigmagfm * tem1)
            qtr[0, 0, 0][ntk] = qtr[0, 0, 0][ntk] + 0.5 * sigmagfm * ptem * ptem


class ScaleAwareMassFluxShallowConvection:
    """
    Fortran name is samfshalconv, original docstring follows:
    The scale-aware mass-flux shallow (SAMF_shal) convection scheme is an updated
    version of the previous mass-flux shallow convection scheme with scale and aerosol
    awareness and parameterizes the effect of shallow convection on the environment.
    The SAMF_shal scheme is similar to the SAMF deep convection scheme but with a few
    key differences. First, no quasi-equilibrium assumption is used for any grid size
    and the shallow cloud base mass flux is parameterized using a mean updraft velocity.
    Further, there are no convective downdrafts, the entrainment rate is greater than
    for deep convection, and the shallow convection is limited to not extend over the
    level where \f$p=0.7p_{sfc}\f$. The paramerization of scale and aerosol awareness
    follows that of the SAMF deep convection scheme.

    The previous version of the shallow convection scheme (shalcnv.f) is described in
    Han and Pan (2011) cite han_and_pan_2011 and differences between the shallow and
    deep convection schemes are presented in Han and Pan (2011) cite han_and_pan_2011
    and Han et al. (2017) cite han_et_al_2017 . Details of scale- and aerosol-aware
    parameterizations are described in Han et al. (2017) cite han_et_al_2017 .

    In further update for FY19 GFS implementation, interaction with turbulent kinetic
    energy (TKE), which is a prognostic variable used in a scale-aware TKE-based moist
    EDMF vertical turbulent mixing scheme, is included. Entrainment rates in updrafts
    are proportional to sub-cloud mean TKE. TKE is transported by cumulus convection.
    TKE contribution from cumulus convection is deduced from cumulus mass flux. On the
    other hand, tracers such as ozone and aerosol are also transported by cumulus
    convection.

    To reduce too much convective cooling at the cloud top, the convection schemes have
    been modified for the rain conversion rate, entrainment and detrainment rates,
    overshooting layers, and maximum allowable cloudbase mass flux (as of June 2018).
    section intraphysics Intraphysics Communication

    This routine follows the \ref SAMF deep scheme quite closely, although it can be
    interpreted as only having the "static" and "feedback" control portions, since the
    "dynamic" control is not necessary to find the cloud base mass flux. The algorithm
    is simplified from SAMF deep convection by excluding convective downdrafts and
    being confined to operate below \f$p=0.7p_{sfc}\f$. Also, entrainment is both
    simpler and stronger in magnitude compared to the deep scheme.

    param[in] im number of used points
    param[in] ix horizontal dimension
    param[in] km vertical layer dimension
    param[in] delt physics time step in seconds
    param[in] ntk index for TKE
    param[in] ntr total number of tracers including TKE
    param[in] delp pressure difference between level k and k+1 (Pa)
    param[in] prslp mean layer presure (Pa)
    param[in] psp surface pressure (Pa)
    param[in] phil layer geopotential (\f$m^s/s^2\f$)
    param[in] qtr tracer array including cloud condensate (\f$kg/kg\f$)
    param[inout] ql cloud water or ice (kg/kg)
    param[inout] q1 updated tracers (kg/kg)
    param[inout] t1 updated temperature (K)
    param[inout] u1 updated zonal wind (\f$m s^{-1}\f$)
    param[inout] v1 updated meridional wind (\f$m s^{-1}\f$)
    param[out] rn convective rain (m)
    param[out] kbot index for cloud base
    param[out] ktop index for cloud top
    param[out] kcnv flag to denote deep convection (0=no, 1=yes)
    param[in] islimsk sea/land/ice mask (=0/1/2)
    param[in] dot layer mean vertical velocity (Pa/s)
    param[in] ncloud number of cloud species
    param[in] hpbl PBL height (m)
    param[in] heat surface sensible heat flux (K m/s)
    param[in] evap surface latent heat flux (kg/kg m/s)
    param[out] ud_mf updraft mass flux multiplied by time step (\f$kg/m^2\f$)
    param[out] dt_mf ud_mf at cloud top (\f$kg/m^2\f$)
    param[out] cnvw convective cloud water (kg/kg)
    param[out] cnvc convective cloud cover (unitless)
    param[in] clam coefficient for entrainment rate
    param[in] c0s convective rain conversion parameter (1/m)
    param[in] c1 conversion parameter of detrainment from liquid water into grid-scale
        cloud water (1/m)
    param[in] pgcon reduction factor in momentum transport due to convection induced
        pressure gradient force
    param[in] asolfac aerosol-aware parameter inversely proportional to CCN number
        concentraion

    General Algorithm
    Compute preliminary quantities needed for the static and feedback control portions
        of the algorithm.
    Perform calculations related to the updraft of the entraining/detraining cloud
        model ("static control").
    The cloud base mass flux is obtained using the cumulus updraft velocity averaged
        over the whole cloud depth.
    Calculate the tendencies of the state variables (per unit cloud base mass flux) and
        the cloud base mass flux.
    For the "feedback control", calculate updated values of the state variables by
        multiplying the cloud base mass flux and the tendencies calculated per unit
        cloud base mass flux from the static control.
    """

    # TODO resolve tracers

    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        config: ShallowConvectionConfig,
    ):
        grid_indexing = stencil_factory.grid_indexing

        self._ntk = config.ntke
        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntvap = config.ntvap
        self._ntcld = config.ntcld
        self._ntr = config.nsamftrac
        self._ncloud = config.ncld
        self._dt2 = config.dt_atmos
        self._cthk = config.cthk
        self._top_shal = config.top_shal
        self._limit_shal_conv = config.limit_shal_conv

        # Determine whether to perform aerosol transport #
        self._do_aerosols = (config.itc >= 0) and (config.ntchm > 0) and (self._ntr > 0)
        if self._do_aerosols:
            self._do_aerosols = self._ntr >= config.itc
        if self._do_aerosols:
            raise NotImplementedError(
                "Shallow convection of aerosols is not implemented yet"
            )

        self._clam = config.clam_shal
        self._c0s = config.c0s_shal
        self._c1 = config.c1_shal
        self._pgcon = config.pgcon_shal
        self._asolfac = config.asolfac_shal

        self._km = grid_indexing.domain[2]
        self._km1 = grid_indexing.domain[2] - 1
        self.TRACER_DIM = TRACER_DIM

        self.quantity_factory = quantity_factory
        if self.TRACER_DIM not in self.quantity_factory.sizer.data_dimensions.keys():
            self.quantity_factory.add_data_dimensions(
                {
                    self.TRACER_DIM: int(self._ntr + 4),
                }
            )

        # Tracers are kind of borked right now. In Fortran the water vapor is passed in
        # separately, while all others come in via the variable "qtr" which has
        # dimensions (i, j, k, n_tracers - 1), ice and liquid water are stored in
        # qtr[:, :, :, 0] and qtr[:, :, :, 1] and everything is reorganized to
        # accomodate that. Aerosols live in qtr[:, :, :, itc:].
        # If we're being straightforward about it we'd have our 4D tracer array with
        # special handling for ntvapor, ntiw, and ntcw (like we do for TKE), have an
        # `if n not in [ntvap, ntiw, ntcw]:` around the other tracer calls, and then
        # do something similar with the aerosols.
        # A better solution would be to have the attributes we want accessible easily
        # so we can send the aerosols into the aerosol calculations by attribute, and
        # similarly except (or invoke) vapor etc. from the other calculations.

        def make_quantity():
            return quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type=Float):
            return quantity_factory.zeros([I_DIM, J_DIM], units="unknown", dtype=type)

        # Allocate arrays

        # Layer mask:
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(grid_indexing.domain[2]):
            self._k_mask[:, :, k] = k

        self._cnvflg = make_quantity_2D(Bool)
        self._heo_kb = make_quantity_2D()
        self._drag = make_quantity()
        self._ps = make_quantity_2D()
        self._prsl = make_quantity()
        self._del0 = make_quantity()
        self._kbcon = make_quantity_2D(Int)
        self._kbcon1 = make_quantity_2D(Int)
        self._kb = make_quantity_2D(Int)
        self._ktcon = make_quantity_2D(Int)
        self._ktconn = make_quantity_2D(Int)
        self._pdot = make_quantity_2D()
        self._qlko_ktcon = make_quantity_2D()
        self._edt = make_quantity_2D()
        self._aa1 = make_quantity_2D()
        self._cina = make_quantity_2D()
        self._vshear = make_quantity_2D()
        self._gdx = make_quantity_2D()
        self._c0 = make_quantity_2D()
        self._c0t = make_quantity()
        self._kbm = make_quantity_2D(Int)
        self._kmax = make_quantity_2D(Int)
        self._tx1 = make_quantity_2D()
        self._kpbl = make_quantity_2D(Int)
        self._flg = make_quantity_2D(Bool)
        self._zo = make_quantity()
        self._zi = make_quantity()
        self._pfld = make_quantity()
        self._eta = make_quantity()
        self._hcko = make_quantity()
        self._qcko = make_quantity()
        self._qrcko = make_quantity()
        self._ucko = make_quantity()
        self._vcko = make_quantity()
        self._dbyo = make_quantity()
        self._pwo = make_quantity()
        self._dellal = make_quantity()
        self._to = make_quantity()
        self._qo = make_quantity()
        self._uo = make_quantity()
        self._vo = make_quantity()
        self._wu2 = make_quantity()
        self._buo = make_quantity()
        self._cnvwt = make_quantity()
        self._qeso = make_quantity()
        self._heo = make_quantity()
        self._heso = make_quantity()
        self._hmax = make_quantity_2D()
        self._po = make_quantity()
        self._pfld_kb = make_quantity_2D()
        self._pfld_kbcon = make_quantity_2D()
        self._pfld_kbcon1 = make_quantity_2D()
        self._sumx = make_quantity_2D()
        self._wc = make_quantity_2D()
        self._tkemean = make_quantity_2D()
        self._clamt = make_quantity_2D()
        self._xlamue = make_quantity()
        self._xlamud = make_quantity_2D()
        self._xmbmax = make_quantity_2D()
        self._ktcon1 = make_quantity_2D(Int)
        self._zi_kb = make_quantity_2D()
        self._zi_ktcon = make_quantity_2D()
        self._zi_kbcon = make_quantity_2D()
        self._dellah = make_quantity()
        self._dellaq = make_quantity()
        self._dellau = make_quantity()
        self._dellav = make_quantity()
        self._dtconv = make_quantity_2D()
        self._tauadv = make_quantity_2D()
        self._xmb = make_quantity_2D()
        self._sigmagfm = make_quantity_2D()
        self._scaldfunc = make_quantity_2D()
        self._umean = make_quantity_2D()
        self._delhbar = make_quantity_2D()
        self._delqbar = make_quantity_2D()
        self._deltbar = make_quantity_2D()
        self._delubar = make_quantity_2D()
        self._delvbar = make_quantity_2D()
        self._qcond = make_quantity_2D()
        self._rntot = make_quantity_2D()
        self._delqev = make_quantity_2D()
        self._delq2 = make_quantity_2D()
        self._deltv = make_quantity_2D()
        self._delq = make_quantity_2D()
        self._qevap = make_quantity_2D()
        self._ptem = make_quantity_2D()
        self._pfld_ktcon = make_quantity_2D()
        self._prsl_ktcon = make_quantity_2D()

        self._ctr = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ctro = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._ecko = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._dellae = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._delebar = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        # Configure stencils
        self._pa_to_cb = stencil_factory.from_dims_halo(
            func=pa_to_cb,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_col_arr = stencil_factory.from_dims_halo(
            func=init_col_arr,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_par_and_arr = stencil_factory.from_dims_halo(
            func=init_par_and_arr,
            externals={
                "asolfac": self._asolfac,
                "c0s": self._c0s,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_kbm_kmax = stencil_factory.from_dims_halo(
            func=init_kbm_kmax,
            externals={"km": self._km},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._init_final = stencil_factory.from_dims_halo(
            func=init_final,
            compute_dims=[I_DIM, J_DIM, K_DIM],
            externals={
                "ntvap": self._ntvap,
            },
        )
        self._init_tracers = stencil_factory.from_dims_halo(
            func=init_tracers,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static0 = stencil_factory.from_dims_halo(
            func=stencil_static0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static1 = stencil_factory.from_dims_halo(
            func=stencil_static1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static2 = stencil_factory.from_dims_halo(
            func=stencil_static2,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static3 = stencil_factory.from_dims_halo(
            func=stencil_static3,
            externals={
                "ntk": self._ntk,
                "clam": self._clam,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic0 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic0,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static5 = stencil_factory.from_dims_halo(
            func=stencil_static5,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic1 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic1,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static7 = stencil_factory.from_dims_halo(
            func=stencil_static7,
            externals={"pgcon": self._pgcon},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_ntrstatic2 = stencil_factory.from_dims_halo(
            func=stencil_ntrstatic2,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_update_kbcon1_cnvflg = stencil_factory.from_dims_halo(
            func=stencil_update_kbcon1_cnvflg,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static9 = stencil_factory.from_dims_halo(
            func=stencil_static9,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static10 = stencil_factory.from_dims_halo(
            func=stencil_static10,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static11 = stencil_factory.from_dims_halo(
            func=stencil_static11,
            externals={
                "c1": self._c1,
                "cthk": self._cthk,
                "dt2": self._dt2,
                "limit_shal_conv": self._limit_shal_conv,
                "ncloud": self._ncloud,
                "top_shal": self._top_shal,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._stencil_static12 = stencil_factory.from_dims_halo(
            func=stencil_static12,
            externals={"c1": self._c1, "ncloud": self._ncloud},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        if self._ncloud > 0:
            self._stencil_static13 = stencil_factory.from_dims_halo(
                func=stencil_static13,
                compute_dims=[I_DIM, J_DIM, K_DIM],
            )
        self._stencil_static14 = stencil_factory.from_dims_halo(
            func=stencil_static14,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._comp_tendencies = stencil_factory.from_dims_halo(
            func=comp_tendencies,
            externals={"dt2": self._dt2},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._comp_tendencies_tr = stencil_factory.from_dims_halo(
            func=comp_tendencies_tr,
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._feedback_control_update_mass_flux = stencil_factory.from_dims_halo(
            func=feedback_control_update_mass_flux,
            externals={
                "dt2": self._dt2,
                "ntvap": self._ntvap,
            },
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        self._feedback_control_upd_trr = stencil_factory.from_dims_halo(
            func=feedback_control_upd_trr,
            externals={"dt2": self._dt2},
            compute_dims=[I_DIM, J_DIM, K_DIM],
        )
        if self._ncloud > 0:
            self._separate_detrained_cw = stencil_factory.from_dims_halo(
                func=separate_detrained_cw,
                externals={
                    "dt2": self._dt2,
                    "ntiw": self._ntiw,
                    "ntcw": self._ntcw,
                },
                compute_dims=[I_DIM, J_DIM, K_DIM],
            )
        if self._ntk > 0:
            self._tke_contribution = stencil_factory.from_dims_halo(
                func=tke_contribution,
                externals={
                    "ntk": self._ntk,
                },
                compute_dims=[I_DIM, J_DIM, K_DIM],
            )
        # if self._do_aerosols:
        #     self._store_aero_conc = stencil_factory.from_dims_halo(
        #         func=store_aero_conc,
        #         compute_dims=[I_DIM, J_DIM, K_DIM],
        #     )

    def __call__(
        self,
        state: SAMFShalConvState,
    ):
        # Convert input Pa terms to Cb terms
        self._pa_to_cb(
            state.psp,
            state.prslp,
            state.delp,
            self._ps,
            self._prsl,
            self._del0,
        )

        self._init_col_arr(
            state.kcnv,
            self._cnvflg,
            state.kbot,
            state.ktop,
            self._kbcon,
            self._kb,
            self._ktcon,
            self._ktconn,
            self._pdot,
            state.rn,
            self._qlko_ktcon,
            self._edt,
            self._aa1,
            self._cina,
            self._vshear,
            self._gdx,
            state.garea,
        )

        if exit_routine(self._cnvflg.view[:]):
            return

        self._init_par_and_arr(
            state.islimsk,
            self._c0,
            state.t1,
            self._c0t,
            state.cnvw,
            state.cnvc,
            state.ud_mf,
            state.dt_mf,
        )
        self._init_kbm_kmax(
            self._kbm,
            self._kmax,
            self._tx1,
            self._ps,
            self._prsl,
            self._k_mask,
        )
        self._init_final(
            self._kbm,
            self._kmax,
            self._flg,
            self._cnvflg,
            self._kpbl,
            self._prsl,
            self._zo,
            state.phil,
            self._zi,
            self._pfld,
            self._eta,
            self._hcko,
            self._qcko,
            self._qrcko,
            self._ucko,
            self._vcko,
            self._dbyo,
            self._pwo,
            self._dellal,
            self._to,
            self._qo,
            self._uo,
            self._vo,
            self._wu2,
            self._buo,
            self._drag,
            self._cnvwt,
            self._qeso,
            self._heo,
            self._heso,
            state.hpbl,
            state.t1,
            state.qtr,
            state.u1,
            state.v1,
            self._k_mask,
        )

        # Init tracers
        for n_tracer in range(self._ntr + 4):
            if (
                (n_tracer != self._ntiw)
                and (n_tracer != self._ntcw)
                and (n_tracer != self._ntcld)
                and (n_tracer != self._ntvap)
            ):
                self._init_tracers(
                    self._cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctr,
                    self._ctro,
                    self._ecko,
                    state.qtr,
                    n_tracer,
                )

        self._stencil_static0(
            self._cnvflg,
            self._hmax,
            self._heo,
            self._kb,
            self._k_mask,
            self._kpbl,
            self._kmax,
            self._zo,
            self._to,
            self._qeso,
            self._qo,
            self._po,
            self._uo,
            self._vo,
            self._heso,
            self._pfld,
        )
        for n_tracer in range(self._ntr + 4):
            if (
                (n_tracer != self._ntiw)
                and (n_tracer != self._ntcw)
                and (n_tracer != self._ntcld)
                and (n_tracer != self._ntvap)
            ):
                self._stencil_ntrstatic0(
                    self._cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static1(
            self._cnvflg,
            self._flg,
            self._kbcon,
            self._kmax,
            self._k_mask,
            self._kbm,
            self._kb,
            self._heo_kb,
            self._heo,
            self._heso,
        )

        if exit_routine(self._cnvflg.view[:]):
            return

        self._stencil_static2(
            self._cnvflg,
            self._pdot,
            state.dot,
            state.islimsk,
            self._k_mask,
            self._kbcon,
            self._kb,
            self._pfld,
            self._pfld_kb,
            self._pfld_kbcon,
        )

        if exit_routine(self._cnvflg.view[:]):
            return

        self._stencil_static3(
            self._sumx,
            self._tkemean,
            self._cnvflg,
            self._k_mask,
            self._kb,
            self._kbcon,
            self._zo,
            state.qtr,
            self._clamt,
        )

        self._stencil_static5(
            self._cnvflg,
            self._xlamue,
            self._clamt,
            self._zi,
            self._xlamud,
            self._k_mask,
            self._kbcon,
            self._kb,
            self._eta,
            self._ktconn,
            self._kmax,
            self._kbm,
            self._hcko,
            self._ucko,
            self._vcko,
            self._heo,
            self._uo,
            self._vo,
            self._ptem,
            self._flg,
        )

        for n_tracer in range(self._ntr + 4):
            if (
                (n_tracer != self._ntiw)
                and (n_tracer != self._ntcw)
                and (n_tracer != self._ntcld)
                and (n_tracer != self._ntvap)
            ):
                self._stencil_ntrstatic1(
                    self._cnvflg,
                    self._k_mask,
                    self._kb,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_static7(
            self._cnvflg,
            self._k_mask,
            self._kb,
            self._kmax,
            self._zi,
            self._xlamue,
            self._xlamud,
            self._hcko,
            self._heo,
            self._dbyo,
            self._heso,
            self._ucko,
            self._uo,
            self._vcko,
            self._vo,
        )

        for n_tracer in range(self._ntr + 4):
            if (
                (n_tracer != self._ntiw)
                and (n_tracer != self._ntcw)
                and (n_tracer != self._ntcld)
                and (n_tracer != self._ntvap)
            ):
                self._stencil_ntrstatic2(
                    self._cnvflg,
                    self._k_mask,
                    self._kb,
                    self._kmax,
                    self._zi,
                    self._xlamue,
                    self._ecko,
                    self._ctro,
                    n_tracer,
                )

        self._stencil_update_kbcon1_cnvflg(
            self._dbyo,
            self._cnvflg,
            self._kmax,
            self._kbm,
            self._kbcon,
            self._kbcon1,
            self._flg,
            self._k_mask,
        )

        if exit_routine(self._cnvflg.view[:]):
            return

        self._stencil_static9(
            self._cnvflg,
            self._pfld,
            self._pfld_kbcon,
            self._pfld_kbcon1,
            self._k_mask,
            self._kbcon1,
        )

        if exit_routine(self._cnvflg.view[:]):
            return

        self._stencil_static10(
            self._cina,
            self._cnvflg,
            self._k_mask,
            self._kb,
            self._kbcon1,
            self._zo,
            self._qeso,
            self._to,
            self._dbyo,
            self._qo,
            self._pdot,
            state.islimsk,
        )

        if exit_routine(self._cnvflg.view[:]):
            return

        self._stencil_static11(
            self._flg,
            self._cnvflg,
            self._ktcon,
            self._kbm,
            self._kbcon1,
            self._dbyo,
            self._kbcon,
            self._del0,
            self._xmbmax,
            self._aa1,
            self._kb,
            self._tx1,
            self._qcko,
            self._qo,
            self._qrcko,
            self._zi,
            self._qeso,
            self._to,
            self._xlamue,
            self._xlamud,
            self._eta,
            self._c0t,
            self._dellal,
            self._buo,
            self._drag,
            self._zo,
            self._k_mask,
            self._pwo,
            self._cnvwt,
            self._pfld,
            self._prsl,
            self._pfld_kbcon,
            self._pfld_ktcon,
            self._prsl_ktcon,
        )

        if exit_routine(self._cnvflg.view[:]):
            return

        self._stencil_static12(
            self._cnvflg,
            self._aa1,
            self._flg,
            self._ktcon1,
            self._kbm,
            self._k_mask,
            self._ktcon,
            self._zo,
            self._qeso,
            self._to,
            self._dbyo,
            self._zi,
            self._xlamue,
            self._xlamud,
            self._qcko,
            self._qrcko,
            self._qo,
            self._eta,
            self._del0,
            self._c0t,
            self._pwo,
            self._cnvwt,
            self._buo,
            self._wu2,
            self._wc,
            self._sumx,
            self._kbcon1,
            self._drag,
            self._dellal,
        )

        if self._ncloud > 0:
            self._stencil_static13(
                self._cnvflg,
                self._k_mask,
                self._ktcon,
                self._qeso,
                self._to,
                self._dbyo,
                self._qcko,
                self._qlko_ktcon,
            )

        self._stencil_static14(
            self._cnvflg,
            self._vshear,
            self._k_mask,
            self._kb,
            self._ktcon,
            self._uo,
            self._vo,
            self._zi,
            self._zi_kb,
            self._zi_ktcon,
            self._edt,
        )

        self._comp_tendencies(
            self._cnvflg,
            self._k_mask,
            self._kmax,
            self._kb,
            self._ktcon,
            self._ktcon1,
            self._kbcon1,
            self._kbcon,
            self._dellah,
            self._dellaq,
            self._dellau,
            self._dellav,
            self._del0,
            self._zi,
            self._zi_ktcon,
            self._zi_kbcon,
            self._heo,
            self._qo,
            self._xlamue,
            self._xlamud,
            self._eta,
            self._hcko,
            self._qrcko,
            self._uo,
            self._ucko,
            self._vo,
            self._vcko,
            self._qcko,
            self._dellal,
            self._qlko_ktcon,
            self._wc,
            self._gdx,
            self._dtconv,
            state.u1,
            state.v1,
            self._po,
            self._to,
            self._tauadv,
            self._xmb,
            self._sigmagfm,
            state.garea,
            self._scaldfunc,
            self._xmbmax,
            self._sumx,
            self._umean,
        )

        for n_tracer in range(self._ntr + 4):
            if (
                (n_tracer != self._ntiw)
                and (n_tracer != self._ntcw)
                and (n_tracer != self._ntcld)
                and (n_tracer != self._ntvap)
            ):
                self._comp_tendencies_tr(
                    self._cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._kb,
                    self._ktcon,
                    self._dellae,
                    self._del0,
                    self._eta,
                    self._ctro,
                    self._ecko,
                    n_tracer,
                )

        # if self._do_aerosols:
        #     samfshalcnv_aerosols()

        self._feedback_control_update_mass_flux(
            self._cnvflg,
            self._k_mask,
            self._kmax,
            self._kb,
            self._ktcon,
            self._flg,
            state.islimsk,
            state.ktop,
            state.kbot,
            self._kbcon,
            state.kcnv,
            self._qeso,
            self._pfld,
            self._delhbar,
            self._delqbar,
            self._deltbar,
            self._delubar,
            self._delvbar,
            self._qcond,
            self._dellah,
            self._dellaq,
            state.t1,
            self._xmb,
            state.qtr,
            state.u1,
            self._dellau,
            state.v1,
            self._dellav,
            self._del0,
            self._rntot,
            self._delqev,
            self._delq2,
            self._pwo,
            self._deltv,
            self._delq,
            self._qevap,
            state.rn,
            self._edt,
            state.cnvw,
            self._cnvwt,
            state.cnvc,
            state.ud_mf,
            state.dt_mf,
            self._eta,
        )

        for n_tracer in range(self._ntr + 4):
            if (
                (n_tracer != self._ntiw)
                and (n_tracer != self._ntcw)
                and (n_tracer != self._ntcld)
                and (n_tracer != self._ntvap)
            ):
                self._feedback_control_upd_trr(
                    self._cnvflg,
                    self._k_mask,
                    self._kmax,
                    self._ktcon,
                    self._del0,
                    self._delebar,
                    self._ctr,
                    self._dellae,
                    self._xmb,
                    state.qtr,
                    n_tracer,
                )

        if self._ncloud > 0:
            self._separate_detrained_cw(
                self._cnvflg,
                self._k_mask,
                self._kbcon,
                self._ktcon,
                self._dellal,
                self._xmb,
                state.t1,
                state.qtr,
            )

        # if self._do_aerosols:
        #     store_aero_conc()

        if self._ntk > 0:
            self._tke_contribution(
                self._cnvflg,
                self._k_mask,
                self._kb,
                state.ktop,
                self._eta,
                self._xmb,
                self._pfld,
                state.t1,
                self._sigmagfm,
                state.qtr,
            )
