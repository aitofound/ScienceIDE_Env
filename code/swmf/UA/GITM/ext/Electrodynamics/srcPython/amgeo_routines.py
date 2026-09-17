#!/usr/bin/env python

import numpy as np
import h5py
import re
from datetime import datetime

def read_amgeo_file(filename):

    print('Reading File : ', filename)

    f = h5py.File(filename, 'r')

    varsTop = []

    latsFound = False
    lonsFound = False

    timesNames = []
    times = []
    for key in f.keys():
        if (key == '20150317_030200S'):
            print('bad time found!')
        else:
            m = re.match(r'lats', key)
            if m:
                latsFound = True
            m = re.match(r'lons', key)
            if m:
                lonsFound = True
            m = re.match(r'(\d\d\d\d)(\d\d)(\d\d)_(\d\d)(\d\d)(\d\d)([NS]).*', key)
            if m:
                timesNames.append(key)
                times.append(datetime(int(m.group(1)), \
                                      int(m.group(2)), \
                                      int(m.group(3)), \
                                      int(m.group(4)), \
                                      int(m.group(5)), \
                                      int(m.group(6))))
                sHem = m.group(7)

            varsTop.append(key)

    didWork = True
    doAddPole = False
    dataToWrite = {}
    if (latsFound):
        print(' --> Found Lats')
        lats = np.array(f['lats'])[:, 0]
        if (lats[0] < 89.0):
            lats = np.concatenate([[90.0],lats])
            doAddPole = True
            print(' ---> Need to add the pole')
    else:
        print('lats variable not found!')
        didWork = False

    if (lonsFound):
        mlts = np.array(f['lons'])[0, :] / 15.0
        print(' --> Found Mlts')
    else:
        print('lons variable not found!')
        didWork = False

    if (not didWork):
        return dataToWrite
    
    cPotential = 'epot'
    cEflux = 'total_electron_energy_flux'
    cAvee = 'average_electron_energy'

    dataToWrite["nLats"] = len(lats)
    nMlts = len(mlts)
    dataToWrite["nMlts"] = nMlts
    dataToWrite["nTimes"] = len(times)

    dataToWrite["lats"] = lats
    dataToWrite["mlts"] = mlts
    dataToWrite["times"] = times

    dataToWrite["Vars"] = ['Potential (kV)', \
                           'Electron Energy Flux (ergs/cm2/s)', \
                           'Electron Mean Energy (keV)']

    dataToWrite["nVars"] = len(dataToWrite["Vars"])
    dataToWrite["version"] = 0.9

    dataToWrite["imf"] = []
    dataToWrite["ae"] = []
    dataToWrite["dst"] = []
    dataToWrite["hp"] = []
    dataToWrite["cpcp"] = []
    dataToWrite["hem"] = sHem

    for var in dataToWrite["Vars"]:
        dataToWrite[var] = []

    for key in timesNames:
        print(' -> Evaluating time : ', key)
        dataIn = f[key]

        cpcp = np.array(dataIn['cross_polar_cap_potential'])/1000.0
        hp = np.array(dataIn['estimated_hemispheric_power'])/1e9
      
        pot = np.array(dataIn[cPotential])
        if (doAddPole):
            pole = np.mean(pot[0,:])
            pole1d = np.zeros([nMlts]) + pole
            pot = np.vstack([pole1d, pot])
            cPot = dataToWrite["Vars"][0]
        dataToWrite[cPot].append(pot)
    
        eflux = np.array(dataIn[cEflux])
        if (doAddPole):
            pole = np.mean(eflux[0,:])
            pole1d = np.zeros([nMlts]) + pole
            eflux = np.vstack([pole1d, eflux])
            cEFlux = dataToWrite["Vars"][1]
            dataToWrite[cEFlux].append(eflux)
    
        avee = np.array(dataIn[cAvee])
        if (doAddPole):
            pole = np.mean(avee[0,:])
            pole1d = np.zeros([nMlts]) + pole
            avee = np.vstack([pole1d, avee])
        cAveE = dataToWrite["Vars"][2]
        dataToWrite[cAveE].append(avee)

        dataToWrite["imf"].append([-1e32, -1e32, -1e32, -1e32])
        dataToWrite["ae"].append([-1e32, -1e32, -1e32, 0.0])
        dataToWrite["dst"].append([0.0, 0.0])
        dataToWrite["hp"].append([hp, 0.0])
        dataToWrite["cpcp"].append(cpcp)

    f.close()
    return dataToWrite
    
