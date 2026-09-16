import ndsl.constants as constants
from ndsl.dsl.gt4py import PARALLEL, computation, interval
from ndsl.dsl.typing import FloatField, IntFieldIJ

RE_LIQ = 10.0
"""Default liquid radius in microns"""
RE_ICE = 50.0
"""Default ice radius in microns"""
RE_SNOW = 250.0
"""Default snow radius in microns"""
RE_RAIN = 1000.0
"""Default rain radius in microns"""
PTOP_C = [[1050.0, 650.0, 400.0, 0.0], [1050.0, 750.0, 500.0, 0.0]]
"""Top pressure of cloud domains for sfc, low, medium, and high (j = 0-3)
in low latitudes and poles (i = 0, 1)"""
PTOP_DIFF = [PTOP_C[1][jj] - PTOP_C[0][jj] for jj in range(len(PTOP_C[0]))]
GFAC = 1.0e5 / constants.GRAV
CLIMIT = 0.001
CLIMIT2 = 0.05
OVCST = 1.0 - 1.0e-8
GORD = constants.GRAV / constants.RDGAS
EPSQ = 1.0e-12


def cld_init(sigma, ivflip):
    """
    Calculates the top of BL cld (llyr), which is the topmost non-cld(low)
    layer for stratiform (at or above lowest 0.1 of theatmosphere).
    """
    llyr = 0
    if ivflip == 0:  # data from toa to sfc
        for k in range(len(sigma) - 1, 0, -1):
            kl = k
            if sigma[k] < 0.9e0:
                break
        llyr = kl
    else:  # data from sfc to top
        for k in range(1, len(sigma)):
            kl = k
            if sigma[k] < 0.9e0:
                break
        llyr = kl - 1
    return llyr


def progcld4(
    plyr: FloatField,
    plvl: FloatField,
    tlyr: FloatField,
    tvly: FloatField,
    clw: FloatField,
    land_mask: IntFieldIJ,
    cldtot: FloatField,
    cwp: FloatField,
    rew: FloatField,
    cip: FloatField,
    rei: FloatField,
):
    """
    Calculates prognostic cloud properties for all-sky radiation calculations.
    This is a stripped-down version of the original Fortran code because we need
    fewer parameters for RTE-RRTMGP.
    Inputs:
        plyr: layer-mean pressure,
        plvl: level (interface) pressure,
        tlyr: layer-mean temperature,
        tvly: layer-mean virtual temperature,
        clw: layer cloud condensate amount,
        land_mask: sea/land/ice mask array (sea:0,land:1,sea-ice:2)

    Outputs:
        cldtot: layer total cloud fraction,
        cwp: layer cloud liq water path (g/m**2),
        rew: mean eff radius for liq cloud (micron),
        cip: layer cloud ice water path (g/m**2),
        rei: mean eff radius for ice cloud (micron),

    Original Fortran docstring follows:
    ! =================   subprogram documentation block   ================ !
    !                                                                       !
    ! subprogram:    progcld1    computes cloud related quantities using    !
    !   zhao/moorthi's prognostic cloud microphysics scheme.                !
    !                                                                       !
    ! abstract:  this program computes cloud fractions from cloud           !
    !   condensates, calculates liquid/ice cloud droplet effective radius,  !
    !   and computes the low, mid, high, total and boundary layer cloud     !
    !   fractions and the vertical indices of low, mid, and high cloud      !
    !   top and base.  the three vertical cloud domains are set up in the   !
    !   initial subroutine "cld_init".                                      !
    !                                                                       !
    ! usage:         call progcld1                                          !
    !                                                                       !
    ! subprograms called:   gethml                                          !
    !                                                                       !
    ! attributes:                                                           !
    !   language:   fortran 90                                              !
    !   machine:    ibm-sp, sgi                                             !
    !                                                                       !
    !                                                                       !
    !  ====================  definition of variables  ====================  !
    !                                                                       !
    ! input variables:                                                      !
    !   plyr  (IX,NLAY) : model layer mean pressure in mb (100Pa)           !
    !   plvl  (IX,NLP1) : model level pressure in mb (100Pa)                !
    !   tlyr  (IX,NLAY) : model layer mean temperature in k                 !
    !   tvly  (IX,NLAY) : model layer virtual temperature in k              !
    !   qlyr  (IX,NLAY) : layer specific humidity in gm/gm                  !
    !   qstl  (IX,NLAY) : layer saturate humidity in gm/gm                  !
    !   rhly  (IX,NLAY) : layer relative humidity (=qlyr/qstl)              !
    !   clw   (IX,NLAY) : layer cloud condensate amount                     !
    !   xlat  (IX)      : grid latitude in radians, default to pi/2 -> -pi/2!
    !                     range, otherwise see in-line comment              !
    !   xlon  (IX)      : grid longitude in radians  (not used)             !
    !   slmsk (IX)      : sea/land mask array (sea:0,land:1,sea-ice:2)      !
    !   IX              : horizontal dimention                              !
    !   NLAY,NLP1       : vertical layer/level dimensions                   !
    !                                                                       !
    ! output variables:                                                     !
    !   clouds(IX,NLAY,NF_CLDS) : cloud profiles                            !
    !      clouds(:,:,1) - layer total cloud fraction                       !
    !      clouds(:,:,2) - layer cloud liq water path         (g/m**2)      !
    !      clouds(:,:,3) - mean eff radius for liq cloud      (micron)      !
    !      clouds(:,:,4) - layer cloud ice water path         (g/m**2)      !
    !      clouds(:,:,5) - mean eff radius for ice cloud      (micron)      !
    !      clouds(:,:,6) - layer rain drop water path         not assigned  !
    !      clouds(:,:,7) - mean eff radius for rain drop      (micron)      !
    !  *** clouds(:,:,8) - layer snow flake water path        not assigned  !
    !      clouds(:,:,9) - mean eff radius for snow flake     (micron)      !
    !  *** fu's scheme need to be normalized by snow density (g/m**3/1.0e6) !
    !   clds  (IX,5)    : fraction of clouds for low, mid, hi, tot, bl      !
    !   mtop  (IX,3)    : vertical indices for low, mid, hi cloud tops      !
    !   mbot  (IX,3)    : vertical indices for low, mid, hi cloud bases     !
    !                                                                       !
    ! module variables:                                                     !
    !   ivflip          : control flag of vertical index direction          !
    !                     =0: index from toa to surface                     !
    !                     =1: index from surface to toa                     !
    !   lsashal         : control flag for shallow convection               !
    !   lcrick          : control flag for eliminating CRICK                !
    !                     =t: apply layer smoothing to eliminate CRICK      !
    !                     =f: do not apply layer smoothing                  !
    !   lcnorm          : control flag for in-cld condensate                !
    !                     =t: normalize cloud condensate                    !
    !                     =f: not normalize cloud condensate                !
    !                                                                       !
    !  ====================    end of description    =====================  !
    """
    from __externals__ import ivflip, lcnorm, lcrick

    with computation(PARALLEL), interval(0, -1):
        # Initialize everything
        cwp = 0.0
        rew = RE_LIQ
        cip = 0.0
        rei = RE_ICE
        clwf = 0.0
        tem_2d = min(1.0, max(0.0, (constants.TTP - tlyr) * 0.5))

    with computation(PARALLEL):
        with interval(0, 1):
            # Set up clwf
            if lcrick:
                clwf = 0.75 * clw + 0.25 * clw[0, 0, 1]
            else:
                clwf = clw
        with interval(1, -2):
            if lcrick:
                clwf = 0.25 * clw[0, 0, -1] + 0.5 * clw + 0.25 * clw[0, 0, 1]
            else:
                clwf = clw
        with interval(-2, -1):
            if lcrick:
                clwf = 0.75 * clw + 0.25 * clw[0, 0, -1]
            else:
                clwf = clw
    with computation(PARALLEL), interval(0, -1):
        # Initialize rain/snow condensate path
        crp = 0.0
        csp = 0.0
        # Compute liquid/ice condensate path in g/m**2
        if ivflip == 0:  # input data from TOA to sfc
            delp = plvl[0, 0, 1] - plvl
            clwt = max(0.0, clwf * GFAC * delp)
            cip = clwt * tem_2d
            cwp = clwt - cip
        else:
            delp = plvl - plvl[0, 0, 1]
            clwt = max(0.0, clwf) * GFAC * delp
            cip = clwt * tem_2d
            cwp = clwt - cip
    with computation(PARALLEL), interval(0, -1):
        # Effective liquid cloud droplet radius over land
        if land_mask == 1:
            rew = 5.0 + 5.0 * tem_2d
        if cldtot < CLIMIT:
            cwp = 0.0
            cip = 0.0
            crp = 0.0
            csp = 0.0
        if lcnorm:
            if cldtot >= CLIMIT:
                tem1 = 1.0 / max(CLIMIT2, cldtot)
                cwp = cwp * tem1
                cip = cip * tem1
                crp = crp * tem1
                csp = csp * tem1

        # Effective ice cloud droplet radius
        tem2 = tlyr - constants.TTP
        if cip > 0.0:
            tem3 = GORD * cip * plyr / (delp * tvly)
            if tem2 < -50.0:
                rei = (1250.0 / 9.917) * tem3**0.109
            elif tem2 < -40.0:
                rei = (1250.0 / 9.337) * tem3**0.08
            elif tem2 < -30.0:
                rei = (1250.0 / 9.208) * tem3**0.055
            else:
                rei = (1250.0 / 9.387) * tem3**0.031
            rei = max(10.0, min(rei, 150.0))


def progcld5(
    plyr: FloatField,
    plvl: FloatField,
    tlyr: FloatField,
    tvly: FloatField,
    clw: FloatField,
    cnvw: FloatField,
    cnvc: FloatField,
    land_mask: IntFieldIJ,
    cldtot: FloatField,
    cwp: FloatField,
    rew: FloatField,
    cip: FloatField,
    rei: FloatField,
):
    """
    Calculates prognostic cloud properties for all-sky radiation calculations.
    This is a stripped-down version of the original Fortran code because we need
    fewer parameters for RTE-RRTMGP.
    Inputs:
        plyr: layer-mean pressure,
        plvl: level (interface) pressure,
        tlyr: layer-mean temperature,
        tvly: layer-mean virtual temperature,
        clw: layer cloud condensate amount,
        cnvw: layer convective cloud condensate,
        cnvc: layer convective cloud cover,
        land_mask: sea/land/ice mask array (sea:0,land:1,sea-ice:2)

    Outputs:
        cldtot: layer total cloud fraction,
        cwp: layer cloud liq water path (g/m**2),
        rew: mean eff radius for liq cloud (micron),
        cip: layer cloud ice water path (g/m**2),
        rei: mean eff radius for ice cloud (micron),

    Original Fortran docstring follows:
    ! =================   subprogram documentation block   ================ !
    !                                                                       !
    ! subprogram:    progcld1    computes cloud related quantities using    !
    !   zhao/moorthi's prognostic cloud microphysics scheme.                !
    !                                                                       !
    ! abstract:  this program computes cloud fractions from cloud           !
    !   condensates, calculates liquid/ice cloud droplet effective radius,  !
    !   and computes the low, mid, high, total and boundary layer cloud     !
    !   fractions and the vertical indices of low, mid, and high cloud      !
    !   top and base.  the three vertical cloud domains are set up in the   !
    !   initial subroutine "cld_init".                                      !
    !                                                                       !
    ! usage:         call progcld1                                          !
    !                                                                       !
    ! subprograms called:   gethml                                          !
    !                                                                       !
    ! attributes:                                                           !
    !   language:   fortran 90                                              !
    !   machine:    ibm-sp, sgi                                             !
    !                                                                       !
    !                                                                       !
    !  ====================  definition of variables  ====================  !
    !                                                                       !
    ! input variables:                                                      !
    !   plyr  (IX,NLAY) : model layer mean pressure in mb (100Pa)           !
    !   plvl  (IX,NLP1) : model level pressure in mb (100Pa)                !
    !   tlyr  (IX,NLAY) : model layer mean temperature in k                 !
    !   tvly  (IX,NLAY) : model layer virtual temperature in k              !
    !   qlyr  (IX,NLAY) : layer specific humidity in gm/gm                  !
    !   qstl  (IX,NLAY) : layer saturate humidity in gm/gm                  !
    !   rhly  (IX,NLAY) : layer relative humidity (=qlyr/qstl)              !
    !   clw   (IX,NLAY) : layer cloud condensate amount                     !
    !   cnvw  (ix,nlay) : layer convective cloud condensate                 !
    !   cnvc  (ix,nlay) : layer convective cloud cover                      !
    !   xlat  (IX)      : grid latitude in radians, default to pi/2 -> -pi/2!
    !                     range, otherwise see in-line comment              !
    !   xlon  (IX)      : grid longitude in radians  (not used)             !
    !   slmsk (IX)      : sea/land mask array (sea:0,land:1,sea-ice:2)      !
    !   IX              : horizontal dimention                              !
    !   NLAY,NLP1       : vertical layer/level dimensions                   !
    !                                                                       !
    ! output variables:                                                     !
    !   clouds(IX,NLAY,NF_CLDS) : cloud profiles                            !
    !      clouds(:,:,1) - layer total cloud fraction                       !
    !      clouds(:,:,2) - layer cloud liq water path         (g/m**2)      !
    !      clouds(:,:,3) - mean eff radius for liq cloud      (micron)      !
    !      clouds(:,:,4) - layer cloud ice water path         (g/m**2)      !
    !      clouds(:,:,5) - mean eff radius for ice cloud      (micron)      !
    !      clouds(:,:,6) - layer rain drop water path         not assigned  !
    !      clouds(:,:,7) - mean eff radius for rain drop      (micron)      !
    !  *** clouds(:,:,8) - layer snow flake water path        not assigned  !
    !      clouds(:,:,9) - mean eff radius for snow flake     (micron)      !
    !  *** fu's scheme need to be normalized by snow density (g/m**3/1.0e6) !
    !   clds  (IX,5)    : fraction of clouds for low, mid, hi, tot, bl      !
    !   mtop  (IX,3)    : vertical indices for low, mid, hi cloud tops      !
    !   mbot  (IX,3)    : vertical indices for low, mid, hi cloud bases     !
    !                                                                       !
    ! module variables:                                                     !
    !   ivflip          : control flag of vertical index direction          !
    !                     =0: index from toa to surface                     !
    !                     =1: index from surface to toa                     !
    !   lsashal         : control flag for shallow convection               !
    !   lcrick          : control flag for eliminating CRICK                !
    !                     =t: apply layer smoothing to eliminate CRICK      !
    !                     =f: do not apply layer smoothing                  !
    !   lcnorm          : control flag for in-cld condensate                !
    !                     =t: normalize cloud condensate                    !
    !                     =f: not normalize cloud condensate                !
    !                                                                       !
    !  ====================    end of description    =====================  !
    """
    from __externals__ import gfs_cloud_overlap, ivflip, lcnorm, lcrick

    with computation(PARALLEL), interval(0, -1):
        # Initialize everything
        cldtot = 0.0
        cwp = 0.0
        rew = RE_LIQ
        cip = 0.0
        rei = RE_ICE
        clwf = 0.0
        tem_2d = min(1.0, max(0.0, (constants.TTP - tlyr) * 0.5))

    with computation(PARALLEL):
        with interval(0, 1):
            # Set up clwf
            if lcrick:
                clwf = 0.75 * clw + 0.25 * clw[0, 0, 1]
            else:
                clwf = clw + cnvw
        with interval(1, -2):
            if lcrick:
                clwf = 0.25 * clw[0, 0, -1] + 0.5 * clw + 0.25 * clw[0, 0, 1]
            else:
                clwf = clw + cnvw
        with interval(-2, -1):
            if lcrick:
                clwf = 0.75 * clw + 0.25 * clw[0, 0, -1]
            else:
                clwf = clw + cnvw
    with computation(PARALLEL), interval(0, -1):
        # Compute liquid/ice condensate path in g/m**2
        if ivflip == 0:  # input data from TOA to sfc
            delp = plvl[0, 0, 1] - plvl
            clwt = max(0.0, clwf * GFAC * delp)
            cip = clwt * tem_2d
            cwp = clwt - cip
            if not gfs_cloud_overlap:
                cldtot = cnvc + (1 - cnvc) * cldtot
                cldtot = max(cldtot, 0.0)
                cldtot = min(cldtot, 1.0)
        else:
            delp = plvl - plvl[0, 0, 1]
            clwt = max(0.0, clwf) * GFAC * delp
            cip = clwt * tem_2d
            cwp = clwt - cip
            if not gfs_cloud_overlap:
                cldtot = cnvc + (1 - cnvc) * cldtot
                cldtot = max(cldtot, 0.0)
                cldtot = min(cldtot, 1.0)
    with computation(PARALLEL), interval(0, -1):
        # Effective liquid cloud droplet radius over land
        if land_mask == 1:
            rew = 5.0 + 5.0 * tem_2d
        if cldtot < CLIMIT:
            cwp = 0.0
            cip = 0.0
            crp = 0.0
            csp = 0.0
        if lcnorm:
            if cldtot >= CLIMIT:
                tem1 = 1.0 / max(CLIMIT2, cldtot)
                cwp = cwp * tem1
                cip = cip * tem1
                crp = crp * tem1
                csp = csp * tem1

        # Effective ice cloud droplet radius
        tem2 = tlyr - constants.TTP
        if cip > 0.0:
            tem3 = GORD * cip * plyr / (delp * tvly)
            if tem2 < -50.0:
                rei = (1250.0 / 9.917) * tem3**0.109
            elif tem2 < -40.0:
                rei = (1250.0 / 9.337) * tem3**0.08
            elif tem2 < -30.0:
                rei = (1250.0 / 9.208) * tem3**0.055
            else:
                rei = (1250.0 / 9.387) * tem3**0.031
            # rei = max(20.0, min(rei, 300.0))
            # rei = max(10.0, min(rei, 100.0))
            rei = max(10.0, min(rei, 150.0))
            # rei = max(5.0,  min(rei, 130.0))
