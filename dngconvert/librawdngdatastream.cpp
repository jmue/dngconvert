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

#include "librawdngdatastream.h"

#include "dng_exceptions.h"

LibRawDngDataStream::LibRawDngDataStream(dng_stream& stream)
    : m_Stream(stream)
{
}

LibRawDngDataStream::~LibRawDngDataStream(void)
{
}

int LibRawDngDataStream::valid()
{
    return (m_Stream.Length() > 0) ? 1 : 0;
}

int LibRawDngDataStream::read(void* ptr, size_t size, size_t nmemb)
{
    if (substream)
        return substream->read(ptr, size, nmemb);

    uint64 oldPos = m_Stream.Position();
    uint64 bytes = std::min((uint64)size*nmemb, m_Stream.Length() - oldPos);
    m_Stream.Get(ptr, bytes);
    return int((m_Stream.Position() - oldPos + size - 1) / size);
}

int LibRawDngDataStream::seek(INT64 offset, int whence)
{
    if (substream)
        return substream->seek(offset, whence);

    switch (whence)
    {
    case SEEK_SET:
        m_Stream.SetReadPosition(offset);
        break;
    case SEEK_CUR:
        m_Stream.SetReadPosition(m_Stream.Position() + offset);
        break;
    case SEEK_END:
        m_Stream.SetReadPosition(m_Stream.Length() + offset);
        break;
    }
    return (int)m_Stream.Position();
}

INT64 LibRawDngDataStream::tell()
{
    if (substream)
        return substream->tell();

    return m_Stream.Position();
}

int LibRawDngDataStream::get_char()
{
    if (substream)
        return substream->get_char();

    return m_Stream.Get_uint8();
}

char* LibRawDngDataStream::gets(char* str, int size)
{
    if (substream)
        return substream->gets(str, size);

    memset(str, 0, size);

    int32 index = 0;

    while (true)
    {
        char c = (char)m_Stream.Get_uint8();

        if (index + 1 < size)
            str[index++] = c;

        if (c == '\n')
            break;
    }

    return str;
}

int LibRawDngDataStream::scanf_one(const char* fmt, void* val)
{
    if (substream)
        return substream->scanf_one(fmt, val);

    try
    {
        /* HUGE ASSUMPTION: *fmt is either "%d" or "%f" */
        if (strcmp(fmt, "%d") == 0)
        {
            int32 d = m_Stream.Get_int32();
            *(static_cast<int*>(val)) = d;
        }
        else
        {
            real32 f = m_Stream.Get_real32();
            *(static_cast<float*>(val)) = f;
        }
    }
    catch (dng_exception&)
    {
        return EOF;
    }

    return 1;
}

int LibRawDngDataStream::eof()
{
    if (substream)
        return substream->eof();

    return m_Stream.Position() >= m_Stream.Length();
}
