Python Scripts
==============

Collection of Python scripts to make using [SWMF](https://github.com/SWMFsoftware/SWMF) easier. Note most of these require a recent version of Python __3__. Supercomputers typically have this already.

Make sure [swmfpy](https://github.com/SWMFsoftware/swmfpy) is installed.

```bash
$ pip install swmfpy || pip install --user swmfpy
```

Table of Contents:
------------------

- [prepare_geospace.py](#prepare_geospacepy)
- [plot_indices.py](#plot_indices)

prepare_geospace.py
-------------------

This script is to help with the inputs of the geospace model.

### Pre-steps

If you already have a run directory and `PARAM.in` file then skip ahead.

In the base directory of SWMF run:

```bash
SWMF$ ./Config.pl -install
# Compile then make the run directory if worked
SWMF$ make -j test_swpc_compile && make rundir
```

### Set up

It would be useful to set up an environment variable to this script location.
```bash
SWMF$ echo 'export PYSCRIPTS='"$(realpath share/Python/Scripts)" >> ~/.profile
SWMF$ . ~/.profile
```

Go to your run directory and copy a sensible `PARAM.in`:

```bash
SWMF$ cd run
SWMF/run$ cp Param/SWPC/PARAM.in_SWPC_v2_init PARAM.in
```

### Running

Then the final steps copy the script and the run directory into a work directory that your supercomputer allows before jobs:

```bash
SWMF$ cp run /some/work/dir/
SWMF$ cd /some/work/dir/
/some/work/dir$ python3 "$PYSCRIPTS"/prepare_geospace.py --start_time 2014 2 3 4 5 6 --end_time 2014 3 4 5 6 7
# Your PARAM.in will be overwritten with those values
# Then submit job
```

### Options

To find out the options of `prepare_geospace.py` then run:

```bash
/some/work/dir$ ./prepare_geospace.py --help
# Help message output
```

Options can be useful to script this file with shell scripting, for example, when submitting jobs.

plot_indices.py
---------------

A script to plot the global indices of your geospace run (SYM-H, cross polar cap
potential, AL.. etc.). This uses `swmfpy` to get the observed indices to
compare.

To run simply type on your command line:

```bash
python plot_indices.py -h
```

A simply way of using this script is to make sure you know where your
`log_****.log` and corresponding `geo_****.log` are and running:

```bash
# replace with your file names
python plot_indices.py log_YYYYMMMDD.log geo_YYYMMMDD.log
```
