#!/usr/bin/env python3
import signal
import os
import argparse
import sys

# Version 1.0.2

def signal_handler(signal, frame):
        print('You pressed Ctrl+C! Stopping!')
        sys.exit(0)
signal.signal(signal.SIGINT, signal_handler)

def argParsing():

    parser = argparse.ArgumentParser(description='PSI MasHelPrep.')

    parser.add_argument('-cordir',
                        help='Directory of MAS coronal time-depenedent run.',
                        dest='cordir',
                        type=str,
                        required=True)

#    parser.add_argument('-helrelaxdir',
#                        help='Directory of MAS heliospheric relaxation run.  This must have used the same boundary conditions as the first slice here. It should be already shifted back to the Carrington frame (mhd_shifted).',
#                        dest='helrelaxdir',
#                        type=str,
#                        required=True)

    parser.add_argument('-itemplate',
                        help='Template input file.',
                        dest='input_template_file',
                        required=True)
                        
    parser.add_argument('-helrunname',
                        help='Run name you want for the helio run',
                        dest='helrunname',
                        default='mas',
                        required=False)                        

    parser.add_argument('-outdir',
                        help='Directory for setting up helio runs.',
                        dest='outdir',
                        default='helcme',
                        required=False)

#    parser.add_argument('-ext_out',
#                        help='Write out bc slices in the specified format [hdf|h5]',
#                        dest='ext_out',
#                        default='h5',
#                        required=False)

    parser.add_argument('-digits_in',
                        help='Specify 3 or 6 digit sequence format of input 3D or slice files [hdf|h5]',
                        dest='digits_in',
                        default=6,
                        required=False)

#    parser.add_argument('-ext_in',
#                        help='Specify format of input 3D or slice files [hdf|h5]',
#                        dest='ext_in',
#                        required=True)
#    parser.add_argument('-norelaxrun',
#                        help='Do not set up a relaxation run.',
#                        dest='norelax',
#                        action='store_true',
#                        default=False,
#                        required=False)
#    parser.add_argument('-plot',
#                        help='Plot the final boundary slices.',
#                        dest='plot',
#                        action='store_true',
#                        default=False,
#                        required=False)
#    parser.add_argument('-extract_slices',
#              help='Extract the boundary slices from the 3D coronal data at r1.',
#                        dest='extract_slices',
#                        action='store_true',
#                        default=False,
#                        required=False)
#    parser.add_argument('-r1',
#                        help='Radius in Rs of boundary slices.',
#                        dest='r1',
#                        required=True)
#    parser.add_argument('-remesh_slices_to_template_file',
#                        help='Set to 2D hdf template file.',
#                        dest='remesh_template_file',
#                        required=False)

    return parser.parse_args()


## Get input agruments:
args = argParsing()
cordir = args.cordir
cordir_full = os.path.realpath(args.cordir)
outdir = args.outdir
outdir_full = os.path.realpath(args.outdir)
#helrelaxdir = args.helrelaxdir
#helrelaxdir_full = os.path.realpath(args.helrelaxdir)

# Check inputs:
#   Check for valid ext_in, ext_out, and digits_in here

if (args.digits_in==3):
    digits_in_fmt='%03d'
else:
    digits_in_fmt='%06d'
digits_out_fmt='%06d'

# ASSUME! One slice only, and that slice is at r=28.0!
slice_base_name = 'slice_tp001_'
r1 = 28.0

# Get list of fields. For now, ASSUME! helio-needed fields are there.
field_list = ['rho','t','vr','vt','vp','br','bt','bp']

# Make output directory
try:
    os.makedirs(outdir_full, exist_ok=True)
except OSError as error:
    print('ERROR!  Cannot create output directory '+outdir_full)
    sys.exit(1)

# Make input slice and initial condition sub-directory.
os.makedirs(outdir_full+'/bc', exist_ok=True)
#os.makedirs(outdir_full+'/init', exist_ok=True)

# Open dump slice file.
dump_slice_file = open(cordir+'/mas_dumps_slices.txt')

num_slices = 0
for line in dump_slice_file:
   line = str(line)
   if line != "\n":
       num_slices += 1
       for field in field_list:
           fileout = outdir_full+'/bc/'+slice_base_name+field+'%06d.hdf' % int(num_slices)
           filein = cordir_full+'/'+slice_base_name+field+'%06d.hdf' % int(num_slices)
           try: 
               os.remove(fileout)
           except OSError as error:
               pass
           os.symlink(filein,fileout)


# Create input file using helrunname
template_file = open(args.input_template_file,'r')

input_file = open(outdir_full+'/i'+args.helrunname,'w')

# Add lines below:
original_stdout = sys.stdout
sys.stdout = input_file

for line in template_file:
   line = str(line)
   if line != "\n":
       print(line.replace('\n',''))

print('! INITIAL PLASMA SETUP (overridden by remesh)')
print('!')
print("  initial_plasma='2DFILE'")
print("  twodfile_rho='bc/"+slice_base_name+"rho000001.hdf'")
print("  twodfile_t='bc/"+slice_base_name+"t000001.hdf'")
print("  twodfile_vr='bc/"+slice_base_name+"vr000001.hdf'")
print('!')
print('! INITIAL MAGNETIC FIELD SETUP')
print('!')
print("  initial_field='POTENTIAL_FIELD'")
print("  bnfile='bc/"+slice_base_name+"br000001.hdf'")
print("  b_in_gauss=.false.  ")
print("  potential_field_bc='CLOSED_WALL'")
print(' /')
print("!")
print(" &interplanetary")
print("  interplanetary_run=.true.")
print("  ip_bc_use_pot_solves=.true.")
print("  ip_radius=28.0")
print("  boundary_frame='COROTATING'")
print("!")
print("  brfile='bc/"+slice_base_name+"br'")
print("  btfile='bc/"+slice_base_name+"bt'")
print("  bpfile='bc/"+slice_base_name+"bp'")
print("  rhofile='bc/"+slice_base_name+"rho'")
print("  tfile='bc/"+slice_base_name+"t'")
print("  vrfile='bc/"+slice_base_name+"vr'")
print("  vtfile='bc/"+slice_base_name+"vt'")
print("  vpfile='bc/"+slice_base_name+"vp'")
print("!")
print("  deltat_ip=0.0")
print("!")
           
dump_slice_file.seek(0)
           
i=0
j=0
print('  ip_sequence=')
for line in dump_slice_file:

    i=i+1
    j=j+1
    line = str(line).replace('\n','')
    line  = line.split()
    curr_idx  = int(line[0])
    if (i==num_slices):
        print('  %06d' % curr_idx)
    elif (j<7):
        print('  %06d,' % curr_idx, end = "")
    else:
        print('  %06d,' % curr_idx)
        j=0

dump_slice_file.seek(0)

i=0
j=0
print("!")
print('  ip_node=')
for line in dump_slice_file:

    i=i+1
    j=j+1
    line = str(line).replace('\n','')
    line  = line.split()
    curr_time = float(line[1])
    if (i==num_slices):
        print('  %14.8f' % curr_time)
    elif (j<4):
        print('  %14.8f,' % curr_time, end = "")
    else:
        print('  %14.8f,' % curr_time)
        j=0

print(' /')

sys.stdout = original_stdout

