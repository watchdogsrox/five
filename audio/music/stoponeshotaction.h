// 
// audio/music/stoponeshotaction.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_STOPONESHOTACTION_H
#define AUD_STOPONESHOTACTION_H

#include "musicaction.h"

namespace rage
{
	struct StopOneShotAction;
}
class audStopOneShotAction : public audMusicAction
{
public:

	audStopOneShotAction(const StopOneShotAction *metadata);

	virtual void Trigger();

private:

};

#endif // AUD_STOPONESHOTACTION_H
