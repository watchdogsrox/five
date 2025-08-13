/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoFileView.cpp
// PURPOSE : Provides a customized for picking video files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoFileView.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

// framework
#include "video/VideoPlaybackThumbnailManager.h"

// game
#include "frontend/VideoEditor/DataProviders/VideoProjectFileDataProvider.h"

FRONTEND_OPTIMISATIONS();

CVideoFileView::CVideoFileView()
	: CThumbnailFileView() 
{

}

bool CVideoFileView::initialize( CVideoFileDataProvider& videoFileDataProvider )
{
	return CThumbnailFileView::initialize( videoFileDataProvider );
}

bool CVideoFileView::IsValidVideoReference( FileViewIndex const index ) const
{
	bool const c_result = isFileReferenceValid( index );
	return c_result;
}

u32 CVideoFileView::getVideoDurationMs( FileViewIndex const index ) const
{
	uiAssertf( isPopulated(), "CVideoFileView::getVideoDurationMs - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CVideoFileView::getVideoDurationMs - Invalid index!" );

	return isPopulated() && index >= 0 && index < getFilteredFileCount() ? 
		getVideoFileDataProvider()->GetVideoDurationMs( getFileDataIndex( index ) ) : 0;
}

#if RSG_ORBIS
void CVideoFileView::TempAddVideoDisplayName(const char* title)
{
	getVideoFileDataProvider()->TempAddVideoDisplayName( title );
}

s64 CVideoFileView::getVideoContentId( FileViewIndex const index ) const
{
	uiAssertf( isPopulated(), "CVideoFileView::getVideoContentId - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CVideoFileView::getVideoContentId - Invalid index!" );

	return isPopulated() && index >= 0 && index < getFilteredFileCount() ? 
		getVideoFileDataProvider()->GetVideoContentId( getFileDataIndex( index ) ) : 0;
}

bool CVideoFileView::isValidFilterForProvider( int const filterType ) const
{
	bool const c_isValid = CThumbnailFileView::isValidFilterForProvider( filterType ) && 
		filterType != FILTERTYPE_OTHER_USER && filterType != FILTERTYPE_ANY_USER; // orbis doesn't need this filter for video

	return c_isValid;
}

#endif // RSG_ORBIS

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
