CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

PROJECT( dngconvert )

# The version number.
set (dngconvert_VERSION_MAJOR 0)
set (dngconvert_VERSION_MINOR 1)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
  "${PROJECT_SOURCE_DIR}/dngconvertconfig.h.in"
  "${PROJECT_BINARY_DIR}/dngconvertconfig.h"
  )

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

ADD_SUBDIRECTORY( cmake )
ADD_SUBDIRECTORY( libdng )
ADD_SUBDIRECTORY( dngconvert )
ADD_SUBDIRECTORY( dngcompare )
ADD_SUBDIRECTORY( dngvalidate )
ADD_SUBDIRECTORY( dnganalyze )
