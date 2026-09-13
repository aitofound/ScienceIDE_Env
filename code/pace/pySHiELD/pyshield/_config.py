from __future__ import annotations

import dataclasses
from enum import Enum, unique
from typing import List, Optional, Tuple

import f90nml
from dacite import Config, from_dict

import pyshield.constants as physcons
from ndsl import MetaEnumStr
from ndsl.dsl.typing import Float, set_4d_field_size
from ndsl.utils import f90nml_as_dict
from pyshield.tracer_workarounds import tracer_variables

# TODO: This will become a TracerBundle when ready
FloatFieldTracer = set_4d_field_size(9, Float)

DEFAULT_INT = 0
DEFAULT_FLOAT = 0.0
DEFAULT_STR = ""
DEFAULT_BOOL = False
DEFAULT_FLOAT = Float(0.0)
DEFAULT_SCHEMES = ["GFS_microphysics"]
TRACER_DIM = "n_tracers"
DEFAULT_PHYS_NML_GROUPS = (
    "main_nml",
    "coupler_nml",
    "fv_core_nml",
    "gfdl_cloud_microphysics_nml",
    "integ_phys_nml",
    "gfs_physics_nml",
)


# TODO: Should we have an enum for each class of parameterization
# microphysics, PBL, shallow convection, etc?
@unique
class PHYSICS_PACKAGES(Enum, metaclass=MetaEnumStr):
    SAMF_SHALCONV = "SAMF_SHALCONV"
    GFS_microphysics = "GFS_microphysics"
    GFDL_cloud_microphysics = "GFDL_cloud_microphysics"
    SATM_EDMF = "SATM_EDMF"
    SFC_layer = "SFC_layer"
    RTE_RRTMGP = "RTE_RRTMGP"


@dataclasses.dataclass
class PhysicsConfig:
    dt_atmos: float = DEFAULT_FLOAT
    """physics timestep (secs)"""
    fhswr: float = 3600.0
    """frequency for shortwave radiation (secs)"""
    fhlwr: float = 3600.0
    """frequency for longwave radiation (secs)"""
    nsswr: int = 1
    """frequency for shortwave radiation (timesteps), default is every step"""
    nslwr: int = 1
    """frequency for longwave radiation (timesteps)"""
    hydrostatic: bool = DEFAULT_BOOL
    hydro_delp: bool = False
    """Flag to use hydrostatic mean layer pressures"""
    npx: int = DEFAULT_INT
    npy: int = DEFAULT_INT
    npz: int = DEFAULT_INT
    nwat: int = DEFAULT_INT
    schemes: List = None
    ntracers: int = int(len(tracer_variables))
    do_qa: bool = DEFAULT_BOOL
    do_inline_mp: bool = False
    """Whether microphysics is inlined in the dycore"""
    c_cracw: float = 0.8
    """Rain accretion efficiency"""
    c_paut: float = 0.5
    """Autoconversion cloud water to rain (use 0.5 to reduce autoconversion"""
    c_pgacs: float = 0.01
    """Snow to graupel "accretion" eff. (was 0.1 in zetac)"""
    c_psaci: float = 0.05
    """Accretion: cloud ice to snow (was 0.1 in zetac)"""
    ccn_l: float = 300.0
    """CCN over land (cm^-3)"""
    ccn_o: float = 100.0
    """CCN over ocean (cm^-3)"""
    const_vg: bool = False
    """Fall velocity tuning constant of graupel"""
    const_vi: bool = False
    """Fall velocity tuning constant of ice"""
    const_vr: bool = False
    """Fall velocity tuning constant of rain water"""
    const_vs: bool = False
    """Fall velocity tuning constant of snow"""
    vs_fac: float = 1.0
    """if const_vs: 1."""
    vg_fac: float = 1.0
    """if const_vg: 2."""
    vi_fac: float = 1.0
    """if const_vi: 1/3"""
    vr_fac: float = 1.0
    """if const_vr: 4."""
    de_ice: bool = False
    """To prevent excessive build-up of cloud ice from external sources"""
    layout: Tuple[int, int] = (1, 1)
    # gfdl_cloud_microphys.F90
    tau_imlt: float = 600.0
    """cloud ice melting"""
    tau_i2s: float = 1000.0
    """cloud ice to snow auto - conversion"""
    tau_g2v: float = 1200.0
    """graupel sublimation"""
    tau_v2g: float = 21600.0
    """graupel deposition -- make it a slow process"""
    ql_mlt: float = 2.0e-3
    """max value of cloud water allowed from melted cloud ice"""
    qs_mlt: float = 1.0e-6
    """max cloud water due to snow melt"""
    t_sub: float = 184.0
    """min temp for sublimation of cloud ice"""
    qi_gen: float = 1.82e-6
    """max cloud ice generation during remapping step"""
    qi_lim: float = 1.0
    """cloud ice limiter to prevent large ice build up"""
    qi0_max: float = 1.0e-4
    """max cloud ice value (by other sources)"""
    rad_snow: bool = True
    """consider snow in cloud fraction calculation"""
    rad_rain: bool = True
    """consider rain in cloud fraction calculation"""
    dw_ocean: float = 0.10
    """base value for ocean"""
    dw_land: float = 0.15
    """base value for subgrid deviation / variability over land"""
    # cloud scheme 0 - ?
    # 1: old fvgfs gfdl) mp implementation
    # 2: binary cloud scheme (0 / 1)
    tau_l2v: float = 300.0
    """cloud water to water vapor (evaporation)"""
    c2l_ord: int = 4
    do_sedi_heat: bool = False
    """Transport of heat in sedimentation"""
    do_sedi_w: bool = True
    """Transport of vertical motion in sedimentation"""
    fast_sat_adj: bool = True
    qc_crt: float = 5.0e-8
    """Minimum condensate mixing ratio to allow partial cloudiness"""
    fix_negative: bool = True
    """Fix negative water species"""
    irain_f: int = 0
    """Cloud water to rain auto conversion scheme"""
    mp_time: float = 225.0
    """Maximum microphysics timestep (sec)"""
    prog_ccn: bool = False
    """Do prognostic ccn (yi ming's method)"""
    qi0_crt: float = 8e-05
    """Cloud ice to snow autoconversion threshold"""
    qs0_crt: float = 0.003
    """Snow to graupel density threshold (0.6e-3 in purdue lin scheme)"""
    rh_inc: float = 0.2
    """RH increment for complete evaporation of cloud water and cloud ice"""
    rh_inr: float = 0.3
    """RH increment for minimum evaporation of rain"""
    # rh_ins: Any
    rthresh: float = 1e-05
    """Critical cloud drop radius (micrometers)"""
    sedi_transport: bool = True
    """Transport of momentum in sedimentation"""
    # use_ccn: Any
    use_ppm: bool = False
    """Use ppm fall scheme"""
    vg_max: float = 16.0
    """Maximum fall speed for graupel"""
    vi_max: float = 1.0
    """Maximum fall speed for ice"""
    vr_max: float = 16.0
    """Maximum fall speed for rain"""
    vs_max: float = 2.0
    """Maximum fall speed for snow"""
    z_slope_ice: bool = True
    """Use linear mono slope for autoconversions"""
    z_slope_liq: bool = True
    """Use linear mono slope for autoconversions"""
    tice: float = 273.16
    """set tice = 165. to turn off ice - phase phys (kessler emulator)"""
    alin: float = 842.0
    """value for 'a' in lin1983"""
    clin: float = 4.8
    """"c" in lin 1983, 4.8 -- > 6. (to enhance ql -- > qs)"""
    ntke: int = DEFAULT_INT
    """Tracer index of turbulent kinetic energy"""
    ntiw: int = DEFAULT_INT
    """Tracer index of ice condensate"""
    ntcw: int = DEFAULT_INT
    """Tracer index of cloud water"""
    namelist_override: Optional[str] = None
    target_nml_groups: Optional[Tuple[str, ...]] = DEFAULT_PHYS_NML_GROUPS
    daily_mean: bool = DEFAULT_BOOL
    """flag to replace cosz with daily mean value"""
    prescribe_sst: bool = DEFAULT_BOOL
    """whether to use prescribed sea surface temperatures"""
    max_sst: float = 293.95
    """maximum temperature for prescribed SSTs"""
    min_sst: float = float(physcons.TICE)
    """minimum temperature for prescribed SSTs"""

    def __post_init__(self):
        if self.schemes is None:
            self.schemes = DEFAULT_SCHEMES
        package_schemes = []
        for scheme in self.schemes:
            if scheme not in PHYSICS_PACKAGES:
                raise NotImplementedError(f"{scheme} physics scheme not implemented")
            package_schemes.append(PHYSICS_PACKAGES[scheme])
        self.schemes = package_schemes
        self.ntiw = tracer_variables.index("qice")
        self.ntcw = tracer_variables.index("qliquid")
        self.ntke = tracer_variables.index("qsgs_tke")
        if self.namelist_override is not None:
            try:
                f90_nml = f90nml.read(self.namelist_override)
            except FileNotFoundError:
                print(f"{self.namelist_override} does not exist")
            # TODO: Find a better way to do below. Passing self.* as an argument
            # to a class function of the same class is always a bit fishy.
            physics_config = self.from_f90nml(f90_nml, self.target_nml_groups)
            for var in physics_config.__dict__.keys():
                setattr(self, var, physics_config.__dict__[var])
        if self.dt_atmos != 0:
            self.nsswr = int(self.fhswr / self.dt_atmos)
            self.nslwr = int(self.fhlwr / self.dt_atmos)

    @classmethod
    def from_f90nml(
        cls,
        nml: f90nml.Namelist,
        target_groups: Tuple[str, ...] | None = DEFAULT_PHYS_NML_GROUPS,
    ) -> PhysicsConfig:
        """Uses the nml to create a PhysicsConfig.

        Args:
            nml: f90nml.Namelist
            target_groups: Tuple[str,...] | None
                This list will be used to specify which groups in the nml to
                use when initializing the PhysicsConfig. If None, all
                groups will be used. (Default: DEFAULT_PHYS_NML_GROUPS)
        """
        groups = list(target_groups) if target_groups is not None else None
        nml_dict = f90nml_as_dict(nml, flatten=True, target_groups=groups)
        nml_dict["target_nml_groups"] = target_groups
        return cls.from_dict(nml_dict)

    @classmethod
    def from_dict(
        cls,
        data: dict,
    ) -> PhysicsConfig:
        """Create a PhysicsConfig from the given data.

        Args:
            data: "flattened" dictionary where the keys match the class member variables
        """
        # NOTE: We're setting strict to False so that extra keys in the data are
        # ignored. Eventually, we'd like to turn this to True once we move away from
        # expecting dicts that are basically flattened f90nml files.
        dacite_config = Config(
            strict=False,
            type_hooks={
                Tuple[int, int]: lambda x: tuple(x),
                Tuple[str, ...]: lambda x: tuple(x) if x is not None else None,
            },
        )
        return from_dict(data_class=PhysicsConfig, data=data, config=dacite_config)
