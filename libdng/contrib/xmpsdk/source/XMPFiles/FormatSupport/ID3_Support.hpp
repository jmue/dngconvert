#ifndef __ID3_Support_hpp__
#define __ID3_Support_hpp__ 1

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
#include "UnicodeConversions.hpp"
#include "Reconcile_Impl.hpp"
#include <vector>

#if XMP_WinBuild
	#define stricmp _stricmp
#else
	static int stricmp ( const char * left, const char * right )	// Case insensitive ASCII compare.
	{
		char chL = *left;	// ! Allow for 0 passes in the loop (one string is empty).
		char chR = *right;	// ! Return -1 for stricmp ( "a", "Z" ).

		for ( ; (*left != 0) && (*right != 0); ++left, ++right ) {
			chL = *left;
			chR = *right;
			if ( chL == chR ) continue;
			if ( ('A' <= chL) && (chL <= 'Z') ) chL |= 0x20;
			if ( ('A' <= chR) && (chR <= 'Z') ) chR |= 0x20;
			if ( chL != chR ) break;
		}
		
		if ( chL == chR ) return 0;
		if ( chL < chR ) return -1;
		return 1;	
	}
#endif

#define MIN(a, b)       ((a) < (b) ? (a) : (b))

namespace ID3_Support 
{
	// Genres
	static char Genres[128][32]={
		"Blues",			// 0
		"Classic Rock",		// 1
		"Country",			// 2
		"Dance",
		"Disco",
		"Funk",
		"Grunge",
		"Hip-Hop",
		"Jazz",				// 8
		"Metal",
		"New Age",			// 10
		"Oldies",
		"Other",			// 12
		"Pop",
		"R&B",
		"Rap",
		"Reggae",			// 16
		"Rock",				// 17
		"Techno",
		"Industrial",
		"Alternative",
		"Ska",
		"Death Metal",
		"Pranks",
		"Soundtrack",		// 24
		"Euro-Techno",
		"Ambient",
		"Trip-Hop",
		"Vocal",
		"Jazz+Funk",
		"Fusion",
		"Trance",
		"Classical",		// 32
		"Instrumental",
		"Acid",
		"House",
		"Game",
		"Sound Clip",
		"Gospel",
		"Noise",
		"AlternRock",
		"Bass",
		"Soul",				//42
		"Punk",
		"Space",
		"Meditative",
		"Instrumental Pop",
		"Instrumental Rock",
		"Ethnic",
		"Gothic",
		"Darkwave",
		"Techno-Industrial",
		"Electronic",
		"Pop-Folk",
		"Eurodance",
		"Dream",
		"Southern Rock",
		"Comedy",
		"Cult",
		"Gangsta",
		"Top 40",
		"Christian Rap",
		"Pop/Funk",
		"Jungle",
		"Native American",
		"Cabaret",
		"New Wave",			// 66
		"Psychadelic",
		"Rave",
		"Showtunes",
		"Trailer",
		"Lo-Fi",
		"Tribal",
		"Acid Punk",
		"Acid Jazz",
		"Polka",
		"Retro",
		"Musical",
		"Rock & Roll",
		"Hard Rock",
		"Folk",				// 80
		"Folk-Rock",
		"National Folk",
		"Swing",
		"Fast Fusion",
		"Bebob",
		"Latin",
		"Revival",
		"Celtic",
		"Bluegrass",		// 89
		"Avantgarde",
		"Gothic Rock",
		"Progressive Rock",
		"Psychedelic Rock",
		"Symphonic Rock",
		"Slow Rock",
		"Big Band",
		"Chorus",
		"Easy Listening",
		"Acoustic",
		"Humour",			// 100
		"Speech",
		"Chanson",
		"Opera",
		"Chamber Music",
		"Sonata",
		"Symphony",
		"Booty Bass",
		"Primus",
		"Porn Groove",
		"Satire",
		"Slow Jam",
		"Club",
		"Tango",
		"Samba",
		"Folklore",
		"Ballad",
		"Power Ballad",
		"Rhythmic Soul",
		"Freestyle",
		"Duet",
		"Punk Rock",
		"Drum Solo",
		"A capella",
		"Euro-House",
		"Dance Hall",
		"Unknown"			// 126
	};

	//////////////////////////////////////////////////////////
	// ID3 specific helper routines
	inline XMP_Int32 synchToInt32(XMP_Uns32 rawDataBE)  // see part 6.2 of spec
	{
		XMP_Validate( 0 == (rawDataBE & 0x80808080),"input not synchsafe", kXMPErr_InternalFailure );
		XMP_Int32 r =
			( rawDataBE & 0x0000007F )
			+	( rawDataBE >> 1 & 0x00003F80 )
			+	( rawDataBE >> 2 & 0x001FC000 )	
			+	( rawDataBE >> 3 & 0x0FE00000 );
		return r;
	}

	inline XMP_Uns32 int32ToSynch(XMP_Int32 value)
	{
		XMP_Validate( 0 <= 0x0FFFFFFF, "value too big", kXMPErr_InternalFailure );
		XMP_Uns32 r =
			( (value & 0x0000007F) << 0 )
			+	( (value & 0x00003F80) << 1 )
			+	( (value & 0x001FC000) << 2 )
			+	( (value & 0x0FE00000) << 3 );
		return r;
	}

	//////////////////////////////////////////////////////////
	// data structures
	class ID3Header
	{
	public:
		const static XMP_Uns16	o_id			=		0;
		const static XMP_Uns16	o_version_major	=		3;
		const static XMP_Uns16	o_version_minor	=		4;
		const static XMP_Uns16	o_flags			=		5;
		const static XMP_Uns16	o_size			=		6;

		const static int FIXED_SIZE = 10;
		char fields[FIXED_SIZE];

		bool read(LFA_FileRef file)
		{
			LFA_Read( file, fields, FIXED_SIZE, true );

			if ( !CheckBytes( &fields[ID3Header::o_id], "ID3", 3 ) )
			{
				// chuck in default contents:
				const static char defaultHeader[FIXED_SIZE] = { 'I', 'D', '3', 3, 0, 0, 0, 0, 0, 0 };
				memcpy( this->fields, defaultHeader, FIXED_SIZE);	// NBA: implicitly protected by FIXED_SIZE design.

				return false; // no header found (o.k.) thus stick with new, default header constructed above
			}

			XMP_Uns8 major = fields[o_version_major];
			XMP_Uns8 minor = fields[o_version_minor];
			XMP_Validate( major==3 || major==4, "invalid ID3 major version", kXMPErr_BadFileFormat );
			XMP_Validate( minor != 0xFF,		"invalid ID3 minor version", kXMPErr_BadFileFormat );

			return true;
		}

		void write(LFA_FileRef file, XMP_Int64 tagSize)
		{
			XMP_Assert( tagSize < 0x10000000 ); // 256 MB limit due to synching.
			XMP_Assert( tagSize > 0x0 ); // 256 MB limit due to synching.
			PutUns32BE( ( int32ToSynch ( (XMP_Uns32)tagSize - 10 )), &this->fields[ID3Header::o_size] );

			LFA_Write( file, fields, FIXED_SIZE );
		}

		~ID3Header()
		{
			//nothing
		};
	}; // ID3Header


	class ID3v2Frame
	{
	public:
		// fixed
		const static XMP_Uns16	o_id		=		0;
		const static XMP_Uns16	o_size		=		4; // size after unsync, excludes frame header.
		const static XMP_Uns16	o_flags		=		8;

		const static int FIXED_SIZE = 10;
		char fields[FIXED_SIZE];

		XMP_Uns32 id;
		XMP_Uns16 flags;

		bool active; //default: true. flag is lowered, if another frame with replaces this one as "last meaningful frame of its kind"
		bool changed; //default: false. flag is raised, if setString() is used

		// variable-size frame content
		char* content;
		XMP_Int32 contentSize; // size of variable content, right as its stored in o_size (total size - 10)

		/**
		* a mere empty hull for reading in frames.
		*/
		ID3v2Frame():id(0),content(0),contentSize(0),flags(0),active(true),changed(false)
		{	
			memset(fields, 0, FIXED_SIZE);
		}

		/**
		*	frame constructor 
		*/
		ID3v2Frame( XMP_Uns32 id ):id(0),content(0),contentSize(0),flags(0),active(true),changed(false)
		{
			memset(fields, 0, FIXED_SIZE);

			this->id = id;
			PutUns32BE( id, &fields[o_id]);
			this->flags = 0x0000;
			PutUns16BE( flags, &fields[o_flags]);
		}

		/**
		* copy constructor:
		*/
		ID3v2Frame( const ID3v2Frame& orig)
		{
			XMP_Throw("not implemented",kXMPErr_InternalFailure);
		}

		/**
		* sets a new Content (aka property)
		*/ 
		void setFrameValue( const std::string& rawvalue, bool needDescriptor = false , bool utf16le = false, bool isXMPPRIVFrame = false, bool needEncodingByte = true )
		{
			// the thing to insert
			std::string value;
			value.erase();

			if ( isXMPPRIVFrame )
			{
				// this flag must be used exclusive:
				XMP_Assert( ! needDescriptor );
				XMP_Assert( ! utf16le );
				
				value.append( "XMP\0", 4 );
				value.append( rawvalue );
				value.append( "\0", 1  ); // final zero byte
			}
			else // not an XMP Priv frame
			{
				if (needEncodingByte) // don't add encoding byte for example for WCOP
				{
					if ( utf16le )
						value.append( "\x1", 1  ); // 'unicode encoding' (must be UTF-16, be or le)
					else
						value.append( "\x0", 1  );
				}

				if ( needDescriptor )	// language descriptor
					value.append( "eng", 3 ); // language, empty content description
				if ( utf16le )
				{
					if ( needDescriptor )
						value.append( "\xFF\xFE\0\0", 4 ); // BOM, 16-bit zero (empty descriptor)

					value.append( "\xFF\xFE", 2 );
					std::string utf16str;
					ToUTF16( (XMP_Uns8*) rawvalue.c_str(), rawvalue.size(), &utf16str, false );
					value.append( utf16str );
					value.append( "\0\0", 2 );
				}
				else  // write as plain Latin-1
				{
					std::string convertedValue; // just precaution (mostly used for plain ascii like numbers, etc)
					convertedValue.erase();
					ReconcileUtils::UTF8ToLatin1( rawvalue.c_str(), rawvalue.size(), &convertedValue );

					if ( needDescriptor )
						value.append( "\0", 1 ); // BOM, 16-bit zero (empty descriptor)
					value.append( convertedValue );
					value.append( "\0", 1  ); // final zero byte
				}	
			} // of "not an XMP Priv frame"

			this->changed = true;
			this->release();

			XMP_StringPtr valueCStr = value.c_str();
			this->contentSize = (XMP_Int32) value.size();
			XMP_Validate( this->contentSize < 0xA00000, "XMP Property exceeds 20MB in size", kXMPErr_InternalFailure );
			content = new char[ this->contentSize ];
			memcpy( content , valueCStr , this->contentSize );
		}

		void write(LFA_FileRef file, XMP_Uns8 majorVersion)
		{
			// write back size field:
			if ( majorVersion < 4 )
				PutUns32BE( contentSize, &fields[o_size] );
			else
				PutUns32BE( int32ToSynch( contentSize ), &fields[o_size] );
			
			// write out fixed fields
			LFA_Write( file, this->fields, FIXED_SIZE);
			// write out contents
			LFA_Write( file, this->content, contentSize );
		}

		/**
		* majorVersion must be 3 or 4
		* @returns total size of frame
		*/
		XMP_Int64 read(LFA_FileRef file, XMP_Uns8 majorVersion )
		{
			XMP_Assert( majorVersion == 3 || majorVersion == 4 );
			this->release(); // ensures/allows reuse of 'curFrame'

			XMP_Int64 start = LFA_Tell( file );
			LFA_Read( file, fields, FIXED_SIZE, true );

			id = GetUns32BE( &fields[o_id] );	

			//only padding to come?
			if (id == 0x00000000)
			{
				LFA_Seek( file, -10, SEEK_CUR );   //rewind 10 byte
				return 0; // zero signals, not a valid frame
			}
			//verify id to be valid (4x A-Z and 0-9)			
			XMP_Validate( 
				(( fields[0]>0x40 && fields[0]<0x5B) || ( fields[0]>0x2F && fields[0]<0x3A)) && //A-Z,0-9
				(( fields[1]>0x40 && fields[1]<0x5B) || ( fields[1]>0x2F && fields[1]<0x3A)) && 
				(( fields[2]>0x40 && fields[2]<0x5B) || ( fields[2]>0x2F && fields[2]<0x3A)) && 
				(( fields[3]>0x40 && fields[3]<0x5B) || ( fields[3]>0x2F && fields[3]<0x3A)) 
				,"invalid Frame ID", kXMPErr_BadFileFormat);

			flags = GetUns16BE( &fields[o_flags] );
			XMP_Validate( 0==(flags & 0xEE ),"invalid lower bits in frame flags",kXMPErr_BadFileFormat );

			//*** flag handling, spec :429ff aka line 431ff  (i.e. Frame should be discarded)
			//  compression and all of that..., unsynchronisation
			contentSize = ( majorVersion < 4) ? 
				GetUns32BE( &fields[o_size] )
				:	synchToInt32(GetUns32BE( &fields[o_size] ));

			XMP_Validate( contentSize >= 0, "negative frame size", kXMPErr_BadFileFormat );
			// >20MB in a single frame?, assume broken file
			XMP_Validate( contentSize < 0xA00000, "single frame exceeds 20MB", kXMPErr_BadFileFormat );

			content = new char[ contentSize ];

			LFA_Read( file, content, contentSize, true );
			XMP_Assert( (LFA_Tell( file ) - start) == FIXED_SIZE + contentSize ); //trivial
			return LFA_Tell( file ) - start;
		}

		/**
 		 * two types of COMM frames should be preserved but otherwise ignored
		 * * a 6-field long header just having 
		 *      encoding(1 byte),lang(3 bytes) and 0x00 31 (no descriptor, "1" as content")
		 *      perhaps only used to indicate client language
		 * * COMM frames whose description begins with engiTun, these are iTunes flags
		 *
		 * returns true: job done as expted
		 *         false: do not use this frame, but preserve (i.e. iTunes marker COMM frame)
		 */
		bool advancePastCOMMDescriptor(XMP_Int32& pos)
		{
				if ( (contentSize-pos) <= 3 )
					return false; // silent error, no room left behing language tag

				if ( !CheckBytes( &content[pos], "eng", 3 ) )
					return false; // not an error, but leave all non-eng tags alone...

				pos += 3; // skip lang tag
				if ( pos >= contentSize )
					return false; // silent error

				// skip past descriptor:
				while ( pos < contentSize )
					if ( content[pos++] == 0x00 )
						break;
				// skip yet-another double zero if applicable
				if ( (pos<contentSize) && (content[pos] == 0x00) )
					pos++;
			
				// check for "1" language indicator comment
				if (pos == 5) 
					if ( contentSize == 6)
						if ( GetUns16BE(&content[4]) == 0x0031 )
							return false;

				// check for iTunes tag-comment
				if ( pos > 4 ) //there's a content descriptor ( 1 + 3 + zeroTerminator)
				{
					std::string descriptor = std::string(&content[4],pos-1);
					if ( 0 == descriptor.substr(0,4).compare( "iTun" )) // begins with engiTun ?
						return false; // leave alone, then
				}

				return true; //o.k., descriptor skipped, time for the real thing.			
		}

		/*
		* returns the frame content as a proper UTF8 string
		*    * w/o the initial encoding byte
		*    * dealing with BOM's
		*
		* @returns: by value: character string with the value 
		*			as return value: false if frame is "not of intereset" despite a generally 
		*                            "interesting" frame ID, these are 
		*                                * iTunes-'marker' COMM frame
		*/
		bool getFrameValue( XMP_Uns8 majorVersion, XMP_Uns32 frameID, std::string* utf8string )
		{
			XMP_Assert( contentSize >= 0 );
			XMP_Assert( contentSize < 0xA00000 ); // more than 20 MB per Propety would be very odd...
			XMP_Assert( content != 0 );

			if ( contentSize == 0)
			{
				utf8string->erase();
				return true; // ...it is "of interest", even if empty contents.
			}

			XMP_Int32 pos = 0; // going through input string
			XMP_Uns8 encByte = 0;
			// WCOP does not have an encoding byte 
			// => for all others: use [0] as EncByte, advance pos
			if ( frameID != 0x57434F50 )
			{
				encByte = content[0];
				pos++;
			}	

			// mode specific forks (so far only COMM)
			bool commMode = (
							( frameID == 0x434F4D4D ) || // COMM
							( frameID == 0x55534C54 )    // USLT
							);

			switch (encByte)
			{
			case 0: //ISO-8859-1, 0-terminated
					{
						if (commMode)
							if (! advancePastCOMMDescriptor( pos )) // do and check result
								return false; // not a frame of interest!

						char* localPtr  = &content[pos];
						size_t localLen = contentSize - pos;
						ReconcileUtils::Latin1ToUTF8 ( localPtr, localLen, utf8string );
					}
					break;
			case 1: // Unicode, v2.4: UTF-16 (undetermined Endianess), with BOM, terminated 0x00 00
			case 2: // UTF-16BE without BOM, terminated 0x00 00
				{
					std::string tmp(this->content, this->contentSize);

					if (commMode)
						if (! advancePastCOMMDescriptor( pos )) // do and check result
							return false; // not a frame of interest!

					bool bigEndian = true;	// assume for now (if no BOM follows)
					if ( GetUns16BE(&content[pos]) == 0xFEFF )
					{
						pos += 2;
						bigEndian = true;
					}
					else if ( GetUns16BE(&content[pos]) == 0xFFFE )
					{
						pos += 2;
						bigEndian = false;
					}

					FromUTF16( (UTF16Unit*)&content[pos], (contentSize - pos) / 2, utf8string, bigEndian );
				}
				break;
			case 3: // UTF-8 unicode, terminated \0
					// swallow any BOM, just in case
					if ( (GetUns32BE(&content[pos]) & 0xFFFFFF00 ) == 0xEFBBBF00 )
						pos += 3;

					if (commMode)
						if (! advancePastCOMMDescriptor( pos )) // do and check result
							return false; // not a frame of interest!

					utf8string->assign( &content[pos], contentSize - pos );
					break;
			default:	
					XMP_Throw ( "unknown text encoding", kXMPErr_BadFileFormat); //COULDDO assume latin-1 or utf-8 as best-effort
					break;
			}
			return true;
		}

		~ID3v2Frame()
		{
			this->release();
		}

		void release()
		{
			if ( content )
				delete content;
			content = 0;
			contentSize = 0;
		}
	}; // ID3Header



	class ID3v1Tag
	{
	public:
		// fixed
		const static XMP_Uns16	o_tag		=    0; // must be "TAG"
		const static XMP_Uns16	o_title		=    3;
		const static XMP_Uns16	o_artist	=   33;
		const static XMP_Uns16	o_album		=   63;
		const static XMP_Uns16	o_year		=   93;
		const static XMP_Uns16	o_comment	=	97;
		const static XMP_Uns16	o_genre		=  127; // last by: index, or 255
		
		// optional (ID3v1.1) track number)
		const static XMP_Uns16	o_zero		=	125; //must be zero, for trackNo to be valid
		const static XMP_Uns16	o_trackNo	=	126; //trackNo

		const static int FIXED_SIZE = 128;

		/**
		* @returns returns true, if ID3v1 (or v1.1) exists, otherwise false.
		// sets XMP properties en route
		*/
		bool read( LFA_FileRef file, SXMPMeta* meta )
		{
			if ( LFA_Measure( file ) <= 128 )  // ensure sufficient room
				return false;
			LFA_Seek( file, -128, SEEK_END);

			XMP_Uns32 tagID = LFA_ReadInt32_BE( file );
			tagID = tagID & 0xFFFFFF00; // wipe 4th byte
			if ( tagID != 0x54414700 )
				return false; // must be "TAG"
			LFA_Seek( file, -1, SEEK_CUR ); //rewind 1

			/////////////////////////////////////////////////////////
			XMP_Uns8 buffer[31]; // nothing is bigger here, than 30 bytes (offsets [0]-[29])
			buffer[30]=0;		 // wipe last byte
			std::string utf8string;

			// title //////////////////////////////////////////////////////
			LFA_Read( file, buffer, 30, true );
			std::string title( (char*) buffer ); //security: guaranteed to 0-terminate after 30 bytes
			if ( ! title.empty() )
			{
				ReconcileUtils::Latin1ToUTF8 ( title.c_str(), title.size(), &utf8string );
				meta->SetLocalizedText( kXMP_NS_DC, "title", "" , "x-default" , utf8string.c_str() );				
			}
			// artist //////////////////////////////////////////////////////
			LFA_Read( file, buffer, 30, true );
			std::string artist( (char*) buffer );
			if ( ! artist.empty() )
			{
				ReconcileUtils::Latin1ToUTF8 ( artist.c_str(), artist.size(), &utf8string );
				meta->SetProperty( kXMP_NS_DM, "artist", utf8string.c_str() );				
			}
			// album //////////////////////////////////////////////////////
			LFA_Read( file, buffer, 30, true );
			std::string album( (char*) buffer );
			if ( ! album.empty() )
			{
				ReconcileUtils::Latin1ToUTF8 ( album.c_str(), album.size(), &utf8string );
				meta->SetProperty( kXMP_NS_DM, "album", utf8string.c_str() );				
			}
			// year //////////////////////////////////////////////////////
			LFA_Read( file, buffer, 4, true );
			buffer[4]=0; // ensure 0-term
			std::string year( (char*) buffer );
			if ( ! year.empty() )
			{	// should be moot for a year, but let's be safe:
				ReconcileUtils::Latin1ToUTF8 ( year.c_str(), year.size(), &utf8string );
				meta->SetProperty( kXMP_NS_XMP, "CreateDate",  utf8string.c_str() );
			}
			// comment //////////////////////////////////////////////////////
			LFA_Read( file, buffer, 30, true );
			std::string comment( (char*) buffer );
			if ( ! comment.empty() )
			{
				ReconcileUtils::Latin1ToUTF8 ( comment.c_str(), comment.size(), &utf8string );
				meta->SetProperty( kXMP_NS_DM, "logComment", utf8string.c_str() );				
			}
			// trackNo (ID3v1.1) /////////////////////////////////////////////
			if ( buffer[28] == 0 )
			{
				XMP_Uns8 trackNo = buffer[29];
				if ( trackNo > 0 ) // 0 := unset
				{
					std::string trackStr;
					SXMPUtils::ConvertFromInt( trackNo, 0, &trackStr );
					meta->SetProperty( kXMP_NS_DM, "trackNumber", trackStr.c_str() );
				}
			}
			// genre //////////////////////////////////////////////////////
			XMP_Uns8 genreNo = LFA_ReadUns8( file );
			if ( (genreNo > 0) && (genreNo < 127) ) // 0 := unset, 127 := 'unknown'
			{
				meta->SetProperty( kXMP_NS_DM, "genre", Genres[genreNo] );
			}
			
			return true; // ID3Tag found
		}

		void write( LFA_FileRef file, SXMPMeta* meta )
		{
			// move in position (extension if applicable happens by caller)
			std::string zeros( 128, '\0' );
			std::string utf8, latin1;

			LFA_Seek( file, -128, SEEK_END);
			LFA_Write( file, zeros.data(), 128 );

			// TAG /////////////////////////////////////////////
			LFA_Seek( file, -128, SEEK_END);
			LFA_WriteUns8( file, 'T' );
			LFA_WriteUns8( file, 'A' );
			LFA_WriteUns8( file, 'G' );

			// title //////////////////////////////////////////////////////
			if ( meta->GetLocalizedText( kXMP_NS_DC, "title", "", "x-default", 0, &utf8, kXMP_NoOptions ))
			{
				LFA_Seek( file, -128 + 3, SEEK_END);
				ReconcileUtils::UTF8ToLatin1( utf8.c_str(), utf8.size(), &latin1 );
				LFA_Write( file, latin1.c_str(), MIN( 30, (XMP_Int32)latin1.size() ) );
			}
			// artist //////////////////////////////////////////////////////
			if ( meta->GetProperty( kXMP_NS_DM, "artist", &utf8, kXMP_NoOptions ))
			{
				LFA_Seek( file, -128 + 33, SEEK_END);
				ReconcileUtils::UTF8ToLatin1( utf8.c_str(), utf8.size(), &latin1 );
				LFA_Write( file, latin1.c_str(), MIN( 30, (XMP_Int32)latin1.size() ) );
			}
			// album //////////////////////////////////////////////////////
			if ( meta->GetProperty( kXMP_NS_DM, "album", &utf8, kXMP_NoOptions ))
			{
				LFA_Seek( file, -128 + 63, SEEK_END);
				ReconcileUtils::UTF8ToLatin1( utf8.c_str(), utf8.size(), &latin1 );
				LFA_Write( file, latin1.c_str(), MIN( 30, (XMP_Int32)latin1.size() ) );
			}
			// year //////////////////////////////////////////////////////
			if ( meta->GetProperty( kXMP_NS_XMP, "CreateDate", &utf8, kXMP_NoOptions ))
			{
				XMP_DateTime dateTime;
				SXMPUtils::ConvertToDate( utf8, &dateTime );
				if ( dateTime.hasDate )
				{
					SXMPUtils::ConvertFromInt ( dateTime.year, "", &latin1 );
					LFA_Seek ( file, -128 + 93, SEEK_END );
					LFA_Write ( file, latin1.c_str(), MIN ( 4, (XMP_Int32)latin1.size() ) );
				}
			}
			// comment (write 30 bytes first, see truncation later) ////////////
			if ( meta->GetProperty( kXMP_NS_DM, "logComment", &utf8, kXMP_NoOptions ))
			{
				LFA_Seek ( file, -128 + 97, SEEK_END );
				ReconcileUtils::UTF8ToLatin1 ( utf8.c_str(), utf8.size(), &latin1 );
				LFA_Write ( file, latin1.c_str(), MIN ( 30, (XMP_Int32)latin1.size() ) );
			}
			// genre ////////////////////////////////////////////////////////////////
			if ( meta->GetProperty( kXMP_NS_DM, "genre", &utf8, kXMP_NoOptions ))
			{
				XMP_Uns8 genreNo = 0;

				// try to find (case insensitive) match:
				int i;
				const char* genreCString = utf8.c_str();
				for ( i=0; i < 127; ++i ) {
					if ( (strlen( genreCString ) == strlen(Genres[i])) &&  //fixing buggy stricmp behaviour on PPC
						 (stricmp( genreCString, Genres[i] ) == 0 )) {
						genreNo = i; // found
						break;
					} // if
				} // for

				LFA_Seek( file, -128 + 127, SEEK_END);
				LFA_WriteUns8( file, genreNo );
			}
			
			// trackNo ////////////////////////////////////////////////////////////
			if ( meta->GetProperty( kXMP_NS_DM, "trackNumber", &utf8, kXMP_NoOptions ))
			{
				XMP_Uns8 trackNo = 0;
				try
				{
					trackNo = (XMP_Uns8) SXMPUtils::ConvertToInt( utf8.c_str() );

					LFA_Seek( file, -128 + 125, SEEK_END);
					LFA_WriteUns8( file , 0 ); // ID3v1.1 extension
					LFA_WriteUns8( file, trackNo );
				} 	
				catch ( XMP_Error& )
				{
					// forgive, just don't set this one.
				}
			}
		}

	}; // ID3v1Tag

} // namespace ID3_Support
#endif	// __ID3_Support_hpp__
