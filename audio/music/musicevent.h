// 
// audio/music/musicevent.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_MUSICEVENT_H
#define AUD_MUSICEVENT_H

#include "audio/gameobjects.h"
#include "atl/array.h"
#include "fwtl/pool.h"

#include "musicaction.h"

class audMusicEvent
{
public:
	FW_REGISTER_CLASS_POOL(audMusicEvent);

	audMusicEvent();
	~audMusicEvent();

	bool Init(const char *eventName, const u32 eventNameHash, const u32 ownerId);
	audMusicAction::PrepareState Prepare();
	void Trigger();
	void Cancel();

	BANK_ONLY(const char *GetName() const { return m_Name; })
	u32 GetNameHash() const { return m_NameHash; }

	u32 GetOwnerId() const { return m_OwnerId; }

	enum {kMaxActiveEvents = 15};

	static audMusicEvent *Create(const char *eventName, const u32 ownerId)
	{
		return Create(eventName, atStringHash(eventName), ownerId);
	}
	static audMusicEvent *Create(const u32 eventNameHash, const u32 ownerId)
	{
		return Create(NULL, eventNameHash, ownerId);
	}
	static audMusicEvent *Create(const char *eventName, const u32 eventNameHash, const u32 ownerId);

private:
	
	atFixedArray<audMusicAction*, MusicEvent::MAX_ACTIONS> m_Actions;
	BANK_ONLY(const char *m_Name);
	u32 m_NameHash;
	u32 m_OwnerId;
};

#endif // AUD_MUSICEVENT_H
