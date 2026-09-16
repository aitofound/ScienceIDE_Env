"""
Routines for reading/writing PSI style HDF5 and HDF4 data files.

- Use rdhdf_1d, rdhdf_2d, and rdhdf_3d for reading full datasets.

- Use wrhdf_1d, wrhdf_2d, and wrhdf_3d for writing.

- Use read_hdf_by_index or read_hdf_by_value for reading portions of datasets.

- Use np_interpolate_slice_from_hdf or interpolate_positions_from_hdf for
  slicing or interpolating directly to arbitrary positions.

- The HDF type is determined by the filename extension (.hdf or .h5).

Package Requirements:
    - REQUIRED: h5py + HDF5 (for .h5). h5py is available in default conda.
    - OPTIONAL: pyhdf + HDF4 (for .hdf), scipy (for special interpolation routines).
    - It is easiest to install these with conda (from anaconda or miniconda),
      but HDF4, HDF5, pyhdf, and h5py can be installed manually too.
      - pyhdf is available in the conda-forge channel.
      - h5py is available in the default conda channel.

Notes:
    1) Python uses C-style array indexing, while PSI tools use FORTRAN style
       array indexing, so 3D fields f(r,t,p) in FORTRAN are read and manipulated
       as f(p,t,r) in PYTHON. But we set the rdhdf_1d (2d) (3d) returns to resemble
       how we call them in FORTRAN or IDL. So, if in doubt check the shapes.

    2) Not all PSI FORTRAN tools can read HDF4 files written by the pyhdf.SD interface.
      - If you have a problem, use the PSI tool "hdfsd2hdf" to convert.

Written by Ronald M. Caplan, Ryder Davidson, & Cooper Downs.

2023/09: Start with SVN version r454, 2023/09/12 by RC, Predictive Science Inc.
2024/06: CD: add the get_scales subroutines.
2024/11: RD: Major Update: Add several generic data loading capabilites for faster IO.
         - Read only the portions of data required (`read_hdf_by_index`, `read_hdf_by_value`).
         - Interpolate to slices along a given axes (`np_interpolate_slice_from_hdf`) or
           generic positions (`interpolate_positions_from_hdf`).
2025/06: CD: Prep for integration into psi-io package, HDF4 is now optional.
"""
# Standard Python imports
from collections import namedtuple
from pathlib import Path
from typing import Optional, Literal, Tuple, Iterable, List, Dict, Union

# Required Packages
import numpy as np
import h5py as h5

# -----------------------------------------------------------------------------
# Optional Imports and Import Checking
# -----------------------------------------------------------------------------
# These packages are needed by several functions and must be imported in the
# module namespace.
try:
    import pyhdf.SD as h4
    H4_AVAILABLE = True
except ImportError:
    H4_AVAILABLE = False
try:
    from scipy.interpolate import RegularGridInterpolator
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False


# Functions to stop execution if a package doesn't exist.
def except_no_pyhdf():
    if not H4_AVAILABLE:
        raise ImportError('The pyhdf package is required to read/write HDF4 .hdf files!')
    return


def except_no_scipy():
    if not SCIPY_AVAILABLE:
        raise ImportError('The scipy package is required for the interpolation routines!')
    return


# -----------------------------------------------------------------------------
# "Classic" HDF reading and writing routines adapted from psihdf.py or psi_io.py.
# -----------------------------------------------------------------------------
def rdh5(h5_filename):
    """Base reader for 1D, 2D, and 3D HDF5 files.

    Generally this function should not be called directly.
    Use `rdhdf_1d`, `rdhdf_2d`, or `rdhdf_3d` instead.
    """
    x = np.array([])
    y = np.array([])
    z = np.array([])
    f = np.array([])

    h5file = h5.File(h5_filename, 'r')
    f = h5file['Data']
    dims = f.shape
    ndims = np.ndim(f)

    # Get the scales if they exist:
    for i in range(0, ndims):
        if i == 0:
            if (len(h5file['Data'].dims[0].keys()) != 0):
                x = h5file['Data'].dims[0][0]
        elif i == 1:
            if (len(h5file['Data'].dims[1].keys()) != 0):
                y = h5file['Data'].dims[1][0]
        elif i == 2:
            if (len(h5file['Data'].dims[2].keys()) != 0):
                z = h5file['Data'].dims[2][0]

    x = np.array(x)
    y = np.array(y)
    z = np.array(z)
    f = np.array(f)

    h5file.close()

    return (x, y, z, f)


def rdhdf(hdf_filename):
    """Base reader for 1D, 2D, and 3D HDF4 files.

    Generally this function should not be called directly.
    Use `rdhdf_1d`, `rdhdf_2d`, or `rdhdf_3d` instead.
    """
    if (hdf_filename.endswith('h5')):
        x, y, z, f = rdh5(hdf_filename)
        return (x, y, z, f)

    # Check for HDF4
    except_no_pyhdf()

    x = np.array([])
    y = np.array([])
    z = np.array([])
    f = np.array([])

    # Open the HDF file
    sd_id = h4.SD(hdf_filename)

    # Read dataset.  In all PSI hdf4 files, the
    # data is stored in "Data-Set-2":
    sds_id = sd_id.select('Data-Set-2')
    f = sds_id.get()

    # Get number of dimensions:
    ndims = np.ndim(f)

    # Get the scales. Check if theys exist by looking at the 3rd
    # element of dim.info(). 0 = none, 5 = float32, 6 = float64.
    # see http://pysclint.sourceforge.net/pyhdf/pyhdf.SD.html#SD
    # and http://pysclint.sourceforge.net/pyhdf/pyhdf.SD.html#SDC
    for i in range(0, ndims):
        dim = sds_id.dim(i)
        if dim.info()[2] != 0:
            if i == 0:
                x = dim.getscale()
            elif i == 1:
                y = dim.getscale()
            elif i == 2:
                z = dim.getscale()

    sd_id.end()

    x = np.array(x)
    y = np.array(y)
    z = np.array(z)
    f = np.array(f)

    return (x, y, z, f)


def rdhdf_1d(hdf_filename):
    """Read a 1D PSI-style HDF5 or HDF4 file.

    Parameters
    ----------
    hdf_filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to read.

    Returns
    -------
    x: ndarray
        1D array of scales.
    f: ndarray
        1D array of data.
    """
    x, y, z, f = rdhdf(hdf_filename)

    return (x, f)


def rdhdf_2d(hdf_filename):
    """Read a 2D PSI-style HDF5 or HDF4 file.

    The data in the HDF file is assumed to be ordered X,Y in Fortran order.

    Each dimension is assumed to have a 1D "scale" associated with it that
    describes the rectilinear grid coordinates in each dimension.

    Parameters
    ----------
    hdf_filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to read.

    Returns
    -------
    x: ndarray
        1D array of scales in the X dimension.
    y: ndarray
        1D array of scales in the Y dimension.
    f: ndarray
        2D array of data, C-ordered as shape(ny,nx) for Python (see note 1).
    """
    x, y, z, f = rdhdf(hdf_filename)

    if (hdf_filename.endswith('h5')):
        return (x, y, f)
    return (y, x, f)


def rdhdf_3d(hdf_filename):
    """Read a 3D PSI-style HDF5 or HDF4 file.

    The data in the HDF file is assumed to be ordered X,Y,Z in Fortran order.

    Each dimension is assumed to have a 1D "scale" associated with it that
    describes the rectilinear grid coordinates in each dimension.

    Parameters
    ----------
    hdf_filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to read.

    Returns
    -------
    x: ndarray
        1D array of scales in the X dimension.
    y: ndarray
        1D array of scales in the Y dimension.
    z: ndarray
        1D array of scales in the Z dimension.
    f: ndarray
        3D array of data, C-ordered as shape(nz,ny,nx) for Python (see note 1).
    """
    x, y, z, f = rdhdf(hdf_filename)
    if (hdf_filename.endswith('h5')):
        return (x, y, z, f)

    return (z, y, x, f)


def wrh5(h5_filename, x, y, z, f):
    """Base writer for 1D, 2D, and 3D HDF5 files.

    Generally this function should not be called directly.
    Use `wrhdf_1d`, `wrhdf_2d`, or `wrhdf_3d` instead.
    """
    h5file = h5.File(h5_filename, 'w')

    # Create the dataset (Data is the name used by the psi data)).
    h5file.create_dataset("Data", data=f)

    # Make sure the scales are desired by checking x type, which can
    # be None or None converted by np.asarray (have to trap seperately)
    if x is None:
        x = np.array([], dtype=f.dtype)
        y = np.array([], dtype=f.dtype)
        z = np.array([], dtype=f.dtype)
    if x.any() == None:
        x = np.array([], dtype=f.dtype)
        y = np.array([], dtype=f.dtype)
        z = np.array([], dtype=f.dtype)

    # Make sure scales are the same precision as data.
    x = x.astype(f.dtype)
    y = y.astype(f.dtype)
    z = z.astype(f.dtype)

    # Get number of dimensions:
    ndims = np.ndim(f)

    # Set the scales:
    for i in range(0, ndims):
        if i == 0 and len(x) != 0:
            dim = h5file.create_dataset("dim1", data=x)
            #            h5file['Data'].dims.create_scale(dim,'dim1')
            dim.make_scale('dim1')
            h5file['Data'].dims[0].attach_scale(dim)
            h5file['Data'].dims[0].label = 'dim1'
        if i == 1 and len(y) != 0:
            dim = h5file.create_dataset("dim2", data=y)
            #            h5file['Data'].dims.create_scale(dim,'dim2')
            dim.make_scale('dim2')
            h5file['Data'].dims[1].attach_scale(dim)
            h5file['Data'].dims[1].label = 'dim2'
        elif i == 2 and len(z) != 0:
            dim = h5file.create_dataset("dim3", data=z)
            #            h5file['Data'].dims.create_scale(dim,'dim3')
            dim.make_scale('dim3')
            h5file['Data'].dims[2].attach_scale(dim)
            h5file['Data'].dims[2].label = 'dim3'

    # Close the file:
    h5file.close()


def wrhdf(hdf_filename, x, y, z, f):
    """Base writer for 1D, 2D, and 3D HDF4 files.

    Generally this function should not be called directly.
    Use `wrhdf_1d`, `wrhdf_2d`, or `wrhdf_3d` instead.
    """
    if (hdf_filename.endswith('h5')):
        wrh5(hdf_filename, x, y, z, f)
        return

    # Check for HDF4
    except_no_pyhdf()

    # Create an HDF file
    sd_id = h4.SD(hdf_filename, h4.SDC.WRITE | h4.SDC.CREATE | h4.SDC.TRUNC)

    # Due to bug, need to only write 64-bit.
    f = f.astype(np.float64)
    ftype = h4.SDC.FLOAT64

    #    if f.dtype == np.float32:
    #        ftype = h4.SDC.FLOAT32
    #    elif f.dtype == np.float64:
    #        ftype = h4.SDC.FLOAT64

    # Create the dataset (Data-Set-2 is the name used by the psi data)).
    sds_id = sd_id.create("Data-Set-2", ftype, f.shape)

    # Get number of dimensions:
    ndims = np.ndim(f)

    # Make sure the scales are desired by checking x type, which can
    # be None or None converted by np.asarray (have to trap seperately)
    if x is None:
        x = np.array([], dtype=f.dtype)
        y = np.array([], dtype=f.dtype)
        z = np.array([], dtype=f.dtype)
    if x.any() == None:
        x = np.array([], dtype=f.dtype)
        y = np.array([], dtype=f.dtype)
        z = np.array([], dtype=f.dtype)

    # Due to python hdf4 bug, need to use double scales only.

    x = x.astype(np.float64)
    y = y.astype(np.float64)
    z = z.astype(np.float64)

    # Set the scales (or don't if x is none or length zero)
    for i in range(0, ndims):
        dim = sds_id.dim(i)
        if i == 0 and len(x) != 0:
            if x.dtype == np.float32:
                stype = h4.SDC.FLOAT32
            elif x.dtype == np.float64:
                stype = h4.SDC.FLOAT64
            dim.setscale(stype, x)
        elif i == 1 and len(y) != 0:
            if y.dtype == np.float32:
                stype = h4.SDC.FLOAT32
            elif y.dtype == np.float64:
                stype = h4.SDC.FLOAT64
            dim.setscale(stype, y)
        elif i == 2 and len(z) != 0:
            if z.dtype == np.float32:
                stype = h4.SDC.FLOAT32
            elif z.dtype == np.float64:
                stype = h4.SDC.FLOAT64
            dim.setscale(stype, z)

    # Write the data:
    sds_id.set(f)

    # Close the dataset:
    sds_id.endaccess()

    # Flush and close the HDF file:
    sd_id.end()


def wrhdf_1d(hdf_filename, x, f):
    """Write a 1D PSI-style HDF5 or HDF4 file.

    Parameters
    ----------
    hdf_filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to write.
    x: ndarray
        1D array of scales.
    f: ndarray
        1D array of data.
    """
    x = np.asarray(x)
    y = np.array([])
    z = np.array([])
    f = np.asarray(f)
    wrhdf(hdf_filename, x, y, z, f)


def wrhdf_2d(hdf_filename, x, y, f):
    """Write a 2D PSI-style HDF5 or HDF4 file.

    The data in the HDF file will appear as X,Y in Fortran order.

    Each dimension requires a 1D "scale" associated with it that
    describes the rectilinear grid coordinates in each dimension.

    Parameters
    ----------
    hdf_filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to write.
    x: ndarray
        1D array of scales in the X dimension.
    y: ndarray
        1D array of scales in the Y dimension.
    f: ndarray
        2D array of data, C-ordered as shape(ny,nx) for Python (see note 1).
    """
    x = np.asarray(x)
    y = np.asarray(y)
    z = np.array([])
    f = np.asarray(f)
    if (hdf_filename.endswith('h5')):
        wrhdf(hdf_filename, x, y, z, f)
        return
    wrhdf(hdf_filename, y, x, z, f)


def wrhdf_3d(hdf_filename, x, y, z, f):
    """Write a 3D PSI-style HDF5 or HDF4 file.

    The data in the HDF file will appear as X,Y,Z in Fortran order.

    Each dimension requires a 1D "scale" associated with it that
    describes the rectilinear grid coordinates in each dimension.

    Parameters
    ----------
    hdf_filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to write.
    x: ndarray
        1D array of scales in the X dimension.
    y: ndarray
        1D array of scales in the Y dimension.
    z: ndarray
        1D array of scales in the Z dimension.
    f: ndarray
        2D array of data, C-ordered as shape(ny,nx) for Python (see note 1).
    """
    x = np.asarray(x)
    y = np.asarray(y)
    z = np.asarray(z)
    f = np.asarray(f)
    if (hdf_filename.endswith('h5')):
        wrhdf(hdf_filename, x, y, z, f)
        return
    wrhdf(hdf_filename, z, y, x, f)


def get_scales_1d(filename):
    """Wrapper to return the scales of a 1D PSI style HDF5 or HDF4 dataset.

    This routine does not load the data array so it can be quite fast for large files.

    Parameters
    ----------
    filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to read.

    Returns
    -------
    x: ndarray
        1D array of scales in the X dimension.
    """
    if filename.endswith('h5'):
        x = get_scales_h5(filename)
    elif filename.endswith('hdf'):
        x = get_scales_h4(filename)
    return x


def get_scales_2d(filename):
    """Wrapper to return the scales of a 2D PSI style HDF5 or HDF4 dataset.

    This routine does not load the data array so it can be quite fast for large files.

    The data in the HDF file is assumed to be ordered X,Y in Fortran order.

    Parameters
    ----------
    filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to read.

    Returns
    -------
    x: ndarray
        1D array of scales in the X dimension.
    y: ndarray
        1D array of scales in the Y dimension.
    """
    if filename.endswith('h5'):
        x, y = get_scales_h5(filename)
    elif filename.endswith('hdf'):
        x, y = get_scales_h4(filename)
    return x, y


def get_scales_3d(filename):
    """Wrapper to return the scales of a 3D PSI style HDF5 or HDF4 dataset.

    This routine does not load the data array so it can be quite fast for large files.

    The data in the HDF file is assumed to be ordered X,Y,Z in Fortran order.

    Parameters
    ----------
    filename : Path or str
        The path to the 1D HDF5 (.h5) or HDF4 (.hdf) file to read.

    Returns
    -------
    x: ndarray
        1D array of scales in the X dimension.
    y: ndarray
        1D array of scales in the Y dimension.
    z: ndarray
        1D array of scales in the Z dimension.
    """
    if filename.endswith('h5'):
        x, y, z = get_scales_h5(filename)
    elif filename.endswith('hdf'):
        x, y, z = get_scales_h4(filename)
    return x, y, z


def get_scales_h4(hdf_filename):
    """Base 1D scale reader for 1D, 2D, and 3D HDF4 files.

    Generally this function should not be called directly.
    Use `get_scales_1d`, `get_scales_2d`, or `get_scales_3d` instead.
    """
    except_no_pyhdf()
    # get the file info
    try:
        sd_id = h4.SD(hdf_filename)
    except:
        raise Exception(f'### ERROR!!! get_scales_h4: COULD NOT OPEN\n'
                        f' {hdf_filename}')

    # open up the SDS assuming it is a PSI style file
    sds_id = sd_id.select('Data-Set-2')

    # get the dimensions of the dataset
    dim_dict = sds_id.dimensions()
    ndims = len(dim_dict)

    # Get the scales. Check if they exist by looking at the 3rd
    # element of dim.info(). 0 = none, 5 = float32, 6 = float64.
    # see http://pysclint.sourceforge.net/pyhdf/pyhdf.SD.html#SD
    # and http://pysclint.sourceforge.net/pyhdf/pyhdf.SD.html#SDC
    for i in range(ndims):
        dim = sds_id.dim(i)
        if dim.info()[2] == 0:
            raise Exception(f'  ERROR! Dim {i} does not have scales in \n {hdf_filename}')
        if i == 0:
            scale1 = np.array(dim.getscale())
        elif i == 1:
            scale2 = np.array(dim.getscale())
        elif i == 2:
            scale3 = np.array(dim.getscale())

    # close the dataset
    sd_id.end()

    # note the scales are returned according to the C-Style array shape.
    # instead return them in the fortran style order, which depends on the dimensionality
    if ndims == 1:
        return scale1
    elif ndims == 2:
        return scale2, scale1
    elif ndims == 3:
        return scale3, scale2, scale1


def get_scales_h5(h5_filename):
    """Base 1D scale reader for 1D, 2D, and 3D HDF5 files.

    Here the arrays are converted to 64 bit for better compatibility with numpy,
    which (weirdly) is generally as fast or faster than keeping float32.

    Generally this function should not be called directly.
    Use `get_scales_1d`, `get_scales_2d`, or `get_scales_3d` instead.
    """
    # get the file info
    try:
        h5file = h5.File(h5_filename, 'r')
    except:
        raise Exception(f'### ERROR!!! get_scales_h5: COULD NOT OPEN\n'
                        f' {h5_filename}')

    # here the scales are datasets at the main level (dim1, dim2, dim3, etc)
    # --> we can just get those directly without wasing time on the dataset
    keys = h5file.keys()

    ndims = 0
    if 'dim1' in keys:
        scale1 = np.array(h5file['dim1'], dtype=np.float64)
        ndims = ndims + 1
    if 'dim2' in keys:
        scale2 = np.array(h5file['dim2'], dtype=np.float64)
        ndims = ndims + 1
    if 'dim3' in keys:
        scale3 = np.array(h5file['dim3'], dtype=np.float64)
        ndims = ndims + 1

    # close the file
    h5file.close()

    # note in the hdf5 format the dim1, dim2, dim3 names correspond to the
    # fortran array ordering --> return them in this order (no flip like the
    # HDF4 version of this subroutine).
    if ndims == 1:
        return scale1
    elif ndims == 2:
        return scale1, scale2
    elif ndims == 3:
        return scale1, scale2, scale3


# -----------------------------------------------------------------------------
# NEWER ALTERNATIVE METHODS FOR SLICING & INTERPOLATING HDFs
# -----------------------------------------------------------------------------
# These methods are aimed at eliminating the need for
# reading entire datasets into memory when the objective
# is to inspect only a small subset of the data e.g.
# producing 1D or 2D slices.
#
# More examples and use cases are forthcoming, along
# with benchmarking tests and further documentation.
# -----------------------------------------------------------------------------

"""
Helper dictionary for mapping HDF4 types to numpy dtypes
"""
SDC_TYPE_CONVERSIONS = {
    3: np.dtype("ubyte"),
    4: np.dtype("byte"),
    5: np.dtype("float32"),
    6: np.dtype("float64"),
    20: np.dtype("int8"),
    21: np.dtype("uint8"),
    22: np.dtype("int16"),
    23: np.dtype("uint16"),
    24: np.dtype("int32"),
    25: np.dtype("uint32")

}

"""
Helper structures for formatting metadata returned by `read_hdf_meta`
"""
HdfScaleMeta = namedtuple('HdfScaleMeta', ['name', 'type', 'shape', 'imin', 'imax'])
HdfDataMeta = namedtuple('HdfDataMeta', ['name', 'type', 'shape', 'scales'])


def read_hdf_meta(ifile: Union[Path, str],
                  dataset_id: Optional[ Union[ str, Literal['all']]] = None
                  ) -> List[HdfDataMeta]:
    """
    Read metadata from an HDF4 (.hdf) or HDF5 (.h5) file.

    Parameters
    ----------
    ifile : Path or str
        The path to the HDF file to read.
    dataset_id : str or {'all'}, optional
        The identifier of the dataset for which to read metadata.
        If 'all', metadata for all datasets is returned.
        If None, the default PSI standard dataset_id is used.

    Returns
    -------
    List[HdfDataMeta]
        A list of metadata objects corresponding to the specified datasets.

    Raises
    ------
    ValueError
        If the file does not have a `.hdf` or `.h5` extension.

    Notes
    -----
    This function delegates to `read_h5_meta` for HDF5 files and `read_h4_meta`
    for HDF4 files based on the file extension.

    """
    if ifile.endswith('.h5'):
        return read_h5_meta(ifile, dataset_id=dataset_id)
    elif ifile.endswith('.hdf'):
        return read_h4_meta(ifile, dataset_id=dataset_id)
    else:
        raise ValueError("File must be HDF4 (.hdf) or HDF5 (.h5)")


def read_rtp_meta(ifile: Union[ Path, str]) -> Dict:
    """
    Read the scale metadata for PSI's 3D cubes. This function assumes that the
    dataset has a shape (p, t, r) with radial, theta, and phi scales corresponding
    to dimension r, t, p respectively.

    Parameters
    ----------
    ifile : Path or str
        The path to the HDF file to read.

    Returns
    -------
    Dict
        A dictionary containing the RTP metadata.
        The value for each key ('r', 't', and 'p') is a tuple containing:
            1. The scale length
            2. The scale's value at the first index
            3. The scale's value at the last index

    Raises
    ------
    ValueError
        If the file does not have a `.hdf` or `.h5` extension.

    Notes
    -----
    This function delegates to `read_h5_rtp` for HDF5 files and `read_h4_rtp`
    for HDF4 files based on the file extension.

    """
    if ifile.endswith('.h5'):
        return read_h5_rtp(ifile)
    elif ifile.endswith('.hdf'):
        return read_h4_rtp(ifile)
    else:
        raise ValueError("File must be HDF4 (.hdf) or HDF5 (.h5)")


def read_hdf_by_value(*xi: Union[ float, Tuple[float, float], None],
                      ifile: Union[ Path, str],
                      dataset_id: Optional[str] = None,
                      return_scales: bool = True,
                      ) -> Union[ np.ndarray, Tuple[np.ndarray]]:
    """
    Read data from an HDF4 (.hdf) or HDF5 (.h5) file by value(s).

    Parameters
    ----------
    x0, x1,..., xn : float or tuple of floats or None
        Values or value ranges corresponding to each dimension of the `n` dimensional
        dataset specified by the `dataset_id`. If no arguments are passed, the
        entire dataset (and its scales) will be returned.
    ifile : Path or str
        The path to the HDF file to read.
    dataset_id : str, optional
        The identifier of the dataset to read.
        If None, a default dataset is used.
    return_scales : bool, optional
        If True, the scales for the specified dataset are also returned.

    Returns
    -------
    np.ndarray or Tuple[np.ndarray]
        The selected data array.
        If `return_scales` is True, returns a tuple containing the data array
        and the scales for each dimension.

    Raises
    ------
    ValueError
        If the file does not have a `.hdf` or `.h5` extension.

    Notes
    -----
    This function delegates to `read_h5_by_value` for HDF5 files and `read_h4_by_value`
    for HDF4 files based on the file extension.

    This function assumes that the dataset is FORTRAN ordered *i.e.* for an array with
    shape (i, j, ..., n): len(scale_0) == n, ..., len(scale_n-1) == j, len(scale_n) == i.
    The main purpose of this is for compatability with PSI's data ecosystem.

    This function extracts a subset of the given dataset/scales without reading the
    entire data into memory. For a given scale `j` (x0, x1, ..., xj, ... xn), if:
        i) a single float is provided ("xj_value"), the function will return a 2-element
        subset of scale `xj` where `xj[0] <= xj_value < xj[1]`.
        ii) a (float, float) tuple is provided ("xj_range"), the function will return an
        m-element subset of scale `xj` where `xj[0] <= xj_range[0] and xj[m] > xj_range[1]
        iii) a None value is provided, the function will return the entire scale `xj`
    The returned subset can then be passed to a linear interpolation routine to extract the
    "slice" at the desired fixed dimensions.

    Note that for each dimension, the minimum number of elements returned will be 2 *e.g.*
    if 3 floats are passed (as *xi) for a 3D dataset, the resulting subset will have a shape of
    (2, 2, 2,) with scales of length 2.

    Examples
    --------
    # To extract a radial slice at r=15. from a 3D cube:
    f, r, t, p = read_hdf_by_value(15, None, None, ifile='path/to/hdf.h5')

    # To extract a theta slice at t=3.14 from a 3D cube:
    f, r, t, p = read_hdf_by_value(None, 3.14, None, ifile='path/to/hdf.h5')

    # To extract the values between 3.2 and 6.4 (in the radial dimension) and with
    # phi equal to 4.5
    f, r, t, p = read_hdf_by_value((3.2, 6.4), None, 4.5, ifile='path/to/hdf.h5')

    # Suppose there is a dataset with id "MyDataSet" with shape (1e6, 1e7, 1e8, 1e9)
    # and each scale is linearly spaced between 0 and 1; to select the subset of data
    # that surrounds the scale values 0.5, 0.4, 0.3, and 0.2:
    f, *scales = read_hdf_by_value(0.5, 0.4, 0.3, 0.2, dataset_id='MyDataSet', ifile='path/to/large/data.h5')
    print(f.shape)              # (2, 2, 2, 2,)
    for scale in scales:
        print(scale.shape)      # (2,)

    """
    if ifile.endswith('.h5'):
        return read_h5_by_value(*xi, ifile=ifile, dataset_id=dataset_id, return_scales=return_scales)
    elif ifile.endswith('.hdf'):
        return read_h4_by_value(*xi, ifile=ifile, dataset_id=dataset_id, return_scales=return_scales)
    else:
        raise ValueError("File must be HDF4 (.hdf) or HDF5 (.h5)")


def read_hdf_by_index(*xi,
                      ifile: Union[ Path, str],
                      dataset_id: Optional[str] = None,
                      return_scales: bool = True,
                      ) -> Union[ np.ndarray, Tuple[np.ndarray]]:
    """
    Read data from an HDF4 (.hdf) or HDF5 (.h5) file by index ranges.

    Parameters
    ----------
    *xi : x0, x1,..., xn : int or tuple of int or None
       Indices or ranges for each dimension of the `n` dimensional dataset.
       Use None for a dimension to select all indices.
       This function is the counterpart to `read_hdf_by_value` except that the
       dataset is directly indexed at the provided values.
    ifile : Path or str
       The path to the HDF file to read.
    dataset_id : str, optional
       The identifier of the dataset to read.
       If None, a default dataset is used.
    return_scales : bool, optional
       If True, the scales (coordinate arrays) for each dimension are also returned.

    Returns
    -------
    np.ndarray or Tuple[np.ndarray]
       The selected data array.
       If `return_scales` is True, returns a tuple containing the data array
       and the scales for each dimension.

    Raises
    ------
    ValueError
       If the file does not have a `.hdf` or `.h5` extension.

    Notes
    -----
    This function delegates to `read_h5_by_index` for HDF5 files and `read_h4_by_index`
    for HDF4 files based on the file extension.

    This function assumes that the dataset is FORTRAN ordered *i.e.* for an array with
    shape (i, j, ..., n): len(scale_0) == n, ..., len(scale_n-1) == j, len(scale_n) == i.
    The main purpose of this is for compatability with PSI's data ecosystem.

    This function is used to directly access a dataset/scale(s) at a specified index or
    slice *e.g.* to fetch the subset of data at the first position of the radial scale
    (with respect to a standard MAS 3D cube), one would pass: 0, None, None to the
    function. The result would be a dataset of shape (n, m, 1).

    """
    if ifile.endswith('.h5'):
        return read_h5_by_index(*xi, ifile=ifile, dataset_id=dataset_id, return_scales=return_scales)
    elif ifile.endswith('.hdf'):
        return read_h4_by_index(*xi, ifile=ifile, dataset_id=dataset_id, return_scales=return_scales)
    else:
        raise ValueError("File must be HDF4 (.hdf) or HDF5 (.h5)")


def read_h5_rtp(ifile: Union[ Path, str]):
    """
    Read the scale metadata for PSI's HDF5 (.h5) 3D cubes. This function assumes that the
    dataset has a shape (p, t, r) with radial, theta, and phi scales corresponding
    to dimension r, t, p respectively.

    Parameters
    ----------
    ifile : Path or str
        The path to the HDF file to read.

    Returns
    -------
    Dict
        A dictionary containing the RTP metadata.
        The value for each key ('r', 't', and 'p') is a tuple containing:
            1. The scale length
            2. The scale's value at the first index
            3. The scale's value at the last index
    Notes
    -----
    This function extracts the shape, first, and last values for the dimensions 'dim1',
    'dim2', and 'dim3' corresponding to 'r', 't', and 'p'.

    """
    with h5.File(ifile, 'r') as hdf:
        return {dim[0]: (hdf[dim[1]].shape[0], hdf[dim[1]][0], hdf[dim[1]][-1]) for dim in
                zip('rtp', ('dim1', 'dim2', 'dim3'))}


def read_h4_rtp(ifile: Union[ Path, str]):
    """
    Read the scale metadata for PSI's HDF4 (.hdf) 3D cubes. This function assumes that the
    dataset has a shape (p, t, r) with radial, theta, and phi scales corresponding
    to dimension r, t, p respectively.

    Parameters
    ----------
    ifile : Path or str
        The path to the HDF file to read.

    Returns
    -------
    Dict
        A dictionary containing the RTP metadata.
        The value for each key ('r', 't', and 'p') is a tuple containing:
            1. The scale length
            2. The scale's value at the first index
            3. The scale's value at the last index
    Notes
    -----
    This function extracts the shape, first, and last values for the dimensions 'dim1',
    'dim2', and 'dim3' corresponding to 'r', 't', and 'p'.

    """
    except_no_pyhdf()
    hdf = h4.SD(ifile)
    data = hdf.select('Data-Set-2')
    return {dim[0]: (dim[1][1][0], hdf.select(dim[1][0])[0], hdf.select(dim[1][0])[-1]) for dim in
            zip('ptr', (data.dimensions(full=1).items()))}


def read_h5_meta(ifile: Union[ Path, str],
                 dataset_id: Optional[ Union[ str, Literal['all']]] = None
                 ):
    """
    Read metadata from an HDF5 (.h5) file.

    Parameters
    ----------
    ifile : Path or str
       The path to the HDF5 file to read.
    dataset_id : str or {'all'}, optional
       The identifier of the dataset for which to read metadata.
       If 'all', metadata for all datasets is returned.
       If None, a default dataset ('Data') is used.

    Returns
    -------
    List[HdfDataMeta]
       A list of metadata objects corresponding to the specified datasets.

    Notes
    -----
    This function reads the dataset(s) and their associated scales from the HDF5 file.

    """
    with h5.File(ifile, 'r') as hdf:
        # match dataset_id:
        #     case None:
        #         datasets = [('Data', hdf['Data'])]
        #     case 'all':
        #         datasets = [(k, v) for k, v in hdf.items() if not v.is_scale]
        #     case _:
        #         datasets = [(dataset_id, hdf[dataset_id])]
        if dataset_id is None:
            datasets = [('Data', hdf['Data'])]
        elif dataset_id == 'all':
            datasets = [(k, v) for k, v in hdf.items() if not v.is_scale]
        else:
            datasets = [(dataset_id, hdf[dataset_id])]
        meta = [HdfDataMeta(name=k,
                            type=v.dtype,
                            shape=v.shape,
                            scales=[HdfScaleMeta(dim.label,
                                                 dim[0].dtype,
                                                 dim[0].shape,
                                                 dim[0][0],
                                                 dim[0][-1])
                                    for dim in v.dims]
                            )
                for k, v in datasets]
        return meta


def read_h4_meta(ifile: Union[ Path, str],
                 dataset_id: Optional[ Union[ str, Literal['all']]] = None
                 ):
    """
    Read metadata from an HDF4 (.hdf) file.

    Parameters
    ----------
    ifile : Path or str
        The path to the HDF4 file to read.
    dataset_id : str or {'all'}, optional
        The identifier of the dataset for which to read metadata.
        If 'all', metadata for all datasets is returned.
        If None, a default dataset ('Data-Set-2') is used.

    Returns
    -------
    List[HdfDataMeta]
        A list of metadata objects corresponding to the specified datasets.

    Notes
    -----
    This function reads the dataset(s) and their associated scales from the HDF4 file.

    """
    except_no_pyhdf()
    hdf = h4.SD(ifile)
    # match dataset_id:
    #     case None:
    #         datasets = [('Data-Set-2', hdf.select('Data-Set-2'))]
    #     case 'all':
    #         datasets = [(k, hdf.select(k)) for k in hdf.datasets().keys() if not hdf.select(k).iscoordvar()]
    #     case _:
    #         datasets = [(dataset_id, hdf.select(dataset_id))]
    if dataset_id is None:
        datasets = [('Data-Set-2', hdf.select('Data-Set-2'))]
    elif dataset_id == 'all':
        datasets = [(k, hdf.select(k)) for k in hdf.datasets().keys() if not hdf.select(k).iscoordvar()]
    else:
        datasets = [(dataset_id, hdf.select(dataset_id))]
    meta = [HdfDataMeta(name=k,
                        type=SDC_TYPE_CONVERSIONS[v.info()[3]],
                        shape=tuple(v.info()[2]),
                        scales=[HdfScaleMeta(k_,
                                             SDC_TYPE_CONVERSIONS[v_[3]],
                                             (v_[0],),
                                             hdf.select(k_)[0],
                                             hdf.select(k_)[-1])
                                for k_, v_ in v.dimensions(full=1).items()][::-1]
                        )
            for k, v in datasets]
    return meta


def read_h5_by_value(*xi,
                     ifile: Union[ Path, str],
                     dataset_id: Optional[str] = None,
                     return_scales: bool = True,
                     ) -> Union[ np.ndarray, Tuple[np.ndarray]]:
    """
    Read data from an HDF5 (.h5) file by value(s).

    Parameters
    ----------
    x0, x1,..., xn : float or tuple of floats or None
        Values or value ranges corresponding to each dimension of the `n` dimensional
        dataset specified by the `dataset_id`. If no arguments are passed, the
        entire dataset (and its scales) will be returned.
    ifile : Path or str
        The path to the HDF file to read.
    dataset_id : str, optional
        The identifier of the dataset to read.
        If None, a default dataset is used.
    return_scales : bool, optional
        If True, the scales for the specified dataset are also returned.

    Returns
    -------
    np.ndarray or Tuple[np.ndarray]
        The selected data array.
        If `return_scales` is True, returns a tuple containing the data array
        and the scales for each dimension.

    See Also
    --------
    For detailed documentation see:
        `read_hdf_by_value`

    """

    with h5.File(ifile, 'r') as hdf:
        if dataset_id is None:
            dataset_id = 'Data'
        data = hdf[dataset_id]

        if not xi:
            if return_scales:
                return data[:], *[dim[0][:] for dim in data.dims]
            else:
                return np.array(data)
        else:
            assert len(xi) == data.ndim, f"len(xi) must equal the number of scales for {dataset_id}"

            slices = [None]*data.ndim
            for i, dim in enumerate(data.dims):
                dim_proxy = dim[0]
                if xi[i] is None:
                    slices[i] = slice(None, None)
                elif not isinstance(xi[i], Iterable):
                    insert_index = np.searchsorted(dim_proxy[:], xi[i])
                    slices[i] = slice(*_check_index_ranges(dim_proxy.size, insert_index, insert_index))
                else:
                    temp_range = list(xi[i])
                    if temp_range[0] is None:
                        temp_range[0] = -np.inf
                    if temp_range[-1] is None:
                        temp_range[-1] = np.inf
                    insert_indices = np.searchsorted(dim_proxy[:], temp_range)
                    slices[i] = slice(*_check_index_ranges(dim_proxy.size, *insert_indices))
            if return_scales:
                return data[tuple(slices)[::-1]], *[dim[0][slices[i]] for i, dim in enumerate(data.dims)]
            else:
                return data[tuple(slices)[::-1]]


def read_h4_by_value(*xi,
                     ifile: Union[ Path, str],
                     dataset_id: Optional[str] = None,
                     return_scales: bool = True,
                     ) -> Union[ np.ndarray, Tuple[np.ndarray]]:
    """
    Read data from an HDF4 (.hdf) file by value(s).

    Parameters
    ----------
    x0, x1,..., xn : float or tuple of floats or None
        Values or value ranges corresponding to each dimension of the `n` dimensional
        dataset specified by the `dataset_id`. If no arguments are passed, the
        entire dataset (and its scales) will be returned.
    ifile : Path or str
        The path to the HDF file to read.
    dataset_id : str, optional
        The identifier of the dataset to read.
        If None, a default dataset is used.
    return_scales : bool, optional
        If True, the scales for the specified dataset are also returned.

    Returns
    -------
    np.ndarray or Tuple[np.ndarray]
        The selected data array.
        If `return_scales` is True, returns a tuple containing the data array
        and the scales for each dimension.

    See Also
    --------
    For detailed documentation see:
        `read_hdf_by_value`

    """
    except_no_pyhdf()
    hdf = h4.SD(ifile)
    if dataset_id is None:
        dataset_id = 'Data-Set-2'
    data = hdf.select(dataset_id)
    ndim = data.info()[1]

    if not xi:
        if return_scales:
            return data[:], *[hdf.select(dim)[:] for dim in data.dimensions().keys()][::-1]
        else:
            return data[:]
    else:
        assert len(xi) == ndim, f"len(xi) must equal the number of scales for {dataset_id}"
        xi = list(xi)[::-1]
        slices = [None]*ndim
        for i, dim in enumerate(data.dimensions().keys()):
            dim_proxy = hdf.select(dim)
            if xi[i] is None:
                slices[i] = slice(None, None)
            elif not isinstance(xi[i], Iterable):
                insert_index = int(np.searchsorted(dim_proxy[:], xi[i]))
                slices[i] = slice(*_check_index_ranges(dim_proxy.info()[2], insert_index, insert_index))
            else:
                temp_range = list(xi[i])
                if temp_range[0] is None:
                    temp_range[0] = -np.inf
                if temp_range[-1] is None:
                    temp_range[-1] = np.inf
                insert_indices = np.searchsorted(dim_proxy[:], temp_range)
                slices[i] = slice(*_check_index_ranges(dim_proxy.info()[2], int(insert_indices[0]), int(insert_indices[1])))
        if return_scales:
            return data[tuple(slices)], *[hdf.select(dim)[slices[i]] for i, dim in enumerate(data.dimensions().keys())][::-1]
        else:
            return data[tuple(slices)]


def read_h5_by_index(*xi,
                     ifile: Union[ Path, str],
                     dataset_id: Optional[str] = None,
                     return_scales: bool = True,
                     ) -> Union[ np.ndarray, Tuple[np.ndarray]]:
    """
    Read data from an HDF5 (.h5) file by index ranges.

    Parameters
    ----------
    *xi : x0, x1,..., xn : int or tuple of int or None
       Indices or ranges for each dimension of the `n` dimensional dataset.
       Use None for a dimension to select all indices.
       This function is the counterpart to `read_hdf_by_value` except that the
       dataset is directly indexed at the provided values.
    ifile : Path or str
       The path to the HDF file to read.
    dataset_id : str, optional
       The identifier of the dataset to read.
       If None, a default dataset is used.
    return_scales : bool, optional
       If True, the scales (coordinate arrays) for each dimension are also returned.

    Returns
    -------
    np.ndarray or Tuple[np.ndarray]
       The selected data array.
       If `return_scales` is True, returns a tuple containing the data array
       and the scales for each dimension.

    See Also
    --------
    For detailed documentation see:
        `read_hdf_by_index`

    """
    with h5.File(ifile, 'r') as hdf:
        if dataset_id is None:
            dataset_id = 'Data'
        data = hdf[dataset_id]

        if not xi:
            if return_scales:
                return data[:], *[dim[0][:] for dim in data.dims]
            else:
                return np.array(data)
        else:
            assert len(xi) == data.ndim, f"len(xi) must equal the number of scales for {dataset_id}"

            slices = [None]*data.ndim
            for i, dim in enumerate(data.dims):
                if xi[i] is None:
                    slices[i] = slice(None, None)
                elif not isinstance(xi[i], Iterable):
                    slices[i] = slice(xi[i], xi[i] + 1)
                else:
                    slices[i] = slice(*xi[i])
            if return_scales:
                return data[tuple(slices)[::-1]], *[dim[0][slices[i]] for i, dim in enumerate(data.dims)]
            else:
                return data[tuple(slices)[::-1]]


def read_h4_by_index(*xi,
                     ifile: Union[ Path, str],
                     dataset_id: Optional[str] = None,
                     return_scales: bool = True,
                     ) -> Union[ np.ndarray, Tuple[np.ndarray]]:
    """
    Read data from an HDF4 (.hdf) file by index ranges.

    Parameters
    ----------
    *xi : x0, x1,..., xn : int or tuple of int or None
       Indices or ranges for each dimension of the `n` dimensional dataset.
       Use None for a dimension to select all indices.
       This function is the counterpart to `read_hdf_by_value` except that the
       dataset is directly indexed at the provided values.
    ifile : Path or str
       The path to the HDF file to read.
    dataset_id : str, optional
       The identifier of the dataset to read.
       If None, a default dataset is used.
    return_scales : bool, optional
       If True, the scales (coordinate arrays) for each dimension are also returned.

    Returns
    -------
    np.ndarray or Tuple[np.ndarray]
       The selected data array.
       If `return_scales` is True, returns a tuple containing the data array
       and the scales for each dimension.

    See Also
    --------
    For detailed documentation see:
        `read_hdf_by_index`

    """
    except_no_pyhdf()
    hdf = h4.SD(ifile)
    if dataset_id is None:
        dataset_id = 'Data-Set-2'
    data = hdf.select(dataset_id)
    ndim = data.info()[1]

    if not xi:
        if return_scales:
            return data[:], *[hdf.select(dim)[:] for dim in data.dimensions().keys()][::-1]
        else:
            return data[:]
    else:
        assert len(xi) == ndim, f"len(xi) must equal the number of scales for {dataset_id}"
        xi = list(xi)[::-1]
        slices = [None]*ndim
        for i, dim in enumerate(data.dimensions().keys()):
            if xi[i] is None:
                slices[i] = slice(None, None)
            elif not isinstance(xi[i], Iterable):
                slices[i] = slice(xi[i], xi[i] + 1)
            else:
                slices[i] = slice(*xi[i])
        if return_scales:
            return data[tuple(slices)], *[hdf.select(dim)[slices[i]] for i, dim in enumerate(data.dimensions().keys())][::-1]
        else:
            return data[tuple(slices)]


def instantiate_linear_interpolator(*args, **kwargs):
    """
    Instantiate a linear interpolator using the provided data and scales.

    Parameters
    ----------
    *args : variable length argument list
        The first argument is the data array.
        Subsequent arguments are the scales (coordinate arrays) for each dimension.
    **kwargs : dict
        Additional keyword arguments to pass to `RegularGridInterpolator`.

    Returns
    -------
    RegularGridInterpolator
        An instance of `scipy.interpolate.RegularGridInterpolator` initialized
        with the provided data and scales.

    Notes
    -----
    This function transposes the data array and passes it along with the scales
    to `RegularGridInterpolator`.

    Examples
    --------
    interpolator = instantiate_linear_interpolator(read_hdf_by_value(5., None, None, ifile='path/to/hdf.h5'))

    # Or

    f, x, y = read_hdf_by_value(ifile='path/to/chmap.h5')
    interpolator = instantiate_linear_interpolator(f, x, y, bounds_error=False, fill_value=0)

    """
    except_no_scipy()
    return RegularGridInterpolator(
        values=np.transpose(args[0]),
        points=args[1:],
        **kwargs)


def sp_interpolate_slice_from_hdf(*xi, **kwargs):
    """
    Interpolate a slice from HDF data using SciPy's `RegularGridInterpolator`.

    Parameters
    ----------
    *xi : variable length argument list
        Values or ranges for each dimension to select data.
    **kwargs : dict
        Keyword arguments to pass to `read_hdf_by_value`.

    Returns
    -------
    np.ndarray
        The interpolated data slice.
    list
        A list of scales for the dimensions that were not fixed.

    Notes
    -----
    This function reads data from an HDF file, creates a linear interpolator,
    and interpolates a slice based on the provided values.

    Examples
    --------
    # Fetch radial slice from 3D cube
    slice_2d, t, p = sp_interpolate_slice_from_hdf(10., None, None, ifile='path/to/hdf.h5')

    # Fetch a single point from 2D map
    point_value = sp_interpolate_slice_from_hdf(3.14, 3.14, ifile='path/to/hdf.h5')

    """
    result = read_hdf_by_value(*xi, **kwargs)
    interpolator = instantiate_linear_interpolator(*result)
    grid = [yi[0] if yi[0] is not None else yi[1] for yi in zip(xi, result[1:])]
    slice_ = interpolator(tuple(np.meshgrid(*grid, indexing='ij')))
    indices = [0 if yi is not None else slice(None, None) for yi in xi]
    return slice_[tuple(indices)], *[yi[1] for yi in zip(xi, result[1:]) if yi[0] is None]


def np_interpolate_slice_from_hdf(*xi, by_index=False, **kwargs):
    """
    Interpolate a slice from HDF data using linear interpolation.

    Parameters
    ----------
    *xi : variable length argument list
        Values or ranges for each dimension to select data.
    by_index : bool, optional
        Instead of using the scales for interpolation, use the grid indexes (0 indexed). E.g. to get the average
        of the first two layers in r, you would ask for an index value of 0.5. Default is False.
        (e.g. np_interpolate_slice_from_hdf(0.5, None, None, by_index=True, ifile=ifile) for a 3D file.)
    **kwargs : dict
        Keyword arguments to pass to `read_hdf_by_value`.

    Returns
    -------
    np.ndarray
        The interpolated data slice.

    Raises
    ------
    ValueError
        If the number of dimensions to interpolate over is not supported.

    Notes
    -----
    This function supports linear, bilinear, and trilinear interpolation
    depending on the number of dimensions fixed in `xi`.

    """
    f, *scales = read_hdf_by_value(*xi, **kwargs)
    if by_index is True:
        index_scales = []
        for scale in scales:
            index_scales.append(np.arange(len(scale)))
            scales = tuple(index_scales)
    f_ = np.transpose(f)
    slice_type = sum([yi is not None for yi in xi])
    # match slice_type:
    #     case 1:
    #         return _np_linear_interpolation(xi, scales, f_), *[yi[1] for yi in zip(xi, scales) if yi[0] is None]
    #     case 2:
    #         return _np_bilinear_interpolation(xi, scales, f_), *[yi[1] for yi in zip(xi, scales) if yi[0] is None]
    #     case 3:
    #         return _np_trilinear_interpolation(xi, scales, f_), *[yi[1] for yi in zip(xi, scales) if yi[0] is None]
    #     case _:
    #         raise ValueError("Not a valid number of dimensions for supported linear interpolation methods")
    if slice_type == 1:
        return _np_linear_interpolation(xi, scales, f_).T, *[yi[1] for yi in zip(xi, scales) if yi[0] is None]
    elif slice_type == 2:
        return _np_bilinear_interpolation(xi, scales, f_).T, *[yi[1] for yi in zip(xi, scales) if yi[0] is None]
    elif slice_type == 3:
        return _np_trilinear_interpolation(xi, scales, f_).T, *[yi[1] for yi in zip(xi, scales) if yi[0] is None]
    else:
        raise ValueError("Not a valid number of dimensions for supported linear interpolation methods")


def interpolate_positions_from_hdf(*xi, **kwargs):
    """
    Interpolate at a list of scale positions using SciPy's `RegularGridInterpolator`.

    Parameters
    ----------
    *xi : x0, x1,..., xn : array-like
       Iterable scale values for each dimension of the `n` dimensional dataset.
       Each array should have the same shape *i.e.* (m, ) – the function composes
       these array into a mxn column stack for interpolation.
    **kwargs : dict
        Keyword arguments to pass to `read_hdf_by_value`.

    Returns
    -------
    np.ndarray
        The interpolated values at the provided positions.

    Notes
    -----
    This function reads data from an HDF file, creates a linear interpolator,
    and interpolates at the provided scale values. This function was designed
    for use with mhdweb *viz.* for reading in a list of spacecraft positions
    in r, t, p coordinates and interpolating a MAS file at those locations.

    """
    xi_ = [(np.nanmin(i), np.nanmax(i)) for i in xi]
    f, *scales = read_hdf_by_value(*xi_, **kwargs)
    interpolator = instantiate_linear_interpolator(f, *scales, bounds_error=False)
    return interpolator(np.stack(xi, axis=len(xi[0].shape)))


def interpolate_point_from_1d_slice(xi, scalex, values):
    if scalex[0] > scalex[-1]:
        scalex, values = scalex[::-1], values[::-1]
    xi_ = int(np.searchsorted(scalex, xi))
    sx_ = slice(*_check_index_ranges(len(scalex), xi_, xi_))
    return _np_linear_interpolation([xi], [scalex[sx_]], values[sx_])


def interpolate_point_from_2d_slice(xi, yi, scalex, scaley, values):
    values = np.transpose(values)
    if scalex[0] > scalex[-1]:
        scalex, values = scalex[::-1], values[::-1, :]
    if scaley[0] > scaley[-1]:
        scaley, values = scaley[::-1], values[:, ::-1]
    xi_, yi_ = int(np.searchsorted(scalex, xi)), int(np.searchsorted(scaley, yi))
    sx_, sy_ = slice(*_check_index_ranges(len(scalex), xi_, xi_)), slice(*_check_index_ranges(len(scaley), yi_, yi_))
    return _np_bilinear_interpolation([xi, yi], [scalex[sx_], scaley[sy_]], values[(sx_, sy_)])


def _np_linear_interpolation(xi, scales, values):
    """
    Perform linear interpolation over one dimension.

    Parameters
    ----------
    xi : list
        List of values or None for each dimension.
    scales : list
        List of scales (coordinate arrays) for each dimension.
    values : np.ndarray
        The data array to interpolate.

    Returns
    -------
    np.ndarray
        The interpolated data.

    """
    index0 = next((i for i, v in enumerate(xi) if v is not None), None)
    t = (xi[index0] - scales[index0][0])/(scales[index0][1] - scales[index0][0])
    f0 = [slice(None, None)]*values.ndim
    f1 = [slice(None, None)]*values.ndim
    f0[index0] = 0
    f1[index0] = 1

    return (1 - t)*values[tuple(f0)] + t*values[tuple(f1)]


def _np_bilinear_interpolation(xi, scales, values):
    """
    Perform bilinear interpolation over two dimensions.

    Parameters
    ----------
    xi : list
        List of values or None for each dimension.
    scales : list
        List of scales (coordinate arrays) for each dimension.
    values : np.ndarray
        The data array to interpolate.

    Returns
    -------
    np.ndarray
        The interpolated data.

    """
    index0, index1 = [i for i, v in enumerate(xi) if v is not None]
    t, u = [(xi[i] - scales[i][0])/(scales[i][1] - scales[i][0]) for i in (index0, index1)]

    f00 = [slice(None, None)]*values.ndim
    f10 = [slice(None, None)]*values.ndim
    f01 = [slice(None, None)]*values.ndim
    f11 = [slice(None, None)]*values.ndim
    f00[index0], f00[index1] = 0, 0
    f10[index0], f10[index1] = 1, 0
    f01[index0], f01[index1] = 0, 1
    f11[index0], f11[index1] = 1, 1

    return (
          (1 - t)*(1 - u)*values[tuple(f00)] +
          t*(1 - u)*values[tuple(f10)] +
          (1 - t)*u*values[tuple(f01)] +
          t*u*values[tuple(f11)]
    )


def _np_trilinear_interpolation(xi, scales, values):
    """
    Perform trilinear interpolation over three dimensions.

    Parameters
    ----------
    xi : list
        List of values or None for each dimension.
    scales : list
        List of scales (coordinate arrays) for each dimension.
    values : np.ndarray
        The data array to interpolate.

    Returns
    -------
    np.ndarray
        The interpolated data.

    """
    index0, index1, index2 = [i for i, v in enumerate(xi) if v is not None]
    t, u, v = [(xi[i] - scales[i][0])/(scales[i][1] - scales[i][0]) for i in (index0, index1, index2)]

    f000 = [slice(None, None)]*values.ndim
    f100 = [slice(None, None)]*values.ndim
    f010 = [slice(None, None)]*values.ndim
    f110 = [slice(None, None)]*values.ndim
    f001 = [slice(None, None)]*values.ndim
    f101 = [slice(None, None)]*values.ndim
    f011 = [slice(None, None)]*values.ndim
    f111 = [slice(None, None)]*values.ndim

    f000[index0], f000[index1], f000[index2] = 0, 0, 0
    f100[index0], f100[index1], f100[index2] = 1, 0, 0
    f010[index0], f010[index1], f010[index2] = 0, 1, 0
    f110[index0], f110[index1], f110[index2] = 1, 1, 0
    f001[index0], f001[index1], f001[index2] = 0, 0, 1
    f101[index0], f101[index1], f101[index2] = 1, 0, 1
    f011[index0], f011[index1], f011[index2] = 0, 1, 1
    f111[index0], f111[index1], f111[index2] = 1, 1, 1

    c00 = values[tuple(f000)]*(1 - t) + values[tuple(f100)]*t
    c10 = values[tuple(f010)]*(1 - t) + values[tuple(f110)]*t
    c01 = values[tuple(f001)]*(1 - t) + values[tuple(f101)]*t
    c11 = values[tuple(f011)]*(1 - t) + values[tuple(f111)]*t

    c0 = c00*(1 - u) + c10*u
    c1 = c01*(1 - u) + c11*u

    return c0*(1 - v) + c1*v


def _check_index_ranges(arr_size: int,
                        i0: int,
                        i1: int
                        ) -> Tuple[int, int]:
    """
    Adjust index ranges to ensure they cover at least two indices.

    Parameters
    ----------
    arr_size : int
        The size of the array along the dimension.
    i0 : int
        The starting index.
    i1 : int
        The ending index.

    Returns
    -------
    Tuple[int, int]
        Adjusted starting and ending indices.

    Notes
    -----
    This function ensures that the range between `i0` and `i1` includes at least
    two indices for interpolation purposes.

    """
    if i0 == 0:
        return (i0, i1 + 2) if i1 == 0 else (i0, i1 + 1)
    elif i0 == arr_size:
        return i0 - 2, i1
    else:
        return i0 - 1, i1 + 1
