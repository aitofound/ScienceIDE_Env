from contextlib import nullcontext
from math import cos, sin
from pathlib import Path

import numpy as np
import pytest

from ndsl import constants as constants
from pyshield.radiation.rad_astro import (
    date_to_julian,
    read_NOAA_solar_file,
    sol_init,
    solar_update,
)


@pytest.mark.parametrize("datapath", ["test_data/global_solarconstant_noaa_an.txt"])
@pytest.mark.parametrize(
    "year, expected",
    [
        pytest.param(
            2000,
            nullcontext(1361.7800),
            id="2000",
        ),
        pytest.param(
            1833,
            pytest.raises(KeyError),
            id="1833",
        ),
    ],
)
def test_read_data(datapath, year, expected):
    data_dict = read_NOAA_solar_file(datapath)
    with expected as e:
        assert (data_dict["constants"][year]) == e


@pytest.mark.parametrize("datapath", ["test_data/global_solarconstant_noaa_an.txt"])
@pytest.mark.parametrize(
    "isolar, year, expected",
    [
        pytest.param(
            0,
            1990,
            1.3660e3,
            id="old_constant",
        ),
        pytest.param(
            10,
            1990,
            1.3608e3,
            id="new_constant",
        ),
        pytest.param(
            2,
            1990,
            1361.5300,
            id="data_read",
        ),
        pytest.param(
            2,
            2020,
            1360.2900,
            id="data_extrap",
        ),
    ],
)
def test_solinit(datapath, isolar, year, expected):
    _, sol_const_data, solc0 = sol_init(isolar, Path(datapath), year)
    assert isinstance(sol_const_data, dict)
    assert solc0 == expected


@pytest.mark.parametrize(
    "date, expected",
    [
        pytest.param([2020, 1, 1, 0, 0, 0], 2458849.5, id="midnight"),
        pytest.param([2020, 1, 1, 12, 0, 0], 2458850.0, id="noon"),
    ],
)
def test_julian_date(date, expected):
    jd1 = date_to_julian(date[0], date[1], date[2])
    if date[3] < 12:
        jd1 -= 1
        fjd1 = (
            0.5
            + float(date[3]) * (1.0 / 24.0)
            + float(date[4]) * (1.0 / 1440.0)
            + float(date[5]) * (1.0 / 86400.0)
        )
    else:
        fjd1 = (
            float(date[3] - 12) * (1.0 / 24.0)
            + float(date[4]) * (1.0 / 1440.0)
            + float(date[5]) * (1.0 / 86400.0)
        )
    fjd1 = fjd1 + jd1
    assert fjd1 == expected


@pytest.mark.parametrize("datapath", ["test_data/global_solarconstant_noaa_an.txt"])
@pytest.mark.parametrize(
    "isolar, sdate, deltsw, deltim, iyr_sav, exp_slag, exp_solcon, exp_dec",
    [
        pytest.param(
            2,
            [2020, 1, 1, 0, 0, 0],
            3600.0,
            225.0,
            2019,
            -0.013631,
            1406.8144765,
            -23.0578509,
            id="test_20200101",
        ),
        pytest.param(
            2,
            [2020, 1, 1, 0, 0, 0],
            3600.0,
            225.0,
            2020,
            -0.013631,
            1406.8144765,
            -23.0578509,
            id="test_20200101-noupdate",
        ),
    ],
)
def test_solar_update(
    datapath,
    sdate,
    deltsw,
    deltim,
    iyr_sav,
    isolar,
    exp_slag,
    exp_solcon,
    exp_dec,
):
    isolflag, sol_const_data, solc0 = sol_init(isolar, Path(datapath), iyr_sav)
    lsol_chg = (isolflag not in [0, 10]) and sdate[0] != iyr_sav
    (
        slag,
        sdec,
        cdec,
        anginc,
        solcon,
        solc0,
        nstp,
        saved_iyear,
    ) = solar_update(
        sdate,
        solc0,
        deltsw,
        deltim,
        lsol_chg,
        iyr_sav,
        isolflag,
        sol_const_data,
    )
    rad_dec = exp_dec * constants.PI / 180.0
    exp_sdec = sin(rad_dec)
    exp_cdec = cos(rad_dec)
    assert np.isclose(slag, exp_slag)
    assert np.isclose(solcon, exp_solcon)
    assert np.isclose(sdec, exp_sdec)
    assert np.isclose(cdec, exp_cdec)
