// 
// audio/music/starttrackaction.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_STARTTRACKACTION_H
#define AUD_STARTTRACKACTION_H

#include "musicaction.h"

namespace rage
{
	struct StartTrackAction;
}
class audStartTrackAction : public audMusicAction
{
public:

	audStartTrackAction(const StartTrackAction *metadata);
	
	virtual void Trigger();

private:

	const char *GetName() const;

	u32 ChooseSong();
	bool ShouldOverrideRadio() const;
	bool ShouldRandomizeStartOffset() const;
	bool ShouldMuteRadioOffSound() const;

	const StartTrackAction *m_Metadata;
	
	static bool sm_HaveCheckedData;
	static bool sm_AltSongsEnabled;
	
};

#endif // AUD_STARTTRACKACTION_H
