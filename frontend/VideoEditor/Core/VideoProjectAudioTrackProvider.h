/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : VideoProjectAudioTrackProvider.h
// PURPOSE : Class for listing and validating audio tracks for use in replays
//
// AUTHOR  : james.strain
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//
/////////////////////////////////////////////////////////////////////////////////
#include "control/replay/ReplaySettings.h"

#if defined( GTA_REPLAY ) && GTA_REPLAY

#ifndef VIDEO_PROJECT_AUDIO_TRACK_PROVIDER_H_
#define VIDEO_PROJECT_AUDIO_TRACK_PROVIDER_H_

// game
#include "replaycoordinator/ReplayAudioTrackProvider.h"

class CVideoProjectAudioTrackProvider : public CReplayAudioTrackProvider
{
public:
	// TODO - drop in any UI level functionality here
};

#endif // VIDEO_PROJECT_AUDIO_TRACK_PROVIDER_H_

#endif // defined( GTA_REPLAY ) && GTA_REPLAY
