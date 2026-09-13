import numpy as np
from f90nml import Namelist

from ndsl import StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.gt4py import FORWARD, PARALLEL, computation, interval
from ndsl.dsl.typing import FloatField, FloatFieldIJ
from pyshield.functions.physics_functions import fpvs, fpvsx
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


def set_xval(xin: FloatField, xout: FloatField):
    with computation(PARALLEL), interval(...):
        xout = xin


def test_fpvs(
    temp: FloatFieldIJ,
    fp: FloatFieldIJ,
    fpx: FloatFieldIJ,
):
    with computation(FORWARD), interval(0, 1):
        fp = fpvs(temp)
        fpx = fpvsx(temp)


def test_table(
    xval: FloatField,
    tab_fpvs: FloatField,
    tab_fpvsx: FloatField,
):
    with computation(PARALLEL), interval(...):
        tab_fpvs = fpvs(xval)
        tab_fpvsx = fpvsx(xval)


class FPVS:
    def __init__(
        self,
        stencil_factory,
        quantity_factory,
        xmin,
        xmax,
        nxpvs,
        xinc,
    ):
        print("SAVED VALUES ARE ", xmin, xmax, nxpvs, xinc)
        # nt = ((np.arange(24 * 24 * 91) + 1) % 7501).reshape(24, 24, 91)
        nt = np.zeros((32, 18, 91))
        for i in range(32):
            for j in range(18):
                for k in range(91):
                    nt[i, j, k] = (i + 1) * 91 + k
        nt = nt.reshape((24, 24, 91))
        xx = xmin + (nt - 1) * xinc
        xx = np.pad(xx, ((3, 4), (3, 4), (0, 1)))
        self._x = quantity_factory.from_array(
            xx,
            [I_DIM, J_DIM, K_DIM],
            units="unknown",
        )
        grid_indexing = stencil_factory.grid_indexing

        self._set_xval = stencil_factory.from_origin_domain(
            set_xval,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )

        self._test_fpvs = stencil_factory.from_origin_domain(
            test_fpvs,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )
        self._test_table = stencil_factory.from_origin_domain(
            test_table,
            origin=grid_indexing.origin_compute(),
            domain=grid_indexing.domain_compute(),
        )

    def __call__(
        self,
        temp: FloatFieldIJ,
        fp: FloatFieldIJ,
        fpx: FloatFieldIJ,
        xval: FloatField,
        tab_fpvs: FloatField,
        tab_fpvsx: FloatField,
    ):
        self._set_xval(
            self._x,
            xval,
        )
        self._test_fpvs(temp, fp, fpx)
        self._test_table(xval, tab_fpvs, tab_fpvsx)


class TranslateFPVS(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {
            "temp": {"shield": True},
            "tab_fpvsx": {"shield": True},
            "tab_fpvs": {"shield": True},
            "xval": {"shield": True},
            "fp": {"shield": True},
            "fpx": {"shield": True},
        }
        self.in_vars["parameters"] = [
            "xmin",
            "xmax",
            "nxpvs",
            "xinc",
        ]
        self.out_vars = {
            "tab_fpvsx": {"shield": True},
            "tab_fpvs": {"shield": True},
            "xval": {"shield": True},
            "fp": {"shield": True},
            "fpx": {"shield": True},
        }
        self.stencil_factory = stencil_factory

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        self.compute_func = FPVS(
            self.stencil_factory,
            quantity_factory=self.grid.quantity_factory,
            xmin=inputs.pop("xmin"),
            xmax=inputs.pop("xmax"),
            nxpvs=inputs.pop("nxpvs"),
            xinc=inputs.pop("xinc"),
        )
        self.compute_func(**inputs)
        return self.slice_output(inputs)
