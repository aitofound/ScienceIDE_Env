#!/usr/bin/env python3
import swmf_datafile as swmf

# minimal data
print('=====================================================================')
print("Creating a minimal data dictionary with 2D grid")
data = {
    "name": "longitude latitude dBN dBE",
    "coord": [[[1.,2.,3.,4.],
               [1.,2.,3.,4.],
               [1.,2.,3.,4.]],
              [[1.,1.,1.,1.],
               [2.,2.,2.,2.],
               [3.,3.,3.,3.]]],
    "state": [[[10.,11.,12.,13.],
               [100.,110.,120.,130.],
               [1000.,1100.,1200.,1300]],
              [[20.,21.,22.,23.],
               [200.,210.,220.,230.],
               [2000.,2100.,2200.,2300]]]
}
swmf.show_data(data)

print('=====================================================================')
print("Write ASCII file with 3 decimals")
fileout = 'file_ascii.outs'
swmf.write_file(data, fileout, format='10.3f')
print("Add another snapshot with time=1")
data["time"] = 1.0
swmf.write_file(data, fileout, format='10.3f', append=True)
print('Wrote 2 snapshots into ', fileout)

print('=====================================================================')
print("Write single precision binary file")
fileout = 'file_real4.out'
swmf.write_file(data, fileout, "real4")
print("Wrote out ", fileout)

print('=====================================================================')
print("Add head line,  dimensions, parameters to data")
data["head"] = "Proper header"
data["cart"] = False
data["dims"] = [1, 12] # typical for AMR grid
data["pars"] = [0.1, 0.2, 0.3, 0.4, 0.5] # add 5 scalar parameters
# New data name is more than 79 characters
data["name"] += " ParameterName1 ParameterName2 ParameterName3 ParameterName4 ParameterName5"
print("Write double precision binary file")
fileout = 'file_real8.outs'
swmf.write_file(data, fileout, "real8")
print("Add another snapshot with time=2")
data["time"] = 2.0
swmf.write_file(data, fileout, "real8", append=True)
print('Wrote 2 snapshots into ', fileout)

print('=====================================================================')
filein = 'file_ascii.outs'
print("Read", filein, "of type", swmf.file_format(filein),
      "with", swmf.read_file(filein, size=True),"snapshot(s)")
print("Read first snapshot -------------------------------------------------")
data2 = swmf.read_file(filein, verbose=True)
print("Read second snapshot ------------------------------------------------")
data2 = swmf.read_file(filein, skip=1, verbose=True)

print('=====================================================================')
filein = 'file_real4.out'
print("Read", filein, "of type", swmf.file_format(filein),
      "with", swmf.read_file(filein, size=True),"snapshot(s)")
data2 = swmf.read_file(filein)
swmf.show_data(data2)
    
print('=====================================================================')
filein = 'file_real8.outs'
print("Read", filein, "of type", swmf.file_format(filein),
      "with", swmf.read_file(filein, size=True),"snapshot(s)")
f = open(filein,'rb')
print('Reading  ', filein)
data2 = swmf.read_file(f)
print("time=", data2["time"])
print('Read next snapshot --------------------------------------------------')
data3 = swmf.read_file(f)
print("time=", data3["time"], "last=", data3["last"])
f.close
print('Read the second snapshot only ---------------------------------------')
data3 = swmf.read_file(filein, skip=1, verbose=True)
print('=====================================================================')
print("Creating a data dictionary with 1D grid and 1 variable")
data = {
    "head": "1D data",
    "name": "longitude dBH",
    "coord": [ 1., 2., 3., 4.],
    "state": [10.,11.,12.,13.]}
swmf.show_data(data)
print('=====================================================================')
fileout = 'file1d_ascii.outs'
swmf.write_file(data, fileout, format='10.3f')
print("Add another snapshot with time=1")
data["time"] = 1.0
swmf.write_file(data, fileout, format='10.3f', append=True)
print('Wrote 2 snapshots into ', fileout)
print('=====================================================================')
print("Write single precision binary file")
fileout = 'file1d_real4.outs'
swmf.write_file(data, fileout, "real4")
print("Add another snapshot with time=2")
data["time"] = 2.0
swmf.write_file(data, fileout, "real4", append=True)
print('Wrote 2 snapshots into ', fileout)
print('=====================================================================')
for filein in ['file1d_ascii.outs', 'file1d_real4.outs']:
    #print("Read", filein, "of type", swmf.file_format(filein),
    #      "with", swmf.read_file(filein, size=True),"snapshot(s)")
    print("Read first snapshot -------------------------------------------------")
    data2 = swmf.read_file(filein, verbose=True)
    print("Read second snapshot ------------------------------------------------")
    data2 = swmf.read_file(filein, skip=1, verbose=True)
    print('=====================================================================')
