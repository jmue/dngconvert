PROJECT( dngconvert )

# where to look first for cmake modules, before ${CMAKE_ROOT}/Modules/ is checked
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

CMAKE_MINIMUM_REQUIRED( VERSION 2.8 )

ADD_SUBDIRECTORY( cmake )
ADD_SUBDIRECTORY( libdng )
ADD_SUBDIRECTORY( dngconvert )
