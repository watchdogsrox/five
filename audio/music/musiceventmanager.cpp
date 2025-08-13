#include "audio/gameobjects.h"
#include "audio/northaudioengine.h"
#include "musicaction.h"
#include "musicevent.h"
#include "musiceventmanager.h"
#include "grcore/debugdraw.h"

AUDIO_MUSIC_OPTIMISATIONS()

bool audMusicEventManager::Init()
{
	audMusicEvent::InitPool(MEMBUCKET_AUDIO);
	audMusicAction::InitPool(MEMBUCKET_AUDIO);
	
	BANK_ONLY(m_DebugDraw = false);
	return true;
}

void audMusicEventManager::Shutdown()
{
	m_ActiveEventMap.Reset();

	audMusicEvent::ShutdownPool();
	audMusicAction::ShutdownPool();
}

const audMusicEvent *audMusicEventManager::Find(const u32 eventNameHash)
{
	const audMusicEvent **entry = m_ActiveEventMap.Access(eventNameHash);
	if(entry)
	{
		return *entry;
	}
	return NULL;
}

bool audMusicEventManager::Register(const audMusicEvent *musicEvent)
{
	if(audVerifyf(Find(musicEvent->GetNameHash()) == NULL, "Trying to register music event %s twice", musicEvent->GetName()))
	{
		m_ActiveEventMap.Insert(musicEvent->GetNameHash(), musicEvent);
		return true;
	}
	return false;
}

bool audMusicEventManager::Unregister(const audMusicEvent *musicEvent)
{
	return m_ActiveEventMap.Delete(musicEvent->GetNameHash());
}

bool audMusicEventManager::CancelEvent(const char *eventName)
{
	audMusicEvent *musicEvent = const_cast<audMusicEvent*>(Find(eventName));
	return Cancel(musicEvent);
}

bool audMusicEventManager::Cancel(audMusicEvent *musicEvent)
{
	if(musicEvent)
	{
#if __BANK
		if(m_DebugDraw)
		{			
			audDisplayf("Cancelled music event: %s", musicEvent->GetName());
		}
#endif
		musicEvent->Cancel();

		Unregister(musicEvent);
		delete musicEvent;
		return true;
	}
	return false;
}

bool audMusicEventManager::TriggerEvent(const char *eventName)
{
	return TriggerEvent(eventName, atStringHash(eventName));
}

bool audMusicEventManager::TriggerEvent(const char *eventName, const u32 eventNameHash)
{
	audMusicEvent *musicEvent = const_cast<audMusicEvent*>(Find(eventNameHash));
	bool needToUnregisterEvent = true;
	if(!musicEvent)
	{
		needToUnregisterEvent = false;
		musicEvent = audMusicEvent::Create(eventName, eventNameHash, 0);
	}

	if(musicEvent)
	{
#if __BANK
		if(m_DebugDraw)
		{			
			audDisplayf("Triggered music event: %s", musicEvent->GetName());
		}
#endif
		musicEvent->Trigger();

		if(needToUnregisterEvent)
		{
			Unregister(musicEvent);
		}

		delete musicEvent;
		return true;
	}
	return false;
}

audMusicAction::PrepareState audMusicEventManager::PrepareEvent(const char *eventName, const u32 ownerId)
{
	audMusicEvent *musicEvent = const_cast<audMusicEvent*>(Find(eventName));
	if(!musicEvent)
	{
		musicEvent = audMusicEvent::Create(eventName, ownerId);
		if(musicEvent)
		{
			Register(musicEvent);
		}
	}

	if(!musicEvent)
	{
		return audMusicAction::PREPARE_FAILED;
	}

#if __BANK
	if(m_DebugDraw)
	{
		audDisplayf("Prepare music event: %s", musicEvent->GetName());
	}
#endif
	audMusicAction::PrepareState ret = musicEvent->Prepare();

#if __BANK
	if(m_DebugDraw)
	{
		const char *states[] = {
			"PREPARED",
			"PREPARING",
			"PREPARE_FAILED"
		};
		audDisplayf("Prepare music event: %s - %s", musicEvent->GetName(), states[ret]);
	}
#endif

	return ret;
}

bool audMusicEventManager::CancelAllEvents(const u32 ownerId)
{
	bool ret = false;
	atArray<audMusicEvent *> eventsToCancel;
	atMap<u32, const audMusicEvent *>::Iterator iter = m_ActiveEventMap.CreateIterator();

	for(iter.Start(); !iter.AtEnd(); iter.Next())
	{
		audMusicEvent *event = const_cast<audMusicEvent*>(*iter);
		if(event->GetOwnerId() == ownerId)
		{
			eventsToCancel.Grow() = event;
		}
	}

	for(s32 i = 0; i < eventsToCancel.GetCount(); i++)
	{
#if __BANK
		audWarningf("Auto-cancelling event %s owned by %08X", eventsToCancel[i]->GetName(), ownerId);
#endif
		ret |= Cancel(eventsToCancel[i]);
	}

	return ret;
}

#if __BANK
void audMusicEventManager::AddWidgets(bkBank &bank)
{
	bank.AddToggle("DrawEvents", &m_DebugDraw);
}

void audMusicEventManager::DebugDraw()
{
	if(m_DebugDraw)
	{
		Vector2 pos(0.1f,0.1f);
		
		grcDebugDraw::Text(pos, Color32(255,255,255), "Active Events:");
		pos.y += 0.02f;
		
		atMap<u32, const audMusicEvent *>::Iterator iter = m_ActiveEventMap.CreateIterator();

		for(iter.Start(); !iter.AtEnd(); iter.Next())
		{
			const audMusicEvent *event = *iter;
			char lineBuf[128];
			formatf(lineBuf, "%s", event->GetName());
			grcDebugDraw::Text(pos, Color32(255,255,255), lineBuf);
			pos.y += 0.02f;
		}
	}
}

#endif

