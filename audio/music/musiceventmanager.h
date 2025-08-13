// 
// audio/music/musiceventmanager.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef AUD_MUSICEVENTMANAGER_H
#define AUD_MUSICEVENTMANAGER_H

#include "atl/map.h"
#include "bank/bkmgr.h"
#include "musicaction.h"

class audMusicEvent;
class audMusicEventManager
{
public:

	bool Init();
	void Shutdown();

	audMusicAction::PrepareState PrepareEvent(const char *eventName, const u32 ownerId);

	bool TriggerEvent(const char *eventName);
	bool TriggerEvent(const char * eventName, const u32 eventNameHash);

	bool CancelEvent(const char *eventName);

	// PURPOSE
	//	Cancel any active events for the specified owner
	bool CancelAllEvents(const u32 ownerId);

#if __BANK
	void DebugDraw();
	void AddWidgets(bkBank &bank);
#endif // __BANK

private:

	bool Cancel(audMusicEvent *event);
	bool Register(const audMusicEvent *event);

	bool Unregister(const audMusicEvent *event);

	const audMusicEvent *Find(const char *eventName)
	{
		return Find(atStringHash(eventName));
	}
	const audMusicEvent *Find(const u32 eventNameHash);


	atMap<u32, const audMusicEvent *> m_ActiveEventMap;

	BANK_ONLY(bool m_DebugDraw);
};


#endif // AUD_MUSICEVENTMANAGER_H
