

SET( EXIV2_MIN_VERSION "0.21" )
FIND_PACKAGE( Exiv2 )
INCLUDE_DIRECTORIES( ${EXIV2_INCLUDE_DIR} )
ADD_DEFINITIONS(${EXIV2_DEFINITIONS})

FIND_PACKAGE( ZLIB )
INCLUDE_DIRECTORIES( ${ZLIB_INCLUDE_DIR} )
ADD_DEFINITIONS(${ZLIB_DEFINITIONS})

FIND_PACKAGE( OpenMP )
IF(OPENMP_FOUND)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
ENDIF(OPENMP_FOUND)

FIND_PACKAGE(Threads)

FIND_PACKAGE( LibRaw )
INCLUDE_DIRECTORIES( ${LIBRAW_INCLUDE_DIR} )
ADD_DEFINITIONS(${LIBRAW_DEFINITIONS})

FIND_PACKAGE( LCMS )
INCLUDE_DIRECTORIES( ${LCMS_INCLUDE_DIR} )
ADD_DEFINITIONS(${LCMS_DEFINITIONS})


INCLUDE_DIRECTORIES( ${dngconvert_SOURCE_DIR}/libdng )
INCLUDE_DIRECTORIES( ${dngconvert_SOURCE_DIR}/libdng/contrib/dng_sdk/source )
INCLUDE_DIRECTORIES( ${PROJECT_BINARY_DIR} )

# Set platteforms env.
IF(WIN32)
    ADD_DEFINITIONS(
                    # Set Windows rules.
                    -DqWinOS=1
                    -DqMacOS=0
                   )
ELSE(WIN32)
    ADD_DEFINITIONS(
                    # Set MACOS-X, Linux & co rules.
                    -DqWinOS=0
                    -DqMacOS=0

                    # Must be set to 1, else do not compile under Linux.
                    -DqDNGUseStdInt=1
                   )
ENDIF(WIN32)

# Check processor endianness
INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(IS_BIG_ENDIAN)
IF(NOT IS_BIG_ENDIAN)
    ADD_DEFINITIONS(-DqDNGLittleEndian=1)
ENDIF(NOT IS_BIG_ENDIAN)

# Thread safe support under Mac and Linux using pthread library
IF(NOT WIN32)
    ADD_DEFINITIONS(-DqDNGThreadSafe=1)
ENDIF(NOT WIN32)

SET( DNGCONVERT_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/dngconvert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/exiv2meta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/exiv2dngstreamio.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/librawimage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/librawdngdatastream.cpp
   )

# Level of debug info in the console.
ADD_DEFINITIONS(-DqDNGDebug=0)
ADD_DEFINITIONS(-DqDNGValidateTarget=1)

ADD_EXECUTABLE( dngconvert ${DNGCONVERT_SRCS} )

TARGET_LINK_LIBRARIES(dngconvert ${ZLIB_LIBRARIES}
                                 ${EXIV2_LIBRARIES}
                                 ${LIBRAW_LIBRARIES}
                                 ${LCMS_LIBRARIES}
                                 ${CMAKE_THREAD_LIBS_INIT}
                                 dng)

# add the install targets
INSTALL(TARGETS dngconvert DESTINATION bin)
INSTALL(FILES "${PROJECT_BINARY_DIR}/dngconvertconfig.h"
         DESTINATION include)
