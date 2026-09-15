import math

import ndsl.constants as constants
from ndsl.dsl.typing import Float

# Constant parameters used in the GFDL Cloud Microphysics
TICE0 = Float(constants.TICE - 0.01)

C_ICE = Float(2.106e3)
"""Heat capacity of ice at 0 degrees Celsius"""
C_LIQ = Float(4.218e3)
"""Heat capacity of water at 0 degrees Celsius"""
DC_ICE = Float(C_LIQ - C_ICE)
"""Isobaric heating / cooling (J/kg/K)"""
DC_VAP = Float(constants.CP_VAP - C_LIQ)
"""Isobaric heating / cooling (J/kg/K)"""
D2ICE = Float(DC_VAP + DC_ICE)
"""Isobaric heating / cooling (J/kg/K)"""
LV0 = Float(constants.HLV - DC_VAP * TICE0)
"""3148711.3338762247, evaporation latent heat coefficient at 0 degrees Kelvin"""
LI00 = Float(constants.HLF - DC_ICE * TICE0)
"""-242413.92000000004, fusion latent heat coefficient at 0 degrees Kelvin"""
LI2 = Float(LV0 + LI00)
"""2906297.413876225, sublimation latent heat coefficient at 0 degrees Kelvin"""
LI0 = Float(constants.HLF - DC_ICE * TICE0)

RHO_0 = Float(1.0)
"""reference air density (kg/m^3), ref: IFS"""
RHO_W = Float(1.0e3)
"""density of cloud water (kg/m^3)"""
RHO_I = Float(9.17e2)
"""density of cloud ice (kg/m^3)"""
RHO_R = Float(1.0e3)
"""density of rain (Lin et al. 1983) (kg/m^3)"""
RHO_S = Float(1.0e2)
"""density of snow (Lin et al. 1983) (kg/m^3)"""
RHO_G = Float(4.0e2)
"""density of graupel (Rutledge and Hobbs 1984) (kg/m^3)"""
RHO_H = Float(9.17e2)
"""density of hail (Lin et al. 1983) (kg/m^3)"""

VISD = Float(1.717e-5)
"""dynamics viscosity of air at 0 deg C and 1000 hPa (Mason, 1971) (kg/m/s)"""
VISK = Float(1.35e-5)
"""kinematic viscosity of air at 0 deg C  and 1000 hPa (Mason, 1971) (m^2/s)"""
VDIFU = Float(2.25e-5)
"""diffusivity of water vapor in air at 0 deg C  and 1000 hPa (Mason, 1971) (m^2/s)"""
TCOND = Float(2.40e-2)
"""thermal conductivity of air at 0 C and 1000 hPa (Mason, 1971) (J/m/s/K)"""
SCM3 = Float(math.exp(1.0 / 3 * math.log(VISK / VDIFU)))
"""Schmidt number, Sc ** (1 / 3) Lin et al. (1983)"""

QCMIN = Float(1.0e-15)
"""min value for cloud condensates (kg/kg)"""
QFMIN = Float(1.0e-8)
"""min value for sedimentation (kg/kg)"""
DT_FR = Float(8.0)
"""t_wfr - dt_fr: minimum temperature water can exist (Moore and Molinero 2011)"""
CDG = Float(3.15121)
"""drag coefficient of graupel (Locatelli and Hobbs, 1974)"""
CDH = Float(0.5)
"""drag coefficient of hail (Heymsfield and Wright, 2014)"""

DZ_MIN_FLIP = Float(1.0e-2)
"""used for correcting flipped height (m)"""

# Terminal Velocity Parameters, Lin et al. (1983)
GCON = Float((4.0 * constants.GRAV * RHO_G / (3.0 * CDG * RHO_0)) ** 0.5)
HCON = Float((4.0 * constants.GRAV * RHO_H / (3.0 * CDH * RHO_0)) ** 0.5)
