#!/usr/bin/env python
"""Plot the indices based on the SWMF log files.

Plot the IMF, SYM-H, AL, and CPCP of an SWMF geospace run. This needs you to
output the log files like in the SWPC_v2 PARAM file.

Run from command line for help. There may be helpful functions here as a
module.

Usage:
    python plot_indices.py log_YYYMMDD...log geoindex_YYYYMMDD...log
"""
__author__ = 'Qusai Al Shidi'
__email__ = 'qusai@umich.edu'

import argparse
from functools import partial
import os
import numpy as np
import matplotlib
import matplotlib as mpl
from matplotlib.dates import AutoDateLocator, ConciseDateFormatter, date2num
import matplotlib.pyplot as plt
from swmfpy.web import get_omni_data
from swmfpy.io import read_gm_log


def rmse(predictions: np.ndarray, targets: np.ndarray):
    'Calculates root mean square error'
    return np.sqrt(np.nanmean((predictions - targets) ** 2))


def get_omni(logdata):
    """Download and return OMNI data based on the GM log file output `logdata`"""
    return get_omni_data(logdata['times'][0], logdata['times'][-1])


def get_errors(times_data, data_swmf, times_omni, data_omni):
    'Gets the root mean square error from SWMF log and OMNI'
    nonan = np.nonzero(np.isnan(data_omni) == 0)
    omni_times = list(np.array(times_omni)[nonan[0]])
    omni_vals = data_omni[nonan[0]]
    interp_vals = np.interp(date2num(omni_times),
                            date2num(times_data),
                            data_swmf
                            )
    stdev, errors = std_errors(interp_vals, omni_vals)
    return (rmse(omni_vals, interp_vals),
            stdev,
            (omni_times, errors)
            )


def plot_omni(axis: matplotlib.axes.Axes,
              times: np.ndarray,
              data_swmf: np.ndarray,
              data_omni: np.ndarray,
              ylabel=r'$\Delta B$ [nT]',
              **kwargs
              ):
    """Plots swmf dst with sym-h

    Args:
        axis (Axes):
            Matplotlib axes to plot on.
        times ([datetime.datetime]):
            A [start time, end time] sequence.
        data_swmf (numpy.ndarray):
            Simulation data.
        data_omni (numpy.ndarray):
            Observation data.
        ylabel (str):
            Y-axis label.
        kwargs (dict):
            Passed to the matplotlib plot function.

    Returns:
        times_omni ([float]):
            Sequence of times in matplotlib float format.
        data_omni (numpy.ndarray):
            Observation data.
        data_swmf (numpy.ndarray):
            Simulation data.
    """
    times_mpl = [date2num(t) for t in times]
    times_omni = np.linspace(times_mpl[0], times_mpl[-1], len(data_omni))
    axis.plot(times_omni, data_omni, 'k-', alpha=0.5, **kwargs)
    axis.plot(times, data_swmf, 'b-', label='SWMF')
    axis.set_ylabel(ylabel)
    locator = AutoDateLocator()
    axis.xaxis.set_major_locator(locator)
    axis.xaxis.set_major_formatter(ConciseDateFormatter(locator))
    axis.grid(True)
    axis.legend()
    return times_omni, data_omni, data_swmf


def plot_indices(axis, title,
                 times, data_swmf,
                 times_omni, data_omni,
                 units='nT',
                 errors=False,
                 **kwargs):
    'Plots canonical graph'
    if errors:  # Errors are not shown by default, too ugly
        error, stdev, errors = get_errors(times, data_swmf, times_omni, data_omni)
        plot_title = (title + ', rms_error='
                      + str(round(error, 2))
                      + ' ' + units
                      + r', $\sigma$='
                      + str(round(stdev, 2))
                      + ' ' + units)
        plot_error(axis.twinx(), errors)
    else:
        plot_title = title

    axis.set_title(plot_title)
    plot_omni(axis, times, data_swmf, data_omni, **kwargs)
    return axis


def plot_error(axis, errors):
    'Plot errors'
    color, alpha = 'red', 0.2
    times = date2num(errors[0])
    vals = errors[1]
    axis.plot(times, vals, linestyle='-', color=color, alpha=alpha, label='%')
    axis.set_ylabel(r'error [$|\Delta x/\bar{x}_{obs}|$]', color=color)
    axis.set_ylim([0, 3])


def std_errors(x_1, x_2):
    'Find the standard deviation normalized errors'
    delta = np.abs(x_1-x_2)
    mean = np.mean(np.abs(x_2))
    stdev = np.nanstd(np.array([x_1, x_2]))
    return stdev, delta/mean


def ridley_2004(times, pci):
    """Returns CPCP based on Ridley (2004)

    Args:
        start_time (datetime.datetime): The start of the values you want.
        end_time (datetime.datetime): The end of the values you want.

    Returns:
        (numpy.array): Cross Polar Cap Potential based on Ridley (2004)
    """
    t_norm = np.pi/6.*np.array(
        [element.month for element in times], dtype=float)
    return 29.28 - 3.31*np.sin(t_norm+1.49) + 17.81*pci


def newell(velocity, b_y, b_z):
    """Calculate the Newell (2015) function.

    Args:
        velocity (float or ArrayLike):
            Solar wind speed.
        b_y (float or ArrayLike):
            Solar wind magnetic field component in the Y (GSM) direction.
        b_z (float or ArrayLike):
            Solar wind magnetic field component in the Z (GSM) direction.
    
    Returns:
        (float or ArrayLike): $d\\Phi_{MP} / dt$ in Wb/s

    Notes:
        Newell (2015): doi:10.1029/2006JA012015
        Cai and Clauer (2013): doi:10.1002/2013JA018819
    """

    b_total = np.sqrt(b_y**2 + b_z**2)
    clock_angle = np.arctan2(b_y, b_z)

    value = (velocity**2
             * b_total
             * np.sin(0.5*clock_angle)**4
             )

    return 100*np.power(value, 2/3)  # times 100 by Cai and Clauer (2013)


def _interp_nans(x_vals, y_vals):
    'remove nans'
    nonans = np.nonzero(np.isnan(y_vals) == 0)
    return np.interp(x_vals, x_vals[nonans], y_vals[nonans])


def get_newell(omni):
    'get the newell function in certain time'
    omni_mtimes = np.array(date2num(omni['times']))
    interpolater = partial(_interp_nans, omni_mtimes)
    velocity = interpolater(omni['v'])  # m/s
    b_y = interpolater(omni['by'])  # T
    b_z = interpolater(omni['bz'])  # T
    return (omni['times'],
            newell(velocity, b_y, b_z)
            )


def plot_newell(axis, data_omni):
    'Plots the newell function'
    axis.plot(*get_newell(data_omni),
              'k-', label=r'(Newell 2007)+OMNI')
    axis.grid(True)
    axis.legend()
    axis.set_ylabel(r'$\frac{d\Phi_{MP}}{dt} [Wb/s]$')
    axis.axhline(0, color='black')
    axis.set_title('Newell Function')


def plot_solar_wind(axis, data_omni):
    'Plots solar wind magnetic field'
    axis.axhline(0, color='black')
    axis.plot(data_omni['times'], data_omni['bz'], label='$B_z$')
    b_total = np.sqrt(
        data_omni['bz']**2 + data_omni['bx']**2 + data_omni['by']**2)
    axis.plot(data_omni['times'], b_total, label='$|B|$')
    axis.set_ylabel('$B$ [nT]')
    axis.grid(True)
    axis.legend()


def plot_geospace(data_dst, data_al,
                  newell_plot=False,
                  **kwargs):
    """Default plotting script.

    Args:
        data_dst (dict): data from swmfpy.io.read_gm_log() of log*.log data.
        data_al (dict): data from swmfpy.io.read_gm_log() of geoindex*.log data.
        newell_plot (Bool): (default False) First plot is the Newell function,
                            otherwise it is the solar wind magnetic field.

    **kwargs:
        keyword arguments for matplotlib plot functions.

    """
    if kwargs.get('plot_file', False):
        mpl.style.use('fast')

    figure, axes = plt.subplots(4, 1, sharex=True)
    figure.set_size_inches((10, 10))
    figure.set_tight_layout(True)
    if kwargs.get('verbose', False):
        print('Plotting:', str(data_dst['times'][0]))

    # Plot
    data_omni = get_omni_data(data_dst['times'][0],
                              data_dst['times'][-1])
    if newell_plot:
        plot_newell(axes[0], data_omni)
    else:
        plot_solar_wind(axes[0], data_omni)
    plot_indices(axes[1], 'SYM-H',
                 data_dst['times'], data_dst['dst_sm'],
                 data_omni['times'], data_omni['sym_h'],
                 label='SYM-H')
    plot_indices(axes[2], 'Auroral Index (AL)',
                 data_al['times'], data_al['AL'],
                 data_omni['times'], data_omni['al'],
                 label='AL')
    plot_indices(axes[3], 'Cross Polar Cap Potential (CPCP)',
                 data_dst['times'],
                 data_dst['cpcpn'],
                 data_omni['times'],
                 ridley_2004(data_omni['times'], data_omni['pc_n']),
                 ylabel=r'$\Phi [kV]$',
                 label='[Ridley 2004 (PCI_N)]', units='kV')

    # For sanity
    xlims = [date2num(t) for t in (data_omni['times'][0],
                                   data_omni['times'][-1])]
    axes[0].set_xlim(xlims)

    if filename := kwargs.get('plot_file', False):
        plt.close()
        figure.set_size_inches((8,11))
        figure.tight_layout()
        save_dir = kwargs.get('save_dir', '.')
        os.makedirs(save_dir, exist_ok=True)
        if kwargs.get('verbose', False):
            print('Saving:', filename)
        figure.savefig(filename,
                       orientation='portrait',
                       dpi='figure',
                       )
        figure.clear()
        plt.close()
    else:
        plt.show()

    return figure, axes


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Plot SWMF Geospace global indices.')
    parser.add_argument('log_files', nargs=2,
        help=('In order: the log_*.log file and the geoindex*.log file from'
              + ' SWMF output.'))
    parser.add_argument('--plot-file', '-f', type=str,
        help='File name to save to. This will disable showing in a window.')
    parser.add_argument('--verbose', '-v',
        action=argparse.BooleanOptionalAction,
        help=('Verbose output to sandard out.'
              + ' Warnings will still show if this is disabled.'))
    parser.add_argument('--newell', '-n',
        action=argparse.BooleanOptionalAction,
        help='Show the newell function instead of solar wind magnetic field.')
    args = parser.parse_args()
    # set up based on arguments passed in the command-line
    plot_func = partial(plot_geospace,
                        plot_file=args.plot_file, verbose=args.verbose,
                        newell_plot=args.newell)
    plot_func(read_gm_log(args.log_files[0]), read_gm_log(args.log_files[1]))
