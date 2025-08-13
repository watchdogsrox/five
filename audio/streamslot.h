// 
// audio/streamslot.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_STREAMSLOT_H
#define AUD_STREAMSLOT_H

#include "Control/Replay/ReplaySettings.h"

const u32 g_NumStreamSlots = 5;
const u32 g_NumVirtualStreamSlots = 2;
const u32 g_NullStreamSlot = g_NumStreamSlots;

typedef bool (*streamStopCallbackPtr)(u32 userData);

namespace rage
{
	class audWaveSlot;
	class audDebugDrawManager;
}

struct audStreamClientSettings
{
	audStreamClientSettings()
	{
		stopCallback = NULL;
		hasStoppedCallback = NULL;
		userData = 0;
		priority = 0;
	}
	streamStopCallbackPtr stopCallback;
	streamStopCallbackPtr hasStoppedCallback;
	u32 userData;
	u8 priority;
};

class audStreamSlot
{
public:
	enum audStreamPriority
	{
		STREAM_PRIORITY_IDLE,
		STREAM_PRIORITY_ENTITY_EMITTER_FAR,
		STREAM_PRIORITY_ENTITY_EMITTER_NEAR,
		STREAM_PRIORITY_STATIC_AMBIENT_EMITTER,
		STREAM_PRIORITY_ENTITY_EMITTER_NETWORK,
		STREAM_PRIORITY_SCRIPT_VEHICLE,
		STREAM_PRIORITY_FRONTENDMENU,
		STREAM_PRIORITY_SCRIPT,
		STREAM_PRIORITY_SCRIPT_AMBIENT_EMITTER,
		STREAM_PRIORITY_MUSIC,
		STREAM_PRIORITY_PLAYER_RADIO,
		STREAM_PRIORITY_REPLAY_MUSIC,
		STREAM_PRIORITY_CUTSCENE,
		NUM_STREAM_PRIORITIES
	};

	enum audStreamSlotStatus
	{
		STREAM_SLOT_STATUS_IDLE,
		STREAM_SLOT_STATUS_ACTIVE,
		STREAM_SLOT_STATUS_FREEING
	};

	static void InitClass(void);
	static void ShutdownClass(void);
	static void UpdateSlots(u32 timeInMs);
	static audStreamSlot *AllocateSlot(audStreamClientSettings *clientSettings);
	static audStreamSlot *AllocateVirtualSlot(audStreamClientSettings *clientSettings);

	audStreamSlot(const char* waveSlotName);
	~audStreamSlot();

	u32 GetState(void)
	{
		return m_State;
	}
	u32 GetPriority(void)
	{
		return m_ActiveClientSettings.priority;
	}
	void SetPriority(u32 priority)
	{
		Assign(m_ActiveClientSettings.priority, priority);
	}
	audWaveSlot* GetWaveSlot(void)
	{
		return m_WaveSlot;
	}

	void Free(void);
	void StopAndFree(void);
	void ClearRequestedSettings();
	void ClearActiveSettings();

	BANK_ONLY(static void DrawDebug(audDebugDrawManager &drawMgr));
	
	static audStreamSlot *GetSlot(u32 index);

#if GTA_REPLAY
	static void CleanUpSlotsForReplay();
#endif

protected:
	
	static audStreamSlot *FindIdleSlot(void);

	BANK_ONLY(void DrawSlotDebug(audDebugDrawManager &drawMgr));
	void Allocate(audStreamClientSettings *clientSettings);
	void Update(u32 timeInMs);
	
	//Default behaviour for HasStoppedCallback; will be used if no callback is specified in settings
	bool HasStoppedCallback(u32 userData);

	audStreamClientSettings m_RequestedClientSettings, m_ActiveClientSettings;
	audWaveSlot* m_WaveSlot;
	u8 m_State;

	static audStreamSlot *sm_StreamSlots[g_NumStreamSlots];
	static audStreamSlot *sm_VirtualStreamSlots[g_NumVirtualStreamSlots];
};

#endif // AUD_STREAMSLOT_H
