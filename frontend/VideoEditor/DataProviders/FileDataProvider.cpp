/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : FileDataProvider.cpp
// PURPOSE : Provides access to a filtered array of file data.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "file/device.h"
#include "string/string.h"
#include "string/unicode.h"

// game
#include "control/replay/replay.h"
#include "FileDataProvider.h"
#include "system/FileMgr.h"
#include "text/TextConversion.h"

#if RSG_ORBIS
#include <_fs.h>
#endif

FRONTEND_OPTIMISATIONS();

// Uses the unicode converter
#define UTF8_TO_UTF16(x) reinterpret_cast<const wchar_t* const>(UTF8_TO_WIDE(x))
#define UTF16_TO_UTF8(x) WIDE_TO_UTF8(reinterpret_cast<const char16* const>(x))

bool CFileDataProvider::initialize( char const * const directoryPath, char const * const filter, u64 const activeUserId )
{
	bool success = false;

	if( uiVerifyf( directoryPath, "CFileDataProvider::initialize - Null path for enumeration" ) &&
		uiVerifyf( !isInitialized(), "Double initialization of CFileProviderBase" ) )
	{
		m_directoryPath.Set( directoryPath );
		m_filter.Set( filter );
		m_activeUserId = activeUserId;
		success = populate();
	}

	return success;
}

void CFileDataProvider::markAsDelete( FileDataIndex const REPLAY_ONLY(index), bool const REPLAY_ONLY(isMarked) )
{
#if GTA_REPLAY
	PathBuffer pathBuffer;
	if( getFilePath( index, pathBuffer.getBufferRef() ) )
	{
		if( isMarked )
		{
			CReplayMgr::MarkAsDelete( pathBuffer.getBuffer() );
		}
		else
		{
			CReplayMgr::UnmarkAsDelete( pathBuffer.getBuffer() );
		}
	}
#endif // GTA_REPLAY
}

void CFileDataProvider::toggleMarkAsDeleteState( FileDataIndex const REPLAY_ONLY(index) )
{
#if GTA_REPLAY
	PathBuffer pathBuffer;
	if( getFilePath( index, pathBuffer.getBufferRef() ) )
	{
		bool const c_isMarked = CReplayMgr::IsMarkedAsDelete( pathBuffer.getBuffer() );

		if( c_isMarked )
		{
			CReplayMgr::UnmarkAsDelete( pathBuffer.getBuffer() );
		}
		else
		{
			CReplayMgr::MarkAsDelete( pathBuffer.getBuffer() );
		}
	}
#endif // GTA_REPLAY
}

bool CFileDataProvider::isMarkedAsDelete( FileDataIndex const index ) const
{
	CFileData const* fileData = getFileData( index );

	bool const c_isMarked = fileData ? isMarkedAsDelete( *fileData ) : false;
	return c_isMarked;
}

bool CFileDataProvider::isMarkedAsDelete( CFileData const& REPLAY_ONLY(fileData) ) const
{
	bool isMarked = false;
#if GTA_REPLAY
	PathBuffer pathBuffer;
	if( getFilePath( fileData, pathBuffer.getBufferRef() ) )
	{
		isMarked = CReplayMgr::IsMarkedAsDelete( pathBuffer.getBuffer() );
	}
#endif // GTA_REPLAY
	return isMarked;
}

void CFileDataProvider::deleteMarkedFiles()
{
#if GTA_REPLAY
	bool success = CReplayMgr::StartMultiDelete(m_filter.getBuffer());
	if (success)
	{
		int const c_totalFiles = static_cast<int>(getFileCount());
		for( int index = c_totalFiles-1; index >= 0; --index )
		{
			CFileData const & fileData = m_fileData[ index ];
			if (isMarkedAsDelete(fileData))
			{
				RemoveEntry( static_cast<FileDataIndex>(index) );
			}
		}
	}
#endif // GTA_REPLAY
}

bool CFileDataProvider::isDeletingMarkedFiles()
{
#if GTA_REPLAY
	bool saveComplete = false;
	bool const c_actionInProcess = CReplayMgr::CheckMultiDelete( saveComplete );

	return c_actionInProcess && !saveComplete;
#else
	return false;
#endif // GTA_REPLAY
}

bool CFileDataProvider::hasDeleteMarkedFilesFailed()
{
#if GTA_REPLAY
	bool deleteSuccessful = false;

	bool const c_callSuccessful = CReplayMgr::CheckMultiDelete( deleteSuccessful );

	return c_callSuccessful && !deleteSuccessful;
#else
	return false;
#endif
}

bool CFileDataProvider::hasDeleteMarkedFilesFinished()
{
#if GTA_REPLAY
	bool deleteSuccessful = false;
	bool const c_callSuccessful = CReplayMgr::CheckMultiDelete( deleteSuccessful );

	return c_callSuccessful && deleteSuccessful;
#else
	return false;
#endif
}

u32 CFileDataProvider::getCountOfFilesToDelete() const
{
#if GTA_REPLAY
	u32 noofFiles = CReplayMgr::GetCountOfFilesToDelete(m_filter.getBuffer());
	return noofFiles;
#else
	return 0;
#endif // GTA_REPLAY
}

size_t CFileDataProvider::getFileCount() const
{
	return m_fileData.size();
}

size_t CFileDataProvider::getFileCountForUser( u64 const userId ) const
{
	size_t fileCount = 0;

	size_t const c_totalFiles = getFileCount();
	for( int index = 0; index < c_totalFiles; ++index )
	{
		CFileData const & fileData = m_fileData[ index ];
		if( fileData.getUserId() == userId )
		{
			++fileCount;
		}
	}

	return fileCount;
}

CFileDataProvider::FileDataIndex CFileDataProvider::getIndexForFileName( char const * const filename, bool const ignoreExtension, u64 const userId  ) const
{
	FileDataIndex returnIndex = FileIndexInvald;

	uiAssertf( filename, "CFileDataProvider::getIndexForFileName - Null file name!" );
	if( filename )
	{
        size_t const c_fileCount = getFileCount();
		for( int index = 0; index < c_fileCount; ++index )
		{
			CFileData const & fileData = m_fileData[ index ];

			if (userId == 0 || userId == fileData.getUserId() )
			{
                char const * const c_currentFileName = fileData.getFilename();
				if( ( ignoreExtension && fwTextUtil::ComapreFileNamesIgnoreExtension( c_currentFileName, filename ) ) ||
					strcmpi( c_currentFileName, filename ) == 0 )
				{
					returnIndex = index;
					break;
				}
			}
		}
	}

	return returnIndex;
}

CFileDataProvider::FileDataIndex CFileDataProvider::getIndexForFiledata( CFileData const& fileData ) const
{
	FileDataIndex returnIndex = FileIndexInvald;

    size_t const c_fileCount = getFileCount();
	for( int index = 0; index < c_fileCount; ++index )
	{
		CFileData const & currentFileData = m_fileData[ index ];
		if( &currentFileData == &fileData )
		{
			returnIndex = index;
			break;
		}
	}

	return returnIndex;
}

CFileDataProvider::FileDataIndex CFileDataProvider::getIndexForDisplayName( char const * const displayName, u64 const userId ) const
{
	FileDataIndex returnIndex = FileIndexInvald;

	if( uiVerifyf( displayName, "CFileDataProvider::getIndexForDisplayName - Null display name!" ) )
	{
		for( int index = 0; index < getFileCount(); ++index )
		{
			CFileData const & fileData = m_fileData[ index ];

			if (userId == 0 || userId == fileData.getUserId() )
			{
				if( strcmpi( fileData.getDisplayName(), displayName ) == 0 )
				{
					returnIndex = index;
					break;
				}
			}


		}
	}

	return returnIndex;
}

CFileData const * CFileDataProvider::getFileData( FileDataIndex const index ) const
{
	uiAssertf( isInitialized(), "CFileDataProvider::getFileData - Attempting to access file data before initialized!" );
	uiAssertf( isValidIndex( index ), "CFileDataProvider::getFileData - Invalid index %d!", index );

	return isInitialized() && isValidIndex( index ) ? &m_fileData[ index ] : NULL;
}

bool CFileDataProvider::getDisplayName( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;
	CFileData const * fileData = getFileData( index );

	if( fileData )
	{
		safecpy( out_buffer, fileData->getDisplayName() );
	}

	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;
}

bool CFileDataProvider::getFileName( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;
	CFileData const * fileData = getFileData( index );

	if( fileData )
	{
		safecpy( out_buffer, fileData->getFilename() );
	}

	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;
}

u64 CFileDataProvider::getFileUserId( FileDataIndex const index ) const
{
	CFileData const * fileData = getFileData( index );
	return fileData ? fileData->getUserId() : 0;
}

bool CFileDataProvider::getFileUserId( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	CFileData const * fileData = getFileData( index );

	return getFileUserId(*fileData, out_buffer);
}

bool CFileDataProvider::getFileUserId( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;
	u64 userId = fileData.getUserId();
	if (userId)
	{
#if RSG_ORBIS
		u32 userId32 = static_cast<u32>(userId);
		formatf_n( out_buffer, RAGE_MAX_PATH, "%u", userId32 );
#else
		formatf_n( out_buffer, RAGE_MAX_PATH, "%" I64FMT "u", userId );
#endif
	}

	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;
}

u64 CFileDataProvider::getFileSize( FileDataIndex const index ) const
{
	CFileData const * fileData = getFileData( index );
	return fileData ? fileData->getSize() : 0;
}

float CFileDataProvider::getExtraDataFloat( FileDataIndex const index ) const
{
	CFileData const * fileData = getFileData( index );
	return fileData ? fileData->getExtraDataFloat() : 0.f;
}

u32 CFileDataProvider::getExtraDataU32( FileDataIndex const index ) const
{
	CFileData const * fileData = getFileData( index );
	return fileData ? fileData->getExtraDataU32() : 0;
}

bool CFileDataProvider::getFileDateString( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;
	CFileData const * fileData = getFileData( index );

	if( fileData )
	{
		CTextConversion::ConvertFileTimeToLongString( out_buffer, RAGE_MAX_PATH, fileData->getLastWriteTime() );
	}

	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;
}

bool CFileDataProvider::getFilePath( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	CFileData const * fileData = getFileData( index );
	bool const c_result = fileData ? getFilePath( *fileData, out_buffer ) : false;

	return c_result;
}

bool CFileDataProvider::getFilePath( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;

	char path[ RAGE_MAX_PATH ];
	if (getFileDirectory(fileData, path))
	{
		formatf_n( out_buffer, RAGE_MAX_PATH, "%s%s", path, fileData.getFilename() );
	}

	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;
}

bool CFileDataProvider::getFileDirectory( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	CFileData const * fileData = getFileData( index );
	bool const c_result = fileData ? getFileDirectory( *fileData, out_buffer ) : false;
	
	return c_result;
}

bool CFileDataProvider::getFileDirectory( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;

#if SEE_ALL_USERS_FILES
	
	char userIDString[ RAGE_MAX_PATH ];
	if( getFileUserId( fileData, userIDString ) )
	{
		formatf_n( out_buffer, RAGE_MAX_PATH, "%s%s/", m_directoryPath.getBuffer(), userIDString );
	}
#if DO_CUSTOM_WORKING_DIR
	else
	{
		formatf_n( out_buffer, RAGE_MAX_PATH, "%s", m_directoryPath.getBuffer() );
	}
#endif // DO_CUSTOM_WORKING_DIR
#else

	(void)fileData;
	formatf_n( out_buffer, RAGE_MAX_PATH, "%s", m_directoryPath.getBuffer() );

#endif
	
	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;
}

bool CFileDataProvider::doesFileExistInProvider( char const * const filename, bool const checkDisplayName, u64 const userId ) const
{
	FileDataIndex fileDataIndex = FileIndexInvald;

	if( checkDisplayName )
	{
		fileDataIndex = getIndexForDisplayName( filename, userId );
	}
	else
	{
		fileDataIndex = getIndexForFileName( filename, true, userId );
	}

	bool const c_fileExists = fileDataIndex >= 0;
	return c_fileExists;
}

bool CFileDataProvider::doesFileExist( char const * const filename, char const * const dirPath ) const
{
	bool doesExist = false;

	if( isInitialized() && dirPath && filename )
	{
		PathBuffer fullPath( dirPath );
        fullPath += filename;

		FileHandle fileHandle = CFileMgr::OpenFile( fullPath.getBuffer() );
		doesExist = CFileMgr::IsValidFileHandle(fileHandle);

		if( doesExist )
		{
			CFileMgr::CloseFile( fileHandle );
		}
#if RSG_ORBIS
		else
		{
			int sceHandle = sceKernelOpen(fullPath.getBuffer(), SCE_KERNEL_O_RDONLY, SCE_KERNEL_S_INONE);
			doesExist = (sceHandle >= SCE_OK);
			if (doesExist)
			{
				sceKernelClose(sceHandle);
			}
		}
#endif
	}

	return doesExist;
}

bool CFileDataProvider::startDeleteFile( FileDataIndex const index )
{
	bool deleteStarted = false;

	PathBuffer path;
	if( getFilePath( index, path.getBufferRef() ) )
	{
		deleteStarted = DeleteFunctionImpl( path.getBuffer() );

		if( deleteStarted ) 
		{
			RemoveEntry( index );
		}
	}

	return deleteStarted;
}

bool CFileDataProvider::hasDeleteFileFailed() const
{
	bool deleteSuccessful = false;
	bool const c_callSuccessful = CheckDeleteFunctionImpl( deleteSuccessful );

	return c_callSuccessful && !deleteSuccessful;
}
bool CFileDataProvider::hasDeleteFileFinished() const
{
	bool deleteSuccessful = false;
	bool const c_callSuccessful = CheckDeleteFunctionImpl( deleteSuccessful );

	return c_callSuccessful && deleteSuccessful;
}

void CFileDataProvider::refresh()
{
	refreshInternal();
}

bool CFileDataProvider::isValidIndex( FileDataIndex const index ) const
{
	return index >= 0 && index < getFileCount();
}

static fiHandle FindFileBegin(const char *filename,fiFindData &outData, fiDevice const * device)
{
	return device->FindFileBegin( filename, outData );
}

static bool FindFileNext(fiHandle handle,fiFindData &outData, fiDevice const * device)
{
	return device->FindFileNext( handle, outData );
}

bool CFileDataProvider::EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter )
{
	bool success = false;

	fiDevice const * c_dev = filepath ? fiDevice::GetDevice( filepath ) : NULL;
	if( c_dev )
	{
		// As long as we got the device, we have succeeded since we may not find any files to match our filter.
		success = true;

		// Enumerate directory for files
		fiFindData findData;

		CFileData scratchData;

		fiHandle hFileHandle = fiHandleInvalid;
		bool validDataFound = true;

		char name[RAGE_MAX_PATH];
		DisplayName displayName;
		PathBuffer pathBuffer;
		FileDataIndex existingIndex;

		// todo - maximum amount of files allowed?
		for( hFileHandle = FindFileBegin( filepath, findData, c_dev );
			fiIsValidHandle( hFileHandle ) && validDataFound; 
			validDataFound = FindFileNext( hFileHandle, findData, c_dev ) )
		{
			bool fileIsInvalid = false;
			bool attemptToAddFile = true;
			safecpy( name, findData.m_Name );

			if( ( filter == NULL || strstr( strlwr( name ), filter ) != NULL ) && 
				strcmp( findData.m_Name, "." ) != 0 && strcmp( findData.m_Name, ".." ) != 0 ) 
			{
				if( ( strlen( findData.m_Name ) + 1 ) < scratchData.getMaxFilenameSizeBytes() )
				{	
					// Check if this file already exists, if it does check if it changed.
					existingIndex = getIndexForFileName( findData.m_Name, false );
					if( existingIndex != FileIndexInvald )
					{
						CFileData& oldData = fileList[existingIndex];
						if( oldData.getLastWriteTime() != findData.m_LastWriteTime )
						{
							fileList.erase( fileList.begin() + existingIndex );
						}
						else
						{
							attemptToAddFile = false;
						}
					}

					if( attemptToAddFile )
					{
						if( ValidateFileAndPopulateCustomData( filepath, findData.m_Name, scratchData ) )
						{
							scratchData.setFilename( findData.m_Name );

							displayName = findData.m_Name;
							fwTextUtil::TruncateFileExtension( displayName.getWritableBuffer() );
							scratchData.setDisplayName( displayName.getBuffer() );

							scratchData.setLastWriteTime( findData.m_LastWriteTime );
							scratchData.setSize( findData.m_Size );

							//! Default enum function assumes all files belong to me because the file
							//! ownership is all janky and not designed properly
							scratchData.setUserId( getActiveUserId() );

							fileList.PushAndGrow( scratchData );
						}
						else
						{
							uiWarningf( "CFileDataProvider::EnumerationFunctionImpl - File %s matches filters but is not a valid file!", findData.m_Name );
							fileIsInvalid = true;	
						}
					}
				}
				else
				{
					uiWarningf( "CFileDataProvider::EnumerationFunctionImpl - File %s has too long a name! Max name size is be %" SIZETFMT "u" , 
                        findData.m_Name, scratchData.getMaxFilenameSizeBytes() );
					fileIsInvalid = true;
				}

				if( fileIsInvalid && DeleteInvalidFilesOnEnum() )
				{
					pathBuffer.sprintf( "%s%s", m_directoryPath.getBuffer(), findData.m_Name );
					uiWarningf( "CFileDataProvider::EnumerationFunctionImpl - Deleting %s as it is invalid", pathBuffer.getBuffer() );

					DeleteFunctionImpl( pathBuffer.getBuffer() );
				}
			}
		}

		if( fiIsValidHandle( hFileHandle ) )
		{
			c_dev->FindFileEnd( hFileHandle );
		}
	}

	return success;
}

bool CFileDataProvider::CheckEnumerationFunctionImpl( bool& result ) const
{
	result = true;
	return true;
}

bool CFileDataProvider::ValidateFileAndPopulateCustomData( char const * const directoryPath, char const * const fileName, CFileData& out_data )
{
	bool const c_success = directoryPath != NULL && fileName != NULL;
	(void)out_data;

	return c_success;
}

bool CFileDataProvider::DeleteFunctionImpl( const char* filepath )
{
	bool hasDeleted = false;

	if( filepath )
	{
#if RSG_PC || RSG_DURANGO
		USES_CONVERSION;
		wchar_t const * const wideName = UTF8_TO_UTF16( filepath );
		hasDeleted = DeleteFileW( wideName ) == TRUE;
#endif
	}

	return hasDeleted;
}

bool CFileDataProvider::CheckDeleteFunctionImpl( bool& result ) const
{
	result = true;
	return true;
}

void CFileDataProvider::refreshInternal()
{
	if( isInitialized() )
	{
		PathBuffer backupPath( GetDirectoryPath() );
		PathBuffer backupFilter( GetFilter() );
		u64 const c_activeUserId( getActiveUserId() );

		shutdown();
		initialize( backupPath.getBuffer(), backupFilter.getBuffer(), c_activeUserId );
	}	
}

bool CFileDataProvider::repopulate()
{
	return populate();
}

bool CFileDataProvider::populate()
{
	bool const c_success = EnumerationFunctionImpl( GetDirectoryPath(), m_fileData, GetFilter() );
	return c_success;
}

void CFileDataProvider::RemoveEntry( FileDataIndex const index )
{
	if( isInitialized() && isValidIndex( index ) )
	{
		m_fileData.erase( m_fileData.begin() + index );
	}
}
