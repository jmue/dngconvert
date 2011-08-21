// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2004 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "Basic_Handler.hpp"

using namespace std;

// =================================================================================================
/// \file Basic_Handler.cpp
/// \brief Base class for basic handlers that only process in-place XMP.
///
/// This header ...
///
// =================================================================================================

// =================================================================================================
// Basic_MetaHandler::~Basic_MetaHandler
// =====================================

Basic_MetaHandler::~Basic_MetaHandler()
{
	// ! Inherit the base cleanup.

}	// Basic_MetaHandler::~Basic_MetaHandler

// =================================================================================================
// Basic_MetaHandler::UpdateFile
// =============================

// ! This must be called from the destructor for all derived classes. It can't be called from the
// ! Basic_MetaHandler destructor, by then calls to the virtual functions would not go to the
// ! actual implementations for the derived class.

void Basic_MetaHandler::UpdateFile ( bool doSafeUpdate )
{
	IgnoreParam ( doSafeUpdate );
	XMP_Assert ( ! doSafeUpdate );	// Not supported at this level.
	if ( ! this->needsUpdate ) return;
	
	LFA_FileRef fileRef = this->parent->fileRef;
	XMP_PacketInfo & packetInfo = this->packetInfo;
	std::string &    xmpPacket  = this->xmpPacket;
	
	XMP_AbortProc abortProc  = this->parent->abortProc;
	void *        abortArg   = this->parent->abortArg;
	const bool    checkAbort = (abortProc != 0);
		
	this->CaptureFileEnding();	// ! Do this first, before any location info changes.
	if ( checkAbort && abortProc(abortArg) ) {
		XMP_Throw ( "Basic_MetaHandler::UpdateFile - User abort", kXMPErr_UserAbort );
	}
	
	this->NoteXMPRemoval();
	this->ShuffleTrailingContent();
	if ( checkAbort && abortProc(abortArg) ) {
		XMP_Throw ( "Basic_MetaHandler::UpdateFile - User abort", kXMPErr_UserAbort );
	}
	
	XMP_Int64 tempLength = this->xmpFileOffset - this->xmpPrefixSize + this->trailingContentSize;
	LFA_Truncate ( fileRef, tempLength );
	LFA_Flush ( fileRef );
	
	packetInfo.offset = tempLength + this->xmpPrefixSize;
	this->NoteXMPInsertion();

	LFA_Seek ( fileRef, 0, SEEK_END );
	this->WriteXMPPrefix();
	LFA_Write ( fileRef, xmpPacket.c_str(), (XMP_StringLen)xmpPacket.size() );
	this->WriteXMPSuffix();
	if ( checkAbort && abortProc(abortArg) ) {
		XMP_Throw ( "Basic_MetaHandler::UpdateFile - User abort", kXMPErr_UserAbort );
	}
	
	this->RestoreFileEnding();
	LFA_Flush ( fileRef );
	
	this->xmpFileOffset = packetInfo.offset;
	this->xmpFileSize = packetInfo.length;
	this->needsUpdate = false;

}	// Basic_MetaHandler::UpdateFile

// =================================================================================================
// Basic_MetaHandler::WriteFile
// ============================

// *** What about computing the new file length and pre-allocating the file?

void Basic_MetaHandler::WriteFile ( LFA_FileRef sourceRef, const std::string & sourcePath )
{
	IgnoreParam ( sourcePath );
	
	XMP_AbortProc abortProc  = this->parent->abortProc;
	void *        abortArg   = this->parent->abortArg;
	const bool    checkAbort = (abortProc != 0);
	
	LFA_FileRef destRef = this->parent->fileRef;
	
	// Capture the "back" of the source file.
	
	this->parent->fileRef = sourceRef;
	this->CaptureFileEnding();
	this->parent->fileRef = destRef;
	if ( checkAbort && abortProc(abortArg) ) {
		XMP_Throw ( "Basic_MetaHandler::UpdateFile - User abort", kXMPErr_UserAbort );
	}
	
	// Seek to the beginning of the source and destination files, truncate the destination.
	
	LFA_Seek ( sourceRef, 0, SEEK_SET );
	LFA_Seek ( destRef, 0, SEEK_SET );
	LFA_Truncate ( destRef, 0 );
	
	// Copy the front of the source file to the destination. Note the XMP (pseudo) removal and
	// insertion. This mainly updates info about the new XMP length.
	
	XMP_Int64 xmpSectionOffset = this->xmpFileOffset - this->xmpPrefixSize;
	XMP_Int32 oldSectionLength = this->xmpPrefixSize + this->xmpFileSize + this->xmpSuffixSize;
	
	LFA_Copy ( sourceRef, destRef, xmpSectionOffset, abortProc, abortArg );
	this->NoteXMPRemoval();
	packetInfo.offset = this->xmpFileOffset;	// ! The packet offset does not change.
	this->NoteXMPInsertion();
	LFA_Seek ( destRef, 0, SEEK_END );	
	if ( checkAbort && abortProc(abortArg) ) {
		XMP_Throw ( "Basic_MetaHandler::WriteFile - User abort", kXMPErr_UserAbort );
	}

	// Write the new XMP section to the destination.
	
	this->WriteXMPPrefix();
	LFA_Write ( destRef, this->xmpPacket.c_str(), (XMP_StringLen)this->xmpPacket.size() );
	this->WriteXMPSuffix();
	if ( checkAbort && abortProc(abortArg) ) {
		XMP_Throw ( "Basic_MetaHandler::WriteFile - User abort", kXMPErr_UserAbort );
	}

	// Copy the trailing file content from the source and write the "back" of the file.
	
	XMP_Int64 remainderOffset = xmpSectionOffset + oldSectionLength;

	LFA_Seek ( sourceRef, remainderOffset, SEEK_SET );
	LFA_Copy ( sourceRef, destRef, this->trailingContentSize, abortProc, abortArg );
	this->RestoreFileEnding();
	
	// Done.
	
	LFA_Flush ( destRef );
	
	this->xmpFileOffset = packetInfo.offset;
	this->xmpFileSize = packetInfo.length;
	this->needsUpdate = false;

}	// Basic_MetaHandler::WriteFile

// =================================================================================================
// ShuffleTrailingContent
// ======================
//
// Shuffle the trailing content portion of a file forward. This does not include the final "back"
// portion of the file, just the arbitrary length content between the XMP section and the back.
// Don't use LFA_Copy, that assumes separate files and hence separate I/O positions.

// ! The XMP packet location and prefix/suffix sizes must still reflect the XMP section that is in
// ! the process of being removed.

void Basic_MetaHandler::ShuffleTrailingContent()
{
	LFA_FileRef fileRef = this->parent->fileRef;

	XMP_Int64 readOffset  = this->packetInfo.offset + xmpSuffixSize;
	XMP_Int64 writeOffset = this->packetInfo.offset - xmpPrefixSize;

	XMP_Int64 remainingLength = this->trailingContentSize;
	
	enum { kBufferSize = 64*1024 };
	char buffer [kBufferSize];
	
	XMP_AbortProc abortProc  = this->parent->abortProc;
	void *        abortArg   = this->parent->abortArg;
	const bool    checkAbort = (abortProc != 0);
	
	while ( remainingLength > 0 ) {

		XMP_Int32 ioCount = kBufferSize;
		if ( remainingLength < kBufferSize ) ioCount = (XMP_Int32)remainingLength;

		LFA_Seek ( fileRef, readOffset, SEEK_SET );
		LFA_Read ( fileRef, buffer, ioCount, kLFA_RequireAll );
		LFA_Seek ( fileRef, writeOffset, SEEK_SET );
		LFA_Write ( fileRef, buffer, ioCount );

		readOffset  += ioCount;
		writeOffset += ioCount;
		remainingLength -= ioCount;

		if ( checkAbort && abortProc(abortArg) ) {
			XMP_Throw ( "Basic_MetaHandler::ShuffleTrailingContent - User abort", kXMPErr_UserAbort );
		}

	}
	
	LFA_Flush ( fileRef );

}	// ShuffleTrailingContent

// =================================================================================================
// Dummies needed for VS.Net
// =========================

void Basic_MetaHandler::WriteXMPPrefix()
{
	XMP_Throw ( "Basic_MetaHandler::WriteXMPPrefix - Needs specific override", kXMPErr_InternalFailure );
}

void Basic_MetaHandler::WriteXMPSuffix()
{
	XMP_Throw ( "Basic_MetaHandler::WriteXMPSuffix - Needs specific override", kXMPErr_InternalFailure );
}

void Basic_MetaHandler::NoteXMPRemoval()
{
	XMP_Throw ( "Basic_MetaHandler::NoteXMPRemoval - Needs specific override", kXMPErr_InternalFailure );
}

void Basic_MetaHandler::NoteXMPInsertion()
{
	XMP_Throw ( "Basic_MetaHandler::NoteXMPInsertion - Needs specific override", kXMPErr_InternalFailure );
}

void Basic_MetaHandler::CaptureFileEnding()
{
	XMP_Throw ( "Basic_MetaHandler::CaptureFileEnding - Needs specific override", kXMPErr_InternalFailure );
}

void Basic_MetaHandler::RestoreFileEnding()
{
	XMP_Throw ( "Basic_MetaHandler::RestoreFileEnding - Needs specific override", kXMPErr_InternalFailure );
}

// =================================================================================================
