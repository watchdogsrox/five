// 
// audio/streamslot.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#include "audio_channel.h"
#include "streamslot.h"
#include "debug/debug.h"
#include "grcore/debugdraw.h"
#include "system/memory.h"

#include "audiohardware/waveslot.h"

#include "debugaudio.h"

AUDIO_OPTIMISATIONS()

const char *g_StreamingWaveSlotNames[g_NumStreamSlots] =
{
	"RADIO1_A", "RADIO1_B",
	"RADIO2_A", "RADIO2_B",
	"RADIO3_A"/*, "RADIO3_B"*/
};

const char *g_VirtualStreamingWaveSlotNames[g_NumVirtualStreamSlots] =
{
	"STREAM_VIRTUAL_0",
	"STREAM_VIRTUAL_1"
};

audStreamSlot *audStreamSlot::sm_StreamSlots[g_NumStreamSlots];
audStreamSlot *audStreamSlot::sm_VirtualStreamSlots[g_NumVirtualStreamSlots];

void audStreamSlot::InitClass(void)
{
	for(u32 slotIndex = 0; slotIndex < g_NumStreamSlots; slotIndex++)
	{
		sm_StreamSlots[slotIndex] = rage_new audStreamSlot(g_StreamingWaveSlotNames[slotIndex]);
	}

	for(u32 slotIndex = 0; slotIndex < g_NumVirtualStreamSlots; slotIndex++)
	{
		sm_VirtualStreamSlots[slotIndex] = rage_new audStreamSlot(g_VirtualStreamingWaveSlotNames[slotIndex]);
	}
}

void audStreamSlot::ShutdownClass(void)
{
	for(u32 slotIndex = 0; slotIndex < g_NumStreamSlots; slotIndex++)
	{
		if(sm_StreamSlots[slotIndex])
		{
			delete sm_StreamSlots[slotIndex];
			sm_StreamSlots[slotIndex] = NULL;
		}
	}

	for(u32 slotIndex = 0; slotIndex < g_NumVirtualStreamSlots; slotIndex++)
	{
		if(sm_VirtualStreamSlots[slotIndex])
		{
			delete sm_VirtualStreamSlots[slotIndex];
			sm_VirtualStreamSlots[slotIndex] = NULL;
		}
	}
}

audStreamSlot *audStreamSlot::GetSlot(u32 index)
{
	naCErrorf(index < g_NumStreamSlots, "Invalid index paseed into GetSlot");
	return ((index < g_NumStreamSlots) ? sm_StreamSlots[index] : NULL);
}

audStreamSlot *audStreamSlot::FindIdleSlot(void)
{
	for(u32 slotIndex = 0; slotIndex < g_NumStreamSlots; slotIndex++)
	{
		audStreamSlot *slot = sm_StreamSlots[slotIndex];
		if(slot && (slot->GetState() == STREAM_SLOT_STATUS_IDLE))
		{
			return slot;
		}
	}

	return NULL;
}

audStreamSlot *audStreamSlot::AllocateSlot(audStreamClientSettings *clientSettings)
{
	audStreamSlot *slotToAllocate = FindIdleSlot();
	if(!slotToAllocate)
	{
		//We don't have a free slot, so let's see if we can steal a slot with a lower priority.
		// search for the lowest priority slot
		audStreamPriority lowestPrio = NUM_STREAM_PRIORITIES;
		u32 lowestPrioSlot = 0;
		for(u32 slotIndex = 0; slotIndex < g_NumStreamSlots; slotIndex++)
		{
			if(sm_StreamSlots[slotIndex] && sm_StreamSlots[slotIndex]->GetState() == STREAM_SLOT_STATUS_ACTIVE && 
				(audStreamPriority)sm_StreamSlots[slotIndex]->GetPriority() < lowestPrio)
			{
				lowestPrio = (audStreamPriority)sm_StreamSlots[slotIndex]->GetPriority();
				lowestPrioSlot = slotIndex;
			}
			if(lowestPrio <= STREAM_PRIORITY_ENTITY_EMITTER_FAR)
			{
				break;
			}
		}
		if(sm_StreamSlots[lowestPrioSlot] && (audStreamPriority)sm_StreamSlots[lowestPrioSlot]->GetPriority() < clientSettings->priority)
		{
			//Only try to stop a stream slot if it's active, otherwise try and allocate a slot next frame.
			sm_StreamSlots[lowestPrioSlot]->StopAndFree();
			slotToAllocate = sm_StreamSlots[lowestPrioSlot];
		}		
	}

	if(slotToAllocate)
	{
		slotToAllocate->Allocate(clientSettings);
	}

	return slotToAllocate;
}

audStreamSlot *audStreamSlot::AllocateVirtualSlot(audStreamClientSettings *clientSettings)
{
	for(u32 slotIndex = 0; slotIndex < g_NumVirtualStreamSlots; slotIndex++)
	{
		audStreamSlot *slot = sm_VirtualStreamSlots[slotIndex];
		if(slot && (slot->GetState() == STREAM_SLOT_STATUS_IDLE))
		{
			slot->Allocate(clientSettings);
			return slot;
		}
	}

	return nullptr;
}

void audStreamSlot::UpdateSlots(u32 timeInMs)
{
	for(u32 slotIndex = 0; slotIndex < g_NumStreamSlots; slotIndex++)
	{
		sm_StreamSlots[slotIndex]->Update(timeInMs);
	}

	for(u32 slotIndex = 0; slotIndex < g_NumVirtualStreamSlots; slotIndex++)
	{
		sm_VirtualStreamSlots[slotIndex]->Update(timeInMs);
	}
}

audStreamSlot::audStreamSlot(const char* waveSlotName) : m_State(STREAM_SLOT_STATUS_IDLE)
{
	m_WaveSlot = audWaveSlot::FindWaveSlot((char *)(waveSlotName));
	memset(&m_RequestedClientSettings, 0, sizeof(audStreamClientSettings));
	memset(&m_ActiveClientSettings, 0, sizeof(audStreamClientSettings));
}

audStreamSlot::~audStreamSlot()
{
}

void audStreamSlot::Update(u32 UNUSED_PARAM(timeInMs))
{
	if(m_State == STREAM_SLOT_STATUS_FREEING)
	{
		//Check if this stream slot is completely free yet.
		bool isFree = true;
		if(m_ActiveClientSettings.hasStoppedCallback)
		{
			isFree = m_ActiveClientSettings.hasStoppedCallback(m_ActiveClientSettings.userData);
		}
		else
		{
			isFree = HasStoppedCallback(m_ActiveClientSettings.userData);
		}

		if(isFree)
		{
			if(m_RequestedClientSettings.priority > STREAM_PRIORITY_IDLE)
			{
				//The slot is ready to be used by a waiting client, so mark as active.
				m_ActiveClientSettings = m_RequestedClientSettings;
				memset(&m_RequestedClientSettings, 0, sizeof(audStreamClientSettings));
				m_State = STREAM_SLOT_STATUS_ACTIVE;
			}
			else
			{
				//We don't have a client waiting, so mark as idle.
				memset(&m_ActiveClientSettings, 0, sizeof(audStreamClientSettings));
				m_State = STREAM_SLOT_STATUS_IDLE;
			}
		}
	}
}

void audStreamSlot::Allocate(audStreamClientSettings *clientSettings)
{
	naAssertf(m_State != STREAM_SLOT_STATUS_ACTIVE, "Attempting to allocate to a stream slot but it is already active");

	if(m_State == STREAM_SLOT_STATUS_IDLE)
	{
		//We are free to go active immediately.
		m_ActiveClientSettings = *clientSettings;
		//Make sure we have a has stopped callback
		naAssertf(m_ActiveClientSettings.stopCallback, "Missing an on-stop callback during audStreamSlot allocation; please implement one");
		memset(&m_RequestedClientSettings, 0, sizeof(audStreamClientSettings));
		m_State = STREAM_SLOT_STATUS_ACTIVE;
	}
	else
	{
		m_RequestedClientSettings = *clientSettings;
		//Make sure we have a has stopped callback
		naAssertf(m_RequestedClientSettings.stopCallback, "Missing an on-stop callback during audStreamSlot allocation; please implement one");
	}
}

void audStreamSlot::Free(void)
{
	//Assume we are safe to be reused right away.
	m_State = STREAM_SLOT_STATUS_FREEING;
}

void audStreamSlot::ClearRequestedSettings()
{
	memset(&m_RequestedClientSettings, 0, sizeof(audStreamClientSettings));
}

void audStreamSlot::ClearActiveSettings()
{
	memset(&m_ActiveClientSettings, 0, sizeof(audStreamClientSettings));
}

void audStreamSlot::StopAndFree(void)
{
	if(m_ActiveClientSettings.stopCallback)
	{
		m_ActiveClientSettings.stopCallback(m_ActiveClientSettings.userData);
		m_ActiveClientSettings.stopCallback = NULL; //Clear the callback, so we only call it once.
	}

	Free();
}

bool audStreamSlot::HasStoppedCallback(u32 UNUSED_PARAM(userData))
{
	bool hasStopped = true;

	//Check if we are still loading or playing from our allocated wave slots.
	audWaveSlot* waveSlot = GetWaveSlot();
	if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
	{
		hasStopped = false;
	}

	return hasStopped;
}

#if GTA_REPLAY
void audStreamSlot::CleanUpSlotsForReplay()
{
	for(u32 slotIndex=0; slotIndex<g_NumStreamSlots; slotIndex++)
	{
		if(sm_StreamSlots[slotIndex]->GetPriority() != audStreamSlot::STREAM_PRIORITY_REPLAY_MUSIC)
		{
			sm_StreamSlots[slotIndex]->StopAndFree();
		}
	}
}
#endif

#if RSG_BANK

void audStreamSlot::DrawDebug(audDebugDrawManager &drawMgr)
{
	for(u32 slotIndex=0; slotIndex < g_NumStreamSlots; slotIndex++)
	{
		sm_StreamSlots[slotIndex]->DrawSlotDebug(drawMgr);
	}

	for(u32 slotIndex=0; slotIndex < g_NumVirtualStreamSlots; slotIndex++)
	{
		sm_VirtualStreamSlots[slotIndex]->DrawSlotDebug(drawMgr);
	}
}

void audStreamSlot::DrawSlotDebug(audDebugDrawManager &drawMgr)
{
	const char *prio[NUM_STREAM_PRIORITIES] = 	
	{
		"PrioIdle",
		"PrioEntityEmitter_Far",
		"PrioEntityEmitter_Near",
		"PrioStaticEmitter",
		"PrioEntityEmitter_Network",
		"ScriptedVehicleRadio",
		"PrioFrontendMenu",
		"PrioScript",
		"PrioScriptEmitter",
		"PrioMusic",
		"PrioPlayerRadio",
		"PrioReplayMusic",
		"PrioCutscene",
	};
	
	const char *states[] =
	{
		"Idle",
		"Active",
		"Freeing"
	};

	drawMgr.DrawLinef("StreamSlot: %s %s Active: [%s] Requested: [%s]", m_WaveSlot ? m_WaveSlot->GetSlotName() : "NULL", states[m_State], prio[m_ActiveClientSettings.priority], prio[m_RequestedClientSettings.priority]);
}
#endif // RSG_BANK

