/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ImposedImageEntry.cpp
// PURPOSE : Represents an individual image to be imposed.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

//! TODO - Move this outside of Replay as not replay specific...

// game
#include "frontend/ui_channel.h"
#include "ImposedImageEntry.h"

FRONTEND_OPTIMISATIONS();

CImposedImageEntry::CImposedImageEntry( char const * const filePath, atFinalHashString const& textureName, atFinalHashString const& txdName,
										VideoResManager::eDownscaleFactor const downscale )
	: m_downloadRequest()
	, m_requestHandle( CTextureDownloadRequest::INVALID_HANDLE )
	, m_filepath()
{
	Initialize( filePath, textureName, txdName, downscale );
}

CImposedImageEntry::~CImposedImageEntry()
{
	Shutdown();
}

void CImposedImageEntry::Initialize( char const * const filePath, atFinalHashString const& textureName, atFinalHashString const& txdName, 
									VideoResManager::eDownscaleFactor const downscale )
{
	if( uiVerifyf( filePath, "CImposedImageEntry::Initialize - NULL file path" ) &&
		uiVerifyf( textureName.IsNotNull(), "CImposedImageEntry::Initialize - NULL texture name" ) &&
		uiVerifyf( textureName == txdName, "CImposedImageEntry::Initialize - texture mismatch" ) )
	{
		m_filepath.Set( filePath );

		m_downloadRequest.m_Type = CTextureDownloadRequestDesc::DISK_FILE;
		m_downloadRequest.m_CloudFilePath = m_filepath.getBuffer();
		m_downloadRequest.m_TxtAndTextureHash = textureName;
		m_downloadRequest.m_JPEGScalingFactor = downscale;

		load();
	}
}

bool CImposedImageEntry::hasLoadFailed() const
{
	return m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE ? DOWNLOADABLETEXTUREMGR.HasRequestFailed( m_requestHandle ) : true;
}

bool CImposedImageEntry::isLoading() const
{
	return m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE ? DOWNLOADABLETEXTUREMGR.IsRequestPending( m_requestHandle ) : false;
}

bool CImposedImageEntry::isLoaded() const
{
	return m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE ? DOWNLOADABLETEXTUREMGR.IsRequestReady( m_requestHandle ) : false;
}

void CImposedImageEntry::Shutdown()
{
	if( m_requestHandle != CTextureDownloadRequest::INVALID_HANDLE )
	{
		DOWNLOADABLETEXTUREMGR.ReleaseTextureDownloadRequest( m_requestHandle );
	}

	m_filepath.Clear();
	m_downloadRequest.m_TxtAndTextureHash.Clear();
	m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
}

void CImposedImageEntry::load()
{
	CTextureDownloadRequest::Status const c_status = DOWNLOADABLETEXTUREMGR.RequestTextureDownload( m_requestHandle, m_downloadRequest );

	Assertf( c_status != CTextureDownloadRequest::REQUEST_FAILED &&
		c_status != CTextureDownloadRequest::TRY_AGAIN_LATER, "CImposedImageEntry::load - Texture load request failed, returned %u", c_status );

	if( c_status == CTextureDownloadRequest::TRY_AGAIN_LATER )
	{
		Displayf( "CImposedImageEntry::load - Too many thumbnail requests made!" );
	}

	(void)c_status; // TODO - What happens if this fails? Other places just march on anyway or show corrupt texture icon...
}
