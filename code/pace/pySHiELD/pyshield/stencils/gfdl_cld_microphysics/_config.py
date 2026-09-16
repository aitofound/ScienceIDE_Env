import dataclasses
import math
from typing import List, Tuple

import ndsl.constants as constants
import pyshield.stencils.gfdl_cld_microphysics.constants as mpcons

DEFAULT_INT = 0
DEFAULT_FLOAT = 0.0
DEFAULT_STR = ""
DEFAULT_BOOL = False


@dataclasses.dataclass
class AdjustNegativeTracerConfig:
    ntimes: float
    c1_ice: float
    c1_liq: float
    c1_vap: float
    d1_ice: float
    d1_vap: float
    li00: float
    li20: float
    lv00: float
    t_wfr: float


@dataclasses.dataclass
class FastMPConfig:
    do_warm_rain_mp: bool
    do_wbf: bool
    c1_vap: float
    c1_liq: float
    c1_ice: float
    lv00: float
    li00: float
    li20: float
    d1_vap: float
    d1_ice: float
    t_wfr: float
    ql_mlt: float
    qs_mlt: float
    tau_imlt: float
    tice_mlt: float
    do_cond_timescale: bool
    do_evap_timescale: bool
    do_hail: bool
    rh_fac_evap: float
    rh_fac_cond: float
    rhc_cevap: float
    tau_l2v: float
    tau_v2l: float
    tau_r2g: float
    tau_smlt: float
    tau_gmlt: float
    tau_l2r: float
    use_rhc_cevap: bool
    qi0_crt: float
    qi0_max: float
    ql0_max: float
    tau_wbf: float
    do_psd_water_num: bool
    do_psd_ice_num: bool
    muw: float
    mui: float
    mur: float
    mus: float
    mug: float
    muh: float
    pcaw: float
    pcbw: float
    pcai: float
    pcbi: float
    prog_ccn: float
    inflag: int
    igflag: int
    qi_lim: float
    t_sub: float
    is_fac: float
    tau_i2s: float
    fast_fr_mlt: bool
    fast_dep_sub: bool
    delay_cond_evap: bool
    nconds: int


@dataclasses.dataclass
class GFDLCloudMPConfig:
    dt_full: float = 1.0
    dt_split: float = dataclasses.field(init=False)
    ntimes: int = 1
    """Number of cloud microphysics sub cycles"""
    nconds: int = 1
    """condensation sub cycles"""
    hydrostatic: bool = DEFAULT_BOOL
    npx: int = DEFAULT_INT
    npy: int = DEFAULT_INT
    npz: int = DEFAULT_INT
    layout: Tuple[int, int] = (1, 1)
    nwat: int = DEFAULT_INT
    tau_r2g: float = 900.0
    """rain freezing during fast_sat"""
    tau_smlt: float = 900.0
    """snow melting timescale"""
    tau_gmlt: float = 600.0
    """snow melting timescale"""
    tau_g2r: float = 600.0
    """graupel melting to rain"""
    tau_imlt: float = 1200.0
    """cloud ice melting"""
    tau_i2s: float = 1000.0
    """cloud ice to snow auto - conversion"""
    tau_l2r: float = 900.0
    """cloud water to rain auto - conversion"""
    tau_l2v: float = 300.0
    """cloud water to water vapor (evaporation)"""
    tau_v2l: float = 150.0
    """water vapor to cloud water (condensation)"""
    tau_revp: float = 0.0
    """rain evaporation time scale (s)"""
    tau_g2v: float = 1200.0
    """graupel sublimation"""
    tau_v2g: float = 21600.0
    """graupel deposition -- make it a slow process"""
    tau_wbf: float = 300.0
    """wegener bergeron findeisen timescale"""
    sat_adj0: float = 0.90
    """adjustment factor (0: no, 1: full) during fast_sat_adj"""
    ql_gen: float = 1.0e-3
    """max new cloud water during remapping step if fast_sat_adj = .t."""
    ql_mlt: float = 2.0e-3
    """max value of cloud water allowed from melted cloud ice"""
    qs_mlt: float = 1.0e-6
    """max cloud water due to snow melt"""
    ql0_max: float = 2.0e-3
    """max cloud water value (auto converted to rain)"""
    t_min: float = 178.0
    """minimum temperature to freeze - dry all water vapor (K)"""
    t_sub: float = 184.0
    """min temp for sublimation of cloud ice"""
    qi_gen: float = 1.82e-6
    """max cloud ice generation during remapping step"""
    qi_lim: float = 1.0
    """cloud ice limiter to prevent large ice build up"""
    qi0_max: float = 1.0e-4
    """max cloud ice value (by other sources)"""
    rad_snow: bool = True
    """consider snow in cloud fraciton calculation"""
    rad_rain: bool = True
    """consider rain in cloud fraction calculation"""
    rad_graupel: bool = True
    """consider graupel in cloud fraction calculation"""
    do_cld_adj: bool = False
    """do cloud fraction adjustment"""
    tintqs: bool = False
    """use temperature in the saturation mixing in PDF"""
    dw_ocean: float = 0.10
    """base value for ocean"""
    dw_land: float = 0.20
    """
    base value for subgrid deviation / variability over land
     - cloud scheme 0 - ?
     - 1: old fvgfs gfdl) mp implementation
     - 2: binary cloud scheme (0 / 1)
    """
    icloud_f: int = 0
    """
    GFDL cloud scheme
     - 0: subgrid variability based scheme
     - 1: same as 0, but for old fvgfs implementation
     - 2: binary cloud scheme
     - 3: extension of 0
    """
    cld_min: float = 0.05
    """minimum cloud fraction"""
    c2l_ord: int = 4
    regional: bool = False
    m_split: int = 0
    convert_ke: bool = False
    breed_vortex_inline: bool = False
    use_old_omega: bool = True
    use_logp: bool = False
    rf_fast: bool = False
    p_ref: float = 1.0e5
    """Surface pressure used to construct a horizontally-uniform reference"""
    adiabatic: bool = False
    nf_omega: int = 1
    fv_sg_adj: int = -1
    n_sponge: int = 1
    fast_sat_adj: bool = True
    qc_crt: float = 5.0e-8
    """Minimum condensate mixing ratio to allow partial cloudiness"""
    c_cracw: float = 0.8
    """Rain accretion efficiency"""
    c_paut: float = 0.55
    """Autoconversion cloud water to rain (use 0.5 to reduce autoconversion)"""
    c_pracs: float = 1.0
    """snow to rain accretion efficiency"""
    c_psacr: float = 1.0
    """rain to snow accretion efficiency"""
    c_pgacr: float = 1.0
    """rain to graupel accretion efficiency"""
    c_pgacs: float = 0.01
    """Snow to graupel "accretion" eff. (was 0.1 in zetac)"""
    c_psacw: float = 1.0
    """Cloud water to snow accretion efficiency"""
    c_psaci: float = 0.05
    """Accretion: cloud ice to snow (was 0.1 in zetac)"""
    c_pracw: float = 0.8
    """Cloud water to rain accretion efficiency"""
    c_praci: float = 1.0
    """Cloud ice to rain accretion efficiency"""
    c_pgacw: float = 1.0
    """Cloud water to graupel accretion efficiency"""
    c_pgaci: float = 0.05
    """Cloud ice to graupel accretion efficiency (was 0.1 in ZETAC)"""
    ccn_l: float = 270.0
    """CCN over land (cm^-3)"""
    ccn_o: float = 90.0
    """CCN over ocean (cm^-3)"""
    use_rhc_cevap: bool = False
    """cap of rh for cloud water evaporation"""
    use_rhc_revap: bool = False
    """cap of rh for rain evaporation"""
    const_vw: bool = False
    """Whether to tune fall velocity constant of cloud water by v_fac"""
    const_vg: bool = False
    """Whether to tune fall velocity constant of graupel by v_fac"""
    const_vi: bool = False
    """Whether to tune fall velocity constant of ice by v_fac"""
    const_vr: bool = False
    """Whether to tune fall velocity constant of rain water by v_fac"""
    const_vs: bool = False
    """Whether to tune fall velocity constant of snow by v_fac"""
    is_fac: float = 0.2
    """Cloud ice sublimation temperature factor"""
    ss_fac: float = 0.2
    """Snow sublimation temperature factor"""
    gs_fac: float = 0.2
    """Graupel sublimation temperature factor"""
    rh_fac_evap: float = 10.0
    """cloud water evaporation relative humidity factor"""
    rh_fac_cond: float = 10.0
    """cloud water condensation relative humidity factor"""
    sed_fac: float = 1.0
    """coefficient for sedimentation fall,
    scale from 1.0 (implicit) to 0.0 (lagrangian)"""
    xr_a: float = 0.25
    """p value in Xu and Randall (1996)"""
    xr_b: float = 100.0
    """alpha_0 value in Xu and Randall (1996)"""
    xr_c: float = 0.49
    """gamma value in Xu and Randall (1996)"""
    te_err: float = 1.0e-5
    """64bit: 1.e-14, 32bit: 1.e-7; turn off to save computer time"""
    tw_err: float = 1.0e-8
    """64bit: 1.e-14, 32bit: 1.e-7; turn off to save computer time"""
    rh_thres: float = 0.75
    """minimum relative humidity for cloud fraction"""
    rhc_cevap: float = 0.85
    """maximum relative humidity for cloud water evaporation"""
    rhc_revap: float = 0.85
    """maximum relative humidity for rain evaporation"""
    f_dq_p: float = 1.0
    """cloud fraction adjustment for supersaturation"""
    f_dq_m: float = 1.0
    """cloud fraction adjustment for undersaturation"""
    fi2s_fac: float = 1.0
    """maximum sink of cloud ice to form snow: 0-1"""
    fi2g_fac: float = 1.0
    """maximum sink of cloud ice to form graupel: 0-1"""
    fs2g_fac: float = 1.0
    """maximum sink of snow to form graupel: 0-1"""
    vw_fac: float = 1.0
    vi_fac: float = 1.0
    """if const_vi: 1/3"""
    vs_fac: float = 1.0
    """if const_vs: 1."""
    vg_fac: float = 1.0
    """if const_vg: 2."""
    vr_fac: float = 1.0
    """if const_vr: 4."""
    de_ice: bool = False
    """To prevent excessive build-up of cloud ice from external sources"""
    do_qa: bool = True
    """Do inline cloud fraction"""
    do_sedi_heat: bool = True
    """Transport of heat in sedimentation"""
    do_sedi_melt: bool = True
    """Melt cloud ice, snow, and graupel during sedimentation"""
    do_sedi_uv: bool = True
    """Transport of horizontal momentum in sedimentation"""
    do_sedi_w: bool = True
    """Transport of vertical motion in sedimentation"""
    fix_negative: bool = True
    """Fix negative water species"""
    do_cond_timescale: bool = False
    """Whether to apply a timescale to condensation"""
    do_evap_timescale: bool = True
    """Whether to apply a timescale to evaporation"""
    do_hail: bool = False
    """Use hail parameters instead of graupel"""
    consv_checker: bool = False
    """Turn on energy and water conservation check in microphysics"""
    do_warm_rain_mp: bool = False
    """Do only warm rain microphysics"""
    do_wbf: bool = False
    """Do Wegener Bergeron Findeisen process"""
    do_psd_water_fall: bool = False
    """Calculate cloud water terminal velocity based on PSD"""
    do_psd_ice_fall: bool = False
    """Calculate cloud ice terminal velocity based on PSD"""
    do_psd_water_num: bool = False
    """Calculate cloud water number concentration based on PSD"""
    do_psd_ice_num: bool = False
    """Calculate cloud ice number concentration based on PSD"""
    do_new_acc_water: bool = False
    """Perform the new accretion for cloud water"""
    do_new_acc_ice: bool = False
    """Perform the new accretion for cloud water"""
    cp_heating: bool = False
    """update temperature based on constant pressure"""
    delay_cond_evap: bool = False
    """do condensation evaporation only at the last time step"""
    fast_fr_mlt: bool = True
    """do freezing and melting in fast microphysics"""
    fast_dep_sub: bool = True
    """do deposition and sublimation in fast microphysics"""
    mono_prof: bool = False
    """Perform terminal fall with mono ppm scheme"""
    mp_time: float = 150.0
    """Maximum microphysics timestep (sec)"""
    prog_ccn: bool = False
    """Do prognostic ccn (yi ming's method)"""
    qi0_crt: float = 1.0e-04
    """Cloud ice to snow autoconversion threshold"""
    qs0_crt: float = 1.0e-3
    """Snow to graupel density threshold (0.6e-3 in purdue lin scheme)"""
    rh_inc: float = 0.25
    """RH increment for complete evaporation of cloud water and cloud ice"""
    rh_inr: float = 0.25
    """RH increment for minimum evaporation of rain"""
    rthresh: float = 10.0e-6
    """Critical cloud drop radius (micrometers)"""
    sedi_transport: bool = True
    """Transport of momentum in sedimentation"""
    use_ppm: bool = False
    """Use ppm fall scheme"""
    vw_max: float = 0.01
    """Maximum fall speed for cloud water"""
    vg_max: float = 8.0
    """Maximum fall speed for graupel"""
    vi_max: float = 0.5
    """Maximum fall speed for ice"""
    vr_max: float = 12.0
    """Maximum fall speed for rain"""
    vs_max: float = 5.0
    """Maximum fall speed for snow"""
    z_slope_ice: bool = True
    """Use linear mono slope for autoconversions"""
    z_slope_liq: bool = True
    """Use linear mono slope for autoconversions"""
    tice: float = 273.16
    """set tice = 165. to turn off ice - phase phys (kessler emulator)"""
    tice_mlt: float = 273.16
    """Can set ice melting temperature to 268
    based on observation (Kay et al. 2016) (K)"""
    alin: float = 842.0
    """'a' in lin1983"""
    alinw: float = 3.0e7
    """'a' in Lin et al. (1983) for cloud water (Ikawa and Saito 1990)"""
    alini: float = 7.0e2
    """'a' in Lin et al. (1983) for cloud ice (Ikawa and Saita 1990)"""
    alinr: float = 842.0
    """'a' in Lin et al. (1983) for rain (Liu and Orville 1969)"""
    alins: float = 4.8
    """'a' in Lin et al. (1983) for snow (straka 2009)"""
    aling: float = 1.0
    """'a' in Lin et al. (1983) for graupel (Pruppacher and Klett 2010)"""
    alinh: float = 1.0
    """'a' in Lin et al. (1983) for hail (Pruppacher and Klett 2010)"""
    blinw: float = 2.0
    """'b' in Lin et al. (1983) for cloud water (Ikawa and Saito 1990)"""
    blini: float = 1.0
    """'b' in Lin et al. (1983) for cloud ice (Ikawa and Saita 1990)"""
    blinr: float = 0.8
    """'b' in Lin et al. (1983) for rain (Liu and Orville 1969)"""
    blins: float = 0.25
    """'b' in Lin et al. (1983) for snow (straka 2009)"""
    bling: float = 0.5
    """'b' in Lin et al. (1983) for graupel (Pruppacher and Klett 2010)"""
    blinh: float = 0.5
    """'b' in Lin et al. (1983) for hail (Pruppacher and Klett 2010)"""
    clin: float = 4.8
    """'c' in lin 1983, 4.8 -- > 6. (to ehance ql -- > qs)"""
    do_inline_mp: bool = False
    """Whether the microphsyics is called inside of the dycore"""
    do_mp_table_emulation: bool = False
    """Whether lookup tables should be emulated instead of
    directly calculating values in microphysics, useful for validation"""
    n0w_sig: float = 1.1
    """cwater significand (Lin et al. 1983) (m^-4) (Martin et al. 1994)"""
    n0i_sig: float = 1.3
    """cice significand (Lin et al. 1983) (m^-4) (McFarquhar et al. 2015)"""
    n0r_sig: float = 8.0
    """rain significand (Lin et al. 1983) (m^-4) (Marshall and Palmer 1948)"""
    n0s_sig: float = 3.0
    """snow significand (Lin et al. 1983) (m^-4) (Gunn and Marshall 1958)"""
    n0g_sig: float = 4.0
    """graupel significand (Rutledge and Hobbs 1984) (m^-4) (Houze et al. 1979)"""
    n0h_sig: float = 4.0
    """hail significand (Lin et al. 1983) (m^-4) (Federer and Waldvogel 1975)"""
    n0w_exp: float = 41.0
    """cwater exponent (Lin et al. 1983) (m^-4) (Martin et al. 1994)"""
    n0i_exp: float = 18.0
    """cice exponent (Lin et al. 1983) (m^-4) (McFarquhar et al. 2015)"""
    n0r_exp: float = 6.0
    """rain exponent (Lin et al. 1983) (m^-4) (Marshall and Palmer 1948)"""
    n0s_exp: float = 6.0
    """snow exponent (Lin et al. 1983) (m^-4) (Gunn and Marshall 1958)"""
    n0g_exp: float = 6.0
    """graupel exponent (Rutledge and Hobbs 1984) (m^-4) (Houze et al. 1979)"""
    n0h_exp: float = 4.0
    """hail exponent (Lin et al. 1983) (m^-4) (Federer and Waldvogel 1975)"""
    muw: float = 6.0
    """shape parameter of cloud water in Gamma distribution (Martin et al. 1994)"""
    mui: float = 3.35
    """Gamma shape parameter of cloud ice (McFarquhar et al. 2015)"""
    mur: float = 1.0
    """shape parameter of rain in Gamma distribution (Marshall and Palmer 1948)"""
    mus: float = 1.0
    """shape parameter of snow in Gamma distribution (Gunn and Marshall 1958)"""
    mug: float = 1.0
    """shape parameter of graupel in Gamma distribution (Houze et al. 1979)"""
    muh: float = 1.0
    """shape parameter of hail in Gamma distribution (Federer and Waldvogel 1975)"""
    cfflag: int = 1
    """
    cloud fraction scheme
     - 1: GFDL cloud scheme
     - 2: Xu and Randall (1996)
     - 3: Park et al. (2016)
     - 4: Gultepe and Isaac (2007)
    """
    irain_f: int = 0
    """
    cloud water to rain auto conversion scheme
     - 0: subgrid variability based scheme
     - 1: no subgrid varaibility
    """
    inflag: int = 1
    """
    Ice nucleation scheme:
     - 1: Hong et al. (2004)
     - 2: Meyers et al. (1992)
     - 3: Meyers et al. (1992)
     - 4: Cooper (1986)
     - 5: Fletcher (1962)
    """
    igflag: int = 3
    """
    Ice generation scheme
     - 1: WSM6
     - 2: WSM6 with 0 at 0 C
     - 3: WSM6 with 0 at 0 C and fixed value at - 10 C
     - 4: combination of 1 and 3
     """
    ifflag: int = 1
    """
    Ice fall scheme
     - 1: Deng and Mace (2008)
     - 2: Heymsfield and Donner (1990)
    """
    sedflag: int = 1
    """
    sedimentation scheme
     - 1: implicit scheme
     - 2: explicit scheme
     - 3: lagrangian scheme
     - 4: combined implicit and lagrangian scheme
    """
    vdiffflag: int = 1
    """
    wind difference scheme in accretion
     - 1: Wisner et al. (1972)
     - 2: Mizuno (1990)
     - 3: Murakami (1990)
    """
    c_air: float = dataclasses.field(init=False)
    c_vap: float = dataclasses.field(init=False)
    d0_vap: float = dataclasses.field(init=False)
    lv00: float = dataclasses.field(init=False)
    li00: float = dataclasses.field(init=False)
    li20: float = dataclasses.field(init=False)
    d1_vap: float = dataclasses.field(init=False)
    d1_ice: float = dataclasses.field(init=False)
    c1_vap: float = dataclasses.field(init=False)
    c1_liq: float = dataclasses.field(init=False)
    c1_ice: float = dataclasses.field(init=False)
    n_min: int = dataclasses.field(init=False)
    delt: float = dataclasses.field(init=False)
    esbasw: float = dataclasses.field(init=False)
    tbasw: float = dataclasses.field(init=False)
    esbasi: float = dataclasses.field(init=False)
    tmin: float = dataclasses.field(init=False)
    t_wfr: float = dataclasses.field(init=False)
    pcaw: float = dataclasses.field(init=False)
    pcbw: float = dataclasses.field(init=False)
    pcai: float = dataclasses.field(init=False)
    pcbi: float = dataclasses.field(init=False)
    pcar: float = dataclasses.field(init=False)
    pcbr: float = dataclasses.field(init=False)
    pcas: float = dataclasses.field(init=False)
    pcbs: float = dataclasses.field(init=False)
    pcag: float = dataclasses.field(init=False)
    pcbg: float = dataclasses.field(init=False)
    pcah: float = dataclasses.field(init=False)
    pcbh: float = dataclasses.field(init=False)
    edaw: float = dataclasses.field(init=False)
    edbw: float = dataclasses.field(init=False)
    edai: float = dataclasses.field(init=False)
    edbi: float = dataclasses.field(init=False)
    edar: float = dataclasses.field(init=False)
    edbr: float = dataclasses.field(init=False)
    edas: float = dataclasses.field(init=False)
    edbs: float = dataclasses.field(init=False)
    edag: float = dataclasses.field(init=False)
    edbg: float = dataclasses.field(init=False)
    edah: float = dataclasses.field(init=False)
    edbh: float = dataclasses.field(init=False)
    oeaw: float = dataclasses.field(init=False)
    oebw: float = dataclasses.field(init=False)
    oeai: float = dataclasses.field(init=False)
    oebi: float = dataclasses.field(init=False)
    oear: float = dataclasses.field(init=False)
    oebr: float = dataclasses.field(init=False)
    oeas: float = dataclasses.field(init=False)
    oebs: float = dataclasses.field(init=False)
    oeag: float = dataclasses.field(init=False)
    oebg: float = dataclasses.field(init=False)
    oeah: float = dataclasses.field(init=False)
    oebh: float = dataclasses.field(init=False)
    rraw: float = dataclasses.field(init=False)
    rrbw: float = dataclasses.field(init=False)
    rrai: float = dataclasses.field(init=False)
    rrbi: float = dataclasses.field(init=False)
    rrar: float = dataclasses.field(init=False)
    rrbr: float = dataclasses.field(init=False)
    rras: float = dataclasses.field(init=False)
    rrbs: float = dataclasses.field(init=False)
    rrag: float = dataclasses.field(init=False)
    rrbg: float = dataclasses.field(init=False)
    rrah: float = dataclasses.field(init=False)
    rrbh: float = dataclasses.field(init=False)
    tvai: float = dataclasses.field(init=False)
    tvbi: float = dataclasses.field(init=False)
    tvar: float = dataclasses.field(init=False)
    tvbr: float = dataclasses.field(init=False)
    tvas: float = dataclasses.field(init=False)
    tvbs: float = dataclasses.field(init=False)
    tvag: float = dataclasses.field(init=False)
    tvbg: float = dataclasses.field(init=False)
    tvah: float = dataclasses.field(init=False)
    tvbh: float = dataclasses.field(init=False)
    crevp_1: float = dataclasses.field(init=False)
    crevp_2: float = dataclasses.field(init=False)
    crevp_3: float = dataclasses.field(init=False)
    crevp_4: float = dataclasses.field(init=False)
    crevp_5: float = dataclasses.field(init=False)
    cssub_1: float = dataclasses.field(init=False)
    cssub_2: float = dataclasses.field(init=False)
    cssub_3: float = dataclasses.field(init=False)
    cssub_4: float = dataclasses.field(init=False)
    cssub_5: float = dataclasses.field(init=False)
    cgsub_1: float = dataclasses.field(init=False)
    cgsub_2: float = dataclasses.field(init=False)
    cgsub_3: float = dataclasses.field(init=False)
    cgsub_4: float = dataclasses.field(init=False)
    cgsub_5: float = dataclasses.field(init=False)
    csmlt_1: float = dataclasses.field(init=False)
    csmlt_2: float = dataclasses.field(init=False)
    csmlt_3: float = dataclasses.field(init=False)
    csmlt_4: float = dataclasses.field(init=False)
    cgmlt_1: float = dataclasses.field(init=False)
    cgmlt_2: float = dataclasses.field(init=False)
    cgmlt_3: float = dataclasses.field(init=False)
    cgmlt_4: float = dataclasses.field(init=False)
    cgfr_1: float = dataclasses.field(init=False)
    cgfr_2: float = dataclasses.field(init=False)
    normw: float = dataclasses.field(init=False)
    normr: float = dataclasses.field(init=False)
    normi: float = dataclasses.field(init=False)
    norms: float = dataclasses.field(init=False)
    normg: float = dataclasses.field(init=False)
    expow: float = dataclasses.field(init=False)
    expor: float = dataclasses.field(init=False)
    expoi: float = dataclasses.field(init=False)
    expos: float = dataclasses.field(init=False)
    expog: float = dataclasses.field(init=False)
    cracw: float = dataclasses.field(init=False)
    craci: float = dataclasses.field(init=False)
    csacw: float = dataclasses.field(init=False)
    csaci: float = dataclasses.field(init=False)
    cgacw: float = dataclasses.field(init=False)
    cgaci: float = dataclasses.field(init=False)
    cracs: float = dataclasses.field(init=False)
    csacr: float = dataclasses.field(init=False)
    cgacr: float = dataclasses.field(init=False)
    cgacs: float = dataclasses.field(init=False)
    acc: List[float] = dataclasses.field(init=False)
    acco: List[List[float]] = dataclasses.field(init=False)

    def __post_init__(self):
        if self.hydrostatic:
            self.c_air = constants.CP_AIR
            self.c_vap = constants.CP_VAP
            self.do_sedi_w = False
        else:
            self.c_air = constants.CV_AIR
            self.c_vap = constants.CV_VAP
        self.d0_vap = self.c_vap - mpcons.C_LIQ

        # scaled constants to reduce 32 bit floating errors
        self.lv00 = (constants.HLV - self.d0_vap * mpcons.TICE0) / self.c_air
        self.li00 = (constants.HLF - mpcons.DC_ICE * mpcons.TICE0) / self.c_air
        self.li20 = self.lv00 + self.li00

        self.d1_vap = self.d0_vap / self.c_air
        self.d1_ice = mpcons.DC_ICE / self.c_air

        self.c1_vap = self.c_vap / self.c_air
        self.c1_liq = mpcons.C_LIQ / self.c_air
        self.c1_ice = mpcons.C_ICE / self.c_air

        self._calculate_particle_parameters()

        self._calculate_slope_parameters()

        self._calculate_evaporation_and_sublimation_constants()

        self._calculate_accretion_parameters()

        self._calculate_melting_and_freezing_constants()

        self._set_timestepping()

        self.n_min = 1600
        self.delt = 0.1
        self.esbasw = 1013246.0
        self.tbasw = mpcons.TICE0 + 100.0
        self.esbasi = 6107.1
        self.tmin = mpcons.TICE0 - self.n_min * self.delt
        if self.do_warm_rain_mp:  # unsupported
            self.t_wfr = self.tmin
        else:
            self.t_wfr = mpcons.TICE0 - 40.0

    @property
    def adjustnegative(self) -> AdjustNegativeTracerConfig:
        return AdjustNegativeTracerConfig(
            ntimes=self.ntimes,
            c1_ice=self.c1_ice,
            c1_liq=self.c1_liq,
            c1_vap=self.c1_vap,
            d1_ice=self.d1_ice,
            d1_vap=self.d1_vap,
            li00=self.li00,
            li20=self.li20,
            lv00=self.lv00,
            t_wfr=self.t_wfr,
        )

    @property
    def fastmp(self) -> FastMPConfig:
        return FastMPConfig(
            do_warm_rain_mp=self.do_warm_rain_mp,
            do_wbf=self.do_wbf,
            c1_vap=self.c1_vap,
            c1_liq=self.c1_liq,
            c1_ice=self.c1_ice,
            lv00=self.lv00,
            li00=self.li00,
            li20=self.li20,
            d1_vap=self.d1_vap,
            d1_ice=self.d1_ice,
            t_wfr=self.t_wfr,
            ql_mlt=self.ql_mlt,
            qs_mlt=self.qs_mlt,
            tau_imlt=self.tau_imlt,
            tice_mlt=self.tice_mlt,
            do_cond_timescale=self.do_cond_timescale,
            do_evap_timescale=self.do_evap_timescale,
            do_hail=self.do_hail,
            rh_fac_evap=self.rh_fac_evap,
            rh_fac_cond=self.rh_fac_cond,
            rhc_cevap=self.rhc_cevap,
            tau_l2v=self.tau_l2v,
            tau_v2l=self.tau_v2l,
            tau_r2g=self.tau_r2g,
            tau_smlt=self.tau_smlt,
            tau_gmlt=self.tau_gmlt,
            tau_l2r=self.tau_l2r,
            use_rhc_cevap=self.use_rhc_cevap,
            qi0_crt=self.qi0_crt,
            qi0_max=self.qi0_max,
            ql0_max=self.ql0_max,
            tau_wbf=self.tau_wbf,
            do_psd_water_num=self.do_psd_water_num,
            do_psd_ice_num=self.do_psd_ice_num,
            muw=self.muw,
            mui=self.mui,
            mur=self.mur,
            mus=self.mus,
            mug=self.mug,
            muh=self.muh,
            pcaw=self.pcaw,
            pcbw=self.pcbw,
            pcai=self.pcai,
            pcbi=self.pcbi,
            prog_ccn=self.prog_ccn,
            inflag=self.inflag,
            igflag=self.igflag,
            qi_lim=self.qi_lim,
            t_sub=self.t_sub,
            is_fac=self.is_fac,
            tau_i2s=self.tau_i2s,
            fast_fr_mlt=self.fast_fr_mlt,
            fast_dep_sub=self.fast_dep_sub,
            delay_cond_evap=self.delay_cond_evap,
            nconds=self.nconds,
        )

    def _set_timestepping(self):
        """
        Set split timestepping info
        full_timestep is equivalent to dtm
        split_timestep is equivalent to dts
        """
        self.ntimes = int(
            max(self.ntimes, self.dt_full / min(self.dt_full, self.mp_time))
        )
        self.dt_split = self.dt_full / self.ntimes

    def _calculate_particle_parameters(self):
        """
        Calculate parameters for particle concentration, effective diameter,
        optical extinction, radar reflectivity, and terminal velocity
        for each tracer species
        """
        muw = self.muw
        mui = self.mui
        mur = self.mur
        mus = self.mus
        mug = self.mug
        muh = self.muh
        n0w_exp = self.n0w_exp
        n0i_exp = self.n0i_exp
        n0r_exp = self.n0r_exp
        n0s_exp = self.n0s_exp
        n0g_exp = self.n0g_exp
        n0h_exp = self.n0h_exp
        n0w_sig = self.n0w_sig
        n0i_sig = self.n0i_sig
        n0r_sig = self.n0r_sig
        n0s_sig = self.n0s_sig
        n0g_sig = self.n0g_sig
        n0h_sig = self.n0h_sig
        alinw = self.alinw
        alini = self.alini
        alinr = self.alinr
        alins = self.alins
        aling = self.aling
        alinh = self.alinh
        blinw = self.blinw
        blini = self.blini
        blinr = self.blinr
        blins = self.blins
        bling = self.bling
        blinh = self.blinh

        # TODO: Refactor these to be functions, holy cow

        # Particle Concentration:
        self.pcaw = (
            math.exp(3 / (muw + 3) * math.log(n0w_sig))
            * math.gamma(muw)
            * math.exp(3 * n0w_exp / (muw + 3) * math.log(10.0))
        )
        self.pcbw = math.exp(
            muw
            / (muw + 3)
            * math.log(constants.PI * mpcons.RHO_W * math.gamma(muw + 3))
        )
        self.pcai = (
            math.exp(3 / (mui + 3) * math.log(n0i_sig))
            * math.gamma(mui)
            * math.exp(3 * n0i_exp / (mui + 3) * math.log(10.0))
        )
        self.pcbi = math.exp(
            mui
            / (mui + 3)
            * math.log(constants.PI * mpcons.RHO_I * math.gamma(mui + 3))
        )

        self.pcar = (
            math.exp(3 / (mur + 3) * math.log(n0r_sig))
            * math.gamma(mur)
            * math.exp(3 * n0r_exp / (mur + 3) * math.log(10.0))
        )
        self.pcbr = math.exp(
            mur
            / (mur + 3)
            * math.log(constants.PI * mpcons.RHO_R * math.gamma(mur + 3))
        )

        self.pcas = (
            math.exp(3 / (mus + 3) * math.log(n0s_sig))
            * math.gamma(mus)
            * math.exp(3 * n0s_exp / (mus + 3) * math.log(10.0))
        )
        self.pcbs = math.exp(
            mus
            / (mus + 3)
            * math.log(constants.PI * mpcons.RHO_S * math.gamma(mus + 3))
        )

        self.pcag = (
            math.exp(3 / (mug + 3) * math.log(n0g_sig))
            * math.gamma(mug)
            * math.exp(3 * n0g_exp / (mug + 3) * math.log(10.0))
        )
        self.pcbg = math.exp(
            mug
            / (mug + 3)
            * math.log(constants.PI * mpcons.RHO_G * math.gamma(mug + 3))
        )

        self.pcah = (
            math.exp(3 / (muh + 3) * math.log(n0h_sig))
            * math.gamma(muh)
            * math.exp(3 * n0h_exp / (muh + 3) * math.log(10.0))
        )
        self.pcbh = math.exp(
            muh
            / (muh + 3)
            * math.log(constants.PI * mpcons.RHO_H * math.gamma(muh + 3))
        )

        # Effective Diameter
        self.edaw = (
            math.exp(-1.0 / (muw + 3) * math.log(n0w_sig))
            * (muw + 2)
            * math.exp(-n0w_exp / (muw + 3) * math.log(10.0))
        )
        self.edbw = math.exp(
            1.0
            / (muw + 3)
            * math.log(constants.PI * mpcons.RHO_W * math.gamma(muw + 3))
        )

        self.edai = (
            math.exp(-1.0 / (mui + 3) * math.log(n0i_sig))
            * (mui + 2)
            * math.exp(-n0i_exp / (mui + 3) * math.log(10.0))
        )
        self.edbi = math.exp(
            1.0
            / (mui + 3)
            * math.log(constants.PI * mpcons.RHO_I * math.gamma(mui + 3))
        )

        self.edar = (
            math.exp(-1.0 / (mur + 3) * math.log(n0r_sig))
            * (mur + 2)
            * math.exp(-n0r_exp / (mur + 3) * math.log(10.0))
        )
        self.edbr = math.exp(
            1.0
            / (mur + 3)
            * math.log(constants.PI * mpcons.RHO_R * math.gamma(mur + 3))
        )

        self.edas = (
            math.exp(-1.0 / (mus + 3) * math.log(n0s_sig))
            * (mus + 2)
            * math.exp(-n0s_exp / (mus + 3) * math.log(10.0))
        )
        self.edbs = math.exp(
            1.0
            / (mus + 3)
            * math.log(constants.PI * mpcons.RHO_S * math.gamma(mus + 3))
        )

        self.edag = (
            math.exp(-1.0 / (mug + 3) * math.log(n0g_sig))
            * (mug + 2)
            * math.exp(-n0g_exp / (mug + 3) * math.log(10.0))
        )
        self.edbg = math.exp(
            1.0
            / (mug + 3)
            * math.log(constants.PI * mpcons.RHO_G * math.gamma(mug + 3))
        )

        self.edah = (
            math.exp(-1.0 / (muh + 3) * math.log(n0h_sig))
            * (muh + 2)
            * math.exp(-n0h_exp / (muh + 3) * math.log(10.0))
        )
        self.edbh = math.exp(
            1.0
            / (muh + 3)
            * math.log(constants.PI * mpcons.RHO_H * math.gamma(muh + 3))
        )

        # Optical Extinction
        self.oeaw = (
            math.exp(1.0 / (muw + 3) * math.log(n0w_sig))
            * constants.PI
            * math.gamma(muw + 2)
            * math.exp(n0w_exp / (muw + 3) * math.log(10.0))
        )
        self.oebw = 2 * math.exp(
            (muw + 2)
            / (muw + 3)
            * math.log(constants.PI * mpcons.RHO_W * math.gamma(muw + 3))
        )

        self.oeai = (
            math.exp(1.0 / (mui + 3) * math.log(n0i_sig))
            * constants.PI
            * math.gamma(mui + 2)
            * math.exp(n0i_exp / (mui + 3) * math.log(10.0))
        )
        self.oebi = 2 * math.exp(
            (mui + 2)
            / (mui + 3)
            * math.log(constants.PI * mpcons.RHO_I * math.gamma(mui + 3))
        )

        self.oear = (
            math.exp(1.0 / (mur + 3) * math.log(n0r_sig))
            * constants.PI
            * math.gamma(mur + 2)
            * math.exp(n0r_exp / (mur + 3) * math.log(10.0))
        )
        self.oebr = 2 * math.exp(
            (mur + 2)
            / (mur + 3)
            * math.log(constants.PI * mpcons.RHO_R * math.gamma(mur + 3))
        )

        self.oeas = (
            math.exp(1.0 / (mus + 3) * math.log(n0s_sig))
            * constants.PI
            * math.gamma(mus + 2)
            * math.exp(n0s_exp / (mus + 3) * math.log(10.0))
        )
        self.oebs = 2 * math.exp(
            (mus + 2)
            / (mus + 3)
            * math.log(constants.PI * mpcons.RHO_S * math.gamma(mus + 3))
        )

        self.oeag = (
            math.exp(1.0 / (mug + 3) * math.log(n0g_sig))
            * constants.PI
            * math.gamma(mug + 2)
            * math.exp(n0g_exp / (mug + 3) * math.log(10.0))
        )
        self.oebg = 2 * math.exp(
            (mug + 2)
            / (mug + 3)
            * math.log(constants.PI * mpcons.RHO_G * math.gamma(mug + 3))
        )

        self.oeah = (
            math.exp(1.0 / (muh + 3) * math.log(n0h_sig))
            * constants.PI
            * math.gamma(muh + 2)
            * math.exp(n0h_exp / (muh + 3) * math.log(10.0))
        )
        self.oebh = 2 * math.exp(
            (muh + 2)
            / (muh + 3)
            * math.log(constants.PI * mpcons.RHO_H * math.gamma(muh + 3))
        )

        # Radar Reflectivity
        self.rraw = (
            math.exp(-3 / (muw + 3) * math.log(n0w_sig))
            * math.gamma(muw + 6)
            * math.exp(-3 * n0w_exp / (muw + 3) * math.log(10.0))
        )
        self.rrbw = math.exp(
            (muw + 6)
            / (muw + 3)
            * math.log(constants.PI * mpcons.RHO_W * math.gamma(muw + 3))
        )

        self.rrai = (
            math.exp(-3 / (mui + 3) * math.log(n0i_sig))
            * math.gamma(mui + 6)
            * math.exp(-3 * n0i_exp / (mui + 3) * math.log(10.0))
        )
        self.rrbi = math.exp(
            (mui + 6)
            / (mui + 3)
            * math.log(constants.PI * mpcons.RHO_I * math.gamma(mui + 3))
        )

        self.rrar = (
            math.exp(-3 / (mur + 3) * math.log(n0r_sig))
            * math.gamma(mur + 6)
            * math.exp(-3 * n0r_exp / (mur + 3) * math.log(10.0))
        )
        self.rrbr = math.exp(
            (mur + 6)
            / (mur + 3)
            * math.log(constants.PI * mpcons.RHO_R * math.gamma(mur + 3))
        )

        self.rras = (
            math.exp(-3 / (mus + 3) * math.log(n0s_sig))
            * math.gamma(mus + 6)
            * math.exp(-3 * n0s_exp / (mus + 3) * math.log(10.0))
        )
        self.rrbs = math.exp(
            (mus + 6)
            / (mus + 3)
            * math.log(constants.PI * mpcons.RHO_S * math.gamma(mus + 3))
        )

        self.rrag = (
            math.exp(-3 / (mug + 3) * math.log(n0g_sig))
            * math.gamma(mug + 6)
            * math.exp(-3 * n0g_exp / (mug + 3) * math.log(10.0))
        )
        self.rrbg = math.exp(
            (mug + 6)
            / (mug + 3)
            * math.log(constants.PI * mpcons.RHO_G * math.gamma(mug + 3))
        )

        self.rrah = (
            math.exp(-3 / (muh + 3) * math.log(n0h_sig))
            * math.gamma(muh + 6)
            * math.exp(-3 * n0h_exp / (muh + 3) * math.log(10.0))
        )
        self.rrbh = math.exp(
            (muh + 6)
            / (muh + 3)
            * math.log(constants.PI * mpcons.RHO_H * math.gamma(muh + 3))
        )

        # Terminal Velocity
        self.tvaw = (
            math.exp(-blinw / (muw + 3) * math.log(n0w_sig))
            * alinw
            * math.gamma(muw + blinw + 3)
            * math.exp(-blinw * n0w_exp / (muw + 3) * math.log(10.0))
        )
        self.tvbw = math.exp(
            blinw
            / (muw + 3)
            * math.log(constants.PI * mpcons.RHO_W * math.gamma(muw + 3))
        ) * math.gamma(muw + 3)

        self.tvai = (
            math.exp(-blini / (mui + 3) * math.log(n0i_sig))
            * alini
            * math.gamma(mui + blini + 3)
            * math.exp(-blini * n0i_exp / (mui + 3) * math.log(10.0))
        )
        self.tvbi = math.exp(
            blini
            / (mui + 3)
            * math.log(constants.PI * mpcons.RHO_I * math.gamma(mui + 3))
        ) * math.gamma(mui + 3)

        self.tvar = (
            math.exp(-blinr / (mur + 3) * math.log(n0r_sig))
            * alinr
            * math.gamma(mur + blinr + 3)
            * math.exp(-blinr * n0r_exp / (mur + 3) * math.log(10.0))
        )
        self.tvbr = math.exp(
            blinr
            / (mur + 3)
            * math.log(constants.PI * mpcons.RHO_R * math.gamma(mur + 3))
        ) * math.gamma(mur + 3)

        self.tvas = (
            math.exp(-blins / (mus + 3) * math.log(n0s_sig))
            * alins
            * math.gamma(mus + blins + 3)
            * math.exp(-blins * n0s_exp / (mus + 3) * math.log(10.0))
        )
        self.tvbs = math.exp(
            blins
            / (mus + 3)
            * math.log(constants.PI * mpcons.RHO_S * math.gamma(mus + 3))
        ) * math.gamma(mus + 3)

        self.tvag = (
            math.exp(-bling / (mug + 3) * math.log(n0g_sig))
            * aling
            * math.gamma(mug + bling + 3)
            * math.exp(-bling * n0g_exp / (mug + 3) * math.log(10.0))
        ) * mpcons.GCON
        self.tvbg = math.exp(
            bling
            / (mug + 3)
            * math.log(constants.PI * mpcons.RHO_G * math.gamma(mug + 3))
        ) * math.gamma(mug + 3)

        self.tvah = (
            math.exp(-blinh / (muh + 3) * math.log(n0h_sig))
            * alinh
            * math.gamma(muh + blinh + 3)
            * math.exp(-blinh * n0h_exp / (muh + 3) * math.log(10.0))
        ) * mpcons.HCON
        self.tvbh = math.exp(
            blinh
            / (muh + 3)
            * math.log(constants.PI * mpcons.RHO_H * math.gamma(muh + 3))
        ) * math.gamma(muh + 3)

    def _calculate_slope_parameters(self):
        """
        Calculates slope parameters used for other variables
        """
        self.normw = (
            constants.PI * mpcons.RHO_W * self.n0w_sig * math.gamma(self.muw + 3)
        )
        self.normi = (
            constants.PI * mpcons.RHO_I * self.n0i_sig * math.gamma(self.mui + 3)
        )
        self.normr = (
            constants.PI * mpcons.RHO_R * self.n0r_sig * math.gamma(self.mur + 3)
        )
        self.norms = (
            constants.PI * mpcons.RHO_S * self.n0s_sig * math.gamma(self.mus + 3)
        )
        self.normg = (
            constants.PI * mpcons.RHO_G * self.n0g_sig * math.gamma(self.mug + 3)
        )
        self.normh = (
            constants.PI * mpcons.RHO_H * self.n0h_sig * math.gamma(self.muh + 3)
        )

        self.expow = math.exp(self.n0w_exp / (self.muw + 3) * math.log(10.0))
        self.expoi = math.exp(self.n0i_exp / (self.mui + 3) * math.log(10.0))
        self.expor = math.exp(self.n0r_exp / (self.mur + 3) * math.log(10.0))
        self.expos = math.exp(self.n0s_exp / (self.mus + 3) * math.log(10.0))
        self.expog = math.exp(self.n0g_exp / (self.mug + 3) * math.log(10.0))
        self.expoh = math.exp(self.n0h_exp / (self.muh + 3) * math.log(10.0))

    def _calculate_evaporation_and_sublimation_constants(self):
        """
        calculates crevp, cssub, and cgsub constants for rain evaporation,
        snow sublimation, and graupel or hail sublimation, Lin et al. (1983)
        """

        # TODO: These should also be made functions

        self.crevp_1 = (
            2.0
            * constants.PI
            * mpcons.VDIFU
            * mpcons.TCOND
            * constants.RVGAS
            * self.n0r_sig
            * math.gamma(1 + self.mur)
            / math.exp((1 + self.mur) / (self.mur + 3) * math.log(self.normr))
            * math.exp(2.0 * math.log(self.expor))
        )
        self.crevp_2 = 0.78
        self.crevp_3 = (
            0.31
            * mpcons.SCM3
            * math.sqrt(self.alinr / mpcons.VISK)
            * math.gamma((3 + 2 * self.mur + self.blinr) / 2)
            / math.exp(
                (3 + 2 * self.mur + self.blinr)
                / (self.mur + 3)
                / 2
                * math.log(self.normr)
            )
            * math.exp((1 + self.mur) / (self.mur + 3) * math.log(self.normr))
            / math.gamma(1 + self.mur)
            * math.exp((-1 - self.blinr) / 2.0 * math.log(self.expor))
        )
        self.crevp_4 = mpcons.TCOND * constants.RVGAS
        self.crevp_5 = mpcons.VDIFU

        self.cssub_1 = (
            2.0
            * constants.PI
            * mpcons.VDIFU
            * mpcons.TCOND
            * constants.RVGAS
            * self.n0s_sig
            * math.gamma(1 + self.mus)
            / math.exp((1 + self.mus) / (self.mus + 3) * math.log(self.norms))
            * math.exp(2.0 * math.log(self.expos))
        )
        self.cssub_2 = 0.78
        self.cssub_3 = (
            0.31
            * mpcons.SCM3
            * math.sqrt(self.alins / mpcons.VISK)
            * math.gamma((3 + 2 * self.mus + self.blins) / 2)
            / math.exp(
                (3 + 2 * self.mus + self.blins)
                / (self.mus + 3)
                / 2
                * math.log(self.norms)
            )
            * math.exp((1 + self.mus) / (self.mus + 3) * math.log(self.norms))
            / math.gamma(1 + self.mus)
            * math.exp((-1 - self.blins) / 2.0 * math.log(self.expos))
        )
        self.cssub_4 = mpcons.TCOND * constants.RVGAS
        self.cssub_5 = mpcons.VDIFU

        if self.do_hail:
            self.cgsub_1 = (
                2.0
                * constants.PI
                * mpcons.VDIFU
                * mpcons.TCOND
                * constants.RVGAS
                * self.n0h_sig
                * math.gamma(1 + self.muh)
                / math.exp((1 + self.muh) / (self.muh + 3) * math.log(self.normh))
                * math.exp(2.0 * math.log(self.expoh))
            )
            self.cgsub_2 = 0.78
            self.cgsub_3 = (
                0.31
                * mpcons.SCM3
                * math.sqrt(self.alinh * mpcons.HCON / mpcons.VISK)
                * math.gamma((3 + 2 * self.muh + self.blinh) / 2)
                / math.exp(
                    1.0
                    / (self.muh + 3)
                    * (3 + 2 * self.muh + self.blinh)
                    / 2
                    * math.log(self.normh)
                )
                * math.exp(1.0 / (self.muh + 3) * (1 + self.muh) * math.log(self.normh))
                / math.gamma(1 + self.muh)
                * math.exp((-1 - self.blinh) / 2.0 * math.log(self.expoh))
            )
        else:
            self.cgsub_1 = (
                2.0
                * constants.PI
                * mpcons.VDIFU
                * mpcons.TCOND
                * constants.RVGAS
                * self.n0g_sig
                * math.gamma(1 + self.mug)
                / math.exp((1 + self.mug) / (self.mug + 3) * math.log(self.normg))
                * math.exp(2.0 * math.log(self.expog))
            )
            self.cgsub_2 = 0.78
            self.cgsub_3 = (
                0.31
                * mpcons.SCM3
                * math.sqrt(self.aling * mpcons.GCON / mpcons.VISK)
                * math.gamma((3 + 2 * self.mug + self.bling) / 2)
                / math.exp(
                    (3 + 2 * self.mug + self.bling)
                    / (self.mug + 3)
                    / 2
                    * math.log(self.normg)
                )
                * math.exp((1 + self.mug) / (self.mug + 3) * math.log(self.normg))
                / math.gamma(1 + self.mug)
                * math.exp((-1 - self.bling) / 2.0 * math.log(self.expog))
            )
        self.cgsub_4 = mpcons.TCOND * constants.RVGAS
        self.cgsub_5 = mpcons.VDIFU

    def _calculate_accretion_parameters(self):
        """
        Accretion between cloud water, cloud ice, rain,snow, and graupel or hail,
        Lin et al. (1983)
        """
        self.cracw = (
            constants.PI
            * self.n0r_sig
            * self.alinr
            * math.gamma(2 + self.mur + self.blinr)
            / (
                4.0
                * math.exp(
                    (2 + self.mur + self.blinr) / (self.mur + 3) * math.log(self.normr)
                )
            )
            * math.exp((1 - self.blinr) * math.log(self.expor))
        )
        self.craci = self.cracw
        self.csacw = (
            constants.PI
            * self.n0s_sig
            * self.alins
            * math.gamma(2 + self.mus + self.blins)
            / (
                4.0
                * math.exp(
                    (2 + self.mus + self.blins) / (self.mus + 3) * math.log(self.norms)
                )
            )
            * math.exp((1 - self.blins) * math.log(self.expos))
        )
        self.csaci = self.csacw
        if self.do_hail:
            self.cgacw = (
                constants.PI
                * self.n0h_sig
                * self.alinh
                * math.gamma(2 + self.muh + self.blinh)
                * mpcons.HCON
                / (
                    4.0
                    * math.exp(
                        (2 + self.muh + self.blinh)
                        / (self.muh + 3)
                        * math.log(self.normh)
                    )
                )
                * math.exp((1 - self.blinh) * math.log(self.expoh))
            )
            self.cgaci = self.cgacw
        else:
            self.cgacw = (
                constants.PI
                * self.n0g_sig
                * self.aling
                * math.gamma(2 + self.mug + self.bling)
                * mpcons.GCON
                / (
                    4.0
                    * math.exp(
                        (2 + self.mug + self.bling)
                        / (self.mug + 3)
                        * math.log(self.normg)
                    )
                )
                * math.exp((1 - self.bling) * math.log(self.expog))
            )
            self.cgaci = self.cgacw

        if self.do_new_acc_water:
            self.cracw = (
                constants.PI**2 * self.n0r_sig * self.n0w_sig * mpcons.RHO_W / 24.0
            )
            self.csacw = (
                constants.PI**2 * self.n0s_sig * self.n0w_sig * mpcons.RHO_W / 24.0
            )
            if self.do_hail:
                self.cgacw = (
                    constants.PI**2 * self.n0h_sig * self.n0w_sig * mpcons.RHO_W / 24.0
                )
            else:
                self.cgacw = (
                    constants.PI**2 * self.n0g_sig * self.n0w_sig * mpcons.RHO_W / 24.0
                )

        if self.do_new_acc_ice:
            self.craci = (
                constants.PI**2 * self.n0r_sig * self.n0i_sig * mpcons.RHO_I / 24.0
            )
            self.csaci = (
                constants.PI**2 * self.n0s_sig * self.n0i_sig * mpcons.RHO_I / 24.0
            )
            if self.do_hail:
                self.cgaci = (
                    constants.PI**2 * self.n0h_sig * self.n0i_sig * mpcons.RHO_I / 24.0
                )
            else:
                self.cgaci = (
                    constants.PI**2 * self.n0g_sig * self.n0i_sig * mpcons.RHO_I / 24.0
                )
        else:
            pass

        self.cracw = self.cracw * self.c_pracw
        self.craci = self.craci * self.c_praci
        self.csacw = self.csacw * self.c_psacw
        self.csaci = self.csaci * self.c_psaci
        self.cgacw = self.cgacw * self.c_pgacw
        self.cgaci = self.cgaci * self.c_pgaci

        self.cracs = constants.PI**2 * self.n0r_sig * self.n0s_sig * mpcons.RHO_S / 24.0
        self.csacr = constants.PI**2 * self.n0s_sig * self.n0r_sig * mpcons.RHO_R / 24.0
        if self.do_hail:
            self.cgacs = (
                constants.PI**2 * self.n0h_sig * self.n0s_sig * mpcons.RHO_S / 24.0
            )
            self.cgacr = (
                constants.PI**2 * self.n0h_sig * self.n0r_sig * mpcons.RHO_R / 24.0
            )
        else:
            self.cgacs = (
                constants.PI**2 * self.n0g_sig * self.n0s_sig * mpcons.RHO_S / 24.0
            )
            self.cgacr = (
                constants.PI**2 * self.n0g_sig * self.n0r_sig * mpcons.RHO_R / 24.0
            )

        self.cracs *= self.c_pracs
        self.csacr *= self.c_psacr
        self.cgacs *= self.c_pgacs
        self.cgacr *= self.c_pgacr

        """
        act / ace / acc:
         0 -  1: racs (s - r)
         2 -  3: sacr (r - s)
         4 -  5: gacr (r - g)
         6 -  7: gacs (s - g)
         8 -  9: racw (w - r)
        10 - 11: raci (i - r)
        12 - 13: sacw (w - s)
        14 - 15: saci (i - s)
        16 - 17: gacw (w - g)
        18 - 19: gaci (i - g)
        """
        act = []
        act.append(self.norms)
        act.append(self.normr)
        act.append(act[1])
        act.append(act[0])
        act.append(act[1])
        if self.do_hail:
            act.append(self.normh)
        else:
            act.append(self.normg)
        act.append(act[0])
        act.append(act[5])
        act.append(self.normw)
        act.append(act[1])
        act.append(self.normi)
        act.append(act[1])
        act.append(act[8])
        act.append(act[0])
        act.append(act[10])
        act.append(act[0])
        act.append(act[8])
        act.append(act[5])
        act.append(act[10])
        act.append(act[5])

        ace = []
        ace.append(self.expos)
        ace.append(self.expor)
        ace.append(ace[1])
        ace.append(ace[0])
        ace.append(ace[1])
        if self.do_hail:
            ace.append(self.expoh)
        else:
            ace.append(self.expog)
        ace.append(ace[0])
        ace.append(ace[5])
        ace.append(self.expow)
        ace.append(ace[1])
        ace.append(self.expoi)
        ace.append(ace[1])
        ace.append(ace[8])
        ace.append(ace[0])
        ace.append(ace[10])
        ace.append(ace[0])
        ace.append(ace[8])
        ace.append(ace[5])
        ace.append(ace[10])
        ace.append(ace[5])

        acc = []
        acc.append(self.mus)
        acc.append(self.mur)
        acc.append(acc[1])
        acc.append(acc[0])
        acc.append(acc[1])
        if self.do_hail:
            acc.append(self.muh)
        else:
            acc.append(self.mug)
        acc.append(acc[0])
        acc.append(acc[5])
        acc.append(self.muw)
        acc.append(acc[1])
        acc.append(self.mui)
        acc.append(acc[1])
        acc.append(acc[8])
        acc.append(acc[0])
        acc.append(acc[10])
        acc.append(acc[0])
        acc.append(acc[8])
        acc.append(acc[5])
        acc.append(acc[10])
        acc.append(acc[5])

        self.acc = acc

        acco = []
        occ = [1.0, 2.0, 1.0]
        for i in range(3):
            accoi = []
            for k in range(10):
                accoi.append(
                    occ[i]
                    * math.gamma(6 + acc[2 * k] - (i + 1))
                    * math.gamma(acc[2 * k + 1] + i)
                    / (
                        math.exp(
                            (6 + acc[2 * k] - (i + 1))
                            / (acc[2 * k] + 3)
                            * math.log(act[2 * k])
                        )
                        * math.exp(
                            (acc[2 * k + 1] + i)
                            / (acc[2 * k + 1] + 3)
                            * math.log(act[2 * k + 1])
                        )
                    )
                    * math.exp((i - 2) * math.log(ace[2 * k]))
                    * math.exp((4 - (i + 1)) * math.log(ace[2 * k + 1]))
                )
            acco.append(accoi)

        self.acco = acco

    def _calculate_melting_and_freezing_constants(self):
        """
        Calculates parameters for snow and graupel melting,
        and rain freezing
        """

        # Snow melting, Lin et al. (1983)
        self.csmlt_1 = (
            2.0
            * constants.PI
            * mpcons.TCOND
            * self.n0s_sig
            * math.gamma(1 + self.mus)
            / math.exp((1 + self.mus) / (self.mus + 3) * math.log(self.norms))
            * math.exp(2.0 * math.log(self.expos))
        )
        self.csmlt_2 = (
            2.0
            * constants.PI
            * mpcons.VDIFU
            * self.n0s_sig
            * math.gamma(1 + self.mus)
            / math.exp((1 + self.mus) / (self.mus + 3) * math.log(self.norms))
            * math.exp(2.0 * math.log(self.expos))
        )
        self.csmlt_3 = self.cssub_2
        self.csmlt_4 = self.cssub_3

        # Graupel or hail melting, Lin et al. (1983)
        if self.do_hail:
            self.cgmlt_1 = (
                2.0
                * constants.PI
                * mpcons.TCOND
                * self.n0h_sig
                * math.gamma(1 + self.muh)
                / math.exp((1 + self.muh) / (self.muh + 3) * math.log(self.normh))
                * math.exp(2.0 * math.log(self.expoh))
            )
            self.cgmlt_2 = (
                2.0
                * constants.PI
                * mpcons.VDIFU
                * self.n0h_sig
                * math.gamma(1 + self.muh)
                / math.exp((1 + self.muh) / (self.muh + 3) * math.log(self.normh))
                * math.exp(2.0 * math.log(self.expoh))
            )
        else:
            self.cgmlt_1 = (
                2.0
                * constants.PI
                * mpcons.TCOND
                * self.n0g_sig
                * math.gamma(1 + self.mug)
                / math.exp((1 + self.mug) / (self.mug + 3) * math.log(self.normg))
                * math.exp(2.0 * math.log(self.expog))
            )
            self.cgmlt_2 = (
                2.0
                * constants.PI
                * mpcons.VDIFU
                * self.n0g_sig
                * math.gamma(1 + self.mug)
                / math.exp((1 + self.mug) / (self.mug + 3) * math.log(self.normg))
                * math.exp(2.0 * math.log(self.expog))
            )
        self.cgmlt_3 = self.cgsub_2
        self.cgmlt_4 = self.cgsub_3

        # Rain freezing, Lin et al. (1983)
        self.cgfr_1 = (
            1.0e2
            / 36
            * constants.PI**2
            * self.n0r_sig
            * mpcons.RHO_R
            * math.gamma(6 + self.mur)
            / math.exp((6 + self.mur) / (self.mur + 3) * math.log(self.normr))
            * math.exp(-3.0 * math.log(self.expor))
        )
        self.cgfr_2 = 0.66
