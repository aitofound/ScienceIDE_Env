import dataclasses

from pyshield.tracer_workarounds import tracer_variables

DEFAULT_INT = 0
DEFAULT_FLOAT = 0.0
DEFAULT_BOOL = False


@dataclasses.dataclass
class PBLConfig:
    dt_atmos: float = DEFAULT_FLOAT
    hydrostatic: bool = DEFAULT_BOOL
    isatmedmf: int = 0
    """flag for specific scale-aware turbulent moist edmf scheme"""
    # 0: Initial version of satmedmf by Kun Gao in 2018
    # 1: Updated version of satmedmf by Kun Gao in 2019
    # Only 0 has been implemented so far
    xkzm_h: float = 1.0
    """Background vertical diffusion for heat q over ocean"""
    xkzm_m: float = 1.0
    """Background vertical diffusion for momentum over ocean"""
    xkzm_hl: float = 1.0
    """Background vertical diffusion for heat q over land"""
    xkzm_ml: float = 1.0
    """Background vertical diffusion for momentum over land"""
    xkzm_hi: float = 1.0
    """Background vertical diffusion for heat q over ice"""
    xkzm_mi: float = 1.0
    """Background vertical diffusion for momentum over ice"""
    xkzm_ho: float = 1.0
    """Background vertical diffusion for heat q over ocean"""
    xkzm_mo: float = 1.0
    """Background vertical diffusion for momentum over ocean"""
    xkzminv: float = 0.15
    """Diffusivity in inversion layers"""
    xkzm_s: float = 1.0
    """Sigma threshold for background momentum diffusion"""
    xkzm_lim: float = 0.01
    """Background diffusion limit"""
    xkgdx: float = 25.0e3
    """Background vertical diffusion threshold"""
    do_dk_hb19: bool = DEFAULT_BOOL
    """Flag to use HB19 background diffusion formula in satmedmf"""
    rlmn: float = 30.0
    """Lower limit on asymptotic mixing length in satmedmf"""
    rlmx: float = 300.0
    """Upper limit on asymptotic mixing length in satmedmf"""
    ntracers: int = int(len(tracer_variables))
    """Number of tracers"""
    ntiw: int = DEFAULT_INT
    """Tracer index of ice water"""
    ntcw: int = DEFAULT_INT
    """Tracer index of cloud water"""
    ntke: int = DEFAULT_INT
    """Tracer index of subgrid turbulent kinetic energy"""
    dspheat: bool = False
    """Flag for dissipative heating"""
    cap_k0_land: bool = True
    """Flag to apply limiter on background diffusivity in inversion layer over land"""

    def __post_init__(self):
        if self.isatmedmf != 0:
            raise NotImplementedError(
                f"PBL Config: isatmedmf == {self.isatmedmf} not implemented"
            )
        self.ntiw = tracer_variables.index("qice")
        self.ntcw = tracer_variables.index("qliquid")
        self.ntke = tracer_variables.index("qsgs_tke")
