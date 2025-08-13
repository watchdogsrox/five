// 
// audio/music/djspeechaction.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_DJSPEECHACTION_H
#define AUD_DJSPEECHACTION_H

#include "musicaction.h"

namespace rage
{
	struct RadioDjSpeechAction;
}
class audRadioDjSpeechAction : public audMusicAction
{
public:

	audRadioDjSpeechAction(const RadioDjSpeechAction *metadata);

	virtual PrepareState Prepare();
	virtual void Trigger();
	virtual void Cancel();

private:

	NOTFINAL_ONLY(const char *GetName() const);

	const RadioDjSpeechAction *m_Metadata;
	bool m_HasPrepared;
};

#endif // AUD_STARTONESHOTACTION_H
