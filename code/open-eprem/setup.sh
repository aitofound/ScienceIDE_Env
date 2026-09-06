#!/bin/bash

# Import functions.
funcfile=tools.sh
if [ -f $funcfile ]; then
    . $funcfile
else
    echo "Cannot source necessary functions to set up EPREM."
    echo "Is your distribution missing $funcfile?"
    exit 1
fi

# Set up the function that prints a section header.
dry_run_string=
print_banner() {
    if [ $verbose == 1 ]; then
        print_header "${dry_run_string}$@"
    fi
}

# See https://www.gnu.org/software/bash/manual/html_node/The-Set-Builtin.html
# - e => Exit immediately if a pipeline returns non-zero status.
# - u => Treat unset variables and parameters (with certain exceptions) as
#   errors when performing parameter expansion.
set -eu

# Store the initial working directory.
top_dir="$(dirname "$(readlink -f "${0}")")"

# Declare the name of the log file.
logfile=${top_dir}/setup.log

# Check whether we are in a git repository. This is necessary for avoiding a
# fatal error when setting the default alias within a distributed package.
gitcheck=$(git -C . rev-parse 2>/dev/null; echo $?)

# Set option defaults.
verbose=0
dry_run=0
if [ $gitcheck == 0 ]; then
    alias=$(git symbolic-ref --short -q HEAD)
else
    alias=new-build
fi
user_alias=0
download_deps=0
requested_deps=
default_deps_dir=${top_dir}/deps
deps_dir=
mpi_dir=
zlib_dir=
libconfig_dir=
netcdf_dir=
hdf4_dir=
hdf5_dir=
enable_hdf4=0
debug=0
optimize=0

# This is the CLI's main help text.
show_help()
{
    cli_name=${0##*/}
    echo "
${textbf}NAME${textnm}
        $cli_name - Configure the EPREM build process

${textbf}SYNOPSIS${textnm}
        ${textbf}$cli_name${textnm} [${startul}OPTION${endul}]

${textbf}DESCRIPTION${textnm}
        This script will run set-up tasks necessary for building EPREM.
        From the top-level repository directory, it is equivalent to running

        $ autoreconf --install --symlink [--verbose]
        $ ./configure [...] [--silent]

        where the arguments to ./configure depend on the selected options.

        Typical use cases will only require running this script once. Furthermore, 
        after downloading and building external dependencies (see --download-deps), 
        setting up additional builds with the same dependencies will simply require 
        pointing to the dependencies directory (see --with-deps-dir).

        ${textbf}-h${textnm}, ${textbf}--help${textnm}
                Display help and exit.
        ${textbf}-v${textnm}, ${textbf}--verbose${textnm}
                Print runtime messages. Use of this option will enable the 
                --verbose argument to autoreconf and will disable the --silent 
                option to ./configure. By default, --verbose is disabled for 
                autoreconf and --silent is enabled for ./configure.
        ${textbf}--dry-run${textnm}
                Display the sequence of commands but don't run anything.
        ${textbf}--alias=ALIAS${textnm}
                Declare an alias for this installation. This will create a new 
                subdirectory called ALIAS in which to configure the build. By default, 
                this script will create the build-directory from the current git 
                branch, appending "-dbg" for --debug builds and "-opt" for --optimize
                builds. This script will not append anything to ALIAS, if given.
        ${textbf}--download-deps[=A[,B,...]]${textnm}
                Download external dependencies. By default, this will download
                all required packages and build them inside 
                ${default_deps_dir}. 
                Setting --with-deps-dir=DIR will cause it to download and build 
                packages inside DIR. This script will create the necessary path in 
                either case. You may also pass a comma-separated list of packages 
                to install. Note that whitespace between names will cause an error.
        ${textbf}--with-deps-dir=DIR${textnm}
                Install dependencies in DIR when --download-deps is present.
                Otherwise, look for external dependencies in DIR/include and DIR/lib.
        ${textbf}--with-mpi-dir=DIR${textnm}
                Look for MPI header files in DIR/include and look for MPI 
                libraries in DIR/lib.
        ${textbf}--with-zlib-dir=DIR${textnm}
                Look for zlib header files in DIR/include and look for 
                libraries in DIR/lib. Supersedes the --with-deps-dir option.
        ${textbf}--with-libconfig-dir=DIR${textnm}
                Look for libconfig header files in DIR/include and look for 
                libraries in DIR/lib. Supersedes the --with-deps-dir option.
        ${textbf}--with-netcdf-dir=DIR${textnm}
                Look for NetCDF header files in DIR/include and look for 
                libraries in DIR/lib. Supersedes the --with-deps-dir option.
        ${textbf}--with-hdf4-dir=DIR${textnm}
                Look for HDF4 header files in DIR/include and look for 
                libraries in DIR/lib. Supersedes the --with-deps-dir option.
        ${textbf}--with-hdf5-dir=DIR${textnm}
                Look for HDF5 header files in DIR/include and look for 
                libraries in DIR/lib. Supersedes the --with-deps-dir option.
        ${textbf}--enable-hdf4${textnm}
                Attempt to build with HDF4 input functionality. Requires running 
                make clean. Note that the ability to read HDF4 MHD files may 
                be deprecated in future versions.
        ${textbf}--debug${textnm}
                Build a debugging version of EPREM. Specifically, this will 
                pass the '-g' compiler flag and the '-DDEBUG' pre-processor 
                directive to ./configure.
        ${textbf}--optimize${textnm}
                Build an optimized version of EPREM. Specifically, this will 
                pass the '-O3' compiler flag and the '-DNDEBUG' pre-processor 
                directive to ./configure.
"
}

# This will run if the CLI gets an unrecognized option.
report_bad_arg()
{
    printf "\nUnrecognized command: ${1}\n\n"
}

# Parse command-line options. See
# `/usr/share/doc/util-linux/examples/getopt-example.bash` and the `getopt`
# manual page for more information.
TEMP=$(getopt \
    -n 'setup.sh' \
    -o 'hv' \
    -l 'help,verbose,' \
    -l 'dry-run' \
    -l 'alias:' \
    -l 'download-deps::' \
    -l 'with-deps-dir:' \
    -l 'with-mpi-dir:' \
    -l 'with-zlib-dir:' \
    -l 'with-libconfig-dir:' \
    -l 'with-netcdf-dir:' \
    -l 'with-hdf4-dir:' \
    -l 'with-hdf5-dir:' \
    -l 'enable-hdf4' \
    -l 'debug,optimize' \
    -- "$@")

if [ $? -ne 0 ]; then
    echo "Failed to parse command-line arguments. Terminating."
    exit 1
fi

eval set -- "$TEMP"
unset TEMP

while [ $# -gt 0 ]; do
    case "$1" in
        '-h'|'--help')
            show_help
            exit
        ;;
        '-v'|'--verbose')
            verbose=1
            shift
            continue
        ;;
        '--dry-run')
            dry_run=1
            shift
            continue
        ;;
        '--alias')
            alias="${2}"
            shift 2
            user_alias=1
            continue
        ;;
        '--download-deps')
            download_deps=1
            if [ -z "$2" ]; then
                requested_deps=('zlib' 'libconfig' 'hdf4' 'hdf5' 'netcdf')
            else
                IFS=',' requested_deps=($2)
            fi
            shift 2
            continue
        ;;
        '--with-deps-dir')
            deps_dir="$(realpath -m "${2}")"
            shift 2
            continue
        ;;
        '--with-mpi-dir')
            mpi_dir="$(readlink -e "${2}")"
            shift 2
            continue
        ;;
        '--with-zlib-dir')
            zlib_dir="$(readlink -e "${2}")"
            shift 2
            continue
        ;;
        '--with-libconfig-dir')
            libconfig_dir="$(readlink -e "${2}")"
            shift 2
            continue
        ;;
        '--with-netcdf-dir')
            netcdf_dir="$(readlink -e "${2}")"
            shift 2
            continue
        ;;
        '--with-hdf4-dir')
            hdf4_dir="$(readlink -e "${2}")"
            shift 2
            continue
        ;;
        '--with-hdf5-dir')
            hdf5_dir="$(readlink -e "${2}")"
            shift 2
            continue
        ;;
        '--with-deps-dir')
            deps_dir="$(readlink -e "${2}")"
            shift 2
            continue
        ;;
        '--enable-hdf4')
            enable_hdf4=1
            shift
            continue
        ;;
        '--debug')
            debug=1
            shift
            continue
        ;;
        '--optimize')
            optimize=1
            shift
            continue
        ;;
        '--')
            shift
            break
        ;;
    esac
done

# Collect extra arguments. These may indicate that the user incorrectly entered
# an argument.
extra_args="$@"
if [ -n "${extra_args}" ]; then
    echo
    echo "Found the following unrecognized argument(s):"
    echo
    for arg in ${extra_args[@]}; do
        echo ">   $arg"
    done
    echo
    echo "Did you misspell something?"
    echo
    echo "Note that this program does not support passing arbitrary arguments"
    echo "to ./configure. If you want (and know how to) configure the code"
    echo "in a way that this program does not provide, you may directly call"
    echo "./configure with the appropriate arguments."
    echo
    exit 1
fi

# Create a fresh log file. We do this after reading and checking command-like
# arguments because in order to avoid unnecessarily creating a new log.
> $logfile

# Declare a status flag.
status=

# Define special flag to indicate success.
success="@!SUCCESS!@"

# Declare the build-specific directory.
if [ $user_alias == 0 ]; then
    if [ $debug == 1 ]; then
        alias="${alias}-dbg"
    else if [ $optimize == 1 ]; then
        alias="${alias}-opt"
        fi
    fi
fi
build_dir=${top_dir}/${alias}

# Define a clean-up function.
cleanup() {
    if [ "$status" != "$success" ]; then
        echo
        echo "Setup failed. See $logfile for details."
        exit 1
    else
        echo &>> $logfile
        echo "Setup succeeded." &>> $logfile
        print_banner "Done"
        echo
        echo "You may now run make && make install in ${build_dir}"
        echo "or run ./build.sh --alias=${alias}"
        echo
        echo "See ./build.sh -h for all available options."
        echo
    fi
}

trap cleanup EXIT

# Create the build-specific directory if it doesn't already exist.
if [ -d "${build_dir}" ]; then
    echo "Refusing to overwrite existing directory ${build_dir}" &>> $logfile
    exit 1
fi
mkdir ${build_dir}

# Check for --dry-run option.
if [ ${dry_run} == 1 ]; then
    dry_run_string="[DRY RUN] "
fi

# Set the target directory for external dependencies.
if [ -z "$deps_dir" ]; then
    deps_dir=$default_deps_dir
fi

if [ -z "${zlib_dir}" ]; then
    zlib_dir=${deps_dir}/zlib
fi
if [ -z "${libconfig_dir}" ]; then
    libconfig_dir=${deps_dir}/libconfig
fi
if [ -z "${hdf4_dir}" ]; then
    hdf4_dir=${deps_dir}/hdf4
fi
if [ -z "${hdf5_dir}" ]; then
    hdf5_dir=${deps_dir}/hdf5
fi
if [ -z "${netcdf_dir}" ]; then
    netcdf_dir=${deps_dir}/netcdf
fi

# Set any unset environment variables to null.
${CFLAGS:=}
${CXXFLAGS:=}
${CPPFLAGS:=}
${LDFLAGS:=}
${LIBS:=}

# Declare the directory for downloaded source code.
src_dir="$deps_dir"/src

# Download a named package.
download_package() {
    local pkg_alias="${1}"
    local pkg_url="${2}"

    print_banner "Downloading $pkg_alias"
    if [ $dry_run == 0 ]; then
        wget $pkg_url
    fi
}

# Configure and make a named package.
build_package() {
    local pkg_alias="${1}"
    local pkg_tar="${2}"
    local pkg_dir="${3}"
    local pkg_args="${4-}"

    print_banner "Setting up $pkg_alias ($pkg_dir)"
    if [ $dry_run == 0 ]; then
        tar -xvzf $pkg_tar &>> $logfile
        pushd $pkg_dir &> /dev/null
        ./configure --prefix="$deps_dir"/"$pkg_dir" $pkg_args &>> $logfile && \
        make &>> $logfile && \
        make install &>> $logfile && \
        make check &>> $logfile && \
        popd &> /dev/null
        /bin/rm -rf $pkg_dir
    fi
}

link_package() {
    local pkg_alias="${1}"
    local pkg_dir="${2}"

    print_banner "Creating symbolic link $pkg_alias -> $pkg_dir"
    if [ $dry_run == 0 ]; then
        if [ -L "$pkg_alias" ]; then
            /bin/rm -f $pkg_alias
        fi
        ln -s $pkg_dir $pkg_alias
    fi
}

install_package() {
    local pkg_alias="${1}"
    local pkg_url="${2}"
    local pkg_tar="${3}"
    local pkg_dir="${4}"
    local pkg_args="${5-}"

    if printf '%s\0' "${requested_deps[@]}" | grep -F -x -z -q -- "${pkg_alias}"; then
        pushd $src_dir 1> /dev/null 2>> $logfile
        download_package $pkg_alias $pkg_url
        popd &> /dev/null
    fi
    if [ ! -d "${deps_dir}/${pkg_dir}" ]; then
        pushd $src_dir 1> /dev/null 2>> $logfile
        build_package $pkg_alias $pkg_tar $pkg_dir $pkg_args
        popd &> /dev/null
        link_package $pkg_alias $pkg_dir
    fi
}

install_zlib() {
    local pkg_alias=zlib
    local pkg_dir=$pkg_alias-1.2.11
    local pkg_tar=$pkg_dir.tar.gz
    local pkg_url=https://zlib.net/fossils/$pkg_tar
    local pkg_args=""

    install_package $pkg_alias $pkg_url $pkg_tar $pkg_dir $pkg_args
}

install_libconfig() {
    local pkg_alias=libconfig
    local pkg_dir=$pkg_alias-1.5
    local pkg_tar=$pkg_dir.tar.gz
    local pkg_url=https://src.fedoraproject.org/repo/pkgs/libconfig/$pkg_tar/a939c4990d74e6fc1ee62be05716f633/$pkg_tar
    local pkg_args=""

    install_package $pkg_alias $pkg_url $pkg_tar $pkg_dir $pkg_args
}

install_hdf4() {
    local pkg_alias=hdf4
    local pkg_dir=hdf-4.2.16
    local pkg_tar=$pkg_dir.tar.gz
    local pkg_url=https://support.hdfgroup.org/ftp/HDF/releases/HDF4.2.16/src/$pkg_tar
    local pkg_args="--disable-netcdf"

    install_package $pkg_alias $pkg_url $pkg_tar $pkg_dir $pkg_args
}

install_hdf5() {
    local pkg_alias=hdf5
    local pkg_dir=$pkg_alias-1.8.17
    local pkg_tar=$pkg_dir.tar.gz
    local pkg_url=https://support.hdfgroup.org/ftp/HDF5/releases/hdf5-1.8/hdf5-1.8.17/src/$pkg_tar
    local pkg_args=""

    install_package $pkg_alias $pkg_url $pkg_tar $pkg_dir $pkg_args
}

install_netcdf() {
    local pkg_alias=netcdf
    local pkg_dir=netcdf-c-4.4.1.1
    local pkg_tar=v4.4.1.1.tar.gz
    local pkg_url=https://github.com/Unidata/netcdf-c/archive/refs/tags/$pkg_tar
    local pkg_args="--disable-dap-remote-tests"

    if [ -z "${zlib_dir}" ]; then
        echo "Cannot install NetCDF4 without zlib support." &>> $logfile
        exit 1
    fi
    export CPPFLAGS="${CPPFLAGS} -I${zlib_dir}/include"
    export LDFLAGS="${LDFLAGS} -L${zlib_dir}/lib"
    if [ -z "${hdf5_dir}" ]; then
        echo "Cannot install NetCDF4 without HDF5 support." &>> $logfile
        exit 1
    fi
    export CPPFLAGS="${CPPFLAGS} -I${hdf5_dir}/include"
    export LDFLAGS="${LDFLAGS} -L${hdf5_dir}/lib"
    install_package $pkg_alias $pkg_url $pkg_tar $pkg_dir $pkg_args
}

# Booleans to prevent redundant installations.
installed_zlib=0
installed_libconfig=0
installed_hdf4=0
installed_hdf5=0
installed_netcdf=0

# The function that will download and build external dependencies.
install_dep() {
    local dep_name="${1}"

    # Create and enter the top-level directory for external dependencies.
    mkdir -p ${deps_dir}
    pushd ${deps_dir} 1> /dev/null 2>> $logfile
    mkdir -p $src_dir

    # --> zlib
    if [ "${dep_name}" == "zlib" ]; then
        install_zlib && installed_zlib=1
    fi

    # --> libconfig
    if [ "${dep_name}" == "libconfig" ]; then
        install_libconfig && installed_libconfig=1
    fi

    # --> HDF4
    if [ "${dep_name}" == "hdf4" ]; then
        install_hdf4 && installed_hdf4=1
    fi

    # --> HDF5
    if [ "${dep_name}" == "hdf5" ]; then
        install_hdf5 && installed_hdf5=1
    fi

    # --> NetCDF4
    if [ "${dep_name}" == "netcdf" ]; then
        if [ $installed_hdf5 == 0 ]; then
            install_hdf5 && installed_hdf5=1
        fi
        install_netcdf && installed_netcdf=1
    fi

    # Exit the top-level external-dependencies directory.
    popd &> /dev/null
}

# Process the --download-deps option.
for dep in ${requested_deps[@]}; do
    install_dep "$dep"
done

# Initialize temporary variables for --with-<package>-dir options.
SH_CFLAGS=
SH_CXXFLAGS=
SH_CPPFLAGS=
SH_LDFLAGS=
SH_LIBS=

# Update flags based on --with-<package>-dir options.
if [ -n "$mpi_dir" ]; then
    SH_CFLAGS="  -I${mpi_dir}/include $SH_CFLAGS"
    SH_CXXFLAGS="-I${mpi_dir}/include $SH_CXXFLAGS"
    SH_CPPFLAGS="-I${mpi_dir}/include $SH_CPPFLAGS"
    SH_LDFLAGS=" -L${mpi_dir}/lib -Wl,-rpath,${mpi_dir}/lib $SH_LDFLAGS"
fi
if [ -n "$zlib_dir" ]; then
    SH_CFLAGS="  -I${zlib_dir}/include $SH_CFLAGS"
    SH_CXXFLAGS="-I${zlib_dir}/include $SH_CXXFLAGS"
    SH_CPPFLAGS="-I${zlib_dir}/include $SH_CPPFLAGS"
    SH_LDFLAGS=" -L${zlib_dir}/lib -Wl,-rpath,${zlib_dir}/lib $SH_LDFLAGS"
fi
if [ -n "$libconfig_dir" ]; then
    SH_CFLAGS="  -I${libconfig_dir}/include $SH_CFLAGS"
    SH_CXXFLAGS="-I${libconfig_dir}/include $SH_CXXFLAGS"
    SH_CPPFLAGS="-I${libconfig_dir}/include $SH_CPPFLAGS"
    SH_LDFLAGS=" -L${libconfig_dir}/lib -Wl,-rpath,${libconfig_dir}/lib $SH_LDFLAGS"
fi
if [ -n "$netcdf_dir" ]; then
    SH_CFLAGS="  -I${netcdf_dir}/include $SH_CFLAGS"
    SH_CXXFLAGS="-I${netcdf_dir}/include $SH_CXXFLAGS"
    SH_CPPFLAGS="-I${netcdf_dir}/include $SH_CPPFLAGS"
    SH_LDFLAGS=" -L${netcdf_dir}/lib -Wl,-rpath,${netcdf_dir}/lib $SH_LDFLAGS"
fi
if [ -n "$hdf4_dir" ]; then
    SH_CFLAGS="  -I${hdf4_dir}/include $SH_CFLAGS"
    SH_CXXFLAGS="-I${hdf4_dir}/include $SH_CXXFLAGS"
    SH_CPPFLAGS="-I${hdf4_dir}/include $SH_CPPFLAGS"
    SH_LDFLAGS=" -L${hdf4_dir}/lib -Wl,-rpath,${hdf4_dir}/lib $SH_LDFLAGS"
fi
if [ -n "$hdf5_dir" ]; then
    SH_CFLAGS="  -I${hdf5_dir}/include $SH_CFLAGS"
    SH_CXXFLAGS="-I${hdf5_dir}/include $SH_CXXFLAGS"
    SH_CPPFLAGS="-I${hdf5_dir}/include $SH_CPPFLAGS"
    SH_LDFLAGS=" -L${hdf5_dir}/lib -Wl,-rpath,${hdf5_dir}/lib $SH_LDFLAGS"
fi

# Update flags based on --debug or --optimize options.
if [ $debug == 1 ]; then
    SH_CFLAGS="-g ${SH_CFLAGS}"
    SH_CXXFLAGS="-g ${SH_CXXFLAGS}"
    SH_CPPFLAGS="-DDEBUG ${SH_CPPFLAGS}"
else
    if [ $optimize == 1 ]; then
        SH_CFLAGS="-O3 ${SH_CFLAGS}"
        SH_CXXFLAGS="-O3 ${SH_CXXFLAGS}"
        SH_CPPFLAGS="-DNDEBUG ${SH_CPPFLAGS}"
    fi
fi

# Check for --enable-hdf4 option.
if [ $enable_hdf4 == 1 ]; then
    SH_LIBS="-lhdf5 -lhdf5_hl -lmfhdf -ldf -ljpeg -lz -lm"
    SH_CPPFLAGS="-DHAVE_HDF4 ${SH_CPPFLAGS}"
else
    SH_LIBS="-lhdf5 -lhdf5_hl -ljpeg -lz -lm"
fi

# Initialize a temporary variable for ./configure options.
CF_FLAGS=

# Collect all non-empty flags.
SH_CFLAGS=$(trim "${SH_CFLAGS} ${CFLAGS}")
if [ -n "${SH_CFLAGS}" ]; then
    CF_FLAGS+="CFLAGS=\"${SH_CFLAGS}\" "
fi
SH_CXXFLAGS=$(trim "${SH_CXXFLAGS} ${CXXFLAGS}")
if [ -n "${SH_CXXFLAGS}" ]; then
    CF_FLAGS+="CXXFLAGS=\"${SH_CXXFLAGS}\" "
fi
SH_CPPFLAGS=$(trim "${SH_CPPFLAGS} ${CPPFLAGS}")
if [ -n "${SH_CPPFLAGS}" ]; then
    CF_FLAGS+="CPPFLAGS=\"${SH_CPPFLAGS}\" "
fi
SH_LDFLAGS=$(trim "${SH_LDFLAGS} ${LDFLAGS}")
if [ -n "${SH_LDFLAGS}" ]; then
    CF_FLAGS+="LDFLAGS=\"${SH_LDFLAGS}\" "
fi
SH_LIBS=$(trim "${SH_LIBS} ${LIBS}")
if [ -n "${SH_LIBS}" ]; then
    CF_FLAGS+="LIBS=\"${SH_LIBS}\" "
fi

# Check for --verbose option.
AR_FLAGS="--install --symlink"
if [ ${verbose} == 1 ]; then
    AR_FLAGS="${AR_FLAGS} --verbose"
else
    CF_FLAGS="${CF_FLAGS} --silent"
fi

# Run Autotools setup stage, if necessary.
if [ -d "./build-aux" ]; then
    print_banner "Setting Up Autotools"
    echo "Found ./build-aux directory. Not running autoreconf." &>> $logfile
else
    autoreconf ${AR_FLAGS}
fi

# Move to the build-specific subdirectory.
pushd ${build_dir} 1> /dev/null 2>> $logfile

# Update configure flags based on the build alias.
CF_FLAGS="${CF_FLAGS} --prefix=${build_dir}"

# Configure the build.
print_banner "Configuring"
if [ $dry_run == 0 ]; then
    eval "$top_dir/configure ${CF_FLAGS}" &>> $logfile
fi

# Exit the build-specific subdirectory.
popd 1> /dev/null 2>> $logfile

# Set the status flag to indicate success.
status=$success

