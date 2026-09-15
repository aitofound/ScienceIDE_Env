import datetime
import re
from pathlib import Path

import numpy as np

import ndsl.constants as constants
from ndsl.dsl.gt4py import FORWARD, acos, computation, cos, interval, sin
from ndsl.dsl.typing import BoolFieldIJ, Float, FloatFieldIJ, Int
from ndsl.logging import ndsl_log

CCR = Float(1.3e-6)  # iteration limit
CYEAR = Float(365.25)  # days of year
SVT6 = Float(78.035)  # days between perihelion passage and march equinox of 1900
TPP = Float(1.55)  # days between epoch and perihelion passage of 1900
CZLIMT = Float(0.0001)  # ~ cos(89.99427)
JDOR = Float(2415020)  # jd of epoch which is january 0, 1900 at 12 hours ut
HRDAY = Float(1.0 / 24.0)  # 1 hour in days
MINDAY = Float(1.0 / 1440.0)  # 1 minute in days
SECDAY = Float(1.0 / 86400.0)  # 1 second in days

CON_SOLR = Float(1.3608e3)  # Default Solar constant
CON_SOLR_OLD = Float(1.3660e3)  # Previous default Solar constant
SMON_SAV = [CON_SOLR for i in range(12)]


def date_to_julian(iyear: int, imonth: int, iday: int) -> int:
    """
    Converts integer day, month, and year to julian day number.
    Fortran name is iw3jdn:
    Computes julian day number from year (4 digits), month,
    and day. iw3jdn is valid for years 1583 a.d. to 3300 a.d.
    Julian day number can be used to compute day of week, day of
    year, record numbers in an archive, replace day of century,
    find the number of days between two dates.
    Program history log:
    - Ralph Jones 1987-03-29
    - Ralph Jones 1989-10-25 Convert to cray cft77 fortran.
    @param[in] IYEAR Integer year (4 Digits)
    @param[in] MONTH Integer month of year (1 - 12)
    @param[in] IDAY Integer day of month (1 - 31)
    @return IW3JDN Integer Julian day number
    - Jan 1, 1960 is Julian day number 2436935
    - Jan 1, 1987 is Julian day number 2446797

    Args:
        iday (int): day of month (1-31)
        imonth (int): month number (1-12)
        iyear (int): 4-digit year

    Returns:
        jdn (int): julian day number, i.e. days since 01/01 4713 BC
    """
    jdn = int(
        iday
        - 32075
        + 1461 * (iyear + 4800 + (imonth - 14) / 12) / 4
        + 367 * (imonth - 2 - (imonth - 14) / 12 * 12) / 12
        - 3 * ((iyear + 4900 + (imonth - 14) / 12) / 100) / 4
    )
    return jdn


def read_NOAA_solar_file(
    solar_fname: Path,
) -> dict[str, float | int | dict[str, int | float | dict[str, float]]]:
    """
    function to read in a file of solar constants structured as:
        first_year last_year first_cycle last_cycle mean_value info_string
        year solar_constant
        year solar_constant
        . . .
        . .
        .  .
        *******************************************************************
        data_arrangement_info

    and parses the data into a dict of the format:
        iyr1: first_year
        iyr2: last_year
        icy1: first_cycle
        icy2: last_cycle
        smean: mean_value
        constants:
            year: solar_constant
    """
    solar_constant_data: dict = {}
    solar_constant_data["constants"] = {}
    sol_file = open(solar_fname)
    for i, line in enumerate(sol_file):
        if line.split():  # skip blank lines
            table_dat = line.split()
            if i == 0:
                solar_constant_data["iyr1"] = int(table_dat[0])
                solar_constant_data["iyr2"] = int(table_dat[1])
                solar_constant_data["icy1"] = int(table_dat[2])
                solar_constant_data["icy2"] = int(table_dat[3])
                solar_constant_data["smean"] = float(table_dat[4])
            elif re.fullmatch(r"^(\*)\1{1,}$", table_dat[0]) is not None:
                break  # end at the asterisks
            else:
                year = int(table_dat[0])
                solar_constant = float(table_dat[1])
                solar_constant_data["constants"][year] = solar_constant
    sol_file.close()
    return solar_constant_data


def assign_solar_constant_from_data(
    solar_constant_data: dict, year: int, isolflg: int
) -> float:
    """Gets solar constant for a year based on a dict of observed constants

    Args:
        solar_constant_data (dict): dictionary mapping years to
            the solar constants of those years
        year (int): Year to find the solar constant for
        isolflg (int): flag determining the type of data

    Returns:
        solc0: the solar constant for the given year
    """
    if not solar_constant_data:
        raise RuntimeError(
            "assign_solar_constant_from_data received an empty data dictionary!"
        )
    icy1 = solar_constant_data["icy1"]
    icy2 = solar_constant_data["icy2"]
    iyr1 = solar_constant_data["iyr1"]
    iyr2 = solar_constant_data["iyr2"]

    iyr = year
    # Check cycle range
    if iyr < iyr1:
        icy = icy1 - iyr1 + 1  # range of the earlest cycle in data table
        while iyr < iyr1:
            iyr += icy
        ndsl_log.info(f"year {year} out of table range, using closest cycle year {iyr}")
    elif iyr > iyr2:
        icy = iyr2 - icy2 + 1  # range of the latest cycle in data table
        while iyr > iyr2:
            iyr -= icy
        ndsl_log.info(f"year {year} out of table range, using closest cycle year {iyr}")
    if isolflg < 4:
        solc1 = solar_constant_data["constants"][iyr]
        solc0: float = solc1 + solar_constant_data["smean"]
    else:
        raise NotImplementedError(
            f"isol {isolflg} solar constant data assignment "
            "has not been implemented yet"
        )
    return solc0


def sol_init(
    isolar: int,
    solar_constant_file: Path,
    year: int,
) -> tuple[int, dict, float]:
    """
    Initializes solar constant data and sol flag used at runtime
    based on requested sol flag, external solar data, and starting year
    Original fortran docstring follows:
    !  ===================================================================  !
    !                                                                       !
    !  initialize astronomy process, set up module constants.               !
    !                                                                       !
    !  inputs:                                                              !
    !     me      - print message control flag                              !
    !                                                                       !
    !  outputs:  (to module variable)                                       !
    !     ( none )                                                          !
    !                                                                       !
    !  external module variable: (in physparam)                             !
    !   isolar    - = 0: use the old fixed solar constant in "physcon"      !
    !               =10: use the new fixed solar constant in "physcon"      !
    !               = 1: use noaa ann-mean tsi tbl abs-scale with cyc apprx !
    !               = 2: use noaa ann-mean tsi tbl tim-scale with cyc apprx !
    !               = 3: use cmip5 ann-mean tsi tbl tim-scale with cyc apprx!
    !               = 4: use cmip5 mon-mean tsi tbl tim-scale with cyc apprx!
    !   solar_file- external solar constant data table                      !
    !                                                                       !
    !  internal module variable:                                            !
    !   isolflg   - internal solar constant scheme control flag             !
    !   solc0     - solar constant  (w/m**2)                                !
    !   solar_fname-file name for solar constant table assigned based on    !
    !               the scheme control flag, isolflg.                       !
    !                                                                       !
    !  usage:    call sol_init                                              !
    !                                                                       !
    !  subprograms called:  none                                            !
    !                                                                       !
    !  ===================================================================  !
    """
    isolflg = isolar
    sol_const_data = {}
    if isolar == 0:
        solc0 = CON_SOLR_OLD
        ndsl_log.info(f" - Using old fixed solar constant = {solc0}")
    elif isolar == 10:
        solc0 = CON_SOLR
        ndsl_log.info(f" - Using new fixed solar constant = {solc0}")
    elif isolar == 2:  # noaa ann-mean tsi in tim scale
        ndsl_log.info(
            " - Using NOAA annual mean TSI table in TIM scale "
            "with cycle approximation (new values)!"
        )
        if not solar_constant_file.is_file():
            ndsl_log.warning(
                f"Requested solar data file {solar_constant_file} not found! "
                f"Using the default solar constant value {CON_SOLR}"
            )
            isolflg = 10
            solc0 = CON_SOLR
        else:
            sol_const_data = read_NOAA_solar_file(solar_constant_file)
            solc0 = assign_solar_constant_from_data(sol_const_data, year, isolflg)
    else:
        raise NotImplementedError(
            f"isolar {isolar} not implemented. Current options are 0, 2, and 10"
        )

    return isolflg, sol_const_data, solc0


def solar(
    jd: int,
    fjd: float,
) -> tuple[float, float, float, float, float, float]:
    """
    !  ===================================================================  !
    !                                                                       !
    !  solar computes radius vector, declination and right ascension of     !
    !  sun, and equation of time.                                           !
    !                                                                       !
    !  inputs:                                                              !
    !    jd       - julian day                                              !
    !    fjd      - fraction of the julian day                              !
    !                                                                       !
    !  outputs:                                                             !
    !    r1       - earth-sun radius vector                                 !
    !    dlt      - declination of sun in radians                           !
    !    alp      - right ascension of sun in radians                       !
    !                                                                       !
    !  module variables:                                                    !
    !    sollag   - equation of time in radians                             !
    !    sindec   - sine of declination angle                               !
    !    cosdec   - cosine of declination angle                             !
    !                                                                       !
    !  usage:    call solar                                                 !
    !                                                                       !
    !  external subroutines called: none                                    !
    !                                                                       !
    !  ===================================================================  !
    """
    # computes time in julian centuries after epoch

    t1 = float(jd - JDOR) / 36525.0

    # computes length of anomalistic and tropical years (minus 365 days)

    year = 0.25964134e0 + 0.304e-5 * t1
    tyear = 0.24219879e0 - 0.614e-5 * t1

    # computes orbit eccentricity and angle of earth's inclination from t

    ec = 0.01675104e0 - (0.418e-4 + 0.126e-6 * t1) * t1
    angin = 23.452294e0 - (0.0130125e0 + 0.164e-5 * t1) * t1

    ador = JDOR
    jdoe = ador + (SVT6 * CYEAR) / (year - tyear)

    # deleqn is updated svt6 for current date

    deleqn = float(jdoe - jd) * (year - tyear) / CYEAR
    year = year + 365.0
    sni = np.sin(angin / (180.0 / constants.PI))
    tini = 1.0 / np.tan(angin / (180.0 / constants.PI))
    er = np.sqrt((1.0 + ec) / (1.0 - ec))
    qq = deleqn * 2.0 * constants.PI / year

    # determine true anomaly at equinox
    e1 = 1.0
    cd = 1.0
    iter = 0

    while cd > CCR:
        ep = e1 - (e1 - ec * np.sin(e1) - qq) / (1.0 - ec * np.cos(e1))
        cd = abs(e1 - ep)
        e1 = ep
        iter = iter + 1
        if iter > 10:
            print(f"ITERATION COUNT FOR LOOP 32 = {iter}")
            print(f"E, EP, CD =', {e1}, {ep}, {cd}")
            break

    eq = 2.0 * np.arctan(er * np.tan(0.5 * e1))

    # date is days since last perihelion passage

    dat = float(jd - JDOR) - TPP + fjd
    date = dat % year

    # solve orbit equations by newton's method

    em = 2.0 * constants.PI * date / year
    e1 = 1.0
    cr = 1.0
    iter = 0

    while cr > CCR:
        ep = e1 - (e1 - ec * np.sin(e1) - em) / (1.0 - ec * np.cos(e1))
        cr = abs(e1 - ep)
        e1 = ep
        iter = iter + 1

        if iter > 10:
            print(f"ITERATION COUNT FOR LOOP 31 = {iter}")
            break

    w1 = 2.0 * np.arctan(er * np.tan(0.5 * e1))

    r1 = 1.0 - ec * np.cos(e1)

    sindec = sni * np.sin(w1 - eq)
    cosdec = np.sqrt(1.0 - sindec * sindec)

    dlt = np.arcsin(sindec)
    alp = np.arcsin(np.tan(dlt) * tini)

    tst = np.cos(w1 - eq)
    if tst < 0.0:
        alp = constants.PI - alp
    if alp < 0.0:
        alp = alp + 2.0 * constants.PI

    sun = 2.0 * constants.PI * (date - deleqn) / year
    if sun < 0.0:
        sun = sun + 2.0 * constants.PI
    sollag = sun - alp - 0.03255e0
    return r1, dlt, alp, sollag, sindec, cosdec


def solar_update(
    sdate: datetime.datetime,
    solc0: float,
    deltsw: float,
    deltim: float,
    lsol_chg: bool,
    iyr_sav: int,
    isolflg: int,
    solar_constant_data: dict = None,
) -> tuple[float, float, float, float, float, float, int, int]:
    """
    Updates solar parameters during model steps, including solar constant
    and incident angle data. Original Fortran docstring follows:
    !  ===================================================================  !
    !                                                                       !
    !  sol_update computes solar parameters at forecast time                !
    !                                                                       !
    !  inputs:                                                              !
    !     jdate(8)- ncep absolute date and time at fcst time                !
    !                (yr, mon, day, t-zone, hr, min, sec, mil-sec)          !
    !     sdate(8)- absolute date and time for solar calculations           !
    !                (yr, mon, day, t-zone, hr, min, sec, mil-sec)          !
    !     kyear   - usually kyear=jdate(1). if not, it is for hindcast mode,!
    !               and it is usually the init cond time and serves as the  !
    !               upper limit of data can be used.                        !
    !     deltsw  - time duration in seconds per sw calculation             !
    !     deltim  - timestep in seconds                                     !
    !     lsol_chg- logical flags for change solar constant                 !
    !     me      - print message control flag                              !
    !                                                                       !
    !  outputs:                                                             !
    !    slag          - equation of time in radians                        !
    !    sdec, cdec    - sin and cos of the solar declination angle         !
    !    solcon        - sun-earth distance adjusted solar constant (w/m2)  !
    !                                                                       !
    !                                                                       !
    !  module variable:                                                     !
    !   solc0   - solar constant  (w/m**2) not adjusted by earth-sun dist   !
    !   isolflg - solar constant control flag                               !
    !             = 0: use the old fixed solar constant                     !
    !             =10: use the new fixed solar constant                     !
    !             = 1: use noaa ann-mean tsi tbl abs-scale with cycle apprx !
    !             = 2: use noaa ann-mean tsi tbl tim-scale with cycle apprx !
    !             = 3: use cmip5 ann-mean tsi tbl tim-scale with cycle apprx!
    !             = 4: use cmip5 mon-mean tsi tbl tim-scale with cycle apprx!
    !   solar_fname-external solar constant data table                      !
    !   sindec  - sine of the solar declination angle                       !
    !   cosdec  - cosine of the solar declination angle                     !
    !   anginc  - solar angle increment per iteration for cosz calc         !
    !   nstp    - total number of zenith angle iterations                   !
    !   smon_sav- saved monthly solar constants (isolflg=4 only)            !
    !   iyr_sav - saved year  of data previously used                       !
    !                                                                       !
    !  usage:    call sol_update                                            !
    !                                                                       !
    !  subprograms called:  solar, prtime                                   !
    !                                                                       !
    !  external functions called: iw3jdn                                    !
    !                                                                       !
    !  ===================================================================  !
    """
    iyear = sdate.year
    imon = sdate.month
    iday = sdate.day
    ihr = sdate.hour
    imin = sdate.minute
    isec = sdate.second
    if lsol_chg:  # get solar constant from data table
        if iyear == iyr_sav:  # same year, no new reading necessary
            if isolflg == 4:
                raise NotImplementedError("isolflg = 4 not implemented")
        else:  # need to read in new data
            iyr_sav = iyear
            # TODO finish all of this
            solc0 = assign_solar_constant_from_data(solar_constant_data, iyear, isolflg)

    # calculate forecast julian day and fraction of julian day
    jd1 = date_to_julian(iyear, imon, iday)

    # unlike in normal applications, where day starts from 0 hr,
    # julian day numbers, day stats from noon.
    if ihr < 12:
        jd1 -= 1
        fjd1 = 0.5 + float(ihr) * HRDAY + float(imin) * MINDAY + float(isec) * SECDAY
    else:
        fjd1 = float(ihr - 12) * HRDAY + float(imin) * MINDAY + float(isec) * SECDAY
    fjd1 = fjd1 + jd1
    jd = int(fjd1)
    fjd = fjd1 - jd

    r1, dlt, alp, slag, sdec, cdec = solar(jd, fjd)
    # calculate sun-earth distance adjustment factor appropriate to date
    solcon = solc0 / (r1 * r1)

    # TODO: Print out solar info and time here

    # setting up calculation parameters used by subr coszmn

    nswr = np.round(deltsw / deltim)  # number of mdl t-step per sw call
    dtswh = deltsw / 3600.0  # time length in hours

    if deltsw >= 3600.0:  # for longer sw call interval
        nn = max(6, min(12, Int(3600.0 / deltim)))  # num of calc per hour
        nstp = Int(dtswh) * nn + 1  # num of calc per sw call
    else:  # for shorter sw sw call interval
        nstp = max(2, min(20, nswr)) + 1

    anginc = (
        (constants.PI / 12.0) * dtswh / float(nstp - 1)
    )  # solar angle inc during each calc step

    return slag, sdec, cdec, anginc, solcon, solc0, nstp, iyr_sav


def coszmn(
    xlon: FloatFieldIJ,
    xlat: FloatFieldIJ,
    coslat: FloatFieldIJ,
    solhr: Float,
    sollag: Float,
    sindec: Float,
    cosdec: Float,
    anginc: Float,
    coszen: FloatFieldIJ,
    coszdg: FloatFieldIJ,
    daymask: BoolFieldIJ,
):
    """
    !  ===================================================================  !
    !                                                                       !
    !  coszmn computes mean cos solar zenith angle over sw calling interval !
    !                                                                       !
    !  inputs:                                                              !
    !    xlon  (IM)    - grids' longitudes in radians, work both on zonal   !
    !                    0->2pi and -pi->+pi arrangements                   !
    !    sinlat(IM)    - sine of the corresponding latitudes                !
    !    coslat(IM)    - cosine of the corresponding latitudes              !
    !    solhr         - time after 00z in hours                            !
    !    IM            - num of grids in horizontal dimension               !
    !    me            - print message control flag                         !
    !    daily_mean    - replace cosz with daily mean value                 !
    !    fixed_sollat  - fix solar latitude                                 !
    !    sollat        - latitude the solar position fixed to (-90. to 90.) !
    !                                                                       !
    !  outputs:                                                             !
    !    coszen(IM)    - average of cosz for daytime only in sw call interval
    !    coszdg(IM)    - average of cosz over entire sw call interval       !
    !                                                                       !
    !  module variables:                                                    !
    !    sollag        - equation of time                                   !
    !    sindec        - sine of the solar declination angle                !
    !    cosdec        - cosine of the solar declination angle              !
    !    anginc        - solar angle increment per iteration for cosz calc  !
    !    nstp          - total number of zenith angle iterations            !
    !                                                                       !
    !  usage:    call comzmn                                                !
    !                                                                       !
    !  external subroutines called: none                                    !
    !                                                                       !
    !  ===================================================================  !
    """
    from __externals__ import daily_mean, fixed_sollat, nstp, sollat

    with computation(FORWARD), interval(0, 1):
        sinlat = sin(xlat)
        coslat = cos(xlat)
        daymask = False

        solar_angle = (constants.PI / 12.0) * (
            solhr - 12.0
        )  # solar angle at present time
        rstp = 1.0 / nstp

        coszen = 0.0
        istsun = 0

        iter = 0
        while iter < nstp:
            cns = solar_angle + (iter - 1.0) * anginc + sollag
            if fixed_sollat:
                ss = sin(sollat * constants.PI / 180.0) * sindec
                cc = cos(sollat * constants.PI / 180.0) * cosdec
            else:
                ss = sinlat * sindec
                cc = coslat * cosdec

            if iter == 0:
                h = acos(min(max(-ss / cc, -1.0), 1.0))
                coszenm = ss * h / constants.PI + cc * (sin(h) - sin(-h)) / (
                    2 * constants.PI
                )
            coszn = ss + cc * cos(cns + xlon)
            coszen = coszen + max(0.0, coszn)
            if coszn > CZLIMT:
                istsun = istsun + 1
            iter += 1

        coszdg = coszen * rstp
        if istsun > 0:
            coszen = coszen / istsun

        if daily_mean:
            coszdg = coszenm
            coszen = coszenm
        if coszen >= 0.0001:
            daymask = True
