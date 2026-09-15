import ndsl.constants as constants
import pyshield.constants as physcons
from ndsl.dsl.gt4py import exp, floor
from ndsl.dsl.gt4py import function as gtfunction
from ndsl.dsl.gt4py import max, min


@gtfunction
def fpvsx(t):
    """
    Computes saturation water vapor pressure, adapted from Fortran:
    ! Subprogram: fpvsx        Compute saturation vapor pressure
    !   Author: N Phillips            w/NMC2X2   Date: 30 dec 82
    !
    ! Abstract: Exactly compute saturation vapor pressure from temperature.
    !   The saturation vapor pressure over either liquid and ice is computed
    !   over liquid for temperatures above the triple point,
    !   over ice for temperatures 20 degress below the triple point,
    !   and a linear combination of the two for temperatures in between.
    !   The water model assumes a perfect gas, constant specific heats
    !   for gas, liquid and ice, and neglects the volume of the condensate.
    !   The model does account for the variation of the latent heat
    !   of condensation and sublimation with temperature.
    !   The Clausius-Clapeyron equation is integrated from the triple point
    !   to get the formula
    !       pvsl=con_psat*(tr**xa)*exp(xb*(1.-tr))
    !   where tr is ttp/t and other values are physical constants.
    !   The reference for this computation is Emanuel(1994), pages 116-117.
    !   This function should be expanded inline in the calling routine.
    !
    ! Program History Log:
    !   91-05-07  Iredell             made into inlinable function
    !   94-12-30  Iredell             exact computation
    ! 1999-03-01  Iredell             f90 module
    ! 2001-02-26  Iredell             ice phase
    ! 2024-07-09  Oelbert             Python version
    !
    ! Usage:   pvs=fpvsx(t)
    !
    !   Input argument list:
    !     t          Real(krealfp) temperature in Kelvin
    !
    !   Output argument list:
    !     fpvsx      Real(krealfp) saturation vapor pressure in Pascals
    """
    tliq = constants.TTP
    tice = constants.TTP - 20.0
    dldtl = constants.CP_VAP - physcons.CPH2O1
    heatl = constants.HLV
    xponal = -dldtl / constants.RVGAS
    xponbl = -dldtl / constants.RVGAS + heatl / (constants.RVGAS * constants.TTP)
    dldti = constants.CP_VAP - constants.C_ICE_0
    heati = constants.HLV + constants.HLF
    xponai = -dldti / constants.RVGAS
    xponbi = -dldti / constants.RVGAS + heati / (constants.RVGAS * constants.TTP)

    tr = constants.TTP / t

    fpvsx = 0.0
    if t >= tliq:
        fpvsx = constants.PSAT * (tr**xponal) * exp(xponbl * (1.0 - tr))
    elif t < tice:
        fpvsx = constants.PSAT * (tr**xponai) * exp(xponbi * (1.0 - tr))
    else:
        w = (t - tice) / (tliq - tice)
        pvl = constants.PSAT * (tr**xponal) * exp(xponbl * (1.0 - tr))
        pvi = constants.PSAT * (tr**xponai) * exp(xponbi * (1.0 - tr))
        fpvsx = w * pvl + (1.0 - w) * pvi

    return fpvsx


@gtfunction
def fpvs(t):
    xmin = 180.0
    xmax = 330.0
    nxpvs = 7501
    xinc = (xmax - xmin) / (nxpvs - 1)
    c2xpvs = 1.0 / xinc
    c1xpvs = 1.0 - (xmin * c2xpvs)

    xj = min(max(c1xpvs + c2xpvs * t, 1.0), nxpvs)
    jx = min(xj, nxpvs - 1.0)
    jx = floor(jx)

    x = xmin + (jx * xinc)
    xm = xmin + ((jx - 1) * xinc)

    fpvs = fpvsx(xm) + (xj - jx) * (fpvsx(x) - fpvsx(xm))

    return fpvs
