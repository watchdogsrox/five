// 
// audio/music/forceradiotrackaction.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_FORCERADIOTRACKACTION_H
#define AUD_FORCERADIOTRACKACTION_H

#include "musicaction.h"

namespace rage
{
	struct ForceRadioTrackAction;
}

class audForceRadioTrackAction : public audMusicAction
{
public:

	audForceRadioTrackAction(const ForceRadioTrackAction *metadata);

	virtual audMusicAction::PrepareState Prepare();
	virtual void Trigger();
	virtual void Cancel();

	NOTFINAL_ONLY(const char *GetName() const);

private:

	const ForceRadioTrackAction *m_Metadata;
	u32 m_RequestedTrack;
	bool m_IsPrepared;
	bool m_HasFrozen;

};

#endif // AUD_FORCERADIOTRACKACTION_H
