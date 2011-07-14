 
# =======================================================
# dnganalyze command line tool

FIND_PACKAGE( ZLIB )
INCLUDE_DIRECTORIES( ${ZLIB_INCLUDE_DIR} )
ADD_DEFINITIONS(${ZLIB_DEFINITIONS})

FIND_PACKAGE(Threads)

INCLUDE_DIRECTORIES( ${dngconvert_SOURCE_DIR}/libdng )
INCLUDE_DIRECTORIES( ${dngconvert_SOURCE_DIR}/libdng/contrib/dng_sdk/source )

# Set platteforms env.
IF(WIN32)
    ADD_DEFINITIONS(
                    # Set Windows rules.
                    -DqWinOS=1
                    -DqMacOS=0
                   )
ELSE(WIN32)
    ADD_DEFINITIONS(
                    # MACOS-X, Set Linux & co rules.
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

ADD_DEFINITIONS(-DqDNGValidateTarget=1)

SET( DNGANALYZE_SRCS dnganalyze.cpp )

ADD_EXECUTABLE( dnganalyze ${DNGANALYZE_SRCS} )

TARGET_LINK_LIBRARIES( dnganalyze ${ZLIB_LIBRARIES}
                                  ${CMAKE_THREAD_LIBS_INIT}
                                  dng)

#INSTALL( TARGETS dnganalyze ${INSTALL_TARGETS_DEFAULT_ARGS} )
