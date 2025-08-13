// 
// audio/speechmanager.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_SPEECHMANAGER_H
#define AUD_SPEECHMANAGER_H

#include "audio/speechaudioentity.h"
#include "audioengine/entity.h"
#include "gameobjects.h"
#include "script/script.h"
#include "control/replay/replay.h"

//class audSpeechSound;
#include "audiosoundtypes/speechsound.h"

#define AUD_DEFERRED_SPEECH_DEBUG_PRINT (__DEV && 1)

#define AUD_MAX_PHONE_CONVERSATIONS_PER_VOICE (6)

const u8 g_MaxAmbientSpeechSlots = 6;
const u8 g_MaxScriptedSpeechSlots = 8;
const u32 g_PhraseMemorySize = 70;
const u32 g_MaxVariationNameLength = 128;
const u32 g_PhoneConvMemorySize = 100;

enum
{
	AUD_SPEECH_SLOT_VACANT,
	AUD_SPEECH_SLOT_ALLOCATED,
	AUD_SPEECH_SLOT_WALLA,
	AUD_SPEECH_SLOT_STOPPING
};

enum
{
	AUD_PAIN_SLOT_LOADING,
	AUD_PAIN_SLOT_READY,
	AUD_PAIN_SLOT_IN_USE,
	AUD_PAIN_SLOT_USED_UP,
	AUD_PAIN_SLOT_FINISHING
};

enum
{
	AUD_PAIN_VOICE_TYPE_PLAYER,
	AUD_PAIN_VOICE_TYPE_MALE,
	AUD_PAIN_VOICE_TYPE_FEMALE,
	AUD_PAIN_VOICE_TYPE_SPECIFIC,
	AUD_PAIN_VOICE_TYPE_BREATHING,
	AUD_PAIN_VOICE_NUM_TYPES
};

enum
{
	AUD_PAIN_LEVEL_WEAK,
	AUD_PAIN_LEVEL_NORMAL,
	AUD_PAIN_LEVEL_TOUGH,
	AUD_PAIN_LEVEL_MIXED,
	AUD_PAIN_NUM_LEVELS
};

enum{
	AUD_BREATHING_TYPE_NONE,
	AUD_BREATHING_TYPE_MICHAEL,
	AUD_BREATHING_TYPE_FRANKLIN,
	AUD_BREATHING_TYPE_TREVOR,
	AUD_BREATHING_TYPE_MP_MALE,
	AUD_BREATHING_TYPE_MP_FEMALE,
	AUD_BREATHING_TYPE_TOTAL
};

//change back to 2 if we bring back more female types than just normal
#define PAIN_DEBUG_NUM_GENDERS (1)

enum
{
	//bank-loaded
	AUD_PAIN_ID_PAIN_LOW_WEAK,
	AUD_PAIN_ID_PAIN_LOW_GENERIC,
	AUD_PAIN_ID_PAIN_LOW_TOUGH,
	AUD_PAIN_ID_PAIN_MEDIUM_WEAK,
	AUD_PAIN_ID_PAIN_MEDIUM_GENERIC,
	AUD_PAIN_ID_PAIN_MEDIUM_TOUGH,
	AUD_PAIN_ID_DEATH_LOW_WEAK,
	AUD_PAIN_ID_DEATH_LOW_GENERIC,
	AUD_PAIN_ID_DEATH_LOW_TOUGH,
	AUD_PAIN_ID_HIGH_FALL_DEATH,
	AUD_PAIN_ID_CYCLING_EXHALE,
	AUD_PAIN_ID_EXHALE,
	AUD_PAIN_ID_INHALE,
	AUD_PAIN_ID_SHOVE,
	AUD_PAIN_ID_WHEEZE,
	AUD_PAIN_ID_DEATH_HIGH_SHORT,
	AUD_PAIN_ID_PAIN_HIGH_WEAK,
	AUD_PAIN_ID_PAIN_HIGH_GENERIC,
	AUD_PAIN_ID_PAIN_HIGH_TOUGH,
	AUD_PAIN_ID_SCREAM_SHOCKED_WEAK,
	AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC,
	AUD_PAIN_ID_SCREAM_SHOCKED_TOUGH,
	AUD_PAIN_ID_DEATH_GARGLE,
	AUD_PAIN_ID_CLIMB_LARGE,
	AUD_PAIN_ID_CLIMB_SMALL,
	AUD_PAIN_ID_JUMP,
	AUD_PAIN_ID_MELEE_SMALL_GRUNT,
	AUD_PAIN_ID_MELEE_LARGE_GRUNT,
	AUD_PAIN_ID_SNEEZE,


	AUD_PAIN_ID_NUM_BANK_LOADED,
	//wave-loaded
	AUD_PAIN_ID_PAIN_GARGLE,
	AUD_PAIN_ID_DEATH_HIGH_MEDIUM,
	AUD_PAIN_ID_DEATH_HIGH_LONG,
	AUD_PAIN_ID_COUGH,
	AUD_PAIN_ID_ON_FIRE,
	AUD_PAIN_ID_HIGH_FALL,
	AUD_PAIN_ID_SUPER_HIGH_FALL,
	AUD_PAIN_ID_SCREAM_PANIC,
	AUD_PAIN_ID_SCREAM_PANIC_SHORT,
	AUD_PAIN_ID_SCREAM_SCARED,
	AUD_PAIN_ID_SCREAM_TERROR_WEAK,
	AUD_PAIN_ID_SCREAM_TERROR_GENERIC,
	AUD_PAIN_ID_SCREAM_TERROR_TOUGH,
	AUD_PAIN_ID_TAZER,
	AUD_PAIN_ID_DEATH_UNDERWATER,
	AUD_PAIN_ID_COWER,
	AUD_PAIN_ID_WHIMPER,
	AUD_PAIN_ID_DYING_MOAN,
	AUD_PAIN_ID_PAIN_RAPIDS,


	AUD_PAIN_ID_NUM_TOTAL
};

enum
{
	AUD_PAIN_COUGH,
	AUD_HIGH_FALL,
	AUD_SUPER_HIGH_FALL,
	AUD_HIGH_FALL_DEATH,
	AUD_SCREAM_PANIC,
	AUD_SCREAM_PANIC_SHORT,
	AUD_SCREAM_SCARED,
	AUD_SCREAM_SHOCKED,
	AUD_SCREAM_TERROR,
	AUD_PAIN_INHALE,
	AUD_PAIN_EXHALE,
	AUD_PAIN_SHOVE,
	AUD_PAIN_WHEEZE,
	AUD_PAIN_NONE,
	AUD_PAIN_LOW,
	AUD_PAIN_MEDIUM,
	AUD_PAIN_HIGH,
	AUD_ON_FIRE,
	AUD_DEATH_LOW,
	AUD_DEATH_HIGH,
	AUD_PAIN_TAZER,
	AUD_DEATH_UNDERWATER,
	AUD_COWER,
	AUD_WHIMPER,
	AUD_CLIMB_LARGE,
	AUD_CLIMB_SMALL,
	AUD_JUMP,
	AUD_MELEE_SMALL_GRUNT,
	AUD_MELEE_LARGE_GRUNT,
	AUD_DYING_MOAN,
	AUD_CYCLING_EXHALE,
	AUD_PAIN_RAPIDS,
	AUD_PAIN_SNEEZE,
	AUD_NUM_PAIN_TYPES
};

class audSpeechAudioEntity;
namespace rage {class audWaveSlot;}
struct tVoice;

struct audSpeechSlot
{
public:
	audSpeechSlot() :
		PlayState(AUD_SPEECH_SLOT_VACANT),
		SpeechAudioEntity(NULL),
		WaveSlot(NULL),
		Voicename(0),
		VariationNumber(-1), 
		Priority(0.0f),
		WallaPlayCount(0),
		TimeCanPlayWalla(0),
		UseFirstHalfForWalla(true)
	{
#if __BANK
		Context[0] = '\0';
#endif
	};
	u8 PlayState;
	audSpeechAudioEntity* SpeechAudioEntity;
	audWaveSlot* WaveSlot;
	u32 Voicename;
#if __BANK
	char Context[64];
#endif
	s32 VariationNumber;
	f32 Priority;
	u32 WallaPlayCount;
	u32 TimeCanPlayWalla;
	bool UseFirstHalfForWalla;
};

struct audPainSlot
{
public:
	audPainSlot() :
	  SlotState(AUD_PAIN_SLOT_USED_UP),
	  PainLevel(AUD_PAIN_LEVEL_NORMAL),
	  WaveSlot(NULL),
	  BankId(0),
	  VoiceNameHash(0)
	{
#if __BANK
		VoiceName[0] = '\0';
#endif
	};
	u8 SlotState;
	u8 PainLevel;
	audWaveSlot* WaveSlot;
	u16 BankId;
	u32 VoiceNameHash;
#if __BANK
	char VoiceName[64];
#endif
};

struct audAnimalContextInfo
{
	audAnimalContextInfo() :
		ContextPHash(0),
		NumVars(0),
		LastVarUsed(0),
		NextTimeCanPlay(0),
		MinTimeBetween(-1),
		MaxTimeBetween(-1),
		UsedUp(0),
		ForcesSwap(false)
	{
	};

	u32 ContextPHash;
	u32 NumVars;
	u32 LastVarUsed;
	u32 NextTimeCanPlay;
	s32 MinTimeBetween;
	s32 MaxTimeBetween;
	s8 UsedUp;
	bool ForcesSwap;
};

struct audAnimalBankSlot
{
	audAnimalBankSlot() :
		WaveSlot(NULL),
		BankdID(~0U),
		VoiceName(0),
		Owner(kAnimalNone),
		IsLoading(false),
		FailedLastTime(false)
	{
	};

	atArray<audAnimalContextInfo> ContextInfo;
	audWaveSlot* WaveSlot;
	u32 BankdID;
	u32 VoiceName;
	AnimalType Owner;
	bool IsLoading;
	bool FailedLastTime;
};

struct audAnimalInfo
{
	audAnimalInfo() :
		Data(NULL),
		AnimalType(kAnimalNone),
		Priority(0.0f),
		SlotNumber(-1)
	{
	};

	AnimalParams* Data;
	AnimalType AnimalType;
	f32 Priority;
	s8 SlotNumber;
};

struct audAnimalQSortStruct
{
	audAnimalQSortStruct() :
		AnimalType(kAnimalNone),
		Priority(0.0f)
	{
	};

	AnimalType AnimalType;
	f32 Priority;
};


class audSpeechManager : public audEntity // it might not actually need to be an audEntity - depends if it actually triggers sounds itself.
{
public:
	AUDIO_ENTITY_NAME(audSpeechManager);

#if __BANK
	static void AddWidgets(bkBank &bank);
#endif

#if GTA_REPLAY
	friend void CReplayMgr::ClearSpeechSlots();
	void FreeAllAmbientSpeechSlots();
#endif // GTA_REPLAY

	void Init();
	void Shutdown();
	void Update();
	void UpdateAnimalBanks();

	bool IsStreamingBusyForContext(u32 contextPHash);

	void ReserveAmbientSpeechSlotForScript();
	void ReleaseAmbientSpeechSlotForScript();
	s32 GetAmbientSpeechSlot(audSpeechAudioEntity* requestingEntity, bool* hadToStopSpeech=NULL, f32 priority=3.0, scrThreadId scriptThreadId = THREAD_INVALID
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
		, const char* context = NULL
#endif
		, bool isHeli = false);
	f32 GetRealAmbientSpeechPriority(audSpeechAudioEntity* requestingEntity, f32 priority);
	void MarkSlotAsAllocated(int slotIndex, audSpeechAudioEntity* requestingEntity, f32 priority);
	void FreeAmbientSpeechSlot(s32 slot, bool okForEntityToBeNull = false);
	void PopulateAmbientSpeechSlotWithPlayingSpeechInfo(s32 slot, u32 voiceName,
#if __BANK
		const char* context, 
#endif
		s32 variation);
	bool IsSlotVacant(s32 slotIndex);
	bool IsSafeToReuseSlot(s32 slotIndex, const audSpeechAudioEntity* entity);
	bool IsScriptSpeechSlotVacant(s32 slotIndex);
	s32 GetScriptSpeechSlotRefCount(s32 slotIndex);
	s32 GetScriptedSpeechSlot(audSpeechAudioEntity* requestingEntity);
	void FreeAllScriptedSpeechSlots();
	void FreeScriptedSpeechSlot(s32 slot);
	u32 GetNumVacantAmbientSlots();

	s32 WhenWasVariationLastPlayed(u32 voiceHash, u32 contextHash, u32 variationNum, u32& timeInMs);
	void BuildMemoryListForVoiceAndContext(u32 voiceHash, u32 contextHash, u32& memorySize, u32 aPhraseMemory[g_PhraseMemorySize][4]);
	void AddVariationToHistory(u32 voiceHash, u32 contextHash, u32 variationNum, u32 timeInMs);

	void GetTimeWhenContextCanNextPlay(SpeechContext *speechContext, u32* timeCanNextPlay, bool* isControlledByContext);
	void GetTimeWhenTriggeredPrimaryContextCanNextPlay(TriggeredSpeechContext *speechContext, u32* timeCanNextPlay, bool* isControlledByContext);
	void GetTimeWhenTriggeredBackupContextCanNextPlay(TriggeredSpeechContext *speechContext, u32* timeCanNextPlay, bool* isControlledByContext);
	void UpdateTimeContextWasLastUsed(const char *contextName, CPed* speaker, u32 timeInMs);
	void UpdateTimeContextWasLastUsed(SpeechContext *speechContext, u32 timeInMs);
	u32 GetRepeatTimeContextCanPlayOnVoice(SpeechContext *speechContext);
	u32 GetRepeatTimePrimaryTriggeredContextCanPlayOnVoice(TriggeredSpeechContext *speechContext);
	u32 GetRepeatTimeBackupTriggeredContextCanPlayOnVoice(TriggeredSpeechContext *speechContext);

	u32 GetTimeLastPlayedForContext(u32 contextPHash);

	void UpdateTimeTriggeredContextWasLastUsed(TriggeredSpeechContext* speechContext, u32 timeInMs);
	void UpdateTimePrimaryTriggeredContextWasLastUsed(TriggeredSpeechContext* speechContext, u32 timeInMs);
	void UpdateTimeBackupTriggeredContextWasLastUsed(TriggeredSpeechContext* speechContext, u32 timeInMs);

	u32 GetVoiceForPhoneConversation(u32 pedVoiceGroup);
	u8 GetPhoneConversationVariationFromVoiceHash(u32 voiceHash);
	void AddPhoneConversationVariationToHistory(u32 voiceHash, u32 variation, u32 timeInMs);
	s32 WhenWasPhoneConversationLastPlayed(u32 voiceHash, u32 variation, u32& timeInMs);

	SpeechContext *GetSpeechContext(const char* contextName, CPed* speaker, bool* returningDefault = NULL);
	SpeechContext *GetSpeechContext(const char* contextName, CPed* speaker, CPed* replyingPed = NULL, bool* returningDefault = NULL);
	SpeechContext *GetSpeechContext(u32 contextHash, CPed* speaker, bool* returningDefault = NULL);
	SpeechContext *GetSpeechContext(u32 contextHash, CPed* speaker, CPed* replyingPed=NULL, bool* returningDefault = NULL);
	SpeechContext* ResolveVirtualSpeechContext(const SpeechContext* speechContext, CPed* speaker, CPed* replyingPed = NULL);

	// Temporary hack to get the 'loudness' of a context - this'll live in the speech game object very soon
	//s32 GetVolumeForContext(const char* context);
	// New, proper version, that goes to the game object
	//s32 GetVolumeForContext(const s32 contextIndex);

	bool DoesContextHaveANoGenderVersion(SpeechContext *speechContext);

	audWaveSlot* GetAmbientWaveSlotFromId(s32 slotId);
	audWaveSlot* GetScriptedWaveSlotFromId(s32 slotId);

	void SpeechAudioEntityBeingDeleted(audSpeechAudioEntity* audioEntity);

	bool IsPainLoaded(u32 painVoiceType);
	const char* GetPainLevelString(u8 toughness, bool shouldLowerToughness, bool& isSpecificPain) const;
	audWaveSlot* GetWaveSlotForPain(u32 painVoiceType);
	u8 GetBankNumForPain(u32 painVoiceType);
	void PlayedPain(u32 painVoiceType, u32 painType);
	u32 GetVariationForPain(u32 painVoiceType, u32 painType);
	u32 GetWaveLoadedPainVoiceFromVoiceType(u32 painVoiceType, const CPed* ped);
	
#if GTA_REPLAY
	void PlayedPainForReplay(u32 painVoiceType, u32 painType, u8 numTimes, u8 nextVariation, u32 nextTimeToLoad, bool isJumpingOrRwnd, bool isGoingForwards);
	void GetPlayedPainForReplayVars(u32 painVoiceType, u8* numTimes, u8* nextVariation, u32& nextTimeToLoad, u8& currentPainBank) const;
	void SetPlayedPainForReplayVars(u32 painVoiceType, const u8* numTimes, const u8* nextVariation, const u32& nextTimeToLoad, const u8& currentPainBank);
	void SetBankNumForPain(u32 painVoiceType, u8 bank);
	void SetVariationForPain(u32 painVoiceType, u32 painType, u8 variation);
	u8 GetNumTimesPlayed(u32 painVoiceType, u32 painType) const;
	u32 GetNextTimeToLoad(u32 painVoiceType) const;
#endif // GTA_REPLAY

	void PlayedBreath(PlayerVocalEffectType vfxType);
	const char* GetBreathingInfo(PlayerVocalEffectType eEffectType, u32& voiceNameHash, u8& variation);

	u32 GetPrimaryVoice(u32 pedVoiceGroup);
	u32 GetGangVoice(u32 pedVoiceGroup);
	u32 GetBestVoice(u32 pedVoiceGroup, u32 contextPHash, CPed* speaker, bool allowBackupPVG=true, s32* overallMinReferenceArg=NULL);
	void AddReferenceToVoice(u32 pedVoiceGroup, u32 voiceNameHash BANK_ONLY(, audSpeechAudioEntity* speechEnt = NULL));
	void RemoveReferenceToVoice(u32 pedVoiceGroup, u32 voiceNameHash);

	u32 GetBestPrimaryVoice(PedVoiceGroups* pedVoiceGroupObject, s32& minReferences, u32 contextPartialHash=0);
	u32 GetBestMiniVoice(PedVoiceGroups* pedVoiceGroupObject, s32& minReferences, u32 contextPartialHash);

	u16 GetPossibleConversationTopicsForPvg(u32 pvg);

	void SetPlayerPainRootBankName(const CPed& newPlayer);
	void SetPlayerPainRootBankName(const char* bankName);

	s32 GetVariationForNearAnimalContext(AnimalType animalType, const char* context);
	s32 GetVariationForNearAnimalContext(AnimalType animalType, u32 contextPHash);
	void NearAnimalContextPlayed(AnimalType animalType, u32 contextPHash);
	void FarAnimalContextPlayed(u32 contextPHash, u32 timeInMs);
	bool CanFarAnimalContextPlayed(u32 contextPHash, u32 timeInMs);

	void GetWaveSlotAndVoiceNameForAnimal(AnimalType animalType, audWaveSlot*& waveSlot, u32& voiceName);
	void IncrementAnimalAppearance(AnimalType animalType) {m_NumAnimalAppearancesSinceLastUpdate[animalType]++;}
	void SetIfNearestAnimalInstance(AnimalType animalType, Vec3V_In pos);
	void IncrementAnimalsMakingSound(AnimalType animalType);
	void DecrementAnimalsMakingSound(AnimalType animalType);
	u8 GetNumAnimalsMakingSound(AnimalType animalType);
	void ForceAnimalBankUpdate() {m_ForceAnimalBankUpdate = true;}
	bool GetIsCurrentlyLoadingAnimalBank();

	void RegisterPedAimedAtOrHurt(u8 toughness, bool shouldLowerToughness, bool hurt);


	void RequestTennisBanks(CPed* opponent BANK_ONLY(, bool debugingTennis=false));
	void UnrequestTennisBanks();
	audWaveSlot* GetTennisVocalizationInfo(CPed* m_Parent, const TennisVocalEffectType& tennisVFXType, u32& contextPhash, u32& voiceHash, u32& variation);
	bool GetIsTennisUsingAnimalBanks() { return m_TennisIsUsingAnimalBanks; }
	s32 GetTennisScriptId() { return m_TennisScriptIndex; }
/*
#if __DEV
	void GetMaxReferencesForPVG(u32 pvg, s32& full, s32& gang, s32& mini, s32& miniDriving);
#endif
	*/
#if GTA_REPLAY
	void SetReplayPainBanks(u8 prevBank, u8 nextBank, bool backwards)
	{
		if(m_ReplayNeedToReconcile == false)
		{	// Replay hasn't started modifying vars yet so copy across what
			// they all should be
			for(int pvt = 0; pvt < AUD_PAIN_VOICE_NUM_TYPES; ++pvt)
			{
				for(int pt = 0; pt < AUD_PAIN_ID_NUM_BANK_LOADED; ++pt)
				{
					m_ReplayNumTimesPainPlayed[pvt][pt]		= m_NumTimesPainPlayed[pvt][pt];
					m_ReplayNextVariationForPain[pvt][pt]	= m_NextVariationForPain[pvt][pt];
				}
				m_ReplayNextTimeToLoadPain[pvt]	= m_NextTimeToLoadPain[pvt];
			}
		}

		m_ReplayPrevPainBank			= prevBank;
		m_ReplayNextPainBank			= nextBank;
		m_ReplayNeedToReconcile			= true;

		m_ReplayWaveBankChangeWasLast	= true;
		m_ReplayWasGoingForwards		= !backwards;
	}

	void ReconcilePainBank()
	{
		// 
		if(m_ReplayNeedToReconcile)
		{
			u8 nextPainBank = m_ReplayNextPainBank;

			// Special case for going backwards...
			// If a bank change was the most recent action then go to the next
			// bank
			if(m_ReplayWaveBankChangeWasLast && !m_ReplayWasGoingForwards)
			{
				nextPainBank = (nextPainBank % 2) + 1;
			}

			// See if we're going to need to force a load of the next bank...
			if(m_CurrentPainBank[AUD_PAIN_VOICE_TYPE_PLAYER] != nextPainBank)
			{
				m_NeedToLoad[AUD_PAIN_VOICE_TYPE_PLAYER] = true;
			}

			m_CurrentPainBank[AUD_PAIN_VOICE_TYPE_PLAYER] = nextPainBank;

			// If we need to load the next bank then go backwards one as the loading will increment for us
			if(m_NeedToLoad[AUD_PAIN_VOICE_TYPE_PLAYER] == true)
			{
				m_CurrentPainBank[AUD_PAIN_VOICE_TYPE_PLAYER] = (m_CurrentPainBank[AUD_PAIN_VOICE_TYPE_PLAYER] % 2) + 1;
			}

			// Loop through and set all the managers variables from what we've stored from replay
			bool skipBankAhead = false;
			for(int pvt = 0; pvt < AUD_PAIN_VOICE_NUM_TYPES; ++pvt)
			{
				for(int pt = 0; pt < AUD_PAIN_ID_NUM_BANK_LOADED; ++pt)
				{
					m_NumTimesPainPlayed[pvt][pt]	= m_ReplayNumTimesPainPlayed[pvt][pt];
					m_NextVariationForPain[pvt][pt]	= m_ReplayNextVariationForPain[pvt][pt];
					// Is this bank going to be out of pains to play?
					if(m_NumTimesPainPlayed[pvt][pt] == m_NumPainInCurrentBank[pvt][pt])
						skipBankAhead = true;
				}
				m_NextTimeToLoadPain[pvt]	= m_ReplayNextTimeToLoadPain[pvt];
			}

			if(skipBankAhead && m_ReplayWasGoingForwards && !m_ReplayWaveBankChangeWasLast)
			{
				m_CurrentPainBank[AUD_PAIN_VOICE_TYPE_PLAYER] = (m_CurrentPainBank[AUD_PAIN_VOICE_TYPE_PLAYER] % 2) + 1;
			}
		}

		// Reset vars
		m_ReplayNeedToReconcile			= false;
		m_ReplayWaveBankChangeWasLast	= false;
		m_ReplayWasGoingForwards		= false;
	}
#endif // GTA_REPLAY

#if __BANK
	void ResetSpecificPainDrawStats();
#endif

private:
	void UpdatePainSlots();
	void SelectSpecificPainBank(u32 currentTime);
	void ResetSpecificPainBankTrackingVars();

	void UpdateBreathingBanks();

	const char* GetRandomContextForChurn();

	void UpdateGlobalSpeechMetrics();

	void LoadAnimalBank(AnimalType animalId, s8 slotNum=-1);
	void SortAnimalList(audAnimalQSortStruct* outList);

	bool IsAMiniDrivingContext(char* context);
	bool IsAMiniNonDrivingContext(char* context);
	bool IsAGangContext(char* context);
	u8 GetMiniVoiceType(const u32 voiceNameHash);

	u32 GetBestGangVoice(PedVoiceGroups* pedVoiceGroupObject, s32& minReferences, u32 contextPartialHash);

	tVoice* GetPrimaryVoices(u32 pedVoiceGroup, u8& numPrimaryVoices);
	tVoice* GetMiniVoices(u32 pedVoiceGroup, u8& numMiniVoices);
	tVoice* GetGangVoices(u32 pedVoiceGroup, u8& numMiniVoices);
	tVoice* GetGangVoices(PedVoiceGroups* pedVoiceGroupObject, u8& numMiniVoices);

	void UpdateTennisBanks();
	void LoadTennisBanks();

#if __BANK
	void DrawSpeechSlots();
	void DrawPainPlayedTotals();
	void DrawSpecificPainInfo();
	void DrawAnimalBanks();
#endif

	audSpeechSlot m_AmbientSpeechSlots[g_MaxAmbientSpeechSlots];
	u8 m_AmbientSpeechSlotToNextSearchFrom;
	audSpeechSlot m_ScriptedSpeechSlots[g_MaxScriptedSpeechSlots];
	//audSpeechSlot m_PlayerSpeechSlot;
	audSpeechSound* m_ChurnSpeechSound;
	s32 m_ChurnSlot;
	scrThreadId m_ScriptOwningReservedSlot;
	u8 m_ScriptReservedSlotNum;


	atRangeArray<audPainSlot, AUD_PAIN_VOICE_NUM_TYPES+1>  m_PainSlots;
	atRangeArray<u8, AUD_PAIN_VOICE_NUM_TYPES> m_CurrentPainBank;
	atRangeArray<u32, AUD_PAIN_VOICE_NUM_TYPES> m_NextTimeToLoadPain;
	atMultiRangeArray<u8, AUD_PAIN_VOICE_NUM_TYPES, AUD_PAIN_ID_NUM_BANK_LOADED> m_NextVariationForPain;
	atMultiRangeArray<u8, AUD_PAIN_VOICE_NUM_TYPES, AUD_PAIN_ID_NUM_BANK_LOADED> m_NumTimesPainPlayed;
	atMultiRangeArray<u8, AUD_PAIN_VOICE_NUM_TYPES, AUD_PAIN_ID_NUM_BANK_LOADED> m_NumPainInCurrentBank;
	atRangeArray<u32, AUD_PAIN_VOICE_NUM_TYPES> m_TimeLastLoaded;
	u32 m_TimeSpecificBankLastLoaded;
	s8 m_PainTypeLoading;
	s8 m_SpecificPainLevelToLoad;
	s8 m_SpecificPainLevelLoaded;
	bool m_SpecificPainToLoadIsMale;
	bool m_SpecificPainLoadingIsMale;
	bool m_SpecificPainLoadedIsMale;
	atRangeArray<bool, AUD_PAIN_VOICE_NUM_TYPES> m_NeedToLoad;

#if GTA_REPLAY
	u8 m_ReplayNextPainBank;
	bool m_ReplayShouldSkipBankOnJump;
	bool m_ReplayNeedToReconcile;
	u8 m_ReplayPrevPainBank;
	atMultiRangeArray<u8, AUD_PAIN_VOICE_NUM_TYPES, AUD_PAIN_ID_NUM_BANK_LOADED> m_ReplayNumTimesPainPlayed;
	atMultiRangeArray<u8, AUD_PAIN_VOICE_NUM_TYPES, AUD_PAIN_ID_NUM_BANK_LOADED> m_ReplayNextVariationForPain;
	atRangeArray<u32, AUD_PAIN_VOICE_NUM_TYPES>	m_ReplayNextTimeToLoadPain;
	bool m_ReplayBankVarsForJumping;
	bool m_ReplayWaveBankChangeWasLast;
	bool m_ReplayWasGoingForwards;
#endif // GTA_REPLAY

	u32 m_LastTimeSpecificPainBankSelected;
	//3 for WEAK, NORMAL, TOUGH,  2 for MALE, FEMALE
	atMultiRangeArray<u32, 3, 2> m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected;

	// 0 is voice, 1 is contexthash, 2 is the variation numm 3 is time
	atMultiRangeArray<u32, g_PhraseMemorySize, 4> m_PhraseMemory;
	u32 m_NextPhraseMemoryWriteIndex;

	// 0 is voice, 1 is last variation used, 2 is time the variation last played
	atMultiRangeArray<u32, g_PhoneConvMemorySize,3> m_PhoneConversationMemory;
	u32 m_NextPhoneConvMemoryWriteIndex;

	atRangeArray<u8, AUD_NUM_PLAYER_VFX_TYPES> m_BreathVariationsInCurrentBank;
	atRangeArray<u8, AUD_NUM_PLAYER_VFX_TYPES> m_NextVariationForBreath;
	u8 m_BreathingTypeCurrentlyLoaded;
	u8 m_BreathingTypeToLoad;

	atArray<audAnimalBankSlot> m_AnimalBankSlots;
	atRangeArray<audAnimalInfo, ANIMALTYPE_MAX> m_AnimalInfoArray;
	atRangeArray<u32, ANIMALTYPE_MAX> m_NumAnimalAppearancesSinceLastUpdate;
	atRangeArray<f32, ANIMALTYPE_MAX> m_NearestAnimalAppearancesSinceLastUpdate;
	atRangeArray<u32, ANIMALTYPE_MAX> m_NextAnimalBankNumToLoad;
	atRangeArray<u32, ANIMALTYPE_MAX> m_NumAnimalBanks;
	audAnimalQSortStruct m_SortedAnimalList[ANIMALTYPE_MAX];
	atRangeArray<u8, ANIMALTYPE_MAX> m_NumAnimalsMakingSound;
	u32 m_TimeOfLastFullAnimalBankUpdate;
	u16 m_MaxNumAnimalSlots;
	bool m_ForceAnimalBankUpdate;
	bool m_LoadingAfterAnimalUpdate;

	atMultiRangeArray<u32, 2, AUD_NUM_PLAYER_VFX_TYPES> m_NumTennisContextPHashes;
	atMultiRangeArray<u32, 2, AUD_NUM_PLAYER_VFX_TYPES> m_NumTennisVariations;
	atMultiRangeArray<u32, 2, AUD_NUM_PLAYER_VFX_TYPES> m_LastTennisVariations;
	RegdPed m_TennisOpponent;
	s32 m_TennisScriptIndex;
	s8 m_CurrentTennisSlotLoading;
	bool m_TennisIsUsingAnimalBanks;
	bool m_TennisOpponentIsMale;
	bool m_TennisPlayerIsMichael;
	bool m_TennisPlayerIsFranklin;
	bool m_TennisPlayerIsTrevor;

#if __BANK
	u32 m_DebugSpecPainCountTotal; 
	u32 m_DebugNonSpecPainCountTotal;
	atMultiRangeArray<u32,AUD_PAIN_LEVEL_MIXED,PAIN_DEBUG_NUM_GENDERS> m_DebugSpecPainTimeLastLoaded; 
	atMultiRangeArray<u32,AUD_PAIN_LEVEL_MIXED,PAIN_DEBUG_NUM_GENDERS> m_DebugNonSpecPainCount; 
	u32 m_DebugSpecPainBankLoadCountTotal; 
	u32 m_DebugNonSpecPainBankLoadCountTotal; 
	atMultiRangeArray<u32,AUD_PAIN_LEVEL_MIXED,PAIN_DEBUG_NUM_GENDERS> m_DebugSpecPainBankLoadCount; 
#endif

};

extern audSpeechManager g_SpeechManager;

#endif // AUD_SPEECHMANAGER_H
