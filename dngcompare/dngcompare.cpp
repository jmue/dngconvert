/* This file is part of the dngconvert project
   Copyright (C) 2011 Jens Mueller <tschensensinger at gmx dot de>
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public   
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "dng_file_stream.h"
#include "dng_host.h"
#include "dng_info.h"
#include "dng_xmp_sdk.h"

#include <cmath>
#include <limits>

#include "dnghost.h"

bool AreSame(real64 a, real64 b)
{
    return std::fabs(a - b) < std::numeric_limits<real32>::epsilon();
}

void compareExif(const dng_exif& exif1, const dng_exif& exif2)
{
    if (!AreSame(exif1.fApertureValue.As_real64(), exif2.fApertureValue.As_real64()))
        printf("    ApertureValue: %.1f %.1f\n", exif1.fApertureValue.As_real64(), exif2.fApertureValue.As_real64());
    if (exif1.fArtist != exif2.fArtist)
        printf("    Artist: '%s' '%s'\n", exif1.fArtist.Get(), exif2.fArtist.Get());
    if (exif1.fBatteryLevelA != exif2.fBatteryLevelA)
        printf("    BatteryLevelA: '%s' '%s'\n", exif1.fBatteryLevelA.Get(), exif2.fBatteryLevelA.Get());
    if (!AreSame(exif1.fBatteryLevelR.As_real64(), exif2.fBatteryLevelR.As_real64()))
        printf("    BatteryLevelR: %.1f %.1f\n", exif1.fBatteryLevelR.As_real64(), exif2.fBatteryLevelR.As_real64());
    if (!AreSame(exif1.fBrightnessValue.As_real64(), exif2.fBrightnessValue.As_real64()))
        printf("    BrightnessValue: %.1f %.1f\n", exif1.fBrightnessValue.As_real64(), exif2.fBrightnessValue.As_real64());
    if (exif1.fCameraSerialNumber != exif2.fCameraSerialNumber)
        printf("    CameraSerialNumber: '%s' '%s'\n", exif1.fCameraSerialNumber.Get(), exif2.fCameraSerialNumber.Get());
    if (0 != memcmp(exif1.fCFAPattern, exif2.fCFAPattern, kMaxCFAPattern * kMaxCFAPattern * sizeof(uint8)))
        printf("    CFAPattern\n");
    if (exif1.fCFARepeatPatternCols != exif2.fCFARepeatPatternCols)
        printf("    CFARepeatPatternCols: %d %d\n", exif1.fCFARepeatPatternCols, exif2.fCFARepeatPatternCols);
    if (exif1.fCFARepeatPatternRows != exif2.fCFARepeatPatternRows)
        printf("    CFARepeatPatternRows: %d %d\n", exif1.fCFARepeatPatternRows, exif2.fCFARepeatPatternRows);
    if (exif1.fColorSpace != exif2.fColorSpace)
        printf("    ColorSpace: %d %d\n", exif1.fColorSpace, exif2.fColorSpace);
    if (exif1.fComponentsConfiguration != exif2.fComponentsConfiguration)
        printf("    ComponentsConfiguration: %d %d\n", exif1.fComponentsConfiguration, exif2.fComponentsConfiguration);
    if (!AreSame(exif1.fCompresssedBitsPerPixel.As_real64(), exif2.fCompresssedBitsPerPixel.As_real64()))
        printf("    CompresssedBitsPerPixel: %.1f %.1f\n", exif1.fCompresssedBitsPerPixel.As_real64(), exif2.fCompresssedBitsPerPixel.As_real64());
    if (exif1.fContrast != exif2.fContrast)
        printf("    Contrast: %d %d\n", exif1.fContrast, exif2.fContrast);
    if (exif1.fCopyright != exif2.fCopyright)
        printf("    Copyright: '%s' '%s'\n", exif1.fCopyright.Get(), exif2.fCopyright.Get());
    if (exif1.fCopyright2 != exif2.fCopyright2)
        printf("    Copyright2: '%s' '%s'\n", exif1.fCopyright2.Get(), exif2.fCopyright2.Get());
    if (exif1.fCustomRendered != exif2.fCustomRendered)
        printf("    CustomRendered: %d %d\n", exif1.fCustomRendered, exif2.fCustomRendered);
    if (exif1.fDateTime.DateTime() != exif2.fDateTime.DateTime())
        printf("    DateTime: '%s' '%s'\n", exif1.fDateTime.Encode_ISO_8601().Get(), exif2.fDateTime.Encode_ISO_8601().Get());
    if (exif1.fDateTimeDigitized.DateTime() != exif2.fDateTimeDigitized.DateTime())
        printf("    DateTimeDigitized: '%s' '%s'\n", exif1.fDateTimeDigitized.Encode_ISO_8601().Get(), exif2.fDateTimeDigitized.Encode_ISO_8601().Get());
    //if ((exif1.fDateTimeDigitizedStorageInfo.IsValid() != exif2.fDateTimeDigitizedStorageInfo.IsValid()) ||
    //    ((exif1.fDateTimeDigitizedStorageInfo.IsValid()) &&
    //     (exif1.fDateTimeDigitizedStorageInfo.Format() != exif2.fDateTimeDigitizedStorageInfo.Format()) ||
    //     (exif1.fDateTimeDigitizedStorageInfo.Offset() != exif2.fDateTimeDigitizedStorageInfo.Offset())))
    //  printf("    DateTimeDigitizedStorageInfo\n");
    if (exif1.fDateTimeOriginal.DateTime() != exif2.fDateTimeOriginal.DateTime())
        printf("    DateTimeOriginal: '%s' '%s'\n", exif1.fDateTimeOriginal.Encode_ISO_8601().Get(), exif2.fDateTimeOriginal.Encode_ISO_8601().Get());
    //if ((exif1.fDateTimeOriginalStorageInfo.IsValid() != exif2.fDateTimeOriginalStorageInfo.IsValid()) ||
    //    ((exif1.fDateTimeOriginalStorageInfo.IsValid()) &&
    //     (exif1.fDateTimeOriginalStorageInfo.Format() != exif2.fDateTimeOriginalStorageInfo.Format()) ||
    //     (exif1.fDateTimeOriginalStorageInfo.Offset() != exif2.fDateTimeOriginalStorageInfo.Offset())))
    //  printf("    DateTimeOriginalStorageInfo\n");
    //if ((exif1.fDateTimeStorageInfo.IsValid() != exif2.fDateTimeStorageInfo.IsValid()) ||
    //    ((exif1.fDateTimeStorageInfo.IsValid()) &&
    //     (exif1.fDateTimeStorageInfo.Format() != exif2.fDateTimeStorageInfo.Format()) ||
    //     (exif1.fDateTimeStorageInfo.Offset() != exif2.fDateTimeStorageInfo.Offset())))
    //  printf("    DateTimeStorageInfo\n");
    if (!AreSame(exif1.fDigitalZoomRatio.As_real64(), exif2.fDigitalZoomRatio.As_real64()))
        printf("    DigitalZoomRatio: %.1f %.1f\n", exif1.fDigitalZoomRatio.As_real64(), exif2.fDigitalZoomRatio.As_real64());
    if (exif1.fExifVersion != exif2.fExifVersion)
        printf("    ExifVersion: %d %d\n", exif1.fExifVersion, exif2.fExifVersion);
    if (!AreSame(exif1.fExposureBiasValue.As_real64(), exif2.fExposureBiasValue.As_real64()))
        printf("    ExposureBiasValue: %.1f %.1f\n", exif1.fExposureBiasValue.As_real64(), exif2.fExposureBiasValue.As_real64());
    if (!AreSame(exif1.fExposureIndex.As_real64(), exif2.fExposureIndex.As_real64()))
        printf("    ExposureIndex: %.1f %.1f\n", exif1.fExposureIndex.As_real64(), exif2.fExposureIndex.As_real64());
    if (exif1.fExposureMode != exif2.fExposureMode)
        printf("    ExposureMode: %d %d\n", exif1.fExposureMode, exif2.fExposureMode);
    if (exif1.fExposureProgram != exif2.fExposureProgram)
        printf("    ExposureProgram: %d %d\n", exif1.fExposureProgram, exif2.fExposureProgram);
    if (!AreSame(exif1.fExposureTime.As_real64(), exif2.fExposureTime.As_real64()))
        printf("    ExposureTime: %.1f %.1f\n", exif1.fExposureTime.As_real64(), exif2.fExposureTime.As_real64());
    if (exif1.fFileSource != exif2.fFileSource)
        printf("    FileSource: %d %d\n", exif1.fFileSource, exif2.fFileSource);
    if (exif1.fFirmware != exif2.fFirmware)
        printf("    Firmware: '%s' '%s'\n", exif1.fFirmware.Get(), exif2.fFirmware.Get());
    if (exif1.fFlash != exif2.fFlash)
        printf("    Flash: %d %d\n", exif1.fFlash, exif2.fFlash);
    if (!AreSame(exif1.fFlashCompensation.As_real64(), exif2.fFlashCompensation.As_real64()))
        printf("    FlashCompensation: %.1f %.1f\n", exif1.fFlashCompensation.As_real64(), exif2.fFlashCompensation.As_real64());
    if (exif1.fFlashMask != exif2.fFlashMask)
        printf("    FlashMask: %d %d\n", exif1.fFlashMask, exif2.fFlashMask);
    if (exif1.fFlashPixVersion != exif2.fFlashPixVersion)
        printf("    FlashPixVersion: %d %d\n", exif1.fFlashPixVersion, exif2.fFlashPixVersion);
    if (!AreSame(exif1.fFNumber.As_real64(), exif2.fFNumber.As_real64()))
        printf("    FNumber: %.1f %.1f\n", exif1.fFNumber.As_real64(), exif2.fFNumber.As_real64());
    if (!AreSame(exif1.fFocalLength.As_real64(), exif2.fFocalLength.As_real64()))
        printf("    FocalLength: %.1f %.1f\n", exif1.fFocalLength.As_real64(), exif2.fFocalLength.As_real64());
    if (exif1.fFocalLengthIn35mmFilm != exif2.fFocalLengthIn35mmFilm)
        printf("    FocalLengthIn35mmFilm: %d %d\n", exif1.fFocalLengthIn35mmFilm, exif2.fFocalLengthIn35mmFilm);
    if (exif1.fFocalPlaneResolutionUnit != exif2.fFocalPlaneResolutionUnit)
        printf("    FocalPlaneResolutionUnit: %d %d\n", exif1.fFocalPlaneResolutionUnit, exif2.fFocalPlaneResolutionUnit);
    if (!AreSame(exif1.fFocalPlaneXResolution.As_real64(), exif2.fFocalPlaneXResolution.As_real64()))
        printf("    FocalPlaneXResolution: %.1f %.1f\n", exif1.fFocalPlaneXResolution.As_real64(), exif2.fFocalPlaneXResolution.As_real64());
    if (!AreSame(exif1.fFocalPlaneYResolution.As_real64(), exif2.fFocalPlaneYResolution.As_real64()))
        printf("    FocalPlaneYResolution: %.1f %.1f\n", exif1.fFocalPlaneYResolution.As_real64(), exif2.fFocalPlaneYResolution.As_real64());
    if (exif1.fGainControl != exif2.fGainControl)
        printf("    GainControl: %d %d\n", exif1.fGainControl, exif2.fGainControl);
    if (!AreSame(exif1.fGamma.As_real64(), exif2.fGamma.As_real64()))
        printf("    Gamma: %.1f %.1f\n", exif1.fGamma.As_real64(), exif2.fGamma.As_real64());
    if (!AreSame(exif1.fGPSAltitude.As_real64(), exif2.fGPSAltitude.As_real64()))
        printf("    GPSAltitude: %.1f %.1f\n", exif1.fGPSAltitude.As_real64(), exif2.fGPSAltitude.As_real64());
    if (exif1.fGPSAltitudeRef != exif2.fGPSAltitudeRef)
        printf("    GPSAltitudeRef: %d %d\n", exif1.fGPSAltitudeRef, exif2.fGPSAltitudeRef);
    if (exif1.fGPSAreaInformation != exif2.fGPSAreaInformation)
        printf("    GPSAreaInformation: '%s' '%s'\n", exif1.fGPSAreaInformation.Get(), exif2.fGPSAreaInformation.Get());
    if (exif1.fGPSDateStamp != exif2.fGPSDateStamp)
        printf("    GPSDateStamp: '%s' '%s'\n", exif1.fGPSDateStamp.Get(), exif2.fGPSDateStamp.Get());
    if (!AreSame(exif1.fGPSDestBearing.As_real64(), exif2.fGPSDestBearing.As_real64()))
        printf("    GPSDestBearing: %.1f %.1f\n", exif1.fGPSDestBearing.As_real64(), exif2.fGPSDestBearing.As_real64());
    if (exif1.fGPSDestBearingRef != exif2.fGPSDestBearingRef)
        printf("    GPSDestBearingRef: '%s' '%s'\n", exif1.fGPSDestBearingRef.Get(), exif2.fGPSDestBearingRef.Get());
    if (!AreSame(exif1.fGPSDestDistance.As_real64(), exif2.fGPSDestDistance.As_real64()))
        printf("    GPSDestDistance: %.1f %.1f\n", exif1.fGPSDestDistance.As_real64(), exif2.fGPSDestDistance.As_real64());
    if (exif1.fGPSDestDistanceRef != exif2.fGPSDestDistanceRef)
        printf("    GPSDestDistanceRef: '%s' '%s'\n", exif1.fGPSDestDistanceRef.Get(), exif2.fGPSDestDistanceRef.Get());
    if ((!AreSame(exif1.fGPSDestLatitude[0].As_real64(), exif2.fGPSDestLatitude[0].As_real64())) ||
            (!AreSame(exif1.fGPSDestLatitude[1].As_real64(), exif2.fGPSDestLatitude[1].As_real64())) ||
            (!AreSame(exif1.fGPSDestLatitude[2].As_real64(), exif2.fGPSDestLatitude[2].As_real64())))
        printf("    GPSDestLatitude\n");
    if (exif1.fGPSDestLatitudeRef != exif2.fGPSDestLatitudeRef)
        printf("    GPSDestLatitudeRef: '%s' '%s'\n", exif1.fGPSDestLatitudeRef.Get(), exif2.fGPSDestLatitudeRef.Get());
    if ((!AreSame(exif1.fGPSDestLongitude[0].As_real64(), exif2.fGPSDestLongitude[0].As_real64())) ||
            (!AreSame(exif1.fGPSDestLongitude[1].As_real64(), exif2.fGPSDestLongitude[1].As_real64())) ||
            (!AreSame(exif1.fGPSDestLongitude[2].As_real64(), exif2.fGPSDestLongitude[2].As_real64())))
        printf("    GPSDestLongitude\n");
    if (exif1.fGPSDestLongitudeRef != exif2.fGPSDestLongitudeRef)
        printf("    GPSDestLongitudeRef: '%s' '%s'\n", exif1.fGPSDestLongitudeRef.Get(), exif2.fGPSDestLongitudeRef.Get());
    if (exif1.fGPSDifferential != exif2.fGPSDifferential)
        printf("    GPSDifferential: %d %d\n", exif1.fGPSDifferential, exif2.fGPSDifferential);
    if (!AreSame(exif1.fGPSDOP.As_real64(), exif2.fGPSDOP.As_real64()))
        printf("    GPSDOP\n: %.1f %.1f\n", exif1.fGPSDOP.As_real64(), exif2.fGPSDOP.As_real64());
    if (!AreSame(exif1.fGPSImgDirection.As_real64(), exif2.fGPSImgDirection.As_real64()))
        printf("    GPSImgDirection: %.1f %.1f\n", exif1.fGPSImgDirection.As_real64(), exif2.fGPSImgDirection.As_real64());
    if (exif1.fGPSImgDirectionRef != exif2.fGPSImgDirectionRef)
        printf("    GPSImgDirectionRef: '%s' '%s'\n", exif1.fGPSImgDirectionRef.Get(), exif2.fGPSImgDirectionRef.Get());
    if ((!AreSame(exif1.fGPSLatitude[0].As_real64(), exif2.fGPSLatitude[0].As_real64())) ||
            (!AreSame(exif1.fGPSLatitude[1].As_real64(), exif2.fGPSLatitude[1].As_real64())) ||
            (!AreSame(exif1.fGPSLatitude[2].As_real64(), exif2.fGPSLatitude[2].As_real64())))
        printf("    GPSLatitude\n");
    if (exif1.fGPSLatitudeRef != exif2.fGPSLatitudeRef)
        printf("    GPSLatitudeRef: '%s' '%s'\n", exif1.fGPSLatitudeRef.Get(), exif2.fGPSLatitudeRef.Get());
    if ((!AreSame(exif1.fGPSLongitude[0].As_real64(), exif2.fGPSLongitude[0].As_real64())) ||
            (!AreSame(exif1.fGPSLongitude[1].As_real64(), exif2.fGPSLongitude[1].As_real64())) ||
            (!AreSame(exif1.fGPSLongitude[2].As_real64(), exif2.fGPSLongitude[2].As_real64())))
        printf("    GPSLongitude\n");
    if (exif1.fGPSLongitudeRef != exif2.fGPSLongitudeRef)
        printf("    GPSLongitudeRef: '%s' '%s'\n", exif1.fGPSLongitudeRef.Get(), exif2.fGPSLongitudeRef.Get());
    if (exif1.fGPSMapDatum != exif2.fGPSMapDatum)
        printf("    GPSMapDatum: '%s' '%s'\n", exif1.fGPSMapDatum.Get(), exif2.fGPSMapDatum.Get());
    if (exif1.fGPSMeasureMode != exif2.fGPSMeasureMode)
        printf("    GPSMeasureMode: '%s' '%s'\n", exif1.fGPSMeasureMode.Get(), exif2.fGPSMeasureMode.Get());
    if (exif1.fGPSProcessingMethod != exif2.fGPSProcessingMethod)
        printf("    GPSProcessingMethod: '%s' '%s'\n", exif1.fGPSProcessingMethod.Get(), exif2.fGPSProcessingMethod.Get());
    if (exif1.fGPSSatellites != exif2.fGPSSatellites)
        printf("    GPSSatellites: '%s' '%s'\n", exif1.fGPSSatellites.Get(), exif2.fGPSSatellites.Get());
    if (!AreSame(exif1.fGPSSpeed.As_real64(), exif2.fGPSSpeed.As_real64()))
        printf("    GPSSpeed: %.1f %.1f\n", exif1.fGPSSpeed.As_real64(), exif2.fGPSSpeed.As_real64());
    if (exif1.fGPSSpeedRef != exif2.fGPSSpeedRef)
        printf("    GPSSpeedRef: '%s' '%s'\n", exif1.fGPSSpeedRef.Get(), exif2.fGPSSpeedRef.Get());
    if (exif1.fGPSStatus != exif2.fGPSStatus)
        printf("    GPSStatus: '%s' '%s'\n", exif1.fGPSStatus.Get(), exif2.fGPSStatus.Get());
    if ((!AreSame(exif1.fGPSTimeStamp[0].As_real64(), exif2.fGPSTimeStamp[0].As_real64())) ||
            (!AreSame(exif1.fGPSTimeStamp[1].As_real64(), exif2.fGPSTimeStamp[1].As_real64())) ||
            (!AreSame(exif1.fGPSTimeStamp[2].As_real64(), exif2.fGPSTimeStamp[2].As_real64())))
        printf("    GPSTimeStamp\n");
    if (!AreSame(exif1.fGPSTrack.As_real64(), exif2.fGPSTrack.As_real64()))
        printf("    GPSTrack: %.1f %.1f\n", exif1.fGPSTrack.As_real64(), exif2.fGPSTrack.As_real64());
    if (exif1.fGPSTrackRef != exif2.fGPSTrackRef)
        printf("    GPSTrackRef: '%s' '%s'\n", exif1.fGPSTrackRef.Get(), exif2.fGPSTrackRef.Get());
    if (exif1.fGPSVersionID != exif2.fGPSVersionID)
        printf("    GPSVersionID %d %d\n", exif1.fGPSVersionID, exif2.fGPSVersionID);
    if (exif1.fImageDescription != exif2.fImageDescription)
        printf("    ImageDescription: '%s' '%s'\n", exif1.fImageDescription.Get(), exif2.fImageDescription.Get());
    if (exif1.fImageNumber != exif2.fImageNumber)
        printf("    ImageNumber: %d %d\n", exif1.fImageNumber, exif2.fImageNumber);
    if (exif1.fImageUniqueID != exif2.fImageUniqueID)
        printf("    ImageUniqueID\n");
    if (exif1.fInteroperabilityIndex != exif2.fInteroperabilityIndex)
        printf("    InteroperabilityIndex: '%s' '%s'\n", exif1.fInteroperabilityIndex.Get(), exif2.fInteroperabilityIndex.Get());
    if (exif1.fInteroperabilityVersion != exif2.fInteroperabilityVersion)
        printf("    InteroperabilityVersion: %d %d\n", exif1.fInteroperabilityVersion, exif2.fInteroperabilityVersion);
    if ((exif1.fISOSpeedRatings[0] != exif2.fISOSpeedRatings[0]) ||
            (exif1.fISOSpeedRatings[1] != exif2.fISOSpeedRatings[1]) ||
            (exif1.fISOSpeedRatings[2] != exif2.fISOSpeedRatings[2]))
        printf("    ISOSpeedRatings\n");
    if (exif1.fLensID != exif2.fLensID)
        printf("    LensID: '%s' '%s'\n", exif1.fLensID.Get(), exif2.fLensID.Get());
    if ((!AreSame(exif1.fLensInfo[0].As_real64(), exif2.fLensInfo[0].As_real64())) ||
            (!AreSame(exif1.fLensInfo[1].As_real64(), exif2.fLensInfo[1].As_real64())) ||
            (!AreSame(exif1.fLensInfo[2].As_real64(), exif2.fLensInfo[2].As_real64())) ||
            (!AreSame(exif1.fLensInfo[3].As_real64(), exif2.fLensInfo[3].As_real64())))
        printf("    LensInfo\n");
    if (exif1.fLensName != exif2.fLensName)
        printf("    LensName: '%s' '%s'\n", exif1.fLensName.Get(), exif2.fLensName.Get());
    if (exif1.fLensSerialNumber != exif2.fLensSerialNumber)
        printf("    LensSerialNumber: '%s' '%s'\n", exif1.fLensSerialNumber.Get(), exif2.fLensSerialNumber.Get());
    if (exif1.fLightSource != exif2.fLightSource)
        printf("    LightSource: %d %d\n", exif1.fLightSource, exif2.fLightSource);
    if (exif1.fMake != exif2.fMake)
        printf("    Make: '%s' '%s'\n", exif1.fMake.Get(), exif2.fMake.Get());
    if (!AreSame(exif1.fMaxApertureValue.As_real64(), exif2.fMaxApertureValue.As_real64()))
        printf("    MaxApertureValue: %.1f %.1f\n", exif1.fMaxApertureValue.As_real64(), exif2.fMaxApertureValue.As_real64());
    if (exif1.fMeteringMode != exif2.fMeteringMode)
        printf("    MeteringMode: %d %d\n", exif1.fMeteringMode, exif2.fMeteringMode);
    if (exif1.fModel != exif2.fModel)
        printf("    Model: '%s' '%s'\n", exif1.fModel.Get(), exif2.fModel.Get());
    if (exif1.fOwnerName != exif2.fOwnerName)
        printf("    OwnerName: '%s' '%s'\n", exif1.fOwnerName.Get(), exif2.fOwnerName.Get());
    if (exif1.fPixelXDimension != exif2.fPixelXDimension)
        printf("    PixelXDimension: %d %d\n", exif1.fPixelXDimension, exif2.fPixelXDimension);
    if (exif1.fPixelYDimension != exif2.fPixelYDimension)
        printf("    PixelYDimension: %d %d\n", exif1.fPixelYDimension, exif2.fPixelYDimension);
    if (exif1.fRelatedImageFileFormat != exif2.fRelatedImageFileFormat)
        printf("    RelatedImageFileFormat: '%s' '%s'\n", exif1.fRelatedImageFileFormat.Get(), exif2.fRelatedImageFileFormat.Get());
    if (exif1.fRelatedImageLength != exif2.fRelatedImageLength)
        printf("    RelatedImageLength: %d %d\n", exif1.fRelatedImageLength, exif2.fRelatedImageLength);
    if (exif1.fRelatedImageWidth != exif2.fRelatedImageWidth)
        printf("    RelatedImageWidth: %d %d\n", exif1.fRelatedImageWidth, exif2.fRelatedImageWidth);
    if (exif1.fSaturation != exif2.fSaturation)
        printf("    Saturation: %d %d\n", exif1.fSaturation, exif2.fSaturation);
    if (exif1.fSceneCaptureType != exif2.fSceneCaptureType)
        printf("    SceneCaptureType: %d %d\n", exif1.fSceneCaptureType, exif2.fSceneCaptureType);
    if (exif1.fSceneType != exif2.fSceneType)
        printf("    SceneType: %d %d\n", exif1.fSceneType, exif2.fSceneType);
    if (exif1.fSelfTimerMode != exif2.fSelfTimerMode)
        printf("    SelfTimerMode: %d %d\n", exif1.fSelfTimerMode, exif2.fSelfTimerMode);
    if (exif1.fSensingMethod != exif2.fSensingMethod)
        printf("    SensingMethod: %d %d\n", exif1.fSensingMethod, exif2.fSensingMethod);
    if (exif1.fSharpness != exif2.fSharpness)
        printf("    Sharpness: %d %d\n", exif1.fSharpness, exif2.fSharpness);
    if (!AreSame(exif1.fShutterSpeedValue.As_real64(), exif2.fShutterSpeedValue.As_real64()))
        printf("    ShutterSpeedValue: %.1f %.1f\n", exif1.fShutterSpeedValue.As_real64(), exif2.fShutterSpeedValue.As_real64());
    if (exif1.fSoftware != exif2.fSoftware)
        printf("    Software: '%s' '%s'\n", exif1.fSoftware.Get(), exif2.fSoftware.Get());
    if (0 != memcmp(exif1.fSubjectArea, exif2.fSubjectArea, 4 * sizeof(uint32)))
        printf("    SubjectArea\n");
    if (exif1.fSubjectAreaCount != exif2.fSubjectAreaCount)
        printf("    SubjectAreaCount: %d %d\n", exif1.fSubjectAreaCount, exif2.fSubjectAreaCount);
    if (!AreSame(exif1.fSubjectDistance.As_real64(), exif2.fSubjectDistance.As_real64()))
        printf("    SubjectDistance: %.1f %.1f\n", exif1.fSubjectDistance.As_real64(), exif2.fSubjectDistance.As_real64());
    if (exif1.fSubjectDistanceRange != exif2.fSubjectDistanceRange)
        printf("    SubjectDistanceRange: %d %d\n", exif1.fSubjectDistanceRange, exif2.fSubjectDistanceRange);
    if (exif1.fTIFF_EP_StandardID != exif2.fTIFF_EP_StandardID)
        printf("    TIFF_EP_StandardID: %d %d\n", exif1.fTIFF_EP_StandardID, exif2.fTIFF_EP_StandardID);
    if (exif1.fUserComment != exif2.fUserComment)
        printf("    UserComment: '%s' '%s'\n", exif1.fUserComment.Get(), exif2.fUserComment.Get());
    if (exif1.fWhiteBalance != exif2.fWhiteBalance)
        printf("    WhiteBalance: %d %d\n", exif1.fWhiteBalance, exif2.fWhiteBalance);
}

void compareIfd(const dng_ifd& ifd1, const dng_ifd& ifd2)
{
    if (ifd1.fActiveArea != ifd2.fActiveArea)
        printf("    ActiveArea (t/l/b/r): %d/%d/%d/%d %d/%d/%d/%d\n",
               ifd1.fActiveArea.t, ifd1.fActiveArea.l, ifd1.fActiveArea.b, ifd1.fActiveArea.r,
               ifd2.fActiveArea.t, ifd2.fActiveArea.l, ifd2.fActiveArea.b, ifd2.fActiveArea.r);
    if (!AreSame(ifd1.fAntiAliasStrength.As_real64(), ifd2.fAntiAliasStrength.As_real64()))
        printf("    AntiAliasStrength: %.1f %.1f\n", ifd1.fAntiAliasStrength.As_real64(), ifd2.fAntiAliasStrength.As_real64());
    if (ifd1.fBayerGreenSplit != ifd2.fBayerGreenSplit)
        printf("    BayerGreenSplit: %d %d\n", ifd1.fBayerGreenSplit, ifd2.fBayerGreenSplit);
    if (!AreSame(ifd1.fBestQualityScale.As_real64(), ifd2.fBestQualityScale.As_real64()))
        printf("    BestQualityScale: %.3f %.3f\n", ifd1.fBestQualityScale.As_real64(), ifd2.fBestQualityScale.As_real64());
    if (0 != memcmp(ifd1.fBitsPerSample, ifd2.fBitsPerSample, kMaxSamplesPerPixel * sizeof(uint32)))
        printf("    BitsPerSample\n");
    if (0 != memcmp(ifd1.fBlackLevel, ifd2.fBlackLevel, kMaxBlackPattern * kMaxBlackPattern * kMaxSamplesPerPixel * sizeof(real64)))
        printf("    BlackLevel\n");
    if (ifd1.fBlackLevelDeltaHCount != ifd2.fBlackLevelDeltaHCount)
        printf("    BlackLevelDeltaHCount: %d %d\n", ifd1.fBlackLevelDeltaHCount, ifd2.fBlackLevelDeltaHCount);
    //if (ifd1.fBlackLevelDeltaHOffset != ifd2.fBlackLevelDeltaHOffset)
    //    printf("    BlackLevelDeltaHOffset: %lld %lld\n", ifd1.fBlackLevelDeltaHOffset, ifd2.fBlackLevelDeltaHOffset);
    if (ifd1.fBlackLevelDeltaHType != ifd2.fBlackLevelDeltaHType)
        printf("    BlackLevelDeltaHType: %d %d\n", ifd1.fBlackLevelDeltaHType, ifd2.fBlackLevelDeltaHType);
    if (ifd1.fBlackLevelDeltaVCount != ifd2.fBlackLevelDeltaVCount)
        printf("    BlackLevelDeltaVCount: %d %d\n", ifd1.fBlackLevelDeltaVCount, ifd2.fBlackLevelDeltaVCount);
    //if (ifd1.fBlackLevelDeltaVOffset != ifd2.fBlackLevelDeltaVOffset)
    //    printf("    BlackLevelDeltaVOffset: %lld %lld\n", ifd1.fBlackLevelDeltaVOffset, ifd2.fBlackLevelDeltaVOffset);
    if (ifd1.fBlackLevelDeltaVType != ifd2.fBlackLevelDeltaVType)
        printf("    BlackLevelDeltaVType: %d %d\n", ifd1.fBlackLevelDeltaVType, ifd2.fBlackLevelDeltaVType);
    if (ifd1.fBlackLevelRepeatCols != ifd2.fBlackLevelRepeatCols)
        printf("    BlackLevelRepeatCols: %d %d\n", ifd1.fBlackLevelRepeatCols, ifd2.fBlackLevelRepeatCols);
    if (ifd1.fBlackLevelRepeatRows != ifd2.fBlackLevelRepeatRows)
        printf("    BlackLevelRepeatRows: %d %d\n", ifd1.fBlackLevelRepeatRows, ifd2.fBlackLevelRepeatRows);
    if (!AreSame(ifd1.fChromaBlurRadius.As_real64(), ifd2.fChromaBlurRadius.As_real64()))
        printf("    ChromaBlurRadius: %.1f %.1f\n", ifd1.fChromaBlurRadius.As_real64(), ifd2.fChromaBlurRadius.As_real64());
    if (ifd1.fCompression != ifd2.fCompression)
        printf("    Compression: %d %d\n", ifd1.fCompression, ifd2.fCompression);
    if (!AreSame(ifd1.fDefaultCropOriginH.As_real64(), ifd2.fDefaultCropOriginH.As_real64()))
        printf("    DefaultCropOriginH: %.1f %.1f\n", ifd1.fDefaultCropOriginH.As_real64(), ifd2.fDefaultCropOriginH.As_real64());
    if (!AreSame(ifd1.fDefaultCropOriginV.As_real64(), ifd2.fDefaultCropOriginV.As_real64()))
        printf("    DefaultCropOriginV: %.1f %.1f\n", ifd1.fDefaultCropOriginV.As_real64(), ifd2.fDefaultCropOriginV.As_real64());
    if (!AreSame(ifd1.fDefaultCropSizeH.As_real64(), ifd2.fDefaultCropSizeH.As_real64()))
        printf("    DefaultCropSizeH: %.1f %.1f\n", ifd1.fDefaultCropSizeH.As_real64(), ifd2.fDefaultCropSizeH.As_real64());
    if (!AreSame(ifd1.fDefaultCropSizeV.As_real64(), ifd2.fDefaultCropSizeV.As_real64()))
        printf("    DefaultCropSizeV: %.1f %.1f\n", ifd1.fDefaultCropSizeV.As_real64(), ifd2.fDefaultCropSizeV.As_real64());
    if (!AreSame(ifd1.fDefaultScaleH.As_real64(), ifd2.fDefaultScaleH.As_real64()))
        printf("    DefaultScaleH: %.3f %.3f\n", ifd1.fDefaultScaleH.As_real64(), ifd2.fDefaultScaleH.As_real64());
    if (!AreSame(ifd1.fDefaultScaleV.As_real64(), ifd2.fDefaultScaleV.As_real64()))
        printf("    DefaultScaleV: %.3f %.3f\n", ifd1.fDefaultScaleV.As_real64(), ifd2.fDefaultScaleV.As_real64());
    if (0 != memcmp(ifd1.fExtraSamples, ifd2.fExtraSamples, kMaxSamplesPerPixel * sizeof(uint32)))
        printf("    ExtraSamples\n");
    if (ifd1.fExtraSamplesCount != ifd2.fExtraSamplesCount)
        printf("    ExtraSamplesCount: %d %d\n", ifd1.fExtraSamplesCount, ifd2.fExtraSamplesCount);
    if (ifd1.fFillOrder != ifd2.fFillOrder)
        printf("    FillOrder: %d %d\n", ifd1.fFillOrder, ifd2.fFillOrder);
    if (ifd1.fImageLength != ifd2.fImageLength)
        printf("    ImageLength: %d %d\n", ifd1.fImageLength, ifd2.fImageLength);
    if (ifd1.fImageWidth != ifd2.fImageWidth)
        printf("    ImageWidth: %d %d\n", ifd1.fImageWidth, ifd2.fImageWidth);
    if (ifd1.fJPEGInterchangeFormat != ifd2.fJPEGInterchangeFormat)
        printf("    JPEGInterchangeFormat: %lld %lld\n", ifd1.fJPEGInterchangeFormat, ifd2.fJPEGInterchangeFormat);
    if (ifd1.fJPEGInterchangeFormatLength != ifd2.fJPEGInterchangeFormatLength)
        printf("    JPEGInterchangeFormatLength: %d %d\n", ifd1.fJPEGInterchangeFormatLength, ifd2.fJPEGInterchangeFormatLength);
    if (ifd1.fJPEGTablesCount != ifd2.fJPEGTablesCount)
        printf("    JPEGTablesCount: %d %d\n", ifd1.fJPEGTablesCount, ifd2.fJPEGTablesCount);
    //if (ifd1.fJPEGTablesOffset != ifd2.fJPEGTablesOffset)
    //    printf("    JPEGTablesOffset: %lld %lld\n", ifd1.fJPEGTablesOffset, ifd2.fJPEGTablesOffset);
    if (ifd1.fLinearizationTableCount != ifd2.fLinearizationTableCount)
        printf("    LinearizationTableCount: %d %d\n", ifd1.fLinearizationTableCount, ifd2.fLinearizationTableCount);
    //if (ifd1.fLinearizationTableOffset != ifd2.fLinearizationTableOffset)
    //  printf("    LinearizationTableOffset: %d %d\n", ifd1.fLinearizationTableOffset, ifd2.fLinearizationTableOffset);
    if (ifd1.fLinearizationTableType != ifd2.fLinearizationTableType)
        printf("    LinearizationTableType: %d %d\n", ifd1.fLinearizationTableType, ifd2.fLinearizationTableType);
    if (ifd1.fLosslessJPEGBug16 != ifd2.fLosslessJPEGBug16)
        printf("    LosslessJPEGBug16: %d %d\n", ifd1.fLosslessJPEGBug16, ifd2.fLosslessJPEGBug16);
    if (ifd1.fMaskedArea != ifd2.fMaskedArea)
      printf("    MaskedArea\n");
    if (ifd1.fMaskedAreaCount != ifd2.fMaskedAreaCount)
        printf("    MaskedAreaCount: %d %d\n", ifd1.fMaskedAreaCount, ifd2.fMaskedAreaCount);
    if (ifd1.fNewSubFileType != ifd2.fNewSubFileType)
        printf("    NewSubFileType: %d %d\n", ifd1.fNewSubFileType, ifd2.fNewSubFileType);
    if (ifd1.fNextIFD != ifd2.fNextIFD)
        printf("    NextIFD: %lld %lld\n", ifd1.fNextIFD, ifd2.fNextIFD);
    if (ifd1.fOpcodeList1Count != ifd2.fOpcodeList1Count)
        printf("    OpcodeList1Count: %d %d\n", ifd1.fOpcodeList1Count, ifd2.fOpcodeList1Count);
    //if (ifd1.fOpcodeList1Offset != ifd2.fOpcodeList1Offset)
    //    printf("    OpcodeList1Offset\n");
    if (ifd1.fOpcodeList2Count != ifd2.fOpcodeList2Count)
        printf("    OpcodeList2Count: %d %d\n", ifd1.fOpcodeList2Count, ifd2.fOpcodeList2Count);
    //if (ifd1.fOpcodeList2Offset != ifd2.fOpcodeList2Offset)
    //    printf("    OpcodeList2Offset\n");
    if (ifd1.fOpcodeList3Count != ifd2.fOpcodeList3Count)
        printf("    OpcodeList3Count: %d %d\n", ifd1.fOpcodeList3Count, ifd2.fOpcodeList3Count);
    //if (ifd1.fOpcodeList3Offset != ifd2.fOpcodeList3Offset)
    //    printf("    OpcodeList3Offset\n");
    if (ifd1.fOrientation != ifd2.fOrientation)
        printf("    Orientation: %d %d\n", ifd1.fOrientation, ifd2.fOrientation);
    if (ifd1.fOrientationBigEndian != ifd2.fOrientationBigEndian)
        printf("    OrientationBigEndian: %d %d\n", ifd1.fOrientationBigEndian, ifd2.fOrientationBigEndian);
    //if (ifd1.fOrientationOffset != ifd2.fOrientationOffset)
    //    printf("    OrientationOffset: %lld %lld\n", ifd1.fOrientationOffset, ifd2.fOrientationOffset);
    if (ifd1.fOrientationType != ifd2.fOrientationType)
        printf("    OrientationType: %d %d\n", ifd1.fOrientationType, ifd2.fOrientationType);
    if (ifd1.fPhotometricInterpretation != ifd2.fPhotometricInterpretation)
        printf("    PhotometricInterpretation: %d %d\n", ifd1.fPhotometricInterpretation, ifd2.fPhotometricInterpretation);
    if (ifd1.fPlanarConfiguration != ifd2.fPlanarConfiguration)
        printf("    PlanarConfiguration: %d %d\n", ifd1.fPlanarConfiguration, ifd2.fPlanarConfiguration);
    if (ifd1.fPredictor != ifd2.fPredictor)
        printf("    Predictor: %d\n", ifd1.fPredictor, ifd2.fPredictor);
    if (ifd1.fPreviewInfo.fApplicationName != ifd2.fPreviewInfo.fApplicationName)
        printf("    PreviewInfo ApplicationName: '%s' '%s'\n", ifd1.fPreviewInfo.fApplicationName.Get(), ifd2.fPreviewInfo.fApplicationName.Get());
    if (ifd1.fPreviewInfo.fApplicationVersion != ifd2.fPreviewInfo.fApplicationVersion)
        printf("    PreviewInfo ApplicationVersion: '%s' '%s'\n", ifd1.fPreviewInfo.fApplicationVersion.Get(), ifd2.fPreviewInfo.fApplicationVersion.Get());
    if (ifd1.fPreviewInfo.fColorSpace != ifd2.fPreviewInfo.fColorSpace)
        printf("    PreviewInfo ColorSpace\n");
    if (ifd1.fPreviewInfo.fDateTime != ifd2.fPreviewInfo.fDateTime)
        printf("    PreviewInfo DateTime\n");
    if (ifd1.fPreviewInfo.fIsPrimary != ifd2.fPreviewInfo.fIsPrimary)
        printf("    PreviewInfo\n");
    if (ifd1.fPreviewInfo.fSettingsDigest != ifd2.fPreviewInfo.fSettingsDigest)
        printf("    PreviewInfo\n");
    if (ifd1.fPreviewInfo.fSettingsName != ifd2.fPreviewInfo.fSettingsName)
        printf("    PreviewInfo\n");
    if (0 != memcmp(ifd1.fReferenceBlackWhite, ifd2.fReferenceBlackWhite, 6 * sizeof(real64)))
        printf("    ReferenceBlackWhite\n");
    if (ifd1.fResolutionUnit != ifd2.fResolutionUnit)
        printf("    ResolutionUnit: %d %d\n", ifd1.fResolutionUnit, ifd2.fResolutionUnit);
    if (ifd1.fRowInterleaveFactor != ifd2.fRowInterleaveFactor)
        printf("    RowInterleaveFactor: %d %d\n", ifd1.fRowInterleaveFactor, ifd2.fRowInterleaveFactor);
    if (ifd1.fSampleBitShift != ifd2.fSampleBitShift)
        printf("    SampleBitShift: %d %d\n", ifd1.fSampleBitShift, ifd2.fSampleBitShift);
    if (0 != memcmp(ifd1.fSampleFormat, ifd2.fSampleFormat, kMaxSamplesPerPixel * sizeof(uint32)))
        printf("    SampleFormat\n");
    if (ifd1.fSamplesPerPixel != ifd2.fSamplesPerPixel)
        printf("    SamplesPerPixel: %d %d\n", ifd1.fSamplesPerPixel, ifd2.fSamplesPerPixel);
    if (ifd1.fSubIFDsCount != ifd2.fSubIFDsCount)
        printf("    SubIFDsCount: %d %d\n", ifd1.fSubIFDsCount, ifd2.fSubIFDsCount);
    //if (ifd1.fSubIFDsOffset != ifd2.fSubIFDsOffset)
    //    printf("    SubIFDsOffset\n");
    if (ifd1.fSubTileBlockCols != ifd2.fSubTileBlockCols)
        printf("    SubTileBlockCols: %d %d\n", ifd1.fSubTileBlockCols, ifd2.fSubTileBlockCols);
    if (ifd1.fSubTileBlockRows != ifd2.fSubTileBlockRows)
        printf("    SubTileBlockRows: %d %d\n", ifd1.fSubTileBlockRows, ifd2.fSubTileBlockRows);
    //if (ifd1.fThisIFD != ifd2.fThisIFD)
    //  printf("    ThisIFD:\n");
    if (0 != memcmp(ifd1.fTileByteCount, ifd2.fTileByteCount, dng_ifd::kMaxTileInfo * sizeof(uint32)))
        printf("    TileByteCount\n");
    if (ifd1.fTileByteCountsCount != ifd2.fTileByteCountsCount)
        printf("    TileByteCountsCount: %d %d\n", ifd1.fTileByteCountsCount, ifd2.fTileByteCountsCount);
    //if (ifd1.fTileByteCountsOffset != ifd2.fTileByteCountsOffset)
    //  printf("    TileByteCountsOffset\n");
    if (ifd1.fTileByteCountsType != ifd2.fTileByteCountsType)
        printf("    TileByteCountsType: %d %d\n", ifd1.fTileByteCountsType, ifd2.fTileByteCountsType);
    if (ifd1.fTileLength != ifd2.fTileLength)
        printf("    TileLength: %d %d\n", ifd1.fTileLength, ifd2.fTileLength);
    //if (ifd1.fTileOffset != ifd2.fTileOffset)
    //  printf("    TileOffset\n");
    if (ifd1.fTileOffsetsCount != ifd2.fTileOffsetsCount)
        printf("    TileOffsetsCount: %d %d\n", ifd1.fTileOffsetsCount, ifd2.fTileOffsetsCount);
    //if (ifd1.fTileOffsetsOffset != ifd2.fTileOffsetsOffset)
    //  printf("    TileOffsetsOffset\n");
    if (ifd1.fTileOffsetsType != ifd2.fTileOffsetsType)
        printf("    TileOffsetsType: %d %d\n", ifd1.fTileOffsetsType, ifd2.fTileOffsetsType);
    if (ifd1.fTileWidth != ifd2.fTileWidth)
        printf("    TileWidth: %d %d\n", ifd1.fTileWidth, ifd2.fTileWidth);
    if (ifd1.fUsesNewSubFileType != ifd2.fUsesNewSubFileType)
        printf("    UsesNewSubFileType: %d %d\n", ifd1.fUsesNewSubFileType, ifd2.fUsesNewSubFileType);
    if (ifd1.fUsesStrips != ifd2.fUsesStrips)
        printf("    UsesStrips: %d %d\n", ifd1.fUsesStrips, ifd2.fUsesStrips);
    if (ifd1.fUsesTiles != ifd2.fUsesTiles)
        printf("    UsesTiles: %d %d\n", ifd1.fUsesTiles, ifd2.fUsesTiles);
    if (0 != memcmp(ifd1.fWhiteLevel, ifd2.fWhiteLevel, kMaxSamplesPerPixel * sizeof(real64)))
        printf("    WhiteLevel\n");
    if (!AreSame(ifd1.fXResolution, ifd2.fXResolution))
        printf("    XResolution: %.1f %.1f\n", ifd1.fXResolution, ifd2.fXResolution);
    if (!AreSame(ifd1.fYCbCrCoefficientB, ifd2.fYCbCrCoefficientB))
        printf("    YCbCrCoefficientB: %.1f %.1f\n", ifd1.fYCbCrCoefficientB, ifd2.fYCbCrCoefficientB);
    if (!AreSame(ifd1.fYCbCrCoefficientG, ifd2.fYCbCrCoefficientG))
        printf("    YCbCrCoefficientG: %.1f %.1f\n", ifd1.fYCbCrCoefficientG, ifd2.fYCbCrCoefficientG);
    if (!AreSame(ifd1.fYCbCrCoefficientR, ifd2.fYCbCrCoefficientR))
        printf("    YCbCrCoefficientR: %.1f %.f1\n", ifd1.fYCbCrCoefficientR, ifd2.fYCbCrCoefficientR);
    if (ifd1.fYCbCrPositioning != ifd2.fYCbCrPositioning)
        printf("    YCbCrPositioning: %d %d\n", ifd1.fYCbCrPositioning, ifd2.fYCbCrPositioning);
    if (ifd1.fYCbCrSubSampleH != ifd2.fYCbCrSubSampleH)
        printf("    YCbCrSubSampleH: %d %d\n", ifd1.fYCbCrSubSampleH, ifd2.fYCbCrSubSampleH);
    if (ifd1.fYCbCrSubSampleV != ifd2.fYCbCrSubSampleV)
        printf("    YCbCrSubSampleV: %d %d\n", ifd1.fYCbCrSubSampleV, ifd2.fYCbCrSubSampleV);
    if (!AreSame(ifd1.fYResolution, ifd2.fYResolution))
        printf("    YResolution: %.1f %.1f\n", ifd1.fYResolution, ifd2.fYResolution);
}

void compareNegative(const dng_negative& negative1, const dng_negative& negative2)
{
    if (negative1.AsShotProfileName() != negative2.AsShotProfileName())
        printf("    AsShotProfileName: '%s' '%s'\n", negative1.AsShotProfileName().Get(), negative2.AsShotProfileName().Get());
    if (!AreSame(negative1.AntiAliasStrength().As_real64(), negative2.AntiAliasStrength().As_real64()))
        printf("    BaselineExposure: %.1f %.1f\n", negative1.AntiAliasStrength().As_real64(), negative2.AntiAliasStrength().As_real64());
    if (negative1.AspectRatio() != negative2.AspectRatio())
        printf("    AspectRatio: %.3f %.3f\n", negative1.AspectRatio(), negative2.AspectRatio());
    if (negative1.BaselineExposure() != negative2.BaselineExposure())
        printf("    BaselineExposure: %.1f %.1f\n", negative1.BaselineExposure(), negative2.BaselineExposure());
    if (negative1.BaselineNoise() != negative2.BaselineNoise())
        printf("    BaselineNoise: %.1f %.1f\n", negative1.BaselineNoise(), negative2.BaselineNoise());
    if (negative1.BaselineSharpness() != negative2.BaselineSharpness())
        printf("    BaselineSharpness: %.1f %.1f\n", negative1.BaselineSharpness(), negative2.BaselineSharpness());
    if (negative1.BaseOrientation() != negative2.BaseOrientation())
        printf("    BaseOrientation: %d %d\n", negative1.BaseOrientation().GetAdobe(), negative2.BaseOrientation().GetAdobe());
    //if (negative1.BestQualityFinalHeight() != negative2.BestQualityFinalHeight())
    //  printf("    BestQualityFinalHeight: %d %d\n", negative1.BestQualityFinalHeight(), negative2.BestQualityFinalHeight());
    //if (negative1.BestQualityFinalWidth() != negative2.BestQualityFinalWidth())
    //  printf("    BestQualityFinalWidth: %d %d\n", negative1.BestQualityFinalWidth(), negative2.BestQualityFinalWidth());
    if (!AreSame(negative1.BestQualityScale().As_real64(), negative2.BestQualityScale().As_real64()))
        printf("    BestQualityScale\n");
    if (negative1.HasCameraNeutral() &&
            negative2.HasCameraNeutral())
    {
        if (negative1.CameraNeutral() != negative2.CameraNeutral())
            printf("    CameraNeutral\n");
    }
    if (negative1.HasCameraWhiteXY() &&
            negative2.HasCameraWhiteXY())
    {
        if (negative1.CameraWhiteXY() != negative2.CameraWhiteXY())
            printf("    CameraWhiteXY\n");
    }
    if (!AreSame(negative1.ChromaBlurRadius().As_real64(), negative2.ChromaBlurRadius().As_real64()))
        printf("    ChromaBlurRadius: %.1f %.1f\n", negative1.ChromaBlurRadius().As_real64(), negative2.ChromaBlurRadius().As_real64());
    if (negative1.ColorChannels() != negative2.ColorChannels())
        printf("    ColorChannels: %d %d\n", negative1.ColorChannels(), negative2.ColorChannels());
    if (negative1.ColorimetricReference() != negative2.ColorimetricReference())
        printf("    ColorimetricReference: %d %d\n", negative1.ColorimetricReference(), negative2.ColorimetricReference());
    //if (!AreSame(negative1.DefaultCropOriginH().As_real64(), negative2.DefaultCropOriginH().As_real64()))
    //  printf("    DefaultCropOriginH: %.1f %.1f\n", negative1.DefaultCropOriginH().As_real64(), negative2.DefaultCropOriginH().As_real64());
    //if (!AreSame(negative1.DefaultCropOriginV().As_real64(), negative2.DefaultCropOriginV().As_real64()))
    //  printf("    DefaultCropOriginV: %.1f %.1f\n", negative1.DefaultCropOriginV().As_real64(), negative2.DefaultCropOriginV().As_real64());
    //if (!AreSame(negative1.DefaultCropSizeH().As_real64(), negative2.DefaultCropSizeH().As_real64()))
    //  printf("    DefaultCropSizeH: %.1f %.1f\n", negative1.DefaultCropSizeH().As_real64(), negative2.DefaultCropSizeH().As_real64());
    //if (!AreSame(negative1.DefaultCropSizeV().As_real64(), negative2.DefaultCropSizeV().As_real64()))
    //  printf("    DefaultCropSizeV: %.1f %.1f\n", negative1.DefaultCropSizeV().As_real64(), negative2.DefaultCropSizeV().As_real64());
    if (negative1.DefaultScale() != negative2.DefaultScale())
        printf("    DefaultScale: %.3f %.3f\n", negative1.DefaultScale(), negative2.DefaultScale());
    if (!AreSame(negative1.DefaultScaleH().As_real64(), negative2.DefaultScaleH().As_real64()))
        printf("    DefaultScaleH: %.3f %.3f\n", negative1.DefaultScaleH().As_real64(), negative2.DefaultScaleH().As_real64());
    if (!AreSame(negative1.DefaultScaleV().As_real64(), negative2.DefaultScaleV().As_real64()))
        printf("    DefaultScaleV: %.3f %.3f\n", negative1.DefaultScaleV().As_real64(), negative2.DefaultScaleV().As_real64());
    //if (negative1.DefaultFinalHeight() != negative2.DefaultFinalHeight())
    //  printf("    DefaultFinalHeight: %d %d\n", negative1.DefaultFinalHeight(), negative2.DefaultFinalHeight());
    //if (negative1.DefaultFinalWidth() != negative2.DefaultFinalWidth())
    //  printf("    DefaultFinalWidth: %d %d\n", negative1.DefaultFinalWidth(), negative2.DefaultFinalWidth());
    if (negative1.HasBaseOrientation() != negative2.HasBaseOrientation())
        printf("    HasBaseOrientation: %d %d\n", negative1.HasBaseOrientation(), negative2.HasBaseOrientation());
    if (negative1.HasCameraNeutral() != negative2.HasCameraNeutral())
        printf("    HasCameraNeutral: %d %d\n", negative1.HasCameraNeutral(), negative2.HasCameraNeutral());
    if (negative1.HasCameraWhiteXY() != negative2.HasCameraWhiteXY())
        printf("    HasCameraWhiteXY: %d %d\n", negative1.HasCameraWhiteXY(), negative2.HasCameraWhiteXY());
    if (negative1.HasNoiseProfile() != negative2.HasNoiseProfile())
        printf("    HasNoiseProfile: %d %d\n", negative1.HasNoiseProfile(), negative2.HasNoiseProfile());
    if (negative1.HasOriginalRawFileName() != negative2.HasOriginalRawFileName())
        printf("    HasOriginalRawFileName: %d %d\n", negative1.HasOriginalRawFileName(), negative2.HasOriginalRawFileName());
    if (negative1.IsMakerNoteSafe() != negative2.IsMakerNoteSafe())
        printf("    IsMakerNoteSafe: %d %d\n", negative1.IsMakerNoteSafe(), negative2.IsMakerNoteSafe());
    if (negative1.IsMonochrome() != negative2.IsMonochrome())
        printf("    IsMonochrome: %d %d\n", negative1.IsMonochrome(), negative2.IsMonochrome());
    if (negative1.LinearResponseLimit() != negative2.LinearResponseLimit())
        printf("    LinearResponseLimit: %.1f %.1f\n", negative1.LinearResponseLimit(), negative2.LinearResponseLimit());
    if (negative1.LocalName() != negative2.LocalName())
        printf("    LocalName: '%s' '%s'\n", negative1.LocalName().Get(), negative2.LocalName().Get());
    if (negative1.MakerNoteLength() != negative2.MakerNoteLength())
        printf("    MakerNoteLength: %d %d\n", negative1.MakerNoteLength(), negative2.MakerNoteLength());
    if (negative1.ModelName() != negative2.ModelName())
        printf("    ModelName: '%s' '%s'\n", negative1.ModelName().Get(), negative2.ModelName().Get());
    if (!AreSame(negative1.NoiseReductionApplied().As_real64(), negative2.NoiseReductionApplied().As_real64()))
        printf("    NoiseReductionApplied: %.1f %.1f\n", negative1.NoiseReductionApplied().As_real64(), negative2.NoiseReductionApplied().As_real64());
    //if (negative1.Orientation() != negative2.Orientation())
    //    printf("    Orientation: %d %d\n", negative1.Orientation(), negative2.Orientation());
    if (negative1.OriginalRawFileDataLength() != negative2.OriginalRawFileDataLength())
        printf("    OriginalRawFileDataLength: %d %d\n", negative1.OriginalRawFileDataLength(), negative2.OriginalRawFileDataLength());
    if (0 != memcmp(negative1.OriginalRawFileDigest().data, negative2.OriginalRawFileDigest().data, 16 * sizeof(uint8)))
        printf("    OriginalRawFileDigest\n");
    if (negative1.OriginalRawFileName() != negative2.OriginalRawFileName())
        printf("    OriginalRawFileName: '%s' '%s'\n", negative1.OriginalRawFileName().Get(), negative2.OriginalRawFileName().Get());
    if (negative1.PixelAspectRatio() != negative2.PixelAspectRatio())
        printf("    PixelAspectRatio: %.3f %.3f\n", negative1.PixelAspectRatio(), negative2.PixelAspectRatio());
    if (negative1.PrivateLength() != negative2.PrivateLength())
        printf("    PrivateLength: %d %d\n", negative1.PrivateLength(), negative2.PrivateLength());
    if (negative1.ProfileCount() != negative2.ProfileCount())
        printf("    ProfileCount: %d %d\n", negative1.ProfileCount(), negative2.ProfileCount());
    if (negative1.RawDataUniqueID().Collapse32() != negative2.RawDataUniqueID().Collapse32())
        printf("    RawDataUniqueID\n");
    if (negative1.RawImageDigest().Collapse32() != negative2.RawImageDigest().Collapse32())
        printf("    RawImageDigest\n");
    if (negative1.RawToFullScaleH() != negative2.RawToFullScaleH())
        printf("    RawToFullScaleH: %.3f %.3f\n", negative1.RawToFullScaleH(), negative2.RawToFullScaleH());
    if (negative1.RawToFullScaleV() != negative2.RawToFullScaleV())
        printf("    RawToFullScaleV: %.3f %.3f\n", negative1.RawToFullScaleV(), negative2.RawToFullScaleV());
    if (negative1.ShadowScale() != negative2.ShadowScale())
        printf("    ShadowScale: %.1f %.1f\n", negative1.ShadowScale(), negative2.ShadowScale());
    //if (negative1.SquareHeight() != negative2.SquareHeight())
    //  printf("    SquareHeight: %.1f %.1f\n", negative1.SquareHeight(), negative2.SquareHeight());
    //if (negative1.SquareWidth() != negative2.SquareWidth())
    //  printf("    SquareWidth: %.1f %.1f\n", negative1.SquareWidth(), negative2.SquareWidth());

    const dng_mosaic_info* mosaicInfo1 = negative1.GetMosaicInfo();
    const dng_mosaic_info* mosaicInfo2 = negative2.GetMosaicInfo();

    if (mosaicInfo1->fBayerGreenSplit != mosaicInfo2->fBayerGreenSplit)
        printf("    BayerGreenSplit: %d %d\n", mosaicInfo1->fBayerGreenSplit, mosaicInfo2->fBayerGreenSplit);
    if (mosaicInfo1->fCFALayout != mosaicInfo2->fCFALayout)
        printf("    CFALayout: %d %d\n", mosaicInfo1->fCFALayout, mosaicInfo2->fCFALayout);
    if (0 != memcmp(mosaicInfo1->fCFAPattern, mosaicInfo2->fCFAPattern, kMaxCFAPattern * kMaxCFAPattern * sizeof(uint8)))
        printf("    CFAPattern\n");
    if (mosaicInfo1->fCFAPatternSize != mosaicInfo2->fCFAPatternSize)
        printf("    CFAPatternSize: %d,%d %d,%d\n", mosaicInfo1->fCFAPatternSize.h, mosaicInfo1->fCFAPatternSize.v, mosaicInfo2->fCFAPatternSize.h, mosaicInfo2->fCFAPatternSize.v);
    if (0 != memcmp(mosaicInfo1->fCFAPlaneColor, mosaicInfo2->fCFAPlaneColor, kMaxColorPlanes * sizeof(uint8)))
        printf("    CFAPlaneColor\n");
    if (mosaicInfo1->fColorPlanes != mosaicInfo2->fColorPlanes)
        printf("    ColorPlanes: %d %d\n", mosaicInfo1->fColorPlanes, mosaicInfo2->fColorPlanes);
}

int main(int argc, const char* argv [])
{
    if(argc == 1)
    {
        fprintf(stderr,
                "\n"
                "dngcompare - DNG comparsion tool\n"
                "Usage: %s [options] <dngfile1> <dngfile2>\n",
                argv[0]);

        return -1;
    }

    const char* fileName1 = argv[1];
    const char* fileName2 = argv[2];

    dng_xmp_sdk::InitializeSDK();

    dng_file_stream stream1(fileName1);
    DngHost host1;
    host1.SetKeepOriginalFile(true);

    dng_info info1;
    AutoPtr<dng_negative> negative1;
    {
        info1.Parse(host1, stream1);
        info1.PostParse(host1);

        if (!info1.IsValidDNG())
        {
            return dng_error_bad_format;
        }

        negative1.Reset(host1.Make_dng_negative());
        negative1->Parse(host1, stream1, info1);
        negative1->PostParse(host1, stream1, info1);
    }

    dng_file_stream stream2(fileName2);
    DngHost host2;
    host2.SetKeepOriginalFile(true);

    dng_info info2;
    AutoPtr<dng_negative> negative2;
    {
        info2.Parse(host2, stream2);
        info2.PostParse(host2);

        if (!info2.IsValidDNG())
        {
            return dng_error_bad_format;
        }

        negative2.Reset(host2.Make_dng_negative());
        negative2->Parse(host2, stream2, info2);
        negative2->PostParse(host2, stream2, info2);
    }

    negative1->SynchronizeMetadata();
    negative2->SynchronizeMetadata();

    const dng_exif* exif1 = negative1->GetExif();
    const dng_exif* exif2 = negative2->GetExif();

    printf(" Exif\n");
    compareExif(*exif1, *exif2);

    printf(" Main Ifd\n");
    compareIfd(*info1.fIFD[info1.fMainIndex].Get(), *info2.fIFD[info1.fMainIndex].Get());

    printf(" Negative\n");
    compareNegative(*negative1, *negative2);

    dng_xmp_sdk::TerminateSDK();

    return 0;
}

