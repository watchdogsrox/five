/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : RawClipFileView.cpp
// PURPOSE : Provides a customized for picking raw clip files.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "RawClipFileView.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// game
#include "frontend/VideoEditor/DataProviders/RawClipFileDataProvider.h"
#include "replaycoordinator/ReplayCoordinator.h"

FRONTEND_OPTIMISATIONS();

CRawClipFileView::CRawClipFileView()
	: CThumbnailFileView() 
{

}

bool CRawClipFileView::initialize( CRawClipFileDataProvider& rawClipFileDataProvider )
{
	return CThumbnailFileView::initialize( rawClipFileDataProvider );
}

void CRawClipFileView::markClipAsFavourite( FileViewIndex const fileIndex, bool const isFavourite )
{
	if( uiVerifyf( isPopulated(), "CRawClipFileView::markClipAsFavourite - Attempting to access file data before populated!" ) &&
		uiVerifyf( fileIndex >= 0 && fileIndex < getFilteredFileCount(), "CRawClipFileView::markClipAsFavourite - Invalid index!" ) )
	{
		getRawClipFileDataProvider()->markClipAsFavourite( getFileDataIndex( fileIndex ), isFavourite );
	}
}

void CRawClipFileView::toggleClipFavouriteState( FileViewIndex const fileIndex )
{
	if( uiVerifyf( isPopulated(), "CRawClipFileView::toggleClipFavouriteState - Attempting to access file data before populated!" ) &&
		uiVerifyf( fileIndex >= 0 && fileIndex < getFilteredFileCount(), "CRawClipFileView::toggleClipFavouriteState - Invalid index!" ) )
	{
		getRawClipFileDataProvider()->toggleClipFavouriteState( getFileDataIndex( fileIndex ) );
	}
}

bool CRawClipFileView::isClipFavourite( FileViewIndex const fileIndex ) const
{
	bool const c_isFavourite = isPopulated() && fileIndex >= 0 && fileIndex < getFilteredFileCount() ? 
		getRawClipFileDataProvider()->isClipFavourite( getFileDataIndex( fileIndex ) ) : false;

	return c_isFavourite;
}

void CRawClipFileView::saveClipFavourites()
{
	if( isPopulated() )
	{
		getRawClipFileDataProvider()->saveClipFavourites();
	}
}

bool CRawClipFileView::isSavingFavourites()
{
	bool const c_isSaving = isPopulated() ? getRawClipFileDataProvider()->isSavingFavourites() : false;
	return c_isSaving;
}

IReplayMarkerStorage::eEditRestrictionReason CRawClipFileView::getClipRestrictions( FileViewIndex const fileIndex ) const
{
	IReplayMarkerStorage::eEditRestrictionReason restrictions = IReplayMarkerStorage::EDIT_RESTRICTION_NONE; 
	
	if (isPopulated() && fileIndex >= 0 && fileIndex < getFilteredFileCount()) 
		restrictions = getRawClipFileDataProvider()->getClipRestrictions( getFileDataIndex( fileIndex ) );

	return restrictions;
}

bool CRawClipFileView::IsValidClipReference( FileViewIndex const index ) const
{
	bool const c_result = isFileReferenceValid( index );
	return c_result;
}

u32 CRawClipFileView::getClipFileDurationMs( FileViewIndex const index ) const
{
	uiAssertf( isPopulated(), "CRawClipFileView::getClipFileDurationMs - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CRawClipFileView::getClipFileDurationMs - Invalid index!" );

	return isPopulated() && index >= 0 && index < getFilteredFileCount() ? 
		getRawClipFileDataProvider()->getExtraDataU32( getFileDataIndex( index ) ) : 0;
}

ClipUID const& CRawClipFileView::getClipUID( FileViewIndex const index ) const
{
	return uiVerifyf( isPopulated(), "CRawClipFileView::getClipUID - Attempting to access file data before populated!" ) && 
		uiVerifyf( index >= 0 && index < getFilteredFileCount(), "CRawClipFileView::getClipUID - Invalid index!" ) ? 
		getRawClipFileDataProvider()->getClipUID( getFileDataIndex( index ) ) : ClipUID::INVALID_UID;
}

CFileViewFilter::SortCompareFunc CRawClipFileView::getSortFunctionForCustomSort( int const sortType, bool const invert ) const
{
	CFileViewFilter::SortCompareFunc sortFunc = NULL;

	switch( sortType )
	{
	case SORTTYPE_USED_IN_PROJECT:
		{
			sortFunc = invert ? CRawClipFileView::RSortByUsedInProject : CRawClipFileView::SortByUsedInProject;
			break;
		}

	case SORTTYPE_FAVOURITES:
		{
			sortFunc = invert ? CRawClipFileView::RSortByFavourite : CRawClipFileView::SortByFavourite;
			break;
		}

	default:
		{
			uiAssertf( false, "Unknown sort type %d requested!", sortType );
		}
	}

	return sortFunc;
}

char const* CRawClipFileView::getLocKeyForCustomSort( int const sortType ) const
{
	int const c_translatedIndex = sortType - SORTTYPE_CUSTOM_FIRST;

	static char const * const s_customSortTypeStrings[ SORTTYPE_CUSTOM_COUNT ] =
	{
		"IB_SORT_PROJU", "IB_SORT_FAV"
	};

	char const * const lngKey = c_translatedIndex >= 0 && c_translatedIndex < SORTTYPE_CUSTOM_COUNT ? 
		s_customSortTypeStrings[ c_translatedIndex ] : "";

	return lngKey;
}

bool CRawClipFileView::isValidSortForProvider( int const sortType ) const
{
	return CThumbnailFileView::isValidSortForProvider( sortType ) || sortType == SORTTYPE_USED_IN_PROJECT || sortType == SORTTYPE_FAVOURITES;
}

CFileViewFilter::FilterFunc CRawClipFileView::getFilterFunctionForCustomFilter( int const filterType ) const
{
	CFileViewFilter::FilterFunc filterFunc = NULL;

	switch( filterType )
	{
	case FILTERTYPE_USED_IN_PROJECT:
		{
			filterFunc = FilterByUsedInProject;
			break;
		}

	case FILTERTYPE_FAVOURITES:
		{
			filterFunc = FilterByFavourited;
			break;
		}

	default:
		{
			uiAssertf( false, "Unknown filter type %d requested!", filterType );
		}
	}

	return filterFunc;
}

char const* CRawClipFileView::getLocKeyForCustomFilter( int const filterType ) const
{
	int const c_translatedIndex = filterType - FILTERTYPE_CUSTOM_FIRST;

	static char const * const s_customFilterTypeStrings[ FILTERTYPE_CUSTOM_COUNT ] =
	{
		"IB_FILT_PROJU", "IB_FILT_FAV"
	};

	char const * const lngKey = c_translatedIndex >= 0 && c_translatedIndex < FILTERTYPE_CUSTOM_COUNT ? 
		s_customFilterTypeStrings[ c_translatedIndex ] : "";

	return lngKey;
}

bool CRawClipFileView::isValidFilterForProvider( int const filterType ) const
{
	return CThumbnailFileView::isValidFilterForProvider( filterType ) || 
		filterType == FILTERTYPE_USED_IN_PROJECT || filterType == FILTERTYPE_FAVOURITES;
}

bool CRawClipFileView::FilterByUsedInProject( CFileData const * const fileData, CFileDataProvider const * const dataProvider )
{
    bool result = false;

    // Check it belongs to us first
    if( CFileViewFilter::FilterBelongsToCurrentUser( fileData, dataProvider ) )
    {
        CRawClipFileDataProvider const * const c_clipFileProvider = (CRawClipFileDataProvider const * const)dataProvider;
        ClipUID const c_clipUID = c_clipFileProvider && fileData ? c_clipFileProvider->getClipUID( *fileData ) : ClipUID::INVALID_UID;
        result = CReplayCoordinator::IsClipUsedInAnyProject( c_clipUID );
    }

    return result;
}

bool CRawClipFileView::FilterByFavourited( CFileData const * const fileData, CFileDataProvider const * const dataProvider )
{
    bool result = false;

    // Check it belongs to us first
    if( CFileViewFilter::FilterBelongsToCurrentUser( fileData, dataProvider ) )
    {
        CRawClipFileDataProvider const * const c_clipFileProvider = (CRawClipFileDataProvider const * const)dataProvider;
        result = c_clipFileProvider && fileData ? c_clipFileProvider->isClipFavourite( *fileData ) : false;
    }

	return result;
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
