#!/bin/csh
###########################################
echo "Setting up virtual display"
/usr/bin/Xvfb :79 -screen 0 2048x1024x24 &
set XCFB_PID=$!
#echo $SSH_PID > xcfb.pid
setenv DISPLAY :79.0
echo "Done setting up virtual disaply"
#######################################
setenv IDL_PATH +~/MFLAMPA/SWMF/share/IDL:$IDL_PATH

idl animate_xyz.pro > log_animate_xyz

kill $XCFB_PID
