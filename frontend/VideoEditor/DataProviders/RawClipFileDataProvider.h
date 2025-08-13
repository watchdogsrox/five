/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : RawClipFileDataProvider.h
// PURPOSE : Provides access to details relevant to raw clip files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef RAW_CLIP_FILE_DATA_PROVIDER_H_
#define RAW_CLIP_FILE_DATA_PROVIDER_H_

// game
#include "control/replay/File/device_replay.h"
#include "replaycoordinator/ReplayEditorParameters.h"
#include "ThumbnailFileDataProvider.h"
#include "control/replay/IReplayMarkerStorage.h"

class CImposedImageHandler;

class CRawClipFileDataProvider : public CThumbnailFileDataProvider
{
public:
	CRawClipFileDataProvider( CImposedImageHandler& imageHandler );
	virtual ~CRawClipFileDataProvider();

	void markClipAsFavourite( FileDataIndex const index, bool const isFavourite );
	void toggleClipFavouriteState( FileDataIndex const index );

	bool isClipFavourite( FileDataIndex const index ) const;
	bool isClipFavourite( CFileData const& fileData ) const;

	IReplayMarkerStorage::eEditRestrictionReason getClipRestrictions( FileDataIndex const index ) const;
	IReplayMarkerStorage::eEditRestrictionReason getClipRestrictions( CFileData const& fileData ) const;

	void saveClipFavourites();
	bool isSavingFavourites();

	ClipUID const& getClipUID( FileDataIndex const index ) const;
	ClipUID const& getClipUID( CFileData const& fileData ) const;

protected:
	virtual bool EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter );
	virtual bool CheckEnumerationFunctionImpl( bool& result ) const;

	virtual bool DeleteFunctionImpl( const char* filepath );
	virtual bool CheckDeleteFunctionImpl( bool& result ) const;

private: // methods

	virtual bool generateThumbnailPath( char const * const in_filePath, char (&out_buffer)[ RAGE_MAX_PATH ] ) const;
	virtual bool generateThumbnailPath( char const * const in_path, char const * const in_fileName, 
		char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] ) const;
};

#endif // RAW_CLIP_FILE_DATA_PROVIDER_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
