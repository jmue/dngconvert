#ifndef __SWF_Support_hpp__
#define __SWF_Support_hpp__ 1

// =================================================================================================
// ADOBE SYSTEMS INCORPORATED
// Copyright 2008 Adobe Systems Incorporated
// All Rights Reserved
//
// NOTICE: Adobe permits you to use, modify, and distribute this file in accordance with the terms
// of the Adobe license agreement accompanying it.
// =================================================================================================

//#include "XMP_Environment.h"	// ! This must be the first include.

#include "XMPFiles_Impl.hpp"

#define SWF_SIGNATURE_LEN				3
#define SWF_C_SIGNATURE_DATA			"\x43\x57\x53"
#define SWF_F_SIGNATURE_DATA			"\x46\x57\x53"

#define SWF_TAG_ID_FILEATTRIBUTES		69
#define SWF_TAG_ID_METADATA				77
#define SWF_TAG_ID_ENDTAG				0

#define SWF_METADATA_FLAG				0x10
#define SWF_DEFAULT_COMPRESSION_LEVEL	Z_DEFAULT_COMPRESSION

#define SWF_COMPRESSION_BEGIN			8

#define CHUNK							16384

#include "zlib.h"

namespace IO
{
	//------------------------------------------------------------------
	//input/output stream declaration
	typedef enum {FLUSH, FLUSH_NO} EFlush;

	class InputStream
	{
		public:
			virtual ~InputStream() {};
			virtual XMP_Int32 Read(XMP_Uns8 * ioBuf, XMP_Int32 len) = 0;
			virtual XMP_Int64 Skip(XMP_Int64 len) = 0;
			virtual void Reset(void) = 0;
			virtual void Close(void) = 0;
			virtual bool IsEOF(void) = 0;
			virtual XMP_Int64 GetCurrentPos(void) = 0;
	};

	
	class FileInputStream : public InputStream
	{
		public:
			FileInputStream(LFA_FileRef file) : iFile(file), iPos(0), iEndPos(0) { InitStream(); };
			virtual ~FileInputStream() {};

			virtual XMP_Int32 Read(XMP_Uns8 * ioBuf, XMP_Int32 len);
			virtual XMP_Int64 Skip(XMP_Int64 len);
			virtual void Reset(void);
			virtual void Close(void) {};
			virtual bool IsEOF(void);
			virtual XMP_Int64 GetCurrentPos(void) { return iPos; };

		protected:
			void InitStream(void);

			LFA_FileRef iFile;
			XMP_Int64 iPos;
			XMP_Int64 iEndPos;
	};

	typedef enum {
		STATUS_WRITE, 
		STATUS_READ, 
		STATUS_EOF, 
		STATUS_BUFFER_OVERFLOW,
		STATUS_SKIP
	} EStatus;

	class IOException
	{
		public:
			IOException(EStatus status) : iStatus(status) {};
			~IOException(void) {};
			EStatus GetErrorCode(void) { return iStatus; };
		protected:
			EStatus iStatus;

	};

	namespace ZIP
	{
		typedef enum {DEFLATE, DEFLATE_NO} EDeflate;

		class DeflateInputStream : public FileInputStream
		{	
			public:
				DeflateInputStream(LFA_FileRef file, XMP_Int32 bufferLength);
				virtual ~DeflateInputStream();

				virtual XMP_Int32 Read(XMP_Uns8 * ioBuf, XMP_Int32 len);
				virtual XMP_Int32 Read(XMP_Uns8 * ioBuf);
				virtual void Close(void);
				virtual bool IsEOF(void);
				virtual XMP_Int64 Skip(XMP_Int64 len);
				virtual XMP_Int64 Skip(XMP_Int64 len, EDeflate deflate);
				virtual XMP_Int64 GetCurrentPos(void) { return iPos; };
				
				
			protected:
				void InitStream(void);
				z_stream iStream;
				XMP_Int32 iStatus;
				XMP_Uns8 * iBuffer;
				XMP_Int32 iBufferLength;

		};

		class ZIPException
		{
		public:
			ZIPException(XMP_Int32 err) : iErrorCode(err) {};
			~ZIPException(void) {};

			XMP_Int32 GetErrorCode(void) { return iErrorCode; };

		protected:
			XMP_Int32 iErrorCode;

		};


	} // namespace zip

} // namespace IO

namespace SWF_Support
{
	class TagData
	{
		public:
			TagData() : pos(0), len(0), id(0), offset(0), xmp(false) {}
			virtual ~TagData() {}

			// Short tag:
			// | code/length |    data     |
			// |      2      | val(length) |
			// Long tag (data > 63):
			// | code/length | length |    data     |
			// |      2      |   4    | val(length) |
			XMP_Uns64	pos;		// file offset of tag
			XMP_Uns32	len;		// length of tag data
			long		id;			// tag ID
			long		offset;		// offset of data in tag (short vs. long tag)
			bool		xmp;		// tag with XMP ?
	};

	typedef std::vector<TagData> TagVector;
	typedef TagVector::iterator TagIterator;

	class TagState
	{
		public:
			TagState() : xmpPos(0), xmpLen(0), headerSize(0),hasFileAttrTag(false), cachingFile(false), 
				hasXMP(false), xmpPacket(""), fileAttrFlags(0) {}
			virtual ~TagState() {}

			XMP_Uns64	xmpPos;
			XMP_Uns32	xmpLen;
			TagData		xmpTag;
			TagVector	tags;
			XMP_Uns32	headerSize;
			TagData		fileAttrTag;
			XMP_Uns32	fileAttrFlags;
			bool		hasFileAttrTag;
			bool		cachingFile;
			bool		hasXMP;
			std::string xmpPacket;
	};

	//compression related data types

	typedef enum swf_mode { CWS, FWS } SWF_MODE;
	typedef int (*CompressionFnc)(LFA_FileRef source, LFA_FileRef dest);
	
	class FileInfo
	{
		public:
			FileInfo(LFA_FileRef fileRef, const std::string & origPath);
			virtual ~FileInfo() {}

			bool IsCompressed();
			LFA_FileRef Decompress();
			void Compress(LFA_FileRef sourceRef, LFA_FileRef destRef);
			void Clean();
			inline XMP_Uns32 GetSize() { return iSize; }

		private:
			std::string tmpFilePath;
			std::string origFilePath;
			LFA_FileRef fileRef;
			bool compressedFile;
			XMP_Uns32 iSize;

			//tmp Data
			LFA_FileRef tmpFileRef;
			

			void CheckFormat(LFA_FileRef fileRef);
			void CleanTempFiles();
			
			int Encode ( LFA_FileRef fileRef, LFA_FileRef updateRef, SWF_MODE swfMode, CompressionFnc cmpFnc );

			static int Inf ( LFA_FileRef source, LFA_FileRef dest );
			static int Def ( LFA_FileRef source, LFA_FileRef dest );

	};
	
	long OpenSWF ( IO::InputStream *inputStream, TagState & inOutTagState );
	bool  ReadTag ( IO::InputStream * inputStream, TagState & inOutTagState, long * tagType, 
		XMP_Uns32 * tagLength, XMP_Uns64 & inOutPosition );
	unsigned long CheckTag ( IO::InputStream * inputStream, const TagState& inOutTagState, const TagData& inOutTagData );
	bool HasMetadata(IO::InputStream * inputStream, TagState& tagState);
	XMP_Uns32 ReadFileAttrFlags(IO::InputStream * inputStream);

	bool WriteXMPTag ( LFA_FileRef fileRef, XMP_Uns32 len, const char* inBuffer );
	bool CopyHeader ( LFA_FileRef sourceRef, LFA_FileRef destRef, const TagState & tagState );
	bool UpdateHeader ( LFA_FileRef fileRef  );
	bool CopyTag ( LFA_FileRef sourceRef, LFA_FileRef destRef, TagData& tag );

	bool UpdateFileAttrTag(LFA_FileRef fileRef, const TagData& fileAttrTag, const TagState& tagState);
	bool WriteFileAttrFlags(LFA_FileRef fileRef, const TagData& fileAttrTag, XMP_Uns32 flags);

	bool ReadBuffer ( LFA_FileRef fileRef, XMP_Uns64& pos, XMP_Uns32 len, XMP_Uns8* outBuffer );
	bool WriteBuffer ( LFA_FileRef fileRef, XMP_Uns64& pos, XMP_Uns32 len, const char* inBuffer );


	typedef struct TailBufferDef
	{
		XMP_Uns64 tailStartPosition;
		XMP_Uns64 writePosition;
		XMP_Uns64 tailEndPosition;

		TailBufferDef() : tailStartPosition(0), writePosition(0), tailEndPosition(0) {};
		XMP_Uns32 GetTailSize(void) { return static_cast<XMP_Uns32>(tailEndPosition - tailStartPosition); }
	} TailBufferDef;



} // namespace SWF_Support

#endif	// __SWF_Support_hpp__
