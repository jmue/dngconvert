// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2008 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XDCAMEX_Handler.hpp"
#include "XDCAM_Support.hpp"
#include "MD5.h"

using namespace std;

// =================================================================================================
/// \file XDCAMEX_Handler.cpp
/// \brief Folder format handler for XDCAMEX.
///
/// This handler is for the XDCAMEX video format.
///
/// .../MyMovie/
///		BPAV/
///			MEDIAPRO.XML
///			MEDIAPRO.BUP
///			CLPR/
///				709_001_01/
///					709_001_01.SMI
///					709_001_01.MP4
///					709_001_01M01.XML
///					709_001_01R01.BIM
///					709_001_01I01.PPN
///				709_001_02/
///				709_002_01/
///				709_003_01/
///			TAKR/
///				709_001/
///					709_001.SMI
///					709_001M01.XML
///
/// The Backup files (.BUP) are optional. No files or directories other than those listed are
/// allowed in the BPAV directory. The CLPR (clip root) directory may contain only clip directories,
/// which may only contain the clip files listed. The TAKR (take root) direcory may contail only
/// take directories, which may only contain take files. The take root directory can be empty.
/// MEDIPRO.XML contains information on clip and take management.
///
/// Each clip directory contains a media file (.MP4), a clip info file (.SMI), a real time metadata
/// file (.BIM), a non real time metadata file (.XML), and a picture pointer file (.PPN). A take
/// directory conatins a take info and non real time take metadata files.
// =================================================================================================

// =================================================================================================
// XDCAMEX_CheckFormat
// ===================
//
// This version checks for the presence of a top level BPAV directory, and the required files and
// directories immediately within it. The CLPR and TAKR subfolders are required, as is MEDIAPRO.XML.
//
// The state of the string parameters depends on the form of the path passed by the client. If the
// client passed a logical clip path, like ".../MyMovie/012_3456_01", the parameters are:
//   rootPath   - ".../MyMovie"
//   gpName     - empty
//   parentName - empty
//   leafName   - "012_3456_01"
// If the client passed a full file path, like ".../MyMovie/BPAV/CLPR/012_3456_01/012_3456_01M01.XML", they are:
//   rootPath   - ".../MyMovie/BPAV"
//   gpName     - "CLPR"
//   parentName - "012_3456_01"
//   leafName   - "012_3456_01M01"

// ! The common code has shifted the gpName, parentName, and leafName strings to upper case. It has
// ! also made sure that for a logical clip path the rootPath is an existing folder, and that the
// ! file exists for a full file path.

// ! Using explicit '/' as a separator when creating paths, it works on Windows.

bool XDCAMEX_CheckFormat ( XMP_FileFormat format,
						   const std::string & _rootPath,
						   const std::string & gpName,
						   const std::string & parentName,
						   const std::string & _leafName,
						   XMPFiles * parent )
{
	std::string rootPath = _rootPath;
	std::string clipName = _leafName;
	std::string grandGPName;
	
	std::string bpavPath ( rootPath );
	
	// Do some initial checks on the gpName and parentName.
	
	if ( gpName.empty() != parentName.empty() ) return false;	// Must be both empty or both non-empty.
	
	if ( gpName.empty() ) {
		
		// This is the logical clip path case. Make sure .../MyMovie/BPAV/CLPR is a folder.
		bpavPath += kDirChar;	// The rootPath was just ".../MyMovie".
		bpavPath += "BPAV";
		if ( GetChildMode ( bpavPath, "CLPR" ) != kFMode_IsFolder ) return false;
		
	} else {

		// This is the explicit file case. Make sure the ancestry is OK. Set the clip name from the
		// parent folder name.
		
		if ( gpName != "CLPR" ) return false;
		SplitLeafName ( &rootPath, &grandGPName );
		MakeUpperCase ( &grandGPName );
		if ( grandGPName != "BPAV" ) return false;
		if ( ! XMP_LitNMatch ( parentName.c_str(), clipName.c_str(), parentName.size() ) ) return false;
		
		clipName = parentName;

	}
	
	// Check the rest of the required general structure.
	if ( GetChildMode ( bpavPath, "TAKR" ) != kFMode_IsFolder ) return false;
	if ( GetChildMode ( bpavPath, "MEDIAPRO.XML" ) != kFMode_IsFile ) return false;
	
	// Make sure the clip's .MP4 and .SMI files exist.
	std::string tempPath ( bpavPath );
	tempPath += kDirChar;
	tempPath += "CLPR";
	tempPath += kDirChar;
	tempPath += clipName;
	tempPath += kDirChar;
	tempPath += clipName;
	tempPath += ".MP4";
	if ( GetFileMode ( tempPath.c_str() ) != kFMode_IsFile ) return false;
	tempPath.erase ( tempPath.size()-3 );
	tempPath += "SMI";
	if ( GetFileMode ( tempPath.c_str() ) != kFMode_IsFile ) return false;
	
	// And now save the psuedo path for the handler object.
	tempPath = rootPath;
	tempPath += kDirChar;
	tempPath += clipName;
	size_t pathLen = tempPath.size() + 1;	// Include a terminating nul.
	parent->tempPtr = malloc ( pathLen );
	if ( parent->tempPtr == 0 ) XMP_Throw ( "No memory for XDCAMEX clip info", kXMPErr_NoMemory );
	memcpy ( parent->tempPtr, tempPath.c_str(), pathLen );

	return true;

}	// XDCAMEX_CheckFormat

// =================================================================================================
// XDCAMEX_MetaHandlerCTor
// =======================

XMPFileHandler * XDCAMEX_MetaHandlerCTor ( XMPFiles * parent )
{
	return new XDCAMEX_MetaHandler ( parent );
	
}	// XDCAMEX_MetaHandlerCTor

// =================================================================================================
// XDCAMEX_MetaHandler::XDCAMEX_MetaHandler
// ========================================

XDCAMEX_MetaHandler::XDCAMEX_MetaHandler ( XMPFiles * _parent ) : expat(0)
{
	this->parent = _parent;	// Inherited, can't set in the prefix.
	this->handlerFlags = kXDCAMEX_HandlerFlags;
	this->stdCharForm  = kXMP_Char8Bit;
	
	// Extract the root path and clip name from tempPtr.
	
	XMP_Assert ( this->parent->tempPtr != 0 );
	
	this->rootPath.assign ( (char*) this->parent->tempPtr );
	free ( this->parent->tempPtr );
	this->parent->tempPtr = 0;

	SplitLeafName ( &this->rootPath, &this->clipName );

}	// XDCAMEX_MetaHandler::XDCAMEX_MetaHandler

// =================================================================================================
// XDCAMEX_MetaHandler::~XDCAMEX_MetaHandler
// =========================================

XDCAMEX_MetaHandler::~XDCAMEX_MetaHandler()
{

	this->CleanupLegacyXML();
	if ( this->parent->tempPtr != 0 ) {
		free ( this->parent->tempPtr );
		this->parent->tempPtr = 0;
	}

}	// XDCAMEX_MetaHandler::~XDCAMEX_MetaHandler

// =================================================================================================
// XDCAMEX_MetaHandler::MakeClipFilePath
// =====================================

void XDCAMEX_MetaHandler::MakeClipFilePath ( std::string * path, XMP_StringPtr suffix )
{

	*path = this->rootPath;
	*path += kDirChar;
	*path += "BPAV";
	*path += kDirChar;
	*path += "CLPR";
	*path += kDirChar;
	*path += this->clipName;
	*path += kDirChar;
	*path += this->clipName;
	*path += suffix;

}	// XDCAMEX_MetaHandler::MakeClipFilePath

// =================================================================================================
// XDCAMEX_MetaHandler::MakeLegacyDigest
// =====================================

// *** Early hack version.

#define kHexDigits "0123456789ABCDEF"

void XDCAMEX_MetaHandler::MakeLegacyDigest ( std::string * digestStr )
{
	digestStr->erase();
	if ( this->clipMetadata == 0 ) return;	// Bail if we don't have any legacy XML.
	XMP_Assert ( this->expat != 0 );
	
	XMP_StringPtr xdcNS = this->xdcNS.c_str();
	XML_NodePtr legacyContext, legacyProp;
	
	legacyContext = this->clipMetadata->GetNamedElement ( xdcNS, "Access" );
	if ( legacyContext == 0 ) return;

	MD5_CTX    context;
	unsigned char digestBin [16];
	MD5Init ( &context );
	
	legacyProp = legacyContext->GetNamedElement ( xdcNS, "Creator" );
	if ( (legacyProp != 0) && legacyProp->IsLeafContentNode() && (! legacyProp->content.empty()) ) {
		const XML_Node * xmlValue = legacyProp->content[0];
		MD5Update ( &context, (XMP_Uns8*)xmlValue->value.c_str(), (unsigned int)xmlValue->value.size() );
	}
	
	legacyProp = legacyContext->GetNamedElement ( xdcNS, "CreationDate" );
	if ( (legacyProp != 0) && legacyProp->IsLeafContentNode() && (! legacyProp->content.empty()) ) {
		const XML_Node * xmlValue = legacyProp->content[0];
		MD5Update ( &context, (XMP_Uns8*)xmlValue->value.c_str(), (unsigned int)xmlValue->value.size() );
	}
	
	legacyProp = legacyContext->GetNamedElement ( xdcNS, "LastUpdateDate" );
	if ( (legacyProp != 0) && legacyProp->IsLeafContentNode() && (! legacyProp->content.empty()) ) {
		const XML_Node * xmlValue = legacyProp->content[0];
		MD5Update ( &context, (XMP_Uns8*)xmlValue->value.c_str(), (unsigned int)xmlValue->value.size() );
	}

	MD5Final ( digestBin, &context );

	char buffer [40];
	for ( int in = 0, out = 0; in < 16; in += 1, out += 2 ) {
		XMP_Uns8 byte = digestBin[in];
		buffer[out]   = kHexDigits [ byte >> 4 ];
		buffer[out+1] = kHexDigits [ byte & 0xF ];
	}
	buffer[32] = 0;
	digestStr->append ( buffer );

}	// XDCAMEX_MetaHandler::MakeLegacyDigest

// =================================================================================================
// XDCAMEX_MetaHandler::CleanupLegacyXML
// =====================================

void XDCAMEX_MetaHandler::CleanupLegacyXML()
{
	
	if ( this->expat != 0 ) { delete ( this->expat ); this->expat = 0; }
	
	clipMetadata = 0;	// ! Was a pointer into the expat tree.

}	// XDCAMEX_MetaHandler::CleanupLegacyXML

// =================================================================================================
// XDCAMEX_MetaHandler::CacheFileData
// ==================================

void XDCAMEX_MetaHandler::CacheFileData()
{
	XMP_Assert ( ! this->containsXMP );
	
	// See if the clip's .XMP file exists.
	
	std::string xmpPath;
	this->MakeClipFilePath ( &xmpPath, "M01.XMP" );
	if ( GetFileMode ( xmpPath.c_str() ) != kFMode_IsFile ) return;	// No XMP.
	
	// Read the entire .XMP file.
	
	char openMode = 'r';
	if ( this->parent->openFlags & kXMPFiles_OpenForUpdate ) openMode = 'w';
	
	LFA_FileRef xmpFile = LFA_Open ( xmpPath.c_str(), openMode );
	if ( xmpFile == 0 ) return;	// The open failed.
	
	XMP_Int64 xmpLen = LFA_Measure ( xmpFile );
	if ( xmpLen > 100*1024*1024 ) {
		XMP_Throw ( "XDCAMEX XMP is outrageously large", kXMPErr_InternalFailure );	// Sanity check.
	}
	
	this->xmpPacket.erase();
	this->xmpPacket.reserve ( (size_t)xmpLen );
	this->xmpPacket.append ( (size_t)xmpLen, ' ' );

	XMP_Int32 ioCount = LFA_Read ( xmpFile, (void*)this->xmpPacket.data(), (XMP_Int32)xmpLen, kLFA_RequireAll );
	XMP_Assert ( ioCount == xmpLen );
	
	this->packetInfo.offset = 0;
	this->packetInfo.length = (XMP_Int32)xmpLen;
	FillPacketInfo ( this->xmpPacket, &this->packetInfo );
	
	XMP_Assert ( this->parent->fileRef == 0 );
	if ( openMode == 'r' ) {
		LFA_Close ( xmpFile );
	} else {
		this->parent->fileRef = xmpFile;
	}
	
	this->containsXMP = true;
	
}	// XDCAMEX_MetaHandler::CacheFileData

// =================================================================================================
// XDCAMEX_MetaHandler::GetTakeDuration
// ====================================

void XDCAMEX_MetaHandler::GetTakeDuration ( const std::string & takeURI, std::string & duration )
{

	// Some versions of gcc can't tolerate goto's across declarations.
	// *** Better yet, avoid this cruft with self-cleaning objects.
	#define CleanupAndExit	\
		{																		\
			if (expat != 0) delete expat;										\
			if (takeXMLFile.fileRef != 0) LFA_Close ( takeXMLFile.fileRef );	\
			return;																\
		}

	duration.clear();
	
	// Build a directory string to the take .xml file.

	std::string takeDir ( takeURI );
	takeDir.erase ( 0, 1 );	// Change the leading "//" to "/", then all '/' to kDirChar.
	if ( kDirChar != '/' ) {
		for ( size_t i = 0, limit = takeDir.size(); i < limit; ++i ) {
			if ( takeDir[i] == '/' ) takeDir[i] = kDirChar;
		}
	}

	std::string takePath ( this->rootPath );
	takePath += kDirChar;
	takePath += "BPAV";
	takePath += takeDir;

	// Replace .SMI with M01.XML.
	if ( takePath.size() > 4 ) {
		takePath.erase ( takePath.size() - 4, 4 );
		takePath += "M01.XML";
	}

	// Parse MEDIAPRO.XML

	XML_NodePtr takeRootElem = 0;
	XML_NodePtr context = 0;
	AutoFile takeXMLFile;

	takeXMLFile.fileRef = LFA_Open ( takePath.c_str(), 'r' );
	if ( takeXMLFile.fileRef == 0 ) return;	// The open failed.

	ExpatAdapter * expat = XMP_NewExpatAdapter ( ExpatAdapter::kUseLocalNamespaces );
	if ( this->expat == 0 ) return;
	
	XMP_Uns8 buffer [64*1024];
	while ( true ) {
		XMP_Int32 ioCount = LFA_Read ( takeXMLFile.fileRef, buffer, sizeof(buffer) );
		if ( ioCount == 0 ) break;
		expat->ParseBuffer ( buffer, ioCount, false /* not the end */ );
	}

	expat->ParseBuffer ( 0, 0, true );	// End the parse.
	LFA_Close ( takeXMLFile.fileRef );
	takeXMLFile.fileRef = 0;
	
	// Get the root node of the XML tree.

	XML_Node & mediaproXMLTree = expat->tree;
	for ( size_t i = 0, limit = mediaproXMLTree.content.size(); i < limit; ++i ) {
		if ( mediaproXMLTree.content[i]->kind == kElemNode ) {
			takeRootElem = mediaproXMLTree.content[i];
		}
	}
	if ( takeRootElem == 0 ) CleanupAndExit
	
	XMP_StringPtr rlName = takeRootElem->name.c_str() + takeRootElem->nsPrefixLen;
	if ( ! XMP_LitMatch ( rlName, "NonRealTimeMeta" ) ) CleanupAndExit
	
	// MediaProfile, Contents
	XMP_StringPtr ns = takeRootElem->ns.c_str();
	context = takeRootElem->GetNamedElement ( ns, "Duration" );
	if ( context != 0 ) {
		XMP_StringPtr durationValue = context->GetAttrValue ( "value" );
		if ( durationValue != 0 ) duration = durationValue;
	}

	CleanupAndExit
	#undef CleanupAndExit

}

// =================================================================================================
// XDCAMEX_MetaHandler::GetTakeUMID
// ================================

void XDCAMEX_MetaHandler::GetTakeUMID ( const std::string& clipUMID, 
										std::string&	   takeUMID, 
										std::string&	   takeXMLURI )
{

	// Some versions of gcc can't tolerate goto's across declarations.
	// *** Better yet, avoid this cruft with self-cleaning objects.
	#define CleanupAndExit	\
		{																				\
			if (expat != 0) delete expat;												\
			if (mediaproXMLFile.fileRef != 0) LFA_Close ( mediaproXMLFile.fileRef );	\
			return;																		\
		}

	takeUMID.clear();
	takeXMLURI.clear();
	
	// Build a directory string to the MEDIAPRO file.

	std::string mediapropath ( this->rootPath );
	mediapropath += kDirChar;
	mediapropath += "BPAV";
	mediapropath += kDirChar;
	mediapropath += "MEDIAPRO.XML";

	// Parse MEDIAPRO.XML.

	XML_NodePtr mediaproRootElem = 0;
	XML_NodePtr contentContext = 0, materialContext = 0;

	AutoFile mediaproXMLFile;
	mediaproXMLFile.fileRef = LFA_Open ( mediapropath.c_str(), 'r' );
	if ( mediaproXMLFile.fileRef == 0 ) return;	// The open failed.

	ExpatAdapter * expat = XMP_NewExpatAdapter ( ExpatAdapter::kUseLocalNamespaces );
	if ( this->expat == 0 ) return;
	
	XMP_Uns8 buffer [64*1024];
	while ( true ) {
		XMP_Int32 ioCount = LFA_Read ( mediaproXMLFile.fileRef, buffer, sizeof(buffer) );
		if ( ioCount == 0 ) break;
		expat->ParseBuffer ( buffer, ioCount, false /* not the end */ );
	}

	expat->ParseBuffer ( 0, 0, true );	// End the parse.
	LFA_Close ( mediaproXMLFile.fileRef );
	mediaproXMLFile.fileRef = 0;
	
	// Get the root node of the XML tree.

	XML_Node & mediaproXMLTree = expat->tree;
	for ( size_t i = 0, limit = mediaproXMLTree.content.size(); i < limit; ++i ) {
		if ( mediaproXMLTree.content[i]->kind == kElemNode ) {
			mediaproRootElem = mediaproXMLTree.content[i];
		}
	}

	if ( mediaproRootElem == 0 ) CleanupAndExit
	XMP_StringPtr rlName = mediaproRootElem->name.c_str() + mediaproRootElem->nsPrefixLen;
	if ( ! XMP_LitMatch ( rlName, "MediaProfile" ) ) CleanupAndExit
	
	//  MediaProfile, Contents

	XMP_StringPtr ns = mediaproRootElem->ns.c_str();
	contentContext = mediaproRootElem->GetNamedElement ( ns, "Contents" );

	if ( contentContext != 0 ) {

		size_t numMaterialElems = contentContext->CountNamedElements ( ns, "Material" );

		for ( size_t i = 0; i < numMaterialElems; ++i ) {	// Iterate over Material tags.

			XML_NodePtr materialElement = contentContext->GetNamedElement ( ns, "Material", i );
			XMP_Assert ( materialElement != 0 );

			XMP_StringPtr umid = materialElement->GetAttrValue ( "umid" );
			XMP_StringPtr uri = materialElement->GetAttrValue ( "uri" );
			
			if ( umid == 0 ) umid = "";
			if ( uri == 0 ) uri = "";

			size_t numComponents = materialElement->CountNamedElements ( ns, "Component" );

			for ( size_t j = 0; j < numComponents; ++j ) {

				XML_NodePtr componentElement = materialElement->GetNamedElement ( ns, "Component", j );
				XMP_Assert ( componentElement != 0 );
				
				XMP_StringPtr compUMID = componentElement->GetAttrValue ( "umid" );

				if ( (compUMID != 0) && (compUMID == clipUMID) ) {
					takeUMID = umid;
					takeXMLURI = uri;
					break;
				}

			}

			if ( ! takeUMID.empty() ) break;

		}

	}

	CleanupAndExit
	#undef CleanupAndExit

}

// =================================================================================================
// XDCAMEX_MetaHandler::ProcessXMP
// ===============================

void XDCAMEX_MetaHandler::ProcessXMP()
{

	// Some versions of gcc can't tolerate goto's across declarations.
	// *** Better yet, avoid this cruft with self-cleaning objects.
	#define CleanupAndExit	\
		{																								\
			bool openForUpdate = XMP_OptionIsSet ( this->parent->openFlags, kXMPFiles_OpenForUpdate );	\
			if ( ! openForUpdate ) this->CleanupLegacyXML();											\
			return;																						\
		}

	if ( this->processedXMP ) return;
	this->processedXMP = true;	// Make sure only called once.
	
	if ( this->containsXMP ) {
		this->xmpObj.ParseFromBuffer ( this->xmpPacket.c_str(), (XMP_StringLen)this->xmpPacket.size() );
	}
	
	// NonRealTimeMeta -> XMP by schema.
	std::string thisUMID, takeUMID, takeXMLURI, takeDuration;
	std::string xmlPath;
	this->MakeClipFilePath ( &xmlPath, "M01.XML" );
	
	AutoFile xmlFile;
	xmlFile.fileRef = LFA_Open ( xmlPath.c_str(), 'r' );
	if ( xmlFile.fileRef == 0 ) return;	// The open failed.
	
	this->expat = XMP_NewExpatAdapter ( ExpatAdapter::kUseLocalNamespaces );
	if ( this->expat == 0 ) XMP_Throw ( "XDCAMEX_MetaHandler: Can't create Expat adapter", kXMPErr_NoMemory );
	
	XMP_Uns8 buffer [64*1024];
	while ( true ) {
		XMP_Int32 ioCount = LFA_Read ( xmlFile.fileRef, buffer, sizeof(buffer) );
		if ( ioCount == 0 ) break;
		this->expat->ParseBuffer ( buffer, ioCount, false /* not the end */ );
	}
	this->expat->ParseBuffer ( 0, 0, true );	// End the parse.
	
	LFA_Close ( xmlFile.fileRef );
	xmlFile.fileRef = 0;
	
	// The root element should be NonRealTimeMeta in some namespace. Take whatever this file uses.

	XML_Node & xmlTree = this->expat->tree;
	XML_NodePtr rootElem = 0;
	
	for ( size_t i = 0, limit = xmlTree.content.size(); i < limit; ++i ) {
		if ( xmlTree.content[i]->kind == kElemNode ) rootElem = xmlTree.content[i];
	}
	
	if ( rootElem == 0 ) CleanupAndExit
	XMP_StringPtr rootLocalName = rootElem->name.c_str() + rootElem->nsPrefixLen;
	if ( ! XMP_LitMatch ( rootLocalName, "NonRealTimeMeta" ) ) CleanupAndExit
	
	this->legacyNS = rootElem->ns;

	// Check the legacy digest.

	XMP_StringPtr legacyNS = this->legacyNS.c_str();
	this->clipMetadata = rootElem;	// ! Save the NonRealTimeMeta pointer for other use.
	
	std::string oldDigest, newDigest;
	bool digestFound = this->xmpObj.GetStructField ( kXMP_NS_XMP, "NativeDigests", kXMP_NS_XMP, "XDCAMEX", &oldDigest, 0 );
	if ( digestFound ) {
		this->MakeLegacyDigest ( &newDigest );
		if ( oldDigest == newDigest ) CleanupAndExit
	}
	
	// If we get here we need find and import the actual legacy elements using the current namespace.
	// Either there is no old digest in the XMP, or the digests differ. In the former case keep any
	// existing XMP, in the latter case take new legacy values.
	this->containsXMP = XDCAM_Support::GetLegacyMetaData ( &this->xmpObj, rootElem, legacyNS, digestFound, thisUMID );
	
	// If this clip is part of a take, add the take number to the relation field, and get the
	// duration from the take metadata.
	GetTakeUMID ( thisUMID, takeUMID, takeXMLURI );
	
	// If this clip is part of a take, update the duration to reflect the take duration rather than
	// the clip duration, and add the take name as a shot name.

	if ( ! takeXMLURI.empty() ) {
	
		// Update duration. This property already exists from clip legacy metadata.
		GetTakeDuration ( takeXMLURI, takeDuration );
		if ( ! takeDuration.empty() ) {
			this->xmpObj.SetStructField ( kXMP_NS_DM, "duration", kXMP_NS_DM, "value", takeDuration );
			containsXMP = true;
		}
		
		if ( digestFound || (! this->xmpObj.DoesPropertyExist ( kXMP_NS_DM, "shotName" )) ) {
		
			std::string takeName;
			SplitLeafName ( &takeXMLURI, &takeName );

			// Check for the xml suffix, and delete if it exists. 
			size_t pos = takeName.rfind(".SMI");
			if ( pos != std::string::npos ) {

				takeName.erase ( pos );
				
				// delete the take number suffix if it exists.
				if ( takeName.size() > 3 ) {

					size_t suffix = takeName.size() - 3;
					char c1 = takeName[suffix];
					char c2 = takeName[suffix+1];
					char c3 = takeName[suffix+2];
					if ( ('U' == c1) && ('0' <= c2) && (c2 <= '9') && ('0' <= c3) && (c3 <= '9') ) {
						takeName.erase ( suffix );
					}
	
					this->xmpObj.SetProperty ( kXMP_NS_DM, "shotName", takeName, kXMP_DeleteExisting );
					containsXMP = true;

				}

			}

		}
		
	}
	
	if ( (! takeUMID.empty()) &&
		 (digestFound || (! this->xmpObj.DoesPropertyExist ( kXMP_NS_DC, "relation" ))) ) {
		this->xmpObj.DeleteProperty ( kXMP_NS_DC, "relation" );
		this->xmpObj.AppendArrayItem ( kXMP_NS_DC, "relation", kXMP_PropArrayIsUnordered, takeUMID );
		this->containsXMP = true;
	}

	CleanupAndExit
	#undef CleanupAndExit

}	// XDCAMEX_MetaHandler::ProcessXMP


// =================================================================================================
// XDCAMEX_MetaHandler::UpdateFile
// ===============================
//
// Note that UpdateFile is only called from XMPFiles::CloseFile, so it is OK to close the file here.

void XDCAMEX_MetaHandler::UpdateFile ( bool doSafeUpdate )
{
	if ( ! this->needsUpdate ) return;
	this->needsUpdate = false;	// Make sure only called once.
	
	LFA_FileRef oldFile = 0;
	std::string filePath, tempPath;
	
	// Update the internal legacy XML tree if we have one, and set the digest in the XMP.
	
	bool updateLegacyXML = false;
	
	if ( this->clipMetadata != 0 ) {
		updateLegacyXML = XDCAM_Support::SetLegacyMetaData ( this->clipMetadata, &this->xmpObj, this->legacyNS.c_str());
	}

	std::string newDigest;
	this->MakeLegacyDigest ( &newDigest );
	this->xmpObj.SetStructField ( kXMP_NS_XMP, "NativeDigests", kXMP_NS_XMP, "XDCAMEX", newDigest.c_str(), kXMP_DeleteExisting );
	this->xmpObj.SerializeToBuffer ( &this->xmpPacket, this->GetSerializeOptions() );
	
	// Update the legacy XML file if necessary.
	
	if ( updateLegacyXML ) {
	
		std::string legacyXML;
		this->expat->tree.Serialize ( &legacyXML );
	
		this->MakeClipFilePath ( &filePath, "M01.XML" );
		oldFile = LFA_Open ( filePath.c_str(), 'w' );
	
		if ( oldFile == 0 ) {
		
			// The XML does not exist yet.
	
			this->MakeClipFilePath ( &filePath, "M01.XML" );
			oldFile = LFA_Create ( filePath.c_str() );
			if ( oldFile == 0 ) XMP_Throw ( "Failure creating XDCAMEX legacy XML file", kXMPErr_ExternalFailure );
			LFA_Write ( oldFile, legacyXML.data(), (XMP_StringLen)legacyXML.size() );
			LFA_Close ( oldFile );
		
		} else if ( ! doSafeUpdate ) {
	
			// Over write the existing XML file.
			
			LFA_Seek ( oldFile, 0, SEEK_SET );
			LFA_Truncate ( oldFile, 0 );
			LFA_Write ( oldFile, legacyXML.data(), (XMP_StringLen)legacyXML.size() );
			LFA_Close ( oldFile );
	
		} else {
	
			// Do a safe update.
			
			// *** We really need an LFA_SwapFiles utility.
			
			this->MakeClipFilePath ( &filePath, "M01.XML" );
			
			CreateTempFile ( filePath, &tempPath );
			LFA_FileRef tempFile = LFA_Open ( tempPath.c_str(), 'w' );
			LFA_Write ( tempFile, legacyXML.data(), (XMP_StringLen)legacyXML.size() );
			LFA_Close ( tempFile );
			
			LFA_Close ( oldFile );
			LFA_Delete ( filePath.c_str() );
			LFA_Rename ( tempPath.c_str(), filePath.c_str() );
	
		}
		
	}
	
	oldFile = this->parent->fileRef;
	
	if ( oldFile == 0 ) {
	
		// The XMP does not exist yet.

		std::string xmpPath;
		this->MakeClipFilePath ( &xmpPath, "M01.XMP" );
		
		LFA_FileRef xmpFile = LFA_Create ( xmpPath.c_str() );
		if ( xmpFile == 0 ) XMP_Throw ( "Failure creating XDCAMEX XMP file", kXMPErr_ExternalFailure );
		LFA_Write ( xmpFile, this->xmpPacket.data(), (XMP_StringLen)this->xmpPacket.size() );
		LFA_Close ( xmpFile );
	
	} else if ( ! doSafeUpdate ) {

		// Over write the existing XMP file.
		
		LFA_Seek ( oldFile, 0, SEEK_SET );
		LFA_Truncate ( oldFile, 0 );
		LFA_Write ( oldFile, this->xmpPacket.data(), (XMP_StringLen)this->xmpPacket.size() );
		LFA_Close ( oldFile );

	} else {

		// Do a safe update.
		
		// *** We really need an LFA_SwapFiles utility.
		
		std::string xmpPath, tempPath;
		
		this->MakeClipFilePath ( &xmpPath, "M01.XMP" );
		
		CreateTempFile ( xmpPath, &tempPath );
		LFA_FileRef tempFile = LFA_Open ( tempPath.c_str(), 'w' );
		LFA_Write ( tempFile, this->xmpPacket.data(), (XMP_StringLen)this->xmpPacket.size() );
		LFA_Close ( tempFile );
		
		LFA_Close ( oldFile );
		LFA_Delete ( xmpPath.c_str() );
		LFA_Rename ( tempPath.c_str(), xmpPath.c_str() );

	}

	this->parent->fileRef = 0;
	
}	// XDCAMEX_MetaHandler::UpdateFile

// =================================================================================================
// XDCAMEX_MetaHandler::WriteFile
// ==============================

void XDCAMEX_MetaHandler::WriteFile ( LFA_FileRef sourceRef, const std::string & sourcePath )
{

	// ! WriteFile is not supposed to be called for handlers that own the file.
	XMP_Throw ( "XDCAMEX_MetaHandler::WriteFile should not be called", kXMPErr_InternalFailure );
	
}	// XDCAMEX_MetaHandler::WriteFile

// =================================================================================================
