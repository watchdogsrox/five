/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ImposedImageHandler.cpp
// PURPOSE : Wraps an imposed DownloadableTextureManager and provides functionality 
//			 for loading/unloading images.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "file/limits.h"
#include "string/string.h"

// framework
#include "fwlocalisation/textUtil.h"
#include "fwscene/stores/imposedTextureDictionary.h"
#include "fwscene/stores/txdstore.h"

// game
#include "frontend/ui_channel.h"
#include "ImposedImageHandler.h"
#include "ImposedImageEntry.h"

FRONTEND_OPTIMISATIONS();

CImposedImageHandler::CImposedImageHandler()
{

}

CImposedImageHandler::~CImposedImageHandler()
{

}

void CImposedImageHandler::cleanup()
{
	while( m_imageRequests.size() > 0 )
	{
		CImposedImageEntry* entry = m_imageRequests[ 0 ];
		if( entry )
		{
			m_imageRequests.erase( m_imageRequests.begin() );
			delete entry;
		}
	}

	m_imageRequests.Reset();
}

bool CImposedImageHandler::isImageRequested( atFinalHashString const& textureName ) const
{
	return getImageEntryInternal( textureName ) != NULL;
}

bool CImposedImageHandler::isImageLoaded( atFinalHashString const& textureName ) const
{
	CImposedImageEntry const* entry = getImageEntryInternal( textureName );
	return entry && entry->isLoaded();
}

bool CImposedImageHandler::hasImageRequestFailed( atFinalHashString const& textureName ) const
{
	CImposedImageEntry const* entry = getImageEntryInternal( textureName );
	return entry ? entry->hasLoadFailed() : true; // Assume failure if not requested
}

grcTexture const* CImposedImageHandler::getTexture( atFinalHashString const& textureName ) const
{
	grcTexture const* resultTexture = NULL;

	strLocalIndex const txdSlotIndex = g_TxdStore.FindSlot( textureName.GetHash() );
	fwTxdDef const* txdDef = txdSlotIndex != INDEX_NONE ? g_TxdStore.GetSlot( txdSlotIndex ) : NULL;
	if( txdDef && txdDef->m_pObject )
	{
		resultTexture = txdDef->m_pObject->Lookup( textureName );
	}

	return resultTexture;
}


bool CImposedImageHandler::canRequestImages() const
{
    bool const c_canRequest = !fwImposedTextureDictionary::IsAnyPendingCleanup() && !DOWNLOADABLETEXTUREMGR.IsAnyRequestPendingRelease();
    return c_canRequest;
}

bool CImposedImageHandler::requestImage( char const * const filename, atFinalHashString const& textureName, u32 const downscale )
{
	bool success = false;

	if( uiVerifyf( filename, "CImposedImageHandler::requestImage - Invalid path specified!" ) )
	{
		CImposedImageEntry* textureNameEntry = getImageEntry( textureName );
		if( uiVerifyf( !textureNameEntry, "CImposedImageHandler::requestImage - Texture already exists with this name!" ) )
		{
			CImposedImageEntry* targetEntry = rage_new CImposedImageEntry( filename, textureName, textureName, VideoResManager::GetDownscaleFactor( downscale ) );

			uiAssertf( targetEntry, "CImposedImageHandler::requestImage - Unable to allocate new image entry!" );
			if( targetEntry )
			{
				m_imageRequests.PushAndGrow( targetEntry );
				success = true;
			}
		}
	}

	return success;
}

void CImposedImageHandler::releaseImage( atFinalHashString const& textureName )
{
	uiAssertf( textureName.IsNotNull(), "CImposedImageHandler::requestImage - Null object name!" );
	if( textureName.IsNotNull() )
	{
		CImposedImageEntry** itr = m_imageRequests.begin();
		while( itr && itr != m_imageRequests.end() )
		{
			CImposedImageEntry* entry = *itr;
			if( entry && entry->getTextureName() == textureName )	
			{
				m_imageRequests.erase( itr );
				delete entry;
				break;
			}

			++itr;
		}
	}
}

void CImposedImageHandler::releaseAllImages()
{
	cleanup();
}

CImposedImageEntry* CImposedImageHandler::getImageEntry( atFinalHashString const& textureName )
{
	CImposedImageEntry* returnEntry = const_cast<CImposedImageEntry*>( getImageEntryInternal( textureName ) );
	return returnEntry;
}
 
CImposedImageEntry const* CImposedImageHandler::getImageEntryInternal( atFinalHashString const& textureName ) const
{
	CImposedImageEntry const* returnEntry = 0;

	for( u32 index = 0; index < m_imageRequests.size(); ++index )
	{
		CImposedImageEntry const* entry = m_imageRequests[ index ];
		if( entry && entry->getTextureName().GetHash() == textureName.GetHash() )
		{
			returnEntry = entry;
			break;
		}
	}

	return returnEntry;
}