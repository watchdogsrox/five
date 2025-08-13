/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoFileDataProvider.h
// PURPOSE : Provides access to details relevant to video files
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

#ifndef VIDEO_FILE_DATA_PROVIDER_H_
#define VIDEO_FILE_DATA_PROVIDER_H_

// game
#include "ThumbnailFileDataProvider.h"
#include "VideoFileDataEnumerator.h"

#if RSG_ORBIS
typedef atArray<CFileDataVideo> VideoFileStorage;
#endif

class CFileData;

class CVideoFileDataProvider : public CThumbnailFileDataProvider
{
public:
	CVideoFileDataProvider( IImageLoader& imageLoader );
	virtual ~CVideoFileDataProvider();

	// PURPOSE: Initialize the provider and populate with file data for the given directory
	// PARAMS:
	//		directoryPath - Directory to check for file data
	//		filter - Text pattern to filter full filename & extension on. Pass null to provide no filtering.
	// RETURNS:
	//		True if successful, false otherwise.
	virtual bool initialize( char const * const directoryPath, char const * const filter, u64 const activeUserId );

	virtual bool EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter );

	virtual bool DeleteFunctionImpl( const char* filepath );
	virtual bool CheckDeleteFunctionImpl( bool& result ) const;

	virtual void releaseAllThumbnails();

	virtual bool getFilePath( FileDataIndex const index, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	// PURPOSE: Generates a full path from the given file data
	// PARAMS:
	//		fileData - File data to generate full path for
	//		out_buffer - Output buffer to populate
	// RETURNS:
	//		True if successful, false otherwise
	virtual bool getFilePath( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	virtual bool getFileDirectory( CFileData const& fileData, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;

	u32 GetVideoDurationMs( FileDataIndex const index ) const;
#if RSG_ORBIS
	virtual bool CheckEnumerationFunctionImpl( bool& result ) const;

	s64 GetVideoContentId( FileDataIndex const index ) const;

	virtual bool requestThumbnail( FileDataIndex const fileIndex );
	virtual CFileDataProvider::FileDataIndex getIndexForDisplayName( char const * const displayName, u64 const userId = 0) const;

	void TempAddVideoDisplayName(const char* title);
#endif
protected:
	virtual bool ValidateFileAndPopulateCustomData( char const * const directoryPath, char const * const fileName, CFileData& out_data );

	virtual bool DeleteInvalidFilesOnEnum() const 
	{ 
#if RSG_DURANGO
		return true; 
#else
		return false;
#endif
	}

private: // methods

	virtual bool generateThumbnailPath( char const * const in_filePath, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	virtual bool generateThumbnailPath( char const * const in_path, char const * const in_fileName, 
		char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] ) const;

#if RSG_DURANGO
	bool BuildVideoList( const char* filepath, FileDataStorage& fileList, const char* filter, bool initialEntry = false );
#endif

#if RSG_ORBIS
	CFileDataVideo const * getVideoData( FileDataIndex const index ) const;
	CFileDataVideo const * getVideoData( CFileData const& fileData ) const;

	virtual void preCleanup();
	virtual void RemoveEntry( FileDataIndex const index );

	CVideoFileDataEnumerator m_videoFileDataEnumerator;
	VideoFileStorage	m_videoData;
#endif
};

#endif // VIDEO_FILE_DATA_PROVIDER_H_

#endif // defined(VIDEO_PLAYBACK_ENABLED) && VIDEO_PLAYBACK_ENABLED

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
