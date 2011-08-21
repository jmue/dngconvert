#ifndef __SWF_Handler_hpp__
#define __SWF_Handler_hpp__	1

// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2006 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

//#include "XMP_Environment.h"	// ! This must be the first include.

#include "XMPFiles_Impl.hpp"

//#include "SWF_Support.hpp"

// =================================================================================================
/// \file SWF_Handler.hpp
/// \brief File format handler for SWF.
///
/// This header ...
///
// =================================================================================================

// *** Could derive from Basic_Handler - buffer file tail in a temp file.

extern XMPFileHandler* SWF_MetaHandlerCTor ( XMPFiles* parent );

extern bool SWF_CheckFormat ( XMP_FileFormat format,
							  XMP_StringPtr  filePath,
                              LFA_FileRef    fileRef,
                              XMPFiles *     parent );

static const XMP_OptionBits kSWF_HandlerFlags = ( kXMPFiles_CanInjectXMP | 
												  kXMPFiles_CanExpand | 
												  kXMPFiles_PrefersInPlace | 
												  kXMPFiles_AllowsOnlyXMP | 
												  kXMPFiles_ReturnsRawPacket ); 

class SWF_MetaHandler : public XMPFileHandler
{
public:

	void CacheFileData();
	void ProcessXMP();

	XMP_OptionBits GetSerializeOptions();
	
	void UpdateFile ( bool doSafeUpdate );
    void WriteFile  ( LFA_FileRef sourceRef, const std::string& sourcePath );

	SWF_MetaHandler ( XMPFiles* parent );
	virtual ~SWF_MetaHandler();

};	// SWF_MetaHandler

// =================================================================================================

#endif /* __SWF_Handler_hpp__ */
