//
// audio/pedscenariomanager.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_PEDSECENARIOMANAGER_H
#define AUD_PEDSECENARIOMANAGER_H

#include "control/replay/ReplaySettings.h"
#include "audiosoundtypes/sound.h"
#include "gameobjects.h"

#if GTA_REPLAY
#include "control/replay/PacketBasics.h"
#endif

class audStreamSlot;
class audPedAudioEntity;
class audPedScenarioManager;
class naEnvironmentGroup;

extern  DURANGO_ONLY(ALIGNAS(16))audPedScenarioManager g_PedScenarioManager DURANGO_ONLY();

#if GTA_REPLAY
struct audReplayScenarioState
{
	CPacketVector3 scenarioCenter;
	u32 scenarioName;
	u32 scenarioID;
	u32 soundHash;
};
#endif

// ----------------------------------------------------------------
// Class representing a single ped scenario sound - can have multiple
// peds associated with it
// ----------------------------------------------------------------
class audPedScenario
{
public:
	audPedScenario();
	~audPedScenario() {};

	void Init(u32 name, u32 prop, audPedAudioEntity* ped, PedScenarioAudioSettings* settings);
	void Reset();
	void Update();
	void StopStream();
	bool HasStreamStopped() const;
	bool DoesPropShareExistingSound(u32 prop) const;
	void RemovePed(audPedAudioEntity* ped);
	bool IsActive() const;
	
	inline Vec3V_Out GetCenter() const						{ return m_Center; }	
	inline u32 GetName() const								{ return m_NameHash; }
	inline u32 GetProp() const								{ return m_PropHash; }
	inline u32 GetUniqueID() const							{ return m_UniqueID; }
	inline u32 GetSoundHash() const							{ return m_SoundHash; }
	inline bool ContainsPed(audPedAudioEntity* ped) const	{ return m_Peds.Find(ped) >= 0; }
	inline PedScenarioAudioSettings* GetSettings()const		{ return m_ScenarioSettings; }
	inline bool IsSoundValid() const						{ return m_Sound != NULL; }

	inline void AddPed(audPedAudioEntity* ped)							{ m_Peds.PushAndGrow(ped); }
	inline void SetSlotIndex(u32 slotIndex)								{ m_SlotIndex = slotIndex; }
	inline void SetCenter(Vec3V center)									{ m_Center = center; }
	inline void SetUniqueID(u32 uniqueID)								{ m_UniqueID = uniqueID; }
	inline void SetSoundHash(u32 soundHash)								{ m_SoundHash = soundHash; }
	inline void SetScenarioSettings(PedScenarioAudioSettings* settings) { m_ScenarioSettings = settings; }

#if __BANK
	void DebugDraw();
#endif

private:
	naEnvironmentGroup* CreateEnvironmentGroup();

private:
	Vec3V m_Center;
	PedScenarioAudioSettings* m_ScenarioSettings;
	audSound* m_Sound;
	audStreamSlot* m_StreamSlot;
	u32 m_NameHash;
	u32 m_PropHash;
	u32 m_SoundHash;
	u32 m_SlotIndex;
	u32 m_UniqueID;

	bool m_HasPlayed;
	atArray<audPedAudioEntity*> m_Peds;
	static u32 sm_PedScenarioUniqueID;
};

// ----------------------------------------------------------------
// audPedScenarioManager manages all active scenarios
// ----------------------------------------------------------------
class audPedScenarioManager
{
public:
	audPedScenarioManager();
	~audPedScenarioManager() {};

	audPedScenario* StartScenario(audPedAudioEntity* ped, u32 name, u32 prop);
	void Update();

	inline audPedScenario* GetPedScenario(u32 index) { return &m_ActiveScenarios[index]; }

#if __BANK
	void DebugDraw();
#endif

	static bool OnScenarioStreamStopCallback(u32 userData);
	static bool HasScenarioStreamStoppedCallback(u32 userData);

#if GTA_REPLAY
	void UpdateReplayScenarioStates();
	void ClearReplayActiveScenarios() { return m_ReplayActiveScenarios.clear(); }
	void AddReplayActiveScenario(audReplayScenarioState replayScenario);
	atArray<audReplayScenarioState>& GetReplayScenarioStates() { return m_ReplayActiveScenarios; }
#endif

private:
	enum { kMaxActiveScenarios = 3 };
	atRangeArray<audPedScenario, kMaxActiveScenarios> m_ActiveScenarios;

#if GTA_REPLAY	
	atArray<audReplayScenarioState> m_ReplayActiveScenarios;
#endif
};

#endif // AUD_PEDSECENARIOMANAGER_H
