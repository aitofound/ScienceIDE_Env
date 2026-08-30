#*****************************************************************************
#
# Copyright (c) 2000 - 2018, Lawrence Livermore National Security, LLC
# Produced at the Lawrence Livermore National Laboratory
# LLNL-CODE-442911
# All rights reserved.
#
# This file is  part of VisIt. For  details, see https://visit.llnl.gov/.  The
# full copyright notice is contained in the file COPYRIGHT located at the root
# of the VisIt distribution or at http://www.llnl.gov/visit/copyright.html.
#
# Redistribution  and  use  in  source  and  binary  forms,  with  or  without
# modification, are permitted provided that the following conditions are met:
#
#  - Redistributions of  source code must  retain the above  copyright notice,
#    this list of conditions and the disclaimer below.
#  - Redistributions in binary form must reproduce the above copyright notice,
#    this  list of  conditions  and  the  disclaimer (as noted below)  in  the
#    documentation and/or other materials provided with the distribution.
#  - Neither the name of  the LLNS/LLNL nor the names of  its contributors may
#    be used to endorse or promote products derived from this software without
#    specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT  HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR  IMPLIED WARRANTIES, INCLUDING,  BUT NOT  LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND  FITNESS FOR A PARTICULAR  PURPOSE
# ARE  DISCLAIMED. IN  NO EVENT  SHALL LAWRENCE  LIVERMORE NATIONAL  SECURITY,
# LLC, THE  U.S.  DEPARTMENT OF  ENERGY  OR  CONTRIBUTORS BE  LIABLE  FOR  ANY
# DIRECT,  INDIRECT,   INCIDENTAL,   SPECIAL,   EXEMPLARY,  OR   CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT  LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR
# SERVICES; LOSS OF  USE, DATA, OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER
# CAUSED  AND  ON  ANY  THEORY  OF  LIABILITY,  WHETHER  IN  CONTRACT,  STRICT
# LIABILITY, OR TORT  (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY  WAY
# OUT OF THE  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
# DAMAGE.
#
# Modifications:
#   David M. Camp, Th Jan 14 11:50:00 PST 2010
#   Added new function ADD_TARGET_DEFINITIONS to add defines to targets.
#   This was needed for the plots to define ENGINE only for the engine build.
#
#   Brad Whitlock, Thu May 27 09:30:00 PDT 2010
#   Wrap setting the compiler in an if-exists test.
#
#   Mark C. Miller, Thu Jul 29 17:37:32 PDT 2010
#   Added block for compiler version comments.
#
#   Brad Whitlock, Tue Aug 17 09:17:19 PDT 2010
#   I added NETCDF_CXX_LIB.
#
#   Brad Whitlock, Wed Oct 27 14:12:19 PDT 2010
#   Make sure that TESSELLATION_LIBRARY gets saved.
#
#   Kathleen Bonnell, Thu Dec 2 15:45:44 MST 2010
#   Allow plugin installs on Windows. 
#   Added BOOST_INCLUDE_DIR, TESSELLATION_LIBRARY. 
#   Expand SLIVR_INCLUDE_DIR.  
#   Add defintions for HAVE_LIBGLEW, HAVE_LIBSLIVR (if VISIT_SLIVR).  
#   Only set TUVOK_LIB if VISIT_TUVOK is ON.  
#   Add preprocessor definitions for Windows.
#   Add VISIT_PYTHON_SCRIPTING.
#
#   Brad Whitlock, Thu Jun 23 17:03:35 PDT 2011
#   Fix python library extension for Mac.
#
#   Kathleen Biagas, Thu Jun 21 11:12:57 MST 2012
#   Include paths for vtk, exodusii, python and lib names for ptyhon are 
#   different on windows than unix, so use special vars set in root 
#   CMakeLists.txt file.  Fix boxlib include and lib var names.
#   Added fix for automatic debug flags setting on windows, to allow proper
#   linking with our default release-version third-party libraries.
#   Added 'ADD_TARGET_INCLUDE' function, change how parallel flags are 
#   handled on windows. Fix glitch with ADD_TARGET_DEFINITIONS when target
#   had more than one definition set already.
#
#   Kathleen Biagas, Tue Oct 29 12:24:32 MST 2013
#   Fixed typos in Functions that prevented caused problems on Windows.
#
#   Kathleen Biagas, Fri Oct 31 11:22:25 PDT 2014
#   Compare visual studio versions on windows (if using MSVC) instead of
#   comparing compiler paths.  Enclose /bin/gcc in quotes to 
#   handle spaces-in-paths on windows that causes warnings for this script.
#   Remove setting of Windows Definitions, VISIT_PLUGIN_TARGET_RTOD,
#   ADD_TARGET_INCLUDE, ADD_TARGET_DEFINITIONS, ADD_PARALLEL_LIBRARY, as
#   these are now available in VisItMacros.cmake, and can be used without
#   re-write.  Include VisItMacros.cmake.
#
#   Cyrus Harrison, Tue Feb 10 20:06:07 PST 2015
#   Changed boost support.
#
#   Iulian R. Grindeanu & Vijay S. Mahadevan via Mark C Miller
#   Wed Aug 10 14:54:05 PDT 2016
#   Added support for ANL's Mesh Object datABase (MOAB)
#
#   Kathleen Biagas, Thu Sep 27 11:29:37 PDT 2018
#   Added mfem. Fix cmake logic for MSVC. Fix boost, Qt.  Add
#   get_filename_component calls to get root install directory for setting
#   VISIT_INCLUDE_DIR, VISIT_LIBRARY_DIR, VISIT_BINARY_DIR, VISIT_ARCHIVE_DIR.
#
#****************************************************************************/

##
## This file gets generated by VisIt's top-level CMakeLists.txt and gets 
## installed along with VisIt so 3rd party plugins can be developed against an 
## installed version of VisIt.
##

# Build shared libraries since we're building plugins.
set(BUILD_SHARED_LIBS 1)

# Warn if compilers might be incompatible.
if(WIN32 AND MSVC)
    if(NOT  STREQUAL ${MSVC_VERSION})
      message(WARNING "MSVC version used for VisIt () differs from the one used for the current project ${MSVC_VERSION}!  They may incompatible.")
    endif()
else()
    if(EXISTS "/bin/gcc" AND
       NOT ${CMAKE_C_COMPILER} STREQUAL "/bin/gcc")
      message(WARNING "C compiler used for VisIt differs from the one used for\n"
                      " the current project!  They may be incompatible.")
    endif()
    if(EXISTS "/bin/g++" AND
       NOT ${CMAKE_CXX_COMPILER} STREQUAL "/bin/g++")
      message(WARNING "C++ compiler used for VisIt differs from the one used for\n"
                       "the current project!  They may be incompatible.")
    endif()
endif()

# The comment below is intended to capture version information
# of the compiler in use at the time CMake was invoked to build
# VisIt. If there is no comment in the intervening lines, then
# compiler version information was not successfully obtained.
# -------------------------------------------------------------
# gcc (Ubuntu 7.3.0-27ubuntu1~18.04) 7.3.0
# Copyright (C) 2017 Free Software Foundation, Inc.
# This is free software; see the source for copying conditions.  There is NO
# warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# 
# 
# g++ (Ubuntu 7.3.0-27ubuntu1~18.04) 7.3.0
# Copyright (C) 2017 Free Software Foundation, Inc.
# This is free software; see the source for copying conditions.  There is NO
# warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# 
# 
# -------------------------------------------------------------

# this should return the installed/include path
get_filename_component(_IMPORT_PREFIX "${CMAKE_CURRENT_LIST_FILE}" PATH)
# this should strip off the include
get_filename_component(_IMPORT_PREFIX "${_IMPORT_PREFIX}" PATH)

set(VISIT_INCLUDE_DIR "${_IMPORT_PREFIX}/include")
set(VISIT_LIBRARY_DIR "${_IMPORT_PREFIX}/lib")
if(WIN32)
  set(VISIT_BINARY_DIR  "${_IMPORT_PREFIX}")
  set(VISIT_ARCHIVE_DIR "${_IMPORT_PREFIX}/lib")
else()
  set(VISIT_BINARY_DIR  "${_IMPORT_PREFIX}/bin")
  set(VISIT_ARCHIVE_DIR "${_IMPORT_PREFIX}/archives")
endif()

# Commonly used VisIt include directories
set(VISIT_COMMON_INCLUDES
    ${VISIT_INCLUDE_DIR}/visit/include
    ${VISIT_INCLUDE_DIR}/visit/common/Exceptions/Database
    ${VISIT_INCLUDE_DIR}/visit/common/Exceptions/Pipeline
    ${VISIT_INCLUDE_DIR}/visit/common/Exceptions/Plotter
    ${VISIT_INCLUDE_DIR}/visit/common/comm
    ${VISIT_INCLUDE_DIR}/visit/common/expr
    ${VISIT_INCLUDE_DIR}/visit/common/icons
    ${VISIT_INCLUDE_DIR}/visit/common/misc
    ${VISIT_INCLUDE_DIR}/visit/common/parser
    ${VISIT_INCLUDE_DIR}/visit/common/plugin
    ${VISIT_INCLUDE_DIR}/visit/common/proxybase
    ${VISIT_INCLUDE_DIR}/visit/common/state
    ${VISIT_INCLUDE_DIR}/visit/common/utility
)

# VisIt options
set(VISIT_PYTHON_SCRIPTING       ON)
set(VISIT_SERVER_COMPONENTS_ONLY OFF)
set(VISIT_ENGINE_ONLY            OFF)
set(VISIT_DBIO_ONLY              OFF)
set(VISIT_PARALLEL               ON)
set(VISIT_JAVA                   OFF)
set(VISIT_SLIVR                  TRUE)
set(VISIT_TUVOK                  OFF)

# Set up VTK
set(VTK_INCLUDE_DIRS ${VISIT_INCLUDE_DIR}/vtk/vtk-6.1)
set(VTK_LIBRARY_DIRS ${VISIT_LIBRARY_DIR})

# Set up Mesa
set(MESA_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/mesa/include)
set(MESA_LIBRARY_DIR ${VISIT_LIBRARY_DIR})

# Set up GLEW
set(GLEW_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/visit/third_party_builtin/glew/glew/include)
set(GLEW_LIBRARY_DIR ${VISIT_LIBRARY_DIR})
set(GLEW_LIB         visitGLEW)
add_definitions(-DHAVE_LIBGLEW)

# Set up OpenGL
set(OPENGL_INCLUDE_DIR  /usr/include)
set(OPENGL_gl_LIBRARY   /usr/lib64/libGL.so)
set(OPENGL_glu_LIBRARY  /usr/lib64/libGLU.so)

# Set up the tessellation library
set(TESSELLATION_LIBRARY /usr/lib64/libGLU.so;/usr/lib64/libGL.so)

# EAVL
set(EAVL_INCLUDE_DIR )
set(EAVL_LIBRARY_DIR )

# Set up BOOST
set(BOOST_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/boost/include/boost)

set(VISIT_QT5 ON)

# Set up Qt
set(QT_INCLUDE_DIR           ${VISIT_INCLUDE_DIR}/qt/include)
set(QT_QTCORE_INCLUDE_DIR    ${VISIT_INCLUDE_DIR}/qt/include/QtCore)
set(QT_QTGUI_INCLUDE_DIR     ${VISIT_INCLUDE_DIR}/qt/include/QtGui)
set(QT_QTOPENGL_INCLUDE_DIR  ${VISIT_INCLUDE_DIR}/qt/include/QtOpenGL)
set(QT_QTWIDGETS_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/qt/include/QtWidgets)
set(QT_QTXML_INCLUDE_DIR     ${VISIT_INCLUDE_DIR}/qt/include/QtXml)

set(QT_LIBRARY_DIR           ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(QT_BIN                   ${VISIT_BINARY_DIR})
set(QT_MOC_EXECUTABLE        ${VISIT_BINARY_DIR}/moc)

# Set up Python
set(PYTHON_INCLUDE_PATH    ${VISIT_INCLUDE_DIR}/python/include/python2.7)
set(PYTHON_LIBRARY         ${VISIT_LIBRARY_DIR}/libpython2.7.so)

# Set up SLIVR
if(VISIT_SLIVR)
    set(SLIVR_INCLUDE_DIR
        ${VISIT_INCLUDE_DIR}/visit/third_party_builtin/slivr
        ${VISIT_INCLUDE_DIR}/visit/third_party_builtin/slivr/slivr
        ${VISIT_INCLUDE_DIR}/visit/third_party_builtin/slivr/teem-1.9.0-src/src
        ${VISIT_INCLUDE_DIR}/visit/third_party_builtin/slivr/teem-1.9.0-src/src/teem
    )
    set(SLIVR_GUI_IMPL    QvisCMap2Display.C QvisCMap2Widget.C)
    set(SLIVR_GUI_HDR     QvisCMap2Display.h QvisCMap2Widget.h)
    set(SLIVR_LIB         slivrG slivrV)
    add_definitions(-DHAVE_LIBSLIVR)
endif()

# Set up 3rd party I/O libraries
set(ADIOS_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/adios/include)
set(ADIOS_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(ADIOS_LIB libadiosread.a;libadiosread_nompi.a)

set(ADVIO_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/advio/include)
set(ADVIO_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(ADVIO_LIB libAdvDocIO.a;libAdvFileIO.a;libAdvBase.a)

set(BOXLIB_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/boxlib/include)
set(BOXLIB_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(BOXLIB_2D_LIB libbox2D.a)
set(BOXLIB_3D_LIB libbox3D.a)

set(CCMIO_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/ccmio/include)
set(CCMIO_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(CCMIO_LIB libccmio.a;libadf.a)

set(CFITSIO_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/cfitsio/include)
set(CFITSIO_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(CFITSIO_LIB libcfitsio.a)

set(CGNS_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/cgns/include)
set(CGNS_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(CGNS_LIB libcgns.so;libhdf5.so;libsz.so;libz.so)

set(EXODUSII_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/)
set(EXODUSII_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(EXODUSII_LIB )

set(FASTBIT_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/fastbit/include)
set(FASTBIT_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(FASTBIT_LIB )

set(GDAL_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/gdal/include)
set(GDAL_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(GDAL_LIB libgdal.a)

set(HDF4_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/hdf4/include)
set(HDF4_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(HDF4_LIB libmfhdf.a;libdf.a;libsz.so;libvtkjpeg-6.1.so)

set(HDF5_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/hdf5/include)
set(HDF5_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(HDF5_LIB libhdf5.so;libsz.so;libz.so)
if(VISIT_PARALLEL)
    set(HDF5_MPI_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/hdf5_mpi/include)
    set(HDF5_MPI_LIBRARY_DIR ${VISIT_LIBRARY_DIR}/hdf5_mpi ${VISIT_ARCHIVE_DIR})
    set(HDF5_MPI_LIB libhdf5_mpi.so;libsz.so;libz.so)
endif()

set(H5PART_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/h5part/include)
set(H5PART_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(H5PART_LIB libH5Part.a;libhdf5.so;libsz.so;libz.so)

set(MOAB_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/moab/include)
set(MOAB_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(MOAB_LIB libMOAB.so)
if(VISIT_PARALLEL)
    set(MOAB_MPI_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/moab_mpi/include)
    set(MOAB_MPI_LIBRARY_DIR ${VISIT_LIBRARY_DIR}/moab_mpi ${VISIT_ARCHIVE_DIR})
    set(MOAB_MPI_LIB libMOAB_mpi.so)
endif()

set(MILI_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/mili/include)
set(MILI_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(MILI_LIB libmili.a)

set(NETCDF_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/netcdf/include)
set(NETCDF_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(NETCDF_LIB libnetcdf.a;libhdf5_hl.so;libhdf5.so;libsz.so;libz.so)
set(NETCDF_CXX_LIB libnetcdf_c++.a)

set(PDB_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/silo/include)
set(PDB_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(PDB_LIB libsiloh5.so;libhdf5.so;libsz.so;libz.so;libz.so)

set(SILO_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/silo/include)
set(SILO_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(SILO_LIB libsiloh5.so;libhdf5.so;libsz.so;libz.so;libz.so)

set(XDMF_INCLUDE_DIR  ${VISIT_INCLUDE_DIR}/xdmf/include)
set(XDMF_LIBRARY_DIR  ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(XDMF_LIB libXdmf.so;libhdf5.so;libvtklibxml2-6.1.so)

set(MFEM_INCLUDE_DIR  ${VISIT_INCLUDE_DIR}/mfem/include)
set(MFEM_LIBRARY_DIR  ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(MFEM_LIB libmfem.a;libz.so)

set(CONDUIT_INCLUDE_DIR  ${VISIT_INCLUDE_DIR}/conduit/conduit)
set(CONDUIT_LIBRARY_DIR  ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
set(CONDUIT_LIB libconduit.so;libconduit_relay.so;libconduit_blueprint.so;libhdf5.so;libsz.so;libz.so)
if(VISIT_PARALLEL)
    set(CONDUIT_MPI_INCLUDE_DIR ${VISIT_INCLUDE_DIR}/conduit/conduit)
    set(CONDUIT_MPI_LIBRARY_DIR ${VISIT_LIBRARY_DIR} ${VISIT_ARCHIVE_DIR})
    set(CONDUIT_MPI_LIB libconduit_relay_mpi.so)
endif()

# Set up TUVOK
if(VISIT_TUVOK)
    set(TUVOK_LIB         tuvok)
endif()

include(${VISIT_INCLUDE_DIR}/VisItMacros.cmake)

# Installation macros
macro(VISIT_INSTALL_DATABASE_PLUGINS)
    foreach(target ${ARGN})
        set_target_properties(${target} PROPERTIES
           RUNTIME_OUTPUT_DIRECTORY "${VISIT_PLUGIN_DIR}/databases"
           LIBRARY_OUTPUT_DIRECTORY "${VISIT_PLUGIN_DIR}/databases"
        )
    endforeach()
endmacro()

macro(VISIT_INSTALL_OPERATOR_PLUGINS)
    foreach(target ${ARGN})
        set_target_properties(${target} PROPERTIES
           RUNTIME_OUTPUT_DIRECTORY "${VISIT_PLUGIN_DIR}/operators"
           LIBRARY_OUTPUT_DIRECTORY "${VISIT_PLUGIN_DIR}/operators"
        )
    endforeach()
endmacro()

macro(VISIT_INSTALL_PLOT_PLUGINS)
    foreach(target ${ARGN})
        set_target_properties(${target} PROPERTIES
           RUNTIME_OUTPUT_DIRECTORY "${VISIT_PLUGIN_DIR}/plots"
           LIBRARY_OUTPUT_DIRECTORY "${VISIT_PLUGIN_DIR}/plots"
        )
    endforeach()
endmacro()

macro(VISIT_PLUGIN_TARGET_FOLDER type pname)
    message(STATUS "Skipping VISIT_PLUGIN_TARGET_FOLDER macro")
endmacro()

# Parallel settings
set(VISIT_PARALLEL_CXXFLAGS     -DPARALLEL -DMPICH_IGNORE_CXX_SEEK -DMPICH_SKIP_MPICXX -DOMPI_SKIP_MPICXX -DMPI_NO_CPPBIND -DPARALLEL -DMPICH_IGNORE_CXX_SEEK  -I${VISIT_INCLUDE_DIR}/mpich/include -fPIC)
set(VISIT_PARALLEL_LINKER_FLAGS -Wl,-rpath -Wl,${VISIT_LIBRARY_DIR})
set(VISIT_PARALLEL_LIBS         ${VISIT_LIBRARY_DIR}/libmpich.so;${VISIT_LIBRARY_DIR}/libopa.so;${VISIT_LIBRARY_DIR}/libmpl.so;/usr/lib64/librt.so;/usr/lib64/libpthread.so)
set(VISIT_PARALLEL_INCLUDE      "")
set(VISIT_PARALLEL_DEFS         )
set(VISIT_NOLINK_MPI_WITH_LIBRARIES OFF)

