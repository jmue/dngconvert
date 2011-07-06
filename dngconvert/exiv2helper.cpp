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

#include "exiv2helper.h"
#include "stdio.h"

Exiv2Helper::Exiv2Helper(void)
{
}

Exiv2Helper::~Exiv2Helper(void)
{
}

void Exiv2Helper::printExiv2ExceptionError(const char* msg, Exiv2::Error& e)
{
    std::string s(e.what());
    fprintf(stderr, "%s  (Error #%i: %s)\n", msg, e.code(), s.c_str());
}

bool Exiv2Helper::load(const char* filePath)
{
    if (NULL == filePath)
        return false;

    try
    {
        Exiv2::Image::AutoPtr image;

        image = Exiv2::ImageFactory::open(filePath);

        if (!image.get())
        {
            fprintf(stderr, "File '%s' is not readable.", filePath);
            return false;
        }

        //d->filePath = filePath;
        image->readMetadata();

        // Image comments ---------------------------------
        m_ImageComments = image->comment();

        // Exif metadata ----------------------------------
        m_ExifMetadata = image->exifData();

        // Iptc metadata ----------------------------------
        m_IptcMetadata = image->iptcData();

        // Xmp metadata -----------------------------------
        m_XmpMetadata = image->xmpData();

        return true;
    }
    catch( Exiv2::Error& e )
    {
        printExiv2ExceptionError("Cannot load metadata using Exiv2 ", e);
    }

    return false;
}

std::string Exiv2Helper::getExifTagString(const char* exifTagName) const
{
    try
    {
        Exiv2::ExifKey exifKey(exifTagName);
        Exiv2::ExifData exifData(m_ExifMetadata);
        Exiv2::ExifData::iterator it = exifData.findKey(exifKey);
        if (it != exifData.end())
        {
            std::string val = it->print(&exifData);
            return val;
        }
    }
    catch(Exiv2::Error& e)
    {
        std::string err = std::string("Cannot find Exif String value from key '") + std::string(exifTagName) + std::string("' into image using Exiv2 ");
        printExiv2ExceptionError(err.c_str(), e);
    }

    return std::string();
}

bool Exiv2Helper::getExifTagRational(const char* exifTagName, long int& num, long int& den, int component) const
{
    try
    {
        Exiv2::ExifKey exifKey(exifTagName);
        Exiv2::ExifData exifData(m_ExifMetadata);
        Exiv2::ExifData::iterator it = exifData.findKey(exifKey);
        if (it != exifData.end())
        {
            num = (*it).toRational(component).first;
            den = (*it).toRational(component).second;
            return true;
        }
    }
    catch(Exiv2::Error& e)
    {
        std::string err = std::string("Cannot find Exif Rational value from key '") + std::string(exifTagName) + std::string("' into image using Exiv2 ");
        printExiv2ExceptionError(err.c_str(), e);
    }

    return false;
}

bool Exiv2Helper::getExifTagLong(const char* exifTagName, long& val, int component) const
{
    try
    {
        Exiv2::ExifKey exifKey(exifTagName);
        Exiv2::ExifData exifData(m_ExifMetadata);
        Exiv2::ExifData::iterator it = exifData.findKey(exifKey);
        if (it != exifData.end() && it->count() > 0)
        {
            val = it->toLong(component);
            return true;
        }
    }
    catch(Exiv2::Error& e)
    {
        std::string err = std::string("Cannot find Exif Long value from key '") + std::string(exifTagName) + std::string("' into image using Exiv2 ");
        printExiv2ExceptionError(err.c_str(), e);
    }

    return false;
}

bool Exiv2Helper::getExifTagData(const char* exifTagName, long* size, unsigned char** data)
{
    try
    {
        Exiv2::ExifKey exifKey(exifTagName);
        Exiv2::ExifData exifData(m_ExifMetadata);
        Exiv2::ExifData::iterator it = exifData.findKey(exifKey);
        if (it != exifData.end())
        {
            *data = new unsigned char[(*it).size()];
            *size = (*it).size();
            (*it).copy((Exiv2::byte*)*data, Exiv2::bigEndian);
            return true;
        }
    }
    catch(Exiv2::Error& e)
    {
        std::string err = std::string("Cannot find Exif value from key '") + std::string(exifTagName) + std::string("' into image using Exiv2 ");
        printExiv2ExceptionError(err.c_str(), e);
    }

    return false;
}


