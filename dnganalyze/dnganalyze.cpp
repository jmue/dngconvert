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

#include "stdio.h"
//#include "string.h"
#include <string>
#include "assert.h"

#include "dnghost.h"
#include "dngimagewriter.h"

#include "dng_camera_profile.h"
#include "dng_color_space.h"
#include "dng_file_stream.h"
#include "dng_image.h"
#include "dng_info.h"
#include "dng_memory_stream.h"
#include "dng_opcodes.h"
#include "dng_opcode_list.h"
#include "dng_parse_utils.h"
#include "dng_string.h"
#include "dng_render.h"
#include "dng_xmp_sdk.h"

#include "zlib.h"
#define CHUNK 65536

#ifdef WIN32
#define snprintf _snprintf
#include <windows.h>
#endif

int main(int argc, const char* argv [])
{
    if(argc == 1)
    {
        fprintf(stderr,
                "\n"
                "dnganalyze - DNG file analyzer tool\n"
                "Usage: %s [options] <dngfile>\n"
                "Valid options:\n"
                "  -o            extract embedded original\n"
                "  -i            extract ifd images\n",
                argv[0]);

        return -1;
    }

    int32 index;
    bool extractOriginal = false;
    bool extractIfd = false;
    for (index = 1; index < argc && argv[index][0] == '-'; index++)
    {
        std::string option = &argv[index][1];

        if (0 == strcmp(option.c_str(), "o"))
        {
            extractOriginal = true;
        }

        if (0 == strcmp(option.c_str(), "i"))
        {
            extractIfd = true;
        }
    }

    if (index == argc)
    {
        fprintf (stderr, "*** No file specified\n");
        return 1;
    }

    const char* fileName = argv[index];

    dng_xmp_sdk::InitializeSDK();

    dng_file_stream stream(fileName);
    DngHost host;
    host.SetKeepOriginalFile(true);

    AutoPtr<dng_negative> negative;
    {
        dng_info info;
        info.Parse(host, stream);
        info.PostParse(host);

        if (!info.IsValidDNG())
        {
            return dng_error_bad_format;
        }

        negative.Reset(host.Make_dng_negative());
        negative->Parse(host, stream, info);
        negative->PostParse(host, stream, info);

        printf("Model: %s\n", negative->ModelName().Get());
        printf("\n");
        dng_rect defaultCropArea = negative->DefaultCropArea();
        dng_rect activeArea = negative->GetLinearizationInfo()->fActiveArea;
        printf("FinalImageSize: %i x %i\n", negative->DefaultFinalWidth(), negative->DefaultFinalHeight());
        printf("RawImageSize: %i x %i\n", info.fIFD[info.fMainIndex]->fImageWidth, info.fIFD[info.fMainIndex]->fImageLength);
        printf("ActiveArea: %i, %i : %i x %i\n", activeArea.t, activeArea.l, activeArea.W(), activeArea.H());
        printf("DefaultCropArea: %i, %i : %i x %i\n", defaultCropArea.t, defaultCropArea.l, defaultCropArea.W(), defaultCropArea.H());
        printf("\n");
        printf("OriginalData: %i bytes\n", negative->OriginalRawFileDataLength());
        printf("PrivateData: %i bytes\n", negative->PrivateLength());
        printf("\n");
        printf("CameraProfiles: %i\n", negative->ProfileCount());
        for (uint32 i = 0; i < negative->ProfileCount(); i++)
        {
            printf("  Profile: %i\n", i);
            dng_camera_profile dcp = negative->ProfileByIndex(i);
            printf("    Name: %s\n", dcp.Name().Get());
            printf("    Copyright: %s\n", dcp.Copyright().Get());
        }
        printf("\n");
        printf("Opcodes(1): %i\n", info.fIFD[info.fMainIndex]->fOpcodeList1Count);
        printf("Opcodes(2): %i\n", info.fIFD[info.fMainIndex]->fOpcodeList2Count);
        printf("Opcodes(3): %i\n", info.fIFD[info.fMainIndex]->fOpcodeList3Count);
        printf("\n");
        printf("MainImage: %i\n", info.fMainIndex);
        printf("ChainedCount: %i\n", info.fChainedIFDCount);
        printf("\n");
        for (uint32 ifdIdx = 0; ifdIdx < info.fIFDCount; ifdIdx++)
        {
            dng_ifd &ifd = *info.fIFD[ifdIdx].Get();

            printf("IFD: %i\n", ifdIdx);
            printf("  ImageWidth: %i\n", ifd.fImageWidth);
            printf("  ImageLength: %i\n", ifd.fImageLength);
            printf("  BitsPerSample:");
            for (uint32 i = 0; i < ifd.fSamplesPerPixel; i++)
            {
                printf(" %i", ifd.fBitsPerSample[i]);
            }
            printf("\n");
            printf("  Compression: %s\n", LookupCompression(ifd.fCompression));
            printf("  PhotometricInterpretation: %s\n", LookupPhotometricInterpretation(ifd.fPhotometricInterpretation));
            printf("  SamplesPerPixel: %i\n", ifd.fSamplesPerPixel);
            printf("  PlanarConfiguration: %i\n", ifd.fPlanarConfiguration);
            printf("  LinearizationTableCount: %i\n", ifd.fLinearizationTableCount);
            printf("  LinearizationTableType: %i\n", ifd.fLinearizationTableType);

            printf("\n");

            if (true == extractIfd)
            {
                if ((ifd.fPlanarConfiguration == pcInterleaved) &&
                        (ifd.fCompression == ccJPEG) &&
                        (ifd.fSamplesPerPixel == 3) &&
                        (ifd.fBitsPerSample[0] == 8) &&
                        (ifd.fBitsPerSample[1] == 8) &&
                        (ifd.fBitsPerSample[2] == 8) &&
                        (ifd.TilesAcross() == 1) &&
                        (ifd.TilesDown() == 1))
                {
                    uint64 tileOffset  = ifd.fTileOffset[0];
                    uint64 tileLength  = ifd.fTileByteCount[0];

                    uint8* pBuffer = new uint8[tileLength];
                    stream.SetReadPosition(tileOffset);
                    stream.Get(pBuffer, tileLength);

                    char outfn2[1024];
                    snprintf(outfn2, sizeof(outfn2), "%s-ifd%#08x.jpeg", fileName, ifdIdx);

                    dng_file_stream streamOF2(outfn2, true);
                    streamOF2.Put(pBuffer, tileLength);

                    delete[] pBuffer;
                }
                else
                {
                    AutoPtr<dng_image> image;

                    image.Reset(host.Make_dng_image(ifd.Bounds(), ifd.fSamplesPerPixel, ifd.PixelType()));
                    ifd.ReadImage(host, stream, *image.Get());

                    char outfn[1024];
                    snprintf(outfn, sizeof(outfn), "%s-ifd%#08x.tiff", fileName, ifdIdx);

                    dng_file_stream streamOF(outfn, true);

                    DngImageWriter writer;
                    writer.WriteTIFF(host, streamOF, *image.Get(), image->Planes() >= 3 ? piRGB : piBlackIsZero, ccUncompressed);
                }
            }
        }


        dng_string originalFileName = negative->OriginalRawFileName();
        std::string strTmp = originalFileName.Get();
        uint32 originalDataLength = negative->OriginalRawFileDataLength();
        const void* originalData = negative->OriginalRawFileData();

        if (true == extractOriginal)
        {
            if(originalDataLength > 0)
            {
                dng_memory_allocator memalloc(gDefaultDNGMemoryAllocator);
                dng_memory_stream compressedDataStream(memalloc);
                compressedDataStream.Put(originalData, originalDataLength);
                compressedDataStream.SetReadPosition(0);
                compressedDataStream.SetBigEndian(true);
                uint32 forkLength = compressedDataStream.Get_uint32();
                uint32 forkBlocks = (uint32)floor((forkLength + 65535.0) / 65536.0);
                std::vector<uint32> offsets;
                for(uint32 block = 0; block <= forkBlocks; block++)
                {
                    uint32 offset = compressedDataStream.Get_uint32();
                    offsets.push_back(offset);
                }

                dng_file_stream originalDataStream(strTmp.c_str(), true);

                int ret;
                z_stream zstrm;
                unsigned char inBuffer[CHUNK * 2];
                unsigned char outBuffer[CHUNK];

                for (uint32 block = 0; block < forkBlocks; block++)
                {
                    uint32 compressedBlockLength = offsets[block + 1] - offsets[block];
                    compressedDataStream.SetReadPosition(offsets[block]);
                    compressedDataStream.Get(inBuffer, compressedBlockLength);

                    /* allocate inflate state */
                    zstrm.zalloc = Z_NULL;
                    zstrm.zfree = Z_NULL;
                    zstrm.opaque = Z_NULL;
                    zstrm.avail_in = 0;
                    zstrm.next_in = Z_NULL;
                    ret = inflateInit(&zstrm);
                    if (ret != Z_OK)
                        return ret;

                    /* decompress */
                    zstrm.avail_in = compressedBlockLength;
                    if (zstrm.avail_in == 0)
                        break;
                    zstrm.next_in = inBuffer;

                    zstrm.avail_out = CHUNK;
                    zstrm.next_out = outBuffer;
                    ret = inflate(&zstrm, Z_NO_FLUSH);
                    assert(ret == Z_STREAM_END);

                    uint32 originalBlockLength = zstrm.total_out;

                    /* clean up */
                    (void)inflateEnd(&zstrm);

                    originalDataStream.Put(outBuffer, originalBlockLength);
                }
            }
            else
            {
                fprintf(stderr, "no embedded originals found\n");
            }
        }
    }

    dng_xmp_sdk::TerminateSDK();

    return 0;
}

