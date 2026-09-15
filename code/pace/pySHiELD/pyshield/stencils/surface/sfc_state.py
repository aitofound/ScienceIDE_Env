from dataclasses import dataclass, field, fields
from typing import Any, Dict, Mapping

import xarray as xr

import ndsl.dsl.gt4py_utils as gt_utils
from ndsl import GridSizer, Quantity, QuantityFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM
from ndsl.dsl.typing import Bool, Float, Int


@dataclass()
class SurfaceState:
    tsfc: Quantity = field(
        metadata={
            "name": "surface_temperature",
            "dims": [I_DIM, J_DIM],
            "units": "K",
            "intent": "inout",
        }
    )

    stc: Quantity = field(
        metadata={
            "name": "soil_temperature_content",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "K",
            "intent": "inout",
        }
    )

    qsfc: Quantity = field(
        metadata={
            "name": "surface_specific_humidity",
            "dims": [I_DIM, J_DIM],
            "units": "kg/kg",
            "intent": "inout",
        }
    )

    snowd: Quantity = field(
        metadata={
            "name": "snow_depth",
            "dims": [I_DIM, J_DIM],
            "units": "mm",
            "intent": "inout",
        }
    )

    sncovr: Quantity = field(
        metadata={
            "name": "snow_cover_area_fraction",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    snoalb: Quantity = field(
        metadata={
            "name": "maximum_snow_albedo_in_fraction",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    albedo: Quantity = field(
        metadata={
            "name": "mean_albedo",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    alvsf: Quantity = field(
        metadata={
            "name": "mean_visible_albedo_with_strong_cosz_dependency",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    alnsf: Quantity = field(
        metadata={
            "name": "mean_near_ir_albedo_with_strong_cosz_dependency",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    alvwf: Quantity = field(
        metadata={
            "name": "mean_visible_albedo_with_weak_cosz_dependency",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    alnwf: Quantity = field(
        metadata={
            "name": "mean_near_ir_albedo_with_weak_cosz_dependency",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
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

    ztrl: Quantity = field(
        metadata={
            "name": "t_and_q_surface_roughness",
            "dims": [I_DIM, J_DIM],
            "units": "cm",
            "intent": "in",
        }
    )

    hprim: Quantity = field(
        metadata={
            "name": "",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    wind: Quantity = field(
        metadata={
            "name": "surface_wind_speed",
            "dims": [I_DIM, J_DIM],
            "units": "m/s",
            "intent": "out",
        }
    )

    uustar: Quantity = field(
        metadata={
            "name": "boundary_layer_param",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "out",
        }
    )

    islmsk: Quantity = field(
        metadata={
            "name": "ice_sea_land_mask",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
            "type": Int,
        }
    )

    vegtype: Quantity = field(
        metadata={
            "name": "vegetation_type",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
            "type": Int,
        }
    )

    vfrac: Quantity = field(
        metadata={
            "name": "vegetation_fraction",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )

    shdmax: Quantity = field(
        metadata={
            "name": "max_fractional_green_vegetation_cover",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )

    ffmm: Quantity = field(
        metadata={
            "name": "fm_PBL_parameter",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    ffhh: Quantity = field(
        metadata={
            "name": "fh_PBL_parameter",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    f10m: Quantity = field(
        metadata={
            "name": "sigma1_10m_wind_ratio",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    sfcemis: Quantity = field(
        metadata={
            "name": "sfc_lw_emissivity_fraction",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    srflag: Quantity = field(
        metadata={
            "name": "rain_snow_precipitation_flag",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
            "type": Bool,
        }
    )

    facsf: Quantity = field(
        metadata={
            "name": "fractional_coverage_with_strong_cosz_dependency",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    facwf: Quantity = field(
        metadata={
            "name": "fractional_coverage_with_weak_cosz_dependency",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    hice: Quantity = field(
        metadata={
            "name": "sea_ice_thickness",
            "dims": [I_DIM, J_DIM],
            "units": "unknown",
            "intent": "inout",
        }
    )

    fice: Quantity = field(
        metadata={
            "name": "ice_fraction_over_open_water_grid",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )

    tisfc: Quantity = field(
        metadata={
            "name": "surface_temperature_over_ice_fraction",
            "dims": [I_DIM, J_DIM],
            "units": "K",
            "intent": "inout",
        }
    )

    weasd: Quantity = field(
        metadata={
            "name": "water_equiv_accumulated_snow_depth",
            "dims": [I_DIM, J_DIM],
            "units": "kg/m**2",
            "intent": "inout",
        }
    )

    tprcp: Quantity = field(
        metadata={
            "name": "total_precip",
            "dims": [I_DIM, J_DIM],
            "units": "unknown",
            "intent": "out",
        }
    )

    u1: Quantity = field(
        metadata={
            "name": "lowest_level_x_wind",
            "dims": [I_DIM, J_DIM],
            "units": "m/s",
            "intent": "in",
        }
    )

    v1: Quantity = field(
        metadata={
            "name": "lowest_level_y_wind",
            "dims": [I_DIM, J_DIM],
            "units": "m/s",
            "intent": "in",
        }
    )

    qvapor: Quantity = field(
        metadata={
            "name": "lowest_level_specific_humidity",
            "dims": [I_DIM, J_DIM],
            "units": "kg/kg",
            "intent": "in",
        }
    )

    t1: Quantity = field(
        metadata={
            "name": "lowest_level_temperature",
            "dims": [I_DIM, J_DIM],
            "units": "K",
            "intent": "in",
        }
    )

    ps: Quantity = field(
        metadata={
            "name": "surface_pressure",
            "dims": [I_DIM, J_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )

    phil: Quantity = field(
        metadata={
            "name": "layer_geopotential_height",
            "dims": [I_DIM, J_DIM],
            "units": "m",
            "intent": "in",
        }
    )

    prsl1: Quantity = field(
        metadata={
            "name": "",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )

    prsik: Quantity = field(
        metadata={
            "name": "surface_layer_mean_pressure",
            "dims": [I_DIM, J_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )

    prslk: Quantity = field(
        metadata={
            "name": "Exner_function",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "in",
        }
    )

    rb: Quantity = field(
        metadata={
            "name": "bulk_Richardson_number",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "out",
        }
    )

    stress: Quantity = field(
        metadata={
            "name": "surface_wind_stress",
            "dims": [I_DIM, J_DIM],
            "units": "Pa",
            "intent": "out",
        }
    )

    hflx: Quantity = field(
        metadata={
            "name": "sensible_heat_flux",
            "dims": [I_DIM, J_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )

    evap: Quantity = field(
        metadata={
            "name": "evaporation_from_latent_heat_flux",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "out",
        }
    )

    sfcdlw: Quantity = field(
        metadata={
            "name": "downward_longwave_surface_flux",
            "dims": [I_DIM, J_DIM],
            "units": "W/m**2",
            "intent": "in",
        }
    )

    sfcdsw: Quantity = field(
        metadata={
            "name": "downward_shortwave_surface_flux",
            "dims": [I_DIM, J_DIM],
            "units": "W/m**2",
            "intent": "in",
        }
    )

    sfcnsw: Quantity = field(
        metadata={
            "name": "net_shortwave_surface_flux",
            "dims": [I_DIM, J_DIM],
            "units": "W/m**2",
            "intent": "in",
        }
    )

    @classmethod
    def init_zeros(
        cls,
        quantity_factory,
    ) -> "SurfaceState":
        initial_arrays = {}
        for _field in fields(cls):
            if "dims" in _field.metadata.keys():
                dtype = (
                    _field.metadata["type"]
                    if "type" in _field.metadata.keys()
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
    ) -> "SurfaceState":
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
            if name not in ["extra_fields"]:
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
