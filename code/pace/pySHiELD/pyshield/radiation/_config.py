import dataclasses
import datetime
from pathlib import Path

from ndsl.dsl.typing import Float, Int


@dataclasses.dataclass
class RTE_RRTMGPConfig:
    deltsw: Float
    delt_rad: Float
    date: datetime.datetime
    solar_constant_file: Path
    input_dir: Path
    aerosol_file: Path
    fhswr: Float
    fhlwr: Float
    isolar: Int = 0
    """
    Solar constant computation
        0: use the old fixed solar constant in "physcon"
        10: use the new fixed solar constant in "physcon"
        1: use noaa ann-mean tsi tbl abs-scale with cyc apprx
        2: use noaa ann-mean tsi tbl tim-scale with cyc apprx
        3: use cmip5 ann-mean tsi tbl tim-scale with cyc apprx
        4: use cmip5 mon-mean tsi tbl tim-scale with cyc apprx
    """
    icmphys: Int = 4
    """Prognostic cloud property calculation scheme"""
    ico2flg: Int = 0
    """
    co2 data source control flag
        0: use prescribed co2 global mean value
        1: use input global mean co2 value (co2_glb)
        2: use input 2-d monthly co2 value (co2vmr_sav)
    """
    ioznflg: Int = 1
    """
    Flag for ozone control
         0: climatological ozone profile
        >0: interactive ozone profile
    """
    ictmflg: Int = 0
    """
    co2 data ic time/date control flag
        -2: same as 0, but superimpose seasonal cycle
            from climatology data set.
        -1: use user provided external data for the fcst
            time, no extrapolation.
        0: use data at initial cond time, if not existed
            then use latest, without extrapolation.
        1: use data at the forecast time, if not existed
            then use latest and extrapolate to fcst time.
        yyyy0: use yyyy data for the forecast time, no
            further data extrapolation.
        yyyy1: use yyyy data for the fcst. if needed, do
           extrapolation to match the fcst time.
    """
    ialbflg: Int = -2
    """
    Flag for albedo scheme
        -2: prescribed ocean, land, ice albedos for SW
        -1: constant albedo for SW
        0: climatology surface albedo scheme for SW
        1: MODIS based land surface albedo for SW
        2: land surface model albedo for SW
    """
    iemsflg: Int = 0
    """
    Flag for surface emissivity.
        0: fixed SFC emissivity at 1.0
        1: input SFC emissivity type map from "semis_file"
        2: SFC emissivity from land model
    """
    ldisable_radiation_quasi_sea_ice: bool = False
    daily_mean: bool = False
    fixed_sollat: bool = False
    sollat: Float = 0.0
    nstp: Int = 6
    ivflip: Int = 1
    lcnorm: bool = False
    lcrick: bool = False
    gfs_cloud_overlap: bool = False

    def __post_init__(self):
        if self.ioznflg == 0:
            raise NotImplementedError(
                "climatological ozone (ioznflg = 0) is not supported"
            )
