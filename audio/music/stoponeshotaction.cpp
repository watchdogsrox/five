// 
// audio/music/stoponeshotaction.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "musicplayer.h"
#include "stoponeshotaction.h"

AUDIO_MUSIC_OPTIMISATIONS()

audStopOneShotAction::audStopOneShotAction(const StopOneShotAction *UNUSED_PARAM(metadata))
{

}

void audStopOneShotAction::Trigger()
{
	g_InteractiveMusicManager.StopOneShot();
}
