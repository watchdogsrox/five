/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : RawClipFileView.h
// PURPOSE : Provides a customized for raw clip files.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef RAW_CLIP_FILE_VIEW_H_
#define RAW_CLIP_FILE_VIEW_H_

// framework
#include "fwlocalisation/templateString.h"

// game
#include "frontend/VideoEditor/DataProviders/FileDataProvider.h"
#include "frontend/VideoEditor/DataProviders/RawClipFileDataProvider.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "ThumbnailFileView.h"

class CRawClipFileView : protected CThumbnailFileView
{
public: // Declarations and variables

	enum eClipSortType
	{
		SORTTYPE_CUSTOM_FIRST = CFileViewFilter::SORTTYPE_CUSTOM,

		SORTTYPE_USED_IN_PROJECT = SORTTYPE_CUSTOM_FIRST,
		SORTTYPE_FAVOURITES,

		SORTTYPE_CUSTOM_MAX,

		SORTTYPE_CUSTOM_COUNT = SORTTYPE_CUSTOM_MAX - SORTTYPE_CUSTOM_FIRST
	};

	enum eClipFilterType
	{
		FILTERTYPE_CUSTOM_FIRST = CFileViewFilter::FILTERTYPE_CUSTOM,

		FILTERTYPE_USED_IN_PROJECT = FILTERTYPE_CUSTOM_FIRST,
		FILTERTYPE_FAVOURITES,

		FILTERTYPE_CUSTOM_MAX,

		FILTERTYPE_CUSTOM_COUNT = FILTERTYPE_CUSTOM_MAX - FILTERTYPE_CUSTOM_FIRST
	};

public: // methods
	CRawClipFileView();
	virtual ~CRawClipFileView() {}

	bool initialize( CRawClipFileDataProvider& rawClipFileDataProvider );

	void markClipAsFavourite( FileViewIndex const fileIndex, bool const isFavourite );
	void toggleClipFavouriteState( FileViewIndex const fileIndex );
	bool isClipFavourite( FileViewIndex const fileIndex ) const;

	void saveClipFavourites();
	bool isSavingFavourites();

	IReplayMarkerStorage::eEditRestrictionReason getClipRestrictions( FileViewIndex const fileIndex ) const;

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

	bool IsValidClipReference( FileViewIndex const index ) const;
	u32 getClipFileDurationMs( FileViewIndex const index ) const;
	ClipUID const& getClipUID( FileViewIndex const index ) const;

protected: // methods
	CRawClipFileDataProvider * getRawClipFileDataProvider() { return static_cast<CRawClipFileDataProvider*>( getThumbnailFileDataProvider() ); }
	CRawClipFileDataProvider const * getRawClipFileDataProvider() const { return static_cast<CRawClipFileDataProvider const *>( getThumbnailFileDataProvider() ); }

	// Derived sorting
	virtual SortCompareFunc getSortFunctionForCustomSort( int const sortType, bool const invert ) const;
	virtual char const*		getLocKeyForCustomSort( int const sortType ) const;
	virtual bool			isValidSortForProvider( int const sortType ) const;
	virtual int				getMaxSortValue() const	{ return SORTTYPE_CUSTOM_MAX; }

	// Derived filtering
	virtual FilterFunc		getFilterFunctionForCustomFilter( int const filterType ) const;
	virtual char const*		getLocKeyForCustomFilter( int const filterType ) const;
	virtual bool			isValidFilterForProvider( int const filterType ) const;
	virtual int				getMaxFilterValue() const { return FILTERTYPE_CUSTOM_MAX; }


private: // methods
	template< bool _inverse >
	static int _SortByUsedInProject( CFileViewFilter::CFileViewEntry const * data1, CFileViewFilter::CFileViewEntry const * data2 )
	{
		int result = 0;

		CFileViewFilter::CFileViewEntry const* entry1 = (CFileViewFilter::CFileViewEntry*)data1;
		CFileViewFilter::CFileViewEntry const* entry2 = (CFileViewFilter::CFileViewEntry*)data2;

		CRawClipFileDataProvider const* provider1 = entry1 ? (CRawClipFileDataProvider const*)entry1->getFileDataProvider() : NULL;
		CRawClipFileDataProvider const* provider2 = entry2 ? (CRawClipFileDataProvider const*)entry2->getFileDataProvider() : NULL;

		if( provider1 && provider2 && provider1 == provider2 )
		{
			CFileDataProvider::FileDataIndex const index1 = entry1->getFileDataIndex();
			CFileDataProvider::FileDataIndex const index2 = entry2->getFileDataIndex();

			ClipUID const& uid1 = provider1->getClipUID( index1 );
			ClipUID const& uid2 = provider2->getClipUID( index2 );

			bool const c_isUsed1 = CReplayCoordinator::IsClipUsedInAnyProject( uid1 );
			bool const c_isUsed2 = CReplayCoordinator::IsClipUsedInAnyProject( uid2 );

			// If both are equal, sort by name
			result = ( c_isUsed1 == c_isUsed2 ) ? 
				CFileData::CompareName( provider1->getFileData( index1 ), provider2->getFileData( index2 ) ) : c_isUsed1 ? 1 : -1;

		}

		return ( _inverse && result != 0 ) ? result : -result;
	}

	static int SortByUsedInProject( CFileViewFilter::CFileViewEntry const * data1, CFileViewFilter::CFileViewEntry const * data2 )
	{
		return _SortByUsedInProject<false>( data1, data2 );
	}

	static int RSortByUsedInProject( CFileViewFilter::CFileViewEntry const * data1, CFileViewFilter::CFileViewEntry const * data2 )
	{
		return _SortByUsedInProject<true>( data1, data2 );
	}

	template< bool _inverse >
	static int _SortByFavourite( CFileViewFilter::CFileViewEntry const * data1, CFileViewFilter::CFileViewEntry const * data2 )
	{
		int result = 0;

		CFileViewFilter::CFileViewEntry const* entry1 = (CFileViewFilter::CFileViewEntry*)data1;
		CFileViewFilter::CFileViewEntry const* entry2 = (CFileViewFilter::CFileViewEntry*)data2;

		CRawClipFileDataProvider const* provider1 = entry1 ? (CRawClipFileDataProvider*)entry1->getFileDataProvider() : NULL;
		CRawClipFileDataProvider const* provider2 = entry2 ? (CRawClipFileDataProvider*)entry2->getFileDataProvider() : NULL;

		if( provider1 && provider2 && provider1 == provider2 )
		{
			CFileDataProvider::FileDataIndex const index1 = entry1->getFileDataIndex();
			CFileDataProvider::FileDataIndex const index2 = entry2->getFileDataIndex();

			bool const c_isFavourite1 = provider1->isClipFavourite( index1 );
			bool const c_isFavourite2 = provider2->isClipFavourite( index2 );

			// If both are equal, sort by name
			result = ( c_isFavourite1 == c_isFavourite2 ) ? 
				CFileData::CompareName( provider1->getFileData( index1 ), provider2->getFileData( index2 ) ) : c_isFavourite1 ? 1 : -1;

		}

		return ( _inverse && result != 0 ) ? result : -result;
	}

	static int SortByFavourite( CFileViewFilter::CFileViewEntry const * data1, CFileViewFilter::CFileViewEntry const * data2 )
	{
		return _SortByFavourite<false>( data1, data2 );
	}

	static int RSortByFavourite( CFileViewFilter::CFileViewEntry const * data1, CFileViewFilter::CFileViewEntry const * data2 )
	{
		return _SortByFavourite<true>( data1, data2 );
	}

	static bool FilterByUsedInProject( CFileData const * const fileData, CFileDataProvider const * const dataProvider );

	static bool FilterByFavourited( CFileData const * const fileData, CFileDataProvider const * const dataProvider );
};

#endif // RAW_CLIP_FILE_VIEW_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
