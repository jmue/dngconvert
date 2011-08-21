// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2007 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

#include "SWF_Support.hpp"
#include "zlib.h"


namespace SWF_Support
{

	// =============================================================================================

	static int CalcHeaderSize ( IO::InputStream* inputStream )
	{
		int size = 0; 		

		try {

			XMP_Uns8 buffer[1];
			long bytesRead = inputStream->Read ( buffer, 1 );
			if ( bytesRead != 1 ) return 0;

			// The RECT datatype has a variable size depending on the packed bit-values -> rectDatatypeSize = size in bytes
			int bits = int(buffer[0] >> 3);
			int bytes = ((5 + (4 * bits)) / 8) + 1; 

			size = 12 + bytes;

		} catch ( ... ) {

			return 0;
		}

		inputStream->Skip ( size - inputStream->GetCurrentPos() );
		return size;

	}	// CalcHeaderSize

	// =============================================================================================

	static unsigned long CheckTag ( IO::InputStream* inputStream, TagState& inOutTagState, TagData& inOutTagData )
	{
		unsigned long ret = 0;
		XMP_Uns8 * buffer = 0;

		try {

			if ( inOutTagData.id == SWF_TAG_ID_METADATA ) {

				buffer = new XMP_Uns8[inOutTagData.len];
				XMP_Uns32 bytes = inputStream->Read ( buffer, inOutTagData.len );

				XMP_Assert ( bytes == inOutTagData.len );

				inOutTagState.xmpPos = inOutTagData.pos + inOutTagData.offset;
				inOutTagState.xmpLen = inOutTagData.len;
				inOutTagData.xmp = true;
				
				inOutTagState.xmpPacket.assign ( (char*)buffer, inOutTagData.len );

				delete[] buffer;
				buffer = 0;
				ret = inOutTagState.xmpLen;
				
			}

		} catch ( ... ) {
		
			if ( buffer != 0 ) {
				delete[] buffer;
				buffer = 0;
			}
		
		}
		
		if ( buffer != 0 ) {
			delete[] buffer;
			buffer = 0;
		}
	
		return ret;

	}	// CheckTag

	// =============================================================================================

	bool HasMetadata ( IO::InputStream* inputStream, TagState& tagState )
	{

		XMP_Uns32 flags = ReadFileAttrFlags ( inputStream );
		tagState.fileAttrFlags = flags;
		return ((flags & SWF_METADATA_FLAG) == SWF_METADATA_FLAG);

	}	// HasMetadata

	// =============================================================================================

	XMP_Uns32 ReadFileAttrFlags ( IO::InputStream* inputStream )
	{
		XMP_Uns8 buffer[4];
		
		XMP_Uns32 bytes = inputStream->Read ( buffer, 4 );
		XMP_Assert ( bytes == 4 );

		return GetUns32LE ( buffer );

	}	// ReadFileAttrFlags

	// =============================================================================================

	long OpenSWF ( IO::InputStream *inputStream, TagState & inOutTagState )
	{
		XMP_Uns64 pos = 0;
		long name;
		XMP_Uns32 len;

		inOutTagState.headerSize = CalcHeaderSize ( inputStream );
		pos = inOutTagState.headerSize;
		
		// read first and following chunks
		bool running = true;
		while ( running ) {
			running = ReadTag ( inputStream, inOutTagState, &name, &len, pos );
			if ( inOutTagState.cachingFile ) {
				if ( (! inOutTagState.hasXMP) || (inOutTagState.xmpLen > 0) ) running = false;
			}
		}
			
		return (long) inOutTagState.tags.size();

	}	// OpenSWF

	// =============================================================================================

	bool  ReadTag ( IO::InputStream* inputStream, TagState & inOutTagState,
					long* tagType, XMP_Uns32* tagLength, XMP_Uns64& inOutPosition )
	{

		try {

			XMP_Uns64 startPosition = inOutPosition;
			long bytesRead;
			XMP_Uns8 buffer[4];
			
			bytesRead = inputStream->Read ( buffer, 2 );
			if ( bytesRead != 2 ) return false;

			inOutPosition += 2;
			XMP_Uns16 code = GetUns16LE ( buffer );
			*tagType = code >> 6;
			*tagLength = code & 0x3f;

			bool longTag = false;

			if ( *tagLength == 0x3f ) {
				longTag = true;
				bytesRead = inputStream->Read ( buffer, 4 );
				if ( bytesRead != 4 ) return false;
				inOutPosition += 4;
				*tagLength = GetUns32LE ( buffer );
			}

			inOutPosition += *tagLength;

			TagData	newTag;
	
			newTag.pos = startPosition;
			newTag.len = *tagLength;
			newTag.id = *tagType;
			newTag.offset = ( (! longTag) ? 2 : 6 );

			// we cannot check valid XMP within the handler
			// provide validating XMP by invoking XMPCore
			// check tag for XMP
			if ( newTag.id == SWF_TAG_ID_METADATA ) {
				newTag.xmp = true;
				inOutTagState.xmpTag = newTag;
				CheckTag ( inputStream, inOutTagState, newTag );
				if ( ! inOutTagState.hasFileAttrTag ) inOutTagState.hasXMP = true;
			}
				
			//store FileAttribute Tag
			if ( newTag.id == SWF_TAG_ID_FILEATTRIBUTES ) {
				inOutTagState.hasFileAttrTag = true;
				inOutTagState.fileAttrTag = newTag;
				inOutTagState.hasXMP = HasMetadata ( inputStream, inOutTagState );
				//decreasing since stream moved on within HasMetadata function
				*tagLength -= 4;
			}
			
			//store tag in vector to process later
			inOutTagState.tags.push_back ( newTag );

			//seek to next tag
			if ( ! newTag.xmp ) inputStream->Skip ( *tagLength );
			if ( inputStream->IsEOF() ) return false;

		} catch ( ... ) {

			return false;

		}
	
		return true;

	}	// ReadTag

	// =============================================================================================

	bool WriteXMPTag ( LFA_FileRef fileRef, XMP_Uns32 len, const char* inBuffer )
	{
		bool ret = false;

		XMP_Uns16 code = MakeUns16LE ( (SWF_TAG_ID_METADATA << 6) | 0x3F );
		XMP_Uns32 length = MakeUns32LE ( len );

		try {
			LFA_Write (fileRef, &code, 2 );
			LFA_Write (fileRef, &length, 4 );
			LFA_Write (fileRef, inBuffer, len );
			ret = true;
		} catch ( ... ) {}

		return ret;

	}	// WriteXMPTag

	// =============================================================================================

	bool CopyHeader ( LFA_FileRef sourceRef, LFA_FileRef destRef, const TagState& tagState )
	{
		try {
			int headerSize = tagState.headerSize;
			LFA_Seek ( sourceRef, 0, SEEK_SET );
			LFA_Copy ( sourceRef, destRef, headerSize );
			return true;
		} catch ( ... ) {}

		return false;

	}	// CopyHeader

	// =============================================================================================

	bool UpdateHeader ( LFA_FileRef fileRef )
	{

		try {

			XMP_Int64 length64 = LFA_Measure ( fileRef );
			if ( (length64 < 8) || (length64 > (XMP_Int64)0xFFFFFFFFULL) ) return false;

			XMP_Uns32 length32 = MakeUns32LE ( (XMP_Uns32)length64 );

			LFA_Seek ( fileRef, 4, SEEK_SET );
			LFA_Write ( fileRef, &length32, 4 );
			
			return true;

		} catch ( ... ) {}
	
		return false;

	}	// UpdateHeader

	// =============================================================================================

	bool CopyTag ( LFA_FileRef sourceRef, LFA_FileRef destRef, TagData& tag )
	{

		try {
			LFA_Seek ( sourceRef, tag.pos, SEEK_SET );
			LFA_Copy ( sourceRef, destRef, (tag.len + tag.offset) );
			return true;
		} catch ( ... ) {}
	
		return false;

	}	// CopyTag

	// =============================================================================================

	bool ReadBuffer ( LFA_FileRef fileRef, XMP_Uns64& pos, XMP_Uns32 len, XMP_Uns8* outBuffer )
	{

		try {
			if ( (fileRef == 0) || (outBuffer == 0) ) return false;
			LFA_Seek ( fileRef, pos, SEEK_SET );
			long bytesRead = LFA_Read ( fileRef, outBuffer, len );
			return ( (XMP_Uns32)bytesRead == len );
		} catch ( ... ) {}

		return false;

	}	// ReadBuffer

	// =============================================================================================

	bool WriteBuffer ( LFA_FileRef fileRef, XMP_Uns64& pos, XMP_Uns32 len, const char* inBuffer )
	{

		try {
			if ( (fileRef == 0) || (inBuffer == 0) ) return false;
			LFA_Seek ( fileRef, pos, SEEK_SET );
			LFA_Write ( fileRef, inBuffer, len );
			return true;
		}
		catch ( ... ) {}

		return false;

	}	// WriteBuffer

	// =============================================================================================

	bool UpdateFileAttrTag ( LFA_FileRef fileRef, const TagData& fileAttrTag, const TagState& tagState )
	{

		try {
			XMP_Uns32 flags = tagState.fileAttrFlags;
			flags |= SWF_METADATA_FLAG;
			return WriteFileAttrFlags ( fileRef, fileAttrTag, flags );
		} catch ( ... ) {}

		return false;

	}	// UpdateFileAttrTag

	// =============================================================================================


	bool WriteFileAttrFlags ( LFA_FileRef fileRef, const TagData& fileAttrTag, XMP_Uns32  flags )
	{

		try {
			XMP_Uns32 bitMask = MakeUns32LE ( flags );
			LFA_Seek ( fileRef, fileAttrTag.pos + fileAttrTag.offset, SEEK_SET );
			LFA_Write ( fileRef, &bitMask, 4 );
			return true;
		} catch ( ... ) {}

		return false;

	}	// WriteFileAttrFlags
	
	// =============================================================================================
	// =============================================================================================

	FileInfo::FileInfo ( LFA_FileRef fileRef, const std::string& origPath )
	{

		this->compressedFile = false;
		this->iSize = 0;
		this->CheckFormat ( fileRef );
		this->origFilePath.assign ( origPath );
		this->fileRef = fileRef;

	}	// FileInfo::FileInfo

	// =============================================================================================

	void FileInfo::CheckFormat ( LFA_FileRef fileRef )
	{
		IOBuffer ioBuf;
	
		LFA_Seek ( fileRef, 0, SEEK_SET );

		if ( CheckFileSpace ( fileRef, &ioBuf, SWF_SIGNATURE_LEN ) ) {

			if ( CheckBytes ( ioBuf.ptr, SWF_F_SIGNATURE_DATA, SWF_SIGNATURE_LEN ) ) {
				this->compressedFile = false;
			} else if ( CheckBytes ( ioBuf.ptr, SWF_C_SIGNATURE_DATA, SWF_SIGNATURE_LEN ) ) {
				this->compressedFile = true;
			}

			LFA_Seek ( fileRef, 4, SEEK_SET );
			XMP_Uns8 buffer[4];
			LFA_Read ( fileRef, buffer, 4 );
			iSize = GetUns32LE ( buffer );

		}

		LFA_Seek ( fileRef, 0, SEEK_SET );

	}	// FileInfo::CheckFormat

	// =============================================================================================

	bool FileInfo::IsCompressed()
	{

		return this->compressedFile;

	}	// FileInfo::IsCompressed

	// =============================================================================================

	LFA_FileRef FileInfo::Decompress()
	{

		if ( ! this->IsCompressed() ) return this->fileRef;
		
		LFA_FileRef updateRef = 0;
		std::string updatePath;

		try {

			CreateTempFile ( this->origFilePath, &updatePath, kCopyMacRsrc );
			updateRef = LFA_Open ( updatePath.c_str(), 'w' );
			this->tmpFilePath.assign ( updatePath );

			int ret = this->Encode ( this->fileRef, updateRef, FWS, Inf );
			this->tmpFileRef = updateRef;
			if ( ret != Z_OK ) XMP_Throw ( "zstream error occured", kXMPErr_ExternalFailure );

			return this->tmpFileRef;

		} catch ( ... ) {

			LFA_Close ( updateRef );
			LFA_Delete ( updatePath.c_str() );
			return this->fileRef;

		}

	}	// FileInfo::Decompress

	// =============================================================================================

	void FileInfo::Compress ( LFA_FileRef sourceRef, LFA_FileRef destRef )
	{

		if ( this->IsCompressed() ) this->Encode ( sourceRef, destRef, CWS, Def );

	}	// FileInfo::Compress

	// =============================================================================================

	void FileInfo::Clean()
	{

		if ( this->tmpFileRef != 0 ) LFA_Close ( this->tmpFileRef );
		this->tmpFileRef = 0;
		this->CleanTempFiles();

	}	// FileInfo::Clean

	// =============================================================================================

	void FileInfo::CleanTempFiles()
	{

		if ( ! this->tmpFilePath.empty() ) {
			LFA_Delete ( this->tmpFilePath.c_str() );
			this->tmpFilePath.erase();
		}

	}	// FileInfo::CleanTempFiles

	// =============================================================================================

	int FileInfo::Encode ( LFA_FileRef fileRef, LFA_FileRef updateRef, SWF_MODE swfMode, CompressionFnc cmpFnc )
	{

		LFA_Seek ( updateRef, 0, SEEK_SET );
		
		if ( swfMode == CWS ) {
			LFA_Write ( updateRef, SWF_C_SIGNATURE_DATA, SWF_SIGNATURE_LEN );
		} else {
			XMP_Assert ( swfMode == FWS );
			LFA_Write ( updateRef, SWF_F_SIGNATURE_DATA, SWF_SIGNATURE_LEN );
		}

		LFA_Seek ( fileRef, SWF_SIGNATURE_LEN, SEEK_SET );
		LFA_Seek ( updateRef, SWF_SIGNATURE_LEN, SEEK_SET );
		LFA_Copy ( fileRef, updateRef, 5 );

		int ret = cmpFnc ( fileRef, updateRef );
		LFA_Flush ( updateRef );

		return ret;

	}	// FileInfo::Encode
	
	// =============================================================================================

	int FileInfo::Inf ( LFA_FileRef source, LFA_FileRef dest )
	{
		int ret;
		unsigned have;
		z_stream strm;
		unsigned char in[CHUNK];
		unsigned char out[CHUNK];

		XMP_Uns64 allBytes = 0;

		// allocate inflate state 
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;

		ret = inflateInit ( &strm );

		if ( ret != Z_OK ) return ret;

		// decompress until deflate stream ends or end of file 

		LFA_Seek ( source, SWF_COMPRESSION_BEGIN, SEEK_SET );
		XMP_Uns64 outPos = SWF_COMPRESSION_BEGIN;

		try {

			do { 

				strm.avail_in = LFA_Read ( source, in, CHUNK );
				if ( strm.avail_in == 0 ) {
					ret = Z_STREAM_END;
					break;
				}

				strm.next_in = in;

				// run inflate() on input until output buffer not full 
				do {

					strm.avail_out = CHUNK; 
					strm.next_out = out;
					ret = inflate ( &strm, Z_NO_FLUSH );
					XMP_Assert ( ret != Z_STREAM_ERROR );  // state not clobbered 

					switch ( ret ) {
						case Z_NEED_DICT:	ret = Z_DATA_ERROR;	// and fall through 
						case Z_DATA_ERROR:
						case Z_MEM_ERROR:	inflateEnd ( &strm );
											return ret;
					}

					have = CHUNK - strm.avail_out;
					LFA_Seek ( dest, outPos, SEEK_SET );
					LFA_Write ( dest, out, have );
					
					outPos += have;
					
				} while ( strm.avail_out == 0 );

				// done when inflate() says it's done 

			} while ( ret != Z_STREAM_END );

		} catch ( ... ) {

			inflateEnd ( &strm );
			return Z_ERRNO;

		}

		// clean up and return 
		inflateEnd ( &strm );
		return ( (ret == Z_STREAM_END) ? Z_OK : Z_DATA_ERROR );

	}	// FileInfo::Inf

	// =============================================================================================

	int FileInfo::Def ( LFA_FileRef source, LFA_FileRef dest )
	{
		int ret, flush;
		unsigned have;
		z_stream strm;
		unsigned char in[CHUNK];
		unsigned char out[CHUNK];

		// allocate deflate state
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit ( &strm, SWF_DEFAULT_COMPRESSION_LEVEL );
		if ( ret != Z_OK ) return ret;

		// compress until end of file

		LFA_Seek ( source, SWF_COMPRESSION_BEGIN, SEEK_SET );
		XMP_Uns64 outPos = SWF_COMPRESSION_BEGIN;

		try {

			do {

				strm.avail_in = LFA_Read ( source, in, CHUNK );

				flush = ( (strm.avail_in < CHUNK) ? Z_FINISH : Z_NO_FLUSH );
				
				strm.next_in = in;

				// run deflate() on input until output buffer not full, finish
				// compression if all of source has been read in
				do {

					strm.avail_out = CHUNK;
					strm.next_out = out;
					ret = deflate ( &strm, flush );    // no bad return value
					XMP_Assert ( ret != Z_STREAM_ERROR );  // state not clobbered
					have = CHUNK - strm.avail_out;
					
					LFA_Seek ( dest, outPos, SEEK_SET );
					LFA_Write ( dest, out, have );
					outPos += have;

				} while ( strm.avail_out == 0 );
				XMP_Assert ( strm.avail_in == 0 );	// all input will be used

				// done when last data in file processed

			} while ( flush != Z_FINISH );
			XMP_Assert ( ret == Z_STREAM_END );	// stream will be complete

		} catch ( ... ) {

			deflateEnd ( &strm );
			return Z_ERRNO;

		}
		
		/* clean up and return */
		deflateEnd ( &strm );
		return Z_OK;

	}	// FileInfo::Def

	// =============================================================================================

} // namespace SWF_Support

// =================================================================================================
// =================================================================================================

namespace IO
{

	// =============================================================================================

	void FileInputStream::InitStream()
	{

		iEndPos = LFA_Seek ( iFile, 0, SEEK_END );
		iPos = LFA_Seek ( iFile, 0, SEEK_SET );

	}	// FileInputStream::InitStream
	
	// =============================================================================================

	XMP_Int32 FileInputStream::Read ( XMP_Uns8* ioBuf, XMP_Int32 len )
	{
		if ( IsEOF() ) throw new IOException ( IO::STATUS_EOF );
		
		XMP_Int32 bytes = LFA_Read ( iFile, ioBuf, len );
		iPos += bytes;

		return bytes;
			
	}	// FileInputStream::Read

	// =============================================================================================

	XMP_Int64 FileInputStream::Skip ( XMP_Int64 len )
	{

		if ( IsEOF() ) return 0;
	
		iPos += len;
		XMP_Int64 bytes = LFA_Seek ( iFile, iPos, SEEK_SET );
		return bytes;

	}	// FileInputStream::Skip

	// =============================================================================================

	void FileInputStream::Reset()
	{

		iPos = LFA_Seek ( iFile, 0, SEEK_SET );

	}	// FileInputStream::Reset

	// =============================================================================================

	bool FileInputStream::IsEOF()
	{

		return (iPos >= iEndPos);

	}	// FileInputStream::IsEOF

	// =============================================================================================
	// =============================================================================================

	namespace ZIP
	{

		// =========================================================================================

		DeflateInputStream::DeflateInputStream ( LFA_FileRef file, XMP_Int32 bufferLength )
			: FileInputStream(file), iStatus(0), iBufferLength(bufferLength)
		{

			InitStream();
			iBuffer = new XMP_Uns8[bufferLength];

		}	// DeflateInputStream::DeflateInputStream
		
		// =========================================================================================

		DeflateInputStream::~DeflateInputStream()
		{

			inflateEnd ( &iStream );
			delete[] iBuffer;
			iBuffer = NULL;

		}	// DeflateInputStream::~DeflateInputStream

		// =========================================================================================

		void DeflateInputStream::InitStream()
		{

			iStream.zalloc = Z_NULL;
			iStream.zfree = Z_NULL;
			iStream.opaque = Z_NULL;
			iStream.avail_in = 0;
			iStream.next_in = Z_NULL;
			iStream.avail_out = 1;

			iStatus = inflateInit ( &iStream );
			if ( iStatus != Z_OK ) throw new ZIPException ( iStatus );

		}	// DeflateInputStream::InitStream

		// =========================================================================================

		XMP_Int32 DeflateInputStream::Read ( XMP_Uns8* ioBuf, XMP_Int32 len )
		{

			// *** if ( len > iBufferLength ) throw new IOException ( STATUS_BUFFER_OVERFLOW );

			if( iStream.avail_out != 0 ) {
				XMP_Int64 pos = GetCurrentPos();
				iStream.avail_in = FileInputStream::Read ( iBuffer, iBufferLength );
				iPos = pos + len;
				iStream.next_in = iBuffer;
			}

			iStream.avail_out = len;				
			iStream.next_out = ioBuf;

			iStatus = inflate ( &iStream, Z_NO_FLUSH );

			if ( iStatus == Z_MEM_ERROR ) {
				inflateEnd ( &iStream );
				throw new ZIPException ( Z_MEM_ERROR );
			}

			return (len - iStream.avail_out);
			
		}	// DeflateInputStream::Read

		// =========================================================================================

		XMP_Int32 DeflateInputStream::Read ( XMP_Uns8* ioBuf )
		{

		   return Read ( ioBuf, iBufferLength );

		}	// DeflateInputStream::Read

		// =========================================================================================

		void DeflateInputStream::Close()
		{

			inflateEnd ( &iStream );
			iPos = 0;

		}	// DeflateInputStream::Close

		// =========================================================================================

		bool DeflateInputStream::IsEOF()
		{

			return (iStatus == Z_STREAM_END);

		}	// DeflateInputStream::IsEOF

		// =========================================================================================

		XMP_Int64 DeflateInputStream::Skip ( XMP_Int64 len )
		{

			return Skip ( len, DEFLATE );

		}	// DeflateInputStream::Skip

		// =========================================================================================

		XMP_Int64 DeflateInputStream::Skip ( XMP_Int64 len, EDeflate deflate )
		{

			switch ( deflate ) {

				case DEFLATE:
					{
						XMP_Uns8 * buffer = new XMP_Uns8 [ (XMP_Uns32)len ];
						XMP_Int64 bytes = Read ( buffer, (XMP_Uns32)len );
						delete[] buffer;
						return bytes;
					}
					break;

				case DEFLATE_NO:
					return FileInputStream::Skip ( len );

				default:
					throw new IOException ( STATUS_SKIP );

			}

		}	// DeflateInputStream::Skip

		// =========================================================================================

	} // namespace ZIP

	// =============================================================================================
	
} // namespace IO
