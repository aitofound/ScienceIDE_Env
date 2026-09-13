import os
import re
from pathlib import Path

import numpy as np

import ndsl.constants as constants
from ndsl.dsl.gt4py import PARALLEL, computation, interval
from ndsl.dsl.typing import Float, FloatField, FloatFieldIJ, Int
from ndsl.logging import ndsl_log

NDAYS_MONTH = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 30]
MINYEAR = 1957  # earliest year 2-d co2 data available
NF_VGAS = 10  # number of gas species
IMXCO2 = 24  # input co2 dat lon points
JMXCO2 = 12  # input co2 data lat points
RESCO2 = 15.0  # horizontal resolution in degree
PRSCO2 = 788.0  # pressure limitation for 2-d co2 (mb)

CO2VMR_DEF = 350.0e-6
N2OVMR_DEF = 0.31e-6
CH4VMR_DEF = 1.50e-6
O2VMR_DEF = 0.209
N2VMR_DEF = 0.7808
COVMR_DEF = 1.50e-8
F11VMR_DEF = 3.520e-10  # aer 2003 value
F12VMR_DEF = 6.358e-10  # aer 2003 value
F22VMR_DEF = 1.500e-10  # aer 2003 value
CL4VMR_DEF = 1.397e-10  # aer 2003 value
F113VMR_DEF = 8.2000e-11  # gfdl 1999 value


def read_global_annual_co2(co2gbl_file: Path) -> dict[str, Float | Int]:
    """
    Function to read a text file of CO2 global half-yearly means
    and growth rates into a model.
    Assumes a format of:
    HEADER
    yyyy     co2  co2   growth_rate   growth_rate
    Returns a dictionary containing start and end years and mapping year to data
    """
    global_annual_co2_data = {}
    co2_file = open(co2gbl_file)
    for i, line in enumerate(co2_file):
        if line.split():  # skip blank lines
            table_dat = line.split()
            if i == 0:
                global_annual_co2_data["start_year"] = Int(table_dat[0])
                global_annual_co2_data["end_year"] = Int(table_dat[2])
            elif table_dat[0] == "---":
                break  # end at the dashes
            else:
                year = Int(table_dat[0])
                global_annual_co2_data[year] = [
                    Float(data) * 1.0e-6 for data in table_dat[1:]
                ]
    co2_file.close()
    return global_annual_co2_data


def read_monthly_resolved_co2(
    co2dat_file: Path,
) -> tuple[int, dict[str | Int, Float | list[Float]]]:
    """
    Function to read a text file of 15-degree CO2 monthly means into a model.
    Assumes a format of:
    HEADER
    24 values (15 degree resolution longitudinally)
    There should be 144 lines (12 months, resolved 15 degrees latitudinally)
    Assuming here that it's grouped by month, so 12 lines of January,
    12 of February, and so on?

    Returns the year and a dictionary mapping month to 2D data
    """
    resolved_monthly_co2_data = {}
    co2_file = open(co2dat_file)
    imonth = 1
    for i, line in enumerate(co2_file):
        if line.split():  # skip blank lines
            table_dat = line.split()
            if i == 0:
                year = Int(table_dat[0])
                resolved_monthly_co2_data["mean"] = (
                    Float(table_dat[16].replace(",", "")) * 1.0e-6
                )
                resolved_monthly_co2_data["growth_rate"] = (
                    Float(table_dat[20].replace(",", "")) * 1.0e-6
                )
                resolved_monthly_co2_data["missing"] = Float(table_dat[-1]) * 1.0e-6
            else:
                ilat = (i - 1) % 12
                if ilat == 0:
                    co2_data = []
                co2_data.append([Float(data) * 1.0e-6 for data in table_dat])
                if ilat == 11:
                    resolved_monthly_co2_data[Int(imonth)] = co2_data
                    imonth += 1
    co2_file.close()
    return year, resolved_monthly_co2_data


def read_monthly_cycle_co2(
    co2cyc_file: Path,
) -> dict[str | Int, Int | Float | dict[str, Float | list[Float]]]:
    """
    Function to read a text file of 15-degree CO2 monthly deviations into a model.
    Assumes a format of:
    HEADER
    MONTH
    12 lines of 24 values (15 degree resolution longitudinally)
    repeated for each month of the year

    Returns a dictionary mapping month to the global mean and 2D data
    """
    resolved_monthly_co2_cycle = {}
    co2_file = open(co2cyc_file)
    for i, line in enumerate(co2_file):
        if line.split():  # skip blank lines
            table_dat = line.split()
            if i == 0:
                resolved_monthly_co2_cycle["start_year"] = Int(
                    table_dat[0].split("-")[0]
                )
                resolved_monthly_co2_cycle["end_year"] = Int(table_dat[0].split("-")[1])
                resolved_monthly_co2_cycle["annual_mean"] = (
                    Float(table_dat[15]) * 1.0e-6
                )
                resolved_monthly_co2_cycle["growth_rate"] = (
                    Float(table_dat[19]) * 1.0e-6
                )
                resolved_monthly_co2_cycle["missing"] = Float(table_dat[-1]) * 1.0e-6
                resolved_monthly_co2_cycle["mean"] = []
            elif i < 13:
                resolved_monthly_co2_cycle["mean"].append(
                    [Float(data) * 1.0e-6 for data in table_dat]
                )
            else:
                if table_dat[0] == "MONTH":
                    imonth = Int(table_dat[2])
                    resolved_monthly_co2_cycle[imonth] = {}
                    resolved_monthly_co2_cycle[imonth]["mean"] = (
                        Float(table_dat[-1]) * 1.0e-6
                    )
                    resolved_monthly_co2_cycle[imonth]["data"] = []
                else:
                    resolved_monthly_co2_cycle[imonth]["data"].append(
                        [Float(data) * 1.0e-6 for data in table_dat]
                    )
    co2_file.close()
    return resolved_monthly_co2_cycle


def read_co2_files(input_dir: Path, prefix="") -> tuple[dict, dict, dict]:
    """
    Function to read in input CO2 data files
    """

    co2_glb_data = {}
    co2_mvr_data = {}
    co2_cyc_data = {}

    glob_fname = prefix + "co2historicaldata_glob.txt"
    cyc_fname = prefix + "co2monthlycyc1976_2009.txt" if prefix else "co2monthlycyc.txt"

    co2_glob_file = input_dir.joinpath(glob_fname)
    monthly_co2_files = [
        f
        for f in os.listdir(input_dir)
        if re.search(r"co2historicaldata_[0-9]{4}\.txt", f)
    ]
    co2_cycle_file = input_dir.joinpath(cyc_fname)

    if co2_glob_file.is_file():
        co2_glb_data = read_global_annual_co2(co2_glob_file)
    if monthly_co2_files:
        for monthly_file in monthly_co2_files:
            year, data = read_monthly_resolved_co2(input_dir.joinpath(monthly_file))
            co2_mvr_data[year] = data
    if co2_cycle_file.is_file():
        co2_cyc_data = read_monthly_cycle_co2(co2_cycle_file)
    return co2_glb_data, co2_mvr_data, co2_cyc_data


def broadcast_co2_to_grid(
    co2dat,
    gridlon,
    gridlat,
) -> np.ndarray:
    """
    Function to take input CO2 data assumed to be at 15-degree resolution and
    broadcast it to a model-resolution cubed-sphere grid by assigning values from
    the coarse grid directly to the relevant fine-grid elements.

    Inputs:
        co2dat: CO2 concentration data at RESCO2 resolution in list format as
                from read_monthly_resolved_co2 above
        gridlon: Grid longitudes, ndarray-like
        gridlat: Grid latitudes, ndarray-like
    Outputs:
        gridded_data: CO2 concentration data in the same array shape as lat and lon
    """
    gridded_data = np.zeros_like(gridlon)

    nx = gridlon.shape[0]
    ny = gridlon.shape[1]
    tmp = (180.0 / constants.PI) / RESCO2
    for i in range(nx):
        for j in range(ny):
            jres = (0.5 * constants.PI - gridlat[i, j]) * tmp
            jres = min(JMXCO2 - 1, int(jres))
            ires = (
                (gridlon[i, j] * tmp)
                if gridlon[i, j] >= 0.0
                else ((gridlon[i, j] + 2.0 * constants.PI) * tmp)
            )
            ires = min(IMXCO2 - 1, int(ires))
            gridded_data[i, j] = co2dat[jres][ires]
    return gridded_data


def gas_init(
    input_dir: Path,
    ico2flg: Int,
    ioznflg: Int,
    ictmflg: Int,
    iyear: Int,
    imonth: Int,
    gridlon: np.ndarray,
    gridlat: np.ndarray,
    prefix: str = "",
) -> tuple[
    float,
    float,
    float,
    float,
    float,
    float,
    float,
    float,
    float,
    float,
    float,
    np.ndarray,
    np.ndarray,
    dict,
    dict,
    dict,
]:
    """
    Function to init gases for radiation. Returns a global mean CO2 concentration,
    and 3 optional dictionaries of CO2 data: one for annual global means, one for
    monthly resolved means, and one for monthly CO2 cycle data.
    """
    # TODO add climatological ozone?
    if ioznflg == 0:
        raise NotImplementedError("climatological ozone (ioznflg = 0) is not supported")
    co2_glb = CO2VMR_DEF
    n2o = N2OVMR_DEF
    ch4 = CH4VMR_DEF
    o2 = O2VMR_DEF
    co = COVMR_DEF
    n2 = N2VMR_DEF
    cfc11 = F11VMR_DEF
    cfc12 = F12VMR_DEF
    cfc22 = F22VMR_DEF
    cfc113 = F113VMR_DEF
    ccl4 = CL4VMR_DEF

    co2_arr = np.zeros_like(gridlon)
    co2_cyc = np.zeros_like(gridlon)

    if ico2flg == 0:
        co2_arr[:] = co2_glb
        return (
            n2o,
            ch4,
            o2,
            co,
            n2,
            cfc11,
            cfc12,
            cfc22,
            cfc113,
            ccl4,
            co2_glb,
            co2_arr,
            co2_cyc,
            None,
            None,
            None,
        )

    co2_glb_data, co2_mvr_data, co2_cyc_data = read_co2_files(input_dir, prefix)

    if ico2flg == 1 and not co2_glb_data:
        raise FileNotFoundError(
            (
                f"Annual CO2 global data file not found in {input_dir}, "
                "stopping in gas_init"
            )
        )
    if ico2flg == 2 and not co2_mvr_data:
        raise FileNotFoundError(
            f"Monthly CO2 data file not found in {input_dir}, stopping in gas_init"
        )
    if ictmflg == -2 and not co2_cyc_data:
        raise FileNotFoundError(
            f"Monthly CO2 cycle file not found in {input_dir}, stopping in gas_init"
        )
    if ictmflg == -1:
        if ico2flg == 1:
            if iyear not in co2_glb_data.keys():
                raise KeyError(
                    f"{iyear} not found in CO2 global data, stopping in gas_init"
                )
            else:
                co2_glb = (co2_glb_data[iyear][0] + co2_glb_data[iyear][1]) * 0.5
            co2_arr[:] = co2_glb
        else:  # ico2flg == 2
            if iyear not in co2_mvr_data.keys():
                raise NameError(
                    f"{iyear} not found in monthly CO2 data, stopping in gas_init"
                )
            co2_glb = co2_mvr_data[iyear]["mean"]
            co2_arr = broadcast_co2_to_grid(
                co2_mvr_data[iyear][imonth], gridlon, gridlat
            )
    return (
        n2o,
        ch4,
        o2,
        co,
        n2,
        cfc11,
        cfc12,
        cfc22,
        cfc113,
        ccl4,
        co2_glb,
        co2_arr,
        co2_cyc,
        co2_mvr_data,
        co2_glb_data,
        co2_cyc_data,
    )


def ozone_update(
    imon: Int,
    iday: Int,
    ihour: Int,
    ioznflg: Int,
    loz1st: Int,
) -> tuple[Int, Int, Float]:
    """
    Ozone portion of gas_update fortran routine. Computes O3 climatology parameters
    """
    # TODO: Add support for non-prognostic Ozone.
    k1oz = 0
    k2oz = 0
    facoz = 0.0
    if ioznflg == 0:
        midmon = NDAYS_MONTH[imon - 1] // 2 + 1
        change = loz1st != 0 or ((iday == midmon) and (ihour == 0))

        if change:
            if iday < midmon:
                k1oz = Int((imon + 10) % 12)
                midm = NDAYS_MONTH[k1oz] / 2 + 1
                k2oz = Int(imon - 1)
                midp = NDAYS_MONTH[k1oz] + midmon
            else:
                k1oz = Int(imon - 1)
                midm = midmon
                k2oz = Int(imon % 12)
                midp = NDAYS_MONTH[k2oz] / 2 + 1 + NDAYS_MONTH[k1oz]
        if iday < midmon:
            id = iday + NDAYS_MONTH[k1oz]
        else:
            id = iday
        facoz = Float(id - midm) / Float(midp - midm)
    return k1oz, k2oz, facoz


def co2_update(
    iyear: Int,
    imon: Int,
    ico2flg: Int,
    ldoco2: Int,
    ictmflg: Int,
    co2_glb: Float,
    co2_arr: np.ndarray,
    co2_cyc: np.ndarray,
    gridlon: np.ndarray,
    gridlat: np.ndarray,
    co2_annual_means: dict = None,
    co2_monthly_means: dict = None,
    co2_monthly_cycle: dict = None,
) -> tuple[float, np.ndarray, np.ndarray]:
    """
    CO2 portion of gas_update Fortran subroutine.

    Fortran docstring follows
    !  ===================================================================  !
    !                                                                       !
    !  gas_update reads in 2-d monthly co2 data set for a specified year.   !
    !  data are in a 15 degree lat/lon horizontal resolution.               !
    !                                                                       !
    !  inputs:                                               dimemsion      !
    !     iyear   - year of the requested data for fcst         1           !
    !     imon    - month of the year                           1           !
    !     iday    - day of the month                            1           !
    !     ihour   - hour of the day                             1           !
    !     loz1st  - clim ozone 1st time update control flag     1           !
    !     ldoco2  - co2 update control flag                     1           !
    !     me      - print message control flag                  1           !
    !                                                                       !
    !  outputs: (to the module variables)                                   !
    !    ( none )                                                           !
    !                                                                       !
    !  external module variables:  (in physparam)                           !
    !     ico2flg    - co2 data source control flag                         !
    !                   =0: use prescribed co2 global mean value            !
    !                   =1: use input global mean co2 value (co2_glb)       !
    !                   =2: use input 2-d monthly co2 value (co2vmr_sav)    !
    !     ictmflg    - =yyyy#, data ic time/date control flag               !
    !                  =   -2: same as 0, but superimpose seasonal cycle    !
    !                          from climatology data set.                   !
    !                  =   -1: use user provided external data for the fcst !
    !                          time, no extrapolation.                      !
    !                  =    0: use data at initial cond time, if not existed!
    !                          then use latest, without extrapolation.      !
    !                  =    1: use data at the forecast time, if not existed!
    !                          then use latest and extrapolate to fcst time.!
    !                  =yyyy0: use yyyy data for the forecast time, no      !
    !                          further data extrapolation.                  !
    !                  =yyyy1: use yyyy data for the fcst. if needed, do    !
    !                          extrapolation to match the fcst time.        !
    !     ioznflg    - ozone data control flag                              !
    !                   =0: use climatological ozone profile                !
    !                   >0: use interactive ozone profile                   !
    !     ivflip     - vertical profile indexing flag                       !
    !     co2dat_file- external co2 2d monthly obsv data table              !
    !     co2gbl_file- external co2 global annual mean data table           !
    !                                                                       !
    !  internal module variables:                                           !
    !     co2vmr_sav - monthly co2 volume mixing ratio     IMXCO2*JMXCO2*12 !
    !     co2cyc_sav - monthly cycle co2 vol mixing ratio  IMXCO2*JMXCO2*12 !
    !     co2_glb    - global annual mean co2 mixing ratio                  !
    !     gco2cyc    - global monthly mean co2 variation       12           !
    !     k1oz,k2oz,facoz                                                   !
    !                - climatology ozone parameters             1           !
    !                                                                       !
    !  usage:    call gas_update                                            !
    !                                                                       !
    !  subprograms called:  none                                            !
    !                                                                       !
    !  ===================================================================  !
    """

    # These need to be respected in the driver:
    # if ( ico2flg == 0 ) return    ! use prescribed global mean co2 data
    # if ( ictmflg ==-1 ) return    ! use user provided co2 data
    # if ( .not. ldoco2 ) return    ! no need to update co2 data

    if ico2flg == 0:  # use prescribed global mean co2 data
        return co2_glb, co2_arr, co2_cyc
    if ictmflg == -1:  # use user provided co2 data
        return co2_glb, co2_arr, co2_cyc
    if not ldoco2:  # no need to update co2 data
        return co2_glb, co2_arr, co2_cyc

    if ictmflg <= 0:  # use user provided external data
        lextpl = False  # no time extrapolation
        idyr = iyear  # use the model year
    else:  # use historically observed data
        lextpl = (ictmflg % 10) == 1  # flag for data extrapolation
        idyr = ictmflg // 10  # year of data source used
        if idyr == 0:  # not specified, use model year
            idyr = iyear
    iyr = idyr

    # for data earlier than MINYEAR (1957), the data are in
    # the form of semi-yearly global mean values.  otherwise,
    # data are monthly mean in horizontal 2-d map.
    co2vmr_sav = np.zeros((JMXCO2, IMXCO2))
    if idyr < MINYEAR and ictmflg > 0:
        # We're looking far enough back that we only have annual data
        assert (
            co2_annual_means
        ), f"For CO2 concentrations before {MINYEAR} annual mean data must be provided"
        if idyr < co2_annual_means["start_year"]:
            start_year = co2_annual_means["start_year"]
            ndsl_log.info(f"{idyr} before data range, using first year: {start_year}")
            iyr = start_year
        co2_growth = 0.5 * (co2_annual_means[iyr][2] + co2_annual_means[iyr][3])
        if ico2flg == 2:
            if imon < 7:
                co2_glb = co2_annual_means[iyr][0]
            else:
                co2_glb = co2_annual_means[iyr][1]
        else:
            co2_glb = (co2_annual_means[iyr][0] + co2_annual_means[iyr][1]) * 0.5
        co2_arr[:] = co2_glb
        ndsl_log.info(f"CO2 value for {idyr}: {co2_arr[0, 0]}")
    elif ictmflg > 10:  # we only want a specific annual value and won't update
        if (ictmflg % 10) == 0:
            assert (
                iyr in co2_annual_means.keys()
            ), f"{iyr} is not in annual mean CO2 data. Stopping in gas_update"
        co2_growth = 0.5 * (co2_annual_means[iyr][2] + co2_annual_means[iyr][3])
        if ico2flg == 2:
            if imon < 7:
                co2_glb = co2_annual_means[iyr][0]
            else:
                co2_glb = co2_annual_means[iyr][1]
        else:
            co2_glb = (co2_annual_means[iyr][0] + co2_annual_means[iyr][1]) * 0.5
            co2_arr[:] = co2_glb
        ndsl_log.info(f"CO2 value for {idyr}: {co2_arr[0, 0]}")
    else:  # ictmflg = 0, 1, or -2, so we first get the latest available data
        if ico2flg == 1:
            if iyr not in co2_annual_means.keys():
                if iyr > co2_annual_means["end_year"]:
                    iyr = co2_annual_means["end_year"]
                elif iyr < co2_annual_means["start_year"]:
                    iyr = co2_annual_means["start_year"]
                else:
                    while iyr not in co2_annual_means.keys():
                        iyr -= 1
                        if iyr < co2_annual_means["start_year"]:
                            iyr = co2_annual_means["start_year"]
                            break  # safety first!
            co2_growth = 0.5 * (co2_annual_means[iyr][2] + co2_annual_means[iyr][3])
            co2_glb = (co2_annual_means[iyr][0] + co2_annual_means[iyr][1]) * 0.5
            co2_arr[:] = co2_glb
        elif ico2flg == 2:
            if iyr not in co2_monthly_means.keys():
                if iyr > max(co2_monthly_means.keys()):
                    iyr = max(co2_monthly_means.keys())
                elif iyr < min(co2_monthly_means.keys()):
                    iyr = min(co2_monthly_means.keys())
                else:
                    while iyr not in co2_monthly_means.keys():
                        iyr -= 1
                        if iyr < min(co2_monthly_means):
                            iyr = min(co2_monthly_means)
                            break  # safety first!
            co2_growth = co2_monthly_means[iyr]["growth_rate"]
            co2_glb = co2_monthly_means[iyr]["mean"]
            if ictmflg == -2:  # Take mean of all months
                co2_arr[:] = 0.0
                for i in range(1, 13):
                    co2_arr += broadcast_co2_to_grid(
                        co2_monthly_means[iyr][i], gridlon, gridlat
                    )
                co2_arr /= 12.0
                pass
            else:
                co2_arr = broadcast_co2_to_grid(
                    co2_monthly_means[iyr][imon], gridlon, gridlat
                )
        else:
            raise ValueError(f"ico2flg = {ico2flg} not recognized")

    if lextpl:
        # rate = co2_growth * (iyear - iyr)  # rate from early year
        # rate = 1.60  * (iyear - iyr)  # avg rate over long period
        rate = 2.00 * (iyear - iyr)  # avg rate for recent period
    else:
        rate = 0.0
    co2_arr[:] += rate
    co2_glb += rate
    if ictmflg == -2:  # Save monthly cycle too
        co2_cyc[:] = broadcast_co2_to_grid(co2_monthly_cycle[imon], gridlon, gridlat)
    return co2_glb, co2_arr, co2_cyc


def get_gases_topdown(
    co2: FloatField,
    plvl: FloatField,
    co2_glb: Float,
    co2_cyc: FloatFieldIJ,
    co2vmr_sav: FloatFieldIJ,
):
    """
    ivflip = 0
    """
    from __externals__ import ico2flg

    with computation(PARALLEL), interval(...):
        if ico2flg == 1:
            co2 = co2_glb + co2_cyc
        elif ico2flg == 2:
            if plvl >= PRSCO2:
                co2 = co2vmr_sav
            else:
                co2 = co2_glb + co2_cyc


def get_gases_bottomup(
    co2: FloatField,
    plvl: FloatField,
    co2_glb: Float,
    co2_cyc: FloatFieldIJ,
    co2vmr_sav: FloatFieldIJ,
):
    """
    ivflip = 1
    """
    from __externals__ import ico2flg

    with computation(PARALLEL):
        with interval(0, -1):
            if ico2flg == 1:
                co2 = co2_glb + co2_cyc
            elif ico2flg == 2:
                if plvl[0, 0, 1] >= PRSCO2:
                    co2 = co2vmr_sav
                else:
                    co2 = co2_glb + co2_cyc

        with interval(-1, None):
            co2 = co2_glb + co2_cyc
