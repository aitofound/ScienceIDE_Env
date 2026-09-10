#!/usr/bin/env python

import numpy as np
import h5py
import re
import datetime as dt
import matplotlib.pyplot as plt
from pylab import cm
from amie_routines import *

# ----------------------------------------------------------------------
# If the pole is not included, make a point at the pole and fill it in
# with the average value
# ----------------------------------------------------------------------

def add_pole_value(value):
    pole = np.mean(value[0,:])
    nMlts = len(value[0,:])
    pole1d = np.zeros([nMlts]) + pole
    valueOut = np.vstack([pole1d, value])
    return valueOut

# ----------------------------------------------------------------------
# Shift an array by a give amount and copy the first to the last
# ----------------------------------------------------------------------

def shift_mlts_and_copy(data):
    nMlts = len(data[0,:])
    nLats = len(data[:,0])
    nShift = int((nMlts)/2)
    newData = np.zeros((nLats, nMlts+1))
    newData[:, 0:nShift] = data[:, nShift:]
    newData[:, nShift:-1] = data[:, 0:nShift]
    newData[:, -1] = newData[:, 0]
    return newData

# ----------------------------------------------------------------------
# Copy a dictionary:
# ----------------------------------------------------------------------

def copy_dict(dictIn):
    dictOut = {}
    for key in dictIn:
        dictOut[key] = dictIn[key]
    return dictOut

# ----------------------------------------------------------------------
# read in the mage file and return north and south dictionaries
# ----------------------------------------------------------------------

def read_mage_file(filename):

    print('Reading File : ', filename)

    f = h5py.File(filename, 'r')

    nTimes = 0
    steps = []
    for a in f.keys():
        if ('Step' in a):
            nTimes += 1
            steps.append(a)

    print('nTimes : ', nTimes)

    # I can't figure out what other variables would be lat and mlt, so these
    # are the closest I can figure out to use:
    # X and Y are defined at the cell corners and not the centers. Ugh.
    x = np.array(f['X'])*180.0/np.pi
    y = np.array(f['Y'])*180.0/np.pi

    r = np.sqrt(x**2 + y**2)
    lat = 90.0 - r
    lats1d = (lat[1:,0] + lat[:-1,0])/2
    print(lats1d)
    if (lats1d[0] < 89.5):
        lats1d = np.concatenate([[90.0],lats1d])
        doAddPole = True
    else:
        doAddPole = False

    lon = np.arctan2(y, x) * 180.0 / np.pi # + 90.0) % 360.0
    lon1d = (lon[0,:-1] + lon[0,1:])/2
    loncheck = (lon[0,:-1] * lon[0,1:])
    lon1d[loncheck < -10.0] = lon1d[loncheck < -10.0] + 180.0
    lon1d[lon1d < -0.5] = lon1d[lon1d < -0.5] + 360.0
    lon1d = (lon1d + 180.0) % 360.0
    nLons = len(lon1d)
    nShift = int(nLons/2)
    lon1d = np.roll(lon1d, nShift)
    lon1d = np.concatenate([lon1d, [lon1d[0] + 360]])
    mlts1d = lon1d/15.0
    nMlts = len(mlts1d)

    mlts2d, lats2d = np.meshgrid(mlts1d, lats1d)

    dLat = lats1d[2] - lats1d[1]
    dLon = lon[0,2] - lon[0,1]
    dtom = 2 * np.pi * (6371.0 + 120.0) * 1000.0 / 360.0
    area = dtom * dtom * dLat * dLon * np.cos(lats2d * np.pi / 180.0)

    sPotA = 'Potential (V)'
    sEfluxA = 'Electron Energy Flux (mW/m2)'
    sAveeA = 'Electron Mean Energy (keV)'
    # this is in kV
    sPotN = 'Potential NORTH'
    sPotS = 'Potential SOUTH'
    # this is in keV
    sAveeN = 'Average energy NORTH'
    sAveeS = 'Average energy SOUTH'
    # this is in /cm2/s
    sNfluxN = 'Number flux NORTH'
    sNfluxS = 'Number flux SOUTH'
        
    dataN = {}
    dataN["nLats"] = len(lats1d)
    dataN["nMlts"] = nMlts
    dataN["lats"] = lats1d
    dataN["mlts"] = mlts1d
    dataN["Vars"] = [sPotA, sEfluxA, sAveeA]    
    dataN["nVars"] = len(dataN["Vars"])
    dataN["version"] = 0.9
    dataN["imf"] = []
    dataN["ae"] = []
    dataN["dst"] = []
    dataN["hp"] = []
    dataN["cpcp"] = []
    dataN["times"] = []
    for var in dataN["Vars"]:
        dataN[var] = []

    dataS = {}
    dataS["nLats"] = len(lats1d)
    dataS["nMlts"] = nMlts
    dataS["lats"] = lats1d
    dataS["mlts"] = mlts1d
    dataS["Vars"] = [sPotA, sEfluxA, sAveeA]    
    dataS["nVars"] = len(dataS["Vars"])
    dataS["version"] = 0.9
    dataS["imf"] = []
    dataS["ae"] = []
    dataS["dst"] = []
    dataS["hp"] = []
    dataS["cpcp"] = []
    dataS["times"] = []
    for var in dataS["Vars"]:
        dataS[var] = []

    dataN["hem"] = 'N'
    dataS["hem"] = 'S'
    
    iTimes = range(0,nTimes)

    for iTime in iTimes:
        step = 'Step#%d' % iTime
        print('Reading time : ', iTime, ' ', step)
        s0 = f[step]
        mjd = s0.attrs['MJD']
        time = dt.datetime(1858, 11, 17) + dt.timedelta(days=mjd)
        print(' --> ',time)

        if (doAddPole):
            potentialN = shift_mlts_and_copy(add_pole_value(np.array(s0[sPotN]))) * 1000.0
        else:
            potentialN = shift_mlts_and_copy(np.array(s0[sPotN])) * 1000.0

        if (doAddPole):
            aveeN = shift_mlts_and_copy(add_pole_value(np.array(s0[sAveeN])))
        else:
            aveeN = shift_mlts_and_copy(np.array(s0[sAveeN]))
        if (doAddPole):
            nfluxN = shift_mlts_and_copy(add_pole_value(np.array(s0[sNfluxN])))
        else:
            nfluxN = shift_mlts_and_copy(np.array(s0[sNfluxN]))
        # keV/cm2/s -> mW/m2 (keV->J * /cm2->/m2 * W->mW)
        efluxN = nfluxN * aveeN * 1.60218e-16 * 1e4 * 1e3

        if (doAddPole):
            potentialS = shift_mlts_and_copy(add_pole_value(np.array(s0[sPotS]))) * 1000.0
        else:
            potentialS = shift_mlts_and_copy(np.array(s0[sPotS])) * 1000.0
        if (doAddPole):
            aveeS = shift_mlts_and_copy(add_pole_value(np.array(s0[sAveeS])))
        else:
            aveeS = shift_mlts_and_copy(np.array(s0[sAveeS]))
        if (doAddPole):
            nfluxS = shift_mlts_and_copy(add_pole_value(np.array(s0[sNfluxS])))
        else:
            nfluxS = shift_mlts_and_copy(np.array(s0[sNfluxS]))
        
        # keV/cm2/s -> mW/m2 (keV->J * /cm2->/m2 * W->mW)
        efluxS = nfluxS * aveeS * 1.60218e-16 * 1e4 * 1e3

        hpN = np.sum(area * efluxN/1000.0) / 1e9
        hpS = np.sum(area * efluxS/1000.0) / 1e9
        cpcpN = np.max(potentialN) - np.min(potentialN)
        cpcpS = np.max(potentialS) - np.min(potentialS)

        dataN["imf"].append([-1e32, -1e32, -1e32, -1e32])
        dataN["ae"].append([-1e32, -1e32, -1e32, 0.0])
        dataN["dst"].append([0.0, 0.0])
        dataN["hp"].append([hpN, 0.0])
        dataN["cpcp"].append(cpcpN)
        dataN["times"].append(time)
        dataN[sPotA].append(potentialN)
        dataN[sEfluxA].append(aveeN)
        dataN[sAveeA].append(efluxN)

        dataS["imf"].append([-1e32, -1e32, -1e32, -1e32])
        dataS["ae"].append([-1e32, -1e32, -1e32, 0.0])
        dataS["dst"].append([0.0, 0.0])
        dataS["hp"].append([hpS, 0.0])
        dataS["cpcp"].append(cpcpS)
        dataS["times"].append(time)
        dataS[sPotA].append(np.flip(potentialS, axis = 1))
        dataS[sEfluxA].append(np.flip(aveeS, axis = 1))
        dataS[sAveeA].append(np.flip(efluxS, axis = 1))

    f.close()
    dataN["nTimes"] = len(dataN["times"])
    dataS["nTimes"] = len(dataS["times"])
    return dataN, dataS

