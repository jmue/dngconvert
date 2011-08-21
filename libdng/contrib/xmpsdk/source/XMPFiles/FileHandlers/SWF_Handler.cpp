// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2006 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "SWF_Handler.hpp"

#include "SWF_Support.hpp"


using namespace std;

// =================================================================================================
/// \file SWF_Handler.hpp
/// \brief File format handler for SWF.
///
/// This handler ...
///
// =================================================================================================

// =================================================================================================
// SWF_MetaHandlerCTor
// ====================

XMPFileHandler * SWF_MetaHandlerCTor ( XMPFiles * parent )
{
	return new SWF_MetaHandler ( parent );

}	// SWF_MetaHandlerCTor

// =================================================================================================
// SWF_CheckFormat
// ===============

bool SWF_CheckFormat ( XMP_FileFormat format,
					   XMP_StringPtr  filePath,
                       LFA_FileRef    fileRef,
                       XMPFiles *     parent )
{
	IgnoreParam(format); IgnoreParam(fileRef); IgnoreParam(parent); 
	XMP_Assert ( format == kXMP_SWFFile );

	IOBuffer ioBuf;
	
	LFA_Seek ( fileRef, 0, SEEK_SET );
	if ( ! CheckFileSpace ( fileRef, &ioBuf, SWF_SIGNATURE_LEN ) ) return false;

	if ( !(CheckBytes ( ioBuf.ptr, SWF_F_SIGNATURE_DATA, SWF_SIGNATURE_LEN ) || 
		  CheckBytes(ioBuf.ptr, SWF_C_SIGNATURE_DATA, SWF_SIGNATURE_LEN)) ) 
			return false;

	return true;

}	// SWF_CheckFormat

// =================================================================================================
// SWF_MetaHandler::SWF_MetaHandler
// ==================================

SWF_MetaHandler::SWF_MetaHandler ( XMPFiles * _parent )
{
	this->parent = _parent;
	this->handlerFlags = kSWF_HandlerFlags;
	this->stdCharForm  = kXMP_Char8Bit;

}

// =================================================================================================
// SWF_MetaHandler::~SWF_MetaHandler
// ===================================

SWF_MetaHandler::~SWF_MetaHandler()
{
}

// =================================================================================================
// SWF_MetaHandler::CacheFileData
// ===============================

void SWF_MetaHandler::CacheFileData()
{
	
	this->containsXMP = false;

	LFA_FileRef fileRef ( this->parent->fileRef );
	if ( fileRef == 0) return;
	
	SWF_Support::FileInfo fileInfo(fileRef, this->parent->filePath);
	IO::InputStream * is = NULL;
	if(fileInfo.IsCompressed())
	{
		XMP_Uns32 fileSize = fileInfo.GetSize();
		is = new IO::ZIP::DeflateInputStream(fileRef, fileSize);
		
		IO::ZIP::DeflateInputStream * in = dynamic_cast<IO::ZIP::DeflateInputStream*>(is);
		in->Skip(SWF_COMPRESSION_BEGIN, IO::ZIP::DEFLATE_NO);
	}
	else
	{
		is = new IO::FileInputStream(fileRef);
		is->Skip(SWF_COMPRESSION_BEGIN);
	}

	SWF_Support::TagState tagState;
	//important flag to stop iteration over all tags when xmp flag within FileAttributeTag isn't set
	tagState.cachingFile = true;

	long numTags = SWF_Support::OpenSWF ( is, tagState );
	
	is->Close();
	delete is;

	if ( numTags == 0 ) return;

	if (tagState.hasXMP && tagState.xmpLen != 0)
	{
		this->xmpPacket.assign(tagState.xmpPacket);
		this->containsXMP = true;
	}
	else
	{
		// no XMP
	}
	

}	// SWF_MetaHandler::CacheFileData

// =================================================================================================
// SWF_MetaHandler::ProcessXMP
// ============================
//
// Process the raw XMP and legacy metadata that was previously cached.

void SWF_MetaHandler::ProcessXMP()
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

}	// SWF_MetaHandler::ProcessXMP


// =================================================================================================
// XMPFileHandler::GetSerializeOptions
// ===================================
//
// Override default implementation to ensure omitting xmp wrapper
// 
XMP_OptionBits SWF_MetaHandler::GetSerializeOptions()
{
	return (kXMP_OmitPacketWrapper | kXMP_OmitAllFormatting | kXMP_OmitXMPMetaElement);
} // XMPFileHandler::GetSerializeOptions


// =================================================================================================
// SWF_MetaHandler::UpdateFile
// ============================

void SWF_MetaHandler::UpdateFile ( bool doSafeUpdate )
{
	bool updated = false;
	
	if ( ! this->needsUpdate ) return;
	if ( doSafeUpdate ) XMP_Throw ( "SWF_MetaHandler::UpdateFile: Safe update not supported", kXMPErr_Unavailable );

	LFA_FileRef sourceRef = this->parent->fileRef;
	std::string sourcePath = this->parent->filePath;

	SWF_Support::FileInfo fileInfo(sourceRef, sourcePath);
	
	if(fileInfo.IsCompressed())
		sourceRef = fileInfo.Decompress();

	IO::InputStream * is = new IO::FileInputStream(sourceRef);
	//processing SWF starts after byte SWF_COMPRESSION_BEGIN - currently 8 bytes
	is->Skip(SWF_COMPRESSION_BEGIN);
	
	SWF_Support::TagState tagState;
	
	long numTags = SWF_Support::OpenSWF ( is, tagState );
	
	//clean objects
	is->Close();
	delete is;

	bool foundTag = false;
	SWF_Support::TailBufferDef tailBuffer;
	

	//find end position to measure tail buffer
	tailBuffer.tailEndPosition = LFA_Seek(sourceRef, 0, SEEK_END);

	SWF_Support::TagIterator curPos = tagState.tags.begin();
	SWF_Support::TagIterator endPos = tagState.tags.end();

	for( ;(curPos != endPos) && ! foundTag; ++curPos)
	{
		SWF_Support::TagData tag = *curPos;
		
		//write XMP Tag at the beginning of the file
		if(! tagState.hasXMP && ! tagState.hasFileAttrTag)
		{
			tailBuffer.tailStartPosition = tag.pos;
			tailBuffer.writePosition = tag.pos;
			foundTag = true;
		}
		//update existing XMP Tag
		if(tagState.hasXMP && (tagState.xmpTag.pos == tag.pos))
		{
			++curPos;
			SWF_Support::TagData nextTag = *curPos;
			tailBuffer.tailStartPosition = nextTag.pos;
			tailBuffer.writePosition = tagState.xmpTag.pos;
			foundTag = true;
		}
		//write XMP Tag after FileAttribute Tag
		else if(! tagState.hasXMP && (tag.id == SWF_TAG_ID_FILEATTRIBUTES))
		{
			++curPos;
			SWF_Support::TagData nextTag = *curPos;
			tailBuffer.tailStartPosition = nextTag.pos;
			tailBuffer.writePosition = nextTag.pos;
			foundTag = true;
		}
	}

	//cache tail of file
	XMP_Assert(tailBuffer.tailEndPosition > tailBuffer.tailStartPosition);
	XMP_Uns32 tailSize = tailBuffer.GetTailSize();
	XMP_Uns8 * buffer = new XMP_Uns8[tailSize];
	SWF_Support::ReadBuffer(sourceRef, tailBuffer.tailStartPosition, tailSize, buffer);

	//write new XMP packet
	XMP_StringPtr packetStr = xmpPacket.c_str();
	XMP_StringLen packetLen = (XMP_StringLen)xmpPacket.size();

	LFA_Seek(sourceRef, tailBuffer.writePosition, SEEK_SET);
	SWF_Support::WriteXMPTag(sourceRef, packetLen, packetStr);

	// truncate to minimal size
	LFA_Truncate(sourceRef, LFA_Tell(sourceRef));
	
	//move tail of the file
	LFA_Write(sourceRef, buffer, tailSize);
	
	//cleaning buffer
	delete [] buffer;

	//update FileAttribute Tag if exists
	if(tagState.hasFileAttrTag)
		SWF_Support::UpdateFileAttrTag(sourceRef, tagState.fileAttrTag, tagState);

	
	//update File Size
	SWF_Support::UpdateHeader(sourceRef);

	//compress file after writing XMP
	if(fileInfo.IsCompressed())
	{
		fileInfo.Compress(sourceRef, this->parent->fileRef);
		fileInfo.Clean();
	}
	
	
	if ( ! updated )return;	// If there's an error writing the chunk, bail.

	this->needsUpdate = false;

}	// SWF_MetaHandler::UpdateFile

// =================================================================================================
// SWF_MetaHandler::WriteFile
// ===========================

void SWF_MetaHandler::WriteFile ( LFA_FileRef sourceRef, const std::string & sourcePath )
{

	LFA_FileRef destRef = this->parent->fileRef;

	SWF_Support::TagState tagState;

	LFA_FileRef origRef(destRef);
	std::string updatePath;
	

	SWF_Support::FileInfo fileInfo(sourceRef, sourcePath);
	
	if(fileInfo.IsCompressed())
	{
		sourceRef = fileInfo.Decompress();
		CreateTempFile ( sourcePath, &updatePath, kCopyMacRsrc );
		destRef = LFA_Open ( updatePath.c_str(), 'w' );
	}

	IO::InputStream * is = NULL;
	
	is = new IO::FileInputStream(sourceRef);
	is->Skip(SWF_COMPRESSION_BEGIN);
	
	long numTags = SWF_Support::OpenSWF( is, tagState );

	is->Close();
	delete is;

	if ( numTags == 0 ) return;

	LFA_Truncate(destRef, 0);
	SWF_Support::CopyHeader(sourceRef, destRef, tagState);

	SWF_Support::TagIterator curPos = tagState.tags.begin();
	SWF_Support::TagIterator endPos = tagState.tags.end();

	XMP_StringPtr packetStr = xmpPacket.c_str();
	XMP_StringLen packetLen = (XMP_StringLen)xmpPacket.size();

	bool copying = true;
	bool isXMPTagWritten = false;

	for (; (curPos != endPos); ++curPos)
	{
		SWF_Support::TagData tag = *curPos;

		// write XMP tag right after FileAttribute Tag if no XMP tag exists
		if (! tagState.hasXMP &&  (tag.id == SWF_TAG_ID_FILEATTRIBUTES))
			SWF_Support::WriteXMPTag(destRef, packetLen, packetStr );

		//no FileAttribute Tag and no XMP tag write XMP Tag at the beginning of the file
		if(!tagState.hasXMP && !tagState.hasFileAttrTag && ! isXMPTagWritten)
		{
			isXMPTagWritten = true;
			SWF_Support::WriteXMPTag(destRef, packetLen, packetStr );
		}
		
		// write XMP tag where old XMP exists
		if(tagState.hasXMP && (tag.pos == tagState.xmpTag.pos))
		{
			copying = false;
			SWF_Support::WriteXMPTag(destRef, packetLen, packetStr );
		}

		// copy any other chunk
		if(copying)
			SWF_Support::CopyTag(sourceRef, destRef, tag);

		copying = true;
	}

	// update FileAttribute Tag in new file
	if(tagState.hasFileAttrTag)
		SWF_Support::UpdateFileAttrTag(destRef, tagState.fileAttrTag, tagState);

	
	// update file header by measuring new file size
	SWF_Support::UpdateHeader(origRef);

	//compress re-written file
	if(fileInfo.IsCompressed())
	{
		fileInfo.Compress(destRef, origRef);
		fileInfo.Clean();
		LFA_Close(destRef);
		LFA_Delete(updatePath.c_str());
	}

	


}	// SWF_MetaHandler::WriteFile

