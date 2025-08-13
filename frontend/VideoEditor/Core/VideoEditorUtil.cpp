/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoEditorUtil.cpp
// PURPOSE : Utility functions for the video editor
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoEditorUtil.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// rage
#include "string/string.h"

// framework
#include "fwlocalisation/textTypes.h"
#include "fwlocalisation/textUtil.h"
#include "fwutil/ProfanityFilter.h"

// game
#include "frontend/ui_channel.h"
#include "replaycoordinator/storage/Clip.h"
#include "replaycoordinator/storage/Montage.h"

#include "control/replay/file/ReplayFileManager.h"
#if RSG_PC
#include "control/replay/file/FileStorePC.h"
#endif
#if RSG_ORBIS
#include "control/replay/file/FileStoreOrbis.h"
#endif
#if RSG_DURANGO
#include "control/replay/File/FileStoreDurango.h"
#endif

FRONTEND_OPTIMISATIONS();

bool CVideoEditorUtil::IsValidSaveName( char const * const szFilename, u32 const maxByteCount )
{
    u32 const c_byteCount = szFilename ? fwTextUtil::GetByteCount( szFilename ) : 0;
    fwProfanityFilter::eRESULT const c_resultReservedTerms = szFilename ? fwProfanityFilter::GetInstance().CheckForReservedTermsOnly( szFilename ) : fwProfanityFilter::RESULT_VALID;
    fwProfanityFilter::eRESULT const c_resultReservedChars = szFilename ? fwProfanityFilter::GetInstance().CheckForUnsupportedFileSystemCharactersOnly( szFilename ) : fwProfanityFilter::RESULT_VALID;

    bool const c_isValid = szFilename && c_byteCount > 0 && c_byteCount < maxByteCount &&
        c_resultReservedTerms == fwProfanityFilter::RESULT_VALID && c_resultReservedChars == fwProfanityFilter::RESULT_VALID;

    return c_isValid;
}

bool CVideoEditorUtil::generateMontageClipThumbnailPath( CMontage const& montage, u32 clipIndex, 
														char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] )
{
	out_path[0] = rage::TEXT_CHAR_TERMINATOR;
	out_fileName[0] = rage::TEXT_CHAR_TERMINATOR;

	bool success = false;

	CClip const * const c_thumbnailClip = montage.GetClip( clipIndex );
	uiAssertf( c_thumbnailClip, "CImposedImageEntry::generateMontageClipThumbnailPath - No clip in montage!" );
	char const * const fileName = c_thumbnailClip ? c_thumbnailClip->GetName() : NULL;

	if( fileName )
	{
		CVideoEditorUtil::getClipDirectoryForUser( c_thumbnailClip->GetOwnerId(), out_path );

		formatf_n( out_fileName, "%s%s",fileName, THUMBNAIL_FILE_EXTENSION );
		success = true;
	}
	else
	{
		success = false;
	}

	return success;
}

void CVideoEditorUtil::getClipDirectoryForUser( u64 const userId, char (&out_buffer)[ RAGE_MAX_PATH ] )
{
	ReplayFileManager::getClipDirectoryForUser( userId, out_buffer );
}

void CVideoEditorUtil::getClipDirectory(char (&out_buffer)[ RAGE_MAX_PATH ])
{
	ReplayFileManager::getClipDirectory( out_buffer, false );
}

void CVideoEditorUtil::getVideoProjectDirectory(char (&out_buffer)[ RAGE_MAX_PATH ])
{
	ReplayFileManager::getVideoProjectDirectory( out_buffer, false );
}

void CVideoEditorUtil::getVideoProjectDirectoryForCurrentUser(char (&out_buffer)[ RAGE_MAX_PATH ])
{
	ReplayFileManager::getVideoProjectDirectory( out_buffer, true );
}

void CVideoEditorUtil::getVideoDirectory(char (&out_buffer)[ RAGE_MAX_PATH ])
{
	ReplayFileManager::getVideoDirectory( out_buffer, false );
}

void CVideoEditorUtil::refreshVideoDirectory()
{
	ReplayFileManager::InitVideoOutputPath();
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
