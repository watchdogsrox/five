/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ImposedImageEntry.h
// PURPOSE : Represents an individual image to be imposed.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2013 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef IMPOSED_TEXTURE_ENTRY_H_
#define IMPOSED_TEXTURE_ENTRY_H_

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "file/limits.h"

// framework
#include "fwlocalisation/templateString.h"
#include "streaming/streamingmodule.h"

// game
#include "renderer/rendertargets.h"
#include "scene/DownloadableTextureManager.h"

class CImposedImageEntry
{
public:
	CImposedImageEntry( char const * const filePath, atFinalHashString const& textureName, atFinalHashString const& txdName, VideoResManager::eDownscaleFactor const downscale );
	~CImposedImageEntry();

	void Initialize( char const * const filePath, atFinalHashString const& textureName, atFinalHashString const& txdName, 
						VideoResManager::eDownscaleFactor const downscale );

	inline bool isInitialized() const { return getTextureName().IsNotNull(); }
	bool hasLoadFailed() const;
	bool isLoading() const;
	bool isLoaded() const;

	void Shutdown();

	inline atFinalHashString const& getTextureName() const { return m_downloadRequest.m_TxtAndTextureHash; }

protected: // methods
	void load();

private: // declarations and variables
	CTextureDownloadRequestDesc		m_downloadRequest;
	TextureDownloadRequestHandle	m_requestHandle;
	PathBuffer						m_filepath;
};

#endif // IMPOSED_TEXTURE_ENTRY_H_
