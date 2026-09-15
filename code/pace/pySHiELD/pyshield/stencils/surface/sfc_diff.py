import ndsl.constants as constants
import pyshield.stencils.surface.constants as sfcons
from ndsl import StencilFactory
from ndsl.dsl.gt4py import FORWARD, computation, exp
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import interval, log, sqrt
from ndsl.dsl.typing import Bool, BoolFieldIJ, Float, FloatFieldIJ, Int, IntFieldIJ
from ndsl.stencils.arithmetic_functions import sign
from pyshield.functions.physics_functions import fpvsx


@gtfunction
def monin_obukhov_similarity(
    z1: FloatFieldIJ,
    snwdph: FloatFieldIJ,
    thv1: Float,
    wind: FloatFieldIJ,
    z0max: Float,
    ztmax: Float,
    tvs: Float,
):
    z1i = 1.0 / z1

    tem1 = z0max / z1
    if abs(1.0 - tem1) > 1.0e-6:
        ztmax1 = -sfcons.BETA * log(tem1) / (sfcons.ALPHA2 * (1.0 - tem1))
    else:
        ztmax1 = 99.0
    if (z0max < 0.05) and (snwdph < 10.0):
        ztmax1 = 99.0

    # compute stability indices (rb and hlinf)
    dtv = thv1 - tvs
    adtv = max(abs(dtv), 0.001)
    dtv = sign(1.0, dtv) * adtv
    rb = max(
        -5000.0,
        (constants.GRAV + constants.GRAV) * dtv * z1 / ((thv1 + tvs) * wind * wind),
    )
    tem1 = 1.0 / z0max
    tem2 = 1.0 / ztmax
    fm = log((z0max + z1) * tem1)
    fh = log((ztmax + z1) * tem2)
    fm10 = log((z0max + 10.0) * tem1)
    fh2 = log((ztmax + 2.0) * tem2)
    hlinf = rb * fm * fm / fh
    hlinf = min(max(hlinf, sfcons.ZTMIN), ztmax1)

    # stable case
    if dtv >= 0.0:
        hl1 = hlinf
        if hlinf > 0.25:
            tem1 = hlinf * z1i
            hl0inf = z0max * tem1
            hltinf = ztmax * tem1
            aa = sqrt(1.0 + sfcons.ALPHA4 * hlinf)
            aa0 = sqrt(1.0 + sfcons.ALPHA4 * hl0inf)
            bb = aa
            bb0 = sqrt(1.0 + sfcons.ALPHA4 * hltinf)
            pm = aa0 - aa + log((aa + 1.0) / (aa0 + 1.0))
            ph = bb0 - bb + log((bb + 1.0) / (bb0 + 1.0))
            fms = fm - pm
            fhs = fh - ph
            hl1 = fms * fms * rb / fhs
            hl1 = min(max(hl1, sfcons.ZTMIN), ztmax1)

        # second iteration
        tem1 = hl1 * z1i
        hl0 = z0max * tem1
        hlt = ztmax * tem1
        aa = sqrt(1.0 + sfcons.ALPHA4 * hl1)
        aa0 = sqrt(1.0 + sfcons.ALPHA4 * hl0)
        bb = aa
        bb0 = sqrt(1.0 + sfcons.ALPHA4 * hlt)
        pm = aa0 - aa + log((1.0 + aa) / (1.0 + aa0))
        ph = bb0 - bb + log((1.0 + bb) / (1.0 + bb0))
        hl110 = hl1 * 10.0 * z1i
        hl110 = min(max(hl110, sfcons.ZTMIN), ztmax1)
        aa = sqrt(1.0 + sfcons.ALPHA4 * hl110)
        pm10 = aa0 - aa + log((1.0 + aa) / (1.0 + aa0))
        hl12 = (hl1 + hl1) * z1i
        hl12 = min(max(hl12, sfcons.ZTMIN), ztmax1)
        bb = sqrt(1.0 + sfcons.ALPHA4 * hl12)
        ph2 = bb0 - bb + log((1.0 + bb) / (1.0 + bb0))

        # unstable case - check for unphysical obukhov length

    else:  # dtv < 0 case
        olinf = z1 / hlinf
        tem1 = 50.0 * z0max
        if abs(olinf) <= tem1:
            hlinf = -z1 / tem1
            hlinf = min(max(hlinf, sfcons.ZTMIN), ztmax1)

        # get pm and ph
        if hlinf >= -0.5:
            hl1 = hlinf
            pm = (
                (sfcons.A0 + sfcons.A1 * hl1)
                * hl1
                / (1.0 + (sfcons.B1 + sfcons.B2 * hl1) * hl1)
            )
            ph = (
                (sfcons.A0P + sfcons.A1P * hl1)
                * hl1
                / (1.0 + (sfcons.B1P + sfcons.B2P * hl1) * hl1)
            )
            hl110 = hl1 * 10.0 * z1i
            hl110 = min(max(hl110, sfcons.ZTMIN), ztmax1)
            pm10 = (
                (sfcons.A0 + sfcons.A1 * hl110)
                * hl110
                / (1.0 + (sfcons.B1 + sfcons.B2 * hl110) * hl110)
            )
            hl12 = (hl1 + hl1) * z1i
            hl12 = min(max(hl12, sfcons.ZTMIN), ztmax1)
            ph2 = (
                (sfcons.A0P + sfcons.A1P * hl12)
                * hl12
                / (1.0 + (sfcons.B1P + sfcons.B2P * hl12) * hl12)
            )
        else:  # hlinf < 0.05
            hl1 = -hlinf
            tem1 = 1.0 / sqrt(hl1)
            pm = log(hl1) + 2.0 * sqrt(tem1) - 0.8776
            ph = log(hl1) + 0.5 * tem1 + 1.386
            hl110 = hl1 * 10.0 * z1i
            hl110 = min(max(hl110, sfcons.ZTMIN), ztmax1)
            pm10 = log(hl110) + 2.0 / sqrt(sqrt(hl110)) - 0.8776
            hl12 = (hl1 + hl1) * z1i
            hl12 = min(max(hl12, sfcons.ZTMIN), ztmax1)
            ph2 = log(hl12) + 0.5 / sqrt(hl12) + 1.386

    # finish the exchange coefficient computation to provide fm and fh
    fm = fm - pm
    fh = fh - ph
    fm10 = fm10 - pm10
    fh2 = fh2 - ph2
    cm = sfcons.CA * sfcons.CA / (fm * fm)
    ch = sfcons.CA * sfcons.CA / (fm * fh)
    tem1 = 0.00001 / z1
    cm = max(cm, tem1)
    ch = max(ch, tem1)
    stress = cm * wind * wind
    ustar = sqrt(stress)

    return rb, fm, fh, fm10, fh2, cm, ch, stress, ustar


@gtfunction
def cal_z0_hwrf15(ws10m):
    # coded by Kun Gao (Kun.Gao@noaa.gov)
    # originally developed by URI/GFDL
    a0 = -8.367276172397277e-12
    a1 = 1.7398510865876079e-09
    a2 = -1.331896578363359e-07
    a3 = 4.507055294438727e-06
    a4 = -6.508676881906914e-05
    a5 = 0.00044745137674732834
    a6 = -0.0010745704660847233
    b0 = 2.1151080765239772e-13
    b1 = -3.2260663894433345e-11
    b2 = -3.329705958751961e-10
    b3 = 1.7648562021709124e-07
    b4 = 7.107636825694182e-06
    b5 = -0.0013914681964973246
    b6 = 0.0406766967657759

    if ws10m <= 5.0:
        z0 = 0.0185 / 9.8 * (7.59e-4 * ws10m * ws10m + 2.46e-2 * ws10m) ** 2
    elif (ws10m > 5.0) and (ws10m <= 10.0):
        z0 = 0.00000235 * (ws10m**2 - 25.0) + 3.805129199617346e-05
    elif (ws10m > 10.0) and (ws10m <= 60.0):
        z0 = (
            a6
            + a5 * ws10m
            + a4 * ws10m**2
            + a3 * ws10m**3
            + a2 * ws10m**4
            + a1 * ws10m**5
            + a0 * ws10m**6
        )
    else:
        z0 = (
            b6
            + b5 * ws10m
            + b4 * ws10m**2
            + b3 * ws10m**3
            + b2 * ws10m**4
            + b1 * ws10m**5
            + b0 * ws10m**6
        )
    return z0


@gtfunction
def cal_zt_hwrf15(ws10m):
    # coded by Kun Gao (Kun.Gao@noaa.gov)
    # originally developed by URI/GFDL
    a0 = 2.51715926619e-09
    a1 = -1.66917514012e-07
    a2 = 4.57345863551e-06
    a3 = -6.64883696932e-05
    a4 = 0.00054390175125
    a5 = -0.00239645231325
    a6 = 0.00453024927761
    b0 = -1.72935914649e-14
    b1 = 2.50587455802e-12
    b2 = -7.90109676541e-11
    b3 = -4.40976353607e-09
    b4 = 3.68968179733e-07
    b5 = -9.43728336756e-06
    b6 = 8.90731312383e-05
    c0 = 4.68042680888e-14
    c1 = -1.98125754931e-11
    c2 = 3.41357133496e-09
    c3 = -3.05130605309e-07
    c4 = 1.48243563819e-05
    c5 = -0.000367207751936
    c6 = 0.00357204479347

    if ws10m <= 7.0:
        zt = 0.0185 / 9.8 * (7.59e-4 * ws10m**2 + 2.46e-2 * ws10m) ** 2
    elif (ws10m > 7.0) and (ws10m <= 15.0):
        zt = (
            a6
            + a5 * ws10m
            + a4 * ws10m**2
            + a3 * ws10m**3
            + a2 * ws10m**4
            + a1 * ws10m**5
            + a0 * ws10m**6
        )
    elif (ws10m > 15.0) and (ws10m <= 60.0):
        zt = (
            b6
            + b5 * ws10m
            + b4 * ws10m**2
            + b3 * ws10m**3
            + b2 * ws10m**4
            + b1 * ws10m**5
            + b0 * ws10m**6
        )
    else:
        zt = (
            c6
            + c5 * ws10m
            + c4 * ws10m**2
            + c3 * ws10m**3
            + c2 * ws10m**4
            + c1 * ws10m**5
            + c0 * ws10m**6
        )
    return zt


@gtfunction
def cal_z0_hwrf17(ws10m):
    # coded by Kun Gao (Kun.Gao@noaa.gov)
    p13 = -1.296521881682694e-02
    p12 = 2.855780863283819e-01
    p11 = -1.597898515251717e00
    p10 = -8.396975715683501e00
    p25 = 3.790846746036765e-10
    p24 = 3.281964357650687e-09
    p23 = 1.962282433562894e-07
    p22 = -1.240239171056262e-06
    p21 = 1.739759082358234e-07
    p20 = 2.147264020369413e-05
    p35 = 1.840430200185075e-07
    p34 = -2.793849676757154e-05
    p33 = 1.735308193700643e-03
    p32 = -6.139315534216305e-02
    p31 = 1.255457892775006e00
    p30 = -1.663993561652530e01
    p40 = 4.579369142033410e-04

    if ws10m <= 6.5:
        z0 = exp(p10 + p11 * ws10m + p12 * ws10m**2 + p13 * ws10m**3)
    elif (ws10m > 6.5) and (ws10m <= 15.7):
        z0 = (
            p25 * ws10m**5
            + p24 * ws10m**4
            + p23 * ws10m**3
            + p22 * ws10m**2
            + p21 * ws10m
            + p20
        )
    elif (ws10m > 15.7) and (ws10m <= 53.0):
        z0 = exp(
            p35 * ws10m**5
            + p34 * ws10m**4
            + p33 * ws10m**3
            + p32 * ws10m**2
            + p31 * ws10m
            + p30
        )
    else:
        z0 = p40
    return z0


@gtfunction
def cal_zt_hwrf17(ws10m):
    # coded by Kun Gao (Kun.Gao@noaa.gov)
    p00 = 1.100000000000000e-04
    p15 = -9.144581627678278e-10
    p14 = (7.020346616456421e-08,)
    p13 = -2.155602086883837e-06
    p12 = (3.333848806567684e-05,)
    p11 = -2.628501274963990e-04
    p10 = (8.634221567969181e-04,)
    p25 = -8.654513012535990e-12
    p24 = (1.232380050058077e-09,)
    p23 = -6.837922749505057e-08
    p22 = (1.871407733439947e-06,)
    p21 = -2.552246987137160e-05
    p20 = (1.428968311457630e-04,)
    p35 = 3.207515102100162e-12
    p34 = (-2.945761895342535e-10,)
    p33 = 8.788972147364181e-09
    p32 = (-3.814457439412957e-08,)
    p31 = -2.448983648874671e-06
    p30 = (3.436721779020359e-05,)
    p45 = -3.530687797132211e-11
    p44 = (3.939867958963747e-09,)
    p43 = -1.227668406985956e-08
    p42 = (-1.367469811838390e-05,)
    p41 = 5.988240863928883e-04
    p40 = (-7.746288511324971e-03,)
    p56 = -1.187982453329086e-13
    p55 = (4.801984186231693e-11,)
    p54 = -8.049200462388188e-09
    p53 = (7.169872601310186e-07,)
    p52 = -3.581694433758150e-05
    p51 = (9.503919224192534e-04,)
    p50 = (-1.036679430885215e-02,)
    p60 = 4.751256171799112e-05

    if (ws10m >= 0.0) and (ws10m < 5.9):
        zt = p00
    elif (ws10m >= 5.9) and (ws10m <= 15.4):
        zt = p10 + ws10m * (
            p11 + ws10m * (p12 + ws10m * (p13 + ws10m * (p14 + ws10m * p15)))
        )
    elif (ws10m > 15.4) and (ws10m <= 21.6):
        zt = p20 + ws10m * (
            p21 + ws10m * (p22 + ws10m * (p23 + ws10m * (p24 + ws10m * p25)))
        )
    elif (ws10m > 21.6) and (ws10m <= 42.2):
        zt = p30 + ws10m * (
            p31 + ws10m * (p32 + ws10m * (p33 + ws10m * (p34 + ws10m * p35)))
        )
    elif (ws10m > 42.2) and (ws10m <= 53.3):
        zt = p40 + ws10m * (
            p41 + ws10m * (p42 + ws10m * (p43 + ws10m * (p44 + ws10m * p45)))
        )
    elif (ws10m > 53.3) and (ws10m <= 80.0):
        zt = p50 + ws10m * (
            p51
            + ws10m
            * (p52 + ws10m * (p53 + ws10m * (p54 + ws10m * (p55 + ws10m * p56))))
        )
    elif ws10m > 80.0:
        zt = p60
    return zt


@gtfunction
def cal_z0_moon(ws10m):
    # coded by Kun Gao (Kun.Gao@noaa.gov)
    charnock = 0.014
    wind_th_moon = 20.0
    a = 0.56
    b = -20.255
    c = wind_th_moon - 2.458

    ustar_th = (-b - sqrt(b * b - 4 * a * c)) / (2 * a)

    z0_adj = (
        0.001 * (0.085 * wind_th_moon - 0.58)
        - (charnock / constants.GRAV) * ustar_th * ustar_th
    )

    z0 = (
        0.001 * (0.085 * ws10m - 0.58) - z0_adj
    )  # Eq(8b) Moon et al. 2007 modified by kgao
    return z0


def sfc_diff(
    u1: FloatFieldIJ,
    v1: FloatFieldIJ,
    t1: FloatFieldIJ,
    qvapor: FloatFieldIJ,
    ddvel: FloatFieldIJ,
    tsurf: FloatFieldIJ,
    tskin: FloatFieldIJ,
    prslki: FloatFieldIJ,
    prsl1: FloatFieldIJ,
    z0rl: FloatFieldIJ,
    z1: FloatFieldIJ,
    shdmax: FloatFieldIJ,
    sigmaf: FloatFieldIJ,
    ustar: FloatFieldIJ,
    snwdph: FloatFieldIJ,
    ztrl: FloatFieldIJ,
    cm: FloatFieldIJ,
    ch: FloatFieldIJ,
    rb: FloatFieldIJ,
    stress: FloatFieldIJ,
    fm: FloatFieldIJ,
    fh: FloatFieldIJ,
    wind: FloatFieldIJ,
    fm10: FloatFieldIJ,
    fh2: FloatFieldIJ,
    islimsk: IntFieldIJ,
    vegtype: IntFieldIJ,
    flag_iter: BoolFieldIJ,
):
    """
    Probably want to split this into functions and rename a bunch
    """
    from __externals__ import (
        do_z0_hwrf15,
        do_z0_hwrf17,
        do_z0_hwrf17_hwonly,
        do_z0_moon,
        ivegsrc,
        redrag,
        wind_th_hwrf,
        z0s_max,
    )

    with computation(FORWARD), interval(0, 1):

        if flag_iter:
            # Get lowest atmospheric level variables:
            wind = max(sqrt(u1 * u1 + v1 * v1) + max(0.0, min(ddvel, 30.0)), 1.0)
            tem1 = 1.0 + constants.ZVIR * max(qvapor[0, 0], 1.0e-8)
            thv1 = t1 * prslki * tem1
            tvs = 0.5 * (tsurf + tskin) * tem1
            qs1 = fpvsx(t1)
            qs1 = max(1.0e-8, constants.EPS * qs1 / (prsl1 + constants.EPSM1 * qs1))

            # Get surface level variables:
            if (islimsk[0, 0] == 1) or (islimsk[0, 0] == 2):  # Over land or sea ice:
                # get surface roughness for momentum (z0)
                z0 = 0.01 * z0rl
                z0max = max(1.0e-6, min(z0, z1))
                # xubin's new z0  over land and sea ice
                tem1 = 1.0 - shdmax  # shdmax is max vegetation area fraction
                tem2 = tem1 * tem1
                tem1 = 1.0 - tem2

                if ivegsrc == 1:
                    if vegtype == 10:
                        z0max = exp(tem2 * log(0.01) + tem1 * log(0.07))
                    elif vegtype == 6:
                        z0max = exp(tem2 * log(0.01) + tem1 * log(0.05))
                    elif vegtype == 7:
                        z0max = 0.01
                    elif vegtype == 16:
                        z0max = 0.01
                    else:
                        z0max = exp(tem2 * log(0.01) + tem1 * log(z0max))
                elif ivegsrc == 2:
                    if vegtype == 7:
                        z0max = exp(tem2 * log(0.01) + tem1 * log(0.07))
                    elif vegtype == 8:
                        z0max = exp(tem2 * log(0.01) + tem1 * log(0.05))
                    elif vegtype == 9:
                        z0max = 0.01
                    elif vegtype == 11:
                        z0max = 0.01
                    else:
                        z0max = exp(tem2 * log(0.01) + tem1 * log(z0max))

                z0max = max(z0max, 1.0e-6)

                # get surface roughness for heat (zt)
                czilc = 0.8

                tem1 = 1.0 - sigmaf
                ztmax = z0max * exp(
                    -tem1 * tem1 * czilc * sfcons.CA * sqrt(ustar * (0.01 / 1.5e-05))
                )

                ztmax = max(ztmax, 1.0e-6)

                # call similarity

                rb, fm, fh, fm10, fh2, cm, ch, stress, ustar = monin_obukhov_similarity(
                    z1, snwdph, thv1, wind, z0max, ztmax, tvs
                )

            elif islimsk == 0:  # over water

                # if over water (redesigned by Kun Gao)
                # iteration 1
                #     step 1 get z0/zt from previous step
                #     step 2 call similarity
                # iteration 2
                #     step 1 update z0/zt
                #     step 2 call similarity

                # iteration 1
                # get z0/zt
                z0 = 0.01 * z0rl
                zt = 0.01 * ztrl

                z0max = max(1.0e-6, min(z0, z1))
                ztmax = max(zt, 1.0e-6)

                rb, fm, fh, fm10, fh2, cm, ch, stress, ustar = monin_obukhov_similarity(
                    z1, snwdph, thv1, wind, z0max, ztmax, tvs
                )

                # iteration 2
                # get z0/zt following the old sfc_diff.f
                z0 = (sfcons.CHARNOCK / constants.GRAV) * ustar * ustar
                if redrag:
                    z0 = max(min(z0, z0s_max), 1.0e-7)
                else:
                    z0 = max(min(z0, 0.1), 1.0e-7)

                ustar_1 = sqrt(constants.GRAV * z0 / sfcons.CHARNOCK)
                restar = max(ustar_1 * z0max * sfcons.VISI, 0.000001)
                rat = min(7.0, 2.67 * sqrt(sqrt(restar)) - 2.57)
                zt = z0max * exp(-rat)  # zeng, zhao and dickinson 1997 (eq 25)

                # update z0/zt with new options
                # only z0 options in the following
                # will add zt options in the future
                u10m = u1 * fm10 / fm
                v10m = v1 * fm10 / fm
                ws10m = sqrt(u10m * u10m + v10m * v10m)

                if do_z0_hwrf15:
                    # option 1: HWRF15, originally developed by URI/GFDL
                    z0 = cal_z0_hwrf15(ws10m)
                    zt = cal_zt_hwrf15(ws10m)

                elif do_z0_hwrf17:
                    # option 2: HWRF17
                    z0 = cal_z0_hwrf17(ws10m)
                    zt = cal_zt_hwrf17(ws10m)

                elif do_z0_hwrf17_hwonly:
                    # option 3: HWRF17 under high wind only
                    if ws10m > wind_th_hwrf:
                        z0 = cal_z0_hwrf17(ws10m)
                        z0 = max(min(z0, z0s_max), 1.0e-7)  # must apply limiter here

                elif do_z0_moon:
                    # option 4: Moon et al 2007 under high winds (same as in HiRAM)
                    ws10m_moon = 2.458 + ustar * (
                        20.255 - 0.56 * ustar
                    )  # Eq(7) Moon et al. 2007
                    if ws10m_moon > 20.0:
                        z0 = cal_z0_moon(ws10m_moon)
                        z0 = max(min(z0, z0s_max), 1.0e-7)  # must apply limiter here

                z0max = max(z0, 1.0e-6)
                ztmax = max(zt, 1.0e-6)

                rb, fm, fh, fm10, fh2, cm, ch, stress, ustar = monin_obukhov_similarity(
                    z1, snwdph, thv1, wind, z0max, ztmax, tvs
                )

                z0rl = 100.0 * z0max
                ztrl = 100.0 * ztmax


class SurfaceExchange:
    def __init__(
        self,
        stencil_factory: StencilFactory,
        ivegsrc: Int,
        do_z0_hwrf15: Bool,
        do_z0_hwrf17: Bool,
        do_z0_hwrf17_hwonly: Bool,
        do_z0_moon: Bool,
        redrag: Bool,
        wind_th_hwrf: Float,
    ):
        """
        Calculates surface exchanges and near-surface winds.
        Fortran name is sfc_diff_gfdl
        """
        # TODO: This should be an enum in the config and only one gets passed here
        assert (
            sum(
                [
                    do_z0_hwrf15,
                    do_z0_hwrf17,
                    do_z0_hwrf17_hwonly,
                    do_z0_moon,
                ]
            )
            == 1
        ), "sfc_diff: exactly one ocean surface option must be enabled"
        grid_indexing = stencil_factory.grid_indexing
        origin, domain2d = grid_indexing.get_2d_compute_origin_domain()
        self._sfc_diff = stencil_factory.from_origin_domain(
            sfc_diff,
            externals={
                "do_z0_hwrf15": bool(do_z0_hwrf15),
                "do_z0_hwrf17": bool(do_z0_hwrf17),
                "do_z0_hwrf17_hwonly": bool(do_z0_hwrf17_hwonly),
                "do_z0_moon": bool(do_z0_moon),
                "ivegsrc": ivegsrc,
                "redrag": bool(redrag),
                "wind_th_hwrf": wind_th_hwrf,
                "z0s_max": sfcons.Z0S_MAX,
            },
            origin=origin,
            domain=domain2d,
        )

    def __call__(
        self,
        u1: FloatFieldIJ,
        v1: FloatFieldIJ,
        t1: FloatFieldIJ,
        qvapor: FloatFieldIJ,
        ddvel: FloatFieldIJ,
        tsurf: FloatFieldIJ,
        tsfc: FloatFieldIJ,
        prslki: FloatFieldIJ,
        prsl1: FloatFieldIJ,
        z0rl: FloatFieldIJ,
        z1: FloatFieldIJ,
        shdmax: FloatFieldIJ,
        sigmaf: FloatFieldIJ,
        ustar: FloatFieldIJ,
        snowdepth: FloatFieldIJ,
        ztrl: FloatFieldIJ,
        cm: FloatFieldIJ,
        ch: FloatFieldIJ,
        rb: FloatFieldIJ,
        stress: FloatFieldIJ,
        fm: FloatFieldIJ,
        fh: FloatFieldIJ,
        wind: FloatFieldIJ,
        fm10: FloatFieldIJ,
        fh2: FloatFieldIJ,
        islimsk: IntFieldIJ,
        vegtype: IntFieldIJ,
        flag_iter: BoolFieldIJ,
    ):
        self._sfc_diff(
            u1,
            v1,
            t1,
            qvapor,
            ddvel,
            tsurf,
            tsfc,
            prslki,
            prsl1,
            z0rl,
            z1,
            shdmax,
            sigmaf,
            ustar,
            snowdepth,
            ztrl,
            cm,
            ch,
            rb,
            stress,
            fm,
            fh,
            wind,
            fm10,
            fh2,
            islimsk,
            vegtype,
            flag_iter,
        )
