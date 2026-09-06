import glob
import paraview.simple as pvs
import sys
import time
import re
import os
from amrex2tec import amrex2tec
from clean_dat import clean_dat

# Q: How to fix "no module named paraview" error?
# A: Add paraview python package location to $PYTHONPATH. For example:
#    export PYTHONPATH=$PYTHONPATH:/usr/lib/python3.10/site-packages

def tec2vtk(datPath):
    # The performance of converting a 6G .dat file
    # None: fastest write.        speed: fail    file size: fail
    # LZ4: fastest compressed.    speed: 60s        file size: 1.4G
    # ZLib: balanced              speed: 96s        file size: 1.0G
    # LZMA: smallest file size    speed: 455s       file size: 0.84G
    compressor = "ZLib"

    tStart = time.time()

    line = clean_dat(datPath)

    datFile = datPath.split('/')[-1]
    dat = pvs.TecplotReader(registrationName=datFile, FileNames=datPath)

    vtkPath = datPath.replace('.dat', '_vtk')+"/"

    pvs.SaveData(vtkPath+datFile.split(".")[0]+".vtm", proxy=dat,
                 PointDataArrays=dat.PointData.keys(),
                 CompressorType=compressor, CompressionLevel='5')

    with open(vtkPath+"unit.txt", 'w') as f:
        f.write(line)

    tEnd = time.time()
    print("tec2vtk conversion takes {0} s".format(tEnd-tStart), flush=True)
    return vtkPath

######################################################################


def amrex2vtk(datPath):
    tStart = time.time()

    tecPath = amrex2tec(datPath)
    vtkPath = tec2vtk(tecPath)
    os.system("rm -rf "+tecPath)

    tEnd = time.time()
    print("amrex2vtk conversion takes {0} s".format(tEnd-tStart), flush=True)


if __name__ == "__main__":
    files = sys.argv[1:]
    for f in files:
        if f.find(".dat") >= 0:
            print("\n Converting ", f, flush=True)
            tec2vtk(f)
        else:
            print("\nConverting ", f, flush=True)
            amrex2vtk(f)
