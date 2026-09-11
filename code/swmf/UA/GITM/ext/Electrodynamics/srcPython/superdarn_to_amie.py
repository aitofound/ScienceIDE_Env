#!/usr/bin/env python3

import matplotlib.pyplot as plt
from pylab import cm

import numpy as np
from datetime import datetime
from amie_routines import *
from superdarn_routines import *
import argparse
from glob import glob
import os

# ----------------------------------------------------------------------
# Function to parse input arguments
# ----------------------------------------------------------------------

def parse_args():

    parser = argparse.ArgumentParser(description = \
                                     'Convert SuperDARN file to AMIE')

    parser.add_argument('-file', default = '20130628.pot', \
                        help = 'file to process')

    args = parser.parse_args()

    return args

#------------------------------------------------------------------------------
# SuperDARN is organized as an equal area grid, so that at each
# latitude, there are a different number of longitudes. To find points
# in this grid, we create a hash table, so we can input the latitude
# and get all of the longitudes associated with the latitude.
# ------------------------------------------------------------------------------

def make_hash_table(data):
    
    lats = np.abs(data.vec_lat)
    lons = data.vec_lon

    hashTable = {
        'indices': {},
        'values': {},
        'lats': [],
        'lons': {},
        'dlons': {}}

    tmpLats = lats
    allVals = data.pot

    while (np.max(tmpLats) > 0):
        maxLat = np.max(tmpLats)
        if (maxLat > 0):
            ind = np.where(tmpLats == maxLat)
            hashTable['indices'][maxLat] = ind
            hashTable['lons'][maxLat] = lons[ind]
            hashTable['values'][maxLat] = allVals[ind]
            hashTable['lats'].append(maxLat)
            tmpLats[ind] = -1.0
            
    return hashTable

#------------------------------------------------------------------------------
# Given an array of longitudes, find the linear interpolation indices
# for the input longitude, then interpolate the values using those indices
# ------------------------------------------------------------------------------

def interpret_lon(inLon, lons, vals):
    
    nLons = len(lons)
    dLon = 360.0/nLons

    if (inLon < lons[0]):
        # This is below the first longitude:
        left = vals[-1]
        right = vals[0]
        r = (lons[0] - lon) / dLon
    else:
        if (inLon >= lons[-1]):
            # this is above the last longitude:
            left = vals[-1]
            right = vals[0]
            r = 1.0 - (lon - lons[-1]) / dLon
        else:
            # this is the general case - in the grid:
            iL = int(np.floor((inLon - lons[0]) / dLon))
            left = vals[iL]
            right = vals[iL+1]
            r = (lons[iL+1] - inLon) / dLon
    outVal = (1 - r) * right + r * left
    
    return outVal
    
#------------------------------------------------------------------------------
# Find a value within the hash table.  There are four regions:
# 1. below the absolute minimum latitude: just set to zero
# 2. Between the absolute minimum latitude and the minimum hash-table latitude:
#    - Do a linear interpolation between 0 and value at the longitude an minimum
#      latitude of the hash table
# 3. In the grid:
#    - Find the latitudes above and below the requested latitude
#    - linearly interpolate the values to the longitudes at lats above and below
#    - linearly interpolate those two values to the requested latitude
# 4. Above the max latitude:
#    - take the average of the potentials at the maximum latitude
#    - find the potential at the right longitude using the max latitude
#    - linearly interpolate these value between the requested lat and 90.0 deg
#------------------------------------------------------------------------------

def find_val(inLat, inLon, Hash, minlat = 0.0):
    lats = Hash['lats']
    if (inLat < minlat):
        # below minimum latitude (1):
        value = 0.0
    else:
        minHash = np.min(lats)
        maxHash = np.max(lats)
        if (inLat <= minHash):
            # below the minimum superDARN latitude (2):
            downVal = 0.0
            lons = Hash['lons'][minHash]
            vals = Hash['values'][minHash]
            upVal = interpret_lon(inLon, lons, vals)
            r = (minHash - inLat) / (minHash - minlat)
        else:
            if (inLat >= maxHash):
                # Above the maximum superDARN latitude (4):
                lons = Hash['lons'][maxHash]
                vals = Hash['values'][maxHash]
                upVal = np.mean(vals)
                downVal = interpret_lon(inLon, lons, vals)
                r = (90.0 - inLat) / (90.0 - maxHash)
            else:
                # in the grid (3):
                # The hash table goes from high to low latitude!
                d = lats - inLat
                inds = np.where(d <= 0.0)
                iDown = inds[0][0].item()
                iUp = iDown - 1  # Reversed!
                upHash = lats[iUp]
                downHash = lats[iDown]

                lons = Hash['lons'][downHash]
                vals = Hash['values'][downHash]
                downVal = interpret_lon(inLon, lons, vals)
                lons = Hash['lons'][upHash]
                vals = Hash['values'][upHash]
                upVal = interpret_lon(inLon, lons, vals)
                r = (upHash - inLat) / (upHash - downHash)
        value = (1.0 - r) * upVal + r * downVal
    return value

#------------------------------------------------------------------------------
# main code:
#------------------------------------------------------------------------------

# Get the input arguments
args = parse_args()

file = args.file

if (not os.path.isfile(file)):
    print('SuperDARN file does not exist: ', file)
    exit()

# Define the grid to output:
dLat = 1.0
dLon = 3.0
lats1d = np.arange(50.0, 90.0 + dLat, dLat)
lons1d = np.arange(0, 360 + dLon, dLon)

nLats = len(lats1d)
nLons = len(lons1d)

lats2d = np.zeros((nLons, nLats))
lons2d = np.zeros((nLons, nLats))
vals2d = np.zeros((nLons, nLats))

dataToWrite = {}
dataToWrite["nLats"] = nLats
dataToWrite["nMlts"] = nLons
# Set to zero to start and then increment:
dataToWrite["nTimes"] = 0

dataToWrite["lats"] = lats1d
dataToWrite["mlts"] = lons1d / 15.0

# These are the AMIE variable names to write that IE understands:
sPot = 'Potential (V)'
dataToWrite["Vars"] = [sPot]

dataToWrite["nVars"] = len(dataToWrite["Vars"])
dataToWrite["version"] = 0.9
dataToWrite["imf"] = []
dataToWrite["ae"] = []
dataToWrite["dst"] = []
dataToWrite["hp"] = []
dataToWrite["cpcp"] = []
dataToWrite["hem"] = 'N'
dataToWrite["times"] = []
for var in dataToWrite["Vars"]:
    dataToWrite[var] = []

fpin = open(file, 'r')
Eof = False
i = 0
while (not Eof):
    i += 1
    data = readGlobalPot(fpin)

    if (hasattr(data, 'vec_lat')):

        if (np.min(data.vec_lat) < -80.0):
            dataToWrite["hem"] = 'S'
            
        # Create a hash table for the equal area grid
        hashTable = make_hash_table(data)

        dataToWrite["imf"].append([-1e32, -1e32, -1e32, -1e32])
        dataToWrite["ae"].append([-1e32, -1e32, -1e32, 0.0])
        dataToWrite["dst"].append([0.0, 0.0])
        dataToWrite["hp"].append([0.0, 0.0])
        dataToWrite["cpcp"].append(data.max_pot - data.min_pot)
        dataToWrite['times'].append(data.time)

        # estimate what magnetic longitudes we need based on the desired
        # magnetic local time:
        ut = data.hr + data.mt/60.0 + data.sc/3600.0
        mlts = lons1d / 15.0
        magpole = 275.0
        maglons1d = (((mlts - ut) * 15.0 + 275.0) + 180.0) % 360.0

        # Move the potential to a uniform grid:
        vals2d = np.zeros((nLons, nLats))
        for iLat, lat in enumerate(lats1d):
            lons2d[:, iLat] = lons1d
            lats2d[:, iLat] = lats1d[iLat]    
            if (lat < np.min(hashTable['lats'])):
                vals2d[:, iLat] = 0.0    
            else:
                for iLon, lon in enumerate(maglons1d):
                    vals2d[iLon, iLat] = find_val(lat, lon, hashTable)
        v2d = vals2d.transpose()
        dataToWrite[sPot].append(vals2d.transpose())
    else:
        Eof = True

dataToWrite['nTimes'] = len(dataToWrite['times'])

ymd = dataToWrite['times'][0].strftime('%Y%m%d')
outfile = 'superdarn' + ymd + dataToWrite['hem'] + '.bin'
print(' -> Writing AMIE-style file : ', outfile)
amie_write_binary(outfile, dataToWrite)
