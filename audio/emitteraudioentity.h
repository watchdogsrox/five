#ifndef EMITTERAUDIOENTITY_H_INCLUDED
#define EMITTERAUDIOENTITY_H_INCLUDED
// 
// audio/emitteraudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "gameobjects.h"
#include "radioemitter.h"

#include "audioentity.h"

#include "audioengine/entity.h"
#include "audioengine/widgets.h"
#include "audioengine/curve.h"
#include "audioengine/engineutil.h"
#include "atl/bitset.h"
#include "bank/bank.h"
#include "scene/RegdRefTypes.h"
#include "system/memops.h"
#include "vector/quaternion.h"


const u32 g_audMaxStaticEmitters = 700;
const u32 g_audMaxEntityEmitters = 160;
const u32 g_audNumEntitiesPerBin = 8;
const u32 g_audNumBuildingAnimEvents = 24;

namespace rage
{
	class audSound;
}
class audStreamSlot;
class CEntity;
struct CAudioAttr;
class naEnvironmentGroup;

struct audBuildingAnimEvent
{
	audBuildingAnimEvent()
	{
		hash = 0;
	}
	Vec3V pos;
	u32 hash;
};

struct StaticEmitterEntry
{
	StaticEmitterEntry()
	{
		Reset();
	}

	void Reset()
	{
		lastTriggerTime = 0;
		sound = NULL;
		damageSound = NULL;
		streamSlot = NULL;
		linkedObject = NULL;

		emitter = NULL;
		alarmSettings = NULL;
		playingRadioStation = 0;
		isScriptLinkedEmitter = false;
	}

	s32 GetRequestedPCMChannel() const;
	bool IsHighPriorityEmitter() const;
	bool IsClubEmitter() const;
	
	Vector3 position;
	NA_RADIO_ENABLED_ONLY(audStaticRadioEmitter radioEmitter);
	StaticEmitter *emitter;
	audSound *sound;
	audSound *damageSound;
	audStreamSlot *streamSlot;
	u32 lastTriggerTime;
	RegdEnt	linkedObject;
	naEnvironmentGroup *occlusionGroup;
	
	u32 nameHash;
	u32 nextDamageFlipTime;
	AlarmSettings* alarmSettings;

	u32 playingRadioStation;
	bool isScriptLinkedEmitter;
};
// Split the flags out from StaticEmitterEntry to avoid L2cache misses when iterating over the staticEmitter array. [2/1/2013 mdawe]
struct StaticEmitterFlags
{
	StaticEmitterFlags()		
	{
		Reset();
	}

	void Reset()
	{
		bWasBrokenLastFrame = false;
		bNeedsInteriorInfo = false;
		bIsPlayingRadio = false;
		bIsLinked = false;
		bFilterByAngle = false;
		bIsScanner = false;
		bIsEnabled = false;
		bWasValidTime = false;
		bIsActive24h = true;
		bStartStopImmediatelyAtTimeLimits = true;
		bWasActive = false;
		bIsStreamingSound = false;
	}

	bool bWasBrokenLastFrame : 1;
	bool bNeedsInteriorInfo : 1;
	bool bIsPlayingRadio : 1;
	bool bIsLinked : 1;
	bool bFilterByAngle : 1;
	bool bIsScanner : 1;
	bool bIsEnabled : 1;
	bool bWasValidTime : 1;
	bool bIsActive24h : 1;
	bool bStartStopImmediatelyAtTimeLimits : 1;
	bool bWasActive : 1;
	bool bIsStreamingSound : 1;
};

class C2dEffect;
struct EntityEmitterEntry
{
	EntityEmitterEntry()
	{
		lastTriggerTime = 0;
		randomSeed = 0.0f;
		sound = NULL;
		emitter = NULL;
		frequentUpdates = false;
		prevEmitterIdx = 0xffff;
	}
	Vector3 offset;
	audCompressedQuat rotation;
	CEntity *entity;
	audSound *sound;
	EntityEmitter* emitter;
	u32 lastTriggerTime;
	f32 randomSeed;
	u16 localRandomSeed;
	u16 prevEmitterIdx;
	bool frequentUpdates;

	inline bool IsOccluded()			{return emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_COMPUTEOCCLUSION)==AUD_TRISTATE_TRUE;}
	inline bool IsConed()				{return emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_VOLUMECONE)==AUD_TRISTATE_TRUE;}
	inline bool IsWindAffected()		{return emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_WINDBASEDVOLUME)==AUD_TRISTATE_TRUE;}
	inline bool ShouldStopOnRain()		{return emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_STOPWHENRAINING)==AUD_TRISTATE_TRUE;}
	inline bool GeneratesTreeRain()		{return emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_GENERATESTREERAIN)==AUD_TRISTATE_TRUE;}
	inline bool OnlyWhenRaining()		{return emitter && AUD_GET_TRISTATE_VALUE(emitter->Flags, FLAG_ID_ENTITYEMITTER_ONLYWHENRAINING)==AUD_TRISTATE_TRUE;}
};

struct tEntityBin
{
	tEntityBin()
	{
		numEntities = 0;
		sysMemZeroBytes<sizeof(entities)>(&entities);
		sysMemSet(&tailEmitters, 0xff, sizeof(tailEmitters));
	}
	atRangeArray<CEntity *, g_audNumEntitiesPerBin> entities;
	atRangeArray<u16, g_audNumEntitiesPerBin> tailEmitters;
	u16 numEntities;
};

class audEmitterAudioEntity : naAudioEntity
{
public:
	AUDIO_ENTITY_NAME(audEmitterAudioEntity);

	audEmitterAudioEntity()
	{
		m_CurrentStaticEmitterIndex = 0;
		m_PreserveEntityEmitters = false;
	}
	virtual ~audEmitterAudioEntity(){}
	
	
	void Init();
	void Shutdown();
	void Update();
	void ResetTimers();

	void InitStaticEmitters();
	void ReInitStaticEmitters();
	void StopAndResetStaticEmitters();

	// only call from the non game-audio thread
	void CheckStaticEmittersForInteriorInfo();

	void SetActive(const bool active)
	{
		m_IsActive = active;
	}

	// PURPOSE
	//	Called by the entity for each 2dEffect audio emitter attached to the entity.
	// PARAMS
	//	entity - owning entity
	//	hash - emitter game object reference
	//	offset - offset is space from entity position
	//	direction - emitter direction
	void RegisterEntityEmitter(CEntity *entity, u32 hash, Vector3 &offset, Quaternion &rotation);

	void UpdateLightAudio(CLightEntity* lightEntity, CEntity *parentEntity, const C2dEffect *effect, const bool isLightOn, const u32 slotIndex);

	// PURPOSE
	//	Called when the specified entity is being removed from the world.  Removes all active emitters
	//	owned by that entity.
	void UnregisterEmittersForEntity(CEntity *entity);

	void AddStaticEmitter(u32 emitterName, const u16 gameTimeMinutes);
	void UpdateStaticEmitterPosition(StaticEmitter* emitter);

	void SetStaticEmitterInteriorInfo(const u32 i);
	void UpdateStaticEmitterEnvironmentGroup(const u32 i);

#if __BANK
	void RetuneEmittersToStation(audRadioStation* station);
	void CreateOrUpdateEmitter(u32 nameHash);
#endif

	// PURPOSE
	//	Called when a loud sound (gunshot etc) happens
	void NotifyLoudSound();

	virtual void UpdateSound(audSound *sound, audRequestedSettings *reqSets, u32 timeInMs);

	void StopEmitter(u32 emitterIndex);
	bool IsEmitterActive(u32 emitterIndex);

	static bool OnStopCallback(u32 emitterIndex);
	static bool HasStoppedCallback(u32 emitterIndex);
	static naEnvironmentGroup* CreateStaticEmitterEnvironmentGroup(const StaticEmitter* staticEmitter, const Vector3& pos);

#if __BANK
	static void AddWidgets(bkBank &bank);
	static bool sm_ShouldDrawEntityEmitters;
	static bool sm_ShouldDrawStaticEmitters;
	static bool sm_DrawStaticEmitterOpennessValues;
	void HandleRaveStaticEmitterCreatedNotification(u32 nameHash);
#endif

	u32 GetNumEntityEmitters() const
	{
		return m_NumEntityEmitters;
	}

	bool GetGeneratesWindAudio(const CAudioAttr *audioAttr) const;

	void SetPreserveEntityEmitters(const bool preserve)
	{
		m_PreserveEntityEmitters = preserve;
	}

	void SetEmitterLinkedObject(const char *emitterName, CEntity* entity, bool isScriptLinkedEmitter = false);
	void SetEmitterRadioStation(const char *emitterName, const char *radioStationName);
	bool SetEmitterEnabled(const char *emitterName, const bool enabled);

	void AddBuildingAnimEvent(u32 hash, Vec3V_In inPos);
	void ClearBuildingAnimEvents();

	bool IsWithinRangeOfRadio(const Vector3& inPosition, float fRange);

private:

	u32 GetRadioStationForEmitter(const u32 emitterIdx) const;
	bool IsEntityEnabled(const u32 index) const { return m_EntityEnabledList.IsSet(index); }

#if __BANK
	void DrawDebug();
#endif

	void FindAudioBinAndIndex(CEntity *entity, u32 &bin, u32 &idx);

	void UpdateStaticEmitters();
	void UpdateEntityEmitters();
	void UpdateBuildingAnimEvents();
	void CreateStaticEmitters();
	void UpdateScriptPositionedEmitter(u32 emitterIndex);

	f32 ComputeEntityEmitterConeAttenuation(u32 emitterIdx);

	bool ShouldStaticEmitterBePlaying(u32 emitterIdx, const bool playerSwitchActive, const bool gameWorldHidden, const u16 gameTimeMinutes);
	bool ShouldStaticEmitterBeStopped(u32 emitterIdx, const u16 gameTimeMinutes);
	bool ShouldEntityEmitterBePlaying(u32 emitterIdx, bool updateCheckTime, const bool playerSwitchActive);

	bool ShouldEntityEmitterBeRegistered(EntityEmitter *emitter, const Vector3& pos);

	void GetEntityEmitterPosition(u32 emittedIdx, Vector3 &pos);
	void GetEntityEmitterPositionAndOrientation(u32 emitterIdx, Vector3 &pos,audCompressedQuat &orientation);

	naEnvironmentGroup* CreateEntityEmitterEnvironmentGroup(EntityEmitterEntry* entityEmitter, Vector3& pos, u8 maxPathDepth);

	Vec3V m_LastListenerPos;

	atRangeArray<StaticEmitterEntry, g_audMaxStaticEmitters> m_StaticEmitterList;
	atRangeArray<StaticEmitterFlags, g_audMaxStaticEmitters> m_StaticEmitterFlags;

	atRangeArray<EntityEmitterEntry,g_audMaxEntityEmitters> m_EntityEmitterList;
	atRangeArray<audBuildingAnimEvent, g_audNumBuildingAnimEvents> m_BuildingAnimEvents;

	audCurve m_ConeCurve;
	u32 m_CurrentStaticEmitterIndex;
	
	atBitSet m_EntityEnabledList;
	u32 m_EntityCheckTimes[g_audMaxEntityEmitters];

	u32 m_NumEntityEmitters;
	audSimpleSmoother m_FillsInteriorSpaceDuckingSmoother;

	tEntityBin m_EntityBins[255];
	u32 m_NumEntities;
	u32 m_FirstFreeBin;
	bool m_IsActive;
	bool m_HasInitialisedStaticEmitters;
	bool m_PreserveEntityEmitters;
	bool m_FirstUpdate;

#if __BANK
	struct audEntityEmitterDebugInfo 
	{
		CEntity* entity;
		u32 hash;
		Vector3 offset;
	};
	
	atArray<audEntityEmitterDebugInfo> m_EntityEmitterDebugInfo;
#endif
};

extern audEmitterAudioEntity g_EmitterAudioEntity;

#endif // EMITTERAUDIOENTITY_H_INCLUDED

