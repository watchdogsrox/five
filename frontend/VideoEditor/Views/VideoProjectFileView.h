/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoProjectFileView.h
// PURPOSE : Provides a customized for picking video project files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_PROJECT_FILE_VIEW_H_
#define VIDEO_PROJECT_FILE_VIEW_H_

// game
#include "frontend/VideoEditor/DataProviders/VideoProjectFileDataProvider.h"
#include "ThumbnailFileView.h"

class CVideoProjectFileView : protected CThumbnailFileView
{
public: // methods
	CVideoProjectFileView();
	virtual ~CVideoProjectFileView() {}

	virtual bool initialize( CVideoProjectFileDataProvider& videoProjectFileDataProvider );

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

	bool IsValidProjectReference( FileViewIndex const index ) const;
	u32 getProjectFileDurationMs( FileViewIndex const index ) const;

	u32 getMaxProjectCount() const;

protected:
	CVideoProjectFileDataProvider * getVideoProjectFileDataProvider() { return static_cast<CVideoProjectFileDataProvider*>( getThumbnailFileDataProvider() ); }
	CVideoProjectFileDataProvider const * getVideoProjectFileDataProvider() const { return static_cast<CVideoProjectFileDataProvider const *>( getThumbnailFileDataProvider() ); }
};

#endif // VIDEO_PROJECT_FILE_VIEW_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
