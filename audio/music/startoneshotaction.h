// 
// audio/music/startoneshotaction.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_STARTONESHOTACTION_H
#define AUD_STARTONESHOTACTION_H

#include "musicaction.h"

namespace rage
{
	struct StartOneShotAction;
}
class audStartOneShotAction : public audMusicAction
{
public:

	audStartOneShotAction(const StartOneShotAction *metadata);

	virtual PrepareState Prepare();
	virtual void Trigger();
	virtual void Cancel();

private:

	const char *GetName() const;
	
	bool ShouldUnfreezeRadio() const
	{
		return AUD_GET_TRISTATE_VALUE(m_Metadata->Flags, FLAG_ID_STOPTRACKACTION_UNFREEZERADIO) == AUD_TRISTATE_TRUE;
	}

	const StartOneShotAction *m_Metadata;
	u32 m_PrepareTime;
	bool m_HasPrepared;

};

#endif // AUD_STARTONESHOTACTION_H
