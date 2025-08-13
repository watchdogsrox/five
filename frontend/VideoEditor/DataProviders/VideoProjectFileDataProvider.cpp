/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoProjectFileDataProvider.cpp
// PURPOSE : Provides access to details relevant to video project files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "VideoProjectFileDataProvider.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// game
#include "control/replay/replay.h"
#include "frontend/VideoEditor/Core/ImposedImageHandler.h"
#include "frontend/VideoEditor/Core/VideoEditorUtil.h"

FRONTEND_OPTIMISATIONS();

CVideoProjectFileDataProvider::CVideoProjectFileDataProvider( CImposedImageHandler& imageHandler )
	: CThumbnailFileDataProvider( imageHandler )
{

}

CVideoProjectFileDataProvider::~CVideoProjectFileDataProvider()
{

}

u32 CVideoProjectFileDataProvider::getMaxProjectCount() const
{
	return CReplayMgr::GetMaxProjects();
}

bool CVideoProjectFileDataProvider::EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter )
{
	return CReplayMgr::StartEnumerateProjectFiles( filepath, fileList, filter );
}

bool CVideoProjectFileDataProvider::CheckEnumerationFunctionImpl( bool& result ) const
{
	return CReplayMgr::CheckEnumerateProjectFiles( result );
}

bool CVideoProjectFileDataProvider::DeleteFunctionImpl( const char* filepath )
{
	return CReplayMgr::StartDeleteFile( filepath );
}

bool CVideoProjectFileDataProvider::CheckDeleteFunctionImpl( bool& result ) const
{
	return CReplayMgr::CheckDeleteFile( result );
}

bool CVideoProjectFileDataProvider::generateThumbnailPath( char const * const in_path, char const * const in_fileName, 
														  char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] ) const
{
	bool success = false;

	if( uiVerifyf( in_path, "CVideoProjectFileDataProvider::generateThumbnailPath - Null path provided!" ) && 
		uiVerifyf( in_fileName, "CVideoProjectFileDataProvider::generateThumbnailPath - Null path provided!" ) )
	{
		// We need to get the first clip from our project and use it's thumbnail
		CMontage* previewMontage = CMontage::Create();
		uiAssertf( previewMontage, "CVideoProjectFileDataProvider::createThumbnailRequest - Failed to create montage!" );
		if( previewMontage )
		{
            PathBuffer filePath( in_path );
            filePath.append( in_fileName );

			u64 const c_userId = ReplayFileManager::getUserIdFromPath( filePath );
			u64 const c_loadResult = previewMontage->Load( filePath, c_userId, true, NULL );
			success = ( c_loadResult & CReplayEditorParameters::PROJECT_LOAD_RESULT_FAILED ) == 0;
			success = success && previewMontage->GetClipCount() > 0;

			success = success && CVideoEditorUtil::generateMontageClipThumbnailPath( *previewMontage, 0, out_path, out_fileName );
			CMontage::Destroy( previewMontage );
		}

		success = true;
	}

	return success;
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
