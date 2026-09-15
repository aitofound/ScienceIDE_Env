from dataclasses import InitVar, dataclass, field, fields
from typing import Any, Dict, Mapping

import numpy as np
import xarray as xr

import ndsl.dsl.gt4py_utils as gt_utils
from ndsl import GridSizer, Quantity, QuantityFactory
from ndsl.constants import I_DIM, J_DIM, K_DIM, K_INTERFACE_DIM
from ndsl.dsl.typing import Float


@dataclass()
class RTE_RRTMGPState:
    prsi: Quantity = field(
        metadata={
            "name": "interface_pressure",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "Pa",
            "intent": "in",
        }
    )
    prsl: Quantity = field(
        metadata={
            "name": "layer_pressure",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "Pa",
            "intent": "inout",
        }
    )
    tlyr: Quantity = field(
        metadata={
            "name": "layer_air_temperature",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK",
            "intent": "in",
        }
    )
    tlvl: Quantity = field(
        metadata={
            "name": "level_air_temperature",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "degK",
            "intent": "inout",
        }
    )
    tsfc: Quantity = field(
        metadata={
            "name": "surface_temperature",
            "dims": [I_DIM, J_DIM],
            "units": "degK",
            "intent": "in",
        }
    )
    mu0: Quantity = field(
        metadata={
            "name": "cosine_zenith_angle",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    albedo: Quantity = field(
        metadata={
            "name": "surface_albedo",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    sfc_emis: Quantity = field(
        metadata={
            "name": "surface_emissivity",
            "dims": [I_DIM, J_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    qvapor: Quantity = field(
        metadata={
            "name": "specific_humidity",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "mol/mol",
            "intent": "in",
        }
    )
    qliquid: Quantity = field(
        metadata={
            "name": "cloud_water_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "in",
        }
    )
    qice: Quantity = field(
        metadata={
            "name": "cloud_ice_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "kg/kg",
            "intent": "in",
        }
    )
    qo3mr: Quantity = field(
        metadata={
            "name": "ozone_mixing_ratio",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "mol/mol",
            "intent": "in",
        }
    )
    qcld: Quantity = field(
        metadata={
            "name": "cloud_fraction",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "",
            "intent": "in",
        }
    )
    co2: Quantity = field(
        metadata={
            "name": "co2_concentration",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "",
            "intent": "inout",
        }
    )
    clwp: Quantity = field(
        metadata={
            "name": "cloud_liquid_water_path",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "g/m**2",
            "intent": "inout",
        }
    )
    cip: Quantity = field(
        metadata={
            "name": "cloud_ice_path",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "g/m**2",
            "intent": "inout",
        }
    )
    clwr: Quantity = field(
        metadata={
            "name": "cloud_liquid_water_radius",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "micron",
            "intent": "inout",
        }
    )
    cir: Quantity = field(
        metadata={
            "name": "cloud_ice_radius",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "micron",
            "intent": "inout",
        }
    )
    flwu: Quantity = field(
        metadata={
            "name": "longwave_flux_up",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    flwd: Quantity = field(
        metadata={
            "name": "longwave_flux_down",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    fswu: Quantity = field(
        metadata={
            "name": "shortwave_flux_up",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    fswd: Quantity = field(
        metadata={
            "name": "shortwave_flux_down",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    fswn: Quantity = field(
        metadata={
            "name": "net_sfc_shortwave_flux",
            "dims": [I_DIM, J_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    flwu_clr: Quantity = field(
        metadata={
            "name": "clearsky_longwave_flux_up",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    flwd_clr: Quantity = field(
        metadata={
            "name": "clearsky_longwave_flux_down",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    fswu_clr: Quantity = field(
        metadata={
            "name": "clearsky_shortwave_flux_up",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    fswd_clr: Quantity = field(
        metadata={
            "name": "clearsky_shortwave_flux_down",
            "dims": [I_DIM, J_DIM, K_INTERFACE_DIM],
            "units": "W/m**2",
            "intent": "out",
        }
    )
    hrtlw: Quantity = field(
        metadata={
            "name": "longwave_heating_rate",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK/s",
            "intent": "out",
        }
    )
    hrtsw: Quantity = field(
        metadata={
            "name": "shortwave_heating_rate",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK/s",
            "intent": "out",
        }
    )
    hrtlw_clr: Quantity = field(
        metadata={
            "name": "clearsky_longwave_heating_rate",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK/s",
            "intent": "out",
        }
    )
    hrtsw_clr: Quantity = field(
        metadata={
            "name": "clearsky_shortwave_heating_rate",
            "dims": [I_DIM, J_DIM, K_DIM],
            "units": "degK/s",
            "intent": "out",
        }
    )
    quantity_factory: InitVar[QuantityFactory]

    def __post_init__(
        self,
        quantity_factory: QuantityFactory,
    ):
        self._nz = quantity_factory.sizer.nz
        self._nx = quantity_factory.sizer.nx
        self._ny = quantity_factory.sizer.ny

    @classmethod
    def init_zeros(
        cls,
        quantity_factory,
    ) -> "RTE_RRTMGPState":
        initial_arrays = {}
        for _field in fields(cls):
            if "dims" in _field.metadata.keys():
                initial_arrays[_field.name] = quantity_factory.zeros(
                    _field.metadata["dims"],
                    _field.metadata["units"],
                    dtype=Float,
                )
        return cls(
            **initial_arrays,
            quantity_factory=quantity_factory,
        )

    @classmethod
    def init_from_storages(
        cls,
        storages: Mapping[str, Any],
        sizer: GridSizer,
        quantity_factory: QuantityFactory,
    ) -> "RTE_RRTMGPState":
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
        return cls(
            **inputs,
            quantity_factory=quantity_factory,
        )

    @property
    def xr_dataset(self):
        data_vars = {}
        for name, field_info in self.__dataclass_fields__.items():
            if name not in ["quantity_factory", "np_like"]:
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

    # @property
    def to_rterrtmgp_xr(self) -> xr.Dataset:
        """
        creates an xarray dataset of the state for RTE-RRTMGP
        Halos and padding points are removed, and horizontal axes are combined
        and dimension attributes are also changed to columns and levels or layers
        to fit pyRTE-RRTMGP's api
        For example, prsl's shape is changed from (npx, npy, npz) to (nx * ny, nz)
        with dimensions (column, layer)
        """
        data_vars = {}
        for name, field_info in self.__dataclass_fields__.items():
            if name not in ["quantity_factory", "np_like"]:
                if field_info.metadata["intent"] != "out":
                    if issubclass(field_info.type, Quantity):  # type: ignore[arg-type]
                        dims = []
                        slice_list = []
                        ndims = len(field_info.metadata["dims"])
                        nz = self._nz
                        for dim_name in field_info.metadata["dims"]:
                            # dims.append(f"{dim_name}_{name}")
                            if dim_name == "z_interface":
                                slice_list.append(
                                    np.s_[:]  # type: ignore[attr-defined]
                                )
                                nz = self._nz + 1
                                dims.append("level")
                            elif dim_name == "z":
                                slice_list.append(
                                    np.s_[:-1]  # type: ignore[attr-defined]
                                )
                                dims.append("layer")
                            elif "INTERFACE" in dim_name:
                                slice_list.append(
                                    np.s_[3:-3]  # type: ignore[attr-defined]
                                )
                            else:
                                slice_list.append(
                                    np.s_[3:-4]  # type: ignore[attr-defined]
                                )
                        # We have to reshape to get the max 2D shape rterrtmgp expects:
                        if ndims == 3:  # x-y-z array:
                            newshape = (-1, nz)
                            dims.insert(0, "column")
                        elif ndims == 2:  # x-y array:
                            newshape = (-1,)  # type: ignore[assignment]
                            dims.insert(0, "column")
                        elif ndims == 1:  # z-array
                            newshape = (nz,)  # type: ignore[assignment]
                        else:
                            raise NotImplementedError(
                                (
                                    "RadiationShape doesn't support more than 3D "
                                    f"arrays, {dim_name} has {ndims} axes"
                                )
                            )
                        data_vars[name] = xr.DataArray(
                            gt_utils.asarray(getattr(self, name)[:])[
                                tuple(slice_list)
                            ].reshape(newshape),
                            dims=dims,
                            attrs={
                                "long_name": field_info.metadata["name"],
                            },
                        )
        return xr.Dataset(data_vars=data_vars)
