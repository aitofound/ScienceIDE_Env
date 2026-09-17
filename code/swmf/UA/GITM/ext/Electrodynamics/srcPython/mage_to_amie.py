#!/usr/bin/env python

import numpy as np
import datetime as dt
from mage_routines import *
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

for filename in allFiles:

    dataN, dataS = read_mage_file(filename)

    ymd = dataN['times'][0].strftime('%Y%m%d')
    outfile = 'mage' + ymd + dataN['hem'] + '.bin'
    print(' -> Writing AMIE-style file : ', outfile)
    amie_write_binary(outfile, dataN)

    outfile = 'mage' + ymd + dataS['hem'] + '.bin'
    print(' -> Writing AMIE-style file : ', outfile)
    amie_write_binary(outfile, dataS)


