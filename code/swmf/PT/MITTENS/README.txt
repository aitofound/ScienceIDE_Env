# --------------------------------------------------------------------------- #
MITTENS: Monte carlo Integration of Turbulent Transport and ENergization of SEPs
    Written by Alex Shane

Solves the equivalent stochastic differential equations of the isotropic
    Parker transport equation.
Uses 1D Lagrangian frame along single IMF line similar to MFLAMPA
Uses forward-in-time Euler-Maruyama method
Conserves particles
Particle splitting is used to increase high energy statistics
    
Currently only runs in standalone mode.
MFLAMPA or synthetic field lines files used as input.
    
To run locally from ~/SWMF/PT/MITTENS:
        make MITTENS
        make rundirloc
        cd run
        mpiexec -n 10 ./MITTENS.exe
# --------------------------------------------------------------------------- #