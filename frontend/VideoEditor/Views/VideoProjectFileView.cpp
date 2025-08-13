/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoProjectFileView.cpp
// PURPOSE : Provides a customized for picking video project files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoProjectFileView.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// game
#include "frontend/VideoEditor/DataProviders/VideoProjectFileDataProvider.h"

FRONTEND_OPTIMISATIONS();

CVideoProjectFileView::CVideoProjectFileView()
	: CThumbnailFileView() 
{

}

bool CVideoProjectFileView::initialize( CVideoProjectFileDataProvider& videoProjectFileDataProvider )
{
	return CThumbnailFileView::initialize( videoProjectFileDataProvider );
}

bool CVideoProjectFileView::IsValidProjectReference( FileViewIndex const index ) const
{
	bool const c_result = isFileReferenceValid( index );
	return c_result;
}

u32 CVideoProjectFileView::getProjectFileDurationMs( FileViewIndex const index ) const
{
	uiAssertf( isPopulated(), "CVideoProjectFileView::getProjectFileDurationMs - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CVideoProjectFileView::getProjectFileDurationMs - Invalid index!" );

	return isPopulated() && index >= 0 && index < getFilteredFileCount() ? 
		getVideoProjectFileDataProvider()->getExtraDataU32( getFileDataIndex( index ) ) : 0;
}

u32 CVideoProjectFileView::getMaxProjectCount() const
{
	uiAssertf( isPopulated(), "CVideoProjectFileView::getMaxProjectCount - Attempting to access file data before populated!" );

	return isPopulated() ? getVideoProjectFileDataProvider()->getMaxProjectCount() : 0;
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
