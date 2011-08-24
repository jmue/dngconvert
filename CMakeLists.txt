PROJECT( dngconvert )

CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

# The version number.
SET(DNGCONVERT_VERSION_MAJOR 0)
SET(DNGCONVERT_VERSION_MINOR 1)
SET(DNGCONVERT_VERSION_PATCH 0)

# configure a header file to pass some of the CMake settings
# to the source code
CONFIGURE_FILE (
  "${PROJECT_SOURCE_DIR}/config.h.cmake"
  "${PROJECT_BINARY_DIR}/config.h"
  )

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

IF( MINGW OR UNIX )
    ADD_DEFINITIONS(-Wall
                    -Wcast-align
                    -Wpointer-arith
                    -Wformat-security
                    -Wmissing-format-attribute
                    -Woverloaded-virtual
                    -W
                   )
ENDIF( MINGW OR UNIX )

ADD_SUBDIRECTORY( cmake )
ADD_SUBDIRECTORY( libdng )
ADD_SUBDIRECTORY( dngconvert )
ADD_SUBDIRECTORY( dngcompare )
ADD_SUBDIRECTORY( dngvalidate )
ADD_SUBDIRECTORY( dnganalyze )
