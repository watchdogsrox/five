/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ThumbnailFileDataProvider.cpp
// PURPOSE : Provides access to thumbnail image loading/access for files
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "file/limits.h"
#include "string/string.h"

// framework
#include "fwlocalisation/textUtil.h"
#include "video/ImageLoader.h"

// game
#include "renderer/rendertargets.h"
#include "ThumbnailFileDataProvider.h"

FRONTEND_OPTIMISATIONS();

CThumbnailFileDataProvider::CThumbnailFileDataProvider( rage::IImageLoader& imageLoader )
	: CFileDataProvider()
	, m_imageLoader( imageLoader )
{

}

CThumbnailFileDataProvider::~CThumbnailFileDataProvider()
{

}

bool CThumbnailFileDataProvider::getThumbnailName( FileDataIndex const fileIndex, char (&out_buffer)[ RAGE_MAX_PATH ] ) const
{
	char nameBuffer[ RAGE_MAX_PATH ];
	bool const success = getDisplayName( fileIndex, nameBuffer );
	if( success )
	{
		formatf( out_buffer, "thumb_%s%u", nameBuffer, (unsigned)fileIndex );
	}
	return success;
}

atFinalHashString CThumbnailFileDataProvider::getThumbnailName( FileDataIndex const fileIndex ) const
{
	atFinalHashString name;
	if( isInitialized() )
	{
		char nameBuffer[ RAGE_MAX_PATH ];
		if( getThumbnailName( fileIndex, nameBuffer ) )
		{
			name = nameBuffer;
		}
	}

	return name;
}

bool CThumbnailFileDataProvider::canRequestThumbnails() const
{
    bool const c_canRequest = m_imageLoader.canRequestImages();
    return c_canRequest;
}

// PURPOSE: Request that we load a thumbnail for the given file index and add it to our texture dictionary under the given name.
// PARAMS:
//		fileIndex - File we want the thumbnail of
//		textureName - name to register the texture under
// RETURNS:
//		True if successful, false otherwise
bool CThumbnailFileDataProvider::requestThumbnail( FileDataIndex const fileIndex )
{
    bool const c_thumbnailAlreadyRequested = isThumbnailRequested( fileIndex );
    bool success = c_thumbnailAlreadyRequested;

	if( uiVerifyf( isInitialized(), "CThumbnailFileDataProvider::requestThumbnail - Not initialized!" ) &&
		!c_thumbnailAlreadyRequested )
	{
		char nameBuffer[ RAGE_MAX_PATH ];
		if( getFileName( fileIndex, nameBuffer ) )
		{
			char directoryPathBuffer[ RAGE_MAX_PATH ];
			if (getFileDirectory(fileIndex, directoryPathBuffer))
			{
				char sourcePathBuffer[ RAGE_MAX_PATH ];
				char sourceNameBuffer[ RAGE_MAX_PATH ];
				if( generateThumbnailPath( directoryPathBuffer, nameBuffer, sourcePathBuffer, sourceNameBuffer ) &&
					doesFileExist( sourceNameBuffer, sourcePathBuffer ) )
				{
					PathBuffer fullPath( sourcePathBuffer );
                    fullPath += sourceNameBuffer;

					// Just re-used the nameBuffer to get the thumbnail name
					if( getThumbnailName( fileIndex, nameBuffer ) )
					{
						atFinalHashString textureName( nameBuffer );
						success = m_imageLoader.requestImage( fullPath.getBuffer(), textureName, VideoResManager::DOWNSCALE_HALF );
					}
				}
			}
		}
	}

	return success;
}

bool CThumbnailFileDataProvider::isThumbnailLoaded( FileDataIndex const fileIndex ) const
{
	bool isLoaded = false;

	atFinalHashString const textureName = getThumbnailName( fileIndex );
	if( textureName.IsNotNull() )
	{
		isLoaded = m_imageLoader.isImageLoaded( textureName );
	}

	return isLoaded;
}

bool CThumbnailFileDataProvider::isThumbnailRequested( FileDataIndex const fileIndex ) const
{
	bool isRequested = false;

	atFinalHashString const textureName = getThumbnailName( fileIndex );
	if( textureName.IsNotNull() )
	{
		isRequested = m_imageLoader.isImageRequested( textureName );
	}

	return isRequested;	
}

bool CThumbnailFileDataProvider::hasThumbnailLoadFailed( FileDataIndex const fileIndex ) const
{
	bool loadFailed = false;

	atFinalHashString const textureName = getThumbnailName( fileIndex );
	if( textureName.IsNotNull() )
	{
		loadFailed = m_imageLoader.hasImageRequestFailed( textureName );
	}

	return loadFailed;	
}

void CThumbnailFileDataProvider::releaseThumbnail( FileDataIndex const fileIndex )
{
	atFinalHashString const textureName = getThumbnailName( fileIndex );
	if( textureName.IsNotNull() )
	{
		if( uiVerifyf( isInitialized(), "CThumbnailFileDataProvider::releaseThumbnail - Not initialized!" ) )
		{
			m_imageLoader.releaseImage( textureName );	
		}
	}
}

void CThumbnailFileDataProvider::releaseAllThumbnails()
{
	for( int index = 0; index < getFileCount(); ++index )
	{
		if( isThumbnailRequested( index ) )
		{
			releaseThumbnail( index );
		}
	}
}

bool CThumbnailFileDataProvider::doesThumbnailExist( FileDataIndex const fileIndex ) const
{
	bool thumbnailExists = false;

	if( uiVerifyf( isInitialized(), "CThumbnailFileDataProvider::doesThumbnailExist - Not initialized!" ) )
	{
		char nameBuffer[ RAGE_MAX_PATH ];
		if( getFileName( fileIndex, nameBuffer ) )
		{
			char directoryPathBuffer[ RAGE_MAX_PATH ];
			if (getFileDirectory(fileIndex, directoryPathBuffer))
			{
				char sourcePathBuffer[ RAGE_MAX_PATH ];
				char sourceNameBuffer[ RAGE_MAX_PATH ];
				if( generateThumbnailPath( directoryPathBuffer, nameBuffer, sourcePathBuffer, sourceNameBuffer ) )
				{
					thumbnailExists = doesFileExist( sourceNameBuffer, sourcePathBuffer );
				}
			}
		}
	}

	return thumbnailExists;
}

void CThumbnailFileDataProvider::preCleanup()
{
	releaseAllThumbnails();
}
