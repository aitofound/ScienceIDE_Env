# **********************************************************************
# Makefile for the Relativistic HydroDynamics (RHD) Module
#
# Note that we add Src/RMHD to VPATH since RHD and RMHD
# module share some files (see below)
# **********************************************************************

VPATH        += $(SRC)/RHD/:$(SRC)/RMHD
INCLUDE_DIRS += -I$(SRC)/RHD/

OBJ += eigenv.o fluxes.o mappers.o mappers_loc.o prim_eqn.o \
       hll_speed.o hll.o hllc.o set_solver.o   \
       rhd_energy_solve.o rhd_entropy_solve.o rhd_pressure_fix.o\
       tvdlf.o two_shock.o
 
#  The following files are shared with the RMHD/

OBJ += four_vel.o rhs.o rhs_source.o vel_limiter.o   
