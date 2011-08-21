// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2004 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "Trivial_Handler.hpp"

using namespace std;

// =================================================================================================
/// \file Trivial_Handler.cpp
/// \brief Base class for trivial handlers that only process in-place XMP.
///
/// This header ...
///
// =================================================================================================

// =================================================================================================
// Trivial_MetaHandler::~Trivial_MetaHandler
// =========================================

Trivial_MetaHandler::~Trivial_MetaHandler()
{
	// Nothing to do.
	
}	// Trivial_MetaHandler::~Trivial_MetaHandler

// =================================================================================================
// Trivial_MetaHandler::UpdateFile
// ===============================

void Trivial_MetaHandler::UpdateFile ( bool doSafeUpdate )
{
	IgnoreParam ( doSafeUpdate );
	XMP_Assert ( ! doSafeUpdate );	// Not supported at this level.
	if ( ! this->needsUpdate ) return;
	
	LFA_FileRef      fileRef    = this->parent->fileRef;
	XMP_PacketInfo & packetInfo = this->packetInfo;
	std::string &    xmpPacket  = this->xmpPacket;
	
	LFA_Seek ( fileRef, packetInfo.offset, SEEK_SET );
	LFA_Write ( fileRef, xmpPacket.c_str(), packetInfo.length );
	XMP_Assert ( xmpPacket.size() == (size_t)packetInfo.length );
	
	this->needsUpdate = false;

}	// Trivial_MetaHandler::UpdateFile

// =================================================================================================
// Trivial_MetaHandler::WriteFile
// ==============================

void Trivial_MetaHandler::WriteFile ( LFA_FileRef sourceRef, const std::string & sourcePath )
{
	IgnoreParam ( sourceRef ); IgnoreParam ( sourcePath );
	
	XMP_Throw ( "Trivial_MetaHandler::WriteFile: Not supported", kXMPErr_Unavailable );

}	// Trivial_MetaHandler::WriteFile

// =================================================================================================
