// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2008 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XDCAM_Support.hpp"

// =================================================================================================
/// \file XDCAM_Support.cpp
///
// =================================================================================================

namespace XDCAM_Support 
{

// =================================================================================================
// CreateChildElement
// ==================

namespace
{
XML_Node * CreateChildElement ( XML_Node * parent, XMP_StringPtr localName, XMP_StringPtr legacyNS, int indent /* = 0 */ )
{
	XML_Node * wsNode;
	XML_Node * childNode = parent->GetNamedElement ( legacyNS, localName );
	
	if ( childNode == 0 ) {
	
		// The indenting is a hack, assuming existing 2 spaces per level.
		
		wsNode = new XML_Node ( parent, "", kCDataNode );
		wsNode->value = "  ";	// Add 2 spaces to the existing WS before the parent's close tag.
		parent->content.push_back ( wsNode );

		childNode = new XML_Node ( parent, localName, kElemNode );
		childNode->ns = parent->ns;
		childNode->nsPrefixLen = parent->nsPrefixLen;
		childNode->name.insert ( 0, parent->name, 0, parent->nsPrefixLen );
		parent->content.push_back ( childNode );

		wsNode = new XML_Node ( parent, "", kCDataNode );
		wsNode->value = '\n';
		for ( ; indent > 1; --indent ) wsNode->value += "  ";	// Indent less 1, to "outdent" the parent's close.
		parent->content.push_back ( wsNode );

	}
	
	return childNode;
	
}	
}

// =================================================================================================
// XDCAM_Support::GetLegacyMetaData
// =================================

bool GetLegacyMetaData ( SXMPMeta *		xmpObjPtr,
						 XML_NodePtr	rootElem,
						 XMP_StringPtr	legacyNS,
						 bool			digestFound,
						 std::string&	umid )
{
	bool containsXMP = false;
	
	XML_NodePtr legacyContext = 0, legacyProp = 0;
	
	// UMID
	if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DC, "identifier" )) ) {
		legacyProp = rootElem->GetNamedElement ( legacyNS, "TargetMaterial" );
		if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
			XMP_StringPtr legacyValue = legacyProp->GetAttrValue ( "umidRef" );
			if ( legacyValue != 0 ) {
				umid = legacyValue;
				xmpObjPtr->SetProperty ( kXMP_NS_DC, "identifier", legacyValue, kXMP_DeleteExisting );
				containsXMP = true;
			}
		}
	}

	// Creation date
	if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_XMP, "CreateDate" )) ) {
		legacyProp = rootElem->GetNamedElement ( legacyNS, "CreationDate" );
		if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
			XMP_StringPtr legacyValue = legacyProp->GetAttrValue ( "value" );
			if ( legacyValue != 0 ) {
				xmpObjPtr->SetProperty ( kXMP_NS_XMP, "CreateDate", legacyValue, kXMP_DeleteExisting );
				containsXMP = true;
			}
		}
	}

	// Modify Date
	if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_XMP, "ModifyDate" )) ) {
		legacyProp = rootElem->GetNamedElement ( legacyNS, "LastUpdate" );
		if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
			XMP_StringPtr legacyValue = legacyProp->GetAttrValue ( "value" );
			if ( legacyValue != 0 ) {
				xmpObjPtr->SetProperty ( kXMP_NS_XMP, "ModifyDate", legacyValue, kXMP_DeleteExisting );
				containsXMP = true;
			}
		}
	}

	// Metadata Modify Date
	if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_XMP, "MetadataDate" )) ) {
		legacyProp = rootElem->GetNamedElement ( legacyNS, "lastUpdate" );
		if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
			XMP_StringPtr legacyValue = legacyProp->GetAttrValue ( "value" );
			if ( legacyValue != 0 ) {
				xmpObjPtr->SetProperty ( kXMP_NS_XMP, "MetadataDate", legacyValue, kXMP_DeleteExisting );
				containsXMP = true;
			}
		}
	}

	// Description
	if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DC, "description" )) ) {
		legacyProp = rootElem->GetNamedElement ( legacyNS, "Description" );
		if ( (legacyProp != 0) && legacyProp->IsLeafContentNode() ) {
			XMP_StringPtr legacyValue = legacyProp->GetLeafContentValue();
			if ( legacyValue != 0 ) {
				xmpObjPtr->SetLocalizedText ( kXMP_NS_DC, "description", "", "x-default", legacyValue, kXMP_DeleteExisting );
				containsXMP = true;
			}
		}
	}

	legacyContext = rootElem->GetNamedElement ( legacyNS, "VideoFormat" );

	if ( legacyContext != 0 ) {
	
		// frame size
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DM, "videoFrameSize" )) ) {
			legacyProp = legacyContext->GetNamedElement ( legacyNS, "VideoLayout" );
			if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
	
				XMP_StringPtr widthValue  = legacyProp->GetAttrValue ( "pixel" );
				XMP_StringPtr heightValue = legacyProp->GetAttrValue ( "numOfVerticalLine" );
	
				if ( (widthValue != 0) && (heightValue != 0) ) {
	
					xmpObjPtr->DeleteProperty ( kXMP_NS_DM, "videoFrameSize" );
					xmpObjPtr->SetStructField ( kXMP_NS_DM, "videoFrameSize", kXMP_NS_XMP_Dimensions, "w", widthValue );
					xmpObjPtr->SetStructField ( kXMP_NS_DM, "videoFrameSize", kXMP_NS_XMP_Dimensions, "h", heightValue );
					xmpObjPtr->SetStructField ( kXMP_NS_DM, "videoFrameSize", kXMP_NS_XMP_Dimensions, "unit", "pixels" );
												  
					containsXMP = true;
	
				}
	
			}
		}
		
		// Aspect ratio
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DM, "videoPixelAspectRatio" )) ) {
			legacyProp = legacyContext->GetNamedElement ( legacyNS, "VideoLayout" );
			if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
				XMP_StringPtr aspectRatio = legacyProp->GetAttrValue ( "aspectRatio" );
				if ( aspectRatio != 0 ) {
					xmpObjPtr->SetProperty ( kXMP_NS_DM, "videoPixelAspectRatio", aspectRatio, kXMP_DeleteExisting );
					containsXMP = true;
				}
			}
		}
	
		// Frame rate
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DM, "videoFrameRate" )) ) {
			legacyProp = legacyContext->GetNamedElement ( legacyNS, "VideoFrame" );
			if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
				XMP_StringPtr prop = legacyProp->GetAttrValue ( "formatFps" );
				if ( prop != 0 ) {
					xmpObjPtr->SetProperty ( kXMP_NS_DM, "videoFrameRate", prop, kXMP_DeleteExisting );
					containsXMP = true;
				}
			}
		}
	
		// Video codec
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DM, "videoCompressor" )) ) {
			legacyProp = legacyContext->GetNamedElement ( legacyNS, "VideoFrame" );
			if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
				XMP_StringPtr prop = legacyProp->GetAttrValue ( "videoCodec" );
				if ( prop != 0 ) {
					xmpObjPtr->SetProperty ( kXMP_NS_DM, "videoCompressor", prop, kXMP_DeleteExisting );
					containsXMP = true;
				}
			}
		}
	
	}	// VideoFormat
	
	legacyContext = rootElem->GetNamedElement ( legacyNS, "AudioFormat" );

	if ( legacyContext != 0 ) {

		// Audio codec
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DM, "audioCompressor" )) ) {
			legacyProp = legacyContext->GetNamedElement ( legacyNS, "AudioRecPort" );
			if ( (legacyProp != 0) && legacyProp->IsEmptyLeafNode() ) {
				XMP_StringPtr prop = legacyProp->GetAttrValue ( "audioCodec" );
				if ( prop != 0 ) {
					xmpObjPtr->SetProperty ( kXMP_NS_DM, "audioCompressor", prop, kXMP_DeleteExisting );
					containsXMP = true;
				}
			}
		}
	
	}	// AudioFormat

	// Duration
	if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DM, "duration" )) ) {

		std::string durationFrames, timecodeFPS;
		legacyProp = rootElem->GetNamedElement ( legacyNS, "Duration" );
		if ( legacyProp != 0 ) {
			XMP_StringPtr durationValue = legacyProp->GetAttrValue ( "value" );
			if ( durationValue != 0 ) durationFrames = durationValue;
		}

		legacyProp = rootElem->GetNamedElement ( legacyNS, "LtcChangeTable" );
		if ( legacyProp != 0 ) {
			// [TODO] Verify that this is the correct framerate to use from the legacy metadata.  gemiller
			XMP_StringPtr fps = legacyProp->GetAttrValue ( "tcFps" );
			if ( fps != 0 ) {
				timecodeFPS = "1/";
				timecodeFPS += fps;
			}
		}

		if ( (!timecodeFPS.empty()) && (!durationFrames.empty())) {
			xmpObjPtr->DeleteProperty ( kXMP_NS_DM, "duration" );
			xmpObjPtr->SetStructField ( kXMP_NS_DM, "duration", kXMP_NS_DM, "value", durationFrames );
			xmpObjPtr->SetStructField ( kXMP_NS_DM, "duration", kXMP_NS_DM, "scale", timecodeFPS );
			containsXMP = true;
		}

	}
	
	legacyContext = rootElem->GetNamedElement ( legacyNS, "Device" );
	if ( legacyContext != 0 ) {

		std::string model; 
		  
		// manufacturer string
		XMP_StringPtr manufacturer = legacyContext->GetAttrValue ( "manufacturer" );
		if ( manufacturer != 0 ) {
			model += manufacturer;
		}
		
		// model string
		XMP_StringPtr modelName = legacyContext->GetAttrValue ( "modelName" );
		if ( modelName != 0 ) {
			if ( model.size() > 0 ) {
				model += " ";
			}
			model += modelName;
		}
		

		// For the dm::cameraModel property, concat the make and model.
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_DM, "cameraModel" )) ) {
			if ( model.size() != 0 ) {
				xmpObjPtr->SetProperty ( kXMP_NS_DM, "cameraModel", model, kXMP_DeleteExisting );
				containsXMP = true;
			}
		}
		
		// EXIF Model
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_TIFF, "Model" )) ) {
			xmpObjPtr->SetProperty ( kXMP_NS_TIFF, "Model", modelName, kXMP_DeleteExisting );
		}
	
		// EXIF Make
		if ( digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_TIFF, "Make" )) ) {
			xmpObjPtr->SetProperty ( kXMP_NS_TIFF, "Make", manufacturer, kXMP_DeleteExisting );
		}
	
		// EXIF-AUX Serial number
		XMP_StringPtr serialNumber = legacyContext->GetAttrValue ( "serialNo" );
		if ( serialNumber != 0 && (digestFound || (! xmpObjPtr->DoesPropertyExist ( kXMP_NS_EXIF_Aux, "SerialNumber" ))) ) {
			xmpObjPtr->SetProperty ( kXMP_NS_EXIF_Aux, "SerialNumber", serialNumber, kXMP_DeleteExisting );
		}
	
	}

	
	return containsXMP;

}

// =================================================================================================
// XDCAM_Support::SetLegacyMetaData
// ================================

bool SetLegacyMetaData ( XML_Node *		clipMetadata,
						 SXMPMeta *		xmpObj,
						 XMP_StringPtr	legacyNS )
{
	bool updateLegacyXML = false;
	bool xmpFound = false;
	std::string xmpValue;
	XML_Node * xmlNode = 0;
	
	xmpFound = xmpObj->GetProperty ( kXMP_NS_DC, "title", &xmpValue, 0 );
	
	if ( xmpFound ) {

		xmlNode = CreateChildElement ( clipMetadata, "Title", legacyNS, 3 );
		if ( xmpValue != xmlNode->GetLeafContentValue() ) {
			xmlNode->SetLeafContentValue ( xmpValue.c_str() );
			updateLegacyXML = true;
		}

	}
	
	xmpFound = xmpObj->GetArrayItem ( kXMP_NS_DC, "creator", 1, &xmpValue, 0 );

	if ( xmpFound ) {
		xmlNode = CreateChildElement ( clipMetadata, "Creator", legacyNS, 3 );
		XMP_StringPtr creatorName = xmlNode->GetAttrValue ( "name" );
		if ( creatorName == 0 ) creatorName = "";
		if ( xmpValue != creatorName ) {
			xmlNode->SetAttrValue ( "name", xmpValue.c_str() );
			updateLegacyXML = true;
		}
	}
	
	xmpFound = xmpObj->GetProperty ( kXMP_NS_DC, "description", &xmpValue, 0 );

	if ( xmpFound ) {
		xmlNode = CreateChildElement ( clipMetadata, "Description", legacyNS, 3 );
		if ( xmpValue != xmlNode->GetLeafContentValue() ) {
			// description in non real time metadata is limited to 2047 bytes
			if ( xmpValue.size() > 2047 ) xmpValue.resize ( 2047 );
			xmlNode->SetLeafContentValue ( xmpValue.c_str() );
			updateLegacyXML = true;
		}
	}
	
	return updateLegacyXML;

}

// =================================================================================================

}	// namespace XDCAM_Support
