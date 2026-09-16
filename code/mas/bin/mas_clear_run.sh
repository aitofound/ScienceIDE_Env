#!/bin/bash

# Delete all run files of a run, keeping the input files.

if [ "$1" == "" ]; then
  runid=""
else
  runid=$1
fi

# ADD "-y" option to override prompt.
# ADD PROMPT MAKING SURE USER REALLY WANTS THIS

rm {[abvjezt]?,p,rho,heat,t,tracers_pos,eta,ef_*,rs,slice*}[0-9][0-9][0-9]*.h* 2> /dev/null
rm br_photo.h* 2> /dev/null
rm br_r0_init.h5 2> /dev/null
rm heatflux*.h* 2> /dev/null
rm potfld*.h* 2> /dev/null
rm bhat*.h* 2> /dev/null
rm *.out 2> /dev/null
rm *.log 2> /dev/null
rm *.err 2> /dev/null

rm rs${runid}.h5 2> /dev/null
rm [vhwot]${runid} 2> /dev/null
rm mas_dumps*.txt 2> /dev/null


