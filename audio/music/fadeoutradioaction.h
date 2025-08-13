// 
// audio/music/fadeoutradioaction.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_FADEOUTRADIOACTION_H
#define AUD_FADEOUTRADIOACTION_H

#include "musicaction.h"

namespace rage
{
	struct FadeOutRadioAction;
}

class audFadeOutRadioAction : public audMusicAction
{
public:

	audFadeOutRadioAction(const FadeOutRadioAction *metadata);

	virtual void Trigger();

	NOTFINAL_ONLY(const char *GetName() const);

private:

	const FadeOutRadioAction *m_Metadata;

};

#endif // AUD_FADEOUTRADIOACTION_H
