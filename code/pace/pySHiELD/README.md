# pySHiELD

Python implementation of FV3 GFS physics built using the NDSL domain-specific language middleware in Python.

## Description

pySHiELD is under active development. Currently, the pace level docker environment should be used for development.

## QuickStart

We recommend setting up a virtual environment, e.g.

```bash
python -m venv .venv
source .venv/bin/activate
```

Make sure to have a C/C++ and a Fortran compiler installed. To run models, you will also need an MPI installation. A simple way to satisfy the MPI requirement is to install pre-built binaries from `pip`, e.g.

```bash
pip install openmpi
```

A standard `pyproject.toml` configuration file is provided and can be installed with `pip`.

```bash
pip install .[tests,pyfv3,ndsl]
```

If you are planning on modifying the source files, use the following command:

```bash
pip install -e .[dev]
```

to install in editable mode and get extra dependencies only needed for development (e.g. linting tools). We also highly recommend to install the `pre-commit` hooks

```bash
pre-commit install
```

if you are contributing code. Doing so will automatically run linting tools before you commit, ensuring your code changes meet our code quality standards.
