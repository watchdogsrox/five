/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FileDataProvider.h
// PURPOSE : Provides access to a filtered array of file data.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef FILE_DATA_PROVIDER_H_
#define FILE_DATA_PROVIDER_H_

//! TODO - Move this outside of Replay as not replay specific... 

// rage
#include "atl/array.h"
#include "file/limits.h"
#include "string/string.h"
#include "system/nelem.h"

// Framework Includes
#include "fwlocalisation/templateString.h"
#include "fwlocalisation/textTypes.h"
#include "fwlocalisation/textUtil.h"
#include "fwutil/xmacro.h"

// game
#include "control/replay/file/FileData.h"
#include "control/replay/File/device_replay.h"
#include "frontend/ui_channel.h"

namespace rage
{
	class fiDevice;
}

class CFileDataProvider
{
public: // declarations and variables
	typedef int FileDataIndex;

#define FileIndexInvald -1

public: // methods
	CFileDataProvider()
		: m_directoryPath()
		, m_filter()
	{
	}

	virtual ~CFileDataProvider()
	{
		shutdown();
	}

	// PURPOSE: Initialize the provider and populate with file data for the given directory
	// PARAMS:
	//		directoryPath - Directory to check for file data
	//		filter - Text pattern to filter full filename & extension on. Pass null to provide no filtering.
	// RETURNS:
	//		True if successful, false otherwise.
	virtual bool initialize( char const * const directoryPath, char const * const filter, u64 const activeUserId );

	inline bool isInitialized() const
	{
		return	!m_directoryPath.IsNullOrEmpty();
	}

	inline bool isPopulated() const
	{
		bool result = false;
		bool callSucceeded = false;

		if( isInitialized() )
		{
			callSucceeded = CheckEnumerationFunctionImpl( result );
		}

		return callSucceeded && result;
	}

	inline bool hasPopulationFailed() const
	{
		bool result = false;
		bool callSucceeded = false;

		if( isInitialized() )
		{
			callSucceeded = CheckEnumerationFunctionImpl( result );
		}

		return callSucceeded && !result;
	}

	void shutdown()
	{
		cleanup();
		m_directoryPath.Clear();
		m_filter.Clear();
		m_activeUserId = 0;
	}

	void markAsDelete( FileDataIndex const index, bool const isMarked );
	void toggleMarkAsDeleteState( FileDataIndex const index );

	bool isMarkedAsDelete( FileDataIndex const index ) const;
	bool isMarkedAsDelete( CFileData const& fileData ) const;

	void deleteMarkedFiles();
	bool isDeletingMarkedFiles();
	bool hasDeleteMarkedFilesFailed();
	bool hasDeleteMarkedFilesFinished();

	u32 getCountOfFilesToDelete() const;

	// PURPOSE: Get a count of files we have data for
	// RETURNS:
	//		Count of files
	size_t getFileCount() const;

	// PURPOSE: Get a count of files we have data for, based on a given user id
	// RETURNS:
	//		Count of files
	size_t getFileCountForUser( u64 const userId ) const;
	size_t getFileCountForCurrentUser() const { return getFileCountForUser( m_activeUserId ); }

	// PURPOSE: Return a file index for the given file name.
	// PARAMS:
	//		filename - Filename to search for.
	// RETURNS:
	//		Valid file index if found, FileIndexInvald otherwise
	FileDataIndex getIndexForFileName( char const * const filename, bool const ignoreExtension = false, u64 const userId = 0 ) const;

	// PURPOSE: Return a file index for the given file data.
	// PARAMS:
	//		fileData - Filedata to search for.
	// RETURNS:
	//		Valid file index if found, FileIndexInvald otherwise
	FileDataIndex getIndexForFiledata( CFileData const& fileData ) const;

	// PURPOSE: Return a file index for the given display name.
	// PARAMS:
	//		displayName - Displayname to search for.
	// RETURNS:
	//		Valid file index if found, FileIndexInvald otherwise
	virtual FileDataIndex getIndexForDisplayName( char const * const displayName, u64 const userId = 0  ) const;

	// PURPOSE: Gets a given file by index
	// PARAMS:
	//		index - Index of the file.
	// RETURNS:
	//		CFileData pointer if valid index and initialized. Null pointer otherwise.
	CFileData const * getFileData( FileDataIndex const index ) const;

	// PURPOSE: Generates a UI display name for the given file
	// PARAMS:
	//		index - File index to generate name for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	// NOTES:
	//		At the base level we just return the file name. Inheriting classes can derive this to generate better display names.
	bool getDisplayName( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Generates a filename for the given file
	// PARAMS:
	//		index - File index to generate name for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	bool getFileName( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Gets the user id of a file, from the path
	// PARAMS:
	//		index - File index to get size of
	// RETURNS:
	//		user id, in u64 (no platform conversions)
	u64 getFileUserId( FileDataIndex const index ) const;

	// PURPOSE: Gets the user id of a file, from the path
	// PARAMS:
	//		index - File index to get user for
	//		out_buffer - Output buffer to populate with id as string
	// RETURNS:
	//		True if successful, false otherwise
	bool getFileUserId( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Gets the user id of a file, from the path
	// PARAMS:
	//		fileData - Filedata to read user from
	//		out_buffer - Output buffer to populate with id as string
	// RETURNS:
	//		True if successful, false otherwise
	bool getFileUserId( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Returns the size of the given file.
	// PARAMS:
	//		index - File index to get size of
	// RETURNS:
	//		Size in bytes of the given file.
	u64 getFileSize( FileDataIndex const index ) const;

	// PURPOSE: Returns the float extra data member for the given file
	// PARAMS:
	//		index - File index to get float data from
	// RETURNS:
	//		Float data as set on enumeration.
	float getExtraDataFloat( FileDataIndex const index ) const;

	// PURPOSE: Returns the u32 extra data member for the given file
	// PARAMS:
	//		index - File index to get float data from
	// RETURNS:
	//		u32 data as set on enumeration.
	u32 getExtraDataU32( FileDataIndex const index ) const;

	// PURPOSE: Generates a date string for the given file
	// PARAMS:
	//		index - File index to generate name for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	bool getFileDateString( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Generates a full path to the given file
	// PARAMS:
	//		index - File index to generate full path for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	virtual bool getFilePath( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Generates a full path from the given file data
	// PARAMS:
	//		fileData - File data to generate full path for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	virtual bool getFilePath( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Generates a path directory of the given file
	// PARAMS:
	//		index - File index to generate full path for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	bool getFileDirectory( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Generates a path directory of the given file
	// PARAMS:
	//		fileData - File data to generate full path for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	virtual bool getFileDirectory( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Check if the given filename exists in this provider
	// PARAMS:
	//		filename - Name of file to check for
	// RETURNS:
	//		True if exists, false otherwise
	bool doesFileExistInProvider( char const * const filename, bool const checkDisplayName = false, u64 const userId = 0 ) const;

	// PURPOSE: Check if the given filename exists in the directory this provider is looking at
	// PARAMS:
	//		filename - Name of file to check for
	// RETURNS:
	//		True if exists, false otherwise
	bool doesFileExist( char const * const filename, char const * const dirPath ) const;

	// PURPOSE: Delete the given file from storage
	// PARAMS:
	//		index - File index to generate full path for
	// RETURNS:
	//		True if successful, false otherwise
	bool startDeleteFile( FileDataIndex const index );

	bool hasDeleteFileFailed() const;
	bool hasDeleteFileFinished() const;

	// PURPOSE: Force a refresh of the index file data
	void refresh();

	bool isValidIndex( FileDataIndex const index ) const;

	u64 getActiveUserId() const { return m_activeUserId; }

protected: // declarations and variables

	FileDataStorage					m_fileData;
	PathBuffer						m_directoryPath;
	PathBuffer						m_filter;

	u64								m_activeUserId;

protected: // methods

	virtual bool EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter );
	virtual bool CheckEnumerationFunctionImpl( bool& result ) const;

	virtual bool ValidateFileAndPopulateCustomData( char const * const directoryPath, char const * const fileName, CFileData& out_data );

	virtual bool DeleteInvalidFilesOnEnum() const { return false; }

	virtual bool DeleteFunctionImpl( const char* filepath );
	virtual bool CheckDeleteFunctionImpl( bool& result ) const;

	virtual void refreshInternal();
	bool repopulate();

	// Accessor to the directory path/filter we are providing data for
	inline char const * GetFilter() const { return m_filter.getBuffer(); }
	inline bool HasFilter() const { return !m_filter.IsNullOrEmpty(); } 

private: // methods

	// PURPOSE: Populate with file data.
	// PARAMS:
	//		filter - Text pattern to filter full filename & extension on. Pass null to provide no filtering.
	// RETURNS:
	//		True if successful, false otherwise.
	bool populate();

	// PURPOSE: Allow derived classes to do pre-cleanup cleanup
	virtual void preCleanup() {};

	// PURPOSE: Cleanup previously populated file data.
	void cleanup()
	{
		uiAssertf( isInitialized(), "CFileProviderBase::cleanup - called before initialization!" );
		preCleanup();
		m_fileData.Reset();
	}

	virtual void RemoveEntry( FileDataIndex const index );

	inline char const * GetDirectoryPath() const { return m_directoryPath.getBuffer(); }

};

#endif // FILE_DATA_PROVIDER_H_
