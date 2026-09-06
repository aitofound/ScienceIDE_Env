cmake_minimum_required(VERSION 3.15...3.26)
project(${SKBUILD_PROJECT_NAME} LANGUAGES C)

find_package(
  Python
  COMPONENTS Interpreter Development.Module NumPy
  REQUIRED)

python_add_library(_lie_ops_c MODULE src/mink/lie/_lie_ops_c.c WITH_SOABI)
target_include_directories(_lie_ops_c PRIVATE ${Python_NumPy_INCLUDE_DIRS})

install(TARGETS _lie_ops_c DESTINATION mink/lie)
