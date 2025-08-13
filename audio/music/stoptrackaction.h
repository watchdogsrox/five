// 
// audio/music/stoptrackaction.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_STOPTRACKACTION_H
#define AUD_STOPTRACKACTION_H

#include "musicaction.h"

namespace rage
{
	struct StopTrackAction;
}

class audStopTrackAction : public audMusicAction
{
public:

	audStopTrackAction(const StopTrackAction *metadata);

	virtual void Trigger();

private:

	bool ShouldUnfreezeRadio() const
	{
		return AUD_GET_TRISTATE_VALUE(m_Metadata->Flags, FLAG_ID_STOPTRACKACTION_UNFREEZERADIO) == AUD_TRISTATE_TRUE;
	}

	const StopTrackAction *m_Metadata;

};

#endif // AUD_STOPTRACKACTION_H
