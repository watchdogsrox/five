/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ThumbnailFileDataProvider.h
// PURPOSE : Provides access to thumbnail image loading/access for files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef THUMBNAIL_FILE_DATA_PROVIDER_H_
#define THUMBNAIL_FILE_DATA_PROVIDER_H_

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "atl/array.h"
#include "atl/hashstring.h"

// game
#include "FileDataProvider.h"

namespace rage
{
	class IImageLoader;
}

class CThumbnailFileDataProvider : public CFileDataProvider
{
public:
	CThumbnailFileDataProvider( rage::IImageLoader& imageLoader );
	virtual ~CThumbnailFileDataProvider();

	bool getThumbnailName( FileDataIndex const fileIndex, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	atFinalHashString getThumbnailName( FileDataIndex const fileIndex ) const;

    bool canRequestThumbnails() const;
	virtual bool requestThumbnail( FileDataIndex const fileIndex );
	bool isThumbnailLoaded( FileDataIndex const fileIndex ) const;
	bool isThumbnailRequested( FileDataIndex const fileIndex ) const;
	bool hasThumbnailLoadFailed( FileDataIndex const fileIndex ) const;
	void releaseThumbnail( FileDataIndex const fileIndex );
	virtual void releaseAllThumbnails();

	bool doesThumbnailExist( FileDataIndex const fileIndex ) const;

protected: // declarations and variables
	rage::IImageLoader&		m_imageLoader;

private: // methods
	virtual void preCleanup();

	virtual bool generateThumbnailPath( char const * const in_path, char const * const in_fileName, 
											char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] ) const = 0;
};

#endif // THUMBNAIL_FILE_DATA_PROVIDER_H_
