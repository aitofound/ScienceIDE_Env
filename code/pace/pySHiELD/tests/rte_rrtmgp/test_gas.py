from pathlib import Path

import numpy as np
import pytest
import xarray as xr

from ndsl import constants as constants
from pyshield.radiation.rad_gases import (
    CH4VMR_DEF,
    CL4VMR_DEF,
    CO2VMR_DEF,
    COVMR_DEF,
    F11VMR_DEF,
    F12VMR_DEF,
    F22VMR_DEF,
    F113VMR_DEF,
    N2OVMR_DEF,
    N2VMR_DEF,
    O2VMR_DEF,
    broadcast_co2_to_grid,
    co2_update,
    gas_init,
    read_co2_files,
)


@pytest.mark.parametrize("datapath", ["test_data/"])
@pytest.mark.parametrize(
    "prefix, year, month, expected_glb, expected_mon, expected_cyc",
    [
        pytest.param(
            "global_",
            1977,
            1,
            [1765, 1990, [333.16, 333.84, 1.33, 1.35]],
            [333.93, 336.32],
            [1976, 2009, 358.20, 0.44, 2.93],
            id="jan_1977",
        ),
    ],
)
def test_dataread(
    datapath, prefix, year, month, expected_glb, expected_mon, expected_cyc
):
    co2_glb_data, co2_mvr_data, co2_cyc_data = read_co2_files(Path(datapath), prefix)
    assert co2_glb_data["start_year"] == expected_glb[0]
    assert co2_glb_data["end_year"] == expected_glb[1]
    assert co2_glb_data[year] == [val * 1.0e-6 for val in expected_glb[2]]
    assert co2_mvr_data[year]["mean"] == expected_mon[0] * 1.0e-6
    assert co2_mvr_data[year][month][0][0] == expected_mon[1] * 1.0e-6
    assert co2_cyc_data["start_year"] == expected_cyc[0]
    assert co2_cyc_data["end_year"] == expected_cyc[1]
    assert co2_cyc_data["annual_mean"] == expected_cyc[2] * 1.0e-6
    assert co2_cyc_data[month]["mean"] == expected_cyc[3] * 1.0e-6
    assert co2_cyc_data[month]["data"][0][0] == expected_cyc[4] * 1.0e-6


@pytest.mark.parametrize(
    "expect",
    [
        pytest.param(
            np.array(
                [
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        0.0,
                        1.0,
                        1.0,
                        1.0,
                        1.0,
                    ],  # fmt: skip
                    [
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        25.0,
                        25.0,
                        25.0,
                        25.0,
                    ],  # fmt: skip
                    [
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        25.0,
                        25.0,
                        25.0,
                        25.0,
                    ],  # fmt: skip
                    [
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        25.0,
                        25.0,
                        25.0,
                        25.0,
                    ],  # fmt: skip
                    [
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        24.0,
                        25.0,
                        25.0,
                        25.0,
                        25.0,
                    ],  # fmt: skip
                ]
            ),
            id="simple_test",
        ),
    ],
)
def test_broadcast_co2_to_grid(expect):
    test_data = []
    for i in range(12):
        test_data.append([j + 24.0 * i for j in range(24)])
    gridlon = np.ones((20, 20))
    for i in range(gridlon.shape[0]):
        gridlon[:, i] = i * constants.PI / 180.0
    gridlat = np.ones((20, 20))
    for i in range(gridlon.shape[1]):
        gridlat[i, :] = (90 - i) * constants.PI / 180.0
    griddat = broadcast_co2_to_grid(test_data, gridlon, gridlat)
    assert np.all(griddat == expect)


# TODO add tests for ictmflg == -1, 0, -2, yyyy0, yyyy1 with ico2flg == 1, 2?
@pytest.mark.parametrize("datapath", ["test_data/"])
@pytest.mark.parametrize(
    "ico2flg, ioznflg, ictmflg, iyear, imonth, expco2",
    [
        pytest.param(
            0,
            1,
            0,
            2000,
            1,
            [CO2VMR_DEF, CO2VMR_DEF, 0.0],
            id="0_1_0_jan_2000",
        ),
        pytest.param(
            1,
            1,
            -1,
            1990,
            1,
            [353.745e-6, 353.745e-6, 0.0],
            id="1_1_-1_jan_1990",
        ),
        pytest.param(
            2,
            1,
            -1,
            1990,
            1,
            [354.53e-6, 352.03e-6, 0.0],
            id="2_1_-1_jan_1990",
        ),
        pytest.param(
            1,
            1,
            1,
            1990,
            1,
            [CO2VMR_DEF, 0.0, 0.0],
            id="1_1_1_jan_1990",
        ),
        pytest.param(
            2,
            1,
            1,
            2000,
            1,
            [CO2VMR_DEF, 0.0, 0.0],
            id="2_1_1_jan_2000",
        ),
        pytest.param(
            2,
            1,
            1,
            2020,
            1,
            [CO2VMR_DEF, 0.0, 0.0],
            id="2_1_1_jan_2020",
        ),
    ],
)
def test_gas_init(
    datapath,
    ico2flg,
    ioznflg,
    ictmflg,
    iyear,
    imonth,
    expco2,
):
    prefix = "global_"
    sdat = xr.open_dataset(
        Path(datapath).joinpath("sfc_data.tile1.nc"), engine="netcdf4"
    )

    gridlon = sdat.geolon[:] * constants.PI / 180.0
    gridlat = sdat.geolat[:] * constants.PI / 180.0
    arrays = gas_init(
        Path(datapath),
        ico2flg,
        ioznflg,
        ictmflg,
        iyear,
        imonth,
        gridlon,
        gridlat,
        prefix,
    )
    co2arr = arrays[10]
    co2cyc = arrays[11]

    assert arrays[:10] == (
        N2OVMR_DEF,
        CH4VMR_DEF,
        O2VMR_DEF,
        COVMR_DEF,
        N2VMR_DEF,
        F11VMR_DEF,
        F12VMR_DEF,
        F22VMR_DEF,
        F113VMR_DEF,
        CL4VMR_DEF,
    )
    assert np.isclose(arrays[10], expco2[0])
    if ico2flg == 0:
        assert arrays[-3:] == (None, None, None)
        assert np.all(np.isclose(arrays[11], expco2[1]))
        assert np.all(np.isclose(arrays[12], expco2[2]))
    else:
        assert np.isclose(arrays[11][0, 0], expco2[1])
        assert np.isclose(arrays[12][0, 0], expco2[2])
        for dat in arrays[-3:]:
            assert dat


# TODO add tests for ictmflg == -1, 0, -2, yyyy0, yyyy1 with ico2flg == 1, 2
@pytest.mark.parametrize("datapath", ["test_data/"])
@pytest.mark.parametrize(
    "ico2flg, ioznflg, ictmflg, iyear, imon, saved_year, saved_month, expco2",
    [
        pytest.param(
            0,
            1,
            0,
            2000,
            1,
            2000,
            1,
            [CO2VMR_DEF, CO2VMR_DEF, 0.0],
            id="internal_const_jan_2000",
        ),
        pytest.param(
            2,
            1,
            0,
            1990,
            1,
            1990,
            1,
            [354.53e-6, 352.03e-6, 0.0],
            id="read2_const_mon_jan_1990",
        ),
        pytest.param(
            1,
            1,
            0,
            1990,
            1,
            1990,
            1,
            [353.745e-6, 353.745e-6, 0.0],
            id="read2_const_jan_1990",
        ),
        pytest.param(
            1,
            1,
            0,
            2020,
            1,
            2020,
            1,
            [353.745e-6, 353.745e-6, 0.0],
            id="ext_const_ood_jan_2020",
        ),
        pytest.param(
            2,
            1,
            0,
            2020,
            1,
            2020,
            1,
            [394.90e-6, 391.74e-6, 0.0],
            id="ext_month_ood_jan_2020",
        ),
        # pytest.param(
        #     1,
        #     1,
        #     -1,
        #     2020,
        #     1,
        #     1990,
        #     1,
        #     [353.745e-6, 353.745e-6, 0.0],
        #     id="external_const_jan_2020",
        # ),
        # pytest.param(
        #     2,
        #     1,
        #     -1,
        #     2020,
        #     1,
        #     1990,
        #     1,
        #     [0.0e-6, 0.0e-6, 0.0],
        #     id="external_monthly_const_jan_2020",
        # ),
        # pytest.param(
        #     1,
        #     1,
        #     1,
        #     1990,
        #     1,
        #     [],
        #     id="read_ann_jan_1990",
        # ),
        # pytest.param(
        #     2,
        #     1,
        #     1,
        #     2000,
        #     1,
        #     [],
        #     id="read_month_jan_2000",
        # ),
        # pytest.param(
        #     2,
        #     1,
        #     1,
        #     2020,
        #     1,
        #     [],
        #     id="extrap_month_jan_2020",
        # ),
        # pytest.param(
        #     1,
        #     1,
        #     1,
        #     2020,
        #     1,
        #     [],
        #     id="extrap_ann_jan_2020",
        # ),
    ],
)
def test_co2_update(
    datapath,
    iyear,
    imon,
    saved_year,
    saved_month,
    ico2flg,
    ictmflg,
    ioznflg,
    expco2,
):
    prefix = "global_"
    sdat = xr.open_dataset(
        Path(datapath).joinpath("sfc_data.tile1.nc"), engine="netcdf4"
    )

    gridlon = sdat.geolon[:] * constants.PI / 180.0
    gridlat = sdat.geolat[:] * constants.PI / 180.0
    (
        _,
        _,
        _,
        _,
        _,
        _,
        _,
        _,
        _,
        _,
        co2_glb,
        co2_arr,
        co2_cyc,
        co2_mvr_data,
        co2_glb_data,
        co2_cyc_data,
    ) = gas_init(
        Path(datapath),
        ico2flg,
        ioznflg,
        ictmflg,
        saved_year,
        saved_month,
        gridlon,
        gridlat,
        prefix,
    )
    co2_glb, co2_arr, co2_cyc = co2_update(
        saved_year,
        saved_month,
        ico2flg,
        True,
        ictmflg,
        co2_glb,
        co2_arr,
        co2_cyc,
        gridlon,
        gridlat,
        co2_glb_data,
        co2_mvr_data,
        co2_cyc_data,
    )
    ldoco2 = imon != saved_month
    co2_glb, co2_arr, co2_cyc = co2_update(
        iyear,
        imon,
        ico2flg,
        ldoco2,
        ictmflg,
        co2_glb,
        co2_arr,
        co2_cyc,
        gridlon,
        gridlat,
        co2_glb_data,
        co2_mvr_data,
        co2_cyc_data,
    )
    assert np.isclose(co2_glb, expco2[0])
    if ico2flg == 0:
        assert np.all(np.isclose(co2_arr, expco2[1]))
        assert np.all(np.isclose(co2_cyc, expco2[2]))
    elif (ictmflg in [-1, 0]) and (ico2flg == 1):
        assert np.all(np.isclose(co2_arr, expco2[1]))
        assert np.all(np.isclose(co2_cyc, expco2[2]))
    else:
        assert np.all(np.isclose(co2_arr[0, 0], expco2[1]))
        assert np.all(np.isclose(co2_cyc[0, 0], expco2[2]))
