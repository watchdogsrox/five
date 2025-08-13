/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoFileDataProvider.cpp
// PURPOSE : Provides access to details relevant to video files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoFileDataProvider.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#if defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

// rage
#include "atl/string.h"
#include "file/cachepartition.h"
#include "file/asset.h"

// framework
#include "fwlocalisation/textUtil.h"
#include "video/media_transcoding_allocator.h"
#include "video/VideoPlaybackThumbnailManager.h"
#include "video/VideoPlaybackSettings.h"

// game
#include "system/FileMgr.h"

#include "control/replay/file/ReplayFileManager.h"

#if RSG_ORBIS
#include "renderer/rendertargets.h"
#include "video/contentDelete_Orbis.h"
#elif RSG_DURANGO
#include "control/replay/File/FileStoreDurango.h"
#endif

FRONTEND_OPTIMISATIONS();

CVideoFileDataProvider::CVideoFileDataProvider( IImageLoader& imageLoader )
	: CThumbnailFileDataProvider( imageLoader )
{
#if RSG_ORBIS
	m_videoFileDataEnumerator.StartThread();
#endif
}

CVideoFileDataProvider::~CVideoFileDataProvider()
{
#if RSG_ORBIS
	m_videoFileDataEnumerator.StopThread();
#endif
}

bool CVideoFileDataProvider::initialize( char const * const directoryPath, char const * const filter, u64 const activeUserId )
{
	char path[RAGE_MAX_PATH];

	ReplayFileManager::FixName(path, directoryPath);

	return CThumbnailFileDataProvider::initialize( path, filter, activeUserId );
}


bool CVideoFileDataProvider::EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter )
{

#if RSG_ORBIS

	m_videoFileDataEnumerator.StartEnumeration(&m_fileData, &m_videoData, m_activeUserId);
	return true;

#elif RSG_PC || RSG_DURANGO


	MediaCommon::ePrepareState threadPrepareState( MediaCommon::STATE_NONE );
	threadPrepareState = MediaCommon::PrepareThread();

#if RSG_PC
	bool const c_success =  videoVerifyf( threadPrepareState == MediaCommon::STATE_PREPARED, "CVideoFileDataProvider::EnumerationFunctionImpl - "
		"Failed to initialize media foundation, thread prepare state was %d", threadPrepareState ) ? 
		CFileDataProvider::EnumerationFunctionImpl( filepath, fileList, filter ) : false;
#else
	bool const c_success =  videoVerifyf( threadPrepareState == MediaCommon::STATE_PREPARED, "CVideoFileDataProvider::EnumerationFunctionImpl - "
		"Failed to initialize media foundation, thread prepare state was %d", threadPrepareState ) ? 
		BuildVideoList( filepath, fileList, filter, true ) : false;
#endif

	MediaCommon::CleanupThread( threadPrepareState );

	return c_success;

#endif

}

bool CVideoFileDataProvider::DeleteFunctionImpl( const char* filepath )
{
#if RSG_ORBIS
	return CContentDelete::DoDeleteVideo(filepath);
#else
	return ReplayFileManager::StartDeleteFile( filepath );
#endif
}

bool CVideoFileDataProvider::CheckDeleteFunctionImpl( bool& result ) const
{
#if RSG_ORBIS
	result = true;
	return true;
#else
	return ReplayFileManager::CheckDeleteFile( result );
#endif
}

#if RSG_DURANGO
bool CVideoFileDataProvider::BuildVideoList( const char* filepath, FileDataStorage& fileList, const char* filter, bool initialEntry )
{
	bool success = false;

	videoDisplayf("BuildVideoList - %s", filepath);

	fiDevice const * c_dev = filepath ? fiDevice::GetDevice( filepath ) : NULL;
	if( c_dev )
	{
		// As long as we got the device, we have succeeded since we may not find any files to match our filter.
		success = true;

		u64 userId = 0;

		if( !initialEntry )
		{
			char pathBufferForUserId[RAGE_MAX_PATH];
			formatf(pathBufferForUserId, strlen(filepath), "%s", filepath);
			char* lastSlash = strrchr(pathBufferForUserId, '\\');
			if (lastSlash == NULL)
			{
				lastSlash = strrchr(pathBufferForUserId, '/');
			}

			if (lastSlash[1] >= '0' && lastSlash[1] <= '9')
			{
				userId = _strtoui64(&lastSlash[1], NULL, 10);
			}
		}

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
			s32 nameLength = static_cast<s32>(strlen(findData.m_Name));
			if( ( findData.m_Attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
			{
#if SEE_ALL_USERS_FILES
				// make sure directory is a proper user directory and not just '.' or '..'
				if (nameLength > REPLAYDIRMINLENGTH)
				{
					replayDisplayf("ENUMERATE CLIPS - directory - %s", findData.m_Name);
					// should enumerate this directory if requested
					char childPath[REPLAYPATHLENGTH] = { 0 };
					formatf_n( childPath, REPLAYPATHLENGTH, "%s%s/", filepath, findData.m_Name );
					BuildVideoList( childPath, fileList, filter );
				}
#endif
				continue;
			}

			// If the filename+path is greater than REPLAYPATHLENGTH, then discard (since we can't cache it & thus can't load the file later using the name)
			if( nameLength > (REPLAYPATHLENGTH - 1) )		// -1 for the zero terminator that will be required.
				continue;

#if SEE_ALL_USERS_FILES
			// if base directory, don't list any files ... shouldn't be any in here, but might catch old dev stuff
			if (initialEntry)
				continue;
#endif

			bool fileIsInvalid = false;
			bool attemptToAddFile = true;
			safecpy( name, findData.m_Name );

			if( ( filter == NULL || strstr( strlwr( name ), filter ) != NULL ) && 
				strcmp( findData.m_Name, "." ) != 0 && strcmp( findData.m_Name, ".." ) != 0 ) 
			{
				if( strlen( findData.m_Name ) + 1 < scratchData.getMaxFilenameSizeBytes() )
				{	
					// Check if this file already exists, if it does check if it changed.
					existingIndex = getIndexForFileName( findData.m_Name, false, userId );
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

							scratchData.setUserId( userId );

							fileList.PushAndGrow( scratchData );
						}
						else
						{
							uiWarningf( "CVideoFileDataProvider::BuildVideoList - File %s matches filters but is not a valid file!", findData.m_Name );
							fileIsInvalid = true;	
						}
					}
				}
				else
				{
					uiWarningf( "CVideoFileDataProvider::BuildVideoList - File %s has too long a name! Max name size is be %" SIZETFMT "u" , 
                        findData.m_Name, scratchData.getMaxFilenameSizeBytes() );

					fileIsInvalid = true;
				}

				if( fileIsInvalid && DeleteInvalidFilesOnEnum() )
				{
					pathBuffer.sprintf( "%s%s", filepath, findData.m_Name );
					uiWarningf( "CVideoFileDataProvider::BuildVideoList - Deleting %s as it is invalid", pathBuffer.getBuffer() );

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
#endif

void CVideoFileDataProvider::releaseAllThumbnails()
{
	VideoPlaybackThumbnailManager::releaseAllImages();
}

bool CVideoFileDataProvider::getFilePath( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	bool const result = CThumbnailFileDataProvider::getFilePath( index, out_buffer );
	return result;
}

bool CVideoFileDataProvider::getFilePath( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
#if RSG_ORBIS
	// on PS4, we build a full path from the av_content info, rather than a set folder, as the paths can be different 
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;
	CFileDataVideo const * videoData = getVideoData( fileData );

	if( videoData )
	{
		formatf_n( out_buffer, RAGE_MAX_PATH, "%s/%s", videoData->getPath(), fileData.getFilename() );
	}

	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;

#else

	bool const result = CThumbnailFileDataProvider::getFilePath( fileData, out_buffer );
	return result;

#endif
}

bool CVideoFileDataProvider::getFileDirectory( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
#if RSG_ORBIS
	// on PS4, we build a full path from the av_content info, rather than a set folder, as the paths can be different 
	out_buffer[0] = rage::TEXT_CHAR_TERMINATOR;
	CFileDataVideo const * videoData = getVideoData( fileData );

	if( videoData )
	{
		formatf_n( out_buffer, RAGE_MAX_PATH, "%s/", videoData->getPath() );
	}

	return out_buffer[0] != rage::TEXT_CHAR_TERMINATOR;

#else

	bool const result = CThumbnailFileDataProvider::getFileDirectory( fileData, out_buffer );
	return result;

#endif
}

u32 CVideoFileDataProvider::GetVideoDurationMs( FileDataIndex const index ) const
{
	CFileData const * fileData = getFileData( index );
	u32 const c_durationMs = fileData ? fileData->getExtraDataU32() : 0;

	return c_durationMs;
}

#if RSG_ORBIS
bool CVideoFileDataProvider::CheckEnumerationFunctionImpl( bool& result ) const
{
	return m_videoFileDataEnumerator.CheckEnumeration(result);
}

s64 CVideoFileDataProvider::GetVideoContentId( FileDataIndex const index ) const
{
	if( uiVerifyf( index < getFileCount(), "CVideoFileDataProvider::GetVideoContentId - Invalid file data index" ) )
	{
		PathBuffer path;

		CFileDataVideo const * videoData = getVideoData( index );
		if (videoData)
		{
			return videoData->getContentId();
		}
	}

	return 0;
}

#endif // RSG_ORBIS

bool CVideoFileDataProvider::ValidateFileAndPopulateCustomData( char const * const directoryPath, char const * const fileName, CFileData& out_data )
{
	bool valid = CThumbnailFileDataProvider::ValidateFileAndPopulateCustomData( directoryPath, fileName, out_data ) &&
		( ( MediaCommon::IsMp4Supported() && fwTextUtil::HasFileExtension( fileName, ".mp4") ) || 
		 ( MediaCommon::IsWmvSupported() && fwTextUtil::HasFileExtension(fileName, ".wmv") ) );

	if( valid )
	{
		PathBuffer path( directoryPath );
        path.append( fileName );

		u32 durationMs(0);
		valid = VideoPlayback::IsVideoSupported( path.getBuffer(), MAX_VIDEO_WIDTH, MAX_VIDEO_HEIGHT, MIN_VIDEO_DURATION_MS, NULL, NULL, &durationMs );
		out_data.setExtraDataU32( durationMs );
	}

	return valid;
}

bool CVideoFileDataProvider::generateThumbnailPath( char const * const in_filePath, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	bool success = false;

	if( uiVerifyf( in_filePath, "CVideoFileDataProvider::generateThumbnailPath - Null path provided!" ) )
	{
		safecpy( out_buffer, in_filePath );
		success = true;
	}
	return success;
}

bool CVideoFileDataProvider::generateThumbnailPath( char const * const in_path, char const * const in_fileName, 
												   char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] ) const
{
	bool success = false;

	if( uiVerifyf( in_path, "CVideoFileDataProvider::generateThumbnailPath - Null path provided!" ) && 
		uiVerifyf( in_fileName, "CVideoFileDataProvider::generateThumbnailPath - Null path provided!" ) )
	{
		safecpy( out_path, in_path );
		safecpy( out_fileName, in_fileName );

		success = true;
	}

	return success;
}

#if RSG_ORBIS
bool CVideoFileDataProvider::requestThumbnail( FileDataIndex const fileIndex )
{
	bool success = false;

	uiAssertf( isInitialized(), "CThumbnailFileDataProvider::requestThumbnail - Not initialized!" );
	if( isInitialized() )
	{
		char pathBuffer[ RAGE_MAX_PATH ];
		if( getFilePath( fileIndex, pathBuffer ) )
		{
			char sourcePathBuffer[ RAGE_MAX_PATH ];
			if( generateThumbnailPath( pathBuffer, sourcePathBuffer ) )
			{
				// Just re-used the path buffer to get the thumbnail name
				if( !isThumbnailRequested( fileIndex ) && getThumbnailName( fileIndex, pathBuffer ) )
				{
					atFinalHashString textureName( pathBuffer );

					// For PS4 loading thumbnails from av_content
					// we need the previous data that the other platforms need for searches and whatnot
					// but here we use the proper thumbnail location on av_content to actually load it
					CFileDataVideo const * videoData = getVideoData( fileIndex );
					if (videoData && videoData->getThumbnail() && (strlen(videoData->getThumbnail()) > 5) )
					{
						success = m_imageLoader.requestImage( videoData->getThumbnail(), textureName, VideoResManager::DOWNSCALE_HALF );
					}
				}
			}
		}
	}

	return success;
}


CFileDataProvider::FileDataIndex CVideoFileDataProvider::getIndexForDisplayName( char const * const displayName, u64 const /*userId*/ ) const
{
	FileDataIndex returnIndex = FileIndexInvald;
	returnIndex = CThumbnailFileDataProvider::getIndexForDisplayName(displayName);

	if (returnIndex != FileIndexInvald)
	{
		// if the file has a size of 0, then that means it's a recent display name that's pre-enumeration
		const CFileData* fileData = getFileData(returnIndex);
		if (fileData->getSize() == 0)
			return returnIndex;

		// if the file doesn't exist, it might have been deleted in the system software since the last enumeration 
		char pathBuffer[RAGE_MAX_PATH];
		if (getFilePath(returnIndex, pathBuffer))
		{
			if ( ASSET.Exists(pathBuffer, "mp4") || ASSET.Exists(pathBuffer, "wmv") )
				return returnIndex;
		}
	}

	return FileIndexInvald;
}

void CVideoFileDataProvider::TempAddVideoDisplayName(const char* title)
{
	// set a display name, and size of zero
	// we use the size to tell us not to look to carefully
	// need a temp addition to avoid big enumerations at the end of exports that can cause stalls
	CFileData scratchData;
	scratchData.setDisplayName( title );
	scratchData.setSize(0);
	m_fileData.PushAndGrow( scratchData );
}

CFileDataVideo const * CVideoFileDataProvider::getVideoData( FileDataIndex const index ) const
{
	uiAssertf( isInitialized(), "CVideoFileDataProvider::getVideoData - Attempting to access file data before initialized!" );
	uiAssertf( isValidIndex( index ), "CVideoFileDataProvider::getVideoData - Invalid index %d!", index );

	return isInitialized() && isValidIndex( index ) ? &m_videoData[ index ] : NULL;
}

CFileDataVideo const * CVideoFileDataProvider::getVideoData( CFileData const& fileData ) const
{
	uiAssertf( isInitialized(), "CVideoFileDataProvider::getVideoData - Attempting to access file data before initialized!" );

	FileDataIndex const c_dataIndex = getIndexForFiledata( fileData );
	uiAssertf( isValidIndex( c_dataIndex ), "CVideoFileDataProvider::getVideoData - Invalid index %d!", c_dataIndex );

	return isInitialized() && isValidIndex( c_dataIndex ) ? &m_videoData[ c_dataIndex ] : NULL;
}

void CVideoFileDataProvider::preCleanup()
{
	CThumbnailFileDataProvider::releaseAllThumbnails();

	m_videoData.Reset();
}

void CVideoFileDataProvider::RemoveEntry( FileDataIndex const index )
{
	if( isInitialized() && isValidIndex( index ) )
	{
		m_fileData.erase( m_fileData.begin() + index );
		m_videoData.erase( m_videoData.begin() + index);
	}
}


#endif

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
