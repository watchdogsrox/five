/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoProjectFileDataProvider.h
// PURPOSE : Provides access to details relevant to video project files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_PROJECT_FILE_DATA_PROVIDER_H_
#define VIDEO_PROJECT_FILE_DATA_PROVIDER_H_

// game
#include "replaycoordinator/storage/Montage.h"
#include "ThumbnailFileDataProvider.h"

#define VIDEO_PROJECT_FILE_EXTENSION STR_MONTAGE_FILE_EXT

class CImposedImageHandler;

class CVideoProjectFileDataProvider : public CThumbnailFileDataProvider
{
public:
	CVideoProjectFileDataProvider( CImposedImageHandler& imageHandler );
	virtual ~CVideoProjectFileDataProvider();

	u32 getMaxProjectCount() const;

protected:
	virtual bool EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter );
	virtual bool CheckEnumerationFunctionImpl( bool& result ) const;

	virtual bool DeleteFunctionImpl( const char* filepath );
	virtual bool CheckDeleteFunctionImpl( bool& result ) const;

private: // methods
	virtual bool generateThumbnailPath( char const * const in_path, char const * const in_fileName, 
		char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] ) const;
};

#endif // VIDEO_PROJECT_FILE_DATA_PROVIDER_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
