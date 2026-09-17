#!/usr/bin/env python3
"""Prepares a geospace run by downloading inputs.

You need swmfpy installed.
Installation:
    pip install -U --user \
        git+https://github.com/SWMFsoftware/swmfpy.git@master

Usage:
    python prepare_geospace.py
    # To avoid F10.7 change in PARAM.in
    python prepare_geospace.py --no_write_f107
    # To avoid writing an IMF.dat solar wind file
    python prepare_geospace.py --no_write_imf
"""
__author__ = 'Qusai Al Shidi'
__email__ = 'qusai@umich.edu'

import argparse
import datetime as dt
from operator import itemgetter
import numpy as np
from swmfpy import write_imf_from_omni, paramin
from swmfpy.web import get_omni_data
DESC = """It makes the IMF input file (IMF.dat) and also replaces the F10.7
value in the PARAM.in file.
This uses an *already prepared* PARAM.in files to work with. If start and end
times aren't specified then it reads it from the PARAM.in file.
Please run in the same directory as your PARAM.in file (run dir).
"""
__doc__ += DESC


def arguments():
    """Make arguments"""
    parser = argparse.ArgumentParser(description=DESC)

    parser.add_argument(
        '-t0', '--start_time',
        help="""The start time of the solar wind.
This well replace PARAM.in values.
If times are not specified this reads the PARAM.in file.""",
        metavar='year month day',
        nargs='+',
        type=int
        )

    parser.add_argument(
        '-t1', '--end_time',
        help="""The end time of the solar wind.
This well replace PARAM.in values.
If times are not specified this reads the PARAM.in file.""",
        metavar='year month day',
        nargs='+',
        type=int
        )

    parser.add_argument(
        '-v', '--verbose',
        help="Output verbose messages.",
        action="store_true",
        )

    parser.add_argument(
        '-s', '--no_write_imf',
        help="Don't write IMF.dat (s)olar wind data",
        action="store_true",
        )

    parser.add_argument(
        '-l', '--no_write_f107',
        help="Don't write F10.7 f(l)ux value in the PARAM.in files",
        action="store_true",
        )

    parser.add_argument(
        '-f', '--paramin',
        help="PARAM.in file to read and write F10.7 value.",
        type=str,
        metavar='PARAM.in',
        default="PARAM.in"
        )

    return parser.parse_args()


def get_times(paramin_file='PARAM.in'):
    """Start and end time for data retrieval.

       Returns:
            Time data starts from param.in file.
    """

    time_command = paramin.read_command('#STARTTIME',
                                        paramin_file)
    start_time = dt.datetime(*list(map(int, time_command[1:6])))

    time_command = paramin.read_command('#ENDTIME',
                                        paramin_file)
    end_time = dt.datetime(*list(map(int, time_command[1:6])))

    return (start_time, end_time)


def vprint(arg, *msg):
    "Verbose print of message"
    if arg.verbose:
        print(*msg)


def write_times(times, paramin_file='PARAM.in'):
    """Changes the times in the paramin file
    """
    change = {"#STARTTIME": [], "#ENDTIME": []}
    time_desc = [
        'year',
        'month',
        'day',
        'hour',
        'minute',
        'second',
        ]

    def time_as_list(thetime):
        'return time as list of str'
        raw = [thetime.year,
               thetime.month,
               thetime.day,
               thetime.hour,
               thetime.minute,
               thetime.second,
               ]
        return list(map(str, raw))

    change['#STARTTIME'] = zip(time_as_list(times[0]),
                               time_desc)
    change['#ENDTIME'] = zip(time_as_list(times[1]),
                             time_desc)
    paramin.replace_command(change, paramin_file)


IMF_KEYS = ['times',
            'bx', 'by_gse', 'bz_gse',
            'vx_gse', 'vy_gse', 'vz_gse',
            'density', 'temperature'
            ]


def omni_largest_gap(omni_data):
    """Returns largest time gap in omni data"""
    # Zip all important values together
    raw = zip(*[omni_data[key] for key in IMF_KEYS])
    filtered = filter(
        lambda x: all([
            not np.isnan(elem)
            for elem in itemgetter(*list(range(1, len(IMF_KEYS))))(x)
                ]),
        raw
        )
    times = np.array(list(map(itemgetter(0), filtered)))
    return str(max(np.diff(times)))


def omni_integrity(omni_data):
    """Returns percent of omni that is complete"""
    num_of_nans = sum(np.isnan(omni_data[key]).sum() for key in IMF_KEYS[1:])
    num_of_values = sum(omni_data[key].size for key in IMF_KEYS[1:])
    integrity = str(
        round((num_of_values - num_of_nans) / num_of_values * 100, 2)
        )
    return integrity + '%'


def _main():
    """The hidden main block"""
    args = arguments()

    # Time handling
    if args.start_time and args.end_time:
        start_time = dt.datetime(*args.start_time)
        end_time = dt.datetime(*args.end_time)
        times = (start_time, end_time)
        write_times(times, args.paramin)
    elif args.start_time or args.end_time:
        raise ValueError('Please specify both start and end times')
    else:
        times = get_times(args.paramin)
    vprint(args, 'Start time:', times[0], '| End Time:', times[1])

    if not args.no_write_imf:
        vprint(args, 'Getting omni data..')
        omni_data = write_imf_from_omni(*times, verbose=args.verbose)
        vprint(args, 'Omni data integrity:', omni_integrity(omni_data))
        vprint(args, 'Omni largest time gap:', omni_largest_gap(omni_data))
        vprint(args, 'IMF.dat written')

    iono_descriptions = [
        'TypeConductanceModel',
        'UseFullCurrent',
        'UseFakeRegion2',
        'F107Flux',
        'StarLightPedConductance',
        'PolarCapPedConductance',
        ]
    if not args.no_write_f107:
        omni_lores = get_omni_data(*times, resolution='low')
        iono_command = paramin.read_command('#IONOSPHERE', args.paramin)
        change = {}
        iono_command[4] = '-1.0'  # Use Param/f107.txt
        change[iono_command[0]] = map(
            list,
            zip(iono_command[1:], iono_descriptions)
            )
        paramin.replace_command(change, args.paramin)
        vprint(args, 'F10.7 using Param/f107.txt in PARAM.in file')


if __name__ == '__main__':
    _main()
