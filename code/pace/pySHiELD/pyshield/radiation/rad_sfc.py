from pathlib import Path

import numpy as np

import ndsl.constants as constants
import pyshield.constants as physcons
from ndsl.dsl.typing import Float, Int
from ndsl.logging import ndsl_log

LAND_ALBEDO = 0.25
OCEAN_ALBEDO = 0.06
ICE_ALBEDO = 0.65
CONST_EMS = 0.97
EMS_REF = [0.97, 0.95, 0.94, 0.90, 0.93, 0.96, 0.96, 0.99]
IMXEMS = 360
JMXEMS = 180


def map_sfc_to_grid(
    sfc_data: np.ndarray,
    gridlon: np.ndarray,
    gridlat: np.ndarray,
) -> np.ndarray:
    """
    Takes an input array and assigns values to the grid resolution based on lon/lat.
    note: this is a simple mapping method, an upgrade is needed if
    the model grid is much coarser than the 1-deg data resolution.
    """
    sfc_grid_data = np.zeros_like(gridlon)
    nx = gridlon.shape[0]
    ny = gridlon.shape[1]
    degres_i = sfc_data.shape[0] / 360.0
    degres_j = sfc_data.shape[1] / 180.0
    for i in range(sfc_grid_data.shape[0]):
        for j in range(sfc_grid_data.shape[1]):
            lon = (
                gridlon[i, j]
                if gridlon[i, j] >= 0
                else gridlon[i, j] + (2.0 * constants.PI)
            )
            lat = 0.5 * constants.PI - gridlat[i, j]
            i2 = int((lon * (180.0 / constants.PI)) // degres_i)
            j2 = int((lat * (180.0 / constants.PI)) // degres_j)
            sfc_grid_data[i, j] = sfc_data[i2, j2]
    return sfc_grid_data


def sfc_init(
    ialbflg: Int,
    iemsflg: Int,
    ldisable_radiation_quasi_sea_ice: bool,
    sfcemis_datafile: Path,
):
    # Initialization of surface albedo section
    # physparam::ialbflg
    # - -2: using prescribed ocean, land, ice albedos for SW
    # - -1: using constant albedo for SW
    # -  0: using climatology surface albedo scheme for SW
    # -  1: using MODIS based land surface albedo for SW
    # -  2: using land surface model albedo for SW
    if ialbflg == 0:
        ndsl_log.info("Using climatology surface albedo scheme for sw")
    elif ialbflg == 1:
        ndsl_log.info("Using MODIS based land surface albedo for sw")
    elif ialbflg == 2:
        ndsl_log.info("Using Albedo From Land Model")
    elif ialbflg == -1:
        ndsl_log.info(f"Using Constant Albedo {OCEAN_ALBEDO}")
    elif ialbflg == -2:
        ndsl_log.info(
            "Using Prescribed Ocean, Land, Ice Albedos: "
            f"{OCEAN_ALBEDO, LAND_ALBEDO, ICE_ALBEDO}"
        )
    else:
        raise ValueError(f"ialbflg must be -2, -1, 0, 1, or 2, got {ialbflg}")
    # physparam::ldisable_radiation_quasi_sea_ice
    # - = .false.: use a sea-ice-like albedo and emissivity for below
    #     freezing ocean grid cells.
    # - = .true.:  treat all ocean grid cells as if they were
    #     above freezing when determing the albedo and emissivity
    if ldisable_radiation_quasi_sea_ice:
        ndsl_log.info("Disabling radiation quasi-sea-ice")
    else:
        ndsl_log.info("Enable radiation quasi-sea-ice")

    # Initialization of surface emissivity section
    # physparam::iemsflg
    # - = 0: fixed SFC emissivity at 1.0
    # - = 1: input SFC emissivity type map from "semis_file"
    # - = 2: using SFC emissivity from land model
    ext_sfcemis_data = None
    iemslw = iemsflg % 10  # emissivity control
    if iemslw == 0:
        ndsl_log.info(f"Using Fixed Surface Emissivity {CONST_EMS} for lw")
    elif iemslw == 1:
        ndsl_log.info(
            f"Using Varying Surface Emissivity for lw from {sfcemis_datafile}"
        )
        if not sfcemis_datafile.is_file():
            raise FileExistsError(f"{sfcemis_datafile} does not exist")
        ext_sfcemis_data = np.genfromtxt(
            sfcemis_datafile, dtype=int, delimiter=1, skip_header=1
        ).reshape(IMXEMS, JMXEMS)
    elif iemslw == 2:
        ndsl_log.info("Using Surface Emissivity From Land Model")
    else:
        raise ValueError(f"iemslw must be 0, 1, or 2, got {iemslw}")
    return iemslw, ext_sfcemis_data


def set_albedo(
    ialbflg: Int,
    islmsk: np.ndarray,
    snowf: np.ndarray,
    sncovr: np.ndarray,
    snoalb: np.ndarray,
    zorlf: np.ndarray,
    coszf: np.ndarray,
    tsknf: np.ndarray,
    hprif: np.ndarray,
    alvsf: np.ndarray,
    alnsf: np.ndarray,
    alvwf: np.ndarray,
    alnwf: np.ndarray,
    facsf: np.ndarray,
    facwf: np.ndarray,
    fice: np.ndarray,
    tisfc: np.ndarray,
    lsmalbedo: np.ndarray,
    sfcalb: np.ndarray,
    ldisable_radiation_quasi_sea_ice: bool,
):
    """
    !  ===================================================================  !
    !                                                                       !
    !  this program computes four components of surface albedos (i.e.       !
    !  vis-nir, direct-diffused) according to controflag ialbflg.           !
    !   1) climatological surface albedo scheme (briegleb 1992)             !
    !   2) modis retrieval based scheme from boston univ.                   !
    !                                                                       !
    !                                                                       !
    ! usage:         call setalb                                            !
    !                                                                       !
    ! subprograms called:  none                                             !
    !                                                                       !
    !  ====================  defination of variables  ====================  !
    !                                                                       !
    !  inputs:                                                              !
    !     slmsk (IMAX)  - sea(0),land(1),ice(2) mask on fcst model grid     !
    !     snowf (IMAX)  - snow depth water equivalent in mm                 !
    !     sncovr(IMAX)  - ialgflg=0: not used                               !
    !                     ialgflg=1: snow cover over land in fraction       !
    !     snoalb(IMAX)  - ialbflg=0: not used                               !
    !                     ialgflg=1: max snow albedo over land in fraction  !
    !     zorlf (IMAX)  - surface roughness in cm                           !
    !     coszf (IMAX)  - cosin of solar zenith angle                       !
    !     tsknf (IMAX)  - ground surface temperature in k                   !
    !     tairf (IMAX)  - lowest model layer air temperature in k           !
    !     hprif (IMAX)  - topographic sdv in m                              !
    !           ---  for ialbflg=0 climtological albedo scheme  ---         !
    !     alvsf (IMAX)  - 60 degree vis albedo with strong cosz dependency  !
    !     alnsf (IMAX)  - 60 degree nir albedo with strong cosz dependency  !
    !     alvwf (IMAX)  - 60 degree vis albedo with weak cosz dependency    !
    !     alnwf (IMAX)  - 60 degree nir albedo with weak cosz dependency    !
    !           ---  for ialbflg=1 modis based land albedo scheme ---       !
    !     alvsf (IMAX)  - visible black sky albedo at zenith 60 degree      !
    !     alnsf (IMAX)  - near-ir black sky albedo at zenith 60 degree      !
    !     alvwf (IMAX)  - visible white sky albedo                          !
    !     alnwf (IMAX)  - near-ir white sky albedo                          !
    !                                                                       !
    !     facsf (IMAX)  - fractional coverage with strong cosz dependency   !
    !     facwf (IMAX)  - fractional coverage with weak cosz dependency     !
    !     fice  (IMAX)  - sea-ice fraction                                  !
    !     tisfc (IMAX)  - sea-ice surface temperature                       !
    !     IMAX          - array horizontal dimension                        !
    !                                                                       !
    !  outputs:                                                             !
    !     sfcalb(IMAX,NF_ALBD)                                              !
    !           ( :, 1) -     near ir direct beam albedo                    !
    !           ( :, 2) -     near ir diffused albedo                       !
    !           ( :, 3) -     uv+vis direct beam albedo                     !
    !           ( :, 4) -     uv+vis diffused albedo                        !
    !                                                                       !
    !  module internal control variables:                                   !
    !     ialbflg       - =0 use the default climatology surface albedo     !
    !                     =1 use modis retrieved albedo and input snow cover!
    !                        for land areas                                 !
    !                                                                       !
    !  ====================    end of description    =====================  !
    """
    if ialbflg == -2:
        alb = np.zeros_like(islmsk)
        alb[islmsk == 0] = Float(OCEAN_ALBEDO)  # ocean
        alb[islmsk == 1] = Float(LAND_ALBEDO)  # land
        alb[islmsk == 2] = Float(ICE_ALBEDO)  # sea-ice
        for k in range(sfcalb.shape[2]):
            sfcalb[:, :, k] = alb
    elif ialbflg == -1:
        sfcalb[:] = OCEAN_ALBEDO  # Assuming an aquaplanet
    elif ialbflg == 0:  # use climatological albedo scheme
        # Modified snow albedo scheme - units convert to m (originally
        # snowf in mm; zorlf in cm)
        for i in range(snowf.shape[0]):
            for j in range(snowf.shape[1]):
                asnow = 0.02 * snowf[i, j]
                argh = min(0.50, max(0.025, 0.01 * zorlf[i, j]))
                hrgh = min(1.0, max(0.20, 1.0577 - 1.1538e-3 * hprif[i, j]))
                fsno0 = asnow / (argh + asnow) * hrgh
                if islmsk == 0 and (
                    (tsknf[i, j] > physcons.TICE) or ldisable_radiation_quasi_sea_ice
                ):
                    fsno0 = 0.0

                fsno1 = 1.0 - fsno0
                flnd0 = min(1.0, facsf[i, j] + facwf[i, j])
                fsea0 = max(0.0, 1.0 - flnd0)
                fsno = fsno0
                fsea = fsea0 * fsno1
                flnd = flnd0 * fsno1

                # Calculate diffused sea surface albedo
                if tsknf[i, j] >= 271.5 or ldisable_radiation_quasi_sea_ice:
                    asevd = 0.06
                    asend = 0.06
                elif tsknf[i, j] < 271.1:
                    asevd = 0.70
                    asend = 0.65
                else:
                    a1 = (tsknf[i, j] - 271.1) ** 2
                    asevd = 0.7 - 4.0 * a1
                    asend = 0.65 - 3.6875 * a1

                # Calculate diffused snow albedo.
                if islmsk[i, j] == 2:
                    ffw = 1.0 - fice[i, j]
                    if ffw < 1.0:
                        dtgd = max(0.0, min(5.0, (constants.TTP - tisfc[i, j])))
                        b1 = 0.03 * dtgd
                    else:
                        b1 = 0.0

                    b3 = 0.06 * ffw
                    asnvd = (0.70 + b1) * fice[i, j] + b3
                    asnnd = (0.60 + b1) * fice[i, j] + b3
                    asevd = 0.70 * fice[i, j] + b3
                    asend = 0.60 * fice[i, j] + b3
                else:
                    asnvd = 0.90
                    asnnd = 0.75

                # Calculate direct snow albedo.
                if coszf[i, j] < 0.5:
                    csnow = 0.5 * (3.0 / (1.0 + 4.0 * coszf[i, j]) - 1.0)
                    asnvb = min(0.98, asnvd + (1.0 - asnvd) * csnow)
                    asnnb = min(0.98, asnnd + (1.0 - asnnd) * csnow)
                else:
                    asnvb = asnvd
                    asnnb = asnnd

                # Calculate direct sea surface albedo.
                if coszf[i, j] > 0.0001:
                    # rfcs = 1.4 / (1.0 + 0.8*coszf[i, j])
                    # rfcw = 1.3 / (1.0 + 0.6*coszf[i, j])
                    rfcs = 2.14 / (1.0 + 1.48 * coszf[i, j])
                    rfcw = rfcs

                    if (
                        tsknf[i, j] >= constants.TICE0
                        or ldisable_radiation_quasi_sea_ice
                    ):
                        asevb = max(
                            asevd,
                            0.026 / (coszf[i, j] ** 1.7 + 0.065)
                            + 0.15
                            * (coszf[i, j] - 0.1)
                            * (coszf[i, j] - 0.5)
                            * (coszf[i, j] - 1.0),
                        )
                        asenb = asevb
                    else:
                        asevb = asevd
                        asenb = asend
                else:
                    rfcs = 1.0
                    rfcw = 1.0
                    asevb = asevd
                    asenb = asend

                a1 = alvsf[i, j] * facsf[i, j]
                b1 = alvwf[i, j] * facwf[i, j]
                a2 = alnsf[i, j] * facsf[i, j]
                b2 = alnwf[i, j] * facwf[i, j]
                ab1bm = a1 * rfcs + b1 * rfcw
                ab2bm = a2 * rfcs + b2 * rfcw
                sfcalb[i, j, 0] = min(0.99, ab2bm) * flnd + asenb * fsea + asnnb * fsno
                sfcalb[i, j, 1] = (a2 + b2) * 0.96 * flnd + asend * fsea + asnnd * fsno
                sfcalb[i, j, 2] = min(0.99, ab1bm) * flnd + asevb * fsea + asnvb * fsno
                sfcalb[i, j, 3] = (a1 + b1) * 0.96 * flnd + asevd * fsea + asnvd * fsno

    elif ialbflg == 1:  # If use modis based albedo for land area:
        for i in range(snowf.shape[0]):
            for j in range(snowf.shape[1]):
                # Calculate snow cover input directly for land model, no
                # conversion needed.
                fsno0 = sncovr[i, j]

                if islmsk[i, j] == 0 and (
                    tsknf[i, j] > physcons.TICE or ldisable_radiation_quasi_sea_ice
                ):
                    fsno0 = 0.0

                if islmsk[i, j] == 2:
                    asnow = 0.02 * snowf[i, j]
                    argh = min(0.50, max(0.025, 0.01 * zorlf[i, j]))
                    hrgh = min(1.0, max(0.20, 1.0577 - 1.1538e-3 * hprif[i, j]))
                    fsno0 = asnow / (argh + asnow) * hrgh

                fsno1 = 1.0 - fsno0
                flnd0 = min(1.0, facsf[i, j] + facwf[i, j])
                fsea0 = max(0.0, 1.0 - flnd0)
                fsno = fsno0
                fsea = fsea0 * fsno1
                flnd = flnd0 * fsno1

                # Calculate diffused sea surface albedo.
                if tsknf[i, j] >= 271.5 or ldisable_radiation_quasi_sea_ice:
                    asevd = 0.06
                    asend = 0.06
                elif tsknf[i, j] < 271.1:
                    asevd = 0.70
                    asend = 0.65
                else:
                    a1 = (tsknf[i, j] - 271.1) ** 2
                    asevd = 0.7 - 4.0 * a1
                    asend = 0.65 - 3.6875 * a1

                # Calculate diffused snow albedo, land area use input max snow albedo
                if islmsk[i, j] == 2:
                    ffw = 1.0 - fice[i, j]
                    if ffw < 1.0:
                        dtgd = max(0.0, min(5.0, (constants.TTP - tisfc[i, j])))
                        b1 = 0.03 * dtgd
                    else:
                        b1 = 0.0

                    b3 = 0.06 * ffw
                    asnvd = (0.70 + b1) * fice[i, j] + b3
                    asnnd = (0.60 + b1) * fice[i, j] + b3
                    asevd = 0.70 * fice[i, j] + b3
                    asend = 0.60 * fice[i, j] + b3
                else:
                    asnvd = snoalb[i, j]
                    asnnd = snoalb[i, j]

                # Calculate direct snow albedo.
                if islmsk[i, j] == 2:
                    if coszf[i, j] < 0.5:
                        csnow = 0.5 * (3.0 / (1.0 + 4.0 * coszf[i, j]) - 1.0)
                        asnvb = min(0.98, asnvd + (1.0 - asnvd) * csnow)
                        asnnb = min(0.98, asnnd + (1.0 - asnnd) * csnow)
                    else:
                        asnvb = asnvd
                        asnnb = asnnd
                else:
                    asnvb = snoalb[i, j]
                    asnnb = snoalb[i, j]

                # Calculate direct sea surface albedo,
                # use fanglin's zenith angle treatment

                if coszf[i, j] > 0.0001:
                    # rfcs = 1.89 - 3.34*coszf[i, j] + 4.13*coszf[i, j]*coszf[i, j] - (
                    #     2.02*coszf[i, j]*coszf[i, j]*coszf[i, j]
                    # )
                    rfcs = 1.775 / (1.0 + 1.55 * coszf[i, j])

                    if (
                        tsknf[i, j] >= constants.TICE0
                        or ldisable_radiation_quasi_sea_ice
                    ):
                        asevb = max(
                            asevd,
                            0.026 / (coszf[i, j] ** 1.7 + 0.065)
                            + 0.15
                            * (coszf[i, j] - 0.1)
                            * (coszf[i, j] - 0.5)
                            * (coszf[i, j] - 1.0),
                        )
                        asenb = asevb
                    else:
                        asevb = asevd
                        asenb = asend
                else:
                    rfcs = 1.0
                    asevb = asevd
                    asenb = asend

                ab1bm = min(0.99, alnsf[i, j] * rfcs)
                ab2bm = min(0.99, alvsf[i, j] * rfcs)
                sfcalb[i, j, 0] = ab1bm * flnd + asenb * fsea + asnnb * fsno
                sfcalb[i, j, 1] = alnwf[i, j] * flnd + asend * fsea + asnnd * fsno
                sfcalb[i, j, 2] = ab2bm * flnd + asevb * fsea + asnvb * fsno
                sfcalb[i, j, 3] = alvwf[i, j] * flnd + asevd * fsea + asnvd * fsno

    else:  # ialbflg == 2
        for i in range(snowf.shape[0]):
            for j in range(snowf.shape[1]):
                # Calculate snow cover input directly for land model,
                # no conversion needed
                fsno0 = sncovr[i, j]

                if islmsk[i, j] == 0 and (
                    tsknf[i, j] > physcons.TICE or ldisable_radiation_quasi_sea_ice
                ):
                    fsno0 = 0.0

                if islmsk[i, j] == 2:
                    asnow = 0.02 * snowf[i, j]
                    argh = min(0.50, max(0.025, 0.01 * zorlf[i, j]))
                    hrgh = min(1.0, max(0.20, 1.0577 - 1.1538e-3 * hprif[i, j]))
                    fsno0 = asnow / (argh + asnow) * hrgh

                fsno1 = 1.0 - fsno0
                flnd0 = min(1.0, facsf[i, j] + facwf[i, j])
                fsea0 = max(0.0, 1.0 - flnd0)
                fsno = fsno0
                fsea = fsea0 * fsno1
                flnd = flnd0 * fsno1

                # Calculate diffused sea surface albedo.
                if tsknf[i, j] >= 271.5 or ldisable_radiation_quasi_sea_ice:
                    asevd = 0.06
                    asend = 0.06
                elif tsknf[i, j] < 271.1:
                    asevd = 0.70
                    asend = 0.65
                else:
                    a1 = (tsknf[i, j] - 271.1) ** 2
                    asevd = 0.7 - 4.0 * a1
                    asend = 0.65 - 3.6875 * a1

                # Calculate diffused snow albedo, land area use input max snow albedo
                if islmsk[i, j] == 2:
                    ffw = 1.0 - fice[i, j]
                    if ffw < 1.0:
                        dtgd = max(0.0, min(5.0, (constants.TTP - tisfc[i, j])))
                        b1 = 0.03 * dtgd
                    else:
                        b1 = 0.0

                    b3 = 0.06 * ffw
                    asnvd = (0.70 + b1) * fice[i, j] + b3
                    asnnd = (0.60 + b1) * fice[i, j] + b3
                    asevd = 0.70 * fice[i, j] + b3
                    asend = 0.60 * fice[i, j] + b3
                else:
                    asnvd = snoalb[i, j]
                    asnnd = snoalb[i, j]

                # Calculate direct snow albedo.
                if islmsk[i, j] == 2:
                    if coszf[i, j] < 0.5:
                        csnow = 0.5 * (3.0 / (1.0 + 4.0 * coszf[i, j]) - 1.0)
                        asnvb = min(0.98, asnvd + (1.0 - asnvd) * csnow)
                        asnnb = min(0.98, asnnd + (1.0 - asnnd) * csnow)
                    else:
                        asnvb = asnvd
                        asnnb = asnnd

                # Calculate direct sea surface albedo,
                # use fanglin's zenith angle treatment
                if coszf[i, j] > 0.0001:
                    # rfcs = 1.89 - 3.34*coszf[i, j] + (
                    #     4.13*coszf[i, j]*coszf[i, j]
                    # ) - 2.02*coszf[i, j]*coszf[i, j]*coszf[i, j]
                    rfcs = 1.775 / (1.0 + 1.55 * coszf[i, j])

                    if (
                        tsknf[i, j] >= constants.TICE0
                        or ldisable_radiation_quasi_sea_ice
                    ):
                        asevb = max(
                            asevd,
                            0.026 / (coszf[i, j] ** 1.7 + 0.065)
                            + 0.15
                            * (coszf[i, j] - 0.1)
                            * (coszf[i, j] - 0.5)
                            * (coszf[i, j] - 1.0),
                        )
                        asenb = asevb
                    else:
                        asevb = asevd
                        asenb = asend
                else:
                    rfcs = 1.0
                    asevb = asevd
                    asenb = asend
                sfcalb[i, j, 0] = (
                    min(0.99, max(0.01, lsmalbedo[i, j, 0])) * flnd
                    + asenb * fsea
                    + asnnb * fsno
                )
                sfcalb[i, j, 1] = (
                    min(0.99, max(0.01, lsmalbedo[i, j, 1])) * flnd
                    + asend * fsea
                    + asnnd * fsno
                )
                sfcalb[i, j, 2] = (
                    min(0.99, max(0.01, lsmalbedo[i, j, 2])) * flnd
                    + asevb * fsea
                    + asnvb * fsno
                )
                sfcalb[i, j, 3] = (
                    min(0.99, max(0.01, lsmalbedo[i, j, 3])) * flnd
                    + asevd * fsea
                    + asnvd * fsno
                )


def set_sfcemis(
    gridlon: np.ndarray,
    gridlat: np.ndarray,
    islmsk: np.ndarray,
    snowf: np.ndarray,
    sncovr: np.ndarray,
    zorlf: np.ndarray,
    tskin: np.ndarray,
    hprif: np.ndarray,
    iemslw: Int,
    ialbflg: Int,
    ldisable_radiation_quasi_sea_ice: bool,
    sfcemis: np.ndarray,
    ext_sfcemis_data: np.ndarray,
    sfcemis_lsm: np.ndarray,
):
    """
    !  ===================================================================  !
    !                                                                       !
    !  this program computes surface emissivity for lw radiation.           !
    !                                                                       !
    !  usage:         call setemis                                          !
    !                                                                       !
    !  subprograms called:  none                                            !
    !                                                                       !
    !  ====================  defination of variables  ====================  !
    !                                                                       !
    !  inputs:                                                              !
    !     xlon  (IMAX)  - longitude in radiance, ok for both 0->2pi or      !
    !                     -pi -> +pi ranges                                 !
    !     xlat  (IMAX)  - latitude  in radiance, default to pi/2 -> -pi/2   !
    !                     range, otherwise see in-line comment              !
    !     slmsk (IMAX)  - sea(0),land(1),ice(2) mask on fcst model grid     !
    !     snowf (IMAX)  - snow depth water equivalent in mm                 !
    !     sncovr(IMAX)  - ialbflg=1: snow cover over land in fraction       !
    !     zorlf (IMAX)  - surface roughness in cm                           !
    !     tsknf (IMAX)  - ground surface temperature in k                   !
    !     tairf (IMAX)  - lowest model layer air temperature in k           !
    !     hprif (IMAX)  - topographic sdv in m                              !
    !     lsmemiss(IMAX)- emissivity from lsm                               !
    !                                                                       !
    !     IMAX          - array horizontal dimension                        !
    !                                                                       !
    !  outputs:                                                             !
    !     sfcemis(IMAX) - surface emissivity                                !
    !                                                                       !
    !  -------------------------------------------------------------------  !
    !                                                                       !
    !  surface type definations:                                            !
    !     1. open water                   2. grass/wood/shrub land          !
    !     3. tundra/bare soil             4. sandy desert                   !
    !     5. rocky desert                 6. forest                         !
    !     7. ice                          8. snow                           !
    !                                                                       !
    !  input index data lon from 0 towards east, lat from n to s            !
    !                                                                       !
    !  ====================    end of description    =====================  !
    """
    if iemslw == 0:
        sfcemis[:] = Float(CONST_EMS)
        return

    sfcemis[islmsk == 0] = Float(EMS_REF[0])  # sea
    sfcemis[islmsk == 2] = Float(EMS_REF[6])  # sea-ice
    if iemslw == 1:
        sfcemis_grid_data = map_sfc_to_grid(ext_sfcemis_data, gridlon, gridlat)
        for i in range(sfcemis.shape[0]):
            for j in range(sfcemis.shape[1]):
                if islmsk[i, j] == 1:
                    idx = max(1, sfcemis_grid_data[i, j])
                    if idx >= 6:
                        idx = 2
                    sfcemis[i, j] = Float(EMS_REF[idx])
                if ialbflg == 1 and islmsk[i, j] == 1:  # input land area snow cover
                    fsno = 1.0 - sncovr[i, j]
                    sfcemis[i, j] = Float(sfcemis[i, j] * fsno) + Float(
                        EMS_REF[7] * sncovr[i, j]
                    )
                else:  # compute snow cover from snow depth
                    if snowf[i, j] > 0.0:
                        asnow = 0.02 * snowf[i, j]
                        argh = min(0.50, max(0.025, 0.01 * zorlf[i, j]))
                        hrgh = min(1.0, max(0.20, 1.0577 - 1.1538e-3 * hprif[i, j]))
                        fsno0 = asnow / (argh + asnow) * hrgh
                        if islmsk[i, j] == 0 and (
                            tskin[i, j] > 271.2 or ldisable_radiation_quasi_sea_ice
                        ):
                            fsno0 = 0.0
                        fsno1 = 1.0 - fsno0
                        sfcemis[i, j] = Float(sfcemis[i, j] * fsno1) + Float(
                            EMS_REF[7] * fsno0
                        )
    else:  # iemslw == 2
        sfcemis[islmsk == 1] = sfcemis_lsm[islmsk == 1]  # land from LSM
