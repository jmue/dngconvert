// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2006 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "PNG_Handler.hpp"

#include "PNG_Support.hpp"

using namespace std;

// =================================================================================================
/// \file PNG_Handler.hpp
/// \brief File format handler for PNG.
///
/// This handler ...
///
// =================================================================================================

// =================================================================================================
// PNG_MetaHandlerCTor
// ====================

XMPFileHandler * PNG_MetaHandlerCTor ( XMPFiles * parent )
{
	return new PNG_MetaHandler ( parent );

}	// PNG_MetaHandlerCTor

// =================================================================================================
// PNG_CheckFormat
// ===============

bool PNG_CheckFormat ( XMP_FileFormat format,
					   XMP_StringPtr  filePath,
                       LFA_FileRef    fileRef,
                       XMPFiles *     parent )
{
	IgnoreParam(format); IgnoreParam(fileRef); IgnoreParam(parent);
	XMP_Assert ( format == kXMP_PNGFile );

	IOBuffer ioBuf;
	
	LFA_Seek ( fileRef, 0, SEEK_SET );
	if ( ! CheckFileSpace ( fileRef, &ioBuf, PNG_SIGNATURE_LEN ) ) return false;	// We need at least 8, the buffer is filled anyway.

	if ( ! CheckBytes ( ioBuf.ptr, PNG_SIGNATURE_DATA, PNG_SIGNATURE_LEN ) ) return false;

	return true;

}	// PNG_CheckFormat

// =================================================================================================
// PNG_MetaHandler::PNG_MetaHandler
// ==================================

PNG_MetaHandler::PNG_MetaHandler ( XMPFiles * _parent )
{
	this->parent = _parent;
	this->handlerFlags = kPNG_HandlerFlags;
	this->stdCharForm  = kXMP_Char8Bit;

}

// =================================================================================================
// PNG_MetaHandler::~PNG_MetaHandler
// ===================================

PNG_MetaHandler::~PNG_MetaHandler()
{
}

// =================================================================================================
// PNG_MetaHandler::CacheFileData
// ===============================

void PNG_MetaHandler::CacheFileData()
{

	this->containsXMP = false;

	LFA_FileRef fileRef ( this->parent->fileRef );
	if ( fileRef == 0) return;

	PNG_Support::ChunkState chunkState;
	long numChunks = PNG_Support::OpenPNG ( fileRef, chunkState );
	if ( numChunks == 0 ) return;

	if (chunkState.xmpLen != 0)
	{
		// XMP present

		this->xmpPacket.reserve(chunkState.xmpLen);
		this->xmpPacket.assign(chunkState.xmpLen, ' ');

		if (PNG_Support::ReadBuffer ( fileRef, chunkState.xmpPos, chunkState.xmpLen, const_cast<char *>(this->xmpPacket.data()) ))
		{
			this->packetInfo.offset = chunkState.xmpPos;
			this->packetInfo.length = chunkState.xmpLen;
			this->containsXMP = true;
		}
	}
	else
	{
		// no XMP
	}

}	// PNG_MetaHandler::CacheFileData

// =================================================================================================
// PNG_MetaHandler::ProcessXMP
// ============================
//
// Process the raw XMP and legacy metadata that was previously cached.

void PNG_MetaHandler::ProcessXMP()
{
	this->processedXMP = true;	// Make sure we only come through here once.

	// Process the XMP packet.

	if ( ! this->xmpPacket.empty() ) {
	
		XMP_Assert ( this->containsXMP );
		XMP_StringPtr packetStr = this->xmpPacket.c_str();
		XMP_StringLen packetLen = (XMP_StringLen)this->xmpPacket.size();

		this->xmpObj.ParseFromBuffer ( packetStr, packetLen );

		this->containsXMP = true;

	}

}	// PNG_MetaHandler::ProcessXMP

// =================================================================================================
// PNG_MetaHandler::UpdateFile
// ============================

void PNG_MetaHandler::UpdateFile ( bool doSafeUpdate )
{
	bool updated = false;
	
	if ( ! this->needsUpdate ) return;
	if ( doSafeUpdate ) XMP_Throw ( "PNG_MetaHandler::UpdateFile: Safe update not supported", kXMPErr_Unavailable );
	
	XMP_StringPtr packetStr = xmpPacket.c_str();
	XMP_StringLen packetLen = (XMP_StringLen)xmpPacket.size();
	if ( packetLen == 0 ) return;

	LFA_FileRef fileRef(this->parent->fileRef);
	if ( fileRef == 0 ) return;

	PNG_Support::ChunkState chunkState;
	long numChunks = PNG_Support::OpenPNG ( fileRef, chunkState );
	if ( numChunks == 0 ) return;

	// write/update chunk
	if (chunkState.xmpLen == 0)
	{
		// no current chunk -> inject
		updated = SafeWriteFile();
	}
	else if (chunkState.xmpLen >= packetLen )
	{
		// current chunk size is sufficient -> write and update CRC (in place update)
		updated = PNG_Support::WriteBuffer(fileRef, chunkState.xmpPos, packetLen, packetStr );
		PNG_Support::UpdateChunkCRC(fileRef, chunkState.xmpChunk );
	}
	else if (chunkState.xmpLen < packetLen)
	{
		// XMP is too large for current chunk -> expand 
		updated = SafeWriteFile();
	}

	if ( ! updated )return;	// If there's an error writing the chunk, bail.

	this->needsUpdate = false;

}	// PNG_MetaHandler::UpdateFile

// =================================================================================================
// PNG_MetaHandler::WriteFile
// ===========================

void PNG_MetaHandler::WriteFile ( LFA_FileRef sourceRef, const std::string & sourcePath )
{
	LFA_FileRef destRef = this->parent->fileRef;

	PNG_Support::ChunkState chunkState;
	long numChunks = PNG_Support::OpenPNG ( sourceRef, chunkState );
	if ( numChunks == 0 ) return;

	LFA_Truncate(destRef, 0);
	LFA_Write(destRef, PNG_SIGNATURE_DATA, PNG_SIGNATURE_LEN);

	PNG_Support::ChunkIterator curPos = chunkState.chunks.begin();
	PNG_Support::ChunkIterator endPos = chunkState.chunks.end();

	for (; (curPos != endPos); ++curPos)
	{
		PNG_Support::ChunkData chunk = *curPos;

		// discard existing XMP chunk
		if (chunk.xmp)
			continue;

		// copy any other chunk
		PNG_Support::CopyChunk(sourceRef, destRef, chunk);

		// place XMP chunk immediately after IHDR-chunk
		if (PNG_Support::CheckIHDRChunkHeader(chunk))
		{
			XMP_StringPtr packetStr = xmpPacket.c_str();
			XMP_StringLen packetLen = (XMP_StringLen)xmpPacket.size();

			PNG_Support::WriteXMPChunk(destRef, packetLen, packetStr );
		}
	}

}	// PNG_MetaHandler::WriteFile

// =================================================================================================
// PNG_MetaHandler::SafeWriteFile
// ===========================

bool PNG_MetaHandler::SafeWriteFile ()
{
	bool ret = false;

	std::string origPath = this->parent->filePath;
	LFA_FileRef origRef  = this->parent->fileRef;
	
	std::string updatePath;
	LFA_FileRef updateRef = 0;
	
	CreateTempFile ( origPath, &updatePath, kCopyMacRsrc );
	updateRef = LFA_Open ( updatePath.c_str(), 'w' );

	this->parent->filePath = updatePath;
	this->parent->fileRef  = updateRef;
	
	try {
		this->WriteFile ( origRef, origPath );
		ret = true;
	} catch ( ... ) {
		LFA_Close ( updateRef );
		LFA_Delete ( updatePath.c_str() );
		this->parent->filePath = origPath;
		this->parent->fileRef  = origRef;
		throw;
	}

	LFA_Close ( origRef );
	LFA_Delete ( origPath.c_str() );

	LFA_Close ( updateRef );
	LFA_Rename ( updatePath.c_str(), origPath.c_str() );
	this->parent->filePath = origPath;

	this->parent->fileRef = 0;

	return ret;

} // PNG_MetaHandler::SafeWriteFile

// =================================================================================================
