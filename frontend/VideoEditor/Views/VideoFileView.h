/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoFileView.h
// PURPOSE : Provides a customized for picking video files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"
#include "video/VideoPlaybackSettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#ifndef VIDEO_FILE_VIEW_H_
#define VIDEO_FILE_VIEW_H_

// game
#include "frontend/VideoEditor/DataProviders/VideoFileDataProvider.h"
#include "ThumbnailFileView.h"

class CVideoFileView : protected CThumbnailFileView
{
public: // methods
	CVideoFileView();
	virtual ~CVideoFileView() {}

	virtual bool initialize( CVideoFileDataProvider& videoProjectFileDataProvider );

	//! Publicly expose functions from the base filter
	using CThumbnailFileView::markAsDelete;
	using CThumbnailFileView::toggleMarkAsDeleteState;
	using CThumbnailFileView::isMarkedAsDelete;
	using CThumbnailFileView::deleteMarkedFiles;
	using CThumbnailFileView::isDeletingMarkedFiles;
	using CThumbnailFileView::getCountOfFilesToDelete;
	using CThumbnailFileView::getThumbnailTxdName;
    using CThumbnailFileView::getThumbnailName;
    using CThumbnailFileView::canRequestThumbnails;
	using CThumbnailFileView::requestThumbnail;
	using CThumbnailFileView::isThumbnailLoaded;
	using CThumbnailFileView::isThumbnailRequested;
	using CThumbnailFileView::hasThumbnailLoadFailed;
	using CThumbnailFileView::doesThumbnailExist;
	using CThumbnailFileView::releaseThumbnail;
	using CThumbnailFileView::releaseAllThumbnails;
	using CThumbnailFileView::isInitialized;
	using CThumbnailFileView::isPopulated;
	using CThumbnailFileView::hasPopulationFailed;
	using CThumbnailFileView::shutdown;
	using CThumbnailFileView::update;
	using CThumbnailFileView::getUnfilteredFileCount;
	using CThumbnailFileView::getFilteredFileCount;
	using CThumbnailFileView::getCountOfFilesForFilter;
	using CThumbnailFileView::getIndexForFileName;
	using CThumbnailFileView::getIndexForDisplayName;
	using CThumbnailFileView::getDisplayName;
	using CThumbnailFileView::getFileName;
	using CThumbnailFileView::getFileSize;
	using CThumbnailFileView::getOwnerId;
	using CThumbnailFileView::getFileDateString;
	using CThumbnailFileView::getFilePath;
	using CThumbnailFileView::isFileReferenceValid;
	using CThumbnailFileView::doesFileExistInProvider;
	using CThumbnailFileView::doesFileExist;
	using CThumbnailFileView::applySort;
	using CThumbnailFileView::calculateNextSortType;
	using CThumbnailFileView::incrementSortType;
	using CThumbnailFileView::invertSort;
	using CThumbnailFileView::getSortTypeLocKey;
	using CThumbnailFileView::getCurrentSortTypeLocKey;
	using CThumbnailFileView::getNextSortTypeLocKey;
	using CThumbnailFileView::GetCurrentSortType;
	using CThumbnailFileView::IsSortInverted;
	using CThumbnailFileView::applyFilter;
	using CThumbnailFileView::calculateNextFilterType;
	using CThumbnailFileView::incrementFilterType;
	using CThumbnailFileView::getFilterTypeLocKey;
	using CThumbnailFileView::GetCurrentFilterType;
	using CThumbnailFileView::applyFilterAndSort;
	using CThumbnailFileView::startDeleteFile;
	using CThumbnailFileView::canDelete;
	using CThumbnailFileView::isDeletingFile;
	using CThumbnailFileView::hasDeleteFailed;
	using CThumbnailFileView::hasDeleteFinished;
	using CThumbnailFileView::refresh;
	using CThumbnailFileView::isValidIndex;
	using CThumbnailFileView::getFileDataIndex;
	using CThumbnailFileView::getFileViewIndex;

	bool IsValidVideoReference( FileViewIndex const index ) const;
	u32 getVideoDurationMs( FileViewIndex const index ) const;

#if RSG_ORBIS
	void TempAddVideoDisplayName(const char* title);
	s64 getVideoContentId( FileViewIndex const index ) const;
	virtual bool isValidFilterForProvider( int const filterType ) const;
#endif

protected:
	CVideoFileDataProvider * getVideoFileDataProvider() { return static_cast<CVideoFileDataProvider*>( getThumbnailFileDataProvider() ); }
	CVideoFileDataProvider const * getVideoFileDataProvider() const { return static_cast<CVideoFileDataProvider const *>( getThumbnailFileDataProvider() ); }
};

#endif // VIDEO_FILE_VIEW_H_

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
