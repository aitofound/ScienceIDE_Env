#!/bin/tcsh

setenv IDL_PATH +$1/share/IDL
setenv SSW $HOME/ssw
setenv SECCHI $SSW/stereo/secchi
setenv SECCHI_CAL $SECCHI/calibration
setenv NRL_LIB $SSW/soho/lasco
setenv SSW_INSTR "gen soho aia hmi xrt eit lasco secchi nrl festival"
source $SSW/gen/setup/setup.ssw
setenv SSWDB $HOME/sswdb
setenv SECCHI_BKG $SSWDB/stereo/secchi/backgrounds
setenv MONTHLY_IMAGES $SSWDB/soho/lasco/monthly
setenv CDF_LEAPSECONDSTABLE CDFLeapSeconds
setenv IDL_STARTUP ssw_startup_insitu

printf "compare_insitu, dir_sim='$2', dir_plot='$3', ModelIn='$4',dir_obs='$5', EventTimeDist='$6', TimeWindowDist=$7, dir_CME_list='$8'" | sswidl > $3/log_insitu

exit
