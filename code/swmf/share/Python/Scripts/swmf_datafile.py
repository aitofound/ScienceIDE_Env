#!/usr/bin/env python3
import numpy as np

# Read and write SWMF data files in ascii, real4 and real8 formats
# After reading the data dictionary contains
#   "head": headline string
#   "step": time step (integer)
#   "time": time (real)
#   "ndim": number of coordinates (1, 2, or 3)
#   "cart": True for Cartesian, False for non-Cartesian grid (ndim<0 in file)
#   "npar": number of scalar parameters (0, 1, ...)
#   "nvar": number of variables in the state (1, 2, ...)
#   "dims": array of ndim integers giving the grid dimension (Python order)
#   "pars": array of npar reals with scalar parameters (if npar > 0)
#   "name": coordinate, variable and parameter names separated by spaces
#   "coord": coordinate array (ndim,ngrid)
#   "state": state array (nvar,ngrid)
#   "last": snapshot index (starting from 1) if this was the last snapshot
#
# For writing, the data dictionary must contain "name", "coord", and "state".
# It is recommended to always set "head" (show_data gives a warning).
# The "pars" is optional. If "dims" is present it is used and checked
# against the size of "coord" (but the shape can be changed).
# The "cart" can be set to False to indicate non-Cartesian grid.

###############################################################################
def file_format(filename):

    # Return the file format

    f = open(filename, 'rb')
    # check if first four characters look like a record length
    reclen = int.from_bytes(f.read(4),'little')
    if reclen != 79 and reclen != 500:
        f.close()
        return "ascii"
    # skip the head line
    f.read(reclen+4)
    reclen = int.from_bytes(f.read(4),'little')
    f.close()
    if reclen == 20:
        return "real4"
    elif reclen == 24:
        return "real8"
    else:
        return "ascii"
###############################################################################
def show_data(data):

    # Show information stored in "data"

    if "last" in data:
        print('last=', data["last"])
    if "head" in data:
        print('head=', data["head"])
    else:
        print('WARNING in show_data: missing "head" string')
    if "step" in data:
        print('step=', data["step"])
    if "time" in data:
        print('time=', data["time"])
    if "dims" in data:
        print('dims=',data["dims"])
    if "cart" in data:
        print("cart=", data["cart"])
    if "pars" in data:
        print('pars=', data["pars"])
    if "name" in data:
        print('name=', data["name"])
    else:
        print('ERROR in show_data: missing "name" string')
    if "coord" in data:
        print('coord.shape=', np.shape(data["coord"]))
    else:
        print('ERROR in show_data: missing "coord" array')
    if "state" in data:
        print('state.shape=', np.shape(data["state"]))
    else:
        print('ERROR in show_data: missing "state" array')
###############################################################################
def read_file(fileid, fileformat='unknown', skip=0, size=False, verbose=False):

    # Read data from fileid and return data dictionary or size.
    # The fileid can be a filename string or a file object.
    # For a filename start from the beginning, for fielid start
    # at the current position.
    # If size is set to true, return the number of snapshots in the file.
    # Otherwise skip "skip" snapshots (default is 0) and
    # read the next snapshot and return a dictionary with the content.

    # Use fileformat if set, otherwise figure it out
    if fileformat == 'unknown':
        if type(fileid) is str:
            fileformat = file_format(fileid)
        else:
            fileformat = "binary" if 'b' in fileid.mode else "ascii" 

    # Set file object
    if type(fileid) is str:
        if fileformat == "ascii":
            f = open(fileid, 'r') # add error handling here
        else:
            f = open(fileid, 'rb')
    else:
        f = fileid

    # Read snapshot(s) from f
    i = 0
    while i <= skip or size:
        if fileformat == "ascii":
            data = read_ascii(f)
        else:
            data = read_binary(f)
        i = i+1
        # check if we are at the end of the file
        pos = f.tell()
        if f.read(1):
            # reset the position
            f.seek(pos)
        else:
            data["last"] = i
            break

    # Close file if it was opened above or we reached the end for size
    if size or type(fileid) is str:
        f.close()

    if verbose:
        if type(fileid) is str:
            print('filename=', fileid)
        print('fileformat=', fileformat)
        show_data(data)

    if size:
        return data["last"]
    else:
        return data
###############################################################################
def write_file(data, filename="swmfdata.out", format="18.10e", append=False):

    # Write data into the SWMF file "filename" with format "format"

    if format == 'real4' or format == 'real8':
        write_binary(data, filename, format, append)
    else:
        write_ascii(data, filename, format, append)
###############################################################################
def read_ascii(f):

    # Read ASCII SWMF file f and return a dictionary with the content
    # If end of file is reached set the "last" field in the dictionary True.

    head = f.readline()[:-1]
    step, time, ndim, npar, nvar = f.readline().split()
    step = np.int32(step)
    time = np.float64(time)
    ndim = np.int32(ndim)
    cart = ndim > 0 # Negative ndim is for non-cartesian
    ndim = abs(ndim)
    npar = np.int32(npar)
    nvar = np.int32(nvar)
    dims = np.flip(np.array(f.readline().split(), dtype=np.int32))
    if npar > 0:
        pars = np.array(f.readline().split(), dtype=np.float64)
    name = f.readline()[:-1]

    # read ngrid lines into a string
    ngrid = np.prod(dims)
    line = ""
    for i in range(ngrid):
        line += f.readline()
    griddata = np.array(line.split(), dtype=np.float64)
    # reshape and transpose array to match default Python ordering
    griddata = griddata.reshape(ngrid, ndim+nvar).T

    # extract coordinates and state and reshape with dims
    coord = griddata[:ndim,:].reshape([ndim]+list(dims))
    state = griddata[ndim:,:].reshape([nvar]+list(dims))

    # Create dictionary
    data ={"head"   : head,
           "step"   : step,
           "time"   : time,
           "ndim"   : ndim,
           "dims"   : dims,
           "cart"   : cart,
           "npar"   : npar,
           "nvar"   : nvar,
           "name"   : name,
           "coord"  : coord,
           "state"  : state}
    if npar > 0:
        data["pars"] = pars 

    return data
###############################################################################
def read_binary(f):

    # Read binary SWMF file "fileid" and return a dictionary with the content.
    # If end of file is reached set the "last" field in the dictionary True.

    stringlength = int.from_bytes(f.read(4),'little')
    head = f.read(stringlength).decode().rstrip()

    f.read(4) # skip markers
    len2 = int.from_bytes(f.read(4),'little')
    if len2 == 20:
        # single precision reals
        nreal = 4
        dtype = np.float32
    else:
        # double precision reals
        nreal = 8
        dtype = np.float64
        
    step = int.from_bytes(f.read(4),'little')
    time = np.frombuffer(f.read(nreal), dtype=dtype)[0]
    ndim = np.frombuffer(f.read(4), dtype=np.int32)[0]
    cart = ndim > 0
    ndim = abs(ndim)
    npar = int.from_bytes(f.read(4),'little')
    nvar = int.from_bytes(f.read(4),'little')
    f.read(8) # skip markers
    dims = np.flip(np.frombuffer(f.read(4*ndim), dtype=np.int32))
    f.read(8) # skip markers
    if npar > 0:
        pars = np.frombuffer(f.read(nreal*npar), dtype=dtype)
        f.read(8) # skip markers
    name = f.read(stringlength).decode().rstrip()
    f.read(8) # skip markers
    shape = [ndim]+list(dims)
    ngrid = np.prod(dims)
    coord = np.frombuffer(f.read(nreal*ngrid*ndim),
                          dtype=dtype).reshape([ndim]+list(dims))
    state = np.empty((nvar, ngrid), dtype=dtype)
    for i in range(nvar):
        f.read(8) # skip markers
        state[i,:] = np.frombuffer(f.read(nreal*ngrid), dtype=dtype)

    f.read(4) # skip last marker

    # Reformat state based on dims
    state = state.reshape([nvar]+list(dims))

    # Create dictionary
    data ={"head"   : head,
           "step"   : step,
           "time"   : time,
           "ndim"   : ndim,
           "cart"   : cart,
           "dims"   : dims,
           "npar"   : npar,
           "nvar"   : nvar,
           "name"   : name,
           "coord"  : coord,
           "state"  : state}
    if npar > 0:
        data["pars"] = pars

    return data
###############################################################################
def fortran_string(string, l):
    # Create bytestring of length l with spaces added at the end
    if len(string) > l:
        return string.encode()[:l]
    else:
        return string.encode() + b' '*(l - len(string))
###############################################################################
def fortran_record(bytearray):
    # Write a Fortran record with 4-byte markers at both ends
    reclen = np.int32(len(bytearray)).tobytes()
    return reclen + bytearray + reclen
###############################################################################
def get_dims(coord):
    # Get the grid dimensions from the coordinate array
    dims = list(coord.shape)
    if coord.ndim > 1:
        dims = dims[1:] # remove the first index
    return dims
###############################################################################
def write_ascii(data, filename="swmfdata.out", format="18.10e", append=False):

    # Write data into ascii file filename using format for real numbers.
    if not "name" in data or not "state" in data or not "coord" in data:
        print("ERROR in write_ascii: missing name, state or coord in data")
        print("Could not write file", filename)
        return 1

    # Default ASCII format
    if format == "ascii":
        format = "18.10e"
    
    if append:
        f = open(filename,'a')
    else:
        f = open(filename,'w')

    # required information
    name = data["name"]
    coord = np.float64(data["coord"])
    state = np.float64(data["state"])

    # optional information
    head = data["head"] if "head" in data else "Missing head"
    dims = np.int32(data["dims"] if "dims" in data else get_dims(coord))
    step = data["step"] if "step" in data else 0
    time = data["time"] if "time" in data else 0.0
    pars = np.float64(data["pars"]) if "pars" in data else []

    # Derived information
    ndim = len(dims)
    npar = len(pars)
    nvar = len(name.split()) - ndim - npar
    
    # Check consistency
    ngrid = np.prod(dims) # number of grid points
    if coord.size != ngrid*ndim:
        print("ERROR in write_ascii: incorrect size of coord=", coord.size)
        print('dims=', dims, 'ndim=', ndim,' ngrid=',ngrid)
        exit(1)

    if state.size != ngrid*nvar:
        print("ERROR in write_ascii: incorrect size of state=", state.size)
        print('dims=', dims, 'ndim=', ndim,' ngrid=', ngrid)
        print("name=", name," npar=", npar)
        exit(1)

    coord = coord.reshape(ndim,ngrid)
    state = state.reshape(nvar,ngrid)
        
    float_formatter = "{:"+format+"}"
    np.set_printoptions(formatter={'float_kind':float_formatter.format},linewidth=np.inf)
    f.writelines(head+"\n")
    template = "{0:d} {1:"+format+"} {2:d} {3:d} {4:d}\n"
    if "cart" in data and not data["cart"]:
        ndim = -ndim # negative ndim for non-Cartesian grid
    f.write(template.format(step, time, ndim, npar, nvar))
    f.write(str(np.flip(dims))[1:-1]+"\n")
    template = "{:18.10e}"
    if npar > 0:
        f.write(str(pars)[1:-1]+"\n")
    f.writelines(name+"\n")
    for i in range(ngrid):
        f.write(str(coord[:,i])[1:-1] + str(state[:,i])[1:-1] + "\n")

    f.close()
###############################################################################
def write_binary(data, filename="swmfdata.out", format="real4", append=False):

    # Write data in binary format (real4 or real8) into file filename.

    if not "name" in data or not "state" in data or not "coord" in data:
        print("ERROR in write_binary: missing name, state or coord in data")
        print("Could not write file", filename)
        return 1

    if append:
        f = open(filename,'ab')
    else:
        f = open(filename,'wb')

    # required
    name  = data["name"]
    coord = data["coord"]
    state = data["state"]

    # optional
    time = data["time"] if "time" in data else 0.0
    pars = data["pars"] if "pars" in data else []
    npar = np.int32(len(pars))

    # convert to real4 / real8 as needed
    if format == "real8":
        if not isinstance(coord, np.ndarray) or coord.dtype != "float64":
            coord = np.float64(coord)
        if not isinstance(state, np.ndarray) or state.dtype != "float64":
            state = np.float64(state)
        time = np.float64(time)
        if npar > 0:
            pars = np.float64(pars)
    else:
        if not isinstance(coord, np.ndarray) or coord.dtype != "float32":
            coord = np.float32(coord)
        if not isinstance(state, np.ndarray) or state.dtype != "float32":
            state = np.float32(state)
        time = np.float32(time)
        if npar > 0 and pars.dtype != "float32":
            pars = np.float32(pars)

    head = data["head"] if "head" in data else "Missing head"
    dims = np.int32(data["dims"] if "dims" in data else get_dims(coord))
    step = np.int32(data["step"] if "step" in data else 0)

    ndim = np.int32(len(dims))
    nvar = np.int32(len(name.split()) - ndim - npar)

    # Check consistency
    ngrid = np.prod(dims)
    if coord.size != ngrid*ndim:
        print("ERROR in write_binary: incorrect size of coord=", coord.size)
        print('dims=', dims, 'ndim=', ndim,' ngrid=',ngrid)
        exit(1)

    if state.size != ngrid*nvar:
        print("ERROR in write_binary: incorrect size of state=", state.size)
        print('dims=', dims, 'ndim=', ndim,' ngrid=', ngrid)
        print("name=", name," npar=", npar)
        exit(1)

    # Reshape state to make writing easy
    state = state.reshape(nvar,ngrid)
    coord = coord.reshape(ndim,ngrid)

    # write data into the file
    stringlength = 79 if len(head) < 80 and len(name) < 80 else 500
    f.write(fortran_record(fortran_string(head, stringlength)))

    if "cart" in data and not data["cart"]:
        ndim = -ndim # negative ndim for non-Cartesian grid
    f.write(fortran_record(step.tobytes()
                           + time.tobytes()
                           + ndim.tobytes()
                           + npar.tobytes()
                           + nvar.tobytes()))
    f.write(fortran_record(np.flip(dims).tobytes()))
    if npar > 0:
        f.write(fortran_record(pars.tobytes()))
    f.write(fortran_record(fortran_string(name, stringlength)))
    f.write(fortran_record(coord.tobytes()))
    for i in range(nvar):
        f.write(fortran_record(state[i,:].tobytes()))
    f.close()
###############################################################################
def convert_data():

    import argparse

    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawTextHelpFormatter, description="""
    swmf_datafile.py can be imported into a python code to read and write
    ascii and binary SWMF data files efficiently. See swmf_datafile_test.py.
    Alternatively, it can be used to examine the content, select snapshots
    and convert the format of SWMF data files.""",
    epilog="""
examples:
  Check the content of file.outs:
    swmf_datafile.py file.outs
  Show the header info for the first snapshot:
    swmf_datafile.py -v -s 0 file.outs
  Extract the first snapshot and then every 10th from 30th to 200th:
    swmf_datafile.py -s 0,30:200:10 file.outs selected.outs
  Convert the file into an ascii file with 3 digit floats:
    swmf_datafile.py -f 10.3f file.outs ascii.out
  Convert the file into a single precision binary file:
    swmf_datafile.py -f real4 file.outs singleprec.outs
  Extract the last two snapshots (note = sign!) and convert to real8 binary:
    swmf_datafile.py -f real8 -s=-2: file.outs doubleprec.out
    """)
    parser.add_argument('inputfile',
                        help="Input SWMF data file, required")
    parser.add_argument('outputfile', nargs="?",
                        help="Output SWMF data file, optional")
    parser.add_argument("-s", "--select",
                        help="select snapshots (default is all)")
    parser.add_argument("-f", "--format",
                        help="format of output file (default is same as input file)")
    parser.add_argument("-v", "--verbose", action='store_true',
                        help="verbose output")
    args = parser.parse_args()
    filein  = args.inputfile
    fileout = args.outputfile
    selection = args.select
    formatout = args.format
    verbose = args.verbose
    if fileout == None and formatout is not None:
        parser.error("-f FORMAT option requires an output file")
    if selection is None and formatout is None and fileout is not None:
        parser.error("Provide -f FORMAT and/or -s=SELECT for output file")

    npictin  = read_file(filein, size=True)
    formatin = file_format(filein)

    if selection is not None:
        # process the selection string
        savepict = np.full(npictin, False)
        for part in selection.split(","):
            parts = part.split(":")
            start = int(parts[0]) if len(parts[0]) > 0 else 0
            if start < 0:
                start += npictin
            end   = start+1 # default
            step  = 1       # default
            if len(parts) > 1:
                end = int(parts[1]) if len(parts[1]) > 0 else npictin
                if end < 0:
                    end += npictin
            if len(parts) > 2:
                step = abs(int(parts[2])) if len(parts[2]) > 0 else 1
            savepict[start:end:step] = True
    else:
        # Save all snapshots
        savepict = np.full(npictin, True)

    if formatout is None:
        formatout = formatin

    npictout = np.sum(savepict)
    if npictout == 0:
        print("No snapshot was selected from file", filein,
              " containing", npictin, "snapshot(s)")
    else:
        if formatin == "ascii":
            f = open(filein,"r")
        else:
            f = open(filein,"rb")
        print("Begin reading    ", filein,"of format", formatin,
              "with", npictin, "snapshot(s)")

        append = False
        skip = 0
        for ipict in range(npictin):
            if savepict[ipict]:
                if verbose:
                    print("------ Snaphot", ipict,"----------")
                data = read_file(f, formatin, skip=skip, verbose=verbose)
                if not verbose:
                    print("------ Snapshot", ipict, "time=", data["time"])
                if fileout is not None:
                    write_file(data, fileout, format=formatout, append=append)
                    append = True
                skip = 0
            else:
                skip += 1
        f.close
        if fileout is None:
            print("Finsihed reading", npictout, "snapshots from", filein)
        else:
            print("Finished writing ",fileout,"of format", formatout,
                  "with", npictout, "snapshot(s)")

###############################################################################
if __name__ == "__main__":
    convert_data()
