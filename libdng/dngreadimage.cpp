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

#include "dngreadimage.h"

#include <dng_host.h>
#include <dng_stream.h>

#include <jpeglib.h>

#include <iostream>

static const int max_buf = 4096;

struct DngStreamSourceMgr 
        : public jpeg_source_mgr
{
    JOCTET buffer[max_buf];
    dng_stream* stream;

    DngStreamSourceMgr(dng_stream* s);

    static void jpeg_init_buffer(jpeg_decompress_struct* cinfo);
    static boolean jpeg_fill_input_buffer(jpeg_decompress_struct* cinfo);
    static void jpeg_skip_input_data(jpeg_decompress_struct* cinfo, long num_bytes);
    static boolean jpeg_resync_to_restart(jpeg_decompress_struct* cinfo, int desired);
    static void jpeg_term_source(jpeg_decompress_struct* cinfo);
};

void DngStreamSourceMgr::jpeg_init_buffer(jpeg_decompress_struct* cinfo)
{
}

boolean DngStreamSourceMgr::jpeg_fill_input_buffer(jpeg_decompress_struct* cinfo)
{
    DngStreamSourceMgr* src = (DngStreamSourceMgr*)cinfo->src;
    src->stream->Get(src->buffer, max_buf);
    src->next_input_byte = src->buffer;
    src->bytes_in_buffer = max_buf;
    return true;
}

void DngStreamSourceMgr::jpeg_skip_input_data(jpeg_decompress_struct* cinfo, long num_bytes)
{
    DngStreamSourceMgr* src = (DngStreamSourceMgr*)cinfo->src;

    if (num_bytes > 0)
    {
        while (num_bytes > (long) src->bytes_in_buffer)
        {
	    num_bytes -= (long) src->bytes_in_buffer;
	    (void) jpeg_fill_input_buffer(cinfo);
        }
    }
    src->next_input_byte += (size_t) num_bytes;
    src->bytes_in_buffer -= (size_t) num_bytes;
}

boolean DngStreamSourceMgr::jpeg_resync_to_restart(jpeg_decompress_struct* cinfo, int desired)
{
    return false;
}

void DngStreamSourceMgr::jpeg_term_source(jpeg_decompress_struct* cinfo)
{
}

DngStreamSourceMgr::DngStreamSourceMgr(dng_stream* s)
{
    jpeg_source_mgr::init_source       = jpeg_init_buffer;
    jpeg_source_mgr::fill_input_buffer = jpeg_fill_input_buffer;
    jpeg_source_mgr::skip_input_data   = jpeg_skip_input_data;
    jpeg_source_mgr::resync_to_restart = jpeg_resync_to_restart;
    jpeg_source_mgr::term_source       = jpeg_term_source;

    this->stream = s;

    bytes_in_buffer = 0;
    next_input_byte = buffer;
}

struct DngStreamErrorMgr
        : public jpeg_error_mgr
{
    dng_host* host;

    DngStreamErrorMgr(dng_host* h);
};

DngStreamErrorMgr::DngStreamErrorMgr(dng_host* h)
{
    /*
  jpeg_error_mgr::output_message = ;
  jpeg_error_mgr::emit_message = ;
  jpeg_error_mgr::output_message = ;
  */
    this->host = h;
}

DngReadImage::DngReadImage(void)
{
}

DngReadImage::~DngReadImage(void)
{
}

bool DngReadImage::ReadBaselineJPEG(dng_host& host,
                                    const dng_ifd& ifd,
                                    dng_stream& stream,
                                    dng_image& image,
                                    const dng_rect& tileArea,
                                    uint32 plane,
                                    uint32 planes,
                                    uint32 tileByteCount)
{
    uint64 startPos = stream.Position();
    uint8 header[2];
    header[0] = stream.Get_uint8();
    header[1] = stream.Get_uint8();

    unsigned char jpegID[] = { 0xFF, 0xD8 };
    if (memcmp(header, jpegID, 2) != 0)
    {
        return false;
    }
    stream.SetReadPosition(startPos);

    DngStreamSourceMgr smgr(&stream);
    DngStreamErrorMgr jerr(&host);

    AutoPtr<dng_memory_block> dstData(host.Allocate(image.Width() * image.Height() * 3 * sizeof(uint8)));
    
    dng_pixel_buffer buffer;

    buffer.fArea       = dng_rect(image.Height(), image.Width());
    buffer.fPlane      = 0;
    buffer.fPlanes     = 3;
    buffer.fRowStep    = buffer.fPlanes * image.Width();
    buffer.fColStep    = buffer.fPlanes;
    buffer.fPlaneStep  = 1;
    buffer.fPixelType  = ttByte;
    buffer.fPixelSize  = TagTypeSize(ttByte);
    buffer.fData       = dstData->Buffer();

    struct jpeg_decompress_struct cinfo;
    jpeg_create_decompress(&cinfo);

    cinfo.src = &smgr;
    cinfo.err = jpeg_std_error(&jerr);
    
    jpeg_read_header(&cinfo, true);

    jpeg_start_decompress(&cinfo);

    JSAMPROW row_pointer;
    while (cinfo.output_scanline < cinfo.output_height)
    {
        row_pointer = (JSAMPROW) buffer.ConstPixel_uint8(cinfo.output_scanline, 0);
        jpeg_read_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    image.Put(buffer);

    return true;
}


