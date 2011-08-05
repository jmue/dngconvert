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

   This file uses code from dng_threaded_host.cpp -- Sandy McGuffog CornerFix utility
   (http://sourceforge.net/projects/cornerfix, sandy dot cornerfix at gmail dot com),
   dng_threaded_host.cpp is copyright 2007-2011, by Sandy McGuffog and Contributors.
*/

#include "dngexif.h"
#include "dngtagcodes.h"

#include "dng_globals.h"
#include "dng_parse_utils.h"
#include "dng_tag_types.h"

DngExif::DngExif(void)
{
}

DngExif::~DngExif(void)
{
}

bool DngExif::Parse_ifd0_exif (dng_stream &stream,
                               dng_shared &shared,
                               uint32 parentCode,
                               uint32 tagCode,
                               uint32 tagType,
                               uint32 tagCount,
                               uint64 tagOffset)
{
    switch (tagCode)
    {
    case tcCameraOwnerName:
    {
        CheckTagType(parentCode, tagCode, tagType, ttAscii);
        ParseStringTag(stream, parentCode, tagCode, tagCount, fOwnerName);
#if qDNGValidate
        if(gVerbose)
        {
            printf("CameraOwnerName: ");
            DumpString(fOwnerName);
            printf("\n");
        }
#endif
        break;
    }
    case tcBodySerialNumber:
    {
        CheckTagType(parentCode, tagCode, tagType, ttAscii);
        ParseStringTag(stream, parentCode, tagCode, tagCount, fCameraSerialNumber);
#if qDNGValidate
        if(gVerbose)
        {
            printf("BodySerialNumber: ");
            DumpString(fCameraSerialNumber);
            printf("\n");
        }
#endif
        break;
    }
    case tcLensSpecification:
    {
        CheckTagType(parentCode, tagCode, tagType, ttRational);
        if (!CheckTagCount(parentCode, tagCode, tagCount, 4))
            return false;
        fLensInfo[0] = stream.TagValue_urational(tagType);
        fLensInfo[1] = stream.TagValue_urational(tagType);
        fLensInfo[2] = stream.TagValue_urational(tagType);
        fLensInfo[3] = stream.TagValue_urational(tagType);

        // Some third party software wrote zero rather and undefined values
        // for unknown entries.  Work around this bug.
        for (uint32 j = 0; j < 4; j++)
        {
            if (fLensInfo[j].IsValid() && fLensInfo[j].As_real64() <= 0.0)
            {
                fLensInfo[j] = dng_urational(0, 0);
#if qDNGValidate
                ReportWarning ("Zero entry in LensInfo tag--should be undefined");
#endif
            }
        }
#if qDNGValidate
        if(gVerbose)
        {
            printf("LensSpecification: ");
            real64 minFL = fLensInfo[0].As_real64();
            real64 maxFL = fLensInfo[1].As_real64();
            if (minFL == maxFL)
                printf ("%0.1f mm", minFL);
            else
                printf ("%0.1f-%0.1f mm", minFL, maxFL);
            if (fLensInfo[2].d)
            {
                real64 minFS = fLensInfo[2].As_real64();
                real64 maxFS = fLensInfo[3].As_real64();
                if (minFS == maxFS)
                    printf(" f/%0.1f", minFS);
                else
                    printf(" f/%0.1f-%0.1f", minFS, maxFS);
            }
            printf("\n");
        }
#endif
        break;
    }
    case tcLensMake:
        break;
    case tcLensModel:
    {
        CheckTagType(parentCode, tagCode, tagType, ttAscii);
        ParseStringTag(stream, parentCode, tagCode, tagCount, fLensName);
#if qDNGValidate
        if(gVerbose)
        {
            printf("LensModel: ");
            DumpString(fLensName);
            printf("\n");
        }
#endif
        break;
    }
    case tcLensSerialNumber:
    {
        CheckTagType(parentCode, tagCode, tagType, ttAscii);
        ParseStringTag(stream, parentCode, tagCode, tagCount, fLensSerialNumber);
#if qDNGValidate
        if(gVerbose)
        {
            printf("LensSerialNumber: ");
            DumpString(fLensSerialNumber);
            printf("\n");
        }
#endif
        break;
    }
    default:
    {
        return dng_exif::Parse_ifd0_exif(stream, shared, parentCode, tagCode, tagType, tagCount, tagOffset);
    }			
    }
    return true;
}

