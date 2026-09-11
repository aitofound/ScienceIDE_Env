#!/usr/bin/env python3

import numpy as np
import h5py
import re
from datetime import datetime
from amie_routines import *
from amgeo_routines import *
import argparse

# ----------------------------------------------------------------------
# Function to parse input arguments
# ----------------------------------------------------------------------

def parse_args():

    parser = argparse.ArgumentParser(description = 'Convert AMGeo to AMIE')

    parser.add_argument('filelist', nargs='+', \
                        help = 'list files to use for generating plots')

    args = parser.parse_args()

    return args


#------------------------------------------------------------------------------
# main code:
#------------------------------------------------------------------------------

# Get the input arguments
args = parse_args()

allFiles = args.filelist

nTimes = []

for file in allFiles:
   dataToWrite = read_amgeo_file(file)
   ymd = dataToWrite['times'][0].strftime('%Y%m%d')
   nTimes.append(len(dataToWrite['times']))
   outfile = 'amgeo' + ymd + dataToWrite['hem'] + '.bin'
   print(' -> Writing AMIE-style file : ', outfile)
   amie_write_binary(outfile, dataToWrite)

for i, file in enumerate(allFiles):
   print('nTimes in file : ', file, nTimes[i])

if (np.max(nTimes) > np.min(nTimes)):
   print('nTimes dont all match!!!  Be careful!!!')
