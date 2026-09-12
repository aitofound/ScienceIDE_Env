#!/usr/bin/env python3

import numpy as np
from datetime import datetime
from amie_routines import *
from rim_routines import *
import argparse
from glob import glob

# ----------------------------------------------------------------------
# Function to parse input arguments
# ----------------------------------------------------------------------

def parse_args():

    parser = argparse.ArgumentParser(description = 'Convert SWMF/RIM to AMIE')

    parser.add_argument('-dir', default = './', \
                        help = 'directory of files to process')

    # select altitude to plot:
    parser.add_argument('-maxfilesize',
                        default = 1000, type = float, \
                        help = 'maximum file size in MB') 

    # variable to plot as a number
    parser.add_argument('-name',  \
                        default = 'swmf', \
                        help = 'name to add to file')
    
    args = parser.parse_args()

    return args

#------------------------------------------------------------------------------
# main code:
#------------------------------------------------------------------------------

# Get the input arguments
args = parse_args()

maxFileSize = args.maxfilesize

filelist = sorted(glob(args.dir + '/it*.idl'))

if (len(filelist) <= 1):
    print(' --> Checking for gz files:')
    filelist = sorted(glob(args.dir + '/it*.gz'))

dataToWriteN, dataToWriteS = read_all_rim_files([filelist[0]])

print('  --> Figuring out how many chunks to make')
vars = dataToWriteS["Vars"]
nTimes, nLats, nMlts = np.shape(dataToWriteS[vars[0]])
nVars = dataToWriteN['nVars']
# Size in MB:
roughSizePerTime = 4.0 * nVars * nLats * nMlts / 1e6
nTimes = len(filelist)
print('   --> Rough Size per Time (MB): ', roughSizePerTime)
print('   --> Rough Size total (MB): ', roughSizePerTime * nTimes)

nFilesMax = int(maxFileSize / roughSizePerTime)

nFilesTotal = 0
iStart = 0
iChunk = 0
while (iStart < len(filelist)):
    iEnd = iStart + nFilesMax
    if (iEnd > len(filelist)):
        iEnd = len(filelist)

    dataToWriteN, dataToWriteS = read_all_rim_files(filelist[iStart:iEnd])
    if (iStart == 0):
        ymd = dataToWriteN['times'][0].strftime('%Y%m%d')
    if ((iStart == 0) and (iEnd == len(filelist))):
        sNum = ''
    else:
        sNum = '_%02d' % iChunk
    outfile = args.name + ymd + dataToWriteN['hem'] + sNum + '.bin'
    print(' -> Writing AMIE-style file : ', outfile)
    amie_write_binary(outfile, dataToWriteN)

    outfile = args.name + ymd + dataToWriteS['hem'] + sNum + '.bin'
    print(' -> Writing AMIE-style file : ', outfile)
    amie_write_binary(outfile, dataToWriteS)
    
    iStart = iEnd
    iChunk = iChunk + 1
    

