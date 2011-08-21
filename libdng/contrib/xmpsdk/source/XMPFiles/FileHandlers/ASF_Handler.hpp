#ifndef __ASF_Handler_hpp__
#define __ASF_Handler_hpp__	1

// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2006 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XMPFiles_Impl.hpp"
#include "ASF_Support.hpp"

// =================================================================================================
/// \file ASF_Handler.hpp
/// \brief File format handler for ASF.
///
/// This header ...
///
// =================================================================================================

// *** Could derive from Basic_Handler - buffer file tail in a temp file.

extern XMPFileHandler* ASF_MetaHandlerCTor ( XMPFiles* parent );

extern bool ASF_CheckFormat ( XMP_FileFormat format,
							  XMP_StringPtr  filePath,
                              LFA_FileRef    fileRef,
                              XMPFiles *     parent );

static const XMP_OptionBits kASF_HandlerFlags = ( kXMPFiles_CanInjectXMP | 
												  kXMPFiles_CanExpand | 
												  kXMPFiles_PrefersInPlace | 
                                                  kXMPFiles_CanReconcile |
												  kXMPFiles_AllowsOnlyXMP | 
												  kXMPFiles_ReturnsRawPacket |
												  kXMPFiles_NeedsReadOnlyPacket );

class ASF_MetaHandler : public XMPFileHandler
{
public:

	void CacheFileData();
	void ProcessXMP();
	
	void UpdateFile ( bool doSafeUpdate );
    void WriteFile  ( LFA_FileRef sourceRef, const std::string& sourcePath );

	bool SafeWriteFile ();

	ASF_MetaHandler ( XMPFiles* parent );
	virtual ~ASF_MetaHandler();

private:

	ASF_LegacyManager legacyManager;

};	// ASF_MetaHandler

// =================================================================================================

#endif /* __ASF_Handler_hpp__ */
