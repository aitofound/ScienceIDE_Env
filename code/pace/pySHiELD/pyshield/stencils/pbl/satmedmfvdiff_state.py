from dataclasses import dataclass, field, fields
from typing import Any, Dict, Mapping

import xarray as xr

import ndsl.dsl.gt4py_utils as gt_utils
from ndsl import GridSizer, Quantity, QuantityFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM, K_INTERFACE_DIM
from ndsl.dsl.typing import Float, Int
from pyshield._config import TRACER_DIM


@dataclass()
class SATMEDMFVDiffState:
    u1: Quantity = field(
        metadata={
            "name": "eastward_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
            "intent": "in",
        }
    )
    v1: Quantity = field(
        metadata={
            "name": "northward_wind",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s",
            "intent": "in",
        }
    )
    t1: Quantity = field(
        metadata={
            "name": "air_temperature",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK",
            "intent": "in",
        }
    )
    q1: Quantity = field(
        metadata={
            "name": "tracer_quantities",
            "dims": [I_DIM, J_DIM, K_DIM, TRACER_DIM],
            "units": "",
            "intent": "in",
        }
    )
    du: Quantity = field(
        metadata={
            "name": "eastward_wind_tendency",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s**2",
            "intent": "inout",
        }
    )
    dv: Quantity = field(
        metadata={
            "name": "northward_wind_tendency",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m/s**2",
            "intent": "inout",
        }
    )
    dtdt: Quantity = field(
        metadata={
            "name": "air_temperature_tendency",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK/s",
            "intent": "inout",
        }
    )
    rtg: Quantity = field(
        metadata={
            "name": "tracer_tendency",
            "dims": [I_DIM, J_DIM, K_DIM, TRACER_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    prsl: Quantity = field(
        metadata={
            "name": "mean_layer_pressure",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )
    phii: Quantity = field(
        metadata={
            "name": "interface_geopotential_height",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "m",
            "intent": "in",
        }
    )
    phil: Quantity = field(
        metadata={
            "name": "layer_geopotential_height",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "m",
            "intent": "in",
        }
    )
    prsi: Quantity = field(
        metadata={
            "name": "interface_pressure",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )
    prslk: Quantity = field(
        metadata={
            "name": "Exner_function",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )
    hsw: Quantity = field(
        metadata={
            "name": "shortwave_heating_rate",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "k/s",
            "intent": "in",
        }
    )
    hlw: Quantity = field(
        metadata={
            "name": "longwave_heating_rate",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "k/s",
            "intent": "in",
        }
    )
    islimsk: Quantity = field(
        metadata={
            "name": "sea_land_ice_mask",
            "dims": [I_DIM, J_DIM],
            "units": "-",
            "intent": "in",
            "dtype": Int,
        }
    )
    kpbl: Quantity = field(
        metadata={
            "name": "pbl_index",
            "dims": [I_DIM, J_DIM],
            "units": "-",
            "intent": "inout",
            "dtype": Int,
        }
    )
    kinver: Quantity = field(
        metadata={
            "name": "inversion_layer_index",
            "dims": [I_DIM, J_DIM],
            "units": "-",
            "intent": "in",
            "dtype": Int,
        }
    )
    hpbl: Quantity = field(
        metadata={
            "name": "pbl_height",
            "dims": [I_DIM, J_DIM],
            "units": "m",
            "intent": "inout",
        }
    )
    xmu: Quantity = field(
        metadata={
            "name": "zenith_angle_adjust_factor",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )
    psk: Quantity = field(
        metadata={
            "name": "log_surface_pressure",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )
    rbsoil: Quantity = field(
        metadata={
            "name": "bulk_Richardson_number",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )
    zorl: Quantity = field(
        metadata={
            "name": "composite_surface_roughness",
            "dims": [I_DIM, J_DIM],
            "units": "cm",
            "intent": "in",
        }
    )
    tsea: Quantity = field(
        metadata={
            "name": "surface_temperature",
            "dims": [I_DIM, J_DIM],
            "units": "degK",
            "intent": "in",
        }
    )
    u10m: Quantity = field(
        metadata={
            "name": "10_m_eastward_wind",
            "dims": [I_DIM, J_DIM],
            "units": "m/s",
            "intent": "in",
        }
    )
    v10m: Quantity = field(
        metadata={
            "name": "10_m_northward_wind",
            "dims": [I_DIM, J_DIM],
            "units": "m/s",
            "intent": "in",
        }
    )
    fm: Quantity = field(
        metadata={
            "name": "fm_PBL_parameter",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )
    fh: Quantity = field(
        metadata={
            "name": "fh_PBL_parameter",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )
    evap: Quantity = field(
        metadata={
            "name": "evaporation_from_latent_heat_flux",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )
    heat: Quantity = field(
        metadata={
            "name": "surface_heat_flux",
            "dims": [I_DIM, J_DIM],
            "units": "W/m**2",
            "intent": "in",
        }
    )
    stress: Quantity = field(
        metadata={
            "name": "surface_wind_stress",
            "dims": [I_DIM, J_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )
    spd1: Quantity = field(
        metadata={
            "name": "surface_wind_speed",
            "dims": [I_DIM, J_DIM],
            "units": "m/s",
            "intent": "in",
        }
    )
    delta: Quantity = field(
        metadata={
            "name": "atmospheric_pressure_thickness",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )  # Fortran name is del
    dusfc: Quantity = field(
        metadata={
            "name": "surface_eastward_wind_tendency",
            "dims": [I_DIM, J_DIM],
            "units": "m/s**2",
            "intent": "inout",
        }
    )
    dvsfc: Quantity = field(
        metadata={
            "name": "surface_northward_wind_tendency",
            "dims": [I_DIM, J_DIM],
            "units": "m/s**2",
            "intent": "inout",
        }
    )
    dtsfc: Quantity = field(
        metadata={
            "name": "surface_air_temperature_tendency",
            "dims": [I_DIM, J_DIM],
            "units": "degK/s",
            "intent": "inout",
        }
    )
    dqsfc: Quantity = field(
        metadata={
            "name": "surface_humidity_tendency",
            "dims": [I_DIM, J_DIM],
            "units": "km/km*s",
            "intent": "inout",
        }
    )
    dkt: Quantity = field(
        metadata={
            "name": "",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "",
            "intent": "out",
        }
    )

    @classmethod
    def init_zeros(
        cls,
        quantity_factory,
    ) -> "SATMEDMFVDiffState":
        initial_arrays = {}
        for _field in fields(cls):
            if "dims" in _field.metadata.keys():
                dtype = (
                    _field.metadata["dtype"]
                    if "dtype" in _field.metadata.keys()
                    else Float
                )
                initial_arrays[_field.name] = quantity_factory.zeros(
                    _field.metadata["dims"],
                    _field.metadata["units"],
                    dtype=dtype,
                )
        return cls(**initial_arrays)

    @classmethod
    def init_from_storages(
        cls,
        storages: Mapping[str, Any],
        sizer: GridSizer,
        quantity_factory: QuantityFactory,
    ) -> "SATMEDMFVDiffState":
        inputs: Dict[str, Quantity] = {}
        for _field in fields(cls):
            if "dims" in _field.metadata.keys():
                dims = _field.metadata["dims"]
                if _field.name in storages.keys():
                    quantity = Quantity(
                        storages[_field.name],
                        dims,
                        _field.metadata["units"],
                        origin=sizer.get_origin(dims),
                        extent=sizer.get_extent(dims),
                        backend=quantity_factory.backend,
                    )
                else:
                    quantity = quantity_factory.zeros(
                        dims,
                        _field.metadata["units"],
                    )
                inputs[_field.name] = quantity
        return cls(**inputs)

    @property
    def xr_dataset(self):
        data_vars = {}
        for name, field_info in self.__dataclass_fields__.items():
            if name not in ["extra_field"]:
                if issubclass(field_info.type, Quantity):
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
