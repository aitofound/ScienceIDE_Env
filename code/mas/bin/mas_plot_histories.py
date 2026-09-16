#!/usr/bin/env python
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.ticker import FormatStrFormatter
import argparse
import numpy
import sys

parser = argparse.ArgumentParser(description='Plot/compare histories for one or more MAS runs')
parser.add_argument('-name',help='Name of the set of plots',type=str, required=True)
parser.add_argument('-dirs',help='A comma separated list of paths to the directory each run is located in', type=str, required=True)
parser.add_argument('-time1',help='The time to start plotting the data at (default:first time in mas file)',default=-9999)
parser.add_argument('-time2',help='The time to stop plotting the data at (default:last time in mas file)',default=9999)
parser.add_argument('-skip',help='Number of points to skip in plotting, this helps with larger files (default:1)',default=1)
parser.add_argument('-samples',help='Number of points to plot, this helps with larger files (default:all)',default=200,type=int)
parser.add_argument('-labels',help='A comma separated list of labels to display in the plots for each run (default:"Run 1","Run 2",...)',type=str,default=' ')
parser.add_argument('-cu',help='Display quantities in code units.',action='store_true',default=False)


args = parser.parse_args()
arg_dict = vars(args)
dir_list = arg_dict['dirs'].split(',')
label_list = arg_dict['labels'].split(',')
code_units = arg_dict['cu']


if code_units:
    unit_fac_energy = 1.0
    unit_fac_time = 1.0
    unit_fac_time2 = 1.0
    unit_fac_jxb = 1.0
    unit_fac_jdb = 1.0
    unit_str_energy = ''
    unit_str_time = ''
    unit_str_time2 = ''
    unit_str_jxb = ''
    unit_str_jdb = ''
else:
    unit_fac_energy = 1.30671e32
    unit_fac_time = 1445.87003081/3600.0
    unit_fac_time2 = 1445.87003081
    unit_fac_jxb = 5.56855893e-12
    unit_fac_jdb = 5.56855893e-12
    unit_str_energy = '(erg)'
    unit_str_time = '(hrs)'
    unit_str_time2 = '(sec)'
    unit_str_jxb = '(dynes/cm$^3$)'
    unit_str_jdb = '(dynes/cm$^3$)'


# Build the default list of labels based on the number runs the user entered
# if no label list is entered
if arg_dict['labels'] == ' ':
    label_list = []
    def_label = 'Run '
    for i,run in enumerate(dir_list):
        label_list.append(def_label+str(i+1))

LABEL_LEN = len(label_list)
# Validate the list arguments:
if not len(dir_list) == LABEL_LEN:
    print('ERROR: Number of runs, dirs, and labels must match. Use -h for more information.')
    sys.exit()

if LABEL_LEN > 5:
    print('ERROR: Can only compare a maximum of 5 runs.')

print("==> Reading history files...")
h_data_table_list = []
v_data_table_list = []
W_list = []
K_list = []

# Files are assumed to have these headers, Declared V headers in
# case they are needed in the future.
H_HEADERS = {
    'Time':0,'Wr':1, 'Wt':2, 'Wp':3, 'Kr':4, 'Kt':5, 'Kp':6,
    'E':7,'|JxB|':8, '|J.B|':9
}
V_HEADERS = {
    'Time':0, 'K_par_r':1, 'K_par_t':2, 'K_par_p':3, 'K_perp_r':4,
    'K_perp_t':5, 'K_perp_p':6, 'R':7, 'V':8, 'P':9, 'E_cons':10,
    'dt':11, 'Flow_CFL':12, 'Wave_CFL':13
}
COLORS = ['#0000ff','#ff0000','#008000','#800080','#b79f00']
COLOR_NAMES = ['Blue', 'Red', 'Green', 'Purple', 'Gold']
MARKERS = ['s','o','d','^','v']
LW = 2.0
MS = 3.0

# Helper function to parse_TSV that "inclusively" finds the index closest to the value
def find_nearest(array, value, min=0):
    array = numpy.asarray(array)
    idx = (numpy.abs(array - value)).argmin()
    if min != 0:
        if array[idx] > value and idx > 0:
            return idx-1
    return idx

# Parse the TSV to a numpy ndarray, parse each row and append it to the i-1th row of the numpy array. Doing so ignores
# the first line in the file, the header line.
def parse_TSV(file_name,headers,time1,time2,skip,samples):
    try:
        with open(file_name, 'r') as file:
            lines_list = file.readlines()
    except OSError:
        print("ERROR: can not find {}\n\tMake sure the file exists or has the proper permissions".format(file_name))
        sys.exit(1)

    data_table = numpy.zeros(shape=(len(lines_list)-1, len(headers.keys())))
    for i,line in enumerate(lines_list):
        # Skip the header line
        if i == 0:
            continue
        table_row = line.strip().split('\t')
        data_table[i-1] = table_row

    # Time is the first column in all data files
    time_list = data_table[:,0]
    start_idx = 0
    end_idx = len(data_table)
    time1 = float(time1)
    time2 = float(time2)
    if time1 > time2:
        tmp = time1
        time1 = time2
        time2 = tmp
    if time1 != -9999 or time2 != 9999:
        start_idx = find_nearest(time_list,time1,1)
        end_idx = find_nearest(time_list,time2)
    
     # Step 1: Apply skipping first
    indices = numpy.arange(start_idx, end_idx, skip)
    
    # Step 2: If samples is specified, select at most 'samples' points
    if samples > 0 and len(indices) > samples:
        indices = numpy.linspace(indices[0], indices[-1], samples, dtype=int)
    
    # Allocate return table and fill it with selected data
    return_table = data_table[indices]

    return return_table

# For every run name given, build file path, then parse the data
# and store a list of all parsed data
for i,run in enumerate(dir_list):
    h_file_name = "{}/mas_history_a.out".format(dir_list[i])
    v_file_name = "{}/mas_history_b.out".format(dir_list[i])
    temp_h_table = parse_TSV(h_file_name, H_HEADERS, arg_dict['time1'], arg_dict['time2'], arg_dict['skip'], arg_dict['samples'])
    temp_v_table = parse_TSV(v_file_name, V_HEADERS, arg_dict['time1'], arg_dict['time2'], arg_dict['skip'], arg_dict['samples'])
    temp_W = temp_h_table[:,H_HEADERS['Wr']] + temp_h_table[:,H_HEADERS['Wt']] + temp_h_table[:,H_HEADERS['Wp']]
    temp_K = temp_h_table[:,H_HEADERS['Kr']] + temp_h_table[:,H_HEADERS['Kt']] + temp_h_table[:,H_HEADERS['Kp']]
    h_data_table_list.append(temp_h_table)
    v_data_table_list.append(temp_v_table)
    W_list.append(temp_W)
    K_list.append(temp_K)

# Init figure
print("==> Plotting history comparison plots...")
fig, ax = plt.subplots(3,3,figsize=[15,10], dpi=96, facecolor='w')

# Build and set title from label list
title = arg_dict['name']
if LABEL_LEN > 1:
    for i, label in enumerate(label_list[:-1]):
        title = title + ': ' + '{} ({})'.format(label, COLOR_NAMES[i])
        title = title + ': ' + ' vs. '
    title = title + '{} ({})'.format(label_list[-1], COLOR_NAMES[LABEL_LEN - 1])
fig.suptitle(title, fontsize=20)

MS = 3.0
LW = 2.0
LWM = LW/LABEL_LEN

# Plot Magnetic Energy
plt.subplot(3,3,1)

for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], unit_fac_energy*W_list[run],
             color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title('Total Magnetic Energy',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.ylabel(unit_str_energy)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Plot JxB
plt.subplot(3,3,2)
for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], h_data_table_list[run][:,H_HEADERS['|JxB|']],
             color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title(r'${\bf \Sigma\,|\,\vec J\times \vec B\,|}$',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.ylabel(unit_str_jxb)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Plot Wave CFL
plt.subplot(3,3,3)
for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], v_data_table_list[run][:,V_HEADERS['Wave_CFL']],
             color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title('Wave CFL',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Plot Kinetic Energy
plt.subplot(3,3,4)
for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], unit_fac_energy*K_list[run],
             color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title('Total Kinetic Energy',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.ylabel(unit_str_energy)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Plot J*B
plt.subplot(3,3,5)
for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], h_data_table_list[run][:,H_HEADERS['|J.B|']],
    color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title(r'${\bf \Sigma\,|\,\vec J\cdot \vec B\,|}$',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.ylabel(unit_str_jdb)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Plot Wave CFL
plt.subplot(3,3,6)
for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], v_data_table_list[run][:,V_HEADERS['Flow_CFL']],
    color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title('Flow CFL',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Plot Thermal Energy
plt.subplot(3,3,7)
for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], unit_fac_energy*h_data_table_list[run][:,H_HEADERS['E']],
    color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title('Total Thermal Energy',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.ylabel(unit_str_energy)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Plot Time Step
plt.subplot(3,3,8)
for run,table in enumerate(h_data_table_list):
    LWu = LW-run*LWM
    plt.plot(unit_fac_time*h_data_table_list[run][:,H_HEADERS['Time']], unit_fac_time2*v_data_table_list[run][:,V_HEADERS['dt']],
    color=COLORS[run],linewidth=LWu,marker=MARKERS[run],markersize=MS,markeredgewidth=0.0,fillstyle='full',markeredgecolor=COLORS[run])
plt.title('Time Step',fontweight='bold')
plt.xlabel('Time '+unit_str_time)
plt.ylabel(unit_str_time2)
plt.autoscale(enable=True, axis='x', tight=True)
plt.gca().yaxis.set_major_formatter(FormatStrFormatter('%.3e'))
plt.grid()

# Remove axes in subplot(3,3,9) to match the figure from R
fig.delaxes(ax[2][2])

plt.tight_layout(rect=[0, 0.03, 1, 0.95])

if len(dir_list) > 1:
  fig.savefig("mas_histories_comparison.pdf",bbox_inches='tight',dpi=96)
else:
  fig.savefig("mas_histories.pdf",bbox_inches='tight',dpi=96)




