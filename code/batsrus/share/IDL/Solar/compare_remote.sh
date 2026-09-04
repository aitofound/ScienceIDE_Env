#!/bin/tcsh

setenv IDL_PATH +$1/share/IDL
setenv SSW $HOME/ssw
setenv SECCHI $SSW/stereo/secchi
setenv SECCHI_CAL $SECCHI/calibration
setenv NRL_LIB $SSW/soho/lasco
setenv SSW_INSTR "gen soho aia hmi xrt eit lasco secchi nrl festival sunspice"
setenv SSWDB $HOME/sswdb
setenv SECCHI_BKG $SSWDB/stereo/secchi/backgrounds
setenv MONTHLY_IMAGES $SSWDB/soho/lasco/monthly
setenv CDF_LEAPSECONDSTABLE CDFLeapSeconds
setenv IDL_STARTUP ssw_startup_remote
source $SSW/gen/setup/setup.ssw

printf "compare_remote, dir_sim='$2', dir_plot='$3', dir_obs='$4' " | sswidl > $3/log_remote

exit
