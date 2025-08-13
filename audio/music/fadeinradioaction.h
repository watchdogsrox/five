// 
// audio/music/fadeinradioaction.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_FADEINRADIOACTION_H
#define AUD_FADEINRADIOACTION_H

#include "musicaction.h"

namespace rage
{
	struct FadeInRadioAction;
}

class audFadeInRadioAction : public audMusicAction
{
public:

	audFadeInRadioAction(const FadeInRadioAction *metadata);

	virtual void Trigger();

	NOTFINAL_ONLY(const char *GetName() const);

private:

	const FadeInRadioAction *m_Metadata;

};

#endif // AUD_FADEINRADIOACTION_H
