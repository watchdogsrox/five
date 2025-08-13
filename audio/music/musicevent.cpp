// 
// audio/music/musicevent.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "audio/audio_channel.h"
#include "audio/northaudioengine.h"
#include "musicactionfactory.h"
#include "musicevent.h"
#include "musicplayer.h"

#include "audioengine/engine.h"

FW_INSTANTIATE_CLASS_POOL(audMusicEvent, audMusicEvent::kMaxActiveEvents, atHashString("MusicEvent",0x30e4eada));

AUDIO_MUSIC_OPTIMISATIONS()

audMusicEvent *audMusicEvent::Create(const char *eventName, const u32 eventNameHash, const u32 ownerId /* = ~0U */)
{
	audMusicEvent *musicEvent = rage_new audMusicEvent();
	if(audVerifyf(musicEvent, "Failed to allocate new music event for %s/%u", eventName ? eventName : "unknown", eventNameHash))
	{
		if(!musicEvent->Init(eventName, eventNameHash, ownerId))
		{
			delete musicEvent;
			return NULL;
		}
	}
	return musicEvent;
}

audMusicEvent::audMusicEvent()
: m_NameHash(0)
, m_OwnerId(~0U)
{
	BANK_ONLY(m_NameHash = 0);
}

audMusicEvent::~audMusicEvent()
{
	for(u32 i = 0; i < m_Actions.GetCount(); i++)
	{
		delete m_Actions[i];
	}
	m_Actions.Reset();
}

bool audMusicEvent::Init(const char *eventName, const u32 eventNameHash, const u32 ownerId)
{
	eventName = eventName ? eventName : "UnknownEvent";

	m_OwnerId = ownerId;

	audMetadataObjectInfo metadataObjectInfo;
	if(audVerifyf(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(eventNameHash, metadataObjectInfo), "Failed to find music event with name %s", eventName))
	{
		m_NameHash = eventNameHash;	
		BANK_ONLY(m_Name = metadataObjectInfo.GetName());

		if(gGameObjectsIsOfType(metadataObjectInfo.GetType(), MusicAction::TYPE_ID))
		{
			// Create a simple wrapper event around a single action
			audMusicAction *action = audMusicActionFactory::Create(metadataObjectInfo);
			if(audVerifyf(action, "Failed to create music action for event %s", eventName))
			{
				m_Actions.Append() = action;
				return true;
			}
			else
			{
				return false;
			}	
		}
		else
		{
			const MusicEvent *metadata = metadataObjectInfo.GetObject<MusicEvent>();
			if(audVerifyf(metadata, "Invalid music event name %s (type %u)", eventName, metadataObjectInfo.GetType()))
			{
				for(u32 i = 0; i < metadata->numActions; i++)
				{
					audMusicAction *action = audMusicActionFactory::Create(metadata->Actions[i].Action);
					if(audVerifyf(action, "Failed to create music action %u for event %s", i, eventName))
					{
						m_Actions.Append() = action;
					}
					else
					{
						return false;
					}			
				}
				return true;
			}
		}
	}
	
	
	return false;
}

audMusicAction::PrepareState audMusicEvent::Prepare()
{
	audMusicAction::PrepareState eventState = audMusicAction::PREPARED;

#if !__FINAL
	if(!g_AudioEngine.IsAudioEnabled())
	{
		// Lie and pretend like we're prepared
		return audMusicAction::PREPARED;
	}
#endif

	for(u32 i = 0; i < m_Actions.GetCount(); i++)
	{
		if(audVerifyf(m_Actions[i], "NULL action at index %u", i))
		{
			switch(m_Actions[i]->Prepare())
			{
			case audMusicAction::PREPARED:
				break;
			case audMusicAction::PREPARE_FAILED:
				return audMusicAction::PREPARE_FAILED;
			case audMusicAction::PREPARING:
				eventState = audMusicAction::PREPARING;
				break;
			}
		}
	}
	return eventState;
}

void audMusicEvent::Trigger()
{
	for(u32 i = 0; i < m_Actions.GetCount(); i++)
	{
		m_Actions[i]->Trigger();
	}
}

void audMusicEvent::Cancel()
{
	for(u32 i = 0; i < m_Actions.GetCount(); i++)
	{
		m_Actions[i]->Cancel();
	}
}


