from __future__ import annotations

import dataclasses
from datetime import timedelta
from math import floor

import f90nml
import yaml
from dacite import Config, from_dict

from ndsl.utils import f90nml_as_dict

DEFAULT_INT = 0
DEFAULT_STR = ""
DEFAULT_FLOAT = 0.0
DEFAULT_BOOL = False
DEFAULT_DYCORE_NML_GROUPS = (
    "main_nml",
    "coupler_nml",
    "fv_core_nml",
)


@dataclasses.dataclass(frozen=True)
class SatAdjustConfig:
    hydrostatic: bool
    rad_snow: bool
    rad_rain: bool
    rad_graupel: bool
    tintqs: bool
    sat_adj0: float
    ql_gen: float
    qs_mlt: float
    ql0_max: float
    t_sub: float
    qi_gen: float
    qi_lim: float
    qi0_max: float
    dw_ocean: float
    dw_land: float
    icloud_f: int
    cld_min: float
    tau_i2s: float
    tau_v2l: float
    tau_r2g: float
    tau_l2r: float
    tau_l2v: float
    tau_imlt: float
    tau_smlt: float


@dataclasses.dataclass(frozen=True)
class RemappingConfig:
    fill: bool
    kord_tm: int
    kord_tr: int
    kord_wz: int
    kord_mt: int
    do_sat_adj: bool
    sat_adjust: SatAdjustConfig

    @property
    def hydrostatic(self) -> bool:
        return self.sat_adjust.hydrostatic


@dataclasses.dataclass(frozen=True)
class RiemannConfig:
    p_fac: float
    a_imp: float
    use_logp: bool
    beta: float


@dataclasses.dataclass(frozen=True)
class DGridShallowWaterLagrangianDynamicsConfig:
    dddmp: float
    d2_bg: float
    d2_bg_k1: float
    d2_bg_k2: float
    d4_bg: float
    ke_bg: float
    nord: int
    n_sponge: int
    grid_type: int
    d_ext: float
    hord_dp: int
    hord_tm: int
    hord_mt: int
    hord_vt: int
    do_f3d: bool
    do_skeb: bool
    d_con: float
    vtdm4: float
    inline_q: bool
    convert_ke: bool
    do_vort_damp: bool
    hydrostatic: bool


@dataclasses.dataclass(frozen=True)
class AcousticDynamicsConfig:
    tau: float
    k_split: int
    n_split: int
    m_split: int
    delt_max: float
    rf_cutoff: float
    rf_fast: bool
    breed_vortex_inline: bool
    """
    setting for nudging where we can insert tropical cyclone tracks
    and add fake tropical cyclones at a certain point in the code
    not used so much right now because we can run at high enough
    resolution to directly nudge to tropical cyclone data
    """
    use_old_omega: bool
    """
    mainly for backwards compatibility, not really used anymore
    """
    riemann: RiemannConfig
    d_grid_shallow_water: DGridShallowWaterLagrangianDynamicsConfig

    @property
    def nord(self) -> int:
        return self.d_grid_shallow_water.nord

    @property
    def grid_type(self) -> int:
        return self.d_grid_shallow_water.grid_type

    @property
    def hydrostatic(self) -> bool:
        return self.d_grid_shallow_water.hydrostatic

    @property
    def hord_tm(self) -> int:
        return self.d_grid_shallow_water.hord_tm

    @property
    def p_fac(self) -> float:
        return self.riemann.p_fac

    @property
    def d_ext(self) -> float:
        return self.d_grid_shallow_water.d_ext

    @property
    def d_con(self) -> float:
        return self.d_grid_shallow_water.d_con

    @property
    def beta(self) -> float:
        return self.riemann.beta

    @property
    def use_logp(self) -> bool:
        return self.riemann.use_logp


@dataclasses.dataclass
class DynamicalCoreConfig:
    dt_atmos: int = DEFAULT_INT
    n_steps: int = 1
    a_imp: float = DEFAULT_FLOAT
    beta: float = DEFAULT_FLOAT
    consv_te: float = DEFAULT_FLOAT
    d2_bg: float = DEFAULT_FLOAT
    d2_bg_k1: float = DEFAULT_FLOAT
    d2_bg_k2: float = DEFAULT_FLOAT
    d4_bg: float = DEFAULT_FLOAT
    d_con: float = DEFAULT_FLOAT
    d_ext: float = DEFAULT_FLOAT
    dddmp: float = DEFAULT_FLOAT
    delt_max: float = DEFAULT_FLOAT
    do_sat_adj: bool = DEFAULT_BOOL
    do_vort_damp: bool = DEFAULT_BOOL
    fill: bool = DEFAULT_BOOL
    hord_dp: int = DEFAULT_INT
    hord_mt: int = DEFAULT_INT
    hord_tm: int = DEFAULT_INT
    hord_tr: int = DEFAULT_INT
    hord_vt: int = DEFAULT_INT
    hydrostatic: bool = DEFAULT_BOOL
    k_split: int = DEFAULT_INT
    ke_bg: float = DEFAULT_FLOAT
    kord_mt: int = DEFAULT_INT
    kord_tm: int = DEFAULT_INT
    kord_tr: int = DEFAULT_INT
    kord_wz: int = DEFAULT_INT
    n_split: int = DEFAULT_INT
    nord: int = DEFAULT_INT
    npx: int = DEFAULT_INT
    npy: int = DEFAULT_INT
    npz: int = DEFAULT_INT
    ntiles: int = DEFAULT_INT
    nwat: int = DEFAULT_INT
    p_fac: float = DEFAULT_FLOAT
    rf_cutoff: float = DEFAULT_FLOAT
    tau: float = DEFAULT_FLOAT
    vtdm4: float = DEFAULT_FLOAT
    z_tracer: bool = DEFAULT_BOOL
    do_qa: bool = DEFAULT_BOOL
    layout: tuple[int, int] = (1, 1)
    grid_type: int = 0
    u_max: float = 350.0
    """max windspeed for dp config"""
    do_f3d: bool = False
    inline_q: bool = False
    do_skeb: bool = False
    """save dissipation estimate"""
    use_logp: bool = False
    moist_phys: bool = True
    check_negative: bool = False
    # gfdl_cloud_microphys.F90
    tau_r2g: float = 900.0
    """rain freezing during fast_sat"""
    tau_smlt: float = 900.0
    """snow melting"""
    tau_g2r: float = 600.0
    """graupel melting to rain"""
    tau_imlt: float = 600.0
    """cloud ice melting"""
    tau_i2s: float = 1000.0
    """cloud ice to snow auto - conversion"""
    tau_l2r: float = 900.0
    """cloud water to rain auto - conversion"""
    tau_g2v: float = 1200.0
    """graupel sublimation"""
    tau_v2g: float = 21600.0
    """graupel deposition -- make it a slow process"""
    sat_adj0: float = 0.90
    """adjustment factor (0: no 1: full) during fast_sat_adj"""
    ql_gen: float = (
        1.0e-3  # max new cloud water during remapping step if fast_sat_adj = .t.
    )
    ql_mlt: float = 2.0e-3
    """max value of cloud water allowed from melted cloud ice"""
    qs_mlt: float = 1.0e-6
    """max cloud water due to snow melt"""
    ql0_max: float = 2.0e-3
    """max cloud water value (auto converted to rain)"""
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
    rad_graupel: bool = True
    """consider graupel in cloud fraction calculation"""
    tintqs: bool = False
    """use temperature in the saturation mixing in PDF"""
    dw_ocean: float = 0.10
    """base value for ocean"""
    dw_land: float = 0.15
    """base value for subgrid deviation / variability over land"""
    # cloud scheme 0 - ?
    # 1: old fvgfs gfdl) mp implementation
    # 2: binary cloud scheme (0 / 1)
    icloud_f: int = 0
    cld_min: float = 0.05
    """!< minimum cloud fraction"""
    tau_l2v: float = 300.0
    """cloud water to water vapor (evaporation)"""
    tau_v2l: float = 90.0
    """water vapor to cloud water (condensation)"""
    c2l_ord: int = 4
    regional: bool = False
    m_split: int = 0
    convert_ke: bool = False
    breed_vortex_inline: bool = False
    use_old_omega: bool = True
    rf_fast: bool = False
    adiabatic: bool = False
    nf_omega: int = 1
    fv_sg_adj: int = -1
    n_sponge: int = 1
    sw_dynamics: bool = False
    """shallow water conditions"""
    namelist_override: str | None = None
    target_nml_groups: tuple[str, ...] | None = DEFAULT_DYCORE_NML_GROUPS

    def __post_init__(self) -> None:
        if self.namelist_override is not None:
            try:
                f90_nml = f90nml.read(self.namelist_override)
            except FileNotFoundError:
                print(f"{self.namelist_override} does not exist")
                raise
            # TODO: Find a better way to do below. Passing self.* as an argument
            # to a class function of the same class is always a bit fishy.
            dycore_config = self.from_f90nml(f90_nml, self.target_nml_groups)
            for var in dycore_config.__dict__.keys():
                setattr(self, var, dycore_config.__dict__[var])
        # Single tile cartesian grids
        if self.grid_type > 3:
            self.nf_omega = 0

    @classmethod
    def from_f90nml(
        cls,
        nml: f90nml.Namelist,
        target_groups: tuple[str, ...] | None = DEFAULT_DYCORE_NML_GROUPS,
    ) -> DynamicalCoreConfig:
        """Uses the nml to create a DynamicalCoreConfig.

        Args:
            nml: f90nml.Namelist
            target_groups: tuple[str,...] | None
                This list will be used to specify which groups in the nml to
                use when initializing the DynamicalCoreConfig. If None, all
                groups will be used. (Default: DEFAULT_DYCORE_NML_GROUPS)
        """
        groups = list(target_groups) if target_groups is not None else None
        nml_dict = f90nml_as_dict(nml, flatten=True, target_groups=groups)
        nml_dict["target_nml_groups"] = target_groups
        return cls.from_dict(nml_dict)

    @classmethod
    def from_dict(
        cls,
        data: dict,
    ) -> DynamicalCoreConfig:
        """Create a DynamicalCoreConfig from the given data.

        Args:
            data: "flattened" dictionary where the keys match the class member variables
        """
        # NOTE: We're setting strict to False so that extra keys in the data are
        # ignored. Eventually, we'd like to turn this to True once we move away from
        # expecting dicts that are basically flattened yamls and f90nml files.
        dacite_config = Config(
            strict=False,
            type_hooks={
                tuple[int, int]: lambda x: tuple(x),
                tuple[str, ...]: lambda x: tuple(x) if x is not None else None,
            },
        )
        dycore_config = from_dict(
            data_class=DynamicalCoreConfig, data=data, config=dacite_config
        )
        return dycore_config

    @classmethod
    def from_yaml(cls, yaml_config: str) -> DynamicalCoreConfig:
        config = cls()
        with open(yaml_config, "r") as f:
            raw_config = yaml.safe_load(f)
        flat_config: dict = {}
        timestep = timedelta(seconds=raw_config["dt_atmos"])
        runtime = {
            "days": 0.0,
            "hours": 0.0,
            "minutes": 0.0,
            "seconds": 0.0,
        }
        for key in runtime.keys():
            if key in raw_config.keys():
                runtime[key] = raw_config[key]

        total_time = timedelta(
            days=runtime["days"],
            hours=runtime["hours"],
            minutes=runtime["minutes"],
            seconds=runtime["seconds"],
        )
        for key, value in raw_config.items():
            if isinstance(value, dict):
                for subkey, subvalue in value.items():
                    if subkey in config.__annotations__.keys():
                        if subkey in flat_config:
                            if subvalue != flat_config[subkey]:
                                raise ValueError(
                                    "Cannot flatten this config ",
                                    f"duplicate keys: {subkey}",
                                )
                        flat_config[subkey] = subvalue
            else:
                if key == "nx_tile":
                    flat_config["npx"] = value + 1
                    flat_config["npy"] = value + 1
                elif key == "nz":
                    flat_config["npz"] = value
                else:
                    if key in config.__annotations__.keys():
                        flat_config[key] = value
        for field in dataclasses.fields(config):
            if field.name in flat_config.keys():
                setattr(config, field.name, flat_config[field.name])
        config.n_steps = floor(total_time.total_seconds() / timestep.total_seconds())
        return config

    @property
    def do_dry_convective_adjustment(self) -> bool:
        return self.fv_sg_adj > 0

    @property
    def riemann(self) -> RiemannConfig:
        return RiemannConfig(
            p_fac=self.p_fac,
            a_imp=self.a_imp,
            use_logp=self.use_logp,
            beta=self.beta,
        )

    @property
    def d_grid_shallow_water(self) -> DGridShallowWaterLagrangianDynamicsConfig:
        return DGridShallowWaterLagrangianDynamicsConfig(
            dddmp=self.dddmp,
            d2_bg=self.d2_bg,
            d2_bg_k1=self.d2_bg_k1,
            d2_bg_k2=self.d2_bg_k2,
            d4_bg=self.d4_bg,
            ke_bg=self.ke_bg,
            nord=self.nord,
            n_sponge=self.n_sponge,
            grid_type=self.grid_type,
            d_ext=self.d_ext,
            inline_q=self.inline_q,
            hord_dp=self.hord_dp,
            hord_tm=self.hord_tm,
            hord_mt=self.hord_mt,
            hord_vt=self.hord_vt,
            do_f3d=self.do_f3d,
            do_skeb=self.do_skeb,
            d_con=self.d_con,
            vtdm4=self.vtdm4,
            do_vort_damp=self.do_vort_damp,
            hydrostatic=self.hydrostatic,
            convert_ke=self.convert_ke,
        )

    @property
    def acoustic_dynamics(self) -> AcousticDynamicsConfig:
        return AcousticDynamicsConfig(
            tau=self.tau,
            k_split=self.k_split,
            n_split=self.n_split,
            m_split=self.m_split,
            delt_max=self.delt_max,
            rf_fast=self.rf_fast,
            rf_cutoff=self.rf_cutoff,
            breed_vortex_inline=self.breed_vortex_inline,
            use_old_omega=self.use_old_omega,
            riemann=self.riemann,
            d_grid_shallow_water=self.d_grid_shallow_water,
        )

    @property
    def sat_adjust(self) -> SatAdjustConfig:
        return SatAdjustConfig(
            hydrostatic=self.hydrostatic,
            rad_snow=self.rad_snow,
            rad_rain=self.rad_rain,
            rad_graupel=self.rad_graupel,
            tintqs=self.tintqs,
            sat_adj0=self.sat_adj0,
            ql_gen=self.ql_gen,
            qs_mlt=self.qs_mlt,
            ql0_max=self.ql0_max,
            t_sub=self.t_sub,
            qi_gen=self.qi_gen,
            qi_lim=self.qi_lim,
            qi0_max=self.qi0_max,
            dw_ocean=self.dw_ocean,
            dw_land=self.dw_land,
            icloud_f=self.icloud_f,
            cld_min=self.cld_min,
            tau_i2s=self.tau_i2s,
            tau_v2l=self.tau_v2l,
            tau_r2g=self.tau_r2g,
            tau_l2r=self.tau_l2r,
            tau_l2v=self.tau_l2v,
            tau_imlt=self.tau_imlt,
            tau_smlt=self.tau_smlt,
        )

    @property
    def remapping(self) -> RemappingConfig:
        return RemappingConfig(
            fill=self.fill,
            kord_tm=self.kord_tm,
            kord_tr=self.kord_tr,
            kord_wz=self.kord_wz,
            kord_mt=self.kord_mt,
            do_sat_adj=self.do_sat_adj,
            sat_adjust=self.sat_adjust,
        )
