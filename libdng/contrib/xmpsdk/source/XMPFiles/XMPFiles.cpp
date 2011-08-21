// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2004 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include <vector>
#include <string.h>

#include "XMPFiles_Impl.hpp"
#include "UnicodeConversions.hpp"

// These are the official, fully supported handlers.
#include "FileHandlers/JPEG_Handler.hpp"
#include "FileHandlers/TIFF_Handler.hpp"
#include "FileHandlers/PSD_Handler.hpp"
#include "FileHandlers/InDesign_Handler.hpp"
#include "FileHandlers/PostScript_Handler.hpp"
#include "FileHandlers/Scanner_Handler.hpp"
#include "FileHandlers/MPEG2_Handler.hpp"
#include "FileHandlers/PNG_Handler.hpp"
#include "FileHandlers/RIFF_Handler.hpp"
#include "FileHandlers/MP3_Handler.hpp"
#include "FileHandlers/SWF_Handler.hpp"
#include "FileHandlers/UCF_Handler.hpp"
#include "FileHandlers/MPEG4_Handler.hpp"
#include "FileHandlers/FLV_Handler.hpp"
#include "FileHandlers/P2_Handler.hpp"
#include "FileHandlers/SonyHDV_Handler.hpp"
#include "FileHandlers/XDCAM_Handler.hpp"
#include "FileHandlers/XDCAMEX_Handler.hpp"
#include "FileHandlers/AVCHD_Handler.hpp"
#include "FileHandlers/ASF_Handler.hpp"

// =================================================================================================
/// \file XMPFiles.cpp
/// \brief High level support to access metadata in files of interest to Adobe applications.
///
/// This header ...
///
// =================================================================================================
 
// =================================================================================================

XMP_Int32 sXMPFilesInitCount = 0;

#if GatherPerformanceData
	APIPerfCollection* sAPIPerf = 0;
#endif

// These are embedded version strings.

#if XMP_DebugBuild
	#define kXMPFiles_DebugFlag 1
#else
	#define kXMPFiles_DebugFlag 0
#endif

#define kXMPFiles_VersionNumber	( (kXMPFiles_DebugFlag << 31)    |	\
                                  (XMP_API_VERSION_MAJOR << 24) |	\
						          (XMP_API_VERSION_MINOR << 16) |	\
						          (XMP_API_VERSION_MICRO << 8) )

	#define kXMPFilesName "XMP Files"
	#define kXMPFiles_VersionMessage kXMPFilesName " " XMP_API_VERSION_STRING
const char * kXMPFiles_EmbeddedVersion   = kXMPFiles_VersionMessage;
const char * kXMPFiles_EmbeddedCopyright = kXMPFilesName " " kXMP_CopyrightStr;

// =================================================================================================

struct XMPFileHandlerInfo {
	XMP_FileFormat format;
	XMP_OptionBits flags;
	void *         checkProc;
	XMPFileHandlerCTor handlerCTor;
	XMPFileHandlerInfo() : format(0), flags(0), checkProc(0), handlerCTor(0) {};
	XMPFileHandlerInfo ( XMP_FileFormat _format, XMP_OptionBits _flags,
	                     CheckFileFormatProc _checkProc, XMPFileHandlerCTor _handlerCTor )
		: format(_format), flags(_flags), checkProc((void*)_checkProc), handlerCTor(_handlerCTor) {};
	XMPFileHandlerInfo ( XMP_FileFormat _format, XMP_OptionBits _flags,
	                     CheckFolderFormatProc _checkProc, XMPFileHandlerCTor _handlerCTor )
		: format(_format), flags(_flags), checkProc((void*)_checkProc), handlerCTor(_handlerCTor) {};
};

// Don't use a map for the handler tables, 
typedef std::map <XMP_FileFormat, XMPFileHandlerInfo> XMPFileHandlerTable;
typedef XMPFileHandlerTable::iterator XMPFileHandlerTablePos;
typedef std::pair <XMP_FileFormat, XMPFileHandlerInfo> XMPFileHandlerTablePair;

static XMPFileHandlerTable * sFolderHandlers = 0;	// The directory-oriented handlers.
static XMPFileHandlerTable * sNormalHandlers = 0;	// The normal file-oriented handlers.
static XMPFileHandlerTable * sOwningHandlers = 0;	// The file-oriented handlers that "own" the file.

static XMPFileHandlerInfo kScannerHandlerInfo ( kXMP_UnknownFile, kScanner_HandlerFlags, (CheckFileFormatProc)0, Scanner_MetaHandlerCTor );

// =================================================================================================

static void
RegisterFolderHandler ( XMP_FileFormat        format,
						XMP_OptionBits        flags,
						CheckFolderFormatProc checkProc,
						XMPFileHandlerCTor    handlerCTor )
{
	XMP_Assert ( format != kXMP_UnknownFile );
	std::string noExt;

	XMP_Assert ( flags & kXMPFiles_HandlerOwnsFile );
	XMP_Assert ( flags & kXMPFiles_FolderBasedFormat );
	XMP_Assert ( (flags & kXMPFiles_CanInjectXMP) ? (flags & kXMPFiles_CanExpand) : 1 );
	
	XMP_Assert ( sFolderHandlers->find ( format ) == sFolderHandlers->end() );
	XMP_Assert ( sNormalHandlers->find ( format ) == sNormalHandlers->end() );
	XMP_Assert ( sOwningHandlers->find ( format ) == sOwningHandlers->end() );

	XMPFileHandlerInfo handlerInfo ( format, flags, checkProc, handlerCTor );
	sFolderHandlers->insert ( sFolderHandlers->end(), XMPFileHandlerTablePair ( format, handlerInfo ) );
	
}	// RegisterFolderHandler

// =================================================================================================

static void
RegisterNormalHandler ( XMP_FileFormat      format,
						XMP_OptionBits      flags,
						CheckFileFormatProc checkProc,
						XMPFileHandlerCTor  handlerCTor )
{
	XMP_Assert ( format != kXMP_UnknownFile );
	std::string noExt;

	XMP_Assert ( ! (flags & kXMPFiles_HandlerOwnsFile) );
	XMP_Assert ( ! (flags & kXMPFiles_FolderBasedFormat) );
	XMP_Assert ( (flags & kXMPFiles_CanInjectXMP) ? (flags & kXMPFiles_CanExpand) : 1 );
	
	XMP_Assert ( sFolderHandlers->find ( format ) == sFolderHandlers->end() );
	XMP_Assert ( sNormalHandlers->find ( format ) == sNormalHandlers->end() );
	XMP_Assert ( sOwningHandlers->find ( format ) == sOwningHandlers->end() );

	XMPFileHandlerInfo handlerInfo ( format, flags, checkProc, handlerCTor );
	sNormalHandlers->insert ( sNormalHandlers->end(), XMPFileHandlerTablePair ( format, handlerInfo ) );
	
}	// RegisterNormalHandler

// =================================================================================================

static void
RegisterOwningHandler ( XMP_FileFormat      format,
						XMP_OptionBits      flags,
						CheckFileFormatProc checkProc,
						XMPFileHandlerCTor  handlerCTor )
{
	XMP_Assert ( format != kXMP_UnknownFile );
	std::string noExt;

	XMP_Assert ( flags & kXMPFiles_HandlerOwnsFile );
	XMP_Assert ( ! (flags & kXMPFiles_FolderBasedFormat) );
	XMP_Assert ( (flags & kXMPFiles_CanInjectXMP) ? (flags & kXMPFiles_CanExpand) : 1 );
	
	XMP_Assert ( sFolderHandlers->find ( format ) == sFolderHandlers->end() );
	XMP_Assert ( sNormalHandlers->find ( format ) == sNormalHandlers->end() );
	XMP_Assert ( sOwningHandlers->find ( format ) == sOwningHandlers->end() );

	XMPFileHandlerInfo handlerInfo ( format, flags, checkProc, handlerCTor );
	sOwningHandlers->insert ( sOwningHandlers->end(), XMPFileHandlerTablePair ( format, handlerInfo ) );
	
}	// RegisterOwningHandler

// =================================================================================================

static XMPFileHandlerInfo *
PickDefaultHandler ( XMP_FileFormat format, const std::string & fileExt )
{
	if ( (format == kXMP_UnknownFile) && (! fileExt.empty()) ) {
		for ( int i = 0; kFileExtMap[i].format != 0; ++i ) {
			if ( fileExt == kFileExtMap[i].ext ) {
				format = kFileExtMap[i].format;
				break;
			}
		}
	}
	
	if ( format == kXMP_UnknownFile ) return 0;

	XMPFileHandlerTablePos handlerPos;
	
	handlerPos = sNormalHandlers->find ( format );
	if ( handlerPos != sNormalHandlers->end() ) return &handlerPos->second;
	
	handlerPos = sOwningHandlers->find ( format );
	if ( handlerPos != sOwningHandlers->end() ) return &handlerPos->second;
	
	handlerPos = sFolderHandlers->find ( format );
	if ( handlerPos != sFolderHandlers->end() ) return &handlerPos->second;
	
	return 0;
	
}	// PickDefaultHandler

// =================================================================================================

static const char * kP2ContentChildren[] = { "CLIP", "VIDEO", "AUDIO", "ICON", "VOICE", "PROXY", 0 };

static inline bool CheckP2ContentChild ( const std::string & folderName )
{
	for ( int i = 0; kP2ContentChildren[i] != 0; ++i ) {
		if ( folderName == kP2ContentChildren[i] ) return true;
	}
	return false;
}

// -------------------------------------------------------------------------------------------------

static XMP_FileFormat
CheckParentFolderNames ( const std::string & rootPath,   const std::string & gpName,
						 const std::string & parentName, const std::string & leafName )
{
	IgnoreParam ( parentName );
	
	// This is called when the input path to XMPFiles::OpenFile names an existing file. We need to
	// quickly decide if this might be inside a folder-handler's structure. See if the containing
	// folders might match any of the registered folder handlers. This check does not have to be
	// precise, the handler will do that. This does have to be fast.
	//
	// Since we don't have many folder handlers, this is simple hardwired code. Note that the caller
	// has already shifted the names to upper case.

	// P2  .../MyMovie/CONTENTS/<group>/<file>.<ext>  -  check CONTENTS and <group>
	if ( (gpName == "CONTENTS") && CheckP2ContentChild ( parentName ) ) return kXMP_P2File;

	// XDCAM-EX  .../MyMovie/BPAV/CLPR/<clip>/<file>.<ext>  -  check for BPAV/CLPR
	// ! This must be checked before XDCAM-SAM because both have a "CLPR" grandparent.
	if ( gpName == "CLPR" ) {
		std::string tempPath, greatGP;
		tempPath = rootPath;
		SplitLeafName ( &tempPath, &greatGP );
		MakeUpperCase ( &greatGP );
		if ( greatGP == "BPAV" ) return kXMP_XDCAM_EXFile;
	}

	// XDCAM-FAM  .../MyMovie/<group>/<file>.<ext>  -  check that <group> is CLIP, or EDIT, or SUB
	// ! The standard says Clip/Edit/Sub, but the caller has already shifted to upper case.
	if ( (parentName == "CLIP") || (parentName == "EDIT") || (parentName == "SUB") ) return kXMP_XDCAM_FAMFile;

	// XDCAM-SAM  .../MyMovie/PROAV/<group>/<clip>/<file>.<ext>  -  check for PROAV and CLPR or EDTR
	if ( (gpName == "CLPR") || (gpName == "EDTR") ) {
		std::string tempPath, greatGP;
		tempPath = rootPath;
		SplitLeafName ( &tempPath, &greatGP );
		MakeUpperCase ( &greatGP );
		if ( greatGP == "PROAV" ) return kXMP_XDCAM_SAMFile;
	}

	// Sony HDV  .../MyMovie/VIDEO/HVR/<file>.<ext>  -  check for VIDEO and HVR
	if ( (gpName == "VIDEO") && (parentName == "HVR") ) return kXMP_SonyHDVFile;
	
	// AVCHD .../MyMovie/BDMV/<group>/<file>.<ext>  -  check for BDMV and CLIPINF or STREAM
	if ( (gpName == "BDMV") && ((parentName == "CLIPINF") || (parentName == "STREAM")) ) return kXMP_AVCHDFile;
	
	return kXMP_UnknownFile;
	
}	// CheckParentFolderNames

// =================================================================================================

static XMP_FileFormat
CheckTopFolderName ( const std::string & rootPath )
{
	// This is called when the input path to XMPFiles::OpenFile does not name an existing file (or
	// existing anything). We need to quickly decide if this might be a logical path for a folder
	// handler. See if the root contains the top content folder for any of the registered folder
	// handlers. This check does not have to be precise, the handler will do that. This does have to
	// be fast.
	//
	// Since we don't have many folder handlers, this is simple hardwired code.
	
	std::string childPath = rootPath;
	childPath += kDirChar;
	size_t baseLen = childPath.size();
	
	// P2 .../MyMovie/CONTENTS/<group>/...  -  only check for CONTENTS/CLIP
	childPath += "CONTENTS";
	childPath += kDirChar;
	childPath += "CLIP";
	if ( GetFileMode ( childPath.c_str() ) == kFMode_IsFolder ) return kXMP_P2File;
	childPath.erase ( baseLen );

	// XDCAM-FAM  .../MyMovie/<group>/...  -  only check for Clip and MEDIAPRO.XML
	childPath += "Clip";	// ! Yes, mixed case.
	if ( GetFileMode ( childPath.c_str() ) == kFMode_IsFolder ) {
		childPath.erase ( baseLen );
		childPath += "MEDIAPRO.XML";
		if ( GetFileMode ( childPath.c_str() ) == kFMode_IsFile ) return kXMP_XDCAM_FAMFile;
	}
	childPath.erase ( baseLen );

	// XDCAM-SAM .../MyMovie/PROAV/<group>/...  -  only check for PROAV/CLPR
	childPath += "PROAV";
	childPath += kDirChar;
	childPath += "CLPR";
	if ( GetFileMode ( childPath.c_str() ) == kFMode_IsFolder ) return kXMP_XDCAM_SAMFile;
	childPath.erase ( baseLen );
	
	// XDCAM-EX  .../MyMovie/BPAV/<group>/...  -  check for BPAV/CLPR
	childPath += "BPAV";
	childPath += kDirChar;
	childPath += "CLPR";
	if ( GetFileMode ( childPath.c_str() ) == kFMode_IsFolder ) return kXMP_XDCAM_EXFile;
	childPath.erase ( baseLen );
	
	// Sony HDV  .../MyMovie/VIDEO/HVR/<file>.<ext>  -  check for VIDEO/HVR
	childPath += "VIDEO";
	childPath += kDirChar;
	childPath += "HVR";
	if ( GetFileMode ( childPath.c_str() ) == kFMode_IsFolder ) return kXMP_SonyHDVFile;
	childPath.erase ( baseLen );

	// AVCHD  .../MyMovie/BDMV/CLIPINF/<file>.<ext>  -  check for BDMV/CLIPINF
	childPath += "BDMV";
	childPath += kDirChar;
	childPath += "CLIPINF";
	if ( GetFileMode ( childPath.c_str() ) == kFMode_IsFolder ) return kXMP_AVCHDFile;
	childPath.erase ( baseLen );

	return kXMP_UnknownFile;

}	// CheckTopFolderName

// =================================================================================================

static XMPFileHandlerInfo *
TryFolderHandlers ( XMP_FileFormat format,
					const std::string & rootPath,
					const std::string & gpName,
					const std::string & parentName,
					const std::string & leafName,
					XMPFiles * parentObj )
{
	bool foundHandler = false;
	XMPFileHandlerInfo * handlerInfo = 0;
	XMPFileHandlerTablePos handlerPos;
	
	// We know we're in a possible context for a folder-oriented handler, so try them.
	
	if ( format != kXMP_UnknownFile ) {

		// Have an explicit format, pick that or nothing.
		handlerPos = sFolderHandlers->find ( format );
		if ( handlerPos != sFolderHandlers->end() ) {
			handlerInfo = &handlerPos->second;
			CheckFolderFormatProc CheckProc = (CheckFolderFormatProc) (handlerInfo->checkProc);
			foundHandler = CheckProc ( handlerInfo->format, rootPath, gpName, parentName, leafName, parentObj );
			XMP_Assert ( foundHandler || (parentObj->tempPtr == 0) );
		}

	} else {
		
		// Try all of the folder handlers.
		for ( handlerPos = sFolderHandlers->begin(); handlerPos != sFolderHandlers->end(); ++handlerPos ) {
			handlerInfo = &handlerPos->second;
			CheckFolderFormatProc CheckProc = (CheckFolderFormatProc) (handlerInfo->checkProc);
			foundHandler = CheckProc ( handlerInfo->format, rootPath, gpName, parentName, leafName, parentObj );
			XMP_Assert ( foundHandler || (parentObj->tempPtr == 0) );
			if ( foundHandler ) break;	// ! Exit before incrementing handlerPos.
		}

	}
	
	if ( ! foundHandler ) handlerInfo = 0;
	return handlerInfo;

}	// TryFolderHandlers

// =================================================================================================

static XMPFileHandlerInfo *
SelectSmartHandler ( XMPFiles * thiz, XMP_StringPtr clientPath, XMP_FileFormat format, XMP_OptionBits openFlags )
{
	
	// There are 4 stages in finding a handler, ending at the first success:
	//   1. If the client passes in a format, try that handler.
	//   2. Try all of the folder-oriented handlers.
	//   3. Try a file-oriented handler based on the file extension.
	//   4. Try all of the file-oriented handlers.
	//
	// The most common case is almost certainly #3, so we want to get there quickly. Most of the
	// time the client won't pass in a format, so #1 takes no time. The folder-oriented handler
	// checks are preceded by minimal folder checks. These checks are meant to be fast in the
	// failure case. The folder-oriented checks have to go before the general file-oriented checks
	// because the client path might be to one of the inner files, and we might have a file-oriented
	// handler for that kind of file, but we want to recognize the clip. More details are below.
	//
	// In brief, the folder-oriented formats use shallow trees with specific folder names and
	// highly stylized file names. The user thinks of the tree as a collection of clips, each clip
	// is stored as multiple files for video, audio, metadata, etc. The folder-oriented stage has
	// to be first because there can be files in the structure that are also covered by a
	// file-oriented handler.
	//
	// In the file-oriented case, the CheckProc should do as little as possible to determine the
	// format, based on the actual file content. If that is not possible, use the format hint. The
	// initial CheckProc calls (steps 1 and 3) has the presumed format in this->format, the later
	// calls (step 4) have kXMP_UnknownFile there.
	//
	// The folder-oriented checks need to be well optimized since the formats are relatively rare,
	// but have to go first and could require multiple file system calls to identify. We want to
	// get to the first file-oriented guess as quickly as possible, that is the real handler most of
	// the time.
	//
	// The folder-oriented handlers are for things like P2 and XDCAM that use files distributed in a
	// well defined folder structure. Using a portion of P2 as an example:
	//	.../MyMovie
	//		CONTENTS
	//			CLIP
	//				0001AB.XML
	//				0002CD.XML
	//			VIDEO
	//				0001AB.MXF
	//				0002CD.MXF
	//			VOICE
	//				0001AB.WAV
	//				0002CD.WAV
	//
	// The user thinks of .../MyMovie as the container of P2 stuff, in this case containing 2 clips
	// called 0001AB and 0002CD. The exact folder structure and file layout differs, but the basic
	// concepts carry across all of the folder-oriented handlers.
	//
	// The client path can be a conceptual clip path like .../MyMovie/0001AB, or a full path to any
	// of the contained files. For file paths we have to behave the same as the implied conceptual
	// path, e.g. we don't want .../MyMovie/CONTENTS/VOICE/0001AB.WAV to invoke the WAV handler.
	// There might also be a mapping from user friendly names to clip names (e.g. Intro to 0001AB).
	// If so that is private to the handler and does not affect this code.
	//
	// In order to properly handle the file path input we have to look for the folder-oriented case
	// before any of the file-oriented cases. And since these are relatively rare, hence fail most of
	// the time, we have to get in and out fast in the not handled case. That is what we do here.
	//
	// The folder-oriented processing done here is roughly:
	//
	// 1. Get the state of the client path: does-not-exist, is-file, is-folder, is-other.
	// 2. Reject is-folder and is-other, they can't possibly be a valid case.
	// 3. For does-not-exist:
	//	3a. Split the client path into a leaf component and root path.
	//	3b. Make sure the root path names an existing folder.
	//	3c. Make sure the root folder has a viable top level child folder (e.g. CONTENTS).
	// 4. For is-file:
	//	4a. Split the client path into a root path, grandparent folder, parent folder, and leaf name.
	//	4b. Make sure the parent or grandparent has a viable name (e.g. CONTENTS).
	// 5. Try the registered folder handlers.
	//
	// For the common case of "regular" files, we should only get as far as 3b. This is just 1 file
	// system call to get the client path state and some string processing.
	
	char openMode = 'r';
	if ( openFlags & kXMPFiles_OpenForUpdate ) openMode = 'w';

	XMPFileHandlerInfo * handlerInfo = 0;
	bool foundHandler = false;
	
	FileMode clientMode = GetFileMode ( clientPath );
	if ( (clientMode == kFMode_IsFolder) || (clientMode == kFMode_IsOther) ) return 0;
	
	// Extract some info from the clientPath, needed for various checks.
	
	std::string rootPath, leafName, fileExt;

	rootPath = clientPath;
	SplitLeafName ( &rootPath, &leafName );
	if ( leafName.empty() ) return 0;

	size_t extPos = leafName.size();
	for ( --extPos; extPos > 0; --extPos ) if ( leafName[extPos] == '.' ) break;
	if ( leafName[extPos] == '.' ) {
		fileExt.assign ( &leafName[extPos+1] );
		MakeLowerCase ( &fileExt );
		leafName.erase ( extPos );
	}
	
	thiz->format = kXMP_UnknownFile;	// Make sure it is preset for later checks.
	thiz->openFlags = openFlags;
	
	// If the client passed in a format, try that first.
	
	if ( format != kXMP_UnknownFile ) {

		std::string emptyStr;
		handlerInfo = PickDefaultHandler ( format, emptyStr );	// Picks based on just the format.
	
		if ( handlerInfo != 0 ) {

			if ( (thiz->fileRef == 0) && (! (handlerInfo->flags & kXMPFiles_HandlerOwnsFile)) ) {
				thiz->fileRef = LFA_Open ( clientPath, openMode );
				XMP_Assert ( thiz->fileRef != 0 ); // LFA_Open must either succeed or throw.
			}
			thiz->format = format;	// ! Hack to tell the CheckProc thiz is an initial call.

			if ( ! (handlerInfo->flags & kXMPFiles_FolderBasedFormat) ) {
				CheckFileFormatProc CheckProc = (CheckFileFormatProc) (handlerInfo->checkProc);
				foundHandler = CheckProc ( format, clientPath, thiz->fileRef, thiz );
			} else {
				// *** Don't try here yet. These are messy, needing existence checking and path processing.
				// *** CheckFolderFormatProc CheckProc = (CheckFolderFormatProc) (handlerInfo->checkProc);
				// *** foundHandler = CheckProc ( handlerInfo->format, rootPath, gpName, parentName, leafName, thiz );
				// *** Don't let OpenStrictly cause an early exit:
				if ( openFlags & kXMPFiles_OpenStrictly ) openFlags ^= kXMPFiles_OpenStrictly;
			}

			XMP_Assert ( foundHandler || (thiz->tempPtr == 0) );
			if ( foundHandler ) return handlerInfo;
			handlerInfo = 0;	// ! Clear again for later use.

		}
	
		if ( openFlags & kXMPFiles_OpenStrictly ) return 0;

	}

	// Try the folder handlers if appropriate.

	XMP_Assert ( handlerInfo == 0 );
	XMP_Assert ( (clientMode == kFMode_IsFile) || (clientMode == kFMode_DoesNotExist) );

	std::string gpName, parentName;
	
	if ( clientMode == kFMode_DoesNotExist ) {

		// 3. For does-not-exist:
		//	3a. Split the client path into a leaf component and root path.
		//	3b. Make sure the root path names an existing folder.
		//	3c. Make sure the root folder has a viable top level child folder.
		
		// ! This does "return 0" on failure, the file does not exist so a normal file handler can't apply.

		if ( GetFileMode ( rootPath.c_str() ) != kFMode_IsFolder ) return 0;
		thiz->format = CheckTopFolderName ( rootPath );
		if ( thiz->format == kXMP_UnknownFile ) return 0;

		handlerInfo = TryFolderHandlers ( thiz->format, rootPath, gpName, parentName, leafName, thiz );	// ! Parent and GP are empty.
		return handlerInfo;	// ! Return found handler or 0.

	}

	XMP_Assert ( clientMode == kFMode_IsFile );

	// 4. For is-file:
	//	4a. Split the client path into root, grandparent, parent, and leaf.
	//	4b. Make sure the parent or grandparent has a viable name.

	// ! Don't "return 0" on failure, this has to fall through to the normal file handlers.

	SplitLeafName ( &rootPath, &parentName );
	SplitLeafName ( &rootPath, &gpName );
	std::string origGPName ( gpName );	// ! Save the original case for XDCAM-FAM.
	MakeUpperCase ( &parentName );
	MakeUpperCase ( &gpName );

	thiz->format = CheckParentFolderNames ( rootPath, gpName, parentName, leafName );

	if ( thiz->format != kXMP_UnknownFile ) {

		if ( (thiz->format == kXMP_XDCAM_FAMFile) &&
			 ((parentName == "CLIP") || (parentName == "EDIT") || (parentName == "SUB")) ) {
			// ! The standard says Clip/Edit/Sub, but we just shifted to upper case.
			gpName = origGPName;	// ! XDCAM-FAM has just 1 level of inner folder, preserve the "MyMovie" case.
		}

		handlerInfo = TryFolderHandlers ( thiz->format, rootPath, gpName, parentName, leafName, thiz );
		if ( handlerInfo != 0 ) return handlerInfo;

	}

	// Try an initial file-oriented handler based on the extension.

	handlerInfo = PickDefaultHandler ( kXMP_UnknownFile, fileExt );	// Picks based on just the extension.

	if ( handlerInfo != 0 ) {
		if ( (thiz->fileRef == 0) && (! (handlerInfo->flags & kXMPFiles_HandlerOwnsFile)) ) {
			thiz->fileRef = LFA_Open ( clientPath, openMode );
			XMP_Assert ( thiz->fileRef != 0 ); // LFA_Open must either succeed or throw.
		} else if ( (thiz->fileRef != 0) && (handlerInfo->flags & kXMPFiles_HandlerOwnsFile) ) {
			LFA_Close ( thiz->fileRef );
			thiz->fileRef = 0;
		}
		thiz->format = handlerInfo->format;	// ! Hack to tell the CheckProc thiz is an initial call.
		CheckFileFormatProc CheckProc = (CheckFileFormatProc) (handlerInfo->checkProc);
		foundHandler = CheckProc ( handlerInfo->format, clientPath, thiz->fileRef, thiz );
		XMP_Assert ( foundHandler || (thiz->tempPtr == 0) );
		if ( foundHandler ) return handlerInfo;
	}
			
	// Search the handlers that don't want to open the file themselves.

	if ( thiz->fileRef == 0 ) thiz->fileRef = LFA_Open ( clientPath, openMode );
	XMP_Assert ( thiz->fileRef != 0 ); // LFA_Open must either succeed or throw.
	XMPFileHandlerTablePos handlerPos = sNormalHandlers->begin();

	for ( ; handlerPos != sNormalHandlers->end(); ++handlerPos ) {
		thiz->format = kXMP_UnknownFile;	// ! Hack to tell the CheckProc this is not an initial call.
		handlerInfo = &handlerPos->second;
		CheckFileFormatProc CheckProc = (CheckFileFormatProc) (handlerInfo->checkProc);
		foundHandler = CheckProc ( handlerInfo->format, clientPath, thiz->fileRef, thiz );
		XMP_Assert ( foundHandler || (thiz->tempPtr == 0) );
		if ( foundHandler ) return handlerInfo;
	}

	// Search the handlers that do want to open the file themselves.

	LFA_Close ( thiz->fileRef );
	thiz->fileRef = 0;
	handlerPos = sOwningHandlers->begin();

	for ( ; handlerPos != sOwningHandlers->end(); ++handlerPos ) {
		thiz->format = kXMP_UnknownFile;	// ! Hack to tell the CheckProc this is not an initial call.
		handlerInfo = &handlerPos->second;
		CheckFileFormatProc CheckProc = (CheckFileFormatProc) (handlerInfo->checkProc);
		foundHandler = CheckProc ( handlerInfo->format, clientPath, thiz->fileRef, thiz );
		XMP_Assert ( foundHandler || (thiz->tempPtr == 0) );
		if ( foundHandler ) return handlerInfo;
	}
	
	// Failed to find a smart handler.
	
	return 0;

}	// SelectSmartHandler

// =================================================================================================

/* class-static */
void
XMPFiles::GetVersionInfo ( XMP_VersionInfo * info )
{

	memset ( info, 0, sizeof(XMP_VersionInfo) );
	
	info->major   = XMP_API_VERSION_MAJOR;
	info->minor   = XMP_API_VERSION_MINOR;
	info->micro   = XMP_API_VERSION_MICRO;
	info->isDebug = kXMPFiles_DebugFlag;
	info->flags   = 0;	// ! None defined yet.
	info->message = kXMPFiles_VersionMessage;
	
}	// XMPFiles::GetVersionInfo

// =================================================================================================

#if XMP_TraceFilesCalls
	FILE * xmpFilesLog = stderr;
#endif

#if UseGlobalLibraryLock & (! XMP_StaticBuild )
	XMP_BasicMutex sLibraryLock;	// ! Handled in XMPMeta for static builds.
#endif

/* class static */
bool
XMPFiles::Initialize ( XMP_OptionBits options /* = 0 */ )
{
	++sXMPFilesInitCount;
	if ( sXMPFilesInitCount > 1 ) return true;
	
	#if XMP_TraceFilesCallsToFile
		xmpFilesLog = fopen ( "XMPFilesLog.txt", "w" );
		if ( xmpFilesLog == 0 ) xmpFilesLog = stderr;
	#endif

	#if UseGlobalLibraryLock & (! XMP_StaticBuild )
		InitializeBasicMutex ( sLibraryLock );	// ! Handled in XMPMeta for static builds.
	#endif
	
	SXMPMeta::Initialize();	// Just in case the client does not.
	
	if ( ! Initialize_LibUtils() ) return false;
	
	#if GatherPerformanceData
		sAPIPerf = new APIPerfCollection;
	#endif
	
	XMP_Uns16 endianInt  = 0x00FF;
	XMP_Uns8  endianByte = *((XMP_Uns8*)&endianInt);
	if ( kBigEndianHost ) {
		if ( endianByte != 0 ) XMP_Throw ( "Big endian flag mismatch", kXMPErr_InternalFailure );
	} else {
		if ( endianByte != 0xFF ) XMP_Throw ( "Little endian flag mismatch", kXMPErr_InternalFailure );
	}
	
	XMP_Assert ( kUTF8_PacketHeaderLen == strlen ( "<?xpacket begin='xxx' id='W5M0MpCehiHzreSzNTczkc9d'" ) );
	XMP_Assert ( kUTF8_PacketTrailerLen == strlen ( (const char *) kUTF8_PacketTrailer ) );
	
	sFolderHandlers = new XMPFileHandlerTable;
	sNormalHandlers = new XMPFileHandlerTable;
	sOwningHandlers = new XMPFileHandlerTable;

	InitializeUnicodeConversions();
	
	ignoreLocalText = XMP_OptionIsSet ( options, kXMPFiles_IgnoreLocalText );
	#if XMP_UNIXBuild
		if ( ! ignoreLocalText ) XMP_Throw ( "Generic UNIX clients must pass kXMPFiles_IgnoreLocalText", kXMPErr_EnforceFailure );
	#endif
	
	// -----------------------------------------
	// Register the directory-oriented handlers.

	RegisterFolderHandler ( kXMP_P2File, kP2_HandlerFlags, P2_CheckFormat, P2_MetaHandlerCTor );
	RegisterFolderHandler ( kXMP_SonyHDVFile, kSonyHDV_HandlerFlags, SonyHDV_CheckFormat, SonyHDV_MetaHandlerCTor );
	RegisterFolderHandler ( kXMP_XDCAM_FAMFile, kXDCAM_HandlerFlags, XDCAM_CheckFormat, XDCAM_MetaHandlerCTor );
	RegisterFolderHandler ( kXMP_XDCAM_SAMFile, kXDCAM_HandlerFlags, XDCAM_CheckFormat, XDCAM_MetaHandlerCTor );
	RegisterFolderHandler ( kXMP_XDCAM_EXFile, kXDCAMEX_HandlerFlags, XDCAMEX_CheckFormat, XDCAMEX_MetaHandlerCTor );
	RegisterFolderHandler ( kXMP_AVCHDFile, kAVCHD_HandlerFlags, AVCHD_CheckFormat, AVCHD_MetaHandlerCTor );

	// ------------------------------------------------------------------------------------------
	// Register the file-oriented handlers that don't want to open and close the file themselves.

	RegisterNormalHandler ( kXMP_JPEGFile, kJPEG_HandlerFlags, JPEG_CheckFormat, JPEG_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_TIFFFile, kTIFF_HandlerFlags, TIFF_CheckFormat, TIFF_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_PhotoshopFile, kPSD_HandlerFlags, PSD_CheckFormat, PSD_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_InDesignFile, kInDesign_HandlerFlags, InDesign_CheckFormat, InDesign_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_PNGFile, kPNG_HandlerFlags, PNG_CheckFormat, PNG_MetaHandlerCTor );

	// ! EPS and PostScript have the same handler, EPS is a proper subset of PostScript.
	RegisterNormalHandler ( kXMP_EPSFile, kPostScript_HandlerFlags, PostScript_CheckFormat, PostScript_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_PostScriptFile, kPostScript_HandlerFlags, PostScript_CheckFormat, PostScript_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_WMAVFile, kASF_HandlerFlags, ASF_CheckFormat, ASF_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_MP3File, kMP3_HandlerFlags, MP3_CheckFormat, MP3_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_WAVFile, kRIFF_HandlerFlags, RIFF_CheckFormat, RIFF_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_AVIFile, kRIFF_HandlerFlags, RIFF_CheckFormat, RIFF_MetaHandlerCTor );

	RegisterNormalHandler ( kXMP_SWFFile, kSWF_HandlerFlags, SWF_CheckFormat, SWF_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_UCFFile, kUCF_HandlerFlags, UCF_CheckFormat, UCF_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_MPEG4File, kMPEG4_HandlerFlags, MPEG4_CheckFormat, MPEG4_MetaHandlerCTor );
	RegisterNormalHandler ( kXMP_MOVFile, kMPEG4_HandlerFlags, MPEG4_CheckFormat, MPEG4_MetaHandlerCTor );	// ! Yes, MPEG-4 includes MOV.
	RegisterNormalHandler ( kXMP_FLVFile, kFLV_HandlerFlags, FLV_CheckFormat, FLV_MetaHandlerCTor );

	// ---------------------------------------------------------------------------------------
	// Register the file-oriented handlers that do want to open and close the file themselves.

	RegisterOwningHandler ( kXMP_MPEGFile, kMPEG2_HandlerFlags, MPEG2_CheckFormat, MPEG2_MetaHandlerCTor );
	RegisterOwningHandler ( kXMP_MPEG2File, kMPEG2_HandlerFlags, MPEG2_CheckFormat, MPEG2_MetaHandlerCTor );

	// Make sure the embedded info strings are referenced and kept.
	if ( (kXMPFiles_EmbeddedVersion[0] == 0) || (kXMPFiles_EmbeddedCopyright[0] == 0) ) return false;
	// Verify critical type sizes.
	XMP_Assert ( sizeof(XMP_Int8) == 1 );
	XMP_Assert ( sizeof(XMP_Int16) == 2 );
	XMP_Assert ( sizeof(XMP_Int32) == 4 );
	XMP_Assert ( sizeof(XMP_Int64) == 8 );
	XMP_Assert ( sizeof(XMP_Uns8) == 1 );
	XMP_Assert ( sizeof(XMP_Uns16) == 2 );
	XMP_Assert ( sizeof(XMP_Uns32) == 4 );
	XMP_Assert ( sizeof(XMP_Uns64) == 8 );

	return true;

}	// XMPFiles::Initialize
 
// =================================================================================================

#if GatherPerformanceData

#if XMP_WinBuild
	#pragma warning ( disable : 4996 )	// '...' was declared deprecated
#endif

#include "PerfUtils.cpp"

static void ReportPerformanceData()
{
	struct SummaryInfo {
		size_t callCount;
		double totalTime;
		SummaryInfo() : callCount(0), totalTime(0.0) {};
	};
	
	SummaryInfo perfSummary [kAPIPerfProcCount];
	
	XMP_DateTime now;
	SXMPUtils::CurrentDateTime ( &now );
	std::string nowStr;
	SXMPUtils::ConvertFromDate ( now, &nowStr );
	
	#if XMP_WinBuild
		#define kPerfLogPath "C:\\XMPFilesPerformanceLog.txt"
	#else
		#define kPerfLogPath "/XMPFilesPerformanceLog.txt"
	#endif
	FILE * perfLog = fopen ( kPerfLogPath, "ab" );
	if ( perfLog == 0 ) return;
	
	fprintf ( perfLog, "\n\n// =================================================================================================\n\n" );
	fprintf ( perfLog, "XMPFiles performance data\n" );
	fprintf ( perfLog, "   %s\n", kXMPFiles_VersionMessage );
	fprintf ( perfLog, "   Reported at %s\n", nowStr.c_str() );
	fprintf ( perfLog, "   %s\n", PerfUtils::GetTimerInfo() );
	
	// Gather and report the summary info.
	
	for ( size_t i = 0; i < sAPIPerf->size(); ++i ) {
		SummaryInfo& summaryItem = perfSummary [(*sAPIPerf)[i].whichProc];
		++summaryItem.callCount;
		summaryItem.totalTime += (*sAPIPerf)[i].elapsedTime;
	}
	
	fprintf ( perfLog, "\nSummary data:\n" );
	
	for ( size_t i = 0; i < kAPIPerfProcCount; ++i ) {
		long averageTime = 0;
		if ( perfSummary[i].callCount != 0 ) {
			averageTime = (long) (((perfSummary[i].totalTime/perfSummary[i].callCount) * 1000.0*1000.0) + 0.5);
		}
		fprintf ( perfLog, "   %s, %d total calls, %d average microseconds per call\n",
				  kAPIPerfNames[i], perfSummary[i].callCount, averageTime );
	}

	fprintf ( perfLog, "\nPer-call data:\n" );
	
	// Report the info for each call.
	
	for ( size_t i = 0; i < sAPIPerf->size(); ++i ) {
		long time = (long) (((*sAPIPerf)[i].elapsedTime * 1000.0*1000.0) + 0.5);
		fprintf ( perfLog, "   %s, %d microSec, ref %.8X, \"%s\"\n",
				  kAPIPerfNames[(*sAPIPerf)[i].whichProc], time,
				  (*sAPIPerf)[i].xmpFilesRef, (*sAPIPerf)[i].extraInfo.c_str() );
	}
	
	fclose ( perfLog );
	
}	// ReportAReportPerformanceDataPIPerformance

#endif

// =================================================================================================

/* class static */
void
XMPFiles::Terminate()
{
	--sXMPFilesInitCount;
	if ( sXMPFilesInitCount != 0 ) return;	// Not ready to terminate, or already terminated.

	#if GatherPerformanceData
		ReportPerformanceData();
		EliminateGlobal ( sAPIPerf );
	#endif
	
	EliminateGlobal ( sFolderHandlers );
	EliminateGlobal ( sNormalHandlers );
	EliminateGlobal ( sOwningHandlers );
	
	SXMPMeta::Terminate();	// Just in case the client does not.

	Terminate_LibUtils();

	#if UseGlobalLibraryLock & (! XMP_StaticBuild )
		TerminateBasicMutex ( sLibraryLock );	// ! Handled in XMPMeta for static builds.
	#endif

	#if XMP_TraceFilesCallsToFile
		if ( xmpFilesLog != stderr ) fclose ( xmpFilesLog );
		xmpFilesLog = stderr;
	#endif

}	// XMPFiles::Terminate

// =================================================================================================

XMPFiles::XMPFiles() :
	clientRefs(0),
	format(kXMP_UnknownFile),
	fileRef(0),
	openFlags(0),
	abortProc(0),
	abortArg(0),
	handler(0),
	tempPtr(0),
	tempUI32(0)
{
	// Nothing more to do, clientRefs is incremented in wrapper.

}	// XMPFiles::XMPFiles
 
// =================================================================================================

XMPFiles::~XMPFiles()
{
	XMP_Assert ( this->clientRefs <= 0 );

	if ( this->handler != 0 ) {
		delete this->handler;
		this->handler = 0;
	}

	if ( this->fileRef != 0 ) {
		LFA_Close ( this->fileRef );
		this->fileRef = 0;
	}
	
	if ( this->tempPtr != 0 ) free ( this->tempPtr );	// ! Must have been malloc-ed!

}	// XMPFiles::~XMPFiles
 
// =================================================================================================

/* class static */
bool
XMPFiles::GetFormatInfo ( XMP_FileFormat   format,
                          XMP_OptionBits * flags /* = 0 */ )
{
	if ( flags == 0 ) flags = &voidOptionBits;
	
	XMPFileHandlerTablePos handlerPos;
	
	handlerPos = sFolderHandlers->find ( format );
	if ( handlerPos != sFolderHandlers->end() ) {
		*flags = handlerPos->second.flags;
		return true;
	}
	
	handlerPos = sNormalHandlers->find ( format );
	if ( handlerPos != sNormalHandlers->end() ) {
		*flags = handlerPos->second.flags;
		return true;
	}
	
	handlerPos = sOwningHandlers->find ( format );
	if ( handlerPos != sOwningHandlers->end() ) {
		*flags = handlerPos->second.flags;
		return true;
	}
	
	return false;
	
}	// XMPFiles::GetFormatInfo
 
// =================================================================================================

/* class static */
XMP_FileFormat
XMPFiles::CheckFileFormat ( XMP_StringPtr filePath )
{
	if ( (filePath == 0) || (*filePath == 0) ) return kXMP_UnknownFile;

	XMPFiles bogus;
	XMPFileHandlerInfo * handlerInfo = SelectSmartHandler ( &bogus, filePath, kXMP_UnknownFile, kXMPFiles_OpenForRead );
	if ( handlerInfo == 0 ) return kXMP_UnknownFile;
	return handlerInfo->format;
	
}	// XMPFiles::CheckFileFormat
 
// =================================================================================================

/* class static */
XMP_FileFormat
XMPFiles::CheckPackageFormat ( XMP_StringPtr folderPath )
{
	// This is called with a path to a folder, and checks to see if that folder is the top level of
	// a "package" that should be recognized by one of the folder-oriented handlers. The checks here
	// are not overly extensive, but hopefully enough to weed out false positives.
	//
	// Since we don't have many folder handlers, this is simple hardwired code.
	
	FileMode folderMode = GetFileMode ( folderPath );
	if ( folderMode != kFMode_IsFolder ) return kXMP_UnknownFile;

	return CheckTopFolderName ( std::string ( folderPath ) );

}	// XMPFiles::CheckPackageFormat
 
// =================================================================================================

bool
XMPFiles::OpenFile ( XMP_StringPtr  clientPath,
	                 XMP_FileFormat format /* = kXMP_UnknownFile */,
	                 XMP_OptionBits openFlags /* = 0 */ )
{
	if ( this->handler != 0 ) XMP_Throw ( "File already open", kXMPErr_BadParam );
	if ( this->fileRef != 0 ) {	// ! Sanity check to prevent open file leaks.
		LFA_Close ( this->fileRef );
		this->fileRef = 0;
	}
	
	this->format = kXMP_UnknownFile;	// Make sure it is preset for later check.
	this->openFlags = openFlags;
	
	char openMode = 'r';
	if ( openFlags & kXMPFiles_OpenForUpdate ) openMode = 'w';
	
	FileMode clientMode = GetFileMode ( clientPath );
	if ( (clientMode == kFMode_IsFolder) || (clientMode == kFMode_IsOther) ) return false;
	XMP_Assert ( (clientMode == kFMode_IsFile) || (clientMode == kFMode_DoesNotExist) );
	
	std::string fileExt;	// Used to check for camera raw files and OK to scan files.
	
	if ( clientMode == kFMode_IsFile ) {

		// Find the file extension. OK to be "wrong" for something like "C:\My.dir\file". Any
		// filtering looks for matches with real extensions, "dir\file" won't match any of these.
		XMP_StringPtr extPos = clientPath + strlen ( clientPath );
		for ( ; (extPos != clientPath) && (*extPos != '.'); --extPos ) {}
		if ( *extPos == '.' ) {
			fileExt.assign ( extPos+1 );
			MakeLowerCase ( &fileExt );
		}
		
		// See if this file is one that XMPFiles should never process.
		for ( size_t i = 0; kKnownRejectedFiles[i] != 0; ++i ) {
			if ( fileExt == kKnownRejectedFiles[i] ) return false;
		}

	}
	
	// Find the handler, fill in the XMPFiles member variables, cache the desired file data.

	XMPFileHandlerInfo * handlerInfo  = 0;
	XMPFileHandlerCTor   handlerCTor  = 0;
	XMP_OptionBits       handlerFlags = 0;
	
	if ( ! (openFlags & kXMPFiles_OpenUsePacketScanning) ) {
		handlerInfo = SelectSmartHandler ( this, clientPath, format, openFlags );
	}

	if ( handlerInfo == 0 ) {

		// No smart handler, packet scan if appropriate.

		if ( clientMode != kFMode_IsFile ) return false;
		if ( openFlags & kXMPFiles_OpenUseSmartHandler ) return false;

		if ( openFlags & kXMPFiles_OpenLimitedScanning ) {
			bool scanningOK = false;
			for ( size_t i = 0; kKnownScannedFiles[i] != 0; ++i ) {
				if ( fileExt == kKnownScannedFiles[i] ) { scanningOK = true; break; }
			}
			if ( ! scanningOK ) return false;
		}

		handlerInfo = &kScannerHandlerInfo;
		if ( fileRef == 0 ) fileRef = LFA_Open ( clientPath, openMode );

	}

	XMP_Assert ( handlerInfo != 0 );
	handlerCTor  = handlerInfo->handlerCTor;
	handlerFlags = handlerInfo->flags;
	
	this->filePath = clientPath;
	
	XMPFileHandler* handler = (*handlerCTor) ( this );
	XMP_Assert ( handlerFlags == handler->handlerFlags );
	
	this->handler = handler;
	if ( this->format == kXMP_UnknownFile ) this->format = handlerInfo->format;	// ! The CheckProc might have set it.

	try {
		handler->CacheFileData();
	} catch ( ... ) {
		delete this->handler;
		this->handler = 0;
		if ( ! (handlerFlags & kXMPFiles_HandlerOwnsFile) ) {
			LFA_Close ( this->fileRef );
			this->fileRef = 0;
		}
		throw;
	}
	
	if ( handler->containsXMP ) FillPacketInfo ( handler->xmpPacket, &handler->packetInfo );

	if ( (! (openFlags & kXMPFiles_OpenForUpdate)) && (! (handlerFlags & kXMPFiles_HandlerOwnsFile)) ) {
		// Close the disk file now if opened for read-only access.
		LFA_Close ( this->fileRef );
		this->fileRef = 0;
	}
	
	return true;

}	// XMPFiles::OpenFile
 
// =================================================================================================

void
XMPFiles::CloseFile ( XMP_OptionBits closeFlags /* = 0 */ )
{
	if ( this->handler == 0 ) return;	// Return if there is no open file (not an error).

	bool needsUpdate = this->handler->needsUpdate;
	XMP_OptionBits handlerFlags = this->handler->handlerFlags;
	
	// Decide if we're doing a safe update. If so, make sure the handler supports it. All handlers
	// that don't own the file tolerate safe update using common code below.
	
	bool doSafeUpdate = XMP_OptionIsSet ( closeFlags, kXMPFiles_UpdateSafely );
	#if GatherPerformanceData
		if ( doSafeUpdate ) sAPIPerf->back().extraInfo += ", safe update";	// Client wants safe update.
	#endif

	if ( ! (this->openFlags & kXMPFiles_OpenForUpdate) ) doSafeUpdate = false;
	if ( ! needsUpdate ) doSafeUpdate = false;
	
	bool safeUpdateOK = ( (handlerFlags & kXMPFiles_AllowsSafeUpdate) ||
						  (! (handlerFlags & kXMPFiles_HandlerOwnsFile)) );
	if ( doSafeUpdate && (! safeUpdateOK) ) {
		XMP_Throw ( "XMPFiles::CloseFile - Safe update not supported", kXMPErr_Unavailable );
	}

	// Try really hard to make sure the file is closed and the handler is deleted.

	LFA_FileRef origFileRef = this->fileRef;	// Used during crash-safe saves.
	std::string origFilePath ( this->filePath );

	LFA_FileRef tempFileRef = 0;
	std::string tempFilePath;

	LFA_FileRef copyFileRef = 0;
	std::string copyFilePath;

	try {
	
		if ( (! doSafeUpdate) || (handlerFlags & kXMPFiles_HandlerOwnsFile) ) {	// ! Includes no update case.
		
			// Close the file without doing common crash-safe writing. The handler might do it.

			if ( needsUpdate ) {
				#if GatherPerformanceData
					sAPIPerf->back().extraInfo += ", direct update";
				#endif
				this->handler->UpdateFile ( doSafeUpdate );
			}
			
			delete this->handler;
			this->handler = 0;
			if ( this->fileRef != 0 ) LFA_Close ( this->fileRef );
			this->fileRef = 0;

		} else {
		
			// Update the file in a crash-safe manner. This somewhat convoluted approach preserves
			// the ownership, permissions, and Mac resources of the final file - at a slightly
			// higher risk of confusion in the event of a midstream crash.
						
			if ( handlerFlags & kXMPFiles_CanRewrite ) {

				// The handler can rewrite an entire file based on the original. Do this into a temp
				// file next to the original, with the same ownership and permissions if possible.

				#if GatherPerformanceData
					sAPIPerf->back().extraInfo += ", file rewrite";
				#endif
				
				CreateTempFile ( origFilePath, &tempFilePath, kCopyMacRsrc );
				XMP_Assert ( tempFileRef == 0 );
				tempFileRef = LFA_Open ( tempFilePath.c_str(), 'w' );
				this->fileRef = tempFileRef;
				tempFileRef = 0;
				this->filePath = tempFilePath;
				this->handler->WriteFile ( origFileRef, origFilePath );

			} else {

				// The handler can only update an existing file. Do a little dance so that the final
				// file is the updated original, thus preserving ownership, permissions, etc. This
				// does have the risk that the interim copy under the original name has "current"
				// ownership and permissions. The dance steps:
				//  - Copy the original file to a temp name, the copyFile.
				//  - Rename the original file to a different temp name, the tempFile.
				//  - Rename the copyFile back to the original name.
				//  - Call the handler's UpdateFile method for the tempFile.
				// A failure inside the handler's UpdateFile method will leave the copied file under
				// the original name.
				
				// *** A user abort might leave the copy file under the original name! Need better
				// *** duplicate code that handles all parts of a file, and for CreateTempFile to
				// *** preserve ownership and permissions. Then the original can stay put until
				// *** the final delete/rename.

				#if GatherPerformanceData
					sAPIPerf->back().extraInfo += ", copy update";
				#endif
				
				CreateTempFile ( origFilePath, &copyFilePath, kCopyMacRsrc );
				XMP_Assert ( copyFileRef == 0 );
				copyFileRef = LFA_Open ( copyFilePath.c_str(), 'w' );
				XMP_Int64 fileSize = LFA_Measure ( origFileRef );
				LFA_Seek ( origFileRef, 0, SEEK_SET );
				LFA_Copy ( origFileRef, copyFileRef, fileSize, this->abortProc, this->abortArg );

				LFA_Close ( origFileRef );
				LFA_Close ( copyFileRef );
				copyFileRef = origFileRef = this->fileRef = 0;

				CreateTempFile ( origFilePath, &tempFilePath );
				LFA_Delete ( tempFilePath.c_str() );	// ! Slight risk of name being grabbed before rename.
				LFA_Rename ( origFilePath.c_str(), tempFilePath.c_str() );

				LFA_Rename ( copyFilePath.c_str(), origFilePath.c_str() );
				copyFilePath.clear();

				XMP_Assert ( tempFileRef == 0 );
				tempFileRef = LFA_Open ( tempFilePath.c_str(), 'w' );
				this->fileRef = tempFileRef;
				tempFileRef = 0;
				this->filePath = tempFilePath;

				try {
					this->handler->UpdateFile ( false );	// We're doing the safe update, not the handler.
				} catch ( ... ) {
					this->fileRef = 0;
					this->filePath = origFilePath;	// This is really the copied file.
					LFA_Close ( tempFileRef );
					LFA_Delete ( tempFilePath.c_str() );
					tempFileRef = 0;
					tempFilePath.clear();
					throw;
				}

			}

			delete this->handler;
			this->handler = 0;

			if ( this->fileRef != 0 ) LFA_Close ( this->fileRef );
			if ( origFileRef != 0 ) LFA_Close ( origFileRef );

			this->fileRef = 0;
			origFileRef = 0;
			tempFileRef = 0;
			
			LFA_Delete ( origFilePath.c_str() );
			LFA_Rename ( tempFilePath.c_str(), origFilePath.c_str() );
			tempFilePath.clear();

		}

	} catch ( ... ) {
	
		// *** Don't delete the temp or copy files, not sure which is best.

		try {
			if ( this->fileRef != 0 ) LFA_Close ( this->fileRef );
		} catch ( ... ) { /* Do nothing, throw the outer exception later. */ }
		try {
			if ( origFileRef != 0 ) LFA_Close ( origFileRef );
		} catch ( ... ) { /* Do nothing, throw the outer exception later. */ }
		try {
			if ( tempFileRef != 0 ) LFA_Close ( tempFileRef );
		} catch ( ... ) { /* Do nothing, throw the outer exception later. */ }
		try {
			if ( ! tempFilePath.empty() ) LFA_Delete ( tempFilePath.c_str() );
		} catch ( ... ) { /* Do nothing, throw the outer exception later. */ }
		try {
			if ( copyFileRef != 0 ) LFA_Close ( copyFileRef );
		} catch ( ... ) { /* Do nothing, throw the outer exception later. */ }
		try {
			if ( ! copyFilePath.empty() ) LFA_Delete ( copyFilePath.c_str() );
		} catch ( ... ) { /* Do nothing, throw the outer exception later. */ }
		try {
			if ( this->handler != 0 ) delete this->handler;
		} catch ( ... ) { /* Do nothing, throw the outer exception later. */ }
	
		this->handler   = 0;
		this->format    = kXMP_UnknownFile;
		this->fileRef   = 0;
		this->filePath.clear();
		this->openFlags = 0;
	
		if ( this->tempPtr != 0 ) free ( this->tempPtr );	// ! Must have been malloc-ed!
		this->tempPtr  = 0;
		this->tempUI32 = 0;

		throw;
	
	}
	
	// Clear the XMPFiles member variables.
	
	this->handler   = 0;
	this->format    = kXMP_UnknownFile;
	this->fileRef   = 0;
	this->filePath.clear();
	this->openFlags = 0;
	
	if ( this->tempPtr != 0 ) free ( this->tempPtr );	// ! Must have been malloc-ed!
	this->tempPtr  = 0;
	this->tempUI32 = 0;
	
}	// XMPFiles::CloseFile
 
// =================================================================================================

bool
XMPFiles::GetFileInfo ( XMP_StringPtr *  filePath /* = 0 */,
                        XMP_StringLen *  pathLen /* = 0 */,
	                    XMP_OptionBits * openFlags /* = 0 */,
	                    XMP_FileFormat * format /* = 0 */,
	                    XMP_OptionBits * handlerFlags /* = 0 */ ) const
{
	if ( this->handler == 0 ) return false;
	XMPFileHandler * handler = this->handler;
	
	if ( filePath == 0 ) filePath = &voidStringPtr;
	if ( pathLen == 0 ) pathLen = &voidStringLen;
	if ( openFlags == 0 ) openFlags = &voidOptionBits;
	if ( format == 0 ) format = &voidFileFormat;
	if ( handlerFlags == 0 ) handlerFlags = &voidOptionBits;

	*filePath     = this->filePath.c_str();
	*pathLen      = (XMP_StringLen) this->filePath.size();
	*openFlags    = this->openFlags;
	*format       = this->format;
	*handlerFlags = this->handler->handlerFlags;
	
	return true;
	
}	// XMPFiles::GetFileInfo
 
// =================================================================================================

void
XMPFiles::SetAbortProc ( XMP_AbortProc abortProc,
						 void *        abortArg )
{
	this->abortProc = abortProc;
	this->abortArg  = abortArg;
	
	XMP_Assert ( (abortProc != (XMP_AbortProc)0) || (abortArg != (void*)(unsigned long long)0xDEADBEEFULL) );	// Hack to test the assert callback.
}	// XMPFiles::SetAbortProc
 
// =================================================================================================
// SetClientPacketInfo
// ===================
//
// Set the packet info returned to the client. This is the internal packet info at first, which
// tells what is in the file. But once the file needs update (PutXMP has been called), we return
// info about the latest XMP. The internal packet info is left unchanged since it is needed when
// the file is updated to locate the old packet in the file.

static void
SetClientPacketInfo ( XMP_PacketInfo * clientInfo, const XMP_PacketInfo & handlerInfo,
					  const std::string & xmpPacket, bool needsUpdate )
{

	if ( clientInfo == 0 ) return;
	
	if ( ! needsUpdate ) {
		*clientInfo = handlerInfo;
	} else {
		clientInfo->offset = kXMPFiles_UnknownOffset;
		clientInfo->length = (XMP_Int32) xmpPacket.size();
		FillPacketInfo ( xmpPacket, clientInfo );
	}
	
}	// SetClientPacketInfo

// =================================================================================================

bool
XMPFiles::GetXMP ( SXMPMeta *       xmpObj /* = 0 */,
                   XMP_StringPtr *  xmpPacket /* = 0 */,
                   XMP_StringLen *  xmpPacketLen /* = 0 */,
                   XMP_PacketInfo * packetInfo /* = 0 */ )
{
	if ( this->handler == 0 ) XMP_Throw ( "XMPFiles::GetXMP - No open file", kXMPErr_BadObject );
	
	XMP_OptionBits applyTemplateFlags = kXMPTemplate_AddNewProperties | kXMPTemplate_IncludeInternalProperties;

	if ( ! this->handler->processedXMP ) {
		try {
			this->handler->ProcessXMP();
		} catch ( ... ) {
			// Return the outputs then rethrow the exception.
			if ( xmpObj != 0 ) {
				// ! Don't use Clone, that replaces the internal ref in the local xmpObj, leaving the client unchanged!
				xmpObj->Erase();
				SXMPUtils::ApplyTemplate ( xmpObj, this->handler->xmpObj, applyTemplateFlags );
			}
			if ( xmpPacket != 0 ) *xmpPacket = this->handler->xmpPacket.c_str();
			if ( xmpPacketLen != 0 ) *xmpPacketLen = (XMP_StringLen) this->handler->xmpPacket.size();
			SetClientPacketInfo ( packetInfo, this->handler->packetInfo,
								  this->handler->xmpPacket, this->handler->needsUpdate );
			throw;
		}
	}

	if ( ! this->handler->containsXMP ) return false;

	#if 0	// *** See bug 1131815. A better way might be to pass the ref up from here.
		if ( xmpObj != 0 ) *xmpObj = this->handler->xmpObj.Clone();
	#else
		if ( xmpObj != 0 ) {
			// ! Don't use Clone, that replaces the internal ref in the local xmpObj, leaving the client unchanged!
			xmpObj->Erase();
			SXMPUtils::ApplyTemplate ( xmpObj, this->handler->xmpObj, applyTemplateFlags );
		}
	#endif

	if ( xmpPacket != 0 ) *xmpPacket = this->handler->xmpPacket.c_str();
	if ( xmpPacketLen != 0 ) *xmpPacketLen = (XMP_StringLen) this->handler->xmpPacket.size();
	SetClientPacketInfo ( packetInfo, this->handler->packetInfo,
						  this->handler->xmpPacket, this->handler->needsUpdate );

	return true;

}	// XMPFiles::GetXMP
 
// =================================================================================================

static bool
DoPutXMP ( XMPFiles * thiz, const SXMPMeta & xmpObj, const bool doIt )
{
	// Check some basic conditions to see if the Put should be attempted.
	
	if ( thiz->handler == 0 ) XMP_Throw ( "XMPFiles::PutXMP - No open file", kXMPErr_BadObject );
	if ( ! (thiz->openFlags & kXMPFiles_OpenForUpdate) ) {
		XMP_Throw ( "XMPFiles::PutXMP - Not open for update", kXMPErr_BadObject );
	}

	XMPFileHandler * handler      = thiz->handler;
	XMP_OptionBits   handlerFlags = handler->handlerFlags;
	XMP_PacketInfo & packetInfo   = handler->packetInfo;
	std::string &    xmpPacket    = handler->xmpPacket;
	
	if ( ! handler->processedXMP ) handler->ProcessXMP();	// Might have Open/Put with no GetXMP.
	
	size_t oldPacketOffset = (size_t)packetInfo.offset;
	size_t oldPacketLength = packetInfo.length;
	
	if ( oldPacketOffset == (size_t)kXMPFiles_UnknownOffset ) oldPacketOffset = 0;	// ! Simplify checks.
	if ( oldPacketLength == (size_t)kXMPFiles_UnknownLength ) oldPacketLength = 0;
	
	bool fileHasPacket = (oldPacketOffset != 0) && (oldPacketLength != 0);

	if ( ! fileHasPacket ) {
		if ( ! (handlerFlags & kXMPFiles_CanInjectXMP) ) {
			XMP_Throw ( "XMPFiles::PutXMP - Can't inject XMP", kXMPErr_Unavailable );
		}
		if ( handler->stdCharForm == kXMP_CharUnknown ) {
			XMP_Throw ( "XMPFiles::PutXMP - No standard character form", kXMPErr_InternalFailure );
		}
	}
	
	// Serialize the XMP and update the handler's info.
	
	XMP_Uns8 charForm = handler->stdCharForm;
	if ( charForm == kXMP_CharUnknown ) charForm = packetInfo.charForm;

	XMP_OptionBits options = handler->GetSerializeOptions() | XMP_CharToSerializeForm ( charForm );
	if ( handlerFlags & kXMPFiles_NeedsReadOnlyPacket ) options |= kXMP_ReadOnlyPacket;
	if ( fileHasPacket && (thiz->format == kXMP_UnknownFile) && (! packetInfo.writeable) ) options |= kXMP_ReadOnlyPacket;
	
	bool preferInPlace = ((handlerFlags & kXMPFiles_PrefersInPlace) != 0);
	bool tryInPlace    = (fileHasPacket & preferInPlace) || (! (handlerFlags & kXMPFiles_CanExpand));
	
	if ( handlerFlags & kXMPFiles_UsesSidecarXMP ) tryInPlace = false;
		
	if ( tryInPlace ) {
		try {
			xmpObj.SerializeToBuffer ( &xmpPacket, (options | kXMP_ExactPacketLength), (XMP_StringLen) oldPacketLength );
			XMP_Assert ( xmpPacket.size() == oldPacketLength );
		} catch ( ... ) {
			if ( preferInPlace ) {
				tryInPlace = false;	// ! Try again, out of place this time.
			} else {
				if ( ! doIt ) return false;
				throw;
			}
		}
	}
	
	if ( ! tryInPlace ) {
		try {
			xmpObj.SerializeToBuffer ( &xmpPacket, options );
		} catch ( ... ) {
			if ( ! doIt ) return false;
			throw;
		}
	}

	if ( doIt ) {
		handler->xmpObj = xmpObj.Clone();
		handler->containsXMP = true;
		handler->processedXMP = true;
		handler->needsUpdate = true;
	}
	
	return true;
	
}	// DoPutXMP
 
// =================================================================================================

void
XMPFiles::PutXMP ( const SXMPMeta & xmpObj )
{
	(void) DoPutXMP ( this, xmpObj, true );
	
}	// XMPFiles::PutXMP
 
// =================================================================================================

void
XMPFiles::PutXMP ( XMP_StringPtr xmpPacket,
                   XMP_StringLen xmpPacketLen /* = kXMP_UseNullTermination */ )
{
	SXMPMeta xmpObj ( xmpPacket, xmpPacketLen );
	this->PutXMP ( xmpObj );
	
}	// XMPFiles::PutXMP
 
// =================================================================================================

bool
XMPFiles::CanPutXMP ( const SXMPMeta & xmpObj )
{
	if ( this->handler == 0 ) XMP_Throw ( "XMPFiles::CanPutXMP - No open file", kXMPErr_BadObject );

	if ( ! (this->openFlags & kXMPFiles_OpenForUpdate) ) return false;

	if ( this->handler->handlerFlags & kXMPFiles_CanInjectXMP ) return true;
	if ( ! this->handler->containsXMP ) return false;
	if ( this->handler->handlerFlags & kXMPFiles_CanExpand ) return true;

	return DoPutXMP ( this, xmpObj, false );

}	// XMPFiles::CanPutXMP
 
// =================================================================================================

bool
XMPFiles::CanPutXMP ( XMP_StringPtr xmpPacket,
                      XMP_StringLen xmpPacketLen /* = kXMP_UseNullTermination */ )
{
	SXMPMeta xmpObj ( xmpPacket, xmpPacketLen );
	return this->CanPutXMP ( xmpObj );
	
}	// XMPFiles::CanPutXMP
 
// =================================================================================================
