from ndsl import StencilFactory
from ndsl.dsl.gt4py import PARALLEL, computation, interval
from ndsl.dsl.typing import FloatField
from ndsl.stencils.testing import pad_field_in_j
from pyfv3.stencils import moist_cv
from pyfv3.testing import TranslateDycoreFortranData2Py


def moist_pt(
    qvapor: FloatField,
    qliquid: FloatField,
    qrain: FloatField,
    qsnow: FloatField,
    qice: FloatField,
    qgraupel: FloatField,
    q_con: FloatField,
    pt: FloatField,
    cappa: FloatField,
    delp: FloatField,
    delz: FloatField,
    r_vir: float,
):
    with computation(PARALLEL), interval(...):
        cvm, gz, q_con, cappa, pt = moist_cv.moist_pt_func(
            qvapor,
            qliquid,
            qrain,
            qsnow,
            qice,
            qgraupel,
            q_con,
            pt,
            cappa,
            delp,
            delz,
            r_vir,
        )


class MoistPT:
    """
    Class to test with DaCe orchestration. test class is MoistCVPlusPt_2d
    """

    def __init__(
        self,
        stencil_factory: StencilFactory,
        grid,
    ):
        self._moist_cv_pt = stencil_factory.from_origin_domain(
            moist_pt,
            origin=grid.compute_origin(),
            domain=(grid.nic, 1, grid.npz),
        )

    def __call__(
        self,
        qvapor: FloatField,
        qliquid: FloatField,
        qrain: FloatField,
        qsnow: FloatField,
        qice: FloatField,
        qgraupel: FloatField,
        q_con: FloatField,
        pt: FloatField,
        cappa: FloatField,
        delp: FloatField,
        delz: FloatField,
        r_vir: float,
    ):
        self._moist_cv_pt(
            qvapor,
            qliquid,
            qrain,
            qsnow,
            qice,
            qgraupel,
            q_con,
            pt,
            cappa,
            delp,
            delz,
            r_vir,
        )


class TranslateMoistCVPlusPt_2d(TranslateDycoreFortranData2Py):
    def __init__(self, grid, namelist, stencil_factory):
        super().__init__(grid, stencil_factory)
        self.stencil_factory = stencil_factory
        self.compute_func = MoistPT(stencil_factory, self.grid)
        self.in_vars["data_vars"] = {
            "qvapor": {"serialname": "qvapor_js"},
            "qliquid": {"serialname": "qliquid_js"},
            "qice": {"serialname": "qice_js"},
            "qrain": {"serialname": "qrain_js"},
            "qsnow": {"serialname": "qsnow_js"},
            "qgraupel": {"serialname": "qgraupel_js"},
            "delp": {},
            "delz": {},
            "q_con": {},
            "pt": {},
            "cappa": {},
        }
        self.write_vars = ["gz", "cvm"]
        for k, v in self.in_vars["data_vars"].items():
            if k not in self.write_vars:
                v["axis"] = 1

        self.in_vars["parameters"] = ["r_vir"]
        self.out_vars = {
            "pt": {},
            "cappa": {},
            "q_con": {},
        }

    def compute_from_storage(self, inputs):
        for name, value in inputs.items():
            if hasattr(value, "shape") and len(value.shape) > 1 and value.shape[1] == 1:
                inputs[name] = self.make_storage_data(
                    pad_field_in_j(
                        value, self.grid.njd, backend=self.stencil_factory.backend
                    )
                )
        self.compute_func(**inputs)
        return inputs
