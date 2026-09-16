import ndsl.constants as constants
import pyshield.stencils.pbl.constants as pbl_constants
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import BACKWARD, FORWARD, PARALLEL, computation, exp, interval, sqrt
from ndsl.dsl.stencil import StencilFactory
from ndsl.dsl.typing import (
    Bool,
    BoolField,
    BoolFieldIJ,
    Float,
    FloatField,
    FloatFieldIJ,
    Int,
    IntField,
    IntFieldIJ,
)
from ndsl.initialization.allocator import QuantityFactory
from pyshield._config import TRACER_DIM, FloatFieldTracer
from pyshield.functions.physics_functions import fpvs
from pyshield.stencils.pbl._config import PBLConfig
from pyshield.stencils.pbl.mfpblt import PBLMassFlux
from pyshield.stencils.pbl.mfscu import StratocumulusMassFlux
from pyshield.stencils.pbl.satmedmfvdiff_state import SATMEDMFVDiffState
from pyshield.stencils.pbl.tridiag import tridi2, tridin, tridit


def init_turbulence(
    zi: FloatField,
    zl: FloatField,
    zm: FloatField,
    phii: FloatField,
    phil: FloatField,
    chz: FloatField,
    ckz: FloatField,
    area: FloatFieldIJ,
    gdx: FloatFieldIJ,
    tke: FloatField,
    q1: FloatFieldTracer,
    rdzt: FloatField,
    prn: FloatField,
    kx1: IntFieldIJ,
    prsi: FloatField,
    k_mask: IntField,
    kinver: IntFieldIJ,
    tx1: FloatFieldIJ,
    tx2: FloatFieldIJ,
    xkzo: FloatField,
    xkzmo: FloatField,
    kpblx: IntFieldIJ,
    hpblx: FloatFieldIJ,
    pblflg: BoolFieldIJ,
    sfcflg: BoolFieldIJ,
    pcnvflg: BoolFieldIJ,
    scuflg: BoolFieldIJ,
    zorl: FloatFieldIJ,
    dusfc: FloatFieldIJ,
    dvsfc: FloatFieldIJ,
    dtsfc: FloatFieldIJ,
    dqsfc: FloatFieldIJ,
    kpbl: IntFieldIJ,
    hpbl: FloatFieldIJ,
    rbsoil: FloatFieldIJ,
    radmin: FloatFieldIJ,
    mrad: IntFieldIJ,
    krad: IntFieldIJ,
    lcld: IntFieldIJ,
    kcld: IntFieldIJ,
    theta: FloatField,
    prslk: FloatField,
    psk: FloatFieldIJ,
    t1: FloatField,
    pix: FloatField,
    qlx: FloatField,
    slx: FloatField,
    thvx: FloatField,
    qtx: FloatField,
    thlx: FloatField,
    thlvx: FloatField,
    svx: FloatField,
    thetae: FloatField,
    gotvx: FloatField,
    prsl: FloatField,
    plyr: FloatField,
    rhly: FloatField,
    qstl: FloatField,
    bf: FloatField,
    cfly: FloatField,
    crb: FloatFieldIJ,
    dtdz1: FloatFieldIJ,
    evap: FloatFieldIJ,
    heat: FloatFieldIJ,
    hlw: FloatField,
    radx: FloatField,
    sflux: FloatFieldIJ,
    shr2: FloatField,
    stress: FloatFieldIJ,
    hsw: FloatField,
    thermal: FloatFieldIJ,
    tsea: FloatFieldIJ,
    ustar: FloatFieldIJ,
    u1: FloatField,
    v1: FloatField,
    u10m: FloatFieldIJ,
    v10m: FloatFieldIJ,
    xmu: FloatFieldIJ,
    islimsk: IntFieldIJ,
    ptop: FloatFieldIJ,
    pbot: FloatFieldIJ,
    xkzm_hx: FloatFieldIJ,
    xkzm_mx: FloatFieldIJ,
    tvx: FloatField,
    tem3: FloatField,
):
    from __externals__ import (
        cap_k0_land,
        do_dk_hb19,
        dt2,
        km1,
        ntcw,
        ntiw,
        ntke,
        xkzm_hi,
        xkzm_hl,
        xkzm_ho,
        xkzm_lim,
        xkzm_mi,
        xkzm_ml,
        xkzm_mo,
        xkzm_s,
        xkzminv,
    )

    with computation(FORWARD), interval(0, 1):
        pcnvflg = False
        scuflg = True
        dusfc = 0.0
        dvsfc = 0.0
        dtsfc = 0.0
        dqsfc = 0.0
        kpbl = 0
        hpbl = 0.0
        kpblx = 0
        hpblx = 0.0
        pblflg = True
        lcld = km1 - 1
        kcld = km1 - 1
        mrad = km1 - 1
        krad = 0
        radmin = 0.0
        pbot = phii
        sfcflg = True
        if rbsoil[0, 0] > 0.0:
            sfcflg = False
        gdx = sqrt(area[0, 0])

    with computation(PARALLEL):
        with interval(0, -1):
            zi = phii[0, 0, 0] * constants.RGRAV
            zl = phil[0, 0, 0] * constants.RGRAV
            tke = max(q1[0, 0, 0][ntke], pbl_constants.TKMIN)
            ckz = pbl_constants.CK1
            chz = pbl_constants.CH1
        with interval(-1, None):
            zi = phii[0, 0, 0] * constants.RGRAV
    with computation(FORWARD):
        with interval(0, -2):
            prn = 1.0
            zm = zi[0, 0, 1]
            rdzt = 1.0 / (zl[0, 0, 1] - zl[0, 0, 0])
        with interval(-2, -1):
            zm = zi[0, 0, 1]
    with computation(FORWARD), interval(0, 1):
        #  set background diffusivities as a function of
        #  horizontal grid size with xkzm_h & xkzm_m for gdx >= 25km
        #  and 0.01 for gdx=5m
        kx1 = 0
        tx1 = 1.0 / prsi[0, 0, 0]
        tx2 = 1.0 / prsi[0, 0, 0]
        if do_dk_hb19:
            if gdx[0, 0] >= pbl_constants.XKGDX:
                if islimsk == 1:  # Land points
                    xkzm_hx = xkzm_hl
                    xkzm_mx = xkzm_ml
                elif islimsk == 2:  # Sea ice points
                    xkzm_hx = xkzm_hi
                    xkzm_mx = xkzm_mi
                else:  # Ocean points
                    xkzm_hx = xkzm_ho
                    xkzm_mx = xkzm_mo
            else:
                tem = 1.0 / (pbl_constants.XKGDX - 5.0)
                if islimsk == 1:  # Land points
                    tem1 = (xkzm_hl - xkzm_lim) * tem
                    tem2 = (xkzm_ml - xkzm_lim) * tem
                elif islimsk == 2:  # Sea ice points
                    tem1 = (xkzm_hi - xkzm_lim) * tem
                    tem2 = (xkzm_mi - xkzm_lim) * tem
                else:  # Ocean points
                    tem1 = (xkzm_hi - xkzm_lim) * tem
                    tem2 = (xkzm_mi - xkzm_lim) * tem
                ptem = gdx - 5.0
                xkzm_hx = xkzm_lim + tem1 * ptem
                xkzm_mx = xkzm_lim + tem2 * ptem
        else:  # use values in the namelist; no res dependency
            if islimsk == 1:  # Land points
                xkzm_hx = xkzm_hl
                xkzm_mx = xkzm_ml
            elif islimsk == 2:  # Sea ice points
                xkzm_hx = xkzm_hi
                xkzm_mx = xkzm_mi
            else:  # Ocean points
                xkzm_hx = xkzm_ho
                xkzm_mx = xkzm_mo

    with computation(FORWARD), interval(0, -2):
        xkzo[0, 0, 0] = 0.0
        xkzmo[0, 0, 0] = 0.0
        if k_mask[0, 0, 0] < kinver[0, 0]:
            # vertical background diffusivity
            ptem = prsi[0, 0, 1] * tx1[0, 0]
            tem1 = (1.0 - ptem) * (1.0 - ptem) * 10.0
            xkzo[0, 0, 0] = xkzm_hx * min(1.0, exp(-tem1))
            # vertical background diffusivity for momentum
            if ptem >= xkzm_s:
                xkzmo[0, 0, 0] = xkzm_mx
                kx1 = k_mask[0, 0, 0] + 1
            else:
                if (k_mask[0, 0, 0] == kx1) and (k_mask[0, 0, 0] > 1):
                    tx2[0, 0] = 1.0 / prsi[0, 0, 0]
                tem1 = 1.0 - prsi[0, 0, 1] * tx2[0, 0]
                tem1 = tem1 * tem1 * 5.0
                xkzmo = xkzm_mx * min(1.0, exp(-tem1))
    with computation(FORWARD), interval(0, -1):
        pix = psk[0, 0] / prslk[0, 0, 0]
        theta = t1[0, 0, 0] * pix[0, 0, 0]
        if (ntiw + 1) > 0:
            tem = max(q1[0, 0, 0][ntcw], pbl_constants.QLMIN)
            tem1 = max(q1[0, 0, 0][ntiw], pbl_constants.QLMIN)
            ptem = constants.HLV * tem + (constants.HLV + constants.HLF) * tem1
            qlx = tem + tem1
            slx = constants.CP_AIR * t1[0, 0, 0] + phil[0, 0, 0] - ptem
        else:
            qlx = max(q1[0, 0, 0][ntcw], pbl_constants.QLMIN)
            slx = (
                constants.CP_AIR * t1[0, 0, 0]
                + phil[0, 0, 0]
                - constants.HLV * qlx[0, 0, 0]
            )

        tem2 = (
            1.0
            + constants.ZVIR * max(q1[0, 0, 0][0], pbl_constants.PBL_QMIN)
            - qlx[0, 0, 0]
        )
        thvx = theta[0, 0, 0] * tem2
        tvx = t1 * tem2
        qtx = max(q1[0, 0, 0][0], pbl_constants.PBL_QMIN) + qlx[0, 0, 0]
        thlx = theta[0, 0, 0] - pix[0, 0, 0] * pbl_constants.ELOCP * qlx[0, 0, 0]
        thlvx = thlx[0, 0, 0] * (1.0 + constants.ZVIR * qtx[0, 0, 0])
        svx = constants.CP_AIR * tvx
        thetae = theta[0, 0, 0] + pbl_constants.ELOCP * pix[0, 0, 0] * max(
            q1[0, 0, 0][0], pbl_constants.PBL_QMIN
        )
        gotvx = constants.GRAV / (tvx)

    with computation(FORWARD), interval(0, -2):
        # The background vertical diffusivities in the inversion layers are limited
        # to be less than or equal to xkzminv
        tem3 = (tvx[0, 0, 1] - tvx[0, 0, 0]) * rdzt[0, 0, 0]
        if cap_k0_land:
            if tem3 > 1.0e-5:
                xkzo[0, 0, 0] = min(xkzo[0, 0, 0], xkzminv)
                xkzmo[0, 0, 0] = min(xkzmo[0, 0, 0], xkzminv)
        else:
            # kgao note: do not apply upper-limiter over land and sea ice points
            # (consistent with change in satmedmfdifq.f in Jun 2020)
            if (tem3 > 0.0) and (islimsk == 0):
                xkzo[0, 0, 0] = min(xkzo[0, 0, 0], xkzminv)
                xkzmo[0, 0, 0] = min(xkzmo[0, 0, 0], xkzminv)

    with computation(FORWARD), interval(0, -1):
        #  Compute empirical cloud fraction based on Xu & Randall (1996, JAS)
        plyr = 0.01 * prsl[0, 0, 0]
        es = 0.01 * fpvs(t1)
        qs = max(
            pbl_constants.PBL_QMIN,
            constants.EPS * es / (plyr[0, 0, 0] + (constants.EPS - 1) * es),
        )
        rhly = max(0.0, min(1.0, max(pbl_constants.PBL_QMIN, q1[0, 0, 0][0]) / qs))
        qstl = qs

    with computation(FORWARD), interval(0, -1):
        cfly = 0.0
        clwt = 1.0e-6 * (plyr[0, 0, 0] * 0.001)
        if qlx[0, 0, 0] > clwt:
            onemrh = max(1.0e-10, 1.0 - rhly[0, 0, 0])
            tem1 = pbl_constants.CQL / min(
                max((onemrh * qstl[0, 0, 0]) ** 0.49, 0.0001), 1.0
            )
            val = max(min(tem1 * qlx[0, 0, 0], 50.0), 0.0)
            cfly = min(max(sqrt(sqrt(rhly[0, 0, 0])) * (1.0 - exp(-val)), 0.0), 1.0)

    #  Compute buoyancy modified by clouds
    with computation(PARALLEL), interval(0, -2):
        tem1 = 0.5 * (t1[0, 0, 0] + t1[0, 0, 1])
        cfh = min(cfly[0, 0, 1], 0.5 * (cfly[0, 0, 0] + cfly[0, 0, 1]))
        alp = constants.GRAV / (0.5 * (svx[0, 0, 0] + svx[0, 0, 1]))
        gamma = (
            pbl_constants.EL2ORC * (0.5 * (qstl[0, 0, 0] + qstl[0, 0, 1])) / (tem1**2)
        )
        epsi = tem1 / pbl_constants.ELOCP
        beta = (1.0 + gamma * epsi * (1.0 + constants.ZVIR)) / (1.0 + gamma)
        chx = cfh * alp * beta + (1.0 - cfh) * alp
        cqx = cfh * alp * constants.HLV * (beta - epsi)
        cqx = cqx + (1.0 - cfh) * constants.ZVIR * constants.GRAV
        bf = chx * ((slx[0, 0, 1] - slx[0, 0, 0]) * rdzt[0, 0, 0]) + cqx * (
            (qtx[0, 0, 1] - qtx[0, 0, 0]) * rdzt[0, 0, 0]
        )
        radx = (zi[0, 0, 1] - zi[0, 0, 0]) * (hsw[0, 0, 0] * xmu[0, 0] + hlw[0, 0, 0])

    with computation(FORWARD):
        #  Compute critical bulk richardson number
        with interval(0, 1):
            sflux = heat[0, 0] + evap[0, 0] * constants.ZVIR * theta[0, 0, 0]

            if (not sfcflg[0, 0]) or (sflux[0, 0] <= 0.0):
                pblflg = False

            if pblflg[0, 0]:
                thermal = thlvx[0, 0, 0]
                crb = pbl_constants.RBCR
            else:
                tem1 = 1e-7 * (
                    max(sqrt(u10m[0, 0] ** 2 + v10m[0, 0] ** 2), 1.0)
                    / (pbl_constants.F0 * 0.01 * zorl[0, 0])
                )
                thermal = tsea[0, 0] * (
                    1.0 + constants.ZVIR * max(q1[0, 0, 0][0], pbl_constants.PBL_QMIN)
                )
                crb = max(
                    min(0.16 * (tem1 ** (-0.18)), pbl_constants.CRBMAX),
                    pbl_constants.CRBMIN,
                )

            dtdz1 = dt2 / (zi[0, 0, 1] - zi[0, 0, 0])
            ustar = sqrt(stress[0, 0])
    #  Compute buoyancy (bf) and winshear square
    with computation(FORWARD):
        with interval(0, -2):
            dw2 = (u1[0, 0, 0] - u1[0, 0, 1]) ** 2 + (v1[0, 0, 0] - v1[0, 0, 1]) ** 2
            shr2 = max(dw2, pbl_constants.DW2MIN) * rdzt[0, 0, 0] * rdzt[0, 0, 0]
        with interval(-2, -1):
            ptop = phii


def mrf_pbl_scheme_part1(
    crb: FloatFieldIJ,
    flg: BoolFieldIJ,
    kpblx: IntFieldIJ,
    k_mask: IntField,
    rbdn: FloatFieldIJ,
    rbup: FloatFieldIJ,
    rbsoil: FloatFieldIJ,
    thermal: FloatFieldIJ,
    thlvx: FloatField,
    thlvx_0: FloatFieldIJ,
    u1: FloatField,
    v1: FloatField,
    zl: FloatField,
):

    with computation(FORWARD):
        with interval(0, 1):
            flg = False
            rbup = rbsoil[0, 0]
            thlvx_0 = thlvx[0, 0, 0]

    with computation(FORWARD):
        with interval(...):
            if not flg[0, 0]:
                rbdn = rbup[0, 0]
                rbup = (
                    (thlvx[0, 0, 0] - thermal[0, 0])
                    * (constants.GRAV * zl[0, 0, 0] / thlvx_0[0, 0])
                    / max(u1[0, 0, 0] ** 2 + v1[0, 0, 0] ** 2, 1.0)
                )
                kpblx = k_mask[0, 0, 0]
                flg = rbup[0, 0] > crb[0, 0]


def mrf_pbl_2_thermal_excess(
    crb: FloatFieldIJ,
    evap: FloatFieldIJ,
    fh: FloatFieldIJ,
    flg: BoolFieldIJ,
    fm: FloatFieldIJ,
    gotvx: FloatField,
    heat: FloatFieldIJ,
    hpbl: FloatFieldIJ,
    hpblx: FloatFieldIJ,
    kpbl: IntFieldIJ,
    kpblx: IntFieldIJ,
    k_mask: IntField,
    pblflg: BoolFieldIJ,
    pcnvflg: BoolFieldIJ,
    phih: FloatFieldIJ,
    phim: FloatFieldIJ,
    rbdn: FloatFieldIJ,
    rbup: FloatFieldIJ,
    rbsoil: FloatFieldIJ,
    sfcflg: BoolFieldIJ,
    sflux: FloatFieldIJ,
    thermal: FloatFieldIJ,
    theta: FloatField,
    ustar: FloatFieldIJ,
    vpert: FloatFieldIJ,
    zi: FloatField,
    zl: FloatField,
    zol: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        if kpblx <= 0:
            hpblx = zl[0, 0, 0]
            kpblx = 0
    with computation(FORWARD), interval(1, None):
        if k_mask[0, 0, 0] == kpblx[0, 0]:
            if kpblx[0, 0] > 0:
                if rbdn[0, 0] >= crb[0, 0]:
                    rbint = 0.0
                elif rbup[0, 0] <= crb[0, 0]:
                    rbint = 1.0
                else:
                    rbint = (crb[0, 0] - rbdn[0, 0]) / (rbup[0, 0] - rbdn[0, 0])
                hpblx = zl[0, 0, -1] + rbint * (zl[0, 0, 0] - zl[0, 0, -1])

                if hpblx[0, 0] < zi[0, 0, 0]:
                    kpblx = kpblx[0, 0] - 1

    with computation(FORWARD), interval(0, 1):
        hpbl = hpblx[0, 0]
        kpbl = kpblx[0, 0]

        if kpbl[0, 0] <= 0:
            pblflg = False

        # Compute similarity parameters
        zol = max(rbsoil[0, 0] * fm[0, 0] * fm[0, 0] / fh[0, 0], pbl_constants.RIMIN)
        if sfcflg[0, 0]:
            zol = min(zol[0, 0], -pbl_constants.ZFMIN)
        else:
            zol = max(zol[0, 0], pbl_constants.ZFMIN)

        zol1 = zol[0, 0] * pbl_constants.SFCFRAC * hpbl[0, 0] / zl[0, 0, 0]

        if sfcflg[0, 0]:
            phih = sqrt(1.0 / (1.0 - pbl_constants.APHI16 * zol1))
            phim = sqrt(phih[0, 0])
        else:
            phim = 1.0 + pbl_constants.APHI5 * zol1
            phih = phim[0, 0]

        pcnvflg = pblflg[0, 0] and (zol[0, 0] < pbl_constants.ZOLCRU)

        wst3 = gotvx[0, 0, 0] * sflux[0, 0] * hpbl[0, 0]
        ust3 = ustar[0, 0] ** 3.0

        if pblflg[0, 0]:
            wscale = max(
                (
                    ust3
                    + pbl_constants.WFAC
                    * pbl_constants.VK
                    * wst3
                    * pbl_constants.SFCFRAC
                )
                ** pbl_constants.H1,
                ustar[0, 0] / pbl_constants.APHI5,
            )

        flg = 1

        # Compute a thermal excess
        if pcnvflg[0, 0]:
            hgamt = heat[0, 0] / wscale
            hgamq = evap[0, 0] / wscale
            vpert = max(hgamt + hgamq * constants.ZVIR * theta[0, 0, 0], 0.0)
            thermal = thermal[0, 0] + min(
                pbl_constants.CFAC * vpert[0, 0], pbl_constants.GAMCRT
            )
            flg = 0
            rbup = rbsoil[0, 0]


def thermal_pbl_calc(
    crb: FloatFieldIJ,
    flg: BoolFieldIJ,
    kpbl: IntFieldIJ,
    k_mask: IntField,
    rbdn: FloatFieldIJ,
    rbup: FloatFieldIJ,
    thermal: FloatFieldIJ,
    thlvx: FloatField,
    thlvx_0: FloatFieldIJ,
    u1: FloatField,
    v1: FloatField,
    zl: FloatField,
):
    # enhance the pbl height by considering the thermal excess
    # (overshoot pbl top)
    with computation(FORWARD):
        with interval(1, None):
            if not flg[0, 0]:
                rbdn = rbup[0, 0]
                rbup = (
                    (thlvx[0, 0, 0] - thermal[0, 0])
                    * (constants.GRAV * zl[0, 0, 0] / thlvx_0[0, 0])
                    / max(u1[0, 0, 0] ** 2 + v1[0, 0, 0] ** 2, 1.0)
                )
                kpbl = k_mask[0, 0, 0]
                flg = rbup[0, 0] > crb[0, 0]


def enhance_pbl_height_thermal(
    crb: FloatFieldIJ,
    hpbl: FloatFieldIJ,
    kpbl: IntFieldIJ,
    k_mask: IntField,
    pblflg: BoolFieldIJ,
    pcnvflg: BoolFieldIJ,
    rbdn: FloatFieldIJ,
    rbup: FloatFieldIJ,
    zi: FloatField,
    zl: FloatField,
):

    with computation(FORWARD), interval(1, None):
        if pcnvflg[0, 0] and (kpbl[0, 0] == k_mask[0, 0, 0]):
            if rbdn[0, 0] >= crb[0, 0]:
                rbint = 0.0
            elif rbup[0, 0] <= crb[0, 0]:
                rbint = 1.0
            else:
                rbint = (crb[0, 0] - rbdn[0, 0]) / (rbup[0, 0] - rbdn[0, 0])

            hpbl[0, 0] = zl[0, 0, -1] + rbint * (zl[0, 0, 0] - zl[0, 0, -1])

            if hpbl[0, 0] < zi[0, 0, 0]:
                kpbl[0, 0] = kpbl[0, 0] - 1

            if kpbl[0, 0] <= 0:
                pblflg[0, 0] = False
                pcnvflg[0, 0] = False


def stratocumulus(
    flg: BoolFieldIJ,
    kcld: IntFieldIJ,
    krad: IntFieldIJ,
    lcld: IntFieldIJ,
    k_mask: IntField,
    radmin: FloatFieldIJ,
    radx: FloatField,
    qlx: FloatField,
    scuflg: BoolFieldIJ,
    zl: FloatField,
):
    from __externals__ import km1

    # look for stratocumulus
    with computation(FORWARD):
        with interval(0, 1):
            flg = scuflg[0, 0]
            if flg[0, 0] and (zl[0, 0, 0] >= pbl_constants.ZSTBLMAX):
                lcld = k_mask[0, 0, 0]
                flg = 0
        with interval(1, -1):
            if flg[0, 0] and (zl[0, 0, 0] >= pbl_constants.ZSTBLMAX):
                lcld = k_mask[0, 0, 0]
                flg = 0

    with computation(FORWARD):
        with interval(0, 1):
            flg = scuflg[0, 0]

    with computation(BACKWARD):
        with interval(-1, None):
            if (
                flg[0, 0]
                and (k_mask[0, 0, 0] <= lcld[0, 0])
                and (qlx[0, 0, 0] >= pbl_constants.QLCR)
            ):
                kcld = k_mask[0, 0, 0]
                flg = 0

        with interval(0, -1):
            if (
                flg[0, 0]
                and (k_mask[0, 0, 0] <= lcld[0, 0])
                and (qlx[0, 0, 0] >= pbl_constants.QLCR)
            ):
                kcld = k_mask[0, 0, 0]
                flg = 0

    with computation(FORWARD):
        with interval(0, 1):
            if scuflg[0, 0] and (kcld[0, 0] == (km1 - 1)):
                scuflg = False
            flg = scuflg[0, 0]

    with computation(BACKWARD):
        with interval(-1, None):
            if flg[0, 0] and (k_mask[0, 0, 0] <= kcld[0, 0]):
                if qlx[0, 0, 0] >= pbl_constants.QLCR:
                    if radx[0, 0, 0] < radmin[0, 0]:
                        radmin = radx[0, 0, 0]
                        krad = k_mask[0, 0, 0]
                else:
                    flg = 0

        with interval(0, -1):
            if flg[0, 0] and (k_mask[0, 0, 0] <= kcld[0, 0]):
                if qlx[0, 0, 0] >= pbl_constants.QLCR:
                    if radx[0, 0, 0] < radmin[0, 0]:
                        radmin = radx[0, 0, 0]
                        krad = k_mask[0, 0, 0]
                else:
                    flg = 0

    with computation(FORWARD), interval(0, 1):
        if scuflg[0, 0] and krad[0, 0] <= 0:
            scuflg = False
        if scuflg[0, 0] and radmin[0, 0] >= 0.0:
            scuflg = False


def compute_mass_flux_prelim(
    pcnvflg: BoolFieldIJ,
    scuflg: BoolFieldIJ,
    t1: FloatField,
    tcdo: FloatField,
    tcko: FloatField,
    u1: FloatField,
    ucdo: FloatField,
    ucko: FloatField,
    v1: FloatField,
    vcdo: FloatField,
    vcko: FloatField,
):
    with computation(PARALLEL), interval(...):
        if pcnvflg[0, 0]:
            tcko = t1[0, 0, 0]
            ucko = u1[0, 0, 0]
            vcko = v1[0, 0, 0]
        if scuflg[0, 0]:
            tcdo = t1[0, 0, 0]
            ucdo = u1[0, 0, 0]
            vcdo = v1[0, 0, 0]


def compute_mass_flux_tracer_prelim(
    qcko: FloatFieldTracer,
    qcdo: FloatFieldTracer,
    q1: FloatFieldTracer,
    pcnvflg: BoolFieldIJ,
    scuflg: BoolFieldIJ,
    n_extra: int,
):
    with computation(PARALLEL), interval(...):
        if pcnvflg[0, 0]:
            qcko[0, 0, 0][n_extra] = q1[0, 0, 0][n_extra]
        if scuflg[0, 0]:
            qcdo[0, 0, 0][n_extra] = q1[0, 0, 0][n_extra]


def compute_prandtl_num_exchange_coeff(
    chz: FloatField,
    ckz: FloatField,
    hpbl: FloatFieldIJ,
    kpbl: IntFieldIJ,
    k_mask: IntField,
    pcnvflg: BoolFieldIJ,
    phih: FloatFieldIJ,
    phim: FloatFieldIJ,
    prn: FloatField,
    zi: FloatField,
):

    with computation(PARALLEL), interval(...):
        ptem = 0.0
        if k_mask[0, 0, 0] < kpbl[0, 0]:
            tem = phih[0, 0] / phim[0, 0]
            ptem = (
                -3.0
                * (max(zi[0, 0, 1] - pbl_constants.SFCFRAC * hpbl[0, 0], 0.0) ** 2.0)
                / hpbl[0, 0] ** 2.0
            )
            if pcnvflg[0, 0]:
                prn = 1.0 + (tem - 1.0) * exp(ptem)
            else:
                prn = tem

            prn = min(prn, pbl_constants.PRMAX)
            prn = max(prn, pbl_constants.PRMIN)
            ckz = min(
                pbl_constants.CK1 + (pbl_constants.CK0 - pbl_constants.CK1) * exp(ptem),
                pbl_constants.CK0,
            )
            ckz = max(ckz, pbl_constants.CK1)
            chz = min(
                pbl_constants.CH1 + (pbl_constants.CH0 - pbl_constants.CH1) * exp(ptem),
                pbl_constants.CH0,
            )
            chz = max(
                chz,
                pbl_constants.CH1,
            )


def compute_asymptotic_mixing_length(
    zldn: FloatField,
    zlup: FloatField,
    thvx: FloatField,
    tke: FloatField,
    gotvx: FloatField,
    zl: FloatField,
    tsea: FloatFieldIJ,
    q1: FloatFieldTracer,
    zi: FloatField,
    rlam: FloatField,
    ele: FloatField,
    elm: FloatField,
    ptem2: FloatField,
    zol: FloatFieldIJ,
    gdx: FloatFieldIJ,
    lev: IntFieldIJ,
    k_mask: IntField,
    mlenflg: BoolField,
):
    from __externals__ import km1

    with computation(FORWARD), interval(...):
        q1_0 = q1[0, 0, 0][0]
    with computation(FORWARD), interval(0, -1):
        mlenflg = True
        zlup = 0.0
        bsum = 0.0
        lev = 0
        while k_mask[0, 0, 0] + lev <= km1:
            if mlenflg:
                dz = zl[0, 0, lev + 1] - zl[0, 0, lev]
                ptem = gotvx[0, 0, lev] * (thvx[0, 0, lev + 1] - thvx) * dz
                bsum = bsum + ptem
                zlup = zlup + dz
                if bsum >= tke:
                    if ptem >= 0.0:
                        tem2 = max(ptem, pbl_constants.ZFMIN)
                    else:
                        tem2 = min(ptem, -pbl_constants.ZFMIN)
                    ptem1 = (bsum - tke) / tem2
                    zlup = zlup - ptem1 * dz
                    zlup = max(zlup, 0.0)
                    mlenflg = False
            lev += 1

        mlenflg = True
        bsum = 0.0
        zldn = 0.0
        lev = 0
        while k_mask[0, 0, 0] + lev >= 0:
            if mlenflg:
                if k_mask[0, 0, 0] + lev == 0:
                    dz = zl[0, 0, lev]
                    tem1 = tsea * (
                        1.0
                        + constants.ZVIR * max(q1_0[0, 0, lev], pbl_constants.PBL_QMIN)
                    )
                else:
                    dz = zl[0, 0, lev] - zl[0, 0, lev - 1]
                    tem1 = thvx[0, 0, lev - 1]
                ptem = gotvx[0, 0, lev] * (thvx - tem1) * dz
                bsum = bsum + ptem
                zldn = zldn + dz
                if bsum >= tke:
                    if ptem >= 0.0:
                        tem2 = max(ptem, pbl_constants.ZFMIN)
                    else:
                        tem2 = min(ptem, -pbl_constants.ZFMIN)
                    ptem1 = (bsum - tke) / tem2
                    zldn = zldn - ptem1 * dz
                    zldn = max(zldn, 0.0)
                    mlenflg = False
            lev -= 1

        tem = 0.5 * (zi[0, 0, 1] - zi)
        tem1 = min(tem, pbl_constants.RLMN)

        ptem2 = min(zlup, zldn)
        rlam = pbl_constants.ELMFAC * ptem2
        rlam = max(rlam, tem1)
        rlam = min(rlam, pbl_constants.RLMX)

        ptem2 = sqrt(zlup * zldn)
        ele = pbl_constants.ELEFAC * ptem2
        ele = max(ele, tem1)
        ele = min(ele, pbl_constants.ELMX)

    with computation(FORWARD):
        with interval(0, -1):
            if zol < 0.0:
                zk = pbl_constants.VK * zl * (1.0 - 100.0 * zol) ** 0.2
            elif zol >= 1.0:
                zk = pbl_constants.VK * zl / 3.7
            else:
                zk = pbl_constants.VK * zl / (1.0 + 2.7 * zol)

            elm = zk * rlam / (rlam + zk)
            dz = zi[0, 0, 1] - zi
            tem = max(gdx, dz)
            elm = min(elm, tem)
            ele = min(ele, tem)

        with interval(-1, None):
            elm = elm[0, 0, -1]
            ele = ele[0, 0, -1]


def compute_eddy_diffusivity_buoy_shear(
    bf: FloatField,
    buod: FloatField,
    buou: FloatField,
    chz: FloatField,
    ckz: FloatField,
    dku: FloatField,
    dkt: FloatField,
    dkq: FloatField,
    elm: FloatField,
    gotvx: FloatField,
    kpbl: IntFieldIJ,
    k_mask: IntField,
    mrad: IntFieldIJ,
    krad: IntFieldIJ,
    pblflg: BoolFieldIJ,
    pcnvflg: BoolFieldIJ,
    phim: FloatFieldIJ,
    prn: FloatField,
    prod: FloatField,
    radj: FloatFieldIJ,
    rdzt: FloatField,
    scuflg: BoolFieldIJ,
    sflux: FloatFieldIJ,
    shr2: FloatField,
    stress: FloatFieldIJ,
    tke: FloatField,
    u1: FloatField,
    ucdo: FloatField,
    ucko: FloatField,
    ustar: FloatFieldIJ,
    v1: FloatField,
    vcdo: FloatField,
    vcko: FloatField,
    xkzo: FloatField,
    xkzmo: FloatField,
    xmf: FloatField,
    xmfd: FloatField,
    zl: FloatField,
    dkt_out: FloatField,
):
    with computation(PARALLEL), interval(0, -1):
        tem = 0.5 * (elm[0, 0, 0] + elm[0, 0, 1])
        tem = tem * sqrt(0.5 * (tke[0, 0, 0] + tke[0, 0, 1]))
        ri = max(bf[0, 0, 0] / shr2[0, 0, 0], pbl_constants.RIMIN)

        if k_mask[0, 0, 0] < kpbl[0, 0]:
            if pblflg[0, 0]:
                dku = ckz[0, 0, 0] * tem
                dkt = dku[0, 0, 0] / prn[0, 0, 0]
            else:
                dkt = chz[0, 0, 0] * tem
                dku = dkt[0, 0, 0] * prn[0, 0, 0]
        else:
            if ri < 0.0:  # Unstable regime
                dku = pbl_constants.CK1 * tem
                dkt = pbl_constants.RCHCK * dku[0, 0, 0]
            else:  # Stable regime
                dkt = pbl_constants.CH1 * tem
                dku = dkt[0, 0, 0] * min(1.0 + 2.1 * ri, pbl_constants.PRMAX)

        tem = ckz[0, 0, 0] * tem
        dku_tmp = max(dku[0, 0, 0], tem)
        dkt_tmp = max(dkt[0, 0, 0], tem / pbl_constants.PRSCU)

        if scuflg[0, 0]:
            if k_mask[0, 0, 0] >= mrad[0, 0] and k_mask[0, 0, 0] < krad[0, 0]:
                dku = dku_tmp
                dkt = dkt_tmp

        dkq = pbl_constants.PRTKE * dkt[0, 0, 0]

        dkt = max(min(dkt[0, 0, 0], pbl_constants.DKMAX), xkzo[0, 0, 0])

        dkq = max(min(dkq[0, 0, 0], pbl_constants.DKMAX), xkzo[0, 0, 0])

        dku = max(min(dku[0, 0, 0], pbl_constants.DKMAX), xkzmo[0, 0, 0])

    with computation(PARALLEL), interval(...):
        if k_mask[0, 0, 0] == krad[0, 0]:
            if scuflg[0, 0]:
                tem1 = max(bf[0, 0, 0] / gotvx[0, 0, 0], pbl_constants.TDZMIN)
                ptem = radj[0, 0] / tem1
                dkt = dkt[0, 0, 0] + ptem
                dku = dku[0, 0, 0] + ptem
                dkq = dkq[0, 0, 0] + ptem
        dkt_out = dkt
    with computation(PARALLEL):
        # Compute buoyancy and shear productions of tke
        with interval(0, 1):
            tem = -dkt[0, 0, 0] * bf[0, 0, 0]
            if scuflg[0, 0] and mrad[0, 0] == 0:
                ptem = xmfd[0, 0, 0] * buod[0, 0, 0]
                ptem1 = ucdo[0, 0, 0] + ucdo[0, 0, 1] - u1[0, 0, 0] - u1[0, 0, 1]
                ptem1 = (
                    0.5
                    * ((u1[0, 0, 1] - u1[0, 0, 0]) * rdzt[0, 0, 0])
                    * xmfd[0, 0, 0]
                    * ptem1
                )
                ptem2 = vcdo[0, 0, 0] + vcdo[0, 0, 1] - v1[0, 0, 0] - v1[0, 0, 1]
                ptem2 = (
                    0.5
                    * ((v1[0, 0, 1] - v1[0, 0, 0]) * rdzt[0, 0, 0])
                    * xmfd[0, 0, 0]
                    * ptem2
                )
            else:
                ptem = 0.0
                ptem1 = 0.0
                ptem2 = 0.0

            buop = 0.5 * (gotvx[0, 0, 0] * sflux[0, 0] + (tem + ptem))

            tem2 = stress * ustar * phim / (pbl_constants.VK * zl)
            shrp = 0.5 * (dku[0, 0, 0] * shr2[0, 0, 0] + ptem1 + ptem2 + tem2)

            prod = buop + shrp

        with interval(1, -1):
            tem1_1 = (u1[0, 0, 1] - u1[0, 0, 0]) * rdzt[0, 0, 0]
            tem2_1 = (u1[0, 0, 0] - u1[0, 0, -1]) * rdzt[0, 0, -1]
            tem1_2 = (v1[0, 0, 1] - v1[0, 0, 0]) * rdzt[0, 0, 0]
            tem2_2 = (v1[0, 0, 0] - v1[0, 0, -1]) * rdzt[0, 0, -1]

            if pcnvflg[0, 0] and k_mask[0, 0, 0] <= kpbl[0, 0]:
                ptem1_0 = 0.5 * (xmf[0, 0, -1] + xmf[0, 0, 0]) * buou[0, 0, 0]
                ptem1_1 = (
                    0.5
                    * (xmf[0, 0, 0] * tem1_1 + xmf[0, 0, -1] * tem2_1)
                    * (u1[0, 0, 0] - ucko[0, 0, 0])
                )
                ptem1_2 = (
                    0.5
                    * (xmf[0, 0, 0] * tem1_2 + xmf[0, 0, -1] * tem2_2)
                    * (v1[0, 0, 0] - vcko[0, 0, 0])
                )
            else:
                ptem1_0 = 0.0
                ptem1_1 = 0.0
                ptem1_2 = 0.0

            if scuflg[0, 0]:
                if k_mask[0, 0, 0] >= mrad[0, 0] and k_mask[0, 0, 0] < krad[0, 0]:
                    ptem2_0 = 0.5 * (xmfd[0, 0, -1] + xmfd[0, 0, 0]) * buod[0, 0, 0]
                    ptem2_1 = (
                        0.5
                        * (xmfd[0, 0, 0] * tem1_1 + xmfd[0, 0, -1] * tem2_1)
                        * (ucdo[0, 0, 0] - u1[0, 0, 0])
                    )
                    ptem2_2 = (
                        0.5
                        * (xmfd[0, 0, 0] * tem1_2 + xmfd[0, 0, -1] * tem2_2)
                        * (vcdo[0, 0, 0] - v1[0, 0, 0])
                    )
                else:
                    ptem2_0 = 0.0
                    ptem2_1 = 0.0
                    ptem2_2 = 0.0
            else:
                ptem2_0 = 0.0
                ptem2_1 = 0.0
                ptem2_2 = 0.0

            buop = (
                0.5 * ((-dkt[0, 0, -1] * bf[0, 0, -1]) + (-dkt[0, 0, 0] * bf[0, 0, 0]))
                + ptem1_0
                + ptem2_0
            )

            shrp = (
                (
                    0.5
                    * (
                        (dku[0, 0, -1] * shr2[0, 0, -1])
                        + (dku[0, 0, 0] * shr2[0, 0, 0])
                    )
                    + ptem1_1
                    + ptem2_1
                )
                + ptem1_2
                + ptem2_2
            )

            prod = buop + shrp


def predict_tke(
    diss: FloatField,
    prod: FloatField,
    rle: FloatField,
    tke: FloatField,
    ele: FloatField,
):
    from __externals__ import dtn, kk

    with computation(PARALLEL), interval(...):
        rle = pbl_constants.CE0 / ele[0, 0, 0]

    with computation(PARALLEL), interval(...):
        n = 0
        while n < kk:
            diss = max(
                min(
                    rle[0, 0, 0] * tke[0, 0, 0] * sqrt(tke[0, 0, 0]),
                    prod[0, 0, 0] + tke[0, 0, 0] / dtn,
                ),
                0.0,
            )
            tke = max(
                tke[0, 0, 0] + dtn * (prod[0, 0, 0] - diss[0, 0, 0]),
                pbl_constants.TKMIN,
            )
            n = n + 1


def tke_up_down_prop(
    pcnvflg: BoolFieldIJ,
    qcdo: FloatFieldTracer,
    qcko: FloatFieldTracer,
    scuflg: BoolFieldIJ,
    tke: FloatField,
    kpbl: IntFieldIJ,
    k_mask: IntField,
    xlamue: FloatField,
    zl: FloatField,
    krad: IntFieldIJ,
    mrad: IntFieldIJ,
    xlamde: FloatField,
):
    from __externals__ import ntke

    with computation(PARALLEL), interval(...):
        if pcnvflg[0, 0]:
            qcko[0, 0, 0][ntke] = tke[0, 0, 0]
        if scuflg[0, 0]:
            qcdo[0, 0, 0][ntke] = tke[0, 0, 0]

    with computation(FORWARD), interval(1, None):
        if pcnvflg[0, 0] and k_mask[0, 0, 0] <= kpbl[0, 0]:
            tem = 0.5 * xlamue[0, 0, -1] * (zl[0, 0, 0] - zl[0, 0, -1])
            qcko[0, 0, 0][ntke] = (
                (1.0 - tem) * qcko[0, 0, -1][ntke]
                + tem * (tke[0, 0, 0] + tke[0, 0, -1])
            ) / (1.0 + tem)

    with computation(BACKWARD), interval(...):
        if k_mask[0, 0, 0] < krad:
            tem = 0.5 * xlamde[0, 0, 0] * (zl[0, 0, 1] - zl[0, 0, 0])
            if scuflg[0, 0] and k_mask[0, 0, 0] < krad[0, 0]:
                if k_mask[0, 0, 0] >= mrad[0, 0]:
                    qcdo[0, 0, 0][ntke] = (
                        (1.0 - tem) * qcdo[0, 0, 1][ntke]
                        + tem * (tke[0, 0, 0] + tke[0, 0, 1])
                    ) / (1.0 + tem)


def tke_tridiag_matrix_ele_comp(
    ad: FloatField,
    ad_p1: FloatFieldIJ,
    al: FloatField,
    au: FloatField,
    delta: FloatField,
    dkq: FloatField,
    f1: FloatField,
    f1_p1: FloatFieldIJ,
    kpbl: IntFieldIJ,
    krad: IntFieldIJ,
    k_mask: IntField,
    mrad: IntFieldIJ,
    pcnvflg: BoolFieldIJ,
    prsl: FloatField,
    qcdo: FloatFieldTracer,
    qcko: FloatFieldTracer,
    rdzt: FloatField,
    scuflg: BoolFieldIJ,
    tke: FloatField,
    xmf: FloatField,
    xmfd: FloatField,
    cu: FloatField,
    rt: FloatField,
):
    from __externals__ import dt2, ntke

    with computation(FORWARD), interval(0, 1):
        ad = 1.0
        f1 = tke[0, 0, 0]
        ad_p1 = 0.0
        f1_p1 = 0.0

    with computation(FORWARD):
        with interval(0, -1):
            if k_mask > 0:
                ad = ad_p1[0, 0]
                f1 = f1_p1[0, 0]

            dtodsd = dt2 / delta[0, 0, 0]
            dtodsu = dt2 / delta[0, 0, 1]
            dsig = prsl[0, 0, 0] - prsl[0, 0, 1]
            rdz = rdzt[0, 0, 0]
            dsdz2 = dsig * dkq[0, 0, 0] * rdz * rdz
            au = -dtodsd * dsdz2
            al = -dtodsu * dsdz2
            ad = ad[0, 0, 0] - au[0, 0, 0]
            ad_p1 = 1.0 - al[0, 0, 0]
            tem2 = dsig * rdz

            if pcnvflg[0, 0] and k_mask[0, 0, 0] < kpbl[0, 0]:
                ptem = 0.5 * tem2 * xmf
                ptem2 = qcko[0, 0, 0][ntke] + qcko[0, 0, 1][ntke]
                tem = tke[0, 0, 0] + tke[0, 0, 1]
                f1 = f1[0, 0, 0] - (ptem2 - tem) * (dtodsd * ptem)
                f1_p1 = tke[0, 0, 1] + (ptem2 - tem) * (dtodsu * ptem)
            else:
                f1_p1 = tke[0, 0, 1]

            if (
                scuflg[0, 0]
                and (k_mask[0, 0, 0] >= mrad[0, 0])
                and (k_mask[0, 0, 0] < krad[0, 0])
            ):
                ptem = 0.5 * tem2 * xmfd
                ptem2 = qcdo[0, 0, 0][ntke] + qcdo[0, 0, 1][ntke]
                tem = tke[0, 0, 0] + tke[0, 0, 1]
                f1 = f1[0, 0, 0] + (ptem2 - tem) * (dtodsd * ptem)
                f1_p1 = f1_p1 - (ptem2 - tem) * (dtodsu * ptem)

        with interval(-1, None):
            ad = ad_p1[0, 0]
            f1 = f1_p1[0, 0]
    with computation(PARALLEL), interval(...):
        cu = au
        rt = f1


def recover_tke_tendency(
    rtg: FloatFieldTracer,
    f1: FloatField,
    q1: FloatFieldTracer,
):
    from __externals__ import ntke, rdt

    with computation(PARALLEL), interval(...):
        f1 = max(f1, pbl_constants.TKMIN)
        qtend = (f1[0, 0, 0] - q1[0, 0, 0][ntke]) * rdt
        rtg[0, 0, 0][ntke] = rtg[0, 0, 0][ntke] + qtend


def heat_moist_tridiag_mat_ele_comp(
    ad: FloatField,
    ad_p1: FloatFieldIJ,
    al: FloatField,
    au: FloatField,
    delta: FloatField,
    dkt: FloatField,
    f1: FloatField,
    f1_p1: FloatFieldIJ,
    f2: FloatFieldTracer,
    f2_p1: FloatFieldIJ,
    kpbl: IntFieldIJ,
    krad: IntFieldIJ,
    k_mask: IntField,
    mrad: IntFieldIJ,
    pcnvflg: BoolFieldIJ,
    prsl: FloatField,
    q1: FloatFieldTracer,
    qcdo: FloatFieldTracer,
    qcko: FloatFieldTracer,
    rdzt: FloatField,
    scuflg: BoolFieldIJ,
    tcdo: FloatField,
    tcko: FloatField,
    t1: FloatField,
    xmf: FloatField,
    xmfd: FloatField,
    dtdz1: FloatFieldIJ,
    evap: FloatFieldIJ,
    heat: FloatFieldIJ,
    cu: FloatField,
    rt: FloatField,
    a2: FloatFieldTracer,
):
    from __externals__ import dt2

    with computation(FORWARD), interval(0, 1):
        ad = 1.0
        f1 = t1[0, 0, 0] + dtdz1[0, 0] * heat[0, 0]
        f2[0, 0, 0][0] = q1[0, 0, 0][0] + dtdz1[0, 0] * evap[0, 0]
        ad_p1 = 0.0
        f1_p1 = 0.0
        f2_p1 = 0.0

    with computation(FORWARD):
        with interval(0, -1):
            if k_mask > 0:
                f1 = f1_p1[0, 0]
                f2[0, 0, 0][0] = f2_p1[0, 0]
                ad = ad_p1[0, 0]

            dtodsd = dt2 / delta[0, 0, 0]
            dtodsu = dt2 / delta[0, 0, 1]
            dsig = prsl[0, 0, 0] - prsl[0, 0, 1]
            rdz = rdzt[0, 0, 0]
            tem1 = dsig * dkt[0, 0, 0] * rdz
            dsdzt = tem1 * (constants.GRAV / constants.CP_AIR)
            dsdz2 = tem1 * rdz
            au = -dtodsd * dsdz2
            al = -dtodsu * dsdz2
            ad = ad[0, 0, 0] - au[0, 0, 0]
            ad_p1 = 1.0 - al[0, 0, 0]

            if pcnvflg[0, 0] and k_mask[0, 0, 0] < kpbl[0, 0]:
                ptem = 0.5 * (dsig * rdz) * xmf[0, 0, 0]
                ptem1 = dtodsd * ptem
                ptem2 = dtodsu * ptem
                tem = t1[0, 0, 0] + t1[0, 0, 1]
                tem = (tcko[0, 0, 0] + tcko[0, 0, 1]) - tem
                f1 = f1[0, 0, 0] + dtodsd * dsdzt - tem * ptem1
                f1_p1 = t1[0, 0, 1] - dtodsu * dsdzt + tem * ptem2
                tem = q1[0, 0, 0][0] + q1[0, 0, 1][0]
                tem = (qcko[0, 0, 0][0] + qcko[0, 0, 1][0]) - tem
                f2[0, 0, 0][0] = f2[0, 0, 0][0] - tem * ptem1
                f2_p1 = q1[0, 0, 1][0] + tem * ptem2
            else:
                f1 = f1[0, 0, 0] + dtodsd * dsdzt
                f1_p1 = t1[0, 0, 1] - dtodsu * dsdzt
                f2_p1 = q1[0, 0, 1][0]

            if (
                scuflg[0, 0]
                and (k_mask[0, 0, 0] >= mrad[0, 0])
                and (k_mask[0, 0, 0] < krad[0, 0])
            ):
                ptem = 0.5 * (dsig * rdz) * xmfd[0, 0, 0]
                ptem1 = dtodsd * ptem
                ptem2 = dtodsu * ptem
                tem = t1[0, 0, 0] + t1[0, 0, 1]
                tem = (tcdo[0, 0, 0] + tcdo[0, 0, 1]) - tem
                f1 = f1[0, 0, 0] + tem * ptem1
                f1_p1 = f1_p1[0, 0] - tem * ptem2
                tem = q1[0, 0, 0][0] + q1[0, 0, 1][0]
                tem = (qcdo[0, 0, 0][0] + qcdo[0, 0, 1][0]) - tem
                f2[0, 0, 0][0] = f2[0, 0, 0][0] + tem * ptem1
                f2_p1 = f2_p1[0, 0] - tem * ptem2
        with interval(-1, None):
            f1 = f1_p1[0, 0]
            f2[0, 0, 0][0] = f2_p1[0, 0]
            ad = ad_p1[0, 0]

    with computation(PARALLEL), interval(...):
        cu = au
        rt = f1
        a2[0, 0, 0][0] = f2[0, 0, 0][0]


def setup_multi_tracer_tridiag(
    pcnvflg: BoolFieldIJ,
    k_mask: IntField,
    kpbl: IntFieldIJ,
    delta: FloatField,
    prsl: FloatField,
    rdzt: FloatField,
    xmf: FloatField,
    qcko: FloatFieldTracer,
    q1: FloatFieldTracer,
    f2: FloatFieldTracer,
    f2_p1: FloatFieldIJ,
    scuflg: BoolFieldIJ,
    mrad: IntFieldIJ,
    krad: IntFieldIJ,
    xmfd: FloatField,
    qcdo: FloatFieldTracer,
    a2: FloatFieldTracer,
    n_index: int,
):
    from __externals__ import dt2

    with computation(FORWARD), interval(0, 1):
        f2[0, 0, 0][n_index] = q1[0, 0, 0][n_index]
        f2_p1 = 0.0

    with computation(FORWARD):
        with interval(0, -1):
            if k_mask > 0:
                f2[0, 0, 0][n_index] = f2_p1

            if pcnvflg[0, 0] and k_mask[0, 0, 0] < kpbl[0, 0]:
                dtodsd = dt2 / delta[0, 0, 0]
                dtodsu = dt2 / delta[0, 0, 1]
                dsig = prsl[0, 0, 0] - prsl[0, 0, 1]
                tem = dsig * rdzt[0, 0, 0]
                ptem = 0.5 * tem * xmf[0, 0, 0]
                ptem1 = dtodsd * ptem
                ptem2 = dtodsu * ptem
                tem1 = qcko[0, 0, 0][n_index] + qcko[0, 0, 1][n_index]
                tem2 = q1[0, 0, 0][n_index] + q1[0, 0, 1][n_index]
                # Kgao note: turmn off non-local mixing
                f2[0, 0, 0][n_index] = f2[0, 0, 0][n_index]  # - (tem1 - tem2) * ptem1
                f2_p1 = q1[0, 0, 1][n_index]  # + (tem1 - tem2) * ptem2
            else:
                f2_p1 = q1[0, 0, 1][n_index]

            if (
                scuflg[0, 0]
                and (k_mask[0, 0, 0] >= mrad[0, 0])
                and (k_mask[0, 0, 0] < krad[0, 0])
            ):
                dtodsd = dt2 / delta[0, 0, 0]
                dtodsu = dt2 / delta[0, 0, 1]
                dsig = prsl[0, 0, 0] - prsl[0, 0, 1]
                tem = dsig * rdzt[0, 0, 0]
                ptem = 0.5 * tem * xmfd[0, 0, 0]
                ptem1 = dtodsd * ptem
                ptem2 = dtodsu * ptem
                tem1 = qcdo[0, 0, 0][n_index] + qcdo[0, 0, 1][n_index]
                tem2 = q1[0, 0, 0][n_index] + q1[0, 0, 1][n_index]
                # Kgao note: turmn off non-local mixing
                f2[0, 0, 0][n_index] = f2[0, 0, 0][n_index]  # + (tem1 - tem2) * ptem1
                f2_p1 = f2_p1  # - (tem1 - tem2) * ptem2

        with interval(-1, None):
            f2[0, 0, 0][n_index] = f2_p1

    with computation(PARALLEL), interval(...):
        a2[0, 0, 0][n_index] = f2[0, 0, 0][n_index]


def recover_moisture_tendency(
    f2: FloatFieldTracer,
    q1: FloatFieldTracer,
    rtg: FloatFieldTracer,
    n_index: int,
):
    from __externals__ import rdt

    with computation(PARALLEL), interval(...):
        qtend = (f2[0, 0, 0][n_index] - q1[0, 0, 0][n_index]) * rdt
        rtg[0, 0, 0][n_index] = rtg[0, 0, 0][n_index] + qtend


def recover_heat_tendency_add_diss_heat(
    tdt: FloatField,
    f1: FloatField,
    t1: FloatField,
    f2: FloatFieldTracer,
    q1: FloatFieldTracer,
    rtg: FloatFieldTracer,
    dtsfc: FloatFieldIJ,
    delta: FloatField,
    dqsfc: FloatFieldIJ,
):
    from __externals__ import rdt

    with computation(FORWARD), interval(...):
        ttend = (f1[0, 0, 0] - t1[0, 0, 0]) * rdt
        qtend = (f2[0, 0, 0][0] - q1[0, 0, 0][0]) * rdt
        tdt = tdt[0, 0, 0] + ttend
        rtg[0, 0, 0][0] = rtg[0, 0, 0][0] + qtend
        dtsfc = (
            dtsfc[0, 0] + (constants.CP_AIR / constants.GRAV) * delta[0, 0, 0] * ttend
        )
        dqsfc = dqsfc[0, 0] + (constants.HLV / constants.GRAV) * delta[0, 0, 0] * qtend


def moment_tridiag_mat_ele_comp(
    ad: FloatField,
    ad_p1: FloatFieldIJ,
    al: FloatField,
    au: FloatField,
    delta: FloatField,
    diss: FloatField,
    dku: FloatField,
    dtdz1: FloatFieldIJ,
    f1: FloatField,
    f1_p1: FloatFieldIJ,
    f2: FloatFieldTracer,
    f2_p1: FloatFieldIJ,
    kpbl: IntFieldIJ,
    krad: IntFieldIJ,
    k_mask: IntField,
    mrad: IntFieldIJ,
    pcnvflg: BoolFieldIJ,
    prsl: FloatField,
    rdzt: FloatField,
    scuflg: BoolFieldIJ,
    spd1: FloatFieldIJ,
    stress: FloatFieldIJ,
    tdt: FloatField,
    u1: FloatField,
    ucdo: FloatField,
    ucko: FloatField,
    v1: FloatField,
    vcdo: FloatField,
    vcko: FloatField,
    xmf: FloatField,
    xmfd: FloatField,
    cu: FloatField,
    rt: FloatField,
    a2: FloatFieldTracer,
):
    from __externals__ import dspheat, dt2

    with computation(PARALLEL), interval(0, -1):
        if dspheat:
            tdt = tdt[0, 0, 0] + pbl_constants.DSPFAC * (
                diss[0, 0, 0] / constants.CP_AIR
            )

    with computation(FORWARD), interval(0, 1):
        ad = 1.0 + dtdz1[0, 0] * stress[0, 0] / spd1[0, 0]
        f1 = u1[0, 0, 0]
        f2[0, 0, 0][0] = v1[0, 0, 0]
        ad_p1 = 0.0
        f1_p1 = 0.0
        f2_p1 = 0.0

    with computation(FORWARD):
        with interval(0, -1):
            if k_mask > 0:
                f1 = f1_p1[0, 0]
                f2[0, 0, 0][0] = f2_p1[0, 0]
                ad = ad_p1[0, 0]

            dtodsd = dt2 / delta[0, 0, 0]
            dtodsu = dt2 / delta[0, 0, 1]
            dsig = prsl[0, 0, 0] - prsl[0, 0, 1]
            rdz = rdzt[0, 0, 0]
            dsdz2 = dsig * dku[0, 0, 0] * rdz * rdz
            au = -dtodsd * dsdz2
            al = -dtodsu * dsdz2
            ad = ad[0, 0, 0] - au[0, 0, 0]
            ad_p1 = 1.0 - al[0, 0, 0]

            if pcnvflg[0, 0] and k_mask[0, 0, 0] < kpbl[0, 0]:
                ptem = 0.5 * (dsig * rdz) * xmf[0, 0, 0]
                ptem1 = dtodsd * ptem
                ptem2 = dtodsu * ptem
                tem = u1[0, 0, 0] + u1[0, 0, 1]
                tem = (ucko[0, 0, 0] + ucko[0, 0, 1]) - tem
                f1 = f1[0, 0, 0] - tem * ptem1
                f1_p1 = u1[0, 0, 1] + tem * ptem2
                tem2 = v1[0, 0, 0] + v1[0, 0, 1]
                tem2 = (vcko[0, 0, 0] + vcko[0, 0, 1]) - tem2
                f2[0, 0, 0][0] = f2[0, 0, 0][0] - tem2 * ptem1
                f2_p1 = v1[0, 0, 1] + tem2 * ptem2
            else:
                f1_p1 = u1[0, 0, 1]
                f2_p1 = v1[0, 0, 1]

            if (
                (scuflg[0, 0])
                and (k_mask[0, 0, 0] >= mrad[0, 0])
                and (k_mask[0, 0, 0] < krad[0, 0])
            ):
                ptem = 0.5 * (dsig * rdz) * xmfd[0, 0, 0]
                ptem1 = dtodsd * ptem
                ptem2 = dtodsu * ptem
                tem = u1[0, 0, 0] + u1[0, 0, 1]
                tem = (ucdo[0, 0, 0] + ucdo[0, 0, 1]) - tem
                f1 = f1[0, 0, 0] + tem * ptem1
                f1_p1 = f1_p1[0, 0] - tem * ptem2
                tem = v1[0, 0, 0] + v1[0, 0, 1]
                tem = (vcdo[0, 0, 0] + vcdo[0, 0, 1]) - tem
                f2[0, 0, 0][0] = f2[0, 0, 0][0] + tem * ptem1
                f2_p1 = f2_p1[0, 0] - tem * ptem2

        with interval(-1, None):
            f1 = f1_p1[0, 0]
            f2[0, 0, 0][0] = f2_p1[0, 0]
            ad = ad_p1[0, 0]
    with computation(PARALLEL), interval(...):
        cu = au
        rt = f1
        a2[0, 0, 0][0] = f2[0, 0, 0][0]


def recover_momentum_tendency_and_finish(
    delta: FloatField,
    du: FloatField,
    dusfc: FloatFieldIJ,
    dv: FloatField,
    dvsfc: FloatFieldIJ,
    f1: FloatField,
    f2: FloatFieldTracer,
    hpbl: FloatFieldIJ,
    hpblx: FloatFieldIJ,
    kpbl: IntFieldIJ,
    kpblx: IntFieldIJ,
    k_mask: IntField,
    u1: FloatField,
    v1: FloatField,
):
    from __externals__ import rdt

    with computation(FORWARD), interval(...):
        if k_mask[0, 0, 0] == 0:
            hpbl = hpblx[0, 0]
            kpbl = kpblx[0, 0]
        utend = (f1[0, 0, 0] - u1[0, 0, 0]) * rdt
        vtend = (f2[0, 0, 0][0] - v1[0, 0, 0]) * rdt
        du = du[0, 0, 0] + utend
        dv = dv[0, 0, 0] + vtend
        dusfc = dusfc[0, 0] + constants.RGRAV * delta[0, 0, 0] * utend
        dvsfc = dvsfc[0, 0] + constants.RGRAV * delta[0, 0, 0] * vtend


class ScaleAwareTKEMoistEDMF:
    """
    Scheme to compute subgrid vertical turbulence mixing
    using scale-aware TKE-based moist eddy-diffusion mass-flux (EDMF)
    parameterization

    Fortran name is satmedmfvdif
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        quantity_factory: QuantityFactory,
        grid_area: FloatFieldIJ,
        config: PBLConfig,
    ):
        if config.do_dk_hb19:
            raise NotImplementedError("do_dk_hb19 has not been implemented")

        self._ntracers = config.ntracers
        assert (
            self._ntracers == 9
        ), f"PBL scheme satmedmfvdif requires ntracer ({config.ntracers}) == 9"

        self._ntrac1 = self._ntracers - 1

        self.TRACER_DIM = TRACER_DIM
        self.quantity_factory = quantity_factory
        if self.TRACER_DIM not in self.quantity_factory.sizer.data_dimensions.keys():
            self.quantity_factory.add_data_dimensions(
                {
                    self.TRACER_DIM: self._ntracers,
                }
            )
        idx = stencil_factory.grid_indexing

        def make_quantity():
            return quantity_factory.zeros(
                [I_DIM, J_DIM, K_DIM],
                units="unknown",
                dtype=Float,
            )

        def make_quantity_2D(type):
            return quantity_factory.zeros([I_DIM, J_DIM], units="unknown", dtype=type)

        # Allocate internal variables:
        km1 = idx.domain[2] - 1
        self._kmpbl = idx.domain[2] // 2 + 1
        self._kmscu = idx.domain[2] // 2 + 1

        self._dt_atmos = config.dt_atmos
        self._rdt = 1.0 / self._dt_atmos
        self._kk = max(round(self._dt_atmos / pbl_constants.CDTN), 1)
        self._dtn = self._dt_atmos / float(self._kk)

        self._area = grid_area

        self._ntiw = config.ntiw
        self._ntcw = config.ntcw
        self._ntke = config.ntke

        self._dspheat = config.dspheat

        # Layer mask:
        self._k_mask = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Int,
        )

        for k in range(idx.domain[2]):
            self._k_mask[:, :, k] = k

        # Internal compute variables
        self._lcld = make_quantity_2D(Int)
        self._kcld = make_quantity_2D(Int)
        self._krad = make_quantity_2D(Int)
        self._kx1 = make_quantity_2D(Int)
        self._kpblx = make_quantity_2D(Int)
        self._tke = make_quantity()
        self._tkeh = make_quantity()
        self._theta = make_quantity()
        self._thvx = make_quantity()
        self._thlvx = make_quantity()
        self._thlvx_0 = make_quantity_2D(Float)
        self._qlx = make_quantity()
        self._thetae = make_quantity()
        self._thlx = make_quantity()
        self._slx = make_quantity()
        self._svx = make_quantity()
        self._qtx = make_quantity()
        self._tvx = make_quantity()
        self._pix = make_quantity()
        self._radx = make_quantity()
        self._dku = make_quantity()
        self._dkt = make_quantity()
        self._dkq = make_quantity()
        self._cku = make_quantity()
        self._ckt = make_quantity()
        self._plyr = make_quantity()
        self._rhly = make_quantity()
        self._cfly = make_quantity()
        self._qstl = make_quantity()
        self._dtdz1 = make_quantity_2D(Float)
        self._gdx = make_quantity_2D(Float)
        self._phih = make_quantity_2D(Float)
        self._phim = make_quantity_2D(Float)
        self._prn = make_quantity()
        self._rbdn = make_quantity_2D(Float)
        self._rbup = make_quantity_2D(Float)
        self._thermal = make_quantity_2D(Float)
        self._ustar = make_quantity_2D(Float)
        self._wstar = make_quantity_2D(Float)
        self._hpblx = make_quantity_2D(Float)
        self._ust3 = make_quantity_2D(Float)
        self._wst3 = make_quantity_2D(Float)
        self._z0 = make_quantity_2D(Float)
        self._crb = make_quantity_2D(Float)
        self._hgamt = make_quantity_2D(Float)
        self._hgamq = make_quantity_2D(Float)
        self._wscale = make_quantity_2D(Float)
        self._vpert = make_quantity_2D(Float)
        self._zol = make_quantity_2D(Float)
        self._sflux = make_quantity_2D(Float)
        self._radj = make_quantity_2D(Float)
        self._tx1 = make_quantity_2D(Float)
        self._tx2 = make_quantity_2D(Float)
        self._radmin = make_quantity_2D(Float)
        self._zi = make_quantity()
        self._zl = make_quantity()
        self._zldn = make_quantity()
        self._zlup = make_quantity()
        self._zm = make_quantity()
        self._xkzo = make_quantity()
        self._xkzmo = make_quantity()
        self._xkzm_hx = make_quantity_2D(Float)
        self._xkzm_mx = make_quantity_2D(Float)
        self._lev = make_quantity_2D(Int)
        self._rdzt = make_quantity()
        self._al = make_quantity()
        self._ad = make_quantity()
        self._au = make_quantity()
        self._f1 = make_quantity()
        self._elm = make_quantity()
        self._ele = make_quantity()
        self._rle = make_quantity()
        self._ckz = make_quantity()
        self._chz = make_quantity()
        self._diss = make_quantity()
        self._prod = make_quantity()
        self._bf = make_quantity()
        self._shr2 = make_quantity()
        self._xlamue = make_quantity()
        self._xlamde = make_quantity()
        self._gotvx = make_quantity()
        self._rlam = make_quantity()
        self._mrad = make_quantity_2D(Int)
        self._ad_p1 = make_quantity_2D(Float)
        self._f1_p1 = make_quantity_2D(Float)
        self._f2_p1 = make_quantity_2D(Float)
        self._tem1 = make_quantity()

        # Variables for updrafts (thermals):
        self._tcko = make_quantity()
        self._ucko = make_quantity()
        self._vcko = make_quantity()
        self._buou = make_quantity()
        self._xmf = make_quantity()

        # Variables for stratocumulus-top induced downdrafts:
        self._tcdo = make_quantity()
        self._ucdo = make_quantity()
        self._vcdo = make_quantity()
        self._buod = make_quantity()
        self._xmfd = make_quantity()

        self._mlenflg = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
            dtype=Bool,
        )
        self._pblflg = make_quantity_2D(Bool)
        self._sfcflg = make_quantity_2D(Bool)
        self._flg = make_quantity_2D(Bool)
        self._scuflg = quantity_factory.ones([I_DIM, J_DIM], units="none", dtype=Bool)
        self._pcnvflg = make_quantity_2D(Bool)

        # Limiting pressures for vertical loops:
        self._ptop = make_quantity_2D(Float)
        self._pbot = make_quantity_2D(Float)

        # Arrays for tridiag calculations
        self._cu = make_quantity()
        self._rt = make_quantity()

        # Allocate higher order fields
        self._f2 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )
        self._a2 = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._qcko = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        self._qcdo = quantity_factory.zeros(
            [I_DIM, J_DIM, K_DIM, self.TRACER_DIM],
            units="unknown",
            dtype=Float,
        )

        # Init stencils:
        self._init_turbulence = stencil_factory.from_origin_domain(
            func=init_turbulence,
            externals={
                "cap_k0_land": config.cap_k0_land,
                "do_dk_hb19": config.do_dk_hb19,
                "dt2": self._dt_atmos,
                "km1": km1,
                "ntcw": self._ntcw,
                "ntiw": self._ntiw,
                "ntke": self._ntke,
                "xkzm_hi": config.xkzm_hi,
                "xkzm_hl": config.xkzm_hl,
                "xkzm_ho": config.xkzm_ho,
                "xkzm_lim": config.xkzm_lim,
                "xkzm_mi": config.xkzm_mi,
                "xkzm_ml": config.xkzm_ml,
                "xkzm_mo": config.xkzm_mo,
                "xkzm_s": config.xkzm_s,
                "xkzminv": config.xkzminv,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(add=(0, 0, 1)),
        )

        self._mrf_pbl_scheme_part1 = stencil_factory.from_origin_domain(
            func=mrf_pbl_scheme_part1,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

        self._mrf_pbl_2_thermal_excess = stencil_factory.from_origin_domain(
            func=mrf_pbl_2_thermal_excess,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._thermal_pbl_calc = stencil_factory.from_origin_domain(
            func=thermal_pbl_calc,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

        self._enhance_pbl_height_thermal = stencil_factory.from_origin_domain(
            func=enhance_pbl_height_thermal,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._stratocumulus = stencil_factory.from_origin_domain(
            func=stratocumulus,
            externals={"km1": km1},
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmscu),
        )

        self._compute_mass_flux_prelim = stencil_factory.from_origin_domain(
            func=compute_mass_flux_prelim,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )
        self._compute_mass_flux_tracer_prelim = stencil_factory.from_origin_domain(
            func=compute_mass_flux_tracer_prelim,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._mfpblt = PBLMassFlux(
            stencil_factory,
            quantity_factory,
            self._dt_atmos,
            self._ntcw,
            self._ntrac1,
            self._kmpbl,
        )

        self._mfscu = StratocumulusMassFlux(
            stencil_factory,
            quantity_factory,
            self._dt_atmos,
            self._ntracers,
            self._ntcw,
            self._ntrac1,
            self._kmscu,
            self._ntke,
        )

        self._compute_prandtl_num_exchange_coeff = stencil_factory.from_origin_domain(
            func=compute_prandtl_num_exchange_coeff,
            origin=idx.origin_compute(),
            domain=(idx.iec, idx.jec, self._kmpbl),
        )

        self._compute_asymptotic_mixing_length = stencil_factory.from_origin_domain(
            func=compute_asymptotic_mixing_length,
            externals={"km1": km1},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._compute_eddy_diffusivity_buoy_shear = stencil_factory.from_origin_domain(
            func=compute_eddy_diffusivity_buoy_shear,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._predict_tke = stencil_factory.from_origin_domain(
            func=predict_tke,
            externals={
                "dtn": self._dtn,
                "kk": self._kk,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(add=(0, 0, -1)),
        )

        self._tke_up_down_prop = stencil_factory.from_origin_domain(
            func=tke_up_down_prop,
            externals={
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tke_tridiag_matrix_ele_comp = stencil_factory.from_origin_domain(
            func=tke_tridiag_matrix_ele_comp,
            externals={
                "dt2": self._dt_atmos,
                "ntke": self._ntke,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tridit = stencil_factory.from_origin_domain(
            func=tridit,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_tke_tendency = stencil_factory.from_origin_domain(
            func=recover_tke_tendency,
            externals={"rdt": self._rdt, "ntke": self._ntke},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._heat_moist_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=heat_moist_tridiag_mat_ele_comp,
            externals={"dt2": self._dt_atmos},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        if self._ntrac1 >= 2:
            self._setup_multi_tracer_tridiag = stencil_factory.from_origin_domain(
                func=setup_multi_tracer_tridiag,
                externals={"dt2": self._dt_atmos},
                origin=idx.origin_compute(),
                domain=idx.domain_compute(),
            )

        self._tridin = stencil_factory.from_origin_domain(
            func=tridin,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_moisture_tendency = stencil_factory.from_origin_domain(
            func=recover_moisture_tendency,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_heat_tendency_add_diss_heat = stencil_factory.from_origin_domain(
            func=recover_heat_tendency_add_diss_heat,
            externals={
                "rdt": self._rdt,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._moment_tridiag_mat_ele_comp = stencil_factory.from_origin_domain(
            func=moment_tridiag_mat_ele_comp,
            externals={
                "dspheat": self._dspheat,
                "dt2": self._dt_atmos,
            },
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._tridi2 = stencil_factory.from_origin_domain(
            func=tridi2,
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

        self._recover_momentum_tendency_and_finish = stencil_factory.from_origin_domain(
            func=recover_momentum_tendency_and_finish,
            externals={"rdt": self._rdt},
            origin=idx.origin_compute(),
            domain=idx.domain_compute(),
        )

    def __call__(
        self,
        state: SATMEDMFVDiffState,
    ):
        """
        ix is the block size in i, for us the same as im since gt4py handles threading
        Still have to figure out what to do with:
        rtg(im,km,ntrac), q1(ix,km,ntrac),
        """

        self._init_turbulence(
            self._zi,
            self._zl,
            self._zm,
            state.phii,
            state.phil,
            self._chz,
            self._ckz,
            self._area,
            self._gdx,
            self._tke,
            state.q1,
            self._rdzt,
            self._prn,
            self._kx1,
            state.prsi,
            self._k_mask,
            state.kinver,
            self._tx1,
            self._tx2,
            self._xkzo,
            self._xkzmo,
            self._kpblx,
            self._hpblx,
            self._pblflg,
            self._sfcflg,
            self._pcnvflg,
            self._scuflg,
            state.zorl,
            state.dusfc,
            state.dvsfc,
            state.dtsfc,
            state.dqsfc,
            state.kpbl,
            state.hpbl,
            state.rbsoil,
            self._radmin,
            self._mrad,
            self._krad,
            self._lcld,
            self._kcld,
            self._theta,
            state.prslk,
            state.psk,
            state.t1,
            self._pix,
            self._qlx,
            self._slx,
            self._thvx,
            self._qtx,
            self._thlx,
            self._thlvx,
            self._svx,
            self._thetae,
            self._gotvx,
            state.prsl,
            self._plyr,
            self._rhly,
            self._qstl,
            self._bf,
            self._cfly,
            self._crb,
            self._dtdz1,
            state.evap,
            state.heat,
            state.hlw,
            self._radx,
            self._sflux,
            self._shr2,
            state.stress,
            state.hsw,
            self._thermal,
            state.tsea,
            self._ustar,
            state.u1,
            state.v1,
            state.u10m,
            state.v10m,
            state.xmu,
            state.islimsk,
            self._ptop,
            self._pbot,
            self._xkzm_hx,
            self._xkzm_mx,
            self._tvx,
            self._tem1,
        )

        self._mrf_pbl_scheme_part1(
            self._crb,
            self._flg,
            self._kpblx,
            self._k_mask,
            self._rbdn,
            self._rbup,
            state.rbsoil,
            self._thermal,
            self._thlvx,
            self._thlvx_0,
            state.u1,
            state.v1,
            self._zl,
        )

        self._mrf_pbl_2_thermal_excess(
            self._crb,
            state.evap,
            state.fh,
            self._flg,
            state.fm,
            self._gotvx,
            state.heat,
            state.hpbl,
            self._hpblx,
            state.kpbl,
            self._kpblx,
            self._k_mask,
            self._pblflg,
            self._pcnvflg,
            self._phih,
            self._phim,
            self._rbdn,
            self._rbup,
            state.rbsoil,
            self._sfcflg,
            self._sflux,
            self._thermal,
            self._theta,
            self._ustar,
            self._vpert,
            self._zi,
            self._zl,
            self._zol,
        )

        self._thermal_pbl_calc(
            self._crb,
            self._flg,
            state.kpbl,
            self._k_mask,
            self._rbdn,
            self._rbup,
            self._thermal,
            self._thlvx,
            self._thlvx_0,
            state.u1,
            state.v1,
            self._zl,
        )

        self._enhance_pbl_height_thermal(
            self._crb,
            state.hpbl,
            state.kpbl,
            self._k_mask,
            self._pblflg,
            self._pcnvflg,
            self._rbdn,
            self._rbup,
            self._zi,
            self._zl,
        )

        self._stratocumulus(
            self._flg,
            self._kcld,
            self._krad,
            self._lcld,
            self._k_mask,
            self._radmin,
            self._radx,
            self._qlx,
            self._scuflg,
            self._zl,
        )

        self._compute_mass_flux_prelim(
            self._pcnvflg,
            self._scuflg,
            state.t1,
            self._tcdo,
            self._tcko,
            state.u1,
            self._ucdo,
            self._ucko,
            state.v1,
            self._vcdo,
            self._vcko,
        )

        for n in range(self._ntracers):
            dim_n = n
            if dim_n != self._ntke:
                self._compute_mass_flux_tracer_prelim(
                    self._qcdo,
                    self._qcko,
                    state.q1,
                    self._pcnvflg,
                    self._scuflg,
                    dim_n,
                )

        self._mfpblt(
            self._pcnvflg,
            self._zl,
            self._zm,
            state.q1,
            state.u1,
            state.v1,
            self._plyr,
            self._pix,
            self._thlx,
            self._thvx,
            self._gdx,
            state.hpbl,
            state.kpbl,
            self._vpert,
            self._buou,
            self._xmf,
            self._tcko,
            self._qcko,
            self._ucko,
            self._vcko,
            self._xlamue,
            self._k_mask,
        )

        self._mfscu(
            self._scuflg,
            self._zl,
            self._zm,
            state.q1,
            state.u1,
            state.v1,
            self._plyr,
            self._pix,
            self._thlx,
            self._thvx,
            self._thlvx,
            self._gdx,
            self._thetae,
            self._radj,
            self._krad,
            self._mrad,
            self._radmin,
            self._buod,
            self._xmfd,
            self._tcdo,
            self._qcdo,
            self._ucdo,
            self._vcdo,
            self._xlamde,
            self._k_mask,
        )

        self._compute_prandtl_num_exchange_coeff(
            self._chz,
            self._ckz,
            state.hpbl,
            state.kpbl,
            self._k_mask,
            self._pcnvflg,
            self._phih,
            self._phim,
            self._prn,
            self._zi,
        )

        self._compute_asymptotic_mixing_length(
            self._zldn,
            self._zlup,
            self._thvx,
            self._tke,
            self._gotvx,
            self._zl,
            state.tsea,
            state.q1,
            self._zi,
            self._rlam,
            self._ele,
            self._elm,
            self._tem1,
            self._zol,
            self._gdx,
            self._lev,
            self._k_mask,
            self._mlenflg,
        )

        self._compute_eddy_diffusivity_buoy_shear(
            self._bf,
            self._buod,
            self._buou,
            self._chz,
            self._ckz,
            self._dku,
            self._dkt,
            self._dkq,
            self._elm,
            self._gotvx,
            state.kpbl,
            self._k_mask,
            self._mrad,
            self._krad,
            self._pblflg,
            self._pcnvflg,
            self._phim,
            self._prn,
            self._prod,
            self._radj,
            self._rdzt,
            self._scuflg,
            self._sflux,
            self._shr2,
            state.stress,
            self._tke,
            state.u1,
            self._ucdo,
            self._ucko,
            self._ustar,
            state.v1,
            self._vcdo,
            self._vcko,
            self._xkzo,
            self._xkzmo,
            self._xmf,
            self._xmfd,
            self._zl,
            state.dkt,
        )

        self._predict_tke(
            self._diss,
            self._prod,
            self._rle,
            self._tke,
            self._ele,
        )

        self._tke_up_down_prop(
            self._pcnvflg,
            self._qcdo,
            self._qcko,
            self._scuflg,
            self._tke,
            state.kpbl,
            self._k_mask,
            self._xlamue,
            self._zl,
            self._krad,
            self._mrad,
            self._xlamde,
        )

        self._tke_tridiag_matrix_ele_comp(
            self._ad,
            self._ad_p1,
            self._al,
            self._au,
            state.delta,
            self._dkq,
            self._f1,
            self._f1_p1,
            state.kpbl,
            self._krad,
            self._k_mask,
            self._mrad,
            self._pcnvflg,
            state.prsl,
            self._qcdo,
            self._qcko,
            self._rdzt,
            self._scuflg,
            self._tke,
            self._xmf,
            self._xmfd,
            self._cu,
            self._rt,
        )

        self._tridit(
            self._cu,
            self._ad,
            self._al,
            self._rt,
            self._au,
            self._f1,
        )

        self._recover_tke_tendency(
            state.rtg,
            self._f1,
            state.q1,
        )

        self._heat_moist_tridiag_mat_ele_comp(
            self._ad,
            self._ad_p1,
            self._al,
            self._au,
            state.delta,
            self._dkt,
            self._f1,
            self._f1_p1,
            self._f2,
            self._f2_p1,
            state.kpbl,
            self._krad,
            self._k_mask,
            self._mrad,
            self._pcnvflg,
            state.prsl,
            state.q1,
            self._qcdo,
            self._qcko,
            self._rdzt,
            self._scuflg,
            self._tcdo,
            self._tcko,
            state.t1,
            self._xmf,
            self._xmfd,
            self._dtdz1,
            state.evap,
            state.heat,
            self._cu,
            self._rt,
            self._a2,
        )

        for n in range(self._ntracers):
            dim_n = n
            if dim_n != self._ntke:
                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._setup_multi_tracer_tridiag(
                            self._pcnvflg,
                            self._k_mask,
                            state.kpbl,
                            state.delta,
                            state.prsl,
                            self._rdzt,
                            self._xmf,
                            self._qcko,
                            state.q1,
                            self._f2,
                            self._f2_p1,
                            self._scuflg,
                            self._mrad,
                            self._krad,
                            self._xmfd,
                            self._qcdo,
                            self._a2,
                            dim_n,
                        )

                self._tridin(
                    self._al,
                    self._ad,
                    self._cu,
                    self._rt,
                    self._a2,
                    self._au,
                    self._f1,
                    self._f2,
                    dim_n,
                )

                if dim_n > 0:
                    if self._ntrac1 >= 2:
                        self._recover_moisture_tendency(
                            self._f2,
                            state.q1,
                            state.rtg,
                            dim_n,
                        )

        self._recover_heat_tendency_add_diss_heat(
            state.dtdt,
            self._f1,
            state.t1,
            self._f2,
            state.q1,
            state.rtg,
            state.dtsfc,
            state.delta,
            state.dqsfc,
        )

        self._moment_tridiag_mat_ele_comp(
            self._ad,
            self._ad_p1,
            self._al,
            self._au,
            state.delta,
            self._diss,
            self._dku,
            self._dtdz1,
            self._f1,
            self._f1_p1,
            self._f2,
            self._f2_p1,
            state.kpbl,
            self._krad,
            self._k_mask,
            self._mrad,
            self._pcnvflg,
            state.prsl,
            self._rdzt,
            self._scuflg,
            state.spd1,
            state.stress,
            state.dtdt,
            state.u1,
            self._ucdo,
            self._ucko,
            state.v1,
            self._vcdo,
            self._vcko,
            self._xmf,
            self._xmfd,
            self._cu,
            self._rt,
            self._a2,
        )

        self._tridi2(
            self._f1,
            self._f2,
            self._au,
            self._al,
            self._ad,
            self._cu,
            self._rt,
            self._a2,
        )

        self._recover_momentum_tendency_and_finish(
            state.delta,
            state.du,
            state.dusfc,
            state.dv,
            state.dvsfc,
            self._f1,
            self._f2,
            state.hpbl,
            self._hpblx,
            state.kpbl,
            self._kpblx,
            self._k_mask,
            state.u1,
            state.v1,
        )
