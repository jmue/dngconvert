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

#include "exiv2dngstreamio.h"

#include "dng_exceptions.h"
#include "dng_memory_stream.h"

#include <exiv2/futils.hpp>
#include <exiv2/types.hpp>
#include <exiv2/error.hpp>

Exiv2DngStreamIO::Exiv2DngStreamIO(dng_stream& stream, dng_memory_allocator &allocator)
    : m_Allocator(allocator), m_Stream(stream), m_MemBlock(0)
{
}

Exiv2DngStreamIO::~Exiv2DngStreamIO(void)
{
}

int Exiv2DngStreamIO::open()
{
    m_Stream.SetReadPosition(0);
    return 0;
}

int Exiv2DngStreamIO::close()
{
    m_Stream.Flush();
    return 0;
}

long Exiv2DngStreamIO::write(const Exiv2::byte* data, long wcount)
{
    uint64 oldPos = m_Stream.Position();
    m_Stream.Get((void*)data, wcount);
    return m_Stream.Position() - oldPos;
}

long Exiv2DngStreamIO::write(BasicIo& src)
{
    if (static_cast<BasicIo*>(this) == &src)
        return 0;
    if (!src.isopen())
        return 0;

    Exiv2::byte buf[4096];
    long readCount = 0;
    long writeTotal = 0;
    while ((readCount = src.read(buf, sizeof(buf))))
    {
        write(buf, readCount);
        writeTotal += readCount;
    }

    return writeTotal;
}

int Exiv2DngStreamIO::putb(Exiv2::byte data)
{
    m_Stream.Put_uint8(data);
    return data;
}

Exiv2::DataBuf Exiv2DngStreamIO::read(long rcount)
{
    Exiv2::DataBuf buf(rcount);
    long readCount = read(buf.pData_, buf.size_);
    buf.size_ = readCount;
    return buf;
}

long Exiv2DngStreamIO::read(Exiv2::byte* buf, long rcount)
{
    uint64 oldPos = m_Stream.Position();
    m_Stream.Get((void*)buf, rcount);
    return m_Stream.Position() - oldPos;
}

int Exiv2DngStreamIO::getb()
{
    return m_Stream.Get_uint8();
}

void Exiv2DngStreamIO::transfer(BasicIo& src)
{
    // Generic reopen to reset position to start
    if (src.open() != 0)
    {
        throw Exiv2::Error(9, src.path(), Exiv2::strError());
    }
    m_Stream.SetReadPosition(0);
    m_Stream.SetLength(0);
    write(src);
    src.close();

    if (error() || src.error())
        throw Exiv2::Error(19, Exiv2::strError());
}

int Exiv2DngStreamIO::seek(long offset, Position pos)
{
    switch (pos)
    {
    case BasicIo::cur:
        m_Stream.SetReadPosition(m_Stream.Position() + offset);
        break;
    case BasicIo::beg:
        m_Stream.SetReadPosition(offset);
        break;
    case BasicIo::end:
        m_Stream.SetReadPosition(m_Stream.Length() + offset);
        break;
    }

    return 0;
}

Exiv2::byte* Exiv2DngStreamIO::mmap(bool isWriteable)
{
    m_MemBlock = std::auto_ptr<dng_memory_block>(m_Stream.AsMemoryBlock(m_Allocator));
    return (Exiv2::byte*)m_MemBlock.get()->Buffer();
}

int Exiv2DngStreamIO::munmap()
{
    m_Stream.SetReadPosition(0);
    m_Stream.SetLength(0);
    m_Stream.Put(m_MemBlock.get()->Buffer(), m_MemBlock.get()->LogicalSize());
    m_MemBlock.release();
    return 0;
}

long Exiv2DngStreamIO::tell() const
{
    return m_Stream.Position();
}

long Exiv2DngStreamIO::size() const
{
    return m_Stream.Length();
}

bool Exiv2DngStreamIO::isopen() const
{
    return true;
}

int Exiv2DngStreamIO::error() const
{
    return 0;
}

bool Exiv2DngStreamIO::eof() const
{
    return m_Stream.Position() >= m_Stream.Length();
}

std::string Exiv2DngStreamIO::path() const
{
    return "Exiv2DngStreamIO";
}

#ifdef EXV_UNICODE_PATH
std::wstring Exiv2DngStreamIO::wpath() const
{
    return EXV_WIDEN("Exiv2DngStreamIO");
}
#endif

Exiv2::BasicIo::AutoPtr Exiv2DngStreamIO::temporary() const
{
    return Exiv2::BasicIo::AutoPtr(new Exiv2::MemIo());
}
