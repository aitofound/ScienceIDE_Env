import os
import time
import sys


def amrex2tec(datPath, savePlt=False):
    tStart = time.time()

    fleksDir = __file__.replace("/tools/amrex2tec.py", "")
    binPath = fleksDir+"/bin/converter.exe"
    if not os.path.exists(binPath):
        os.system("cd "+fleksDir+"; make CONVERTER > /dev/null")

    tecPath = datPath+".dat"
    pltPath = datPath+".plt"

    if (not os.path.exists(tecPath) and not os.path.exists(pltPath) and
        os.system(binPath + " " + datPath + " > /dev/null") == -1):
        print("amrex2tec conversion failed for file ", datPath, flush=True)

    if savePlt:
        if not os.path.exists(pltPath) and os.system("preplot " + tecPath + " > /dev/null") == -1:
            print("tecplot preplot failed for file ", tecPath, flush=True)

        os.system("rm " + tecPath)

    tEnd = time.time()
    print("amrex2tec conversion takes {0} s".format(tEnd-tStart), flush=True)

    return tecPath


if __name__ == "__main__":
    flag = int(sys.argv[1])
    files = sys.argv[2:]
    for f in files:
        print("\nConverting ", f, flush=True)
        amrex2tec(f, flag != 0)
