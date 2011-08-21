#ifndef __XDCAMEX_Handler_hpp__
#define __XDCAMEX_Handler_hpp__	1

// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2008 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XMP_Environment.h"	// ! This must be the first include.

#include "XMPFiles_Impl.hpp"

#include "ExpatAdapter.hpp"

// =================================================================================================
/// \file XDCAMEX_Handler.hpp
/// \brief Folder format handler for XDCAMEX.
// =================================================================================================

extern XMPFileHandler * XDCAMEX_MetaHandlerCTor ( XMPFiles * parent );

extern bool XDCAMEX_CheckFormat ( XMP_FileFormat format,
								  const std::string & rootPath,
								  const std::string & gpName,
								  const std::string & parentName,
								  const std::string & leafName,
								  XMPFiles * parent );

static const XMP_OptionBits kXDCAMEX_HandlerFlags = (kXMPFiles_CanInjectXMP |
													 kXMPFiles_CanExpand |
													 kXMPFiles_CanRewrite |
													 kXMPFiles_PrefersInPlace |
													 kXMPFiles_CanReconcile |
													 kXMPFiles_AllowsOnlyXMP |
													 kXMPFiles_ReturnsRawPacket |
													 kXMPFiles_HandlerOwnsFile |
													 kXMPFiles_AllowsSafeUpdate |
													 kXMPFiles_FolderBasedFormat);

class XDCAMEX_MetaHandler : public XMPFileHandler
{
public:

	void CacheFileData();
	void ProcessXMP();

	XMP_OptionBits GetSerializeOptions()	// *** These should be standard for standalone XMP files.
		{ return (kXMP_UseCompactFormat | kXMP_OmitPacketWrapper); };
	
	void UpdateFile ( bool doSafeUpdate );
    void WriteFile  ( LFA_FileRef sourceRef, const std::string & sourcePath );

	XDCAMEX_MetaHandler ( XMPFiles * _parent );
	virtual ~XDCAMEX_MetaHandler();

private:

	XDCAMEX_MetaHandler() : expat(0) {};	// Hidden on purpose.
	
	void MakeClipFilePath ( std::string * path, XMP_StringPtr suffix );
	void MakeLegacyDigest ( std::string * digestStr );

	void GetTakeUMID( const std::string& clipUMID, std::string& takeUMID, std::string& takeXMLURI );
	void GetTakeDuration( const std::string& takeUMID, std::string& duration );

	void CleanupLegacyXML();
	
	std::string rootPath, clipName, xdcNS, legacyNS, clipUMID;
	
	ExpatAdapter * expat;
	XML_Node * clipMetadata;
	
};	// XDCAMEX_MetaHandler

// =================================================================================================

#endif /* __XDCAMEX_Handler_hpp__ */
