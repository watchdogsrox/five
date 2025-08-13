// 
// audio/policescanner.h
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 

#ifndef SCANNERMANAGER_H_INCLUDED
#define SCANNERMANAGER_H_INCLUDED

#include "audiodefines.h"

#if NA_POLICESCANNER_ENABLED

#include "gameobjects.h"
#include "game/crime.h"

#include "audioentity.h"

#include "atl/array.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/speechsound.h"
#include "audiohardware/waveslot.h"
#include "bank/bkmgr.h"
#include "renderer/color.h"

class CPed;
class CEntity;
class audVehicleAudioEntity;
class naEnvironmentGroup;
class audScene;

#define AUD_NUM_CDI_VOICES (22)

const u32 g_MaxPhrasesPerSentence = 16;
const u32 g_MaxBufferedSentences = 32;

enum audScannerState
{
	AUD_SCANNER_IDLE,
	AUD_SCANNER_PREPARING,
	AUD_SCANNER_PLAYBACK,
	AUD_SCANNER_POSTDELAY,
	AUD_SCANNER_HANDLING_COP_DISPATCH_INTERACTION,
};

enum audScannerSentenceType
{
	AUD_SCANNER_CRIME_REPORT,
	AUD_SCANNER_ASSISTANCE_REQUIRED,
	AUD_SCANNER_SUSPECT_SPOTTED,
	AUD_SCANNER_SINGLE_DISPATCH,
	AUD_SCANNER_MULTI_DISPATCH,
	AUD_SCANNER_SUSPECT_DOWN,
	AUD_SCANNER_RANDOM_CHAT,
	AUD_SCANNER_SCRIPTED,
	AUD_SCANNER_TRAIN,
	AUD_SCANNER_UNKNOWN,
};

enum audUnitType
{
	AUD_UNIT_NORMAL,
	AUD_UNIT_SWAT,
	AUD_UNIT_AIR,
	AUD_UNIT_FBI,
};

enum audDirection
{
	AUD_DIR_NORTH,
	AUD_DIR_EAST,
	AUD_DIR_SOUTH,
	AUD_DIR_WEST,
	AUD_DIR_UNKNOWN,
};

enum audScannerPriority
{
	AUD_SCANNER_PRIO_AMBIENT,
	AUD_SCANNER_PRIO_LOW,
	AUD_SCANNER_PRIO_NORMAL,
	AUD_SCANNER_PRIO_HIGH,
	AUD_SCANNER_PRIO_CRITICAL,
};

enum ScannerSlots
{
	SCANNER_SLOT_NULL = -1,
	SCANNER_SLOT_1 = 0,
	SCANNER_SLOT_2,
	SCANNER_SLOT_3,
	SCANNER_SLOT_4,
	SCANNER_SMALL_SLOT_1,
	SCANNER_SMALL_SLOT_2,
	SCANNER_SMALL_SLOT_3,
	SCANNER_SMALL_SLOT_4,
	NUM_SCANNERSLOTS,
	SCANNERSLOTS_MAX = NUM_SCANNERSLOTS,
};

//enum ScannerCopDispatchInteractionClass
//{
//	SCANNER_CDIC_NONE,
//	SCANNER_CDIC_NORMAL,
//	SCANNER_CDIC_COP_RESPONSE,
//	SCANNER_CDIC_POSSIBLE_HELI_RESPONSE,
//	SCANNER_CDIC_HELI,
//
//	SCANNER_CDIC_TOT_NUM
//};

enum ScannerCopDispatchInteractionType
{
	SCANNER_CDIT_NONE,
	SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE,
	SCANNER_CDIT_REPORT_SUSPECT_ENTERED_FREEWAY,
	SCANNER_CDIT_REPORT_SUSPECT_ENTERED_METRO,
	SCANNER_CDIT_REPORT_SUSPECT_IS_IN_CAR,
	SCANNER_CDIT_REPORT_SUSPECT_IS_ON_FOOT,
	SCANNER_CDIT_REPORT_SUSPECT_IS_ON_MOTORCYCLE,
	SCANNER_CDIT_REPORT_SUSPECT_LEFT_FREEWAY,
	SCANNER_CDIT_REQUEST_BACKUP,
	SCANNER_CDIT_ACKNOWLEDGE_SITUATION_DISPATCH,
	SCANNER_CDIT_UNIT_RESPONDING_DISPATCH,
	SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH,
	SCANNER_CDIT_HELI_MAYDAY_DISPATCH,
	SCANNER_CDIT_SHOT_AT_HELI_MEGAPHONE,
	SCANNER_CDIT_HELI_APPROACHING_DISPATCH,
	SCANNER_CDIT_PLAYER_SPOTTED,

	SCANNER_CDIT_TOT_NUM
};

struct audScannerPhrase
{
	audMetadataRef soundMetaRef;
	audSound *sound;
	audSound *echoSound;
	u32 postDelay;
	ScannerSlots slot;
	bool isPlayerHeading;
#if __BANK
	char debugName[64];
#endif
};

struct audScannerVehicleHistory
{
	audVehicleAudioEntity *audioEntity;
	bool hasDescribedColour;
	bool hasDescribedModel;
	bool hasDescribedManufacturer;
	bool hasDescribedCategory;
	bool isOnFoot;
	bool isPlayer;
};

struct audScannerSentence
{
	Vector3 position;
	audTracker *tracker;
	f32 policeScannerApplyValue;
	u32 timeRequested;
	audScannerPriority priority;
	audScannerSentenceType sentenceType; 
	u32 sentenceSubType;
	audMetadataRef areaHash;
	u32 currentPhrase;
	u32 numPhrases;
	atRangeArray<audScannerPhrase, g_MaxPhrasesPerSentence> phrases;
	audEnvironmentGroupInterface *occlusionGroup;
	audCategory *category;
	u32 backgroundSFXHash;
	u32 predelay;
	u32 fakeEchoTime;
	u32 voiceNum;
	s32 scriptID;
	eCrimeType crimeId;

	ScannerSlots m_LastUsedScannerSlot;
	ScannerSlots m_LastUsedSmallScannerSlot;

	audScannerVehicleHistory vehicleHistory;

	atRangeArray<bool, SCANNERSLOTS_MAX> m_SlotIsInUse;

	bool isPositioned;
	bool inUse;
	bool gotAllSlots;
	bool shouldDuckRadio;
	bool useRadioVolOffset;
	bool isPlayerCrime;
	bool playAcknowledge;
	bool playResponding;

#if __BANK
	char debugName[64];
#endif
};

struct audVehicleDescription
{
	audVehicleDescription() :
		prefixSound(0),
		colourSound(0),
		manufacturerSound(0),
		modelSound(0),
		category(0),
		audioEntity(NULL),
		isInVehicle(false),
		isVehicleKnown(false),
		isMotorbike(false)
		{};

	u32 prefixSound;
	u32 colourSound;
	u32 manufacturerSound;
	u32 modelSound;
	u32 category;
	audVehicleAudioEntity *audioEntity;
	bool isInVehicle;
	bool isVehicleKnown;
	bool isMotorbike;
};

struct audPositionDescription
{
	audMetadataRef areaSound;
	audMetadataRef streetSound;
	u32 inDirectionSound;
	audMetadataRef areaConjunctiveSound;
	audMetadataRef streetConjunctiveSound;
	u32 locationSound;
	u32 locationDirectionSound;
};

struct audPoliceSpotting
{
	audPositionDescription posDescription;
	audVehicleDescription vehDescription;	
	audScannerPriority priority;
	audDirection heading;
	f32 speed;
	bool forceVehicle;
	bool fromScript;
	bool isPlayerHeading;
};

struct audCrimeReport
{
	audCrimeReport()
	{
		duckRadio = false;
	}

	audPositionDescription posDescription;
	audScannerPriority priority;
	ScannerCrimeReport *crime;
	u32 crimeId;
	u32 delay;
	u32 subtype;
	bool duckRadio;
};

struct audWeightedVariation
{
	u32 item;
	f32 weight;
};
class CVehicle;

class audScannerManager : naAudioEntity
{
	friend class audPoliceScanner;
	friend class audTrainAudioEntity;
public:
	AUDIO_ENTITY_NAME(audScannerManager);

	audScannerManager();
	virtual ~audScannerManager();

	void Init();
	void Shutdown();
	void Update();

	audScannerSentence *AllocateSentence(bool priorityCheck=false, audScannerPriority priorityToCheckAgainst=AUD_SCANNER_PRIO_AMBIENT, const u32 voiceNum=0);

	void CancelAnyPlayingReportsNextFade();
	void CancelAnyPlayingReports(const u32 ReleaseTime = 0);
	void CancelAllReports();
	void CancelAnySentencesFromTracker(audTracker *tracker);
	void CancelAnySentencesFromEntity(audVehicleAudioEntity *entity);

	void DisableScanner();
	void EnableScanner();
	void DisableScannerFlagChanged(const bool state);

	s32 TriggerScriptedReport(const char *scriptedReportName,f32 applyValue = 0.f);
	void PopulateSentenceWithPosDescription(const audPositionDescription &desc, audScannerSentence* sentence, u32 &idx);
	void CancelScriptedReport(s32 scriptID);
	bool IsScriptedReportPlaying(s32 scriptID);
	bool IsScriptedReportValid(s32 scriptID);
	s32 GetPlayingScriptedReport();

	f32  GetPoliceScannerApplyValue() {return m_RequestedSentences[m_PlayingSentenceIndex].policeScannerApplyValue;}


	void SetCarUnitValues(s32 UnitTypeNum, s32 BeatNum);
	void SetPositionValue(const Vector3 &pos);
	void SetCrimeValue(s32 crime);
	
	void ReportResistArrest(const Vector3& pos, u32 delay);
	void ResetResistArrest();
	bool HasCrimeBeenReportedRecently(const eCrimeType crime, u32 timeInMs);

	bool ShouldDuckRadio() const
	{
		return (m_ScannerState == AUD_SCANNER_PLAYBACK && m_RequestedSentences[m_PlayingSentenceIndex].shouldDuckRadio);
	}

	inline audMetadataRef GetLastSpokenAreaHash() {return m_LastSpokenAreaHash;}
	inline audDirection GetPlayerHeading() {return m_PlayerHeading;}

	void SetTimeToPlayCDIVehicleLine();
	void TriggerCopDispatchInteraction(ScannerCopDispatchInteractionType interaction);
	bool ReallyTriggerCopDispatchInteraction(ScannerCopDispatchInteractionType interaction);
	void SetPlayerSpottedTime(CPed* pSpotter);

	void SetShouldChooseNewHeliVoice() { m_ShouldChooseNewHeliVoice = true; }
	void SetShouldChooseNewNonHeliVoice() { m_ShouldChooseNewNonHeliVoice = true; }
	void IncrementCopShot() { m_CopShotTally++; }
	bool ShouldTryToPlayShotAtHeli(u32 audioTime) { return audioTime >= m_TimeCDIsCanPlay[SCANNER_CDIT_SHOT_AT_HELI_MEGAPHONE]; }
	void IncrementHelisInChase();
	void DecrementHelisInChase();
	void StartScannerNoise(bool isDispatch = false, s32 predelay = -1);
	void StopScannerNoise();
	bool CanPlayRefuellingReport();

	u32 GetLastTimePlayerWasClean()	{ return m_LastTimePlayerWasClean; }
	void SetTimeCheckedLastPosition(u32 time)	{ m_TimeLastCheckedPosition = time; }

	static audDirection CalculateAudioDirection(const Vector3 &vDirection);

#if __BANK
	void AddWidgets(bkBank &bank);
#endif
private:
	bool ChooseSentenceToPlay(u32 &sentenceIndex);

	audPrepareState PrepareSentence(const u32 sentenceIndex);
	bool CreateSentenceSounds(const u32 sentenceIndex);
	void CleanupSentenceSounds(const u32 sentenceIndex, const u32 ReleaseTime = 0);

	void UpdateHistory();
	bool IsSentenceStillValid(const u32 index);

	void InitSentenceForScanner(const u32 index, const u32 voiceNum = 0);

	u32 GetScannerHashForVoice(const char* input, const u32& voiceNum);
	u32 GetScannerHashForVoice(const u32& input, const u32& voiceNum);
	u32 SelectScannerVoice();

	ScannerSlots GetNextSmallScannerSlot(audScannerSentence& sentence);
	ScannerSlots GetNextScannerSlot(audScannerSentence& sentence);
	void DecrementNextSmallScannerSlot(audScannerSentence& sentence);
	void DecrementNextScannerSlot(audScannerSentence& sentence);

	//Scripted procedural resolving functions
	void SelectCarUnitSounds(u32& compositeCodeSound, u32& unitTypeSound, u32& beatSound, u32 voiceNum);

	void ResetScriptSetVariables();

	void HandleCopDispatchInteraction();
	void TriggerCopDispatchInteractionVehicleLine();
	void FinishCopDispatchInteraction();
	void CancelCopDispatchInteraction();
	u32 GetVoiceHashForCopSpeech();
	u32 GetVoiceHashForCopSpottedSpeech();
	u32 GetVoiceHashForHeliCopSpeech();
	bool PlayCopToDispatchLine();
	void PlayDispatchAcknowledgementLine();
	void PlayHeliGuidanceLine(u32 currentTime);
	u32 GetNextPlayTimeForCDI(ScannerCopDispatchInteractionType m_DispatchInteractionType, u32 currentTime);
	bool CanDoCopSpottingLine();

	Vector3 m_LastPlayerPosition;
	u32 m_TimeLastCheckedPosition;

	Vector3 m_ScriptSetPos;
	eCrimeType m_ScriptSetCrime;

	u32 m_StateChangeTimeMs;
	u32 m_LastReportTimeMs;
	u32 m_CurrentSentenceIndex;
	u32 m_PlayingSentenceIndex;
	audScannerSentenceType m_LastSentenceType;
	u32 m_LastSentenceSubType;

	audMetadataRef m_LastSpokenAreaHash;
	u32 m_NextRandomChatTime;

	u32 m_ValidSentenceTypes;

	audScannerState m_ScannerState;
	audDirection m_PlayerHeading;

	u32 m_LastScriptedLineTimeMs;
	//To allow script to check if a given triggered-line is queued, playing, etc.
	u32 m_NextScriptedLineID;

	s32 m_UnitTypeNumber;
	s32 m_BeatNumber;

	u32 m_TimeResistArrestLastTriggered;
	Vector3 m_PosForDelayedResistArrest;
	u32 m_DelayForDelayedResistArrest;

	audSound *m_NoiseLoop;
	audScene* m_ScannerMixScene;

	atArray<audWaveSlot*> m_ScannerSlots;
	atRangeArray<audScannerSentence, g_MaxBufferedSentences> m_RequestedSentences;

	atRangeArray<u32, SCANNER_CDIT_TOT_NUM> m_DispIntCopSpeechContextPHash;
	audSpeechSound* m_DispIntCopSpeechSound;
	u32 m_DispatchInteractionCopVoicehash;
	u32 m_DispatchInteractionHeliCopVoicehash;
	u32 m_TimeAnyCDICanPlay;
	u32 m_TimeToPlayDispIntLine;
	u32 m_TimeToPlayDispIntResponse;
	u32 m_MinTimeForNextCDIVehicleLine;
	u32 m_MaxTimeForNextCDIVehicleLine;
	u32 m_LastTimeSeenByHeliCop;
	u32 m_LastTimeSeenByNonHeliCop;
	u32 m_TimeLastCDIFinished;
	u32 m_LastTimePlayerWasClean;
	atRangeArray<u32, SCANNER_CDIT_TOT_NUM> m_TimeCDIsCanPlay;
	u8 m_CopShotTally;
	s8 m_HelisInvolvedInChase;
	ScannerCopDispatchInteractionType m_DispatchInteractionType;
	ScannerCopDispatchInteractionType m_DispatchInteractionTypeTriggered;
	bool m_DispIntCopHasSpoken;
	bool m_DispInDispHasSpoken;
	bool m_WaitingOnDispIntResponse;
	bool m_ForceDispIntResponseThrough;
	bool m_DispIntHeliHasSpoken;
	bool m_DispIntHeliMegaphoneHasSpoken;
	bool m_DispIntHeliMegaphoneShouldSpeak;
	bool m_ShouldChooseNewHeliVoice;
	bool m_ShouldChooseNewNonHeliVoice;
	bool m_LastHeliGuidanceLineWasNoVisual;
	bool m_AmbientScannerEnabled;
	bool m_WantToCancelReport;
	bool m_DelayingReportCancellationUntilLineEnd;
	bool m_HasTriggerGuidanceDispatch;
};
#endif // NA_POLICESCANNER_ENABLED
#endif // SCANNERMANAGER_H_INCLUDED

