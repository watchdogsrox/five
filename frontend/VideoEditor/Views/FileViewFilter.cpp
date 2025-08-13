/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FileViewFilter.h
// PURPOSE : Provides a sortable view onto a collection of files.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "atl/string.h"

// framework
#include "fwutil/xmacro.h"

// game
#include "FileViewFilter.h"

FRONTEND_OPTIMISATIONS();

bool CFileViewFilter::initialize( CFileDataProvider& fileDataProvider )
{
	uiAssertf( !isInitialized(), "CFileViewFilter::initialize - Double initialization. Logic error?" );
	if( !isInitialized() )
	{
		m_fileDataProvider = &fileDataProvider;
	}

	return isInitialized();
}

bool CFileViewFilter::isInitialized() const
{ 
	return m_fileDataProvider && m_fileDataProvider->isInitialized(); 
}

bool CFileViewFilter::isPopulated() const
{
	return isInitialized() && m_fileDataProvider->isPopulated() && m_populated;
}

bool CFileViewFilter::hasPopulationFailed() const
{
	return isInitialized() && m_fileDataProvider->hasPopulationFailed();
}

void CFileViewFilter::update( bool allowSortOnPopulate )
{
	if( isInitialized() )
	{
		if( m_fileDataProvider->isPopulated() )
		{
			if( !m_populated )
			{
				BuildViewInternal();

				m_populated = true;

				if( allowSortOnPopulate )
				{
					applySort( DEFAULT_SORT_TYPE, DEFAULT_SORT_INVERSE );
				}
			}

			if( isDeletingMarkedFiles() )
			{
				if( m_fileDataProvider->hasDeleteMarkedFilesFailed() )
				{
					m_deleteState = DELETE_FAILED;
				}
				else if( m_fileDataProvider->hasDeleteMarkedFilesFinished() )
				{
					m_deleteState = DELETE_COMPLETE; 
					refresh( true );
				}
			}
			else if( isDeletingFile() )
			{
				if( m_fileDataProvider->hasDeleteFileFailed() )
				{
					m_deleteState = DELETE_FAILED;
				}
				else if( m_fileDataProvider->hasDeleteFileFinished() )
				{
					m_deleteState = DELETE_COMPLETE;
					refresh( true );
				}
			}
		}
		else if( m_fileDataProvider->hasPopulationFailed() )
		{
			//! If enumeration failed, assume we are populated with zero results
			m_populated = true;
		}
	}
}

void CFileViewFilter::shutdown()
{
	m_fileViews.Reset();
	m_fileDataProvider = NULL;	
	m_currentSort = SORTTYPE_NONE;
	m_currentFilter = DEFAULT_FILTER_TYPE;
	m_populated = false;
}

void CFileViewFilter::markAsDelete( FileViewIndex const fileIndex, bool const isDelete )
{
	if( uiVerifyf( isPopulated(), "CRawClipFileView::markClipAsFavourite - Attempting to access file data before populated!" ) &&
		uiVerifyf( fileIndex >= 0 && fileIndex < getFilteredFileCount(), "CRawClipFileView::markClipAsFavourite - Invalid index!" ) )
	{
		getFileDataProvider()->markAsDelete( getFileDataIndex( fileIndex ), isDelete );
	}
}

void CFileViewFilter::toggleMarkAsDeleteState( FileViewIndex const fileIndex )
{
	if( uiVerifyf( isPopulated(), "CRawClipFileView::toggleClipFavouriteState - Attempting to access file data before populated!" ) &&
		uiVerifyf( fileIndex >= 0 && fileIndex < getFilteredFileCount(), "CRawClipFileView::toggleClipFavouriteState - Invalid index!" ) )
	{
		getFileDataProvider()->toggleMarkAsDeleteState( getFileDataIndex( fileIndex ) );
	}
}

bool CFileViewFilter::isMarkedAsDelete( FileViewIndex const fileIndex ) const
{
	bool const c_isFavourite = isPopulated() && fileIndex >= 0 && fileIndex < getFilteredFileCount() ? 
		getFileDataProvider()->isMarkedAsDelete( getFileDataIndex( fileIndex ) ) : false;

	return c_isFavourite;
}

void CFileViewFilter::deleteMarkedFiles()
{
	if( isPopulated() )
	{
		getFileDataProvider()->deleteMarkedFiles();
		m_deleteState = DELETE_PENDING_MARKED;
		refresh( true );
	}
}

bool CFileViewFilter::isDeletingMarkedFiles()
{
	return m_deleteState == DELETE_PENDING_MARKED;
}

u32 CFileViewFilter::getCountOfFilesToDelete() const
{
	u32 const noofFiles = isPopulated() ? getFileDataProvider()->getCountOfFilesToDelete() : 0;
	return noofFiles;
}

size_t CFileViewFilter::getUnfilteredFileCount( bool const currentUserOnly ) const
{ 
	size_t count = 0;
	
	if( isPopulated() )
	{
		count = currentUserOnly ? m_fileDataProvider->getFileCountForCurrentUser() : m_fileDataProvider->getFileCount();
	}

	return count;
}

size_t CFileViewFilter::getCountOfFilesForFilter( int const filterType ) const
{
	size_t result = 0;

	FilterFunc filteringFunction = getFilterFunction( filterType );
	size_t const c_rawFileCount = isPopulated() ? m_fileDataProvider->getFileCount() : 0;

	for( u32 index = 0; index < c_rawFileCount; ++index )
	{
		CFileData const* fileData = m_fileDataProvider->getFileData( index );
		if( filteringFunction == NULL || filteringFunction( fileData, m_fileDataProvider ) )
		{
			++result;
		}
	}

	return result;
}

FileViewIndex CFileViewFilter::getIndexForFileName( char const * const filename, bool const ignoreExtension /*= false */, u64 const userID )
{
	uiAssertf( isPopulated(), "CFileViewFilter::getIndexForFileName - Attempting to access file data before populated!" );

	CFileDataProvider::FileDataIndex dataIndex = isPopulated() ? m_fileDataProvider->getIndexForFileName( filename, ignoreExtension, userID ) : FileIndexInvald;
	return dataIndex != FileIndexInvald ? getFileViewIndex( dataIndex ) : FileIndexInvald;
}

FileViewIndex CFileViewFilter::getIndexForDisplayName( char const * const displayName, u64 const userID )
{
	uiAssertf( isPopulated(), "CFileViewFilter::getIndexForDisplayName - Attempting to access file data before populated!" );

	CFileDataProvider::FileDataIndex dataIndex = isPopulated() ? m_fileDataProvider->getIndexForDisplayName( displayName, userID ) : FileIndexInvald;
	return dataIndex != FileIndexInvald ? getFileViewIndex( dataIndex ) : FileIndexInvald;
}

bool CFileViewFilter::getDisplayName( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getDisplayName - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CFileViewFilter::getDisplayName - Invalid index %d", index );

	return isPopulated() && index < getFilteredFileCount() ? m_fileDataProvider->getDisplayName( m_fileViews[index].getFileDataIndex(), out_buffer ) : false;
}

bool CFileViewFilter::getDisplayName( FileViewIndex const index, atString& out_string ) const
{
	PathBuffer buffer;
	bool const success = getDisplayName( index, buffer.getBufferRef() );
	out_string = success ? buffer.getBuffer() : NULL;
	return success;
}

bool CFileViewFilter::getFileName( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getFileName - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CFileViewFilter::getFileName - Invalid index %d", index );

	return isPopulated() && index < getFilteredFileCount() ? m_fileDataProvider->getFileName( m_fileViews[index].getFileDataIndex(), out_buffer ) : false;
}

bool CFileViewFilter::getFileName( FileViewIndex const index, atString& out_string ) const
{
	PathBuffer buffer;
	bool const success = getFileName( index, buffer.getBufferRef() );
	out_string = success ? buffer.getBuffer() : NULL;
	return success;
}

u64 CFileViewFilter::getFileSize( FileViewIndex const index ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getFileSize - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CFileViewFilter::getFileSize - Invalid index %d", index );

	return isPopulated() && index >= 0 && index < getFilteredFileCount() ? m_fileDataProvider->getFileSize( m_fileViews[index].getFileDataIndex() ) : 0;
}

u64 CFileViewFilter::getOwnerId( FileViewIndex const index ) const
{
	return uiVerifyf( isPopulated(), "CFileViewFilter::getOwnerId - Attempting to access file data before populated!" ) && 
		uiVerifyf( index >= 0 && index < getFilteredFileCount(), "CFileViewFilter::getOwnerId - Invalid index %d", index ) ? 
		m_fileDataProvider->getFileUserId( m_fileViews[index].getFileDataIndex() ) : 0;
}

bool CFileViewFilter::getFileDateString( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getFileDateString - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CFileViewFilter::getFileDateString - Invalid index %d", index );

	return isPopulated() && index < getFilteredFileCount() ? m_fileDataProvider->getFileDateString( m_fileViews[index].getFileDataIndex(), out_buffer ) : false;
}

bool CFileViewFilter::getFileDateString( FileViewIndex const index, atString& out_string ) const
{
	PathBuffer buffer;
	bool const success = getFileDateString( index, buffer.getBufferRef() );
	out_string = success ? buffer.getBuffer() : NULL;
	return success;
}

bool CFileViewFilter::getFilePath( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getFilePath - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CFileViewFilter::getFilePath - Invalid index %d", index );

	return isPopulated() && index < getFilteredFileCount() ? m_fileDataProvider->getFilePath( m_fileViews[index].getFileDataIndex(), out_buffer ) : false;
}

bool CFileViewFilter::getFilePath( FileViewIndex const index, atString& out_string ) const
{
	PathBuffer buffer;
	bool const success = getFilePath( index, buffer.getBufferRef() );
	out_string = success ? buffer.getBuffer() : NULL;
	return success;
}

bool CFileViewFilter::isFileReferenceValid( FileViewIndex const index ) const
{
	bool const result = doesFileExist( index );
	return result;
}

bool CFileViewFilter::doesFileExistInProvider( char const * const filename, bool const checkDisplayName, u64 const userId ) const
{
	bool const c_result = isPopulated() ? m_fileDataProvider->doesFileExistInProvider( filename, checkDisplayName, userId ) : false;
	return c_result;
}

bool CFileViewFilter::doesFileExist( FileViewIndex const index ) const
{
	PathBuffer buffer;
	bool c_result = getFileName( index, buffer.getBufferRef() );

	if (c_result)
	{
		const CFileDataProvider::FileDataIndex dataIndex = getFileDataIndex( index );

		PathBuffer pathBuffer;
		if (m_fileDataProvider->getFileDirectory( dataIndex, pathBuffer.getBufferRef()))
		{
			c_result = doesFileExist( buffer.getBuffer(), pathBuffer.getBuffer() );
		}	
	}

	return c_result;
}

bool CFileViewFilter::doesFileExist( char const * const filename, char const * const dirPath ) const
{
	bool const c_result = isPopulated() ? m_fileDataProvider->doesFileExistInProvider( filename, false ) &&
		m_fileDataProvider->doesFileExist( filename, dirPath ) : false;

	return c_result;
}

void CFileViewFilter::applySort( int const sortType, bool const invert )
{
	size_t const c_fileCount = getFilteredFileCount();
	uiAssertf( isPopulated(), "CFileViewFilter::applySort - Attempting to sort whilst not populated! Good-luck..." );

	//! Early out if requesting to filter by how we're already filtered
	if( c_fileCount <= 1 || ( sortType == m_currentSort && m_inverse == invert ) )
	{
		return;
	}

	SortCompareFunc compareFunc = getSortCompareFunction( sortType, invert );

	m_currentSort = sortType;
	m_inverse = invert;

	if( compareFunc )
	{
		m_fileViews.QSort( 0, (int)c_fileCount, compareFunc );
	}
}

CFileViewFilter::eSortType CFileViewFilter::calculateNextSortType() const
{
	int const c_maxSort = getMaxSortValue();
	int offset = 0;
	eSortType nextSort;

	do 
	{
		++offset;
		nextSort = (eSortType)( m_currentSort + offset );

		if( nextSort >= c_maxSort )
		{
			nextSort = SORTTYPE_FIRST;
		}

	} while ( !isValidSortForProvider( nextSort ) );

	return nextSort;
}

void CFileViewFilter::incrementSortType()
{
	uiAssertf( isPopulated(), "CFileViewFilter::incrementSortType - Attempting to sort whilst not populated! Good-luck..." );

	eSortType const c_nextSort = calculateNextSortType();
	applySort( c_nextSort, m_inverse );
}

void CFileViewFilter::invertSort()
{
	eSortType const sortType = (eSortType)m_currentSort;
	applySort( sortType, !m_inverse );  
}

char const* CFileViewFilter::getSortTypeLocKey( int const sortType ) const
{
	static char const * const s_commonSortTypeStrings[ SORTTYPE_CUSTOM ] =
	{
		"IB_SORT_DATE", "IB_SORT_NAME", "IB_SORT_SIZE"
	};

	char const * const lngKey = sortType >= SORTTYPE_FIRST && sortType < SORTTYPE_CUSTOM ? 
		s_commonSortTypeStrings[ sortType ] : getLocKeyForCustomSort( sortType );

	return lngKey;
}

void CFileViewFilter::applyFilter( int const filterType )
{
	if( m_currentFilter != filterType )
	{
		// Store the filter, rebuild the view
		m_currentFilter = filterType;
		refresh( true );
	}
}

CFileViewFilter::eFilterType CFileViewFilter::calculateNextFilterType() const
{
	int const c_maxFilter = getMaxFilterValue();
	int offset = 0;
	eFilterType nextFilter;
	
	do 
	{
		++offset;
		nextFilter = (eFilterType)( m_currentFilter + offset );

		if( nextFilter >= c_maxFilter )
		{
			nextFilter = FILTERTYPE_FIRST;
		}

	} while ( !isValidFilterForProvider( nextFilter ) );

	return nextFilter;
}

void CFileViewFilter::incrementFilterType()
{
	uiAssertf( isPopulated(), "CFileViewFilter::incrementFilterType - Attempting to filter whilst not populated! Good-luck..." );

	eFilterType const c_nextFilter = calculateNextFilterType();
	applyFilter( c_nextFilter );
}

char const* CFileViewFilter::getFilterTypeLocKey( int const filterType ) const
{
	static char const * const s_commonFilterTypeStrings[ FILTERTYPE_CUSTOM ] =
	{
		"IB_FILTER_ALL", "IB_FILTER_TODAY", "IB_FILTER_ALL_EVERYONE"
	};

	char const * const lngKey = filterType >= FILTERTYPE_FIRST && filterType < FILTERTYPE_CUSTOM ? 
		s_commonFilterTypeStrings[ filterType ] : getLocKeyForCustomFilter( filterType );

	return lngKey;
}

void CFileViewFilter::applyFilterAndSort( int const filterType, int const sortType, bool const invertedSort )
{
	m_currentSort = sortType;
	m_inverse = invertedSort;
	m_currentFilter = filterType;

	refresh( true );
}

bool CFileViewFilter::startDeleteFile( FileViewIndex const index )
{
	uiAssertf( isPopulated(), "CFileViewFilter::deleteFile - Attempting to access file data before populated!" );
	uiAssertf( isValidIndex( index ), "CFileViewFilter::deleteFile - Invalid index %d", index );
	uiAssertf( canDelete(), "CFileViewFilter::deleteFile - Attempting delete whilst pending delete is in progress" );

	bool const c_deleted = isPopulated() && isValidIndex( index ) ? 
		m_fileDataProvider->startDeleteFile( m_fileViews[index].getFileDataIndex() ) : false;

		m_deleteState = DELETE_PENDING;
		refresh( true );

	return c_deleted;
}

bool CFileViewFilter::canDelete() const
{
	return m_deleteState != DELETE_PENDING;
}

bool CFileViewFilter::isDeletingFile() const
{
	return m_deleteState == DELETE_PENDING || m_deleteState == DELETE_PENDING_MARKED;
}

bool CFileViewFilter::hasDeleteFailed() const
{
	return m_deleteState == DELETE_FAILED;
}

bool CFileViewFilter::hasDeleteFinished() const
{
	return m_deleteState == DELETE_COMPLETE;
}

void CFileViewFilter::refresh( bool const viewOnly )
{
	if( isPopulated() )
	{
		CFileDataProvider* provider = m_fileDataProvider;
		int const c_appliedSort = (eSortType)m_currentSort;
		int const c_appliedFilter = m_currentFilter;
		bool const inverse = m_inverse;

		shutdown();

		if( !viewOnly )
		{
			provider->refresh();
		}

		initialize( *provider );
		m_currentFilter = c_appliedFilter;

		if( viewOnly )
		{
			// Force an update to make sure filtering occurs
			update( false );
			applySort( c_appliedSort, inverse );
		}
	}
}

bool CFileViewFilter::isValidIndex( FileViewIndex const index ) const
{
	return index >= 0 && index < getFilteredFileCount();
}

CFileData const * CFileViewFilter::getFileData( FileViewIndex const index ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getFileData - Attempting to access file data before populated!" );
	uiAssertf( index >= 0 && index < getFilteredFileCount(), "CFileViewFilter::getFileData - Invalid index %d", index );

	return isPopulated() && index < getFilteredFileCount() ? m_fileDataProvider->getFileData( m_fileViews[index].getFileDataIndex() ) : NULL;
}

CFileDataProvider::FileDataIndex CFileViewFilter::getFileDataIndex( FileViewIndex const index ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getFileDataIndex - Attempting to access file data before populated!" );
	uiAssertf( isValidIndex( index ), "CFileViewFilter::getFileData - Invalid index %d", index );

	return isPopulated() && isValidIndex( index ) ? m_fileViews[index].getFileDataIndex() : -1;
}

FileViewIndex CFileViewFilter::getFileViewIndex( CFileDataProvider::FileDataIndex const index ) const
{
	uiAssertf( isPopulated(), "CFileViewFilter::getFileViewIndex - Attempting to access file data before populated!" );

	FileViewIndex returnedIndex = FileIndexInvald;

	for( int i = 0; i < getFilteredFileCount(); ++i )
	{
		CFileViewEntry const& fileView = m_fileViews[i];
		if( fileView.getFileDataIndex() == index )
		{
			returnedIndex = i;
			break;
		}
	}

	return returnedIndex;
}

CFileViewFilter::SortCompareFunc CFileViewFilter::getSortCompareFunction( int const sortType, bool const invert ) const
{
	CFileViewFilter::SortCompareFunc compareFunc = NULL;

	switch( sortType )
	{
	case SORTTYPE_DISPLAYNAME:
		{
			compareFunc = invert ? CFileViewFilter::RSortByDisplayName : CFileViewFilter::SortByDisplayName;
			break;
		}
	case SORTTYPE_FILETIME:
		{
			compareFunc = invert ? CFileViewFilter::RSortByMostRecent : CFileViewFilter::SortByMostRecent;
			break;
		}
	case SORTTYPE_FILESIZE:
		{
			compareFunc = invert ? CFileViewFilter::RSortByFileSize : CFileViewFilter::SortByFileSize;
			break;
		}
	case  SORTTYPE_NONE:
		{
			// Do nothing, leave as is.
			break;
		}
	default:
		{
			// Allow derived class to inject sorting function
			compareFunc = getSortFunctionForCustomSort( sortType, invert );
			break;
		}
	}

	return compareFunc;
}

CFileViewFilter::FilterFunc CFileViewFilter::getFilterFunction( int const filterType ) const
{
	CFileViewFilter::FilterFunc filterFunc = NULL;

	switch( filterType )
	{
	case FILTERTYPE_MODIFIED_TODAY_LOCAL_USER_OWNED:
		{
			filterFunc = CFileViewFilter::FilterByModifiedToday;
			break;
		}
	case FILTERTYPE_LOCAL_USER_OWNED:
		{
			filterFunc = CFileViewFilter::FilterBelongsToCurrentUser;
			break;
		}
	case FILTERTYPE_MARKED_DELETE_ANY_USER:
		{
			filterFunc = CFileViewFilter::FilterMarkedForDelete;
			break;
		}
	case FILTERTYPE_OTHER_USER:
		{
			filterFunc = CFileViewFilter::FilterBelongsToOtherUser;
			break;
		}
	case FILTERTYPE_ANY_USER:
	case FILTERTYPE_NONE:
		{
			// Do nothing, leave as is.
			break;
		}
	case  FILTERTYPE_CUSTOM:
	default:
		{
			// Allow derived class to inject filtering function
			filterFunc = getFilterFunctionForCustomFilter( filterType );
			break;
		}
	}

	return filterFunc;
}

bool CFileViewFilter::isValidFilterForProvider( int const filterType ) const
{
	bool const c_isValid = filterType >= FILTERTYPE_FIRST && 
		filterType < FILTERTYPE_CUSTOM
#if RSG_PC
		&& filterType != FILTERTYPE_ANY_USER && filterType != FILTERTYPE_OTHER_USER
#endif
		;

	return c_isValid;
}

bool CFileViewFilter::FilterBelongsToCurrentUser( CFileData const * const fileData, CFileDataProvider const * const dataProvider )
{
    u64 const c_activeUserId = dataProvider ? dataProvider->getActiveUserId() : 0;
    u64 const c_userID = fileData ? fileData->getUserId() : 0;

    bool const c_result = c_activeUserId == c_userID;
    return c_result;
}

bool CFileViewFilter::FilterBelongsToOtherUser( CFileData const * const fileData, CFileDataProvider const * const dataProvider )
{
	u64 const c_activeUserId = dataProvider ? dataProvider->getActiveUserId() : 0;
	u64 const c_userID = fileData ? fileData->getUserId() : 0;

	bool const c_result = c_activeUserId != c_userID;
	return c_result;
}

bool CFileViewFilter::FilterByModifiedToday( CFileData const * const fileData, CFileDataProvider const * const dataProvider )
{
    bool result = false;

    // Check it belongs to us first
    if( FilterBelongsToCurrentUser( fileData, dataProvider ) )
    {
        u64 const c_fileTime = fileData ? fileData->getLastWriteTime() : 0;

        fiDevice::SystemTime currentTime;
        fiDevice::GetLocalSystemTime( currentTime );

        fiDevice::SystemTime fileWriteTime;
        fiDevice::ConvertFileTimeToSystemTime( c_fileTime, fileWriteTime );

        result = currentTime.wYear == fileWriteTime.wYear && 
            currentTime.wMonth == fileWriteTime.wMonth &&
            currentTime.wDay == fileWriteTime.wDay;
    }

    return result;
}

bool CFileViewFilter::FilterMarkedForDelete( CFileData const * const fileData, CFileDataProvider const * const dataProvider )
{
    bool result = dataProvider && fileData ? dataProvider->isMarkedAsDelete( *fileData ) : false;
    return result;
}

void CFileViewFilter::BuildViewInternal()
{
	m_fileViews.Reset();
	size_t const c_totalFileCount = m_fileDataProvider->getFileCount();
	FilterFunc filteringFunction = getFilterFunction( m_currentFilter );

	CFileViewEntry builderEntry;
	for( CFileDataProvider::FileDataIndex index = 0; index < c_totalFileCount; ++index )
	{
		CFileData const* fileData = m_fileDataProvider->getFileData( index );
		if( filteringFunction == NULL || filteringFunction( fileData, m_fileDataProvider ) )
		{
			builderEntry.setFileDataIndex( index );
			builderEntry.setFileDataProvider( m_fileDataProvider );

			m_fileViews.PushAndGrow( builderEntry );
		}
	}
}
