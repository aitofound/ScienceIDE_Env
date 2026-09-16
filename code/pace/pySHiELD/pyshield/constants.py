import ndsl.constants as constants
from ndsl.dsl.typing import Float

# Driver constants
HOCP = Float(constants.HLV / constants.CP_AIR)
QMIN = Float(1.0e-10)
RAINMIN = Float(1.0e-13)
P850 = Float(85000.0)
EPSQ = Float(1.0e-20)
HSUB = Float(constants.HLV + constants.HLF)
CZMIN = Float(0.0001)
"""cos(89.994)"""
ZERO = Float(0.0e0)
ALBDF = Float(0.06)
CON_P001 = Float(0.001e0)
CON_DAY = Float(86400.0)
TF = Float(258.16)
TCRF = Float(1.0 / (constants.TICE - TF))
CONT = Float(constants.CP_AIR / constants.GRAV)
CONQ = Float(constants.HLV / constants.GRAV)
P00 = Float(1.0e5)
"""idealized surface atmospheric pressure (Pa)"""
P_REF = Float(101325.0)
"""reference surface pressure for sigma calculation"""
TICE = Float(2.7120e2)
"""temp freezing sea (K)"""

# Alternative Units and Precision in Physics:
CPH2O1 = Float(4.218e3)
"""Specific heat of water in J/kg/K"""
CPH2O2 = Float(4.2e6)

C_LIQ0 = Float(4.2180e3)
"""Specific heat of liquid H2O"""

ELOCP = Float(constants.HLV / constants.CP_AIR)
EL2ORC = Float(constants.HLV * constants.HLV / (constants.RVGAS * constants.CP_AIR))
DELTA = Float(constants.RVGAS / constants.RDGAS - 1.0)
FACT1 = Float((constants.CP_VAP - C_LIQ0) / constants.RVGAS)
FACT2 = Float(constants.HLV / constants.RVGAS - FACT1 * constants.TICE0)

# Constants used to convert units for radiation
MMDRY = 2.89644e-2
"""Molar mass of dry air in kg/mol"""
MMVAP = 1.80153e-2
"""Molar mass of water vapor in kg/mol"""
MMO3 = 4.7997e-2
"""Molar mass of ozone in kg/mol"""

# Constants used in interpolating radiation onto physics timestep:
F_EPS = Float(0.0001)
