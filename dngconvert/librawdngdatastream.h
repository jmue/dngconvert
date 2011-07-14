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

#include <stdlib.h>
#include "libraw/libraw_datastream.h"

#include "dng_stream.h"

class LibRawDngDataStream : public LibRaw_abstract_datastream
{
public:
    LibRawDngDataStream(dng_stream& stream);
    ~LibRawDngDataStream(void);

    virtual int valid();
    virtual int read(void* ptr, size_t size, size_t nmemb);
    virtual int seek(INT64 offset, int whence);
    virtual INT64 tell();
    virtual int get_char();
    virtual char* gets(char* str, int size);
    virtual int scanf_one(const char* fmt, void* val);
    virtual int eof();

protected:
    dng_stream& m_Stream;
};
