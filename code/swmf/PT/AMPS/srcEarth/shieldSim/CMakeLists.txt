cmake_minimum_required(VERSION 3.16)
project(shieldSim LANGUAGES CXX)

# Geant4 supplies compiler flags and include/library paths through Geant4_USE_FILE.
# Build example:
#   mkdir build && cd build
#   cmake .. -DGeant4_DIR=/path/to/geant4/lib/Geant4-*/
#   make -j
find_package(Geant4 REQUIRED)
include(${Geant4_USE_FILE})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(shieldSim
  shieldSim.cc
  src/CLI.cc
  src/ComputedQuantities.cc
  src/DetectorConstruction.cc
  src/EventAction.cc
  src/GCRSpectrum.cc
  src/MaterialCatalog.cc
  src/OutputUtils.cc
  src/PrimaryGeneratorAction.cc
  src/RunAction.cc
  src/SteppingAction.cc
)

target_include_directories(shieldSim PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(shieldSim ${Geant4_LIBRARIES})

# Copy example input files into the build tree for convenience.
file(COPY ${PROJECT_SOURCE_DIR}/examples DESTINATION ${PROJECT_BINARY_DIR})
file(COPY ${PROJECT_SOURCE_DIR}/macros DESTINATION ${PROJECT_BINARY_DIR})
