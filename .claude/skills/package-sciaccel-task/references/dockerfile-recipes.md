# Containerising legacy scientific code

The recipes below are for the situations that actually come up when the code
is fifteen years old and was last built on a cluster that no longer exists.

The governing rule: **pin everything, and make failure loud.** A base image
that drifts is a different benchmark every month; a data fetch that silently
half-succeeds is a task that fails for reasons no one can reproduce.

## Choosing the base

| base | when |
| --- | --- |
| `nvidia/cuda:12.6.2-devel-ubuntu24.04` | CUDA C++ ports. `devel`, never `runtime` — the agent has to compile |
| `nvidia/cuda:12.6.2-devel-ubuntu22.04` | code that needs an older glibc, or a CUDA version pinned by a dependency |
| `ubuntu:24.04` | CPU-side incumbent, or the solver brings its own toolkit (HIP, SYCL, OpenMP offload) |
| `nvcr.io/nvidia/nvhpc:*-devel-cuda_multi-ubuntu*` | Fortran that needs `nvfortran` for OpenACC or CUDA Fortran |

Digest-pin at `status = "published"`: `FROM nvidia/cuda:12.6.2-devel-ubuntu24.04@sha256:...`.
A tag can be re-pushed; a digest cannot.

The GPU allocation is requested in `task.toml` (`[environment] gpus`,
`gpu_types`), not in the Dockerfile. A CUDA image with no GPU allocated
builds fine and runs nothing — a confusing failure worth recognising quickly.

## apt: pin the image, not the packages

Ubuntu's archive rewrites package versions, so `apt-get install gfortran=4:13.2.0-7ubuntu1`
breaks within months. Pin the base image instead — that fixes the archive —
and keep the standard hygiene:

```dockerfile
RUN apt-get update && apt-get install -y --no-install-recommends \
      build-essential gfortran cmake git ca-certificates pkg-config \
      libopenmpi-dev openmpi-bin libhdf5-dev libfftw3-dev libopenblas-dev \
    && rm -rf /var/lib/apt/lists/*
```

pip is the opposite: pin exactly, every time.

```dockerfile
RUN pip install --no-cache-dir --break-system-packages numpy==2.1.3 h5py==3.12.1
```

(`--break-system-packages` is needed on Ubuntu 24.04's system Python. On a
`python:3.13-slim` base it is not.)

## MPI

Legacy scientific code is usually MPI-parallel, and the incumbent's rank count
is part of what the speedup is measured against — so the container must be
able to run it.

```dockerfile
RUN apt-get update && apt-get install -y --no-install-recommends \
      libopenmpi-dev openmpi-bin && rm -rf /var/lib/apt/lists/*
ENV OMPI_ALLOW_RUN_AS_ROOT=1 OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1
```

Those two variables matter: containers run as root and OpenMPI refuses to
launch as root without them. Without them the oracle fails with an error
message that looks like a permissions bug and is not.

Make sure `[environment] cpus` is at least the rank count the incumbent uses,
or `mpirun` will oversubscribe and the baseline timing will be nonsense —
which quietly inflates every speedup on the task.

## Fortran and autoconf trees older than the compiler

The classic failure: `gfortran` 13 rejects code that `gfortran` 4 accepted.

```dockerfile
# Argument mismatches that were legal before GCC 10 are errors now.
ENV FFLAGS="-fallow-argument-mismatch -fallow-invalid-boz"
# Fixed-form source with lines past column 72:
# ENV FFLAGS="$FFLAGS -ffixed-line-length-none"
```

For a `configure` script that predates the current autoconf, refresh it in the
image rather than patching it by hand:

```dockerfile
RUN apt-get update && apt-get install -y --no-install-recommends autoconf automake libtool \
    && rm -rf /var/lib/apt/lists/*
RUN cd /opt/src && autoreconf -fi
```

If the code needs a compiler genuinely older than the base image ships, install
it explicitly (`gcc-11 g++-11 gfortran-11`) and set `CC`/`CXX`/`FC` — do not
pick an ancient base image for one compiler and inherit its whole ancient
userland.

**Build the incumbent at image build** where it builds in reasonable time. The
solver should open a shell onto working code, not spend its first hour
discovering that a 2003 Makefile wants a flag that no longer exists. Where the
build genuinely *is* part of the task, say so in `instruction.md` and leave it.

## conda and spack

Prefer apt and pip. Reach for these only when the dependency stack genuinely
needs them — and pin either way.

```dockerfile
# micromamba: much smaller and faster than a full conda install
FROM mambaorg/micromamba:2.0.5
COPY environment.yml /tmp/environment.yml
RUN micromamba install -y -n base -f /tmp/environment.yml && micromamba clean --all --yes
```

Commit the `environment.yml` with explicit versions, not a solved lock from
your laptop's architecture.

Spack is worth it when the code needs a specific MPI/BLAS/HDF5 combination
that apt cannot express — and it is slow, so raise `build_timeout_sec`
accordingly and expect the first build to take an hour.

## Data too large to commit

Caps are 1 MB per text file, 50 MB per blob (hard), with a warning at 10 MB.
Above the cap, data is never committed. Host it, fetch it at build, verify it:

```dockerfile
RUN pip install --no-cache-dir --break-system-packages huggingface_hub==0.27.0
COPY checksums.sha256 /app/data/
RUN python3 -c "from huggingface_hub import snapshot_download; \
      snapshot_download(repo_id='<owner>/<repo>', repo_type='dataset', \
      revision='<40-hex-commit>', allow_patterns='sa-NNNN/input/*', \
      local_dir='/app/data')" \
    && cd /app/data && sha256sum -c checksums.sha256
```

Three things that are not optional:

- **`revision` is a full 40-hex commit SHA, never `main`.** `main` is a
  different dataset every push, which makes the task unreproducible in the
  quietest possible way.
- **The checksum verification is in the same `RUN`**, so a mismatch fails the
  build. A build that cannot verify its data must fail loudly.
- **The namespace split**: `sa-NNNN/input/` is agent-visible and fetched in
  `environment/Dockerfile`; `sa-NNNN/verification/` is verifier-only and
  fetched in `tests/Dockerfile`. Pulling verification data into the agent
  image hands the solver the answer, and it is the single most damaging
  mistake available in this file.

## Build timeouts

Harbor's default `build_timeout_sec` is 600. For a scientific build that is
usually wrong. Set it explicitly at every resource class — the ladder in
`CONTRIBUTING.md` gives the value — and raise it further for spack or for a
tree that runs `autoreconf`.

## Hygiene the validator enforces

```dockerfile
COPY tests/reference /refs      # ERROR — CI rejects the package
COPY ../solution /oracle        # ERROR — same
```

`scripts/validate.mjs` reads the `COPY`/`ADD` instructions in
`environment/Dockerfile` and fails the package if any source resolves into
`solution/` or `tests/`. Neither the oracle nor the equivalence references may
be visible in the room the solver works in.

## Debugging a build

```bash
docker build --progress=plain -t sa-NNNN-env tasks/sa-NNNN/environment   # full log
harbor tasks start-env -p tasks/sa-NNNN -e docker -a -i                  # shell inside
```

The second is the one to reach for when the image builds but the task behaves
oddly — it puts you where the solver will be, looking at what the solver will
see.
