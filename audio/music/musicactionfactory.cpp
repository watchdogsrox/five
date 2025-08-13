// 
// audio/music/musicactionfactory.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 
#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "musicaction.h"
#include "musicactionfactory.h"

#include "djspeechaction.h"
#include "fadeinradioaction.h"
#include "fadeoutradioaction.h"
#include "forceradiotrackaction.h"
#include "setmoodaction.h"
#include "startoneshotaction.h"
#include "starttrackaction.h"
#include "stoponeshotaction.h"
#include "stoptrackaction.h"

AUDIO_MUSIC_OPTIMISATIONS()

audMusicAction *audMusicActionFactory::Create(const audMetadataRef actionMetadataRef)
{
	audMetadataObjectInfo objectInfo;
	if(!actionMetadataRef.IsValid() || !audNorthAudioEngine::GetMetadataManager().GetObjectInfo(actionMetadataRef, objectInfo))
	{
		return NULL;
	}
	else
	{
		return Create(objectInfo);
	}
}

audMusicAction *audMusicActionFactory::Create(const audMetadataObjectInfo &objectInfo)
{
	Assert(gGameObjectsGetBaseTypeId(objectInfo.GetType()) == MusicAction::TYPE_ID);

#define INSTANTIATE_ACTION(x) case x::TYPE_ID: return rage_new aud##x(objectInfo.GetObject<x>())
	switch(objectInfo.GetType())
	{
		INSTANTIATE_ACTION(StartTrackAction);
		INSTANTIATE_ACTION(StopTrackAction);
		INSTANTIATE_ACTION(SetMoodAction);
		INSTANTIATE_ACTION(StartOneShotAction);
		INSTANTIATE_ACTION(StopOneShotAction);
		INSTANTIATE_ACTION(RadioDjSpeechAction);
		INSTANTIATE_ACTION(FadeInRadioAction);
		INSTANTIATE_ACTION(FadeOutRadioAction);
		INSTANTIATE_ACTION(ForceRadioTrackAction);
	default:
		audErrorf("Type Id %u not valid music action", objectInfo.GetType());
		
		return NULL;
	}
}

size_t audMusicActionFactory::GetMaxSize()
{
	size_t maxSize = 0;
#define SIZEOF_ACTION(x) maxSize = Max(maxSize, sizeof(aud##x))
	SIZEOF_ACTION(StartTrackAction);
	SIZEOF_ACTION(StopTrackAction);
	SIZEOF_ACTION(SetMoodAction);
	SIZEOF_ACTION(StartOneShotAction);
	SIZEOF_ACTION(StopOneShotAction);
	SIZEOF_ACTION(RadioDjSpeechAction);
	SIZEOF_ACTION(FadeInRadioAction);
	SIZEOF_ACTION(FadeOutRadioAction);
	SIZEOF_ACTION(ForceRadioTrackAction);
	return maxSize;
}
