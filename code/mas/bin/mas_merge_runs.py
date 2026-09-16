#!/usr/bin/env python3
import sys
import os
import signal
import argparse
import re
import numpy as np

# Version 2.0.0
#
# - Added option to use relatie links instead of absolute ones.
#   The method for doing so is not robust for all cases.

# TODO: 
#  - Add subfolders option to make nice subfolders for everything
#  - Add slice number detection to do all slices, and write out slice list
#  - Add non-tp slices
#  - Add more error checking
#  - Add restart sequence
#  - Add custom sub-list of fields?

def signal_handler(signal, frame):
        print('You pressed Ctrl+C! Stopping!')
        sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

def argParsing():

    parser = argparse.ArgumentParser(description='PSI Merge MAS runs.')

    parser.add_argument('-rundirs',
                        help='Comma-seperated list of directories of MAS runs IN ORDER.',
                        dest='rundirs',
                        type=str,
                        required=True)

    parser.add_argument('-runnames',
                        help='Comma separated list of run names of MAS runs IN ORDER.  I.E. the names such that the input file is i<name> and the output file is o<name> etc.  The default is to assume all runs have a run name of mas (i.e. imas, omas).',
                        dest='runnames',
                        type=str,
                        required=False)

    parser.add_argument('-outdir',
                        help='Directory for output of merged run.',
                        dest='outdir',
                        type=str,
                        default='merged_mas_run',
                        required=False)

    parser.add_argument('-rel',
                        help='Use relative links instead of absolute.  WANRING!  This is not robust and may not always work!',
                        dest='use_rel',
                        action='store_true',
                        default=False,
                        required=False)

    return parser.parse_args()


## Get input agruments:
args = argParsing()
arg_dict = vars(args)

rundirs = arg_dict['rundirs'].split(',')

num_runs = len(rundirs)

if (args.runnames is not None):
    runnames = arg_dict['runnames'].split(',')
else:
    runnames = ['mas'] * num_runs

# Make output directory
try:
    os.makedirs(args.outdir, exist_ok=True)
except OSError as error:
    print('ERROR!  Cannot create output directory '+args.outdir)
    sys.exit(1)

os.makedirs(args.outdir+"/run_logs", exist_ok=True)

outdir_full = os.path.realpath(args.outdir)

# Loop over MAS runs:
run_i = -1
idx_3d = 0
idx_slice = 0
idx_tracer = 0

for rundir in rundirs:

    run_i = run_i + 1
    runname = runnames[run_i]

    if (not os.path.isdir(rundir)):
        print('ERROR!  Run directory '+rundir+' does not exist!')
        sys.exit(1)

    # Get full path of current run:
    rundir_full = os.path.realpath(rundir)

    # Get relative path of rundir with respect to outdir
    # WARNING!  This is not robust in some cases!
    common_dir_run_out = os.path.commonprefix([rundir_full,outdir_full])
    rundir_rel = os.path.relpath(rundir_full, common_dir_run_out)

    hdf_fmt='.h5'

    # Copy run logs:
    rfldstr = "run%06d" % (run_i+1)
    os.makedirs(outdir_full+"/run_logs/"+rfldstr, exist_ok=True)
    os.popen('cp '+rundir_full+'/?'+runname+' '+outdir_full+'/run_logs/'+rfldstr+'/ 2>/dev/null')
    os.popen('cp '+rundir_full+'/*.log '+outdir_full+'/run_logs/'+rfldstr+'/ 2>/dev/null')
    os.popen('cp '+rundir_full+'/*.err '+outdir_full+'/run_logs/'+rfldstr+'/ 2>/dev/null')
    os.popen('cp '+rundir_full+'/*.txt '+outdir_full+'/run_logs/'+rfldstr+'/ 2>/dev/null')
    os.popen('cp '+rundir_full+'/*.out '+outdir_full+'/run_logs/'+rfldstr+'/ 2>/dev/null') 
    os.popen('cp '+rundir_full+'/submit* '+outdir_full+'/run_logs/'+rfldstr+'/ 2>/dev/null')

    #History files merge:
    if (run_i == 0):

        hist_h_file = open(outdir_full+'/hmas','w')
        with open(rundir+'/h'+runname,"r") as file:
            for line in file:
                header = line.replace('\n','')
                print(header,file=hist_h_file)
                break

        hist_v_file = open(outdir_full+'/vmas','w')
        with open(rundir+'/v'+runname,"r") as file:
            for line in file:
                header = line.replace('\n','')
                print(header,file=hist_v_file)
                break
        
    if (run_i < num_runs-1):
    # Get first time of next run:
        with open(rundirs[run_i+1]+'/h'+runnames[run_i+1]) as file:
            tmp = file.readline().replace('\n','')
            tmp = file.readline().replace('\n','')
            next_time = float(tmp.split()[0])
    else:
        next_time = np.Infinity

    with open(rundir+'/h'+runname,"r") as file:
        for line in file.readlines()[1:]:
            tmp = str(line)
            tmp = tmp.split()
            curr_time = tmp[0].replace('\n','')
            if (float(curr_time) < next_time):
                str_to_add = line.replace('\n','')
                print(str_to_add,file=hist_h_file)

    with open(rundir+'/v'+runname,"r") as file:
        for line in file.readlines()[1:]:
            tmp = str(line)
            tmp = tmp.split()
            curr_time = tmp[0].replace('\n','')
            if (float(curr_time) < next_time):
                str_to_add = line.replace('\n','')
                print(str_to_add,file=hist_v_file)

    # Were 3D dumps output?
    mas_dumped_3d = os.path.isfile(rundir+'/mas_dumps_3d.txt')
    if (mas_dumped_3d):

       # Make new dump file if first run:
       if (run_i == 0):
           dump_3d_file = open(outdir_full+'/mas_dumps_3d.txt','w')

       # Get list of fields.
       with open(rundir+'/i'+runname,"r") as file:
           for line in file:
               if re.search('\splotlist', line):
                   field_list_3d = line.replace('\'','').\
                                        replace('plotlist','').\
                                        replace('=','').\
                                        replace('\n','').\
                                        replace(' ','').\
                                        split(',')

    # Were tracers output?
    mas_dumped_tracers = os.path.isfile(rundir+'/mas_dumps_tracers.txt')
    if (mas_dumped_tracers):
       # Make new dump file if first run:
       if (run_i == 0):
           dump_tracer_file = open(outdir_full+'/mas_dumps_tracers.txt','w')

    # Were slices output?
    mas_dumped_slices = os.path.isfile(rundir+'/mas_dumps_slices.txt')
    if (mas_dumped_slices):

       # Make new dump file if first run:
       if (run_i == 0):
           dump_slice_file = open(outdir_full+'/mas_dumps_slices.txt','w')    

        # Get list of fields.
       with open(rundir+'/i'+runname,"r") as file:
           for line in file:
               if re.search('slice_plotlist', line):
                   field_list_slice = line.replace('\'','').\
                                           replace('slice_plotlist','').\
                                           replace('=','').\
                                           replace('\n','').\
                                           replace(' ','').\
                                           split(',')

    # Check input file for number of digits for files:
    dfmt = "%03d"
    with open(rundir+'/i'+runname,"r") as file:
        for line in file:
            if re.search('long_sequence_numbers', line):
                 digit_str = line.replace('long_sequence_numbers','').\
                                  replace('.','').\
                                  replace(' ','').\
                                  replace('\n','').\
                                  replace('=','')
                 if (digit_str == "true" or digit_str == "TRUE"):
                     dfmt = "%06d"
                 else:
                     dfmt = "%03d"

    # Now loop through 3D file list and create links and new out file.

    if (mas_dumped_3d):
        if (run_i < num_runs-1):
            # Get first time of next run:
            with open(rundirs[run_i+1]+'/mas_dumps_3d.txt') as file:
                tmp = file.readline().replace('\n','')
                next_time = float(tmp.split()[1])
        else:
            next_time = np.Infinity
        with open(rundir+'/mas_dumps_3d.txt') as file:
            for line in file:
                tmp = str(line)
                idx_tmp = tmp.split()
                curr_time = idx_tmp[1].replace('\n','')
                curr_idx = idx_tmp[0].replace('\n','')
                if (float(curr_time) < next_time):
                    idx_3d = idx_3d + 1
                    str_to_add = (dfmt+"    "+curr_time) % int(idx_3d)                
                    # Write the line to the new dump file:
                    print(str_to_add,file=dump_3d_file)
                    # Make relative soft links for all fields:
                    for field in field_list_3d:
                       filestr = str(field+dfmt+hdf_fmt)
                       fileout = outdir_full+'/'+filestr % int(idx_3d)
                       if (args.use_rel):
                           filein = rundir_rel+'/'+filestr % int(curr_idx)
                       else:
                           filein = rundir_full+'/'+filestr % int(curr_idx)
                       try: 
                           os.remove(fileout)
                       except OSError as error:
                           pass
                       os.symlink(filein,fileout)

    if (mas_dumped_tracers):
        if (run_i < num_runs-1):
            # Get first time of next run:
            with open(rundirs[run_i+1]+'/mas_dumps_tracers.txt') as file:
                tmp = file.readline().replace('\n','')
                next_time = float(tmp.split()[1])
        else:
            next_time = np.Infinity
        with open(rundir+'/mas_dumps_tracers.txt') as file:
            for line in file:
                tmp = str(line)
                idx_tmp = tmp.split()
                curr_time = idx_tmp[1].replace('\n','')
                curr_idx = idx_tmp[0].replace('\n','')
                if (float(curr_time) < next_time):
                    idx_tracer = idx_tracer + 1
                    str_to_add = (dfmt+"    "+curr_time) % int(idx_tracer)                
                    # Write the line to the new dump file:
                    print(str_to_add,file=dump_tracer_file)
                    # Make soft links for all fields:
                    filestr = str("tracers_pos"+dfmt+hdf_fmt)
                    fileout = outdir_full+'/'+filestr % int(idx_tracer)
                    if (args.use_rel):
                        filein = rundir_rel+'/'+filestr % int(curr_idx)
                    else:
                        filein = rundir_full+'/'+filestr % int(curr_idx)
                    try: 
                        os.remove(fileout)
                    except OSError as error:
                        pass
                    os.symlink(filein,fileout)

# ASSUME!  Only one tp slice file (001)!
# Can mine i-file later to make slice loop.

    if (mas_dumped_slices):
        slice_typ = "tp"
        slice_idx = "001"
        if (run_i < num_runs-1):
            # Get first time of next run:
            with open(rundirs[run_i+1]+'/mas_dumps_slices.txt') as file:
                tmp = file.readline().replace('\n','')
                next_time = float(tmp.split()[1])
        else:
            next_time = np.Infinity
        with open(rundir+'/mas_dumps_slices.txt') as file:
            for line in file:
                tmp = str(line)
                idx_tmp = tmp.split()
                curr_time = idx_tmp[1].replace('\n','')
                curr_idx = idx_tmp[0].replace('\n','')
                if (float(curr_time) < next_time):
                    idx_slice = idx_slice + 1
                    str_to_add = (dfmt+"    "+curr_time) % int(idx_slice)
                    # Write the line to the new dump file:
                    print(str_to_add,file=dump_slice_file)
                    # Make soft links for all fields:
                    for field in field_list_slice:
                       filestr = str("slice_"+slice_typ+slice_idx+"_"+field+dfmt+hdf_fmt)
                       fileout = outdir_full+'/'+filestr % int(idx_slice)
                       if (args.use_rel):
                           filein = rundir_rel+'/'+filestr % int(curr_idx)
                       else:
                           filein = rundir_full+'/'+filestr % int(curr_idx)
                       try: 
                           os.remove(fileout)
                       except OSError as error:
                           pass
                       os.symlink(filein,fileout)

# Make masTime.txt file for STAT runs:
if (mas_dumped_3d):
    os.popen('cat '+outdir_full+'/mas_dumps_3d.txt | while read -r c1 c2; do echo $c2; done > '+outdir_full+'/masTime.txt')






