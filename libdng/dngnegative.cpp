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

#include "dngnegative.h"
#include "dngmosaicinfo.h"

DngNegative::DngNegative(dng_memory_allocator &allocator) :
    dng_negative(allocator)
{
}

DngNegative::~DngNegative(void)
{
}

dng_mosaic_info* DngNegative::MakeMosaicInfo()
{
    DngMosaicInfo* info = new DngMosaicInfo();

    if (!info)
    {
        ThrowMemoryFull();
    }

    return info;
}

dng_negative* DngNegative::Make(dng_memory_allocator &allocator)
{
    AutoPtr<DngNegative> result(new DngNegative(allocator));

    if (!result.Get())
    {
        ThrowMemoryFull();
    }

    result->Initialize();

    return result.Release();
}


