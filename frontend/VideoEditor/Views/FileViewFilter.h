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
#ifndef FILE_VIEW_FILTER_H_
#define FILE_VIEW_FILTER_H_

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "atl/array.h"

// game
#include "control/replay/file/FileData.h"
#include "frontend/VideoEditor/DataProviders/FileDataProvider.h"

namespace rage
{
	class atString;
}

typedef int FileViewIndex;

class CFileViewFilter
{
public: // declarations and variables
	
	enum eSortType
	{
		SORTTYPE_NONE = -1,
		SORTTYPE_FIRST,

		SORTTYPE_FILETIME = SORTTYPE_FIRST,
		SORTTYPE_DISPLAYNAME,
		SORTTYPE_FILESIZE,

		SORTTYPE_CUSTOM		// Derived classes can use this as a base to add their own sorting. Must be last in common sort types
	};

#define DEFAULT_SORT_TYPE		(SORTTYPE_FILETIME)
#define DEFAULT_SORT_INVERSE	(true)

	enum eFilterType
	{
		FILTERTYPE_NONE = -1,
		FILTERTYPE_FIRST,

		FILTERTYPE_LOCAL_USER_OWNED = FILTERTYPE_FIRST,
		FILTERTYPE_MODIFIED_TODAY_LOCAL_USER_OWNED,
		FILTERTYPE_ANY_USER,
		FILTERTYPE_MARKED_DELETE_ANY_USER,
		FILTERTYPE_OTHER_USER,

		FILTERTYPE_CUSTOM		// Derived classes can use this as a base to add their own filtering. Must be last in common filter types
	};

#define DEFAULT_FILTER_TYPE		(FILTERTYPE_LOCAL_USER_OWNED)

public: // methods
	CFileViewFilter()
		: m_fileViews()
		, m_fileDataProvider( NULL )
		, m_deleteState( DELETE_IDLE )
		, m_currentSort( SORTTYPE_NONE )
		, m_currentFilter( DEFAULT_FILTER_TYPE )
		, m_inverse( false )
		, m_populated( false )
	{ }

	virtual ~CFileViewFilter()
	{
		shutdown();
	}

	bool initialize( CFileDataProvider& fileDataProvider );
	bool isInitialized() const;
	bool isPopulated() const;
	bool hasPopulationFailed() const;
	void update( bool allowSortOnPopulate = true );
	void shutdown();

	void markAsDelete( FileViewIndex const fileIndex, bool const isDelete );
	void toggleMarkAsDeleteState( FileViewIndex const fileIndex );
	bool isMarkedAsDelete( FileViewIndex const fileIndex ) const;

	void deleteMarkedFiles();
	bool isDeletingMarkedFiles();

	u32 getCountOfFilesToDelete() const;

	size_t getUnfilteredFileCount( bool const currentUserOnly = false ) const;
	inline size_t getFilteredFileCount() const { return m_fileViews.size(); }
	size_t getCountOfFilesForFilter( int const filterType ) const;

	FileViewIndex getIndexForFileName( char const * const filename, bool const ignoreExtension = false, u64 const userID = 0 );
	FileViewIndex getIndexForDisplayName( char const * const displayName, u64 const userID = 0 );

	bool getDisplayName( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	bool getDisplayName( FileViewIndex const index, atString& out_string ) const;

	bool getFileName( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	bool getFileName( FileViewIndex const index, atString& out_string ) const;

	u64 getFileSize( FileViewIndex const index ) const;
	u64 getOwnerId( FileViewIndex const index ) const;

	bool getFileDateString( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	bool getFileDateString( FileViewIndex const index, atString& out_string ) const;

	bool getFilePath( FileViewIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	bool getFilePath( FileViewIndex const index, atString& out_string ) const;

	bool isFileReferenceValid( FileViewIndex const index ) const;

	bool doesFileExistInProvider( char const * const filename, bool const checkDisplayName = false, u64 const userId = 0 ) const;
	bool doesFileExist( FileViewIndex const index  ) const;
	bool doesFileExist( char const * const filename, char const * const dirPath ) const;

	// PURPOSE: Apply the given sorting to the provided file data
	// PARAMS:
	//		sortType - Sort to apply
	//		invert - Use the inverse sort
	void applySort( int const sortType, bool const invert );

	eSortType calculateNextSortType() const;
	void incrementSortType();

	// PURPOSE: Invert the currently applied sort
	void invertSort();

	char const* getSortTypeLocKey( int const sortType ) const;
	inline char const * getCurrentSortTypeLocKey() const { return getSortTypeLocKey( m_currentSort ); }
	inline char const * getNextSortTypeLocKey() const { return getSortTypeLocKey( calculateNextSortType() ); }

	inline int GetCurrentSortType() const { return m_currentSort; }
	inline bool IsSortInverted() const { return m_inverse; }

	// PURPOSE: <Apply the given filter to the file view
	// PARAMS:
	//		filterType - Filtering to apply
	void applyFilter( int const filterType );

	eFilterType calculateNextFilterType() const;
	void incrementFilterType();

	char const* getFilterTypeLocKey( int const filterType ) const;
	inline char const * getFilterTypeLocKey() const { return getFilterTypeLocKey( m_currentFilter ); }
	inline char const * getNextFilterTypeLocKey() const { return getFilterTypeLocKey( calculateNextFilterType() ); }

	inline int GetCurrentFilterType() const { return m_currentFilter; }

	void applyFilterAndSort( int const filterType, int const sortType, bool const invertedSort );

	// PURPOSE: Delete the given file from storage
	bool startDeleteFile( FileViewIndex const index );

	bool canDelete() const;
	bool isDeletingFile() const;
	bool hasDeleteFailed() const;
	bool hasDeleteFinished() const;

	// PURPOSE: Force a refresh of the underlying files
	// PARAMS:
	//		viewOnly - Whether we should onyl refresh the view data
	// NOTES:
	//		Refreshing the view only will not refresh the underlying file-data cache.
	void refresh( bool const viewOnly );

	bool isValidIndex( FileViewIndex const index ) const;
	CFileDataProvider::FileDataIndex getFileDataIndex( FileViewIndex const index ) const;
	FileViewIndex getFileViewIndex( CFileDataProvider::FileDataIndex const index ) const;

protected: // declarations and variables

	class CFileViewEntry
	{
	public:
		CFileViewEntry()
			: m_fileDataIndex( 0 )
			, m_provider( NULL )
		{

		}

		CFileViewEntry( CFileDataProvider::FileDataIndex fileDataIndex, CFileDataProvider const* provider )
			: m_fileDataIndex( fileDataIndex )
			, m_provider( provider )
		{

		}

		inline void setFileDataIndex( CFileDataProvider::FileDataIndex const fileDataIndex ) { m_fileDataIndex = fileDataIndex; }
		inline CFileDataProvider::FileDataIndex getFileDataIndex() const { return m_fileDataIndex; }

		inline void setFileDataProvider( CFileDataProvider const* provider ) { m_provider = provider; }
		inline CFileDataProvider const* getFileDataProvider() const { return m_provider; }

	private:
		CFileDataProvider::FileDataIndex	m_fileDataIndex;
		CFileDataProvider const*			m_provider;
	};

	typedef int (*SortCompareFunc)( CFileViewEntry const*, CFileViewEntry const* );
	typedef bool (*FilterFunc)( CFileData const * const, CFileDataProvider const * const );

protected: // methods
	CFileDataProvider * getFileDataProvider() { return m_fileDataProvider; }
	CFileDataProvider const * getFileDataProvider() const { return m_fileDataProvider; }

	CFileData const * getFileData( FileViewIndex const index ) const;

	SortCompareFunc			getSortCompareFunction( int const sortType, bool const invert ) const;

	// Support for derived sorting
	virtual SortCompareFunc getSortFunctionForCustomSort( int const sortType, bool const invert ) const	{ (void)sortType; (void)invert; return NULL; }
	virtual char const*		getLocKeyForCustomSort( int const sortType ) const							{ (void)sortType; return ""; }
	virtual bool			isValidSortForProvider( int const sortType ) const							{ return sortType >= SORTTYPE_FIRST && sortType < SORTTYPE_CUSTOM; }
	virtual int				getMaxSortValue() const														{ return SORTTYPE_CUSTOM; }

	FilterFunc				getFilterFunction( int const filterType ) const;

	// Support for derived filtering
	virtual FilterFunc		getFilterFunctionForCustomFilter( int const filterType ) const				{ (void)filterType; return NULL; }
	virtual char const*		getLocKeyForCustomFilter( int const filterType ) const						{ (void)filterType; return ""; }
	virtual bool			isValidFilterForProvider( int const filterType ) const;		
	virtual int				getMaxFilterValue() const													{ return FILTERTYPE_CUSTOM; }

protected: // methods
    static bool FilterBelongsToCurrentUser( CFileData const * const fileData, CFileDataProvider const * const dataProvider );
	static bool FilterBelongsToOtherUser( CFileData const * const fileData, CFileDataProvider const * const dataProvider );
    static bool FilterByModifiedToday( CFileData const * const fileData, CFileDataProvider const * const dataProvider );
    static bool FilterMarkedForDelete( CFileData const * const fileData, CFileDataProvider const * const dataProvider );

private: // declarations and variables

	enum eDeleteState
	{
		DELETE_IDLE,
		DELETE_PENDING,
		DELETE_PENDING_MARKED,
		DELETE_FAILED,
		DELETE_COMPLETE
	};

	atArray< CFileViewEntry >			m_fileViews;
	CFileDataProvider *					m_fileDataProvider;
	eDeleteState						m_deleteState;
	int									m_currentSort;
	int									m_currentFilter;
	bool								m_inverse;
	bool								m_populated;

private: // methods

	void BuildViewInternal();

	template< bool _inverse >
	static int _SortByMostRecent( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		int result = 0;

		CFileViewEntry const* entry1 = (CFileViewEntry*)data1;
		CFileViewEntry const* entry2 = (CFileViewEntry*)data2;

		CFileDataProvider const* provider1 = entry1 ? entry1->getFileDataProvider() : NULL;
		CFileDataProvider const* provider2 = entry2 ? entry2->getFileDataProvider() : NULL;

		result = ( provider1 && provider2 && provider1 == provider2 ) ? 
			CFileData::CompareLastWriteTime( 
			provider1->getFileData( entry1->getFileDataIndex() ), provider2->getFileData( entry2->getFileDataIndex() ) )
			: 0;

		return ( _inverse && result != 0 ) ? -result : result;
	}

	static int SortByMostRecent( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByMostRecent<false>( data1, data2 );
	}

	static int RSortByMostRecent( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByMostRecent<true>( data1, data2 );
	}

	template< bool _inverse >
	static int _SortByFileName( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		int result = 0;

		CFileViewEntry const* entry1 = (CFileViewEntry*)data1;
		CFileViewEntry const* entry2 = (CFileViewEntry*)data2;

		CFileDataProvider const* provider1 = entry1 ? entry1->getFileDataProvider() : NULL;
		CFileDataProvider const* provider2 = entry2 ? entry2->getFileDataProvider() : NULL;

		result = ( provider1 && provider2 && provider1 == provider2 ) ? 
			CFileData::CompareName( 
			provider1->getFileData( entry1->getFileDataIndex() ), provider2->getFileData( entry2->getFileDataIndex() ) )
			: 0;

		return ( _inverse && result != 0 ) ? -result : result;
	}

	static int SortByFileName( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByFileName<false>( data1, data2 );
	}

	static int RSortByFileName( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByFileName<true>( data1, data2 );
	}

	template< bool _inverse >
	static int _SortByDisplayName( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		int result = 0;

		CFileViewEntry const* entry1 = (CFileViewEntry*)data1;
		CFileViewEntry const* entry2 = (CFileViewEntry*)data2;

		CFileDataProvider const* provider1 = entry1 ? entry1->getFileDataProvider() : NULL;
		CFileDataProvider const* provider2 = entry2 ? entry2->getFileDataProvider() : NULL;

		if( provider1 && provider2 && provider1 == provider2 )
		{
			PathBuffer bufferA;
			provider1->getDisplayName( entry1->getFileDataIndex(), bufferA.getBufferRef() );

			PathBuffer bufferB;
			provider1->getDisplayName( entry2->getFileDataIndex(), bufferB.getBufferRef() );

			result = CFileData::CompareAString( bufferA.getBuffer(), bufferB.getBuffer() );

		}

		return ( _inverse && result != 0 ) ? result : -result;
	}

	static int SortByDisplayName( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByDisplayName<false>( data1, data2 );
	}

	static int RSortByDisplayName( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByDisplayName<true>( data1, data2 );
	}

	template< bool _inverse >
	static int _SortByFileSize( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		int result = 0;

		CFileViewEntry const* entry1 = (CFileViewEntry*)data1;
		CFileViewEntry const* entry2 = (CFileViewEntry*)data2;

		CFileDataProvider const* provider1 = entry1 ? entry1->getFileDataProvider() : NULL;
		CFileDataProvider const* provider2 = entry2 ? entry2->getFileDataProvider() : NULL;

		result = ( provider1 && provider2 && provider1 == provider2 ) ? 
			CFileData::CompareFileSize( 
			provider1->getFileData( entry1->getFileDataIndex() ), provider2->getFileData( entry2->getFileDataIndex() ) )
			: 0;

		return ( _inverse && result != 0 ) ? -result : result;
	}

	static int SortByFileSize( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByFileSize<false>( data1, data2 );
	}

	static int RSortByFileSize( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByFileSize<true>( data1, data2 );
	}

	template< bool _inverse >
	static int _SortByMarkedForDelete( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		int result = 0;

		CFileViewEntry const* entry1 = (CFileViewEntry*)data1;
		CFileViewEntry const* entry2 = (CFileViewEntry*)data2;

		CFileDataProvider const* provider1 = entry1 ? entry1->getFileDataProvider() : NULL;
		CFileDataProvider const* provider2 = entry2 ? entry2->getFileDataProvider() : NULL;

		if( provider1 && provider2 && provider1 == provider2 )
		{
			CFileDataProvider::FileDataIndex const index1 = entry1->getFileDataIndex();
			CFileDataProvider::FileDataIndex const index2 = entry2->getFileDataIndex();

			bool const c_isMarkedDelete1 = provider1->isMarkedAsDelete( index1 );
			bool const c_isMarkedDelete2 = provider2->isMarkedAsDelete( index2 );

			// If both are equal, sort by name
			result = ( c_isMarkedDelete1 == c_isMarkedDelete2 ) ? 
				CFileData::CompareName( provider1->getFileData( index1 ), provider2->getFileData( index2 ) ) : c_isMarkedDelete1 ? 1 : -1;
		}

		return ( _inverse && result != 0 ) ? -result : result;
	}

	static int SortByMarkedForDelete( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByMarkedForDelete<false>( data1, data2 );
	}

	static int RSortByMarkedForDelete( CFileViewEntry const * data1, CFileViewEntry const * data2 )
	{
		return _SortByMarkedForDelete<true>( data1, data2 );
	}
};

#endif // FILE_VIEW_FILTER_H_
