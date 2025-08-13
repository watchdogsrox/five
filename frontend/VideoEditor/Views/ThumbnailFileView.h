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
#ifndef THUMBNAIL_FILE_VIEW_H_
#define THUMBNAIL_FILE_VIEW_H_

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "atl/hashstring.h"

// game
#include "frontend/VideoEditor/DataProviders/ThumbnailFileDataProvider.h"
#include "frontend/VideoEditor/Views/FileViewFilter.h"

class CThumbnailFileView : protected CFileViewFilter
{
public: // methods
	CThumbnailFileView();
	virtual ~CThumbnailFileView() {}

	bool initialize( CThumbnailFileDataProvider& thumbnailFileDataProvider );

	virtual bool getThumbnailTxdName( FileViewIndex const fileIndex, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	virtual atFinalHashString getThumbnailTxdName( FileViewIndex const fileIndex ) const;

	bool getThumbnailName( FileViewIndex const fileIndex, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	atFinalHashString getThumbnailName( FileViewIndex const fileIndex ) const;

    bool canRequestThumbnails() const;

	bool requestThumbnail( FileViewIndex const fileIndex );
	bool isThumbnailLoaded( FileViewIndex const fileIndex ) const;
	bool isThumbnailRequested( FileViewIndex const fileIndex ) const;
	bool hasThumbnailLoadFailed( FileViewIndex const fileIndex ) const;
	bool doesThumbnailExist( FileViewIndex const fileIndex ) const;
	void releaseThumbnail( FileViewIndex const fileIndex );
	void releaseAllThumbnails();

	//! Publicly expose functions from the base filter
	using CFileViewFilter::isInitialized;
	using CFileViewFilter::isPopulated;
	using CFileViewFilter::hasPopulationFailed;
	using CFileViewFilter::update;
	using CFileViewFilter::shutdown;
	using CFileViewFilter::getUnfilteredFileCount;
	using CFileViewFilter::getFilteredFileCount;
	using CFileViewFilter::getCountOfFilesForFilter;
	using CFileViewFilter::getIndexForFileName;
	using CFileViewFilter::getIndexForDisplayName;
	using CFileViewFilter::getDisplayName;
	using CFileViewFilter::getFileName;
	using CFileViewFilter::getFileSize;
	using CFileViewFilter::getOwnerId;
	using CFileViewFilter::getFileDateString;
	using CFileViewFilter::getFilePath;
	using CFileViewFilter::isFileReferenceValid;
	using CFileViewFilter::doesFileExistInProvider;
	using CFileViewFilter::doesFileExist;
	using CFileViewFilter::applySort;
	using CFileViewFilter::calculateNextSortType;
	using CFileViewFilter::incrementSortType;
	using CFileViewFilter::invertSort;
	using CFileViewFilter::getSortTypeLocKey;
	using CFileViewFilter::getCurrentSortTypeLocKey;
	using CFileViewFilter::getNextSortTypeLocKey;
	using CFileViewFilter::GetCurrentSortType;
	using CFileViewFilter::IsSortInverted;
	using CFileViewFilter::applyFilter;
	using CFileViewFilter::calculateNextFilterType;
	using CFileViewFilter::incrementFilterType;
	using CFileViewFilter::getFilterTypeLocKey;
	using CFileViewFilter::GetCurrentFilterType;
	using CFileViewFilter::applyFilterAndSort;
	using CFileViewFilter::startDeleteFile;
	using CFileViewFilter::canDelete;
	using CFileViewFilter::isDeletingFile;
	using CFileViewFilter::hasDeleteFailed;
	using CFileViewFilter::hasDeleteFinished;
	using CFileViewFilter::refresh;
	using CFileViewFilter::isValidIndex;
	using CFileViewFilter::getFileDataIndex;
	using CFileViewFilter::getFileViewIndex;

protected:
	CThumbnailFileDataProvider * getThumbnailFileDataProvider() { return static_cast<CThumbnailFileDataProvider*>( getFileDataProvider() ); }
	CThumbnailFileDataProvider const * getThumbnailFileDataProvider() const { return static_cast<CThumbnailFileDataProvider const *>( getFileDataProvider() ); }
};

#endif // THUMBNAIL_FILE_VIEW_H_
