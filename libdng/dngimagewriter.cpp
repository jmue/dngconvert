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

#include "dngimagewriter.h"

#include <jpeglib.h>

#include <dng_host.h>
#include <dng_image.h>
#include <dng_color_space.h>

static const int max_buf = 4096;

struct DngStreamDestinationMgr 
        : public jpeg_destination_mgr
{
    JOCTET buffer[max_buf];
    dng_stream* stream;

    DngStreamDestinationMgr(dng_stream* s);

    static void jpeg_init_buffer(jpeg_compress_struct* cinfo);
    static boolean jpeg_empty_buffer(jpeg_compress_struct* cinfo);
    static void jpeg_term_buffer(jpeg_compress_struct* cinfo);
};

void DngStreamDestinationMgr::jpeg_init_buffer(jpeg_compress_struct* cinfo)
{
}

boolean DngStreamDestinationMgr::jpeg_empty_buffer(jpeg_compress_struct* cinfo)
{
    DngStreamDestinationMgr* dest = (DngStreamDestinationMgr*)cinfo->dest;

    dest->stream->Put(dest->buffer, max_buf);
    dest->next_output_byte = dest->buffer;
    dest->free_in_buffer = max_buf;

    return TRUE;
}

void DngStreamDestinationMgr::jpeg_term_buffer(jpeg_compress_struct* cinfo)
{
    DngStreamDestinationMgr* dest = (DngStreamDestinationMgr*)cinfo->dest;

    uint32 n = max_buf - dest->free_in_buffer;
    dest->stream->Put(dest->buffer, n);
}

DngStreamDestinationMgr::DngStreamDestinationMgr(dng_stream* s)
{
    jpeg_destination_mgr::init_destination    = jpeg_init_buffer;
    jpeg_destination_mgr::empty_output_buffer = jpeg_empty_buffer;
    jpeg_destination_mgr::term_destination    = jpeg_term_buffer;

    this->stream = s;
    next_output_byte = buffer;
    free_in_buffer = max_buf;
}

DngImageWriter::DngImageWriter(void)
{
}

DngImageWriter::~DngImageWriter(void)
{
}

void DngImageWriter::WriteJPEG(dng_host &host,
                               dng_stream &stream,
                               const dng_image &image,
                               uint8 compression,
                               uint8 subsampling,
                               const dng_color_space *space)
{
    if ((image.Planes() != 3))
    {
        ThrowProgramError ();
    }

    const void *profileData = NULL;
    uint32 profileSize = 0;

    const uint8 *data = NULL;
    uint32 size = 0;

    if (space && space->ICCProfile(size, data))
    {
        profileData = data;
        profileSize = size;
    }

    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr       jerr;

    DngStreamDestinationMgr dmgr(&stream);

    AutoPtr<dng_memory_block> srcData(host.Allocate(image.Width() * image.Height() * 3 * sizeof(uint8)));

    dng_pixel_buffer buffer;

    buffer.fArea       = dng_rect(image.Height(), image.Width());
    buffer.fPlane      = 0;
    buffer.fPlanes     = 3;
    buffer.fRowStep    = buffer.fPlanes * image.Width();
    buffer.fColStep    = buffer.fPlanes;
    buffer.fPlaneStep  = 1;
    buffer.fPixelType  = ttByte;
    buffer.fPixelSize  = TagTypeSize(ttByte);
    buffer.fData       = srcData->Buffer();

    image.Get(buffer);

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    cinfo.dest = &dmgr;
    cinfo.image_width      = image.Width();
    cinfo.image_height     = image.Height();
    cinfo.input_components = 3;
    cinfo.in_color_space   = JCS_RGB;

    jpeg_set_defaults(&cinfo);

    switch (subsampling)
    {
    case 1:  // 2x1, 1x1, 1x1 (4:2:2) : Medium
    {
        cinfo.comp_info[0].h_samp_factor = 2;
        cinfo.comp_info[0].v_samp_factor = 1;
        cinfo.comp_info[1].h_samp_factor = 1;
        cinfo.comp_info[1].v_samp_factor = 1;
        cinfo.comp_info[2].h_samp_factor = 1;
        cinfo.comp_info[2].v_samp_factor = 1;
        break;
    }
    case 2:  // 2x2, 1x1, 1x1 (4:1:1) : High
    {
        cinfo.comp_info[0].h_samp_factor = 2;
        cinfo.comp_info[0].v_samp_factor = 2;
        cinfo.comp_info[1].h_samp_factor = 1;
        cinfo.comp_info[1].v_samp_factor = 1;
        cinfo.comp_info[2].h_samp_factor = 1;
        cinfo.comp_info[2].v_samp_factor = 1;
        break;
    }
    default:  // 1x1 1x1 1x1 (4:4:4) : None
    {
        cinfo.comp_info[0].h_samp_factor = 1;
        cinfo.comp_info[0].v_samp_factor = 1;
        cinfo.comp_info[1].h_samp_factor = 1;
        cinfo.comp_info[1].v_samp_factor = 1;
        cinfo.comp_info[2].h_samp_factor = 1;
        cinfo.comp_info[2].v_samp_factor = 1;
        break;
    }
    }

    jpeg_set_quality (&cinfo, compression, true);
    jpeg_start_compress(&cinfo, true);

    JSAMPROW row_pointer;
    while (cinfo.next_scanline < cinfo.image_height)
    {
        row_pointer = (JSAMPROW) buffer.ConstPixel_uint8(cinfo.next_scanline, 0);
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
}
