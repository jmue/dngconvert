// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2008 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================
#include "PNG_Support.hpp"
#include <string.h>

typedef std::basic_string<unsigned char> filebuffer;

namespace CRC
{
	/* Table of CRCs of all 8-bit messages. */
	static unsigned long crc_table[256];

	/* Flag: has the table been computed? Initially false. */
	static int crc_table_computed = 0;

	/* Make the table for a fast CRC. */
	static void make_crc_table(void)
	{
		unsigned long c;
		int n, k;

		for (n = 0; n < 256; n++)
		{
			c = (unsigned long) n;
			for (k = 0; k < 8; k++)
			{
				if (c & 1)
				{
					c = 0xedb88320L ^ (c >> 1);
				}
				else
				{
					c = c >> 1;
				}
			}
			crc_table[n] = c;
		}
		crc_table_computed = 1;
	}

	/* Update a running CRC with the bytes buf[0..len-1]--the CRC
	  should be initialized to all 1's, and the transmitted value
	  is the 1's complement of the final running CRC (see the
	  crc() routine below). */

	static unsigned long update_crc(unsigned long crc, unsigned char *buf, int len)
	{
		unsigned long c = crc;
		int n;

		if (!crc_table_computed)
		{
			make_crc_table();
		}

		for (n = 0; n < len; n++)
		{
			c = crc_table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
		}

		return c;
	}

	/* Return the CRC of the bytes buf[0..len-1]. */
	static unsigned long crc(unsigned char *buf, int len)
	{
		return update_crc(0xffffffffL, buf, len) ^ 0xffffffffL;
	}
} // namespace CRC

namespace PNG_Support
{
	enum chunkType {
		// Critical chunks - (shall appear in this order, except PLTE is optional)
		IHDR = 'IHDR',
		PLTE = 'PLTE',
		IDAT = 'IDAT',
		IEND = 'IEND',
		// Ancillary chunks - (need not appear in this order)
		cHRM = 'cHRM',
		gAMA = 'gAMA',
		iCCP = 'iCCP',
		sBIT = 'sBIT',
		sRGB = 'sRGB',
		bKGD = 'bKGD',
		hIST = 'hIST',
		tRNS = 'tRNS',
		pHYs = 'pHYs',
		sPLT = 'sPLT',
		tIME = 'tIME',
		iTXt = 'iTXt',
		tEXt = 'tEXt',
		zTXt = 'zTXt'

	};

	// =============================================================================================

	long OpenPNG ( LFA_FileRef fileRef, ChunkState & inOutChunkState )
	{
		XMP_Uns64 pos = 0;
		long name;
		XMP_Uns32 len;
	
		pos = LFA_Seek ( fileRef, 8, SEEK_SET );
		if (pos != 8) return 0;
	
		// read first and following chunks
		while ( ReadChunk ( fileRef, inOutChunkState, &name, &len, pos) ) {}
	
		return (long)inOutChunkState.chunks.size();

	}

	// =============================================================================================

	bool  ReadChunk ( LFA_FileRef fileRef, ChunkState & inOutChunkState, long * chunkType, XMP_Uns32 * chunkLength, XMP_Uns64 & inOutPosition )
	{
		try
		{
			XMP_Uns64 startPosition = inOutPosition;
			long bytesRead;
			char buffer[4];

			bytesRead = LFA_Read ( fileRef, buffer, 4 );
			if ( bytesRead != 4 ) return false;
			inOutPosition += 4;
			*chunkLength = GetUns32BE(buffer);

			bytesRead = LFA_Read ( fileRef, buffer, 4 );
			if ( bytesRead != 4 ) return false;
			inOutPosition += 4;
			*chunkType = GetUns32BE(buffer);

			inOutPosition += *chunkLength;

			bytesRead = LFA_Read ( fileRef, buffer, 4 );
			if ( bytesRead != 4 ) return false;
			inOutPosition += 4;
			long crc = GetUns32BE(buffer);

			ChunkData	newChunk;
	
			newChunk.pos = startPosition;
			newChunk.len = *chunkLength;
			newChunk.type = *chunkType;

			// check for XMP in iTXt-chunk
			if (newChunk.type == iTXt)
			{
				CheckiTXtChunkHeader(fileRef, inOutChunkState, newChunk);
			}

			inOutChunkState.chunks.push_back ( newChunk );

			LFA_Seek ( fileRef, inOutPosition, SEEK_SET );

		} catch ( ... ) {

			return false;

		}
	
		return true;

	}

	// =============================================================================================

	bool WriteXMPChunk ( LFA_FileRef fileRef, XMP_Uns32 len, const char* inBuffer )
	{
		bool ret = false;
		unsigned long datalen = (4 + ITXT_HEADER_LEN + len);
		unsigned char* buffer = new unsigned char[datalen];

		try
		{
			size_t pos = 0;
			memcpy(&buffer[pos], ITXT_CHUNK_TYPE, 4);
			pos += 4;
			memcpy(&buffer[pos], ITXT_HEADER_DATA, ITXT_HEADER_LEN);
			pos += ITXT_HEADER_LEN;
			memcpy(&buffer[pos], inBuffer, len);

			unsigned long crc_value = MakeUns32BE( CalculateCRC( buffer, datalen ));
			datalen -= 4;
			unsigned long len_value = MakeUns32BE( datalen );
			datalen += 4;

			LFA_Write(fileRef, &len_value, 4);
			LFA_Write(fileRef, buffer, datalen);
			LFA_Write(fileRef, &crc_value, 4);

			ret = true;
		}
		catch ( ... ) {}

		delete [] buffer;

		return ret;
	}

	// =============================================================================================

	bool CopyChunk ( LFA_FileRef sourceRef, LFA_FileRef destRef, ChunkData& chunk )
	{
		try
		{
			LFA_Seek (sourceRef, chunk.pos, SEEK_SET );
			LFA_Copy (sourceRef, destRef, (chunk.len + 12));

		} catch ( ... ) {

			return false;

		}
	
		return true;
	}

	// =============================================================================================

	unsigned long UpdateChunkCRC( LFA_FileRef fileRef, ChunkData& inOutChunkData )
	{
		unsigned long ret = 0;
		unsigned long datalen = (inOutChunkData.len + 4);
		unsigned char* buffer = new unsigned char[datalen];

		try
		{
			LFA_Seek(fileRef, (inOutChunkData.pos + 4), SEEK_SET);

			size_t pos = 0;
			long bytesRead = LFA_Read ( fileRef, &buffer[pos], (inOutChunkData.len + 4) );

			unsigned long crc = CalculateCRC( buffer, (inOutChunkData.len + 4) );
			unsigned long crc_value = MakeUns32BE( crc );

			LFA_Seek(fileRef, (inOutChunkData.pos + 4 + 4 + inOutChunkData.len), SEEK_SET);
			LFA_Write(fileRef, &crc_value, 4);

			ret = crc;
		}
		catch ( ... ) {}

		delete [] buffer;

		return ret;
	}

	// =============================================================================================

	bool CheckIHDRChunkHeader ( ChunkData& inOutChunkData )
	{
		return (inOutChunkData.type == IHDR);
	}

	// =============================================================================================

	unsigned long CheckiTXtChunkHeader ( LFA_FileRef fileRef, ChunkState& inOutChunkState, ChunkData& inOutChunkData )
	{
		try
		{
			LFA_Seek(fileRef, (inOutChunkData.pos + 8), SEEK_SET);

			char buffer[ITXT_HEADER_LEN];
			long bytesRead = LFA_Read ( fileRef, buffer, ITXT_HEADER_LEN );

			if (bytesRead == ITXT_HEADER_LEN)
			{
				if (memcmp(buffer, ITXT_HEADER_DATA, ITXT_HEADER_LEN) == 0)
				{
					// return length of XMP
					if (inOutChunkData.len > ITXT_HEADER_LEN)
					{
						inOutChunkState.xmpPos = inOutChunkData.pos + 8 + ITXT_HEADER_LEN;
						inOutChunkState.xmpLen = inOutChunkData.len - ITXT_HEADER_LEN;
						inOutChunkState.xmpChunk = inOutChunkData;
						inOutChunkData.xmp = true;

						return inOutChunkState.xmpLen;
					}
				}
			}
		}
		catch ( ... ) {}
	
		return 0;
	}

	bool ReadBuffer ( LFA_FileRef fileRef, XMP_Uns64 & pos, XMP_Uns32 len, char * outBuffer )
	{
		try
		{
			if ( (fileRef == 0) || (outBuffer == 0) ) return false;
		
			LFA_Seek (fileRef, pos, SEEK_SET );
			long bytesRead = LFA_Read ( fileRef, outBuffer, len );
			if ( XMP_Uns32(bytesRead) != len ) return false;
		
			return true;
		}
		catch ( ... ) {}

		return false;
	}

	bool WriteBuffer ( LFA_FileRef fileRef, XMP_Uns64 & pos, XMP_Uns32 len, const char * inBuffer )
	{
		try
		{
			if ( (fileRef == 0) || (inBuffer == 0) ) return false;
		
			LFA_Seek (fileRef, pos, SEEK_SET );
			LFA_Write( fileRef, inBuffer, len );
		
			return true;
		}
		catch ( ... ) {}

		return false;
	}

	unsigned long CalculateCRC( unsigned char* inBuffer, XMP_Uns32 len )
	{
		return CRC::update_crc(0xffffffffL, inBuffer, len) ^ 0xffffffffL;
	}

} // namespace PNG_Support
