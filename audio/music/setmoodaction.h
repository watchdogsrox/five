// 
// audio/music/setmoodaction.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_SETMOODACTION_H
#define AUD_SETMOODACTION_H

#include "musicaction.h"

namespace rage
{
	struct SetMoodAction;
}
class audSetMoodAction : public audMusicAction
{
public:

	audSetMoodAction(const SetMoodAction *metadata);

	virtual void Trigger();

private:

	const SetMoodAction *m_Metadata;

};

#endif // AUD_SETMOODACTION_H