import ndsl.constants as constants
from ndsl.dsl.typing import Float

# sfc_sice constants
FLOAT_EPS = Float(1.0e-8)
HIMAX = Float(8.0)
"""maximum ice thickness allowed"""
HIMIN = Float(0.1)
"""minimum ice thickness required"""
HSMAX = Float(2.0)
"""maximum snow depth allowed"""
TIMIN = Float(173.0)
"""minimum temperature allowed for snow/ice"""
ALBFW = Float(0.06)
"""albedo for lead"""
DSI = Float(1.0 / 0.33)
TSICE = Float(2.7120e2)
"""temp freezing sea (K)"""
TFW = Float(-1.8)
"""TFW - seawater freezing temp (c)"""
CIMIN = Float(0.15)
"""Minimum ice concentration"""
RHO_SNO = Float(330.0)
"""Density of snow over sea ice (kg/m^3)"""
RHO_ICE = Float(917.0)
"""Density of ice   (kg/m^3)"""
DSDW = Float(RHO_SNO / constants.RHO_H2O)
"""Ratio of snow to water density"""
DIDW = Float(RHO_ICE / constants.RHO_H2O)
"""Ratio of ice to water density"""
CI = Float(2054.0)
"""heat capacity of fresh ice (j/kg/k)"""
LI = Float(3.34e5)
"""latent heat of fusion of fresh ice (j/kg-ice)"""
SI = Float(1.0)
"""salinity of sea ice"""
MU = Float(0.054)
"""relates freezing temp to salinity"""
KI = Float(2.03)
"""conductivity of ice  (w/mk)"""
KS = Float(0.31)
"""conductivity of snow   (w/mk)"""
I0 = Float(0.3)
"""ice surface penetrating solar fraction"""
TFI = Float(-MU * SI)
"""sea ice freezing temp = -MU*salinity"""
DICI = Float(RHO_ICE * CI)
"""Ice density times heat capacity (j/m^3/k)"""
DILI = Float(RHO_ICE * LI)
"""Ice density times latent heat of fusion (j/m^3)"""
DSLI = Float(RHO_SNO * LI)
"""Snow density times latent heat of fusion (j/m^3)"""

# SFC_DIFF Constants
ALPHA = Float(5.0)
A0 = Float(-3.975)
CA = Float(0.4)
A1 = Float(12.32)
ALPHA4 = Float(4.0 * ALPHA)
B1 = Float(-7.755)
B2 = Float(6.041)
ALPHA2 = Float(ALPHA + ALPHA)
BETA = Float(1.0)
A0P = Float(-7.941)
A1P = Float(24.75)
B1P = Float(-8.705)
B2P = Float(7.899)
ZTMIN = Float(-999.0)
CHARNOCK = Float(0.014)
VIS = Float(1.4e-5)
RNU = Float(1.51e-5)
VISI = Float(1.0 / VIS)
A2 = Float(17.2693882)
A3 = Float(273.16)
A4 = Float(35.86)
A23M4 = Float(A2 * (A3 - A4))
NSOLD = Float(4)
GS1 = Float(9.8)
GS2 = Float(9.81)
LSUBC = Float(2.501e6)
LSUBF = Float(3.335e5)
LSUBS = Float(2.83e6)
ELCP = Float(2.4888e3)
RD1 = Float(287.04)
CP1 = Float(1004.5)
CP2 = Float(1004.0)
CPVAP = Float(1.8460e3)
CPICE = Float(2.1060e3)
CPICE1 = Float(2.106e6)
SIGMA1 = Float(5.67e-8)
ZBOT = Float(-8.0)
SALP = Float(4.0)
CFACTR = Float(0.5)
CMCMAX = Float(0.5e-3)
SBETA = Float(-2.0)
RSMAX = Float(5000.0)
TOPT = Float(298.0)
REFDK = Float(2.0e-6)
FRZK = Float(0.15)
FXEXP = Float(2.0)
REFKDT = Float(3.0)
CZIL = Float(0.075)
CSOIL = Float(2.00e6)
PSAT = Float(6.1078e2)
"""Pressure at H20 3pt (Pa)"""
Z0S_MAX = Float(0.317e-2)
"""a limiting value for z0 under high winds"""
