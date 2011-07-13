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

#pragma once

#include <exiv2/exif.hpp>
#include <exiv2/image.hpp>

class Exiv2Helper
{
public:
    Exiv2Helper(void);
    ~Exiv2Helper(void);

    bool load(const char* fname);

    std::string getExifTagString(const char* exifTagName) const;
    bool getExifTagRational(const char* exifTagName, long int& num, long int& den, int component=0) const;
    bool getExifTagLong(const char* exifTagName, long &val, int component=0) const;
    bool getExifTagData(const char* exifTagName, long* size, unsigned char** data);

    static void printExiv2ExceptionError(const char* msg, Exiv2::Error& e);

private:
    Exiv2::ExifData m_ExifMetadata;
    Exiv2::IptcData m_IptcMetadata;
    Exiv2::XmpData  m_XmpMetadata;
    std::string     m_ImageComments;
};
