#!/usr/bin/env python

from glob import glob
from datetime import datetime
from datetime import timedelta
from struct import unpack
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.dates as dates
from pylab import cm
import re 

#-----------------------------------------------------------------------------
#
#-----------------------------------------------------------------------------
def find_nearest_index(array, value):
    array = np.asarray(array)
    idx = (np.abs(array - value)).argmin()
    return idx

def read_gitm_header(file):

    if (len(file) == 0):
        filelist = glob('./3DALL*.bin')

        if (len(filelist) == 0):
            print("No 3DALL files found. Checking for 1DALL.")
            filelist = glob('./1DALL*.bin')
            if (len(filelist) == 0):
                print("No 1DALL files found. Stopping.")
                exit()
            file = filelist[0]

    else:

        filelist = glob(file[0])
        file = filelist[0]

    
    header = {}
    header["nFiles"] = len(filelist)
    header["version"] = 0
    header["nLons"] = 0
    header["nLats"] = 0
    header["nAlts"] = 0
    header["nVars"] = 0
    header["vars"] = []
    header["time"] = []
    header["filename"] = []

    header["filename"].append(file)

    f=open(file, 'rb')

    # This is all reading header stuff:

    endChar='>'
    rawRecLen=f.read(4)
    recLen=(unpack(endChar+'l',rawRecLen))[0]
    if (recLen>10000)or(recLen<0):
        # Ridiculous record length implies wrong endian.
        endChar='<'
        recLen=(unpack(endChar+'l',rawRecLen))[0]

    # Read version; read fortran footer+header.
    header["version"] = unpack(endChar+'d',f.read(recLen))[0]

    (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    # Read grid size information.
    (header["nLons"],header["nLats"],header["nAlts"]) = unpack(endChar+'lll',f.read(recLen))
    (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    # Read number of variables.
    header["nVars"]=unpack(endChar+'l',f.read(recLen))[0]
    (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    # Collect variable names.
    for i in range(header["nVars"]):
        v = unpack(endChar+'%is'%(recLen),f.read(recLen))[0]
        header["vars"].append(v.decode('utf-8').replace(" ",""))
        (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    # Extract time.
    (yy,mm,dd,hh,mn,ss,ms)=unpack(endChar+'lllllll',f.read(recLen))
    header["time"].append(datetime(yy,mm,dd,hh,mn,ss,ms*1000))
    # print(header["time"][-1])

    f.close()

    return header

#-----------------------------------------------------------------------------
#
#-----------------------------------------------------------------------------

def read_gitm_headers():

    filelist = sorted(glob('./3DALL*.bin'))

    header = {}
    header["nFiles"] = len(filelist)
    header["version"] = 0
    header["nLons"] = 0
    header["nLats"] = 0
    header["nAlts"] = 0
    header["nVars"] = 0
    header["vars"] = []
    header["time"] = []
    header["filename"] = []

    for file in filelist:

        header["filename"].append(file)

        f=open(file, 'rb')

        # This is all reading header stuff:

        endChar='>'
        rawRecLen=f.read(4)
        recLen=(unpack(endChar+'l',rawRecLen))[0]
        if (recLen>10000)or(recLen<0):
            # Ridiculous record length implies wrong endian.
            endChar='<'
            recLen=(unpack(endChar+'l',rawRecLen))[0]

        # Read version; read fortran footer+header.
        header["version"] = unpack(endChar+'d',f.read(recLen))[0]

        (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

        # Read grid size information.
        (header["nLons"],header["nLats"],header["nAlts"]) = unpack(endChar+'lll',f.read(recLen))
        (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

        # Read number of variables.
        header["nVars"]=unpack(endChar+'l',f.read(recLen))[0]
        (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

        # Collect variable names.
        for i in range(header["nVars"]):
            v = unpack(endChar+'%is'%(recLen),f.read(recLen))[0]
            if (file == filelist[0]):
                header["vars"].append(v.decode('utf-8').replace(" ",""))
            (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

        # Extract time.
        (yy,mm,dd,hh,mn,ss,ms)=unpack(endChar+'lllllll',f.read(recLen))
        header["time"].append(datetime(yy,mm,dd,hh,mn,ss,ms*1000))
        # print(header["time"][-1])

        f.close()

    return header

#-----------------------------------------------------------------------------
#
#-----------------------------------------------------------------------------

def read_gitm_one_file(file_to_read, vars_to_read=-1):

    print("Reading file : "+file_to_read)

    data = {}
    data["version"] = 0
    data["nLons"] = 0
    data["nLats"] = 0
    data["nAlts"] = 0
    data["nVars"] = 0
    data["time"] = 0
    data["vars"] = []

    f=open(file_to_read, 'rb')

    # This is all reading header stuff:

    endChar='>'
    rawRecLen=f.read(4)
    recLen=(unpack(endChar+'l',rawRecLen))[0]
    if (recLen>10000)or(recLen<0):
        # Ridiculous record length implies wrong endian.
        endChar='<'
        recLen=(unpack(endChar+'l',rawRecLen))[0]

    # Read version; read fortran footer+data.
    data["version"] = unpack(endChar+'d',f.read(recLen))[0]

    (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    # Read grid size information.
    (data["nLons"],data["nLats"],data["nAlts"]) = unpack(endChar+'lll',f.read(recLen))
    (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    # Read number of variables.
    data["nVars"]=unpack(endChar+'l',f.read(recLen))[0]
    (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    if (vars_to_read[0] == -1):
        vars_to_read = np.arange[nVars]

    # Collect variable names.
    for i in range(data["nVars"]):
        data["vars"].append(unpack(endChar+'%is'%(recLen),f.read(recLen))[0])
        (oldLen, recLen)=unpack(endChar+'2l',f.read(8))

    # Extract time.
    (yy,mm,dd,hh,mn,ss,ms)=unpack(endChar+'lllllll',f.read(recLen))
    data["time"] = datetime(yy,mm,dd,hh,mn,ss,ms*1000)
    #print(data["time"])

    # Header is this length:
    # Version + start/stop byte
    # nLons, nLats, nAlts + start/stop byte
    # nVars + start/stop byte
    # Variable Names + start/stop byte
    # time + start/stop byte

    iHeaderLength = 8 + 4+4 + 3*4 + 4+4 + 4 + 4+4 + data["nVars"]*40 + data["nVars"]*(4+4) + 7*4 + 4+4

    nTotal = data["nLons"]*data["nLats"]*data["nAlts"]
    iDataLength = nTotal*8 + 4+4

    for iVar in vars_to_read:
        f.seek(iHeaderLength+iVar*iDataLength)
        s=unpack(endChar+'l',f.read(4))[0]
        data[iVar] = np.array(unpack(endChar+'%id'%(nTotal),f.read(s)))
        data[iVar] = data[iVar].reshape(
            (data["nLons"],data["nLats"],data["nAlts"]),order="F")

    f.close()

    return data

def extract_year(filename,pattern):
    match = re.search(pattern, filename)
    if match:
        year = int(match.group(2)[:2])  # Extract the first two digits as the year
        if year < 50:
            # For years less than 50, assume 20xx
            year += 2000
        else:
            # For years greater than or equal to 50, assume 19xx
            year += 1900
        return year
    else:
        return None

def extract_timestamp(filename,pattern):
    match = re.search(pattern, filename)
    if match:
        timestamp = datetime.strptime(str(extract_year(filename,pattern))+match.group(2)[2:]+ match.group(3),\
            '%Y%m%d%H%M%S')
        return timestamp
    else:
        return None
#-----------------------------------------------------------------------------
#
#-----------------------------------------------------------------------------

name_dict = {"Altitude":"Altitude",
                     "Ar Mixing Ratio":"Argon Mixing Ratio",
                     "Ar":"Ar Mass Density",
                     "CH4 Mixing Ratio":"Methane Mixing Ratio",
                     "Conduction":"Conduction", "EuvHeating":"EUV Heating",
                     "H":"H Mass Density", "H!U+!N":"H$^+$ Mass Density",
                     "H2 Mixing Ratio":"H$_2$ Mixing Ratio",
                     "HCN Mixing Ratio":"Hydrogen Cyanide Mixing Ratio",
                     "He":"He Mass Density", "He!U+!N":"He$^+$ Mass Density",
                     "Heating Efficiency":"Heating Efficiency",
                     "Heat Balance Total":"Heat Balance Total",
                     "Latitude":"Latitude", "Longitude":"Longitude",
                     "[N!D2!N]":"[N$_2$]",
                     "[N!D2!U+!N]":"[N$_2$$^+$]",
                     "[N!U+!N]":"[N$^+$]",
                     "[N(!U2!ND)]":"[N($^2$D)]",
                     "[N(!U2!NP)]":"[N($^2$P)]",
                     "[N(!U4!NS)]":"[N($^4$S)]",
                     "N2 Mixing Ratio":"N$_2$ Mixing Ratio",
                     "[NO]":"[NO]", "[NO!U+!N]":"[NO$^+$]",
                     "[O!D2!N]":"[O$_2$] ",
                     "[O(!U1!ND)]":"[O($^1$D)] ",
                     "[O!D2!U+!N]":"[O$_2$$^+$]",
                     "[O(!U2!ND)!]":"[O($^2$D)] ",
                     "[O(!U2!ND)!U+!N]":"[O($^2$D)] ",
                     "[O(!U2!NP)!U+!N]":"[O($^2$P)$^+$] ",
                     "[O(!U2!NP)!U+!N]":"[O($^2$P)] ",
                     "[O(!U3!NP)]":"[O($^3$P)] ",
                     "[O_4SP_!U+!N]":"[O($^4$SP)$^+$] ",
                     "RadCooling":"Radiative Cooling", "Rho":"Neutral Density",
                     "Temperature":"T$_n$", "V!Di!N (east)":"v$_{east}$",
                     "V!Di!N(north)":"v$_{north}$", "V!Di!N(up)":"v$_{up}$",
                     "V!Dn!N(east)":"u$_{east}$",
                     "V!Dn!N(north)":"u$_{north}$", "V!Dn!N(up)":"u$_{up}$",
                     "V!Dn!N(up,N!D2!N              )":"u$_{Up, N_2}$",
                     "V!Dn!N(up,N(!U4!NS)           )":"u$_{Up, N(^4S)}$",
                     "V!Dn!N(up,NO                  )":"u$_{Up, NO}$",
                     "V!Dn!N(up,O!D2!N              )":"u$_{Up, O_2}$",
                     "V!Dn!N(up,O(!U3!NP)           )":"u$_{Up, O(^3P)}$",
                     "e-":"[e-]",
                     "Electron_Average_Energy":"Electron Average Energy",
                     "eTemperature":"T$_e$", "iTemperature":"T$_i$",
                     "Solar Zenith Angle":"Solar Zenith Angle",
                     "Vertical TEC":"VTEC", "CO!D2!N":"CO$_2$ Mass Density",
                     "DivJu FL":"DivJu FL", "DivJuAlt":"DivJuAlt",
                     "Electron_Energy_Flux":"Electron Energy Flux",
                     "FL Length":"Field Line Length",
                     "Pedersen FL Conductance":"$\sigma_P$",
                     "Pedersen Conductance":"$\Sigma_P$",
                     "Hall FL Conductance":"$\sigma_H$",
                     "Potential":"Potential", "Hall Conductance":"$\Sigma_H$",
                     "Je2":"Region 2 Current", "Je1":"Region 1 Current",
                     "Ed1":"Ed1", "Ed2":"Ed2", "LT":"Solar Local Time",
                     "E.F. Vertical":"Vertical Electric Field",
                     "E.F. East":"Eastward Electric Field",
                     "E.F. North":"Northward Electric Field",
                     "E.F. Magnitude":"Electric Field Magnitude",
                     "B.F. Vertical":"Vertical Magnetic Field",
                     "B.F. East":"Eastward Magnetic Field",
                     "B.F. North":"Northward Magnetic Field",
                     "B.F. Magnitude":"Magnetic Field Magnitude",
                     "Magnetic Latitude":"Magnetic Latitude",
                     "Magnetic Longitude":"Magnetic Longitude",
                     "dLat":"Latitude", "dLon":"Longitude", "Gravity":"g",
                     "PressGrad (east)":r"$\nabla_{east}$ (P$_i$ + P$_e$)",
                     "PressGrad (north)":r"$\nabla_{north}$ (P$_i$ + P$_e$)",
                     "PressGrad (up)":r"$\nabla_{up}$ (P$_i$ + P$_e$)",
                     "IN Collision Freq":r"$\nu_{in}$",
                     "Chemical Heating":"Chemical Heating Rate",
                     "Total Abs EUV":"Total Absolute EUV",
                     "O Cooling":"O Cooling", "Joule Heating":"Joule Heating",
                     "Auroral Heating":"Auroral Heating",
                     "Photoelectron Heating":"Photoelectron Heating",
                     "Eddy Conduction":"Eddy Conduction",
                     "Eddy Adiabatic Conduction":"Adiabatic Eddy Conduction",
                     "NO Cooling":"NO Cooling",
                     "Molecular Conduction":"Molecular Conduction",
                     "[CO!D2!N]":"[CO$_2$]",
                     "[O]":"[O]",
                     "[O!D2!N]":"[O$_2$]",
                     "[e-]":"[e-]",
                     "[CO]":"[CO]",
                     "[N!D2!N]":"[N$_2$]",
                     "[Ar]":"[Ar]",
                     "[NO]":"[NO]",
                     
                     }