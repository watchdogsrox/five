// 
// audio/policescanner.cpp
// 
// Copyright (C) 1999-2007 Rockstar Games.  All Rights Reserved.
// 


#include "audiodefines.h"

#if NA_POLICESCANNER_ENABLED

#include "northaudioengine.h"
#include "boataudioentity.h"
#include "gameobjects.h"
#include "policescanner.h"
#include "radioaudioentity.h"
#include "radiostation.h"
#include "caraudioentity.h"
#include "scriptaudioentity.h"
#include "speechaudioentity.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/soundfactory.h"
#include "audiohardware/waveslot.h"

#include "camera/CamInterface.h"
#include "game/dispatch/OrderManager.h"
#include "fwdebug/picker.h"
#include "grcore/debugdraw.h"
#include "game/crime.h"
#include "game/modelindices.h"
#include "game/user.h"
#include "text/text.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "peds/ped.h"
#include "Peds/pedpopulation.h"
#include "peds/popzones.h"
#include "peds/PopCycle.h"
#include "scene/entity.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "streaming/streamingengine.h"
#include "fwsys/timer.h"
#include "vehicles/vehicle.h"
#include "vehicles/boat.h"
#include "vehicles/train.h"
#include "vector/vector3.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "system/param.h"

#include "text/TextConversion.h"

#include "debugaudio.h"

AUDIO_OPTIMISATIONS()

u32 g_MinTimeBetweenReports = 15000;
u32 g_MinTimeBetweenCriticalReports = 5000;
u32 g_ScannerCrimeReportTime = 3000;
f32 g_ReportCrimeProbability = 1.f;
f32 g_ReportSpottingProbability = 0.001f;
f32 g_UnsureOfNewAreaProb = 1.0f;
f32 g_ErrAfterInProb = 0.6f;
f32 g_SkipDuplicateAreaProb = 0.85f;
f32 g_SkipNewAreaProb = 0.5f;
f32 g_ReportSuspectHeadingProb = 0.9f;
f32 g_ReportCarProb = 0.65f;
f32 g_ReportCarPrefixProb = 0.3f;
f32 g_DistFactorToConsiderCentral = 0.01f;
f32 g_AreaSizeTooSmallForDirection = 150.0f;
f32 g_MinPlayerMovementThreshold = 12.0f;
u32 g_DelayBeforeErr = 50;
u32 g_DelayAfterErr = 250;
u32 g_DelayAfterStreet = 0;
u32 g_MinGapAfterScriptedReport = 7000;
u32 g_ScannerAttackTime = 50;
f32 g_MinCopDistSqForCopInstSet = 40.0f * 40.0f;
f32 g_MinPedDistSqForPedInstSet = 40.0f * 40.0f;
f32 g_SquelchProb = 1.0f;


u32 g_ScannerPreemptMs = 85;

bank_float g_VolOffsetForRadio = 4.f;

u32 g_PositionUpdateTimeMs = 2000;
f32 g_PositionUpdateDeltaSq = 0.145f;

bool g_TriggerDuplicateCrimes = false;
bool g_ScannerEnabled = true;
bool g_ShowAreaDebug = false;
bool g_ShowBufferedSentences = false;


u32 g_ScannerSceneHash = ATSTRINGHASH("POLICE_SCANNER_REGULAR", 0x2A7453A6);
u32 g_DispatchScannerSceneHash = ATSTRINGHASH("POLICE_SCANNER_DISPATCH_CONVERSATION", 0xDAFB3E18);
u32 g_DispIntSpeechSoundHash = ATSTRINGHASH("DISPATCH_INTERACTION_SPEECH_SOUND", 0x580384E8);
CopDispatchInteractionSettings* g_CopDispatchInteractionSettings = NULL;
bool g_EnableCopDispatchInteraction = true;

atRangeArray<u32 ,4> g_AudioPoliceScannerDir;

extern f32 g_MinPctForLocationAt;
extern f32 g_MinPctForLocationNear;
extern u32 g_SquelchPostDelay;
extern u32 g_SquelchSoundHash;
extern CPlayerSwitchInterface g_PlayerSwitch;
extern audMetadataRef g_ScannerNullSoundRef;
extern f32 g_MaxDistanceToPlayCopSpeech;	

#if __BANK
bool g_PrintCarColour = false;
bool g_ForceScannerVoice = false;
u32 g_ForcedScannerVoice = 0;
bool g_PrintScannerTriggers = false;

bool g_OverrideCarCodes = false;
u32 g_UnitTypeNumberOverride = 0;
u32 g_BeatNumberOverride = 1;

bool g_DebuggingScanner = false;
char g_TestStreetName[128];
char g_TestAreaName[128];

bool g_TestDispInteraction = false;
bool g_DebugCopDispatchInteraction = false;
bool g_DrawScannerSpecificLocations = false;
#endif

audScannerManager g_AudioScannerManager;

static bank_float g_AmbientScannerVolume = -9.f;
static bank_float g_AmbientScannerPadSpeakerVolume = -6.f;

audCategory *g_PoliceScannerCategory = NULL;
audCategory *g_PoliceScannerDeathScreenCategory = NULL;

#define SCANNER_STATE(x) (u32)(1<<x)
u32 g_ScannerValidStateTransitions[] =
{
	~0U,//AUD_SCANNER_CRIME_REPORT,
	~SCANNER_STATE(AUD_SCANNER_ASSISTANCE_REQUIRED), //AUD_SCANNER_ASSISTANCE_REQUIRED
	~SCANNER_STATE(AUD_SCANNER_SUSPECT_SPOTTED),//AUD_SCANNER_SUSPECT_SPOTTED,
	~0U,//AUD_SCANNER_SINGLE_DISPATCH,
	~SCANNER_STATE(AUD_SCANNER_MULTI_DISPATCH),//AUD_SCANNER_MULTI_DISPATCH,
	SCANNER_STATE(AUD_SCANNER_CRIME_REPORT)|SCANNER_STATE(AUD_SCANNER_SCRIPTED),//AUD_SCANNER_SUSPECT_DOWN,
	~0U,//AUD_SCANNER_RANDOM_CHAT,
	~0U,//AUD_SCANNER_SCRIPTED,
	~0U,//AUD_SCANNER_TRAIN,
	~0U,//AUD_SCANNER_UNKNOWN,
};

const char* g_CarCodeUnitTypeStrings[26] =
{
	"ADAM",
	"BOY",
	"CHARLES",
	"DAVID",
	"EDWARD",
	"FRANK",
	"GEORGE",
	"HENRY",
	"IDA",
	"JOHN",
	"KING",
	"LINCOLN",
	"MARY",
	"NORA",
	"OCEAN",
	"PAUL",
	"QUEEN",
	"ROBERT",
	"SAM",
	"TOM",
	"UNION",
	"VICTOR",
	"WILLIAM",
	"XRAY",
	"YOUNG",
	"ZEBRA"
};

const char* g_VoiceNumToStringMap[AUD_NUM_SCANNER_VOICES] =
{
	"",
	"_B",
	"_C"
};

audMetadataRef g_ScannerCarUnitTriggerRef;
audMetadataRef g_ScannerLocationTriggerRef;
audMetadataRef g_ScannerCrimeTriggerRef;
audMetadataRef g_ScannerCrimeCodeTriggerRef;

extern f32 g_SpeedInfoMinAccuracy;
extern f32 g_HeadingInfoMinAccuracy;
extern f32 g_VehicleInfoMinAccuracy;
extern f32 g_CarPrefixInfoMinAccuracy;
extern f32 g_CarCategoryInfoMinAccuracy;
extern f32 g_CarModelInfoMinAccuracy;

PARAM(audScannerSpew, "Turn on police scanner audio spew.");

audScannerManager::audScannerManager()
{

}

audScannerManager::~audScannerManager()
{

}

void audScannerManager::Init()
{
	m_ScannerState = AUD_SCANNER_IDLE;

	m_LastSentenceType = AUD_SCANNER_UNKNOWN;
	m_LastSentenceSubType = 0;
	m_StateChangeTimeMs = 0;
	m_CurrentSentenceIndex = 0;
	m_PlayingSentenceIndex = ~0U;
	m_LastReportTimeMs = 0;

	m_TimeLastCheckedPosition = 0;
	m_PlayerHeading = AUD_DIR_UNKNOWN;
	m_NextRandomChatTime = 0;

	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_01", 0x039358fbd)));
	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_02", 0x064fae747)));
	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_03", 0x05ebbdac9)));
	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_04", 0x0905ebe0e)));

	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_SMALL_01", 0x0450e1eb6)));
	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_SMALL_02", 0x04ec9322c)));
	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_SMALL_03", 0x057d8c44b)));
	m_ScannerSlots.PushAndGrow(audWaveSlot::FindWaveSlot(ATSTRINGHASH("SCANNER_SMALL_04", 0x0724ef937)));

	for(u32 i = 0; i < g_MaxBufferedSentences; i++)
	{
		m_RequestedSentences[i].inUse = false;
		m_RequestedSentences[i].shouldDuckRadio = false;
		m_RequestedSentences[i].policeScannerApplyValue = 0.f;
		m_RequestedSentences[i].crimeId = CRIME_NONE;
		m_RequestedSentences[i].timeRequested = 0;
	}

	naAudioEntity::Init();
	audEntity::SetName("audScannerManager");

	m_ValidSentenceTypes = SCANNER_STATE(AUD_SCANNER_CRIME_REPORT);
	m_AmbientScannerEnabled = false;

	g_PoliceScannerCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("FRONTEND_GAME_POLICE_SCANNER", 0x0a95bf404));
	g_PoliceScannerDeathScreenCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("POLICE_SCANNER_DEATH_SCREEN", 0xDA38F55D));
	m_WantToCancelReport = false;
	m_DelayingReportCancellationUntilLineEnd = false;

	g_ScannerCarUnitTriggerRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("SCANNER_CAR_UNIT_TRIGGER", 0xC2BCCD6D));
	g_ScannerLocationTriggerRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("SCANNER_LOCATION_TRIGGER", 0xCDB1CED4));
	g_ScannerCrimeTriggerRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("SCANNER_CRIME_TRIGGER", 0x65116338));
	g_ScannerCrimeCodeTriggerRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(ATSTRINGHASH("SCANNER_CRIME_CODE_TRIGGER", 0x5A9F843E));

	g_AudioPoliceScannerDir[0] = ATPARTIALSTRINGHASH("POLICE_SCANNER_HEADING_NORTH", 0x560240F);	
	g_AudioPoliceScannerDir[1] = ATPARTIALSTRINGHASH("POLICE_SCANNER_HEADING_EAST", 0xDA639C6F);	
	g_AudioPoliceScannerDir[2] = ATPARTIALSTRINGHASH("POLICE_SCANNER_HEADING_SOUTH", 0xA898D9E4);	
	g_AudioPoliceScannerDir[3] = ATPARTIALSTRINGHASH("POLICE_SCANNER_HEADING_WEST", 0x3FC544CE);

	m_UnitTypeNumber = 0;
	m_BeatNumber = 0;

	m_TimeResistArrestLastTriggered = 0;
	m_PosForDelayedResistArrest.Set(ORIGIN);
	m_DelayForDelayedResistArrest = 0;

	m_ScriptSetPos.Set(ORIGIN);
	m_ScriptSetCrime = CRIME_NONE;

	g_PoliceScanner.Init();

	m_ScannerMixScene = NULL;
	m_DispatchInteractionType = SCANNER_CDIT_NONE;
	m_DispIntCopHasSpoken = false;
	m_DispInDispHasSpoken = false;
	m_WaitingOnDispIntResponse = false;
	m_DispIntHeliHasSpoken = false;
	m_DispIntHeliMegaphoneHasSpoken = false;
	m_DispIntHeliMegaphoneShouldSpeak = false;
	m_LastHeliGuidanceLineWasNoVisual = false;
	m_DispIntCopSpeechSound = NULL;
	m_DispatchInteractionCopVoicehash = 0;
	m_DispatchInteractionHeliCopVoicehash = 0;
	m_TimeAnyCDICanPlay = 0;
	m_TimeToPlayDispIntLine = 0;
	m_TimeToPlayDispIntResponse = 0;
	m_MinTimeForNextCDIVehicleLine = 0;
	m_MaxTimeForNextCDIVehicleLine = 0;
	m_LastTimeSeenByHeliCop = 0;
	m_LastTimeSeenByNonHeliCop = 0;
	m_TimeLastCDIFinished = 0;
	m_CopShotTally = 0;
	m_ShouldChooseNewHeliVoice = true;
	m_ShouldChooseNewNonHeliVoice = true;
	m_HasTriggerGuidanceDispatch = true;
	m_HelisInvolvedInChase = 0;
	m_LastTimePlayerWasClean = 0;

	for(int i=0; i<SCANNER_CDIT_TOT_NUM ;i++)
		m_TimeCDIsCanPlay[i] = 0;

	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE] = ATPARTIALSTRINGHASH("CDI_REPORT_SUSPECT_CRASHED_VEHICLE", 0x8CB50D98);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REPORT_SUSPECT_ENTERED_FREEWAY] = ATPARTIALSTRINGHASH("CDI_REPORT_SUSPECT_ENTERED_FREEWAY", 0x1CE84367);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REPORT_SUSPECT_ENTERED_METRO] = ATPARTIALSTRINGHASH("CDI_REPORT_SUSPECT_ENTERED_METRO", 0xAB2D7F92);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REPORT_SUSPECT_IS_IN_CAR] = ATPARTIALSTRINGHASH("CDI_REPORT_SUSPECT_IS_IN_CAR", 0xD7BD33CF);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REPORT_SUSPECT_IS_ON_FOOT] = ATPARTIALSTRINGHASH("CDI_REPORT_SUSPECT_IS_ON_FOOT", 0x30AD3569);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REPORT_SUSPECT_IS_ON_MOTORCYCLE] = ATPARTIALSTRINGHASH("CDI_REPORT_SUSPECT_IS_ON_MOTORCYCLE", 0x4C1A6A0D);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REPORT_SUSPECT_LEFT_FREEWAY] = ATPARTIALSTRINGHASH("CDI_REPORT_SUSPECT_LEFT_FREEWAY", 0x6454D746);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REQUEST_BACKUP] = ATPARTIALSTRINGHASH("CDI_REQUEST_BACKUP", 0xC4A70032);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_ACKNOWLEDGE_SITUATION_DISPATCH] = ATPARTIALSTRINGHASH("CDI_ACKNOWLEDGE_SITUATION_DISPATCH", 0x2B2234C7);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_UNIT_RESPONDING_DISPATCH] = ATPARTIALSTRINGHASH("CDI_UNIT_RESPONDING_DISPATCH", 0x635BC429);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH] = ATPARTIALSTRINGHASH("CDI_REQUEST_GUIDANCE_DISPATCH", 0x26BEA5C4);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_HELI_MAYDAY_DISPATCH] = ATPARTIALSTRINGHASH("CDI_HELI_MAYDAY_DISPATCH", 0xF00FD2EF);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_SHOT_AT_HELI_MEGAPHONE] = ATPARTIALSTRINGHASH("CDI_SHOT_AT_HELI_MEGAPHONE", 0x9E998518);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_HELI_APPROACHING_DISPATCH] = ATPARTIALSTRINGHASH("CDI_HELI_APPROACHING_DISPATCH", 0x3AD3B6E1);
	m_DispIntCopSpeechContextPHash[SCANNER_CDIT_PLAYER_SPOTTED] = ATPARTIALSTRINGHASH("CDI_SPOT_SUSPECT", 0x2EEB0FD9);

	g_CopDispatchInteractionSettings = audNorthAudioEngine::GetObject<CopDispatchInteractionSettings>("COP_DISPATCH_INTERACTION_SETTINGS");
	if(!g_CopDispatchInteractionSettings)
		naErrorf("Failed to find CopDispatchInteractionSettings object.");

#if __BANK
	g_TestStreetName[0] = '\0';
	g_TestAreaName[0] = '\0';

	if(PARAM_audScannerSpew.Get())
	{
		g_PrintScannerTriggers = true;
	}
#endif

}

void audScannerManager::Shutdown()
{
	CancelAnyPlayingReports();
	if(m_ScannerMixScene)
	{
		m_ScannerMixScene->Stop();
		m_ScannerMixScene = NULL;
	}
	m_ScannerSlots.Reset();
	audEntity::Shutdown();
	g_PoliceScanner.Shutdown();
}

void audScannerManager::DisableScanner()
{
	g_ScannerEnabled = false;
	for(u32 i = 0; i < g_MaxBufferedSentences; i++)
	{
		m_RequestedSentences[i].inUse = false;
	}

	CancelAllReports();
}

void audScannerManager::EnableScanner()
{
	g_ScannerEnabled = true;
	for(u32 i = 0; i < g_MaxBufferedSentences; i++)
	{
		m_RequestedSentences[i].inUse = false;
	}
}

void audScannerManager::DisableScannerFlagChanged(const bool state)
{
	for(u32 i = 0; i < g_MaxBufferedSentences; i++)
	{
		m_RequestedSentences[i].inUse = false;
	}

	if(state)
	{
		CancelAllReports();
	}

	g_ScannerEnabled = !state;
}

void audScannerManager::CancelAnyPlayingReportsNextFade()
{
	FinishCopDispatchInteraction();
	if(m_ScannerState == AUD_SCANNER_PLAYBACK)
	{
		m_WantToCancelReport = true;
	}
	else if(m_ScannerState == AUD_SCANNER_PREPARING)
	{
		CleanupSentenceSounds(m_PlayingSentenceIndex);
		g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
		m_ScannerState = AUD_SCANNER_IDLE;
	}
}

void audScannerManager::CancelAnyPlayingReports(const u32 ReleaseTime)
{
	FinishCopDispatchInteraction();
	if(m_ScannerState != AUD_SCANNER_IDLE && m_RequestedSentences[m_CurrentSentenceIndex].crimeId == CRIME_SUICIDE)
	{
		//Wait to kill until this sentence is done if its suicide
		m_DelayingReportCancellationUntilLineEnd = true;
		return;
	}

	for(u32 i = 0; i < g_MaxBufferedSentences; i++)
	{
		CleanupSentenceSounds(i, ReleaseTime);
	}
	g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
	m_ScannerState = AUD_SCANNER_IDLE;
	m_TimeLastCheckedPosition = 0;
	m_StateChangeTimeMs = 0; 
	if(m_NoiseLoop)
	{	
		// set volume to silence so that we don't hear the OnStop sound as the screen fades back up
		m_NoiseLoop->SetRequestedVolume(-100.f);
		m_NoiseLoop->StopAndForget();
	}
}

void audScannerManager::CancelAllReports()
{
	FinishCopDispatchInteraction();
	for(u32 i = 0; i < g_MaxBufferedSentences; i++)
	{
		CleanupSentenceSounds(i);
	}
	g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
	m_ScannerState = AUD_SCANNER_IDLE;
	m_TimeLastCheckedPosition = 0;
	m_StateChangeTimeMs = 0; 
	if(m_NoiseLoop)
	{	
		// set volume to silence so that we don't hear the OnStop sound as the screen fades back up
		m_NoiseLoop->SetRequestedVolume(-100.f);
		m_NoiseLoop->StopAndForget();
	}

	for(u32 i = 0;i < g_MaxBufferedSentences; i++)
	{
		m_RequestedSentences[i].inUse = false;
	}
}

bool audScannerManager::ChooseSentenceToPlay(u32 &sentenceIndex)
{
	const u32 now = g_AudioEngine.GetTimeInMilliseconds();
	bool haveHighPriority = false;
	// firstly search for a critical sentence
	for(u32 i = 0; i < g_MaxBufferedSentences; i++)
	{
		// critical sentences are spoken as soon as they are requested
		// high priority sentences are cancelled if they are over 15s old or if history prevents them
		const u32 index = (i+m_CurrentSentenceIndex) % g_MaxBufferedSentences;
		if(m_RequestedSentences[index].inUse && m_RequestedSentences[index].gotAllSlots &&
			(m_RequestedSentences[index].priority == AUD_SCANNER_PRIO_HIGH || m_RequestedSentences[index].priority == AUD_SCANNER_PRIO_CRITICAL)
			&& now >= m_RequestedSentences[index].timeRequested)
		{
			if((m_RequestedSentences[index].priority == AUD_SCANNER_PRIO_CRITICAL || BANK_ONLY(g_DebuggingScanner ||)
				(m_LastScriptedLineTimeMs + g_MinGapAfterScriptedReport < now && now - m_RequestedSentences[index].timeRequested < 15000 
							&& (m_RequestedSentences[index].sentenceType == AUD_SCANNER_CRIME_REPORT || m_RequestedSentences[index].sentenceType == AUD_SCANNER_SUSPECT_DOWN || m_RequestedSentences[index].sentenceType != m_LastSentenceType || m_RequestedSentences[index].sentenceSubType != m_LastSentenceSubType)))
				&& IsSentenceStillValid(index))
			{
				if(m_LastReportTimeMs + g_MinTimeBetweenReports < now BANK_ONLY(|| g_DebuggingScanner))
				{
					sentenceIndex = index;
					return true;
				}
				else
				{
					// prevent something lower priority triggering while this sentence is relevant
					haveHighPriority = true;
				}
			}
			else
			{
				// cancel this report
				m_RequestedSentences[index].inUse = false;
				m_RequestedSentences[index].shouldDuckRadio = false;
			}
		}
	}
	if(!haveHighPriority)
	{
		//Don't play ambient lines if we don't have player control (b* 960566)
		if(FindPlayerPed() && FindPlayerPed()->GetPlayerInfo() && FindPlayerPed()->GetPlayerInfo()->AreControlsDisabled() &&
			!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowPoliceScannerWhenPlayerHasNoControl))
		{
			sentenceIndex = ~0U;
			return false;
		}

		// next search for regular player sentences
		if(m_LastReportTimeMs + g_MinTimeBetweenReports < now BANK_ONLY(|| g_DebuggingScanner))
		{
			for(u32 i = 0; i < g_MaxBufferedSentences; i++)
			{
				u32 elapsed = now - m_RequestedSentences[i].timeRequested;
				if(m_RequestedSentences[i].inUse && m_RequestedSentences[i].gotAllSlots && elapsed < 7500)
				{
					if(IsSentenceStillValid(i) && (g_TriggerDuplicateCrimes || (m_RequestedSentences[i].sentenceType != m_LastSentenceType || m_RequestedSentences[i].sentenceSubType != m_LastSentenceSubType)))
					{
						sentenceIndex = i;
						m_LastReportTimeMs = now;
						return true;
					}
					else
					{
						// cancel this report as it is no longer relevant
						m_RequestedSentences[i].inUse = false;
						m_RequestedSentences[i].shouldDuckRadio = false;
					}
				}
			}
		}
		
		// lastly check for ambient
		for(u32 i = 0; i < g_MaxBufferedSentences; i++)
		{
			const u32 index = (i+m_CurrentSentenceIndex) % g_MaxBufferedSentences;
			if(m_RequestedSentences[index].inUse && m_RequestedSentences[index].gotAllSlots &&
				(m_RequestedSentences[index].priority == AUD_SCANNER_PRIO_AMBIENT && m_AmbientScannerEnabled)
				&& now >= m_RequestedSentences[index].timeRequested)
			{
				sentenceIndex = index;
				return true;
			}
		}
	}
	
	sentenceIndex = ~0U;
	return false;
}

audPrepareState audScannerManager::PrepareSentence(const u32 sentenceIndex)
{
	audPrepareState state = AUD_PREPARED;
	naAssertf(m_RequestedSentences[sentenceIndex].inUse, "Attemprting to prepare sentence with index %d but it's already in use", sentenceIndex);
	naCErrorf(m_RequestedSentences[sentenceIndex].numPhrases, "Attempting to preapare sentence with index %d but it doesn't have any phrases", sentenceIndex);
	for(u32 i = 0; i < m_RequestedSentences[sentenceIndex].numPhrases; i++)
	{
#if __BANK
		if(!m_RequestedSentences[sentenceIndex].phrases[i].sound)
		{
			naAssertf(0, "Attempting to prepare sentence with index %d but phrase %d has no valid audSound", sentenceIndex, i);
			for(u32 i = 0; i < m_RequestedSentences[sentenceIndex].numPhrases; i++)
			{
				naWarningf("Scanner sentence phrase %u: %s", i, m_RequestedSentences[sentenceIndex].phrases[i].debugName);
			}
		}
#endif
		if(!m_RequestedSentences[sentenceIndex].phrases[i].sound)
		{
			return AUD_PREPARE_FAILED;
		}
		audPrepareState thisState = AUD_PREPARED;
		if(m_RequestedSentences[sentenceIndex].phrases[i].slot != SCANNER_SLOT_NULL)
			m_RequestedSentences[sentenceIndex].phrases[i].sound->Prepare(m_ScannerSlots[m_RequestedSentences[sentenceIndex].phrases[i].slot], true);
		else
			m_RequestedSentences[sentenceIndex].phrases[i].sound->Prepare();
		if(thisState == AUD_PREPARE_FAILED)
		{
			return thisState;
		}
		else if(thisState == AUD_PREPARING)
		{
			state = thisState;
		}
	}

	return state;
}

bool audScannerManager::CreateSentenceSounds(const u32 sentenceIndex)
{
	naAssertf(m_RequestedSentences[sentenceIndex].inUse, "Attempting to create sentence sounds for sentence with index %d, but it's already in use", sentenceIndex);
	// if this sentence isn't positioned then it shouldn't have a tracker
	naCErrorf(m_RequestedSentences[sentenceIndex].isPositioned || m_RequestedSentences[sentenceIndex].tracker == NULL, "Sentence should either be positioned or  not have a tracker");
	for(u32 i = 0; i < m_RequestedSentences[sentenceIndex].numPhrases; i++)
	{
		audSoundInitParams params;
		params.AllowLoad = true;
		params.AllowOrphaned = false;
		params.Category = m_RequestedSentences[sentenceIndex].category;
		params.Tracker = m_RequestedSentences[sentenceIndex].tracker;
		params.AttackTime = g_ScannerAttackTime;
		params.EffectRoute = EFFECT_ROUTE_PADSPEAKER_1;
		if(i == 0)
		{
			params.Predelay = m_RequestedSentences[sentenceIndex].predelay;
		}
		if(m_RequestedSentences[sentenceIndex].phrases[i].isPlayerHeading)
		{
			Vector3 playerMovement = CGameWorld::FindLocalPlayerCoors() - m_LastPlayerPosition;
			m_PlayerHeading = CalculateAudioDirection(playerMovement);
			if(m_PlayerHeading >= g_AudioPoliceScannerDir.size())
			{
				return false;
			}
			m_RequestedSentences[sentenceIndex].phrases[i].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_AudioScannerManager.GetScannerHashForVoice(g_AudioPoliceScannerDir[m_PlayerHeading], m_RequestedSentences[sentenceIndex].voiceNum));
		}
		//params.EffectRoute = EFFECT_ROUTE_FRONT_END;
		if(!m_RequestedSentences[sentenceIndex].phrases[i].soundMetaRef.IsValid() || m_RequestedSentences[sentenceIndex].phrases[i].soundMetaRef == g_ScannerNullSoundRef)
			return false;
		CreateSound_PersistentReference(m_RequestedSentences[sentenceIndex].phrases[i].soundMetaRef, &m_RequestedSentences[sentenceIndex].phrases[i].sound, &params); 
		if(!m_RequestedSentences[sentenceIndex].phrases[i].sound)
		{
#if __BANK
			if(g_PrintScannerTriggers)
				Displayf("[audScannerSpew] Police scanner failed.  Context: %s Line: %s LineNum: %u", m_RequestedSentences[sentenceIndex].debugName, m_RequestedSentences[sentenceIndex].phrases[i].debugName, i);

			if(PARAM_audScannerSpew.Get())
			{
				Displayf("[audScannerSpew] Police scanner failed to build sounds.  Context: %s Line: %s LineNum: %u", m_RequestedSentences[sentenceIndex].debugName, m_RequestedSentences[sentenceIndex].phrases[i].debugName, i);
				for(u32 j = 0; j < m_RequestedSentences[sentenceIndex].numPhrases; j++)
				{
					Displayf("[audScannerSpew] Line %i Context %s", j, m_RequestedSentences[sentenceIndex].phrases[i].debugName);
				}
			}
#endif
			return false;
		}
		if(m_RequestedSentences[sentenceIndex].fakeEchoTime != 0)
		{
			audSoundInitParams params;
			params.AllowLoad = true;
			params.AllowOrphaned = false;
			params.Category = m_RequestedSentences[sentenceIndex].category;
			params.Tracker = m_RequestedSentences[sentenceIndex].tracker;
			params.Predelay = m_RequestedSentences[sentenceIndex].fakeEchoTime;
			params.EffectRoute = EFFECT_ROUTE_PADSPEAKER_1;
			if(i == 0)
			{
				params.Predelay += m_RequestedSentences[sentenceIndex].predelay;
			}
			//params.EffectRoute = EFFECT_ROUTE_FRONT_END;
			CreateSound_PersistentReference(m_RequestedSentences[sentenceIndex].phrases[i].soundMetaRef, &m_RequestedSentences[sentenceIndex].phrases[i].echoSound, &params); 
			if(!m_RequestedSentences[sentenceIndex].phrases[i].echoSound)
			{
#if __BANK
				if(g_PrintScannerTriggers)
					Displayf("[audScannerSpew] Police scanner failed.  Context: %s Line: %s LineNum: %u", m_RequestedSentences[sentenceIndex].debugName, m_RequestedSentences[sentenceIndex].phrases[i].debugName, i);
				if(PARAM_audScannerSpew.Get())
				{
					Displayf("[audScannerSpew] Police scanner failed to build echos.  Context: %s Line: %s LineNum: %u", m_RequestedSentences[sentenceIndex].debugName, m_RequestedSentences[sentenceIndex].phrases[i].debugName, i);
					for(u32 j = 0; j < m_RequestedSentences[sentenceIndex].numPhrases; j++)
					{
						Displayf("[audScannerSpew] Line %i soundName %s", j, m_RequestedSentences[sentenceIndex].phrases[i].debugName);
					}
				}
#endif
				return false;
			}
		}
	}
#if __BANK
	if(g_PrintScannerTriggers)
		Displayf("[audScannerSpew] Police scanner succeeded.  Context: %s", m_RequestedSentences[sentenceIndex].debugName);
#endif
	return true;
}

void audScannerManager::CleanupSentenceSounds(const u32 sentenceIndex, const u32 ReleaseTime)
{
	if(sentenceIndex >= g_MaxBufferedSentences)
		return;

	for(u32 i = 0; i < m_RequestedSentences[sentenceIndex].numPhrases; i++)
	{
		if(m_RequestedSentences[sentenceIndex].phrases[i].sound)
		{
			m_RequestedSentences[sentenceIndex].phrases[i].sound->SetReleaseTime(ReleaseTime);
			m_RequestedSentences[sentenceIndex].phrases[i].sound->StopAndForget();
		}
		if(m_RequestedSentences[sentenceIndex].phrases[i].echoSound)
		{
				m_RequestedSentences[sentenceIndex].phrases[i].echoSound->SetReleaseTime(ReleaseTime);
				m_RequestedSentences[sentenceIndex].phrases[i].echoSound->StopAndForget();
		}
		m_RequestedSentences[sentenceIndex].phrases[i].soundMetaRef.SetInvalid();
	}
	m_RequestedSentences[sentenceIndex].inUse = false;
	m_RequestedSentences[sentenceIndex].shouldDuckRadio = false;
}

void audScannerManager::Update()
{
	const Vector3 playerPos = CGameWorld::FindLocalPlayerCoors();
	bool isPlayerClean = FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() == WANTED_CLEAN;

	if( (m_WantToCancelReport && camInterface::GetFadeLevel() > 0.9f) ||
		g_PlayerSwitch.IsActive() ||
		audRadioStation::IsPlayingEndCredits()
		)
	{
		if(m_ScannerState != AUD_SCANNER_IDLE && m_PlayingSentenceIndex != ~0U)
		{
			g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
			CleanupSentenceSounds(m_PlayingSentenceIndex);
		}
		if(m_NoiseLoop)
		{
			m_NoiseLoop->SetRequestedVolume(-100.f);
			m_NoiseLoop->StopAndForget();
		}
		m_ScannerState = AUD_SCANNER_IDLE;
		m_WantToCancelReport = false;
		FinishCopDispatchInteraction();
	}

	if(g_ScannerEnabled)
	{
		u32 now = g_AudioEngine.GetTimeInMilliseconds();

		if(isPlayerClean)
		{
			m_LastTimePlayerWasClean = now;
		}

		if(m_DispatchInteractionTypeTriggered != SCANNER_CDIT_NONE)
		{
			ReallyTriggerCopDispatchInteraction(m_DispatchInteractionTypeTriggered);
			m_DispatchInteractionTypeTriggered = SCANNER_CDIT_NONE;
		}

		if(m_TimeResistArrestLastTriggered != 0 && now - m_TimeResistArrestLastTriggered > 750)
		{ 
			g_PoliceScanner.ReportPlayerCrime(m_PosForDelayedResistArrest, CRIME_RESIST_ARREST, m_DelayForDelayedResistArrest, true);
			m_TimeResistArrestLastTriggered = 0;
		}

		CVehicle *veh = FindPlayerVehicle();
		if(veh && veh->GetVehicleAudioEntity() && (NA_RADIO_ENABLED_ONLY(veh->GetVehicleAudioEntity()->GetHasEmergencyServicesRadio()||)veh->GetModelIndex()==MI_CAR_AMBULANCE) && veh->m_nVehicleFlags.bEngineOn)
		{
			if(!m_AmbientScannerEnabled)
			{
				g_PoliceScanner.SetNextAmbientMs(now + audEngineUtil::GetRandomNumberInRange(5000, 15000));
			}
			m_AmbientScannerEnabled = true;			
		}
		else
		{
			m_AmbientScannerEnabled = false;
		}
#if __BANK
		char scannerDebug[512] = "";
		if(g_ShowAreaDebug)
		{
			const Vector3 vPlayerPosition = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
			Vector3 v = vPlayerPosition;
			CPopZone *zone = CPopZones::FindSmallestForPosition(&v, ZONECAT_ANY, ZONETYPE_SP);
            if(zone)
            {
			    const Vector3 centre = Vector3((f32)zone->m_uberZoneCentreX, (f32)zone->m_uberZoneCentreY, 0.5f * ((f32)CPopZones::GetPopZoneZ1(zone) + (f32)CPopZones::GetPopZoneZ2(zone)));
			    grcDebugDraw::Line(Vector3(centre.x, centre.y, vPlayerPosition.z), vPlayerPosition, Color32(255,0,0),Color32(0,255,0));
			    Vector3 dir = centre - vPlayerPosition;
			    f32 dist = dir.Mag();
			    dir.NormalizeFast();
			    dir *= 2.f;
			    formatf(scannerDebug,255,"%s: %fm [%s]",zone->GetTranslatedName(), dist, (zone->m_bBigZoneForScanner?"big":"small"));
			    grcDebugDraw::Text(vPlayerPosition + Vector3(dir.x, dir.y, 0.0f), Color32(0,0,255),scannerDebug);
			    const char *dirstring[] = {"NORTH","EAST","SOUTH","WEST","UNKNOWN"};
			    formatf(scannerDebug,255,"heading: %s",dirstring[m_PlayerHeading]);
			    grcDebugDraw::PrintToScreenCoors(scannerDebug,10,10);
            }
		}

		if(g_ShowBufferedSentences)
		{
			for(u32 i = 0; i < g_MaxBufferedSentences; i++)
			{
				const char *sentenceTypes[] = {"AUD_SCANNER_CRIME_REPORT","AUD_SCANNER_ASSISTANCE_REQUIRED","AUD_SCANNER_SUSPECT_SPOTTED","AUD_SCANNER_SINGLE_DISPATCH","AUD_SCANNER_MULTI_DISPATCH","AUD_SCANNER_SUSPECT_DOWN","AUD_SCANNER_RANDOM_CHAT","AUD_SCANNER_SCRIPTED","AUD_SCANNER_TRAIN","AUD_SCANNER_UNKNOWN"};
				CompileTimeAssert(sizeof(sentenceTypes)/sizeof(sentenceTypes[0]) == AUD_SCANNER_UNKNOWN+1);
				formatf(scannerDebug, 255, "%s%u %u %s %u %f:", (m_PlayingSentenceIndex==i?"->":""), i, now - m_RequestedSentences[i].timeRequested, sentenceTypes[m_RequestedSentences[i].sentenceType], m_RequestedSentences[i].sentenceSubType,m_RequestedSentences[i].policeScannerApplyValue);
				for(u32 j = 0; j < m_RequestedSentences[i].numPhrases; j++)
				{
					char buf[128];
					if(!__DEV || !m_RequestedSentences[i].phrases[j].sound)
					{
						formatf(buf,sizeof(buf)," (%s) ", m_RequestedSentences[i].phrases[j].debugName);
					}
#if __DEV
					else if(m_RequestedSentences[i].currentPhrase == j)
					{
						formatf(buf, sizeof(buf), " [%s] ", m_RequestedSentences[i].phrases[j].sound->GetName());
					}
					else
					{
						formatf(buf, sizeof(buf), " %s ", m_RequestedSentences[i].phrases[j].sound->GetName());
					}
#endif
					strcat(scannerDebug, buf);
					if(m_RequestedSentences[i].phrases[j].postDelay > 0)
					{
						formatf(buf, sizeof(buf), " [%u] ", m_RequestedSentences[i].phrases[j].postDelay);
					}
				}

				grcDebugDraw::PrintToScreenCoors(scannerDebug, 5, 10 + i);				
			}
		}

		if(g_TestDispInteraction)
		{
			TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE);	
			g_TestDispInteraction = false;
		}
#endif

		// reset the have reported crime flag if wanted level is cleared or if we've not said anything for a while
		if(FindPlayerPed() && isPlayerClean)
		{
			m_ValidSentenceTypes = SCANNER_STATE(AUD_SCANNER_CRIME_REPORT);
			if(!m_AmbientScannerEnabled)
			{
				// reset history
				m_LastSentenceSubType = 0;
			}
			// keep reseting random chat time 
			m_NextRandomChatTime = now + audEngineUtil::GetRandomNumberInRange(5000,15000);
			m_HelisInvolvedInChase = 0;
		}
		else if(now > m_LastReportTimeMs + 60000)
		{
			// we have a wanted level but the scanner has been silent for a while - enable all states
			m_ValidSentenceTypes = ~0U;
		}

		if(m_AmbientScannerEnabled)
		{
			g_PoliceScanner.GenerateAmbientScanner(now);
		}

		if(m_TimeLastCheckedPosition + g_PositionUpdateTimeMs <= now)
		{
			Vector3 playerMovement = playerPos - m_LastPlayerPosition;

			if(playerMovement.Mag2() > g_PositionUpdateDeltaSq)
			{
				// split into 90 degree segments, with north spanning 315 -> 45
				m_PlayerHeading = CalculateAudioDirection(playerMovement);
			}
			else
			{
				m_PlayerHeading = AUD_DIR_UNKNOWN;
			}

			m_TimeLastCheckedPosition = now;
		}

		switch(m_ScannerState)
		{
			case AUD_SCANNER_IDLE:
				if(m_DelayingReportCancellationUntilLineEnd)
				{
					CancelAllReports();
					m_DelayingReportCancellationUntilLineEnd = false;
				}
				m_WantToCancelReport = false;

				if(m_DispatchInteractionType == SCANNER_CDIT_NONE && m_NoiseLoop)
				{
					StopScannerNoise();
				}

				if(m_ScannerMixScene)
				{
					m_ScannerMixScene->Stop();
					m_ScannerMixScene = NULL;
				}

				if(now > m_MaxTimeForNextCDIVehicleLine)
				{
					m_MinTimeForNextCDIVehicleLine = 0;
					m_MaxTimeForNextCDIVehicleLine = 0;
				}

				if(audRadioStation::IsPlayingEndCredits())
				{
					break;
				}

				if(m_MinTimeForNextCDIVehicleLine != 0 && now > m_MinTimeForNextCDIVehicleLine)
				{
					TriggerCopDispatchInteractionVehicleLine();
				}
				else if((now >= m_TimeAnyCDICanPlay && now >= m_TimeCDIsCanPlay[SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH]) BANK_ONLY(|| g_DebugCopDispatchInteraction))
				{
					if(m_HelisInvolvedInChase > 0 && 
						now - m_LastTimeSeenByNonHeliCop > (g_CopDispatchInteractionSettings ? g_CopDispatchInteractionSettings->RequestGuidanceDispatch.TimeNotSpottedToTrigger : 2000))
					{
						if(!m_HasTriggerGuidanceDispatch)
						{
							TriggerCopDispatchInteraction(SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH);
							SetShouldChooseNewNonHeliVoice();
							m_HasTriggerGuidanceDispatch = true;
						}
					}
					else
					{
						m_HasTriggerGuidanceDispatch = false;
					}
				}

				if(m_ForceDispIntResponseThrough ||
					(now > m_StateChangeTimeMs && !strStreamingEngine::GetIsLoadingPriorityObjects() && !g_ScriptAudioEntity.IsScriptedConversationOngoing()))
				{
					m_ForceDispIntResponseThrough = false;
					if(ChooseSentenceToPlay(m_PlayingSentenceIndex))
					{
						if(m_RequestedSentences[m_PlayingSentenceIndex].priority == AUD_SCANNER_PRIO_AMBIENT || 
							m_RequestedSentences[m_PlayingSentenceIndex].priority == AUD_SCANNER_PRIO_CRITICAL ||
							m_RequestedSentences[m_PlayingSentenceIndex].sentenceType == AUD_SCANNER_TRAIN || 
							m_RequestedSentences[m_PlayingSentenceIndex].sentenceType == AUD_SCANNER_SUSPECT_DOWN || 
							NetworkInterface::IsGameInProgress() || 
							(m_ValidSentenceTypes & SCANNER_STATE(m_RequestedSentences[m_PlayingSentenceIndex].sentenceType)) 
							BANK_ONLY(|| g_DebuggingScanner))
						{
							//Uncomment once the special category is made
							/*if(m_RequestedSentences[m_PlayingSentenceIndex].sentenceType == AUD_SCANNER_SUSPECT_DOWN)
								m_RequestedSentences[sentenceIndex].category = specialNoFadeCategory;*/
							if(CreateSentenceSounds(m_PlayingSentenceIndex) && PrepareSentence(m_PlayingSentenceIndex) != AUD_PREPARE_FAILED)
							{
								m_LastSentenceType = m_RequestedSentences[m_PlayingSentenceIndex].sentenceType;
								m_LastSentenceSubType = m_RequestedSentences[m_PlayingSentenceIndex].sentenceSubType;
								if(m_RequestedSentences[m_PlayingSentenceIndex].areaHash.IsValid())
								{
									m_LastSpokenAreaHash = m_RequestedSentences[m_PlayingSentenceIndex].areaHash;
								}
								m_ScannerState = AUD_SCANNER_PREPARING;
								m_ValidSentenceTypes = g_ScannerValidStateTransitions[m_RequestedSentences[m_PlayingSentenceIndex].sentenceType];
							}
							else
							{	
								CleanupSentenceSounds(m_PlayingSentenceIndex);
							}
						}
						else
						{
							m_RequestedSentences[m_PlayingSentenceIndex].inUse = false;
						}
					}
				}
				break;
			case AUD_SCANNER_PREPARING:
				{
					if(!m_RequestedSentences[m_PlayingSentenceIndex].inUse)
					{
						// not sure why this is happening...
						naErrorf("Preparing police scanner sentence that is not in use");
						CleanupSentenceSounds(m_PlayingSentenceIndex);
						g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
						m_ScannerState = AUD_SCANNER_IDLE;
					}
					else
					{
						audPrepareState state = PrepareSentence(m_PlayingSentenceIndex);
						if(state == AUD_PREPARE_FAILED)
						{
							CleanupSentenceSounds(m_PlayingSentenceIndex);
							g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
							m_ScannerState = AUD_SCANNER_IDLE;
						}
						else if(state == AUD_PREPARED)
						{
							audSoundInitParams params;
							params.AllowLoad = false;
							params.Category = m_RequestedSentences[m_PlayingSentenceIndex].category;
							params.EffectRoute = EFFECT_ROUTE_PADSPEAKER_1;
							if(m_RequestedSentences[m_PlayingSentenceIndex].isPositioned)
							{
								if(m_RequestedSentences[m_PlayingSentenceIndex].tracker)
								{
									params.Tracker = m_RequestedSentences[m_PlayingSentenceIndex].tracker;
								}
								else
								{
									params.Position = m_RequestedSentences[m_PlayingSentenceIndex].position;
								}
								params.EnvironmentGroup = m_RequestedSentences[m_PlayingSentenceIndex].occlusionGroup;
							}
							else
							{
								params.Pan = 0;
							}

							if(!m_NoiseLoop)
							{
								CreateSound_PersistentReference(m_RequestedSentences[m_PlayingSentenceIndex].backgroundSFXHash, &m_NoiseLoop, &params);
								if(m_NoiseLoop)
								{
									f32 vol = (m_RequestedSentences[m_PlayingSentenceIndex].useRadioVolOffset?audNorthAudioEngine::GetMusicVolume() * NA_RADIO_ENABLED_SWITCH(g_RadioAudioEntity.GetPlayerVehicleInsideFactor(),1.0f) * g_VolOffsetForRadio:0.f);
									if(m_RequestedSentences[m_PlayingSentenceIndex].priority == AUD_SCANNER_PRIO_AMBIENT && isPlayerClean)
									{
										vol += (RSG_ORBIS && g_AudioEngine.GetEnvironment().GetPadSpeakerEnabled() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) ? g_AmbientScannerPadSpeakerVolume : g_AmbientScannerVolume);
									}
									m_NoiseLoop->SetRequestedVolume(vol);

									m_NoiseLoop->PrepareAndPlay();		
								}
							}

							if(m_ScannerMixScene)
							{
								m_ScannerMixScene->Stop();
								m_ScannerMixScene = NULL;
							}
							if(m_RequestedSentences[m_PlayingSentenceIndex].priority != AUD_SCANNER_PRIO_AMBIENT)
							{
								DYNAMICMIXER.StartScene(g_ScannerSceneHash, &m_ScannerMixScene);
							}

							m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase = 0;
							if(naVerifyf(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound, "During Update currentPhrase on playing sentence has no sound"))
							{
								f32 vol = (m_RequestedSentences[m_PlayingSentenceIndex].useRadioVolOffset?audNorthAudioEngine::GetMusicVolume() * NA_RADIO_ENABLED_SWITCH(g_RadioAudioEntity.GetPlayerVehicleInsideFactor(),1.0f) * g_VolOffsetForRadio:0.f);
								if(m_RequestedSentences[m_PlayingSentenceIndex].priority == AUD_SCANNER_PRIO_AMBIENT)
								{
									vol += (RSG_ORBIS && g_AudioEngine.GetEnvironment().GetPadSpeakerEnabled() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) ? g_AmbientScannerPadSpeakerVolume : g_AmbientScannerVolume);
								}
								m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->SetRequestedVolume(vol);
								
								if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
								{
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->SetRequestedVolume(vol);
								}
								if(m_RequestedSentences[m_PlayingSentenceIndex].isPositioned)
								{
									if(!m_RequestedSentences[m_PlayingSentenceIndex].tracker)
									{
										m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->SetRequestedPosition(m_RequestedSentences[m_PlayingSentenceIndex].position);
										if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
										{
											m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->SetRequestedPosition(m_RequestedSentences[m_PlayingSentenceIndex].position);
										}
									}
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->GetRequestedSettings()->SetEnvironmentGroup(m_RequestedSentences[m_PlayingSentenceIndex].occlusionGroup);
									if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
									{
										m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->GetRequestedSettings()->SetEnvironmentGroup(m_RequestedSentences[m_PlayingSentenceIndex].occlusionGroup);
									}
								}
								else
								{
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->SetRequestedPan(0);
									if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
									{
										m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->SetRequestedPan(0);
									}
								}
								m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->Play();
								if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
								{
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->PrepareAndPlay(m_ScannerSlots[m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].slot]);
								}
								g_ScriptAudioEntity.ApplyPoliceScannerAudioScene();
								m_ScannerState = AUD_SCANNER_PLAYBACK;

								UpdateHistory();
							}
							else
							{
								CleanupSentenceSounds(m_PlayingSentenceIndex);
								g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
								m_ScannerState = AUD_SCANNER_IDLE;
							}
						}
					}
				}
				break;
			case AUD_SCANNER_PLAYBACK:
				{
					if(FindPlayerPed() && !FindPlayerPed()->GetIsInVehicle() && m_RequestedSentences[m_PlayingSentenceIndex].priority == AUD_SCANNER_PRIO_AMBIENT BANK_ONLY(&& !g_DebuggingScanner))
					{
						//make sure we set last player position before returning
						m_LastPlayerPosition = playerPos;
						CancelAnyPlayingReports(500);
						return;
					}
					if(m_NoiseLoop && m_RequestedSentences[m_PlayingSentenceIndex].isPositioned && !m_RequestedSentences[m_PlayingSentenceIndex].tracker)
					{
						m_NoiseLoop->SetRequestedPosition(m_RequestedSentences[m_PlayingSentenceIndex].position);
					}
					bool moveOn = false;
					if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound)
					{
						u32 playTime = m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
						s32 duration = m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->ComputeDurationMsIncludingStartOffsetAndPredelay(NULL);
				
						moveOn = (duration != kSoundLengthUnknown && (playTime > g_ScannerPreemptMs) && (duration-playTime < g_ScannerPreemptMs));
					}
					if(moveOn || !m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound)
					{
						if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].postDelay > 0)
						{
							m_StateChangeTimeMs = now + m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].postDelay;
							m_ScannerState = AUD_SCANNER_POSTDELAY;
						}
						else
						{
							// no delay
							m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase++;
							if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound && m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase < m_RequestedSentences[m_PlayingSentenceIndex].numPhrases)
							{
								f32 vol = (m_RequestedSentences[m_PlayingSentenceIndex].useRadioVolOffset?audNorthAudioEngine::GetMusicVolume() * NA_RADIO_ENABLED_SWITCH(g_RadioAudioEntity.GetPlayerVehicleInsideFactor(),1.0f) * g_VolOffsetForRadio:0.f);
								if(m_RequestedSentences[m_PlayingSentenceIndex].priority == AUD_SCANNER_PRIO_AMBIENT)
								{
									vol += (RSG_ORBIS && g_AudioEngine.GetEnvironment().GetPadSpeakerEnabled() REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) ? g_AmbientScannerPadSpeakerVolume : g_AmbientScannerVolume);
								}

								m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->SetRequestedVolume(vol);	
								if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
								{
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->SetRequestedVolume(vol);
								}
								if(m_RequestedSentences[m_PlayingSentenceIndex].isPositioned)
								{
									if(!m_RequestedSentences[m_PlayingSentenceIndex].tracker)
									{
										m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->SetRequestedPosition(m_RequestedSentences[m_PlayingSentenceIndex].position);
										if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
										{
											m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->SetRequestedPosition(m_RequestedSentences[m_PlayingSentenceIndex].position);
										}
									}
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->GetRequestedSettings()->SetEnvironmentGroup(m_RequestedSentences[m_PlayingSentenceIndex].occlusionGroup);
									if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
									{
										m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->GetRequestedSettings()->SetEnvironmentGroup(m_RequestedSentences[m_PlayingSentenceIndex].occlusionGroup);
									}
								}
								else
								{
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->SetRequestedPan(0);
									if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
									{
										m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->SetRequestedPan(0);
									}
								}
								m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].sound->Play();
								if(m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound)
								{
									m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].echoSound->PrepareAndPlay(m_ScannerSlots[m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].slot]);
								}
							}
							else
							{
								// finished...
								if(m_NoiseLoop)
								{
									m_NoiseLoop->StopAndForget();
								}
								g_ScriptAudioEntity.DeactivatePoliceScannerAudioScene();
								m_StateChangeTimeMs = now + g_MinTimeBetweenCriticalReports;
								m_RequestedSentences[m_PlayingSentenceIndex].inUse = false;
								m_RequestedSentences[m_PlayingSentenceIndex].shouldDuckRadio = false;
								CleanupSentenceSounds(m_PlayingSentenceIndex);
								m_ScannerState = AUD_SCANNER_IDLE;
								if(m_RequestedSentences[m_PlayingSentenceIndex].isPlayerCrime)
									audSpeechAudioEntity::PlayGetWantedLevel();
								
								if(m_WaitingOnDispIntResponse)
								{
									FinishCopDispatchInteraction();
								}

								if(m_RequestedSentences[m_PlayingSentenceIndex].playAcknowledge)
								{
									TriggerCopDispatchInteraction(SCANNER_CDIT_ACKNOWLEDGE_SITUATION_DISPATCH);
								}
								else if(m_RequestedSentences[m_PlayingSentenceIndex].playResponding)
								{
									TriggerCopDispatchInteraction(SCANNER_CDIT_UNIT_RESPONDING_DISPATCH);
								}

								m_PlayingSentenceIndex = ~0U;
							}
						}
					}
				}
				break;			
			
			case AUD_SCANNER_POSTDELAY:
				if(now > m_StateChangeTimeMs)
				{
					// clear post delay so the playback state moves on to the next phrase
					m_RequestedSentences[m_PlayingSentenceIndex].phrases[m_RequestedSentences[m_PlayingSentenceIndex].currentPhrase].postDelay = 0;
					g_ScriptAudioEntity.ApplyPoliceScannerAudioScene();
					m_ScannerState = AUD_SCANNER_PLAYBACK;				
				}
				break;
			case AUD_SCANNER_HANDLING_COP_DISPATCH_INTERACTION:
				HandleCopDispatchInteraction();
				break;
		}
	}
	m_LastPlayerPosition = playerPos;
}

void audScannerManager::HandleCopDispatchInteraction()
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	switch(m_DispatchInteractionType)
	{
	case SCANNER_CDIT_NONE:
		m_ScannerState = AUD_SCANNER_IDLE;
		break;
	case SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE:
	case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_FREEWAY:
	case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_METRO:
	case SCANNER_CDIT_REPORT_SUSPECT_IS_IN_CAR:
	case SCANNER_CDIT_REPORT_SUSPECT_IS_ON_FOOT:
	case SCANNER_CDIT_REPORT_SUSPECT_IS_ON_MOTORCYCLE:
	case SCANNER_CDIT_REPORT_SUSPECT_LEFT_FREEWAY:
		if(!m_DispIntCopHasSpoken)
		{
			if(!m_DispIntCopSpeechSound)
			{
				if(naVerifyf(m_ScannerSlots[0], "Missing m_ScannerSlot[0] (type %d)", m_DispatchInteractionType) && 
				   naVerifyf(m_ScannerSlots[0]->GetReferenceCount() == 0, "m_ScannerSlot[0] has a non-zero (%d) ref count (type %d)", m_ScannerSlots[0]->GetReferenceCount(), m_DispatchInteractionType))
				{
					if(!PlayCopToDispatchLine())
					{
						FinishCopDispatchInteraction();
					}
				}
				else
				{
					FinishCopDispatchInteraction();
				}
			}
		}
		else if(!m_DispInDispHasSpoken)
		{
			if(!m_DispIntCopSpeechSound)
			{
				StopScannerNoise();

				if(naVerifyf(m_ScannerSlots[0], "Missing m_ScannerSlot[0] (type %d)", m_DispatchInteractionType) && 
				   naVerifyf(m_ScannerSlots[0]->GetReferenceCount() == 0, "m_ScannerSlot[0] has a non-zero (%d) ref count (type %d)", m_ScannerSlots[0]->GetReferenceCount(), m_DispatchInteractionType))
				{
					PlayDispatchAcknowledgementLine();
				}
				else
				{
					FinishCopDispatchInteraction();
				}
			}
		}
		else if(!m_DispIntCopSpeechSound && currentTime > m_TimeToPlayDispIntResponse)
		{
			StopScannerNoise();

			if(m_TimeToPlayDispIntResponse == 0)
			{
				m_TimeToPlayDispIntResponse = currentTime + 5000;
				if(g_CopDispatchInteractionSettings)
				{
					s32 predelay = g_CopDispatchInteractionSettings->ScannerPredelay;
					s32 predelayVariance = Max(0, g_CopDispatchInteractionSettings->ScannerPredelayVariance);
					m_TimeToPlayDispIntResponse = currentTime + audEngineUtil::GetRandomNumberInRange(predelay - predelayVariance, predelay + predelayVariance);
				}
			}
			else
			{
				if(m_DispatchInteractionType == SCANNER_CDIT_REQUEST_BACKUP)
				{
					g_PoliceScanner.RequestAssistanceCritical();
				}
				else	
				{
					g_PoliceScanner.ReportPoliceRefocusCritical();
				}
				FinishCopDispatchInteraction();
				m_ForceDispIntResponseThrough = true;
			}
		}
		break;
	case SCANNER_CDIT_PLAYER_SPOTTED:
	case SCANNER_CDIT_ACKNOWLEDGE_SITUATION_DISPATCH:
	case SCANNER_CDIT_UNIT_RESPONDING_DISPATCH:
	case SCANNER_CDIT_REQUEST_BACKUP:
		if(!m_DispIntCopHasSpoken)
		{
			if(!m_DispIntCopSpeechSound)
			{
				if(m_TimeToPlayDispIntLine == 0)
				{
					if(g_CopDispatchInteractionSettings)
					{
						u32 preDelay = 0;
						preDelay = audEngineUtil::GetRandomNumberInRange(g_CopDispatchInteractionSettings->AcknowledgeSituation.MinTimeAfterCrime,
										g_CopDispatchInteractionSettings->AcknowledgeSituation.MaxTimeAfterCrime);
						m_TimeToPlayDispIntLine = currentTime + preDelay;
						
					}
					else
					{
						m_TimeToPlayDispIntLine = currentTime + 2000;
					}
				}
				else if(currentTime > m_TimeToPlayDispIntLine)
				{
					if(naVerifyf(m_ScannerSlots[0], "Missing m_ScannerSlot[0] (type %d)", m_DispatchInteractionType) && 
					   naVerifyf(m_ScannerSlots[0]->GetReferenceCount() == 0, "m_ScannerSlot[0] has a non-zero (%d) ref count (type %d)", m_ScannerSlots[0]->GetReferenceCount(), m_DispatchInteractionType))
					{
						if(!PlayCopToDispatchLine())
						{
							FinishCopDispatchInteraction();
						}
					}
					else
					{
						FinishCopDispatchInteraction();
					}
				}
			}
		}
		else if(!m_DispIntCopSpeechSound)
		{
			FinishCopDispatchInteraction();
		}
		break;
	case SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH:
		if(!m_DispIntCopHasSpoken)
		{
			if(!m_DispIntCopSpeechSound)
			{
				if(naVerifyf(m_ScannerSlots[0], "Missing m_ScannerSlot[0] (type %d)", m_DispatchInteractionType) && 
				   naVerifyf(m_ScannerSlots[0]->GetReferenceCount() == 0, "m_ScannerSlot[0] has a non-zero (%d) ref count (type %d)", m_ScannerSlots[0]->GetReferenceCount(), m_DispatchInteractionType))
				{
					if(!PlayCopToDispatchLine())
					{
						FinishCopDispatchInteraction();
					}
				}
				else
				{
					FinishCopDispatchInteraction();
				}
			}
		}
		else if(!m_DispIntHeliHasSpoken)
		{
			if(!m_DispIntCopSpeechSound)
			{
				StopScannerNoise();
				
				if(naVerifyf(m_ScannerSlots[0], "Missing m_ScannerSlot[0] (type %d)", m_DispatchInteractionType) && 
				   naVerifyf(m_ScannerSlots[0]->GetReferenceCount() == 0, "m_ScannerSlot[0] has a non-zero (%d) ref count (type %d)", m_ScannerSlots[0]->GetReferenceCount(), m_DispatchInteractionType))
				{
					PlayHeliGuidanceLine(currentTime);
				}
				else
				{
					FinishCopDispatchInteraction();
				}
			}
		}
		else if(!m_DispIntHeliMegaphoneHasSpoken)
		{
			if(!m_DispIntCopSpeechSound)
			{
				StopScannerNoise();

				f32 megaphoneLineProb = g_CopDispatchInteractionSettings ? g_CopDispatchInteractionSettings->RequestGuidanceDispatch.ProbabilityOfMegaphoneLine : 0.5f;
				if(m_DispIntHeliMegaphoneShouldSpeak && audEngineUtil::ResolveProbability(megaphoneLineProb))
				{
					for(s32 i = 0; i < COrderManager::GetInstance().GetMaxCount(); i++ )
					{
						//Ensure the order is valid.
						COrder* pOrder = COrderManager::GetInstance().GetOrder(i);

						if (pOrder && pOrder->GetDispatchType() == DT_PoliceHelicopter && pOrder->GetEntity() && 
							pOrder->GetEntity()->GetType() == ENTITY_TYPE_PED)
						{
							CPed* ped = static_cast<CPed*>(pOrder->GetEntity());
							f32 distanceSqToNearestListener = g_AudioEngine.GetEnvironment().ComputeSqdDistanceRelativeToVolumeListenerV(ped->GetMatrix().d()).Getf();
							if(distanceSqToNearestListener < (g_MaxDistanceToPlayCopSpeech * g_MaxDistanceToPlayCopSpeech))
							{
								if(ped && ped->NewSay("SPOT_SUSPECT_CHOPPER_MEGAPHONE"))
									break;
							}
						}
					}
				}
				m_DispIntHeliMegaphoneHasSpoken = true;
			}
		}
		else if(!m_DispIntCopSpeechSound)
		{
			FinishCopDispatchInteraction();
		}
		break;
	case SCANNER_CDIT_HELI_APPROACHING_DISPATCH:
	case SCANNER_CDIT_HELI_MAYDAY_DISPATCH:
		if(!m_DispIntCopHasSpoken)
		{
			PlayCopToDispatchLine();
			m_DispIntCopHasSpoken = true;
		}
		else if(!m_DispIntCopSpeechSound)
		{
			FinishCopDispatchInteraction();
		}
		break;
	default:
		FinishCopDispatchInteraction();
		break;
	}
}

void audScannerManager::TriggerCopDispatchInteraction(ScannerCopDispatchInteractionType interaction)
{
	m_DispatchInteractionTypeTriggered = interaction;
}

bool audScannerManager::ReallyTriggerCopDispatchInteraction(ScannerCopDispatchInteractionType interaction)
{
	if(!g_EnableCopDispatchInteraction)
		return false;

	if(m_ScannerState != AUD_SCANNER_IDLE || m_DispatchInteractionType != SCANNER_CDIT_NONE)
	{
		if(interaction == SCANNER_CDIT_HELI_MAYDAY_DISPATCH && m_DispatchInteractionType == SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH)
		{
			FinishCopDispatchInteraction();
		}
		m_HasTriggerGuidanceDispatch = false;
		return false;
	}

	CWanted* playerWanted = FindPlayerPed() ? FindPlayerPed()->GetPlayerWanted() : NULL;
	if(BANK_ONLY(!g_DebugCopDispatchInteraction &&)
	  playerWanted && playerWanted->GetWantedLevel() == WANTED_CLEAN)
	{
		m_HasTriggerGuidanceDispatch = false;
		return false;
	}

	int pedCount;
	int vehicleCount;
	Vector3 pos = VEC3V_TO_VECTOR3(FindPlayerPed()->GetMatrix().d());
	Vector3 vMinCoors = pos + Vector3(-100.0f, -100.0f, -100.0f);
	Vector3 vMaxCoors = pos + Vector3(100.0f, 100.0f, 100.0f);
	if(interaction != SCANNER_CDIT_HELI_APPROACHING_DISPATCH &&
		interaction != SCANNER_CDIT_HELI_MAYDAY_DISPATCH &&
		interaction != SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH &&
		interaction != SCANNER_CDIT_ACKNOWLEDGE_SITUATION_DISPATCH &&
		interaction != SCANNER_CDIT_UNIT_RESPONDING_DISPATCH &&
		!CPedPopulation::GetCopsInArea(vMinCoors, vMaxCoors, NULL, 10, pedCount, NULL, 0, vehicleCount) BANK_ONLY(&& !g_DebugCopDispatchInteraction) )
	{
		m_HasTriggerGuidanceDispatch = false;
		return false;
	}

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	bool isOkayTimeToPlay = currentTime >= m_TimeAnyCDICanPlay && (interaction == SCANNER_CDIT_HELI_MAYDAY_DISPATCH || currentTime >= m_TimeCDIsCanPlay[interaction]);
	if(!( isOkayTimeToPlay BANK_ONLY(|| g_DebugCopDispatchInteraction) ))
	{
		m_HasTriggerGuidanceDispatch = false;
		return false;
	}

	if(currentTime - m_TimeLastCDIFinished > (g_CopDispatchInteractionSettings ? g_CopDispatchInteractionSettings->TimePassedSinceLastLineToForceVoiceChange : 60000))
	{
		m_ShouldChooseNewNonHeliVoice = true;
	}

	if(m_CopShotTally >= (g_CopDispatchInteractionSettings ? g_CopDispatchInteractionSettings->NumCopsKilledToForceVoiceChange : 4))
	{
		m_ShouldChooseNewNonHeliVoice = true;
	}

	bool dontNeedCop = false;
	switch(interaction)
	{
	case SCANNER_CDIT_HELI_APPROACHING_DISPATCH:
		m_ShouldChooseNewHeliVoice = true;
	case SCANNER_CDIT_HELI_MAYDAY_DISPATCH:
		dontNeedCop = true;
	case SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH:
		if(!FindPlayerPed() || !FindPlayerPed()->GetPlayerWanted() || FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() < WANTED_LEVEL3)
		{
			m_HasTriggerGuidanceDispatch = false;
			return false;
		}

		if(m_ShouldChooseNewHeliVoice)
		{
			m_DispatchInteractionHeliCopVoicehash = GetVoiceHashForHeliCopSpeech();
			if(m_DispatchInteractionHeliCopVoicehash == 0)
			{
				m_HasTriggerGuidanceDispatch = false;
				return false;
			}
			m_ShouldChooseNewHeliVoice = false;
			m_ShouldChooseNewNonHeliVoice = true;
		}
		if(m_ShouldChooseNewNonHeliVoice)
		{
			m_DispatchInteractionCopVoicehash = GetVoiceHashForCopSpeech();
			if(m_DispatchInteractionCopVoicehash == 0 && !dontNeedCop)
			{
				m_HasTriggerGuidanceDispatch = false;
				return false;
			}
			m_ShouldChooseNewNonHeliVoice = false;
		}
		m_DispatchInteractionType = interaction;
		m_ScannerState = AUD_SCANNER_HANDLING_COP_DISPATCH_INTERACTION;
		break;
	case SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE:
		if(!FindPlayerPed() || !FindPlayerPed()->GetVehiclePedInside() || FindPlayerPed()->GetVehiclePedInside()->GetVelocity().Mag2() > 100.0f)
		{
			m_HasTriggerGuidanceDispatch = false;
			return false;
		}
	case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_FREEWAY:
	case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_METRO:
	case SCANNER_CDIT_REPORT_SUSPECT_LEFT_FREEWAY:
	case SCANNER_CDIT_REPORT_SUSPECT_IS_IN_CAR:
	case SCANNER_CDIT_REPORT_SUSPECT_IS_ON_FOOT:
	case SCANNER_CDIT_REPORT_SUSPECT_IS_ON_MOTORCYCLE:
		//Don't do any of these if there isn't a cop to see it
		if(currentTime - m_LastTimeSeenByNonHeliCop > 250)
		{
			m_HasTriggerGuidanceDispatch = false;
			return false;
		}
		if(m_ShouldChooseNewNonHeliVoice)
		{
			m_DispatchInteractionCopVoicehash = GetVoiceHashForCopSpeech();
			if(m_DispatchInteractionCopVoicehash == 0 && !dontNeedCop)
			{
				m_HasTriggerGuidanceDispatch = false;
				return false;
			}
			m_ShouldChooseNewNonHeliVoice = false;
		}
		m_DispatchInteractionType = interaction;
		m_ScannerState = AUD_SCANNER_HANDLING_COP_DISPATCH_INTERACTION;
		break;
	case SCANNER_CDIT_UNIT_RESPONDING_DISPATCH:
	case SCANNER_CDIT_ACKNOWLEDGE_SITUATION_DISPATCH:
		//Don't do any of these if there are cops already witnessing
		if(currentTime - m_LastTimePlayerWasClean > 20000)
		{
			m_HasTriggerGuidanceDispatch = false;
			return false;
		}
	case SCANNER_CDIT_REQUEST_BACKUP:
		if(m_ShouldChooseNewNonHeliVoice)
		{
			m_DispatchInteractionCopVoicehash = GetVoiceHashForCopSpeech();
			if(m_DispatchInteractionCopVoicehash == 0 && !dontNeedCop)
			{
				m_HasTriggerGuidanceDispatch = false;
				return false;
			}
			m_ShouldChooseNewNonHeliVoice = false;
		}
		m_DispatchInteractionType = interaction;
		m_ScannerState = AUD_SCANNER_HANDLING_COP_DISPATCH_INTERACTION;
		break;
	case SCANNER_CDIT_PLAYER_SPOTTED:
		m_DispatchInteractionCopVoicehash = GetVoiceHashForCopSpottedSpeech();
		if(m_DispatchInteractionCopVoicehash == 0)
		{
			m_HasTriggerGuidanceDispatch = false;
			return false;
		}
		m_ShouldChooseNewNonHeliVoice = true;
		m_DispatchInteractionType = interaction;
		m_ScannerState = AUD_SCANNER_HANDLING_COP_DISPATCH_INTERACTION;
		break;
	default: 
		break;
	}
	return true;
}


void audScannerManager::SetTimeToPlayCDIVehicleLine()
{
	if(!FindPlayerPed() || !FindPlayerPed()->GetPlayerWanted() || FindPlayerPed()->GetPlayerWanted()->GetWantedLevel()==WANTED_CLEAN)
		return;

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	s32 maxTimeUntilVehicleLine = g_CopDispatchInteractionSettings ? g_CopDispatchInteractionSettings->MinTimeBetweenSpottedAndVehicleLinePlays : 1500;
	s32 maxTimeUntilVehicleLineVar = Max(0, g_CopDispatchInteractionSettings ? g_CopDispatchInteractionSettings->MinTimeBetweenSpottedAndVehicleLinePlaysVariance : 500);
	m_MinTimeForNextCDIVehicleLine = g_AudioEngine.GetTimeInMilliseconds() + 
		audEngineUtil::GetRandomNumberInRange(Max(0, maxTimeUntilVehicleLine - maxTimeUntilVehicleLineVar), maxTimeUntilVehicleLine + maxTimeUntilVehicleLineVar);
	m_MaxTimeForNextCDIVehicleLine = currentTime + (g_CopDispatchInteractionSettings ? g_CopDispatchInteractionSettings->MaxTimeAfterVehicleChangeSpottedLineCanPlay : 5000);
}

void audScannerManager::TriggerCopDispatchInteractionVehicleLine()
{
	CPed* player = FindPlayerPed();

	if(!player)
	{
		m_MinTimeForNextCDIVehicleLine = 0;
		m_MaxTimeForNextCDIVehicleLine = 0;
		return;
	}

	if(player->GetIsInVehicle())
	{
		CVehicle* vehicle = player->GetMyVehicle();
		if(!vehicle)
		{
			m_MinTimeForNextCDIVehicleLine = 0;
			m_MaxTimeForNextCDIVehicleLine = 0;
			return;
		}

		switch(vehicle->GetVehicleType())
		{
		case VEHICLE_TYPE_CAR:
			if(vehicle->GetVehicleAudioEntity() 
				&& vehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR 
				&& static_cast<audCarAudioEntity*>(vehicle->GetVehicleAudioEntity())->CanBeDescribedAsACar())
			{
				TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_IS_IN_CAR);
			}
			break;
		case VEHICLE_TYPE_BIKE:
			TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_IS_ON_MOTORCYCLE);
			break;
		default:
			break;
		}
	}
	else if(player->GetPedAudioEntity() && !player->GetPedAudioEntity()->HasBeenOnTrainRecently())
	{
		TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_IS_ON_FOOT);
	}

	m_MinTimeForNextCDIVehicleLine = 0;
	m_MaxTimeForNextCDIVehicleLine = 0;
}

void audScannerManager::FinishCopDispatchInteraction()
{
	if(m_DispatchInteractionType == SCANNER_CDIT_NONE)
		return;

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
	if(g_CopDispatchInteractionSettings)
	{
		s32 minTimeBetweenAny = g_CopDispatchInteractionSettings->MinTimeBetweenInteractions;
		s32 minTimeBetweenAnyVar = Max(0, g_CopDispatchInteractionSettings->MinTimeBetweenInteractionsVariance);
		m_TimeAnyCDICanPlay = currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetweenAny - minTimeBetweenAnyVar, minTimeBetweenAny + minTimeBetweenAnyVar);
		m_TimeCDIsCanPlay[m_DispatchInteractionType] = GetNextPlayTimeForCDI(m_DispatchInteractionType, currentTime);
	}
	else
	{
		m_TimeAnyCDICanPlay = currentTime + 60000;
		m_TimeCDIsCanPlay[m_DispatchInteractionType] = currentTime + 60000;;
	}

	if(m_DispatchInteractionType == SCANNER_CDIT_HELI_MAYDAY_DISPATCH)
		SetShouldChooseNewHeliVoice();

	m_TimeLastCDIFinished = currentTime;

	StopScannerNoise();
	CancelCopDispatchInteraction();
}

void audScannerManager::CancelCopDispatchInteraction()
{
	switch(m_DispatchInteractionType)
	{
		case SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE:
		case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_FREEWAY:
		case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_METRO:
		case SCANNER_CDIT_REPORT_SUSPECT_LEFT_FREEWAY:
		case SCANNER_CDIT_REQUEST_BACKUP:
			m_MinTimeForNextCDIVehicleLine = 0;
			m_MaxTimeForNextCDIVehicleLine = 0;
			break;
		case SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH:
			if(!m_LastHeliGuidanceLineWasNoVisual)
			{
				m_MinTimeForNextCDIVehicleLine = 0;
				m_MaxTimeForNextCDIVehicleLine = 0;
			}
			break;
		case SCANNER_CDIT_NONE:
			return;
		default:
			break;
	}
	
	m_DispatchInteractionType = SCANNER_CDIT_NONE;
	m_DispIntCopHasSpoken = false;
	m_DispInDispHasSpoken = false;
	m_WaitingOnDispIntResponse = false;
	m_DispIntHeliHasSpoken = false;
	m_DispIntHeliMegaphoneHasSpoken = false;
	m_WaitingOnDispIntResponse = false;
	m_LastHeliGuidanceLineWasNoVisual = false;
	m_TimeToPlayDispIntLine = 0;
	m_TimeToPlayDispIntResponse = 0;
	CleanupSentenceSounds(m_PlayingSentenceIndex);
	m_ScannerState = AUD_SCANNER_IDLE;

	if(m_DispIntCopSpeechSound)
		m_DispIntCopSpeechSound->StopAndForget();
}

u32 audScannerManager::GetNextPlayTimeForCDI(ScannerCopDispatchInteractionType m_DispatchInteractionType, u32 currentTime)
{
	if(!g_CopDispatchInteractionSettings)
		return 60000;

	s32 minTimeBetween = 0;
	s32 minTimeBetweenVar = 0;

	switch(m_DispatchInteractionType)
	{
	case SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectCrashedVehicle.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectCrashedVehicle.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_FREEWAY:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectEnteredFreeway.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectEnteredFreeway.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REPORT_SUSPECT_ENTERED_METRO:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectEnteredMetro.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectEnteredMetro.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REPORT_SUSPECT_IS_IN_CAR:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectIsInCar.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectIsInCar.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REPORT_SUSPECT_IS_ON_FOOT:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectIsOnFoot.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectIsOnFoot.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REPORT_SUSPECT_IS_ON_MOTORCYCLE:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectIsOnMotorcycle.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectIsOnMotorcycle.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REPORT_SUSPECT_LEFT_FREEWAY:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectLeftFreeway.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectLeftFreeway.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REQUEST_BACKUP:
		minTimeBetween = g_CopDispatchInteractionSettings->RequestBackup.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->RequestBackup.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH:
		minTimeBetween = g_CopDispatchInteractionSettings->RequestGuidanceDispatch.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->RequestGuidanceDispatch.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_HELI_MAYDAY_DISPATCH:
		minTimeBetween = g_CopDispatchInteractionSettings->HeliMaydayDispatch.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->HeliMaydayDispatch.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_SHOT_AT_HELI_MEGAPHONE:
		minTimeBetween = g_CopDispatchInteractionSettings->ShotAtHeli.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->ShotAtHeli.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_HELI_APPROACHING_DISPATCH:
		minTimeBetween = g_CopDispatchInteractionSettings->HeliApproachingDispatch.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->HeliApproachingDispatch.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	case SCANNER_CDIT_PLAYER_SPOTTED:
		minTimeBetween = g_CopDispatchInteractionSettings->SuspectIsOnFoot.MinTimeBetween;
		minTimeBetweenVar = Max(0, g_CopDispatchInteractionSettings->SuspectIsOnFoot.MinTimeBetweenVariance);
		return currentTime + audEngineUtil::GetRandomNumberInRange(minTimeBetween - minTimeBetweenVar, minTimeBetween + minTimeBetweenVar);
	default:
		return 60000;
	}
}

bool audScannerManager::CanDoCopSpottingLine()
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
	return currentTime >= m_TimeAnyCDICanPlay && currentTime >= m_TimeCDIsCanPlay[SCANNER_CDIT_PLAYER_SPOTTED];
}

u32 audScannerManager::GetVoiceHashForCopSpeech()
{
	m_CopShotTally = 0;

	char voice[32];
	u32 startingVoice = audEngineUtil::GetRandomInteger() % AUD_NUM_CDI_VOICES;
	for(int i=0; i<AUD_NUM_CDI_VOICES; i++)
	{
		u32 voiceNum = ((startingVoice + i) % AUD_NUM_CDI_VOICES) + 1;
		formatf(voice, "CDI_VOICE_%.2i", voiceNum);
		u32 voiceHash = atStringHash(voice);
		if(m_DispatchInteractionHeliCopVoicehash == 0 || m_DispatchInteractionHeliCopVoicehash != voiceHash)
		{
			return voiceHash;
		}
	}
	return 0;
}

u32 audScannerManager::GetVoiceHashForCopSpottedSpeech()
{
	m_CopShotTally = 0;

	char voice[32];
	u32 startingVoice = (audEngineUtil::GetRandomInteger() % 5);
	for(int i=0; i<5; i++)
	{
		u32 voiceNum = ((startingVoice + i) % 5) + AUD_NUM_CDI_VOICES + 1;
		formatf(voice, "CDI_VOICE_%.2i", voiceNum);
		u32 voiceHash = atStringHash(voice);
		if(m_DispatchInteractionHeliCopVoicehash == 0 || m_DispatchInteractionHeliCopVoicehash != voiceHash)
		{
			return voiceHash;
		}
	}
	return 0;
}

u32 audScannerManager::GetVoiceHashForHeliCopSpeech()
{
	char voice[32];
	u32 startingVoice = audEngineUtil::GetRandomInteger() % AUD_NUM_CDI_VOICES;
	for(int i=0; i<AUD_NUM_CDI_VOICES; i++)
	{
		u32 voiceNum = ((startingVoice + i) % AUD_NUM_CDI_VOICES) + 1;
		formatf(voice, "CDI_VOICE_%.2i", voiceNum);
		u32 voiceHash = atStringHash(voice);
		if(g_ScriptAudioEntity.GetSpeechAudioEntity().GetRandomVariation(voiceHash, m_DispIntCopSpeechContextPHash[SCANNER_CDIT_REQUEST_GUIDANCE_DISPATCH], NULL, NULL) > 0)
		{
			return voiceHash;
		}
	}
	return 0;
}

bool audScannerManager::PlayCopToDispatchLine()
{
	audSoundInitParams initParams;
	initParams.Predelay = 0;
	if(g_CopDispatchInteractionSettings)
	{
		s32 predelay = g_CopDispatchInteractionSettings->FirstLinePredelay;
		s32 predelayVariance = Max(0, g_CopDispatchInteractionSettings->FirstLinePredelayVariance);
		initParams.Predelay = audEngineUtil::GetRandomNumberInRange(predelay - predelayVariance, predelay + predelayVariance);
	}
	initParams.EffectRoute = EFFECT_ROUTE_PADSPEAKER_1;
	CreateSound_PersistentReference(g_DispIntSpeechSoundHash, (audSound**)&m_DispIntCopSpeechSound, &initParams);

	if (!m_DispIntCopSpeechSound)
	{
		return false;
	}

	//may as well use one of the global speech entities for this
	u32 voiceHash = m_DispatchInteractionCopVoicehash;
	if(m_DispatchInteractionType == SCANNER_CDIT_HELI_APPROACHING_DISPATCH || m_DispatchInteractionType == SCANNER_CDIT_HELI_MAYDAY_DISPATCH)
	{
		voiceHash = m_DispatchInteractionHeliCopVoicehash;
	}

	s32 variation = g_ScriptAudioEntity.GetSpeechAudioEntity().GetRandomVariation(voiceHash, m_DispIntCopSpeechContextPHash[m_DispatchInteractionType], NULL, NULL);
	if(variation <= 0)
	{
		FinishCopDispatchInteraction();
		return false;
	}

	m_DispIntCopSpeechSound->InitSpeech(voiceHash, m_DispIntCopSpeechContextPHash[m_DispatchInteractionType], variation);
	m_DispIntCopSpeechSound->PrepareAndPlay(m_ScannerSlots[0], true, -1);
	m_DispIntCopHasSpoken = true;

	StartScannerNoise(false, initParams.Predelay);

	return true;
}

void audScannerManager::PlayDispatchAcknowledgementLine()
{
	Sound::tSpeakerMask speakerMask; 
	speakerMask.Value = 0; 
	speakerMask.BitFields.FrontCenter = true;

	audSoundInitParams initParams;
	initParams.Category = g_PoliceScannerCategory;
	initParams.SpeakerMask = speakerMask.Value;
	initParams.Predelay = 0;
	initParams.EffectRoute = EFFECT_ROUTE_PADSPEAKER_1;
	if(g_CopDispatchInteractionSettings)
	{
		s32 predelay = g_CopDispatchInteractionSettings->SecondLinePredelay;
		s32 predelayVariance = Max(0, g_CopDispatchInteractionSettings->SecondLinePredelayVariance);
		initParams.Predelay = audEngineUtil::GetRandomNumberInRange(predelay - predelayVariance, predelay + predelayVariance);
	}
	CreateSound_PersistentReference(GetScannerHashForVoice(ATPARTIALSTRINGHASH("POLICE_SCANNER_DISPATCH_CUSTOM_FIELD_REPORT_RESPONSE", 0x785AD656),1),
		(audSound**)&m_DispIntCopSpeechSound, &initParams);
	
	if(m_DispIntCopSpeechSound)
		m_DispIntCopSpeechSound->PrepareAndPlay(m_ScannerSlots[0], true, -1);
	else
	{
		FinishCopDispatchInteraction();
		return;
	}

	m_DispInDispHasSpoken = true;
	StartScannerNoise(true, initParams.Predelay);
}

void audScannerManager::PlayHeliGuidanceLine(u32 currentTime)
{
	audSoundInitParams initParams;
	initParams.Predelay = 0;
	initParams.EffectRoute = EFFECT_ROUTE_PADSPEAKER_1;
	if(g_CopDispatchInteractionSettings)
	{
		s32 predelay = g_CopDispatchInteractionSettings->FirstLinePredelay;
		s32 predelayVariance = Max(0, g_CopDispatchInteractionSettings->FirstLinePredelayVariance);
		initParams.Predelay = audEngineUtil::GetRandomNumberInRange(Max(0, predelay - predelayVariance), predelay + predelayVariance);
	}
	CreateSound_PersistentReference(g_DispIntSpeechSoundHash, (audSound**)&m_DispIntCopSpeechSound, &initParams);

	if (!m_DispIntCopSpeechSound)
	{
		return;
	}

	m_LastHeliGuidanceLineWasNoVisual = false;
	u32 heliResponseContextPHash = 0;
	if(currentTime - m_LastTimeSeenByHeliCop > 150 || !FindPlayerPed())
	{
		m_LastHeliGuidanceLineWasNoVisual = true;
		heliResponseContextPHash = atPartialStringHash("CDI_HELI_NO_VISUAL_DISPATCH");
	}
	else if(FindPlayerPed()->GetVehiclePedInside() || (FindPlayerPed()->GetPedAudioEntity() && FindPlayerPed()->GetPedAudioEntity()->HasBeenOnTrainRecently()))
	{
		m_DispIntHeliMegaphoneShouldSpeak = true;

		Vector3 playerMovement = CGameWorld::FindLocalPlayerCoors() - m_LastPlayerPosition;
		m_PlayerHeading = CalculateAudioDirection(playerMovement);
		if(m_PlayerHeading >= g_AudioPoliceScannerDir.size())
		{
			m_LastHeliGuidanceLineWasNoVisual = true;
			m_DispIntHeliMegaphoneShouldSpeak = false;
			heliResponseContextPHash = atPartialStringHash("CDI_HELI_NO_VISUAL_DISPATCH");
		}
		else
		{
			switch(m_PlayerHeading)
			{
			case AUD_DIR_NORTH:
				heliResponseContextPHash = atPartialStringHash("CDI_HELI_VISUAL_HEADING_NORTH_DISPATCH");
				break;
			case AUD_DIR_EAST:
				heliResponseContextPHash = atPartialStringHash("CDI_HELI_VISUAL_HEADING_EAST_DISPATCH");
				break;
			case AUD_DIR_SOUTH:
				heliResponseContextPHash = atPartialStringHash("CDI_HELI_VISUAL_HEADING_SOUTH_DISPATCH");
				break;
			case AUD_DIR_WEST:
				heliResponseContextPHash = atPartialStringHash("CDI_HELI_VISUAL_HEADING_WEST_DISPATCH");
				break;
			case AUD_DIR_UNKNOWN:
			default:
				m_LastHeliGuidanceLineWasNoVisual = true;
				m_DispIntHeliMegaphoneShouldSpeak = false;
				heliResponseContextPHash = atPartialStringHash("CDI_HELI_NO_VISUAL_DISPATCH");
			}
		}
	}
	else
	{
		m_DispIntHeliMegaphoneShouldSpeak = true;
		heliResponseContextPHash = atPartialStringHash("CDI_HELI_VISUAL_ON_FOOT_DISPATCH");
	}
	

	//may as well use one of the global speech entities for this
	s32 variation = g_ScriptAudioEntity.GetSpeechAudioEntity().GetRandomVariation(m_DispatchInteractionHeliCopVoicehash, heliResponseContextPHash, NULL, NULL);
	if(variation <= 0)
	{
		FinishCopDispatchInteraction();
		return;
	}

	m_DispIntCopSpeechSound->InitSpeech(m_DispatchInteractionHeliCopVoicehash, heliResponseContextPHash, variation);
	m_DispIntCopSpeechSound->PrepareAndPlay(m_ScannerSlots[0], true, -1);
	m_DispIntHeliHasSpoken = true;

	StartScannerNoise(false, initParams.Predelay);

	return;
}

void audScannerManager::SetPlayerSpottedTime(CPed* pSpotter)
{
	if(!pSpotter || !pSpotter->GetMyVehicle())
		return;

	if(pSpotter->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_HELI)
		m_LastTimeSeenByHeliCop =  g_AudioEngine.GetTimeInMilliseconds();
	else
		m_LastTimeSeenByNonHeliCop =  g_AudioEngine.GetTimeInMilliseconds();
}


void audScannerManager::IncrementHelisInChase()
{
	m_HelisInvolvedInChase++;
}

void audScannerManager::DecrementHelisInChase()
{
	m_HelisInvolvedInChase--;
	if(m_HelisInvolvedInChase < 0)
		m_HelisInvolvedInChase = 0;
}

void audScannerManager::StartScannerNoise(bool isDispatch, s32 predelay)
{
	if(!m_NoiseLoop)
	{
		Sound::tSpeakerMask speakerMask; 
		speakerMask.Value = 0; 
		speakerMask.BitFields.FrontCenter = true;

		audSoundInitParams initParams;
		initParams.Category = g_PoliceScannerCategory;
		initParams.SpeakerMask = speakerMask.Value;
		initParams.Predelay = predelay;
		initParams.EffectRoute = EFFECT_ROUTE_PADSPEAKER_1;

		if(isDispatch)
		{
			CreateSound_PersistentReference(ATSTRINGHASH("POSITIONED_NOISE_LOOP", 0x01083f52b), &m_NoiseLoop, &initParams);
		}
		else
		{
			CreateSound_PersistentReference(ATSTRINGHASH("POLICE_SCANNER_DISPATCH_NOISE_LOOP", 0xE566F98), &m_NoiseLoop, &initParams);
		}
		
		if(m_NoiseLoop)
		{
			f32 vol = audNorthAudioEngine::GetMusicVolume() * NA_RADIO_ENABLED_SWITCH(g_RadioAudioEntity.GetPlayerVehicleInsideFactor(),1.0f) * g_VolOffsetForRadio;
			m_NoiseLoop->SetRequestedVolume(vol);

			m_NoiseLoop->PrepareAndPlay();		
		}

		if(m_ScannerMixScene)
		{
			m_ScannerMixScene->Stop();
			m_ScannerMixScene = NULL;
		}
		DYNAMICMIXER.StartScene(g_DispatchScannerSceneHash, &m_ScannerMixScene);
	}
}

void audScannerManager::StopScannerNoise()
{
	if(m_NoiseLoop)
	{	
		// set volume to silence so that we don't hear the OnStop sound as the screen fades back up
		m_NoiseLoop->SetRequestedVolume(-100.f);
		m_NoiseLoop->StopAndForget();
	}

	if(m_ScannerMixScene)
	{
		m_ScannerMixScene->Stop();
		m_ScannerMixScene = NULL;
	}
}

bool audScannerManager::CanPlayRefuellingReport()
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
	return currentTime - m_LastTimeSeenByHeliCop < 100 && currentTime - m_LastTimeSeenByNonHeliCop < 100;
}

void audScannerManager::UpdateHistory()
{
	if(m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.audioEntity)
	{
		if(m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.hasDescribedColour)
		{
			m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.audioEntity->m_HasScannerDescribedColour = true;
		}
		if(m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.hasDescribedCategory)
		{
			m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.audioEntity->m_HasScannerDescribedCategory = true;
		}
		if(m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.hasDescribedModel)
		{
			m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.audioEntity->m_HasScannerDescribedModel = true;
		}
		if(m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.hasDescribedManufacturer)
		{
			m_RequestedSentences[m_PlayingSentenceIndex].vehicleHistory.audioEntity->m_HasScannerDescribedManufacturer = true;
		}
	}

	if(m_RequestedSentences[m_PlayingSentenceIndex].sentenceType == AUD_SCANNER_SCRIPTED)
	{
		m_LastScriptedLineTimeMs = g_AudioEngine.GetTimeInMilliseconds();
	}
}

audScannerSentence *audScannerManager::AllocateSentence(bool priorityCheck, audScannerPriority priorityToCheckAgainst, const u32 voiceNum)
{
	if(m_CurrentSentenceIndex == m_PlayingSentenceIndex)
	{
		m_CurrentSentenceIndex = ((m_CurrentSentenceIndex + 1) % g_MaxBufferedSentences);
	}
	audScannerSentence *ret = &m_RequestedSentences[m_CurrentSentenceIndex];
	if(priorityCheck)
	{
		if(m_RequestedSentences[m_CurrentSentenceIndex].inUse && m_RequestedSentences[m_CurrentSentenceIndex].priority > priorityToCheckAgainst)
		{
			return NULL;
		}
	}
	InitSentenceForScanner(m_CurrentSentenceIndex, voiceNum);
	m_CurrentSentenceIndex = ((m_CurrentSentenceIndex + 1) % g_MaxBufferedSentences);
	return ret;
}

void audScannerManager::InitSentenceForScanner(const u32 index, const u32 voiceNum)
{
	const u32 noiseLoopHash = ATSTRINGHASH("POLICE_SCANNER_NOISE_LOOP", 0x04b5f8dea);

	m_RequestedSentences[index].backgroundSFXHash = noiseLoopHash;
	m_RequestedSentences[index].predelay = 750;
	m_RequestedSentences[index].currentPhrase = 0;
	m_RequestedSentences[index].timeRequested = 0;
	m_RequestedSentences[index].inUse = true;
	m_RequestedSentences[index].gotAllSlots = true;
	m_RequestedSentences[index].isPositioned = false;
	m_RequestedSentences[index].occlusionGroup = NULL;
	m_RequestedSentences[index].sentenceSubType = 0;
	m_RequestedSentences[index].areaHash.SetInvalid();
	m_RequestedSentences[index].tracker = NULL;
	m_RequestedSentences[index].category = g_PoliceScannerCategory;
	m_RequestedSentences[index].fakeEchoTime = 0;
	m_RequestedSentences[index].voiceNum = voiceNum == 0 ? SelectScannerVoice() : voiceNum;
	m_RequestedSentences[index].useRadioVolOffset = true;
	m_RequestedSentences[index].scriptID = -1;
	m_RequestedSentences[index].isPlayerCrime = false;
	m_RequestedSentences[index].playAcknowledge = false;
	m_RequestedSentences[index].playResponding = false;

	// clear out vehicle history
	m_RequestedSentences[index].vehicleHistory.audioEntity = NULL;
	m_RequestedSentences[index].vehicleHistory.hasDescribedCategory = false;
	m_RequestedSentences[index].vehicleHistory.hasDescribedColour = false;
	m_RequestedSentences[index].vehicleHistory.hasDescribedManufacturer = false;
	m_RequestedSentences[index].vehicleHistory.hasDescribedModel = false;
	m_RequestedSentences[index].vehicleHistory.isPlayer = false;
	m_RequestedSentences[index].vehicleHistory.isOnFoot = false;
	for(u32 i = 0; i < g_MaxPhrasesPerSentence; i++)
	{
		m_RequestedSentences[index].phrases[i].isPlayerHeading = false;
		m_RequestedSentences[index].phrases[i].echoSound = m_RequestedSentences[index].phrases[i].sound = NULL;
	}

	m_RequestedSentences[index].m_LastUsedScannerSlot = SCANNERSLOTS_MAX;
	m_RequestedSentences[index].m_LastUsedSmallScannerSlot = SCANNERSLOTS_MAX;

	for(int i=0; i<SCANNERSLOTS_MAX; ++i)
		m_RequestedSentences[index].m_SlotIsInUse[i] = false;
}

bool audScannerManager::IsSentenceStillValid(const u32 index)
{
#if __BANK
	if(g_DebuggingScanner)
		return true;
#endif
	if(m_RequestedSentences[index].sentenceType == AUD_SCANNER_TRAIN)
	{
		// has the player left the train?
		if(m_RequestedSentences[index].sentenceSubType == 1 && !CGameWorld::FindLocalPlayerTrain())
		{
			return false;
		}

		// disable requests if Niko is on a train if this isn't a frontend report
		if(m_RequestedSentences[index].sentenceSubType != 1 && CGameWorld::FindLocalPlayerTrain())
		{
			return false;
		}
		// is this a stale request?
		if(m_RequestedSentences[index].timeRequested + 4000 < g_AudioEngine.GetTimeInMilliseconds())
		{
			return false;
		}
	}
	if(m_RequestedSentences[index].priority == AUD_SCANNER_PRIO_CRITICAL)
	{
		return true;
	}
	if(NetworkInterface::IsGameInProgress())
	{
		return true;
	}
	// don't play anything once the player has lost their wanted level or is outside the circle of wantedness
	if((FindPlayerPed()->GetPlayerWanted()->GetWantedLevel()==WANTED_CLEAN || CGameWorld::FindLocalPlayerWanted()->m_TimeOutsideCircle > 0) 
		&& m_RequestedSentences[index].priority != AUD_SCANNER_PRIO_AMBIENT
		&& m_RequestedSentences[index].sentenceType != AUD_SCANNER_SUSPECT_DOWN)
	{
		return false;
	}
	// has the player left the vehicle?  it sounds weird if they have
	if( m_RequestedSentences[index].vehicleHistory.isPlayer &&
		(m_RequestedSentences[index].vehicleHistory.isOnFoot || m_RequestedSentences[index].vehicleHistory.audioEntity) )
	{
		// or if they are in a vehicle and scanner wants to say on foot
		if(m_RequestedSentences[index].vehicleHistory.isOnFoot && FindPlayerVehicle())
		{
			return false;
		}
		if(!FindPlayerVehicle() || FindPlayerVehicle()->GetVehicleAudioEntity() != m_RequestedSentences[index].vehicleHistory.audioEntity)
		{
			return false;
		}
	}

	return true;
}

void audScannerManager::CancelAnySentencesFromTracker(audTracker *tracker)
{
	for(u32 i = 0;i < g_MaxBufferedSentences; i++)
	{
		if(m_RequestedSentences[i].inUse && m_RequestedSentences[i].tracker == tracker)
		{
			m_RequestedSentences[i].inUse = false;
			m_RequestedSentences[i].tracker = NULL;
		}
	}
}

void audScannerManager::CancelAnySentencesFromEntity(audVehicleAudioEntity *entity)
{
	for(u32 i = 0;i < g_MaxBufferedSentences; i++)
	{
		if(m_RequestedSentences[i].inUse && m_RequestedSentences[i].vehicleHistory.audioEntity == entity)
		{
			m_RequestedSentences[i].inUse = false;
			m_RequestedSentences[i].vehicleHistory.audioEntity = NULL;
		}
	}
}

void audScannerManager::CancelScriptedReport(s32 scriptID)
{
	if(!Verifyf(scriptID >= 0, "Invalid scriptID passed to CancelScriptedReport"))
		return;

	for(u32 i = 0;i < g_MaxBufferedSentences; i++)
	{
		if(m_RequestedSentences[i].inUse && m_RequestedSentences[i].scriptID == scriptID)
		{
			m_RequestedSentences[i].inUse = false;
		}
	}
}

bool audScannerManager::IsScriptedReportPlaying(s32 scriptID)
{
	if(!Verifyf(scriptID >= 0, "Invalid scriptID passed to IsScriptedReportPlaying"))
		return false;

	if(m_ScannerState == AUD_SCANNER_IDLE)
		return false;

	for(u32 i = 0;i < g_MaxBufferedSentences; i++)
	{
		if(m_RequestedSentences[i].scriptID == scriptID && m_PlayingSentenceIndex == i)
		{
			return true;
		}
	}

	return false;
}

bool audScannerManager::IsScriptedReportValid(s32 scriptID)
{
	if(!Verifyf(scriptID >= 0, "Invalid scriptID passed to IsScriptedReportValid"))
		return false;

	for(u32 i = 0;i < g_MaxBufferedSentences; i++)
	{
		if(m_RequestedSentences[i].scriptID == scriptID && m_RequestedSentences[i].inUse)
		{
			return true;
		}
	}

	return false;
}

s32 audScannerManager::GetPlayingScriptedReport()
{
	if(m_ScannerState == AUD_SCANNER_IDLE)
		return -1;

	for(u32 i = 0;i < g_MaxBufferedSentences; i++)
	{
		if(m_CurrentSentenceIndex == i)
		{
			return m_RequestedSentences[i].scriptID;
		}
	}

	return -1;
}

s32 audScannerManager::TriggerScriptedReport(const char *reportName,f32 applyValue)
{
	s32 retValue = -1;
	u32 sentenceIndex=~0U;
	const u32 reportHash = atStringHash(reportName);
	if(g_ScannerEnabled)
	{
		ScriptedScannerLine *line = audNorthAudioEngine::GetObject<ScriptedScannerLine>(reportHash);
		if(naVerifyf(line,"A script has requested an invalid scripted police scanner report, name: %s", reportName))
		{	
			// populate sentence structure
			if(m_CurrentSentenceIndex == m_PlayingSentenceIndex)
			{
				m_CurrentSentenceIndex = ((m_CurrentSentenceIndex + 1) % g_MaxBufferedSentences);
			}

			sentenceIndex = m_CurrentSentenceIndex;
			InitSentenceForScanner(m_CurrentSentenceIndex);
			audScannerSentence& sentence = m_RequestedSentences[m_CurrentSentenceIndex];
			sentence.sentenceType = AUD_SCANNER_SCRIPTED;
			sentence.sentenceSubType = reportHash;
			sentence.timeRequested = g_AudioEngine.GetTimeInMilliseconds();
			sentence.priority = AUD_SCANNER_PRIO_CRITICAL;
			sentence.shouldDuckRadio = true;
			sentence.backgroundSFXHash = (AUD_GET_TRISTATE_VALUE(line->Flags, FLAG_ID_SCRIPTEDSCANNERLINE_DISABLESCANNERSFX)==AUD_TRISTATE_TRUE?g_NullSoundHash:ATSTRINGHASH("POLICE_SCANNER_NOISE_LOOP", 0x04b5f8dea));
			sentence.fakeEchoTime = (AUD_GET_TRISTATE_VALUE(line->Flags, FLAG_ID_SCRIPTEDSCANNERLINE_ENABLEECHO) == AUD_TRISTATE_TRUE?100:0);
			sentence.scriptID = m_NextScriptedLineID++;
			sentence.policeScannerApplyValue = Max(applyValue, 0.01f);
			retValue = sentence.scriptID;
			//Highly doubtful we'll run long enough for this to happen, but may as well check
			if(Unlikely(m_NextScriptedLineID==INT_MAX))
				m_NextScriptedLineID = 0;

			u32 idx = 0;
			for(u32 i = 0;i < line->numPhrases; i++)
			{
				bool isLargeSlot = (ScannerSlotType)line->Phrase[i].Slot == LARGE_SCANNER_SLOT;
				sentence.phrases[idx].slot = isLargeSlot ? GetNextScannerSlot(sentence) : GetNextSmallScannerSlot(sentence);
				sentence.phrases[idx].postDelay = line->Phrase[i].PostDelay;
				sentence.phrases[idx].sound = NULL;
				sentence.phrases[idx].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(line->Phrase[i].SoundRef);

				//they live here to avoid "transfer of control bypasses initialization" error
				u32 compositeCodeSound = 0;
				u32 unitTypeSound = 0;
				u32 beatSound = 0;

				if(sentence.phrases[idx].soundMetaRef == g_ScannerCarUnitTriggerRef)
				{
					SelectCarUnitSounds(compositeCodeSound, unitTypeSound, beatSound, sentence.voiceNum);
					if(compositeCodeSound != 0)
					{
						//should use a large slot
						if(!isLargeSlot)
						{
							DecrementNextSmallScannerSlot(sentence);
							sentence.phrases[idx].slot = GetNextScannerSlot(sentence);
						}

						sentence.phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(compositeCodeSound);
					}
					else
					{
						sentence.phrases[idx].postDelay = 0;
						//should use small slots
						if(isLargeSlot)
						{
							DecrementNextScannerSlot(sentence);
							sentence.phrases[idx].slot = GetNextSmallScannerSlot(sentence);
						}

						sentence.phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(unitTypeSound);

						sentence.phrases[idx].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(beatSound);
						sentence.phrases[idx].postDelay = line->Phrase[i].PostDelay;
						sentence.phrases[idx++].slot = GetNextSmallScannerSlot(sentence);
					}
				}
				else if(sentence.phrases[idx].soundMetaRef == g_ScannerLocationTriggerRef)
				{
					if(Verifyf(!m_ScriptSetPos.IsEqual(ORIGIN), "Calling scripted scanner line with position variable without having set the position."))
					{
						if(isLargeSlot)
						{
							DecrementNextScannerSlot(sentence);
						}
						else
						{
							DecrementNextSmallScannerSlot(sentence);
						}

						audPositionDescription desc;
						g_PoliceScanner.DescribePositionForScriptedLine(m_ScriptSetPos, desc, sentence.voiceNum);
						PopulateSentenceWithPosDescription(desc, &sentence, idx);
						//this is going to overwrite the postdelay given to AREA in the Populate function....hopefully that's okay	
						if(idx > 0)
							sentence.phrases[idx-1].postDelay = line->Phrase[i].PostDelay;
					}
				}
				else if(sentence.phrases[idx].soundMetaRef == g_ScannerCrimeTriggerRef)
				{
					if(Verifyf(!m_ScriptSetCrime == CRIME_NONE, "Calling scripted scanner line with crime variable without having set the crime."))
					{
						const u32 crimeNameHash = GetScannerHashForVoice(g_CrimeNamePHashes[m_ScriptSetCrime], sentence.voiceNum);
						ScannerCrimeReport *metadata = audNorthAudioEngine::GetObject<ScannerCrimeReport>(crimeNameHash);
						if(metadata)
						{
							if(isLargeSlot)
							{
								DecrementNextScannerSlot(sentence);
							}
							else
							{
								DecrementNextSmallScannerSlot(sentence);
							}

							u32 instructionSound = 0;
							u32 crimeSound = 0;
							g_PoliceScanner.ChooseInstructionAndCrimeSound(metadata, instructionSound, crimeSound);

							// INSTRUCTION
							sentence.phrases[idx].slot = GetNextScannerSlot(sentence); //SCANNER_SLOT_1;
							sentence.phrases[idx].postDelay = 0;
							sentence.phrases[idx].sound = NULL;
#if __BANK
							if(g_PrintScannerTriggers)
								formatf(sentence.phrases[idx].debugName, 64, "[audScannerSpew] instructionSound");
#endif
							sentence.phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(instructionSound);

							// CRIME
							sentence.phrases[idx].slot = GetNextScannerSlot(sentence); //SCANNER_SLOT_2;
							sentence.phrases[idx].postDelay = line->Phrase[i].PostDelay;
							sentence.phrases[idx].sound = NULL;
#if __BANK
							if(g_PrintScannerTriggers)
								formatf(sentence.phrases[idx].debugName, 64, "[audScannerSpew] crimeSound");
#endif
							sentence.phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(crimeSound);
						}
					}
				}
				else if(sentence.phrases[idx].soundMetaRef == g_ScannerCrimeCodeTriggerRef)
				{
					if(Verifyf(!m_ScriptSetCrime == CRIME_NONE, "Calling scripted scanner line with crime variable without having set the crime."))
					{
						const u32 crimeNameHash = GetScannerHashForVoice(g_CrimeNamePHashes[m_ScriptSetCrime], sentence.voiceNum);
						ScannerCrimeReport *metadata = audNorthAudioEngine::GetObject<ScannerCrimeReport>(crimeNameHash);
						if(!metadata)
						{
							Assertf(0, "No audio game object for crime %x voice %i", g_CrimeNamePHashes[m_ScriptSetCrime], sentence.voiceNum);
							break;
						}

						if(isLargeSlot)
						{
							DecrementNextScannerSlot(sentence);
						}
						else
						{
							DecrementNextSmallScannerSlot(sentence);
						}

						sentence.phrases[idx].slot = GetNextSmallScannerSlot(sentence);
						sentence.phrases[idx].postDelay = 0;
						sentence.phrases[idx].sound = NULL;
#if __BANK
						if(g_PrintScannerTriggers)
							formatf(sentence.phrases[idx].debugName, 64, "[audScannerSpew] short crime sound %s", g_CrimeNamePHashes[m_ScriptSetCrime]);
#endif
						sentence.phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(metadata->SmallCrimeSoundRef);
					}
				}
				else
				{
					idx++;
				}
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(m_RequestedSentences[m_CurrentSentenceIndex].phrases[i].debugName, 64, "[audScannerSpew] Script Report %s line %u", reportName, i);
#endif
			}			

			//u32 idx = line->numPhrases;
			const u32 dispatchBackup = ATSTRINGHASH("SCRIPTED_REPORTS_DISPATCH_BACKUP", 0x0f8567059);
			if(reportHash == dispatchBackup)
			{
				audPositionDescription desc;
				g_PoliceScanner.DescribePosition(VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition()), desc, m_RequestedSentences[m_CurrentSentenceIndex].voiceNum);
				PopulateSentenceWithPosDescription(desc, &(m_RequestedSentences[m_CurrentSentenceIndex]), idx);
			}
			m_RequestedSentences[m_CurrentSentenceIndex].numPhrases = idx;
			m_RequestedSentences[m_CurrentSentenceIndex].inUse = true;
			m_CurrentSentenceIndex = ((m_CurrentSentenceIndex + 1) % g_MaxBufferedSentences);
		}
	}

	const u32 trainTermination = ATSTRINGHASH("SCRIPTED_REPORT_TRAIN_TERMINATES_HERE", 0x0b7e026ac);
	if(reportHash == trainTermination)
	{
		CTrain *train = (CTrain*)CGameWorld::FindLocalPlayerTrain();
		if(train)
		{
			CTrain *engine = train->FindEngine(train);
			engine->m_nTrainFlags.bDisableNextStationAnnouncements = true;;
		}
		// mark this as a frontend train announcement so it is cancelled if the player leaves the train
		if(sentenceIndex != ~0U)
		{
			m_RequestedSentences[sentenceIndex].priority = AUD_SCANNER_PRIO_HIGH;
			m_RequestedSentences[sentenceIndex].sentenceType = AUD_SCANNER_TRAIN;
			m_RequestedSentences[sentenceIndex].sentenceSubType = 1;
		}
	}

	ResetScriptSetVariables();
	return retValue;
}

void audScannerManager::ResetScriptSetVariables()
{
	m_UnitTypeNumber = 0;
	m_BeatNumber = 0;

	m_ScriptSetPos.Set(ORIGIN);
	m_ScriptSetCrime = CRIME_NONE;
}

ScannerSlots audScannerManager::GetNextScannerSlot(audScannerSentence& sentence)
{
	if(sentence.m_LastUsedScannerSlot == SCANNERSLOTS_MAX)
	{
		sentence.m_LastUsedScannerSlot = SCANNER_SLOT_1;
		sentence.m_SlotIsInUse[sentence.m_LastUsedScannerSlot] = true;
		return sentence.m_LastUsedScannerSlot;
	}

	sentence.m_LastUsedScannerSlot = (ScannerSlots)((s32)sentence.m_LastUsedScannerSlot + 1);
	if(sentence.m_LastUsedScannerSlot >= SCANNER_SMALL_SLOT_1)
		sentence.m_LastUsedScannerSlot = SCANNER_SLOT_1;

	if(sentence.m_SlotIsInUse[sentence.m_LastUsedScannerSlot] && sentence.gotAllSlots)
	{
		sentence.gotAllSlots = false;
		Errorf("Scanner failed to grab slot when building sentence.  The sentence structure at the time of the first failure is");
		for(int i=0; i<g_MaxPhrasesPerSentence && sentence.phrases[i].soundMetaRef.IsValid(); ++i)
		{
			#if __BANK
				Errorf("Phrase %i - Debug Name:%s", i, sentence.phrases[i].debugName);
			#else
				Errorf("Phrase %i", i);
			#endif
		}
		Assertf(0, "Failed to grab slot %i.  See output for details.", sentence.m_LastUsedScannerSlot);
	}
	sentence.m_SlotIsInUse[sentence.m_LastUsedScannerSlot] = true;
	return sentence.m_LastUsedScannerSlot;
}

ScannerSlots audScannerManager::GetNextSmallScannerSlot(audScannerSentence& sentence)
{
	if(sentence.m_LastUsedSmallScannerSlot == SCANNERSLOTS_MAX)
	{
		sentence.m_LastUsedSmallScannerSlot = SCANNER_SMALL_SLOT_1;
		sentence.m_SlotIsInUse[sentence.m_LastUsedSmallScannerSlot] = true;
		return sentence.m_LastUsedSmallScannerSlot;
	}

	sentence.m_LastUsedSmallScannerSlot = (ScannerSlots)((s32)sentence.m_LastUsedSmallScannerSlot + 1);
	if(sentence.m_LastUsedSmallScannerSlot >= NUM_SCANNERSLOTS)
		sentence.m_LastUsedSmallScannerSlot = SCANNER_SMALL_SLOT_1;

	if(sentence.m_SlotIsInUse[sentence.m_LastUsedSmallScannerSlot] && sentence.gotAllSlots)
	{
		sentence.gotAllSlots = false;
		Errorf("Scanner failed to grab slot when building sentence.  The sentence structure at the time of the first failure is");
		for(int i=0; i<g_MaxPhrasesPerSentence && sentence.phrases[i].soundMetaRef.IsValid(); ++i)
		{
			#if __BANK
				Errorf("Phrase %i - Debug Name:%s", i, sentence.phrases[i].debugName);
			#else
				Errorf("Phrase %i", i);
			#endif
		}
		Assertf(0, "Failed to grab slot %i.  See output for details.", sentence.m_LastUsedSmallScannerSlot);
	}
	sentence.m_SlotIsInUse[sentence.m_LastUsedSmallScannerSlot] = true;
	return sentence.m_LastUsedSmallScannerSlot;
}

void audScannerManager::DecrementNextScannerSlot(audScannerSentence& sentence)
{
	if(!Verifyf(sentence.m_LastUsedScannerSlot >= SCANNER_SLOT_1 && sentence.m_LastUsedScannerSlot <= SCANNER_SLOT_4, 
		"Bad value passed into DecrementNextScannerSlot().  This scanner line will be cancelled."))
	{
		sentence.gotAllSlots = false;
		return;
	}

	if(!Verifyf(sentence.m_SlotIsInUse[sentence.m_LastUsedScannerSlot], 
		"Trying to decrement m_LastUsedScannerSlot when the slot is not marked as in use.  This scanner line will be cancelled."))
	{
		sentence.gotAllSlots = false;
		return;
	}

	sentence.m_SlotIsInUse[sentence.m_LastUsedScannerSlot] = false;
	s32 decValue = sentence.m_LastUsedScannerSlot - 1;
	if(decValue < SCANNER_SLOT_1)
		sentence.m_LastUsedScannerSlot = (ScannerSlots)(SCANNER_SMALL_SLOT_1 - 1);
	else
		sentence.m_LastUsedScannerSlot = (ScannerSlots)decValue;
}

void audScannerManager::DecrementNextSmallScannerSlot(audScannerSentence& sentence)
{
	if(!Verifyf(sentence.m_LastUsedSmallScannerSlot >= SCANNER_SMALL_SLOT_1 && sentence.m_LastUsedSmallScannerSlot <= SCANNER_SMALL_SLOT_4, 
				"Bad value passed into DecrementNextSmallScannerSlot().  This scanner line will be cancelled."))
	{
		sentence.gotAllSlots = false;
		return;
	}

	if(!Verifyf(sentence.m_SlotIsInUse[sentence.m_LastUsedSmallScannerSlot], 
		"Trying to decrement m_LastUsedSmallScannerSlot when the slot is not marked as in use.  This scanner line will be cancelled."))
	{
		sentence.gotAllSlots = false;
		return;
	}

	sentence.m_SlotIsInUse[sentence.m_LastUsedSmallScannerSlot] = false;
	s32 decValue = sentence.m_LastUsedSmallScannerSlot - 1;
	if(decValue < SCANNER_SMALL_SLOT_1)
		sentence.m_LastUsedSmallScannerSlot = (ScannerSlots)(NUM_SCANNERSLOTS - 1);
	else
		sentence.m_LastUsedSmallScannerSlot = (ScannerSlots)decValue;
}

u32 audScannerManager::GetScannerHashForVoice(const char* input, const u32& voiceNum)
{
	if(voiceNum > AUD_NUM_SCANNER_VOICES)
		return atStringHash(input);

	return GetScannerHashForVoice(atPartialStringHash(input), voiceNum);
}

u32 audScannerManager::GetScannerHashForVoice(const u32& input, const u32& voiceNum)
{
	if(voiceNum > AUD_NUM_SCANNER_VOICES)
		return atFinalizeHash(input);

	return atStringHash(g_VoiceNumToStringMap[voiceNum], input);
}

void audScannerManager::PopulateSentenceWithPosDescription(const audPositionDescription &desc, audScannerSentence* sentence, u32 &idx)
{
	if(!sentence)
		return;

	//specific location
	if(desc.locationSound != 0 && desc.locationSound != g_NullSoundHash && 
	   desc.locationDirectionSound != 0 && desc.locationDirectionSound != g_NullSoundHash && 
		SOUNDFACTORY.GetMetadataPtr(desc.locationSound) && SOUNDFACTORY.GetMetadataPtr(desc.locationDirectionSound))
	{
		if(!Verifyf(idx < g_MaxPhrasesPerSentence, "Attempting to use too many phrases in audScannerManager::PopulateSentenceWithPosDescription"))
			return;
		// AT/NEAR/___ OF
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
		sentence->phrases[idx].postDelay = 0;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] locationDirectionSound");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(desc.locationDirectionSound);

		if(audEngineUtil::ResolveProbability(g_SquelchProb))
		{
			// SQUELCH
			sentence->phrases[idx].slot = SCANNER_SLOT_NULL; //resident sound
			sentence->phrases[idx].postDelay = g_SquelchPostDelay;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] squelch");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_SquelchSoundHash);
		}

		if(!Verifyf(idx < g_MaxPhrasesPerSentence, "Attempting to use too many phrases in audScannerManager::PopulateSentenceWithPosDescription"))
			return;
		// LOCATION
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
		sentence->phrases[idx].postDelay = g_DelayAfterStreet;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] locationSound");
#endif
		sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(desc.locationSound);

		return;
	}

	// STREET
	bool haveStreet;
	bool shouldUseConjunctive = true;
	// check to see we have a sound for this street
	if(SOUNDFACTORY.GetMetadataPtr(desc.streetSound))
	{
		if(!Verifyf(idx < g_MaxPhrasesPerSentence, "Attempting to use too many phrases in audScannerManager::PopulateSentenceWithPosDescription"))
			return;

		if(desc.streetConjunctiveSound.IsValid() && desc.streetConjunctiveSound != g_ScannerNullSoundRef)
		{
			// ON
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_2;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] streetConjunctiveSound");
#endif
			sentence->phrases[idx++].soundMetaRef = desc.streetConjunctiveSound;
		}

		if(audEngineUtil::ResolveProbability(g_SquelchProb))
		{
			// SQUELCH
			sentence->phrases[idx].slot = SCANNER_SLOT_NULL; //resident sound
			sentence->phrases[idx].postDelay = g_SquelchPostDelay;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] squelch");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_SquelchSoundHash);
		}

		if(!Verifyf(idx < g_MaxPhrasesPerSentence, "Attempting to use too many phrases in audScannerManager::PopulateSentenceWithPosDescription"))
			return;
		// STREET
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
		sentence->phrases[idx].postDelay = g_DelayAfterStreet;
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] streetSound");
#endif
		sentence->phrases[idx++].soundMetaRef = desc.streetSound;
		haveStreet = true;
	}
	else
	{
		if(desc.inDirectionSound != 0)
		{
			if(!Verifyf(idx < g_MaxPhrasesPerSentence, "Attempting to use too many phrases in audScannerManager::PopulateSentenceWithPosDescription"))
				return;
			
			if(audEngineUtil::ResolveProbability(g_SquelchProb))
			{
				// SQUELCH
				sentence->phrases[idx].slot = SCANNER_SLOT_NULL; //resident sound
				sentence->phrases[idx].postDelay = g_SquelchPostDelay;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] squelch");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_SquelchSoundHash);
			}

			// we don't have a street but we do have a direction, lets use that
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_3;
			sentence->phrases[idx].postDelay = 0;
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] inDirectionSound");
#endif
			sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(desc.inDirectionSound);
			shouldUseConjunctive = false;
		}
		haveStreet = false;
	}

	// AREA
	// if we said the street and the area hasn't changed then occasionally skip area
	if(!(haveStreet && (m_LastSpokenAreaHash == desc.areaSound || audEngineUtil::ResolveProbability(g_SkipNewAreaProb)) && audEngineUtil::ResolveProbability(g_SkipDuplicateAreaProb)))
	{
		if(shouldUseConjunctive)
		{
			if(audEngineUtil::ResolveProbability(g_SquelchProb))
			{
				// SQUELCH
				sentence->phrases[idx].slot = SCANNER_SLOT_NULL; //resident sound
				sentence->phrases[idx].postDelay = g_SquelchPostDelay;
				sentence->phrases[idx].sound = NULL;
#if __BANK
				if(g_PrintScannerTriggers)
					formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] squelch");
#endif
				sentence->phrases[idx++].soundMetaRef = SOUNDFACTORY.GetMetadataManager().GetObjectMetadataRefFromHash(g_SquelchSoundHash);
			}

			if(!Verifyf(idx < g_MaxPhrasesPerSentence, "Attempting to use too many phrases in audScannerManager::PopulateSentenceWithPosDescription"))
				return;
			sentence->phrases[idx].slot = g_AudioScannerManager.GetNextSmallScannerSlot(*sentence); //SCANNER_SMALL_SLOT_2;
			// if we're erring postdelay the in...
			sentence->phrases[idx].sound = NULL;
#if __BANK
			if(g_PrintScannerTriggers)
				formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] areaConjunctiveSound");
#endif
			sentence->phrases[idx++].soundMetaRef = desc.areaConjunctiveSound;
		}

		if(!Verifyf(idx < g_MaxPhrasesPerSentence, "Attempting to use too many phrases in audScannerManager::PopulateSentenceWithPosDescription"))
			return;
		sentence->phrases[idx].slot = g_AudioScannerManager.GetNextScannerSlot(*sentence); //SCANNER_SLOT_4;
		sentence->phrases[idx].postDelay = audEngineUtil::GetRandomNumberInRange(100, 2000);
		sentence->phrases[idx].sound = NULL;
#if __BANK
		if(g_PrintScannerTriggers)
			formatf(sentence->phrases[idx].debugName, 64, "[audScannerSpew] areaSound");
#endif
		sentence->phrases[idx++].soundMetaRef = desc.areaSound;
		sentence->areaHash = desc.areaSound;
	}
}

u32 audScannerManager::SelectScannerVoice()
{
	//Change this once we have multiple voices and a way to determine which to use
#if __BANK
	if(g_ForceScannerVoice)
		return g_ForcedScannerVoice;
#endif
	return 1;
}

void audScannerManager::SelectCarUnitSounds(u32& compositeCodeSound, u32& unitTypeSound, u32& beatSound, u32 voiceNum)
{
	if( (m_UnitTypeNumber < 0 || m_BeatNumber <=0 || m_UnitTypeNumber > 25 || m_BeatNumber > 24)
#if __BANK
		&& (!g_OverrideCarCodes || g_BeatNumberOverride ==0 || g_UnitTypeNumberOverride > 25 || g_BeatNumberOverride > 24)
#endif
		)
	{
		//Do composite
		Assertf(0, "Scanner line using car unit playing, even though scripters failed to set proper unit values.");
		compositeCodeSound = GetScannerHashForVoice("POLICE_SCANNER_CAR_CODE_COMPOSITE", voiceNum);	
		if(PARAM_audScannerSpew.Get())
		{
			Displayf("[audScannerSpew] Selecting composite car sound with voice %u and hash %u", voiceNum, compositeCodeSound);
		}
	}
	else
	{
		//Find parts
		//Unit type
		char unitString[64];
#if __BANK
		if(g_OverrideCarCodes)
			formatf(unitString, "POLICE_SCANNER_CAR_CODE_UNIT_TYPE_%s", g_CarCodeUnitTypeStrings[g_UnitTypeNumberOverride]);
		else
#endif
		formatf(unitString, "POLICE_SCANNER_CAR_CODE_UNIT_TYPE_%s", g_CarCodeUnitTypeStrings[m_UnitTypeNumber]);
		unitTypeSound = GetScannerHashForVoice(unitString, voiceNum);
		if(PARAM_audScannerSpew.Get())
		{
			Displayf("[audScannerSpew] Selecting car unit type sound with voice %u name %s and hash %u", voiceNum, unitString, unitTypeSound);
		}

		//Beat
		char beatString[64];
#if __BANK
		if(g_OverrideCarCodes)
			formatf(beatString, "POLICE_SCANNER_CAR_CODE_BEAT_%i", g_BeatNumberOverride);
		else
#endif
		formatf(beatString, "POLICE_SCANNER_CAR_CODE_BEAT_%i", m_BeatNumber);
		beatSound = GetScannerHashForVoice(beatString, voiceNum);
		if(PARAM_audScannerSpew.Get())
		{
			Displayf("[audScannerSpew] Selecting car beat sound with voice %u name %s and hash %u", voiceNum, beatString, beatSound);
		}
	}
}

void audScannerManager::SetCarUnitValues(s32 UnitTypeNum, s32 BeatNum)
{
	m_UnitTypeNumber = UnitTypeNum;
	m_BeatNumber = BeatNum;
}

void audScannerManager::SetPositionValue(const Vector3 &pos)
{
	m_ScriptSetPos.Set(pos);
}

void audScannerManager::SetCrimeValue(s32 crime)
{
	if(crime < (s32)CRIME_NONE || crime >= (s32)MAX_CRIMES)
		m_ScriptSetCrime = CRIME_NONE;
	else
		m_ScriptSetCrime = (eCrimeType)crime;
}

void audScannerManager::ReportResistArrest(const Vector3& pos, u32 delay)
{
	m_TimeResistArrestLastTriggered = g_AudioEngine.GetTimeInMilliseconds();
	m_PosForDelayedResistArrest.Set(pos);
	m_DelayForDelayedResistArrest = delay;
}

void audScannerManager::ResetResistArrest()
{
	m_TimeResistArrestLastTriggered = 0;
}

bool audScannerManager::HasCrimeBeenReportedRecently(const eCrimeType crime, u32 timeInMs)
{
	for(int i=1; i<g_MaxBufferedSentences ; i++)
	{
		int index = (m_CurrentSentenceIndex - i) % g_MaxBufferedSentences;
		if(m_RequestedSentences[index].crimeId == crime)
		{
			if(timeInMs - m_RequestedSentences[index].timeRequested > 30000)
				return false;
			else
				return true;
		}
		//probably reached an area that hasn't been populated yet
		if(m_RequestedSentences[index].timeRequested == 0)
			return false;
	}

	return false;
}

audDirection audScannerManager::CalculateAudioDirection(const Vector3 &vDirection)
{
	const Vector3 vNorth = Vector3(0.0f, 1.0f, 0.0f);
	const Vector3 vEast = Vector3(1.0f, 0.0f, 0.0f);
	Vector3 vDir = vDirection;
	vDir.NormalizeFast();

	f32 fCosAngle = vDir.Dot(vNorth);
	f32 fAngle = AcosfSafe(fCosAngle);
	f32 fDegrees = RtoD * fAngle;

	if(vDir.Dot(vEast) <= 0.0f)
	{
		fDegrees = (360.0f - fDegrees);
	}

	// split into 90 degree segments, with north spanning 315 -> 45
	return static_cast<audDirection>(static_cast<u32>((((fDegrees + 45.f) / 90.0f))) % 4);
}

#if __BANK
char g_TestScriptedLineName[128];

void TriggerScriptedReportCB()
{
	g_AudioScannerManager.TriggerScriptedReport(g_TestScriptedLineName);
}

void ReportSpottingPlayerCB()
{
	CPed* pPlayer = FindPlayerPed();

	if(pPlayer)
	{
		Vector3 pos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		g_PoliceScanner.ReportPoliceSpottingPlayer(pos, 0, true, true, false);
	}
	else
	{
		Warningf("Unable to find local player, skipping call to ReportPoliceSpottingPlayer()");
	}
}

void ReportPlayerVehicleCB()
{
	g_PoliceScanner.ReportPoliceSpottingPlayerVehicle(FindPlayerVehicle());
}

void ReportRandomCrimeCB()
{
	CPed* pPlayer = FindPlayerPed();

	if(pPlayer)
	{
		Vector3 pos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		// pick a random position away from the player
		f32 xOffset = audEngineUtil::GetRandomNumberInRange(400.f, 4000.f);
		f32 yOffset = audEngineUtil::GetRandomNumberInRange(400.f, 4000.f);
		pos.x = pos.x + (Sign(audEngineUtil::GetRandomNumberInRange(-1.f,1.f)) * xOffset);
		pos.y = pos.y + (Sign(audEngineUtil::GetRandomNumberInRange(-1.f,1.f)) * yOffset);

		g_PoliceScanner.ReportRandomCrime(pos);
	}
	else
	{
		Warningf("Unable to find local player, skipping call to ReportRandomCrime()");
	}
}

void RandomDispatchCB()
{
	CPed* pPlayer = FindPlayerPed();

	if(pPlayer)
	{
		s32 numUnits = audEngineUtil::GetRandomNumberInRange(0,7);
		audUnitType type = static_cast<audUnitType>(audEngineUtil::GetRandomNumberInRange(0,2));
		const Vector3 &pos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		g_PoliceScanner.ReportDispatch(numUnits, type, pos);
	}
	else
	{
		Warningf("Unable to find local player, skipping call to ReportDispatch()");
	}
}

void RandomAssistanceCB()
{
	CPed* pPlayer = FindPlayerPed();

	if(pPlayer)
	{
		Vector3 pos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		// pick a random position away from the player
		pos.x = pos.x + audEngineUtil::GetRandomNumberInRange(-1000.f, 1000.f);
		pos.y = pos.y + audEngineUtil::GetRandomNumberInRange(-1000.f, 1000.f);
		g_PoliceScanner.RequestAssistance(pos, AUD_SCANNER_PRIO_HIGH);
	}
	else
	{
		Warningf("Unable to find local player, skipping call to RequestAssistance()");
	}
}

void RandomChatCB()
{
	g_PoliceScanner.TriggerRandomChat();
}

void SpotNPCCB()
{
	fwEntity *pSelectedEntity = g_PickerManager.GetSelectedEntity();
	if(pSelectedEntity == NULL)
	{
		pSelectedEntity = CGameWorld::FindLocalPlayer();
	}
	g_PoliceScanner.ReportPoliceSpottingNonPlayer((CPed*)pSelectedEntity,1.0f,true);
}

void audScannerManager::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Scanner Manager", false);

	bank.AddToggle("EnableScanner", &g_ScannerEnabled);
	bank.AddToggle("g_DebuggingScanner", &g_DebuggingScanner);
	bank.AddToggle("ShowAreaDebug", &g_ShowAreaDebug);
	bank.AddToggle("g_DrawScannerSpecificLocations", &g_DrawScannerSpecificLocations);
	bank.AddToggle("PrintScannerTriggers", &g_PrintScannerTriggers);
	bank.AddToggle("EnableDuplicateCrimes", &g_TriggerDuplicateCrimes);
	bank.AddToggle("ShowBufferedSentences", &g_ShowBufferedSentences);
	bank.AddToggle("ForceScannerVoice", &g_ForceScannerVoice);
	bank.AddSlider("ScannerVoice",&g_ForcedScannerVoice, 0, AUD_NUM_SCANNER_VOICES, 1);
	bank.AddToggle("ShowCarColour", &g_PrintCarColour);
	bank.AddSlider("MinTimeBetweenReports",&g_MinTimeBetweenReports, 0, 20000, 1);
	bank.AddToggle("g_EnableCopDispatchInteraction", &g_EnableCopDispatchInteraction);
	bank.AddToggle("g_TestDispInteraction", &g_TestDispInteraction);
	bank.AddToggle("g_DebugCopDispatchInteraction", &g_DebugCopDispatchInteraction);
	bank.AddSlider("PositionUpdateTime",&g_PositionUpdateTimeMs, 0 ,20000, 1);
	bank.AddSlider("g_PositionUpdateDeltaSq",&g_PositionUpdateDeltaSq, 0.0f, 5.0f, 0.01f);
	bank.AddSlider("MovementThreshold",&g_MinPlayerMovementThreshold, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("ReportCrimeProb",&g_ReportCrimeProbability, 0.0f, 1.0f, 0.05f);
	bank.AddSlider("ReportSpottingProb",&g_ReportSpottingProbability, 0.0f, 1.0f, 0.05f);
	bank.AddSlider("ReportSuspectHeadingProb",&g_ReportSuspectHeadingProb,0.0f,1.0f,0.1f);
	bank.AddSlider("ReportCarProb", &g_ReportCarProb, 0.0f, 1.0f, 0.1f);
	bank.AddSlider("ReportCarPrefixProb", &g_ReportCarPrefixProb, 0.0f, 1.0f, 0.1f);
	bank.AddSlider("PreemptMs",&g_ScannerPreemptMs,0,300, 1);
	bank.AddSlider("DelayAfterStreet", &g_DelayAfterStreet, 0, 1000, 1);
	bank.AddSlider("g_SquelchProb", &g_SquelchProb, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("ErrAfterInProb", &g_ErrAfterInProb, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("DelayBeforeErr", &g_DelayBeforeErr, 0, 1000, 1);
	bank.AddSlider("DelayAfterErr", &g_DelayAfterErr, 0, 1000, 1);
	bank.AddSlider("g_SquelchPostDelay", &g_SquelchPostDelay, 0, 1000, 1);
	bank.AddSlider("UnsureOfNewAreaProb", &g_UnsureOfNewAreaProb,0.0f,1.0f,0.1f);
	bank.AddSlider("SkipDuplicateAreaProb", &g_SkipDuplicateAreaProb,0.0f,1.0f,0.1f);
	bank.AddSlider("g_SkipNewAreaProb", &g_SkipNewAreaProb,0.0f,1.0f,0.1f);
	bank.AddSlider("DistFactorToConsiderCentral", &g_DistFactorToConsiderCentral,0.0f,1.0f,0.01f);
	bank.AddSlider("AreaSizeTooSmallForDirection", &g_AreaSizeTooSmallForDirection, 0.0f, 1000.0f, 0.01f);
	bank.AddText("Street Name", &g_TestStreetName[0], sizeof(g_TestStreetName));
	bank.AddText("Area Name", &g_TestAreaName[0], sizeof(g_TestAreaName));
	bank.AddButton("RandomCrime", CFA(ReportRandomCrimeCB));
	bank.AddButton("SpotPlayer", CFA(ReportSpottingPlayerCB));
	bank.AddButton("SpotPlayerVehicle", CFA(ReportPlayerVehicleCB));
	bank.AddButton("RandomDispatch", CFA(RandomDispatchCB));
	bank.AddButton("RandomAssistanceReqd", CFA(RandomAssistanceCB));
	bank.AddButton("RandomChat", CFA(RandomChatCB));
	bank.AddButton("Spot NPC", CFA(SpotNPCCB));
	bank.AddText("ScriptedReportName", &g_TestScriptedLineName[0], sizeof(g_TestScriptedLineName));
	bank.AddButton("TriggerScriptedReport", CFA(TriggerScriptedReportCB));
	bank.AddToggle("OverrideCarCodes", &g_OverrideCarCodes);
	bank.AddSlider("UnitTypeNumber", &g_UnitTypeNumberOverride,0,25,1);
	bank.AddSlider("BeatNumber", &g_BeatNumberOverride,1,24,1);
	bank.AddSlider("RadioVolOffset", &g_VolOffsetForRadio, 0.f, 12.f, 0.1f);
	bank.AddSlider("ScannerAttackTime", &g_ScannerAttackTime, 0, 250, 1);
	bank.AddSlider("g_MinCopDistSqForCopInstSet", &g_MinCopDistSqForCopInstSet, 0.0f, 10000.0f, 1.0f);
	bank.AddSlider("g_MinPedDistSqForPedInstSet", &g_MinPedDistSqForPedInstSet, 0.0f, 10000.0f, 1.0f);
	bank.AddSlider("g_MinPctForLocationAt", &g_MinPctForLocationAt, 0.0f, 1.0f, 0.01f);
	bank.AddSlider("g_MinPctForLocationNear", &g_MinPctForLocationNear, 0.0f, 1.0f, 0.01f);
	bank.PushGroup("NonPlayer Spotting Accuracy Thresholds");
		bank.AddSlider("g_SpeedInfoMinAccuracy", &g_SpeedInfoMinAccuracy, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_HeadingInfoMinAccuracy", &g_HeadingInfoMinAccuracy, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_VehicleInfoMinAccuracy", &g_VehicleInfoMinAccuracy, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_CarPrefixInfoMinAccuracy", &g_CarPrefixInfoMinAccuracy, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_CarCategoryInfoMinAccuracy", &g_CarCategoryInfoMinAccuracy, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_CarModelInfoMinAccuracy", &g_CarModelInfoMinAccuracy, 0.0f, 1.0f, 0.01f);
	bank.PopGroup();

	bank.PopGroup();
}
#endif
#endif // NA_POLICESCANNER_ENABLED
