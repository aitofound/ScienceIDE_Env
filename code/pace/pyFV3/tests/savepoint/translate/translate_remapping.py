from f90nml import Namelist

import ndsl.dsl.gt4py_utils as utils
from ndsl import StencilFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.stencils.testing import Grid
from pyfv3.stencils import LagrangianToEulerian
from pyfv3.testing import TranslateDycoreFortranData2Py
from pyfv3.tracers import FVTracersAxisName, default_ai2_tracers


class TranslateRemapping(TranslateDycoreFortranData2Py):
    def __init__(
        self,
        grid: Grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {
            "tracers": {},
            "w": {},
            "u": grid.y3d_domain_dict(),
            "v": grid.x3d_domain_dict(),
            "delz": {},
            "pt": {},
            "dp1": {},
            "delp": {},
            "cappa": {},
            "q_con": {},
            "pkz": grid.compute_dict(),
            "pk": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
                "kend": grid.npz,
            },
            "peln": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
                "kaxis": 1,
                "kend": grid.npz,
            },
            "pe": {
                "istart": grid.is_ - 1,
                "iend": grid.ie + 1,
                "jstart": grid.js - 1,
                "jend": grid.je + 1,
                "kend": grid.npz + 1,
                "kaxis": 1,
            },
            "hs": {"serialname": "phis"},
            "ps": {},
            "wsd": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
            },
            # column variables...
            "ak": {},
            "bk": {},
            "pfull": {},
        }
        self.in_vars["parameters"] = [
            "ptop",
            "akap",
            "zvir",
            "last_step",
            "consv_te",
            "mdt",
            "nq",
        ]
        self.out_vars = {}
        self.write_vars = ["wsd"]
        for k in [
            "pe",
            "pkz",
            "pk",
            "peln",
            "pt",
            "tracers",
            "cappa",
            "delp",
            "delz",
            "q_con",
            "u",
            "v",
            "w",
            "ps",
            "dp1",
        ]:
            self.out_vars[k] = self.in_vars["data_vars"][k]
        self.out_vars["ps"] = {"kstart": grid.npz, "kend": grid.npz}
        self.max_error = 2e-8
        self.near_zero = 3e-18
        self.ignore_near_zero_errors = {"q_con": True, "tracers": True}
        self.stencil_factory = stencil_factory
        self.quantity_factory = grid.quantity_factory

    def compute_from_storage(self, inputs):
        default_ai2_tracers(self.quantity_factory)
        wsd_2d = utils.make_storage_from_shape(
            inputs["wsd"].shape[0:2], backend=self.stencil_factory.backend
        )
        wsd_2d[:, :] = inputs["wsd"][:, :, 0]
        inputs["wsd"] = wsd_2d
        inputs["last_step"] = bool(inputs["last_step"])
        pfull = self.quantity_factory.zeros([K_DIM], units="Pa")
        pfull[:] = pfull.np.asarray(inputs.pop("pfull"))

        # Tracers
        quantity_tracers = self.quantity_factory.from_array(
            inputs["tracers"], [I_DIM, J_DIM, K_DIM, FVTracersAxisName], "n/a"
        )
        inputs["tracers"] = quantity_tracers

        lagrangian_to_eulerian = LagrangianToEulerian(
            self.stencil_factory,
            quantity_factory=self.quantity_factory,
            config=self.config.remapping,
            area_64=self.grid.area_64,
            nq=inputs.pop("nq"),
            pfull=pfull,
        )

        lagrangian_to_eulerian(**inputs)

        if not self.stencil_factory.backend.is_fortran_aligned():
            inputs["tracers"] = quantity_tracers[:-1, :-1, :-1, :]
        else:
            inputs["tracers"] = quantity_tracers[:]
        return inputs
