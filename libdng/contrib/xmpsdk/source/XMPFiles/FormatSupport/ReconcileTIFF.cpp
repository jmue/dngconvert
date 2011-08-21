// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2006 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XMP_Environment.h"	// ! This must be the first include.

#include "Reconcile_Impl.hpp"

#include "UnicodeConversions.hpp"

#include <stdio.h>
#if XMP_WinBuild
	#define snprintf _snprintf
#endif

#if XMP_WinBuild
	#pragma warning ( disable : 4146 )	// unary minus operator applied to unsigned type
	#pragma warning ( disable : 4800 )	// forcing value to bool 'true' or 'false'
	#pragma warning ( disable : 4996 )	// '...' was declared deprecated
#endif

// =================================================================================================
/// \file ReconcileTIFF.cpp
/// \brief Utilities to reconcile between XMP and legacy TIFF/Exif metadata.
///
// =================================================================================================

// =================================================================================================

// =================================================================================================
// Tables of the TIFF/Exif tags that are mapped into XMP. For the most part, the tags have obvious
// mappings based on their IFD, tag number, type and count. These tables do not list tags that are
// mapped as subsidiary parts of others, e.g. TIFF SubSecTime or GPS Info GPSDateStamp. Tags that
// have special mappings are marked by having an empty string for the XMP property name.

// ! These tables have the tags listed in the order of tables 3, 4, 5, and 12 of Exif 2.2, with the
// ! exception of ImageUniqueID (which is listed at the end of the Exif mappings). This order is
// ! very important to consistent checking of the legacy status. The NativeDigest properties list
// ! all possible mapped tags in this order. The NativeDigest strings are compared as a whole, so
// ! the same tags listed in a different order would compare as different.

// ! The sentinel tag value can't be 0, that is a valid GPS Info tag, 0xFFFF is unused so far.

enum {
	kExport_Never = 0,		// Never export.
	kExport_Always = 1,		// Add, modify, or delete.
	kExport_NoDelete = 2,	// Add or modify, do not delete if no XMP.
	kExport_InjectOnly = 3	// Add tag if new, never modify or delete existing values.
};

struct TIFF_MappingToXMP {
	XMP_Uns16    id;
	XMP_Uns16    type;
	XMP_Uns32    count;	// Zero means any.
	XMP_Uns8     exportMode;
	const char * name;	// The name of the mapped XMP property. The namespace is implicit.
};

enum { kAnyCount = 0 };

static const TIFF_MappingToXMP sPrimaryIFDMappings[] = {	// A blank name indicates a special mapping.
	{ /*   256 */ kTIFF_ImageWidth,                kTIFF_ShortOrLongType, 1,         kExport_Never,      "ImageWidth" },
	{ /*   257 */ kTIFF_ImageLength,               kTIFF_ShortOrLongType, 1,         kExport_Never,      "ImageLength" },
	{ /*   258 */ kTIFF_BitsPerSample,             kTIFF_ShortType,       3,         kExport_Never,      "BitsPerSample" },
	{ /*   259 */ kTIFF_Compression,               kTIFF_ShortType,       1,         kExport_Never,      "Compression" },
	{ /*   262 */ kTIFF_PhotometricInterpretation, kTIFF_ShortType,       1,         kExport_Never,      "PhotometricInterpretation" },
	{ /*   274 */ kTIFF_Orientation,               kTIFF_ShortType,       1,         kExport_NoDelete,   "Orientation" },
	{ /*   277 */ kTIFF_SamplesPerPixel,           kTIFF_ShortType,       1,         kExport_Never,      "SamplesPerPixel" },
	{ /*   284 */ kTIFF_PlanarConfiguration,       kTIFF_ShortType,       1,         kExport_Never,      "PlanarConfiguration" },
	{ /*   530 */ kTIFF_YCbCrSubSampling,          kTIFF_ShortType,       2,         kExport_Never,      "YCbCrSubSampling" },
	{ /*   531 */ kTIFF_YCbCrPositioning,          kTIFF_ShortType,       1,         kExport_Never,      "YCbCrPositioning" },
	{ /*   282 */ kTIFF_XResolution,               kTIFF_RationalType,    1,         kExport_NoDelete,   "XResolution" },
	{ /*   283 */ kTIFF_YResolution,               kTIFF_RationalType,    1,         kExport_NoDelete,   "YResolution" },
	{ /*   296 */ kTIFF_ResolutionUnit,            kTIFF_ShortType,       1,         kExport_NoDelete,   "ResolutionUnit" },
	{ /*   301 */ kTIFF_TransferFunction,          kTIFF_ShortType,       3*256,     kExport_Never,      "TransferFunction" },
	{ /*   318 */ kTIFF_WhitePoint,                kTIFF_RationalType,    2,         kExport_Never,      "WhitePoint" },
	{ /*   319 */ kTIFF_PrimaryChromaticities,     kTIFF_RationalType,    6,         kExport_Never,      "PrimaryChromaticities" },
	{ /*   529 */ kTIFF_YCbCrCoefficients,         kTIFF_RationalType,    3,         kExport_Never,      "YCbCrCoefficients" },
	{ /*   532 */ kTIFF_ReferenceBlackWhite,       kTIFF_RationalType,    6,         kExport_Never,      "ReferenceBlackWhite" },
	{ /*   306 */ kTIFF_DateTime,                  kTIFF_ASCIIType,       20,        kExport_Always,     "" },	// ! Has a special mapping.
	{ /*   270 */ kTIFF_ImageDescription,          kTIFF_ASCIIType,       kAnyCount, kExport_Always,     "" },	// ! Has a special mapping.
	{ /*   271 */ kTIFF_Make,                      kTIFF_ASCIIType,       kAnyCount, kExport_InjectOnly, "Make" },
	{ /*   272 */ kTIFF_Model,                     kTIFF_ASCIIType,       kAnyCount, kExport_InjectOnly, "Model" },
	{ /*   305 */ kTIFF_Software,                  kTIFF_ASCIIType,       kAnyCount, kExport_Always,     "Software" },	// Has alias to xmp:CreatorTool.
	{ /*   315 */ kTIFF_Artist,                    kTIFF_ASCIIType,       kAnyCount, kExport_Always,     "" },	// ! Has a special mapping.
	{ /* 33432 */ kTIFF_Copyright,                 kTIFF_ASCIIType,       kAnyCount, kExport_Always,     "" },	// ! Has a special mapping.
	{ 0xFFFF, 0, 0, 0 }	// ! Must end with sentinel.
};

// ! A special need, easier than looking up the entry in sExifIFDMappings:
static const TIFF_MappingToXMP kISOSpeedMapping = { kTIFF_ISOSpeedRatings, kTIFF_ShortType, kAnyCount, kExport_InjectOnly, "ISOSpeedRatings" };

static const TIFF_MappingToXMP sExifIFDMappings[] = {
	{ /* 36864 */ kTIFF_ExifVersion,               kTIFF_UndefinedType,   4,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 40960 */ kTIFF_FlashpixVersion,           kTIFF_UndefinedType,   4,         kExport_Never,      "" },	// ! Has a special mapping.
	{ /* 40961 */ kTIFF_ColorSpace,                kTIFF_ShortType,       1,         kExport_InjectOnly, "ColorSpace" },
	{ /* 37121 */ kTIFF_ComponentsConfiguration,   kTIFF_UndefinedType,   4,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 37122 */ kTIFF_CompressedBitsPerPixel,    kTIFF_RationalType,    1,         kExport_InjectOnly, "CompressedBitsPerPixel" },
	{ /* 40962 */ kTIFF_PixelXDimension,           kTIFF_ShortOrLongType, 1,         kExport_InjectOnly, "PixelXDimension" },
	{ /* 40963 */ kTIFF_PixelYDimension,           kTIFF_ShortOrLongType, 1,         kExport_InjectOnly, "PixelYDimension" },
	{ /* 37510 */ kTIFF_UserComment,               kTIFF_UndefinedType,   kAnyCount, kExport_Always,     "" },	// ! Has a special mapping.
	{ /* 40964 */ kTIFF_RelatedSoundFile,          kTIFF_ASCIIType,       kAnyCount, kExport_Always,     "RelatedSoundFile" },	// ! Exif spec says count of 13.
	{ /* 36867 */ kTIFF_DateTimeOriginal,          kTIFF_ASCIIType,       20,        kExport_Always,     "" },	// ! Has a special mapping.
	{ /* 36868 */ kTIFF_DateTimeDigitized,         kTIFF_ASCIIType,       20,        kExport_Always,     "" },	// ! Has a special mapping.
	{ /* 33434 */ kTIFF_ExposureTime,              kTIFF_RationalType,    1,         kExport_InjectOnly, "ExposureTime" },
	{ /* 33437 */ kTIFF_FNumber,                   kTIFF_RationalType,    1,         kExport_InjectOnly, "FNumber" },
	{ /* 34850 */ kTIFF_ExposureProgram,           kTIFF_ShortType,       1,         kExport_InjectOnly, "ExposureProgram" },
	{ /* 34852 */ kTIFF_SpectralSensitivity,       kTIFF_ASCIIType,       kAnyCount, kExport_InjectOnly, "SpectralSensitivity" },
	{ /* 34855 */ kTIFF_ISOSpeedRatings,           kTIFF_ShortType,       kAnyCount, kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 34856 */ kTIFF_OECF,                      kTIFF_UndefinedType,   kAnyCount, kExport_Never,      "" },	// ! Has a special mapping.
	{ /* 37377 */ kTIFF_ShutterSpeedValue,         kTIFF_SRationalType,   1,         kExport_InjectOnly, "ShutterSpeedValue" },
	{ /* 37378 */ kTIFF_ApertureValue,             kTIFF_RationalType,    1,         kExport_InjectOnly, "ApertureValue" },
	{ /* 37379 */ kTIFF_BrightnessValue,           kTIFF_SRationalType,   1,         kExport_InjectOnly, "BrightnessValue" },
	{ /* 37380 */ kTIFF_ExposureBiasValue,         kTIFF_SRationalType,   1,         kExport_InjectOnly, "ExposureBiasValue" },
	{ /* 37381 */ kTIFF_MaxApertureValue,          kTIFF_RationalType,    1,         kExport_InjectOnly, "MaxApertureValue" },
	{ /* 37382 */ kTIFF_SubjectDistance,           kTIFF_RationalType,    1,         kExport_InjectOnly, "SubjectDistance" },
	{ /* 37383 */ kTIFF_MeteringMode,              kTIFF_ShortType,       1,         kExport_InjectOnly, "MeteringMode" },
	{ /* 37384 */ kTIFF_LightSource,               kTIFF_ShortType,       1,         kExport_InjectOnly, "LightSource" },
	{ /* 37385 */ kTIFF_Flash,                     kTIFF_ShortType,       1,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 37386 */ kTIFF_FocalLength,               kTIFF_RationalType,    1,         kExport_InjectOnly, "FocalLength" },
	{ /* 37396 */ kTIFF_SubjectArea,               kTIFF_ShortType,       kAnyCount, kExport_Never,      "SubjectArea" },	// ! Actually 2..4.
	{ /* 41483 */ kTIFF_FlashEnergy,               kTIFF_RationalType,    1,         kExport_InjectOnly, "FlashEnergy" },
	{ /* 41484 */ kTIFF_SpatialFrequencyResponse,  kTIFF_UndefinedType,   kAnyCount, kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 41486 */ kTIFF_FocalPlaneXResolution,     kTIFF_RationalType,    1,         kExport_InjectOnly, "FocalPlaneXResolution" },
	{ /* 41487 */ kTIFF_FocalPlaneYResolution,     kTIFF_RationalType,    1,         kExport_InjectOnly, "FocalPlaneYResolution" },
	{ /* 41488 */ kTIFF_FocalPlaneResolutionUnit,  kTIFF_ShortType,       1,         kExport_InjectOnly, "FocalPlaneResolutionUnit" },
	{ /* 41492 */ kTIFF_SubjectLocation,           kTIFF_ShortType,       2,         kExport_Never,      "SubjectLocation" },
	{ /* 41493 */ kTIFF_ExposureIndex,             kTIFF_RationalType,    1,         kExport_InjectOnly, "ExposureIndex" },
	{ /* 41495 */ kTIFF_SensingMethod,             kTIFF_ShortType,       1,         kExport_InjectOnly, "SensingMethod" },
	{ /* 41728 */ kTIFF_FileSource,                kTIFF_UndefinedType,   1,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 41729 */ kTIFF_SceneType,                 kTIFF_UndefinedType,   1,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 41730 */ kTIFF_CFAPattern,                kTIFF_UndefinedType,   kAnyCount, kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 41985 */ kTIFF_CustomRendered,            kTIFF_ShortType,       1,         kExport_Never,      "CustomRendered" },
	{ /* 41986 */ kTIFF_ExposureMode,              kTIFF_ShortType,       1,         kExport_InjectOnly, "ExposureMode" },
	{ /* 41987 */ kTIFF_WhiteBalance,              kTIFF_ShortType,       1,         kExport_InjectOnly, "WhiteBalance" },
	{ /* 41988 */ kTIFF_DigitalZoomRatio,          kTIFF_RationalType,    1,         kExport_InjectOnly, "DigitalZoomRatio" },
	{ /* 41989 */ kTIFF_FocalLengthIn35mmFilm,     kTIFF_ShortType,       1,         kExport_InjectOnly, "FocalLengthIn35mmFilm" },
	{ /* 41990 */ kTIFF_SceneCaptureType,          kTIFF_ShortType,       1,         kExport_InjectOnly, "SceneCaptureType" },
	{ /* 41991 */ kTIFF_GainControl,               kTIFF_ShortType,       1,         kExport_InjectOnly, "GainControl" },
	{ /* 41992 */ kTIFF_Contrast,                  kTIFF_ShortType,       1,         kExport_InjectOnly, "Contrast" },
	{ /* 41993 */ kTIFF_Saturation,                kTIFF_ShortType,       1,         kExport_InjectOnly, "Saturation" },
	{ /* 41994 */ kTIFF_Sharpness,                 kTIFF_ShortType,       1,         kExport_InjectOnly, "Sharpness" },
	{ /* 41995 */ kTIFF_DeviceSettingDescription,  kTIFF_UndefinedType,   kAnyCount, kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /* 41996 */ kTIFF_SubjectDistanceRange,      kTIFF_ShortType,       1,         kExport_InjectOnly, "SubjectDistanceRange" },
	{ /* 42016 */ kTIFF_ImageUniqueID,             kTIFF_ASCIIType,       33,        kExport_InjectOnly, "ImageUniqueID" },
	{ 0xFFFF, 0, 0, 0 }	// ! Must end with sentinel.
};

static const TIFF_MappingToXMP sGPSInfoIFDMappings[] = {
	{ /*     0 */ kTIFF_GPSVersionID,              kTIFF_ByteType,        4,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /*     2 */ kTIFF_GPSLatitude,               kTIFF_RationalType,    3,         kExport_Always,     "" },	// ! Has a special mapping.
	{ /*     4 */ kTIFF_GPSLongitude,              kTIFF_RationalType,    3,         kExport_Always,     "" },	// ! Has a special mapping.
	{ /*     5 */ kTIFF_GPSAltitudeRef,            kTIFF_ByteType,        1,         kExport_Always,     "GPSAltitudeRef" },
	{ /*     6 */ kTIFF_GPSAltitude,               kTIFF_RationalType,    1,         kExport_Always,     "GPSAltitude" },
	{ /*     7 */ kTIFF_GPSTimeStamp,              kTIFF_RationalType,    3,         kExport_Always,     "" },	// ! Has a special mapping.
	{ /*     8 */ kTIFF_GPSSatellites,             kTIFF_ASCIIType,       kAnyCount, kExport_InjectOnly, "GPSSatellites" },
	{ /*     9 */ kTIFF_GPSStatus,                 kTIFF_ASCIIType,       2,         kExport_InjectOnly, "GPSStatus" },
	{ /*    10 */ kTIFF_GPSMeasureMode,            kTIFF_ASCIIType,       2,         kExport_InjectOnly, "GPSMeasureMode" },
	{ /*    11 */ kTIFF_GPSDOP,                    kTIFF_RationalType,    1,         kExport_InjectOnly, "GPSDOP" },
	{ /*    12 */ kTIFF_GPSSpeedRef,               kTIFF_ASCIIType,       2,         kExport_InjectOnly, "GPSSpeedRef" },
	{ /*    13 */ kTIFF_GPSSpeed,                  kTIFF_RationalType,    1,         kExport_InjectOnly, "GPSSpeed" },
	{ /*    14 */ kTIFF_GPSTrackRef,               kTIFF_ASCIIType,       2,         kExport_InjectOnly, "GPSTrackRef" },
	{ /*    15 */ kTIFF_GPSTrack,                  kTIFF_RationalType,    1,         kExport_InjectOnly, "GPSTrack" },
	{ /*    16 */ kTIFF_GPSImgDirectionRef,        kTIFF_ASCIIType,       2,         kExport_InjectOnly, "GPSImgDirectionRef" },
	{ /*    17 */ kTIFF_GPSImgDirection,           kTIFF_RationalType,    1,         kExport_InjectOnly, "GPSImgDirection" },
	{ /*    18 */ kTIFF_GPSMapDatum,               kTIFF_ASCIIType,       kAnyCount, kExport_InjectOnly, "GPSMapDatum" },
	{ /*    20 */ kTIFF_GPSDestLatitude,           kTIFF_RationalType,    3,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /*    22 */ kTIFF_GPSDestLongitude,          kTIFF_RationalType,    3,         kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /*    23 */ kTIFF_GPSDestBearingRef,         kTIFF_ASCIIType,       2,         kExport_InjectOnly, "GPSDestBearingRef" },
	{ /*    24 */ kTIFF_GPSDestBearing,            kTIFF_RationalType,    1,         kExport_InjectOnly, "GPSDestBearing" },
	{ /*    25 */ kTIFF_GPSDestDistanceRef,        kTIFF_ASCIIType,       2,         kExport_InjectOnly, "GPSDestDistanceRef" },
	{ /*    26 */ kTIFF_GPSDestDistance,           kTIFF_RationalType,    1,         kExport_InjectOnly, "GPSDestDistance" },
	{ /*    27 */ kTIFF_GPSProcessingMethod,       kTIFF_UndefinedType,   kAnyCount, kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /*    28 */ kTIFF_GPSAreaInformation,        kTIFF_UndefinedType,   kAnyCount, kExport_InjectOnly, "" },	// ! Has a special mapping.
	{ /*    30 */ kTIFF_GPSDifferential,           kTIFF_ShortType,       1,         kExport_InjectOnly, "GPSDifferential" },
	{ 0xFFFF, 0, 0, 0 }	// ! Must end with sentinel.
};

// =================================================================================================

static void	// ! Needed by Import2WayExif
ExportTIFF_Date ( const SXMPMeta & xmp, const char * xmpNS, const char * xmpProp, TIFF_Manager * tiff, XMP_Uns16 mainID );

// =================================================================================================

static XMP_Uns32 GatherInt ( const char * strPtr, size_t count )
{
	XMP_Uns32 value = 0;
	const char * strEnd = strPtr + count;

	while ( strPtr < strEnd ) {
		char ch = *strPtr;
		if ( (ch < '0') || (ch > '9') ) break;
		value = value*10 + (ch - '0');
		++strPtr;
	}

	return value;

}	// GatherInt

// =================================================================================================

static void TrimTrailingSpaces ( TIFF_Manager::TagInfo * info )
{
	if ( info->dataLen == 0 ) return;
	XMP_Assert ( info->dataPtr != 0 );

	char * firstChar = (char*)info->dataPtr;
	char * lastChar  = firstChar + info->dataLen - 1;
	
	if ( (*lastChar != ' ') && (*lastChar != 0) ) return;	// Nothing to do.
	
	while ( (firstChar <= lastChar) && ((*lastChar == ' ') || (*lastChar == 0)) ) --lastChar;
	
	XMP_Assert ( (lastChar == firstChar-1) ||
				 ((lastChar >= firstChar) && (*lastChar != ' ') && (*lastChar != 0)) );
	
	++lastChar;
	XMP_Uns32 newLen = (XMP_Uns32)(lastChar - firstChar) + 1;
	XMP_Assert ( newLen <= info->dataLen );

	*lastChar = 0;
	info->dataLen = newLen;

}	// TrimTrailingSpaces

// =================================================================================================

bool PhotoDataUtils::GetNativeInfo ( const TIFF_Manager & exif, XMP_Uns8 ifd, XMP_Uns16 id, TIFF_Manager::TagInfo * info )
{
	bool haveExif = exif.GetTag ( ifd, id, info );

	if ( haveExif ) {

		XMP_Uns32 i;
		char * chPtr;
		
		XMP_Assert ( (info->dataPtr != 0) || (info->dataLen == 0) );	// Null pointer requires zero length.

		bool isDate = ((id == kTIFF_DateTime) || (id == kTIFF_DateTimeOriginal) || (id == kTIFF_DateTimeOriginal));

		for ( i = 0, chPtr = (char*)info->dataPtr; i < info->dataLen; ++i, ++chPtr ) {
			if ( isDate && (*chPtr == ':') ) continue;	// Ignore colons, empty dates have spaces and colons.
			if ( (*chPtr != ' ') && (*chPtr != 0) ) break;	// Break if the Exif value is non-empty.
		}

		if ( i == info->dataLen ) {
			haveExif = false;	// Ignore empty Exif.
		} else {
			TrimTrailingSpaces ( info );
			if ( info->dataLen == 0 ) haveExif = false;
		}

	}

	return haveExif;

}	// PhotoDataUtils::GetNativeInfo

// =================================================================================================

size_t PhotoDataUtils::GetNativeInfo ( const IPTC_Manager & iptc, XMP_Uns8 id, int digestState, bool haveXMP, IPTC_Manager::DataSetInfo * info )
{
	size_t iptcCount = 0;

	if ( (digestState == kDigestDiffers) || ((digestState == kDigestMissing) && (! haveXMP)) ) {
		iptcCount = iptc.GetDataSet ( id, info );
	}
	
	if ( ignoreLocalText && (iptcCount > 0) && (! iptc.UsingUTF8()) ) {
		// Check to see if the new value(s) should be ignored.
		size_t i;
		IPTC_Manager::DataSetInfo tmpInfo;
		for ( i = 0; i < iptcCount; ++i ) {
			(void) iptc.GetDataSet ( id, &tmpInfo, i );
			if ( ReconcileUtils::IsASCII ( tmpInfo.dataPtr, tmpInfo.dataLen ) ) break;
		}
		if ( i == iptcCount ) iptcCount = 0;	// Return 0 if value(s) should be ignored.
	}

	return iptcCount;

}	// PhotoDataUtils::GetNativeInfo

// =================================================================================================

bool PhotoDataUtils::IsValueDifferent ( const TIFF_Manager::TagInfo & exifInfo, const std::string & xmpValue, std::string * exifValue )
{
	if ( exifInfo.dataLen == 0 ) return false;	// Ignore empty Exif values.

	if ( ReconcileUtils::IsUTF8 ( exifInfo.dataPtr, exifInfo.dataLen ) ) {	// ! Note that ASCII is UTF-8.
		exifValue->assign ( (char*)exifInfo.dataPtr, exifInfo.dataLen );
	} else {
		if ( ignoreLocalText ) return false;
		ReconcileUtils::LocalToUTF8 ( exifInfo.dataPtr, exifInfo.dataLen, exifValue );
	}

	return (*exifValue != xmpValue);

}	// PhotoDataUtils::IsValueDifferent

// =================================================================================================

bool PhotoDataUtils::IsValueDifferent ( const IPTC_Manager & newIPTC, const IPTC_Manager & oldIPTC, XMP_Uns8 id )
{
	IPTC_Manager::DataSetInfo newInfo;
	size_t newCount = newIPTC.GetDataSet ( id, &newInfo );
	if ( newCount == 0 ) return false;	// Ignore missing new IPTC values.

	IPTC_Manager::DataSetInfo oldInfo;
	size_t oldCount = oldIPTC.GetDataSet ( id, &oldInfo );
	if ( oldCount == 0 ) return true;	// Missing old IPTC values differ.
	
	if ( newCount != oldCount ) return true;

	std::string oldStr, newStr;

	for ( newCount = 0; newCount < oldCount; ++newCount ) {

		if ( ignoreLocalText & (! newIPTC.UsingUTF8()) ) {	// Check to see if the new value should be ignored.
			(void) newIPTC.GetDataSet ( id, &newInfo, newCount );
			if ( ! ReconcileUtils::IsASCII ( newInfo.dataPtr, newInfo.dataLen ) ) continue;
		}

		(void) newIPTC.GetDataSet_UTF8 ( id, &newStr, newCount );
		(void) oldIPTC.GetDataSet_UTF8 ( id, &oldStr, newCount );
		if ( newStr.size() == 0 ) continue;	// Ignore empty new IPTC.
		if ( newStr != oldStr ) break;

	}

	return ( newCount != oldCount );	// Not different if all values matched.
	
}	// PhotoDataUtils::IsValueDifferent

// =================================================================================================
// =================================================================================================

// =================================================================================================
// ImportSingleTIFF_Short
// ======================

static void
ImportSingleTIFF_Short ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns16 binValue = *((XMP_Uns16*)tagInfo.dataPtr);
		if ( ! nativeEndian ) binValue = Flip2 ( binValue );

		char strValue[20];
		snprintf ( strValue, sizeof(strValue), "%hu", binValue );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_Short

// =================================================================================================
// ImportSingleTIFF_Long
// =====================

static void
ImportSingleTIFF_Long ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns32 binValue = *((XMP_Uns32*)tagInfo.dataPtr);
		if ( ! nativeEndian ) binValue = Flip4 ( binValue );

		char strValue[20];
		snprintf ( strValue, sizeof(strValue), "%lu", (unsigned long)binValue );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_Long

// =================================================================================================
// ImportSingleTIFF_Rational
// =========================

static void
ImportSingleTIFF_Rational ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
							SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns32 * binPtr = (XMP_Uns32*)tagInfo.dataPtr;
		XMP_Uns32 binNum   = binPtr[0];
		XMP_Uns32 binDenom = binPtr[1];
		if ( ! nativeEndian ) {
			binNum = Flip4 ( binNum );
			binDenom = Flip4 ( binDenom );
		}

		char strValue[40];
		snprintf ( strValue, sizeof(strValue), "%lu/%lu", (unsigned long)binNum, (unsigned long)binDenom );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_Rational

// =================================================================================================
// ImportSingleTIFF_SRational
// ==========================

static void
ImportSingleTIFF_SRational ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
							 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int32 * binPtr = (XMP_Int32*)tagInfo.dataPtr;
		XMP_Int32 binNum   = binPtr[0];
		XMP_Int32 binDenom = binPtr[1];
		if ( ! nativeEndian ) {
			Flip4 ( &binNum );
			Flip4 ( &binDenom );
		}

		char strValue[40];
		snprintf ( strValue, sizeof(strValue), "%ld/%ld", (unsigned long)binNum, (unsigned long)binDenom );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_SRational

// =================================================================================================
// ImportSingleTIFF_ASCII
// ======================

static void
ImportSingleTIFF_ASCII ( const TIFF_Manager::TagInfo & tagInfo,
						 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.
	
		TrimTrailingSpaces ( (TIFF_Manager::TagInfo*) &tagInfo );
		if ( tagInfo.dataLen == 0 ) return;	// Ignore empty tags.

		const char * chPtr  = (const char *)tagInfo.dataPtr;
		const bool   hasNul = (chPtr[tagInfo.dataLen-1] == 0);
		const bool   isUTF8 = ReconcileUtils::IsUTF8 ( chPtr, tagInfo.dataLen );

		if ( isUTF8 && hasNul ) {
			xmp->SetProperty ( xmpNS, xmpProp, chPtr );
		} else {
			std::string strValue;
			if ( isUTF8 ) {
				strValue.assign ( chPtr, tagInfo.dataLen );
			} else {
				if ( ignoreLocalText ) return;
				ReconcileUtils::LocalToUTF8 ( chPtr, tagInfo.dataLen, &strValue );
			}
			xmp->SetProperty ( xmpNS, xmpProp, strValue.c_str() );
		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_ASCII

// =================================================================================================
// ImportSingleTIFF_Byte
// =====================

static void
ImportSingleTIFF_Byte ( const TIFF_Manager::TagInfo & tagInfo,
						SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns8 binValue = *((XMP_Uns8*)tagInfo.dataPtr);

		char strValue[20];
		snprintf ( strValue, sizeof(strValue), "%hu", binValue );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_Byte

// =================================================================================================
// ImportSingleTIFF_SByte
// ======================

static void
ImportSingleTIFF_SByte ( const TIFF_Manager::TagInfo & tagInfo,
						 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int8 binValue = *((XMP_Int8*)tagInfo.dataPtr);

		char strValue[20];
		snprintf ( strValue, sizeof(strValue), "%hd", binValue );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_SByte

// =================================================================================================
// ImportSingleTIFF_SShort
// =======================

static void
ImportSingleTIFF_SShort ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int16 binValue = *((XMP_Int16*)tagInfo.dataPtr);
		if ( ! nativeEndian ) Flip2 ( &binValue );

		char strValue[20];
		snprintf ( strValue, sizeof(strValue), "%hd", binValue );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_SShort

// =================================================================================================
// ImportSingleTIFF_SLong
// ======================

static void
ImportSingleTIFF_SLong ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int32 binValue = *((XMP_Int32*)tagInfo.dataPtr);
		if ( ! nativeEndian ) Flip4 ( &binValue );

		char strValue[20];
		snprintf ( strValue, sizeof(strValue), "%ld", (long)binValue );	// AUDIT: Using sizeof(strValue) is safe.

		xmp->SetProperty ( xmpNS, xmpProp, strValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_SLong

// =================================================================================================
// ImportSingleTIFF_Float
// ======================

static void
ImportSingleTIFF_Float ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		float binValue = *((float*)tagInfo.dataPtr);
		if ( ! nativeEndian ) Flip4 ( &binValue );

		xmp->SetProperty_Float ( xmpNS, xmpProp, binValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_Float

// =================================================================================================
// ImportSingleTIFF_Double
// =======================

static void
ImportSingleTIFF_Double ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		double binValue = *((double*)tagInfo.dataPtr);
		if ( ! nativeEndian ) Flip8 ( &binValue );

		xmp->SetProperty_Float ( xmpNS, xmpProp, binValue );	// ! Yes, SetProperty_Float.

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportSingleTIFF_Double

// =================================================================================================
// ImportSingleTIFF
// ================

static void
ImportSingleTIFF ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
				   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{

	// We've got a tag to map to XMP, decide how based on actual type and the expected count. Using
	// the actual type eliminates a ShortOrLong case. Using the expected count is needed to know
	// whether to create an XMP array. The actual count for an array could be 1. Put the most
	// common cases first for better iCache utilization.

	switch ( tagInfo.type ) {

		case kTIFF_ShortType :
			ImportSingleTIFF_Short ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_LongType :
			ImportSingleTIFF_Long ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_RationalType :
			ImportSingleTIFF_Rational ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SRationalType :
			ImportSingleTIFF_SRational ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_ASCIIType :
			ImportSingleTIFF_ASCII ( tagInfo, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_ByteType :
			ImportSingleTIFF_Byte ( tagInfo, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SByteType :
			ImportSingleTIFF_SByte ( tagInfo, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SShortType :
			ImportSingleTIFF_SShort ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SLongType :
			ImportSingleTIFF_SLong ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_FloatType :
			ImportSingleTIFF_Float ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_DoubleType :
			ImportSingleTIFF_Double ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

	}

}	// ImportSingleTIFF

// =================================================================================================
// =================================================================================================

// =================================================================================================
// ImportArrayTIFF_Short
// =====================

static void
ImportArrayTIFF_Short ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns16 * binPtr = (XMP_Uns16*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			XMP_Uns16 binValue = *binPtr;
			if ( ! nativeEndian ) binValue = Flip2 ( binValue );

			char strValue[20];
			snprintf ( strValue, sizeof(strValue), "%hu", binValue );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_Short

// =================================================================================================
// ImportArrayTIFF_Long
// ====================

static void
ImportArrayTIFF_Long ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
					   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns32 * binPtr = (XMP_Uns32*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			XMP_Uns32 binValue = *binPtr;
			if ( ! nativeEndian ) binValue = Flip4 ( binValue );

			char strValue[20];
			snprintf ( strValue, sizeof(strValue), "%lu", (unsigned long)binValue );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_Long

// =================================================================================================
// ImportArrayTIFF_Rational
// ========================

static void
ImportArrayTIFF_Rational ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns32 * binPtr = (XMP_Uns32*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, binPtr += 2 ) {

			XMP_Uns32 binNum   = binPtr[0];
			XMP_Uns32 binDenom = binPtr[1];
			if ( ! nativeEndian ) {
				binNum = Flip4 ( binNum );
				binDenom = Flip4 ( binDenom );
			}

			char strValue[40];
			snprintf ( strValue, sizeof(strValue), "%lu/%lu", (unsigned long)binNum, (unsigned long)binDenom );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_Rational

// =================================================================================================
// ImportArrayTIFF_SRational
// =========================

static void
ImportArrayTIFF_SRational ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
							SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int32 * binPtr = (XMP_Int32*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, binPtr += 2 ) {

			XMP_Int32 binNum   = binPtr[0];
			XMP_Int32 binDenom = binPtr[1];
			if ( ! nativeEndian ) {
				Flip4 ( &binNum );
				Flip4 ( &binDenom );
			}

			char strValue[40];
			snprintf ( strValue, sizeof(strValue), "%ld/%ld", (long)binNum, (long)binDenom );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_SRational

// =================================================================================================
// ImportArrayTIFF_ASCII
// =====================

static void
ImportArrayTIFF_ASCII ( const TIFF_Manager::TagInfo & tagInfo,
						SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.
	
		TrimTrailingSpaces ( (TIFF_Manager::TagInfo*) &tagInfo );
		if ( tagInfo.dataLen == 0 ) return;	// Ignore empty tags.

		const char * chPtr  = (const char *)tagInfo.dataPtr;
		const char * chEnd  = chPtr + tagInfo.dataLen;
		const bool   hasNul = (chPtr[tagInfo.dataLen-1] == 0);
		const bool   isUTF8 = ReconcileUtils::IsUTF8 ( chPtr, tagInfo.dataLen );

		std::string  strValue;

		if ( (! isUTF8) || (! hasNul) ) {
			if ( isUTF8 ) {
				strValue.assign ( chPtr, tagInfo.dataLen );
			} else {
				if ( ignoreLocalText ) return;
				ReconcileUtils::LocalToUTF8 ( chPtr, tagInfo.dataLen, &strValue );
			}
			chPtr = strValue.c_str();
			chEnd = chPtr + strValue.size();
		}
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( ; chPtr < chEnd; chPtr += (strlen(chPtr) + 1) ) {
			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, chPtr );
		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_ASCII

// =================================================================================================
// ImportArrayTIFF_Byte
// ====================

static void
ImportArrayTIFF_Byte ( const TIFF_Manager::TagInfo & tagInfo,
					   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns8 * binPtr = (XMP_Uns8*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			XMP_Uns8 binValue = *binPtr;

			char strValue[20];
			snprintf ( strValue, sizeof(strValue), "%hu", binValue );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_Byte

// =================================================================================================
// ImportArrayTIFF_SByte
// =====================

static void
ImportArrayTIFF_SByte ( const TIFF_Manager::TagInfo & tagInfo,
						SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int8 * binPtr = (XMP_Int8*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			XMP_Int8 binValue = *binPtr;

			char strValue[20];
			snprintf ( strValue, sizeof(strValue), "%hd", binValue );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_SByte

// =================================================================================================
// ImportArrayTIFF_SShort
// ======================

static void
ImportArrayTIFF_SShort ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int16 * binPtr = (XMP_Int16*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			XMP_Int16 binValue = *binPtr;
			if ( ! nativeEndian ) Flip2 ( &binValue );

			char strValue[20];
			snprintf ( strValue, sizeof(strValue), "%hd", binValue );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_SShort

// =================================================================================================
// ImportArrayTIFF_SLong
// =====================

static void
ImportArrayTIFF_SLong ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Int32 * binPtr = (XMP_Int32*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			XMP_Int32 binValue = *binPtr;
			if ( ! nativeEndian ) Flip4 ( &binValue );

			char strValue[20];
			snprintf ( strValue, sizeof(strValue), "%ld", (long)binValue );	// AUDIT: Using sizeof(strValue) is safe.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_SLong

// =================================================================================================
// ImportArrayTIFF_Float
// =====================

static void
ImportArrayTIFF_Float ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		float * binPtr = (float*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			float binValue = *binPtr;
			if ( ! nativeEndian ) Flip4 ( &binValue );

			std::string strValue;
			SXMPUtils::ConvertFromFloat ( binValue, "", &strValue );

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue.c_str() );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_Float

// =================================================================================================
// ImportArrayTIFF_Double
// ======================

static void
ImportArrayTIFF_Double ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
						 SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		double * binPtr = (double*)tagInfo.dataPtr;
		
		xmp->DeleteProperty ( xmpNS, xmpProp );	// ! Don't keep appending, create a new array.

		for ( size_t i = 0; i < tagInfo.count; ++i, ++binPtr ) {

			double binValue = *binPtr;
			if ( ! nativeEndian ) Flip8 ( &binValue );

			std::string strValue;
			SXMPUtils::ConvertFromFloat ( binValue, "", &strValue );	// ! Yes, ConvertFromFloat.

			xmp->AppendArrayItem ( xmpNS, xmpProp, kXMP_PropArrayIsOrdered, strValue.c_str() );

		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportArrayTIFF_Double

// =================================================================================================
// ImportArrayTIFF
// ===============

static void
ImportArrayTIFF ( const TIFF_Manager::TagInfo & tagInfo, const bool nativeEndian,
				  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{

	// We've got a tag to map to XMP, decide how based on actual type and the expected count. Using
	// the actual type eliminates a ShortOrLong case. Using the expected count is needed to know
	// whether to create an XMP array. The actual count for an array could be 1. Put the most
	// common cases first for better iCache utilization.

	switch ( tagInfo.type ) {

		case kTIFF_ShortType :
			ImportArrayTIFF_Short ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_LongType :
			ImportArrayTIFF_Long ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_RationalType :
			ImportArrayTIFF_Rational ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SRationalType :
			ImportArrayTIFF_SRational ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_ASCIIType :
			ImportArrayTIFF_ASCII ( tagInfo, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_ByteType :
			ImportArrayTIFF_Byte ( tagInfo, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SByteType :
			ImportArrayTIFF_SByte ( tagInfo, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SShortType :
			ImportArrayTIFF_SShort ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_SLongType :
			ImportArrayTIFF_SLong ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_FloatType :
			ImportArrayTIFF_Float ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

		case kTIFF_DoubleType :
			ImportArrayTIFF_Double ( tagInfo, nativeEndian, xmp, xmpNS, xmpProp );
			break;

	}

}	// ImportArrayTIFF

// =================================================================================================
// ImportTIFF_CheckStandardMapping
// ===============================

static bool
ImportTIFF_CheckStandardMapping ( const TIFF_Manager::TagInfo & tagInfo, const TIFF_MappingToXMP & mapInfo )
{
	XMP_Assert ( (kTIFF_ByteType <= tagInfo.type) && (tagInfo.type <= kTIFF_LastType) );
	XMP_Assert ( mapInfo.type <= kTIFF_LastType );

	if ( (tagInfo.type < kTIFF_ByteType) || (tagInfo.type > kTIFF_LastType) ) return false;

	if ( tagInfo.type != mapInfo.type ) {
		if ( mapInfo.type != kTIFF_ShortOrLongType ) return false;
		if ( (tagInfo.type != kTIFF_ShortType) && (tagInfo.type != kTIFF_LongType) ) return false;
	}

	if ( (tagInfo.count != mapInfo.count) &&	// Maybe there is a problem because the counts don't match.
		 // (mapInfo.count != kAnyCount) &&		... don't need this because of the new check below ...
		 (mapInfo.count == 1) ) return false;	// Be tolerant of mismatch in expected array size.

	return true;

}	// ImportTIFF_CheckStandardMapping

// =================================================================================================
// ImportTIFF_StandardMappings
// ===========================

static void
ImportTIFF_StandardMappings ( XMP_Uns8 ifd, const TIFF_Manager & tiff, SXMPMeta * xmp )
{
	const bool nativeEndian = tiff.IsNativeEndian();
	TIFF_Manager::TagInfo tagInfo;

	const TIFF_MappingToXMP * mappings = 0;
	const char * xmpNS = 0;

	if ( ifd == kTIFF_PrimaryIFD ) {
		mappings = sPrimaryIFDMappings;
		xmpNS    = kXMP_NS_TIFF;
	} else if ( ifd == kTIFF_ExifIFD ) {
		mappings = sExifIFDMappings;
		xmpNS    = kXMP_NS_EXIF;
	} else if ( ifd == kTIFF_GPSInfoIFD ) {
		mappings = sGPSInfoIFDMappings;
		xmpNS    = kXMP_NS_EXIF;	// ! Yes, the GPS Info tags go into the exif: namespace.
	} else {
		XMP_Throw ( "Invalid IFD for standard mappings", kXMPErr_InternalFailure );
	}

	for ( size_t i = 0; mappings[i].id != 0xFFFF; ++i ) {

		try {	// Don't let errors with one stop the others.

			const TIFF_MappingToXMP & mapInfo =  mappings[i];
			const bool mapSingle = ((mapInfo.count == 1) || (mapInfo.type == kTIFF_ASCIIType));

			if ( mapInfo.name[0] == 0 ) continue;	// Skip special mappings, handled higher up.
			
			bool found = tiff.GetTag ( ifd, mapInfo.id, &tagInfo );
			if ( ! found ) continue;

			XMP_Assert ( tagInfo.type != kTIFF_UndefinedType );	// These must have a special mapping.
			if ( tagInfo.type == kTIFF_UndefinedType ) continue;
			if ( ! ImportTIFF_CheckStandardMapping ( tagInfo, mapInfo ) ) continue;

			if ( mapSingle ) {
				ImportSingleTIFF ( tagInfo, nativeEndian, xmp, xmpNS, mapInfo.name );
			} else {
				ImportArrayTIFF ( tagInfo, nativeEndian, xmp, xmpNS, mapInfo.name );
			}

		} catch ( ... ) {

			// Do nothing, let other imports proceed.
			// ? Notify client?

		}

	}

}	// ImportTIFF_StandardMappings

// =================================================================================================
// =================================================================================================

// =================================================================================================
// ImportTIFF_Date
// ===============
//
// Convert an Exif 2.2 master date/time tag plus associated fractional seconds to an XMP date/time.
// The Exif date/time part is a 20 byte ASCII value formatted as "YYYY:MM:DD HH:MM:SS" with a
// terminating nul. Any of the numeric portions can be blanks if unknown. The fractional seconds
// are a nul terminated ASCII string with possible space padding. They are literally the fractional
// part, the digits that would be to the right of the decimal point.

static void
ImportTIFF_Date ( const TIFF_Manager & tiff, const TIFF_Manager::TagInfo & dateInfo,
				  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	XMP_Uns16 secID;
	switch ( dateInfo.id ) {
		case kTIFF_DateTime          : secID = kTIFF_SubSecTime;			break;
		case kTIFF_DateTimeOriginal  : secID = kTIFF_SubSecTimeOriginal;	break;
		case kTIFF_DateTimeDigitized : secID = kTIFF_SubSecTimeDigitized;	break;
	}
	
	try {	// Don't let errors with one stop the others.
	
		if ( (dateInfo.type != kTIFF_ASCIIType) || (dateInfo.count != 20) ) return;

		const char * dateStr = (const char *) dateInfo.dataPtr;
		if ( (dateStr[4] != ':')  || (dateStr[7] != ':')  ||
			 (dateStr[10] != ' ') || (dateStr[13] != ':') || (dateStr[16] != ':') ) return;

		XMP_DateTime binValue;

		binValue.year   = GatherInt ( &dateStr[0], 4 );
		binValue.month  = GatherInt ( &dateStr[5], 2 );
		binValue.day    = GatherInt ( &dateStr[8], 2 );
		if ( (binValue.year != 0) | (binValue.month != 0) | (binValue.day != 0) ) binValue.hasDate = true;

		binValue.hour   = GatherInt ( &dateStr[11], 2 );
		binValue.minute = GatherInt ( &dateStr[14], 2 );
		binValue.second = GatherInt ( &dateStr[17], 2 );
		binValue.nanoSecond = 0;	// Get the fractional seconds later.
		if ( (binValue.hour != 0) | (binValue.minute != 0) | (binValue.second != 0) ) binValue.hasTime = true;

		binValue.tzSign = 0;	// ! Separate assignment, avoid VS integer truncation warning.
		binValue.tzHour = binValue.tzMinute = 0;
		binValue.hasTimeZone = false;	// Exif times have no zone.

		// *** Consider looking at the TIFF/EP TimeZoneOffset tag?

		TIFF_Manager::TagInfo secInfo;
		bool found = tiff.GetTag ( kTIFF_ExifIFD, secID, &secInfo );	// ! Subseconds are all in the Exif IFD.

		if ( found && (secInfo.type == kTIFF_ASCIIType) ) {
			const char * fracPtr = (const char *) secInfo.dataPtr;
			binValue.nanoSecond = GatherInt ( fracPtr, secInfo.dataLen );
			size_t digits = 0;
			for ( ; (('0' <= *fracPtr) && (*fracPtr <= '9')); ++fracPtr ) ++digits;
			for ( ; digits < 9; ++digits ) binValue.nanoSecond *= 10;
			if ( binValue.nanoSecond != 0 ) binValue.hasTime = true;
		}

		xmp->SetProperty_Date ( xmpNS, xmpProp, binValue );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_Date

// =================================================================================================
// ImportTIFF_LocTextASCII
// =======================

static void
ImportTIFF_LocTextASCII ( const TIFF_Manager & tiff, XMP_Uns8 ifd, XMP_Uns16 tagID,
						  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		TIFF_Manager::TagInfo tagInfo;

		bool found = tiff.GetTag ( ifd, tagID, &tagInfo );
		if ( (! found) || (tagInfo.type != kTIFF_ASCIIType) ) return;

		const char * chPtr  = (const char *)tagInfo.dataPtr;
		const bool   hasNul = (chPtr[tagInfo.dataLen-1] == 0);
		const bool   isUTF8 = ReconcileUtils::IsUTF8 ( chPtr, tagInfo.dataLen );

		if ( isUTF8 && hasNul ) {
			xmp->SetLocalizedText ( xmpNS, xmpProp, "", "x-default", chPtr );
		} else {
			std::string strValue;
			if ( isUTF8 ) {
				strValue.assign ( chPtr, tagInfo.dataLen );
			} else {
				if ( ignoreLocalText ) return;
				ReconcileUtils::LocalToUTF8 ( chPtr, tagInfo.dataLen, &strValue );
			}
			xmp->SetLocalizedText ( xmpNS, xmpProp, "", "x-default", strValue.c_str() );
		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_LocTextASCII

// =================================================================================================
// ImportTIFF_EncodedString
// ========================

static void
ImportTIFF_EncodedString ( const TIFF_Manager & tiff, const TIFF_Manager::TagInfo & tagInfo,
						   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp, bool isLangAlt = false )
{
	try {	// Don't let errors with one stop the others.

		std::string strValue;

		bool ok = tiff.DecodeString ( tagInfo.dataPtr, tagInfo.dataLen, &strValue );
		if ( ! ok ) return;

		if ( ! isLangAlt ) {
			xmp->SetProperty ( xmpNS, xmpProp, strValue.c_str() );
		} else {
			xmp->SetLocalizedText ( xmpNS, xmpProp, "", "x-default", strValue.c_str() );
		}

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_EncodedString

// =================================================================================================
// ImportTIFF_Flash
// ================

static void
ImportTIFF_Flash ( const TIFF_Manager::TagInfo & tagInfo, bool nativeEndian,
				   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		XMP_Uns16 binValue = *((XMP_Uns16*)tagInfo.dataPtr);
		if ( ! nativeEndian ) binValue = Flip2 ( binValue );

		bool fired    = (bool)(binValue & 1);	// Avoid implicit 0/1 conversion.
		int  rtrn     = (binValue >> 1) & 3;
		int  mode     = (binValue >> 3) & 3;
		bool function = (bool)((binValue >> 5) & 1);
		bool redEye   = (bool)((binValue >> 6) & 1);

		static const char * sTwoBits[] = { "0", "1", "2", "3" };

		xmp->SetStructField ( kXMP_NS_EXIF, "Flash", kXMP_NS_EXIF, "Fired", (fired ? kXMP_TrueStr : kXMP_FalseStr) );
		xmp->SetStructField ( kXMP_NS_EXIF, "Flash", kXMP_NS_EXIF, "Return", sTwoBits[rtrn] );
		xmp->SetStructField ( kXMP_NS_EXIF, "Flash", kXMP_NS_EXIF, "Mode", sTwoBits[mode] );
		xmp->SetStructField ( kXMP_NS_EXIF, "Flash", kXMP_NS_EXIF, "Function", (function ? kXMP_TrueStr : kXMP_FalseStr) );
		xmp->SetStructField ( kXMP_NS_EXIF, "Flash", kXMP_NS_EXIF, "RedEyeMode", (redEye ? kXMP_TrueStr : kXMP_FalseStr) );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_Flash

// =================================================================================================
// ImportTIFF_OECFTable
// ====================
//
// Although the XMP for the OECF and SFR tables is the same, the Exif is not. The OECF table has
// signed rational values and the SFR table has unsigned.

static void
ImportTIFF_OECFTable ( const TIFF_Manager::TagInfo & tagInfo, bool nativeEndian,
					   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		const XMP_Uns8 * bytePtr = (XMP_Uns8*)tagInfo.dataPtr;
		const XMP_Uns8 * byteEnd = bytePtr + tagInfo.dataLen;

		XMP_Uns16 columns = *((XMP_Uns16*)bytePtr);
		XMP_Uns16 rows    = *((XMP_Uns16*)(bytePtr+2));
		if ( ! nativeEndian ) {
			columns = Flip2 ( columns );
			rows = Flip2 ( rows );
		}

		char buffer[40];

		snprintf ( buffer, sizeof(buffer), "%d", columns );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Columns", buffer );
		snprintf ( buffer, sizeof(buffer), "%d", rows );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Rows", buffer );

		std::string arrayPath;

		SXMPUtils::ComposeStructFieldPath ( xmpNS, xmpProp, kXMP_NS_EXIF, "Names", &arrayPath );

		bytePtr += 4;	// Move to the list of names.
		for ( size_t i = columns; i > 0; --i ) {
			size_t nameLen = strlen((XMP_StringPtr)bytePtr) + 1;	// ! Include the terminating nul.
			if ( (bytePtr + nameLen) > byteEnd ) { xmp->DeleteProperty ( xmpNS, xmpProp ); return; };
			xmp->AppendArrayItem ( xmpNS, arrayPath.c_str(), kXMP_PropArrayIsOrdered, (XMP_StringPtr)bytePtr );
			bytePtr += nameLen;
		}

		if ( (byteEnd - bytePtr) != (8 * columns * rows) ) { xmp->DeleteProperty ( xmpNS, xmpProp ); return; };	// Make sure the values are present.
		SXMPUtils::ComposeStructFieldPath ( xmpNS, xmpProp, kXMP_NS_EXIF, "Values", &arrayPath );

		XMP_Int32 * binPtr = (XMP_Int32*)bytePtr;
		for ( size_t i = (columns * rows); i > 0; --i, binPtr += 2 ) {

			XMP_Int32 binNum   = binPtr[0];
			XMP_Int32 binDenom = binPtr[1];
			if ( ! nativeEndian ) {
				Flip4 ( &binNum );
				Flip4 ( &binDenom );
			}

			snprintf ( buffer, sizeof(buffer), "%ld/%ld", (long)binNum, (long)binDenom );	// AUDIT: Use of sizeof(buffer) is safe.

			xmp->AppendArrayItem ( xmpNS, arrayPath.c_str(), kXMP_PropArrayIsOrdered, buffer );

		}

		return;

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_OECFTable

// =================================================================================================
// ImportTIFF_SFRTable
// ===================
//
// Although the XMP for the OECF and SFR tables is the same, the Exif is not. The OECF table has
// signed rational values and the SFR table has unsigned.

static void
ImportTIFF_SFRTable ( const TIFF_Manager::TagInfo & tagInfo, bool nativeEndian,
					  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		const XMP_Uns8 * bytePtr = (XMP_Uns8*)tagInfo.dataPtr;
		const XMP_Uns8 * byteEnd = bytePtr + tagInfo.dataLen;

		XMP_Uns16 columns = *((XMP_Uns16*)bytePtr);
		XMP_Uns16 rows    = *((XMP_Uns16*)(bytePtr+2));
		if ( ! nativeEndian ) {
			columns = Flip2 ( columns );
			rows = Flip2 ( rows );
		}

		char buffer[40];

		snprintf ( buffer, sizeof(buffer), "%d", columns );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Columns", buffer );
		snprintf ( buffer, sizeof(buffer), "%d", rows );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Rows", buffer );

		std::string arrayPath;

		SXMPUtils::ComposeStructFieldPath ( xmpNS, xmpProp, kXMP_NS_EXIF, "Names", &arrayPath );

		bytePtr += 4;	// Move to the list of names.
		for ( size_t i = columns; i > 0; --i ) {
			size_t nameLen = strlen((XMP_StringPtr)bytePtr) + 1;	// ! Include the terminating nul.
			if ( (bytePtr + nameLen) > byteEnd ) { xmp->DeleteProperty ( xmpNS, xmpProp ); return; };
			xmp->AppendArrayItem ( xmpNS, arrayPath.c_str(), kXMP_PropArrayIsOrdered, (XMP_StringPtr)bytePtr );
			bytePtr += nameLen;
		}

		if ( (byteEnd - bytePtr) != (8 * columns * rows) ) { xmp->DeleteProperty ( xmpNS, xmpProp ); return; };	// Make sure the values are present.
		SXMPUtils::ComposeStructFieldPath ( xmpNS, xmpProp, kXMP_NS_EXIF, "Values", &arrayPath );

		XMP_Uns32 * binPtr = (XMP_Uns32*)bytePtr;
		for ( size_t i = (columns * rows); i > 0; --i, binPtr += 2 ) {

			XMP_Uns32 binNum   = binPtr[0];
			XMP_Uns32 binDenom = binPtr[1];
			if ( ! nativeEndian ) {
				binNum   = Flip4 ( binNum );
				binDenom = Flip4 ( binDenom );
			}

			snprintf ( buffer, sizeof(buffer), "%lu/%lu", (unsigned long)binNum, (unsigned long)binDenom );	// AUDIT: Use of sizeof(buffer) is safe.

			xmp->AppendArrayItem ( xmpNS, arrayPath.c_str(), kXMP_PropArrayIsOrdered, buffer );

		}

		return;

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_SFRTable

// =================================================================================================
// ImportTIFF_CFATable
// ===================

static void
ImportTIFF_CFATable ( const TIFF_Manager::TagInfo & tagInfo, bool nativeEndian,
					  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		const XMP_Uns8 * bytePtr = (XMP_Uns8*)tagInfo.dataPtr;
		const XMP_Uns8 * byteEnd = bytePtr + tagInfo.dataLen;

		XMP_Uns16 columns = *((XMP_Uns16*)bytePtr);
		XMP_Uns16 rows    = *((XMP_Uns16*)(bytePtr+2));
		if ( ! nativeEndian ) {
			columns = Flip2 ( columns );
			rows = Flip2 ( rows );
		}

		char buffer[20];
		std::string arrayPath;

		snprintf ( buffer, sizeof(buffer), "%d", columns );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Columns", buffer );
		snprintf ( buffer, sizeof(buffer), "%d", rows );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Rows", buffer );

		bytePtr += 4;	// Move to the matrix of values.
		if ( (byteEnd - bytePtr) != (columns * rows) ) goto BadExif;	// Make sure the values are present.

		SXMPUtils::ComposeStructFieldPath ( xmpNS, xmpProp, kXMP_NS_EXIF, "Values", &arrayPath );

		for ( size_t i = (columns * rows); i > 0; --i, ++bytePtr ) {
			snprintf ( buffer, sizeof(buffer), "%hu", *bytePtr );	// AUDIT: Use of sizeof(buffer) is safe.
			xmp->AppendArrayItem ( xmpNS, arrayPath.c_str(), kXMP_PropArrayIsOrdered, buffer );
		}

		return;

	BadExif:	// Ignore the tag if the table is ill-formed.
		xmp->DeleteProperty ( xmpNS, xmpProp );
		return;

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_CFATable

// =================================================================================================
// ImportTIFF_DSDTable
// ===================

static void
ImportTIFF_DSDTable ( const TIFF_Manager & tiff, const TIFF_Manager::TagInfo & tagInfo,
					  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		const XMP_Uns8 * bytePtr = (XMP_Uns8*)tagInfo.dataPtr;
		const XMP_Uns8 * byteEnd = bytePtr + tagInfo.dataLen;

		XMP_Uns16 columns = *((XMP_Uns16*)bytePtr);
		XMP_Uns16 rows    = *((XMP_Uns16*)(bytePtr+2));
		if ( ! tiff.IsNativeEndian() ) {
			columns = Flip2 ( columns );
			rows = Flip2 ( rows );
		}

		char buffer[20];

		snprintf ( buffer, sizeof(buffer), "%d", columns );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Columns", buffer );
		snprintf ( buffer, sizeof(buffer), "%d", rows );	// AUDIT: Use of sizeof(buffer) is safe.
		xmp->SetStructField ( xmpNS, xmpProp, kXMP_NS_EXIF, "Rows", buffer );

		std::string arrayPath;
		SXMPUtils::ComposeStructFieldPath ( xmpNS, xmpProp, kXMP_NS_EXIF, "Settings", &arrayPath );

		bytePtr += 4;	// Move to the list of settings.
		UTF16Unit * utf16Ptr = (UTF16Unit*)bytePtr;
		UTF16Unit * utf16End = (UTF16Unit*)byteEnd;

		std::string utf8;

		// Figure 17 in the Exif 2.2 spec is unclear. It has counts for rows and columns, but the
		// settings are listed as 1..n, not as a rectangular matrix. So, ignore the counts and copy
		// strings until the end of the Exif value.

		while ( utf16Ptr < utf16End ) {

			size_t nameLen = 0;
			while ( utf16Ptr[nameLen] != 0 ) ++nameLen;
			++nameLen;	// ! Include the terminating nul.
			if ( (utf16Ptr + nameLen) > utf16End ) goto BadExif;

			try {
				FromUTF16 ( utf16Ptr, nameLen, &utf8, tiff.IsBigEndian() );
			} catch ( ... ) {
				goto BadExif; // Ignore the tag if there are conversion errors.
			}

			xmp->AppendArrayItem ( xmpNS, arrayPath.c_str(), kXMP_PropArrayIsOrdered, utf8.c_str() );

			utf16Ptr += nameLen;

		}

		return;

	BadExif:	// Ignore the tag if the table is ill-formed.
		xmp->DeleteProperty ( xmpNS, xmpProp );
		return;

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_DSDTable

// =================================================================================================
// ImportTIFF_GPSCoordinate
// ========================

static void
ImportTIFF_GPSCoordinate ( const TIFF_Manager & tiff, const TIFF_Manager::TagInfo & posInfo,
						   SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		const bool nativeEndian = tiff.IsNativeEndian();

		XMP_Uns16 refID = posInfo.id - 1;	// ! The GPS refs and coordinates are all tag n and n+1.
		TIFF_Manager::TagInfo refInfo;
		bool found = tiff.GetTag ( kTIFF_GPSInfoIFD, refID, &refInfo );
		if ( (! found) || (refInfo.type != kTIFF_ASCIIType) || (refInfo.count != 2) ) return;
		char ref = *((char*)refInfo.dataPtr);

		XMP_Uns32 * binPtr = (XMP_Uns32*)posInfo.dataPtr;
		XMP_Uns32 degNum   = binPtr[0];
		XMP_Uns32 degDenom = binPtr[1];
		XMP_Uns32 minNum   = binPtr[2];
		XMP_Uns32 minDenom = binPtr[3];
		XMP_Uns32 secNum   = binPtr[4];
		XMP_Uns32 secDenom = binPtr[5];
		if ( ! nativeEndian ) {
			degNum = Flip4 ( degNum );
			degDenom = Flip4 ( degDenom );
			minNum = Flip4 ( minNum );
			minDenom = Flip4 ( minDenom );
			secNum = Flip4 ( secNum );
			secDenom = Flip4 ( secDenom );
		}

		char buffer[40];

		if ( (degDenom == 1) && (minDenom == 1) && (secDenom == 1) ) {

			snprintf ( buffer, sizeof(buffer), "%lu,%lu,%lu%c", (unsigned long)degNum, (unsigned long)minNum, (unsigned long)secNum, ref );	// AUDIT: Using sizeof(buffer is safe.

		} else {

			XMP_Uns32 maxDenom = degDenom;
			if ( minDenom > degDenom ) maxDenom = minDenom;
			if ( secDenom > degDenom ) maxDenom = secDenom;

			int fracDigits = 1;
			while ( maxDenom > 10 ) { ++fracDigits; maxDenom = maxDenom/10; }

			double temp    = (double)degNum / (double)degDenom;
			double degrees = (double)((XMP_Uns32)temp);	// Just the integral number of degrees.
			double minutes = ((temp - degrees) * 60.0) +
							 ((double)minNum / (double)minDenom) +
							 (((double)secNum / (double)secDenom) / 60.0);

			snprintf ( buffer, sizeof(buffer), "%.0f,%.*f%c", degrees, fracDigits, minutes, ref );	// AUDIT: Using sizeof(buffer is safe.

		}

		xmp->SetProperty ( xmpNS, xmpProp, buffer );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_GPSCoordinate

// =================================================================================================
// ImportTIFF_GPSTimeStamp
// =======================

static void
ImportTIFF_GPSTimeStamp ( const TIFF_Manager & tiff, const TIFF_Manager::TagInfo & timeInfo,
						  SXMPMeta * xmp, const char * xmpNS, const char * xmpProp )
{
	try {	// Don't let errors with one stop the others.

		const bool nativeEndian = tiff.IsNativeEndian();

		bool haveDate;
		TIFF_Manager::TagInfo dateInfo;
		haveDate = tiff.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSDateStamp, &dateInfo );
		if ( ! haveDate ) haveDate = tiff.GetTag ( kTIFF_ExifIFD, kTIFF_DateTimeOriginal, &dateInfo );
		if ( ! haveDate ) haveDate = tiff.GetTag ( kTIFF_ExifIFD, kTIFF_DateTimeDigitized, &dateInfo );
		if ( ! haveDate ) return;

		const char * dateStr = (const char *) dateInfo.dataPtr;
		if ( (dateStr[4] != ':') || (dateStr[7] != ':') ) return;
		if ( (dateStr[10] != 0)  && (dateStr[10] != ' ') ) return;

		XMP_Uns32 * binPtr = (XMP_Uns32*)timeInfo.dataPtr;
		XMP_Uns32 hourNum   = binPtr[0];
		XMP_Uns32 hourDenom = binPtr[1];
		XMP_Uns32 minNum    = binPtr[2];
		XMP_Uns32 minDenom  = binPtr[3];
		XMP_Uns32 secNum    = binPtr[4];
		XMP_Uns32 secDenom  = binPtr[5];
		if ( ! nativeEndian ) {
			hourNum = Flip4 ( hourNum );
			hourDenom = Flip4 ( hourDenom );
			minNum = Flip4 ( minNum );
			minDenom = Flip4 ( minDenom );
			secNum = Flip4 ( secNum );
			secDenom = Flip4 ( secDenom );
		}

		double fHour, fMin, fSec, fNano, temp;
		fSec  =  (double)secNum / (double)secDenom;
		temp  =  (double)minNum / (double)minDenom;
		fMin  =  (double)((XMP_Uns32)temp);
		fSec  += (temp - fMin) * 60.0;
		temp  =  (double)hourNum / (double)hourDenom;
		fHour =  (double)((XMP_Uns32)temp);
		fSec  += (temp - fHour) * 3600.0;
		temp  =  (double)((XMP_Uns32)fSec);
		fNano =  ((fSec - temp) * (1000.0*1000.0*1000.0)) + 0.5;	// Try to avoid n999... problems.
		fSec  =  temp;

		XMP_DateTime binStamp;
		binStamp.year   = GatherInt ( dateStr, 4 );
		binStamp.month  = GatherInt ( dateStr+5, 2 );
		binStamp.day    = GatherInt ( dateStr+8, 2 );
		binStamp.hour   = (XMP_Int32)fHour;
		binStamp.minute = (XMP_Int32)fMin;
		binStamp.second = (XMP_Int32)fSec;
		binStamp.nanoSecond  = (XMP_Int32)fNano;
		binStamp.hasTimeZone = true;	// Exif GPS TimeStamp is implicitly UTC.
		binStamp.tzSign = kXMP_TimeIsUTC;
		binStamp.tzHour = binStamp.tzMinute = 0;

		xmp->SetProperty_Date ( xmpNS, xmpProp, binStamp );

	} catch ( ... ) {
		// Do nothing, let other imports proceed.
		// ? Notify client?
	}

}	// ImportTIFF_GPSTimeStamp

// =================================================================================================
// =================================================================================================

// =================================================================================================
// PhotoDataUtils::Import2WayExif
// ==============================
//
// Import the TIFF/Exif tags that have 2 way mappings to XMP, i.e. no correspondence to IPTC.
// These are always imported for the tiff: and exif: namespaces, but not for others.

void
PhotoDataUtils::Import2WayExif ( const TIFF_Manager & exif, SXMPMeta * xmp, int iptcDigestState )
{
	const bool nativeEndian = exif.IsNativeEndian();

	bool found, foundFromXMP;
	TIFF_Manager::TagInfo tagInfo;

	ImportTIFF_StandardMappings ( kTIFF_PrimaryIFD, exif, xmp );
	ImportTIFF_StandardMappings ( kTIFF_ExifIFD, exif, xmp );
	ImportTIFF_StandardMappings ( kTIFF_GPSInfoIFD, exif, xmp );
	
	// -----------------------------------------------------------------
	// Fixup erroneous files that have a negative value for GPSAltitude.

	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSAltitude, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_RationalType) &&  (tagInfo.count == 1) ) {

		XMP_Uns32 num = exif.GetUns32 ( tagInfo.dataPtr );
		XMP_Uns32 denom = exif.GetUns32 ( (XMP_Uns8*)tagInfo.dataPtr + 4 );
		bool numNeg = num >> 31;
		bool denomNeg = denom >> 31;

		if ( (numNeg != denomNeg) || numNeg ) {	// Does the GPSAltitude look negative?
			if ( denomNeg ) {
				denom = -denom;
				num = -num;
				numNeg = num >> 31;
			}
			if ( numNeg ) {
				char buffer [32];
				num = -num;
				snprintf ( buffer, sizeof(buffer), "%lu/%lu", (unsigned long) num, (unsigned long) denom );	// AUDIT: Using sizeof(buffer) is safe.
				xmp->SetProperty ( kXMP_NS_EXIF, "GPSAltitude", buffer );
				xmp->SetProperty ( kXMP_NS_EXIF, "GPSAltitudeRef", "1" );
			}
		}

	}
	
	// ---------------------------------------------------------------
	// Import DateTimeOriginal and DateTime if the XMP doss not exist.

	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_DateTimeOriginal, &tagInfo );
	foundFromXMP = xmp->DoesPropertyExist ( kXMP_NS_EXIF, "DateTimeOriginal" );
	
	if ( found && (! foundFromXMP) && (tagInfo.type == kTIFF_ASCIIType) ) {
		ImportTIFF_Date ( exif, tagInfo, xmp, kXMP_NS_EXIF, "DateTimeOriginal" );
	}

	found = exif.GetTag ( kTIFF_PrimaryIFD, kTIFF_DateTime, &tagInfo );
	foundFromXMP = xmp->DoesPropertyExist ( kXMP_NS_XMP, "ModifyDate" );
	
	if ( found && (! foundFromXMP) && (tagInfo.type == kTIFF_ASCIIType) ) {
		ImportTIFF_Date ( exif, tagInfo, xmp, kXMP_NS_XMP, "ModifyDate" );
	}

	// ----------------------------------------------------
	// Import the Exif IFD tags that have special mappings.
	
	// 34855 ISOSpeedRatings has special cases for ISO over 65535. The tag is SHORT, some cameras
	// omit the tag and some write 65535, all put the real ISO in MakerNote - which ACR might
	// extract and leave in the XMP. There are 3 import cases:
	//	1. No native tag: Leave existing XMP.
	//	2. All of the native values are under 65535: Clear any XMP and import.
	//	3. One or more of the native values are 65535: Leave existing XMP, else import.
	
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_ISOSpeedRatings, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_ShortType) && (tagInfo.count > 0) ) {

		bool keepXMP = false;
		XMP_Uns16 * itemPtr = (XMP_Uns16*) tagInfo.dataPtr;
		for ( XMP_Uns32 i = 0; i < tagInfo.count; ++i, ++itemPtr ) {
			if ( *itemPtr == 0xFFFF ) { keepXMP = true; break; }	// ! Don't care about BE or LF, same either way.
		}

		if ( ! keepXMP ) xmp->DeleteProperty ( kXMP_NS_EXIF, "ISOSpeedRatings" );
		
		if ( ! xmp->DoesPropertyExist ( kXMP_NS_EXIF, "ISOSpeedRatings" ) ) {
			ImportArrayTIFF ( tagInfo, exif.IsNativeEndian(), xmp, kXMP_NS_EXIF, "ISOSpeedRatings" );
		}

	}

	// 36864 ExifVersion is 4 "undefined" ASCII characters.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_ExifVersion, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_UndefinedType) && (tagInfo.count == 4) ) {
		char str[5];
		*((XMP_Uns32*)str) = *((XMP_Uns32*)tagInfo.dataPtr);
		str[4] = 0;
		xmp->SetProperty ( kXMP_NS_EXIF, "ExifVersion", str );
	}

	// 40960 FlashpixVersion is 4 "undefined" ASCII characters.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_FlashpixVersion, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_UndefinedType) && (tagInfo.count == 4) ) {
		char str[5];
		*((XMP_Uns32*)str) = *((XMP_Uns32*)tagInfo.dataPtr);
		str[4] = 0;
		xmp->SetProperty ( kXMP_NS_EXIF, "FlashpixVersion", str );
	}

	// 37121 ComponentsConfiguration is an array of 4 "undefined" UInt8 bytes.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_ComponentsConfiguration, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_UndefinedType) && (tagInfo.count == 4) ) {
		ImportArrayTIFF_Byte ( tagInfo, xmp, kXMP_NS_EXIF, "ComponentsConfiguration" );
	}

	// 37510 UserComment is a string with explicit encoding.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_UserComment, &tagInfo );
	if ( found ) {
		ImportTIFF_EncodedString ( exif, tagInfo, xmp, kXMP_NS_EXIF, "UserComment", true /* isLangAlt */ );
	}

	// 34856 OECF is an OECF/SFR table.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_OECF, &tagInfo );
	if ( found ) {
		ImportTIFF_OECFTable ( tagInfo, nativeEndian, xmp, kXMP_NS_EXIF, "OECF" );
	}

	// 37385 Flash is a UInt16 collection of bit fields and is mapped to a struct in XMP.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_Flash, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_ShortType) && (tagInfo.count == 1) ) {
		ImportTIFF_Flash ( tagInfo, nativeEndian, xmp, kXMP_NS_EXIF, "Flash" );
	}

	// 41484 SpatialFrequencyResponse is an OECF/SFR table.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_SpatialFrequencyResponse, &tagInfo );
	if ( found ) {
		ImportTIFF_SFRTable ( tagInfo, nativeEndian, xmp, kXMP_NS_EXIF, "SpatialFrequencyResponse" );
	}

	// 41728 FileSource is an "undefined" UInt8.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_FileSource, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_UndefinedType) && (tagInfo.count == 1) ) {
		ImportSingleTIFF_Byte ( tagInfo, xmp, kXMP_NS_EXIF, "FileSource" );
	}

	// 41729 SceneType is an "undefined" UInt8.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_SceneType, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_UndefinedType) && (tagInfo.count == 1) ) {
		ImportSingleTIFF_Byte ( tagInfo, xmp, kXMP_NS_EXIF, "SceneType" );
	}

	// 41730 CFAPattern is a custom table.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_CFAPattern, &tagInfo );
	if ( found ) {
		ImportTIFF_CFATable ( tagInfo, nativeEndian, xmp, kXMP_NS_EXIF, "CFAPattern" );
	}

	// 41995 DeviceSettingDescription is a custom table.
	found = exif.GetTag ( kTIFF_ExifIFD, kTIFF_DeviceSettingDescription, &tagInfo );
	if ( found ) {
		ImportTIFF_DSDTable ( exif, tagInfo, xmp, kXMP_NS_EXIF, "DeviceSettingDescription" );
	}

	// --------------------------------------------------------
	// Import the GPS Info IFD tags that have special mappings.

	// 0 GPSVersionID is 4 UInt8 bytes and mapped as "n.n.n.n".
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSVersionID, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_ByteType) && (tagInfo.count == 4) ) {
		const char * strIn = (const char *) tagInfo.dataPtr;
		char strOut[8];
		strOut[0] = strIn[0];
		strOut[2] = strIn[1];
		strOut[4] = strIn[2];
		strOut[6] = strIn[3];
		strOut[1] = strOut[3] = strOut[5] = '.';
		strOut[7] = 0;
		xmp->SetProperty ( kXMP_NS_EXIF, "GPSVersionID", strOut );
	}

	// 2 GPSLatitude is a GPS coordinate master.
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSLatitude, &tagInfo );
	if ( found ) {
		ImportTIFF_GPSCoordinate ( exif, tagInfo, xmp, kXMP_NS_EXIF, "GPSLatitude" );
	}

	// 4 GPSLongitude is a GPS coordinate master.
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSLongitude, &tagInfo );
	if ( found ) {
		ImportTIFF_GPSCoordinate ( exif, tagInfo, xmp, kXMP_NS_EXIF, "GPSLongitude" );
	}

	// 7 GPSTimeStamp is a UTC time as 3 rationals, mated with the optional GPSDateStamp.
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSTimeStamp, &tagInfo );
	if ( found && (tagInfo.type == kTIFF_RationalType) && (tagInfo.count == 3) ) {
		ImportTIFF_GPSTimeStamp ( exif, tagInfo, xmp, kXMP_NS_EXIF, "GPSTimeStamp" );
	}

	// 20 GPSDestLatitude is a GPS coordinate master.
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSDestLatitude, &tagInfo );
	if ( found ) {
		ImportTIFF_GPSCoordinate ( exif, tagInfo, xmp, kXMP_NS_EXIF, "GPSDestLatitude" );
	}

	// 22 GPSDestLongitude is a GPS coordinate master.
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSDestLongitude, &tagInfo );
	if ( found ) {
		ImportTIFF_GPSCoordinate ( exif, tagInfo, xmp, kXMP_NS_EXIF, "GPSDestLongitude" );
	}

	// 27 GPSProcessingMethod is a string with explicit encoding.
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSProcessingMethod, &tagInfo );
	if ( found ) {
		ImportTIFF_EncodedString ( exif, tagInfo, xmp, kXMP_NS_EXIF, "GPSProcessingMethod" );
	}

	// 28 GPSAreaInformation is a string with explicit encoding.
	found = exif.GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSAreaInformation, &tagInfo );
	if ( found ) {
		ImportTIFF_EncodedString ( exif, tagInfo, xmp, kXMP_NS_EXIF, "GPSAreaInformation" );
	}

}	// PhotoDataUtils::Import2WayExif

// =================================================================================================
// Import3WayDateTime
// ==================

static void Import3WayDateTime ( XMP_Uns16 exifTag, const TIFF_Manager & exif, const IPTC_Manager & iptc,
								 SXMPMeta * xmp, int iptcDigestState, const IPTC_Manager & oldIPTC )
{
	XMP_Uns8  iptcDS;
	XMP_StringPtr xmpNS, xmpProp;
	
	if ( exifTag == kTIFF_DateTimeOriginal ) {
		iptcDS  = kIPTC_DateCreated;
		xmpNS   = kXMP_NS_Photoshop;
		xmpProp = "DateCreated";
	} else if ( exifTag == kTIFF_DateTimeDigitized ) {
		iptcDS  = kIPTC_DigitalCreateDate;
		xmpNS   = kXMP_NS_XMP;
		xmpProp = "CreateDate";
	} else {
		XMP_Throw ( "Unrecognized dateID", kXMPErr_BadParam );
	}

	size_t iptcCount;
	bool haveXMP, haveExif, haveIPTC;	// ! These are manipulated to simplify MWG-compliant logic.
	std::string xmpValue, exifValue, iptcValue;
	TIFF_Manager::TagInfo exifInfo;
	IPTC_Manager::DataSetInfo iptcInfo;

	// Get the basic info about available values.
	haveXMP   = xmp->GetProperty ( xmpNS, xmpProp, &xmpValue, 0 );
	iptcCount = PhotoDataUtils::GetNativeInfo ( iptc, iptcDS, iptcDigestState, haveXMP, &iptcInfo );
	haveIPTC = (iptcCount > 0);
	XMP_Assert ( (iptcDigestState == kDigestMatches) ? (! haveIPTC) : true );
	haveExif  = (! haveXMP) && (! haveIPTC) && PhotoDataUtils::GetNativeInfo ( exif, kTIFF_ExifIFD, exifTag, &exifInfo );
	XMP_Assert ( (! (haveExif & haveXMP)) & (! (haveExif & haveIPTC)) );

	if ( haveIPTC ) {

		PhotoDataUtils::ImportIPTC_Date ( iptcDS, iptc, xmp );

	} else if ( haveExif && (exifInfo.type == kTIFF_ASCIIType) ) {

		// Only import the Exif form if the non-TZ information differs from the XMP.
	
		TIFF_FileWriter exifFromXMP;
		TIFF_Manager::TagInfo infoFromXMP;

		ExportTIFF_Date ( *xmp, xmpNS, xmpProp, &exifFromXMP, exifTag );
		bool foundFromXMP = exifFromXMP.GetTag ( kTIFF_ExifIFD, exifTag, &infoFromXMP );

		if ( (! foundFromXMP) || (exifInfo.dataLen != infoFromXMP.dataLen) ||
			 (! XMP_LitNMatch ( (char*)exifInfo.dataPtr, (char*)infoFromXMP.dataPtr, exifInfo.dataLen )) ) {
			ImportTIFF_Date ( exif, exifInfo, xmp, xmpNS, xmpProp );
		}

	}

}	// Import3WayDateTime

// =================================================================================================
// PhotoDataUtils::Import3WayItems
// ===============================
//
// Handle the imports that involve all 3 of Exif, IPTC, and XMP. There are only 4 properties with
// 3-way mappings, copyright, description, creator, and date/time. Following the MWG guidelines,
// this general policy is applied separately to each:
//
//   If the new IPTC digest differs from the stored digest (favor IPTC over Exif and XMP)
//      If the IPTC value differs from the predicted old IPTC value
//         Import the IPTC value, including deleting the XMP
//      Else if the Exif is non-empty and differs from the XMP
//         Import the Exif value (does not delete existing XMP)
//   Else if the stored IPTC digest is missing (favor Exif over IPTC, or IPTC over missing XMP)
//      If the Exif is non-empty and differs from the XMP
//         Import the Exif value (does not delete existing XMP)
//      Else if the XMP is missing and the Exif is missing or empty
//         Import the IPTC value
//   Else (the new IPTC digest matches the stored digest - ignore the IPTC)
//      If the Exif is non-empty and differs from the XMP
//         Import the Exif value (does not delete existing XMP)
//
// Note that missing or empty Exif will never cause existing XMP to be deleted. This is a pragmatic
// choice to improve compatibility with pre-MWG software. There are few Exif-only editors for these
// 3-way properties, there are important existing IPTC-only editors.

// -------------------------------------------------------------------------------------------------

void PhotoDataUtils::Import3WayItems ( const TIFF_Manager & exif, const IPTC_Manager & iptc, SXMPMeta * xmp, int iptcDigestState )
{
	size_t iptcCount;

	bool haveXMP, haveExif, haveIPTC;	// ! These are manipulated to simplify MWG-compliant logic.
	std::string xmpValue, exifValue, iptcValue;

	TIFF_Manager::TagInfo exifInfo;
	IPTC_Manager::DataSetInfo iptcInfo;

	IPTC_Writer oldIPTC;
	if ( iptcDigestState == kDigestDiffers ) {
		PhotoDataUtils::ExportIPTC ( *xmp, &oldIPTC );	// Predict old IPTC DataSets based on the existing XMP.
	}
	
	// ---------------------------------------------------------------------------------
	// Process the copyright. Replace internal nuls in the Exif to "merge" the portions.
	
	// Get the basic info about available values.
	haveXMP   = xmp->GetLocalizedText ( kXMP_NS_DC, "rights", "", "x-default", 0, &xmpValue, 0 );
	iptcCount = PhotoDataUtils::GetNativeInfo ( iptc, kIPTC_CopyrightNotice, iptcDigestState, haveXMP, &iptcInfo );
	haveIPTC  = (iptcCount > 0);
	XMP_Assert ( (iptcDigestState == kDigestMatches) ? (! haveIPTC) : true );
	haveExif  = (! haveXMP) && (! haveIPTC) && PhotoDataUtils::GetNativeInfo ( exif, kTIFF_PrimaryIFD, kTIFF_Copyright, &exifInfo );
	XMP_Assert ( (! (haveExif & haveXMP)) & (! (haveExif & haveIPTC)) );

	if ( haveExif && (exifInfo.dataLen > 1) ) {	// Replace internal nul characters with linefeed.
		for ( XMP_Uns32 i = 0; i < exifInfo.dataLen-1; ++i ) {
			if ( ((char*)exifInfo.dataPtr)[i] == 0 ) ((char*)exifInfo.dataPtr)[i] = 0x0A;
		}
	}
	
	if ( haveIPTC ) {
		PhotoDataUtils::ImportIPTC_LangAlt ( iptc, xmp, kIPTC_CopyrightNotice, kXMP_NS_DC, "rights" );
	} else if ( haveExif && PhotoDataUtils::IsValueDifferent ( exifInfo, xmpValue, &exifValue ) ) {
		xmp->SetLocalizedText ( kXMP_NS_DC, "rights", "", "x-default", exifValue.c_str() );
	}
	
	// ------------------------
	// Process the description.
	
	// Get the basic info about available values.
	haveXMP   = xmp->GetLocalizedText ( kXMP_NS_DC, "description", "", "x-default", 0, &xmpValue, 0 );
	iptcCount = PhotoDataUtils::GetNativeInfo ( iptc, kIPTC_Description, iptcDigestState, haveXMP, &iptcInfo );
	haveIPTC  = (iptcCount > 0);
	XMP_Assert ( (iptcDigestState == kDigestMatches) ? (! haveIPTC) : true );
	haveExif  = (! haveXMP) && (! haveIPTC) && PhotoDataUtils::GetNativeInfo ( exif, kTIFF_PrimaryIFD, kTIFF_ImageDescription, &exifInfo );
	XMP_Assert ( (! (haveExif & haveXMP)) & (! (haveExif & haveIPTC)) );
	
	if ( haveIPTC ) {
		PhotoDataUtils::ImportIPTC_LangAlt ( iptc, xmp, kIPTC_Description, kXMP_NS_DC, "description" );
	} else if ( haveExif && PhotoDataUtils::IsValueDifferent ( exifInfo, xmpValue, &exifValue ) ) {
		xmp->SetLocalizedText ( kXMP_NS_DC, "description", "", "x-default", exifValue.c_str() );
	}

	// -------------------------------------------------------------------------------------------
	// Process the creator. The XMP and IPTC are arrays, the Exif is a semicolon separated string.
	
	// Get the basic info about available values.
	haveXMP   = xmp->DoesPropertyExist ( kXMP_NS_DC, "creator" );
	haveExif  = PhotoDataUtils::GetNativeInfo ( exif, kTIFF_PrimaryIFD, kTIFF_Artist, &exifInfo );
	iptcCount = PhotoDataUtils::GetNativeInfo ( iptc, kIPTC_Creator, iptcDigestState, haveXMP, &iptcInfo );
	haveIPTC  = (iptcCount > 0);
	XMP_Assert ( (iptcDigestState == kDigestMatches) ? (! haveIPTC) : true );
	haveExif  = (! haveXMP) && (! haveIPTC) && PhotoDataUtils::GetNativeInfo ( exif, kTIFF_PrimaryIFD, kTIFF_Artist, &exifInfo );
	XMP_Assert ( (! (haveExif & haveXMP)) & (! (haveExif & haveIPTC)) );
	
	if ( haveIPTC ) {
		PhotoDataUtils::ImportIPTC_Array ( iptc, xmp, kIPTC_Creator, kXMP_NS_DC, "creator" );
	} else if ( haveExif && PhotoDataUtils::IsValueDifferent ( exifInfo, xmpValue, &exifValue ) ) {
		SXMPUtils::SeparateArrayItems ( xmp, kXMP_NS_DC, "creator", kXMP_PropArrayIsOrdered, exifValue );
	}
	
	// ------------------------------------------------------------------------------
	// Process DateTimeDigitized; DateTimeOriginal and DateTime are 2-way.
	// ***   Exif DateTimeOriginal <-> XMP exif:DateTimeOriginal
	// ***   IPTC DateCreated <-> XMP photoshop:DateCreated
	// ***   Exif DateTimeDigitized <-> IPTC DigitalCreateDate <-> XMP xmp:CreateDate
	// ***   TIFF DateTime <-> XMP xmp:ModifyDate

	Import3WayDateTime ( kTIFF_DateTimeDigitized, exif, iptc, xmp, iptcDigestState, oldIPTC );

}	// PhotoDataUtils::Import3WayItems

// =================================================================================================
// =================================================================================================

// =================================================================================================
// ExportSingleTIFF
// ================
//
// This is only called when the XMP exists and will be exported. And only for standard mappings.

// ! Only implemented for the types known to be needed.

static void
ExportSingleTIFF ( TIFF_Manager * tiff, XMP_Uns8 ifd, const TIFF_MappingToXMP & mapInfo,
				   bool nativeEndian, const std::string & xmpValue )
{
	XMP_Assert ( (mapInfo.count == 1) || (mapInfo.type == kTIFF_ASCIIType) );
	XMP_Assert ( mapInfo.name[0] != 0 );	// Must be a standard mapping.
	
	char nextChar;	// Used to make sure sscanf consumes all of the string.
	
	switch ( mapInfo.type ) {

		case kTIFF_ByteType : {
			unsigned short binValue;
			int items = sscanf ( xmpValue.c_str(), "%hu%c", &binValue, &nextChar );	// AUDIT: Using xmpValue.c_str() is safe.
			if ( items != 1 ) return;	// ? complain? notify client?
			tiff->SetTag_Byte ( ifd, mapInfo.id, (XMP_Uns8)binValue );
			break;
		}

		case kTIFF_ShortType : {
			unsigned long binValue;
			int items = sscanf ( xmpValue.c_str(), "%lu%c", &binValue, &nextChar );	// AUDIT: Using xmpValue.c_str() is safe.
			if ( items != 1 ) return;	// ? complain? notify client?
			tiff->SetTag_Short ( ifd, mapInfo.id, (XMP_Uns16)binValue );
			break;
		}

		case kTIFF_ShortOrLongType : {
			unsigned long binValue;
			int items = sscanf ( xmpValue.c_str(), "%lu%c", &binValue, &nextChar );	// AUDIT: Using xmpValue.c_str() is safe.
			if ( items != 1 ) return;	// ? complain? notify client?
			if ( binValue <= 0xFFFF ) {
				tiff->SetTag_Short ( ifd, mapInfo.id, (XMP_Uns16)binValue );
			} else {
				tiff->SetTag_Long ( ifd, mapInfo.id, (XMP_Uns32)binValue );
			}
			break;
		}

		case kTIFF_RationalType : {	// The XMP is formatted as "num/denom".
			unsigned long num, denom;
			int items = sscanf ( xmpValue.c_str(), "%lu/%lu%c", &num, &denom, &nextChar );	// AUDIT: Using xmpValue.c_str() is safe.
			if ( items != 2 ) {
				if ( items != 1 ) return;	// ? complain? notify client?
				denom = 1;	// The XMP was just an integer, assume a denominator of 1.
			}
			tiff->SetTag_Rational ( ifd, mapInfo.id, (XMP_Uns32)num, (XMP_Uns32)denom );
			break;
		}

		case kTIFF_SRationalType : {	// The XMP is formatted as "num/denom".
			signed long num, denom;
			int items = sscanf ( xmpValue.c_str(), "%ld/%ld%c", &num, &denom, &nextChar );	// AUDIT: Using xmpValue.c_str() is safe.
			if ( items != 2 ) {
				if ( items != 1 ) return;	// ? complain? notify client?
				denom = 1;	// The XMP was just an integer, assume a denominator of 1.
			}
			tiff->SetTag_SRational ( ifd, mapInfo.id, (XMP_Int32)num, (XMP_Int32)denom );
			break;
		}

		case kTIFF_ASCIIType :
			tiff->SetTag ( ifd, mapInfo.id, kTIFF_ASCIIType, (XMP_Uns32)(xmpValue.size()+1), xmpValue.c_str() );
			break;
			
		default:
			XMP_Assert ( false );	// Force a debug assert for unexpected types.

	}

}	// ExportSingleTIFF

// =================================================================================================
// ExportArrayTIFF
// ================
//
// This is only called when the XMP exists and will be exported. And only for standard mappings.

// ! Only implemented for the types known to be needed.

static void
ExportArrayTIFF ( TIFF_Manager * tiff, XMP_Uns8 ifd, const TIFF_MappingToXMP & mapInfo, bool nativeEndian,
				  const SXMPMeta & xmp, const char * xmpNS, const char * xmpArray )
{
	XMP_Assert ( (mapInfo.count != 1) && (mapInfo.type != kTIFF_ASCIIType) );
	XMP_Assert ( mapInfo.name[0] != 0 );	// Must be a standard mapping.
	XMP_Assert ( mapInfo.type == kTIFF_ShortType );	// ! So far ISOSpeedRatings is the only standard array export.
	XMP_Assert ( xmp.DoesPropertyExist ( xmpNS, xmpArray ) );
	
	if ( mapInfo.type != kTIFF_ShortType ) return;	// ! So far ISOSpeedRatings is the only standard array export.
	
	size_t arraySize = xmp.CountArrayItems ( xmpNS, xmpArray );
	if ( arraySize == 0 ) {
		tiff->DeleteTag ( ifd, mapInfo.id );
		return;
	}
	
	std::vector<XMP_Uns16> vecValue;
	vecValue.assign ( arraySize, 0 );
	XMP_Uns16 * binPtr = (XMP_Uns16*) &vecValue[0];
	
	std::string itemPath;
	XMP_Int32 int32;
	XMP_Uns16 uns16;
	for ( size_t i = 1; i <= arraySize; ++i, ++binPtr ) {
		SXMPUtils::ComposeArrayItemPath ( xmpNS, xmpArray, (XMP_Index)i, &itemPath );
		xmp.GetProperty_Int ( xmpNS, itemPath.c_str(), &int32, 0 );
		uns16 = (XMP_Uns16)int32;
		if ( ! nativeEndian ) uns16 = Flip2 ( uns16 );
		*binPtr = uns16;
	}

	tiff->SetTag ( ifd, mapInfo.id, kTIFF_ShortType, (XMP_Uns32)arraySize, &vecValue[0] );

}	// ExportArrayTIFF

// =================================================================================================
// ExportTIFF_StandardMappings
// ===========================

static void
ExportTIFF_StandardMappings ( XMP_Uns8 ifd, TIFF_Manager * tiff, const SXMPMeta & xmp )
{
	const bool nativeEndian = tiff->IsNativeEndian();
	TIFF_Manager::TagInfo tagInfo;
	std::string xmpValue;
	XMP_OptionBits xmpForm;

	const TIFF_MappingToXMP * mappings = 0;
	const char * xmpNS = 0;

	if ( ifd == kTIFF_PrimaryIFD ) {
		mappings = sPrimaryIFDMappings;
		xmpNS    = kXMP_NS_TIFF;
	} else if ( ifd == kTIFF_ExifIFD ) {
		mappings = sExifIFDMappings;
		xmpNS    = kXMP_NS_EXIF;
	} else if ( ifd == kTIFF_GPSInfoIFD ) {
		mappings = sGPSInfoIFDMappings;
		xmpNS    = kXMP_NS_EXIF;	// ! Yes, the GPS Info tags are in the exif: namespace.
	} else {
		XMP_Throw ( "Invalid IFD for standard mappings", kXMPErr_InternalFailure );
	}

	for ( size_t i = 0; mappings[i].id != 0xFFFF; ++i ) {

		try {	// Don't let errors with one stop the others.

			const TIFF_MappingToXMP & mapInfo =  mappings[i];

			if ( mapInfo.exportMode == kExport_Never ) continue;
			if ( mapInfo.name[0] == 0 ) continue;	// Skip special mappings, handled higher up.

			bool haveTIFF = tiff->GetTag ( ifd, mapInfo.id, &tagInfo );
			if ( haveTIFF && (mapInfo.exportMode == kExport_InjectOnly) ) continue;
			
			bool haveXMP  = xmp.GetProperty ( xmpNS, mapInfo.name, &xmpValue, &xmpForm );
			if ( ! haveXMP ) {
			
				if ( haveTIFF && (mapInfo.exportMode == kExport_Always) ) tiff->DeleteTag ( ifd, mapInfo.id );

			} else {
			
				XMP_Assert ( tagInfo.type != kTIFF_UndefinedType );	// These must have a special mapping.
				if ( tagInfo.type == kTIFF_UndefinedType ) continue;
	
				const bool mapSingle = ((mapInfo.count == 1) || (mapInfo.type == kTIFF_ASCIIType));
				if ( mapSingle ) {
					if ( ! XMP_PropIsSimple ( xmpForm ) ) continue;	// ? Notify client?
					ExportSingleTIFF ( tiff, ifd, mapInfo, nativeEndian, xmpValue );
				} else {
					if ( ! XMP_PropIsArray ( xmpForm ) ) continue;	// ? Notify client?
					ExportArrayTIFF ( tiff, ifd, mapInfo, nativeEndian, xmp, xmpNS, mapInfo.name );
				}
				
			}

		} catch ( ... ) {

			// Do nothing, let other imports proceed.
			// ? Notify client?

		}

	}

}	// ExportTIFF_StandardMappings

// =================================================================================================
// ExportTIFF_Date
// ===============
//
// Convert  an XMP date/time to an Exif 2.2 master date/time tag plus associated fractional seconds.
// The Exif date/time part is a 20 byte ASCII value formatted as "YYYY:MM:DD HH:MM:SS" with a
// terminating nul. The fractional seconds are a nul terminated ASCII string with possible space
// padding. They are literally the fractional part, the digits that would be to the right of the
// decimal point.

static void
ExportTIFF_Date ( const SXMPMeta & xmp, const char * xmpNS, const char * xmpProp, TIFF_Manager * tiff, XMP_Uns16 mainID )
{
	XMP_Uns8 mainIFD = kTIFF_ExifIFD;
	XMP_Uns16 fracID;
	switch ( mainID ) {
		case kTIFF_DateTime : mainIFD = kTIFF_PrimaryIFD; fracID = kTIFF_SubSecTime;	break;
		case kTIFF_DateTimeOriginal  : fracID = kTIFF_SubSecTimeOriginal;	break;
		case kTIFF_DateTimeDigitized : fracID = kTIFF_SubSecTimeDigitized;	break;
	}

	try {	// Don't let errors with one stop the others.

		std::string  xmpStr;
		bool foundXMP = xmp.GetProperty ( xmpNS, xmpProp, &xmpStr, 0 );
		if ( ! foundXMP ) {
			tiff->DeleteTag ( mainIFD, mainID );
			tiff->DeleteTag ( kTIFF_ExifIFD, fracID );	// ! The subseconds are always in the Exif IFD.
			return;
		}

		// Format using all of the numbers. Then overwrite blanks for missing fields. The fields
		// missing from the XMP are detected with length checks: YYYY-MM-DDThh:mm:ss
		//   < 18 - no seconds
		//   < 15 - no minutes
		//   < 12 - no hours
		//   <  9 - no day
		//   <  6 - no month
		//   <  1 - no year

		XMP_DateTime xmpBin;
		SXMPUtils::ConvertToDate ( xmpStr.c_str(), &xmpBin );
		
		char buffer[24];
		snprintf ( buffer, sizeof(buffer), "%04d:%02d:%02d %02d:%02d:%02d",	// AUDIT: Use of sizeof(buffer) is safe.
				   xmpBin.year, xmpBin.month, xmpBin.day, xmpBin.hour, xmpBin.minute, xmpBin.second );

		size_t xmpLen = xmpStr.size();
		if ( xmpLen < 18 ) {
			buffer[17] = buffer[18] = ' ';
			if ( xmpLen < 15 ) {
				buffer[14] = buffer[15] = ' ';
				if ( xmpLen < 12 ) {
					buffer[11] = buffer[12] = ' ';
					if ( xmpLen < 9 ) {
						buffer[8] = buffer[9] = ' ';
						if ( xmpLen < 6 ) {
							buffer[5] = buffer[6] = ' ';
							if ( xmpLen < 1 ) {
								buffer[0] = buffer[1] = buffer[2] = buffer[3] = ' ';
							}
						}
					}
				}
			}
		}
		
		tiff->SetTag_ASCII ( mainIFD, mainID, buffer );

		if ( xmpBin.nanoSecond == 0 ) {
		
			tiff->DeleteTag ( kTIFF_ExifIFD, fracID );
		
		} else {

			snprintf ( buffer, sizeof(buffer), "%09d", xmpBin.nanoSecond );	// AUDIT: Use of sizeof(buffer) is safe.
			for ( size_t i = strlen(buffer)-1; i > 0; --i ) {
				if ( buffer[i] != '0' ) break;
				buffer[i] = 0;	// Strip trailing zero digits.
			}

			tiff->SetTag_ASCII ( kTIFF_ExifIFD, fracID, buffer );	// ! The subseconds are always in the Exif IFD.

		}

	} catch ( ... ) {
		// Do nothing, let other exports proceed.
		// ? Notify client?
	}

}	// ExportTIFF_Date

// =================================================================================================
// ExportTIFF_ArrayASCII
// =====================
//
// Catenate all of the XMP array values into a string. Use a "; " separator for Artist, nul for others.

static void
ExportTIFF_ArrayASCII ( const SXMPMeta & xmp, const char * xmpNS, const char * xmpProp,
						TIFF_Manager * tiff, XMP_Uns8 ifd, XMP_Uns16 id )
{
	try {	// Don't let errors with one stop the others.

		std::string    itemValue, fullValue;
		XMP_OptionBits xmpFlags;

		bool foundXMP = xmp.GetProperty ( xmpNS, xmpProp, 0, &xmpFlags );
		if ( ! foundXMP ) {
			tiff->DeleteTag ( ifd, id );
			return;
		}

		if ( ! XMP_PropIsArray ( xmpFlags ) ) return;	// ? Complain? Delete the tag?

		if ( id == kTIFF_Artist ) {
			SXMPUtils::CatenateArrayItems ( xmp, xmpNS, xmpProp, 0, 0, kXMP_PropArrayIsOrdered, &fullValue );
			fullValue += '\x0';	// ! Need explicit final nul for SetTag below.
		} else {
			size_t count = xmp.CountArrayItems ( xmpNS, xmpProp );
			for ( size_t i = 1; i <= count; ++i ) {	// ! XMP arrays are indexed from 1.
				(void) xmp.GetArrayItem ( xmpNS, xmpProp, (XMP_Index)i, &itemValue, &xmpFlags );
				if ( ! XMP_PropIsSimple ( xmpFlags ) ) continue;	// ? Complain?
				fullValue.append ( itemValue );
				fullValue.append ( 1, '\x0' );
			}
		}

		tiff->SetTag ( ifd, id, kTIFF_ASCIIType, (XMP_Uns32)fullValue.size(), fullValue.c_str() );	// ! Already have trailing nul.

	} catch ( ... ) {
		// Do nothing, let other exports proceed.
		// ? Notify client?
	}

}	// ExportTIFF_ArrayASCII

// =================================================================================================
// ExportTIFF_LocTextASCII
// ======================

static void
ExportTIFF_LocTextASCII ( const SXMPMeta & xmp, const char * xmpNS, const char * xmpProp,
						  TIFF_Manager * tiff, XMP_Uns8 ifd, XMP_Uns16 id )
{
	try {	// Don't let errors with one stop the others.

		std::string xmpValue;

		bool foundXMP = xmp.GetLocalizedText ( xmpNS, xmpProp, "", "x-default", 0, &xmpValue, 0 );
		if ( ! foundXMP ) {
			tiff->DeleteTag ( ifd, id );
			return;
		}

		tiff->SetTag ( ifd, id, kTIFF_ASCIIType, (XMP_Uns32)( xmpValue.size()+1 ), xmpValue.c_str() );

	} catch ( ... ) {
		// Do nothing, let other exports proceed.
		// ? Notify client?
	}

}	// ExportTIFF_LocTextASCII

// =================================================================================================
// ExportTIFF_EncodedString
// ========================

static void
ExportTIFF_EncodedString ( const SXMPMeta & xmp, const char * xmpNS, const char * xmpProp,
						   TIFF_Manager * tiff, XMP_Uns8 ifd, XMP_Uns16 id, bool isLangAlt = false )
{
	try {	// Don't let errors with one stop the others.

		std::string    xmpValue;
		XMP_OptionBits xmpFlags;

		bool foundXMP = xmp.GetProperty ( xmpNS, xmpProp, &xmpValue, &xmpFlags );
		if ( ! foundXMP ) {
			tiff->DeleteTag ( ifd, id );
			return;
		}

		if ( ! isLangAlt ) {
			if ( ! XMP_PropIsSimple ( xmpFlags ) ) return;	// ? Complain? Delete the tag?
		} else {
			if ( ! XMP_ArrayIsAltText ( xmpFlags ) ) return;	// ? Complain? Delete the tag?
			bool ok = xmp.GetLocalizedText ( xmpNS, xmpProp, "", "x-default", 0, &xmpValue, 0 );
			if ( ! ok ) return;	// ? Complain? Delete the tag?
		}

		XMP_Uns8 encoding = kTIFF_EncodeASCII;
		for ( size_t i = 0; i < xmpValue.size(); ++i ) {
			if ( (XMP_Uns8)xmpValue[i] >= 0x80 ) {
				encoding = kTIFF_EncodeUnicode;
				break;
			}
		}

		tiff->SetTag_EncodedString ( ifd, id, xmpValue.c_str(), encoding );

	} catch ( ... ) {
		// Do nothing, let other exports proceed.
		// ? Notify client?
	}

}	// ExportTIFF_EncodedString

// =================================================================================================
// ExportTIFF_GPSCoordinate
// ========================
//
// The XMP format is either "deg,min,secR" or "deg,min.fracR", where 'R' is the reference direction,
// 'N', 'S', 'E', or 'W'. The location gets output as ( deg/1, min/1, sec/1 ) for the first form,
// and ( deg/1, minFrac/denom, 0/1 ) for the second form.

// ! We arbitrarily limit the number of fractional minute digits to 6 to avoid overflow in the
// ! combined numerator. But we don't otherwise check for overflow or range errors.

static void
ExportTIFF_GPSCoordinate ( const SXMPMeta & xmp, const char * xmpNS, const char * xmpProp,
						   TIFF_Manager * tiff, XMP_Uns8 ifd, XMP_Uns16 _id )
{
	XMP_Uns16 refID = _id-1;	// ! The GPS refs and locations are all tag N-1 and N pairs.
	XMP_Uns16 locID = _id;

	XMP_Assert ( (locID & 1) == 0 );

	try {	// Don't let errors with one stop the others.

		std::string xmpValue;
		XMP_OptionBits xmpFlags;

		bool foundXMP = xmp.GetProperty ( xmpNS, xmpProp, &xmpValue, &xmpFlags );
		if ( ! foundXMP ) {
			tiff->DeleteTag ( ifd, refID );
			tiff->DeleteTag ( ifd, locID );
			return;
		}

		if ( ! XMP_PropIsSimple ( xmpFlags ) ) return;

		const char * chPtr = xmpValue.c_str();

		XMP_Uns32 deg=0, minNum=0, minDenom=1, sec=0;

		for ( ; ('0' <= *chPtr) && (*chPtr <= '9'); ++chPtr ) deg = deg*10 + (*chPtr - '0');
		if ( *chPtr != ',' ) return;	// Bad XMP string.
		++chPtr;	// Skip the comma.

		for ( ; ('0' <= *chPtr) && (*chPtr <= '9'); ++chPtr ) minNum = minNum*10 + (*chPtr - '0');
		if ( (*chPtr != ',') && (*chPtr != '.') ) return;	// Bad XMP string.

		if ( *chPtr == ',' ) {

			++chPtr;	// Skip the comma.
			for ( ; ('0' <= *chPtr) && (*chPtr <= '9'); ++chPtr ) sec = sec*10 + (*chPtr - '0');

		} else {

			XMP_Assert ( *chPtr == '.' );
			++chPtr;	// Skip the period.
			for ( ; ('0' <= *chPtr) && (*chPtr <= '9'); ++chPtr ) {
				if ( minDenom > 100*1000 ) continue;	// Don't accumulate any more digits.
				minDenom *= 10;
				minNum = minNum*10 + (*chPtr - '0');
			}

		}

		if ( *(chPtr+1) != 0 ) return;	// Bad XMP string.

		char ref[2];
		ref[0] = *chPtr;
		ref[1] = 0;

		tiff->SetTag ( ifd, refID, kTIFF_ASCIIType, 2, &ref[0] );

		XMP_Uns32 loc[6];
		tiff->PutUns32 ( deg, &loc[0] );
		tiff->PutUns32 ( 1,   &loc[1] );
		tiff->PutUns32 ( minNum,   &loc[2] );
		tiff->PutUns32 ( minDenom, &loc[3] );
		tiff->PutUns32 ( sec, &loc[4] );
		tiff->PutUns32 ( 1,   &loc[5] );

		tiff->SetTag ( ifd, locID, kTIFF_RationalType, 3, &loc[0] );

	} catch ( ... ) {
		// Do nothing, let other exports proceed.
		// ? Notify client?
	}

}	// ExportTIFF_GPSCoordinate

// =================================================================================================
// ExportTIFF_GPSTimeStamp
// =======================
//
// The Exif is in 2 tags, GPSTimeStamp and GPSDateStamp. The time is 3 rationals for the hour, minute,
// and second in UTC. The date is a nul terminated string "YYYY:MM:DD".

static const double kBillion = 1000.0*1000.0*1000.0;
static const double mMaxSec  = 4.0*kBillion - 1.0;

static void
ExportTIFF_GPSTimeStamp ( const SXMPMeta & xmp, const char * xmpNS, const char * xmpProp, TIFF_Manager * tiff )
{
	
	try {	// Don't let errors with one stop the others.

		XMP_DateTime binXMP;
		bool foundXMP = xmp.GetProperty_Date ( xmpNS, xmpProp, &binXMP, 0 );
		if ( ! foundXMP ) {
			tiff->DeleteTag ( kTIFF_GPSInfoIFD, kTIFF_GPSTimeStamp );
			tiff->DeleteTag ( kTIFF_GPSInfoIFD, kTIFF_GPSDateStamp );
			return;
		}
		
		SXMPUtils::ConvertToUTCTime ( &binXMP );

		XMP_Uns32 exifTime[6];
		tiff->PutUns32 ( binXMP.hour, &exifTime[0] );
		tiff->PutUns32 ( 1, &exifTime[1] );
		tiff->PutUns32 ( binXMP.minute, &exifTime[2] );
		tiff->PutUns32 ( 1, &exifTime[3] );
		if ( binXMP.nanoSecond == 0 ) {
			tiff->PutUns32 ( binXMP.second, &exifTime[4] );
			tiff->PutUns32 ( 1,   &exifTime[5] );
		} else {
			double fSec = (double)binXMP.second + ((double)binXMP.nanoSecond / kBillion );
			XMP_Uns32 denom = 1000*1000;	// Choose microsecond resolution by default.
			TIFF_Manager::TagInfo oldInfo;
			bool hadExif = tiff->GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSTimeStamp, &oldInfo );
			if ( hadExif && (oldInfo.type == kTIFF_RationalType) && (oldInfo.count == 3) ) {
				XMP_Uns32 oldDenom = tiff->GetUns32 ( &(((XMP_Uns32*)oldInfo.dataPtr)[5]) );
				if ( oldDenom != 1 ) denom = oldDenom;
			}
			fSec *= denom;
			while ( fSec > mMaxSec ) { fSec /= 10; denom /= 10; }
			tiff->PutUns32 ( (XMP_Uns32)fSec, &exifTime[4] );
			tiff->PutUns32 ( denom, &exifTime[5] );
		}
		tiff->SetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSTimeStamp, kTIFF_RationalType, 3, &exifTime[0] );

		char exifDate[16];	// AUDIT: Long enough, only need 11.
		snprintf ( exifDate, 12, "%04d:%02d:%02d", binXMP.year, binXMP.month, binXMP.day );
		if ( exifDate[10] == 0 ) {	// Make sure there is no value overflow.
			tiff->SetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSDateStamp, kTIFF_ASCIIType, 11, exifDate );
		}

	} catch ( ... ) {
		// Do nothing, let other exports proceed.
		// ? Notify client?
	}

}	// ExportTIFF_GPSTimeStamp

// =================================================================================================
// =================================================================================================

// =================================================================================================
// PhotoDataUtils::ExportExif
// ==========================

void
PhotoDataUtils::ExportExif ( SXMPMeta * xmp, TIFF_Manager * exif )
{
	bool haveXMP, haveExif;
	std::string xmpValue;
	XMP_Int32 int32;
	XMP_Uns8 uns8;
	
	// Do all of the table driven standard exports.
	
	ExportTIFF_StandardMappings ( kTIFF_PrimaryIFD, exif, *xmp );
	ExportTIFF_StandardMappings ( kTIFF_ExifIFD, exif, *xmp );
	ExportTIFF_StandardMappings ( kTIFF_GPSInfoIFD, exif, *xmp );

	// Export dc:description to TIFF ImageDescription, and exif:UserComment to EXIF UserComment.
	
	// *** This is not following the MWG guidelines. The policy here tries to be more backward compatible.

	ExportTIFF_LocTextASCII ( *xmp, kXMP_NS_DC, "description",
							  exif, kTIFF_PrimaryIFD, kTIFF_ImageDescription );

	ExportTIFF_EncodedString ( *xmp, kXMP_NS_EXIF, "UserComment",
							   exif, kTIFF_ExifIFD, kTIFF_UserComment, true /* isLangAlt */ );
	
	// Export all of the date/time tags.
	// ! Special case: Don't create Exif DateTimeDigitized. This can avoid PSD full rewrite due to
	// ! new mapping from xmp:CreateDate.

	if ( exif->GetTag ( kTIFF_ExifIFD, kTIFF_DateTimeDigitized, 0 ) ) {
		ExportTIFF_Date ( *xmp, kXMP_NS_XMP, "CreateDate", exif, kTIFF_DateTimeDigitized );
	}

	ExportTIFF_Date ( *xmp, kXMP_NS_EXIF, "DateTimeOriginal", exif, kTIFF_DateTimeOriginal );
	ExportTIFF_Date ( *xmp, kXMP_NS_XMP, "ModifyDate", exif, kTIFF_DateTime );
	
	// 34855 ISOSpeedRatings has special cases for ISO over 65535. The tag is SHORT, some cameras
	// omit the tag and some write 65535, all put the real ISO in MakerNote - which ACR might
	// extract and leave in the XMP. There are 2 export cases:
	//	1. No XMP property, or one or more of the XMP values are over 65535:
	//		Leave both the XMP and native tag alone.
	//	1. Have XMP property and all of the XMP values are under 65535:
	//		Leave existing native tag, else export; strip the XMP either way.
	
	haveXMP = xmp->DoesPropertyExist ( kXMP_NS_EXIF,  "ISOSpeedRatings" );
	if ( haveXMP ) {

		XMP_Index i, count;
		std::string isoValue;
		bool haveHighISO = false;

		for ( i = 1, count = xmp->CountArrayItems ( kXMP_NS_EXIF,  "ISOSpeedRatings" ); i <= count; ++i ) {
			xmp->GetArrayItem ( kXMP_NS_EXIF,  "ISOSpeedRatings", i, &isoValue, 0 );
			if ( SXMPUtils::ConvertToInt ( isoValue.c_str() ) > 0xFFFF ) { haveHighISO = true; break; }
		}
		
		if ( ! haveHighISO ) {
			haveExif = exif->GetTag ( kTIFF_ExifIFD, kTIFF_ISOSpeedRatings, 0 );
			if ( ! haveExif ) {	// ISOSpeedRatings has an inject-only mapping.
				ExportArrayTIFF ( exif, kTIFF_ExifIFD, kISOSpeedMapping, exif->IsNativeEndian(), *xmp, kXMP_NS_EXIF,  "ISOSpeedRatings" );
			}
			xmp->DeleteProperty ( kXMP_NS_EXIF,  "ISOSpeedRatings");
		}
	
	}
	
	// Export the remaining TIFF, Exif, and GPS IFD tags.

	ExportTIFF_ArrayASCII ( *xmp, kXMP_NS_DC, "creator", exif, kTIFF_PrimaryIFD, kTIFF_Artist );

	ExportTIFF_LocTextASCII ( *xmp, kXMP_NS_DC, "rights", exif, kTIFF_PrimaryIFD, kTIFF_Copyright );
		
	haveXMP = xmp->GetProperty ( kXMP_NS_EXIF, "ExifVersion", &xmpValue, 0 );
	if ( haveXMP && (xmpValue.size() == 4) && (! exif->GetTag ( kTIFF_ExifIFD, kTIFF_ExifVersion, 0 )) ) {
		// 36864 ExifVersion is 4 "undefined" ASCII characters.
		exif->SetTag ( kTIFF_ExifIFD, kTIFF_ExifVersion, kTIFF_UndefinedType, 4, xmpValue.data() );
	}
	
	haveXMP = xmp->DoesPropertyExist ( kXMP_NS_EXIF, "ComponentsConfiguration" );
	if ( haveXMP && (xmp->CountArrayItems ( kXMP_NS_EXIF, "ComponentsConfiguration" ) == 4) &&
		 (! exif->GetTag ( kTIFF_ExifIFD, kTIFF_ComponentsConfiguration, 0 )) ) {
		// 37121 ComponentsConfiguration is an array of 4 "undefined" UInt8 bytes.
		XMP_Uns8 compConfig[4];
		xmp->GetProperty_Int ( kXMP_NS_EXIF, "ComponentsConfiguration[1]", &int32, 0 );
		compConfig[0] = (XMP_Uns8)int32;
		xmp->GetProperty_Int ( kXMP_NS_EXIF, "ComponentsConfiguration[2]", &int32, 0 );
		compConfig[1] = (XMP_Uns8)int32;
		xmp->GetProperty_Int ( kXMP_NS_EXIF, "ComponentsConfiguration[3]", &int32, 0 );
		compConfig[2] = (XMP_Uns8)int32;
		xmp->GetProperty_Int ( kXMP_NS_EXIF, "ComponentsConfiguration[4]", &int32, 0 );
		compConfig[3] = (XMP_Uns8)int32;
		exif->SetTag ( kTIFF_ExifIFD, kTIFF_ComponentsConfiguration, kTIFF_UndefinedType, 4, &compConfig[0] );
	}
	
	haveXMP = xmp->DoesPropertyExist ( kXMP_NS_EXIF, "Flash" );
	if ( haveXMP && (! exif->GetTag ( kTIFF_ExifIFD, kTIFF_Flash, 0 )) ) {
		// 37385 Flash is a UInt16 collection of bit fields and is mapped to a struct in XMP.
		XMP_Uns16 binFlash = 0;
		bool field;
		haveXMP = xmp->GetProperty_Bool ( kXMP_NS_EXIF, "Flash/exif:Fired", &field, 0 );
		if ( haveXMP & field ) binFlash |= 0x0001;
		haveXMP = xmp->GetProperty_Int ( kXMP_NS_EXIF, "Flash/exif:Return", &int32, 0 );
		if ( haveXMP ) binFlash |= (int32 & 3) << 1;
		haveXMP = xmp->GetProperty_Int ( kXMP_NS_EXIF, "Flash/exif:Mode", &int32, 0 );
		if ( haveXMP ) binFlash |= (int32 & 3) << 3;
		haveXMP = xmp->GetProperty_Bool ( kXMP_NS_EXIF, "Flash/exif:Function", &field, 0 );
		if ( haveXMP & field ) binFlash |= 0x0020;
		haveXMP = xmp->GetProperty_Bool ( kXMP_NS_EXIF, "Flash/exif:RedEyeMode", &field, 0 );
		if ( haveXMP & field ) binFlash |= 0x0040;
		exif->SetTag_Short ( kTIFF_ExifIFD, kTIFF_Flash, binFlash );
	}
	
	haveXMP = xmp->GetProperty_Int ( kXMP_NS_EXIF, "FileSource", &int32, 0 );
	if ( haveXMP && (! exif->GetTag ( kTIFF_ExifIFD, kTIFF_FileSource, 0 )) ) {
		// 41728 FileSource is an "undefined" UInt8.
		uns8 = (XMP_Uns8)int32;
		exif->SetTag ( kTIFF_ExifIFD, kTIFF_FileSource, kTIFF_UndefinedType, 1, &uns8 );
	}
	
	haveXMP = xmp->GetProperty_Int ( kXMP_NS_EXIF, "SceneType", &int32, 0 );
	if ( haveXMP && (! exif->GetTag ( kTIFF_ExifIFD, kTIFF_SceneType, 0 )) ) {
		// 41729 SceneType is an "undefined" UInt8.
		uns8 = (XMP_Uns8)int32;
		exif->SetTag ( kTIFF_ExifIFD, kTIFF_SceneType, kTIFF_UndefinedType, 1, &uns8 );
	}
	
	// *** Deferred inject-only tags: SpatialFrequencyResponse, DeviceSettingDescription, CFAPattern

	haveXMP = xmp->GetProperty ( kXMP_NS_EXIF, "GPSVersionID", &xmpValue, 0 );	// This is inject-only.
	if ( haveXMP && (xmpValue.size() == 7) && (! exif->GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSVersionID, 0 )) ) {
		char gpsID[4];	// 0 GPSVersionID is 4 UInt8 bytes and mapped in XMP as "n.n.n.n".
		gpsID[0] = xmpValue[0];
		gpsID[1] = xmpValue[2];
		gpsID[2] = xmpValue[4];
		gpsID[3] = xmpValue[6];
		exif->SetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSVersionID, kTIFF_ByteType, 4, &gpsID[0] );
	}
	
	ExportTIFF_GPSCoordinate ( *xmp, kXMP_NS_EXIF, "GPSLatitude", exif, kTIFF_GPSInfoIFD, kTIFF_GPSLatitude );

	ExportTIFF_GPSCoordinate ( *xmp, kXMP_NS_EXIF, "GPSLongitude", exif, kTIFF_GPSInfoIFD, kTIFF_GPSLongitude );
	
	ExportTIFF_GPSTimeStamp ( *xmp, kXMP_NS_EXIF, "GPSTimeStamp", exif );

	// The following GPS tags are inject-only.
	
	haveXMP = xmp->DoesPropertyExist ( kXMP_NS_EXIF, "GPSDestLatitude" );
	if ( haveXMP && (! exif->GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSDestLatitude, 0 )) ) {
		ExportTIFF_GPSCoordinate ( *xmp, kXMP_NS_EXIF, "GPSDestLatitude", exif, kTIFF_GPSInfoIFD, kTIFF_GPSDestLatitude );
	}

	haveXMP = xmp->DoesPropertyExist ( kXMP_NS_EXIF, "GPSDestLongitude" );
	if ( haveXMP && (! exif->GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSDestLongitude, 0 )) ) {
		ExportTIFF_GPSCoordinate ( *xmp, kXMP_NS_EXIF, "GPSDestLongitude", exif, kTIFF_GPSInfoIFD, kTIFF_GPSDestLongitude );
	}

	haveXMP = xmp->GetProperty ( kXMP_NS_EXIF, "GPSProcessingMethod", &xmpValue, 0 );
	if ( haveXMP && (! xmpValue.empty()) && (! exif->GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSProcessingMethod, 0 )) ) {
		// 27 GPSProcessingMethod is a string with explicit encoding.
		ExportTIFF_EncodedString ( *xmp, kXMP_NS_EXIF, "GPSProcessingMethod", exif, kTIFF_GPSInfoIFD, kTIFF_GPSProcessingMethod );
	}

	haveXMP = xmp->GetProperty ( kXMP_NS_EXIF, "GPSAreaInformation", &xmpValue, 0 );
	if ( haveXMP && (! xmpValue.empty()) && (! exif->GetTag ( kTIFF_GPSInfoIFD, kTIFF_GPSAreaInformation, 0 )) ) {
		// 28 GPSAreaInformation is a string with explicit encoding.
		ExportTIFF_EncodedString ( *xmp, kXMP_NS_EXIF, "GPSAreaInformation", exif, kTIFF_GPSInfoIFD, kTIFF_GPSAreaInformation );
	}
	
}	// PhotoDataUtils::ExportExif
