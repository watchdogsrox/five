// 
// audio/music/musicactionfactory.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_MUSICACTIONFACTORY_H
#define AUD_MUSICACTIONFACTORY_H

#include "audioengine/metadatamanager.h"
#include "audioengine/metadataref.h"

class audMusicAction;
class audMusicActionFactory
{
public:
	static audMusicAction *Create(const audMetadataRef actionMetadataRef);
	static audMusicAction *Create(const audMetadataObjectInfo &objectInfo);

	static size_t GetMaxSize();
private:
	
};

#endif // AUD_MUSICACTIONFACTORY_H
