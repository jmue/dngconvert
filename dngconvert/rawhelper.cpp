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

#include "rawhelper.h"

#include "libraw/libraw.h"

RawHelper::RawHelper(void)
{
    m_RawProcessor = new LibRaw();
}

RawHelper::~RawHelper(void)
{
    delete m_RawProcessor;
}

int RawHelper::identifyRawData(const char* fname, 
                               libraw_data_t* imgdata)
{
    int ret = m_RawProcessor->open_file(fname);
    if (ret != LIBRAW_SUCCESS)
    {
        printf("Cannot open %s: %s\n", fname, libraw_strerror(ret));
        m_RawProcessor->recycle();
        return -1;
    }

    ret = m_RawProcessor->adjust_sizes_info_only();
    if (ret != LIBRAW_SUCCESS)
    {
        printf("LibRaw: failed to run adjust_sizes_info_only: %s", libraw_strerror(ret));
        m_RawProcessor->recycle();
        return -1;
    }

    *imgdata = m_RawProcessor->imgdata;
    m_RawProcessor->recycle();
    return 0;
}

int RawHelper::extractRawData(const char* fname, 
                              unsigned int shot, 
                              libraw_data_t* imgdata, 
                              std::vector<unsigned short>* rawdata, 
                              bool fullSensorImage)
{
    int ret = m_RawProcessor->open_file(fname);
    if (ret != LIBRAW_SUCCESS)
    {
        printf("Cannot open %s: %s\n", fname, libraw_strerror(ret));
        m_RawProcessor->recycle();
        return -1;
    }

    m_RawProcessor->imgdata.params.output_bps     = 16;
    m_RawProcessor->imgdata.params.document_mode  = 2;
    m_RawProcessor->imgdata.params.shot_select    = shot;

    ret = m_RawProcessor->unpack();
    if (ret != LIBRAW_SUCCESS)
    {
        m_RawProcessor->recycle();
        return -1;
    }

    if (fullSensorImage)
    {
        ret = m_RawProcessor->add_masked_borders_to_bitmap();
        if (ret != LIBRAW_SUCCESS)
        {
            m_RawProcessor->recycle();
            return -1;
        }
    }

    bool fujiRotate90 = false;
    if ((0 == memcmp("FUJIFILM", m_RawProcessor->imgdata.idata.make, std::min((size_t)8, sizeof(m_RawProcessor->imgdata.idata.make)))) &&
            (2 == m_RawProcessor->COLOR(0, 1)) &&
            (1 == m_RawProcessor->COLOR(1, 0)))
    {
        fujiRotate90 = true;
    }

    *imgdata = m_RawProcessor->imgdata;
    if (fujiRotate90)
    {
        imgdata->sizes.iheight = m_RawProcessor->imgdata.sizes.iwidth;
        imgdata->sizes.iwidth = m_RawProcessor->imgdata.sizes.iheight;
        imgdata->sizes.flip = 6;
    }

    if (m_RawProcessor->imgdata.idata.filters == 0)
    {
        size_t s = imgdata->sizes.iwidth * imgdata->sizes.iheight * imgdata->idata.colors;
        rawdata->resize(s);

        for (unsigned int row = 0; row < imgdata->sizes.iheight; row++)
        {
            for (unsigned int col = 0; col < imgdata->sizes.iwidth; col++)
            {
                for (int color = 0; color < imgdata->idata.colors; color++)
                {
                    unsigned int idx = (row * imgdata->sizes.iwidth + col) * imgdata->idata.colors + color;
                    (*rawdata)[idx] = imgdata->image[row * imgdata->sizes.iwidth + col][color];
                }
            }
        }
    }
    else
    {
        size_t s = imgdata->sizes.iwidth * imgdata->sizes.iheight;
        rawdata->resize(s);

        if (!m_RawProcessor->imgdata.idata.cdesc[3])
            m_RawProcessor->imgdata.idata.cdesc[3] = 'G';

        for (unsigned int row = 0; row < imgdata->sizes.iheight; row++)
        {
            for (unsigned int col = 0; col < imgdata->sizes.iwidth; col++)
            {
                if (false == fujiRotate90)
                {
                    unsigned int idx = row * imgdata->sizes.iwidth + col;
                    (*rawdata)[idx] = imgdata->image[row * imgdata->sizes.iwidth + col][m_RawProcessor->COLOR(row, col)];
                }
                else
                {
                    unsigned int idx = col * imgdata->sizes.iheight + row;
                    (*rawdata)[idx] = imgdata->image[row * imgdata->sizes.iwidth + col][m_RawProcessor->COLOR(row, col)];
                }
            }
        }
    }
    m_RawProcessor->recycle();

    return 0;
}
