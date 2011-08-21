// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2004 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "XMPFiles_Impl.hpp"

#include "UnicodeConversions.hpp"

using namespace std;

// Internal code should be using #if with XMP_MacBuild, XMP_WinBuild, or XMP_UNIXBuild.
// This is a sanity check in case of accidental use of *_ENV. Some clients use the poor
// practice of defining the *_ENV macro with an empty value.
#if defined ( MAC_ENV )
	#if ! MAC_ENV
		#error "MAC_ENV must be defined so that \"#if MAC_ENV\" is true"
	#endif
#elif defined ( WIN_ENV )
	#if ! WIN_ENV
		#error "WIN_ENV must be defined so that \"#if WIN_ENV\" is true"
	#endif
#elif defined ( UNIX_ENV )
	#if ! UNIX_ENV
		#error "UNIX_ENV must be defined so that \"#if UNIX_ENV\" is true"
	#endif
#endif

// =================================================================================================
/// \file XMPFiles_Impl.cpp
/// \brief ...
///
/// This file ...
///
// =================================================================================================

#if XMP_WinBuild
	#pragma warning ( disable : 4290 )	// C++ exception specification ignored except to indicate a function is not __declspec(nothrow)
	#pragma warning ( disable : 4800 )	// forcing value to bool 'true' or 'false' (performance warning)
#endif

bool ignoreLocalText = false;

XMP_FileFormat voidFileFormat = 0;	// Used as sink for unwanted output parameters.
// =================================================================================================

// Add all known mappings, multiple mappings (tif, tiff) are OK.
const FileExtMapping kFileExtMap[] =
	{ { "pdf",  kXMP_PDFFile },
	  { "ps",   kXMP_PostScriptFile },
	  { "eps",  kXMP_EPSFile },

	  { "jpg",  kXMP_JPEGFile },
	  { "jpeg", kXMP_JPEGFile },
	  { "jpx",  kXMP_JPEG2KFile },
	  { "tif",  kXMP_TIFFFile },
	  { "tiff", kXMP_TIFFFile },
	  { "dng",  kXMP_TIFFFile },	// DNG files are well behaved TIFF.
	  { "gif",  kXMP_GIFFile },
	  { "giff", kXMP_GIFFile },
	  { "png",  kXMP_PNGFile },

	  { "swf",  kXMP_SWFFile },
	  { "flv",  kXMP_FLVFile },

	  { "aif",  kXMP_AIFFFile },

	  { "mov",  kXMP_MOVFile },
	  { "avi",  kXMP_AVIFile },
	  { "cin",  kXMP_CINFile },
	  { "wav",  kXMP_WAVFile },
	  { "mp3",  kXMP_MP3File },
	  { "mp4",  kXMP_MPEG4File },
	  { "m4v",  kXMP_MPEG4File },
	  { "m4a",  kXMP_MPEG4File },
	  { "f4v",  kXMP_MPEG4File },
	  { "ses",  kXMP_SESFile },
	  { "cel",  kXMP_CELFile },
	  { "wma",  kXMP_WMAVFile },
	  { "wmv",  kXMP_WMAVFile },

	  { "mpg",  kXMP_MPEGFile },
	  { "mpeg", kXMP_MPEGFile },
	  { "mp2",  kXMP_MPEGFile },
	  { "mod",  kXMP_MPEGFile },
	  { "m2v",  kXMP_MPEGFile },
	  { "mpa",  kXMP_MPEGFile },
	  { "mpv",  kXMP_MPEGFile },
	  { "m2p",  kXMP_MPEGFile },
	  { "m2a",  kXMP_MPEGFile },
	  { "m2t",  kXMP_MPEGFile },
	  { "mpe",  kXMP_MPEGFile },
	  { "vob",  kXMP_MPEGFile },
	  { "ms-pvr", kXMP_MPEGFile },
	  { "dvr-ms", kXMP_MPEGFile },

	  { "html", kXMP_HTMLFile },
	  { "xml",  kXMP_XMLFile },
	  { "txt",  kXMP_TextFile },
	  { "text", kXMP_TextFile },

	  { "psd",  kXMP_PhotoshopFile },
	  { "ai",   kXMP_IllustratorFile },
	  { "indd", kXMP_InDesignFile },
	  { "indt", kXMP_InDesignFile },
	  { "aep",  kXMP_AEProjectFile },
	  { "aepx", kXMP_AEProjectFile },
	  { "aet",  kXMP_AEProjTemplateFile },
	  { "ffx",  kXMP_AEFilterPresetFile },
	  { "ncor", kXMP_EncoreProjectFile },
	  { "prproj", kXMP_PremiereProjectFile },
	  { "prtl", kXMP_PremiereTitleFile },
	  { "ucf", kXMP_UCFFile },
	  { "xfl", kXMP_UCFFile },
	  { "pdfxml", kXMP_UCFFile },
	  { "mars", kXMP_UCFFile },
	  { "idml", kXMP_UCFFile },
	  { "idap", kXMP_UCFFile },
	  { "icap", kXMP_UCFFile },
	  { "", 0 } };	// ! Must be last as a sentinel.

// Files known to contain XMP but have no smart handling, here or elsewhere.
const char * kKnownScannedFiles[] =
	{ "gif",	// GIF, public format but no smart handler.
	  "ai",		// Illustrator, actually a PDF file.
	  "ait",	// Illustrator template, actually a PDF file.
	  "svg",	// SVG, an XML file.
	  "aet",	// After Effects template project file.
	  "ffx",	// After Effects filter preset file.
	  "aep",	// After Effects project file in proprietary format
	  "aepx",	// After Effects project file in XML format
	  "inx",	// InDesign interchange, an XML file.
	  "inds",	// InDesign snippet, an XML file.
	  "inpk",	// InDesign package for GoLive, a text file (not XML).
	  "incd",	// InCopy story, an XML file.
	  "inct",	// InCopy template, an XML file.
	  "incx",	// InCopy interchange, an XML file.
	  "fm",		// FrameMaker file, proprietary format.
	  "book",	// FrameMaker book, proprietary format.
	  "icml",	// an inCopy (inDesign) format 
	  "icmt",	// an inCopy (inDesign) format 
	  "idms",	// an inCopy (inDesign) format 
	  0 };		// ! Keep a 0 sentinel at the end.


// Extensions that XMPFiles never handles.
const char * kKnownRejectedFiles[] =
	{ 
		// RAW files
		"cr2", "erf", "fff", "dcr", "kdc", "mos", "mfw", "mef",
		"raw", "nef", "orf", "pef", "arw", "sr2", "srf", "sti",
		"3fr", "rwl", "crw", "sraw", "mos", "mrw", "nrw", "rw2",
		"c3f",
		// UCF subformats
		"air",
		// Others
		"r3d",
	  0 };	// ! Keep a 0 sentinel at the end.

// =================================================================================================

// =================================================================================================

void LFA_Throw ( const char* msg, int id )
{
	switch ( id ) {
		case kLFAErr_InternalFailure:
			XMP_Throw ( msg, kXMPErr_InternalFailure );
		case kLFAErr_ExternalFailure:
			XMP_Throw ( msg, kXMPErr_ExternalFailure );
		case kLFAErr_UserAbort:
			XMP_Throw ( msg, kXMPErr_UserAbort );
		default:
			XMP_Throw ( msg, kXMPErr_UnknownException );
	}
}

// =================================================================================================

#if XMP_MacBuild | XMP_UNIXBuild
	//copy from LargeFileAccess.cpp
	static bool FileExists ( const char * filePath )
	{	
		struct stat info;
		int err = stat ( filePath, &info );
		return (err == 0);
	}
#endif

// =================================================================================================

static bool CreateNewFile ( const char * newPath, const char * origPath, size_t filePos, bool copyMacRsrc )
{
	// Try to create a new file with the same ownership and permissions as some other file.
	// *** The ownership and permissions are not handled on all platforms.

	#if XMP_MacBuild | XMP_UNIXBuild

		if ( FileExists ( newPath ) ) return false;

	#elif XMP_WinBuild

		{
			std::string wideName;
			const size_t utf8Len = strlen(newPath);
			const size_t maxLen = 2 * (utf8Len+1);
			wideName.reserve ( maxLen );
			wideName.assign ( maxLen, ' ' );
			int wideLen = MultiByteToWideChar ( CP_UTF8, 0, newPath, -1, (LPWSTR)wideName.data(), (int)maxLen );
			if ( wideLen == 0 ) return false;
			HANDLE temp = CreateFileW ( (LPCWSTR)wideName.data(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING,
										(FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS), 0 );
			if ( temp != INVALID_HANDLE_VALUE ) {
				CloseHandle ( temp );
				return false;
			}
		}

	#endif
	
	try {
		LFA_FileRef newFile = LFA_Create ( newPath );
		LFA_Close ( newFile );
	} catch ( ... ) {
		// *** Unfortunate that LFA_Create throws for an existing file.
		return false;
	}

	#if XMP_WinBuild

		IgnoreParam(origPath); IgnoreParam(filePos); IgnoreParam(copyMacRsrc);

		// *** Don't handle Windows specific info yet.

	#elif XMP_MacBuild

		IgnoreParam(filePos);

		OSStatus err;
		FSRef newFSRef, origFSRef;	// Copy the "copyable" catalog info, includes the Finder info.
		
		err = FSPathMakeRef ( (XMP_Uns8*)origPath, &origFSRef, 0 );
		if ( err != noErr ) XMP_Throw ( "CreateNewFile: FSPathMakeRef failure", kXMPErr_ExternalFailure );
		err = FSPathMakeRef ( (XMP_Uns8*)newPath, &newFSRef, 0 );
		if ( err != noErr ) XMP_Throw ( "CreateNewFile: FSPathMakeRef failure", kXMPErr_ExternalFailure );

		FSCatalogInfo catInfo;	// *** What about the GetInfo comment? The Finder label?
		memset ( &catInfo, 0, sizeof(FSCatalogInfo) );

		err = FSGetCatalogInfo ( &origFSRef, kFSCatInfoGettableInfo, &catInfo, 0, 0, 0 );
		if ( err != noErr ) XMP_Throw ( "CreateNewFile: FSGetCatalogInfo failure", kXMPErr_ExternalFailure );
		err = FSSetCatalogInfo ( &newFSRef, kFSCatInfoSettableInfo, &catInfo );
		
		// *** [1841019] tolerate non mac filesystems, i.e. SMB mounts
		// this measure helps under 10.5, albeit not reliably under 10.4
		if ( err == afpAccessDenied ) 
			copyMacRsrc = false;
		else if ( err != noErr ) // all other errors are still an error
			XMP_Throw ( "CreateNewFile: FSSetCatalogInfo failure", kXMPErr_ExternalFailure );

		// *** [1841019] tolerate non mac filesystems, i.e. SMB mounts
		// this measure helps under 10.4 (and besides might be an optimization)
		if ( catInfo.rsrcLogicalSize == 0 )
			copyMacRsrc = false;
		
		if ( copyMacRsrc ) {	// Copy the resource fork as a byte stream.
			LFA_FileRef origRsrcRef = 0;
			LFA_FileRef copyRsrcRef = 0;
			try {
				origRsrcRef = LFA_OpenRsrc ( origPath, 'r' );
				XMP_Int64 rsrcSize = LFA_Measure ( origRsrcRef );
				if ( rsrcSize > 0 ) {
					copyRsrcRef = LFA_OpenRsrc ( newPath, 'w' );
					LFA_Copy ( origRsrcRef, copyRsrcRef, rsrcSize, 0, 0 );	// ! Resource fork small enough to not need abort checking.
					LFA_Close ( copyRsrcRef );
				}
				LFA_Close ( origRsrcRef );
			} catch ( ... ) {
				if ( origRsrcRef != 0 ) LFA_Close ( origRsrcRef );
				if ( copyRsrcRef != 0 ) LFA_Close ( copyRsrcRef );
				throw;
			}
		}
		

	#elif XMP_UNIXBuild
	
		IgnoreParam(filePos); IgnoreParam(copyMacRsrc);
		// *** Can't use on Mac because of frigging CW POSIX header problems!

		// *** Don't handle UNIX specific info yet.
		
		int err, newRef;
		struct stat origInfo;
		err = stat ( origPath, &origInfo );
		if ( err != 0 ) XMP_Throw ( "CreateNewFile: stat failure", kXMPErr_ExternalFailure );
		
		(void) chmod ( newPath, origInfo.st_mode );	// Ignore errors.
	
	#endif
	
	return true;

}	// CreateNewFile

// =================================================================================================

void CreateTempFile ( const std::string & origPath, std::string * tempPath, bool copyMacRsrc )
{
	// Create an empty temp file next to the source file with the same ownership and permissions.
	// The temp file has "._nn_" added as a prefix to the file name, where "nn" is a unique
	// sequence number. The "._" start is important for Bridge, telling it to ignore the file.
	
	// *** The ownership and permissions are not yet handled.
	
	#if XMP_WinBuild
		#define kUseBS	true
	#else
		#define kUseBS	false
	#endif

	// Break the full path into folder path and file name portions.

	size_t namePos;	// The origPath index of the first byte of the file name part.

	for ( namePos = origPath.size(); namePos > 0; --namePos ) {
		if ( (origPath[namePos] == '/') || (kUseBS && (origPath[namePos] == '\\')) ) {
			++namePos;
			break;
		}
	}
	if ( (origPath[namePos] == '/') || (kUseBS && (origPath[namePos] == '\\')) ) ++namePos;
	if ( namePos == origPath.size() ) XMP_Throw ( "CreateTempFile: Empty file name part", kXMPErr_InternalFailure );
	
	std::string folderPath ( origPath, 0, namePos );
	std::string origName ( origPath, namePos );
	
	// First try to create a file with "._nn_" added as a file name prefix.

	char tempPrefix[6] = "._nn_";
	
	tempPath->reserve ( origPath.size() + 5 );
	tempPath->assign ( origPath, 0, namePos );
	tempPath->append ( tempPrefix, 5 );
	tempPath->append ( origName );
	
	for ( char n1 = '0'; n1 <= '9'; ++n1 ) {
		(*tempPath)[namePos+2] = n1;
		for ( char n2 = '0'; n2 <= '9'; ++n2 ) {
			(*tempPath)[namePos+3] = n2;
			if ( CreateNewFile ( tempPath->c_str(), origPath.c_str(), namePos, copyMacRsrc ) ) return;
		}
	}
	
	// Now try to create a file with the name "._nn_XMPFilesTemp"
	
	tempPath->assign ( origPath, 0, namePos );
	tempPath->append ( tempPrefix, 5 );
	tempPath->append ( "XMPFilesTemp" );
	
	for ( char n1 = '0'; n1 <= '9'; ++n1 ) {
		(*tempPath)[namePos+2] = n1;
		for ( char n2 = '0'; n2 <= '9'; ++n2 ) {
			(*tempPath)[namePos+3] = n2;
			if ( CreateNewFile ( tempPath->c_str(), origPath.c_str(), namePos, copyMacRsrc ) ) return;
		}
	}
	
	XMP_Throw ( "CreateTempFile: Can't find unique name", kXMPErr_InternalFailure );
	
}	// CreateTempFile

// =================================================================================================
// File mode and folder info utilities
// -----------------------------------

#if XMP_WinBuild
	
	// ---------------------------------------------------------------------------------------------

	static DWORD kOtherAttrs = (FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_OFFLINE);
	
	FileMode GetFileMode ( const char * path )
	{
		std::string utf16;	// GetFileAttributes wants native UTF-16.
		ToUTF16Native ( (UTF8Unit*)path, strlen(path), &utf16 );
		utf16.append ( 2, '\0' );	// Make sure there are at least 2 final zero bytes.

		// ! A shortcut is seen as a file, we would need extra code to recognize it and find the target.
		
		DWORD fileAttrs = GetFileAttributesW ( (LPCWSTR) utf16.c_str() );
		if ( fileAttrs == INVALID_FILE_ATTRIBUTES ) return kFMode_DoesNotExist;	// ! Any failure turns into does-not-exist.

		if ( fileAttrs & FILE_ATTRIBUTE_DIRECTORY ) return kFMode_IsFolder;
		if ( fileAttrs & kOtherAttrs ) return kFMode_IsOther;
		return kFMode_IsFile;

	}	// GetFileMode
	
	// ---------------------------------------------------------------------------------------------
	
	void XMP_FolderInfo::Open ( const char * folderPath )
	{

		if ( this->dirRef != 0 ) this->Close();
		
		this->folderPath = folderPath;

	}	// XMP_FolderInfo::Open
	
	// ---------------------------------------------------------------------------------------------
	
	void XMP_FolderInfo::Close()
	{

		if ( this->dirRef != 0 ) (void) FindClose ( this->dirRef );
		this->dirRef = 0;
		this->folderPath.erase();

	}	// XMP_FolderInfo::Close
	
	// ---------------------------------------------------------------------------------------------
	
	bool XMP_FolderInfo::GetFolderPath ( std::string * folderPath )
	{

		if ( this->folderPath.empty() ) return false;
		
		*folderPath = this->folderPath;
		return true;

	}	// XMP_FolderInfo::GetFolderPath
	
	// ---------------------------------------------------------------------------------------------
	
	bool XMP_FolderInfo::GetNextChild ( std::string * childName )
	{
		bool found = false;
		WIN32_FIND_DATAW childInfo;

		if ( this->dirRef != 0 ) {

			found = FindNextFile ( this->dirRef, &childInfo );
			if ( ! found ) return false;

		} else {

			if ( this->folderPath.empty() ) {
				XMP_Throw ( "XMP_FolderInfo::GetNextChild - not open", kXMPErr_InternalFailure );
			}

			std::string findPath = this->folderPath;
			findPath += "\\*";
			std::string utf16;	// FindFirstFile wants native UTF-16.
			ToUTF16Native ( (UTF8Unit*)findPath.c_str(), findPath.size(), &utf16 );
			utf16.append ( 2, '\0' );	// Make sure there are at least 2 final zero bytes.

			this->dirRef = FindFirstFileW ( (LPCWSTR) utf16.c_str(), &childInfo );
			if ( this->dirRef == 0 ) return false;
			found = true;

		}

		// Ignore all children with names starting in '.'. This covers ., .., .DS_Store, etc.
		while ( found && (childInfo.cFileName[0] == '.') ) {
			found = FindNextFile ( this->dirRef, &childInfo );
		}
		if ( ! found ) return false;
		
		size_t len16 = 0;
		while ( childInfo.cFileName[len16] != 0 ) ++len16;
		FromUTF16Native ( (UTF16Unit*)childInfo.cFileName, len16, childName );	// The cFileName field is native UTF-16.

		return true;

	}	// XMP_FolderInfo::GetNextChild
	
	// ---------------------------------------------------------------------------------------------

#else	// Mac and UNIX both use POSIX functions.
	
	// ---------------------------------------------------------------------------------------------

	FileMode GetFileMode ( const char * path )
	{
		struct stat fileInfo;
		
		int err = stat ( path, &fileInfo );
		if ( err != 0 ) return kFMode_DoesNotExist;	// ! Any failure turns into does-not-exist.
		
		// ! The target of a symlink is properly recognized, not the symlink itself. A Mac alias is
		// ! seen as a file, we would need extra code to recognize it and find the target.
		
		if ( S_ISREG ( fileInfo.st_mode ) ) return kFMode_IsFile;
		if ( S_ISDIR ( fileInfo.st_mode ) ) return kFMode_IsFolder;
		return kFMode_IsOther;

	}	// GetFileMode
	
	// ---------------------------------------------------------------------------------------------
	
	void XMP_FolderInfo::Open ( const char * folderPath )
	{

		if ( this->dirRef != 0 ) this->Close();
		
		this->dirRef = opendir ( folderPath );
		if ( this->dirRef == 0 ) XMP_Throw ( "XMP_FolderInfo::Open - opendir failed", kXMPErr_ExternalFailure );
		this->folderPath = folderPath;

	}	// XMP_FolderInfo::Open
	
	// ---------------------------------------------------------------------------------------------
	
	void XMP_FolderInfo::Close()
	{

		if ( this->dirRef != 0 ) (void) closedir ( this->dirRef );
		this->dirRef = 0;
		this->folderPath.erase();

	}	// XMP_FolderInfo::Close
	
	// ---------------------------------------------------------------------------------------------
	
	bool XMP_FolderInfo::GetFolderPath ( std::string * folderPath )
	{

		if ( this->folderPath.empty() ) return false;
		
		*folderPath = this->folderPath;
		return true;

	}	// XMP_FolderInfo::GetFolderPath
	
	// ---------------------------------------------------------------------------------------------
	
	bool XMP_FolderInfo::GetNextChild ( std::string * childName )
	{
		struct dirent * childInfo = 0;

		if ( this->dirRef == 0 ) XMP_Throw ( "XMP_FolderInfo::GetNextChild - not open", kXMPErr_InternalFailure );
		
		while ( true ) {
			// Ignore all children with names starting in '.'. This covers ., .., .DS_Store, etc.
			childInfo = readdir ( this->dirRef );	// ! Depends on global lock, readdir is not thread safe.
			if ( childInfo == 0 ) return false;
			if ( *childInfo->d_name != '.' ) break;
		}
		
		*childName = childInfo->d_name;
		return true;

	}	// XMP_FolderInfo::GetNextChild
	
	// ---------------------------------------------------------------------------------------------

#endif

// =================================================================================================
// GetPacketCharForm
// =================
//
// The first character must be U+FEFF or ASCII, typically '<' for an outermost element, initial
// processing instruction, or XML declaration. The second character can't be U+0000.
// The possible input sequences are:
//   Cases with U+FEFF
//      EF BB BF -- - UTF-8
//      FE FF -- -- - Big endian UTF-16
//      00 00 FE FF - Big endian UTF 32
//      FF FE 00 00 - Little endian UTF-32
//      FF FE -- -- - Little endian UTF-16
//   Cases with ASCII
//      nn mm -- -- - UTF-8 -
//      00 00 00 nn - Big endian UTF-32
//      00 nn -- -- - Big endian UTF-16
//      nn 00 00 00 - Little endian UTF-32
//      nn 00 -- -- - Little endian UTF-16

static XMP_Uns8 GetPacketCharForm ( XMP_StringPtr packetStr, XMP_StringLen packetLen )
{
	XMP_Uns8   charForm = kXMP_CharUnknown;
	XMP_Uns8 * unsBytes = (XMP_Uns8*)packetStr;	// ! Make sure comparisons are unsigned.
	
	if ( packetLen < 2 ) return kXMP_Char8Bit;

	if ( packetLen < 4 ) {
	
		// These cases are based on the first 2 bytes:
		//   00 nn Big endian UTF-16
		//   nn 00 Little endian UTF-16
		//   FE FF Big endian UTF-16
		//   FF FE Little endian UTF-16
		//   Otherwise UTF-8
		
		if ( packetStr[0] == 0 ) return kXMP_Char16BitBig;
		if ( packetStr[1] == 0 ) return kXMP_Char16BitLittle;
		if ( CheckBytes ( packetStr, "\xFE\xFF", 2 ) ) return kXMP_Char16BitBig;
		if ( CheckBytes ( packetStr, "\xFF\xFE", 2 ) ) return kXMP_Char16BitLittle;
		return kXMP_Char8Bit;

	}
	
	// If we get here the packet is at least 4 bytes, could be any form.
	
	if ( unsBytes[0] == 0 ) {
	
		// These cases are:
		//   00 nn -- -- - Big endian UTF-16
		//   00 00 00 nn - Big endian UTF-32
		//   00 00 FE FF - Big endian UTF 32
		
		if ( unsBytes[1] != 0 ) {
			charForm = kXMP_Char16BitBig;			// 00 nn
		} else {
			if ( (unsBytes[2] == 0) && (unsBytes[3] != 0) ) {
				charForm = kXMP_Char32BitBig;		// 00 00 00 nn
			} else if ( (unsBytes[2] == 0xFE) && (unsBytes[3] == 0xFF) ) {
				charForm = kXMP_Char32BitBig;		// 00 00 FE FF
			}
		}
		
	} else {
	
		// These cases are:
		//   FE FF -- -- - Big endian UTF-16, FE isn't valid UTF-8
		//   FF FE 00 00 - Little endian UTF-32, FF isn't valid UTF-8
		//   FF FE -- -- - Little endian UTF-16
		//   nn mm -- -- - UTF-8, includes EF BB BF case
		//   nn 00 00 00 - Little endian UTF-32
		//   nn 00 -- -- - Little endian UTF-16
		
		if ( unsBytes[0] == 0xFE ) {
			if ( unsBytes[1] == 0xFF ) charForm = kXMP_Char16BitBig;	// FE FF
		} else if ( unsBytes[0] == 0xFF ) {
			if ( unsBytes[1] == 0xFE ) {
				if ( (unsBytes[2] == 0) && (unsBytes[3] == 0) ) {
					charForm = kXMP_Char32BitLittle;	// FF FE 00 00
				} else {
					charForm = kXMP_Char16BitLittle;	// FF FE
				}
			}
		} else if ( unsBytes[1] != 0 ) {
			charForm = kXMP_Char8Bit;					// nn mm
		} else {
			if ( (unsBytes[2] == 0) && (unsBytes[3] == 0) ) {
				charForm = kXMP_Char32BitLittle;		// nn 00 00 00
			} else {
				charForm = kXMP_Char16BitLittle;		// nn 00
			}
		}

	}
	
	//	XMP_Assert ( charForm != kXMP_CharUnknown );
	return charForm;

}	// GetPacketCharForm

// =================================================================================================
// FillPacketInfo
// ==============
//
// If a packet wrapper is present, the the packet string is roughly:
//   <?xpacket begin= ...?>
//   <outer-XML-element>
//     ... more XML ...
//   </outer-XML-element>
//   ... whitespace padding ...
//   <?xpacket end='.'?>

// The 8-bit form is 14 bytes, the 16-bit form is 28 bytes, the 32-bit form is 56 bytes.
#define k8BitTrailer  "<?xpacket end="
#define k16BitTrailer "<\0?\0x\0p\0a\0c\0k\0e\0t\0 \0e\0n\0d\0=\0"
#define k32BitTrailer "<\0\0\0?\0\0\0x\0\0\0p\0\0\0a\0\0\0c\0\0\0k\0\0\0e\0\0\0t\0\0\0 \0\0\0e\0\0\0n\0\0\0d\0\0\0=\0\0\0"
static XMP_StringPtr kPacketTrailiers[3] = { k8BitTrailer, k16BitTrailer, k32BitTrailer };

void FillPacketInfo ( const std::string & packet, XMP_PacketInfo * info )
{
	XMP_StringPtr packetStr = packet.c_str();
	XMP_StringLen packetLen = (XMP_StringLen) packet.size();
	if ( packetLen == 0 ) return;
	
	info->charForm = GetPacketCharForm ( packetStr, packetLen );
	XMP_StringLen charSize = XMP_GetCharSize ( info->charForm );
	
	// Look for a packet wrapper. For our purposes, we can be lazy and just look for the trailer PI.
	// If that is present we'll assume that a recognizable header is present. First do a bytewise
	// search for '<', then a char sized comparison for the start of the trailer. We don't really
	// care about big or little endian here. We're looking for ASCII bytes with zeroes between.
	// Shorten the range comparisons (n*charSize) by 1 to easily tolerate both big and little endian.

	XMP_StringLen padStart, padEnd;
	XMP_StringPtr packetTrailer = kPacketTrailiers [ charSize>>1 ];

	padEnd = packetLen - 1;
	for ( ; padEnd > 0; --padEnd ) if ( packetStr[padEnd] == '<' ) break;
	if ( (packetStr[padEnd] != '<') || ((packetLen - padEnd) < (18*charSize)) ) return;
	if ( ! CheckBytes ( &packetStr[padEnd], packetTrailer, (13*charSize) ) ) return;
	
	info->hasWrapper = true;
	
	char rwFlag = packetStr [padEnd + 15*charSize];
	if ( rwFlag == 'w' ) info->writeable = true;
	
	// Look for the start of the padding, right after the last XML end tag.
	
	padStart = padEnd;	// Don't do the -charSize here, might wrap below zero.
	for ( ; padStart >= charSize; padStart -= charSize ) if ( packetStr[padStart] == '>' ) break;
	if ( padStart < charSize ) return;
	padStart += charSize;	// The padding starts after the '>'.
	
	info->padSize = padEnd - padStart;	// We want bytes of padding, not character units.

}	// FillPacketInfo

// =================================================================================================
// ReadXMPPacket
// =============

void ReadXMPPacket ( XMPFileHandler * handler )
{
	LFA_FileRef   fileRef   = handler->parent->fileRef;
	std::string & xmpPacket = handler->xmpPacket;
	XMP_StringLen packetLen = handler->packetInfo.length;

	if ( packetLen == 0 ) XMP_Throw ( "ReadXMPPacket - No XMP packet", kXMPErr_BadXMP );
	
	xmpPacket.erase();
	xmpPacket.reserve ( packetLen );
	xmpPacket.append ( packetLen, ' ' );

	XMP_StringPtr packetStr = XMP_StringPtr ( xmpPacket.c_str() );	// Don't set until after reserving the space!

	LFA_Seek ( fileRef, handler->packetInfo.offset, SEEK_SET );
	LFA_Read ( fileRef, (char*)packetStr, packetLen, kLFA_RequireAll );
	
}	// ReadXMPPacket

// =================================================================================================
// XMPFileHandler::ProcessXMP
// ==========================
//
// This base implementation just parses the XMP. If the derived handler does reconciliation then it
// must have its own implementation of ProcessXMP.

void XMPFileHandler::ProcessXMP()
{
	
	if ( (!this->containsXMP) || this->processedXMP ) return;

	if ( this->handlerFlags & kXMPFiles_CanReconcile ) {
		XMP_Throw ( "Reconciling file handlers must implement ProcessXMP", kXMPErr_InternalFailure );
	}

	SXMPUtils::RemoveProperties ( &this->xmpObj, 0, 0, kXMPUtil_DoAllProperties );
	this->xmpObj.ParseFromBuffer ( this->xmpPacket.c_str(), (XMP_StringLen)this->xmpPacket.size() );
	this->processedXMP = true;
	
}	// XMPFileHandler::ProcessXMP

// =================================================================================================
// XMPFileHandler::GetSerializeOptions
// ===================================
//
// This base implementation just selects compact serialization. The character form and padding/in-place
// settings are added in the common code before calling SerializeToBuffer.

XMP_OptionBits XMPFileHandler::GetSerializeOptions()
{

	return kXMP_UseCompactFormat;

}	// XMPFileHandler::GetSerializeOptions

// =================================================================================================
