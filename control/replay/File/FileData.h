/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FileData.h
// PURPOSE : Simple class for common file data used by higher level systems.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef FILE_DATA_H_
#define FILE_DATA_H_

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "string/string.h"
#include "system/nelem.h"

// Framework Includes
#include "fwlocalisation/templateString.h"
#include "fwlocalisation/textTypes.h"
#include "fwlocalisation/textUtil.h"

namespace rage
{
	class fiDevice;
}

// TODO - Refactor all this outside of replay (and outside of this class) to be shared with anywhere
//        else we want to support listing user-generated local files
#define MAX_FILEDATA_FILENAME_BUFFER_SIZE (64)
#define MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE ( MAX_FILEDATA_FILENAME_BUFFER_SIZE - 4 )
#define MAX_FILEDATA_INPUT_FILENAME_LENGTH ( ( MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE - 1 ) / 3 )
#define MAX_FILEDATA_INPUT_FILENAME_LENGTH_WITH_TERMINATOR ( MAX_FILEDATA_INPUT_FILENAME_LENGTH + 1 )

// TODO - Refactor replay to save as ".clp" rather than ".clip" so we can guarantee 4-bytes for extension?

typedef rage::fwTemplateString< MAX_FILEDATA_FILENAME_BUFFER_SIZE > FileName;
typedef rage::fwTemplateString< MAX_FILEDATA_INPUT_FILENAME_BUFFER_SIZE > DisplayName;

class CFileData 
{
public:
	typedef union
	{ 
		float m_floatData; 
		u32 m_u32Data;
	} ExtraData; // TODO - If we need lots of extra data, consider pImpl rather than union of types.

	CFileData()
		: m_fileName()
		, m_displayName()
		, m_lastWriteTime( 0 )
		, m_size( 0 )
		, m_userId( 0 )
	{
		
	}

	bool operator==( CFileData const& other )
	{
		return IsEqual( other );
	}

	bool operator!=( CFileData const& other )
	{
		return !IsEqual( other );
	}

	inline bool IsEqual( CFileData const& other )
	{
		bool const c_same = m_lastWriteTime == other.m_lastWriteTime &&
								m_size == other.m_size &&
									m_fileName == other.m_fileName &&
										m_displayName == other.m_displayName;

		return c_same;
	}

	inline void setFilename( char const * const filename )
	{
		m_fileName.Set( filename );
	}
	inline char const * getFilename() const { return m_fileName.getBuffer(); }

    inline static size_t getMaxFilenameSizeBytes() { return FileName::GetMaxByteCount(); }

	inline void setDisplayName( char const * const displayName )
	{
		m_displayName.Set( displayName );
	}
	inline char const * getDisplayName() const { return m_displayName.getBuffer(); }

    inline static size_t getMaxDisplayNameSizeBytes() { return DisplayName::GetMaxByteCount(); }

	inline void setLastWriteTime( u64 lastWriteTime ) { m_lastWriteTime = lastWriteTime; }
	inline u64 getLastWriteTime() const { return m_lastWriteTime; }

	inline void setSize( u64 size ) { m_size = size; }
	inline u64 getSize() const { return m_size; }

	inline void setUserId( u64 userId ) { m_userId = userId; }
	inline u64 getUserId() const { return m_userId; }

	inline void setExtraDataU32( u32 const extraData ) { m_extraData.m_u32Data = extraData; }
	inline u32 getExtraDataU32() const { return m_extraData.m_u32Data; }

	inline void setExtraDataFloat( float const extraData ) { m_extraData.m_floatData = extraData; }
	inline float getExtraDataFloat() const { return m_extraData.m_floatData; }

	static int CompareLastWriteTime( CFileData const* data1, CFileData const* data2 )
	{
		u64 const c_writeTime1 = data1 ? data1->getLastWriteTime() : 0;
		u64 const c_writeTime2 = data2 ? data2->getLastWriteTime() : 0;

		// Last write time could be equal, so sort by name if found to be equal
		int const c_result =  c_writeTime1 > c_writeTime2 ? 1 : c_writeTime1 < c_writeTime2 ? -1 : 0;
		return c_result == 0 ? CompareName( data1, data2 ) : c_result;
	}

	static int CompareFileSize( CFileData const* data1, CFileData const* data2 )
	{
		u64 const c_size1 = data1 ? data1->getSize() : 0;
		u64 const c_size2 = data2 ? data2->getSize() : 0;

		// File size can be equal, so also sort by name if found to be equal
		int const c_result = c_size1 > c_size2 ? 1 : c_size1 < c_size2 ? -1 : 0;
		return c_result == 0 ? CompareName( data1, data2 ) : c_result;
	}

	static int CompareName( CFileData const* data1, CFileData const* data2 )
	{
		return ( data1 && data2 ) ? CompareAString( data1->getFilename(), data2->getFilename() ) : 0;
	}

	static int CompareAString( char const * const data1, char const * const data2 )
	{
		return ( data1 && data2 ) ? stricmp( data1, data2 ) : 0;
	}

private: 
	FileName    m_fileName;
	DisplayName m_displayName;
	u64			m_lastWriteTime;
	u64			m_size;
	u64			m_userId;
	ExtraData	m_extraData;
};

#if RSG_ORBIS
class CFileDataVideo
{
public:
	CFileDataVideo()
		: m_path()
		, m_thumbnail()
		, m_contentId( 0 )
	{

	}
	inline void setPath( char const * const path )
	{
		m_path.Set( path );
	}
	inline char const * getPath() const { return m_path.getBuffer(); }

	inline void setThumbnail( char const * const thumbnail )
	{
		m_thumbnail.Set( thumbnail );
	}
	inline char const * getThumbnail() const { return m_thumbnail.getBuffer(); }

	inline void setContentId( s64 contentId ) { m_contentId = contentId; }
	inline s64 getContentId() const { return m_contentId; }

private: 
	fwTemplateString<RAGE_MAX_PATH> m_path;
	fwTemplateString<RAGE_MAX_PATH> m_thumbnail;
	s64 m_contentId;

};
#endif

#endif // FILE_DATA_H_
