#!/usr/bin/env python3
"""
This script is to convert NASA OMNI Web data to readable SWMF BATSRUS input IMF data
Converts the date from YR DOY HR MIN to YR MN DY HR MIN SEC MSEC
Note: The rest of the variables must be correct when downloaded from OMNI Web
      This is a python3 script it is not compatible with python2.

USAGE:
    ./omniweb2swmf.py omnifile.lst IMF.dat

    omnifile.lst: is the input file (.lst) downloaded from NASA OMNI Web interface.
        When using OMNI Web please only include the time, bx by bz, ux uy uz, density, temperature.
        Do not add more data or BATSRUS will not accept.
    IMF.dat: is the output file.
        IMF.dat is the default name of the solar wind files in SWMF PARAM.in files.
"""
import sys
from datetime import datetime as d

def convert():
    """
    Start the process of conversion.
    """
    infile = open(sys.argv[1], 'r')
    outfile = open(sys.argv[2], 'w')
    # Write out the header
    outfile.write("OMNI file downloaded from https://omniweb.gsfc.nasa.gov/\n")
    outfile.write("yr mn dy hr min sec msec bx by bz vx vy vz dens temp\n")
    outfile.write("#START\n")
    # Begin conversion line by line
    for line in infile:
        date = d.strptime(line[:14], "%Y %j %H %M")
        correctline = date.strftime("%Y %m %d %H %M %S") + ' 000' + line[14:]
        outfile.write(correctline)
        #print(correctline)
    # Close files
    outfile.close()
    infile.close()

convert()
