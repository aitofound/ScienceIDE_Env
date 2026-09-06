#!/usr/bin/env python
"""Download comparison data for AWSoM runs.
"""
__author__ = 'Qusai Al Shidi'
__email__ = 'qusai@umich.edu'

import os
import argparse
import datetime as dt
import swmfpy


def get_time(paramin_file='PARAM.in'):
    """Start and end time for data retrieval.

       Returns:
            Time data starts from param.in file.
    """

    time_command = swmfpy.paramin.read_command('#STARTTIME',
                                               paramin_file)
    start_time = dt.datetime(year=int(time_command[1]),
                             month=int(time_command[2]),
                             day=int(time_command[3]),
                             hour=int(time_command[4]),
                             minute=int(time_command[5]))

    return start_time


if __name__ == '__main__':

    # Program initiation
    PROG_DESCRIPTION = ('Script to automatically download data'
                        + ' and change PARAM.in if needed')
    ARG_PARSER = argparse.ArgumentParser(description=PROG_DESCRIPTION)
    ARG_PARSER.add_argument('-p', '--poynting_flux',
                            help='(default: 1.0e6 J/m^2/s/T)',
                            type=float,
                            default=1.e6)
    ARG_PARSER.add_argument('-i', '--paramin',
                            help='(default: "PARAM.in")'
                            + ' nPARAM.in file to read',
                            default='PARAM.in')
    ARG_PARSER.add_argument('-d', '--domain',
                            help='(default: rMin=1.0, rMax=2.5.)',
                            type=float,
                            default=[1.0, 2.5],
                            nargs=2)
    ARG_PARSER.add_argument('-g', '--grid',
                            help='(default: 400, 180, 180.)',
                            type=int,
                            default=[400, 180, 180],
                            nargs=3)
    ARG_PARSER.add_argument('-m', '--no_hmi',
                            help='Refuse downlaod of hmi vector magnetograms.',
                            action='count',
                            default=0)
    ARG_PARSER.add_argument('-t', '--time',
                            help='(default: Read PARAM.in time.)'
                            + 'Use if you want'
                            + ' to overwrite PARAM.in time.'
                            + ' Format: yyyy mm dd hh',
                            nargs=4,
                            type=int,
                            default=None)
    ARGS = ARG_PARSER.parse_args()

    # Figure out time
    if ARGS.time is None:  # No time given read param.in
        TIME = get_time(ARGS.paramin)
    elif len(ARGS.time) == 4:  # Argument given
        TIME = dt.datetime(ARGS.time[0],
                           ARGS.time[1],
                           ARGS.time[2],
                           ARGS.time[3])
        swmfpy.paramin.replace_command({'#STARTTIME': ARGS.time},
                                       ARGS.paramin,
                                       ARGS.paramin)
    else:
        raise ValueError('Could not understand time given.')

    # Change grid
    CMD_FDIPS_GRID = {
        '#GRID': [[str(ARGS.grid[0]),
                   'nR     (number of cells in the radial direction)'],
                  [str(ARGS.grid[1]),
                   'nTheta (set 0 to use the magnetogram resolution)'],
                  [str(ARGS.grid[2]),
                   'nPhi   (set 0 to use the magnetogram resolution)']]}
    swmfpy.paramin.replace_command(CMD_FDIPS_GRID,
                                   'SC/FDIPS.in',
                                   'SC/FDIPS.in')

    # Change FDIPS domain size
    CMD_DOMAIN = {
        '#DOMAIN':
        [[str(ARGS.domain[0]),
          'rMin         (default is 1)'],
         [str(ARGS.domain[1]),
          'rMax         (default is 2.5)']]}
    swmfpy.paramin.replace_command(CMD_DOMAIN,
                                   'SC/FDIPS.in',
                                   'SC/FDIPS.in')

    # Poynting Flux
    CMD_PFLUX = {'#POYNTINGFLUX': [[str(ARGS.poynting_flux),
                                    'PoyntingFluxPerBSi [J/m^2/s/T]']]}
    swmfpy.paramin.replace_command(CMD_PFLUX,
                                   ARGS.paramin,
                                   ARGS.paramin)

    # HMI vector magnetograms
    if not ARGS.no_hmi:
        print('HMI:', swmfpy.web.download_magnetogram_hmi(TIME,
            'hmi.b_synoptic_small', verbose=True))

    # Download magnetogram and remap
    FILE = swmfpy.web.download_magnetogram_adapt(TIME)[0]  # default 'fixed'
    os.execv('./remap_magnetogram.py', ['__main__', FILE])
    # Done last because it exits current process
