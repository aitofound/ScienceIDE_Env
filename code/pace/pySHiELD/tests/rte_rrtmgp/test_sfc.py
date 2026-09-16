from pathlib import Path

import numpy as np
import pytest
import xarray

from ndsl import constants as constants
from pyshield.radiation.rad_sfc import (
    IMXEMS,
    JMXEMS,
    OCEAN_ALBEDO,
    map_sfc_to_grid,
    set_albedo,
    set_sfcemis,
    sfc_init,
)


@pytest.mark.parametrize(
    "gridres, expected",
    [
        pytest.param(
            0.5,
            np.array(
                [
                    [0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 4.0, 4.0],
                    [0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 4.0, 4.0],
                    [
                        360.0,
                        360.0,
                        361.0,
                        361.0,
                        362.0,
                        362.0,
                        363.0,
                        363.0,
                        364.0,
                        364.0,
                    ],
                    [
                        360.0,
                        360.0,
                        361.0,
                        361.0,
                        362.0,
                        362.0,
                        363.0,
                        363.0,
                        364.0,
                        364.0,
                    ],
                    [
                        720.0,
                        720.0,
                        721.0,
                        721.0,
                        722.0,
                        722.0,
                        723.0,
                        723.0,
                        724.0,
                        724.0,
                    ],
                    [
                        720.0,
                        720.0,
                        721.0,
                        721.0,
                        722.0,
                        722.0,
                        723.0,
                        723.0,
                        724.0,
                        724.0,
                    ],
                    [
                        1080.0,
                        1080.0,
                        1081.0,
                        1081.0,
                        1082.0,
                        1082.0,
                        1083.0,
                        1083.0,
                        1084.0,
                        1084.0,
                    ],
                    [
                        1080.0,
                        1080.0,
                        1081.0,
                        1081.0,
                        1082.0,
                        1082.0,
                        1083.0,
                        1083.0,
                        1084.0,
                        1084.0,
                    ],
                    [
                        1440.0,
                        1440.0,
                        1441.0,
                        1441.0,
                        1442.0,
                        1442.0,
                        1443.0,
                        1443.0,
                        1444.0,
                        1444.0,
                    ],
                    [
                        1440.0,
                        1440.0,
                        1441.0,
                        1441.0,
                        1442.0,
                        1442.0,
                        1443.0,
                        1443.0,
                        1444.0,
                        1444.0,
                    ],
                ]
            ),
            id="half-degree",
        ),
    ],
)
def test_map_sfc_to_grid(gridres, expected):
    test_data = np.ones((IMXEMS, JMXEMS))
    for i in range(IMXEMS):
        test_data[i, :] *= i
    for j in range(JMXEMS):
        test_data[:, j] += IMXEMS * j
    gridlon = np.ones((10, 10))
    for i in range(gridlon.shape[0]):
        gridlon[:, i] = gridres * i * constants.PI / 180.0
    gridlat = np.ones((10, 10))
    for i in range(gridlon.shape[1]):
        gridlat[i, :] = (
            90.0 - gridres * i
        ) * constants.PI / 180.0 - 0.000000001  # for stability
    griddat = map_sfc_to_grid(test_data, gridlon, gridlat)
    assert np.all(griddat == expected)


@pytest.mark.parametrize(
    "sfcemis_datafile", ["test_data/global_sfc_emissivity_idx.txt"]
)
@pytest.mark.parametrize(
    "ialbflg, iemsflg, ldisable_radiation_quasi_sea_ice, expected,",
    [
        pytest.param(
            1,
            1,
            False,
            [1, 1],
            id="test1",
        ),
        pytest.param(
            0,
            10,
            False,
            [0, None],
            id="test1",
        ),
    ],
)
def test_sfc_init(
    sfcemis_datafile,
    ialbflg,
    iemsflg,
    ldisable_radiation_quasi_sea_ice,
    expected,
):
    data = Path(sfcemis_datafile)
    iemslw, ext_sfcemis_data = sfc_init(
        ialbflg, iemsflg, ldisable_radiation_quasi_sea_ice, data
    )
    assert iemslw == expected[0]
    if iemslw == 1:
        assert ext_sfcemis_data[0, 0] == expected[1]
    else:
        assert ext_sfcemis_data == expected[1]


# TODO: add tests for ialbflg == 0, 1, 2
@pytest.mark.parametrize(
    "sfc_data, ialbflg, drqsi, expected",
    [
        pytest.param(
            "test_data/sfc_data.tile1.nc",
            -1,
            False,
            OCEAN_ALBEDO,
            id="constant_albedo",
        ),
    ],
)
def test_set_albedo(sfc_data, ialbflg, drqsi, expected):
    if ialbflg != -1:
        sfcdat = xarray.open_dataset(Path(sfc_data), engine="netcdf4")
        islmsk = sfcdat.slmsk[:]
        sfcalb = np.zeros((islmsk.shape[0], islmsk.shape[1], 4))
        alvsf = sfcdat.alvsf[:]
        alnsf = sfcdat.alnsf[:]
        alvwf = sfcdat.alvwf[:]
        alnwf = sfcdat.alnwf[:]
        snowf = sfcdat.snwdph[:]
        snoalb = sfcdat.snoalb[:]
        fice = sfcdat.fice[:]
        tisfc = sfcdat.tisfc[:]
        tsknf = sfcdat.t2m[:]
        zorlf = sfcdat.zorl[:]
        facsf = sfcdat.facsf[:]
        facwf = sfcdat.facwf[:]
        sncovr = sfcdat.sncovr[:]
        hprif = sfcdat.hprif[:]
        lsmalbedo = np.ones_like(zorlf) * 0.98
        coszf = np.ones_like(zorlf) * 0.6
    else:
        nx = 20
        sfcalb = np.zeros((nx, nx, 4))
        islmsk = np.zeros((nx, nx))
        alvsf = islmsk
        alnsf = islmsk
        alvwf = islmsk
        alnwf = islmsk
        snowf = islmsk
        snoalb = islmsk
        fice = islmsk
        tisfc = islmsk
        tsknf = islmsk
        zorlf = islmsk
        facsf = islmsk
        facwf = islmsk
        sncovr = islmsk
        hprif = islmsk
        lsmalbedo = islmsk
        coszf = islmsk
    set_albedo(
        ialbflg,
        islmsk,
        snowf,
        sncovr,
        snoalb,
        zorlf,
        coszf,
        tsknf,
        hprif,
        alvsf,
        alnsf,
        alvwf,
        alnwf,
        facsf,
        facwf,
        fice,
        tisfc,
        lsmalbedo,
        sfcalb,
        drqsi,
    )
    assert sfcalb[0, 0, 0] == expected


# TODO: add tests for iemsflg == 1, 2
@pytest.mark.parametrize(
    "sfc_data, sfc_emis, iemsflg, ialbflg, drqsi, expected",
    [
        pytest.param(
            "test_data/sfc_data.tile1.nc",
            "test_data/global_sfc_emissivity_idx.txt",
            0,
            -1,
            False,
            1.0,
            id="constant_sfcemis",
        ),
    ],
)
def test_set_sfcemis(sfc_data, sfc_emis, iemsflg, ialbflg, drqsi, expected):
    iemslw, ext_sfcemis_data = sfc_init(ialbflg, iemsflg, drqsi, Path(sfc_emis))
    if iemslw == 0:
        nx = 10
        gridlon = np.ones((nx, nx))
        gridres = 0.5
        for i in range(gridlon.shape[0]):
            gridlon[:, i] = gridres * i * constants.PI / 180.0
        gridlat = np.ones((10, 10))
        for i in range(gridlon.shape[1]):
            gridlat[i, :] = (
                90.0 - gridres * i
            ) * constants.PI / 180.0 - 0.000000001  # for stability
        islmsk = gridlon
        snowf = gridlon
        sncovr = gridlon
        zorlf = gridlon
        tskin = gridlon
        hprif = gridlon
    else:
        sfcdat = xarray.open_dataset(Path(sfc_data), engine="netcdf4")
        gridlon = sfcdat.geolon[:] * constants.PI / 180.0
        gridlat = sfcdat.geolat[:] * constants.PI / 180.0
        islmsk = sfcdat.slmsk[:]
        snowf = sfcdat.snwdph[:]
        zorlf = sfcdat.zorl[:]
        sncovr = sfcdat.sncovr[:]
        hprif = sfcdat.hprif[:]
        tskin = sfcdat.t2m[:]
    sfcemis_lsm = np.ones_like(gridlon)
    sfcemis = np.zeros((gridlon.shape[0], gridlon.shape[1]))
    set_sfcemis(
        gridlon,
        gridlat,
        islmsk,
        snowf,
        sncovr,
        zorlf,
        tskin,
        hprif,
        iemslw,
        ialbflg,
        drqsi,
        sfcemis,
        ext_sfcemis_data,
        sfcemis_lsm,
    )
    assert sfcemis[0, 0] == expected
