from collections.abc import Mapping
from dataclasses import asdict, dataclass, field, fields
from typing import Any, Self

import xarray as xr

import ndsl.dsl.gt4py_utils as gt_utils
from ndsl import Backend, GridSizer, Quantity, QuantityFactory
from ndsl.constants import (
    I_DIM,
    I_INTERFACE_DIM,
    J_DIM,
    J_INTERFACE_DIM,
    K_DIM,
    K_INTERFACE_DIM,
)
from ndsl.dsl.typing import Float
from ndsl.restart._legacy_restart import open_restart
from ndsl.typing import Communicator


@dataclass()
class DycoreState:
    u: Quantity = field(
        metadata={
            "name": "x_wind",
            "dims": [I_DIM, J_INTERFACE_DIM, K_DIM],
            "units": "m/s",
            "intent": "inout",
        }
    )
    v: Quantity = field(
        metadata={
            "name": "y_wind",
            "dims": [I_INTERFACE_DIM, J_DIM, K_DIM],
            "units": "m/s",
            "intent": "inout",
        }
    )
    w: Quantity = field(
        metadata={
            "name": "vertical_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
            "intent": "inout",
        }
    )
    # TODO: move a-grid winds to temporary internal storage
    ua: Quantity = field(
        metadata={
            "name": "eastward_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
            "intent": "inout",
        }
    )
    va: Quantity = field(
        metadata={
            "name": "northward_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
        }
    )
    uc: Quantity = field(
        metadata={
            "name": "x_wind_on_c_grid",
            "dims": [I_INTERFACE_DIM, J_DIM, K_DIM],
            "units": "m/s",
            "intent": "inout",
        }
    )
    vc: Quantity = field(
        metadata={
            "name": "y_wind_on_c_grid",
            "dims": [I_DIM, J_INTERFACE_DIM, K_DIM],
            "units": "m/s",
            "intent": "inout",
        }
    )
    delp: Quantity = field(
        metadata={
            "name": "pressure_thickness_of_atmospheric_layer",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "Pa",
            "intent": "inout",
        }
    )
    delz: Quantity = field(
        metadata={
            "name": "vertical_thickness_of_atmospheric_layer",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m",
            "intent": "inout",
        }
    )
    ps: Quantity = field(
        metadata={
            "name": "surface_pressure",
            "dims": [I_DIM, J_DIM],
            "units": "Pa",
            "intent": "inout",
        }
    )
    pe: Quantity = field(
        metadata={
            "name": "interface_pressure",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "Pa",
            "n_halo": 1,
            "intent": "inout",
        }
    )
    pt: Quantity = field(
        metadata={
            "name": "air_temperature",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK",
            "intent": "inout",
        }
    )
    peln: Quantity = field(
        metadata={
            "name": "logarithm_of_interface_pressure",
            "dims": [
                I_DIM,
                J_DIM,
                K_INTERFACE_DIM,
            ],
            "units": "ln(Pa)",
            "n_halo": 0,
            "intent": "inout",
        }
    )
    pk: Quantity = field(
        metadata={
            "name": "interface_pressure_raised_to_power_of_kappa",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "unknown",
            "n_halo": 0,
            "intent": "inout",
        }
    )
    pkz: Quantity = field(
        metadata={
            "name": "layer_mean_pressure_raised_to_power_of_kappa",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "unknown",
            "n_halo": 0,
            "intent": "inout",
        }
    )
    qvapor: Quantity = field(
        metadata={
            "name": "specific_humidity",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
        }
    )
    qliquid: Quantity = field(
        metadata={
            "name": "cloud_water_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )
    qice: Quantity = field(
        metadata={
            "name": "cloud_ice_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )
    qrain: Quantity = field(
        metadata={
            "name": "rain_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )
    qsnow: Quantity = field(
        metadata={
            "name": "snow_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )
    qgraupel: Quantity = field(
        metadata={
            "name": "graupel_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )
    qo3mr: Quantity = field(
        metadata={
            "name": "ozone_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )
    qsgs_tke: Quantity = field(
        metadata={
            "name": "turbulent_kinetic_energy",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m**2/s**2",
            "intent": "inout",
        }
    )
    qcld: Quantity = field(
        metadata={
            "name": "cloud_fraction",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    q_con: Quantity = field(
        metadata={
            "name": "total_condensate_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )
    omga: Quantity = field(
        metadata={
            "name": "vertical_pressure_velocity",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "Pa/s",
            "intent": "inout",
        }
    )
    mfxd: Quantity = field(
        metadata={
            "name": "accumulated_x_mass_flux",
            "dims": [I_INTERFACE_DIM, J_DIM, K_DIM],
            "units": "unknown",
            "n_halo": 0,
            "intent": "inout",
        }
    )
    mfyd: Quantity = field(
        metadata={
            "name": "accumulated_y_mass_flux",
            "dims": [I_DIM, J_INTERFACE_DIM, K_DIM],
            "units": "unknown",
            "n_halo": 0,
            "intent": "inout",
        }
    )
    cxd: Quantity = field(
        metadata={
            "name": "accumulated_x_courant_number",
            "dims": [I_INTERFACE_DIM, J_DIM, K_DIM],
            "units": "",
            "n_halo": (0, 3),
            "intent": "inout",
        }
    )
    cyd: Quantity = field(
        metadata={
            "name": "accumulated_y_courant_number",
            "dims": [I_DIM, J_INTERFACE_DIM, K_DIM],
            "units": "",
            "n_halo": (3, 0),
            "intent": "inout",
        }
    )
    diss_estd: Quantity = field(
        metadata={
            "name": "dissipation_estimate_from_heat_source",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "unknown",
            "n_halo": (3, 3),
            "intent": "inout",
        }
    )
    """
    how much energy is dissipated, is mainly captured
    to send to the stochastic physics (in contrast to heat_source)
    """
    phis: Quantity = field(
        metadata={
            "name": "surface_geopotential",
            "units": "m^2 s^-2",
            "dims": [I_DIM, J_DIM],
            "intent": "in",
        }
    )
    bdt: float = field(default=0.0)
    mdt: float = field(default=0.0)

    def __post_init__(self) -> None:
        for _field in fields(self):
            for check_name in ["units", "dims"]:
                if check_name in _field.metadata:
                    required = _field.metadata[check_name]
                    actual = getattr(getattr(self, _field.name), check_name)
                    if isinstance(required, list):
                        actual = list(actual)
                    if actual != required:
                        raise TypeError(
                            f"{_field.name} has metadata {check_name} of {actual}"
                            f"that does not match the requirement {required}"
                        )

    @classmethod
    def init_zeros(cls, quantity_factory: QuantityFactory) -> Self:
        initial_storages = {}
        for _field in fields(cls):
            if "dims" in _field.metadata.keys():
                initial_storages[_field.name] = quantity_factory.zeros(
                    _field.metadata["dims"],
                    _field.metadata["units"],
                    dtype=Float,
                )[:]
        return cls.init_from_storages(
            storages=initial_storages,
            sizer=quantity_factory.sizer,
            backend=quantity_factory.backend,
        )

    @classmethod
    def init_from_numpy_arrays(
        cls, dict_of_numpy_arrays: dict, sizer: GridSizer, backend: Backend
    ) -> Self:
        field_names = [_field.name for _field in fields(cls)]
        for variable_name in dict_of_numpy_arrays.keys():
            if variable_name not in field_names:
                raise KeyError(
                    variable_name + " is provided, but not part of the dycore state"
                )
        dict_state = {}
        for _field in fields(cls):
            if "dims" in _field.metadata.keys():
                dims = _field.metadata["dims"]
                dict_state[_field.name] = Quantity(
                    dict_of_numpy_arrays[_field.name],
                    dims,
                    _field.metadata["units"],
                    origin=sizer.get_origin(dims),
                    extent=sizer.get_extent(dims),
                    backend=backend,
                )
        return cls(**dict_state)  # type: ignore[arg-type,unused-ignore]

    @classmethod
    def init_from_storages(
        cls,
        storages: Mapping[str, Any],
        sizer: GridSizer,
        bdt: float = 0.0,
        mdt: float = 0.0,
        backend: Backend | None = None,
    ) -> Self:
        if not backend:
            backend = Backend.python()
        inputs = {}
        for _field in fields(cls):
            if "dims" in _field.metadata.keys():
                dims = _field.metadata["dims"]
                quantity = Quantity(
                    storages[_field.name],
                    dims,
                    _field.metadata["units"],
                    origin=sizer.get_origin(dims),
                    extent=sizer.get_extent(dims),
                    backend=backend,
                )
                inputs[_field.name] = quantity
        return cls(**inputs, bdt=bdt, mdt=mdt)

    @classmethod
    def from_fortran_restart(
        cls,
        *,
        quantity_factory: QuantityFactory,
        communicator: Communicator,
        path: str,
    ) -> Self:
        state_dict: Mapping[str, Quantity] = open_restart(
            dirname=path,
            communicator=communicator,
            tracer_properties=TRACER_PROPERTIES,
        )

        new = cls.init_zeros(quantity_factory=quantity_factory)
        new.pt.view[:] = new.pt.np.asarray(
            state_dict["air_temperature"].transpose(new.pt.dims).view[:]
        )
        new.delp.view[:] = new.delp.np.asarray(
            state_dict["pressure_thickness_of_atmospheric_layer"]
            .transpose(new.delp.dims)
            .view[:]
        )
        new.phis.view[:] = new.phis.np.asarray(
            state_dict["surface_geopotential"].transpose(new.phis.dims).view[:]
        )
        new.w.view[:] = new.w.np.asarray(
            state_dict["vertical_wind"].transpose(new.w.dims).view[:]
        )
        new.u.view[:] = new.u.np.asarray(
            state_dict["x_wind"].transpose(new.u.dims).view[:]
        )
        new.v.view[:] = new.v.np.asarray(
            state_dict["y_wind"].transpose(new.v.dims).view[:]
        )
        new.qvapor.view[:] = new.qvapor.np.asarray(
            state_dict["specific_humidity"].transpose(new.qvapor.dims).view[:]
        )
        new.qliquid.view[:] = new.qliquid.np.asarray(
            state_dict["cloud_liquid_water_mixing_ratio"]
            .transpose(new.qliquid.dims)
            .view[:]
        )
        new.qice.view[:] = new.qice.np.asarray(
            state_dict["cloud_ice_mixing_ratio"].transpose(new.qice.dims).view[:]
        )
        new.qrain.view[:] = new.qrain.np.asarray(
            state_dict["rain_mixing_ratio"].transpose(new.qrain.dims).view[:]
        )
        new.qsnow.view[:] = new.qsnow.np.asarray(
            state_dict["snow_mixing_ratio"].transpose(new.qsnow.dims).view[:]
        )
        new.qgraupel.view[:] = new.qgraupel.np.asarray(
            state_dict["graupel_mixing_ratio"].transpose(new.qgraupel.dims).view[:]
        )
        new.qo3mr.view[:] = new.qo3mr.np.asarray(
            state_dict["ozone_mixing_ratio"].transpose(new.qo3mr.dims).view[:]
        )
        new.qcld.view[:] = new.qcld.np.asarray(
            state_dict["cloud_fraction"].transpose(new.qcld.dims).view[:]
        )
        new.delz.view[:] = new.delz.np.asarray(
            state_dict["vertical_thickness_of_atmospheric_layer"]
            .transpose(new.delz.dims)
            .view[:]
        )

        return new

    @property
    def xr_dataset(self) -> xr.Dataset:
        data_vars = {}
        for name, field_info in self.__dataclass_fields__.items():
            if issubclass(field_info.type, Quantity):  # type: ignore[arg-type]
                dims = [
                    f"{dim_name}_{name}" for dim_name in field_info.metadata["dims"]
                ]
                data_vars[name] = xr.DataArray(
                    gt_utils.asarray(getattr(self, name)[:]),
                    dims=dims,
                    attrs={
                        "long_name": field_info.metadata["name"],
                        "units": field_info.metadata.get("units", "unknown"),
                    },
                )
        return xr.Dataset(data_vars=data_vars)

    def __getitem__(self, item: str) -> Any:
        return getattr(self, item)

    def as_dict(self, quantity_only: bool = True) -> dict[str, Quantity | int]:
        if quantity_only:
            return {k: v for k, v in asdict(self).items() if isinstance(v, Quantity)}

        return {k: v for k, v in asdict(self).items()}


TRACER_PROPERTIES = {
    "specific_humidity": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "sphum",
        "units": "g/kg",
    },
    "cloud_liquid_water_mixing_ratio": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "liq_wat",
        "units": "g/kg",
    },
    "cloud_ice_mixing_ratio": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "ice_wat",
        "units": "g/kg",
    },
    "rain_mixing_ratio": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "rainwat",
        "units": "g/kg",
    },
    "snow_mixing_ratio": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "snowwat",
        "units": "g/kg",
    },
    "graupel_mixing_ratio": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "graupel",
        "units": "g/kg",
    },
    "ozone_mixing_ratio": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "o3mr",
        "units": "g/kg",
    },
    "turbulent_kinetic_energy": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "sgs_tke",
        "units": "g/kg",
    },
    "cloud_fraction": {
        "dims": [K_DIM, J_DIM, I_DIM],
        "restart_name": "cld_amt",
        "units": "g/kg",
    },
}
