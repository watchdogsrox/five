/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ThumbnailFileView.cpp
// PURPOSE : Provides a customized for getting thumbnails images for files.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

//! TODO - Move this outside of Replay as not replay specific...

// game
#include "ThumbnailFileView.h"

FRONTEND_OPTIMISATIONS();

CThumbnailFileView::CThumbnailFileView()
	: CFileViewFilter() 
{

}

bool CThumbnailFileView::initialize( CThumbnailFileDataProvider& thumbnailFileDataProvider )
{
	return CFileViewFilter::initialize( thumbnailFileDataProvider );
}

bool CThumbnailFileView::getThumbnailTxdName( FileViewIndex const fileIndex, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	return getThumbnailName( fileIndex, out_buffer );
}

rage::atFinalHashString CThumbnailFileView::getThumbnailTxdName( FileViewIndex const fileIndex ) const
{
	return getThumbnailName( fileIndex );
}

bool CThumbnailFileView::getThumbnailName( FileViewIndex const fileIndex, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	bool success = false;

	CThumbnailFileDataProvider const * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider )
	{
		success = thumbnailProvider->getThumbnailName( getFileDataIndex( fileIndex ), out_buffer );
	}

	return success;
}

atFinalHashString CThumbnailFileView::getThumbnailName( FileViewIndex const fileIndex ) const
{
	atFinalHashString name;

	CThumbnailFileDataProvider const * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider )
	{
		name = thumbnailProvider->getThumbnailName( getFileDataIndex( fileIndex ) );
	}

	return name;
}

bool CThumbnailFileView::canRequestThumbnails() const
{
    CThumbnailFileDataProvider const * thumbnailProvider = getThumbnailFileDataProvider();
    bool const c_canRequest = thumbnailProvider && thumbnailProvider->canRequestThumbnails();
    return c_canRequest;
}

// PURPOSE: Request that we load a thumbnail for the given file index and add it to our texture dictionary under the given name.
// PARAMS:
//		fileIndex - File we want the thumbnail of
// RETURNS:
//		True if successful, false otherwise
bool CThumbnailFileView::requestThumbnail( FileViewIndex const fileIndex )
{
	bool success = false;

	CThumbnailFileDataProvider * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider && isFileReferenceValid( fileIndex ) )
	{
		success = thumbnailProvider->requestThumbnail( getFileDataIndex( fileIndex ) );
	}

	return success;
}

bool CThumbnailFileView::isThumbnailLoaded( FileViewIndex const fileIndex ) const
{
	bool loaded = false;

	CThumbnailFileDataProvider const * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider )
	{
		loaded = thumbnailProvider->isThumbnailLoaded( getFileDataIndex( fileIndex ) );
	}

	return loaded;
}

bool CThumbnailFileView::isThumbnailRequested( FileViewIndex const fileIndex ) const
{
	bool requested = false;

	CThumbnailFileDataProvider const * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider )
	{
		requested = thumbnailProvider->isThumbnailRequested( getFileDataIndex( fileIndex ) );
	}

	return requested;
}

bool CThumbnailFileView::hasThumbnailLoadFailed( FileViewIndex const fileIndex ) const
{
	bool loadedFailed = false;

	CThumbnailFileDataProvider const * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider )
	{
		loadedFailed = thumbnailProvider->hasThumbnailLoadFailed( getFileDataIndex( fileIndex ) );
	}

	return loadedFailed;
}

bool CThumbnailFileView::doesThumbnailExist( FileViewIndex const fileIndex ) const
{
	bool loadedFailed = false;

	CThumbnailFileDataProvider const* thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider )
	{
		loadedFailed = thumbnailProvider->doesThumbnailExist( getFileDataIndex( fileIndex ) );
	}

	return loadedFailed;
}

void CThumbnailFileView::releaseThumbnail( FileViewIndex const fileIndex )
{
	CThumbnailFileDataProvider * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider && isInitialized() )
	{
		thumbnailProvider->releaseThumbnail( getFileDataIndex( fileIndex ) );
	}
}


void CThumbnailFileView::releaseAllThumbnails()
{
	CThumbnailFileDataProvider * thumbnailProvider = getThumbnailFileDataProvider();
	if( thumbnailProvider && isInitialized() )
	{
		thumbnailProvider->releaseAllThumbnails();
	}
}
