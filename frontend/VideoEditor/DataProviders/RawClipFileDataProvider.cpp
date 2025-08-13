/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : RawClipFileDataProvider.cpp
// PURPOSE : Provides access to details relevant to raw clip files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "RawClipFileDataProvider.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

// game
#include "control/replay/replay.h"
#include "replaycoordinator/storage/Montage.h"
#include "frontend/VideoEditor/Core/ImposedImageHandler.h"

FRONTEND_OPTIMISATIONS();

CRawClipFileDataProvider::CRawClipFileDataProvider( CImposedImageHandler& imageHandler )
	: CThumbnailFileDataProvider( imageHandler )
{

}

CRawClipFileDataProvider::~CRawClipFileDataProvider()
{

}

void CRawClipFileDataProvider::markClipAsFavourite( FileDataIndex const index, bool const isFavourite )
{
	PathBuffer pathBuffer;
	if( getFilePath( index, pathBuffer.getBufferRef() ) )
	{
		if( isFavourite )
		{
			CReplayMgr::MarkAsFavourite( pathBuffer.getBuffer() );
		}
		else
		{
			CReplayMgr::UnmarkAsFavourite( pathBuffer.getBuffer() );
		}
	}
}

void CRawClipFileDataProvider::toggleClipFavouriteState( FileDataIndex const index )
{
	PathBuffer pathBuffer;
	if( getFilePath( index, pathBuffer.getBufferRef() ) )
	{
		bool const c_isFavourite = CReplayMgr::IsMarkedAsFavourite( pathBuffer.getBuffer() );

		if( c_isFavourite )
		{
			CReplayMgr::UnmarkAsFavourite( pathBuffer.getBuffer() );
		}
		else
		{
			CReplayMgr::MarkAsFavourite( pathBuffer.getBuffer() );
		}
	}
}

bool CRawClipFileDataProvider::isClipFavourite( FileDataIndex const index ) const
{
	CFileData const* fileData = getFileData( index );
	
	bool const c_isFavourite = fileData ? isClipFavourite( *fileData ) : false;
	return c_isFavourite;
}

bool CRawClipFileDataProvider::isClipFavourite( CFileData const& fileData ) const
{
	bool isFavourite = false;

	PathBuffer pathBuffer;
	if( getFilePath( fileData, pathBuffer.getBufferRef() ) )
	{
		isFavourite = CReplayMgr::IsMarkedAsFavourite( pathBuffer.getBuffer() );
	}

	return isFavourite;
}

IReplayMarkerStorage::eEditRestrictionReason CRawClipFileDataProvider::getClipRestrictions( FileDataIndex const index ) const
{
	CFileData const* fileData = getFileData( index );

	IReplayMarkerStorage::eEditRestrictionReason const c_restrictions = fileData ? getClipRestrictions( *fileData ) : IReplayMarkerStorage::EDIT_RESTRICTION_NONE;
	return c_restrictions;
}

IReplayMarkerStorage::eEditRestrictionReason CRawClipFileDataProvider::getClipRestrictions( CFileData const& fileData ) const
{
	IReplayMarkerStorage::eEditRestrictionReason restrictions = IReplayMarkerStorage::EDIT_RESTRICTION_NONE;

	ClipUID const c_uid = getClipUID( fileData );
	if( c_uid.IsValid() )
	{
		bool firstPersonCam = false;
		bool cutsceneCam = false;
		bool camBlocked = false;
		if (CReplayMgr::GetClipRestrictions( c_uid, &firstPersonCam, &cutsceneCam, &camBlocked ) != 0)
		{
			if (firstPersonCam)
				restrictions = IReplayMarkerStorage::EDIT_RESTRICTION_FIRST_PERSON;
			else if (cutsceneCam)
				restrictions = IReplayMarkerStorage::EDIT_RESTRICTION_CUTSCENE;
			else if (camBlocked)
				restrictions = IReplayMarkerStorage::EDIT_RESTRICTION_CAMERA_BLOCKED;
		}
	}

	return restrictions;
}

void CRawClipFileDataProvider::saveClipFavourites()
{
	if( !isSavingFavourites() )
	{
		CReplayMgr::StartUpdateFavourites();
	}
}

bool CRawClipFileDataProvider::isSavingFavourites()
{
	bool saveComplete = false;
	bool const c_actionInProcess = CReplayMgr::CheckUpdateFavourites( saveComplete );

	return c_actionInProcess && !saveComplete;
}

ClipUID const& CRawClipFileDataProvider::getClipUID( FileDataIndex const index ) const
{
	CFileData const* fileData = getFileData( index );
	return fileData ? getClipUID( *fileData ) : ClipUID::INVALID_UID;
}

ClipUID const& CRawClipFileDataProvider::getClipUID( CFileData const& fileData ) const
{
	ClipUID const * uid = CReplayMgr::GetClipUID( fileData.getUserId(), fileData.getDisplayName() );
	return uid ? *uid : ClipUID::INVALID_UID;
}

bool CRawClipFileDataProvider::EnumerationFunctionImpl( const char* filepath, FileDataStorage& fileList, const char* filter )
{
	return CReplayMgr::StartEnumerateClipFiles( filepath, fileList, filter );
}

bool CRawClipFileDataProvider::CheckEnumerationFunctionImpl( bool& result ) const
{
	return CReplayMgr::CheckEnumerateClipFiles( result );
}

bool CRawClipFileDataProvider::DeleteFunctionImpl( const char* filepath )
{
	return CReplayMgr::StartDeleteClipFile( filepath );
}

bool CRawClipFileDataProvider::CheckDeleteFunctionImpl( bool& result ) const
{
	return CReplayMgr::CheckDeleteClipFile( result );
}

bool CRawClipFileDataProvider::generateThumbnailPath( char const * const in_filePath, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	bool success = false;

	if( uiVerifyf( in_filePath, "CRawClipFileDataProvider::generateThumbnailPath - Null path provided!" ) )
	{
		safecpy( out_buffer, in_filePath );
		fwTextUtil::TruncateFileExtension( out_buffer );
		safecat( out_buffer, THUMBNAIL_FILE_EXTENSION, RAGE_MAX_PATH );

		success = true;
	}

	return success;
}

bool CRawClipFileDataProvider::generateThumbnailPath( char const * const in_path, char const * const in_fileName, 
													 char (&out_path)[ RAGE_MAX_PATH ], char (&out_fileName)[ RAGE_MAX_PATH ] ) const
{
	bool success = false;

	if( uiVerifyf( in_path, "CRawClipFileDataProvider::generateThumbnailPath - Null path provided!" ) && 
		uiVerifyf( in_fileName, "CRawClipFileDataProvider::generateThumbnailPath - Null path provided!" ) )
	{
		safecpy( out_path, in_path );

		safecpy( out_fileName, in_fileName );
		fwTextUtil::TruncateFileExtension( out_fileName );
		safecat( out_fileName, THUMBNAIL_FILE_EXTENSION, RAGE_MAX_PATH );

		success = true;
	}

	return success;
}

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
