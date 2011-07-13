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
}

RawHelper::~RawHelper(void)
{
}

int RawHelper::identifyRawData(const char* fname, 
                               libraw_data_t* imgdata)
{
    AutoPtr<LibRaw> rawProcessor(new LibRaw());

    int ret = rawProcessor->open_file(fname);
    if (ret != LIBRAW_SUCCESS)
    {
        printf("Cannot open %s: %s\n", fname, libraw_strerror(ret));
        rawProcessor->recycle();
        return -1;
    }

    ret = rawProcessor->adjust_sizes_info_only();
    if (ret != LIBRAW_SUCCESS)
    {
        printf("LibRaw: failed to run adjust_sizes_info_only: %s", libraw_strerror(ret));
        rawProcessor->recycle();
        return -1;
    }

    *imgdata = rawProcessor->imgdata;
    rawProcessor->recycle();
    return 0;
}
