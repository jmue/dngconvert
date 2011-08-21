// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2005 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#if XMP_WinBuild
	#pragma warning ( disable : 4996 )	// '...' was declared deprecated
#endif

#include "MPEG2_Handler.hpp"

using namespace std;

// =================================================================================================
/// \file MPEG2_Handler.cpp
/// \brief File format handler for MPEG2.
///
/// BLECH! YUCK! GAG! MPEG-2 is done using a sidecar and recognition only by file extension! BARF!!!!!
///
// =================================================================================================

// =================================================================================================
// FindFileExtension
// =================

static inline XMP_StringPtr FindFileExtension ( XMP_StringPtr filePath )
{

	XMP_StringPtr pathEnd = filePath + strlen(filePath);
	XMP_StringPtr extPtr;
	
	for ( extPtr = pathEnd-1; extPtr > filePath; --extPtr ) {
		if ( (*extPtr == '.') || (*extPtr == '/') ) break;
		#if XMP_WinBuild
			if ( (*extPtr == '\\') || (*extPtr == ':') ) break;
		#endif
	}
	
	if ( (extPtr < filePath) || (*extPtr != '.') ) return pathEnd;
	return extPtr;

}	// FindFileExtension

// =================================================================================================
// MPEG2_MetaHandlerCTor
// =====================

XMPFileHandler * MPEG2_MetaHandlerCTor ( XMPFiles * parent )
{
	return new MPEG2_MetaHandler ( parent );

}	// MPEG2_MetaHandlerCTor

// =================================================================================================
// MPEG2_CheckFormat
// =================

// The MPEG-2 handler uses just the file extension, not the file content. Worse yet, it also uses a
// sidecar file for the XMP. This works better if the handler owns the file, we open the sidecar
// instead of the actual MPEG-2 file.

bool MPEG2_CheckFormat ( XMP_FileFormat format,
						XMP_StringPtr  filePath,
						LFA_FileRef    fileRef,
						XMPFiles *     parent )
{
	IgnoreParam(format); IgnoreParam(filePath); IgnoreParam(fileRef);

	XMP_Assert ( (format == kXMP_MPEGFile) || (format == kXMP_MPEG2File) );
	XMP_Assert ( fileRef == 0 );

	return ( (parent->format == kXMP_MPEGFile) || (parent->format == kXMP_MPEGFile) );	// ! Just use the first call's format hint.

}	// MPEG2_CheckFormat

// =================================================================================================
// MPEG2_MetaHandler::MPEG2_MetaHandler
// ====================================

MPEG2_MetaHandler::MPEG2_MetaHandler ( XMPFiles * _parent )
{
	this->parent = _parent;
	this->handlerFlags = kMPEG2_HandlerFlags;
	this->stdCharForm  = kXMP_Char8Bit;

}	// MPEG2_MetaHandler::MPEG2_MetaHandler

// =================================================================================================
// MPEG2_MetaHandler::~MPEG2_MetaHandler
// =====================================

MPEG2_MetaHandler::~MPEG2_MetaHandler()
{
	// Nothing to do.
	
}	// MPEG2_MetaHandler::~MPEG2_MetaHandler

// =================================================================================================
// MPEG2_MetaHandler::CacheFileData
// ================================

void MPEG2_MetaHandler::CacheFileData()
{
	bool readOnly = (! (this->parent->openFlags & kXMPFiles_OpenForUpdate));
	
	this->containsXMP = false;
	this->processedXMP = true;	// Whatever we do here is all that we do for XMPFiles::OpenFile.
	
	// Try to open the sidecar XMP file. Tolerate an open failure, there might not be any XMP.
	// Note that MPEG2_CheckFormat can't save the sidecar path because the handler doesn't exist then.
	
	XMP_StringPtr filePath = this->parent->filePath.c_str();
	XMP_StringPtr extPtr = FindFileExtension ( filePath );
	this->sidecarPath.assign ( filePath, (extPtr - filePath) );
	this->sidecarPath += ".xmp";
	
	if ( readOnly ) {
	
		try {	// *** At this time LFA_Open throws for a failure.
			this->parent->fileRef = LFA_Open ( this->sidecarPath.c_str(), 'r' );
			if ( this->parent->fileRef == 0 ) return;	// *** Could someday check for a permission failure.
		} catch ( ... ) {
			return;	// *** Could someday check for a permission failure.
		}
	
	} else {
	
		try {	// *** At this time LFA_Open throws for a failure.
			this->parent->fileRef = LFA_Open ( this->sidecarPath.c_str(), 'w' );
		} catch ( ... ) {
			this->parent->fileRef = 0;	// *** Could someday check for a permission failure.
		}
		
		if ( this->parent->fileRef == 0 ) {
			// Try to create a file if it does not yet exist.
			// *** Could someday check for a permission failure versus no .xmp file.
			this->parent->fileRef = LFA_Create ( this->sidecarPath.c_str() );
			if ( this->parent->fileRef == 0 ) XMP_Throw ( "Can't create MPEG-2 sidecar", kXMPErr_ExternalFailure );
		}

	}

	// Extract the sidecar's contents and parse.
	
	this->packetInfo.offset = 0;	// We take the whole sidecar file.
	this->packetInfo.length = (XMP_Int32) LFA_Measure ( this->parent->fileRef );
	
	if ( this->packetInfo.length > 0 ) {
	
		this->xmpPacket.assign ( this->packetInfo.length, ' ' );
		LFA_Read ( this->parent->fileRef, (void*)this->xmpPacket.c_str(), this->packetInfo.length, kLFA_RequireAll );
		if ( readOnly ) {
			LFA_Close  ( this->parent->fileRef );
			this->parent->fileRef = 0;
		}
	
		this->xmpObj.ParseFromBuffer ( this->xmpPacket.c_str(), (XMP_StringLen)this->xmpPacket.size() );
		this->containsXMP = true;

	}

}	// MPEG2_MetaHandler::CacheFileData

// =================================================================================================
// MPEG2_MetaHandler::UpdateFile
// =============================

void MPEG2_MetaHandler::UpdateFile ( bool doSafeUpdate )
{
	if ( ! this->needsUpdate ) return;
	
	LFA_FileRef fileRef = this->parent->fileRef;
	XMP_Assert ( fileRef != 0 );

	XMP_StringPtr packetStr = this->xmpPacket.c_str();
	XMP_StringLen packetLen = (XMP_StringLen)this->xmpPacket.size();

	if ( ! doSafeUpdate ) {
	
		// Not doing a crash-safe update, simply rewrite the existing sidecar file.
		LFA_Seek ( fileRef, 0, SEEK_SET );
		LFA_Truncate ( fileRef, 0 );
		LFA_Write ( fileRef, packetStr, packetLen );
	
	} else {

		// Do the usual crash-safe update dance.
		
		LFA_FileRef tempFileRef = 0;
		std::string tempFilePath;
	
		try {

			CreateTempFile ( this->sidecarPath, &tempFilePath, kCopyMacRsrc );
			tempFileRef = LFA_Open ( tempFilePath.c_str(), 'w' );
			LFA_Write ( tempFileRef, packetStr, packetLen );
			
			LFA_Close ( fileRef );
			this->parent->fileRef = 0;
			LFA_Close ( tempFileRef );
			tempFileRef = 0;
			
			LFA_Delete ( this->sidecarPath.c_str() );
			LFA_Rename ( tempFilePath.c_str(), this->sidecarPath.c_str() );

		} catch ( ... ) {
		
			if ( tempFileRef != 0 ) LFA_Close ( tempFileRef );
			if ( ! tempFilePath.empty() ) LFA_Delete ( tempFilePath.c_str() );
		
		}
	
	}

	this->needsUpdate = false;

}	// MPEG2_MetaHandler::UpdateFile

// =================================================================================================
// MPEG2_MetaHandler::WriteFile
// ============================

void MPEG2_MetaHandler::WriteFile ( LFA_FileRef        sourceRef,
								  const std::string & sourcePath )
{
	IgnoreParam(sourceRef); IgnoreParam(sourcePath);

	XMP_Throw ( "MPEG2_MetaHandler::WriteFile: Should never be called", kXMPErr_Unavailable );

}	// MPEG2_MetaHandler::WriteFile
