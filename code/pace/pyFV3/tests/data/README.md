# Data for hybrid pressure calculations at each vertical level

PyFV3 requires the coefficients necessary for calculation of the pressure at each k-level to be supplied during a run. The equation for calculating these pressures takes the form:

$$\\P_k = a_k + b_k * P_s\\$$

where $P_k$ (also $\eta$) is the pressure at the k-level, $a_k$ and $b_k$, the needed coefficients, and $P_s$ the surface level pressure. These coefficients must be supplied in a NetCDF file format, and in a monotonically increasing format.
