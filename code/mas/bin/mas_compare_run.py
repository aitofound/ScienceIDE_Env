#!/usr/bin/env python
# mas_compare_run.py

# imports used
import sys
import numpy as np
import argparse
import os
from psi_io import rdhdf

# Get rid of warning when nan or zero value in divisor
np.seterr(divide='ignore', invalid='ignore')


# Python script to compare the results of two MAS runs at a single time-step.
# If any components of a vector are specified to compare, the 2D or 3D vectore
# Magnitudes are compared as well.

long_file_seq=0

def argParsing():

    parser = argparse.ArgumentParser(description='')

    parser.add_argument('runname',
                        type=str,
                        help="Name of the mas run (input file i<runname>).")

    parser.add_argument('new_rundir',
                        type=str,
                        help="Name of the directory that contains the results of new MAS runs")

    parser.add_argument('ref_rundir',
                        type=str,
                        help="Name of the directory that contains the results of reference MAS runs")

    parser.add_argument('fieldlist',
                        type=str,
                        help="The fields that we would like to compare between the two MAS runs")

    parser.add_argument('idx',
                        type=str,
                        help="The time-step of the runs we would like to compare")

    parser.add_argument('-long_file_seq',
                        default=0,
                        required=False,                        
                        help="longfileseq")

    if len(sys.argv) < 5:
        print("Usage:  mas_compare_run runname new_rundir ref_rundir fieldlist idx [-long_file_seq]\n")
        print("Dirs have no end slash, comma-seperated field list (vr,vt,rho,...)\n")
        quit()

    return parser.parse_args()

# Define comparison functions::::::::::::::::::::::::
# x is new run, y is ref run


def getdiff_rmsd(x,y):
    N = np.size(x)
    d = np.sqrt(np.sum((x - y) ** 2)/N)
    return d

def getdiff_hhabs(x,y):
    top = np.sum((x - y) ** 2)
    bot = np.sum(np.abs(y)*np.abs(x))
    if bot == 0:
        d = -1
    else:
        d = np.sqrt(top/bot)
    return d

def getdiff_maxabs(x, y):
    d = np.max(np.abs(x - y))
    return d


def getdiff_cvrmsd(x, y):
    top = np.sum((x - y) ** 2)
    bot = np.sum(y ** 2)
    if bot == 0:
        d = -1
    else:
        d = np.sqrt(top/bot)
    return d


def getdiff_mape(x, y):
    N = np.size(x)
    d = np.sum(np.abs((x - y)/y))/N
    if d == float('inf') or np.isnan(d):
        d = -1
    return d


def getdiff_maxape(x, y):
    N = np.size(x)
    d = np.max(np.abs((x - y)/y))
    if d == float('inf') or np.isnan(d):
        d = -1
    return d


# End of comparison Functions -------------------------------------------------------------------------
args = argParsing()

RUNNAME = args.runname
RUNDIR1 = args.new_rundir # New
RUNDIR2 = args.ref_rundir # Ref

fieldlist = args.fieldlist.split(',')
idx = int(args.idx)
N = len(sys.argv)

ext='h5'

CALLDIR = os.getcwd()

#---------------------------------------------------
# Start the comparisons:
#---------------------------------------------------

newRunDirList = os.listdir(RUNDIR1)

refRunDirList = os.listdir(RUNDIR2)

if args.long_file_seq == 0:
    idx_str = "%03d" % idx
else:
    idx_str = "%06d" % idx
fnstr = idx_str + "."+ext # Used for vector magnitudes
fidx = 0


fieldlistsDict = {}

# Iterate through fieldList to store each quantity in its corresponding vector

for field in fieldlist:
    if field[0] in fieldlistsDict and len(field) == 2:
        fieldlistsDict[field[0]].append(field)
    elif field[0] in "abjv" and len(field) == 2:
        fieldlistsDict[field[0]] = [field]

# Initialize results matrix:
Nresults = len(fieldlist) + len(fieldlistsDict)
results = np.full((Nresults, 6), 0.0)

# Iterate through the fields and compute differences between reference and the run
for field in fieldlist:
    if field in ["heatflux", "br_photo", "potfld_psi","potfld_psi0"]:
        fname = field
    else:
        fname = field + idx_str
    fname_h5 = fname + "."+ext
    print(fname_h5)
    _, _, _, fieldData1 = rdhdf(RUNDIR1 + "/" + fname_h5)
    fieldData1 = np.ndarray.flatten(fieldData1)
    _, _, _, fieldData2 = rdhdf(RUNDIR2 + "/" + fname_h5)
    fieldData2 = np.ndarray.flatten(fieldData2)
    err_cvrmsd = getdiff_cvrmsd(fieldData1, fieldData2)
    err_rmsd = getdiff_rmsd(fieldData1, fieldData2)
    err_mape = getdiff_mape(fieldData1, fieldData2)
    err_maxabs = getdiff_maxabs(fieldData1, fieldData2)
    err_maxape = getdiff_maxape(fieldData1, fieldData2)
    err_hhabs = getdiff_hhabs(fieldData1, fieldData2)

    results[fidx, 0] = err_hhabs
    results[fidx, 1] = err_rmsd
    results[fidx, 2] = err_maxabs
    results[fidx, 3] = err_cvrmsd
    results[fidx, 4] = err_mape
    results[fidx, 5] = err_maxape
    fidx += 1


# Compute the necessary Magnitude vectors
for key in sorted(fieldlistsDict.keys()):
    fieldlist.append("|%s|" % key.upper() )
    dim1 = dim2 = dim3 = sys.maxsize
    for component in fieldlistsDict[key]:
        path = RUNDIR1 + "/" + component + fnstr
        _, _, _, vvec = rdhdf(path)
        dim1 = min(dim1, vvec.shape[0])
        dim2 = min(dim2, vvec.shape[1])
        if len(vvec.shape) == 3:
            dim3 = min(dim3, vvec.shape[2])
        else:
            dim3 = 0

    # Initialize vector magnitude arrays:
    if dim3 == 0:
        vmag1 = np.full((dim1, dim2), 0.0)
    else:
        vmag1 = np.full((dim1, dim2, dim3), 0.0)
    vmag2 = vmag1

    # read in components, interp, and add square to mag:
    for component in fieldlistsDict[key]:
        path = RUNDIR1 + '/' + component + fnstr
        _, _, _, vvec = rdhdf(path)
        n1 = vvec.shape[0]
        n2 = vvec.shape[1]
        if dim3 != 0:
            n3 = vvec.shape[2]
        if dim3 == 0:
            if n1 == dim1 + 1:
                vvec = 0.5 * (vvec[0:n1 - 1, 0:n2] + vvec[1:n1, 0:n2])
                n1 = dim1
            if n2 == dim2 + 1:
                vvec = 0.5 * (vvec[0:n1, 0:n2 - 1] + vvec[0:n1, 1:n2])
                n2 = dim2
        else:
            if n1 == dim1 + 1:
                vvec = 0.5 * (vvec[0:n1 - 1, 0:n2, 0:n3] + vvec[1:n1, 0:n2, 0:n3])
                n1 = dim1
            if n2 == dim2 + 1:
                vvec = 0.5 * (vvec[0:n1, 0:n2 - 1, 0:n3] + vvec[0:n1, 1:n2, 0:n3])
                n2 = dim2
            if n3 == dim3 + 1:
                vvec = 0.5 * (vvec[0:n1, 0:n2, 0:n3 - 1] + vvec[0:n1, 0:n2, 1:n3])
                n3 = dim3

        # Add square of component to magnitude matrix
        vmag1 = vmag1 + (vvec ** 2)
    vmag1 = np.sqrt(vmag1)

    #  Now get the magnitude vectors of the reference
    for component in fieldlistsDict[key]:
        path = RUNDIR2 + '/' + component + fnstr
        _, _, _, vvec = rdhdf(path)
        n1 = vvec.shape[0]
        n2 = vvec.shape[1]
        if dim3 != 0:
            n3 = vvec.shape[2]
        if dim3 == 0:
            if n1 == dim1 + 1:
                vvec = 0.5 * (vvec[0:n1 - 1, 0:n2] + vvec[1:n1, 0:n2])
                n1 = dim1
            if n2 == dim2 + 1:
                vvec = 0.5 * (vvec[0:n1, 0:n2 - 1] + vvec[0:n1, 1:n2])
                n2 = dim2
        else:
            if n1 == dim1 + 1:
                vvec = 0.5 * (vvec[0:n1 - 1, 0:n2, 0:n3] + vvec[1:n1, 0:n2, 0:n3])
                n1 = dim1
            if n2 == dim2 + 1:
                vvec = 0.5 * (vvec[0:n1, 0:n2 - 1, 0:n3] + vvec[0:n1, 1:n2, 0:n3])
                n2 = dim2
            if n3 == dim3 + 1:
                vvec = 0.5 * (vvec[0:n1, 0:n2, 0:n3 - 1] + vvec[0:n1, 0:n2, 1:n3])
                n3 = dim3
        vmag2 = vmag2 + (vvec ** 2.0)
    vmag2 = np.sqrt(vmag2)

    # Compute differences between the reference magnitude vector and the run magnitude vector
    err_cvrmsd = getdiff_cvrmsd(vmag1, vmag2)
    err_rmsd = getdiff_rmsd(vmag1, vmag2)
    err_mape = getdiff_mape(vmag1, vmag2)
    err_maxabs = getdiff_maxabs(vmag1, vmag2)
    err_maxape = getdiff_maxape(vmag1, vmag2)
    err_hhabs = getdiff_hhabs(vmag1, vmag2)

    results[fidx, 0] = err_hhabs
    results[fidx, 1] = err_rmsd
    results[fidx, 2] = err_maxabs
    results[fidx, 3] = err_cvrmsd
    results[fidx, 4] = err_mape
    results[fidx, 5] = err_maxape
    fidx += 1

# Output formatted resulting values
fOut = RUNNAME + "_solution-compare.txt"

out = "=================================================================================================================\n"
out += "%-11s%17s%17s%17s%17s%17s%17s\n" % ("Field", "HHabs", "RMSD", "MAX[|X-Y|]", "MCV(RMSD)", "MEAN[|(X-Y)/Y|]", "MAX[|(X-Y)/Y|]")
out += "=================================================================================================================\n"

numberOfMags = 0
for field in fieldlist:
    if field.count("|") == 2:
        numberOfMags += 1
listToSort = fieldlist[-numberOfMags:]
listToSort.sort()
fieldlist = fieldlist[:len(fieldlist) - numberOfMags]
fieldlist.extend(listToSort)
for i in range(Nresults):
    out += "%11s" % fieldlist[i]
    for j in range(6):
        if results[i, j] != -1 and results[i, j] != 0:
            out += "   %.8e" % results[i, j]
        else:
            out += "   %14d" % results[i, j]
    out += "\n"

with open(fOut, "w") as f:
    f.write(out)

print("==> Done! Comparison saved in file:")
print("==>       %s" % fOut)






