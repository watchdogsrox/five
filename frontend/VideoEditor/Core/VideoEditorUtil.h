/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorUtil.h
// PURPOSE : Utility functions for the video editor
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_EDITOR_UTIL_H_
#define VIDEO_EDITOR_UTIL_H_

// rage
#include "file/limits.h"

class CMontage;

class CVideoEditorUtil
{
public:
    // TODO - move me to framework or rage project so non-replay systems can validate file names
    static bool IsValidSaveName( char const * const szFilename, u32 const maxByteCount );

	static bool generateMontageClipThumbnailPath( CMontage const& montage, u32 clipIndex, 
		char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] );

	static void getClipDirectoryForUser( u64 const userId, char (&out_buffer)[ RAGE_MAX_PATH ]);
	static void getClipDirectory(char (&out_buffer)[ RAGE_MAX_PATH ]);
	static void getVideoProjectDirectory(char (&out_buffer)[ RAGE_MAX_PATH ]);
	static void getVideoProjectDirectoryForCurrentUser(char (&out_buffer)[ RAGE_MAX_PATH ]);
	static void getVideoDirectory(char (&out_buffer)[ RAGE_MAX_PATH ]);
	static void refreshVideoDirectory();
};

#endif // VIDEO_EDITOR_UTIL_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
