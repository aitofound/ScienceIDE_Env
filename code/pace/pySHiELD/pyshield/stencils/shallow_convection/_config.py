import dataclasses

from pyshield.tracer_workarounds import tracer_variables

_DEFAULT_INT = 0
DEFAULT_BOOL = False
DEFAULT_FLOAT = 0.0


@dataclasses.dataclass
class ShallowConvectionConfig:
    dt_atmos: float = DEFAULT_FLOAT
    """timestep length (s)"""
    ntke: int = -1
    """index of tke tracer"""
    nsamftrac: int = 5
    """number of tracers convected (excludes cloud condensates)"""
    ncld: int = 1
    """Choice of cloud scheme"""
    ntchm: int = _DEFAULT_INT
    """number of chemical tracers"""
    ntvap: int = _DEFAULT_INT
    """index of vapor tracer"""
    ntcw: int = -1
    """index of cloud water tracer"""
    ntiw: int = -1
    """index pf cloud ice tracer"""
    ntcld: int = _DEFAULT_INT
    """Index of cloud tracer"""
    itc: int = _DEFAULT_INT
    """index of first chemical tracer"""
    clam_shal: float = 0.3
    """c_e for shallow convection (Han and Pan, 2011, eq(6))"""
    c0s_shal: float = 0.002
    """conversion parameter of detrainment from liquid water
    into convetive precipitaiton (1/m)"""
    c1_shal: float = 5.0e-4
    """conversion parameter of detrainment from liquid water
    into grid-scale cloud water (1/m)"""
    pgcon_shal: float = 0.55
    """reduction factor in momentum transport
    due to convection induced pressure gradient force
    0.7 : Gregory et al. (1997, QJRMS)
    0.55: Zhang & Wu (2003, JAS)
    """
    cthk: float = 200.0
    """max cloud top for shallow convection"""
    top_shal: float = 0.7
    """max cloud height for shallow convection (P/Ps < top_shal)"""
    limit_shal_conv: bool = False
    """flag for constraining shal conv based on diagnosed cloud depth/top"""
    asolfac_shal: float = 0.89
    """aerosol-aware parameter inversely proportional to CCN number concentraion
    based on Lim & Hong (2012)"""
    fscav: list = dataclasses.field(default_factory=list)
    """aerosol scavenging coefficients"""

    def __post_init__(self):
        if self.ntiw == -1:
            self.ntiw = tracer_variables.index("qice")
        if self.ntcw == -1:
            self.ntcw = tracer_variables.index("qliquid")
        if self.ntke == -1:
            self.ntke = tracer_variables.index("qsgs_tke")
        if self.ntcld == -1:
            self.ntke = tracer_variables.index("qcld")
        if self.ntvap == -1:
            self.ntke = tracer_variables.index("qvapor")
