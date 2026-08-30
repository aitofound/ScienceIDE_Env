import os
import re
try:
    import numpy as np
    import matplotlib.pyplot as plt
    from matplotlib.transforms import Bbox
    from matplotlib.offsetbox import HPacker, VPacker, TextArea, \
        AnchoredOffsetbox
except:
    pass
try:
    from mpl_toolkits.axes_grid1 import make_axes_locatable
except:
    try:
        # Workaround for broken macOS installation
        import sys
        import matplotlib
        sys.path.append(os.path.join(matplotlib.__path__[0],
                                     '..', 'mpl_toolkits'))
        from axes_grid1 import make_axes_locatable
    except:
        pass
try:
    import builtins
except:
    import __builtin__ as builtins

try:
    import sdf
    got_sdf = True
except ImportError:
    got_sdf = False

try:
    from matplotlib.pyplot import *  # NOQA
    got_mpl = True
except ImportError:
    got_mpl = False


# mpl.rcParams['interactive'] = True
# hold()

global data, t, step, p, ppi, ppe, rho, ei, ee, vx, vy, vz, bx, by, bz
global x, y, z, xc, yc, zc, grid, grid_mid, mult_x, mult_y
global old_mtime, wkdir, verbose, fig, im, cbar

verbose = True

wkdir = 'Data'
old_mtime = 0
old_size = 0
old_filename = ''
cached = False
fig = None
im = None
cbar = None
mult_x = 1
mult_y = 1


class ic_type():
    NEW = 1
    RESTART = 2
    SOD = 3
    SALTZMAN = 4
    NOH = 5
    SEDOV = 6
    BRIO = 7
    WAVE = 8
    ADVECT = 9


def get_si_prefix(scale, full_units=False):
    scale = abs(scale)
    mult = 1
    sym = ''

    if scale < 1e-24:
        full_units = True
    elif scale < 1e-21:
        # yocto
        mult = 1e24
        sym = 'y'
    elif scale < 1e-19:
        # zepto
        mult = 1e21
        sym = 'z'
    elif scale < 1e-16:
        # atto
        mult = 1e18
        sym = 'a'
    elif scale < 1e-13:
        # femto
        mult = 1e15
        sym = 'f'
    elif scale < 1e-10:
        # pico
        mult = 1e12
        sym = 'p'
    elif scale < 1e-7:
        # nano
        mult = 1e9
        sym = 'n'
    elif scale < 1e-4:
        # micro
        mult = 1e6
        sym = '{\mu}'
    elif scale < 1e-1:
        # milli
        mult = 1e3
        sym = 'm'
    elif scale >= 1e27:
        full_units = True
    elif scale >= 1e24:
        # yotta
        mult = 1e-24
        sym = 'Y'
    elif scale >= 1e21:
        # zetta
        mult = 1e-21
        sym = 'Z'
    elif scale >= 1e18:
        # exa
        mult = 1e-18
        sym = 'E'
    elif scale >= 1e15:
        # peta
        mult = 1e-15
        sym = 'P'
    elif scale >= 1e12:
        # tera
        mult = 1e-12
        sym = 'T'
    elif scale >= 1e9:
        # giga
        mult = 1e-9
        sym = 'G'
    elif scale >= 1e6:
        # mega
        mult = 1e-6
        sym = 'M'
    elif scale >= 1e3:
        # kilo
        mult = 1e-3
        sym = 'k'

    if full_units:
        scale = scale * mult
        if scale <= 0:
            pwr = 0
        else:
            pwr = (-np.floor(np.log10(scale)))
        mult = mult * np.power(10.0, pwr)
        if np.rint(pwr) != 0:
            sym = "(10^{%.0f})" % (-pwr) + sym

    return mult, sym


def get_title(var=None, geom=False, extra_info=True):
    global data

    if var is not None:
        if hasattr(var, 'blocklist'):
            data = var.blocklist

    t = data.Header['time']
    mult, sym = get_si_prefix(t)

    stitle = r'$t = {:.3}{}s$'.format(mult * t, sym)

    if not extra_info:
        return stitle

    if hasattr(data, 'Logical_flags'):
        if hasattr(data.Logical_flags, 'use_szp') \
                and data.Logical_flags.use_szp:
            stitle += r', $m_f = {:.1}$'.format(data.Real_flags.m_f)
        if hasattr(data.Logical_flags, 'use_tts') \
                and data.Logical_flags.use_tts:
            stitle += r', TTS'
        if hasattr(data.Logical_flags, 'use_tav') \
                and data.Logical_flags.use_tav:
            stitle += r', Tensor viscosity'
        else:
            if hasattr(data.Logical_flags, 'use_qmono') \
                    and data.Logical_flags.use_qmono:
                stitle += r', Edge viscosity'
            if hasattr(data.Logical_flags, 'use_edge') \
                    and data.Logical_flags.use_edge:
                stitle += r', Edge viscosity'

    if hasattr(data, 'Real_flags'):
        if hasattr(data.Real_flags, 'visc1'):
            stitle += r', $c_1 = ' + str(data.Real_flags.visc1) + '$'
        if hasattr(data.Real_flags, 'visc2'):
            stitle += r'$,\,c_2 = ' + str(data.Real_flags.visc2) + '$'

    if geom:
        if hasattr(data, 'Logical_flags'):
            if hasattr(data.Logical_flags, 'use_rz') \
                    and data.Logical_flags.use_rz:
                stitle += r', R-Z'
            else:
                stitle += r', Cartesian'

            if hasattr(data.Logical_flags, 'polar_grid') \
                    and data.Logical_flags.polar_grid:
                stitle += r' Polar'

    return stitle


def get_default_iso(data):
    iso = True
    if hasattr(data, 'Integer_flags') \
            and hasattr(data.Integer_flags, 'ic_type'):
        ic = data.Integer_flags.ic_type
        if ic == ic_type.NOH or ic == ic_type.SEDOV:
            iso = True
    return iso


def get_file_list(wkd=None, base=None, block=None):
    """Get a list of SDF filenames containing sequence numbers

       Parameters
       ----------
       wkd : str
           The directory in which to search
           If no other keyword arguments are passed, then the code will
           automatically attempt to detect if this field is base or block
       base : str
           A representative filename or directory
       block : sdf.Block or sdf.BlockList
           A representative sdf dataset or block

       Returns
       -------
       file_list : str array
           An array of filenames
    """
    import os.path
    import glob
    global wkdir

    if wkd is not None:
        if os.path.isdir(wkd):
            wkdir = wkd
        elif os.path.exists(wkd):
            base = wkd
        elif isinstance(wkd, sdf.BlockList) \
                or isinstance(wkd, sdf.Block) or type(wkd) is dict:
            block = wkd

    if base is None and block is not None:
        if hasattr(block, 'blocklist'):
            bl = block.blocklist
            if hasattr(bl, 'Header') and 'filename' in bl.Header:
                base = bl.Header['filename']
        elif hasattr(block, 'Header') and 'filename' in block.Header:
            base = block.Header['filename']

    if base is not None:
        if os.path.isfile(base[0]):
            apath = os.path.abspath(base[0])
        else:
            apath = os.path.abspath(base)
        wkdir = os.path.dirname(apath)
        flist = glob.glob(wkdir + "/*.sdf")
        flist.remove(apath)
        flist = [apath] + sorted(flist)
    else:
        flist = glob.glob(wkdir + "/*[0-9][0-9]*.sdf")
        if len(flist) == 0:
            flist = glob.glob("*[0-9][0-9]*.sdf")
        flist = sorted(flist)

    return flist


def get_job_id(file_list=None, base=None, block=None):
    """Get a representative job ID for a list of files

       Parameters
       ----------
       file_list : str list
           A list of filenames to search
           If no other keyword arguments are passed, then the code will
           automatically attempt to detect if this field is base or block
       base : str
           A representative filename or directory
       block : sdf.Block or sdf.BlockList
           A representative sdf dataset or block

       Returns
       -------
       job_id : str
           The job ID
    """

    if file_list is not None and type(file_list) is not list:
        if os.path.exists(file_list):
            base = file_list
            file_list = None
        elif isinstance(file_list, sdf.BlockList) \
                or isinstance(file_list, sdf.Block) or type(file_list) is dict:
            block = file_list
            file_list = None

    if block is not None and base is None:
        if hasattr(block, 'blocklist'):
            bl = block.blocklist
            if hasattr(bl, 'Header') and 'filename' in bl.Header:
                base = bl.Header['filename']
        elif hasattr(block, 'Header') and 'filename' in block.Header:
            base = block.Header['filename']

    if base is not None:
        try:
            data = sdf.read(base, mmap=0)
            if len(data.__dict__) > 1:
                return data.Header['jobid1']
        except:
            pass

    # Find the job id
    if file_list is not None:
        for f in file_list:
            try:
                data = sdf.read(f, mmap=0)
                if len(data.__dict__) < 2:
                    continue
                return data.Header['jobid1']
            except:
                pass

    return None


def get_files(wkd=None, base=None, block=None, varname=None, fast=True):
    """Get a list of SDF filenames belonging to the same run

       Parameters
       ----------
       wkd : str
           The directory in which to search
           If no other keyword arguments are passed, then the code will
           automatically attempt to detect if this field is base or block
       base : str
           A representative filename or directory
       block : sdf.Block or sdf.BlockList
           A representative sdf dataset or block
       varname : str
           A variable name that must be present in the file
       fast : bool
           Assume that files follow strict datestamp ordering and exit once
           the first file that doesn't match the job ID

       Returns
       -------
       file_list : str array
           An array of filenames
    """

    if wkd is not None:
        if os.path.isdir(wkd):
            pass
        elif os.path.exists(wkd):
            base = wkd
        elif isinstance(wkd, sdf.BlockList) \
                or isinstance(wkd, sdf.Block) or type(wkd) is dict:
            block = wkd

    if block is not None and base is None:
        if hasattr(block, 'blocklist'):
            bl = block.blocklist
            if hasattr(bl, 'Header') and 'filename' in bl.Header:
                base = bl.Header['filename']
        elif hasattr(block, 'Header') and 'filename' in block.Header:
            base = block.Header['filename']

    flist = get_file_list(wkd=wkd, base=base)
    flist.sort(key=lambda x: os.path.getmtime(x))

    job_id = get_job_id(flist, base=base, block=block)

    # Add all files matching the job id
    file_list = []
    for f in reversed(flist):
        try:
            data = sdf.read(f, mmap=0, dict=True)
            if len(data) < 2:
                continue
            file_job_id = data['Header']['jobid1']
            if file_job_id == job_id:
                if varname is None:
                    file_list.append(f)
                elif varname in data:
                    file_list.append(f)
            elif len(file_list) > 0:
                break
        except:
            pass

    return list(reversed(file_list))


def get_time(time=0, first=False, last=False, wkd=None, base=None, block=None,
             fast=True):
    """Get an SDF dataset that matches a given time

       Parameters
       ----------
       time : float
           The time to search for. If specified then the dateset that is
           closest to this time will be returned
       first : bool
           If set to True then the dataset with the earliest simulation time
           will be returned
       last : bool
           If set to True then the dataset with the latest simulation time
           will be returned
       wkd : str
           The directory in which to search
       base : str
           A representative filename or directory
       block : sdf.Block or sdf.BlockList
           A representative sdf dataset or block
       fast : bool
           Use a faster but less thorough method for returning first/last

       Returns
       -------
       data : sdf.BlockList
           An SDF dataset
    """
    global data, wkdir

    flist = get_file_list(wkd=wkd, base=base, block=block)

    if len(flist) == 0:
        print("No SDF files found")
        return

    flist.sort(key=lambda x: os.path.getmtime(x))
    job_id = get_job_id(flist, base=base, block=block)

    if time is None and not first:
        last = True

    t = None
    fname = None
    if last:
        flist = list(reversed(flist))
        t_old = -1e90
    else:
        t_old = 1e90

    for f in flist:
        dat_tmp = sdf.read(f)
        if len(dat_tmp.__dict__) < 2:
            continue
        if job_id != dat_tmp.Header['jobid1']:
            continue

        t = dat_tmp.Header['time']
        if last:
            if fast:
                fname = f
                break
            if t > t_old:
                fname = f
                t_old = t
        elif first:
            if fast:
                fname = f
                break
            if t < t_old:
                fname = f
                t_old = t
        else:
            td = abs(t - time)
            if td < t_old:
                t_old = td
                fname = f
                if td < 1e-30:
                    # Exact match found. No need to search further
                    break

    if fname is None:
        raise Exception("No valid file found in directory: " + wkdir)

    data = getdata(fname, verbose=False)
    return data


def get_step(step=0, first=False, last=False, wkd=None, base=None, block=None,
             fast=True):
    """Get an SDF dataset that matches a given step

       Parameters
       ----------
       step : int
           The time to search for. If specified then the dateset that is
           closest to this time will be returned
       first : bool
           If set to True then the dataset with the earliest simulation step
           will be returned
       last : bool
           If set to True then the dataset with the latest simulation step
           will be returned
       wkd : str
           The directory in which to search
       base : str
           A representative filename or directory
       block : sdf.Block or sdf.BlockList
           A representative sdf dataset or block
       fast : bool
           Use a faster but less thorough method for returning first/last

       Returns
       -------
       data : sdf.BlockList
           An SDF dataset
    """
    global data, wkdir

    flist = get_file_list(wkd=wkd, base=base, block=block)

    if len(flist) == 0:
        print("No SDF files found")
        return

    flist.sort(key=lambda x: os.path.getmtime(x))
    job_id = get_job_id(flist, base=base, block=block)

    if step is None and not first:
        last = True

    t = None
    fname = None
    if last:
        flist = list(reversed(flist))
        t_old = -1e90
    else:
        t_old = 1e90

    for f in flist:
        dat_tmp = sdf.read(f)
        if len(dat_tmp.__dict__) < 2:
            continue
        if job_id != dat_tmp.Header['jobid1']:
            continue

        t = dat_tmp.Header['step']
        if last:
            if fast:
                fname = f
                break
            if t > t_old:
                fname = f
                t_old = t
        elif first:
            if fast:
                fname = f
                break
            if t < t_old:
                fname = f
                t_old = t
        else:
            td = abs(t - step)
            if td < t_old:
                t_old = td
                fname = f
                if td == 0:
                    # Exact match found. No need to search further
                    break

    if fname is None:
        raise Exception("No valid file found in directory: " + wkdir)

    data = getdata(fname, verbose=False)
    return data


def get_latest(wkd=None, base=None, block=None):
    """Get the latest SDF dataset in a directory

       Parameters
       ----------
       wkd : str
           The directory in which to search
           If no other keyword arguments are passed, then the code will
           automatically attempt to detect if this field is base or block
       base : str
           A representative filename or directory
       block : sdf.Block or sdf.BlockList
           A representative sdf dataset or block

       Returns
       -------
       data : sdf.BlockList
           An SDF dataset
    """
    return get_step(last=True, wkd=wkd, base=base, block=base)


def get_first(every=False, wkd=None, base=None, block=None):
    if not every and not base:
        base = get_newest_file(wkd=wkd, block=block)
    return get_step(first=True, wkd=wkd, base=base, block=base)


def get_last(every=False, wkd=None, base=None, block=None):
    if not every and not base:
        base = get_newest_file(wkd=wkd, block=block)
    return get_step(last=True, wkd=wkd, base=base, block=base)


def get_oldest_file(wkd=None, base=None, block=None):
    import os.path

    flist = get_file_list(wkd=wkd, base=base, block=block)
    flist.sort(key=lambda x: os.path.getmtime(x))
    for n in range(len(flist)):
        f = flist[n]
        data = sdf.read(f, mmap=0, dict=True)
        if len(data) > 1:
            return f

    return None


def get_newest_file(wkd=None, base=None, block=None):
    import os.path

    flist = get_file_list(wkd=wkd, base=base, block=block)
    flist.sort(key=lambda x: os.path.getmtime(x))
    for n in range(len(flist)):
        f = flist[-n-1]
        data = sdf.read(f, mmap=0, dict=True)
        if len(data) > 1:
            return f

    return None


def get_oldest(**kwargs):
    return getdata(get_oldest_file(**kwargs), verbose=False)


def get_newest(**kwargs):
    return getdata(get_newest_file(**kwargs), verbose=False)


def set_wkdir(wkd):
    global wkdir
    wkdir = wkd


def get_wkdir():
    global wkdir
    return wkdir


def sdfr(filename):
    return sdf.read(filename)


def plot_auto(*args, **kwargs):
    try:
        dims = args[0].dims
    except:
        print('error: Variable cannot be auto determined. '
              + 'Use plot1d or plot2d')
        return
    if (len(dims) == 1):
        if type(args[0]) is sdf.BlockStitchedPath:
            plot_rays(*args, **kwargs)
        elif (len(args[0].grid.dims) == 1):
            plot1d(*args, **kwargs)
        else:
            plot_path(*args, **kwargs)
    elif (len(dims) == 2):
        k = 'set_ylabel'
        if k in kwargs:
            del kwargs[k]
        plot2d(*args, **kwargs)
    elif len(dims) == 3 and \
            ('ix' in kwargs or 'iy' in kwargs or 'iz' in kwargs):
        k = 'set_ylabel'
        if k in kwargs:
            del kwargs[k]
        plot2d(*args, **kwargs)
    else:
        print('error: Unable to plot variables of this dimensionality')


def oplot_auto(*args, **kwargs):
    kwargs['set_ylabel'] = False
    kwargs['hold'] = True
    plot_auto(*args, **kwargs)


def oplot1d(*args, **kwargs):
    kwargs['set_ylabel'] = False
    kwargs['hold'] = True
    plot1d(*args, **kwargs)


def plot1d(var, fmt=None, xdir=None, idx=-1, xscale=0, yscale=0, scale=0,
           cgs=False, title=True, sym=True, set_ylabel=True, hold=True,
           subplot=None, figure=None, **kwargs):
    global data
    global x, y, mult_x, mult_y

    if len(var.dims) != 1 and len(var.dims) != 2:
        print("error: Not a 1d dataset")
        return

    if subplot is None:
        if figure is None:
            figure = plt.gcf()
            if len(figure.get_axes()) == 0:
                hold = False
            # Only clear the figure if one isn't supplied by the user
            if not hold:
                try:
                    figure.clf()
                except:
                    pass
        elif not hold:
            figure.clf()
        # Have to add subplot after clearing figure
        subplot = plt.subplot(111)
    elif figure is None:
        figure = subplot.figure
        if not hold:
            subplot.clear()
            if hasattr(subplot, 'colorbar'):
                subplot.colorbar.remove()

    if var.dims[0] == var.grid.dims[0]:
        grid = var.grid
    else:
        grid = var.grid_mid

    if len(var.dims) > 1:
        if xdir is None:
            if var.dims[1] < var.dims[0]:
                xdir = 0
            else:
                xdir = 1
        if xdir == 0:
            if idx == -1:
                idx = int(var.dims[1] / 2)
            s = (slice(None), idx)
        else:
            if idx == -1:
                idx = int(var.dims[0] / 2)
            s = (idx, slice(None))
        Y = var.data[s]
    else:
        xdir = 0
        Y = var.data

    if np.ndim(var.grid.data[0]) == 1:
        X = grid.data[xdir]
    else:
        X = grid.data[xdir][s]

    if xdir is None:
        xdir = 0

    if scale > 0:
        xscale = scale
        yscale = scale

    # This is an attempt to get both axes to use the same scale
    # I'm not at all confident that it always does the right thing
    if xscale == 0 and yscale == 0:
        xlen = max(abs(X[0]), abs(X[-1]))
        ylen = max(abs(Y[0]), abs(Y[-1]))
        ratio1 = ylen / xlen
        ratio2 = xlen / ylen
        if ratio1 >= 1 and ratio1 < 1e3:
            xscale = xlen
            yscale = xlen
        elif ratio2 >= 1 and ratio2 < 1e3:
            xscale = ylen
            yscale = ylen

    if xscale == 0:
        length = max(abs(X[0]), abs(X[-1]))
        mult_x, sym_x = get_si_prefix(length)
    else:
        mult_x, sym_x = get_si_prefix(xscale)

    if yscale == 0:
        length = max(abs(Y[0]), abs(Y[-1]))
        mult_y, sym_y = get_si_prefix(length)
    else:
        mult_y, sym_y = get_si_prefix(yscale)

    X = mult_x * X
    Y = mult_y * Y

    if fmt:
        subplot.plot(X, Y, fmt, **kwargs)
    else:
        subplot.plot(X, Y, **kwargs)

    subplot.set_xlabel(grid.labels[xdir] + ' $('
                       + escape_latex(sym_x + grid.units[xdir]) + ')$')
    if set_ylabel:
        subplot.set_ylabel(var.name + ' $('
                           + escape_latex(sym_y + var.units) + ')$')

    if title:
        subplot.set_title(get_title(var), fontsize='large', y=1.03)

    figure.set_tight_layout(True)
    figure.canvas.draw()


def plot_path(var, xdir=None, ydir=None, xscale=0, yscale=0, scale=0,
              title=True, hold=True, subplot=None, figure=None, iso=True,
              add_cbar=True, cbar_label=True, cbar_wd=5, cbar_top=False,
              svar=None, update=True, axis_only=False, clip_reflect=False,
              power=(-3,3), **kwargs):
    """Plot an SDF path variable (eg. a laser ray)

       Parameters
       ----------
       var : sdf.Block
           The SDF block for the path to plot
       xdir : integer
           The dimension to use for the x-direction. If the path is 2D then
           ydir will automatically be selected.
       ydir : integer
           The dimension to use for the y-direction. If the path is 2D then
           xdir will automatically be selected.
       xscale : real
           Value to use for scaling the x-axis. If not set then the x-axis will
           be scaled automatically. Set this to 1 to disable scaling.
       yscale : real
           Value to use for scaling the y-axis. If not set then the x-axis will
           be scaled automatically. Set this to 1 to disable scaling.
       scale : real
           Value to use for scaling both axes. If not set then the axes will
           be scaled automatically. Set this to 1 to disable scaling.
       title : logical or string
           If set to False, don't add a title to the plot.
           Otherwise, add the specified value if it is a string or
           automatically generate one if it is just True.
       hold : logical
           If True, do not clear the figure before plotting.
       subplot :
           The subplot to use for plotting
       figure :
           The figure to use for plotting
       iso : logical
           If True, generate an isotropic plot (x/y axis equal).
       add_cbar : logical
           Add a colorbar if true
       cbar_label : logical or string
           If set to False, don't add a label to the colorbar.
           Otherwise, add the specified value if it is a string or
           automatically generate one if it is just True.
       cbar_wd : integer
           The width of the colorbar
       cbar_top : logical
           If set to true, the colorbar is plotted along the top of the figure
           instead of the right-hand side
       update : logical
           If set to true then update the axis limits for this path
       axis_only : logical
           If set to true then only update the axis limits for this path
       svar : sdf.Block
           If set, use the extents of this variable to set the axis range for
           this plot
       clip_reflect : logical
           If set to true, then rays are clipped at the point where the path
           gradient is either zero or huge
       power : sequence
           Sets size thresholds for scientific notation.
           Two-element sequence passed throuth to set_powerlimits()

       **kwargs : dict
           All other keyword arguments are passed to matplotlib plotting
           routine
    """
    import matplotlib.pyplot as plt
    from matplotlib.collections import LineCollection
    global data
    global x, y, mult_x, mult_y

    if 'norm_values' not in plot_path.__dict__:
        plot_path.norm_values = None
        plot_path.axis = None

    if axis_only:
        if plot_path.axis is None:
            return
        if subplot is None:
            if figure is None:
                figure = plt.gcf()
            subplot = plt.subplot(111)
        elif figure is None:
            figure = subplot.figure
        subplot.axis(plot_path.axis)
        figure.set_tight_layout(True)
        figure.canvas.draw()
        return

    if len(var.dims) != 1:
        print("error: Not a 1d dataset")
        return

    if len(plt.get_fignums()) == 0:
        hold = False

    if subplot is None:
        if figure is None:
            figure = plt.gcf()
            if len(figure.get_axes()) == 0:
                hold = False
            # Only clear the figure if one isn't supplied by the user
            if not hold:
                try:
                    figure.clf()
                except:
                    pass
        elif not hold:
            figure.clf()
        # Have to add subplot after clearing figure
        subplot = plt.subplot(111)
    elif figure is None:
        figure = subplot.figure
        if not hold:
            subplot.clear()
            if hasattr(subplot, 'colorbar'):
                subplot.colorbar.remove()

    if not hold:
        plot_path.norm_values = None
        plot_path.axis = None

    #if var.dims[0] == var.grid.dims[0]:
    #    grid = var.grid
    #else:
    #    grid = var.grid_mid
    grid = var.grid

    test_dir = False
    if xdir is None:
        xdir = 0
    else:
        test_dir = True

    if ydir is None:
        ydir = 1
    else:
        test_dir = True

    if test_dir:
        dimensions = np.shape(grid.data)[0]
        xdir = xdir % dimensions
        ydir = ydir % dimensions
        if xdir == ydir:
            ydir = (xdir + 1) % dimensions

    X = grid.data[xdir]
    Y = grid.data[ydir]

    if not hold:
        if scale > 0:
            xscale = scale
            yscale = scale

        # This is an attempt to get both axes to use the same scale
        # I'm not at all confident that it always does the right thing
        if xscale == 0 and yscale == 0:
            xlen = max(abs(X[0]), abs(X[-1]))
            ylen = max(abs(Y[0]), abs(Y[-1]))
            ratio1 = ylen / xlen
            ratio2 = xlen / ylen
            if ratio1 >= 1 and ratio1 < 1e3:
                xscale = xlen
                yscale = xlen
            elif ratio2 >= 1 and ratio2 < 1e3:
                xscale = ylen
                yscale = ylen

        if xscale == 0:
            length = max(abs(X[0]), abs(X[-1]))
            mult_x, sym_x = get_si_prefix(length)
        else:
            mult_x, sym_x = get_si_prefix(xscale)

        if yscale == 0:
            length = max(abs(Y[0]), abs(Y[-1]))
            mult_y, sym_y = get_si_prefix(length)
        else:
            mult_y, sym_y = get_si_prefix(yscale)

    X = mult_x * X
    Y = mult_y * Y
    c = var.data

    if clip_reflect:
        g = (X[0] - X[1]) / (Y[0] - Y[1])
        for i in range(len(c) - 1):
            ddx = X[i] - X[i+1]
            ddy = Y[i] - Y[i+1]
            if abs(ddy) < 1e-16:
                grad = 100
            else:
                grad = ddx / ddy / g
            if abs(grad) > 10 or grad < 0:
                X = np.copy(X[:i+1])
                Y = np.copy(Y[:i+1])
                c = np.copy(c[:i+1])
                break

    points = np.array([X, Y]).T.reshape(-1, 1, 2)
    segments = np.concatenate([points[:-1], points[1:]], axis=1)

    if not hold or plot_path.norm_values is None:
        k = 'vmin'
        k1 = 'vrange'
        if k in kwargs:
            vmin = kwargs[k]
        elif k1 in kwargs:
            vmin = kwargs[k1][0]
        else:
            vmin = c.min()

        k = 'vmax'
        if k in kwargs:
            vmax = kwargs[k]
        elif k1 in kwargs:
            vmax = kwargs[k1][1]
        else:
            vmax = c.max()

        plot_path.norm_values = plt.Normalize(vmin, vmax)

    kk = {}
    k = 'lw'
    if k in kwargs:
        kk[k] = kwargs[k]
    k = 'linewidth'
    if k in kwargs:
        kk[k] = kwargs[k]
    lc = LineCollection(segments, norm=plot_path.norm_values, **kk)
    lc.set_array(c)
    im = subplot.add_collection(lc)

    if not hold:
        subplot.set_xlabel(grid.labels[xdir] + ' $('
                           + escape_latex(sym_x + grid.units[xdir]) + ')$')
        subplot.set_ylabel(grid.labels[ydir] + ' $('
                           + escape_latex(sym_y + grid.units[ydir]) + ')$')

        if title:
            if type(title) is str:
                title_label = title
            else:
                title_label = get_title(var, extra_info=False)
            subplot.set_title(title_label, fontsize='large', y=1.03)

        subplot.axis('tight')
        if iso:
            subplot.axis('image')

        plot_path.axis = subplot.axis()
        if update:
            subplot.axis([X.min(), X.max(), Y.min(), Y.max()])
            plot_path.axis = [X.min(), X.max(), Y.min(), Y.max()]

    if not hold and add_cbar:
        ax = subplot.axes
        ca = subplot
        divider = make_axes_locatable(ca)
        pad = int(0.6 * cbar_wd + 0.5)
        if cbar_top:
            cax = divider.append_axes("top", "%i%%" % cbar_wd,
                                      pad="%i%%" % pad)
            cbar = figure.colorbar(im, orientation='horizontal',
                                   cax=cax, ax=ax)
            cax.xaxis.set_ticks_position('top')
        else:
            cax = divider.append_axes("right", "%i%%" % cbar_wd,
                                      pad="%i%%" % pad)
            cbar = figure.colorbar(im, cax=cax, ax=ax)
        try:
            cbar.formatter.set_powerlimits(power)
            cbar.update_ticks()
        except:
            pass
        subplot.colorbar = cax
        plt.sca(ax)
        if cbar_label:
            if type(cbar_label) is str:
                var_label = cbar_label
            else:
                var_label = var.name + ' $(' + escape_latex(var.units) + ')$'
            if cbar_top:
                cbar.set_label(var_label, fontsize='large')
                cax.xaxis.set_label_position('top')
            else:
                cbar.set_label(var_label, fontsize='large', x=1.2)

    if not hold and svar is not None:
        lim = np.array(svar.grid.extents)
        lim[0] *= mult_x
        lim[1] *= mult_x
        lim[2] *= mult_y
        lim[3] *= mult_y
        subplot.axis([lim[0], lim[2], lim[1], lim[3]])
    elif hold:
        if plot_path.axis is None:
            plot_path.axis = [X.min(), X.max(), Y.min(), Y.max()]
        lims = plot_path.axis
        if lims is None:
            lim = [X.min(), X.max(), Y.min(), Y.max()]
        else:
            lim = list(lims)
            v = X.min()
            if v < lim[0]:
                lim[0] = v
            v = X.max()
            if v > lim[1]:
                lim[1] = v
            v = Y.min()
            if v < lim[2]:
                lim[2] = v
            v = Y.max()
            if v > lim[3]:
                lim[3] = v
        plot_path.axis = lim
        if update:
            subplot.axis(lim)

    if update:
        figure.set_tight_layout(True)
        figure.canvas.draw()


def plot_rays(var, skip=1, rays=None, **kwargs):
    """Plot all rays found in an SDF file

       Parameters
       ----------
       var : sdf.Block
           The SDF variable for the rays to plot
       skip : integer
           Number of rays to skip before selecting the next one to plot
    """

    ray_start = -1
    l = 'ray_start'
    if l in kwargs:
        ray_start = kwargs[l]

    ray_stop = 1e9
    l = 'ray_stop'
    if l in kwargs:
        ray_stop = kwargs[l]

    if type(var) is sdf.BlockStitchedPath:
        v = var.data[0]
        l = '_label'
        if l not in kwargs:
            kwargs[l] = var.name

        if type(v) is sdf.BlockStitchedPath:
            for v in var.data:
                plot_rays(v, skip=skip, rays=rays, **kwargs)
                kwargs['hold'] = True
            return

        k = 'cbar_label'
        if k not in kwargs or (kwargs[k] and type(kwargs[k]) != str):
            kwargs[k] = kwargs[l] + ' $(' + escape_latex(v.units) + ')$'
            del kwargs[l]

        k0 = 'vmin'
        k1 = 'vmax'
        k = 'vrange'
        if k not in kwargs and not (k0 in kwargs and k1 in kwargs):
            v = var.data[0]
            vmin = v.data.min()
            vmax = v.data.max()
            for iray, v in enumerate(var.data):
                if iray < ray_start:
                    continue
                if iray > ray_stop:
                    break
                if iray%skip == 0:
                    vmin = min(vmin, v.data.min())
                    vmax = max(vmax, v.data.max())
            if k0 not in kwargs:
                kwargs[k0] = vmin
            if k1 not in kwargs:
                kwargs[k1] = vmax

        for iray, v in enumerate(var.data):
            if iray < ray_start:
                continue
            if iray > ray_stop:
                break
            if iray%skip == 0:
                plot_auto(v, update=False, **kwargs)
                kwargs['hold'] = True

        plot_auto(var.data[0], axis_only=True, **kwargs)
        kwargs['hold'] = True

        return

    split_name = var.name.split('/')
    start = split_name[0] + '_'
    end = '_' + split_name[-1]
    data = var.blocklist.__dict__

    k0 = 'vmin'
    k1 = 'vmax'
    k = 'vrange'
    if k not in kwargs and not (k0 in kwargs and k1 in kwargs):
        vmin = var.data.min()
        vmax = var.data.max()
        iray = -1
        for k in data.keys():
            if k.startswith(start) and k.endswith(end):
                iray += 1
                if iray < ray_start:
                    continue
                if iray > ray_stop:
                    break
                if iray%skip == 0:
                    vmin = min(vmin, data[k].data.min())
                    vmax = max(vmax, data[k].data.max())
        if k0 not in kwargs:
            kwargs[k0] = vmin
        if k1 not in kwargs:
            kwargs[k1] = vmax

    k = 'cbar_label'
    if k not in kwargs or (kwargs[k] and type(kwargs[k]) != str):
        # Remove /Ray[1-9]+ from the name
        kwargs[k] = '/'.join([split_name[0]] + split_name[2:]) \
                     + ' $(' + escape_latex(var.units) + ')$'

    iray = -1
    for k in data.keys():
        if k.startswith(start) and k.endswith(end):
            iray += 1
            if iray < ray_start:
                continue
            if iray > ray_stop:
                break
            if iray%skip == 0:
                plot_auto(data[k], hold=True, update=False, **kwargs)
                kwargs['hold'] = True

    plot_auto(var, axis_only=True, **kwargs)
    kwargs['hold'] = True


def oplot2d(*args, **kwargs):
    kwargs['hold'] = True
    plot2d(*args, **kwargs)


def plot2d_array(array, x, y, extents, var_label, xlabel, ylabel, idx=None,
                 iso=None, fast=None, title=True, full=True, vrange=None,
                 reflect=0, norm=None, hold=True, xscale=0, yscale=0, scale=0,
                 figure=None, subplot=None, add_cbar=True, cbar_label=True,
                 cbar_wd=5, cbar_top=False, power=(-3,3), **kwargs):
    import matplotlib as mpl
    global data, fig, im, cbar
    global mult_x, mult_y

    if idx is None:
        i0 = 0
        i1 = 1
        i2 = 2
        i3 = 3
    else:
        i0 = idx[0]
        i1 = idx[1]
        i2 = idx[2]
        i3 = idx[3]

    cmap = plt.get_cmap()

    if norm is not None:
        v0 = np.min(array) - norm
        v1 = np.max(array) - norm

        if abs(v0/v1) > 1:
            low = 0
            high = 0.5 * (1 - v1/v0)
        else:
            low = 0.5 * (1 + v0/v1)
            high = 1.0

        cmap = mpl.colors.LinearSegmentedColormap.from_list(
            'tr', cmap(np.linspace(low, high, 256)))

    if subplot is None:
        if figure is None:
            figure = plt.gcf()
            if len(figure.get_axes()) == 0:
                hold = False
            # Only clear the figure if one isn't supplied by the user
            if not hold:
                try:
                    figure.clf()
                except:
                    pass
        elif not hold:
            figure.clf()
        # Have to add subplot after clearing figure
        subplot = plt.subplot(111)
    elif figure is None:
        figure = subplot.figure
        if not hold:
            subplot.clear()
            if hasattr(subplot, 'colorbar'):
                subplot.colorbar.remove()

    if iso is None:
        iso = get_default_iso(data)

    if scale > 0:
        xscale = scale
        yscale = scale

    ext = extents[:]
    # This is an attempt to get both axes to use the same scale
    # I'm not at all confident that it always does the right thing
    if xscale == 0 and yscale == 0:
        xlen = max(abs(extents[i2]), abs(extents[i0]))
        ylen = max(abs(extents[i3]), abs(extents[i1]))
        ratio1 = ylen / xlen
        ratio2 = xlen / ylen
        if ratio1 >= 1 and ratio1 < 1e3:
            xscale = xlen
            yscale = xlen
        elif ratio2 >= 1 and ratio2 < 1e3:
            xscale = ylen
            yscale = ylen

    if xscale == 0:
        length = max(abs(extents[i2]), abs(extents[i0]))
        mult_x, sym_x = get_si_prefix(length)
    else:
        mult_x, sym_x = get_si_prefix(xscale)

    if yscale == 0:
        length = max(abs(extents[i3]), abs(extents[i1]))
        mult_y, sym_y = get_si_prefix(length)
    else:
        mult_y, sym_y = get_si_prefix(yscale)

    if vrange == 1:
        v = np.max(abs(array))
        vrange = [-v, v]

    if fast:
        if reflect == 1:
            # about x=0
            ext[i0] = 2 * extents[i0] - extents[i2]
            array = np.vstack((np.flipud(array), array))
        elif reflect == 2:
            # about y=0
            ext[i1] = 2 * extents[i1] - extents[i3]
            array = np.hstack((np.fliplr(array), array))
        elif reflect == 3:
            # about x=0, y=0
            ext[i0] = 2 * extents[i0] - extents[i2]
            ext[i1] = 2 * extents[i1] - extents[i3]
            array = np.vstack((np.flipud(array), array))
            array = np.hstack((np.fliplr(array), array))

    if np.ndim(x) == 1:
        if fast is None:
            fast = True

        if not fast:
            Y, X = np.meshgrid(y, x)
    else:
        if fast is None:
            fast = False

        if not fast:
            X = x
            Y = y

    if fast:
        ext[i0] = mult_x * ext[i0]
        ext[i1] = mult_y * ext[i1]
        ext[i2] = mult_x * ext[i2]
        ext[i3] = mult_y * ext[i3]
        e = ext[i1]
        ext[i1] = ext[i2]
        ext[i2] = e
        ext = [ext[i0], ext[i1], ext[i2], ext[i3]]
        k = 'interpolation'
        if k not in kwargs:
            kwargs[k] = 'none'
        if vrange is None:
            im = subplot.imshow(array.T, origin='lower', extent=ext, cmap=cmap,
                                **kwargs)
        else:
            im = subplot.imshow(array.T, origin='lower', extent=ext, cmap=cmap,
                                vmin=vrange[0], vmax=vrange[1], **kwargs)
    else:
        X = np.multiply(mult_x, X)
        Y = np.multiply(mult_y, Y)
        if vrange is None:
            im = subplot.pcolormesh(X, Y, array, cmap=cmap, **kwargs)
        else:
            im = subplot.pcolormesh(X, Y, array, cmap=cmap,
                                    vmin=vrange[0], vmax=vrange[1], **kwargs)

    subplot.set_xlabel(xlabel)
    subplot.set_ylabel(ylabel)

    title_text = None
    if full:
        if add_cbar and cbar_label:
            title_text = get_title()
        else:
            title_text = var_label + ', ' + get_title()
    elif title:
        if not (add_cbar and cbar_label):
            title_text = var_label

    if title and title_text:
        subplot.set_title(title_text, fontsize='large', y=1.03)

    subplot.axis('tight')
    if iso:
        subplot.axis('image')

    if not hold and add_cbar:
        ax = subplot.axes
        ca = subplot
        divider = make_axes_locatable(ca)
        pad = int(0.6 * cbar_wd + 0.5)
        if cbar_top:
            cax = divider.append_axes("top", "%i%%" % cbar_wd,
                                      pad="%i%%" % pad)
            cbar = figure.colorbar(im, orientation='horizontal',
                                   cax=cax, ax=ax)
            cax.xaxis.set_ticks_position('top')
        else:
            cax = divider.append_axes("right", "%i%%" % cbar_wd,
                                      pad="%i%%" % pad)
            cbar = figure.colorbar(im, cax=cax, ax=ax)
        try:
            cbar.formatter.set_powerlimits(power)
            cbar.update_ticks()
        except:
            pass
        subplot.colorbar = cax
        plt.sca(ax)
        if cbar_label:
            if type(cbar_label) is str:
                var_label = cbar_label
            if cbar_top:
                cbar.set_label(var_label, fontsize='large')
                cax.xaxis.set_label_position('top')
            else:
                cbar.set_label(var_label, fontsize='large', x=1.2)
    figure.canvas.draw()

    figure.set_tight_layout(True)
    figure.canvas.draw()


def plot2d(var, iso=None, fast=None, title=True, full=True, vrange=None,
           ix=None, iy=None, iz=None, reflect=0, norm=None, irange=None,
           jrange=None, hold=True, xscale=0, yscale=0, scale=0, figure=None,
           subplot=None, add_cbar=True, cbar_label=True, cbar_top=False,
           **kwargs):
    global data, fig, im, cbar
    global x, y, mult_x, mult_y

    if type(irange) is list or type(irange) is tuple:
        si = slice(*irange)
    else:
        si = slice(None, irange)

    if type(jrange) is list or type(jrange) is tuple:
        sj = slice(*jrange)
    else:
        sj = slice(None, jrange)

    i0 = 0
    i1 = 1
    if len(var.dims) == 3:
        if ix is not None:
            if iz is None:
                if ix < 0:
                    ix = int(var.dims[0] / 2)
                i0 = 1
                i1 = 2
                ss = ix, si, sj
            else:
                if ix < 0:
                    ix = int(var.dims[2] / 2)
                i0 = 0
                i1 = 2
                ss = si, sj, ix
        elif iy is not None:
            if iy < 0:
                iy = int(var.dims[1] / 2)
            i0 = 0
            i1 = 2
            ss = si, iy, sj
            if iz is not None:
                i0 = 0
                i1 = 1
                ss = si, iy, sj
        elif iz is not None:
            if iz < 0:
                iz = int(var.dims[2] / 2)
            i0 = 0
            i1 = 1
            ss = si, sj, iz
        else:
            print("error: Not a 2d dataset")
            return
        i2 = i0 + 3
        i3 = i1 + 3
    elif len(var.dims) != 2:
        print("error: Not a 2d dataset")
        return
    else:
        i2 = i0 + 2
        i3 = i1 + 2
        ss = si, sj

    array = var.data[ss]
    if np.ndim(var.grid.data[0]) == 1:
        x = var.grid.data[i0][si]
        y = var.grid.data[i1][sj]
    else:
        x = var.grid.data[i0][ss]
        y = var.grid.data[i1][ss]

    idx = [i0, i1, i2, i3]

    if scale > 0:
        xscale = scale
        yscale = scale

    extents = list(var.grid.extents)
    # This is an attempt to get both axes to use the same scale
    # I'm not at all confident that it always does the right thing
    if xscale == 0 and yscale == 0:
        xlen = max(abs(extents[i2]), abs(extents[i0]))
        ylen = max(abs(extents[i3]), abs(extents[i1]))
        ratio1 = ylen / xlen
        ratio2 = xlen / ylen
        if ratio1 >= 1 and ratio1 < 1e3:
            xscale = xlen
            yscale = xlen
        elif ratio2 >= 1 and ratio2 < 1e3:
            xscale = ylen
            yscale = ylen

    if xscale == 0:
        length = max(abs(extents[i2]), abs(extents[i0]))
        mult_x, sym_x = get_si_prefix(length)
    else:
        mult_x, sym_x = get_si_prefix(xscale)

    if yscale == 0:
        length = max(abs(extents[i3]), abs(extents[i1]))
        mult_y, sym_y = get_si_prefix(length)
    else:
        mult_y, sym_y = get_si_prefix(yscale)

    xlabel = var.grid.labels[i0] \
        + ' $(' + escape_latex(sym_x + var.grid.units[i0]) + ')$'
    ylabel = var.grid.labels[i1] \
        + ' $(' + escape_latex(sym_y + var.grid.units[i1]) + ')$'

    var_label = var.name + ' $(' + escape_latex(var.units) + ')$'

    if hasattr(var, 'blocklist'):
        data = var.blocklist

    plot2d_array(array=array, x=x, y=y, extents=extents, var_label=var_label,
                 xlabel=xlabel, ylabel=ylabel, idx=idx, iso=iso, fast=fast,
                 title=title, full=full, vrange=vrange, reflect=reflect,
                 norm=norm, hold=hold, xscale=xscale, yscale=yscale,
                 scale=scale, figure=figure, subplot=subplot,
                 add_cbar=add_cbar, cbar_label=cbar_label, cbar_top=cbar_top,
                 **kwargs)


def plot2d_update(var):
    global fig, im

    im.set_array(var.ravel())
    im.autoscale()
    fig.canvas.draw()


def plot_levels(var, r0=None, r1=None, nl=10, iso=None, out=False,
                title=True, levels=True, **kwargs):
    global data

    try:
        plt.clf()
    except:
        pass

    if iso is None:
        if hasattr(var, 'blocklist'):
            data = var.blocklist
        iso = get_default_iso(data)

    if np.ndim(var.grid.data[0]) == 1:
        X, Y = np.meshgrid(var.grid.data[1], var.grid.data[0])
    else:
        if tuple(var.grid.dims) == tuple(var.dims):
            X = var.grid.data[0]
            Y = var.grid.data[1]
        else:
            X = var.grid_mid.data[0]
            Y = var.grid_mid.data[1]

    if r0 is None:
        r0 = np.min(var.data)
        r1 = np.max(var.data)
        dr = (r1 - r0) / (nl + 1)
        r0 += dr
        r1 -= dr

    rl = r0 + 1.0 * (r1 - r0) * np.array(range(nl)) / (nl - 1)

    fig = plt.gcf()
    if out:
        gs = plt.GridSpec(1, 1)  # , width_ratios=[8, 1])
        ax = plt.subplot(gs[0])
    else:
        ax = plt.gca()

    k = 'colors'
    if k not in kwargs:
        kwargs[k] = 'k'
    k = 'linewidths'
    if k not in kwargs:
        kwargs[k] = 0.5
    k = 'levels'
    if k not in kwargs:
        kwargs[k] = rl

    cs = ax.contour(X, Y, var.data, **kwargs)

    if levels:
        fmt = {}
        for l, i in zip(cs.levels, range(1, len(cs.levels)+1)):
            fmt[l] = str(i)

        sidx = ""
        slvl = ""
        for l, i in reversed(list(zip(cs.levels, range(1, len(cs.levels)+1)))):
            # sidx += rtn + "%i" % i
            # slvl += rtn + "%-6.4g" % l
            # rtn = "\n"
            sidx += "%i\n" % i
            slvl += "%-6.4g\n" % l

        t1 = TextArea('Level', textprops=dict(color='k', fontsize='small'))
        t2 = TextArea(sidx, textprops=dict(color='k', fontsize='small'))
        tl = VPacker(children=[t1, t2], align="center", pad=0, sep=2)

        lname = var.name.replace("_node", "")
        t3 = TextArea(lname, textprops=dict(color='k', fontsize='small'))
        t4 = TextArea(slvl, textprops=dict(color='k', fontsize='small'))
        tr = VPacker(children=[t3, t4], align="center", pad=0, sep=2)

        t = HPacker(children=[tl, tr], align="center", pad=0, sep=8)

        if out:
            t = AnchoredOffsetbox(loc=2, child=t, pad=.4,
                                  bbox_to_anchor=(1.01, 1), frameon=True,
                                  bbox_transform=ax.transAxes, borderpad=0)
        else:
            t = AnchoredOffsetbox(loc=1, child=t, pad=.4,
                                  bbox_to_anchor=(1, 1), frameon=True,
                                  bbox_transform=ax.transAxes, borderpad=.4)
        t.set_clip_on(False)
        ax.add_artist(t)

        plt.clabel(cs, cs.levels, fmt=fmt, inline_spacing=2, fontsize=8)

    ax.set_xlabel(var.grid.labels[0] + ' $('
                  + escape_latex(var.grid.units[0]) + ')$')
    ax.set_ylabel(var.grid.labels[1] + ' $('
                  + escape_latex(var.grid.units[1]) + ')$')

    if title:
        if out:
            # suptitle(get_title(), fontsize='large')
            plt.suptitle(get_title(var), fontsize='large', y=0.92)
        else:
            plt.title(get_title(var), fontsize='large', y=1.03)

    plt.axis('tight')
    if iso:
        plt.axis('image')

    plt.draw()
    if out:
        gs.tight_layout(fig, rect=[0, -0.01, 0.95, 0.92])
        # fw = fig.get_window_extent().width
        # tw = fw*.06+t1.get_children()[0].get_window_extent().width \
        #     + t3.get_children()[0].get_window_extent().width
        # print(1-tw/fw)
        # gs.update(right=1-tw/fw)
        # ax.set_position([box.x0, box.y0, box.width + bw, box.height])
    else:
        fig.set_tight_layout(True)
    plt.draw()


def plot_contour(var, r0=None, r1=None, nl=10, iso=None, title=True, **kwargs):
    return plot_levels(var, r0=r0, r1=r1, nl=nl, iso=iso, out=False,
                       title=title, levels=False, **kwargs)


def getdata(fname, wkd=None, verbose=True, squeeze=False):
    global data, t, step, p, ppi, ppe, rho, ei, ee, vx, vy, vz, bx, by, bz
    global x, y, z, xc, yc, zc, grid, grid_mid
    global old_mtime, old_filename, old_size, cached
    global wkdir

    if wkd is not None:
        wkdir = wkd

    if isinstance(fname, int):
        try:
            filename = wkdir + "/%0.4i.sdf" % fname
        except:
            filename = wkdir + "/" + fname
    else:
        filename = fname

    try:
        st = os.stat(filename)
    except OSError as e:
        try:
            filename = "./%0.4i.sdf" % fname
        except:
            filename = "./" + fname
        try:
            st = os.stat(filename)
            wkdir = '.'
        except OSError as e:
            print("ERROR opening file {0}: {1}".format(filename, e.strerror))
            raise

    if st.st_mtime != old_mtime or st.st_size != old_size \
            or filename != old_filename:
        if verbose:
            print("Reading file " + filename)
        data = sdf.read(filename)
        old_mtime = st.st_mtime
        old_size = st.st_size
        old_filename = filename
    else:
        cached = True
        return data

    cached = False

    if squeeze:
        for key, value in data.__dict__.items():
            # Remove single element dimensions
            try:
                dims = []
                for element in value.dims:
                    dims.append([0, element-1])
                subarray(value, dims)
            except:
                pass

    sdfdict = {}
    for key, value in data.__dict__.items():
        if hasattr(value, "id"):
            sdfdict[value.id] = value
        else:
            sdfdict[key] = value

    fdict = {}
    table = {'time': 't'}
    k = 'Header'
    if k in sdfdict:
        h = sdfdict[k]
        k = list(table.keys())[0]
        if k in h:
            key = table[k]
            var = h[k]
            if verbose:
                print(key + str(np.shape(var)) + ' = ' + k)
            fdict[key] = var
            globals()[key] = var
            builtins.__dict__[key] = var

    table = {'Pressure': 'p',
             'Pressure_ion': 'ppi',
             'Pressure_electron': 'ppe',
             'Rho': 'rho',
             'Energy_ion': 'ei',
             'Energy_electron': 'ee',
             'Vx': 'vx',
             'Vy': 'vy',
             'Vz': 'vz',
             'Vr': 'vx',
             'VTheta': 'vz',
             'Bx': 'bx',
             'By': 'by',
             'Bz': 'bz',
             'Br': 'bx',
             'Bt': 'bz',
             'bx': 'bx',
             'by': 'by',
             'bz': 'bz',
             'ex': 'ex',
             'ey': 'ey',
             'ez': 'ez',
             'jx': 'jx',
             'jy': 'jy',
             'jz': 'jz'}

    rz = False
    if 'Vr' in sdfdict:
        rz = True

    if rz:
        table['Vz'] = 'vy'
        table['Bz'] = 'by'

    inv_table = {}
    for k, v in table.items():
        inv_table[v] = inv_table.get(v, [])
        inv_table[v].append(k)

    for k in table:
        if k in sdfdict:
            key = table[k]
            if hasattr(sdfdict[k], "data"):
                var = sdfdict[k].data
            else:
                var = sdfdict[k]
            dims = str(tuple(int(i) for i in sdfdict[k].dims))
            if verbose:
                print(key + dims + ' = ' + k)
            fdict[key] = var
            globals()[key] = var
            builtins.__dict__[key] = var

    k = 'grid'
    if k in sdfdict:
        vargrid = sdfdict[k]
        grid = vargrid
        keys = 'x', 'y', 'z'
        for n in range(np.size(vargrid.dims)):
            key = keys[n]
            var = vargrid.data[n]
            dims = str(tuple(int(i) for i in sdfdict[k].dims))
            if verbose:
                print(key + dims + ' = ' + k)
            fdict[key] = var
            globals()[key] = var
            builtins.__dict__[key] = var

    k = 'grid_mid'
    if k in sdfdict:
        vargrid = sdfdict[k]
        grid_mid = vargrid
        keys = 'xc', 'yc', 'zc'
        for n in range(np.size(vargrid.dims)):
            key = keys[n]
            var = vargrid.data[n]
            dims = str(tuple(int(i) for i in sdfdict[k].dims))
            if verbose:
                print(key + dims + ' = ' + k)
            fdict[key] = var
            globals()[key] = var
            builtins.__dict__[key] = var

    # Export particle arrays
    for k, value in data.__dict__.items():
        if type(value) != sdf.BlockPointVariable \
                and type(value) != sdf.BlockPointMesh:
            continue
        key = re.sub(r'[^a-z0-9]', '_', value.id.lower())
        if hasattr(value, "data"):
            var = value.data
        else:
            var = value
        dims = str(tuple(int(i) for i in value.dims))
        if type(value) == sdf.BlockPointVariable:
            if verbose:
                print(key + dims + ' = ' + value.name)
            fdict[key] = var
            globals()[key] = var
            builtins.__dict__[key] = var
        else:
            vargrid = value
            grid = vargrid
            keys = 'x', 'y', 'z'
            for n in range(np.size(value.dims)):
                gkey = keys[n] + '_' + key
                var = value.data[n]
                dims = str(tuple(int(i) for i in value.dims))
                if verbose:
                    print(gkey + dims + ' = ' + k + ' ' + keys[n])
                fdict[gkey] = var
                globals()[gkey] = var
                builtins.__dict__[gkey] = var

    # X, Y = np.meshgrid(x, y)
    return data


def ogrid(skip=None, **kwargs):
    global x, y, mult_x, mult_y
    if np.ndim(x) == 1:
        X, Y = np.meshgrid(x, y)
    else:
        s = slice(None, None, skip)
        X = x[s, s]
        Y = y[s, s]
    X = np.multiply(mult_x, X)
    Y = np.multiply(mult_y, Y)
    k = 'lw'
    if k not in kwargs and 'linewidth' not in kwargs:
        kwargs[k] = 0.5
    k = 'color'
    if k not in kwargs:
        kwargs[k] = 'k'
    plt.plot(X, Y, **kwargs)
    plt.plot(X.transpose(), Y.transpose(), **kwargs)


def plotgrid(fname=None, iso=None, title=True):
    if type(fname) is sdf.BlockList or type(fname) is dict:
        dat = fname
    elif fname is not None:
        dat = getdata(fname, verbose=verbose)

    if iso is None:
        iso = get_default_iso(dat)

    ogrid()

    ax = plt.gca()

    ax.set_xlabel(grid.labels[0] + ' $(' + escape_latex(grid.units[0]) + ')$')
    ax.set_ylabel(grid.labels[1] + ' $(' + escape_latex(grid.units[1]) + ')$')

    if title:
        plt.title(get_title(), fontsize='large', y=1.03)

    plt.axis('tight')
    if iso:
        plt.axis('image')

    plt.draw()

    fig = plt.gcf()
    fig.set_tight_layout(True)
    plt.draw()


def axis_offset(boxed=False):
    ax = plt.gca()
    xlab = ax.get_xlabel()
    ylab = ax.get_ylabel()

    f = 1e-3

    # for o in ax.findobj():
    for l in ax.get_lines():
        bb = l.get_clip_box()
        bb._bbox = Bbox([[-f, -f], [1+2*f, 1+2*f]])
        l.set_clip_box(bb)
        # l.set_clip_on(False)

    if boxed:
        r = matplotlib.patches.Rectangle((-f, -f), 1+2*f, 1+2*f,
                                         transform=ax.transAxes)
        r.set_color((0, 0, 0, 0))
        r.set_edgecolor('k')
        r.set_clip_on(False)
        ax.add_patch(r)

    w = 1.1
    gap = 8
    ax.spines['top'].set_visible(False)
    ax.spines['right'].set_visible(False)
    ax.spines['left'].set_position(('outward', gap))
    ax.spines['left'].set_linewidth(w)
    ax.spines['bottom'].set_position(('outward', gap))
    ax.spines['bottom'].set_linewidth(w)
    ax.tick_params(direction='out', width=w, length=4.5, top='off',
                   right='off')
    ax.set_xlabel(xlab)
    ax.set_ylabel(ylab)

    plt.draw()


def tuple_to_slice(slices):
    subscripts = []
    for val in slices:
        start = val[0]
        end = val[1]
        if end is not None:
            end = end + 1
        subscripts.append(slice(start, end, 1))
    subscripts = tuple(subscripts)
    return subscripts


def subarray(base, slices):
    if (len(slices) != len(base.dims)):
        print("Must specify a range in all dimensions")
        return None
    dims = []
    # Construct the lengths of the subarray
    for x in range(0, len(slices)):
        begin = slices[x][0]
        end = slices[x][1]
        if begin is None:
            begin = 0
        if end is None:
            end = base.dims[x]
        if (end-begin != 0):
            dims.append(end-begin+1)

    subscripts = tuple_to_slice(slices)
    base.data = np.squeeze(base.data[subscripts])
    base.dims = tuple(dims)


def list_variables(data):
    dct = data.__dict__
    for key in sorted(dct):
        try:
            val = dct[key]
            print('{} {} {}'.format(key, type(val),
                  np.array2string(np.array(val.dims), separator=', ')))
        except:
            pass


def escape_latex(string):
    return string.replace('%', '\%')


pi = 3.141592653589793238462643383279503
q0 = 1.602176565e-19
m0 = 9.10938291e-31
c = 2.99792458e8
kb = 1.3806488e-23
mu0 = pi * 4e-7
epsilon0 = 1.0 / mu0 / c**2
h_planck = 6.62606957e-34
h_bar = h_planck / 2.0 / pi
