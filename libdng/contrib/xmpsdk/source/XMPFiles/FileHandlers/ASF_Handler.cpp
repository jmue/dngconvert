// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2006 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "ASF_Handler.hpp"

// =================================================================================================
/// \file ASF_Handler.hpp
/// \brief File format handler for ASF.
///
/// This handler ...
///
// =================================================================================================

// =================================================================================================
// ASF_MetaHandlerCTor
// ====================

XMPFileHandler * ASF_MetaHandlerCTor ( XMPFiles * parent )
{
	return new ASF_MetaHandler ( parent );

}	// ASF_MetaHandlerCTor

// =================================================================================================
// ASF_CheckFormat
// ===============

bool ASF_CheckFormat ( XMP_FileFormat format,
					   XMP_StringPtr  filePath,
                       LFA_FileRef    fileRef,
                       XMPFiles *     parent )
{

	IgnoreParam(format); IgnoreParam(fileRef); IgnoreParam(parent);
	XMP_Assert ( format == kXMP_WMAVFile );

	IOBuffer ioBuf;
	
	LFA_Seek ( fileRef, 0, SEEK_SET );
	if ( ! CheckFileSpace ( fileRef, &ioBuf, guidLen ) ) return false;

	GUID guid;
	memcpy ( &guid, ioBuf.ptr, guidLen );

	if ( ! IsEqualGUID ( ASF_Header_Object, guid ) ) return false;

	return true;

}	// ASF_CheckFormat

// =================================================================================================
// ASF_MetaHandler::ASF_MetaHandler
// ==================================

ASF_MetaHandler::ASF_MetaHandler ( XMPFiles * _parent )
{
	this->parent = _parent;
	this->handlerFlags = kASF_HandlerFlags;
	this->stdCharForm  = kXMP_Char8Bit;

}

// =================================================================================================
// ASF_MetaHandler::~ASF_MetaHandler
// ===================================

ASF_MetaHandler::~ASF_MetaHandler()
{
	// Nothing extra to do.
}

// =================================================================================================
// ASF_MetaHandler::CacheFileData
// ===============================

void ASF_MetaHandler::CacheFileData()
{

	this->containsXMP = false;

	LFA_FileRef fileRef ( this->parent->fileRef );
	if ( fileRef == 0 ) return;

	ASF_Support support ( &this->legacyManager );
	ASF_Support::ObjectState objectState;
	long numTags = support.OpenASF ( fileRef, objectState );
	if ( numTags == 0 ) return;

	if ( objectState.xmpLen != 0 ) {

		// XMP present

		XMP_Int32 len = XMP_Int32 ( objectState.xmpLen );

		this->xmpPacket.reserve( len );
		this->xmpPacket.assign ( len, ' ' );

		bool found = ASF_Support::ReadBuffer ( fileRef, objectState.xmpPos, objectState.xmpLen,
											   const_cast<char *>(this->xmpPacket.data()) );
		if ( found ) {
			this->packetInfo.offset = objectState.xmpPos;
			this->packetInfo.length = len;
			this->containsXMP = true;
		}

	}

}	// ASF_MetaHandler::CacheFileData

// =================================================================================================
// ASF_MetaHandler::ProcessXMP
// ============================
//
// Process the raw XMP and legacy metadata that was previously cached.

void ASF_MetaHandler::ProcessXMP()
{

	this->processedXMP = true;	// Make sure we only come through here once.

	// Process the XMP packet.

	if ( this->xmpPacket.empty() ) {

		// import legacy in any case, when no XMP present
		legacyManager.ImportLegacy ( &this->xmpObj );
		this->legacyManager.SetDigest ( &this->xmpObj );

	} else {
	
		XMP_Assert ( this->containsXMP );
		XMP_StringPtr packetStr = this->xmpPacket.c_str();
		XMP_StringLen packetLen = (XMP_StringLen)this->xmpPacket.size();

		this->xmpObj.ParseFromBuffer ( packetStr, packetLen );

		if ( ! legacyManager.CheckDigest ( this->xmpObj ) ) {
			legacyManager.ImportLegacy ( &this->xmpObj );
		}

	}

	// Assume we now have something in the XMP.
	this->containsXMP = true;

}	// ASF_MetaHandler::ProcessXMP

// =================================================================================================
// ASF_MetaHandler::UpdateFile
// ============================

void ASF_MetaHandler::UpdateFile ( bool doSafeUpdate )
{

	bool updated = false;
	
	if ( ! this->needsUpdate ) return;

	LFA_FileRef fileRef ( this->parent->fileRef );
	if ( fileRef == 0 ) return;

	ASF_Support support;
	ASF_Support::ObjectState objectState;
	long numTags = support.OpenASF ( fileRef, objectState );
	if ( numTags == 0 ) return;

	XMP_StringLen packetLen = (XMP_StringLen)xmpPacket.size();

	this->legacyManager.ExportLegacy ( this->xmpObj );
	if ( this->legacyManager.hasLegacyChanged() ) {

		this->legacyManager.SetDigest ( &this->xmpObj );

		// serialize with updated digest
		if ( objectState.xmpLen == 0 ) {

			// XMP does not exist, use standard padding
			this->xmpObj.SerializeToBuffer ( &this->xmpPacket, kXMP_UseCompactFormat );

		} else {

			// re-use padding with static XMP size
			try {
				XMP_OptionBits compactExact = (kXMP_UseCompactFormat | kXMP_ExactPacketLength);
				this->xmpObj.SerializeToBuffer ( &this->xmpPacket, compactExact, XMP_StringLen(objectState.xmpLen) );
			} catch ( ... ) {
				// re-use padding with exact packet length failed (legacy-digest needed too much space): try again using standard padding
				this->xmpObj.SerializeToBuffer ( &this->xmpPacket, kXMP_UseCompactFormat );
			}

		}

	}
	
	XMP_StringPtr packetStr = xmpPacket.c_str();
	packetLen = (XMP_StringLen)xmpPacket.size();
	if ( packetLen == 0 ) return;

	// value, when guessing for sufficient legacy padding (line-ending conversion etc.)
	const int paddingTolerance = 50;

	bool xmpGrows = ( objectState.xmpLen && (packetLen > objectState.xmpLen) && ( ! objectState.xmpIsLastObject) );

	bool legacyGrows = ( this->legacyManager.hasLegacyChanged() &&
						 (this->legacyManager.getLegacyDiff() > (this->legacyManager.GetPadding() - paddingTolerance)) );
	
	if ( doSafeUpdate || legacyGrows || xmpGrows ) {

		// do a safe update in any case
		updated = SafeWriteFile();

	} else {

		// possibly we can do an in-place update

		if ( objectState.xmpLen < packetLen ) {

			updated = SafeWriteFile();

		} else {

			// current XMP chunk size is sufficient -> write (in place update)
			updated = ASF_Support::WriteBuffer(fileRef, objectState.xmpPos, packetLen, packetStr );

			// legacy update
			if ( updated && this->legacyManager.hasLegacyChanged() ) {

				ASF_Support::ObjectIterator curPos = objectState.objects.begin();
				ASF_Support::ObjectIterator endPos = objectState.objects.end();

				for ( ; curPos != endPos; ++curPos ) {

					ASF_Support::ObjectData object = *curPos;

					// find header-object
					if ( IsEqualGUID ( ASF_Header_Object, object.guid ) ) {
						// update header object
						updated = support.UpdateHeaderObject ( fileRef, object, legacyManager );
					}

				}

			}

		}

	}

	if ( ! updated ) return;	// If there's an error writing the chunk, bail.

	this->needsUpdate = false;

}	// ASF_MetaHandler::UpdateFile

// =================================================================================================
// ASF_MetaHandler::WriteFile
// ===========================

void ASF_MetaHandler::WriteFile ( LFA_FileRef sourceRef, const std::string & sourcePath )
{
	LFA_FileRef destRef = this->parent->fileRef;

	ASF_Support support;
	ASF_Support::ObjectState objectState;
	long numTags = support.OpenASF ( sourceRef, objectState );
	if ( numTags == 0 ) return;

	LFA_Truncate ( destRef, 0 );

	ASF_Support::ObjectIterator curPos = objectState.objects.begin();
	ASF_Support::ObjectIterator endPos = objectState.objects.end();

	for ( ; curPos != endPos; ++curPos ) {

		ASF_Support::ObjectData object = *curPos;

		// discard existing XMP object
		if ( object.xmp ) continue;

		// update header-object, when legacy needs update
		if ( IsEqualGUID ( ASF_Header_Object, object.guid) && this->legacyManager.hasLegacyChanged( ) ) {
			// rewrite header object
			support.WriteHeaderObject ( sourceRef, destRef, object, this->legacyManager, false );
		} else {
			// copy any other object
			ASF_Support::CopyObject ( sourceRef, destRef, object );
		}

		// write XMP object immediately after the (one and only) top-level DataObject
		if ( IsEqualGUID ( ASF_Data_Object, object.guid ) ) {
			XMP_StringPtr packetStr = xmpPacket.c_str();
			XMP_StringLen packetLen = (XMP_StringLen)xmpPacket.size();
			ASF_Support::WriteXMPObject ( destRef, packetLen, packetStr );
		}

	}

	support.UpdateFileSize ( destRef );

}	// ASF_MetaHandler::WriteFile

// =================================================================================================
// ASF_MetaHandler::SafeWriteFile
// ===========================

bool ASF_MetaHandler::SafeWriteFile ()
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

} // ASF_MetaHandler::SafeWriteFile

// =================================================================================================
