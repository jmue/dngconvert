#ifndef __XDCAM_Support_hpp__
#define __XDCAM_Support_hpp__	1

// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2008 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XMP_Environment.h"	// ! This must be the first include.
#include "XMP_Const.h"
#include "XMPFiles_Impl.hpp"
#include "ExpatAdapter.hpp"

// =================================================================================================
/// \file XDCAM_Support.hpp
/// \brief XMPFiles support for XDCAM streams.
///
// =================================================================================================

namespace XDCAM_Support 
{
	// Read XDCAM legacy XML metadata and translate to appropriate XMP.
	bool GetLegacyMetaData ( SXMPMeta *		xmpObjPtr,
							 XML_NodePtr	rootElem,
							 XMP_StringPtr	legacyNS,
							 bool			digestFound,
							 std::string&	umid );

	// Write XMP metadata back to legacy XDCAM XML.
	bool SetLegacyMetaData ( XML_Node *		clipMetadata,
							 SXMPMeta *		xmpObj,
							 XMP_StringPtr	legacyNS );


} // namespace XDCAM_Support

// =================================================================================================

#endif	// __XDCAM_Support_hpp__
