import sys
import sdf
import matplotlib.pyplot as plt

fname = "Data/0010.sdf"
varname = "Electric_Field_Ex"

try:
    d = sdf.read(fname)
except:
    print 'File "%s" not found' % fname
    sys.exit()

if not hasattr(d, varname):
    print 'Variable "%s" not found in file' % varname
    sys.exit()

var = d.__dict__[varname]

if len(var.dims) != 2:
    print 'File "%s" is not from a 2D run' % fname
    sys.exit()

iy = var.dims[1] / 2
grid = var.grid_mid
x = grid.data[0]

fig = plt.figure()
plt.plot(x, var.data[:, iy], 'r+-')
plt.xlabel(grid.labels[0] + r' $(' + grid.units[0] + ')$')
plt.ylabel(var.name + r' $(' + var.units + ')$')
plt.show()
