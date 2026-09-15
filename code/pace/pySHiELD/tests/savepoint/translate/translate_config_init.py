from ndsl import StencilFactory
from pyshield.stencils.gfdl_cld_microphysics import GFDLCloudMPConfig
from tests.savepoint.translate.translate_physics import TranslatePhysicsFortranData2Py


class PassVars:
    def __init__(self, config):
        self.config = config
        self.l1 = len(config.acc)
        self.l2 = len(config.acco[1])

    def __call__(
        self,
        c_air,
        c_vap,
        d0_vap,
        lv00,
        li00,
        li20,
        d1_vap,
        d1_ice,
        c1_vap,
        c1_liq,
        c1_ice,
        t_wfr,
        pcaw,
        pcbw,
        pcai,
        pcbi,
        pcar,
        pcbr,
        pcas,
        pcbs,
        pcag,
        pcbg,
        pcah,
        pcbh,
        edaw,
        edbw,
        edai,
        edbi,
        edar,
        edbr,
        edas,
        edbs,
        edag,
        edbg,
        edah,
        edbh,
        oeaw,
        oebw,
        oeai,
        oebi,
        oear,
        oebr,
        oeas,
        oebs,
        oeag,
        oebg,
        oeah,
        oebh,
        rraw,
        rrbw,
        rrai,
        rrbi,
        rrar,
        rrbr,
        rras,
        rrbs,
        rrag,
        rrbg,
        rrah,
        rrbh,
        tvai,
        tvbi,
        tvar,
        tvbr,
        tvas,
        tvbs,
        tvag,
        tvbg,
        tvah,
        tvbh,
        crevp_1,
        crevp_2,
        crevp_3,
        crevp_4,
        crevp_5,
        cssub_1,
        cssub_2,
        cssub_3,
        cssub_4,
        cssub_5,
        cgsub_1,
        cgsub_2,
        cgsub_3,
        cgsub_4,
        cgsub_5,
        csmlt_1,
        csmlt_2,
        csmlt_3,
        csmlt_4,
        cgmlt_1,
        cgmlt_2,
        cgmlt_3,
        cgmlt_4,
        cgfr_1,
        cgfr_2,
        normw,
        normr,
        normi,
        norms,
        normg,
        expow,
        expor,
        expoi,
        expos,
        expog,
        cracw,
        craci,
        csacw,
        csaci,
        cgacw,
        cgaci,
        cracs,
        csacr,
        cgacr,
        cgacs,
        acc,
        acco1,
        acco2,
        acco3,
    ):
        c_air[:] = self.config.c_air
        c_vap[:] = self.config.c_vap
        d0_vap[:] = self.config.d0_vap
        lv00[:] = self.config.lv00
        li00[:] = self.config.li00
        li20[:] = self.config.li20
        d1_vap[:] = self.config.d1_vap
        d1_ice[:] = self.config.d1_ice
        c1_vap[:] = self.config.c1_vap
        c1_liq[:] = self.config.c1_liq
        c1_ice[:] = self.config.c1_ice
        t_wfr[:] = self.config.t_wfr
        pcaw[:] = self.config.pcaw
        pcbw[:] = self.config.pcbw
        pcai[:] = self.config.pcai
        pcbi[:] = self.config.pcbi
        pcar[:] = self.config.pcar
        pcbr[:] = self.config.pcbr
        pcas[:] = self.config.pcas
        pcbs[:] = self.config.pcbs
        pcag[:] = self.config.pcag
        pcbg[:] = self.config.pcbg
        pcah[:] = self.config.pcah
        pcbh[:] = self.config.pcbh
        edaw[:] = self.config.edaw
        edbw[:] = self.config.edbw
        edai[:] = self.config.edai
        edbi[:] = self.config.edbi
        edar[:] = self.config.edar
        edbr[:] = self.config.edbr
        edas[:] = self.config.edas
        edbs[:] = self.config.edbs
        edag[:] = self.config.edag
        edbg[:] = self.config.edbg
        edah[:] = self.config.edah
        edbh[:] = self.config.edbh
        oeaw[:] = self.config.oeaw
        oebw[:] = self.config.oebw
        oeai[:] = self.config.oeai
        oebi[:] = self.config.oebi
        oear[:] = self.config.oear
        oebr[:] = self.config.oebr
        oeas[:] = self.config.oeas
        oebs[:] = self.config.oebs
        oeag[:] = self.config.oeag
        oebg[:] = self.config.oebg
        oeah[:] = self.config.oeah
        oebh[:] = self.config.oebh
        rraw[:] = self.config.rraw
        rrbw[:] = self.config.rrbw
        rrai[:] = self.config.rrai
        rrbi[:] = self.config.rrbi
        rrar[:] = self.config.rrar
        rrbr[:] = self.config.rrbr
        rras[:] = self.config.rras
        rrbs[:] = self.config.rrbs
        rrag[:] = self.config.rrag
        rrbg[:] = self.config.rrbg
        rrah[:] = self.config.rrah
        rrbh[:] = self.config.rrbh
        tvai[:] = self.config.tvai
        tvbi[:] = self.config.tvbi
        tvar[:] = self.config.tvar
        tvbr[:] = self.config.tvbr
        tvas[:] = self.config.tvas
        tvbs[:] = self.config.tvbs
        tvag[:] = self.config.tvag
        tvbg[:] = self.config.tvbg
        tvah[:] = self.config.tvah
        tvbh[:] = self.config.tvbh
        crevp_1[:] = self.config.crevp_1
        crevp_2[:] = self.config.crevp_2
        crevp_3[:] = self.config.crevp_3
        crevp_4[:] = self.config.crevp_4
        crevp_5[:] = self.config.crevp_5
        cssub_1[:] = self.config.cssub_1
        cssub_2[:] = self.config.cssub_2
        cssub_3[:] = self.config.cssub_3
        cssub_4[:] = self.config.cssub_4
        cssub_5[:] = self.config.cssub_5
        cgsub_1[:] = self.config.cgsub_1
        cgsub_2[:] = self.config.cgsub_2
        cgsub_3[:] = self.config.cgsub_3
        cgsub_4[:] = self.config.cgsub_4
        cgsub_5[:] = self.config.cgsub_5
        csmlt_1[:] = self.config.csmlt_1
        csmlt_2[:] = self.config.csmlt_2
        csmlt_3[:] = self.config.csmlt_3
        csmlt_4[:] = self.config.csmlt_4
        cgmlt_1[:] = self.config.cgmlt_1
        cgmlt_2[:] = self.config.cgmlt_2
        cgmlt_3[:] = self.config.cgmlt_3
        cgmlt_4[:] = self.config.cgmlt_4
        cgfr_1[:] = self.config.cgfr_1
        cgfr_2[:] = self.config.cgfr_2
        normw[:] = self.config.normw
        normr[:] = self.config.normr
        normi[:] = self.config.normi
        norms[:] = self.config.norms
        normg[:] = self.config.normg
        expow[:] = self.config.expow
        expor[:] = self.config.expor
        expoi[:] = self.config.expoi
        expos[:] = self.config.expos
        expog[:] = self.config.expog
        cracw[:] = self.config.cracw
        craci[:] = self.config.craci
        csacw[:] = self.config.csacw
        csaci[:] = self.config.csaci
        cgacw[:] = self.config.cgacw
        cgaci[:] = self.config.cgaci
        cracs[:] = self.config.cracs
        csacr[:] = self.config.csacr
        cgacr[:] = self.config.cgacr
        cgacs[:] = self.config.cgacs
        acc[:, :, : self.l1] = self.config.acc
        acco1[:, :, : self.l2] = self.config.acco[0]
        acco2[:, :, : self.l2] = self.config.acco[1]
        acco3[:, :, : self.l2] = self.config.acco[2]


class TranslateConfigInit(TranslatePhysicsFortranData2Py):
    def __init__(
        self,
        grid,
        config,
        stencil_factory: StencilFactory,
    ):
        super().__init__(grid, config, stencil_factory)

        self.in_vars["data_vars"] = {
            "c_air": {"serialname": "ini_c_air", "shield": True},
            "c_vap": {"serialname": "ini_c_vap", "shield": True},
            "d0_vap": {"serialname": "ini_d0_vap", "shield": True},
            "lv00": {"serialname": "ini_lv00", "shield": True},
            "li00": {"serialname": "ini_li00", "shield": True},
            "li20": {"serialname": "ini_li20", "shield": True},
            "d1_vap": {"serialname": "ini_d1_vap", "shield": True},
            "d1_ice": {"serialname": "ini_d1_ice", "shield": True},
            "c1_vap": {"serialname": "ini_c1_vap", "shield": True},
            "c1_liq": {"serialname": "ini_c1_liq", "shield": True},
            "c1_ice": {"serialname": "ini_c1_ice", "shield": True},
            "t_wfr": {"serialname": "ini_t_wfr", "shield": True},
            "pcaw": {"serialname": "ini_pcaw", "shield": True},
            "pcbw": {"serialname": "ini_pcbw", "shield": True},
            "pcai": {"serialname": "ini_pcai", "shield": True},
            "pcbi": {"serialname": "ini_pcbi", "shield": True},
            "pcar": {"serialname": "ini_pcar", "shield": True},
            "pcbr": {"serialname": "ini_pcbr", "shield": True},
            "pcas": {"serialname": "ini_pcas", "shield": True},
            "pcbs": {"serialname": "ini_pcbs", "shield": True},
            "pcag": {"serialname": "ini_pcag", "shield": True},
            "pcbg": {"serialname": "ini_pcbg", "shield": True},
            "pcah": {"serialname": "ini_pcah", "shield": True},
            "pcbh": {"serialname": "ini_pcbh", "shield": True},
            "edaw": {"serialname": "ini_edaw", "shield": True},
            "edbw": {"serialname": "ini_edbw", "shield": True},
            "edai": {"serialname": "ini_edai", "shield": True},
            "edbi": {"serialname": "ini_edbi", "shield": True},
            "edar": {"serialname": "ini_edar", "shield": True},
            "edbr": {"serialname": "ini_edbr", "shield": True},
            "edas": {"serialname": "ini_edas", "shield": True},
            "edbs": {"serialname": "ini_edbs", "shield": True},
            "edag": {"serialname": "ini_edag", "shield": True},
            "edbg": {"serialname": "ini_edbg", "shield": True},
            "edah": {"serialname": "ini_edah", "shield": True},
            "edbh": {"serialname": "ini_edbh", "shield": True},
            "oeaw": {"serialname": "ini_oeaw", "shield": True},
            "oebw": {"serialname": "ini_oebw", "shield": True},
            "oeai": {"serialname": "ini_oeai", "shield": True},
            "oebi": {"serialname": "ini_oebi", "shield": True},
            "oear": {"serialname": "ini_oear", "shield": True},
            "oebr": {"serialname": "ini_oebr", "shield": True},
            "oeas": {"serialname": "ini_oeas", "shield": True},
            "oebs": {"serialname": "ini_oebs", "shield": True},
            "oeag": {"serialname": "ini_oeag", "shield": True},
            "oebg": {"serialname": "ini_oebg", "shield": True},
            "oeah": {"serialname": "ini_oeah", "shield": True},
            "oebh": {"serialname": "ini_oebh", "shield": True},
            "rraw": {"serialname": "ini_rraw", "shield": True},
            "rrbw": {"serialname": "ini_rrbw", "shield": True},
            "rrai": {"serialname": "ini_rrai", "shield": True},
            "rrbi": {"serialname": "ini_rrbi", "shield": True},
            "rrar": {"serialname": "ini_rrar", "shield": True},
            "rrbr": {"serialname": "ini_rrbr", "shield": True},
            "rras": {"serialname": "ini_rras", "shield": True},
            "rrbs": {"serialname": "ini_rrbs", "shield": True},
            "rrag": {"serialname": "ini_rrag", "shield": True},
            "rrbg": {"serialname": "ini_rrbg", "shield": True},
            "rrah": {"serialname": "ini_rrah", "shield": True},
            "rrbh": {"serialname": "ini_rrbh", "shield": True},
            "tvai": {"serialname": "ini_tvai", "shield": True},
            "tvbi": {"serialname": "ini_tvbi", "shield": True},
            "tvar": {"serialname": "ini_tvar", "shield": True},
            "tvbr": {"serialname": "ini_tvbr", "shield": True},
            "tvas": {"serialname": "ini_tvas", "shield": True},
            "tvbs": {"serialname": "ini_tvbs", "shield": True},
            "tvag": {"serialname": "ini_tvag", "shield": True},
            "tvbg": {"serialname": "ini_tvbg", "shield": True},
            "tvah": {"serialname": "ini_tvah", "shield": True},
            "tvbh": {"serialname": "ini_tvbh", "shield": True},
            "crevp_1": {"serialname": "ini_crevp_1", "shield": True},
            "crevp_2": {"serialname": "ini_crevp_2", "shield": True},
            "crevp_3": {"serialname": "ini_crevp_3", "shield": True},
            "crevp_4": {"serialname": "ini_crevp_4", "shield": True},
            "crevp_5": {"serialname": "ini_crevp_5", "shield": True},
            "cssub_1": {"serialname": "ini_cssub_1", "shield": True},
            "cssub_2": {"serialname": "ini_cssub_2", "shield": True},
            "cssub_3": {"serialname": "ini_cssub_3", "shield": True},
            "cssub_4": {"serialname": "ini_cssub_4", "shield": True},
            "cssub_5": {"serialname": "ini_cssub_5", "shield": True},
            "cgsub_1": {"serialname": "ini_cgsub_1", "shield": True},
            "cgsub_2": {"serialname": "ini_cgsub_2", "shield": True},
            "cgsub_3": {"serialname": "ini_cgsub_3", "shield": True},
            "cgsub_4": {"serialname": "ini_cgsub_4", "shield": True},
            "cgsub_5": {"serialname": "ini_cgsub_5", "shield": True},
            "csmlt_1": {"serialname": "ini_csmlt_1", "shield": True},
            "csmlt_2": {"serialname": "ini_csmlt_2", "shield": True},
            "csmlt_3": {"serialname": "ini_csmlt_3", "shield": True},
            "csmlt_4": {"serialname": "ini_csmlt_4", "shield": True},
            "cgmlt_1": {"serialname": "ini_cgmlt_1", "shield": True},
            "cgmlt_2": {"serialname": "ini_cgmlt_2", "shield": True},
            "cgmlt_3": {"serialname": "ini_cgmlt_3", "shield": True},
            "cgmlt_4": {"serialname": "ini_cgmlt_4", "shield": True},
            "cgfr_1": {"serialname": "ini_cgfr_1", "shield": True},
            "cgfr_2": {"serialname": "ini_cgfr_2", "shield": True},
            "normw": {"serialname": "ini_normw", "shield": True},
            "normr": {"serialname": "ini_normr", "shield": True},
            "normi": {"serialname": "ini_normi", "shield": True},
            "norms": {"serialname": "ini_norms", "shield": True},
            "normg": {"serialname": "ini_normg", "shield": True},
            "expow": {"serialname": "ini_expow", "shield": True},
            "expor": {"serialname": "ini_expor", "shield": True},
            "expoi": {"serialname": "ini_expoi", "shield": True},
            "expos": {"serialname": "ini_expos", "shield": True},
            "expog": {"serialname": "ini_expog", "shield": True},
            "cracw": {"serialname": "ini_cracw", "shield": True},
            "craci": {"serialname": "ini_craci", "shield": True},
            "csacw": {"serialname": "ini_csacw", "shield": True},
            "csaci": {"serialname": "ini_csaci", "shield": True},
            "cgacw": {"serialname": "ini_cgacw", "shield": True},
            "cgaci": {"serialname": "ini_cgaci", "shield": True},
            "cracs": {"serialname": "ini_cracs", "shield": True},
            "csacr": {"serialname": "ini_csacr", "shield": True},
            "cgacr": {"serialname": "ini_cgacr", "shield": True},
            "cgacs": {"serialname": "ini_cgacs", "shield": True},
            "acc": {"serialname": "ini_acc", "shield": True},
            "acco1": {"serialname": "ini_acco1", "shield": True},
            "acco2": {"serialname": "ini_acco2", "shield": True},
            "acco3": {"serialname": "ini_acco3", "shield": True},
        }

        self.out_vars = {
            "c_air": {"serialname": "ini_c_air", "shield": True},
            "c_vap": {"serialname": "ini_c_vap", "shield": True},
            "d0_vap": {"serialname": "ini_d0_vap", "shield": True},
            "lv00": {"serialname": "ini_lv00", "shield": True},
            "li00": {"serialname": "ini_li00", "shield": True},
            "li20": {"serialname": "ini_li20", "shield": True},
            "d1_vap": {"serialname": "ini_d1_vap", "shield": True},
            "d1_ice": {"serialname": "ini_d1_ice", "shield": True},
            "c1_vap": {"serialname": "ini_c1_vap", "shield": True},
            "c1_liq": {"serialname": "ini_c1_liq", "shield": True},
            "c1_ice": {"serialname": "ini_c1_ice", "shield": True},
            "t_wfr": {"serialname": "ini_t_wfr", "shield": True},
            "pcaw": {"serialname": "ini_pcaw", "shield": True},
            "pcbw": {"serialname": "ini_pcbw", "shield": True},
            "pcai": {"serialname": "ini_pcai", "shield": True},
            "pcbi": {"serialname": "ini_pcbi", "shield": True},
            "pcar": {"serialname": "ini_pcar", "shield": True},
            "pcbr": {"serialname": "ini_pcbr", "shield": True},
            "pcas": {"serialname": "ini_pcas", "shield": True},
            "pcbs": {"serialname": "ini_pcbs", "shield": True},
            "pcag": {"serialname": "ini_pcag", "shield": True},
            "pcbg": {"serialname": "ini_pcbg", "shield": True},
            "pcah": {"serialname": "ini_pcah", "shield": True},
            "pcbh": {"serialname": "ini_pcbh", "shield": True},
            "edaw": {"serialname": "ini_edaw", "shield": True},
            "edbw": {"serialname": "ini_edbw", "shield": True},
            "edai": {"serialname": "ini_edai", "shield": True},
            "edbi": {"serialname": "ini_edbi", "shield": True},
            "edar": {"serialname": "ini_edar", "shield": True},
            "edbr": {"serialname": "ini_edbr", "shield": True},
            "edas": {"serialname": "ini_edas", "shield": True},
            "edbs": {"serialname": "ini_edbs", "shield": True},
            "edag": {"serialname": "ini_edag", "shield": True},
            "edbg": {"serialname": "ini_edbg", "shield": True},
            "edah": {"serialname": "ini_edah", "shield": True},
            "edbh": {"serialname": "ini_edbh", "shield": True},
            "oeaw": {"serialname": "ini_oeaw", "shield": True},
            "oebw": {"serialname": "ini_oebw", "shield": True},
            "oeai": {"serialname": "ini_oeai", "shield": True},
            "oebi": {"serialname": "ini_oebi", "shield": True},
            "oear": {"serialname": "ini_oear", "shield": True},
            "oebr": {"serialname": "ini_oebr", "shield": True},
            "oeas": {"serialname": "ini_oeas", "shield": True},
            "oebs": {"serialname": "ini_oebs", "shield": True},
            "oeag": {"serialname": "ini_oeag", "shield": True},
            "oebg": {"serialname": "ini_oebg", "shield": True},
            "oeah": {"serialname": "ini_oeah", "shield": True},
            "oebh": {"serialname": "ini_oebh", "shield": True},
            "rraw": {"serialname": "ini_rraw", "shield": True},
            "rrbw": {"serialname": "ini_rrbw", "shield": True},
            "rrai": {"serialname": "ini_rrai", "shield": True},
            "rrbi": {"serialname": "ini_rrbi", "shield": True},
            "rrar": {"serialname": "ini_rrar", "shield": True},
            "rrbr": {"serialname": "ini_rrbr", "shield": True},
            "rras": {"serialname": "ini_rras", "shield": True},
            "rrbs": {"serialname": "ini_rrbs", "shield": True},
            "rrag": {"serialname": "ini_rrag", "shield": True},
            "rrbg": {"serialname": "ini_rrbg", "shield": True},
            "rrah": {"serialname": "ini_rrah", "shield": True},
            "rrbh": {"serialname": "ini_rrbh", "shield": True},
            "tvai": {"serialname": "ini_tvai", "shield": True},
            "tvbi": {"serialname": "ini_tvbi", "shield": True},
            "tvar": {"serialname": "ini_tvar", "shield": True},
            "tvbr": {"serialname": "ini_tvbr", "shield": True},
            "tvas": {"serialname": "ini_tvas", "shield": True},
            "tvbs": {"serialname": "ini_tvbs", "shield": True},
            "tvag": {"serialname": "ini_tvag", "shield": True},
            "tvbg": {"serialname": "ini_tvbg", "shield": True},
            "tvah": {"serialname": "ini_tvah", "shield": True},
            "tvbh": {"serialname": "ini_tvbh", "shield": True},
            "crevp_1": {"serialname": "ini_crevp_1", "shield": True},
            "crevp_2": {"serialname": "ini_crevp_2", "shield": True},
            "crevp_3": {"serialname": "ini_crevp_3", "shield": True},
            "crevp_4": {"serialname": "ini_crevp_4", "shield": True},
            "crevp_5": {"serialname": "ini_crevp_5", "shield": True},
            "cssub_1": {"serialname": "ini_cssub_1", "shield": True},
            "cssub_2": {"serialname": "ini_cssub_2", "shield": True},
            "cssub_3": {"serialname": "ini_cssub_3", "shield": True},
            "cssub_4": {"serialname": "ini_cssub_4", "shield": True},
            "cssub_5": {"serialname": "ini_cssub_5", "shield": True},
            "cgsub_1": {"serialname": "ini_cgsub_1", "shield": True},
            "cgsub_2": {"serialname": "ini_cgsub_2", "shield": True},
            "cgsub_3": {"serialname": "ini_cgsub_3", "shield": True},
            "cgsub_4": {"serialname": "ini_cgsub_4", "shield": True},
            "cgsub_5": {"serialname": "ini_cgsub_5", "shield": True},
            "csmlt_1": {"serialname": "ini_csmlt_1", "shield": True},
            "csmlt_2": {"serialname": "ini_csmlt_2", "shield": True},
            "csmlt_3": {"serialname": "ini_csmlt_3", "shield": True},
            "csmlt_4": {"serialname": "ini_csmlt_4", "shield": True},
            "cgmlt_1": {"serialname": "ini_cgmlt_1", "shield": True},
            "cgmlt_2": {"serialname": "ini_cgmlt_2", "shield": True},
            "cgmlt_3": {"serialname": "ini_cgmlt_3", "shield": True},
            "cgmlt_4": {"serialname": "ini_cgmlt_4", "shield": True},
            "cgfr_1": {"serialname": "ini_cgfr_1", "shield": True},
            "cgfr_2": {"serialname": "ini_cgfr_2", "shield": True},
            "normw": {"serialname": "ini_normw", "shield": True},
            "normr": {"serialname": "ini_normr", "shield": True},
            "normi": {"serialname": "ini_normi", "shield": True},
            "norms": {"serialname": "ini_norms", "shield": True},
            "normg": {"serialname": "ini_normg", "shield": True},
            "expow": {"serialname": "ini_expow", "shield": True},
            "expor": {"serialname": "ini_expor", "shield": True},
            "expoi": {"serialname": "ini_expoi", "shield": True},
            "expos": {"serialname": "ini_expos", "shield": True},
            "expog": {"serialname": "ini_expog", "shield": True},
            "cracw": {"serialname": "ini_cracw", "shield": True},
            "craci": {"serialname": "ini_craci", "shield": True},
            "csacw": {"serialname": "ini_csacw", "shield": True},
            "csaci": {"serialname": "ini_csaci", "shield": True},
            "cgacw": {"serialname": "ini_cgacw", "shield": True},
            "cgaci": {"serialname": "ini_cgaci", "shield": True},
            "cracs": {"serialname": "ini_cracs", "shield": True},
            "csacr": {"serialname": "ini_csacr", "shield": True},
            "cgacr": {"serialname": "ini_cgacr", "shield": True},
            "cgacs": {"serialname": "ini_cgacs", "shield": True},
            "acc": {"serialname": "ini_acc", "kend": 20, "shield": True},
            "acco1": {"serialname": "ini_acco1", "kend": 10, "shield": True},
            "acco2": {"serialname": "ini_acco2", "kend": 10, "shield": True},
            "acco3": {"serialname": "ini_acco3", "kend": 10, "shield": True},
        }

        self.mpconfig = GFDLCloudMPConfig(
            dt_full=self.config.dt_atmos,
            hydrostatic=self.config.hydrostatic,
            npx=self.config.npx,
            npy=self.config.npy,
            npz=self.config.npz,
            nwat=self.config.nwat,
            do_qa=self.config.do_qa,
            do_inline_mp=self.config.do_inline_mp,
            c_cracw=self.config.c_cracw,
            c_paut=self.config.c_paut,
            c_pgacs=self.config.c_pgacs,
            c_psaci=self.config.c_psaci,
            ccn_l=self.config.ccn_l,
            ccn_o=self.config.ccn_o,
            const_vg=self.config.const_vg,
            const_vi=self.config.const_vi,
            const_vr=self.config.const_vr,
            const_vs=self.config.const_vs,
            vs_fac=self.config.vs_fac,
            vg_fac=self.config.vg_fac,
            vi_fac=self.config.vi_fac,
            vr_fac=self.config.vr_fac,
            de_ice=self.config.de_ice,
            layout=self.config.layout,
            tau_imlt=self.config.tau_imlt,
            tau_i2s=self.config.tau_i2s,
            tau_g2v=self.config.tau_g2v,
            tau_v2g=self.config.tau_v2g,
            ql_mlt=self.config.ql_mlt,
            qs_mlt=self.config.qs_mlt,
            t_sub=self.config.t_sub,
            qi_gen=self.config.qi_gen,
            qi_lim=self.config.qi_lim,
            qi0_max=self.config.qi0_max,
            rad_snow=self.config.rad_snow,
            rad_rain=self.config.rad_rain,
            dw_ocean=self.config.dw_ocean,
            dw_land=self.config.dw_land,
            tau_l2v=self.config.tau_l2v,
            c2l_ord=self.config.c2l_ord,
            do_sedi_heat=self.config.do_sedi_heat,
            do_sedi_w=self.config.do_sedi_w,
            fast_sat_adj=self.config.fast_sat_adj,
            qc_crt=self.config.qc_crt,
            fix_negative=self.config.fix_negative,
            irain_f=self.config.irain_f,
            mp_time=self.config.mp_time,
            prog_ccn=self.config.prog_ccn,
            qi0_crt=self.config.qi0_crt,
            qs0_crt=self.config.qs0_crt,
            rh_inc=self.config.rh_inc,
            rh_inr=self.config.rh_inr,
            rthresh=self.config.rthresh,
            sedi_transport=self.config.sedi_transport,
            use_ppm=self.config.use_ppm,
            vg_max=self.config.vg_max,
            vi_max=self.config.vi_max,
            vr_max=self.config.vr_max,
            vs_max=self.config.vs_max,
            z_slope_ice=self.config.z_slope_ice,
            z_slope_liq=self.config.z_slope_liq,
            tice=self.config.tice,
            alin=self.config.alin,
            clin=self.config.clin,
        )

    def compute(self, inputs):
        self.make_storage_data_input_vars(inputs)

        compute_func = PassVars(
            self.mpconfig,
        )

        compute_func(**inputs)

        return self.slice_output(inputs)
