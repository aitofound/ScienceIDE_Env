from typing import Any, Dict

import numpy as np
import pytest
from f90nml import Namelist

import ndsl.dsl.gt4py_utils as utils
import pyfv3.initialization.analytic_init as analytic_init
import pyfv3.initialization.init_utils as init_utils
import pyfv3.initialization.test_cases.initialize_aquaplanet as aq_init
import pyfv3.initialization.test_cases.initialize_baroclinic as baroclinic_init
from ndsl import (
    CubedSphereCommunicator,
    CubedSpherePartitioner,
    QuantityFactory,
    StencilFactory,
    SubtileGridSizer,
    TilePartitioner,
)
from ndsl.constants import (
    I_DIM,
    I_INTERFACE_DIM,
    J_DIM,
    J_INTERFACE_DIM,
    K_DIM,
    K_INTERFACE_DIM,
    N_HALO_DEFAULT,
)
from ndsl.grid import GridData, MetricTerms
from ndsl.stencils.testing import ParallelTranslateBaseSlicing
from ndsl.stencils.testing.grid import TRACER_DIM
from pyfv3 import DycoreState, DynamicalCoreConfig
from pyfv3.testing import NullComm, TranslateDycoreFortranData2Py


class TranslateInitCase(ParallelTranslateBaseSlicing):
    outputs: Dict[str, Any] = {
        "u": {
            "name": "x_wind",
            "dims": [I_DIM, J_INTERFACE_DIM, K_DIM],
            "units": "m/s",
        },
        "v": {
            "name": "y_wind",
            "dims": [I_INTERFACE_DIM, J_DIM, K_DIM],
            "units": "m/s",
        },
        "ua": {
            "name": "eastward_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
        },
        "va": {
            "name": "northward_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
        },
        "uc": {
            "name": "x_wind_on_c_grid",
            "dims": [I_INTERFACE_DIM, J_DIM, K_DIM],
            "units": "m/s",
        },
        "vc": {
            "name": "y_wind_on_c_grid",
            "dims": [I_DIM, J_INTERFACE_DIM, K_DIM],
            "units": "m/s",
        },
        "w": {
            "name": "vertical_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
        },
        "phis": {
            "name": "surface_geopotential",
            "units": "m^2 s^-2",
            "dims": [I_DIM, J_DIM],
        },
        "delp": {
            "name": "pressure_thickness_of_atmospheric_layer",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "Pa",
        },
        "delz": {
            "name": "vertical_thickness_of_atmospheric_layer",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m",
        },
        "ps": {
            "name": "surface_pressure",
            "dims": [I_DIM, J_DIM],
            "units": "Pa",
        },
        "pe": {
            "name": "interface_pressure",
            "dims": [I_DIM, K_INTERFACE_DIM, J_DIM],
            "units": "Pa",
            "n_halo": 1,
        },
        "pk": {
            "name": "interface_pressure_raised_to_power_of_kappa",
            "units": "unknown",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "n_halo": 0,
        },
        "pkz": {
            "name": "layer_mean_pressure_raised_to_power_of_kappa",
            "units": "unknown",
            "dims": [I_DIM, J_DIM, K_DIM],
            "n_halo": 0,
        },
        "peln": {
            "name": "logarithm_of_interface_pressure",
            "units": "ln(Pa)",
            "dims": [I_DIM, K_INTERFACE_DIM, J_DIM],
            "n_halo": 0,
        },
        "pt": {
            "name": "air_temperature",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK",
        },
        "q4d": {
            "name": "tracers",
            "dims": [I_DIM, J_DIM, K_DIM, TRACER_DIM],
            "units": "kg/kg",
        },
    }

    def __init__(
        self,
        grid_list,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid_list, namelist, stencil_factory)
        grid = grid_list[0]
        self._base.in_vars["data_vars"] = {}
        self._base.in_vars["parameters"] = ["ptop"]
        self._base.out_vars = {
            "u": grid.y3d_domain_dict(),
            "v": grid.x3d_domain_dict(),
            "uc": grid.x3d_domain_dict(),
            "vc": grid.y3d_domain_dict(),
            "ua": {},
            "va": {},
            "w": {},
            "pt": {},
            "delp": {},
            "q4d": {},
            "phis": {},
            "delz": {},
            "ps": {"kstart": grid.npz, "kend": grid.npz},
            "pe": {
                "istart": grid.is_ - 1,
                "iend": grid.ie + 1,
                "jstart": grid.js - 1,
                "jend": grid.je + 1,
                "kend": grid.npz + 1,
                "kaxis": 1,
            },
            "peln": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
                "kend": grid.npz,
                "kaxis": 1,
            },
            "pk": grid.compute_buffer_k_dict(),
            "pkz": grid.compute_dict(),
        }
        self.max_error = 6e-14
        self.ignore_near_zero_errors = {}
        for var in ["u", "v"]:
            self.ignore_near_zero_errors[var] = {"near_zero": 2e-13}
        self.stencil_factory = stencil_factory
        self.config = DynamicalCoreConfig.from_f90nml(namelist)

    def compute_sequential(self, *args, **kwargs):
        pytest.skip(
            f"{self.__class__} only has a mpirun implementation, "
            "not running in mock-parallel"
        )

    def outputs_from_state(self, state: DycoreState) -> dict:
        outputs = {}
        arrays = {}
        for name, _properties in self.outputs.items():
            if isinstance(state[name], dict):
                for tracer, _quantity in state[name].items():
                    state[name][tracer] = state[name][tracer][:]
                arrays[name] = state[name]
            elif len(self.outputs[name]["dims"]) > 0:
                arrays[name] = state[name][:]
            else:
                outputs[name] = state[name]  # scalar
        outputs.update(self._base.slice_output(arrays))
        return outputs

    def compute_parallel(self, inputs, communicator):
        metric_terms = MetricTerms.from_tile_sizing(
            npx=self.config.npx,
            npy=self.config.npy,
            npz=self.config.npz,
            communicator=communicator,
            backend=self.stencil_factory.backend,
        )

        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.nx_tile,
            ny_tile=self.config.nx_tile,
            nz=self.config.nz,
            n_halo=N_HALO_DEFAULT,
            layout=self.config.layout,
            tile_partitioner=communicator.partitioner.tile,
            tile_rank=communicator.tile.rank,
            backend=self.stencil_factory.backend,
        )

        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)

        grid_data = GridData.new_from_metric_terms(metric_terms)

        state = analytic_init.init_analytic_state(
            analytic_init_case=analytic_init.AnalyticCase.baroclinic_instability,
            grid_data=grid_data,
            quantity_factory=quantity_factory,
            adiabatic=self.config.adiabatic,
            hydrostatic=self.config.hydrostatic,
            moist_phys=self.config.moist_phys,
            comm=communicator,
        )

        state.q4d = {}
        for tracer in utils.tracer_variables:
            state.q4d[tracer] = getattr(state, tracer)
        return self.outputs_from_state(state)


def make_sliced_inputs_dict(inputs, slice_2d):
    sliced_inputs = {}
    for k, v in inputs.items():
        if isinstance(v, np.ndarray) and len(v.shape) > 1:
            if len(v.shape) == 3:
                slices = (*slice_2d, slice(None))
            if len(v.shape) == 2:
                slices = slice_2d
            sliced_inputs[k] = inputs[k][slices]
        else:
            sliced_inputs[k] = np.asarray(inputs[k])
    return sliced_inputs


class TranslateInitPreJab(TranslateDycoreFortranData2Py):
    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {"ak": {}, "bk": {}, "delp": {}}
        self.in_vars["parameters"] = ["ptop"]
        self.out_vars = {
            "delp": {},
            "ps": {"kstart": grid.npz, "kend": grid.npz},
            "pe": {
                "istart": grid.is_ - 1,
                "iend": grid.ie + 1,
                "jstart": grid.js - 1,
                "jend": grid.je + 1,
                "kend": grid.npz + 1,
                "kaxis": 1,
            },
            "peln": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
                "kend": grid.npz,
                "kaxis": 1,
            },
            "pk": grid.compute_buffer_k_dict(),
            "pkz": grid.compute_dict(),
            "eta": {"istart": 0, "iend": 0, "jstart": 0, "jend": 0},
            "eta_v": {"istart": 0, "iend": 0, "jstart": 0, "jend": 0},
        }
        self.stencil_factory = stencil_factory

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        for k, v in inputs.items():
            if k != "ptop":
                inputs[k] = v[:]
        full_shape = self.grid.domain_shape_full(add=(1, 1, 1))
        for variable in ["pe", "peln", "pk", "pkz"]:
            inputs[variable] = np.zeros(full_shape)
        inputs["ps"] = np.zeros(full_shape[0:2])
        for zvar in ["eta", "eta_v"]:
            inputs[zvar] = np.zeros(self.grid.npz + 1)
        inputs["ps"][:] = baroclinic_init.SURFACE_PRESSURE
        sliced_inputs = make_sliced_inputs_dict(
            inputs, self.grid.compute_interface()[0:2]
        )

        init_utils.setup_pressure_fields(
            **sliced_inputs,
        )
        return self.slice_output(inputs)


class TranslateJablonowskiBaroclinic(TranslateDycoreFortranData2Py):
    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {
            "delp": {},
            "eta_v": {"istart": 0, "iend": 0, "jstart": 0, "jend": 0},
            "eta": {"istart": 0, "iend": 0, "jstart": 0, "jend": 0},
            "peln": {
                "istart": grid.isd,
                "iend": grid.ied,
                "jstart": grid.jsd,
                "jend": grid.jed,
                "kend": grid.npz,
                "kaxis": 1,
            },
        }

        self.in_vars["parameters"] = ["ptop"]
        self.out_vars = {
            "u": grid.y3d_domain_dict(),
            "v": grid.x3d_domain_dict(),
            "w": {},
            "pt": {},
            "phis": {},
            "delz": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
                "kend": grid.npz - 1,
            },
            "qvapor": {},
        }
        self.ignore_near_zero_errors = {}
        for var in ["u", "v"]:
            self.ignore_near_zero_errors[var] = {"near_zero": 2e-13}

        self.max_error = 1e-13
        self.stencil_factory = stencil_factory

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        # testing just numpy arrays for this
        for k, v in inputs.items():
            if k != "ptop":
                inputs[k] = v[:]
        full_shape = self.grid.domain_shape_full(add=(1, 1, 1))
        for variable in ["u", "v", "pt", "delz", "w", "qvapor"]:
            inputs[variable] = np.zeros(full_shape)
        for var2d in ["phis"]:
            inputs[var2d] = np.zeros(full_shape[0:2])

        slice_2d = (
            slice(self.grid.is_, self.grid.ie + 2),
            slice(self.grid.js, self.grid.je + 2),
        )

        grid_vars = {
            "lon": np.asarray(self.grid.bgrid1)[
                slice_2d
            ],  # Convert from memoryview to numpy array for slicing
            "lat": np.asarray(self.grid.bgrid2)[slice_2d],
            "lon_agrid": np.asarray(self.grid.agrid1)[slice_2d],
            "lat_agrid": np.asarray(self.grid.agrid2)[slice_2d],
            "ee1": np.asarray(self.grid.ee1)[slice_2d],
            "ee2": np.asarray(self.grid.ee2)[slice_2d],
            "es1": np.asarray(self.grid.es1)[slice_2d],
            "ew2": np.asarray(self.grid.ew2)[slice_2d],
        }
        inputs["w"][:] = 1e30
        inputs["delz"][:] = 1e30
        inputs["pt"][:] = 1.0
        inputs["phis"][:] = 1.0e25
        sliced_inputs = make_sliced_inputs_dict(inputs, slice_2d)
        baroclinic_init.baroclinic_initialization(
            **sliced_inputs,
            **grid_vars,
            adiabatic=self.config.adiabatic,
            hydrostatic=self.config.hydrostatic,
            nx=self.grid.nic,
            ny=self.grid.njc,
        )
        return self.slice_output(inputs)


class TranslatePVarAuxiliaryPressureVars(TranslateDycoreFortranData2Py):
    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {
            "delp": {},
            "delz": {},
            "pt": {},
            "ps": {"kstart": grid.npz, "kend": grid.npz},
            "qvapor": {},
            "pe": {
                "istart": grid.is_ - 1,
                "iend": grid.ie + 1,
                "jstart": grid.js - 1,
                "jend": grid.je + 1,
                "kend": grid.npz + 1,
                "kaxis": 1,
            },
            "peln": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
                "kend": grid.npz,
                "kaxis": 1,
            },
            "pkz": grid.compute_dict(),
        }

        self.in_vars["parameters"] = ["ptop"]
        self.out_vars = {}
        for var in ["delz", "delp", "ps", "peln"]:
            self.out_vars[var] = self.in_vars["data_vars"][var]
        self.stencil_factory = stencil_factory

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)
        # testing just numpy arrays for this
        for k, v in inputs.items():
            if k != "ptop":
                inputs[k] = v[:]

        inputs["delz"][:] = 1.0e25
        sliced_inputs = make_sliced_inputs_dict(
            inputs, self.grid.compute_interface()[0:2]
        )
        init_utils.p_var(
            **sliced_inputs,
            moist_phys=self.config.moist_phys,
            make_nh=(not self.config.hydrostatic),
        )
        return self.slice_output(inputs)


class TranslateAquaplanet(TranslateDycoreFortranData2Py):
    """Translate the Fortran initialization for the Aquaplanet test case."""

    def __init__(
        self,
        grid,
        namelist: Namelist,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, namelist, stencil_factory)
        self.in_vars["data_vars"] = {
            "u": {},
            "v": {},
            "w": {},
            "ps": {},
            "phis": {},
            "pt": {},
            "delp": {},
            "delz": {},
            "qvapor": {},
        }
        self.in_vars["parameters"] = []

        self.out_vars = {
            "u": grid.y3d_domain_dict(),
            "v": grid.x3d_domain_dict(),
            "w": {},
            "ps": {},
            "phis": {},
            "pt": {},
            "delp": {},
            "delz": {
                "istart": grid.is_,
                "iend": grid.ie,
                "jstart": grid.js,
                "jend": grid.je,
            },
            "qvapor": {},
        }
        self.ignore_near_zero_errors = {}
        self.max_error = 1e-13
        self.stencil_factory = stencil_factory

    def compute(self, inputs):
        mpi_comm = NullComm(
            rank=self.grid.rank,
            total_ranks=6 * self.config.layout[0] * self.config.layout[1],
        )
        partitioner = CubedSpherePartitioner(TilePartitioner(self.config.layout))
        communicator = CubedSphereCommunicator(mpi_comm, partitioner)
        sizer = SubtileGridSizer.from_tile_params(
            nx_tile=self.config.npx - 1,
            ny_tile=self.config.npx - 1,
            nz=self.config.npz,
            n_halo=N_HALO_DEFAULT,
            data_dimensions={},
            layout=self.config.layout,
            backend=self.stencil_factory.backend,
        )
        quantity_factory = QuantityFactory(sizer, backend=self.stencil_factory.backend)
        metric_terms = MetricTerms(
            quantity_factory=quantity_factory,
            communicator=communicator,
            grid_type=self.config.grid_type,
            ak=self.grid.ak,
            bk=self.grid.bk,
        )

        grid_data = GridData.new_from_metric_terms(metric_terms)
        hydrostatic = self.config.hydrostatic
        moist_phys = self.config.moist_phys

        # Main call being tested
        dycore_state = aq_init.init_aquaplanet_state(
            grid_data, quantity_factory, hydrostatic, moist_phys, communicator
        )

        inputs["ps"] = dycore_state.ps
        inputs["phis"] = dycore_state.phis
        inputs["pt"] = dycore_state.pt
        inputs["delp"] = dycore_state.delp
        inputs["delz"] = dycore_state.delz
        inputs["u"] = dycore_state.u
        inputs["v"] = dycore_state.v
        inputs["w"] = dycore_state.w
        inputs["qvapor"] = dycore_state.qvapor
        return self.slice_output(inputs)
