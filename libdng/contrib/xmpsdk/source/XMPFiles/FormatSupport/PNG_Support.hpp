#ifndef __PNG_Support_hpp__
#define __PNG_Support_hpp__ 1

// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2007 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XMP_Environment.h"	// ! This must be the first include.

#include "XMPFiles_Impl.hpp"

#define PNG_SIGNATURE_LEN		8
#define PNG_SIGNATURE_DATA		"\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

#define ITXT_CHUNK_TYPE			"iTXt"

#define ITXT_HEADER_LEN			22
#define ITXT_HEADER_DATA		"XML:com.adobe.xmp\0\0\0\0\0"

namespace PNG_Support 
{
	class ChunkData
	{
		public:
			ChunkData() : pos(0), len(0), type(0), xmp(false) {}
			virtual ~ChunkData() {}

			// | length |  type  |    data     | crc(type+data) |
			// |   4    |   4    | val(length) |       4        |
			//
			XMP_Uns64	pos;		// file offset of chunk
			XMP_Uns32	len;		// length of chunk data
			long		type;		// name/type of chunk
			bool		xmp;		// iTXt-chunk with XMP ?
	};

	typedef std::vector<ChunkData> ChunkVector;
	typedef ChunkVector::iterator ChunkIterator;

	class ChunkState
	{
		public:
			ChunkState() : xmpPos(0), xmpLen(0) {}
			virtual ~ChunkState() {}

			XMP_Uns64	xmpPos;
			XMP_Uns32	xmpLen;
			ChunkData	xmpChunk;
			ChunkVector chunks;	/* vector of chunks */
	};

	long OpenPNG ( LFA_FileRef fileRef, ChunkState& inOutChunkState );

	bool ReadChunk ( LFA_FileRef fileRef, ChunkState& inOutChunkState, long* chunkType, XMP_Uns32* chunkLength, XMP_Uns64& inOutPosition );
	bool WriteXMPChunk ( LFA_FileRef fileRef, XMP_Uns32 len, const char* inBuffer );
	bool CopyChunk ( LFA_FileRef sourceRef, LFA_FileRef destRef, ChunkData& chunk );
	unsigned long UpdateChunkCRC( LFA_FileRef fileRef, ChunkData& inOutChunkData );

	bool CheckIHDRChunkHeader ( ChunkData& inOutChunkData );
	unsigned long CheckiTXtChunkHeader ( LFA_FileRef fileRef, ChunkState& inOutChunkState, ChunkData& inOutChunkData );

	bool ReadBuffer ( LFA_FileRef fileRef, XMP_Uns64& pos, XMP_Uns32 len, char* outBuffer );
	bool WriteBuffer ( LFA_FileRef fileRef, XMP_Uns64& pos, XMP_Uns32 len, const char* inBuffer );

	unsigned long CalculateCRC( unsigned char* inBuffer, XMP_Uns32 len );

} // namespace PNG_Support

#endif	// __PNG_Support_hpp__
