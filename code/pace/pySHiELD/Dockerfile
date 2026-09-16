FROM python:3.11-slim-bookworm@sha256:7cd3fa11d619688317226bc93dc59bc8966e9aec6bc2a6abb847e8ab7d656706

RUN apt-get update &&\
    apt install -y --no-install-recommends \
    software-properties-common

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends\
    g++ \
    gcc \
    gfortran \
    git \
    libproj-dev \
    proj-data \
    proj-bin \
    libgeos-dev \
    libopenmpi3 \
    libopenmpi-dev \
    libhdf5-serial-dev \
    libffi-dev \
    libssl-dev \
    netcdf-bin \
    libnetcdf-dev


RUN python3 -m pip install --upgrade setuptools pip wheel


# Check python & pip
RUN python --version
RUN which python
RUN pip --version
RUN which pip

COPY ./ /pyshield/

# Install pySHiELD and the full dependencies
RUN cd /pyshield && pip install -e .[dev]


RUN pip install \
    matplotlib \
    cython \
    cartopy \
    ipyparallel \
    jupyter \
    jupyterlab \
    shapely \
    jupyterlab_code_formatter \
    mpi4py \
    pytest \
    pytest-subtests \
    pytest-regressions \
    pytest-profiling \
    pytest-cov

# # set up for fv3viz
RUN cd /
RUN git clone --recursive https://github.com/ai2cm/fv3net.git
RUN cd fv3net && git checkout 1d168ef
RUN pip install fv3net/external/vcm
ENV PYTHONPATH=/fv3net/external/fv3viz

ENV CFLAGS="-I/usr/include -DACCEPT_USE_OF_DEPRECATED_PROJ_API_H=1"

ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1
