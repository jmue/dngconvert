
FIND_PACKAGE( EXPAT )
INCLUDE_DIRECTORIES( ${EXPAT_INCLUDE_DIR} )
ADD_DEFINITIONS(${EXPAT_DEFINITIONS})
 
FIND_PACKAGE( JPEG )
INCLUDE_DIRECTORIES( ${JPEG_INCLUDE_DIR} )
ADD_DEFINITIONS(${JPEG_DEFINITIONS})
 
# =======================================================
# XMP SDK source code.

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source
                    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/common
                    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/public/include
                    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/build
                    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/MD5
                   )

# Set platteforms env.
IF(WIN32)
    ADD_DEFINITIONS(
                    # Set Windows rules.
                    -DWIN_ENV=1
                   )
ELSE(WIN32)
    ADD_DEFINITIONS(
                    # Set MACOS-X, Linux & co rules.
                    -DUNIX_ENV=1
                   )
ENDIF(WIN32)

SET( LIBXMPSDK_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/ExpatAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/MD5/MD5.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/ParseRDF.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/common/UnicodeConversions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/WXMPIterator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/WXMPMeta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/WXMPUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/common/XML_Node.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPCore_Impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPIterator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/common/XMP_LibUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPMeta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPMeta-GetSet.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPMeta-Parse.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPMeta-Serialize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPUtils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/xmpsdk/source/XMPCore/XMPUtils-FileInfo.cpp
   )

ADD_LIBRARY( xmpsdk STATIC ${LIBXMPSDK_SRCS} )

TARGET_LINK_LIBRARIES( xmpsdk ${EXPAT_LIBRARY} )

# =======================================================
# DNG SDK source code

INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source )

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

# Level of debug info in the console.
ADD_DEFINITIONS(-DqDNGDebug=0)
ADD_DEFINITIONS(-DqDNGValidateTarget=1)

SET( LIBDNGSDK_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_1d_function.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_date_time.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_ifd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_memory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_point.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_simple_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_1d_table.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_exceptions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_memory_stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_rational.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_spline.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_xmp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_abort_sniffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_exif.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_image_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_preview.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_misc_opcodes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_mosaic_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_read_image.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_xmp_sdk.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_area_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_file_stream.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_mutex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_rect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_string.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_xy_coord.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_bottlenecks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_bad_pixels.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_filter_task.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_iptc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_negative.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_reference.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_string_list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_camera_profile.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_fingerprint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_lens_correction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_linearization_info.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_opcode_list.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_opcodes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_orientation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_render.cpp 
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_tag_types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_color_space.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_globals.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_gain_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_lossless_jpeg.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_parse_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_resample.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_temperature.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_color_spec.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_host.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_pixel_buffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_shared.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_tile_iterator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_tone_curve.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_hue_sat_map.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/contrib/dng_sdk/source/dng_pthread.cpp
   )

ADD_LIBRARY( dngsdk STATIC ${LIBDNGSDK_SRCS} )

TARGET_LINK_LIBRARIES( dngsdk xmpsdk )


# =======================================================
# libdng source code

INCLUDE_DIRECTORIES( ${CMAKE_CURRENT_SOURCE_DIR} )

# Add standalone C++ header files to this list
SET( LIBDNG_HDR         
    ${CMAKE_CURRENT_SOURCE_DIR}/dnghost.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dngifd.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dngimagewriter.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dngmosaicinfo.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dngnegative.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dngreadimage.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dngexif.h
    ${CMAKE_CURRENT_SOURCE_DIR}/dngtagcodes.h
    )

# Add library C++ source files to this list
SET( LIBDNG_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/dnghost.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dngifd.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dngimagewriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dngmosaicinfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dngnegative.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dngreadimage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dngexif.cpp
   )

# Library
ADD_LIBRARY( dng ${STATIC_FLAG} ${LIBDNG_SRCS} )

TARGET_LINK_LIBRARIES( dng ${EXPAT_LIBRARIES} ${JPEG_LIBRARIES} dngsdk xmpsdk )

# add the install targets
#INSTALL(TARGETS dng DESTINATION lib)
#INSTALL(FILES ${LIBDNG_HDR} DESTINATION include)

