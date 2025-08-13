/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : ImposedImageHandler.h
// PURPOSE : Wraps an imposed DownloadableTextureManager and provides functionality 
//			 for loading/unloading images.
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef IMPOSED_IMAGE_HANDLER_H_
#define IMPOSED_IMAGE_HANDLER_H_

//! TODO - Move this outside of Replay as not replay specific...

// rage
#include "atl/array.h"
#include "atl/hashstring.h"

// framework
#include "video/ImageLoader.h"

// game
#include "renderer/rendertargets.h"

namespace rage
{
	class grcTexture;
}

class CImposedImageEntry;

class CImposedImageHandler : public rage::IImageLoader
{
public:
	CImposedImageHandler();
	virtual ~CImposedImageHandler();

	void cleanup();

	virtual bool isImageRequested( atFinalHashString const& textureName ) const;
	virtual bool isImageLoaded( atFinalHashString const& textureName ) const;
	virtual bool hasImageRequestFailed( atFinalHashString const& textureName ) const;

	grcTexture const* getTexture( atFinalHashString const& textureName ) const;

    virtual bool canRequestImages() const;

	virtual bool requestImage( char const * const filename, atFinalHashString const& textureName, u32 const downscale );
	virtual void releaseImage( atFinalHashString const& textureName );
	virtual void releaseAllImages();

private: // declarations and variables
	typedef atArray< CImposedImageEntry* > ImageRequests;
	ImageRequests m_imageRequests;

private: // methods

	CImposedImageEntry* getImageEntry( atFinalHashString const& textureName );
	CImposedImageEntry const* getImageEntryInternal( atFinalHashString const& textureName ) const;

};

#endif // IMPOSED_IMAGE_HANDLER_H_
