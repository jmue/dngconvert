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
   
   This file uses code from dngwriter.cpp -- KDE Kipi-plugins dngconverter utility 
   (https://projects.kde.org/projects/extragear/graphics/kipi-plugins) utility,
   dngwriter.cpp is Copyright 2008-2010 by Gilles Caulier <caulier dot gilles at gmail dot com> 
   and Jens Mueller <tschenser at gmx dot de>
*/

#include "stdio.h"
#include "string.h"
#include "assert.h"

#include "dng_camera_profile.h"
#include "dng_color_space.h"
#include "dng_exceptions.h"
#include "dng_file_stream.h"
#include "dng_globals.h"
#include "dng_host.h"
#include "dng_ifd.h"
#include "dng_image_writer.h"
#include "dng_info.h"
#include "dng_linearization_info.h"
#include "dng_memory_stream.h"
#include "dng_mosaic_info.h"
#include "dng_negative.h"
#include "dng_preview.h"
#include "dng_read_image.h"
#include "dng_render.h"
#include "dng_simple_image.h"
#include "dng_tag_codes.h"
#include "dng_tag_types.h"
#include "dng_tag_values.h"
#include "dng_xmp.h"
#include "dng_xmp_sdk.h"

#include "zlib.h"
#define CHUNK 65536

#include "rawhelper.h"
#include "exiv2helper.h"
#include "librawimage.h"

#include "dnghost.h"
#include "dngimagewriter.h"

#if qWinOS
	#ifndef snprintf
		#define snprintf _snprintf
	#endif
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))

int main(int argc, const char* argv [])
{
    if(argc == 1)
    {
        fprintf(stderr,
                "\n"
                "dngconvert - DNG convertion tool\n"
                "Usage: %s [options] <dngfile>\n"
                "Valid options:\n"
                "  -e                embed original\n"
                "  -p <filename>     use camera profile\n"
                "  -x <filename>     read EXIF from this file\n",
                argv[0]);

        return -1;
    }

    //parse options
    int index;
    const char* outfilename = NULL;
    const char* profilefilename = NULL;
    const char* exiffilename = NULL;
    bool embedOriginal = false;

    for (index = 1; index < argc && argv [index][0] == '-'; index++)
    {
        std::string option = &argv[index][1];

        if (0 == strcmp(option.c_str(), "o"))
        {
            outfilename = argv[++index];
        }

        if (0 == strcmp(option.c_str(), "p"))
        {
            profilefilename = argv[++index];
        }

        if (0 == strcmp(option.c_str(), "e"))
        {
            embedOriginal = true;
        }
        
        if (0 == strcmp(option.c_str(), "x"))
        {
            exiffilename = argv[++index];
        }
    }

    if (index == argc)
    {
        fprintf (stderr, "no file specified\n");
        return 1;
    }

    const char* filename = argv[index++];

    RawHelper rawProcessor;
    libraw_data_t imgdata;
    int ret = rawProcessor.identifyRawData(filename, &imgdata);
    if(ret != 0)
    {
        printf("can not extract raw data");
        return 1;
    }

    dng_memory_allocator memalloc(gDefaultDNGMemoryAllocator);

    DngHost host(&memalloc);

    host.SetSaveDNGVersion(dngVersion_SaveDefault);
    host.SetSaveLinearDNG(false);
    host.SetKeepOriginalFile(true);

    AutoPtr<dng_image> image(new LibRawImage(filename, memalloc));
    LibRawImage* rawImage = static_cast<LibRawImage*>(image.Get());

    // -----------------------------------------------------------------------------------------

    AutoPtr<dng_negative> negative(host.Make_dng_negative());

    negative->SetDefaultScale(dng_urational(rawImage->FinalSize().W(), rawImage->ActiveArea().W()),
                              dng_urational(rawImage->FinalSize().H(), rawImage->ActiveArea().H()));
    if (imgdata.idata.filters != 0)
    {
        negative->SetDefaultCropOrigin(8, 8);
        negative->SetDefaultCropSize(rawImage->ActiveArea().W() - 16, rawImage->ActiveArea().H() - 16);
    }
    else
    {
        negative->SetDefaultCropOrigin(0, 0);
        negative->SetDefaultCropSize(rawImage->ActiveArea().W(), rawImage->ActiveArea().H());
    }
    negative->SetActiveArea(rawImage->ActiveArea());

    std::string file(filename);
    size_t found = std::min(file.rfind("\\"), file.rfind("/"));
    if (found != std::string::npos)
        file = file.substr(found + 1, file.length() - found - 1);
    negative->SetOriginalRawFileName(file.c_str());

    negative->SetColorChannels(rawImage->Channels());

    ColorKeyCode colorCodes[4] = {colorKeyMaxEnum, colorKeyMaxEnum, colorKeyMaxEnum, colorKeyMaxEnum};
    for(uint32 i = 0; i < 4; i++)
    {
        switch(imgdata.idata.cdesc[i])
        {
        case 'R':
            colorCodes[i] = colorKeyRed;
            break;
        case 'G':
            colorCodes[i] = colorKeyGreen;
            break;
        case 'B':
            colorCodes[i] = colorKeyBlue;
            break;
        case 'C':
            colorCodes[i] = colorKeyCyan;
            break;
        case 'M':
            colorCodes[i] = colorKeyMagenta;
            break;
        case 'Y':
            colorCodes[i] = colorKeyYellow;
            break;
        }
    }

    negative->SetColorKeys(colorCodes[0], colorCodes[1], colorCodes[2], colorCodes[3]);

    if (rawImage->Channels() == 4)
    {
        negative->SetQuadMosaic(imgdata.idata.filters);
    }
    else if (0 == memcmp("FUJIFILM", rawImage->MakeName().Get(), MIN(8, sizeof(rawImage->MakeName().Get()))))
    {
        negative->SetFujiMosaic(0);
    }
    else
    {
        uint32 bayerMosaic = 0xFFFFFFFF;
        switch(imgdata.idata.filters)
        {
        case 0xe1e1e1e1:
            bayerMosaic = 0;
            break;
        case 0xb4b4b4b4:
            bayerMosaic = 1;
            break;
        case 0x1e1e1e1e:
            bayerMosaic = 2;
            break;
        case 0x4b4b4b4b:
            bayerMosaic = 3;
            break;
        }
        if (bayerMosaic != 0xFFFFFFFF)
            negative->SetBayerMosaic(bayerMosaic);
    }

    negative->SetWhiteLevel(rawImage->WhiteLevel(0), 0);
    negative->SetWhiteLevel(rawImage->WhiteLevel(1), 1);
    negative->SetWhiteLevel(rawImage->WhiteLevel(2), 2);
    negative->SetWhiteLevel(rawImage->WhiteLevel(3), 3);

    const dng_mosaic_info* mosaicinfo = negative->GetMosaicInfo();
    if ((mosaicinfo != NULL) && (mosaicinfo->fCFAPatternSize == dng_point(2, 2)))
    {
        negative->SetQuadBlacks(rawImage->BlackLevel(0),
                                rawImage->BlackLevel(1),
                                rawImage->BlackLevel(2),
                                rawImage->BlackLevel(3));
    }
    else
    {
        negative->SetBlackLevel(rawImage->BlackLevel(0), 0);
    }

    negative->SetBaselineExposure(0.0);
    negative->SetBaselineNoise(1.0);
    negative->SetBaselineSharpness(1.0);

    dng_orientation orientation;
    switch (imgdata.sizes.flip)
    {
    case 3:
        orientation = dng_orientation::Rotate180();
        break;

    case 5:
        orientation = dng_orientation::Rotate90CCW();
        break;

    case 6:
        orientation = dng_orientation::Rotate90CW();
        break;

    default:
        orientation = dng_orientation::Normal();
        break;
    }
    negative->SetBaseOrientation(orientation);

    negative->SetAntiAliasStrength(dng_urational(100, 100));
    negative->SetLinearResponseLimit(1.0);
    negative->SetShadowScale(dng_urational(1, 1));

    negative->SetAnalogBalance(dng_vector_3(1.0, 1.0, 1.0));

    // -------------------------------------------------------------------------------

    AutoPtr<dng_camera_profile> prof(new dng_camera_profile);
    if (profilefilename != NULL)
    {
        dng_file_stream profStream(profilefilename);
        prof->ParseExtended(profStream);
    }
    else
    {
        char* lpszProfName = new char[255];
        strcpy(lpszProfName, rawImage->MakeName().Get());
        strcat(lpszProfName, " ");
        strcat(lpszProfName, rawImage->ModelName().Get());

        prof->SetName(lpszProfName);
        delete lpszProfName;

        prof->SetColorMatrix1((dng_matrix) rawImage->ColorMatrix());
        prof->SetCalibrationIlluminant1(lsD65);
    }

    negative->AddProfile(prof);

    negative->SetCameraNeutral(rawImage->CameraNeutral());

    // -----------------------------------------------------------------------------------------

    dng_exif *exif = negative->GetExif();
    exif->fModel = rawImage->ModelName();
    exif->fMake = rawImage->MakeName();

    long int num, den;
    long     val;
    std::string str;
    Exiv2Helper meta;
    if (exiffilename == NULL)
        // read exif from raw file
        exiffilename = filename;
    // '-x -' disables exif reading
    if (strcmp(exiffilename, "-") != 0 && meta.load(exiffilename))
    {
        // Time from original shot

        str = meta.getExifTagString("Exif.Photo.DateTimeOriginal");
        if (str.length())
        {
            dng_date_time dt;
            dt.Parse(str.c_str());
            exif->fDateTimeOriginal.SetDateTime(dt);
        }

        str = meta.getExifTagString("Exif.Photo.DateTimeDigitized");
        if (str.length())
        {
            dng_date_time dt;
            dt.Parse(str.c_str());
            exif->fDateTimeDigitized.SetDateTime(dt);
        }

        // CFA Pattern

        if (mosaicinfo != NULL)
        {
            exif->fCFARepeatPatternCols = mosaicinfo->fCFAPatternSize.v;
            exif->fCFARepeatPatternRows = mosaicinfo->fCFAPatternSize.h;
            for (uint16 c = 0; c < exif->fCFARepeatPatternCols; c++)
            {
                for (uint16 r = 0; r < exif->fCFARepeatPatternRows; r++)
                {
                    exif->fCFAPattern[r][c] = mosaicinfo->fCFAPattern[c][r];
                }
            }
        }

        // String Tags

        str = meta.getExifTagString("Exif.Image.Make");
        if (str.length())
        {
            exif->fMake.Set_ASCII(str.c_str());
            exif->fMake.TrimLeadingBlanks();
            exif->fMake.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Image.Model");
        if (str.length())
        {
            exif->fModel.Set_ASCII(str.c_str());
            exif->fModel.TrimLeadingBlanks();
            exif->fModel.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Image.Software");
        if (str.length())
        {
            exif->fSoftware.Set_ASCII(str.c_str());
            exif->fSoftware.TrimLeadingBlanks();
            exif->fSoftware.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Image.ImageDescription");
        if (str.length())
        {
            exif->fImageDescription.Set_ASCII(str.c_str());
            exif->fImageDescription.TrimLeadingBlanks();
            exif->fImageDescription.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Image.Artist");
        if (str.length())
        {
            exif->fArtist.Set_ASCII(str.c_str());
            exif->fArtist.TrimLeadingBlanks();
            exif->fArtist.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Image.Copyright");
        if (str.length())
        {
            exif->fCopyright.Set_ASCII(str.c_str());
            exif->fCopyright.TrimLeadingBlanks();
            exif->fCopyright.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Photo.UserComment");
        if (str.length())
        {
            exif->fUserComment.Set_ASCII(str.c_str());
            exif->fUserComment.TrimLeadingBlanks();
            exif->fUserComment.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Image.CameraSerialNumber");
        if (str.length())
        {
            exif->fCameraSerialNumber.Set_ASCII(str.c_str());
            exif->fCameraSerialNumber.TrimLeadingBlanks();
            exif->fCameraSerialNumber.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSLatitudeRef");
        if (str.length())
        {
            exif->fGPSLatitudeRef.Set_ASCII(str.c_str());
            exif->fGPSLatitudeRef.TrimLeadingBlanks();
            exif->fGPSLatitudeRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSLongitudeRef");
        if (str.length())
        {
            exif->fGPSLongitudeRef.Set_ASCII(str.c_str());
            exif->fGPSLongitudeRef.TrimLeadingBlanks();
            exif->fGPSLongitudeRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSSatellites");
        if (str.length())
        {
            exif->fGPSSatellites.Set_ASCII(str.c_str());
            exif->fGPSSatellites.TrimLeadingBlanks();
            exif->fGPSSatellites.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSStatus");
        if (str.length())
        {
            exif->fGPSStatus.Set_ASCII(str.c_str());
            exif->fGPSStatus.TrimLeadingBlanks();
            exif->fGPSStatus.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSMeasureMode");
        if (str.length())
        {
            exif->fGPSMeasureMode.Set_ASCII(str.c_str());
            exif->fGPSMeasureMode.TrimLeadingBlanks();
            exif->fGPSMeasureMode.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSSpeedRef");
        if (str.length())
        {
            exif->fGPSSpeedRef.Set_ASCII(str.c_str());
            exif->fGPSSpeedRef.TrimLeadingBlanks();
            exif->fGPSSpeedRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSTrackRef");
        if (str.length())
        {
            exif->fGPSTrackRef.Set_ASCII(str.c_str());
            exif->fGPSTrackRef.TrimLeadingBlanks();
            exif->fGPSTrackRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSSpeedRef");
        if (str.length())
        {
            exif->fGPSSpeedRef.Set_ASCII(str.c_str());
            exif->fGPSSpeedRef.TrimLeadingBlanks();
            exif->fGPSSpeedRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSImgDirectionRef");
        if (str.length())
        {
            exif->fGPSSpeedRef.Set_ASCII(str.c_str());
            exif->fGPSSpeedRef.TrimLeadingBlanks();
            exif->fGPSSpeedRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSMapDatum");
        if (str.length())
        {
            exif->fGPSMapDatum.Set_ASCII(str.c_str());
            exif->fGPSMapDatum.TrimLeadingBlanks();
            exif->fGPSMapDatum.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSDestLatitudeRef");
        if (str.length())
        {
            exif->fGPSDestLatitudeRef.Set_ASCII(str.c_str());
            exif->fGPSDestLatitudeRef.TrimLeadingBlanks();
            exif->fGPSDestLatitudeRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSDestLongitudeRef");
        if (str.length())
        {
            exif->fGPSDestLongitudeRef.Set_ASCII(str.c_str());
            exif->fGPSDestLongitudeRef.TrimLeadingBlanks();
            exif->fGPSDestLongitudeRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSDestBearingRef");
        if (str.length())
        {
            exif->fGPSDestBearingRef.Set_ASCII(str.c_str());
            exif->fGPSDestBearingRef.TrimLeadingBlanks();
            exif->fGPSDestBearingRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSDestDistanceRef");
        if (str.length())
        {
            exif->fGPSDestDistanceRef.Set_ASCII(str.c_str());
            exif->fGPSDestDistanceRef.TrimLeadingBlanks();
            exif->fGPSDestDistanceRef.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSProcessingMethod");
        if (str.length())
        {
            exif->fGPSProcessingMethod.Set_ASCII(str.c_str());
            exif->fGPSProcessingMethod.TrimLeadingBlanks();
            exif->fGPSProcessingMethod.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSAreaInformation");
        if (str.length())
        {
            exif->fGPSAreaInformation.Set_ASCII(str.c_str());
            exif->fGPSAreaInformation.TrimLeadingBlanks();
            exif->fGPSAreaInformation.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.GPSInfo.GPSDateStamp");
        if (str.length())
        {
            exif->fGPSDateStamp.Set_ASCII(str.c_str());
            exif->fGPSDateStamp.TrimLeadingBlanks();
            exif->fGPSDateStamp.TrimTrailingBlanks();
        }

        // Rational Tags

        if (meta.getExifTagRational("Exif.Photo.ExposureTime", num, den))
            exif->fExposureTime = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.FNumber", num, den))
            exif->fFNumber = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.ShutterSpeedValue", num, den))
            exif->fShutterSpeedValue = dng_srational(num, den);
        if (meta.getExifTagRational("Exif.Photo.ApertureValue", num, den))
            exif->fApertureValue = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.BrightnessValue", num, den))
            exif->fBrightnessValue = dng_srational(num, den);
        if (meta.getExifTagRational("Exif.Photo.ExposureBiasValue", num, den))
            exif->fExposureBiasValue = dng_srational(num, den);
        if (meta.getExifTagRational("Exif.Photo.MaxApertureValue", num, den))
            exif->fMaxApertureValue = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.FocalLength", num, den))
            exif->fFocalLength = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.DigitalZoomRatio", num, den))
            exif->fDigitalZoomRatio = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.SubjectDistance", num, den))
            exif->fSubjectDistance = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Image.BatteryLevel", num, den))
            exif->fBatteryLevelR = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.FocalPlaneXResolution", num, den))
            exif->fFocalPlaneXResolution = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Photo.FocalPlaneYResolution", num, den))
            exif->fFocalPlaneYResolution = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSAltitude", num, den))
            exif->fGPSAltitude = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDOP", num, den))
            exif->fGPSDOP = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSSpeed", num, den))
            exif->fGPSSpeed = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSTrack", num, den))
            exif->fGPSTrack = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSImgDirection", num, den))
            exif->fGPSImgDirection = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestBearing", num, den))
            exif->fGPSDestBearing = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestDistance", num, den))
            exif->fGPSDestDistance = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSLatitude", num, den, 0))
            exif->fGPSLatitude[0] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSLatitude", num, den, 1))
            exif->fGPSLatitude[1] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSLatitude", num, den, 2))
            exif->fGPSLatitude[2] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSLongitude", num, den, 0))
            exif->fGPSLongitude[0] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSLongitude", num, den, 1))
            exif->fGPSLongitude[1] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSLongitude", num, den, 2))
            exif->fGPSLongitude[2] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSTimeStamp", num, den, 0))
            exif->fGPSTimeStamp[0] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSTimeStamp", num, den, 1))
            exif->fGPSTimeStamp[1] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSTimeStamp", num, den, 2))
            exif->fGPSTimeStamp[2] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestLatitude", num, den, 0))
            exif->fGPSDestLatitude[0] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestLatitude", num, den, 1))
            exif->fGPSDestLatitude[1] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestLatitude", num, den, 2))
            exif->fGPSDestLatitude[2] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestLongitude", num, den, 0))
            exif->fGPSDestLongitude[0] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestLongitude", num, den, 1))
            exif->fGPSDestLongitude[1] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.GPSInfo.GPSDestLongitude", num, den, 2))
            exif->fGPSDestLongitude[1] = dng_urational(num, den);

        // Integer Tags

        if (meta.getExifTagLong("Exif.Photo.ExposureProgram", val))
            exif->fExposureProgram = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.ISOSpeedRatings", val))
            exif->fISOSpeedRatings[0] = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.MeteringMode", val))
            exif->fMeteringMode = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.LightSource", val))
            exif->fLightSource = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.Flash", val))
            exif->fFlash = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.SensingMethod", val))
            exif->fSensingMethod = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.FileSource", val))
            exif->fFileSource = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.SceneType", val))
            exif->fSceneType = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.CustomRendered", val))
            exif->fCustomRendered = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.ExposureMode", val))
            exif->fExposureMode = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.WhiteBalance", val))
            exif->fWhiteBalance = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.SceneCaptureType", val))
            exif->fSceneCaptureType = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.GainControl", val))
            exif->fGainControl = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.Contrast", val))
            exif->fContrast = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.Saturation", val))
            exif->fSaturation = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.Sharpness", val))
            exif->fSharpness = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.SubjectDistanceRange", val))
            exif->fSubjectDistanceRange = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.FocalLengthIn35mmFilm", val))
            exif->fFocalLengthIn35mmFilm = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.ComponentsConfiguration", val))
            exif->fComponentsConfiguration = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.PixelXDimension", val))
            exif->fPixelXDimension = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.PixelYDimension", val))
            exif->fPixelYDimension = (uint32)val;
        if (meta.getExifTagLong("Exif.Photo.FocalPlaneResolutionUnit", val))
            exif->fFocalPlaneResolutionUnit = (uint32)val;
        if (meta.getExifTagLong("Exif.GPSInfo.GPSAltitudeRef", val))
            exif->fGPSAltitudeRef = (uint32)val;
        if (meta.getExifTagLong("Exif.GPSInfo.GPSDifferential", val))
            exif->fGPSDifferential = (uint32)val;
        long gpsVer1 = 0;
        long gpsVer2 = 0;
        long gpsVer3 = 0;
        long gpsVer4 = 0;
        meta.getExifTagLong("Exif.GPSInfo.GPSVersionID", gpsVer1, 0);
        meta.getExifTagLong("Exif.GPSInfo.GPSVersionID", gpsVer2, 1);
        meta.getExifTagLong("Exif.GPSInfo.GPSVersionID", gpsVer3, 2);
        meta.getExifTagLong("Exif.GPSInfo.GPSVersionID", gpsVer4, 3);
        long gpsVer = 0;
        gpsVer += gpsVer1 << 24;
        gpsVer += gpsVer2 << 16;
        gpsVer += gpsVer3 <<  8;
        gpsVer += gpsVer4;
        exif->fGPSVersionID = (uint32)gpsVer;

        if (meta.getExifTagRational("Exif.Nikon3.Lens", num, den, 0))
            exif->fLensInfo[0] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Nikon3.Lens", num, den, 1))
            exif->fLensInfo[1] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Nikon3.Lens", num, den, 2))
            exif->fLensInfo[2] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Nikon3.Lens", num, den, 3))
            exif->fLensInfo[3] = dng_urational(num, den);
        if (meta.getExifTagRational("Exif.Nikon3.ISOSpeed", num, den, 1))
            exif->fISOSpeedRatings[0] = (uint32)num;

        str = meta.getExifTagString("Exif.Nikon3.SerialNO");
        if ((found = str.find("NO=")) != std::string::npos)
            str.replace(found, 3, "");
        if (str.length())
        {
            exif->fCameraSerialNumber.Set_ASCII(str.c_str());
            exif->fCameraSerialNumber.TrimLeadingBlanks();
            exif->fCameraSerialNumber.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Nikon3.SerialNumber");
        if (str.length())
        {
            exif->fCameraSerialNumber.Set_ASCII(str.c_str());
            exif->fCameraSerialNumber.TrimLeadingBlanks();
            exif->fCameraSerialNumber.TrimTrailingBlanks();
        }

        if (meta.getExifTagLong("Exif.Nikon3.ShutterCount", val))
            exif->fImageNumber = (uint32)val;
        if (meta.getExifTagLong("Exif.NikonLd1.LensIDNumber", val))
        {
            char lensType[256];
            snprintf(lensType, sizeof(lensType), "%li", val);
            exif->fLensID.Set_ASCII(lensType);
        }
        if (meta.getExifTagLong("Exif.NikonLd2.LensIDNumber", val))
        {
            char lensType[256];
            snprintf(lensType, sizeof(lensType), "%li", val);
            exif->fLensID.Set_ASCII(lensType);
        }
        if (meta.getExifTagLong("Exif.NikonLd3.LensIDNumber", val))
        {
            char lensType[256];
            snprintf(lensType, sizeof(lensType), "%li", val);
            exif->fLensID.Set_ASCII(lensType);
        }
        if (meta.getExifTagLong("Exif.NikonLd2.FocusDistance", val))
            exif->fSubjectDistance = dng_urational((uint32)pow(10.0, val/40.0), 100);
        if (meta.getExifTagLong("Exif.NikonLd3.FocusDistance", val))
            exif->fSubjectDistance = dng_urational((uint32)pow(10.0, val/40.0), 100);
        str = meta.getExifTagString("Exif.NikonLd1.LensIDNumber");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }
        str = meta.getExifTagString("Exif.NikonLd2.LensIDNumber");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }
        str = meta.getExifTagString("Exif.NikonLd3.LensIDNumber");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }

        // Canon Makernotes

        if (meta.getExifTagLong("Exif.Canon.SerialNumber", val))
        {
            char serialNumber[256];
            snprintf(serialNumber, sizeof(serialNumber), "%li", val);
            exif->fCameraSerialNumber.Set_ASCII(serialNumber);
            exif->fCameraSerialNumber.TrimLeadingBlanks();
            exif->fCameraSerialNumber.TrimTrailingBlanks();
        }

        if (meta.getExifTagLong("Exif.CanonCs.ExposureProgram", val))
        {
            switch (val)
            {
            case 1:
                exif->fExposureProgram = 2;
                break;
            case 2:
                exif->fExposureProgram = 4;
                break;
            case 3:
                exif->fExposureProgram = 3;
                break;
            case 4:
                exif->fExposureProgram = 1;
                break;
            default:
                break;
            }
        }
        if (meta.getExifTagLong("Exif.CanonCs.MeteringMode", val))
        {
            switch (val)
            {
            case 1:
                exif->fMeteringMode = 3;
                break;
            case 2:
                exif->fMeteringMode = 1;
                break;
            case 3:
                exif->fMeteringMode = 5;
                break;
            case 4:
                exif->fMeteringMode = 6;
                break;
            case 5:
                exif->fMeteringMode = 2;
                break;
            default:
                break;
            }
        }

        if (meta.getExifTagLong("Exif.CanonCs.MaxAperture", val))
            exif->fMaxApertureValue = dng_urational(val, 32);

        long canonLensUnits = 1;
        if (meta.getExifTagRational("Exif.CanonCs.Lens", num, den, 2))
            canonLensUnits = num;
        if (meta.getExifTagRational("Exif.CanonCs.Lens", num, den, 0))
            exif->fLensInfo[1] = dng_urational(num, canonLensUnits);
        if (meta.getExifTagRational("Exif.CanonCs.Lens", num, den, 1))
            exif->fLensInfo[0] = dng_urational(num, canonLensUnits);
        if (meta.getExifTagRational("Exif.Canon.FocalLength", num, den, 1))
            exif->fFocalLength = dng_urational(num, canonLensUnits);
        long canonLensType = 65535;

        if ((meta.getExifTagLong("Exif.CanonCs.LensType", canonLensType)) &&
                (canonLensType != 65535))
        {
            char lensType[256];
            snprintf(lensType, sizeof(lensType), "%li", canonLensType);
            exif->fLensID.Set_ASCII(lensType);
        }
        str = meta.getExifTagString("Exif.Canon.LensModel");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }
        else if (canonLensType != 65535)
        {
            str = meta.getExifTagString("Exif.CanonCs.LensType");
            if (str.length())
            {
                exif->fLensName.Set_ASCII(str.c_str());
                exif->fLensName.TrimLeadingBlanks();
                exif->fLensName.TrimTrailingBlanks();
            }
        }

        str = meta.getExifTagString("Exif.Canon.OwnerName");
        if (str.length())
        {
            exif->fOwnerName.Set_ASCII(str.c_str());
            exif->fOwnerName.TrimLeadingBlanks();
            exif->fOwnerName.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Canon.FirmwareVersion");
        if ((found = str.find("Firmware")) != std::string::npos)
            str.replace(found, 8, "");
        if ((found = str.find("Version")) != std::string::npos)
            str.replace(found, 7, "");
        if (str.length())
        {
            exif->fFirmware.Set_ASCII(str.c_str());
            exif->fFirmware.TrimLeadingBlanks();
            exif->fFirmware.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.CanonSi.ISOSpeed");
        if (str.length())
        {
            sscanf(str.c_str(), "%ld", &val);
            exif->fISOSpeedRatings[0] = val;
        }

        if (meta.getExifTagLong("Exif.Canon.FileNumber", val))
            exif->fImageNumber = val;
        if (meta.getExifTagLong("Exif.CanonFi.FileNumber", val))
            exif->fImageNumber = val;

        // Pentax Makernotes

        str = meta.getExifTagString("Exif.Pentax.LensType");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }

        long pentaxLensId1 = 0;
        long pentaxLensId2 = 0;
        if ((meta.getExifTagLong("Exif.Pentax.LensType", pentaxLensId1, 0)) &&
                (meta.getExifTagLong("Exif.Pentax.LensType", pentaxLensId2, 1)))
        {
            char lensType[256];
            snprintf(lensType, sizeof(lensType), "%li %li", pentaxLensId1, pentaxLensId2);
            exif->fLensID.Set_ASCII(lensType);
        }

        // Olympus Makernotes

        str = meta.getExifTagString("Exif.OlympusEq.SerialNumber");
        if (str.length())
        {
            exif->fCameraSerialNumber.Set_ASCII(str.c_str());
            exif->fCameraSerialNumber.TrimLeadingBlanks();
            exif->fCameraSerialNumber.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.OlympusEq.LensSerialNumber");
        if (str.length())
        {
            exif->fLensSerialNumber.Set_ASCII(str.c_str());
            exif->fLensSerialNumber.TrimLeadingBlanks();
            exif->fLensSerialNumber.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.OlympusEq.LensModel");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }

        if (meta.getExifTagLong("Exif.OlympusEq.MinFocalLength", val))
            exif->fLensInfo[0] = dng_urational(val, 1);
        if (meta.getExifTagLong("Exif.OlympusEq.MaxFocalLength", val))
            exif->fLensInfo[1] = dng_urational(val, 1);

        // Panasonic Makernotes

        str = meta.getExifTagString("Exif.Panasonic.LensType");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }

        str = meta.getExifTagString("Exif.Panasonic.LensSerialNumber");
        if (str.length())
        {
            exif->fLensSerialNumber.Set_ASCII(str.c_str());
            exif->fLensSerialNumber.TrimLeadingBlanks();
            exif->fLensSerialNumber.TrimTrailingBlanks();
        }


        // Sony Makernotes

        if (meta.getExifTagLong("Exif.Sony2.LensID", val))
        {
            char lensType[256];
            snprintf(lensType, sizeof(lensType), "%li", val);
            exif->fLensID.Set_ASCII(lensType);
        }

        str = meta.getExifTagString("Exif.Sony2.LensID");
        if (str.length())
        {
            exif->fLensName.Set_ASCII(str.c_str());
            exif->fLensName.TrimLeadingBlanks();
            exif->fLensName.TrimTrailingBlanks();
        }

        //

        if ((exif->fLensName.IsEmpty()) &&
                (exif->fLensInfo[0].As_real64() > 0) &&
                (exif->fLensInfo[1].As_real64() > 0))
        {
            char lensName1[256];
            if (exif->fLensInfo[0].As_real64() == exif->fLensInfo[1].As_real64())
            {
                snprintf(lensName1, sizeof(lensName1), "%.1f mm", exif->fLensInfo[0].As_real64());
            }
            else
            {
                snprintf(lensName1, sizeof(lensName1), "%.1f-%.1f mm", exif->fLensInfo[0].As_real64(), exif->fLensInfo[1].As_real64());
            }

            std::string lensName(lensName1);

            if ((exif->fLensInfo[2].As_real64() > 0) &&
                    (exif->fLensInfo[3].As_real64() > 0))
            {
                char lensName2[256];
                if (exif->fLensInfo[2].As_real64() == exif->fLensInfo[3].As_real64())
                {
                    snprintf(lensName2, sizeof(lensName2), " f/%.1f", exif->fLensInfo[2].As_real64());
                }
                else
                {
                    snprintf(lensName2, sizeof(lensName2), " f/%.1f-%.1f", exif->fLensInfo[2].As_real64(), exif->fLensInfo[3].As_real64());
                }
                lensName.append(lensName2);
            }

            exif->fLensName.Set_ASCII(lensName.c_str());
        }

        // Markernote backup.
        long mknOffset = 0;
        std::string mknByteOrder = meta.getExifTagString("Exif.MakerNote.ByteOrder");
        if ((mknByteOrder.size() == 2) && meta.getExifTagLong("Exif.MakerNote.Offset", mknOffset))
        {
            long bufsize = 0;
            unsigned char* buffer = 0;
            if (meta.getExifTagData("Exif.Photo.MakerNote", &bufsize, &buffer))
            {
                dng_memory_stream streamPriv(memalloc);
                streamPriv.SetBigEndian();

                streamPriv.Put("Adobe", 5);
                streamPriv.Put_uint8(0x00);
                streamPriv.Put("MakN", 4);
                streamPriv.Put_uint32(bufsize + mknByteOrder.size() + 4);
                streamPriv.Put(mknByteOrder.c_str(), mknByteOrder.size());
                streamPriv.Put_uint32(mknOffset);
                streamPriv.Put(buffer, bufsize);
                AutoPtr<dng_memory_block> blockPriv(host.Allocate(streamPriv.Length()));
                streamPriv.SetReadPosition(0);
                streamPriv.Get(blockPriv->Buffer(), streamPriv.Length());
                negative->SetPrivateData(blockPriv);

                delete[] buffer;
            }
        }
    }

    // -----------------------------------------------------------------------------------------

    negative->SetModelName(exif->fModel.Get());

    // -----------------------------------------------------------------------------------------

    if (true == embedOriginal)
    {
        dng_file_stream originalDataStream(filename);
        originalDataStream.SetReadPosition(0);

        uint32 forkLength = (uint32)originalDataStream.Length();
        uint32 forkBlocks = (uint32)floor((forkLength + 65535.0) / 65536.0);

        int level = Z_DEFAULT_COMPRESSION;
        int ret;
        z_stream zstrm;
        unsigned char inBuffer[CHUNK];
        unsigned char outBuffer[CHUNK * 2];

        dng_memory_stream embedDataStream(memalloc);
        embedDataStream.SetBigEndian(true);
        embedDataStream.Put_uint32(forkLength);

        uint32 offset = (2 + forkBlocks) * sizeof(uint32);
        embedDataStream.Put_uint32(offset);

        for (uint32 block = 0; block < forkBlocks; block++)
        {
            embedDataStream.Put_uint32(0);
        }

        for (uint32 block = 0; block < forkBlocks; block++)
        {
            uint32 originalBlockLength = (uint32)std::min((uint64)CHUNK,
                                                          originalDataStream.Length() - originalDataStream.Position());
            originalDataStream.Get(inBuffer, originalBlockLength);

            /* allocate deflate state */
            zstrm.zalloc = Z_NULL;
            zstrm.zfree = Z_NULL;
            zstrm.opaque = Z_NULL;
            ret = deflateInit(&zstrm, level);
            if (ret != Z_OK)
                return ret;

            /* compress */
            zstrm.avail_in = originalBlockLength;
            if (zstrm.avail_in == 0)
                break;
            zstrm.next_in = inBuffer;

            zstrm.avail_out = CHUNK * 2;
            zstrm.next_out = outBuffer;
            ret = deflate(&zstrm, Z_FINISH);
            assert(ret == Z_STREAM_END);

            uint32 compressedBlockLength = zstrm.total_out;

            /* clean up and return */
            (void)deflateEnd(&zstrm);

            embedDataStream.SetWritePosition(offset);
            embedDataStream.Put(outBuffer, compressedBlockLength);

            offset += compressedBlockLength;
            embedDataStream.SetWritePosition((2 + block) * sizeof(int32));
            embedDataStream.Put_uint32(offset);
        }

        embedDataStream.SetWritePosition(offset);
        embedDataStream.Put_uint32(0);
        embedDataStream.Put_uint32(0);
        embedDataStream.Put_uint32(0);
        embedDataStream.Put_uint32(0);
        embedDataStream.Put_uint32(0);
        embedDataStream.Put_uint32(0);
        embedDataStream.Put_uint32(0);

        AutoPtr<dng_memory_block> block(host.Allocate(embedDataStream.Length()));
        embedDataStream.SetReadPosition(0);
        embedDataStream.Get(block->Buffer(), embedDataStream.Length());

        dng_md5_printer md5;
        md5.Process(block->Buffer(), block->LogicalSize());
        negative->SetOriginalRawFileData(block);
        negative->SetOriginalRawFileDigest(md5.Result());
        negative->ValidateOriginalRawFileDigest();
    }

    // -----------------------------------------------------------------------------------------

    // Assign Raw image data.
    negative->SetStage1Image(image);

    // Compute linearized and range mapped image
    negative->BuildStage2Image(host);

    // Compute demosaiced image (used by preview and thumbnail)
    negative->BuildStage3Image(host);

    negative->SynchronizeMetadata();
    negative->RebuildIPTC(true, false);

    // -----------------------------------------------------------------------------------------

    dng_preview_list previewList;

    AutoPtr<dng_image> jpegImage;
    dng_render jpeg_render(host, *negative);
    jpeg_render.SetFinalSpace(dng_space_sRGB::Get());
    jpeg_render.SetFinalPixelType(ttByte);
    jpeg_render.SetMaximumSize(1024);
    jpegImage.Reset(jpeg_render.Render());

    DngImageWriter jpeg_writer;
    AutoPtr<dng_memory_stream> dms(new dng_memory_stream(gDefaultDNGMemoryAllocator));
    jpeg_writer.WriteJPEG(host, *dms, *jpegImage.Get(), 75, 1);
    dms->SetReadPosition(0);

    AutoPtr<dng_jpeg_preview> jpeg_preview;
    jpeg_preview.Reset(new dng_jpeg_preview);
    jpeg_preview->fPhotometricInterpretation = piYCbCr;
    jpeg_preview->fPreviewSize               = jpegImage->Size();
    jpeg_preview->fYCbCrSubSampling          = dng_point(2, 2);
    jpeg_preview->fCompressedData.Reset(host.Allocate(dms->Length()));
    dms->Get(jpeg_preview->fCompressedData->Buffer_char(), dms->Length());
    jpeg_preview->fInfo.fApplicationName.Set_ASCII("DNG SDK");
    jpeg_preview->fInfo.fApplicationVersion.Set_ASCII("1.3");
    //jpeg_preview->fInfo.fDateTime = ;
    jpeg_preview->fInfo.fColorSpace = previewColorSpace_sRGB;

    AutoPtr<dng_preview> pp(dynamic_cast<dng_preview*>(jpeg_preview.Release()));
    previewList.Append(pp);
    dms.Reset();

    // -----------------------------------------------------------------------------------------

    dng_image_preview thumbnail;
    dng_render thumbnail_render(host, *negative);
    thumbnail_render.SetFinalSpace(dng_space_sRGB::Get());
    thumbnail_render.SetFinalPixelType(ttByte);
    thumbnail_render.SetMaximumSize(256);
    thumbnail.fImage.Reset(thumbnail_render.Render());

    // -----------------------------------------------------------------------------------------

    dng_image_writer writer;

    // output filename: replace raw file extension with .dng
    std::string lpszOutFileName(filename);
    if (outfilename != NULL)
    {
        lpszOutFileName.assign(outfilename);
    }
    else
    {
        found = lpszOutFileName.find_last_of(".");
        if(found != std::string::npos)
            lpszOutFileName.resize(found);
        lpszOutFileName.append(".dng");
    }

    dng_file_stream filestream(lpszOutFileName.c_str(), true);

    writer.WriteDNG(host, filestream, *negative.Get(), thumbnail, ccJPEG, &previewList);

    return 0;
}

