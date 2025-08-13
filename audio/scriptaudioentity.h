// 
// audio/scriptaudioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_SCRIPTAUDIOENTITY_H
#define AUD_SCRIPTAUDIOENTITY_H

#include "atl/array.h"
#include "atl/bitset.h"
#include "speechaudioentity.h"
#include "script/gta_thread.h"
#include "script/handlers/GameScriptIds.h"
#include "streamslot.h"
#include "system/filemgr.h"
#include "text/messages.h"

#include "hashactiontable.h"


#define AUD_MAX_SCRIPTED_CONVERSATION_LINES 60
#define AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS 36 // 0-9 and A-Z - 9 is reserved, but to keep it simple we still allocate it a space
#define AUD_MAX_SCRIPT_BANK_SLOTS 11
// we will only have 7 slots in the general pool, this allow us to place specific banks in the last 4 slots(replay)
#define AUD_USABLE_SCRIPT_BANK_SLOTS 10   
#define AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS 2
#define AUD_MAX_SCRIPT_SOUNDS 100
#define AUD_MAX_SCRIPTS 20
#define AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD 4
#define AUD_MAX_SUBTITLE_SUB_STRINGS 2
#define AUD_NET_ALL_PLAYERS 0xFFFFFFFF

#if GTA_REPLAY

#define AUD_REPLAY_SCRIPTED_SPEECH_SLOTS		16
#define AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS	32
#define AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES	2

#endif

enum audScriptStreamState
{
	AUD_SCRIPT_STREAM_IDLE = 0,
	AUD_SCRIPT_STREAM_PREPARING,
	AUD_SCRIPT_STREAM_PREPARED,
	AUD_SCRIPT_STREAM_PLAYING,
};

enum audMissionCompleteState
{
	AUD_MISSIONCOMPLETE_IDLE = 0,
	AUD_MISSIONCOMPLETE_PREPARING,
	AUD_MISSIONCOMPLETE_DELAY,
	AUD_MISSIONCOMPLETE_PLAYING,
};

enum audAircraftWarningTypes{
	AUD_AW_TARGETED_LOCK,
	AUD_AW_MISSLE_FIRED,
	AUD_AW_ACQUIRING_TARGET,
	AUD_AW_TARGET_ACQUIRED,
	AUD_AW_ALL_CLEAR,
	AUD_AW_PLANE_WARNING_STALL,
	AUD_AW_ALTITUDE_WARNING_LOW,
	AUD_AW_ALTITUDE_WARNING_HIGH,
	AUD_AW_ENGINE_1_FIRE,
	AUD_AW_ENGINE_2_FIRE,
	AUD_AW_ENGINE_3_FIRE,
	AUD_AW_ENGINE_4_FIRE,
	AUD_AW_DAMAGED_SERIOUS,
	AUD_AW_DAMAGED_CRITICAL,
	AUD_AW_OVERSPEED,
	AUD_AW_TERRAIN,
	AUD_AW_PULL_UP,
	AUD_AW_LOW_FUEL,

	AUD_AW_NUM_WARNINGS
};

enum audPlayStreamType{
	AUD_PLAYSTREAM_UNKNOWN,
	AUD_PLAYSTREAM_ENTITY,
	AUD_PLAYSTREAM_POSITION,
	AUD_PLAYSTREAM_FRONTEND,
	AUD_PLAYSTREAM_NUM_TYPES
};

struct audAircraftWarningStruct
{
	audAircraftWarningStruct() :
			LastTimeTriggered(0),
			LastTimePlayed(0),
			MaxTimeBetweenTriggerAndPlay(5000),
			MinTimeBetweenPlay(5000),
			ShouldPlay(false)
		{
			Context[0] = '\0';
		}

	char Context[64];
	u32 LastTimeTriggered;
	u32 LastTimePlayed;
	u32 MaxTimeBetweenTriggerAndPlay;
	u32 MinTimeBetweenPlay;
	bool ShouldPlay;
};

// PURPOSE
//	Script audio flags default to false, support multiple script threads (result is logical OR) and auto-reset on script termination
// NOTES
//	Ensure corresponding enum in commands_audio.sch is kept in sync!
struct audScriptAudioFlags
{
	enum FlagId
	{
#define SCRIPT_AUDIO_FLAG(n,h) n,
#include "scriptaudioflags.h"
#undef SCRIPT_AUDIO_FLAG

		kNumScriptAudioFlags
	};

	struct tFlagNames
	{
		u32 Hash;
		BANK_ONLY(const char *Name);
	};
	static const audScriptAudioFlags::tFlagNames s_FlagNames[kNumScriptAudioFlags];

	static FlagId FindFlagId(const char *flagName)
	{
		const u32 hash = atStringHash(flagName);
		for(s32 i = 0; i < kNumScriptAudioFlags; i++)
		{
			if(s_FlagNames[i].Hash == hash)
			{
				return static_cast<FlagId>(i);
			}
		}
		audWarningf("Invalid script audio flag name '%s'", flagName);
		return kNumScriptAudioFlags;
	}

	static bool IsValid(const FlagId flagId)
	{
		return (flagId < kNumScriptAudioFlags);
	}

#if __BANK
	static const char *GetName(const FlagId flagId)
	{
		return s_FlagNames[flagId].Name;
	}
#endif

private:



};

class CPed;
class CEntity;
class audScene;

//?#include "audiosoundtypes/speechsound.h"

struct tScriptedConversationEntryPointers
{
	s32 Speaker;
	char* Context;
	char* Subtitle;
};

struct tScriptedConversationEntry
{
	s32 Speaker;
	char Context[64];
	char Subtitle[32];
	s32 Listener;
	s32 SpeechSlotIndex;
	audConversationAudibility Audibility;
	u8 Volume;
	u8 PreloadSet;
	u8 UnresolvedLineNum;  //Returned by GetCurrentUnresolvedScriptedConversationLine, which is used by script when
							// they are handling subtitles (CnC BAD_COP warnings, etc)
	bool DoNotRemoveSubtitle:1;
	bool IsRandom:1;
	bool IsInterruptible:1;
	bool DucksScore:1;
	bool DucksRadio:1;
	bool Headset:1;
	bool isPadSpeakerRoute:1;
	bool FailedToLoad:1;
	bool DontInterruptForSpecialAbility:1;
	bool HasBeenInterruptedForSpecialAbility:1;
	bool HasRepeatedForRagdoll:1;
	bool IsPlaceholder:1;
#if __BANK
	bool DisplayedDebug:1;
#endif
#if !__FINAL
	u32 TimePrepareFirstRequested;
#endif
};

struct tScriptedSpeechSlots
{
	audWaveSlot* WaveSlot;
	audSpeechSound* ScriptedSpeechSound;
	crClip* AssociatedClip;
	crClipDictionary* AssociatedDic;
	s32 VariationNumber;
	s32 LipsyncPredelay;
	u32 PauseTime;
#if !__FINAL
	char VoiceName[64];
	char Context[64];
#endif
};

#if GTA_REPLAY
struct tReplayScriptedSpeechSlots
{
	audWaveSlot* WaveSlot;
	audSpeechSound* ScriptedSpeechSound;
	s32 VariationNumber;
	u32 VoiceNameHash;
	u32 ContextNameHash;
	u32 TimeStamp;
	s32 WaveSlotIndex;
};
#endif

struct audCarPedCollisionStruct
{
	CPed* Victim;
	u32 TimeOfCollision;
};

typedef enum
{
	AUD_SCRIPT_BANK_QUEUED,
	AUD_SCRIPT_BANK_LOADING,
	AUD_SCRIPT_BANK_LOADED
}audScriptSlotState;

enum
{
	AUD_CELLPHONE_NORMAL,
	AUD_CELLPHONE_QUIET,
	AUD_CELLPHONE_SILENT,
	AUD_CELLPHONE_VIBE,
	AUD_CELLPHONE_VIBE_AND_RING
};

typedef enum
{
	AUD_INTERRUPT_LEVEL_COLLISION_NONE,
	AUD_INTERRUPT_LEVEL_COLLISION_LOW,
	AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM,
	AUD_INTERRUPT_LEVEL_COLLISION_HIGH,
	AUD_INTERRUPT_LEVEL_WANTED,
	AUD_INTERRUPT_LEVEL_IN_AIR,
	AUD_INTERRUPT_LEVEL_SLO_MO,
	AUD_INTERRUPT_LEVEL_SCRIPTED
}audCollisionLevel;

struct audScriptBankSlot
{
public:

	static const u32 INVALID_BANK_ID = 0xffffffff;

	audScriptBankSlot() :
	  BankSlot(NULL),
	  State(AUD_SCRIPT_BANK_LOADED),
	  ReferenceCount(0),
	  HintLoaded(false),
	  BankId(0xffff),
	  BankNameHash(g_NullSoundHash),
	  TimeWeStartedWaitingToFinish(0)
	{
		for(int i=0; i<AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS; i++)
		{
			NetworkScripts[i].Invalidate();
		}
	}

	audWaveSlot* BankSlot;
	audScriptSlotState State;
	// we sometimes tell all sounds to stop, then have to keep checking if the slot's still in use - use this to spot when we've waited unacceptably long.
	u32 TimeWeStartedWaitingToFinish; 
	s16 ReferenceCount;
	// will be the id of the bank we're waiting to load, if there's one queued.
	u16 BankId; 
	// hash of the bank name, needed for replay so lets just add it in, maybe handy for debugging too
	u32 BankNameHash;
	// To know this bank has been loaded from a light request.
	bool HintLoaded;
	// scripts that have a reference to this bank over the network
	atRangeArray<CGameScriptId, AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS> NetworkScripts;
};

struct audScriptSound
{
public:

	static const u32 INVALID_NETWORK_ID = 0xffffffff;

	audScriptSound() :
		SoundRef(),
		Sound(NULL),
		EnvironmentGroup(NULL),
		Entity(NULL),
		ScriptIndex(-1),
		NetworkId(INVALID_NETWORK_ID),
		ScriptHasReference(FALSE),
		OverNetwork(FALSE)
	{
		ScriptId.Invalidate();
		BANK_ONLY(formatf(SoundTriggeredName,"");)
		BANK_ONLY(formatf(SoundSetTriggeredName,"");)
		BANK_ONLY(formatf(ScriptName,"");)
		BANK_ONLY(TimeAllocated = 0;)
		BANK_ONLY(FrameAllocated = 0;)

#if GTA_REPLAY
		SoundNameHash = g_NullSoundHash;
		SoundSetHash = g_NullSoundHash;
		ShouldRecord = true;
		ClearReplayVariableNameHashs();
#endif

	};

#if GTA_REPLAY
	void ClearReplayVariableNameHashs()
	{
		for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
		{
			VariableNameHash[i] = g_NullSoundHash;
			VariableValue[i] = 0.0f;
		}
	}

	void StoreSoundVariable(u32 variableNameHash, f32 value)
	{
		for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
		{
			if(VariableNameHash[i] == variableNameHash)
			{
				VariableValue[i] = value;
				return;
			}
		}
		for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
		{
			if(VariableNameHash[i] == g_NullSoundHash)
			{
				VariableNameHash[i] = variableNameHash;
				VariableValue[i] = value;
				return;
			}
		}
	}
#endif

	void Reset()
	{
		SoundRef = audMetadataRef();
		Sound = NULL;
		EnvironmentGroup = NULL;
		Entity = NULL;
		ScriptIndex = -1;
		NetworkId = INVALID_NETWORK_ID;
		ScriptId.Invalidate();
		ScriptHasReference = false;
		OverNetwork = false;
		BANK_ONLY(formatf(SoundTriggeredName,"");)
		BANK_ONLY(formatf(SoundSetTriggeredName,"");)
		BANK_ONLY(formatf(ScriptName,"");)
		BANK_ONLY(TimeAllocated = 0;)
		BANK_ONLY(FrameAllocated = 0;)

#if GTA_REPLAY
		SoundNameHash = g_NullSoundHash;
		SoundSetHash = g_NullSoundHash;
		ShouldRecord = true;
		ClearReplayVariableNameHashs();
#endif
	}

	audMetadataRef SoundRef;
	audSound* Sound;
	naEnvironmentGroup* EnvironmentGroup;
	RegdEnt Entity;
	// Index into our array of scripts that are currently running.
	s32 ScriptIndex;
	// For Networked sounds
	u32 NetworkId; 
	CGameScriptId ScriptId;
	// True if the script has a reference to this sound, so we need to keep this slot once the sound's finished
	bool ScriptHasReference;	
	// True if this sound is sent over the network
	bool OverNetwork;
	BANK_ONLY(char SoundTriggeredName[128];)// To store the name on the soundset.
	BANK_ONLY(char SoundSetTriggeredName[128];)// To store the name of the soundset.
	BANK_ONLY(char ScriptName[128];)
	BANK_ONLY(u32 TimeAllocated;)
	BANK_ONLY(u32 FrameAllocated;)
#if GTA_REPLAY
	u32 SoundSetHash;
	u32 SoundNameHash;
	bool ShouldRecord;
	audSoundInitParams InitParams;

	u32 VariableNameHash[AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES];
	f32 VariableValue[AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES];
#endif
};

struct audScriptScene
{
	audScriptScene()
		:m_scene(NULL)
		 ,m_next(NULL)
	{}


	audScene * m_scene;
	audScriptScene * m_next;
};

static const s32 g_MaxAudScriptVariables = 8;

class audScript
{
#if __BANK
	friend class audScene;
#endif

public:
	audScript()
		: ScriptThreadId(THREAD_INVALID)
		, BankSlotIndex(0)
		, scenes(NULL)
		, m_SlowMoMode(0)
		, m_ScriptWeaponAudioHash(0)
		, m_ScriptVehicleHash(0)
		, m_ScriptShutDownDelay(0)
		, m_ScriptisShuttingDown(false)
	{
	};

	~audScript();
    void Shutdown();
	
	audScene* GetScene(u32 sceneHash);
	bool StartScene(u32 sceneHash);
	void StopScene(u32 sceneHash);
	void StopScenes();

	void SetSceneVariable(u32 sceneHash, u32 variableHash, f32 value);
	bool SceneHasFinishedFade(u32 sceneHash);

	const variableNameValuePair* GetVariableAddress(u32 nameHash) const;
//	const variableNameValuePair* GetVariableAddress(const char * name);
	void SetVariableValue(const char *name, f32 value);
	void SetVariableValue(u32 nameHash, f32 value);	

	bool RequestScriptWeaponAudio(const char * itemName);
	void ReleaseScriptWeaponAudio();

	bool RequestScriptVehicleAudio(const u32 vehicleModelName);

	void SetAudioFlag(const char *flagName, const bool enabled);
	void SetAudioFlag(const audScriptAudioFlags::FlagId flagId, const bool enabled);

	void ActivateSlowMoMode(const char *mode);
	void DeactivateSlowMoMode(const char *mode);
	
	scrThreadId ScriptThreadId;
	u32 BankSlotIndex;
	u32 BankOverNetwork;
	u32 PlayerBits;
	audScriptScene * scenes; //linked list of scenes associated with the script
	atArray<u32> m_ScriptAmbientZoneChanges;
	atArray<audSpeechAudioEntity*> m_PedsSetToAngry;
	atArray<audVehicleAudioEntity*> m_PriorityVehicles;

	s32 m_ScriptShutDownDelay;
	bool m_ScriptisShuttingDown : 1;

	atFixedBitSet<audScriptAudioFlags::kNumScriptAudioFlags> m_FlagResetState;

private:
	atRangeArray<variableNameValuePair, g_MaxAudScriptVariables> m_Variables;

	u32 m_SlowMoMode;
	u32 m_ScriptWeaponAudioHash;
	u32 m_ScriptVehicleHash;
};

class audScriptAudioEntity : public naAudioEntity
{
#if GTA_REPLAY
	friend class CPacketSoundCreate;
#endif
	friend class audScript;

public:
	AUDIO_ENTITY_NAME(audScriptAudioEntity);

	static void InitClass();
	static void ShutdownClass(){}
	void Init(void);
	void Shutdown();
	virtual void PreUpdateService(u32 timeInMs);
	void UpdateConversation();
	void PostUpdate();

	bool	StartMobilePhoneRinging(s32 ringtoneId);
	bool	StartMobilePhoneCalling();
	void	StopMobilePhoneRinging();
	void	SetMobileRingType(s32 ringTypeId);

	void	StartPedMobileRinging(CPed *ped, const s32 ringtoneId);
	void	StopPedMobileRinging(CPed *ped);
	void	SetPedMobileRingType(CPed *ped, const s32 ringType);

	void	PlayNPCPickup(const u32 pickupHash, const Vector3& pickupPosition);

	void	StartNewScriptedConversationBuffer();
	void	AddLineToScriptedConversation(u32 lineNumber, s32 speakerNumber, const char* pContext, const char* pSubtitle, s32 listenerNumber, u8 volume=0,
				bool isRandom=false, bool interruptible = true, bool ducksRadio = true, bool ducksScore = false, 
				audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL, bool headset = false, bool dontInterruptForSpecialAbility = false, bool isPadSpeakerRoute = false);
	void	GetStreetAndZoneStringsForPos(const Vector3& pos, char* streetName, u32 streetNameSize, char* zoneName, u32 zoneNameSize);
	void	AddConversationSpeaker(u32 speakerConversationIndex, CPed* speaker, char* voiceName);
	void	AddFrontendConversationSpeaker(u32 speakerConversationIndex, char* voiceName);
	void	SetConversationControlledByAnimTriggers(bool enable);
	bool	GetIsConversationControlledByAnimTriggers() { return m_AnimTriggersControllingConversation; }
	void	HandleConversaionAnimTrigger(CPed* ped);
	void	SetConversationPlaceholder(bool isPlaceholder);
	bool	GetIsConversationPlaceholder() const { return m_IsConversationPlaceholder; }
	bool	StartConversation(bool mobileCall, bool displaySubtitles, bool addToBriefing = true, bool cloneConversation = false, bool interruptible = true, bool preload = false);
	bool	IsScriptedConversationOngoing();
	bool	IsScriptedConversationAMobileCall();
	bool	IsScriptBankLoaded(u32 bankID);
	bool	IsScriptedConversationPaused();
	bool	IsScriptedConversationLoaded();
	s32		GetCurrentScriptedConversationLine();
	s32		GetCurrentUnresolvedScriptedConversationLine();
	s32		AbortScriptedConversation(bool finishCurrentLine BANK_ONLY(,const char* reason));
	void	PauseScriptedConversation(bool finishCurrentLine, bool repeatCurrentLine = true, bool fromScript=false);
	void	RestartScriptedConversation(bool startAtMarker=false, bool fromScript=false);
	void	SkipToNextScriptedConversationLine();
	bool	ShouldDuckForScriptedConversation();
	bool	ShouldNotDuckRadioThisLine();
	bool	ShouldDuckScoreThisLine();
	void	SetNoDuckingForConversationFromScript(bool enable);
	s32		GetVariationChosenForScriptedLine(u32 filenameHash);
	bool	IsPedInCurrentConversation(const CPed* ped);
	void	SetPositionForNullConvPed(s32 speakerNumber, const Vector3& position);
	void	SetEntityForNullConvPed(s32 speakerNumber, CEntity* entity);
	bool	ShouldTreatNextLineAsUrgent() { return m_TreatNextLineToPlayAsUrgent; }
	bool	IsPreloadedScriptedSpeechReady();
	audScriptBankSlot* GetBankSlot(u32 index) { return &m_BankSlots[index]; }

	void	SayAmbientSpeechFromPosition(const char* context, const u32 voiceHash, const Vector3& pos, const char* speechParams);

	bool RequestScriptWeaponAudio(const char * itemName);
	void ReleaseScriptWeaponAudio();

	bool RequestScriptVehicleAudio(u32 vehicleName);

	void	RegisterCarInAir(CVehicle* vehicle);
	void	RegisterSlowMoInterrupt(SlowMoType slowMoType);
	void	StopSlowMoInterrupt();
	void	RegisterThrownFromVehicle(CPed* ped);
	void	RegisterPlayerCarCollision(CVehicle* vehicle, CEntity* inflictor, f32 applyDamage);
	void    RegisterCarPedCollision(CVehicle* vehicle, f32 applyDamage, CPed* victim = NULL);
	void	RegisterWantedLevelIncrease(CVehicle* vehicle);
	void	RegisterScriptRequestedInterrupt(CPed* interrupter, const char* context, const char* voiceName, bool pause BANK_ONLY(, bool force = false));
	void	InterruptConversation();
	u32		GetTimeOfLastCollisionReaction() { return m_TimeOfLastCollisionReaction; }
	void	SetAircraftWarningSpeechVoice(CVehicle* vehicle);
	void	TriggerAircraftWarningSpeech(audAircraftWarningTypes warningType);

	void	GetSpeechForEmergencyServiceCall(char* RetString);

	s32		AddScript();
	void	RemoveScript(scrThreadId scriptThreadId);
	void	SetScriptCleanupTime(u32 delay);

	audWaveSlot *	AllocateScriptBank();
	bool	FreeUpScriptBank(audWaveSlot *slot);
	bool	RequestScriptBank(const char* bankName, bool bOverNetwork, unsigned playerBits, bool hintRequest = false);
	bool	RequestScriptBank(u32 bankId, bool bOverNetwork, unsigned playerBits);
#if GTA_REPLAY
	void	CleanUpScriptBankSlotsAfterReplay();
	void	CleanUpScriptAudioEntityForReplay();
	bool	ReplayLoadScriptBank(u32 bankNameHash, u32 bankSlot);
	void	ReplayFreeScriptBank(u32 bankSlot);
	void	ClearRecentPedCollisions();
#endif
	void	HintScriptBank(const char* bankName, bool bOverNetwork, unsigned playerBits);
	void	HintScriptBank(u32 bankId, bool bOverNetwork, unsigned playerBits);
	void	ScriptBankNoLongerNeeded(); 
	void	ScriptBankNoLongerNeeded(const char * bankName);
	void	ScriptBankNoLongerNeeded(u32 bankId);

	bool	RequestScriptBankFromNetwork(u32 bankHash, CGameScriptId& scriptId);
	void	ReleaseScriptBankFromNetwork(u32 bankHash, CGameScriptId& scriptId); 

	audScript *GetScript(s32 index);
	audScript *GetScript() { return GetScript(AddScript()); }

	//Functions called by script---
	bool	StartScene(u32 sceneHash);
	void	StopScene(u32 sceneHash);
	void	StopScenes();

	void	SetSceneVariable(u32 sceneHash, u32 variableHash, f32 value);
	void	SetSceneVariable(const char * sceneName, const char * variableName, f32 value);
	bool	SceneHasFinishedFading(u32 sceneHash);

	void	SetScriptVariable(const char * variableName, f32 value);
	bool	IsSceneActive(const u32 sceneHash);
	bool	IsMobileInterferenceSoundValid() const { return m_MobileInterferenceSound != NULL; }

#if NA_POLICESCANNER_ENABLED
	void	SetPoliceScannerAudioScene(const char* sceneName);
	void	StopAndRemovePoliceScannerAudioScene();
	void	ResetPoliceScannerVariables();
	void	ApplyPoliceScannerAudioScene();
	void	DeactivatePoliceScannerAudioScene();
#endif
	//---

	s32		GetIndexFromScriptThreadId(scrThreadId scriptThreadId);
	s32		GetSoundId();
	void	ReleaseSoundId(s32 soundId);
	void	PlaySound(s32 soundId, const char* soundName, const char * setName, bool bOverNetwork, int nNetworkRange,bool REPLAY_ONLY(enableOnReplay));
	void	PlaySoundFrontend(s32 soundId, const char* soundName, const char * setName, bool REPLAY_ONLY(enableOnReplay));
	void	PlaySoundFromEntity(s32 soundId, const u32 soundNameHash, const CEntity* entity, const u32 setNameHash, bool bOverNetwork, int nNetworkRange OUTPUT_ONLY(, const char* soundSetName, const char* soundName));
	void	PlaySoundFromEntity(s32 soundId, const char* soundName, const CEntity* entity, const char * setName, bool bOverNetwork, int nNetworkRange);
	void	PlaySoundFromEntity(s32 soundId, const u32 soundNameHash, const CEntity* entity, naEnvironmentGroup* environmentGroup, const u32 setNameHash, bool bOverNetwork, int nNetworkRange, const bool ownsEnvGroup OUTPUT_ONLY(, const char* soundSetName, const char* soundName));
	void	PlaySoundFromPosition(s32 soundId, const char* soundName, const Vector3* position, const char * setName, bool bOverNetwork, int nNetworkRange, const bool isExteriorLoc);
	void	PlayFireSoundFromPosition(s32 soundId, const Vector3* position);
	void	UpdateSoundPosition(s32 soundId, const Vector3* position);
	void	StopSound(s32 soundId);
	void	SetVariableOnSound(s32 soundId, const char* variableName, f32 variableValue);
	void	SetVariableOnStream(const char * variableName, f32 variableValue);
	bool	HasSoundFinished(s32 soundId);
	void	ResetScriptSound(const s32 soundId);

	bool	IsPlayingStream();

	const u32 GetStreamPlayTime() const;

	bool	PlaySoundFromNetwork(u32 nNetworkId, CGameScriptId& scriptId, CEntity* entity, u32 setNameHash, u32 soundNameHash);
	bool	PlaySoundFromNetwork(u32 nNetworkId, CGameScriptId& scriptId, const Vector3* position, u32 setNameHash, u32 soundNameHash);
	void	StopSoundFromNetwork(u32 nNetworkId);

	u32		GetNetworkIdFromSoundId(s32 soundId);
	s32		GetSoundIdFromNetworkId(u32 nNetworkId);
	
	void	HandleScriptAudioEvent(u32 hash, CEntity* entity);

	void	PlayMobilePreRing(u32 mobileInterferenceHash, f32 probability);
	void	PlayMobileGetSignal(f32 probability);

	void	ClearUpScriptSetVariables();

	void	EnableChaseAudio(bool enable);
	void	EnableAggressiveHorns(bool enable);

	void	DontAbortCarConversations(bool allowUpsideDown, bool allowOnFire, bool allowDrowning);
	void	DontAbortCarConversationsWithScriptId(bool allowUpsideDown, bool allowOnFire, bool allowDrowning, s32 scriptIndex);
	void	MuteGameWorldAndPositionedRadioForTV(bool mute);

	void	StopPedSpeaking(CPed *ped, bool shouldDisable );
	void	BlockAllSpeechFromPed(CPed *ped, bool shouldBlock, bool shouldSuppressOutgoingNetworkSpeech );
	void	StopPedSpeakingSynced(CPed *ped, bool shouldDisable );
	void	StopPedPainAudio(CPed *ped, bool shouldDisable );

	void SetVehicleScriptPriority(audVehicleAudioEntity* vehicleEntity, audVehicleScriptPriority priorityLevel, scrThreadId scriptThreadID);
	void SetVehicleScriptPriorityForCurrentScript(audVehicleAudioEntity* vehicleEntity, audVehicleScriptPriority priorityLevel);
	void SetPlayerAngry(audSpeechAudioEntity* speechEntity, bool isAngry);
	void SetPlayerAngryShortTime(audSpeechAudioEntity* speechEntity, u32 timeInMs);

	static void DefaultScriptActionHandler(u32 hash, void *context);

	bool			PreloadStream(const char *streamName, const u32 startOffsetMs, const char * setName);	
	bool			PreloadStreamInternal(const char *streamName, const u32 startOffsetMs, const char * setName);
	void			PlayStreamFrontend();
	void			PlayStreamFrontendInternal();
	void			PlayStreamFromPosition(const Vector3 &pos);
	void			PlayStreamFromPositionInternal(const Vector3 &pos);
	void			PlayStreamFromEntity(CEntity *entity);
	void			PlayStreamFromEntityInternal(CEntity *entity);
	void			StopStream();
	bool			IsStreamSoundValid()	{ return m_StreamSound ? true : false; }

	void			SetFlagState(const audScriptAudioFlags::FlagId flagId, const bool state);

	void SetAmbientZoneNonPersistentState(u32 ambientZoneName, bool enabled, bool forceUpdate);
	void ClearAmbientZoneNonPersistentState(u32 ambientZoneName, bool forceUpdate);

	void TriggerMissionComplete(const char *  missionCompleteId, const s32 timerId = 3);
	void StopMissionCompleteSound();
	bool IsPreparingMissionComplete() const;
	bool IsPlayingMissionComplete() const;
	f32 ComputeRadioVolumeForMissionComplete() const;

	void SetDesiredPedWallaDensity(f32 desiredDensity, f32 desiredDensityApplyFactor, bool interior);
	void SetDesiredPedWallaSoundSet(const char* soundSetName, bool interior);

	void PlayPreviewRingtone(const u32 ringtoneId);
	void StopPreviewRingtone();

	void SetConversationCodename(char codeLetter, u8 codeNumber, const char* userName);
	void SetConversationLocation(const Vector3& location);

	bool IsMissionCompleteReadyForUI() {return m_ShouldShowMissionCompleteUI;};

#if GTA_REPLAY
	static tReplayScriptedSpeechSlots sm_ReplayScriptedSpeechSlot[AUD_REPLAY_SCRIPTED_SPEECH_SLOTS];
	static tReplayScriptedSpeechSlots sm_ReplayScriptedSpeechAlreadyPlayed[AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS];
	static char sm_CurrentStreamName[128];
	static u32 sm_CurrentStartOffset;
	static char sm_CurrentSetName[128];

	bool HasAlreadyPlayed(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber);
	void UpdateTimeStamp(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber);
	void AddToPlayedList(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber);
	void RemoveFromAreadyPlayedList(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber);
	void ClearPlayedList();
	void StopAllReplayScriptedSpeech();
	int GetFreeReplayScriptedSpeechSlot( u32 uVoiceNameHash, u32 uContextNameHash, s32 sVariationNumber, s32 speechSlotIndex);
	int LookupReplayPreloadScriptedSpeechSlot( u32 uVoiceNameHash, u32 uContextNameHash, s32 sVariationNumber);
	void UpdateReplayScriptedSpeechSlots();
	void CleanUpReplayScriptedSpeechSlot(s32 slotIndex, bool force = false);
	void CleanUpReplayScriptedSpeechWaveSlot(s32 waveSlotIndex);
	naEnvironmentGroup* CreateReplayScriptedEnvironmentGroup(const CEntity* entity, const Vec3V& pos) const;
	bool IsPlayerConversationActive(const CPed* pPlayer, s16& currentLine);
	void RecordScriptSlotSound(const int setNameHash, const int soundNameHash, audSoundInitParams* initParams, const CEntity* pEntity, audSound* sound);
	void RecordScriptSlotSound(const char* setName, const char* soundName, audSoundInitParams* initParams, const CEntity* pEntity, audSound* sound);
	void RecordUpdateScriptSlotSound(const u32 setNameHash, const u32 soundNameHash, audSoundInitParams* initParams, const CEntity* pEntity, audSound* sound);
	void StoreSoundVariable(u32 variableNameHash, f32 value);
	ePreloadResult PreloadReplayScriptedSpeech(u32 voiceNameHash, u32 contextHash, u32 variationNumber, s32 m_sWaveSlotIndex);
#endif

#if __BANK
	static const char* GetStreamStateName(audScriptStreamState state) ;
	static void AddWidgets(bkBank &bank);
	void DebugScriptedConversation();
	static void DebugScriptedConversationStatic();
	//void AddCallToLog(const char *scriptCommand);
	void LogScriptBankRequest(const char *request,const u32 scriptIndex,const u32 bankId, const u32 bankIndex);
	void LogNetworkScriptBankRequest(const char *request,const char* scriptName,const u32 bankId, const u32 bankIndex);
	void DrawDebug();
	void DumpScriptSoundIdUsage();
	void ForceLowInterrupt();
	void ForceMediumInterrupt();
	void ForceHighInterrupt();
	const char* GetScriptName(s32 index);

	static void DebugEnableFlag(const char *flagName);
	static void DebugDisableFlag(const char *flagName);
#endif
	static s32 sm_ScriptInChargeOfTheStream;

	u32 GetPoliceScannerSceneHash() {return m_PoliceScannerSceneHash;}

	// PURPOSE
	//	Returns the state of the specified script audio flag.
	// NOTES
	//	Default state is false; returns true if any scripts have currently enabled this flag
	bool IsFlagSet(const audScriptAudioFlags::FlagId flagId) const { return m_FlagState[flagId] > 0; }

	audSpeechAudioEntity& GetSpeechAudioEntity() { return m_SpeechAudioEntity; }
	audSpeechAudioEntity& GetAmbientSpeechAudioEntity() { return m_AmbientSpeechAudioEntity; }

	void BlockDeathJingle(bool blocked) { m_BlockDeathJingle = blocked; }
	bool CanPlayDeathJingle() const { return !m_BlockDeathJingle; }

#if !__FINAL
	void SetTimeTriggeredSpeechPlayed(u32 time);
#endif

	bool IsScriptStreamPreparing() { return m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARING; }
	bool IsScriptStreamPrepared()  { return m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARED; }
	bool IsScriptStreamPlaying()   { return m_ScriptStreamState == AUD_SCRIPT_STREAM_PLAYING; }

private:
	void InitAircraftWarningInfo();
	void InitAircraftWarningRaveSettings();

	void ResetFlagState(const audScriptAudioFlags::FlagId flagId) { SetFlagState(flagId, false); }

	void OnFlagStateChange(const audScriptAudioFlags::FlagId flagId, const bool state);

	static bool OnStopCallback(u32 userData);
	static bool HasStoppedCallback(u32 userData);

	void ProcessScriptedConversation(u32 timeInMs);

	CPed* PickRandomInterrupterInVehicle(CVehicle* vehicle);

	s32 GetLipsyncPredelay(tScriptedSpeechSlots& scriptedSlot
#if !__FINAL
		, int slotNumber										 
#endif
		);
	void AdjustOverlapForLipsyncPredelay(u32 timeInMs);
	void ShowSubtitles(bool forceShow = false);
	bool HandleCallToPreloading(u32 timeInMs, bool isPaused = false);
	void FinishScriptedConversation();
	void SetAllPedsHavingAConversation(bool isHavingAConversation);
	u32  GetPauseTimeForConversationLine(char* conversationContext);
	s32  GetOverlapOffsetTime(char* conversationContext);
	bool IsContextAnSFXCommand(char* conversationContext);
	audPrepareState PreLoadScriptedSpeech(s16 lineToStartLoadingFrom);
	bool ShouldWeAbortScriptedConversation();
	bool ShouldWePauseScriptedConversation(u32 timeInMs, bool checkForInAir=false);

	void UpdateScriptBankSlots(u32 timeInMs);
	void UpdateScriptSounds(u32 timeInMs);
	void UpdateMissionComplete();
	void UpdateCollisionInfo(u32 timeInMs);
	
	s32 FindFreeSoundId();
	audSound* PlaySoundAndPopulateScriptSoundSlot(s32 soundId, const char* soundSetName, const char* soundName, audSoundInitParams* initParams, 
													bool bOverNetwork, int nNetworkRange, naEnvironmentGroup* environmentGroup, const CEntity* pEntity, const bool ownsEnvGroup, bool enableOnReplay);
	audSound* PlaySoundAndPopulateScriptSoundSlot(s32 soundId, const u32 soundSetNameHash, const u32 soundNameHash, audSoundInitParams* initParams, 
		bool bOverNetwork, int nNetworkRange, naEnvironmentGroup* environmentGroup, const CEntity* pEntity, const bool ownsEnvGroup, bool enableOnReplay OUTPUT_ONLY(, const char* soundSetName, const char* soundName));

	bool PlaySoundAndPopulateScriptSoundSlotFromNetwork(u32 setNameHash, u32 soundNameHash, audSoundInitParams* initParams, u32 nNetworkId, CGameScriptId& scriptId, 
		naEnvironmentGroup* environmentGroup, CEntity* entity, const bool ownsEnvGroup);

	BANK_ONLY(void LogScriptBanks());

	static void MobilePreRingHandler(u32 hash, void *context);

	naEnvironmentGroup* CreateScriptEnvironmentGroup(const CEntity* entity, const Vec3V& pos) const;

	void	UpdateAircraftWarning();

#if !__FINAL
	void DebugScriptStream(s32 scriptIndex);

	u32 m_TimeTriggeredSpeechPlayed;
	u32 m_TimeSpeechTriggered;
	u32 m_TimeLastWarnedAboutNonScriptRestart;
	bool m_ShouldSetTimeTriggeredSpeechPlayedStat;
#endif

	atFixedBitSet<AUD_MAX_SCRIPT_SOUNDS> m_ScriptOwnedEnvGroup;

	bool m_ScriptedConversationInProgress;
	bool m_DuckingForScriptedConversation;
	bool m_IsScriptedConversationAMobileCall;
	bool m_IsScriptedConversationZit;
    atRangeArray<tScriptedConversationEntry, AUD_MAX_SCRIPTED_CONVERSATION_LINES> m_ScriptedConversationBuffer;
	atRangeArray<tScriptedConversationEntry, AUD_MAX_SCRIPTED_CONVERSATION_LINES> m_ScriptedConversation;
	audSpeechSound* m_ScriptedConversationSpeechSound;
	s16 m_PlayingScriptedConversationLine;
	bool m_CloneConversation;
	bool m_DisplaySubtitles;
	bool m_AddSubtitlesToBriefing;
	bool m_PauseConversation;
	bool m_PausedForRagdoll;
	bool m_PauseWaitingOnCurrentLine;
	bool m_PauseCalledFromScript;
	bool m_ShouldRestartConvAtMarker;
	bool m_AbortScriptedConversationRequested;
	bool m_HasShownSubtitles;
	u32 m_ScriptedConversationVariation;
	u32 m_ScriptedConversationNoAudioDelay;
	audSpeechAudioEntity m_SpeechAudioEntity;
	audSpeechAudioEntity m_AircraftWarningSpeechEnt;
	audSpeechAudioEntity m_AmbientSpeechAudioEntity;
	atRangeArray<RegdPed, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_Ped;
	atRangeArray<RegdPed, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_PedBuffer;
	atRangeArray<Vector3, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_NullPedSpeakerLocations;
	atRangeArray<CEntity*, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_NullPedSpeakerEntities;
	atRangeArray<char[64], AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_VoiceName;
	atRangeArray<char[64], AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_VoiceNameBuffer;
	atRangeArray<bool, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_PedIsInvolvedInCurrentConversation;
	atRangeArray<u32, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_BlockAnimTime;
	atRangeArray<u32, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_ConvPedRagDollTimer;
	atRangeArray<u32, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_ConvPedNotInRagDollTimer;
	atRangeArray<u32, AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS> m_TimeThrownFromVehicle;
	CPed* m_LastSpeakingPed;
	s32 m_LastDisplayedTextBlock;
	atMultiRangeArray<tScriptedSpeechSlots, AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD, 2> m_ScriptedSpeechSlot;
	u8 m_CurrentPreloadSet;
	s16 m_HighestLoadedLine;
	s16 m_HighestLoadingLine;
	s16 m_PreLoadingSpeechLine;
	u32 m_ScriptedConversationOverlapTriggerTime;
	u32 m_TimeTrueOverlapWasSet;
	bool m_OverlapTriggerTimeAdjustedForPredelay;
	bool m_TriggeredOverlapSpeech;
	bool m_TreatNextLineToPlayAsUrgent;
	audSound* m_ScriptedConversationSfx;
	bool m_WaitingForPreload;
	bool m_ConversationIsSfxOnly;
	bool m_ExternalStreamConversationSFXPlaying;
	u32 m_NextTimeScriptedConversationCanStart;
	u32 m_CarUpsideDownTimer;
	//For use in adjusting overlap time during pauses and interrupts
	u32 m_TimeLastFrame;
	u32 m_TimeElapsedSinceLastUpdate;
	bool m_ConversationWasInterruptedForWantedLevel;
	bool m_ConversationIsInterruptible;
	u32 m_TimePlayersCarWasLastOnTheGround;
	u32 m_TimePlayersCarWasLastInTheAir;
	u32 m_TimeOfLastInAirInterrupt;	//time swear plays and we restart conversation
	u32 m_TimeLastRegisteredCarInAir;	//time we first register that we're in the air
	u32 m_TimeOfLastChopCollisionReaction;
	u32 m_TimeOfLastCollisionCDI;

	CSubStringWithinMessage m_ArrayOfSubtitleSubStrings[AUD_MAX_SUBTITLE_SUB_STRINGS];
	u32 m_SizeOfSubtitleSubStringArray;
	u32 m_ExtraConvLines;
	char m_ConversationUserName[128];
	char m_ConversationCodeLetter;
	u8 m_ConversationCodeNumber;
	bool m_CodeNamePreLineNeedsResolving;
	Vector3 m_ConversationLocation;

	atRangeArray<audCarPedCollisionStruct, 8> m_CarPedCollisions;
	CPed* m_Interrupter;
	char m_InterruptingContext[128];
	u32 m_InterruptingVoiceNameHash;
	NOTFINAL_ONLY(u32 m_InterruptingPlaceholderVoiceNameHash;)
	audCollisionLevel m_ColLevel;
	u32 m_TimeToInterrupt;
	u32 m_TimeToRestartConversation;
	u32 m_TimeOfLastCollisionReaction;
	u32 m_TimeOfLastCollision;
	u32 m_TimeOfLastCollisionTrigger;
	u32 m_TimeToLowerCollisionLevel;
	u32 m_SpeechRestartOffset;
	u32 m_MarkerPredelayApplied;
	u32 m_InterruptSpeechRequestTime;
	s16 m_LineCheckedForRestartOffset;
	u32 m_TimeOfLastCarPedCollision;
	u32 m_TimeLastTriggeredPedCollisionScreams;
	u32 m_LastInterruptTimeDiff;
	bool m_InterruptTriggered;
	bool m_PausedAndRepeatingLine;
	bool m_CheckInterruptPreloading;
	bool m_FirstLineHasStarted;

	atRangeArray<audScriptBankSlot, AUD_MAX_SCRIPT_BANK_SLOTS> m_BankSlots;
//	audBankSlot m_MissionBankSlot;
	atRangeArray<audScriptSound, AUD_MAX_SCRIPT_SOUNDS> m_ScriptSounds;
	atRangeArray<audScript, AUD_MAX_SCRIPTS> m_Scripts;

	// Cellphone related variables
	audSound* m_CellphoneRingSound;
	u8 m_MobileRingType;
	audSound* m_MobileInterferenceSound;
	u32 m_LastMobileInterferenceTime;

	s32 m_ScriptInChargeOfAudio;
	BANK_ONLY(const char* m_CurrentScriptName;)

	audWaveSlot* m_MissionCompleteBankSlot;
	u32 m_MissionCompleteTimeOut;
	s32 m_MissionCompleteTimerId;
	// streams
	audStreamSlot *m_StreamSlot;
	audSound *m_StreamSound;
	audPlayStreamType m_StreamType;
	Vector3 m_StreamPosition;
#if GTA_REPLAY
	//track up to 2 variable set on the stream
	u32 m_StreamVariableNameHash[AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES];
	f32 m_StreamVariableValue[AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES];
#endif
	naEnvironmentGroup* m_StreamEnvironmentGroup;
	audScriptStreamState m_ScriptStreamState;
	RegdEnt m_StreamTrackerEntity;
	audMetadataRef m_LoadedStreamRef;

	audSound *m_MissionCompleteSound;
	u32 m_MissionCompleteHash,m_MissionCompleteFadeTime,m_MissionCompleteHitTime;
	audMissionCompleteState m_MissionCompleteState;
	u32 m_MissionCompleteTriggerTime;

	audSound *m_PreviewRingtoneSound;

	s32	m_HeadSetBeepOnLineIndex;
	f32	m_HeadSetBeepOffCachedVolume;

	u32 m_PoliceScannerSceneHash;
	s32 m_PoliceScannerSceneScriptIndex;

	u32 m_FramesVehicleIsInWater; 

	u32 m_TimeLastScriptedConvFinished;
	u32 m_TimeLastBeepOffPlayed;

	u32 m_TimeEnteringSlowMo;
	u32 m_TimeExitingSlowMo;
	bool m_InSlowMoForPause;
	bool m_InSlowMoForSpecialAbility;

	atRangeArray<s32, audScriptAudioFlags::kNumScriptAudioFlags> m_FlagState;

	audScene * m_ConversationDuckingScene;

	atRangeArray<audAircraftWarningStruct, AUD_AW_NUM_WARNINGS> m_AircraftWarningInfo;
	// u32 m_LastTimeOfAircraftWarningSpeech;
	bool m_ShouldUpdateAircraftWarning;

	//f32 m_PoliceScannerApplyValue;
	bool m_PoliceScannerIsApplied;

	bool m_ShouldShowMissionCompleteUI;

	bool m_AnimTriggersControllingConversation;
	bool m_AnimTriggerReported;

	bool m_IsConversationPlaceholder;

	bool m_WaitingForFirstLineToPrepareToDuck;
	bool m_TriggeredAirborneInterrupt;

	bool m_BlockDeathJingle;
	bool m_ScriptedSpeechTimerPaused;
	bool m_ScriptSetNoDucking;

	bool m_DisableReplayScriptStreamRecording;

	static audHashActionTable sm_ScriptActionTable;
#if __BANK
	u32		    m_TimeLoadingScriptBank;
	FileHandle	m_FileHandle;
	FileHandle m_DialogueFileHandle;
	char m_DialogueFileName[256];
	bool m_HasInitializedDialogueFile;

	static bool sm_DrawScriptTriggeredSounds;
	static bool sm_InWorldDrawScriptTriggeredSounds;
	static bool sm_DrawScriptStream;
	static bool sm_LogScriptTriggeredSounds;
	static bool sm_DrawScriptBanksLoaded;
	static bool sm_FilterScriptTriggeredSounds;
	static char sm_ScriptTriggerOverride[64];
	static char sm_ScriptedLineDebugInfo[128];
	static bool sm_DrawReplayScriptSpeechSlots;
#endif 
};

extern audScriptAudioEntity g_ScriptAudioEntity;

#endif // AUD_SCRIPTAUDIOENTITY_H




