// 
// audio/cutsceneaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_CUTSCENEAUDIOENTITY_H
#define AUD_CUTSCENEAUDIOENTITY_H

#include "streamslot.h"

#include "scene/RegdRefTypes.h"

#include "audioentity.h"
#include "atl/array.h"
#include "audiosoundtypes/soundcontrol.h"
#include "cutfile/cutfevent.h"

class audScene;

namespace rage
{
	struct CutsceneSettings;
};

enum CutsceneAudioType
{
	AUD_CUTSCENE_TYPE_NONE,
	AUD_CUTSCENE_TYPE_CUTSCENE,
	AUD_CUTSCENE_TYPE_SYNCHED_SCENE
};

struct audCutSceneEvent
{
	void Init()
	{
		PartialHash = 0;
		ScenePartialHash = 0;
		TriggerTime = 0;
		StartOffset = 0;
		AudioOffset = 0;
		Duration = -1; //We might want to get rid of this...doesn't seem accurate.
		IsConcatSkip = false;
		TrackName[0] = '\0';
		SceneName[0] = '\0';
	}
	u32 PartialHash;
	u32 ScenePartialHash;
	u32 TriggerTime;
	u32 StartOffset;
	u32 AudioOffset;
	s32 Duration;
	bool IsConcatSkip;

	u32 GetCutsceneTimeOffset() {return StartOffset-AudioOffset;}

#if 1 //__BANK make these bank after full transition to audCutSceneTrack system
	char TrackName[64];
	char SceneName[64];
#endif 
};

class audCutTrack
{
protected:
	enum CutSceneTrackState
	{
		UNINITIALIZED,
		INITIALIZED,
		PREPARING,
		PREPARED,
		ACTIVE,
		ENDING
	};

public:

	static const u32 kNumAudCutsceneEvents = 32;
	static const u32 kNumAudSynchedSceneEvents = 2;
	
	audCutTrack()
	{
		Reset();
	}

	virtual ~audCutTrack() {};

	
	virtual bool IsPrepared() = 0;
	virtual void Update() = 0;
	virtual void Trigger() = 0;
	virtual void Stop() = 0;
	virtual void Reset();


	bool GetIsControllingCutsceneAudio() { return m_IsControllingCutsceneAudio; }

protected:

	void SetIsControllingCutsceneAudio(bool isControlling) { m_IsControllingCutsceneAudio = isControlling; }

	atRangeArray<audCutSceneEvent, kNumAudCutsceneEvents> m_EventList;

	bool m_IsControllingCutsceneAudio;
};

class audCutTrackCutScene : public audCutTrack
{
public:

	audCutTrackCutScene()
	{
		m_TriggerTime = 0;
		m_LastSoundTime = 0;
		m_LastEngineTime = 0;
		m_LastCutsceneTime = 0;
#if !__FINAL
		m_Scrubbingoffset = 0;
#endif
#if GTA_REPLAY 
		m_StartCutsceneTime = 0;
#endif 
		Reset();	
	}

	virtual ~audCutTrackCutScene() {};
	virtual void Init(u32 skipTimeInMs = 0);
	virtual bool IsPrepared();
	virtual void Update();
	virtual void Trigger();
	virtual void Stop();
	void Stop(bool isSkipping, u32 release = 500);
	virtual void Reset();
	virtual u32 GetTrackTime(bool isUpdate = false);
	void HandlePlay();
	void SkipToTime(u32 timInMs);
	bool IsActive() {return m_State == ACTIVE; }
	bool IsInitialized() {return m_State >= INITIALIZED; } 
	virtual s32 GetCurrentPlayIndex() { return m_CutscenePlayIndex; }
	virtual audCutSceneEvent& GetEventFromIndex(u32 index) { return m_EventList[index]; }
	virtual u32 GetPlayEventIndex() {return m_PlayEventIndex; }
	virtual u32 GetNumEvents() { return m_NumEvents; }
	void SetScriptOverride(const char * overrideStr)
	{
		formatf(m_ScriptSuffix, "%s", overrideStr);
	}
	void UnsetScriptOverride()
	{
		m_ScriptSuffix[0] = '\0';
	}

	const char * GetScriptSuffix()
	{
		if(strlen(m_ScriptSuffix))
		{
			return m_ScriptSuffix;
		}
		return NULL;
	}

	void SetCanOverrun(bool canOverrun)
	{
		m_CanOverrun = canOverrun;
	}

protected:
	char m_ScriptSuffix[32];
	u32 m_LoadEventIndex;
	s32 m_PlayEventIndex;
	u32 m_NumEvents;
	s32 m_CutsceneLoadIndex;
	s32 m_CutscenePlayIndex;
	u32 m_TriggerTime;
	u32 m_LastEngineTime;
	u32 m_LastSoundTime;
	u32 m_LastCutsceneTime;
	CutSceneTrackState m_State;
	bool m_CanOverrun;

#if GTA_REPLAY 
	u32 m_StartCutsceneTime;
#endif //GTA_REPLAY

#if !__FINAL
	u32 m_Scrubbingoffset;
#endif


};

class audCutTrackSynchedScene : public audCutTrack
{
	public: 
	virtual bool IsPrepared();
	virtual void Update();
	virtual void Trigger();
	virtual void Stop();

	bool IsPrepared(const char * audioName);
	void Trigger(u32 nameHash);
	void Stop(u32 nameHash);

	u32 GetNextSynchedSceneInQueue();
	void Init(const char* audioName, u32 skipMs = 0, u32 parentHash = 0);

	bool HasSyncSceneBeenInitialized(const char* sceneName);
	bool HasSyncSceneBeenTriggered(const char* sceneName);

	u32 GetSceneTime(u32 audioHash);
	u32 GetSceneAudioDuration(u32 hash);
	void ActivateLooping(u32 hash);
	void SkipToTime(u32 audioHash, u32 skipMs);
	void SetEntity(u32 hash, CEntity * entity);
	void SetPosition(u32 hash, Vec3V_In position);
	void ClearUnusedEntities();

	bool UsePosition(int index)
	{
		if(index < 0)
		{
			return false;
		}
		return m_UseScenePosistion[index];
	}
	bool UseEntity(int index)
	{
		if(index < 0)
		{
			return false;
		}
		return m_UseSceneEntity[index];
	}

	Vec3V_Out GetPlayPosition(int index)
	{
		return m_ScenePosition[index];
	}
	CEntity * GetPlayEntity(int index)
	{
		return m_SceneEntity[index];
	}
	int GetEventIndexFromHash(u32 nameHash);


protected:

	enum{
		LOOPING_MASTER = BIT(0),
		LOOPING_SLAVE = BIT(1)
	};

	void Reset();
	void Reset(int index);


	Vec3V m_ScenePosition[kNumAudSynchedSceneEvents];
	bool m_UseScenePosistion[kNumAudSynchedSceneEvents];
	RegdEnt m_SceneEntity[kNumAudSynchedSceneEvents];
	bool m_UseSceneEntity[kNumAudSynchedSceneEvents];

	CutSceneTrackState m_EventStates[kNumAudSynchedSceneEvents];
	int m_CutsceneIndexes[kNumAudSynchedSceneEvents];
	u32 m_TriggerTimes[kNumAudSynchedSceneEvents];
	u32 m_Flags[kNumAudSynchedSceneEvents];
	u32 m_LastSoundTimes[kNumAudSynchedSceneEvents];
	u32 m_LastEngineTimes[kNumAudSynchedSceneEvents];

	int m_SynchSceneToTrigger[kNumAudSynchedSceneEvents];
	bool m_TriggerCutscene;

};



struct audIntermediateCutsceneEvent
{
	cutfEvent * cutEvent;
	float triggerTime;
	u32 concatSection;
};

class audCutsceneAudioEntity : public naAudioEntity
{
	AUDIO_ENTITY_NAME(audCutsceneAudioEntity)
public:
	audCutsceneAudioEntity();

	void Init(void); 
	virtual void Shutdown(void);

	audPrepareState Prepare(audCutSceneEvent &cutEvent, const CutsceneAudioType type, s32 slotIndex);
	bool IsPrepared(s32 slot);
	void Play(s32 slotIndex = -1);
	virtual void PreUpdateService(u32 timeInMs);
	void Stop(bool shouldStopAllStreams = false, s32 slotIndex = -1, s32 release = 500); 
	void Pause();
	void UnPause();
	u32 GetWaveslotIndexForPrepare() {return m_NextWaveSlotIndex; }
	u32 GetNextWaveslotIndexForPrepare() {return (m_NextWaveSlotIndex+1)%2; }
	CutsceneAudioType GetSlotCutsceneAudioType(u32 slotIndex) { return m_CutsceneAudioType[slotIndex]; }

	bool SlotIsFree(u32 slotIndex)
	{
		return m_Sounds[slotIndex] == NULL;
	}

	bool SlotIsPlaying(u32 slotIndex);

	void SetVariableOnSound(u32 variableName, float value, s32 slotIndex = -1);

	void HandleSeamlessPlay(s32 loadSlotIndex);

	s32 GetPlayTimeMs(s32 slotIndex = -1);
	s32 GetPlayTimeIncludingOffsetMs(s32 slotIndex); 
	s32 ComputeSoundDuration(s32 slotIndex = -1);

	audStreamSlot* GetStreamSlot(s32 slotIndex)
	{
		return m_StreamSlots[slotIndex];
	}

	static bool OnStopCallback(u32 userData);
	static bool HasStoppedCallback(u32 userData);

	bool IsCutsceneTrackPrepared();
	bool IsCutsceneTrackActive();
	bool IsSyncSceneTrackPrepared(const char * audioName);
	void TriggerCutscene();
	void StopCutscene(bool isSkipping = false, u32 releaseTime = 500);
	void StopOverunAudio();
	void InitCutsceneTrack(u32 skipTimeInMs = 0);
	u32 GetCutsceneTime();
	void SkipCutsceneToTime(u32 skipTimeMs)
	{
		m_CutsceneTrack.SkipToTime(skipTimeMs);
	}
	u32 GetNumCutsceneEvents() { return m_CutsceneTrack.GetNumEvents(); }

	bool CutsceneAudioIsAvailable() { return !m_CutsceneTrack.GetIsControllingCutsceneAudio() && !m_SynchedSceneTrack.GetIsControllingCutsceneAudio(); }

	void TriggerSyncScene(u32 hash);
	u32 TriggerSyncScene();
	void StopSyncScene(u32 audioHash);
	void StopSyncScene();
	u32 GetSyncSceneTime(u32 audioHash);
	u32 GetSyncSceneDuration(u32 audioHash);
	void SkipSynchedSceneToTime(u32 audioHash, u32 skipMs)
	{
		m_SynchedSceneTrack.SkipToTime(audioHash, skipMs);
	}
	void SetScenePlayEntity(u32 audioHash, CEntity * entity)
	{
		m_SynchedSceneTrack.SetEntity(audioHash, entity);
	}
	void SetScenePlayPosition(u32 audioHash, Vec3V_In position)
	{
		m_SynchedSceneTrack.SetPosition(audioHash, position);
	}

	void InitSynchedScene(const char * audioName, u32 skipToMs = 0)
	{
		m_SynchedSceneTrack.Init(audioName, skipToMs);
	}

	void SetScriptCutsceneOverride(const char * overrideStr)
	{
		m_CutsceneTrack.SetScriptOverride(overrideStr);
	}
	void UnsetScriptCutsceneOverride()
	{
		m_CutsceneTrack.UnsetScriptOverride();
	}

	void PostUpdate();

	u32 GetEngineTime();

	virtual bool IsUnpausable() const { return true; }

	audScene * GetMixScene(u32 index) { return m_Scenes[index]; }

	const audSound *GetSound(int index) { return m_Sounds[index]; }
	BANK_ONLY(const char *GetSoundName(int index) { return m_SoundNames[index]; })

	bool HasSyncSceneBeenInitialized(const char* sceneName)
	{
		return m_SynchedSceneTrack.HasSyncSceneBeenInitialized(sceneName);
	}

	bool HasSyncSceneBeenTriggered(const char* sceneName)
	{
		return m_SynchedSceneTrack.HasSyncSceneBeenTriggered(sceneName);
	}



protected:



	audCutTrackCutScene m_CutsceneTrack;
	audCutTrackSynchedScene m_SynchedSceneTrack;
	
	atRangeArray<Vec3V, 2> m_PlayPos;
	atRangeArray<u32, 2> m_SyncIds;
	atRangeArray<audSound *, 2> m_Sounds;
	struct ShadowSounds
	{
		ShadowSounds()
			: Left(NULL)
			, Right(NULL)
		{
		}
		audSound *Left;
		audSound *Right;
	};
	atRangeArray<ShadowSounds, 2> m_StereoShadowSounds;
	atRangeArray<audStreamSlot *, 2> m_StreamSlots;
	atRangeArray<u32, 2> m_StartPrepareTime;
	atRangeArray<audScene*, 2> m_Scenes;
	atRangeArray<u32, 2> m_ScenePartialHashes;
	atRangeArray<CutsceneAudioType, 2> m_CutsceneAudioType;
	atRangeArray<bool, 2> m_IsPrepared;
	atRangeArray<RegdConstEnt, 2> m_PlayEntity;
	char m_SoundNames[2][64];
	u32 m_SoundHashes[2];
#if __BANK
	char m_SceneNames[2][64];
#endif

	u32 m_NextWaveSlotIndex;
	u32 m_LastStopTime;

	bool m_PlayedThisFrame;
	bool m_NeedToPreloadRadio;
	bool m_WasFaded;
	atRangeArray<bool, 2> m_UsePlayPosition;
};

extern audCutsceneAudioEntity g_CutsceneAudioEntity;
extern bool g_CutsceneAudioPaused;
extern float g_CutsceneAudioCatchupFactor;
extern s32 g_CutsceneTimeOffset;
extern float g_CutsceneTimeScale;
extern bool g_DidCutsceneJumpThisFrame;

#endif // AUD_CUTSCENEAUDIOENTITY_H
