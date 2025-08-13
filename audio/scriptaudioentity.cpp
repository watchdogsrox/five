// 
// audio/scriptaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "scriptaudioentity.h"
#include "frontendaudioentity.h"
#include "planeaudioentity.h"
#include "ambience/ambientaudioentity.h"
#include "music/musicplayer.h"
#include "northaudioengine.h"
#include "radioaudioentity.h"
#include "policescanner.h"
#include "speechmanager.h"
#include "debugaudio.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audioengine/soundfactory.h"
#include "audiosoundtypes/simplesound.h"
#include "audiohardware/waveslot.h"
#include "audiohardware/driver.h"
#include "audio/caraudioentity.h"
#include "debug/debug.h"
#include "debug/debugglobals.h"
#include "dynamicmixer.h"
#include "text/messages.h"
#include "peds/ped.h"
#include "peds/PedPhoneComponent.h"
#include "peds/popzones.h"
#include "control/replay/Audio/ScriptAudioPacket.h"
#include "scene/RefMacros.h"
#include "script/script.h"
#include "system/FileMgr.h"
#include "game/zones.h"
#include "vehicles/automobile.h"
#include "Vfx/Misc/Fire.h"
#include "network/NetworkInterface.h"
#include "network/Events/NetworkEventTypes.h"
#include "game/user.h"
#include "text/TextConversion.h"

#include "debugaudio.h"

#include <ctime>

AUDIO_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(Audio,Conversation)
RAGE_DEFINE_SUBCHANNEL(Audio,NetworkAudio)

//OPTIMISATIONS_OFF() 
namespace rage
{
	XPARAM(audiowidgets);
}

PF_PAGE(ScriptedSpeechTrackerPage, "Scripted Speech Tracker Page");

PF_GROUP(ScriptedSpeechStats);
PF_LINK(ScriptedSpeechTrackerPage, ScriptedSpeechStats);

PF_VALUE_INT(TimeToPreloadScriptedSpeech, ScriptedSpeechStats);
PF_VALUE_INT(TimeTilTriggeredSpeechPlayed, ScriptedSpeechStats);

XPARAM(audiodesigner);
PARAM(enableConvInterrupt, "Enable scripted conversation interrupts.");
PARAM(disableConvInterrupt, "Disable scripted conversation interrupts.");
PARAM(conversationspew, "Ensable scripted conversation spew");
PARAM(noAbortingFailedConv, "Do not abort conversations that fail to load.");
PARAM(noOverlapLipsyncAdjustment, "Do not adjust conversation overlap times for lipsync predelay.");
PARAM(trackMissingDialogue, "Output info on missing scripted speech lines to a timestamp-named CSV file.");
PARAM(trackMissingDialogueMP, "Output info on missing scripted speech lines to a timestamp-named CSV file (including MP).");
PARAM(displayScriptedSpeechLine, "Debug draw the current playing scripted speech line.");
PARAM(useRobotSpeechForAllLines, "If not set, robot speech will only be used for non-placeholder conversations");
PARAM(scriptStreamSpew, "Ensable scripted stream spew");
PARAM(scriptSoundsDebug, "Ensable scripted sounds on screen debug");

audScriptAudioEntity g_ScriptAudioEntity;
extern audAmbientAudioEntity g_AmbientAudioEntity;
extern const char* g_CarCodeUnitTypeStrings[26];

extern f32 g_HealthLostForHighPain;

audHashActionTable audScriptAudioEntity::sm_ScriptActionTable;
s32	audScriptAudioEntity::sm_ScriptInChargeOfTheStream = -1;
const s32 g_PhoneOverlapTime = 250;
u32 g_TimeToWaitForInterruptToPrepare = 2500;

NOTFINAL_ONLY(bool g_EnablePlaceholderDialogue = true);

#if __BANK
bool audScriptAudioEntity::sm_DrawScriptTriggeredSounds = false;
bool audScriptAudioEntity::sm_DrawScriptStream = false;
bool audScriptAudioEntity::sm_InWorldDrawScriptTriggeredSounds = false;
bool audScriptAudioEntity::sm_LogScriptTriggeredSounds = false;
bool audScriptAudioEntity::sm_DrawScriptBanksLoaded = false;
bool audScriptAudioEntity::sm_FilterScriptTriggeredSounds = false;
char audScriptAudioEntity::sm_ScriptTriggerOverride[64] = "\0";
char audScriptAudioEntity::sm_ScriptedLineDebugInfo[128] = "\0";
bool audScriptAudioEntity::sm_DrawReplayScriptSpeechSlots = false;
bool g_FakeConvAnimTrigger = false;
bool g_AnimTriggersControllingConversation = false;
bool g_ConversationDebugSpew = false;
bool g_ScriptedStreamSpew = false;
bool g_ForceMobileInterference = false;
bool g_DisplayScriptedLineInfo = false;
bool g_DisplayScriptSoundIdInfo = false;
bool g_DisplayScriptBankUsage = false;
bool g_DrawScriptSpeechSlots = false;
f32 g_ScriptSoundIdInfoScroll = 0.0f;

static char g_AmbientSpeechDebugContext[128] = "CAR_HIT_PED";
static char g_AmbientSpeechDebugVoice[128] = "MICHAEL_NORMAL";
static char g_AmbientSpeechDebugParams[128] = "SPEECH_PARAMS_STANDARD";
f32 g_AmbientSpeechDebugPosX = 0.0f;
f32 g_AmbientSpeechDebugPosY = 0.0f;
f32 g_AmbientSpeechDebugPosZ = 0.0f;

bool g_TrackMissingDialogue = false;
bool g_TrackMissingDialogueMP = false;
#endif

bool g_ForceScriptedInterrupts = false;
bool g_DoNotAbortFailedConversations = false;
bool g_AdjustOverlapForLipsyncPredelay = true;
bool g_AdjustTriggerTimeForZeroPlaytime = true;
bool g_AdjustTriggerForZeroEvenIfMarkedOverlap = true;

u32 g_MinPedsHitForRunnedOverReaction = 3;
u32 g_TimespanToHitPedForRunnedOverReaction = 6000;
u32 g_MinTimeBetweenLastCollAndRunnedOverReaction = 2000;
u32 g_TimeToWaitBetweenRunnedOverReactions = 5000;

u32 g_AdditionalScriptedSpeechPause = 0;

bool g_DuckForConvAfterPreload = false;

bool g_DebugPlayingScriptSounds = false;

s32 g_MissionCompleteDelay = 1500;

extern s32 g_NextFireStartOffset;
extern bool g_MuteGameWorldAndPositionedRadioForTV;
bool g_AllowUpsideDownConversations = false;
bool g_AllowVehicleOnFireConversations = false;
bool g_AllowVehicleDrowningConversations = false;
s32 g_StopAmbientAnimTime = 1000;
bool g_DebugScriptedConvs = false;

const u32 g_FrontendGameMobileRingingHash = ATSTRINGHASH("FRONTEND_GAME_MOBILE_RINGING", 0x0980d4ab6);
const u32 g_MobilePreRingHash = ATSTRINGHASH("MOBILE_PRERING_SOUND", 0x0b88f89fb);
const u32 g_MobileGetSignalHash = ATSTRINGHASH("MOBILE_GET_SIGNAL_SOUND", 0x0428e734b);

f32 g_MinLowPainBikeReaction = 45.0f;
f32 g_MinLowReactionDamage = 11.0f;
f32 g_MinMediumReactionDamage = 20.0f;
f32 g_MinHighReactionDamage = 100.0f;
u32 g_TimeToPauseConvOnCollision = 1500;
u32 g_MinTimeBetweenCollisionReactions = 5000;
u32 g_MinTimeBetweenCollisionTriggers = 5000;
u32 g_MaxTimeToAllocateSlot = 5000;
bool g_DisableInterrupts = false;
bool g_DisableAirborneInterrupts = false;
bool g_AllowMuliWantedInterrupts = false;
s32 g_MinRagdollTimeBeforeConvPause = 500;
s32 g_MinNotRagdollTimeBeforeConvRestart = 3000;
u32 g_MinTimeCarInAirForInterrupt = 700;
u32 g_MinTimeCarOnGroundForInterrupt = 10000;
u32 g_MinTimeCarOnGroundToRestart = 750;
u32 g_MinAirBorneTimeToKillConversation = 8000;
u32 g_MaxAirBorneTimeToApplyRestartOffset = 4000;
f32 g_MinCarRollForInterrupt = -0.4f;

#if GTA_REPLAY
tReplayScriptedSpeechSlots audScriptAudioEntity::sm_ReplayScriptedSpeechSlot[AUD_REPLAY_SCRIPTED_SPEECH_SLOTS];
tReplayScriptedSpeechSlots audScriptAudioEntity::sm_ReplayScriptedSpeechAlreadyPlayed[AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS];
char audScriptAudioEntity::sm_CurrentStreamName[128];
u32 audScriptAudioEntity::sm_CurrentStartOffset = 0;
char audScriptAudioEntity::sm_CurrentSetName[128];
#endif

#if __BANK
int g_InterruptSpeakerNum = 0;
bool g_ForceNonPlayerInterrupt = false;
char g_szLowInterruptContext[256];
char g_szMediumInterruptContext[256];
char g_szHighInterruptContext[256];
#endif

AircraftWarningSettings* g_AircraftWarningSettings = NULL;
//u32 g_AircraftWarningPHash = ATPARTIALSTRINGHASH("AIRCRAFT_WARNING", 0xF65A0A93);
u32 g_AircraftWarningMale1Hash = ATSTRINGHASH("AIRCRAFT_WARNING_MALE_01", 0xA65A6402);
u32 g_AircraftWarningFemale1Hash = ATSTRINGHASH("AIRCRAFT_WARNING_FEMALE_01", 0x85A08F9C);
bool g_EnableAircraftWarningSpeech = false;


const u32 g_BarkPHash = ATPARTIALSTRINGHASH("BARK", 0xB0EF9479);

// Create flag map

#if RSG_BANK
#define SCRIPT_AUDIO_FLAG(n,h) ATSTRINGHASH(#n,h), #n,
#else
#define SCRIPT_AUDIO_FLAG(n,h) ATSTRINGHASH(#n,h),
#endif

const audScriptAudioFlags::tFlagNames audScriptAudioFlags::s_FlagNames[audScriptAudioFlags::kNumScriptAudioFlags] = {
	#include "scriptaudioflags.h"
};

#undef SCRIPT_AUDIO_FLAG

void audScriptAudioEntity::InitClass()
{
	// default handler handles cases where a sound has to be triggered with no special logic
	sm_ScriptActionTable.SetDefaultActionHandler(&audScriptAudioEntity::DefaultScriptActionHandler);

	// phone call functions
	sm_ScriptActionTable.AddActionHandler("MOBILE_PRERING", &g_ScriptAudioEntity.MobilePreRingHandler);
	//	m_ScriptActionTable.AddActionHandler("START_MOBILE_PHONE_RINGING", &g_ScriptAudioEntity.StartMobilePhoneRinging);
	//	m_ScriptActionTable.AddActionHandler("START_MOBILE_PHONE_CALLING", &g_ScriptAudioEntity.StartMobilePhoneCalling);
	//	m_ScriptActionTable.AddActionHandler("STOP_MOBILE_PHONE_RINGING", &g_ScriptAudioEntity.StopMobilePhoneRinging);
}

void audScriptAudioEntity::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audScriptAudioEntity");
	m_SpeechAudioEntity.Init(NULL);
	m_ScriptInChargeOfAudio = -1;
	BANK_ONLY(m_CurrentScriptName = "");
	m_MobileInterferenceSound = NULL;
	m_CellphoneRingSound = NULL;
	m_ScriptedConversationInProgress = false;
	m_DuckingForScriptedConversation = false;
	m_IsScriptedConversationZit = false;
	m_ScriptedConversationSpeechSound = NULL;
	m_PlayingScriptedConversationLine = -1;
	m_IsScriptedConversationAMobileCall = false;
	m_DisplaySubtitles = false;
	m_CloneConversation = false;
	m_AddSubtitlesToBriefing = false;
	m_PauseConversation = false;
	m_PausedForRagdoll = false;
	m_PauseWaitingOnCurrentLine = false;
	m_ShouldRestartConvAtMarker = false;
	m_AbortScriptedConversationRequested = false;
	m_HighestLoadedLine = -1;
	m_HighestLoadingLine = -1;
	m_PreLoadingSpeechLine = -1;
	m_ScriptedConversationOverlapTriggerTime = 0;
	m_OverlapTriggerTimeAdjustedForPredelay = false;
	m_DisableReplayScriptStreamRecording = false;
	m_ScriptedConversationSfx = NULL;
	m_CurrentPreloadSet = 0;
	m_LastMobileInterferenceTime = 0;
	m_NextTimeScriptedConversationCanStart = 0;
	m_TimeLastScriptedConvFinished = 0;
	m_TimeLastBeepOffPlayed = 0;
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_LINES; i++)
	{
		m_ScriptedConversation[i].Speaker = -1;
		strcpy(m_ScriptedConversation[i].Context, "");
		strcpy(m_ScriptedConversation[i].Subtitle, "");
		m_ScriptedConversation[i].Listener = -1;
		m_ScriptedConversation[i].SpeechSlotIndex = -1;
		m_ScriptedConversation[i].Volume = AUD_SPEECH_NORMAL;
		m_ScriptedConversation[i].SpeechSlotIndex = 0;
#if !__FINAL
		m_ScriptedConversation[i].TimePrepareFirstRequested = 0;
#endif

		m_ScriptedConversationBuffer[i].Speaker = -1;
		strcpy(m_ScriptedConversationBuffer[i].Context, "");
		strcpy(m_ScriptedConversationBuffer[i].Subtitle, "");
		m_ScriptedConversationBuffer[i].Listener = -1;
		m_ScriptedConversationBuffer[i].SpeechSlotIndex = -1;
		m_ScriptedConversationBuffer[i].Volume = AUD_SPEECH_NORMAL;
		m_ScriptedConversationBuffer[i].SpeechSlotIndex = 0;
	}
	m_ScriptedConversationNoAudioDelay = 0;
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		m_Ped[i] = NULL;
		strcpy(m_VoiceName[i], "");

		m_PedBuffer[i] = NULL;
		strcpy(m_VoiceNameBuffer[i], "");

		m_PedIsInvolvedInCurrentConversation[i] = false;
	}

	DYNAMICMIXER.StartScene(ATSTRINGHASH("CONVERSATION_TEST_SCENE", 0x8ba71b0a), &m_ConversationDuckingScene);
	if(m_ConversationDuckingScene)
		m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 0.f);

	m_Interrupter = NULL;
	m_InterruptingContext[0] = '\0';
	m_InterruptingVoiceNameHash = 0;
	NOTFINAL_ONLY(m_InterruptingPlaceholderVoiceNameHash = 0;)
	m_TimeToInterrupt = 0;
	m_TimeToRestartConversation = 0;
	m_TimeOfLastCollisionReaction = 0;
	m_TimeOfLastCollisionTrigger = 0;
	m_TimeToLowerCollisionLevel = 0;
	m_SpeechRestartOffset = 0;
	m_LineCheckedForRestartOffset = -1;
	m_InterruptTriggered = false;
	m_TimeLastFrame = 0;
	m_TimeElapsedSinceLastUpdate = 0;
	m_ConversationIsInterruptible = true;
	m_TimePlayersCarWasLastOnTheGround = 0;
	m_TimePlayersCarWasLastInTheAir = 0;
	m_TimeOfLastInAirInterrupt = 0;
	m_TimeLastRegisteredCarInAir = 0;
	m_ScriptSetNoDucking = false;

	m_HeadSetBeepOnLineIndex = -1;
	m_HeadSetBeepOffCachedVolume = 0.f;

	m_TimeOfLastCarPedCollision = 0;
	m_TimeLastTriggeredPedCollisionScreams = 0;
	for(int i=0; i<m_CarPedCollisions.GetMaxCount(); i++)
	{
		m_CarPedCollisions[i].Victim = NULL;
		m_CarPedCollisions[i].TimeOfCollision = 0;
	}

	// Set up our slots
	for (int i=0; i<AUD_MAX_SCRIPT_BANK_SLOTS; i++)
	{
		char slotName[32];  
		sprintf(slotName, "AMBIENT_SCRIPT_SLOT_%d", i); 
		m_BankSlots[i].BankSlot = audWaveSlot::FindWaveSlot(slotName);
		m_BankSlots[i].BankId = AUD_INVALID_BANK_ID;
		m_BankSlots[i].BankNameHash = g_NullSoundHash;
		m_BankSlots[i].ReferenceCount = 0;    
		m_BankSlots[i].HintLoaded = false;    
		m_BankSlots[i].State = AUD_SCRIPT_BANK_LOADED;
		m_BankSlots[i].TimeWeStartedWaitingToFinish = 0; 
		for(int j=0; j<AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS; j++)
		{
			m_BankSlots[i].NetworkScripts[j].Invalidate();
		}
		Assertf(m_BankSlots[i].BankSlot, "Failed to setup bank slot '%s'", slotName); 
	}
	
	// Cellphone-related stuff
	m_MobileRingType = AUD_CELLPHONE_NORMAL;

	m_LastDisplayedTextBlock = -1;

	for (s32 j=0; j<2; j++)
	{
		for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
		{
			m_ScriptedSpeechSlot[i][j].ScriptedSpeechSound = NULL;
			m_ScriptedSpeechSlot[i][j].AssociatedClip = NULL;
			m_ScriptedSpeechSlot[i][j].AssociatedDic = NULL;
			char slotName[32];
			sprintf(slotName, "SCRIPT_SPEECH_%d", (i+(j*AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD)));
			m_ScriptedSpeechSlot[i][j].WaveSlot = audWaveSlot::FindWaveSlot(slotName);
			//Assert(m_ScriptedSpeechSlot[i][j].WaveSlot);
			m_ScriptedSpeechSlot[i][j].LipsyncPredelay = -1;
			m_ScriptedSpeechSlot[i][j].PauseTime = 0;
			m_ScriptedSpeechSlot[i][j].VariationNumber = -1;
		}
	}

	m_StreamSlot = NULL;
	m_StreamSound = NULL;
	m_StreamType = AUD_PLAYSTREAM_UNKNOWN;
	m_StreamPosition.Set(ORIGIN);
#if GTA_REPLAY
	for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
	{
		m_StreamVariableNameHash[i] = g_NullSoundHash;
		m_StreamVariableValue[i] = 0.0f;
	}
#endif
	m_StreamEnvironmentGroup = NULL;
	m_ScriptStreamState = AUD_SCRIPT_STREAM_IDLE;
	m_LoadedStreamRef.SetInvalid();

	m_PreviewRingtoneSound = NULL;

	m_PoliceScannerSceneHash = 0;
	m_PoliceScannerSceneScriptIndex = -1;
	m_PoliceScannerIsApplied = false;

	m_MissionCompleteSound = NULL;
	m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
	m_MissionCompleteTimeOut = 0;
	m_MissionCompleteBankSlot = NULL;

	m_ExtraConvLines = 0;
	m_ConversationCodeLetter = 'A';
	m_ConversationCodeNumber = 1;
	safecpy(m_ConversationUserName, "UNKNOWN");

	m_ConversationLocation.Set(ORIGIN);

	if(PARAM_enableConvInterrupt.Get())
		g_DisableInterrupts = false;
	if(PARAM_disableConvInterrupt.Get())
		g_DisableInterrupts = true;
	if(PARAM_noAbortingFailedConv.Get())
		g_DoNotAbortFailedConversations = true;
	if(PARAM_noOverlapLipsyncAdjustment.Get())
		g_AdjustOverlapForLipsyncPredelay = false;
#if __BANK
	if(PARAM_trackMissingDialogue.Get())
		g_TrackMissingDialogue = true;
	if(PARAM_trackMissingDialogueMP.Get())
		g_TrackMissingDialogueMP = true;
	if(PARAM_displayScriptedSpeechLine.Get())
		g_DisplayScriptedLineInfo = true;
#endif

	sysMemZeroBytes<sizeof(m_FlagState)>(&m_FlagState);

	m_AnimTriggersControllingConversation = false;
	m_AnimTriggerReported = false;

	m_IsConversationPlaceholder = false;

	m_WaitingForFirstLineToPrepareToDuck = false;

	m_BlockDeathJingle = false;
	m_ScriptedSpeechTimerPaused = false;

	m_ScriptOwnedEnvGroup.Reset();

	m_AircraftWarningSpeechEnt.Init(NULL);
	m_AircraftWarningSpeechEnt.SetAmbientVoiceName(g_AircraftWarningMale1Hash, true);
	InitAircraftWarningInfo();

	m_TimeOfLastCollisionCDI = 0;

	m_AmbientSpeechAudioEntity.Init(NULL);

	m_TimeEnteringSlowMo = 0;
	m_TimeExitingSlowMo = 0;
	m_InSlowMoForPause = false;
	m_InSlowMoForSpecialAbility = false;

	m_TimeOfLastChopCollisionReaction = 0;

#if GTA_REPLAY
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_SLOTS; i++)
	{
		sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound = NULL;
		sm_ReplayScriptedSpeechSlot[i].VariationNumber = -1;
		sm_ReplayScriptedSpeechSlot[i].WaveSlot = NULL;
		sm_ReplayScriptedSpeechSlot[i].VoiceNameHash = 0;
		sm_ReplayScriptedSpeechSlot[i].ContextNameHash = 0;
		sm_ReplayScriptedSpeechSlot[i].TimeStamp = 0;
		sm_ReplayScriptedSpeechSlot[i].WaveSlotIndex = -1;
	}
#endif

#if __BANK
	if(PARAM_conversationspew.Get())
		g_ConversationDebugSpew = true;
	if(PARAM_scriptStreamSpew.Get())
		g_ScriptedStreamSpew = true;
	if(PARAM_scriptSoundsDebug.Get())
	{
		sm_DrawScriptTriggeredSounds = true;
		sm_InWorldDrawScriptTriggeredSounds = true;
	}
	m_TimeLoadingScriptBank = 0;
	m_FileHandle = INVALID_FILE_HANDLE;
	m_DialogueFileHandle = INVALID_FILE_HANDLE;
	m_HasInitializedDialogueFile = false;
	safecpy(g_szLowInterruptContext, "");
	safecpy(g_szMediumInterruptContext, "CRASH_GENERIC_INTERRUPT");
	safecpy(g_szHighInterruptContext, "CRASH_GENERIC_INTERRUPT");
#endif
#if !__FINAL
	m_TimeTriggeredSpeechPlayed = 0;
	m_TimeSpeechTriggered = 0;
	m_ShouldSetTimeTriggeredSpeechPlayedStat = false;
#endif
}

void audScriptAudioEntity::Shutdown()
{
	m_SpeechAudioEntity.Shutdown();
	m_AircraftWarningSpeechEnt.Shutdown();
	m_AmbientSpeechAudioEntity.Shutdown();
	StopStream();
	if(m_ConversationDuckingScene)
		m_ConversationDuckingScene->Stop();
	audEntity::Shutdown();

#if __BANK
	if(m_DialogueFileHandle != INVALID_FILE_HANDLE)
	{
		CFileMgr::CloseFile(m_DialogueFileHandle);
		m_DialogueFileHandle = INVALID_FILE_HANDLE;
	}
#endif
}

void audScriptAudioEntity::InitAircraftWarningInfo()
{
	safecpy(m_AircraftWarningInfo[AUD_AW_TARGETED_LOCK].Context, "TARGETED_LOCK");
	safecpy(m_AircraftWarningInfo[AUD_AW_MISSLE_FIRED].Context, "MISSLE_FIRED");
	safecpy(m_AircraftWarningInfo[AUD_AW_ACQUIRING_TARGET].Context, "ACQUIRING_TARGET");
	safecpy(m_AircraftWarningInfo[AUD_AW_TARGET_ACQUIRED].Context, "TARGET_ACQUIRED");
	safecpy(m_AircraftWarningInfo[AUD_AW_ALL_CLEAR].Context, "ALL_CLEAR");
	safecpy(m_AircraftWarningInfo[AUD_AW_PLANE_WARNING_STALL].Context, "PLANE_WARNING_STALL");
	safecpy(m_AircraftWarningInfo[AUD_AW_ALTITUDE_WARNING_LOW].Context, "ALTITUDE_WARNING_LOW");
	safecpy(m_AircraftWarningInfo[AUD_AW_ALTITUDE_WARNING_HIGH].Context, "ALTITUDE_WARNING_HIGH");
	safecpy(m_AircraftWarningInfo[AUD_AW_ENGINE_1_FIRE].Context, "ENGINE_1_FIRE");
	safecpy(m_AircraftWarningInfo[AUD_AW_ENGINE_2_FIRE].Context, "ENGINE_2_FIRE");
	safecpy(m_AircraftWarningInfo[AUD_AW_ENGINE_3_FIRE].Context, "ENGINE_3_FIRE");
	safecpy(m_AircraftWarningInfo[AUD_AW_ENGINE_4_FIRE].Context, "ENGINE_4_FIRE");
	safecpy(m_AircraftWarningInfo[AUD_AW_DAMAGED_SERIOUS].Context, "DAMAGED_SERIOUS");
	safecpy(m_AircraftWarningInfo[AUD_AW_DAMAGED_CRITICAL].Context, "DAMAGED_CRITICAL");
	safecpy(m_AircraftWarningInfo[AUD_AW_OVERSPEED].Context, "OVERSPEED");
	safecpy(m_AircraftWarningInfo[AUD_AW_TERRAIN].Context, "TERRAIN");
	safecpy(m_AircraftWarningInfo[AUD_AW_PULL_UP].Context, "PULL_UP");
	safecpy(m_AircraftWarningInfo[AUD_AW_LOW_FUEL].Context, "LOW_FUEL");

	m_ShouldUpdateAircraftWarning = false;

	InitAircraftWarningRaveSettings();
}

void audScriptAudioEntity::InitAircraftWarningRaveSettings()
{
	g_AircraftWarningSettings = audNorthAudioEngine::GetObject<AircraftWarningSettings>("AIRCRAFT_WARNING_SETTINGS");

	if(g_AircraftWarningSettings)
	{
		m_AircraftWarningInfo[AUD_AW_TARGETED_LOCK].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->TargetedLock.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_TARGETED_LOCK].MinTimeBetweenPlay = g_AircraftWarningSettings->TargetedLock.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_MISSLE_FIRED].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->MissleFired.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_MISSLE_FIRED].MinTimeBetweenPlay = g_AircraftWarningSettings->MissleFired.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ACQUIRING_TARGET].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->AcquiringTarget.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ACQUIRING_TARGET].MinTimeBetweenPlay = g_AircraftWarningSettings->AcquiringTarget.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_TARGET_ACQUIRED].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->TargetAcquired.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_TARGET_ACQUIRED].MinTimeBetweenPlay = g_AircraftWarningSettings->TargetAcquired.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ALL_CLEAR].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->AllClear.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ALL_CLEAR].MinTimeBetweenPlay = g_AircraftWarningSettings->AllClear.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_PLANE_WARNING_STALL].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->PlaneWarningStall.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_PLANE_WARNING_STALL].MinTimeBetweenPlay = g_AircraftWarningSettings->PlaneWarningStall.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ALTITUDE_WARNING_LOW].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->AltitudeWarningLow.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ALTITUDE_WARNING_LOW].MinTimeBetweenPlay = g_AircraftWarningSettings->AltitudeWarningLow.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ALTITUDE_WARNING_HIGH].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->AltitudeWarningHigh.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ALTITUDE_WARNING_HIGH].MinTimeBetweenPlay = g_AircraftWarningSettings->AltitudeWarningHigh.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_1_FIRE].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->Engine_1_Fire.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_1_FIRE].MinTimeBetweenPlay = g_AircraftWarningSettings->Engine_1_Fire.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_2_FIRE].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->Engine_2_Fire.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_2_FIRE].MinTimeBetweenPlay = g_AircraftWarningSettings->Engine_2_Fire.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_3_FIRE].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->Engine_3_Fire.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_3_FIRE].MinTimeBetweenPlay = g_AircraftWarningSettings->Engine_3_Fire.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_4_FIRE].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->Engine_4_Fire.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_ENGINE_4_FIRE].MinTimeBetweenPlay = g_AircraftWarningSettings->Engine_4_Fire.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_DAMAGED_SERIOUS].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->DamagedSerious.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_DAMAGED_SERIOUS].MinTimeBetweenPlay = g_AircraftWarningSettings->DamagedSerious.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_DAMAGED_CRITICAL].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->DamagedCritical.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_DAMAGED_CRITICAL].MinTimeBetweenPlay = g_AircraftWarningSettings->DamagedCritical.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_OVERSPEED].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->Overspeed.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_OVERSPEED].MinTimeBetweenPlay = g_AircraftWarningSettings->Overspeed.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_TERRAIN].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->Terrain.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_TERRAIN].MinTimeBetweenPlay = g_AircraftWarningSettings->Terrain.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_PULL_UP].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->PullUp.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_PULL_UP].MinTimeBetweenPlay = g_AircraftWarningSettings->PullUp.MinTimeBetweenPlay;
		m_AircraftWarningInfo[AUD_AW_LOW_FUEL].MaxTimeBetweenTriggerAndPlay = g_AircraftWarningSettings->LowFuel.MaxTimeBetweenTriggerAndPlay;
		m_AircraftWarningInfo[AUD_AW_LOW_FUEL].MinTimeBetweenPlay = g_AircraftWarningSettings->LowFuel.MinTimeBetweenPlay;
	}
	else
		naWarningf("Failed to find aircraft warning rave settings.  Using default values.");
}

u32 g_TimeBetweenCollisionReactionAndThresholdDowngrade = 15000;
void audScriptAudioEntity::UpdateConversation()
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
	m_TimeElapsedSinceLastUpdate = currentTime - m_TimeLastFrame;
#if __BANK && !RSG_ORBIS
	if(g_FakeConvAnimTrigger)
	{
		HandleConversaionAnimTrigger(NULL);
		g_FakeConvAnimTrigger = false;
	}

	if(!g_TrackMissingDialogue && !g_TrackMissingDialogueMP && m_DialogueFileHandle != INVALID_FILE_HANDLE)
	{
		CFileMgr::CloseFile(m_DialogueFileHandle);
		m_DialogueFileHandle = INVALID_FILE_HANDLE;
		m_HasInitializedDialogueFile = false;
	}
#endif
	m_SpeechAudioEntity.Update();
	m_AircraftWarningSpeechEnt.Update();
	m_AmbientSpeechAudioEntity.Update();
	// Update any conversations

	SlowMoType slowMoType = audNorthAudioEngine::GetActiveSlowMoMode();
	bool isSlowMo = audNorthAudioEngine::IsInSlowMo() || fwTimer::IsSystemPaused();
	CPed *player = CGameWorld::FindLocalPlayer();
	if (player && player->GetSpecialAbility())
	{
		if(m_InSlowMoForSpecialAbility && slowMoType == AUD_SLOWMO_GENERAL)
		{
			// Only set m_InSlowMoForSpecialAbility to false if we are no longer in special ability
			m_InSlowMoForSpecialAbility = !player->GetSpecialAbility()->IsActive() || !audNorthAudioEngine::IsPlayerSpecialAbilityFadingOut();
			isSlowMo = isSlowMo && !audNorthAudioEngine::IsPlayerSpecialAbilityFadingOut();
		}
	}

	if(isSlowMo && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowScriptedSpeechInSlowMo))
	{
		if(m_ColLevel != AUD_INTERRUPT_LEVEL_SLO_MO)
		{
			if(m_TimeEnteringSlowMo == 0)	
				m_TimeEnteringSlowMo = currentTime;
			if(currentTime - m_TimeEnteringSlowMo > 250 || slowMoType == AUD_SLOWMO_PAUSEMENU)
			{
				RegisterSlowMoInterrupt(slowMoType);
			}
		}
		m_TimeExitingSlowMo = currentTime;
	}
	else if(!isSlowMo)
	{
		m_TimeEnteringSlowMo = 0;
		if(m_ColLevel == AUD_INTERRUPT_LEVEL_SLO_MO && currentTime - m_TimeExitingSlowMo > (m_InSlowMoForPause ? 1000 : 500))
			StopSlowMoInterrupt();
	}

	if(audNorthAudioEngine::IsAudioPaused() && !g_AudioEngine.GetSoundManager().IsPaused(4))
		g_AudioEngine.GetSoundManager().Pause(4);
	else if(!audNorthAudioEngine::IsAudioPaused() && g_AudioEngine.GetSoundManager().IsPaused(4) && !m_ScriptedSpeechTimerPaused)
	{
		m_TimeToRestartConversation = g_AudioEngine.GetTimeInMilliseconds() + 500;
		g_AudioEngine.GetSoundManager().UnPause(4);
	}

	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if(playerVehicle)
	{
		bool notACar = playerVehicle->GetVehicleType() != VEHICLE_TYPE_CAR && 
			playerVehicle->GetVehicleType() != VEHICLE_TYPE_BIKE &&
			playerVehicle->GetVehicleType() != VEHICLE_TYPE_QUADBIKE &&
			playerVehicle->GetVehicleType() != VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE;

		if (playerVehicle->GetNumContactWheels() != 0 || notACar)
		{
			m_TimePlayersCarWasLastOnTheGround = currentTime;
		}
		else
		{
			m_TimePlayersCarWasLastInTheAir = currentTime;
		}
	}
	// Update headset beep state based on the current script flags before processing the conversation
	if(!IsFlagSet(audScriptAudioFlags::EnableHeadsetBeep))
	{			
		m_HeadSetBeepOnLineIndex = -1;
		m_TimeLastScriptedConvFinished = 0;
		m_TimeLastBeepOffPlayed = 0;
	}
	if (m_ScriptedConversationInProgress)
	{
		if(m_ScriptSetNoDucking && m_ConversationDuckingScene)
			m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 0.f);
		ProcessScriptedConversation(currentTime);
	}
	else if (m_TimeLastScriptedConvFinished + 200 < audNorthAudioEngine::GetCurrentTimeInMs())
	{
		if(m_HeadSetBeepOnLineIndex >= 0 && !IsFlagSet(audScriptAudioFlags::DisableHeadsetOffBeep))
		{
			audSoundInitParams beepInitParams; 
			beepInitParams.Pan = 0;
			beepInitParams.Predelay = 300;
			beepInitParams.Volume = m_HeadSetBeepOffCachedVolume;

			CreateAndPlaySound(ATSTRINGHASH("DLC_HEISTS_HEADSET_CALL_END_BEEP", 0x6511F36C),&beepInitParams);
			m_TimeLastBeepOffPlayed = audNorthAudioEngine::GetCurrentTimeInMs();
			//Reset the cached volume for next time.
			m_HeadSetBeepOffCachedVolume = 0.f;
		}
		m_HeadSetBeepOnLineIndex = -1;
		m_TimeLastScriptedConvFinished = 0;
	}
	m_TimeLastFrame = currentTime;
}

void audScriptAudioEntity::PreUpdateService(u32 timeInMs)
{
#if GTA_REPLAY
	UpdateReplayScriptedSpeechSlots();
#endif

	// Update our script bank slots and sounds, to manage their states
	UpdateScriptBankSlots(timeInMs);
	UpdateScriptSounds(timeInMs);
	UpdateMissionComplete();
	UpdateCollisionInfo(timeInMs);
	if(m_ShouldUpdateAircraftWarning)
		UpdateAircraftWarning();
	
	if(m_ScriptStreamState == AUD_SCRIPT_STREAM_PLAYING)
	{
		if(!m_StreamSound)
		{
			if(m_StreamSlot)
			{
				m_StreamSlot->Free();
				m_StreamSlot = NULL;
				m_StreamType = AUD_PLAYSTREAM_UNKNOWN;
			}
			m_ScriptStreamState = AUD_SCRIPT_STREAM_IDLE;
			m_StreamEnvironmentGroup = NULL;
#if GTA_REPLAY
			for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
			{
				m_StreamVariableNameHash[i] = g_NullSoundHash;
				m_StreamVariableValue[i] = 0.0f;
			}
#endif
#if __BANK
			if(g_ScriptedStreamSpew)
			{
				audDebugf1("[audScriptedStream] State: Playing but no sound was found, free stuff and set the state to idle");
			}
#endif 
		}
		else
		{
			if(m_StreamTrackerEntity)
			{
				m_StreamSound->SetRequestedPosition(m_StreamTrackerEntity->GetTransform().GetPosition());
				// If it was played on a ped/vehicle, we'd share that envgroup and those positions are updated, 
				// otherwise we need to update the interior information and position ourselves
				if(m_StreamEnvironmentGroup && !m_StreamTrackerEntity->GetIsTypePed() && !m_StreamTrackerEntity->GetIsTypeVehicle())
				{
					m_StreamEnvironmentGroup->SetSource(m_StreamTrackerEntity->GetTransform().GetPosition());
					m_StreamEnvironmentGroup->SetInteriorInfoWithEntity(m_StreamTrackerEntity);
				}
			}
		}
	}

	// Make sure the interference gets faded out if the radio does
	if(m_MobileInterferenceSound)
	{
		m_MobileInterferenceSound->SetRequestedVolume(g_RadioAudioEntity.GetVolumeOffset());

		CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
		if(!playerVehicle || playerVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() != AUD_VEHICLE_CAR || !g_RadioAudioEntity.IsVehicleRadioOn())
		{
			m_MobileInterferenceSound->StopAndForget();
		}
	}
}

void audScriptAudioEntity::PostUpdate()
{
	for (int i=0; i<AUD_MAX_SCRIPTS; i++)
	{
		if(m_Scripts[i].ScriptThreadId && m_Scripts[i].m_ScriptisShuttingDown)
		{
			m_Scripts[i].m_ScriptShutDownDelay -= (fwTimer::GetTimeInMilliseconds()-fwTimer::GetPrevElapsedTimeInMilliseconds());
			RemoveScript(m_Scripts[i].ScriptThreadId);
		}
	}

	for (s32 i=0; i<AUD_MAX_SCRIPT_SOUNDS; i++)
	{
		// clear out network triggered sounds where the calling script has terminated
		if (m_ScriptSounds[i].NetworkId != audScriptSound::INVALID_NETWORK_ID)
		{
			if(m_ScriptSounds[i].ScriptId.IsValid() && !CTheScripts::GetScriptHandlerMgr().IsScriptActive(m_ScriptSounds[i].ScriptId))
			{
				naDebugf3("PostUpdate: Stopping Sound - SoundID:%d, NetworkID:%d", i, m_ScriptSounds[i].NetworkId);
				
				// This sound belongs to this script, so stop it (if it still exists).
				if (m_ScriptSounds[i].Sound)
				{
					m_ScriptSounds[i].Sound->StopAndForget();
				}
				ResetScriptSound(i);
			}
		}

		if(m_ScriptSounds[i].ScriptIndex >= 0 || m_ScriptSounds[i].NetworkId != audScriptSound::INVALID_NETWORK_ID)
		{
			// This is currently used - do we need an explicit 'in use' flag rather than just this? (script slot reuse on orphanable sounds causing a problem?)
			if(!m_ScriptSounds[i].ScriptHasReference)
			{
				// The script hasn't got an explicit reference to this sound, so if the sound in it's finished, we can remove the association and free the slot.
				if (!m_ScriptSounds[i].Sound)
				{
					ResetScriptSound(i);
				}
			}
		}
	}

	// clear out bank references where the calling script has terminated
	for (s32 i=0; i<AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
	{
		for (s32 j=0; j<AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS; j++)
		{
			if(m_BankSlots[i].NetworkScripts[j].IsValid() && !CTheScripts::GetScriptHandlerMgr().IsScriptActive(m_BankSlots[i].NetworkScripts[j]))
			{
				naAssertf(m_BankSlots[i].ReferenceCount, "About to decrement a ref count of 0 - looks like something leaked");
				m_BankSlots[i].ReferenceCount--; 
				naDisplayf("PostUpdate: Dereferencing Bank - BankSlot:%d, NetSlot:%d, BankID:%d, Ref:%d", i, j, m_BankSlots[i].BankId, m_BankSlots[i].ReferenceCount);
				BANK_ONLY(LogNetworkScriptBankRequest("PostUpdate: Dereferencing Bank - ",m_BankSlots[i].NetworkScripts[j].GetScriptName(),m_BankSlots[i].BankId,i));
				m_BankSlots[i].NetworkScripts[j].Invalidate();
			}
		}
	}
#if __ASSERT
	for(const audScriptBankSlot &slot : m_BankSlots)
	{
		if(slot.BankSlot && slot.State == AUD_SCRIPT_BANK_LOADED && slot.BankId != AUD_INVALID_BANK_ID)
		{
			if(!audWaveSlot::VerifyLoadedBankWaveSlot(slot.BankSlot, slot.BankId))
			{
				const audWaveSlot* waveSlot = audWaveSlot::FindLoadedBankWaveSlot(slot.BankId);
				if(slot.BankSlot != waveSlot)
				{
					// if we are going to hit the assert, wait for 10 milliseconds in case we have a timing issue 
					// and we haven't given enough time to the waveslot code to fill in the table. 
					sysIpcSleep(30);

					if(!audWaveSlot::VerifyLoadedBankWaveSlot(slot.BankSlot, slot.BankId))
					{
						waveSlot = audWaveSlot::FindLoadedBankWaveSlot(slot.BankId);

						naAssertf(slot.BankSlot == waveSlot,"Script loaded banks fell out of step with audWaveSlot::sm_LoadedBankTable, [Script bank: %s, slot: %s, id: %u, loaded id: %u] [LoadedBank: %s, slot: %s, id: %u]",
							slot.BankSlot->GetLoadedBankName(),
							slot.BankSlot->GetSlotName(),
							slot.BankId, 
							slot.BankSlot->GetLoadedBankId(),
							waveSlot ? waveSlot->GetCachedLoadedBankName(): "", 
							waveSlot ? waveSlot->GetSlotName() : "", 
							waveSlot ? waveSlot->GetLoadedBankId() : 0);
					}
				}
			}
		}
	}
#endif
}

void audScriptAudioEntity::UpdateMissionComplete()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditorActive())
	{
		StopMissionCompleteSound(); 
		m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
		m_ShouldShowMissionCompleteUI = true;
		if(FreeUpScriptBank(m_MissionCompleteBankSlot))
		{
			m_MissionCompleteBankSlot = NULL;
		}
	}
#endif
	switch(m_MissionCompleteState)
	{
	case AUD_MISSIONCOMPLETE_PREPARING:
		m_ShouldShowMissionCompleteUI = false;
		if(!m_MissionCompleteSound)
		{
			audSoundInitParams initParams;
			m_MissionCompleteBankSlot =	AllocateScriptBank();
			if(!m_MissionCompleteBankSlot)
			{
				m_MissionCompleteTimeOut += (fwTimer::GetTimeStepInMilliseconds());
				// we failed to create the sound
				if(m_MissionCompleteTimeOut > g_MaxTimeToAllocateSlot)
				{
					naErrorf("Reached time out when trying to allocate a script slot for the mission complete sound.");
					m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
					m_ShouldShowMissionCompleteUI = true;
				}
				break;
			}
			else
			{
				initParams.WaveSlot = m_MissionCompleteBankSlot;
				initParams.TimerId = m_MissionCompleteTimerId; // defaults to unpausable; needs to be stopped by the frontend music
				CreateSound_PersistentReference(m_MissionCompleteHash, &m_MissionCompleteSound, &initParams);
			}
		}

		if(m_MissionCompleteSound)
		{
			audPrepareState prepareState = m_MissionCompleteSound->Prepare(m_MissionCompleteBankSlot, true);
			if(prepareState == AUD_PREPARED)
			{
				m_MissionCompleteFadeTime = ~0U;
				m_MissionCompleteHitTime = 0;
				m_MissionCompleteTimeOut = 0;

				const WrapperSound *wrapperSound = SOUNDFACTORY.DecompressMetadata<WrapperSound>(m_MissionCompleteHash);
				if(wrapperSound)
				{
					const MultitrackSound *multitrack = SOUNDFACTORY.DecompressMetadata<MultitrackSound>(wrapperSound->SoundRef);
					if(multitrack)
					{
						const SimpleSound *simple1 = SOUNDFACTORY.DecompressMetadata<SimpleSound>(multitrack->SoundRef[0].SoundId);
						const SimpleSound *simple2 = SOUNDFACTORY.DecompressMetadata<SimpleSound>(multitrack->SoundRef[1].SoundId);

						if(simple1 && simple2)
						{				
							audWaveRef waveRef;
							audWaveMarkerList markers;
							u32 sampleRate = 0;
							if(waveRef.Init(m_MissionCompleteBankSlot, simple1->WaveRef.WaveName))
							{
								waveRef.FindMarkerData(markers);
								const audWaveFormat *format = waveRef.FindFormat();
								if(format)
								{
									sampleRate = format->SampleRate;
								}
							}
							if(markers.GetCount() == 0)
							{
								if(waveRef.Init(m_MissionCompleteBankSlot, simple2->WaveRef.WaveName))
								{
									const audWaveFormat *format = waveRef.FindFormat();
									if(format)
									{
										sampleRate = format->SampleRate;
									}
								}
							}						

							for(u32 i=0; i< markers.GetCount(); i++)
							{
								const u32  fadeHash = ATSTRINGHASH("FADE", 0x06e9c2bb6);
								const u32  hitHash = ATSTRINGHASH("UIHIT", 0xD8846402);
								if(markers[i].categoryNameHash == fadeHash)
								{
									m_MissionCompleteFadeTime = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
								}
								else if (markers[i].categoryNameHash == hitHash)
								{
									m_MissionCompleteHitTime = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, sampleRate);
								}
							}
						}
					}
				}
				// we're now prepared; go into a waiting state until the delay is up
				m_MissionCompleteState = AUD_MISSIONCOMPLETE_DELAY;
			}
			else if(prepareState == AUD_PREPARE_FAILED)
			{
				m_MissionCompleteSound->StopAndForget();
				m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
				m_ShouldShowMissionCompleteUI = true;
				if(FreeUpScriptBank(m_MissionCompleteBankSlot))
				{
					m_MissionCompleteBankSlot = NULL;
				}
			}
			else if ( prepareState == AUD_PREPARING)
			{
				// we can also time out when preparing.
				m_MissionCompleteTimeOut += (fwTimer::GetTimeStepInMilliseconds());
				// we failed to create the sound
				if(m_MissionCompleteTimeOut > g_MaxTimeToAllocateSlot)
				{
					naErrorf("Reached time out when preparing the mission complete sound.");
					m_MissionCompleteSound->StopAndForget();
					m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
					m_ShouldShowMissionCompleteUI = true;
					if(FreeUpScriptBank(m_MissionCompleteBankSlot))
					{
						m_MissionCompleteBankSlot = NULL;
					}
				}
				break;
			}
		}
		else
		{
			// we failed to create the sound
			naErrorf("Failed to create mission complete sound");
			m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
			m_ShouldShowMissionCompleteUI = true;
			if(FreeUpScriptBank(m_MissionCompleteBankSlot))
			{
				m_MissionCompleteBankSlot = NULL;
			}
		}
		break;
	case AUD_MISSIONCOMPLETE_DELAY:
		m_ShouldShowMissionCompleteUI = false;
		if(!IsFlagSet(audScriptAudioFlags::HoldMissionCompleteWhenPrepared))
		{
			if(IsFlagSet(audScriptAudioFlags::AvoidMissionCompleteDelay) || (fwTimer::GetTimeInMilliseconds() > m_MissionCompleteTriggerTime + g_MissionCompleteDelay))
			{
				if(m_MissionCompleteSound)
				{
					m_MissionCompleteSound->Play();
					m_MissionCompleteState = AUD_MISSIONCOMPLETE_PLAYING;
				}
				else
				{
					naErrorf("No mission complete sound during AUD_MISSIONCOMPLETE_DELAY state");
					m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
					m_ShouldShowMissionCompleteUI = true;
					if(FreeUpScriptBank(m_MissionCompleteBankSlot))
					{
						m_MissionCompleteBankSlot = NULL;
					}
				}
			}
		}
		else
		{
			m_MissionCompleteTriggerTime = fwTimer::GetTimeInMilliseconds();
		}
		break;
	case AUD_MISSIONCOMPLETE_PLAYING:
		if(!m_MissionCompleteSound)
		{
			m_MissionCompleteState = AUD_MISSIONCOMPLETE_IDLE;
			m_ShouldShowMissionCompleteUI = true;
			if(FreeUpScriptBank(m_MissionCompleteBankSlot))
			{
				m_MissionCompleteBankSlot = NULL;
			}
		}
		else 
		{
			const s32 playTimeMs = (s32)m_MissionCompleteSound->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(m_MissionCompleteTimerId));
			if(playTimeMs >= (s32)m_MissionCompleteHitTime)
			{
				m_ShouldShowMissionCompleteUI = true;
			}
		}
		break;
	default:
		break;
	}
}

void audScriptAudioEntity::TriggerMissionComplete(const char *  missionCompleteId, const s32 timerId /* = 3 */)
{
	char missionCompleteName[64];
	formatf(missionCompleteName, "SCRIPTED_MISSION_COMPLETE_%s", missionCompleteId);
	if(m_MissionCompleteSound)
	{
		naErrorf("Called TriggerMissionComplete with %s while %s is already playing", missionCompleteName, m_MissionCompleteSound->GetName());
	}
	if(naVerifyf(m_MissionCompleteState == AUD_MISSIONCOMPLETE_IDLE, "In TriggerMissionComplete we should be in AUD_MISSIONCOMPLETE_DELAY (%d) but our state is %d", AUD_MISSIONCOMPLETE_IDLE, m_MissionCompleteState))
	{

#if !__NO_OUTPUT
		if(!g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataPtr(missionCompleteName))
		{
			audErrorf("Invalid mission complete identifier: %s (failed to find %s)", missionCompleteId, missionCompleteName);
		}
#endif 
		m_MissionCompleteHash = atStringHash(missionCompleteName);
		m_MissionCompleteState = AUD_MISSIONCOMPLETE_PREPARING;
		m_MissionCompleteTriggerTime = fwTimer::GetTimeInMilliseconds();
		m_MissionCompleteTimeOut = 0;
		m_MissionCompleteTimerId = timerId;
		m_ShouldShowMissionCompleteUI = false; 
	}
}
bool audScriptAudioEntity::IsPreparingMissionComplete() const
{
	return (m_MissionCompleteSound && m_MissionCompleteSound->GetPlayState() == AUD_SOUND_PREPARING);
}
void audScriptAudioEntity::StopMissionCompleteSound()
{
	if(m_MissionCompleteSound)
	{
		m_MissionCompleteSound->StopAndForget();
	}
}
bool audScriptAudioEntity::IsPlayingMissionComplete() const
{
	return (m_MissionCompleteSound && m_MissionCompleteSound->GetPlayState() == AUD_SOUND_PLAYING);
}

f32 audScriptAudioEntity::ComputeRadioVolumeForMissionComplete() const
{
	if(m_MissionCompleteState == AUD_MISSIONCOMPLETE_PREPARING || m_MissionCompleteState == AUD_MISSIONCOMPLETE_DELAY)
	{

		const u32 timeInMs = fwTimer::GetTimeInMilliseconds();
		const s32 timeTilTrigger = (s32)m_MissionCompleteTriggerTime + (s32)g_MissionCompleteDelay - (s32)timeInMs;
		dev_s32 radioFadeOutTime = 500;
		if(timeTilTrigger > radioFadeOutTime)
		{
			return 0.f;
		}
		else
		{
			const f32 interp = Max(0.f, (timeTilTrigger / (f32)radioFadeOutTime));
			return audDriverUtil::ComputeDbVolumeFromLinear(interp);
		}		
	}
	else if(m_MissionCompleteSound && m_MissionCompleteState == AUD_MISSIONCOMPLETE_PLAYING)
	{
		const s32 playTimeMs = (s32)m_MissionCompleteSound->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(m_MissionCompleteTimerId));
		if(playTimeMs < (s32)m_MissionCompleteFadeTime)
		{
			return -100.f;
		}
		else
		{
			const s32 duration = m_MissionCompleteSound->ComputeDurationMsIncludingStartOffsetAndPredelay(NULL);
			if(playTimeMs >= duration)
			{
				return 0.f;
			}
			else
			{
				f32 fadeInterp = ((f32)(playTimeMs - (s32)m_MissionCompleteFadeTime) / (f32)(duration - (s32)m_MissionCompleteFadeTime));
				return audDriverUtil::ComputeDbVolumeFromLinear(fadeInterp);
			}
		}
	}
	return 0.f;
}
void audScriptAudioEntity::PlayNPCPickup(const u32 pickupHash, const Vector3 &pickupPosition)
{
	audSoundInitParams initParams;	
	initParams.Position = pickupPosition;
	audSoundSet soundSet;
	audMetadataRef pickupSoundRef = g_NullSoundRef;
	soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_WEAPONS_PICKUPS_NPC_SOUNDSET", 0xF35C567B));
	pickupSoundRef = soundSet.Find(pickupHash);
	if(pickupSoundRef == g_NullSoundRef)
	{
		soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_VEHICLE_PICKUPS_NPC_SOUNDSET", 0xF5E3A26A));
		pickupSoundRef = soundSet.Find(pickupHash);
		if(pickupSoundRef == g_NullSoundRef)
		{
			soundSet.Init(ATSTRINGHASH("HUD_FRONTEND_STANDARD_PICKUPS_NPC_SOUNDSET", 0x1D46A6A2));
			pickupSoundRef = soundSet.Find(pickupHash);
		}
	}
	CreateDeferredSound(pickupSoundRef, NULL,&initParams);
}		
void audScriptAudioEntity::SetDesiredPedWallaDensity(f32 desiredDensity, f32 desiredDensityApplyFactor, bool interior)
{
	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	ASSERT_ONLY(bool success = ) g_AmbientAudioEntity.SetScriptDesiredPedDensity(desiredDensity, desiredDensityApplyFactor, m_Scripts[scriptIndex].ScriptThreadId, interior);
	audAssertf(success, "Script %s attempting to set ped walla density when another thread is already doing so", CTheScripts::GetCurrentScriptName());
}


void audScriptAudioEntity::SetDesiredPedWallaSoundSet(const char* soundSetName, bool interior)
{
	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	ASSERT_ONLY(bool success = ) g_AmbientAudioEntity.SetScriptWallaSoundSet(atStringHash(soundSetName), m_Scripts[scriptIndex].ScriptThreadId, interior);
	audAssertf(success, "Script %s failed to set walla sound set to %s - is it a valid sound set name?", CTheScripts::GetCurrentScriptName(), soundSetName);
}

void audScriptAudioEntity::UpdateScriptBankSlots(u32 UNUSED_PARAM(timeInMs))
{
	// No need to do anything - as bank requests are called repeatedly, we can sort everything from there.
#if GTA_REPLAY
	if(CReplayMgr::IsRecording())
	{
		CReplayMgr::RecordFx<CPacketSndLoadScriptBank>(CPacketSndLoadScriptBank());
	}
#endif
}

void audScriptAudioEntity::UpdateScriptSounds(u32 UNUSED_PARAM(timeInMs))
{
	for (s32 i=0; i<AUD_MAX_SCRIPT_SOUNDS; i++)
	{
		if (m_ScriptOwnedEnvGroup.IsSet(i) && m_ScriptSounds[i].ScriptIndex >= 0 && m_ScriptSounds[i].Sound && m_ScriptSounds[i].EnvironmentGroup)
		{
			if(m_ScriptSounds[i].Entity)
			{
				// removed the assert and added if check to avoid setting the environment group, which should update automatically for peds and vehicles
				//naAssertf(!(m_ScriptSounds[i].Entity->GetIsTypePed() || m_ScriptSounds[i].Entity->GetIsTypeVehicle()), 
				//	"We shouldn't own an environmentGroup if we're playing off an entity that already has an environmentGroup.  Sound: %s", m_ScriptSounds[i].Sound->GetName());
				if(!(m_ScriptSounds[i].Entity->GetIsTypePed() || m_ScriptSounds[i].Entity->GetIsTypeVehicle()))
				{
					m_ScriptSounds[i].EnvironmentGroup->SetSource(m_ScriptSounds[i].Entity->GetTransform().GetPosition());
					m_ScriptSounds[i].EnvironmentGroup->SetInteriorInfoWithEntity(m_ScriptSounds[i].Entity);
				}
			}
			else
			{
				m_ScriptSounds[i].EnvironmentGroup->SetSource(m_ScriptSounds[i].Sound->GetRequestedPosition());
			}
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && m_ScriptSounds[i].Sound && m_ScriptSounds[i].ShouldRecord)
		{
			RecordUpdateScriptSlotSound(m_ScriptSounds[i].SoundSetHash, m_ScriptSounds[i].SoundNameHash, &m_ScriptSounds[i].InitParams, m_ScriptSounds[i].Entity, m_ScriptSounds[i].Sound);
		}

		if(CReplayMgr::ShouldRecord() && m_ScriptSounds[i].Sound && (m_ScriptSounds[i].VariableNameHash[0] != g_NullSoundHash || m_ScriptSounds[i].VariableNameHash[1] != g_NullSoundHash))
		{
			CReplayMgr::RecordPersistantFx<CPacketScriptSetVarOnSound>(
				CPacketScriptSetVarOnSound(m_ScriptSounds[i].VariableNameHash, m_ScriptSounds[i].VariableValue),
				CTrackedEventInfo<tTrackedSoundType>(m_ScriptSounds[i].Sound));
		}
#endif
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && m_StreamSound && (m_StreamVariableNameHash[0] != g_NullSoundHash || m_StreamVariableNameHash[1] != g_NullSoundHash))
	{
		CReplayMgr::RecordPersistantFx<CPacketScriptSetVarOnSound>(
			CPacketScriptSetVarOnSound(m_StreamVariableNameHash, m_StreamVariableValue),
			CTrackedEventInfo<tTrackedSoundType>(m_StreamSound));
	}
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && m_StreamSound && !m_DisableReplayScriptStreamRecording)
	{
		u32 currentOffset = m_StreamSound->GetCurrentPlayTime();
		switch(m_StreamType)
		{
		case AUD_PLAYSTREAM_ENTITY:
			// Stream tracker entity may have been destroyed since the stream started, in which case it isn't valid to record a packet referencing it.
			if(m_StreamTrackerEntity)
			{
				CReplayMgr::ReplayRecordPlayStreamFromEntity(m_StreamTrackerEntity, m_StreamSound, sm_CurrentStreamName, currentOffset, sm_CurrentSetName);
			}
			break;

		case AUD_PLAYSTREAM_POSITION:
			CReplayMgr::ReplayRecordPlayStreamFromPosition(m_StreamPosition, m_StreamSound, sm_CurrentStreamName, currentOffset, sm_CurrentSetName);
			break;

		case AUD_PLAYSTREAM_FRONTEND:
			CReplayMgr::ReplayRecordPlayStreamFrontend(m_StreamSound, sm_CurrentStreamName, currentOffset, sm_CurrentSetName);
			break;
		default:
			break;
		}
	}
#endif

#endif
}

void audScriptAudioEntity::HandleScriptAudioEvent(u32 hash, CEntity* entity)
{
	audScriptAudioEntity::sm_ScriptActionTable.InvokeActionHandler(hash, entity);
}

void audScriptAudioEntity::DefaultScriptActionHandler(u32 hash, void *context)
{
	CEntity* entity = (CEntity*)context;
	naEnvironmentGroup* environmentGroup = NULL;
	audTracker* tracker = NULL;
	if (entity)
	{
		switch (entity->GetType())
		{
			case ENTITY_TYPE_PED:
				{
					CPed* ped = (CPed*) entity;
					environmentGroup = ped->GetPedAudioEntity()->GetEnvironmentGroup(true);
					tracker = ((CPed*)entity)->GetPlaceableTracker();
					break;
				}

			case ENTITY_TYPE_VEHICLE:
				{
					CVehicle* vehicle = (CVehicle*) entity;
					environmentGroup = vehicle->GetVehicleAudioEntity()->GetEnvironmentGroup();
					tracker = ((CVehicle*)entity)->GetPlaceableTracker();
					break;
				}

			case ENTITY_TYPE_OBJECT:
				{
					environmentGroup = g_ScriptAudioEntity.CreateScriptEnvironmentGroup(entity, entity->GetTransform().GetPosition());
					tracker = ((CObject*)entity)->GetPlaceableTracker();
					break;
				}
		}
	}

	// As a default, play the sound, with occlusion group and tracker, where we have one.
	audSoundInitParams initParams;
	initParams.EnvironmentGroup = environmentGroup;
	initParams.Tracker = tracker;
	g_ScriptAudioEntity.CreateAndPlaySound(hash, &initParams);
}

void audScriptAudioEntity::UpdateCollisionInfo(u32 timeInMs)
{
	if(m_TimeToLowerCollisionLevel != 0 && g_AudioEngine.GetTimeInMilliseconds() > m_TimeToLowerCollisionLevel)
	{
		switch(m_ColLevel)
		{
		case AUD_INTERRUPT_LEVEL_COLLISION_HIGH:
			m_ColLevel = AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM;
			m_TimeToLowerCollisionLevel += 10000;
			break;
		case AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM:
			m_ColLevel = AUD_INTERRUPT_LEVEL_COLLISION_LOW;
			m_TimeToLowerCollisionLevel += 10000;
			break;
		case AUD_INTERRUPT_LEVEL_COLLISION_LOW:
			m_ColLevel = AUD_INTERRUPT_LEVEL_COLLISION_NONE;
			m_TimeToLowerCollisionLevel = 0;
			break;
		default:
			break;
		}
	}

	//Only do this check a couple times a second, and only if we've hit a ped recently
	if(m_TimeOfLastCarPedCollision != 0 && fwTimer::GetFrameCount() % 15 == 0 &&
		timeInMs - m_TimeLastTriggeredPedCollisionScreams > g_TimeToWaitBetweenRunnedOverReactions
		)
	{
		u8 numPedsHitRecently= 0;
		for(int i=0; i<m_CarPedCollisions.GetMaxCount(); i++)
		{
			if(m_CarPedCollisions[i].TimeOfCollision > timeInMs - g_TimespanToHitPedForRunnedOverReaction)
				numPedsHitRecently++;
		}
		if(numPedsHitRecently >= g_MinPedsHitForRunnedOverReaction)
		{
			audSpeechAudioEntity::TriggerCollisionScreams();
			m_TimeLastTriggeredPedCollisionScreams = timeInMs;
		}
	}
}

void audScriptAudioEntity::ProcessScriptedConversation(u32 timeInMs)
{
#if !__FINAL
	if(m_ShouldSetTimeTriggeredSpeechPlayedStat)
	{
		PF_SET(TimeTilTriggeredSpeechPlayed, m_TimeTriggeredSpeechPlayed - m_TimeSpeechTriggered);
		m_ShouldSetTimeTriggeredSpeechPlayedStat = false;
	}
#endif

#if __BANK
	if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0 && !m_ScriptedConversation[m_PlayingScriptedConversationLine].DisplayedDebug)
	{
		m_ScriptedConversation[m_PlayingScriptedConversationLine].DisplayedDebug = true;
		Displayf("[Conv Spew] ProcessScriptedConversation %s", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
	}
#endif
	if(m_ScriptedConversationOverlapTriggerTime == 0 && m_OverlapTriggerTimeAdjustedForPredelay && !m_InterruptTriggered)
	{
		m_ScriptedConversationOverlapTriggerTime = timeInMs;
	}

	m_DuckingForScriptedConversation = false; // only set later if we're saying something

	//char convDebug[128] = "";
	//formatf(convDebug, sizeof(convDebug), "Processing: line: %d; preload: %d; highestLoaded: %d", m_PlayingScriptedConversationLine, m_PreLoadingSpeechLine, m_HighestLoadedLine);
	//grcDebugDraw::PrintToScreenCoors(convDebug,15,15);
	if (g_AudioEngine.GetSoundManager().IsPaused(4))
	{
		if(m_ScriptedConversationOverlapTriggerTime != 0 && m_TimeTrueOverlapWasSet == 0)
			m_ScriptedConversationOverlapTriggerTime += timeInMs - m_TimeLastFrame;
		return;
	}

	if(m_ExternalStreamConversationSFXPlaying)
	{
		if(m_ScriptedConversationSfx)
			return;
		else
		{
			m_ExternalStreamConversationSFXPlaying = false;
		}
	}
	// Block peds ambient anims, as appropriate
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		if (m_Ped[i])
		{
			if (m_BlockAnimTime[i] > timeInMs)
			{
				m_Ped[i]->SetAmbientAnimsBlocked(true);
			}
			else if (m_BlockAnimTime[i] > 0)
			{
				m_Ped[i]->SetAmbientAnimsBlocked(true);
			}
		}
	}

	if(m_TimeToInterrupt != 0 && m_TimeToInterrupt < timeInMs)
	{
		naConvDebugf1("Pausing/Interrupting conversation - m_TimeToInterrupt (%u) < timeInMs (%u)", m_TimeToInterrupt, timeInMs);
		PauseScriptedConversation(false, true);
		InterruptConversation();
	}
	if(m_TimeToRestartConversation != 0 && m_TimeToRestartConversation < timeInMs && !ShouldWePauseScriptedConversation(timeInMs, true))
	{
		naConvDebugf1("Restarting Conversation - m_TimeToRestartConversation (%u) < timeInMs (%u)", m_TimeToRestartConversation, timeInMs);
		//Now that we kill the conversation in ShouldWePause...if the car has been flipped or in the air too long, this may be false and we need
		//	to return immediately.
		if(!m_ScriptedConversationInProgress)
		{
			return;
		}

		//adjust overlap time
		if(m_ScriptedConversationOverlapTriggerTime != 0 && m_TimeTrueOverlapWasSet == 0)
		{
			m_ScriptedConversationOverlapTriggerTime += timeInMs - m_TimeLastFrame;
			naConvDebugf1("Adding %u to make trigger time %u at point 1 %s %u", timeInMs - m_TimeLastFrame, m_ScriptedConversationOverlapTriggerTime,
				m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
		}
		if(m_ColLevel == AUD_INTERRUPT_LEVEL_SLO_MO)
		{
			return;
		}

		RestartScriptedConversation(true);
	}

	if(m_InterruptTriggered && m_TimeToRestartConversation == 0)
	{
		audSpeechAudioEntity* speechAudioEntity = &m_SpeechAudioEntity;
		if( m_Interrupter && m_Interrupter->GetSpeechAudioEntity())
		{
			speechAudioEntity = m_Interrupter->GetSpeechAudioEntity();
		}
		if(speechAudioEntity)
		{
			if(m_CheckInterruptPreloading)
			{
				bool playTimeOverZero = false;

				if(!speechAudioEntity->IsPreloadedSpeechReady() && timeInMs - m_InterruptSpeechRequestTime > g_TimeToWaitForInterruptToPrepare)
				{
					naWarningf("Interrupt speech taking too long to load.  Killing it and moving on with the conversation.");
					speechAudioEntity->StopSpeech();
					m_ScriptedConversationOverlapTriggerTime = timeInMs;
					m_TimeToRestartConversation = timeInMs;
					return;
				}
				else if(speechAudioEntity->IsPreloadedSpeechPlaying(&playTimeOverZero) && playTimeOverZero)
				{
					m_CheckInterruptPreloading = false;
				}
			}
			if(!speechAudioEntity->IsAmbientSpeechPlaying() && !ShouldWePauseScriptedConversation(timeInMs, true))
			{
				m_ScriptedConversationOverlapTriggerTime = timeInMs;
				m_TimeToRestartConversation = timeInMs;
				return;
			}
		}
		else
		{
			m_InterruptTriggered = false;
		}
	}

	if(g_AdjustOverlapForLipsyncPredelay && m_PlayingScriptedConversationLine >= 0)
	{
		AdjustOverlapForLipsyncPredelay(timeInMs);
	}

	// don't play for a wee while after the last conversation - 250ms - this should stop any wave slot batchload asserts
	// also allows a wee bit of time - 250ms - for ambient anim blocking.
	if (timeInMs < m_NextTimeScriptedConversationCanStart)
	{
		return;
	}

	// Also wait until any of the chars have stopped speaking - this'll just be ambient speech - and add on a wee bit of time after that too.
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		if (m_Ped[i] && m_PedIsInvolvedInCurrentConversation[i])
		{
			if (m_Ped[i]->GetSpeechAudioEntity() && m_Ped[i]->GetSpeechAudioEntity()->IsAmbientSpeechPlaying())
			{
				m_NextTimeScriptedConversationCanStart = Max(m_NextTimeScriptedConversationCanStart, timeInMs+250);
				return;
			}
			if (m_Ped[i]->GetSpeechAudioEntity() && m_Ped[i]->GetSpeechAudioEntity()->IsPainPlaying())
			{
				if( m_PlayingScriptedConversationLine == -1 || (m_PlayingScriptedConversationLine >= 0 && strcmp(m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, "heat_CMAC") != 0)) // there is a dude rag dolling in pain screaming that's fucking shit up
				{
					// if it's the next ped to speak, we don't want to say anything for a good few seconds
					s16 nextLineToPlay = m_PlayingScriptedConversationLine+1;
					if (nextLineToPlay >= 0 && nextLineToPlay < AUD_MAX_SCRIPTED_CONVERSATION_LINES)
					{
						if (m_ScriptedConversation[nextLineToPlay].Speaker == i)
						{
							m_NextTimeScriptedConversationCanStart = Max(m_NextTimeScriptedConversationCanStart, timeInMs+3000);
							ShouldWePauseScriptedConversation(timeInMs); //Call this so that we pause if it's ragdoll pain, like getting thrown from a bike
							return;
						}
					}
				}
			}
		}
	}

	/*
	u32 zeroBlocked = 0;
	u32 oneBlocked = 0;
	if (m_Ped[0] && m_BlockAnimTime[0] > timeInMs)
	{
		zeroBlocked = 1;
	}
	if (m_Ped[1] && m_BlockAnimTime[1] > timeInMs)
	{
		oneBlocked = 1;
	}
	char convDebug[128] = "";
	formatf(convDebug, sizeof(convDebug), "%d:%d", zeroBlocked, oneBlocked);
	grcDebugDraw::PrintToScreenCoors(convDebug,35,35);
	*/

	// see if we're in an upside down car
	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (playerVehicle && playerVehicle->IsUpsideDown()&& playerVehicle->GetVehicleType()!=VEHICLE_TYPE_BOAT)
	{
		m_CarUpsideDownTimer += fwTimer::GetTimeStepInMilliseconds();
	}
	else
	{
		m_CarUpsideDownTimer = 0;
	}

	// See if there's a good reason why we should abort
	if (ShouldWeAbortScriptedConversation())
	{
#if __BANK
		if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0)
		{
			Displayf("[Conv Spew] AbortScriptedConversation %s", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
		}
#endif
		AbortScriptedConversation(false BANK_ONLY(,"reason above."));
		// think it's best to return here - we clean everything up in FinishConversation anyway, and don't want to do any preloading
		return;
	}
	// See if there's a reason to pause the conversation - we do this instead of aborting most times now, and simply return, so we
	// don't kick off another line
	if (ShouldWePauseScriptedConversation(timeInMs))
	{
		return;
	}

	// If we're currently pausing the conversation, just do nothing at all. We don't update anything normally, so if we're still
	// playing the line we called pause on, it'll merrily carry on playing.
	if (m_PauseConversation)
	{
//		formatf(convDebug, sizeof(convDebug), "Paused");
//		grcDebugDraw::PrintToScreenCoors(convDebug,20,20);
		// We also need to cancel subtitles if no lines are playing. 
		// In the case where a conv is paused, but the line allowed to play out, this is the only place we're able to detect that.
		if (m_PlayingScriptedConversationLine>=0) // Check we've actually started the conversation
		{
			// See if they're still talking
			if (m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker>=0)
			{
				if(m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker] &&
					m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity())
				{
					if (!m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity()->IsScriptedSpeechPlaying())
					{
						m_PauseWaitingOnCurrentLine = false;
						if(m_ConversationDuckingScene)
							m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 0.f);
						if (m_LastDisplayedTextBlock>0 && (m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES-1 || !m_ScriptedConversation[m_PlayingScriptedConversationLine+1].DoNotRemoveSubtitle))
						{
#if __BANK
							sm_ScriptedLineDebugInfo[0] = '\0';
#endif
							CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(m_LastDisplayedTextBlock, false);
							m_LastDisplayedTextBlock = -1; // to avoid trying to cancel the same subtitle repeatedly. 
							// Don't think losing our memory of playing it will ever be a problem.
						}
					}
				}
				else if(!m_SpeechAudioEntity.IsScriptedSpeechPlaying())
				{
					m_PauseWaitingOnCurrentLine = false;
					if(m_ConversationDuckingScene)
						m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 0.f);
					if (m_LastDisplayedTextBlock>0 && (m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES-1 || !m_ScriptedConversation[m_PlayingScriptedConversationLine+1].DoNotRemoveSubtitle))
					{
#if __BANK
						sm_ScriptedLineDebugInfo[0] = '\0';
#endif
						CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(m_LastDisplayedTextBlock, false);
						m_LastDisplayedTextBlock = -1; // to avoid trying to cancel the same subtitle repeatedly. 
						// Don't think losing our memory of playing it will ever be a problem.
					}
				}
			}
		}
		else
		{
			if(!HandleCallToPreloading(timeInMs, true))
				return;
		}
		return;
	}

	m_DuckingForScriptedConversation = true; // canceled later if we're having a wee break between lines - doing it here gives us a little 

	// If we're waiting on something to be preloaded, keep preloading until we're done
	naConvDebugf1("Processing: line: %d; preload: %d; highestLoaded: %d highestLoading %d", m_PlayingScriptedConversationLine, m_PreLoadingSpeechLine, m_HighestLoadedLine, m_HighestLoadingLine);
	if(!HandleCallToPreloading(timeInMs))
	{
		return;
	}

	if(m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES)
	{
		naConvAssertf(0,"Too many lines in this conversation.  Aborting.  First line: %s", m_ScriptedConversation[0].Context);
#if __BANK
		if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0)
		{
			Displayf("[Conv Spew] AbortScriptedConversation - Too many lines %s", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
		}
#endif
		AbortScriptedConversation(false BANK_ONLY(,"reason above."));
		return;
	}

	// Currently playing line m_PlayingScriptedConversationLine
	// Total quick fudge, as we need to do all this stuff properly and differently later anyway.
	// For now, just see if we're currently playing our one sound - if not (it's been nulled) we see
	// if there's more to come, and play the next one if there is.

	// We now need to see if the previous line has finished - which is now being played via one of
	//  two different audio entities.
	bool playingDialogue = false;
	s32 playTime = -1;
	s32 length = -1;
	if (m_PlayingScriptedConversationLine>=0 && m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0) // Check we've actually started the conversation
	{
		// See if they're still talking
		if (m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker] &&
			m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity())
		{
			playingDialogue = m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity()->IsScriptedSpeechPlaying(&length, &playTime);
		}
		else
		{
			playingDialogue = m_SpeechAudioEntity.IsScriptedSpeechPlaying(&length, &playTime);
			// Sanity check that if we're playing frontend speech, not from a ped, that it doesn't have speaker gestures
#if !__FINAL
			if (playingDialogue)
			{
				u32 speakerGestureHash;
				u32 speakerGestureDuration;
				if(m_SpeechAudioEntity.GetLastMetadataHash(AUD_SPEECH_METADATA_CATEGORY_GESTURE_SPEAKER, speakerGestureHash, speakerGestureDuration))
				{
					// We've got a speaker gesture on frontend speech - this is likely a bug
					naConvWarningf("Speaker-gestured frontend speech: line: %d; voice: %s; context: %s", m_PlayingScriptedConversationLine, 
						m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker], m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
					naConvErrorf("Gestured speech playing frontend - please assign bug to level designer");
				}
			}
#endif
		}
		// if we're playing dialogue, print some debug info on how far through we are
		if (playingDialogue)
		{
//			char convDebug[128] = "";
//			formatf(convDebug, sizeof(convDebug), "length: %d; playtime: %d", length, playTime);
//			grcDebugDraw::PrintToScreenCoors(convDebug,25,25);

			// See if we're about to finish, and make sure the next speaker (if not us) has their ambient anims disabled,
			// by setting their off time to a bit ahead. Current speaker's always a bit ahead, so the general setting code will sort it
			naConvAssertf(m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker>=0 &&
				m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS,
				"Speaker num is of bounds of m_BlockAnimTime");
			m_BlockAnimTime[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker] = timeInMs + g_StopAmbientAnimTime;
			if (length>-1 && playTime>-1 && ((length-playTime) < g_StopAmbientAnimTime))
			{
				// Mark our next speaker to not do ambient anims for a while, and us too. 
				s32 nextLine = m_PlayingScriptedConversationLine+1;
				if (nextLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES && m_ScriptedConversation[nextLine].Speaker >= 0)
				{
					m_BlockAnimTime[m_ScriptedConversation[nextLine].Speaker] = timeInMs + g_StopAmbientAnimTime;
				}	
			}

			if(playTime>0 && !m_HasShownSubtitles)
			{
				ShowSubtitles();
				m_HasShownSubtitles = true;
			}
			if(g_AdjustTriggerTimeForZeroPlaytime && playTime == 0 && m_ScriptedConversationOverlapTriggerTime != 0 && 
				(m_TimeTrueOverlapWasSet == 0 || g_AdjustTriggerForZeroEvenIfMarkedOverlap))
			{
					m_ScriptedConversationOverlapTriggerTime += timeInMs - m_TimeLastFrame;
					naConvDebugf1("Adding %u to make trigger time %u at point 10 %s %u", timeInMs - m_TimeLastFrame, m_ScriptedConversationOverlapTriggerTime,
						m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, g_AudioEngine.GetTimeInMilliseconds());
			}
		}
		// We're also playing dialogue if we're playing an sfx
		if (m_ScriptedConversationSfx)
		{
			playingDialogue = true;
		}
	}

	// if we've been requested to abort, and have finished playing, stop the conversation.
	if (m_AbortScriptedConversationRequested && !playingDialogue)
	{
#if __BANK
		if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0)
		{
			Displayf("[Conv Spew] m_AbortScriptedConversationRequested %s", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
		}
#endif
		FinishScriptedConversation();
		return;
	}

	bool animTriggersControllingConv = m_AnimTriggersControllingConversation BANK_ONLY(|| g_AnimTriggersControllingConversation);
	// We might also want to play the next line because we've got an overlap - explicitly check for that, and flag it.
	bool timeToOverlap = false;
	if (!animTriggersControllingConv && !m_AbortScriptedConversationRequested && m_OverlapTriggerTimeAdjustedForPredelay &&
		!m_TriggeredOverlapSpeech && m_ScriptedConversationOverlapTriggerTime > 0 && m_ScriptedConversationOverlapTriggerTime<timeInMs)
	{
		timeToOverlap = true;
		// and skip the next line, as it's the overlap cmd we've already processed
		//m_PlayingScriptedConversationLine++;
	}

	/*
	if (playingDialogue)
	{
		formatf(convDebug, sizeof(convDebug), "playingDialogue: %d", m_PlayingScriptedConversationLine);
		grcDebugDraw::PrintToScreenCoors(convDebug,25,25);
	}
	if (m_ScriptedConversationNoAudioDelay>=timeInMs)
	{
		formatf(convDebug, sizeof(convDebug), "delay");
		grcDebugDraw::PrintToScreenCoors(convDebug,30,30);
	}
	*/

	bool playNextLine = false;
	if(animTriggersControllingConv)
	{
		if(m_AnimTriggerReported)
		{
			Assertf(m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES, "Too many triggers in anim triggered conversation.  Going to overrun the conversation array.");
			s16 nextNonSFXLine = m_PlayingScriptedConversationLine+1;
			for( ; nextNonSFXLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES; nextNonSFXLine++)
			{
				if(!IsContextAnSFXCommand(m_ScriptedConversation[nextNonSFXLine].Context))
					break;
			}
			//make sure first line starts, and also allow overlapping if speakers are different
			if(m_PlayingScriptedConversationLine < 0)
			{
				naConvDebugf1("Reacting to m_AnimTriggerReported 1. %u", g_AudioEngine.GetTimeInMilliseconds());

				//need to skip first line pauses, but will be incrementing this value in a sece, so set to nextNonSFXLine minus 1
				m_PlayingScriptedConversationLine = nextNonSFXLine - 1;
				playNextLine = true;
				m_AnimTriggerReported = false;
			}
			else if(nextNonSFXLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
					m_ScriptedConversation[nextNonSFXLine].Speaker != -1 &&
					m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker != m_ScriptedConversation[nextNonSFXLine].Speaker
				)
			{
				naConvDebugf1("Reacting to m_AnimTriggerReported 2. %u", g_AudioEngine.GetTimeInMilliseconds());

				playNextLine = true;
				m_AnimTriggerReported = false;
			}
			else
			{
				s32 length = -1;
				s32 playtime = -1;
				CPed* ped = m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 ?
					m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() : NULL;
				Assertf(ped, "NULL ped conversation speaker in anim-triggered conversation.");
				if (ped && ped->GetSpeechAudioEntity())
				{
					if(!ped->GetSpeechAudioEntity()->IsScriptedSpeechPlaying(&length, &playtime))
					{
						naConvDebugf1("Reacting to m_AnimTriggerReported 3. %u", g_AudioEngine.GetTimeInMilliseconds());

						playNextLine = true;
						m_AnimTriggerReported = false;
					}
					else
					{
						ped->GetSpeechAudioEntity()->OrphanScriptedSpeechEcho();
						//make sure we ignore lipsync predelay on the next line, for stitching purposes
						m_TreatNextLineToPlayAsUrgent = true;

						naConvDebugf1("Play next line is true, but returning early 2. %u", g_AudioEngine.GetTimeInMilliseconds());

						return;
					}
				}
			}
		}
		//make sure if the last line triggered was the last line of the conversation, we make sure to finish the conversation once the line
		//	is over
		else if((m_PlayingScriptedConversationLine+1 < AUD_MAX_SCRIPTED_CONVERSATION_LINES) &&
				 m_PlayingScriptedConversationLine >= 0 &&
				 m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 &&
			    ((m_ScriptedConversation[m_PlayingScriptedConversationLine + 1].Speaker) < 0)
			)
		{
			CPed* ped = m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker];
			Assertf(ped, "NULL ped conversation speaker in anim-triggered conversation.");
			if (ped && ped->GetSpeechAudioEntity() && !ped->GetSpeechAudioEntity()->IsScriptedSpeechPlaying())
			{
				naConvDebugf1("Reacting to m_AnimTriggerReported 4. %u", g_AudioEngine.GetTimeInMilliseconds());

				playNextLine = true;
				m_AnimTriggerReported = false;
			}
		}
	}
	else
	{
		if(m_PlayingScriptedConversationLine < 0)
		{
			if(IsContextAnSFXCommand(m_ScriptedConversation[0].Context))
			{
				u32 pauseTime = GetPauseTimeForConversationLine(&(m_ScriptedConversation[0].Context[4]));
				if(pauseTime > 0)
				{
					m_PlayingScriptedConversationLine++;
					m_OverlapTriggerTimeAdjustedForPredelay = true;
					m_ScriptedConversationOverlapTriggerTime = timeInMs + pauseTime;
				}
				else
					playNextLine = true;
			}
			else
				playNextLine = true;
		}
		else
			playNextLine = m_PlayingScriptedConversationLine < 0 || timeToOverlap;
	}

	if (playNextLine)
	{
		naConvDisplayf("Play next line is true. %u", g_AudioEngine.GetTimeInMilliseconds());

		// We're not playing anything, so if we were, remove any listeners - I think this is the safest easiest place to do this.
		// Because we sometimes cheat and advance on m_PlayingScriptedConversationLine, just go through them all all remove ANY.
		// Note that we now might be playing stuff, as this could be an overlap line.
		for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
		{
			if (m_Ped[i])
			{
				m_Ped[i]->SetSpeakerListenedTo(NULL);
				m_Ped[i]->SetGlobalSpeakerListenedTo(NULL);
				m_Ped[i]->SetSpeakerListenedToSecondary(NULL);
			}
		}
		// The line we were just playing's finished, so see if we've more to play
		m_PlayingScriptedConversationLine++;
		// store a local flag that we've kicked off a sfx, so things don't break if there isn't the right sound to play
		bool justTriggeredSFX = false;
		if ((m_PlayingScriptedConversationLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES) &&
		((m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker) >= 0))
		{
			bool isSFXContext = IsContextAnSFXCommand(m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
			if ( isSFXContext && 
				GetOverlapOffsetTime(&m_ScriptedConversation[m_PlayingScriptedConversationLine].Context[4]) < 0 &&
				GetPauseTimeForConversationLine(&m_ScriptedConversation[m_PlayingScriptedConversationLine].Context[4]) <= 0)
			{
				// It's not a pause command, so play a sound of the same name, from the ped if we have one
				audSoundInitParams initParams;
				if (m_ScriptedConversation[m_PlayingScriptedConversationLine].Listener < AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS &&
					m_ScriptedConversation[m_PlayingScriptedConversationLine].Listener >= 0)
				{
					// We have a listener id - do we have a real ped
					CPed* listenerPed = m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Listener];
					if (listenerPed)
					{
						initParams.Tracker = listenerPed->GetPlaceableTracker();
					}
				}
				if( (m_HeadSetBeepOnLineIndex == m_PlayingScriptedConversationLine) && !IsFlagSet(audScriptAudioFlags::DisableHeadsetOnBeep) )
				{
					CPed* ped = m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker];
					if (ped && ped->GetSpeechAudioEntity())
					{
						audSoundInitParams beepInitParams; 
						beepInitParams.Pan = 0;
						Vector3 speakerPos = Vector3(0.f,0.f,0.f);
						if(ped->GetSpeechAudioEntity()->GetPositionForSpeechSound(speakerPos,(audSpeechVolume)m_ScriptedConversation[m_PlayingScriptedConversationLine].Volume,
							m_NullPedSpeakerLocations[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
							m_NullPedSpeakerEntities[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]))
						{
							f32 distanceToPed = Max((VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - speakerPos).Mag(),0.f);
							beepInitParams.Volume = ped->GetSpeechAudioEntity()->ComputePositionedHeadsetVolume(ped, AUD_SPEECH_TYPE_SCRIPTED,distanceToPed,true);
							m_HeadSetBeepOffCachedVolume = beepInitParams.Volume;
						}
						CreateAndPlaySound(ATSTRINGHASH("DLC_HEISTS_HEADSET_CALL_START_BEEP", 0xE3C11EED),&beepInitParams);
						initParams.Predelay = 300;
					}
				}
				if(!naConvVerifyf(!m_ScriptedConversationSfx, "Trying to stop scripted sfx but there isn't any"))
				{
					m_ScriptedConversationSfx->StopAndForget();
				}
				CreateAndPlaySound_Persistent(&m_ScriptedConversation[m_PlayingScriptedConversationLine].Context[4], &m_ScriptedConversationSfx, &initParams);
				if(CReplayMgr::ShouldRecord())
				{
					char context[64];
					strncpy(context, m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, 10);
					context[6] = 0;
					if(atStringHash(context) != ATSTRINGHASH("EXPOW_", 0x9DB49833))
					{
						RecordUpdateScriptSlotSound(g_NullSoundHash, atStringHash(&m_ScriptedConversation[m_PlayingScriptedConversationLine].Context[4]), &initParams, NULL,  m_ScriptedConversationSfx);
					}
				}

				justTriggeredSFX = true;
				m_ScriptedConversationOverlapTriggerTime = 0; //We'll return early as long as the conversation SFX sound isn't null
				m_OverlapTriggerTimeAdjustedForPredelay = false;
				m_TriggeredOverlapSpeech = false;
				// Now see if the next line is an overlap, and set our overlap time appropriately
				//every line will now be treated as if it were overlapping, to compensate for lipsync warmup time.
				s32 overlapTime = 0;
				u32 nextLine = m_PlayingScriptedConversationLine+1;
				if ((nextLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES) &&
					((m_ScriptedConversation[nextLine].Speaker) >= 0))
				{
					if (IsContextAnSFXCommand(m_ScriptedConversation[nextLine].Context))
					{
						overlapTime = GetOverlapOffsetTime(&m_ScriptedConversation[nextLine].Context[4]);
					}
				}
				if(overlapTime > 0)
				{
					m_ScriptedConversationOverlapTriggerTime = timeInMs + (u32)overlapTime;
					m_TimeTrueOverlapWasSet = timeInMs;

					naConvDebugf1("Setting trigger time to %u with overlap %u %s %u", m_ScriptedConversationOverlapTriggerTime, (u32)overlapTime,
						m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
				}
				else
				{
					m_ScriptedConversationOverlapTriggerTime = 0;
				}
			}
			else if(m_PlayingScriptedConversationLine > 0)
			{
				if(isSFXContext)	//must be overlap or pause, which we've already accounted for, but still need to increment line
					m_PlayingScriptedConversationLine++;
				if(m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES)
				{
#if __BANK
					if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0)
					{
						Displayf("[Conv Spew] AbortScriptedConversation - Too many lines %s", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
					}
#endif
					naConvAssertf(0,"Too many lines in this conversation.  Aborting.  First line: %s", m_ScriptedConversation[0].Context);
					AbortScriptedConversation(false BANK_ONLY(,"reason above."));
					return;
				}
				naConvAssertf(!IsContextAnSFXCommand(m_ScriptedConversation[m_PlayingScriptedConversationLine].Context), "Two SFX contexts found in a row.  This could mess up conversation logic.");
				//since we now have to overlap all lines to compensate for lipsync predelay, things get a little crazy when the same speaker
				//	has two lines in a row.  So if they do and they are still talking, do this
				CPed* nextSpeakingPed = m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 ? 
					m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() : NULL;
				if( (nextSpeakingPed && nextSpeakingPed->GetSpeechAudioEntity() && nextSpeakingPed->GetSpeechAudioEntity()->IsScriptedSpeechPlaying()) ||
					(!nextSpeakingPed && m_SpeechAudioEntity.IsScriptedSpeechPlaying())
				  )
				{
					if(nextSpeakingPed && nextSpeakingPed->GetSpeechAudioEntity())
						nextSpeakingPed->GetSpeechAudioEntity()->OrphanScriptedSpeechEcho();
					m_PlayingScriptedConversationLine--;

					naConvDebugf1("Decrementing playing conversation line 1. %u", g_AudioEngine.GetTimeInMilliseconds());

					if(isSFXContext)	//undo having accounted for the pause/overlap
					{
						m_PlayingScriptedConversationLine--;
						naConvDebugf1("Decrementing playing conversation line 2. %u", g_AudioEngine.GetTimeInMilliseconds());
					}
					//make sure we ignore lipsync predelay on the next line, for stitching purposes
					m_TreatNextLineToPlayAsUrgent = true;

#if __BANK
					if(g_ConversationDebugSpew)
					{
						char voiceName[64] = "\0";
						char waveName[64] = "\0";
						if(nextSpeakingPed && nextSpeakingPed->GetSpeechAudioEntity())
						{
							nextSpeakingPed->GetSpeechAudioEntity()->GetCurrentlyPlayingScriptedVoiceName(voiceName);
							nextSpeakingPed->GetSpeechAudioEntity()->GetCurrentlyPlayingWaveName(waveName);
						}
						else if(!nextSpeakingPed && m_SpeechAudioEntity.IsScriptedSpeechPlaying())
						{
							m_SpeechAudioEntity.GetCurrentlyPlayingScriptedVoiceName(voiceName);
							m_SpeechAudioEntity.GetCurrentlyPlayingWaveName(waveName);
						}

						naDisplayf("[Conv Spew] Setting m_TreatNextLineToPlayAsUrgent == true.  m_PlayingScriptedConversationLine: %d CurrentlyPlayingVoice: %s CurrentlyPlayingWave: %s", 
							m_PlayingScriptedConversationLine, voiceName, waveName);
					}
#endif


					m_ScriptedConversationOverlapTriggerTime += m_TimeElapsedSinceLastUpdate;
					naConvDebugf1("Play next line is true, but returning early 2. %u", g_AudioEngine.GetTimeInMilliseconds());

					return;
				}
			}
		}
		if(!animTriggersControllingConv)
		{
//			if ( pauseTime>0 && !animTriggersControllingConv)
//			{
//				if (m_LastDisplayedTextBlock>0)
//				{
//					CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(m_LastDisplayedTextBlock, false);
//				}
//				// and stop ducking the radio for a wee bit
//				m_DuckingForScriptedConversation = false;
//#if __BANK
//				if(g_ConversationDebugSpew)
//					naDisplayf("[Conv Spew] Play next line is true, but returning early 3. %u", g_AudioEngine.GetTimeInMilliseconds());
//#endif
//				return;
//			}
//			else
			if (justTriggeredSFX)
			{
				naConvDebugf1("Play next line is true, but returning early 4. %u", g_AudioEngine.GetTimeInMilliseconds());
				return;
			}
		}

		if(justTriggeredSFX && animTriggersControllingConv)
			m_PlayingScriptedConversationLine++;

		if ((m_PlayingScriptedConversationLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES) &&
			 ((m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker) >= 0))
		{
			// We've more to play
			if (g_AudioEngine.IsAudioEnabled() && (strcmp(m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, "")!=0))
			{
				// See if we have more available already loaded - if not, kick off a bunch of loads, and wait until that's done
				if (m_HighestLoadedLine<m_PlayingScriptedConversationLine)
				{
//					PreLoadScriptedSpeech(m_PlayingScriptedConversationLine);
					m_PlayingScriptedConversationLine--; // because next time in, we'll ++ it again. Horrid hack.
					naConvDebugf1("Decrementing playing conversation line 3. %u", g_AudioEngine.GetTimeInMilliseconds());

					// Note that we're actually waiting on a preload
					m_WaitingForPreload = true;
					naConvAssertf((m_PlayingScriptedConversationLine+1)>=0 && (m_PlayingScriptedConversationLine+1)<AUD_MAX_SCRIPTED_CONVERSATION_LINES, "Out of bounds");
					if (m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Speaker >= 0 &&
						m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Speaker < AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS)
					{
						m_BlockAnimTime[m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Speaker] = timeInMs + g_StopAmbientAnimTime;
					}
					m_ScriptedConversationOverlapTriggerTime += m_TimeElapsedSinceLastUpdate;
					//					formatf(convDebug, sizeof(convDebug), "Preload");
//					grcDebugDraw::PrintToScreenCoors(convDebug,20,20);
					naConvDebugf1("Play next line is true, but returning early 5. %u", g_AudioEngine.GetTimeInMilliseconds());

					return; // think this is fine - we'll just come in again next frame, and keep checking until it's loaded
				}
				// Work out who our listener is, if we have one
				CPed* listenerPed = NULL;
				if (m_ScriptedConversation[m_PlayingScriptedConversationLine].Listener < AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS &&
					m_ScriptedConversation[m_PlayingScriptedConversationLine].Listener >= 0)
				{
					// We have a listener id - do we have a real ped
					listenerPed = m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Listener];
				}
				// See what variation we're playing, a use that for subtitling
//				u32 context=0, fullContext=0; 
				// Find the appropriate speech audio entity
				if (m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 && m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker])
				{
					CPed* ped = m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker];
					if (m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex > -1 &&
						ped->GetSpeechAudioEntity())
					{
						s32 predelay = 0;
						if( (m_HeadSetBeepOnLineIndex == m_PlayingScriptedConversationLine) && !IsFlagSet(audScriptAudioFlags::DisableHeadsetOnBeep) )
						{
							audSoundInitParams beepInitParams; 
							beepInitParams.Pan = 0;
							Vector3 speakerPos = Vector3(0.f,0.f,0.f);
							if(ped->GetSpeechAudioEntity()->GetPositionForSpeechSound(speakerPos,(audSpeechVolume)m_ScriptedConversation[m_PlayingScriptedConversationLine].Volume,
								m_NullPedSpeakerLocations[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
								m_NullPedSpeakerEntities[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]))
							{
								f32 distanceToPed = Max((VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - speakerPos).Mag(),0.f);
								beepInitParams.Volume = ped->GetSpeechAudioEntity()->ComputePositionedHeadsetVolume(ped, AUD_SPEECH_TYPE_SCRIPTED,distanceToPed,true);
								m_HeadSetBeepOffCachedVolume = beepInitParams.Volume;
							}
							CreateAndPlaySound(ATSTRINGHASH("DLC_HEISTS_HEADSET_CALL_START_BEEP", 0xE3C11EED),&beepInitParams);
							predelay = 300;
						}
						m_ScriptedConversationVariation = m_ScriptedSpeechSlot[m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex][m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet].VariationNumber;
						ped->GetSpeechAudioEntity()->PlayScriptedSpeech(m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker], 
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, 
							m_ScriptedConversationVariation,
							m_ScriptedSpeechSlot[m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex][m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet].WaveSlot,
							(audSpeechVolume)m_ScriptedConversation[m_PlayingScriptedConversationLine].Volume,
							false,
							(m_ShouldRestartConvAtMarker && m_LineCheckedForRestartOffset == m_PlayingScriptedConversationLine) ? m_SpeechRestartOffset : 0,
							m_NullPedSpeakerLocations[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
							m_NullPedSpeakerEntities[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
							0,
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Audibility,
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Headset,
							m_ScriptedSpeechSlot[m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex][m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet].AssociatedClip,
							m_ScriptedConversation[m_PlayingScriptedConversationLine].isPadSpeakerRoute,
							predelay
						);
						m_OverlapTriggerTimeAdjustedForPredelay = m_ShouldRestartConvAtMarker && m_LineCheckedForRestartOffset == m_PlayingScriptedConversationLine &&
										m_PlayingScriptedConversationLine + 1 < AUD_MAX_SCRIPTED_CONVERSATION_LINES && 
										GetOverlapOffsetTime(&(m_ScriptedConversation[m_PlayingScriptedConversationLine + 1].Context[4])) >= 0;
						m_MarkerPredelayApplied = m_OverlapTriggerTimeAdjustedForPredelay ? m_SpeechRestartOffset : 0;
						m_SpeechRestartOffset = 0;
						m_LineCheckedForRestartOffset = -1;
						m_LastSpeakingPed = ped;
						m_TriggeredOverlapSpeech = true;
						m_TreatNextLineToPlayAsUrgent = false;
						m_FirstLineHasStarted = true;
#if __BANK
						if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0)
						{
							Displayf("[Conv Spew] 1.Playing conversation %s", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
						}
#endif
						naConvDisplayf("Playing line conversation %s %u", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, g_AudioEngine.GetTimeInMilliseconds());
#if !__FINAL
						m_TimeSpeechTriggered = timeInMs;
#endif

						if( ( m_CloneConversation ) && ( NetworkInterface::IsGameInProgress() ) )
						{
#if __BANK
							if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0)
							{
								Displayf("[Conv Spew] Clone conversation %s %d", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context,
									m_ScriptedConversationVariation);
							}
#endif

							// See what variation number we just played, and send that across the network too.
							CPedConversationLineEvent::Trigger( ped, 
																ped->GetSpeechAudioEntity()->GetCurrentlyPlayingVoiceNameHash(),
																m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, 
																m_ScriptedConversationVariation,
																true );
						}

						if (ped->IsLocalPlayer() && !m_ScriptedConversation[m_PlayingScriptedConversationLine].Headset && 
							!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DoNotBlipScriptedSpeech))
						{						
							CMiniMap::CreateSonarBlipAndReportStealthNoise(ped, ped->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_ObjectCollision, HUD_COLOUR_BLUEDARK);
						}
					}
					else
					{
						m_ScriptedConversationVariation = 0;
					}
					// If we have a listener, make them listen to this ped
					if (listenerPed)
					{
						listenerPed->SetSpeakerListenedTo((CEntity*)ped);
					}

					// Mark the other peds in the same vehicle as the secondary listener.
					if(ped->GetIsInVehicle())
					{
						CVehicle* pSpeakerVehicle = ped->GetMyVehicle();
						for(int i = 0; i < pSpeakerVehicle->GetSeatManager()->GetMaxSeats(); i++)
						{
							CPed* pPedInVehicle = pSpeakerVehicle->GetSeatManager()->GetPedInSeat(i);
							if(pPedInVehicle && pPedInVehicle != ped && pPedInVehicle!= listenerPed)
							{
								pPedInVehicle->SetSpeakerListenedToSecondary((CEntity*)ped);
							}
						}
					}
				}
				else
				{
					if (m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex > -1)
					{
						s32 predelay = 0;
						if( (m_HeadSetBeepOnLineIndex == m_PlayingScriptedConversationLine) && !IsFlagSet(audScriptAudioFlags::DisableHeadsetOnBeep) )
						{
							audSoundInitParams beepInitParams; 
							beepInitParams.Pan = 0;
							Vector3 speakerPos = Vector3(0.f,0.f,0.f);
							if(m_SpeechAudioEntity.GetPositionForSpeechSound(speakerPos,(audSpeechVolume)m_ScriptedConversation[m_PlayingScriptedConversationLine].Volume,
								m_NullPedSpeakerLocations[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
								m_NullPedSpeakerEntities[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]))
							{
								f32 distanceToPed = Max((VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) - speakerPos).Mag(),0.f);
								beepInitParams.Volume = m_SpeechAudioEntity.ComputePositionedHeadsetVolume(nullptr, AUD_SPEECH_TYPE_SCRIPTED,distanceToPed,true);
								m_HeadSetBeepOffCachedVolume = beepInitParams.Volume;
							}
							CreateAndPlaySound(ATSTRINGHASH("DLC_HEISTS_HEADSET_CALL_START_BEEP", 0xE3C11EED),&beepInitParams);
							predelay = 300;
						}
						m_ScriptedConversationVariation = m_ScriptedSpeechSlot[m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex][m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet].VariationNumber;
						m_SpeechAudioEntity.PlayScriptedSpeech(m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker], 
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Context,
							m_ScriptedConversationVariation,
							m_ScriptedSpeechSlot[m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex][m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet].WaveSlot,
							(audSpeechVolume)m_ScriptedConversation[m_PlayingScriptedConversationLine].Volume,
							false,
							(m_ShouldRestartConvAtMarker && m_LineCheckedForRestartOffset == m_PlayingScriptedConversationLine) ? m_SpeechRestartOffset : 0,
							m_NullPedSpeakerLocations[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
							m_NullPedSpeakerEntities[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
							0,
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Audibility,
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Headset,
							m_ScriptedSpeechSlot[m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex][m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet].AssociatedClip,
							m_ScriptedConversation[m_PlayingScriptedConversationLine].isPadSpeakerRoute,
							predelay
							);
						m_OverlapTriggerTimeAdjustedForPredelay = m_ShouldRestartConvAtMarker && m_LineCheckedForRestartOffset == m_PlayingScriptedConversationLine &&
							m_PlayingScriptedConversationLine + 1 < AUD_MAX_SCRIPTED_CONVERSATION_LINES && 
							GetOverlapOffsetTime(&(m_ScriptedConversation[m_PlayingScriptedConversationLine + 1].Context[4])) >= 0;
						m_MarkerPredelayApplied = m_OverlapTriggerTimeAdjustedForPredelay ? m_SpeechRestartOffset : 0;
						m_SpeechRestartOffset = 0;
						m_LineCheckedForRestartOffset = -1;
						m_LastSpeakingPed = NULL;
						m_TriggeredOverlapSpeech = true;
						m_TreatNextLineToPlayAsUrgent = false;
						m_FirstLineHasStarted = true;
#if __BANK
						if(NetworkInterface::IsGameInProgress() && m_PlayingScriptedConversationLine >= 0)
						{
							Displayf("[Conv Spew] 2.Playing conversation %s", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context);
						}
#endif
						naConvDisplayf("Playing line conversation %s %u", m_ScriptedConversation[m_PlayingScriptedConversationLine].Context, g_AudioEngine.GetTimeInMilliseconds());
#if !__FINAL
						m_TimeSpeechTriggered = timeInMs;
#endif
					}
					else
					{
						m_ScriptedConversationVariation = 0;
					}
					// If we have a listener, make them listen to the global speech entity
					if (listenerPed)
					{
						// We only have one ped, so let's make that one listen to the global speech audio entity
						listenerPed->SetGlobalSpeakerListenedTo(&m_SpeechAudioEntity);
					}
				}
				// Now see if the next line is an overlap, and set our overlap time appropriately
				//every line will now be treated as if it were overlapping, to compensate for lipsync warmup time.
				s32 overlapTime = 0;
				u32 nextLine = m_PlayingScriptedConversationLine+1;
				if ((nextLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES) &&
					((m_ScriptedConversation[nextLine].Speaker) >= 0))
				{
					if (IsContextAnSFXCommand(m_ScriptedConversation[nextLine].Context))
					{
						overlapTime = GetOverlapOffsetTime(&m_ScriptedConversation[nextLine].Context[4]);
					}
				}
				if(overlapTime > 0)
				{
					m_TimeTrueOverlapWasSet = timeInMs;
					m_ScriptedConversationOverlapTriggerTime = timeInMs + (u32)overlapTime;
				}
				else
				{
					m_ScriptedConversationOverlapTriggerTime = 0;
					m_TimeTrueOverlapWasSet = 0;
				}

				m_OverlapTriggerTimeAdjustedForPredelay = false;
				m_TriggeredOverlapSpeech = false;
			}
			else
			{
				m_ScriptedConversationNoAudioDelay = (timeInMs + 5000);
			}

			m_HasShownSubtitles = false;
			if (m_ScriptedConversationVariation==0)
			{
				// We didn't find any valid line - this is probably just because the speech isn't yet recorded/generated,
				// so just use the first one, and act as if we're running with -noaudio
				m_ScriptedConversationVariation = 1;
				m_ScriptedConversationOverlapTriggerTime = (timeInMs + 3000);
				m_ScriptedConversationNoAudioDelay = (timeInMs + 3000);
				m_OverlapTriggerTimeAdjustedForPredelay = true;

				ShowSubtitles(true);
				m_HasShownSubtitles = true;
			}
		}
		else
		{
			// We don't have any more speech to play
			// Call our tidy up fn
			
			// Need this ugly check now that we're compensating for lipsync predelay.  Otherwise, some conversations end early
			for(int i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; ++i)
			{
				if(m_Ped[i] && m_Ped[i]->GetSpeechAudioEntity() && m_Ped[i]->GetSpeechAudioEntity()->IsScriptedSpeechPlaying())
				{
					m_PlayingScriptedConversationLine--; //to avoid array overrun
					naConvDebugf1("Decrementing playing conversation line 4. %u", g_AudioEngine.GetTimeInMilliseconds());
					return;
				}
			}
			FinishScriptedConversation();
		}
	}
}


s32 audScriptAudioEntity::GetLipsyncPredelay(tScriptedSpeechSlots& scriptedSlot 
#if !__FINAL
												, int slotNumber										 
#endif
											 )
{
	if(!scriptedSlot.ScriptedSpeechSound || !scriptedSlot.WaveSlot)
	{
		naAssertf(0, "Null sound or waveslot passed to audScriptAudioEntity::GetLipsyncPredelay");
		return 0;
	}

	if(scriptedSlot.AssociatedDic)
	{
		MEM_USE_USERDATA(MEMUSERDATA_VISEME);		// THIS IS A DIRTY HACK to get around the non-stadnard Viseme allocation scheme
		scriptedSlot.AssociatedDic->Release();
		scriptedSlot.AssociatedDic = NULL;
	}

#if !__FINAL
	bool failedToInitWaveRef = false;
#endif

	scriptedSlot.AssociatedClip = NULL;
	scriptedSlot.AssociatedDic = audSpeechAudioEntity::GetVisemeData((void **)&(scriptedSlot.AssociatedClip), scriptedSlot.ScriptedSpeechSound
#if !__FINAL
																				, failedToInitWaveRef
#endif
		);
	u32 predelay = 0;
	if(scriptedSlot.AssociatedClip)
	{
		if(scriptedSlot.AssociatedClip->GetProperties())
		{
			const crProperty* property = scriptedSlot.AssociatedClip->GetProperties()->FindProperty("TimeOfPreDelay");
			if(property)
			{
				const crPropertyAttribute* attribute = property->GetAttribute("TimeOfPreDelay");
				if(attribute && attribute->GetType() == crPropertyAttribute::kTypeFloat)
				{
					const crPropertyAttributeFloat* attributeFloat = static_cast<const crPropertyAttributeFloat*>(attribute);
					predelay = (u32)(attributeFloat->GetFloat()*1000);
				}
			}
		}
	}

#if !__FINAL
	if(failedToInitWaveRef)
	{
		naConvErrorf("Failed to init waveRef when trying to get lipsync predelay for scripted speech.  Please include next few lines of output and send bug to audio.");
		for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
		{
			if (m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound)
			{
				if(i == slotNumber)
				{
					naConvErrorf("!!!NEXT SLOT IS THE ONE THAT FAILED!!!");
				}
				naConvErrorf("ScriptedSpeechSlotInfo Waveslot: %s, LoadedWave: %u, context: %s, voicename: %s",
						m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].WaveSlot->GetSlotName(),
						m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].WaveSlot->GetLoadedWaveNameHash(),
						m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].Context,
						m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].VoiceName
					);
				naConvErrorf("SoundInfo Waveslot: %p, WaveNameHash: %u BankId: %u",
					m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound->GetWaveSlot()->GetSlotName(), 
					m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound->GetWaveNameHash(),
					m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound->GetBankId());
			}
		}
	}
#endif
	//// HACK: Delete the viseme clip dictionary
	//if (pDictionary)
	//{
	//	MEM_USE_USERDATA(MEMUSERDATA_VISEME);		// THIS IS A DIRTY HACK to get around the non-stadnard Viseme allocation scheme
	//	pDictionary->Release();
	//}

	return predelay;
}

void audScriptAudioEntity::AdjustOverlapForLipsyncPredelay(u32 timeInMs)
{
	if(!m_OverlapTriggerTimeAdjustedForPredelay && m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES)
	{
		if(m_ScriptedConversationOverlapTriggerTime == 0 || m_TimeTrueOverlapWasSet != 0)
		{
			s32 length = -1;
			if(!IsContextAnSFXCommand(m_ScriptedConversation[m_PlayingScriptedConversationLine].Context))
			{
				s32 playTime = -1;
				if (m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 &&
					m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker] &&
					m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity())
				{
					if(!m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity()->IsScriptedSpeechPlaying(&length, &playTime))
					{
						m_ScriptedConversationOverlapTriggerTime = timeInMs;
						m_OverlapTriggerTimeAdjustedForPredelay = true;
					}
				}
				else
				{
					m_SpeechAudioEntity.IsScriptedSpeechPlaying(&length, &playTime);
				}

				if(length <= 0)
				{
					return;
				}

				//adjust true overlaps for time it takes preceeding line to prepare
				if(m_TimeTrueOverlapWasSet != 0)
				{
					m_ScriptedConversationOverlapTriggerTime += (timeInMs - m_TimeTrueOverlapWasSet);
					m_TimeTrueOverlapWasSet = 0;
					naConvDebugf1("Setting trigger time to %u, adjust true overlap for loading time %s %u", m_ScriptedConversationOverlapTriggerTime,
						m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
				}
				else
				{
					m_ScriptedConversationOverlapTriggerTime = timeInMs + length;
					naConvDebugf1("Setting trigger time to %u with length %u %s %u", m_ScriptedConversationOverlapTriggerTime, length,
						m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
				}
			}
			else if(m_ScriptedConversationSfx)
			{
				if(m_TimeTrueOverlapWasSet != 0)
				{
					m_ScriptedConversationOverlapTriggerTime += (timeInMs - m_TimeTrueOverlapWasSet);
					m_TimeTrueOverlapWasSet = 0;
					naConvDebugf1("Setting trigger time to %u, adjust true overlap for loading time %s %u", m_ScriptedConversationOverlapTriggerTime,
						m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
				}
				else
				{
					bool isLooping = false;
					length = m_ScriptedConversationSfx->ComputeDurationMsIncludingStartOffsetAndPredelay(&isLooping);

					if(isLooping)
					{
						//This catches external stream sounds.  In this case, we have no choice but to disallow interrupts and just wait until the sound
						//	has played
						m_ScriptedConversationOverlapTriggerTime = timeInMs;
						m_ExternalStreamConversationSFXPlaying = true;
						m_OverlapTriggerTimeAdjustedForPredelay = true;
						return;
					}
					if(length <= 0)
					{
						return;
					}
					m_ScriptedConversationOverlapTriggerTime = timeInMs + length;
					m_OverlapTriggerTimeAdjustedForPredelay = true;
					return;
				}
			}
		}
		

		s32 nextLineSpeechSlotIndex = -1;
		if(m_PlayingScriptedConversationLine+1 < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
			m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Speaker >= 0 &&
			!m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Speaker] &&
			!IsContextAnSFXCommand(m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Context))
		{
			naConvDebugf1("Setting conversation predelay adjust true without setting time 1. %u", g_AudioEngine.GetTimeInMilliseconds());
			m_OverlapTriggerTimeAdjustedForPredelay = true;
		}
		else if(m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex >= 0)
		{
			s32 thisLineSpeechSlotIndex = m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex;
			nextLineSpeechSlotIndex = thisLineSpeechSlotIndex + 1;
			u32 thisLinePreloadSet = m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet;
			u32 nextLinePreloadSet = thisLinePreloadSet;
			if(nextLineSpeechSlotIndex >= AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD)
			{
				nextLinePreloadSet = (m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet + 1) % 2;
				nextLineSpeechSlotIndex = nextLineSpeechSlotIndex % AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD;
				naConvAssertf(nextLineSpeechSlotIndex == 0, "nextLineSpeechSlotIndex is not zero when adjusting overlap for lipsync predelay and overshooting PreloadSet.");
			}

			if(m_ScriptedSpeechSlot[nextLineSpeechSlotIndex][nextLinePreloadSet].LipsyncPredelay != -1)
			{
				u32 nextNonSFXLine = m_PlayingScriptedConversationLine;
				nextNonSFXLine++;
				while(nextNonSFXLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES)
				{
					if(
						strcmp(m_ScriptedConversation[nextNonSFXLine].Context, "") == 0 ||
						!IsContextAnSFXCommand(m_ScriptedConversation[nextNonSFXLine].Context)
						)
					{
						break;
					}
					nextNonSFXLine++;
				}
				if(nextNonSFXLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES ||
					(m_ScriptedConversation[nextNonSFXLine].Speaker >= 0 &&
					 m_Ped[m_ScriptedConversation[nextNonSFXLine].Speaker] && 
					 !m_Ped[m_ScriptedConversation[nextNonSFXLine].Speaker]->m_nDEflags.bFrozen))
				{
					m_ScriptedConversationOverlapTriggerTime -= m_ScriptedSpeechSlot[nextLineSpeechSlotIndex][nextLinePreloadSet].LipsyncPredelay;
				}
#if !__NO_OUTPUT
				else
				{
					naConvDebugf1("Frozen ped found for speaker.  Not really applying predelay at point 3. %s %u",
						m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
				}
#endif
				m_ScriptedConversationOverlapTriggerTime += m_ScriptedSpeechSlot[thisLineSpeechSlotIndex][thisLinePreloadSet].PauseTime;
				//make sure to move this forward if we are restarting the preceding line at an interrupt marker.
				m_ScriptedConversationOverlapTriggerTime -= m_MarkerPredelayApplied;
				if(nextNonSFXLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
					m_ScriptedConversation[nextNonSFXLine].Speaker != m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker)
				{
					m_ScriptedConversationOverlapTriggerTime += g_AdditionalScriptedSpeechPause;
				}

				naConvDebugf1("Adding %u and subtracting %u to make trigger time %u at point 3 %s %u", m_ScriptedSpeechSlot[thisLineSpeechSlotIndex][thisLinePreloadSet].PauseTime,
					m_ScriptedSpeechSlot[nextLineSpeechSlotIndex][nextLinePreloadSet].LipsyncPredelay,
					m_ScriptedConversationOverlapTriggerTime,
					m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());

				m_OverlapTriggerTimeAdjustedForPredelay = true;
			}
			else if(m_PlayingScriptedConversationLine+1 >= AUD_MAX_SCRIPTED_CONVERSATION_LINES ||
					strcmp(m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Context, "") == 0 ||
					m_ScriptedConversation[m_PlayingScriptedConversationLine+1].FailedToLoad ||
					IsContextAnSFXCommand(m_ScriptedConversation[m_PlayingScriptedConversationLine+1].Context)
					)
			{
				naConvDebugf1("Setting conversation predelay adjust true without setting time 2. %u", g_AudioEngine.GetTimeInMilliseconds());
				m_OverlapTriggerTimeAdjustedForPredelay = true;
			}
		}
	}
}

void audScriptAudioEntity::ShowSubtitles(bool forceShow)
{
	if(m_PlayingScriptedConversationLine < 0 || m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES ||
		m_ScriptedConversation[m_PlayingScriptedConversationLine].Context[0] == '\0' ||
		IsContextAnSFXCommand(m_ScriptedConversation[m_PlayingScriptedConversationLine].Context))
	{
		return;
	}

	// Display subtitles
	char *pString;

	const char *subtitle = m_ScriptedConversation[m_PlayingScriptedConversationLine].Subtitle;

	char subtitleWithVariation[40];			
	formatf(subtitleWithVariation, "%s_%02d", subtitle, m_ScriptedConversationVariation); 

	if (TheText.DoesTextLabelExist(subtitleWithVariation))
	{
		pString = TheText.Get(subtitleWithVariation);
	}
	else
	{
		// subtitle with variation doesn't exist, so try it without variation
		if (!TheText.DoesTextLabelExist(subtitle))
		{
			naConvWarningf("Subtitle doesn't exist - with or without _XX. (%s) Tried to find variation %s", subtitle, subtitleWithVariation);
		}
		pString = TheText.Get(subtitle);
	}

	if(pString && (forceShow ||  m_ScriptedConversation[m_PlayingScriptedConversationLine].IsPlaceholder) )
	{
		if(naConvVerifyf(pString[0] == '~' && pString[2] == '~', "Trying to force an invalid subtitle %s.", pString))
		{
			pString = pString + 3; //skip the leading ~z~
		}
	}

	s32 TextBlock = TheText.GetBlockContainingLastReturnedString();

	if (m_DisplaySubtitles)
	{
		uiDebugf3("Adding To Brief:  Dialogue Line --- %s --- Spoken By --- %s --- During Mission -- %s", pString, m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker], CPauseMenu::GetCurrentMissionLabel());
		
		if (CPauseMenu::GetMenuPreference(PREF_SUBTITLES) || (CMessages::IsThisTextSwitchedOffByFrontendSubtitleSetting(pString) == false) )
		{	//	Graeme - This is something that Imran asked for (Bug 568480). It seems like a bad idea to me. It means that the game will behave differently depending on whether subtitles are off or on.
			//	If subtitles are switched off in the pause menu then the idea is that scripts can be written so that god text is shown while the conversation plays.
			//	I need to avoid calling AddMessage so that the god text doesn't get cleared by a conversation subtitle that would never be shown anyway.
			if(m_SizeOfSubtitleSubStringArray == 0)
			{

				CMessages::AddMessage( pString, TextBlock, forceShow ? 3000 : 10000, true, 
					m_AddSubtitlesToBriefing && !m_ScriptedConversation[m_PlayingScriptedConversationLine].DoNotRemoveSubtitle, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false,
					atStringHash(m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]), atStringHash(CPauseMenu::GetCurrentMissionLabel()) );
			}
			else
			{
				CMessages::AddMessage( pString, TextBlock, forceShow ? 3000 : 10000, true, 
					m_AddSubtitlesToBriefing && !m_ScriptedConversation[m_PlayingScriptedConversationLine].DoNotRemoveSubtitle, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, m_ArrayOfSubtitleSubStrings, m_SizeOfSubtitleSubStringArray, false,
					atStringHash(m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]), atStringHash(CPauseMenu::GetCurrentMissionLabel()) );
			}
		}
		else
		{
			CSubtitleMessage PreviousBrief;
			PreviousBrief.Set(pString, TextBlock, 
				NULL, 0, 
				NULL, 0, 
				false, true, atStringHash(m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]), atStringHash(CPauseMenu::GetCurrentMissionLabel()));
			CMessages::AddToPreviousBriefArray(PreviousBrief, PREVIOUS_BRIEF_FORCE_DIALOGUE);
		}
		m_LastDisplayedTextBlock = TextBlock;
	}

#if __BANK
	if(g_DisplayScriptedLineInfo)
	{
		formatf(sm_ScriptedLineDebugInfo, "voicename:%s context:%s variation:%u", m_VoiceName[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker],
			m_ScriptedConversation[m_PlayingScriptedConversationLine].Context,
			m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex >= 0 ?
			m_ScriptedSpeechSlot[m_ScriptedConversation[m_PlayingScriptedConversationLine].SpeechSlotIndex][m_ScriptedConversation[m_PlayingScriptedConversationLine].PreloadSet].VariationNumber : 0);
	}
#endif
}

bool audScriptAudioEntity::IsPreloadedScriptedSpeechReady()
{
	if(m_PreLoadingSpeechLine > -1 || m_HighestLoadedLine > -1)
	{
		audPrepareState preloadPrepareState = PreLoadScriptedSpeech(m_PreLoadingSpeechLine);
		if (preloadPrepareState!=AUD_PREPARED)
		{
			if(preloadPrepareState == AUD_PREPARE_FAILED && !g_DoNotAbortFailedConversations)
			{
#if __BANK
				char txt[64]; 
				formatf(txt,"Scripted speech lines failed to prepare.  Aborting conversation.");
#endif
				naConvAssertf(0,"Scripted speech lines failed to prepare.  Aborting conversation.");
				AbortScriptedConversation(false BANK_ONLY(,txt));
			}
			return false;
		}

		return true;
	}
	
	return false;
}

bool audScriptAudioEntity::HandleCallToPreloading(u32 timeInMs, bool isPaused)
{
	if (m_PreLoadingSpeechLine>-1)
	{
		audPrepareState preloadPrepareState = PreLoadScriptedSpeech(m_PreLoadingSpeechLine);
		if (preloadPrepareState!=AUD_PREPARED)
		{
			//			formatf(convDebug, sizeof(convDebug), "Waiting for preload");
			//			grcDebugDraw::PrintToScreenCoors(convDebug,20,20);
			if(preloadPrepareState == AUD_PREPARE_FAILED && !g_DoNotAbortFailedConversations)
			{
#if __BANK
				char txt[128]; 
				formatf(txt,"Scripted speech lines failed to prepare.  Aborting conversation.");
#endif
				naConvAssertf(0,"Scripted speech lines failed to prepare.  Aborting conversation.");
				AbortScriptedConversation(false BANK_ONLY(,txt));
				return false;
			}
			// Exit out if we're waiting on that line being preloaded - we're probably not, unless this is the first line of the conv
			if (m_WaitingForPreload)
			{
				return false;
			}

		}
		if(m_WaitingForFirstLineToPrepareToDuck && !isPaused)
		{
			m_WaitingForFirstLineToPrepareToDuck = false;
			if(m_ConversationDuckingScene && !m_ScriptSetNoDucking)
				m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 1.f);
		}
	}
	else
	{
		if(m_ConversationIsSfxOnly && m_HighestLoadedLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES - 1 && !m_ScriptedConversationSfx)
		{
			AbortScriptedConversation(false BANK_ONLY(,"Aborting from HandleCallToPreloading"));
			return false;
		}
		// See if we should be preloading - are we playing something not far behind the highest loaded thing 
		if (m_HighestLoadedLine < (m_PlayingScriptedConversationLine+3))
		{
			// Special-case the first line, so we can quickly block ambient anims
			if (m_PlayingScriptedConversationLine==-1 && !isPaused)
			{
				if(g_DuckForConvAfterPreload)
					m_WaitingForFirstLineToPrepareToDuck = true;

				if( m_ScriptedConversation[0].Speaker >= 0 &&
					m_ScriptedConversation[0].Speaker < AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS &&
					m_Ped[m_ScriptedConversation[0].Speaker])
				{
					m_BlockAnimTime[m_ScriptedConversation[0].Speaker] = timeInMs + g_StopAmbientAnimTime;
					m_Ped[m_ScriptedConversation[0].Speaker]->SetAmbientAnimsBlocked(true);
				}
			}
			audPrepareState preloadPrepareState = PreLoadScriptedSpeech(m_HighestLoadedLine+1);
			if(preloadPrepareState == AUD_PREPARE_FAILED)
				return false;
		}
	}

	return true;
}

void audScriptAudioEntity::FinishScriptedConversation()
{
	naConvDebugf1("FinishScriptedConversation()");

	if(m_ConversationDuckingScene)
		m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 0.f);

	m_ScriptedConversationInProgress = false;
	m_DuckingForScriptedConversation = false;
	m_IsScriptedConversationZit = false;
	m_PlayingScriptedConversationLine = -1;
	m_ScriptedConversationNoAudioDelay = 0;
	m_AbortScriptedConversationRequested = false;
	m_HighestLoadedLine = -1;
	m_PreLoadingSpeechLine = -1;
	m_ScriptedConversationOverlapTriggerTime = 0;
	m_OverlapTriggerTimeAdjustedForPredelay = false;
	m_TriggeredOverlapSpeech = false;
	m_NextTimeScriptedConversationCanStart = g_AudioEngine.GetTimeInMilliseconds()+250;
	m_AnimTriggersControllingConversation = false;
	m_IsConversationPlaceholder = false;
	m_InterruptTriggered = false;
	m_ScriptSetNoDucking = false;
	m_IsScriptedConversationAMobileCall = false;

	for (s32 j=0; j<2; j++)
	{
		for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
		{
			if (m_ScriptedSpeechSlot[i][j].ScriptedSpeechSound)
			{
				m_ScriptedSpeechSlot[i][j].ScriptedSpeechSound->StopAndForget();
				// This won't actually stop any playing speech, as that's just the preload sound - need to explicitly tell all peds to stop talking
			}
			if(m_ScriptedSpeechSlot[i][j].AssociatedDic)
			{
				MEM_USE_USERDATA(MEMUSERDATA_VISEME);		// THIS IS A DIRTY HACK to get around the non-stadnard Viseme allocation scheme
				m_ScriptedSpeechSlot[i][j].AssociatedDic->Release();
				m_ScriptedSpeechSlot[i][j].AssociatedDic = NULL;
			}
			m_ScriptedSpeechSlot[i][j].AssociatedClip = NULL;
			m_ScriptedSpeechSlot[i][j].LipsyncPredelay = -1;
			m_ScriptedSpeechSlot[i][j].PauseTime = 0;
			m_ScriptedSpeechSlot[i][j].VariationNumber = -1;
#if !__FINAL
			m_ScriptedSpeechSlot[i][j].VoiceName[0] = '\0';
			m_ScriptedSpeechSlot[i][j].Context[0] = '\0';
#endif
		}
	}

	SetAllPedsHavingAConversation(false);

	if (m_LastDisplayedTextBlock>0)
	{
#if __BANK
		sm_ScriptedLineDebugInfo[0] = '\0';
#endif
		CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(m_LastDisplayedTextBlock, false);
	}

	// Clear up our references
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		if (m_Ped[i])
		{
			if (m_BlockAnimTime[i] > 0)
			{
				m_Ped[i]->SetAmbientAnimsBlocked(false);
			}
			m_Ped[i]->RemoveSpeakerListenedTo();
			m_Ped[i]->RemoveSpeakerListenedToSecondary();
			if(m_Ped[i]->GetIsInVehicle())
			{
				CVehicle* pSpeakerVehicle = m_Ped[i]->GetMyVehicle();
				for(int i = 0; i < pSpeakerVehicle->GetSeatManager()->GetMaxSeats(); i++)
				{
					CPed* pPedInVehicle = pSpeakerVehicle->GetSeatManager()->GetPedInSeat(i);
					if(pPedInVehicle && pPedInVehicle != m_Ped[i])
					{
						pPedInVehicle->RemoveSpeakerListenedToSecondary();
					}
				}
			}
			if (m_Ped[i]->GetSpeechAudioEntity())
				m_Ped[i]->GetSpeechAudioEntity()->StopPlayingScriptedSpeech();
			// Remove the old registered reference
			// NOTE: this function can be called from script directly or from the audio update thread, we only want to defer tidyref
			// when this is running on the audio thread

			m_Ped[i] = NULL;
		}
		m_PedIsInvolvedInCurrentConversation[i] = false;
	}

	// Stop playing anything from our frontend speech audio entity
	m_SpeechAudioEntity.StopPlayingScriptedSpeech();
	// Check we don't have a sound playing
	if (m_ScriptedConversationSfx)
	{
		m_ScriptedConversationSfx->StopAndForget();
	}

	m_Interrupter = NULL;
	m_TimeToInterrupt = 0;
	m_TimeToRestartConversation = 0;

	m_TimeLastScriptedConvFinished = audNorthAudioEngine::GetCurrentTimeInMs();
}

audPrepareState audScriptAudioEntity::PreLoadScriptedSpeech(s16 lineToStartLoadingFrom)
{
	if(!g_AudioEngine.IsAudioEnabled())
	{
		m_HighestLoadedLine = AUD_MAX_SCRIPTED_CONVERSATION_LINES;
		return AUD_PREPARED;
	}
	if (lineToStartLoadingFrom<0)
	{
		naConvErrorf("Invalid line to start loading from");
		return AUD_PREPARED;
	}
	// If we aren't already preloading, clear out all the old line-slot-pairs, set up the new ones, and kick off the loads. 
	// We'll only have valid sounds in the slots if we're preloading.
	if (m_PreLoadingSpeechLine>-1)
	{
		// See how many lines there are remaining, and load that many
		audSpeechSoundInfo speechSoundInfo[AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD];
		u32 numSpeechSounds = 0;
		for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
		{
			if (m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound)
			{
				speechSoundInfo[numSpeechSounds].sound = m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound;
				speechSoundInfo[numSpeechSounds].slot  = m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].WaveSlot;
				if(speechSoundInfo[numSpeechSounds].slot && speechSoundInfo[numSpeechSounds].slot->GetReferenceCount() == (u32)-1)
				{
#if __BANK
					char txt[64]; 
					formatf(txt,"Found scripted speech slot with -1 ref count.  Aborting conversations.");
#endif
					naConvAssertf(0,"Found scripted speech slot with -1 ref count.  Aborting conversations.");
					AbortScriptedConversation(false BANK_ONLY(,txt));
					return AUD_PREPARE_FAILED;
				}
				numSpeechSounds++;
			}
		}
		audPrepareState prepareState = audSpeechSound::BatchPrepare(speechSoundInfo, numSpeechSounds);
		if (prepareState == AUD_PREPARED)
		{
#if !__FINAL
			u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			for (s16 conversationLine = lineToStartLoadingFrom; conversationLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES && m_ScriptedConversation[conversationLine].TimePrepareFirstRequested != 0; conversationLine++)
			{
				if(currentTime > m_ScriptedConversation[conversationLine].TimePrepareFirstRequested &&
					currentTime - m_ScriptedConversation[conversationLine].TimePrepareFirstRequested> 1000)
				{
					naConvWarningf("Scripted speech line %s taking too long to load.  Took %u ms", m_ScriptedConversation[conversationLine].Context,
						currentTime - m_ScriptedConversation[conversationLine].TimePrepareFirstRequested);
				}
			}
#endif
			// We've loaded everything - clear our preparing sounds, and flag that we're done
			for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
			{
				if (m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound)
				{
					m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].LipsyncPredelay = GetLipsyncPredelay(m_ScriptedSpeechSlot[i][m_CurrentPreloadSet]
#if !__FINAL
																										, i
#endif
						);
					m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound->StopAndForget();
				}
				m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound = NULL;
			}
#if !__FINAL
			if(lineToStartLoadingFrom >= 0 && lineToStartLoadingFrom < AUD_MAX_SCRIPTED_CONVERSATION_LINES)
			{
				PF_SET(TimeToPreloadScriptedSpeech, currentTime -  m_ScriptedConversation[lineToStartLoadingFrom].TimePrepareFirstRequested);
			}
#endif
			m_PreLoadingSpeechLine = -1;
			m_CurrentPreloadSet = (m_CurrentPreloadSet+1) % 2;
			m_WaitingForPreload = false;
			m_HighestLoadedLine = m_HighestLoadingLine;
			return AUD_PREPARED;
		}
		// else, do nothing, we're still loading
		return prepareState;
	}
	else
	{
		m_PreLoadingSpeechLine = lineToStartLoadingFrom;
		// We've not started preparing yet. Clear out our line-slot pairings, set up new ones, create speech sounds, and prepare
		// Assert if we've any sounds - they should only exist whilst preparing
		for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
		{
			naConvAssertf(!m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound, "Still have sounds in the preload slots when they should be cleared out");
		}
		u32 scriptedSpeechSlot = 0;
		s16 conversationLine = lineToStartLoadingFrom;
		while (scriptedSpeechSlot<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD && conversationLine<AUD_MAX_SCRIPTED_CONVERSATION_LINES)
		{
#if !__FINAL
			//little extra time for the first line to load
			m_ScriptedConversation[conversationLine].TimePrepareFirstRequested = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
#endif
			if (m_ScriptedConversation[conversationLine].Speaker >= 0 && !IsContextAnSFXCommand(m_ScriptedConversation[conversationLine].Context))
			{
				m_ScriptedConversation[conversationLine].SpeechSlotIndex = scriptedSpeechSlot;
				m_ScriptedConversation[conversationLine].PreloadSet = m_CurrentPreloadSet;
				char* voiceName = m_VoiceName[m_ScriptedConversation[conversationLine].Speaker];
				char* context = m_ScriptedConversation[conversationLine].Context;
				s32 variationNum = m_SpeechAudioEntity.GetRandomVariation(atStringHash(voiceName), context, NULL, NULL);
#if !__FINAL
				char placeholderVoiceName[128] = "";
				if(variationNum <= 0)
				{
					if(!CFileMgr::ShouldForceReleaseAudioPacklist())
					{
						if (voiceName 
							&& g_EnablePlaceholderDialogue 
							&& (!m_IsConversationPlaceholder || PARAM_useRobotSpeechForAllLines.Get()))
						{
							formatf(placeholderVoiceName, sizeof(placeholderVoiceName), "%s_PLACEHOLDER", voiceName);
							variationNum = m_SpeechAudioEntity.GetRandomVariation(atStringHash(placeholderVoiceName), context, NULL, NULL);
							if (variationNum <=0)
							{
								Displayf("Missing scripted speech: voice: %s; context: %s", voiceName, context);
							}
							voiceName = placeholderVoiceName;
							m_ScriptedConversation[conversationLine].IsPlaceholder = true;
						}
					}
#if __BANK
					if(!m_HasInitializedDialogueFile)
					{
						static bool dialogueFileFailedToOpen = false;

						if(g_TrackMissingDialogue && m_DialogueFileHandle == INVALID_FILE_HANDLE && !dialogueFileFailedToOpen && 
							(!NetworkInterface::IsGameInProgress() || g_TrackMissingDialogueMP))
						{
							const char* pPathName = NULL;
							PARAM_trackMissingDialogue.Get(pPathName);
							if(strlen(pPathName) == 0)
							{
#if RSG_ORBIS
								formatf(m_DialogueFileName, "common:/audMissingDialogue_%u_%u.csv", audSpeechSound::GetMetadataManager().GetAssetChangelist(), time(0));
#else
								formatf(m_DialogueFileName, "common:/audMissingDialogue_%u_%ui.csv", audSpeechSound::GetMetadataManager().GetAssetChangelist(), std::time(0));
#endif
							}
							else
							{
#if RSG_ORBIS
								formatf(m_DialogueFileName, "%s/audMissingDialogue_%u_%u.csv",pPathName, audSpeechSound::GetMetadataManager().GetAssetChangelist(), time(0));
#else
								formatf(m_DialogueFileName, "%s/audMissingDialogue_%u_%u.csv",pPathName, audSpeechSound::GetMetadataManager().GetAssetChangelist(), std::time(0));
#endif
							}

							m_DialogueFileHandle = CFileMgr::OpenFileForWriting(m_DialogueFileName);
							if(m_DialogueFileHandle != INVALID_FILE_HANDLE)
							{
								const char *header = "VoiceName,FileName,SpeakerID\r\n";
								CFileMgr::Write(m_DialogueFileHandle, header, istrlen(header));
								CFileMgr::CloseFile(m_DialogueFileHandle);
								m_DialogueFileHandle = CFileMgr::OpenFileForAppending(m_DialogueFileName);
							}
							else
							{
								dialogueFileFailedToOpen = true;
							}
						}

						m_HasInitializedDialogueFile = true;
					}
		
					if(m_DialogueFileHandle != INVALID_FILE_HANDLE && (!NetworkInterface::IsGameInProgress() || g_TrackMissingDialogueMP))
					{
						char buf[256];
						formatf(buf,256,"%s,%s,%u\r\n",m_VoiceName[m_ScriptedConversation[conversationLine].Speaker],
								m_ScriptedConversation[conversationLine].Context,m_ScriptedConversation[conversationLine].Speaker);
						CFileMgr::Write(m_DialogueFileHandle, buf, istrlen(buf));
						CFileMgr::CloseFile(m_DialogueFileHandle);
						m_DialogueFileHandle = CFileMgr::OpenFileForAppending(m_DialogueFileName);
					}
#endif
				}
#endif
				//naCErrorf(variationNum>=0, "Missing scripted speech: voice: %s; context: %s", voiceName, context);
				m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].VariationNumber = variationNum;
				audSoundInitParams initParams;
				initParams.WaveSlot = m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].WaveSlot;
				CreateSound_PersistentReference("PRELOAD_SPEECH_SOUND", (audSound**)&m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].ScriptedSpeechSound, &initParams);
				if(m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].ScriptedSpeechSound)
				{
					bool success = m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].ScriptedSpeechSound->InitSpeech(voiceName, context, variationNum);
					if (success)
					{
						m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].PauseTime = 0;
						if(conversationLine + 1 < AUD_MAX_SCRIPTED_CONVERSATION_LINES && IsContextAnSFXCommand(m_ScriptedConversation[conversationLine+1].Context))
							m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].PauseTime = GetPauseTimeForConversationLine(&m_ScriptedConversation[conversationLine+1].Context[4]);

						naConvDisplayf("Preloading line %s %u", context, g_AudioEngine.GetTimeInMilliseconds());

#if !__FINAL && __BANK
						safecpy(m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].VoiceName, voiceName);
						safecpy(m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].Context, context);
#endif//!__FINAL && __BANK

						scriptedSpeechSlot++;
					}
					else
					{
						//naAssertf(0, "Incorrect voicename found for conversation line %s.  Check that the speaker number in d* is correct.", context);
						naDisplayf("InitSpeech Failed - Incorrect voicename found for conversation line %s _%d %s.  Check that the speaker number in d* is correct.", context, variationNum, voiceName);

						// ditch this sound, as it failed to find a variation
						m_ScriptedSpeechSlot[scriptedSpeechSlot][m_CurrentPreloadSet].ScriptedSpeechSound->StopAndForget();
						m_ScriptedConversation[conversationLine].SpeechSlotIndex = -1;
						m_ScriptedConversation[conversationLine].FailedToLoad = true;
					}
				}
				else
				{
#if __BANK
					char txt[64]; 
					formatf(txt,"No scripted speech sound in slot: %d preloadset: %d. Bug Default Audio Code for any associated dialogue problems.", scriptedSpeechSlot, m_CurrentPreloadSet);
					naConvWarningf("%s", txt);
#endif
					AbortScriptedConversation(false BANK_ONLY(,txt));
					return AUD_PREPARE_FAILED;
				}
			}
			m_HighestLoadingLine = conversationLine;
			conversationLine++;
		}
		// See how many lines there are remaining, and load that many
		audSpeechSoundInfo speechSoundInfo[AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD];
		u32 numSpeechSounds = 0;
		for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
		{
			if (m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound)
			{
				speechSoundInfo[numSpeechSounds].sound = m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].ScriptedSpeechSound;
				speechSoundInfo[numSpeechSounds].slot  = m_ScriptedSpeechSlot[i][m_CurrentPreloadSet].WaveSlot;
				numSpeechSounds++;
			}
		}
		audSpeechSound::BatchPrepare(speechSoundInfo, numSpeechSounds);

		if(numSpeechSounds == 0)
			m_HighestLoadedLine = m_HighestLoadingLine;
		return AUD_PREPARING; // we want to come in at least once again, to clear everything up
	}
}

#if !__FINAL
void audScriptAudioEntity::SetTimeTriggeredSpeechPlayed(u32 time)
{
	m_TimeTriggeredSpeechPlayed = time;
	m_ShouldSetTimeTriggeredSpeechPlayedStat = true;
}
#endif

u32 audScriptAudioEntity::GetPauseTimeForConversationLine(char* conversationContext)
{
	// Find the bit before the first underscore
	const s32 len = ((s32)strlen(conversationContext));
	if (len>6 && strncmp(conversationContext, "PAUSE_", 6) == 0)
	{
		char number[32] = "";
		formatf(number, 31, "%s", &conversationContext[6]);
		u32 time = atoi(number);
		return time;
	}
	return 0;
}

s32 audScriptAudioEntity::GetOverlapOffsetTime(char* conversationContext)
{
	// Find the bit before the first underscore
	const s32 len = ((s32)strlen(conversationContext));
	if (len>8 && strncmp(conversationContext, "OVERLAP_", 8) == 0)
	{
		char number[32] = "";
		formatf(number, 31, "%s", &conversationContext[8]);
		if (strcmp(number, "PHONE") == 0)
		{
			return g_PhoneOverlapTime;
		}
		u32 time = atoi(number);
		return time;
	}
	return -1;
}

bool audScriptAudioEntity::IsContextAnSFXCommand(char* conversationContext)
{
	const s32 len = ((s32)strlen(conversationContext));
	if (len>4 && strncmp(conversationContext, "SFX_", 4) == 0)
	{
		return true;
	}
	return false;
}

bool audScriptAudioEntity::StartMobilePhoneRinging(s32 ringtoneId)
{
	if (m_CellphoneRingSound)
	{
		return false;
	}

	char soundName[32];

	naCErrorf(ringtoneId>=0 && ringtoneId<10, "Invalid ringtoneId");
	formatf(soundName, "MOBILE_PHONE_RING_%02d", ringtoneId);
	audSoundInitParams initParams;
	initParams.WaveSlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("PLAYER_RINGTONE", 0x0d2645e26));
	initParams.PrepareTimeLimit = 3000;
	initParams.AllowLoad = true;
	audCategory* category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(g_FrontendGameMobileRingingHash);
	initParams.Category = category;
	CreateAndPlaySound_Persistent(soundName, &m_CellphoneRingSound, &initParams);

	if (!m_CellphoneRingSound)
	{
		naWarningf("Audio: ringtone %d not found", ringtoneId);
		audSoundInitParams altInitParams;
		altInitParams.Category = category;
		CreateAndPlaySound_Persistent("MOBILE_PHONE_RING", &m_CellphoneRingSound, &altInitParams);
	}
	return true;
}

void audScriptAudioEntity::SetMobileRingType(s32 ringModeId)
{
	m_MobileRingType = (u8)ringModeId;
	audCategory* category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(g_FrontendGameMobileRingingHash);
	if (category)
	{
		switch (ringModeId)
		{
		case AUD_CELLPHONE_NORMAL:
			category->SetVolume(0.0f);
			break;
		case AUD_CELLPHONE_QUIET:
			category->SetVolume(-12.0f);
			break;
		case AUD_CELLPHONE_SILENT:
			category->SetVolume(g_SilenceVolume);
			break;
		case AUD_CELLPHONE_VIBE:
			category->SetVolume(g_SilenceVolume);
			break;
		case AUD_CELLPHONE_VIBE_AND_RING:
			category->SetVolume(0.0f);
			break;
		default:
			category->SetVolume(0.0f);
			break;
		}
	}
}

void audScriptAudioEntity::SetPedMobileRingType(CPed *ped, const s32 ringType)
{
	if(ped == NULL)
		ped = FindPlayerPed();

	Assert(ped->GetPhoneComponent());
	if (ped->GetPhoneComponent())
		ped->GetPhoneComponent()->SetMobileRingType(ringType);
}

bool audScriptAudioEntity::StartMobilePhoneCalling()
{
	if (m_CellphoneRingSound)
	{
		return false;
	}
	CreateAndPlaySound_Persistent("CALLING_MOBILE_PHONE_RING", &m_CellphoneRingSound);
	return true;
}

void audScriptAudioEntity::StopMobilePhoneRinging()
{
	if (m_CellphoneRingSound)
	{
		m_CellphoneRingSound->StopAndForget();
	}
}

void audScriptAudioEntity::StartPedMobileRinging(CPed *ped, const s32 ringtoneId)
{
	if(ped == NULL)
		ped = FindPlayerPed();

	Assert(ped->GetPhoneComponent());
	if (ped->GetPhoneComponent())
		ped->GetPhoneComponent()->StartMobileRinging(static_cast<u32>(ringtoneId));
}

void audScriptAudioEntity::StopPedMobileRinging(CPed *ped)
{
	// note: calling sound uses this function to stop...
	if (m_CellphoneRingSound)
	{
		m_CellphoneRingSound->StopAndForget();
	}
	if(ped == NULL)
		ped = FindPlayerPed();

	Assert(ped->GetPhoneComponent());
	if (ped->GetPhoneComponent())
		ped->GetPhoneComponent()->StopMobileRinging();
}
void audScriptAudioEntity::StartNewScriptedConversationBuffer()
{
	naConvDisplayf("StartNewScriptedConversationBuffer()");

	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_LINES; i++)
	{
		m_ScriptedConversationBuffer[i].Speaker = -1;
		m_ScriptedConversationBuffer[i].Listener = -1;
		strcpy(m_ScriptedConversationBuffer[i].Context, "");
		strcpy(m_ScriptedConversationBuffer[i].Subtitle, "");
		m_ScriptedConversationBuffer[i].SpeechSlotIndex = -1;
		m_ScriptedConversationBuffer[i].Volume = 0;
		m_ScriptedConversationBuffer[i].PreloadSet = 0;
		m_ScriptedConversationBuffer[i].UnresolvedLineNum = 0;
		m_ScriptedConversationBuffer[i].DoNotRemoveSubtitle = false;
		m_ScriptedConversationBuffer[i].IsRandom = false;
	}
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		if (m_PedBuffer[i])
		{
			AssertEntityPointerValid( m_PedBuffer[i] );
			m_PedBuffer[i] = NULL;
		}
		strcpy(m_VoiceNameBuffer[i], "");
		m_PedIsInvolvedInCurrentConversation[i] = false;
		m_ConvPedRagDollTimer[i] = 0;
		m_ConvPedNotInRagDollTimer[i] = 0;
		m_TimeThrownFromVehicle[i] = 0;
		m_NullPedSpeakerLocations[i].Set(ORIGIN);
		m_NullPedSpeakerEntities[i] = NULL;
	}

	m_Interrupter = NULL;
	m_TimeToInterrupt = 0;
	m_TimeToRestartConversation = 0;
	m_ExtraConvLines = 0;
	m_CodeNamePreLineNeedsResolving = false;
	m_TriggeredAirborneInterrupt = false;
	m_AbortScriptedConversationRequested = false;
	m_ConversationIsSfxOnly = true;
	m_ExternalStreamConversationSFXPlaying = false;
	m_LastInterruptTimeDiff = 0;
	m_PausedAndRepeatingLine = false;

	m_SizeOfSubtitleSubStringArray = 0;

	m_WaitingForFirstLineToPrepareToDuck = false;
	if(!g_DuckForConvAfterPreload && m_ConversationDuckingScene && !m_ScriptSetNoDucking)
		m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 1.f);
}

void audScriptAudioEntity::AddLineToScriptedConversation(u32 lineNumber, s32 speakerNumber, const char* pContext, const char* pSubtitle, s32 listenerNumber, u8 volume, bool isRandom,
														 bool interruptible, bool ducksRadio, bool ducksScore, audConversationAudibility audibility, bool headset, 
														 bool dontInterruptForSpecialAbility, bool isPadSpeakerRoute)
{
	Displayf("AddLineToScripted...line(%d) speaker(%d) Context(%s) headset(%d) isPad(%d)", lineNumber, speakerNumber, pContext, headset, isPadSpeakerRoute);

	bool lineHandled = false;
	u32 realLineNumber = lineNumber + m_ExtraConvLines;
	naConvAssertf(realLineNumber<AUD_MAX_SCRIPTED_CONVERSATION_LINES, "lineNumber is out of bounds");

	strcpy(m_ScriptedConversationBuffer[realLineNumber].Context, pContext);
	// Check if we have to play the headset beep on.
	if( headset && (m_TimeLastBeepOffPlayed + 1000 < audNorthAudioEngine::GetCurrentTimeInMs()))
	{
		if( m_HeadSetBeepOnLineIndex  < 0 )
		{
			m_HeadSetBeepOnLineIndex = realLineNumber;
		}
		m_TimeLastBeepOffPlayed = 0;
	}
	if(IsContextAnSFXCommand(m_ScriptedConversationBuffer[realLineNumber].Context))
	{
		Assertf(!m_CodeNamePreLineNeedsResolving, "Recieved an SFX line while CODENAME_PRE still needs resolving.");

		if(!strcmp(&(m_ScriptedConversationBuffer[realLineNumber].Context[4]), "CODENAME_POST"))
		{
			Assertf(m_SizeOfSubtitleSubStringArray == 0, "Found two procedural lines in the same conversation.  Subtitling will be screwed.  Alert Audio.");
			Assertf(realLineNumber > 0, "Found CODENAME_POST with no preceeding line.  Subtitling will be screwed. Game may crash.  Alert Audio.");
			Assertf(((int)m_ConversationCodeLetter) -64 > 0, "Bad ConversationCodeLetter value.  Subtitling will be screwed. Game may crash.  Alert Audio.");

			char subString[128];
			formatf(subString, 128, "%s-%i (%s)", g_CarCodeUnitTypeStrings[Clamp<int>(((int)m_ConversationCodeLetter) - 65, 0, 25)], m_ConversationCodeNumber, m_ConversationUserName);

			m_SizeOfSubtitleSubStringArray = 1;
			m_ArrayOfSubtitleSubStrings[0].SetLiteralString(subString, CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT, false);

			char codeContext[64];
			formatf(codeContext, "CODENAME_%c", m_ConversationCodeLetter);
			safecpy(m_ScriptedConversationBuffer[realLineNumber].Context, codeContext);
			safecpy(m_ScriptedConversationBuffer[realLineNumber].Subtitle, m_ScriptedConversationBuffer[realLineNumber-1].Subtitle);
			m_ScriptedConversationBuffer[realLineNumber].Speaker = speakerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Listener = listenerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Volume = volume;
			m_ScriptedConversationBuffer[realLineNumber].UnresolvedLineNum = (u8)realLineNumber - 1;
			m_ScriptedConversationBuffer[realLineNumber].DoNotRemoveSubtitle = true;
			m_ScriptedConversationBuffer[realLineNumber].Audibility = audibility;
			m_ScriptedConversationBuffer[realLineNumber].IsInterruptible = interruptible;
			m_ScriptedConversationBuffer[realLineNumber].DucksScore = ducksScore;
			m_ScriptedConversationBuffer[realLineNumber].DucksRadio = ducksRadio;
			m_ScriptedConversationBuffer[realLineNumber].Headset = headset;
			m_ScriptedConversationBuffer[realLineNumber].DontInterruptForSpecialAbility = dontInterruptForSpecialAbility;
			m_ScriptedConversationBuffer[realLineNumber].isPadSpeakerRoute = isPadSpeakerRoute;
			m_ExtraConvLines++;

			realLineNumber = lineNumber + m_ExtraConvLines;
			naConvAssertf(realLineNumber<AUD_MAX_SCRIPTED_CONVERSATION_LINES, "lineNumber is out of bounds");

			formatf(codeContext, "CODENAME_%u", m_ConversationCodeNumber);
			safecpy(m_ScriptedConversationBuffer[realLineNumber].Context, codeContext);
			safecpy(m_ScriptedConversationBuffer[realLineNumber].Subtitle, m_ScriptedConversationBuffer[realLineNumber-1].Subtitle);
			m_ScriptedConversationBuffer[realLineNumber].Speaker = speakerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Listener = listenerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Volume = volume;
			m_ScriptedConversationBuffer[realLineNumber].UnresolvedLineNum = (u8)realLineNumber - 2;
			m_ScriptedConversationBuffer[realLineNumber].DoNotRemoveSubtitle = true;
			m_ScriptedConversationBuffer[realLineNumber].Audibility = audibility;
			m_ScriptedConversationBuffer[realLineNumber].IsInterruptible = interruptible;
			m_ScriptedConversationBuffer[realLineNumber].DucksScore = ducksScore;
			m_ScriptedConversationBuffer[realLineNumber].DucksRadio = ducksRadio;
			m_ScriptedConversationBuffer[realLineNumber].Headset = headset;
			m_ScriptedConversationBuffer[realLineNumber].DontInterruptForSpecialAbility = dontInterruptForSpecialAbility;
			m_ScriptedConversationBuffer[realLineNumber].isPadSpeakerRoute = isPadSpeakerRoute;

			lineHandled = true;
		}
		else if(!strcmp(&(m_ScriptedConversationBuffer[realLineNumber].Context[4]), "CODENAME_PRE"))
		{
			Assertf(m_SizeOfSubtitleSubStringArray == 0, "Found two procedural lines in the same conversation.  Subtitling will be screwed.  Alert Audio.");
			Assertf(((int)m_ConversationCodeLetter) -64 > 0, "Bad ConversationCodeLetter value.  Subtitling will be screwed. Game may crash.  Alert Audio.");

			char subString[128];
			formatf(subString, 128, "%s-%i (%s)", g_CarCodeUnitTypeStrings[Clamp<int>(((int)m_ConversationCodeLetter) - 65, 0, 25)], m_ConversationCodeNumber, m_ConversationUserName);

			m_SizeOfSubtitleSubStringArray = 1;
			m_ArrayOfSubtitleSubStrings[0].SetLiteralString(subString, CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT, false);

			char codeContext[64];
			formatf(codeContext, "CODENAME_%c", m_ConversationCodeLetter);
			safecpy(m_ScriptedConversationBuffer[realLineNumber].Context, codeContext);
			m_ScriptedConversationBuffer[realLineNumber].Speaker = speakerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Listener = listenerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Volume = volume;
			m_ScriptedConversationBuffer[realLineNumber].Audibility = audibility;
			m_ScriptedConversationBuffer[realLineNumber].IsInterruptible = interruptible;
			m_ScriptedConversationBuffer[realLineNumber].DucksScore = ducksScore;
			m_ScriptedConversationBuffer[realLineNumber].DucksRadio = ducksRadio;
			m_ScriptedConversationBuffer[realLineNumber].Headset = headset;
			m_ScriptedConversationBuffer[realLineNumber].UnresolvedLineNum = (u8)realLineNumber + 1;
			m_ScriptedConversationBuffer[realLineNumber].DontInterruptForSpecialAbility = dontInterruptForSpecialAbility;
			m_ScriptedConversationBuffer[realLineNumber].isPadSpeakerRoute = isPadSpeakerRoute;
			m_ExtraConvLines++;

			realLineNumber = lineNumber + m_ExtraConvLines;
			naConvAssertf(realLineNumber<AUD_MAX_SCRIPTED_CONVERSATION_LINES, "lineNumber is out of bounds");

			formatf(codeContext, "CODENAME_%u", m_ConversationCodeNumber);
			safecpy(m_ScriptedConversationBuffer[realLineNumber].Context, codeContext);
			m_ScriptedConversationBuffer[realLineNumber].Speaker = speakerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Listener = listenerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Volume = volume;
			m_ScriptedConversationBuffer[realLineNumber].UnresolvedLineNum = (u8)realLineNumber;
			m_ScriptedConversationBuffer[realLineNumber].DoNotRemoveSubtitle = true;
			m_ScriptedConversationBuffer[realLineNumber].Audibility = audibility;
			m_ScriptedConversationBuffer[realLineNumber].IsInterruptible = interruptible;
			m_ScriptedConversationBuffer[realLineNumber].DucksScore = ducksScore;
			m_ScriptedConversationBuffer[realLineNumber].DucksRadio = ducksRadio;
			m_ScriptedConversationBuffer[realLineNumber].Headset = headset;
			m_ScriptedConversationBuffer[realLineNumber].DontInterruptForSpecialAbility = dontInterruptForSpecialAbility;
			m_ScriptedConversationBuffer[realLineNumber].isPadSpeakerRoute = isPadSpeakerRoute;

			m_CodeNamePreLineNeedsResolving = true;
			lineHandled = true;
		}
		else if(!strcmp(&(m_ScriptedConversationBuffer[realLineNumber].Context[4]), "LOCATION"))
		{
			Assertf(m_SizeOfSubtitleSubStringArray == 0, "Found two procedural lines in the same conversation.  Subtitling will be screwed.  Alert Audio.");
			naAssertf(realLineNumber<AUD_MAX_SCRIPTED_CONVERSATION_LINES, "lineNumber is out of bounds");
			naAssertf(!m_ConversationLocation.IsEqual(ORIGIN), "Attempting to resolve conversation position set to ORIGIN.");

			char locationContext[255];
			char streetName[255];
			char zoneName[255];

			locationContext[0] = '\0';
			streetName[0] = '\0';
			zoneName[0] = '\0';

			GetStreetAndZoneStringsForPos(m_ConversationLocation, streetName, 255, zoneName, 255);

			m_SizeOfSubtitleSubStringArray = 1;
			m_ArrayOfSubtitleSubStrings[0].SetLiteralString(zoneName, CSubStringWithinMessage::LITERAL_STRING_TYPE_PERSISTENT, false);

			formatf(locationContext, "AREA_%s", zoneName);
			safecpy(m_ScriptedConversationBuffer[realLineNumber].Context, locationContext);
			m_ScriptedConversationBuffer[realLineNumber].Speaker = speakerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Listener = listenerNumber;
			m_ScriptedConversationBuffer[realLineNumber].Volume = volume;
			m_ScriptedConversationBuffer[realLineNumber].UnresolvedLineNum = (u8)lineNumber;
			m_ScriptedConversationBuffer[realLineNumber].DoNotRemoveSubtitle = true;
			m_ScriptedConversationBuffer[realLineNumber].Audibility = audibility;
			m_ScriptedConversationBuffer[realLineNumber].IsInterruptible = interruptible;
			m_ScriptedConversationBuffer[realLineNumber].DucksScore = ducksScore;
			m_ScriptedConversationBuffer[realLineNumber].DucksRadio = ducksRadio;
			m_ScriptedConversationBuffer[realLineNumber].Headset = headset;
			m_ScriptedConversationBuffer[realLineNumber].DontInterruptForSpecialAbility = dontInterruptForSpecialAbility;
			m_ScriptedConversationBuffer[realLineNumber].isPadSpeakerRoute = isPadSpeakerRoute;

			lineHandled = true;
		}
	}
	else
	{
		m_ConversationIsSfxOnly = false;
	}
	
	if(!lineHandled)
	{
		safecpy(m_ScriptedConversationBuffer[realLineNumber].Subtitle, pSubtitle);
		m_ScriptedConversationBuffer[realLineNumber].Speaker = speakerNumber;
		m_ScriptedConversationBuffer[realLineNumber].Listener = listenerNumber;
		m_ScriptedConversationBuffer[realLineNumber].Volume = volume;
		m_ScriptedConversationBuffer[realLineNumber].UnresolvedLineNum = (u8)lineNumber;
		m_ScriptedConversationBuffer[realLineNumber].IsRandom = isRandom;
		m_ScriptedConversationBuffer[realLineNumber].Audibility = audibility;
		m_ScriptedConversationBuffer[realLineNumber].IsInterruptible = interruptible;
		m_ScriptedConversationBuffer[realLineNumber].DucksScore = ducksScore;
		m_ScriptedConversationBuffer[realLineNumber].DucksRadio = ducksRadio;
		m_ScriptedConversationBuffer[realLineNumber].Headset = headset;
		m_ScriptedConversationBuffer[realLineNumber].DontInterruptForSpecialAbility = dontInterruptForSpecialAbility;
		m_ScriptedConversationBuffer[realLineNumber].isPadSpeakerRoute = isPadSpeakerRoute;

		if(m_CodeNamePreLineNeedsResolving)
		{
			naAssertf(realLineNumber > 1, "Attempting to resolve SFX_CODENAME_PRE with bad line number.");
			
			m_ScriptedConversationBuffer[realLineNumber].DoNotRemoveSubtitle = true;
			safecpy(m_ScriptedConversationBuffer[realLineNumber-2].Subtitle, m_ScriptedConversationBuffer[realLineNumber].Subtitle);
			safecpy(m_ScriptedConversationBuffer[realLineNumber-1].Subtitle, m_ScriptedConversationBuffer[realLineNumber].Subtitle);
			m_CodeNamePreLineNeedsResolving = false;
		}
	}
}

void audScriptAudioEntity::GetStreetAndZoneStringsForPos(const Vector3& pos, char* streetName, u32 streetNameSize, char* zoneName, u32 zoneNameSize)
{
	CNodeAddress aNode;
	s32 NodesFound = ThePaths.RecordNodesInCircle(pos, 5.0f, 1, &aNode, false, false, false);
	if(NodesFound != 0)
	{
		u32 streetID = ThePaths.FindNodePointer(aNode)->m_streetNameHash;

		if(streetID != 0)
		{
			safecpy( streetName, TheText.Get(streetID, "Streetname"), streetNameSize );
			for(u32 i = 0; i < streetNameSize; i++)
			{
				if(streetName[i] == ' ')
				{
					streetName[i] = '_';
				}
			}
		}
	}

	CPopZone *zone = CPopZones::FindSmallestForPosition(&pos, ZONECAT_ANY, ZONETYPE_SP);
	naAssertf(zone, "Attempted to find smallest zone for position but a null ptr was returned");
	if(zone)
	{
		safecpy( zoneName, zone->GetTranslatedName(), zoneNameSize );
	}
}

void audScriptAudioEntity::SetConversationCodename(char codeLetter, u8 codeNumber, const char* userName)
{
	naConvDebugf1("SetConversationCodename() codeLetter: %c codeNumber: %u userName: %s", codeLetter, codeNumber, userName);
	m_ConversationCodeLetter = codeLetter;
	m_ConversationCodeNumber = codeNumber;
	safecpy(m_ConversationUserName, userName);
}

void audScriptAudioEntity::SetConversationLocation(const Vector3& location)
{
	naConvDebugf1("SetConversationLocation() location: %f %f %f", location.x, location.y, location.z);
	m_ConversationLocation.Set(location);
}

void audScriptAudioEntity::AddConversationSpeaker(u32 speakerConversationIndex, CPed* speaker, char* voiceName)
{
	naConvDisplayf("AddConversationSpeaker() - speakerConversationIndex: %u ped: %s voiceName: %s", 
		speakerConversationIndex, speaker ? speaker->GetModelName() : "NULL", voiceName);

	if(speaker && speaker->GetSpeechAudioEntity() && !speaker->GetSpeechAudioEntity()->GetIsOkayTimeToStartConversation())
	{
		m_AbortScriptedConversationRequested = true;
	}
	naConvAssertf(speakerConversationIndex<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS, "speakerConversationIndex is out of bounds");
//	Assert(speaker);
	naConvAssertf(!m_PedBuffer[speakerConversationIndex], "About to write into a ped buffer that's not null");
	m_PedBuffer[speakerConversationIndex] = speaker;
	if (m_PedBuffer[speakerConversationIndex])
	{
		AssertEntityPointerValid( ((CEntity*)m_PedBuffer[speakerConversationIndex]) );
	}
	strcpy(m_VoiceNameBuffer[speakerConversationIndex], voiceName);
	// As a debug, make sure we've not added the same ped twice - there's almost certainly no good reason to
#if !__FINAL
	for (u32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		if (i!=speakerConversationIndex && speaker)
		{
			if (speaker == m_PedBuffer[i])
			{
				for (s32 j=0; j<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; j++)
				{
					naConvWarningf("id: %d; ped: %p; voice: %s", j, m_PedBuffer[j].Get(), m_VoiceNameBuffer[j]);
				}
				naConvWarningf("Just added id: %d", speakerConversationIndex);
				naConvErrorf("Multiple speakers from one ped in conversation - please assign bug to level designer");
			}
		}
	}
#endif // !__FINAL
}

void audScriptAudioEntity::AddFrontendConversationSpeaker(u32 speakerConversationIndex, char* voiceName)
{
	naConvAssertf(speakerConversationIndex<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS, "speakerConversationIndex out of bounds");
	naConvAssertf(!m_PedBuffer[speakerConversationIndex], "m_pedBuffer[%i] should be null and isn't", speakerConversationIndex);
	m_PedBuffer[speakerConversationIndex] = NULL;
	strcpy(m_VoiceNameBuffer[speakerConversationIndex], voiceName);
}

void audScriptAudioEntity::SetConversationControlledByAnimTriggers(bool enable)
{
	Assertf(!m_ScriptedConversationInProgress, "Changing whether conversations should be controlled by anim triggers while conversation is ongoing.");
	m_AnimTriggersControllingConversation = enable;
}

void audScriptAudioEntity::HandleConversaionAnimTrigger(CPed* ped)
{
	if(!m_AnimTriggersControllingConversation)
	{
		naConvWarningf("Triggering conversation line via anim trigger when conversation isn't set to handle it.  This could seriously screw up conversation logic!!");
		return;
	}

	naConvDebugf1("Setting m_AnimTriggerReported. %u", g_AudioEngine.GetTimeInMilliseconds());

	naConvAssertf(!m_AnimTriggerReported, "Triggering conversation line via anim trigger before last trigger has been processed.  This could seriously screw up conversation logic!!");
	if(!IsPedInCurrentConversation(ped))
	{
		naConvWarningf("Triggering conversation line via anim trigger from a ped not involved in the conversation.  Ignoring the trigger.");
		return;
	}
	m_AnimTriggerReported = true;
}

void audScriptAudioEntity::SetConversationPlaceholder(bool isPlaceholder)
{
	Assertf(!m_ScriptedConversationInProgress, "Changing whether conversations is placeholder while conversation is ongoing.");
	m_IsConversationPlaceholder = isPlaceholder;
}

bool audScriptAudioEntity::StartConversation(bool mobileCall, bool displaySubtitles, bool addToBriefing, bool cloneConversation, bool interruptible, bool preload)
{
	if (m_ScriptedConversationInProgress)
	{
		return false;
	}

	naConvDebugf1("StartConversation() mobileCall: %s displaySubtitles: %s addToBriefing: %s, cloneConversation: %s interruptible: %s preload: %s",
		mobileCall ? "True" : "False", displaySubtitles ? "True" : "False", addToBriefing ? "True" : "False", 
		cloneConversation ? "True" : "False", interruptible ? "True" : "False", preload ? "True" : "False");

	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_LINES; i++)
	{
		// Check all the speaking peds are alive, and don't play conversation if not.
		if (m_ScriptedConversationBuffer[i].Speaker>=0)
		{
			// also need to check that this line isn't an SFX_ lines, as they may reference a speaker number that doesn't exist
			if (!IsContextAnSFXCommand(m_ScriptedConversationBuffer[i].Context))
			{
				// flag this ped as being involved in the conv (listening doesn't count) - we'll check this before aborting for 
				// missing/dead/etc peds during playback.
				m_PedIsInvolvedInCurrentConversation[m_ScriptedConversationBuffer[i].Speaker] = true;
				naConvAssertf(m_ScriptedConversationBuffer[i].Speaker < AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS, "Speaker index out of bounds");
				// it's a valid line, so if it's not frontend, make sure it's a valid ped, and alive (TODO: and not on fire, falling, etc)
				if (m_PedBuffer[m_ScriptedConversationBuffer[i].Speaker])
				{
					if (m_PedBuffer[m_ScriptedConversationBuffer[i].Speaker]->IsDead())
					{
						naConvWarningf("Didn't start scripted conv due to ped being dead for line %s", m_ScriptedConversationBuffer[i].Context);
						return false;
					}
					else if(m_PedBuffer[m_ScriptedConversationBuffer[i].Speaker]->m_nDEflags.bFrozen)
					{
						if(m_ScriptedConversationBuffer[i].Volume == AUD_SPEECH_FRONTEND ||  m_ScriptedConversationBuffer[i].Headset)
							m_PedBuffer[m_ScriptedConversationBuffer[i].Speaker] = NULL;
						else
						{
							naConvWarningf("Didn't start scripted conv due to ped being frozen for line %s", m_ScriptedConversationBuffer[i].Context);
							naConvWarningf("Speaker: %u VoiceName: %s isInteriorFrozen: %s", m_ScriptedConversationBuffer[i].Speaker, m_VoiceNameBuffer[i],
								m_PedBuffer[m_ScriptedConversationBuffer[i].Speaker]->m_nDEflags.bFrozen ? "TRUE" : "FALSE");
							return false;
						}
					}
				}
			}
		}

		m_ScriptedConversation[i].Speaker = m_ScriptedConversationBuffer[i].Speaker;
		m_ScriptedConversation[i].Listener = m_ScriptedConversationBuffer[i].Listener;
		m_ScriptedConversation[i].Volume = m_ScriptedConversationBuffer[i].Volume;
		strcpy(m_ScriptedConversation[i].Context, m_ScriptedConversationBuffer[i].Context);
		strcpy(m_ScriptedConversation[i].Subtitle, m_ScriptedConversationBuffer[i].Subtitle);
		m_ScriptedConversation[i].UnresolvedLineNum = m_ScriptedConversationBuffer[i].UnresolvedLineNum;
		m_ScriptedConversation[i].DoNotRemoveSubtitle = m_ScriptedConversationBuffer[i].DoNotRemoveSubtitle;
		m_ScriptedConversation[i].IsRandom = m_ScriptedConversationBuffer[i].IsRandom;
		m_ScriptedConversation[i].Audibility = m_ScriptedConversationBuffer[i].Audibility;
		m_ScriptedConversation[i].IsInterruptible = m_ScriptedConversationBuffer[i].IsInterruptible;
		m_ScriptedConversation[i].DucksScore = m_ScriptedConversationBuffer[i].DucksScore;
		m_ScriptedConversation[i].DucksRadio = m_ScriptedConversationBuffer[i].DucksRadio;
		m_ScriptedConversation[i].Headset = m_ScriptedConversationBuffer[i].Headset;
		m_ScriptedConversation[i].DontInterruptForSpecialAbility = m_ScriptedConversationBuffer[i].DontInterruptForSpecialAbility;
		m_ScriptedConversation[i].isPadSpeakerRoute = m_ScriptedConversationBuffer[i].isPadSpeakerRoute;
		m_ScriptedConversation[i].HasBeenInterruptedForSpecialAbility = false;
		m_ScriptedConversation[i].HasRepeatedForRagdoll = false;
		m_ScriptedConversation[i].FailedToLoad = false;
		m_ScriptedConversation[i].IsPlaceholder = false;
#if !__FINAL
		m_ScriptedConversation[i].TimePrepareFirstRequested = 0;
#endif

		if(strcmp(m_ScriptedConversation[i].Context, "") != 0)
		{
			naConvDisplayf("AddLineToScriptedConversation() - Line#: %u Context: %s Subtitle: %s Speaker: %d Listener: %d Volume: %u audibility: %u "
				"DoNotRemoveSubtitle: %s IsInterruptible: %s DucksScore: %s DucksRadio: %s Headset: %s, DontInterruptForSpecialAbility: %s",
				i, 
				m_ScriptedConversation[i].Context,
				m_ScriptedConversation[i].Subtitle, 
				m_ScriptedConversation[i].Speaker,
				m_ScriptedConversation[i].Listener, 
				m_ScriptedConversation[i].Volume, 
				m_ScriptedConversation[i].Audibility, 
				m_ScriptedConversation[i].DoNotRemoveSubtitle ? "True" : "False", 
				m_ScriptedConversation[i].IsInterruptible ? "True" : "False", 
				m_ScriptedConversation[i].DucksScore ? "True" : "False", 
				m_ScriptedConversation[i].DucksRadio ? "True" : "False", 
				m_ScriptedConversation[i].Headset ? "True" : "False", 
				m_ScriptedConversation[i].DontInterruptForSpecialAbility ? "True" : "False");
		}
	}

	// Need to do these things after our last chance to abort and return false
	m_CloneConversation = cloneConversation;
	m_DisplaySubtitles = displaySubtitles;
	m_AddSubtitlesToBriefing = addToBriefing;
	m_IsScriptedConversationAMobileCall = mobileCall;
	m_ScriptedConversationInProgress = true;
	m_PlayingScriptedConversationLine = -1;
	m_PauseConversation = preload;
	m_PauseCalledFromScript = preload;
	m_PausedForRagdoll = false;
	m_PauseWaitingOnCurrentLine = false;
	m_ScriptedConversationOverlapTriggerTime = 0;
	m_TimeTrueOverlapWasSet = 0;
	m_OverlapTriggerTimeAdjustedForPredelay = false;
	m_TriggeredOverlapSpeech = false;
	m_WaitingForPreload = false; // though actually we will be almost instantly
	m_CarUpsideDownTimer = 0;
	m_ConversationWasInterruptedForWantedLevel = false;
	m_ConversationIsInterruptible = interruptible;
	m_HasShownSubtitles = true;
	m_LastSpeakingPed = NULL;
	m_MarkerPredelayApplied = 0;
	m_TreatNextLineToPlayAsUrgent = false;
	m_InterruptSpeechRequestTime = 0;
	m_CheckInterruptPreloading = false;
	m_FirstLineHasStarted = false;
	m_FramesVehicleIsInWater = 0;

#if !__FINAL
	m_TimeLastWarnedAboutNonScriptRestart = 0;
#endif

#if __ASSERT
	bool allSpeakersHaveValidVoicename = true;
	u32 exampleLine = 0;
#endif
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		m_Ped[i] = m_PedBuffer[i];
		if (m_Ped[i])
		{
			AssertEntityPointerValid( ((CEntity*)m_Ped[i]) );
		}
		if (m_PedBuffer[i])
		{
			AssertEntityPointerValid( m_PedBuffer[i] );
			m_PedBuffer[i] = NULL;
		}
		strcpy(m_VoiceName[i], m_VoiceNameBuffer[i]);
		m_BlockAnimTime[i] = 0;

		// Check that any ped involved in this conversation has a valid voice. 
		// Can't check this as peds are added, as we don't at that point know if they're just a listener, where no voice is valid,
		// or any details of the conversation to point script at the correct info.
		if (m_PedIsInvolvedInCurrentConversation[i] && strcmp(m_VoiceName[i], "")==0)
		{
			ASSERT_ONLY(allSpeakersHaveValidVoicename = false;)
			// Find the first line said by that speaker
			for (s32 line=0; line<AUD_MAX_SCRIPTED_CONVERSATION_LINES; line++)
			{
				if (m_ScriptedConversation[line].Speaker == i)
				{
					naConvWarningf("No voicename for speaker %d, and line %s.", 
						i, m_ScriptedConversation[line].Context);
					ASSERT_ONLY(exampleLine = line;)
					break;
				}
			}
		}
	}
	// Assert just once if any speaker is missing their voice (usually there'll multiple missing voicenames)
	//naAssertf(allSpeakersHaveValidVoicename, "No voicename set for speaker in scripted conversation including line %s. Log contains more details. Please bug level design. Assign voicename when calling ADD_PED_FOR_DIALOGUE.", 
	//	m_ScriptedConversation[exampleLine].Context);

	SetAllPedsHavingAConversation(true);

	if (atStringHash(m_VoiceName[1]) == ATSTRINGHASH("LAZLOW", 0x052c5cc91) || atStringHash(m_VoiceName[8]) == ATSTRINGHASH("LAZLOW", 0x052c5cc91))
	{
		m_IsScriptedConversationZit = true;
	}
	else
	{
		m_IsScriptedConversationZit = false;
	}

#if __BANK
	if (g_DebugScriptedConvs)
	{
		DebugScriptedConversation();
	}
#endif

	return true;
}

void audScriptAudioEntity::SetAllPedsHavingAConversation(bool isHavingAConversation)
{
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		if (m_Ped[i] && m_Ped[i]->GetSpeechAudioEntity())
		{
			// Control whether they can say anything
			m_Ped[i]->GetSpeechAudioEntity()->SetIsHavingAConversation(isHavingAConversation);
		}
	}
}

bool audScriptAudioEntity::IsScriptedConversationOngoing()
{
	return m_ScriptedConversationInProgress;
}

bool audScriptAudioEntity::ShouldDuckForScriptedConversation()
{
	return !IsFlagSet(audScriptAudioFlags::ScriptedSpeechDuckingDisabled) && !m_ScriptSetNoDucking && !ShouldNotDuckRadioThisLine() &&
		m_ScriptedConversationInProgress && !m_IsScriptedConversationZit && (!m_PauseConversation || m_PauseWaitingOnCurrentLine) &&
		m_FirstLineHasStarted;//m_DuckingForScriptedConversation;
}

bool audScriptAudioEntity::ShouldNotDuckRadioThisLine()
{
	return !m_ScriptedConversationInProgress || m_PlayingScriptedConversationLine < 0 || m_PlayingScriptedConversationLine > AUD_MAX_SCRIPTED_CONVERSATION_LINES-1 ||
			!m_ScriptedConversation[m_PlayingScriptedConversationLine].DucksRadio || m_ScriptSetNoDucking;
}

bool audScriptAudioEntity::ShouldDuckScoreThisLine()
{
	return m_ScriptedConversationInProgress && m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
		m_ScriptedConversation[m_PlayingScriptedConversationLine].DucksScore && !m_ScriptSetNoDucking;
}

void audScriptAudioEntity::SetNoDuckingForConversationFromScript(bool enable)
{
	m_ScriptSetNoDucking = enable; 
}

s32	audScriptAudioEntity::GetVariationChosenForScriptedLine(u32 filenameHash)
{
	if(!IsScriptedConversationOngoing())
		return -1;

	for(int i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_LINES; ++i)
	{
		tScriptedConversationEntry& entry = m_ScriptedConversation[i];
		if(filenameHash == atStringHash(entry.Context) && entry.SpeechSlotIndex >= 0)
		{
			s32 variationNum = m_ScriptedSpeechSlot[entry.SpeechSlotIndex][entry.PreloadSet].VariationNumber;
			return (variationNum > 0) ? variationNum : 0;
		}
	}

	return -1;
}

bool audScriptAudioEntity::IsPedInCurrentConversation(const CPed* ped)
{
	if(!IsScriptedConversationOngoing() || !ped)
		return false;

	for(int i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; ++i)
	{
		if(m_Ped[i] == ped)
		{
			return m_PedIsInvolvedInCurrentConversation[i];
		}
	}

	return false;
}

void audScriptAudioEntity::SetPositionForNullConvPed(s32 speakerNumber, const Vector3& position)
{
	if(Verifyf(!m_Ped[speakerNumber], "Trying to set position for null speaker when speaker isn't actually NULL.") &&
		Verifyf(!m_NullPedSpeakerEntities[speakerNumber], "Trying to set position for null speaker when speaker already has an entity set.")
		)
	{
		m_NullPedSpeakerLocations[speakerNumber].Set(position);
	}
}

void audScriptAudioEntity::SetEntityForNullConvPed(s32 speakerNumber, CEntity* entity)
{
	if(Verifyf(!m_Ped[speakerNumber], "Trying to set position for null speaker when speaker isn't actually NULL.") &&
		Verifyf(m_NullPedSpeakerLocations[speakerNumber].IsEqual(ORIGIN), "Trying to set position for null speaker when speaker already has an entity set.")
		)
	{
		m_NullPedSpeakerEntities[speakerNumber] = entity;
	}
}

void audScriptAudioEntity::SayAmbientSpeechFromPosition(const char* context, const u32 voiceHash, const Vector3& pos, const char* speechParams)
{
	m_AmbientSpeechAudioEntity.Say(context, speechParams, voiceHash, -1, NULL, 0, -1, 1.0f, true, 0, pos);
}

void audScriptAudioEntity::RegisterCarInAir(CVehicle* vehicle)
{
	if(!vehicle || m_ColLevel == AUD_INTERRUPT_LEVEL_SCRIPTED || m_ColLevel == AUD_INTERRUPT_LEVEL_SLO_MO || m_PauseConversation)
		return;

	CPed* interrupter = PickRandomInterrupterInVehicle(vehicle);
	CPed* currentSpeaker = m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 ?
		m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() :
		NULL;

	if(!currentSpeaker || !interrupter || !interrupter->GetSpeechAudioEntity() || !vehicle->ContainsPed(currentSpeaker) || !vehicle->ContainsPed(interrupter))
		return;

	u32 playTime = ~0U;
	bool hasNoMarkers = false;
	m_SpeechRestartOffset = currentSpeaker->GetSpeechAudioEntity()->GetConversationInterruptionOffset(playTime, hasNoMarkers);
	m_LineCheckedForRestartOffset = m_PlayingScriptedConversationLine;
	if(hasNoMarkers)
		m_SpeechRestartOffset = 0;

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	m_ColLevel = AUD_INTERRUPT_LEVEL_IN_AIR;
	m_Interrupter = interrupter;

	m_TriggeredAirborneInterrupt = true;
	m_TimeToInterrupt = currentTime;
	m_TimeLastRegisteredCarInAir = currentTime;

	naConvDisplayf("RegisterCarInAir() - time: %u m_SpeechRestartOffset: %u m_TimeToInterrupt: %u, interrupter: %s",
		currentTime, m_SpeechRestartOffset, m_TimeToInterrupt, interrupter ? interrupter->GetModelName() : "NULL");
}

CPed* audScriptAudioEntity::PickRandomInterrupterInVehicle(CVehicle* vehicle)
{
	if(vehicle)
	{
		if( CNetwork::IsGameInProgress() && vehicle->GetSeatManager())
		{
			const s32 iNumSeats = vehicle->GetSeatManager()->GetMaxSeats();
			s32	Random = fwRandom::GetRandomNumberInRange(0, iNumSeats);

			for (s32 C = 0; C < vehicle->GetSeatManager()->GetMaxSeats(); C++)
			{
				s32	TestPassenger = (Random + C) % iNumSeats;
				if (vehicle->GetSeatManager()->GetPedInSeat(TestPassenger)&& TestPassenger != vehicle->GetDriverSeat())
				{
					CPed * pedInSeat = vehicle->GetSeatManager()->GetPedInSeat(TestPassenger);
					if (!pedInSeat->IsPlayer())
					{
						return pedInSeat;
					}
				}
			}
		}
		else
		{
			return audEngineUtil::ResolveProbability(0.5f) BANK_ONLY(|| g_ForceNonPlayerInterrupt) ? vehicle->PickRandomPassenger() : vehicle->GetDriver();
		}
	}
	return NULL;
}

void audScriptAudioEntity::RegisterSlowMoInterrupt(SlowMoType slowMoType)
{
	if(!IsScriptedConversationOngoing() || m_AnimTriggersControllingConversation) 
		return;
		
	if(m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
		m_ScriptedConversation[m_PlayingScriptedConversationLine].DontInterruptForSpecialAbility)
		return;
	
	//if(m_ScriptedConversation[m_PlayingScriptedConversationLine].HasBeenInterruptedForSpecialAbility)
	//{
	if(!g_AudioEngine.GetSoundManager().IsPaused(4))
	{
		naConvDisplayf("Registering conversation slow mo interrupt. %u", g_AudioEngine.GetTimeInMilliseconds());
		g_AudioEngine.GetSoundManager().Pause(4);
		m_ColLevel = AUD_INTERRUPT_LEVEL_SLO_MO;
		m_ScriptedSpeechTimerPaused = true;
		m_PauseConversation = true; //make sure we don't get stuck
		if( slowMoType == AUD_SLOWMO_SPECIAL)
		{
			m_InSlowMoForSpecialAbility = true;
		}
		if(slowMoType == AUD_SLOWMO_PAUSEMENU)
			m_InSlowMoForPause = true;
		if (m_LastDisplayedTextBlock>0 && (m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES-1 || !m_ScriptedConversation[m_PlayingScriptedConversationLine+1].DoNotRemoveSubtitle))
		{
#if __BANK
			sm_ScriptedLineDebugInfo[0] = '\0';
#endif
			CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(m_LastDisplayedTextBlock, false);
			m_LastDisplayedTextBlock = -1; // to avoid trying to cancel the same subtitle repeatedly. 
		}
	}
	//return;
	//}

	/*m_ScriptedConversation[m_PlayingScriptedConversationLine].HasBeenInterruptedForSpecialAbility = true;

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
	CPed* currentSpeaker = m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES ? 
		m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() : 
		NULL;

	u32 playTime = ~0U;
	bool hasNoMarkers = false;
	m_SpeechRestartOffset = currentSpeaker ? currentSpeaker->GetSpeechAudioEntity()->GetConversationInterruptionOffset(playTime, hasNoMarkers) : 0;
	m_LineCheckedForRestartOffset = m_PlayingScriptedConversationLine;
	if(hasNoMarkers)
		m_SpeechRestartOffset = 0;

	m_ColLevel = AUD_INTERRUPT_LEVEL_SLO_MO;
	m_Interrupter = currentSpeaker;

	m_TimeToInterrupt = currentTime;*/
}

void audScriptAudioEntity::StopSlowMoInterrupt()
{
	naConvDisplayf("Unregistering conversation slow mo interrupt. %u", g_AudioEngine.GetTimeInMilliseconds());
	m_ColLevel = AUD_INTERRUPT_LEVEL_COLLISION_NONE;
	if(m_TimeToRestartConversation == 0)
		m_TimeToRestartConversation = g_AudioEngine.GetTimeInMilliseconds() + 500;
	m_InSlowMoForPause = false;
	if(g_AudioEngine.GetSoundManager().IsPaused(4))
	{
		if(!m_PausedAndRepeatingLine && m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
			!IsContextAnSFXCommand(m_ScriptedConversation[m_PlayingScriptedConversationLine].Context))
			ShowSubtitles();
		g_AudioEngine.GetSoundManager().UnPause(4);
		m_ScriptedSpeechTimerPaused = false;
	}
}

void audScriptAudioEntity::RegisterThrownFromVehicle(CPed* ped)
{
	if(!ped)
		return;

	//if we can catch this early enough, but right now, all the vehicle occupants are showing up null...bummer
	/*if(ped->GetMyVehicle() && ped->GetMyVehicle()->GetDriver() == ped && ped->GetMyVehicle()->GetVehicleType() == VEHICLE_TYPE_BIKE)
	{
		const CSeatManager* seatManager = ped->GetMyVehicle()->GetSeatManager();
		if(seatManager)
		{
			for (s32 i = 0; i < seatManager->GetMaxSeats(); i++)
			{
				if (seatManager->GetPedInSeat(i) && i != ped->GetMyVehicle()->GetDriverSeat())
				{
					RegisterThrownFromVehicle(seatManager->GetPedInSeat(i));
				}
			}
		}
			
	}*/

	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
	if(!m_ScriptedConversationInProgress)
	{
		audSpeechAudioEntity* speechEnt = ped->GetSpeechAudioEntity();
		if(speechEnt)
		{
			audDamageStats damageStats;
			damageStats.Fatal = false;
			damageStats.IsFromAnim = true; //stop any pain level randomization
			damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
			damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
			damageStats.PedWasAlreadyDead = ped->ShouldBeDead();
			speechEnt->InflictPain(damageStats);

			speechEnt->SetHasBeenThrownFromVehicle();
		}
	}
	else
	{
		for(int i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
		{
			if(m_Ped[i] == ped && m_PedIsInvolvedInCurrentConversation[i])
			{
				m_TimeThrownFromVehicle[i] = currentTime;
				return;
			}
		}
	}
}

void audScriptAudioEntity::RegisterPlayerCarCollision(CVehicle* vehicle, CEntity* inflictor, f32 applyDamage)
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();
	m_TimeOfLastCollision = currentTime;

	if(!vehicle || m_ColLevel == AUD_INTERRUPT_LEVEL_SCRIPTED || m_ColLevel == AUD_INTERRUPT_LEVEL_SLO_MO || m_IsScriptedConversationAMobileCall ||
		m_TriggeredAirborneInterrupt)
		return;

	audCollisionLevel colLevel = AUD_INTERRUPT_LEVEL_COLLISION_LOW;
	if(applyDamage >= g_MinHighReactionDamage)
	{
		colLevel = AUD_INTERRUPT_LEVEL_COLLISION_HIGH;
	}
	else if(applyDamage >= g_MinMediumReactionDamage)
	{
		colLevel = AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM;
	}
	else if(applyDamage < g_MinLowReactionDamage)
	{
		return;
	}

	if(colLevel >= AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM && currentTime - m_TimeOfLastCollisionCDI > 30000)
	{
		g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_REPORT_SUSPECT_CRASHED_VEHICLE);
		m_TimeOfLastCollisionCDI = currentTime;
	}

	if(currentTime - m_TimeOfLastChopCollisionReaction > 4000)
	{
		fwEntity* childAttachment = vehicle->GetChildAttachment();
		while(childAttachment)
		{
			if(childAttachment->GetType() == ENTITY_TYPE_PED)
			{
				CPed* childAttPed = static_cast<CPed*>(childAttachment);
				if(childAttPed && childAttPed->GetPedType() == PEDTYPE_ANIMAL && childAttPed->GetSpeechAudioEntity())
				{
					if(colLevel == AUD_INTERRUPT_LEVEL_COLLISION_HIGH)
					{
						childAttPed->GetSpeechAudioEntity()->PlayAnimalVocalization(0x5E666895); //painDeath phash
					}
					else
					{
						childAttPed->GetSpeechAudioEntity()->PlayAnimalVocalization(g_BarkPHash);
					}
					m_TimeOfLastChopCollisionReaction = currentTime;
					break;
				}
			}

			childAttachment = childAttachment->GetSiblingAttachment();
		}
	}

	//if not a scripted conversation, we still might play something
	if (!IsScriptedConversationOngoing())
	{
		if(vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE || vehicle->GetVehicleType() == VEHICLE_TYPE_HELI)
			return;

		if(inflictor && inflictor->GetIsTypeVehicle())
		{
			CVehicle* vehicle = static_cast<CVehicle*>(inflictor);
			if(vehicle && vehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE && vehicle->GetDriver() &&
				vehicle->GetDriver()->GetSpeechAudioEntity())
			{
				vehicle->GetDriver()->GetSpeechAudioEntity()->Say("GENERIC_INSULT", "SPEECH_PARAMS_INTERRUPT");
				return;
			}
		}

		s32 numPassengers = vehicle->GetNumberOfPassenger();
		CPed* driver = vehicle->GetDriver();

		if(numPassengers <= 0 || audEngineUtil::ResolveProbability(0.5f))
		{
			if(driver)
			{
				if(applyDamage >= g_MinLowPainBikeReaction && vehicle->GetVehicleType() == VEHICLE_TYPE_BIKE && driver->IsLocalPlayer())
				{
					audDamageStats damageStats;
					damageStats.Fatal = false;
					damageStats.IsFromAnim = true; //not really, but this forces the pain level to stay low
					damageStats.RawDamage = 1.0f;
					damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;

					driver->GetSpeechAudioEntity()->InflictPain(damageStats);
				}
				else if(colLevel == AUD_INTERRUPT_LEVEL_COLLISION_HIGH && driver->GetSpeechAudioEntity())
					driver->GetSpeechAudioEntity()->Say("GENERIC_CURSE_MED", "SPEECH_PARAMS_INTERRUPT");
			}
		}
		else if(colLevel >= AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM && vehicle->GetSeatManager())
		{
			s32 maxSeats = vehicle->GetSeatManager()->GetMaxSeats();
			if(maxSeats > 0)
			{
				int startSeat = audEngineUtil::GetRandomInteger() % maxSeats;
				for( s32 iSeat = 0; iSeat < maxSeats; iSeat++ )
				{
					int index = (startSeat + iSeat) % maxSeats;
					CPed* pPed = vehicle->GetSeatManager()->GetPedInSeat(index);

					// Special case - allow Franklin and Lamar to comment on crashes when being driven in multiplayer even if the player is controlling them
					const bool allowSpeechFromPlayerPed = CNetwork::IsGameInProgress() && audSpeechAudioEntity::IsMPPlayerPedAllowedToSpeak(pPed);

					if( pPed && (!pPed->IsLocalPlayer() || allowSpeechFromPlayerPed) && pPed != driver && pPed->GetSpeechAudioEntity())
					{
						if( pPed->GetSpeechAudioEntity()->Say("CRASH_GENERIC_DRIVEN", "SPEECH_PARAMS_INTERRUPT") )
						{
							break;
						}
					}
				}
			}
		}

		m_TimeOfLastCollisionReaction = currentTime;
		return;
	}

	if(g_DisableInterrupts || !IsScriptedConversationOngoing() || m_PauseConversation ||
		!m_ConversationIsInterruptible || 
		(m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES && !m_ScriptedConversation[m_PlayingScriptedConversationLine].IsInterruptible))
	{
		naConvDisplayf("Current script conversation interrupted due to a car collision with the player (audScriptAudioEntity::RegisterPlayerCarCollision).");
		return;
	}


	//Collision triggers tend to come in bunches, which leads to a couple issues.
	//We don't want to play interruptions right in a row, but once the min time between has passed, we don't want to start an interruption
	//	in the middle of a series of collision triggers...we should wait for the next initial trigger.
	

	//randomly pick a person in the car to interrupt in SP
	CPed* interrupter = PickRandomInterrupterInVehicle(vehicle);
	CPed* currentSpeaker = m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES ?
		m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() :
		NULL;

	if(!currentSpeaker || !interrupter || !interrupter->GetSpeechAudioEntity() || !vehicle->ContainsPed(currentSpeaker) || !vehicle->ContainsPed(interrupter))
		return;


	if(inflictor && inflictor->GetIsTypePed())
	{
		//hitting peds should be handled through RegisterCarPedCollision
		return;
	}

	u32 playTime = ~0U;
	bool hasNoMarkers = false;
	m_SpeechRestartOffset = currentSpeaker->GetSpeechAudioEntity()->GetConversationInterruptionOffset(playTime, hasNoMarkers);
	m_LineCheckedForRestartOffset = m_PlayingScriptedConversationLine;
	if(hasNoMarkers)
		return;

	if(colLevel > m_ColLevel)
		m_ColLevel = colLevel;
	else
		return;

	if( m_ColLevel != AUD_INTERRUPT_LEVEL_COLLISION_HIGH &&
		(m_TimeOfLastCollisionReaction + g_MinTimeBetweenCollisionReactions > currentTime || 
		 m_TimeOfLastCollisionTrigger + g_MinTimeBetweenCollisionTriggers > currentTime))
		return;

	//naDisplayf("Player car collision %u", currentTime);

	m_TimeOfLastCollisionTrigger = currentTime;
	m_Interrupter = interrupter;
	m_TimeToLowerCollisionLevel = currentTime + 10000;
	m_LastInterruptTimeDiff = 0;

	s32 timeDiff = playTime - m_SpeechRestartOffset;
	
	naConvDisplayf("Car crash interrupt hit with time diff %i %s %u", timeDiff, m_ScriptedConversation[0].Context, g_AudioEngine.GetTimeInMilliseconds());

	//if we've been given a marker in the future, then there's a small enough amount of time until the next marker to let us finish out
	//	playing then pause
	if(timeDiff < 0)
	{
		m_TimeToInterrupt = currentTime - timeDiff;
	}
	else if(timeDiff < 2000)
	{
		m_TimeToInterrupt = currentTime;
		//Adjust overlap time for repeated part of line
		if(m_ScriptedConversationOverlapTriggerTime != 0)
		{
			m_ScriptedConversationOverlapTriggerTime += timeDiff;
			m_LastInterruptTimeDiff = timeDiff;
			naConvDebugf2("Adding %u to make trigger time %u at point 5 %s %u", timeDiff, m_ScriptedConversationOverlapTriggerTime,
				m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
		}
	}
	else
	{
		m_Interrupter = NULL;
		m_TimeToInterrupt = 0;
		m_TimeToRestartConversation = 0;
		m_InterruptTriggered = false;
	}
}

void audScriptAudioEntity::RegisterCarPedCollision(CVehicle* vehicle, f32 applyDamage, CPed* victim)
{
	//Collision triggers tend to come in bunches, which leads to a couple issues.
	//We don't want to play interruptions right in a row, but once the min time between has passed, we don't want to start an interruption
	//	in the middle of a series of collision triggers...we should wait for the next initial trigger.
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	m_TimeOfLastCollision = currentTime;

	if(victim)
	{
		u32 timeOfOldestCollision = ~0U;
		u32 indexOfOldestCollision = 0;
		bool victimFound = false;
		//we use non-paused time for this bookkeeping, as it is compared to the time handed in to PreUpdateService
		for(int i = 0; i<m_CarPedCollisions.GetMaxCount() && victimFound == false; i++)
		{
			if(m_CarPedCollisions[i].Victim == victim)
			{
				m_CarPedCollisions[i].TimeOfCollision = fwTimer::GetTimeInMilliseconds();
				victimFound = true;
			}
			else if(timeOfOldestCollision == ~0U || m_CarPedCollisions[i].TimeOfCollision < timeOfOldestCollision)
			{
				timeOfOldestCollision = m_CarPedCollisions[i].TimeOfCollision;
				indexOfOldestCollision = i;
			}
		}
		if(!victimFound)
		{
			m_CarPedCollisions[indexOfOldestCollision].Victim = victim;
			m_CarPedCollisions[indexOfOldestCollision].TimeOfCollision = fwTimer::GetTimeInMilliseconds();
		}
	}
	
	m_TimeOfLastCarPedCollision = currentTime;

	if(g_DisableInterrupts || !IsScriptedConversationOngoing() || m_PauseConversation || !vehicle || applyDamage < g_MinLowReactionDamage || 
		!m_ConversationIsInterruptible || m_TriggeredAirborneInterrupt || m_IsScriptedConversationAMobileCall ||
		m_ColLevel == AUD_INTERRUPT_LEVEL_SCRIPTED || m_ColLevel == AUD_INTERRUPT_LEVEL_SLO_MO ||
		(m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES && m_ScriptedConversation[m_PlayingScriptedConversationLine].IsInterruptible))
	{
		naConvDisplayf("Current script conversation interrupted due to a car collision with a ped (audScriptAudioEntity::RegisterCarPedCollision).");
		return;
	}
	//randomly pick a person in the car to interrupt in SP
	CPed* interrupter = PickRandomInterrupterInVehicle(vehicle);
	CPed* currentSpeaker = m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 ?
		m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() :
		NULL;

	if(!currentSpeaker || !interrupter || !interrupter->GetSpeechAudioEntity() || !vehicle->ContainsPed(currentSpeaker) || !vehicle->ContainsPed(interrupter))
		return;

	u32 playTime = ~0U;
	bool hasNoMarkers = false;
	m_SpeechRestartOffset = currentSpeaker->GetSpeechAudioEntity()->GetConversationInterruptionOffset(playTime, hasNoMarkers);
	m_LineCheckedForRestartOffset = m_PlayingScriptedConversationLine;
	if(hasNoMarkers)
		return;

	if(AUD_INTERRUPT_LEVEL_COLLISION_LOW > m_ColLevel)
		m_ColLevel = AUD_INTERRUPT_LEVEL_COLLISION_LOW;
	else
		return;

	if( m_TimeOfLastCollisionReaction + g_MinTimeBetweenCollisionReactions > currentTime || 
		m_TimeOfLastCollisionTrigger + g_MinTimeBetweenCollisionTriggers > currentTime)
		return;

	m_TimeOfLastCollisionTrigger = currentTime;
	m_Interrupter = interrupter;
	m_TimeToLowerCollisionLevel = currentTime + 10000;
	m_LastInterruptTimeDiff = 0;

	s32 timeDiff = playTime - m_SpeechRestartOffset;

	naConvDebugf1("Ped crash interrupt hit with time diff %i %s %u", timeDiff, m_ScriptedConversation[0].Context, g_AudioEngine.GetTimeInMilliseconds());

	//if we've been given a marker in the future, then there's a small enough amount of time until the next marker to let us finish out
	//	playing then pause
	if(timeDiff < 0)
	{
		m_TimeToInterrupt = currentTime - timeDiff;
	}
	else if(timeDiff < 2000)
	{
		m_TimeToInterrupt = currentTime;
		//Adjust overlap time for repeated part of line
		if(m_ScriptedConversationOverlapTriggerTime != 0)
		{
			m_ScriptedConversationOverlapTriggerTime += timeDiff;
			m_LastInterruptTimeDiff = timeDiff;
			naConvDebugf2("Adding %u to make trigger time %u at point 6 %s %u", timeDiff, m_ScriptedConversationOverlapTriggerTime,
				m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
		}
	}
	else
	{
		m_Interrupter = NULL;
		m_TimeToInterrupt = 0;
		m_TimeToRestartConversation = 0;
		m_InterruptTriggered = false;
	}
}

void audScriptAudioEntity::RegisterWantedLevelIncrease(CVehicle* vehicle)
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	if(g_DisableInterrupts || !IsScriptedConversationOngoing() || m_PauseConversation || !vehicle  || m_IsScriptedConversationAMobileCall ||
		(m_ConversationWasInterruptedForWantedLevel BANK_ONLY(&& !g_AllowMuliWantedInterrupts)) ||
		!m_ConversationIsInterruptible || m_TriggeredAirborneInterrupt || 
		m_ColLevel == AUD_INTERRUPT_LEVEL_SCRIPTED || m_ColLevel == AUD_INTERRUPT_LEVEL_SLO_MO ||
		(m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES && !m_ScriptedConversation[m_PlayingScriptedConversationLine].IsInterruptible))
	{
		naConvDisplayf("Current script conversation interrupted due to wanted leave increase (audScriptAudioEntity::RegisterWantedLevelIncrease).");
		return;
	}
	m_ConversationWasInterruptedForWantedLevel = true;
	//Collision triggers tend to come in bunches, which leads to a couple issues.
	//We don't want to play interruptions right in a row, but once the min time between has passed, we don't want to start an interruption
	//	in the middle of a series of collision triggers...we should wait for the next initial trigger.

	//randomly pick a person in the car to interrupt in SP
	CPed* interrupter = PickRandomInterrupterInVehicle(vehicle);
	CPed* currentSpeaker = m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 ?
		m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() :
		NULL;

	if(!currentSpeaker || !interrupter || !interrupter->GetSpeechAudioEntity() || !vehicle->ContainsPed(currentSpeaker) || !vehicle->ContainsPed(interrupter))
		return;

	u32 playTime = ~0U;
	bool hasNoMarkers = false;
	m_SpeechRestartOffset = currentSpeaker->GetSpeechAudioEntity()->GetConversationInterruptionOffset(playTime, hasNoMarkers);
	m_LineCheckedForRestartOffset = m_PlayingScriptedConversationLine;
	if(hasNoMarkers)
		return;

	m_ColLevel = AUD_INTERRUPT_LEVEL_WANTED;

	m_Interrupter = interrupter;

	m_LastInterruptTimeDiff = 0;
	s32 timeDiff = playTime - m_SpeechRestartOffset;

	//if we've been given a marker in the future, then there's a small enough amount of time until the next marker to let us finish out
	//	playing then pause
	if(timeDiff < 0)
	{
		m_TimeToInterrupt = currentTime - timeDiff;
	}
	else if(timeDiff < 2000)
	{
		m_TimeToInterrupt = currentTime;
		//Adjust overlap time for repeated part of line
		if(m_ScriptedConversationOverlapTriggerTime != 0)
		{
			m_ScriptedConversationOverlapTriggerTime += timeDiff;
			m_LastInterruptTimeDiff = timeDiff;
			naConvDebugf2("Adding %u to make trigger time %u at point 7 %s %u", timeDiff, m_ScriptedConversationOverlapTriggerTime,
				m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
		}
	}
	else
	{
		m_Interrupter = NULL;
		m_TimeToInterrupt = 0;
		m_TimeToRestartConversation = 0;
		m_InterruptTriggered = false;
	}
}

void audScriptAudioEntity::RegisterScriptRequestedInterrupt(CPed* interrupter, const char* context, const char* voiceName, bool pause BANK_ONLY(, bool force))
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	if(m_ColLevel == AUD_INTERRUPT_LEVEL_SLO_MO)
		return;

#if !__FINAL
	u32 placeholderVoiceNameHash = 0;
	char placeholderVoiceName[128] = "";
	formatf(placeholderVoiceName, sizeof(placeholderVoiceName), "%s_PLACEHOLDER", voiceName);
	placeholderVoiceNameHash = atStringHash(placeholderVoiceName);
#endif

	u32 voiceNameHash = atStringHash(voiceName);

	if(!IsScriptedConversationOngoing() || m_PauseConversation || m_PlayingScriptedConversationLine < 0 || 
			m_PlayingScriptedConversationLine > AUD_MAX_SCRIPTED_CONVERSATION_LINES)
	{
		NOTFINAL_ONLY(bool success = true);
		if(interrupter->GetSpeechAudioEntity())
			NOTFINAL_ONLY(success = AUD_SPEECH_SAY_FAILED != )interrupter->GetSpeechAudioEntity()->Say(context, 
				g_ForceScriptedInterrupts ? "SPEECH_PARAMS_FORCE" : "SPEECH_PARAMS_INTERRUPT", voiceNameHash);
#if !__FINAL
		if(!success && g_EnablePlaceholderDialogue)
			interrupter->GetSpeechAudioEntity()->Say(context, 
			g_ForceScriptedInterrupts ? "SPEECH_PARAMS_FORCE" : "SPEECH_PARAMS_INTERRUPT", placeholderVoiceNameHash);
#endif

		return;
	}

	CPed* currentSpeaker = m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0 ?
		m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker].Get() : NULL;

	if(g_DisableInterrupts || !currentSpeaker ||
		(m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES && !m_ScriptedConversation[m_PlayingScriptedConversationLine].IsInterruptible))
	{
		naConvDisplayf("Current script conversation interrupted from script (audScriptAudioEntity::RegisterScriptReqestedInterrupt)");
	}

	if(!FindPlayerPed() || !FindPlayerPed()->GetMyVehicle() || (interrupter && (FindPlayerPed()->GetMyVehicle() != interrupter->GetMyVehicle())))
	{
		BANK_ONLY(if(!force))
			return;
	}

	u32 playTime = ~0U;
	bool hasNoMarkers = false;
	m_SpeechRestartOffset = currentSpeaker ? currentSpeaker->GetSpeechAudioEntity()->GetConversationInterruptionOffset(playTime, hasNoMarkers) : 0;
	m_LineCheckedForRestartOffset = m_PlayingScriptedConversationLine;
	if(hasNoMarkers && !g_ForceScriptedInterrupts && !IsFlagSet(audScriptAudioFlags::ForceConversationInterrupt))
		return;
	if(pause)
	{
		PauseScriptedConversation(false,true,true);
	}
	if(m_TimeToInterrupt != 0)
	{
		//We're overriding another interrupt, so make sure we adjust overlap trigger time
		m_ScriptedConversationOverlapTriggerTime -= m_LastInterruptTimeDiff;
		naConvDebugf2("Subtracting %u to make trigger time %u at point 9 %s %u", m_LastInterruptTimeDiff, m_ScriptedConversationOverlapTriggerTime,
			m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());

	}
	m_ColLevel = AUD_INTERRUPT_LEVEL_SCRIPTED;

	m_Interrupter = interrupter;
	safecpy(m_InterruptingContext, context);
	m_InterruptingVoiceNameHash = voiceNameHash;
	NOTFINAL_ONLY(m_InterruptingPlaceholderVoiceNameHash = placeholderVoiceNameHash;)

	s32 timeDiff = playTime - m_SpeechRestartOffset;

	naConvDebugf1("Scripted interrupt hit with time diff %i %s %u", timeDiff, m_ScriptedConversation[0].Context, g_AudioEngine.GetTimeInMilliseconds());

	//if we've been given a marker in the future, then there's a small enough amount of time until the next marker to let us finish out
	//	playing then pause
	if(timeDiff < 0)
	{
		m_TimeToInterrupt = currentTime - timeDiff;
	}
	else
	{
		m_TimeToInterrupt = currentTime;
		//Adjust overlap time for repeated part of line
		if(m_ScriptedConversationOverlapTriggerTime != 0)
		{
			m_ScriptedConversationOverlapTriggerTime += timeDiff;
			naConvDebugf2("Adding %u to make trigger time %u at point 8 %s %u", timeDiff, m_ScriptedConversationOverlapTriggerTime,
				m_PlayingScriptedConversationLine >= 0 ? m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "[line -1]", g_AudioEngine.GetTimeInMilliseconds());
		}
	}
}

void audScriptAudioEntity::InterruptConversation()
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	m_LastInterruptTimeDiff = 0;

	if(m_ColLevel != AUD_INTERRUPT_LEVEL_SLO_MO && m_TimeOfLastCollisionReaction + g_MinTimeBetweenCollisionReactions > currentTime)
		return;

	naConvDisplayf("Interrupting conversation %s %u", m_ScriptedConversation[0].Context, g_AudioEngine.GetTimeInMilliseconds());

	audSpeechAudioEntity* speechAudioEntity = &m_SpeechAudioEntity;
	bool success = false;
	bool isPlayerInterrupter = m_Interrupter && m_Interrupter->IsPlayer();

	m_TimeOfLastCollisionReaction = currentTime;
	m_InterruptTriggered = true;
	m_TimeToInterrupt = 0;
	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	bool playVocal = true;
	if(playerVehicle && playerVehicle->InheritsFromHeli())
	{
		playVocal = false;	// we don't want the vocalizations in some cases because they are no radio voice and the scripted speech is
	}

	switch(m_ColLevel)
	{
	case AUD_INTERRUPT_LEVEL_COLLISION_HIGH:
		if(isPlayerInterrupter || !m_Interrupter)
			m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		else
		{
			if(playVocal)
			{
				success = m_Interrupter->NewSayWithParams(
#if __BANK
					g_szHighInterruptContext 
#else
					"CRASH_GENERIC_INTERRUPT"
#endif
					, "SPEECH_PARAMS_INTERRUPT");
			}
			if(!success)
				m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		}
		break;
	case AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM:
		if(isPlayerInterrupter || !m_Interrupter)
			m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		else
		{
			if(playVocal)
			{
				success = m_Interrupter->NewSayWithParams(
#if __BANK
					g_szMediumInterruptContext
#else
					"CRASH_GENERIC_INTERRUPT"
#endif
					, "SPEECH_PARAMS_INTERRUPT");
			}
			if(!success)
				m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		}
		break;
	case AUD_INTERRUPT_LEVEL_COLLISION_LOW:
		//if(isPlayerInterrupter || !m_Interrupter)
			m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		/*else
			m_Interrupter->NewSayWithParams(
#if __BANK
			g_szLowInterruptContext
#else
			"GENERIC_SHOCKED_MED"
#endif
			, "SPEECH_PARAMS_INTERRUPT");*/
		break;
	case AUD_INTERRUPT_LEVEL_WANTED:
		success = m_Interrupter->NewSayWithParams("GENERIC_SHOCKED_HIGH", "SPEECH_PARAMS_INTERRUPT");
		if(!success)
			m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		m_ColLevel = m_TimeToLowerCollisionLevel != 0 ? AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM : AUD_INTERRUPT_LEVEL_COLLISION_NONE;
	case AUD_INTERRUPT_LEVEL_IN_AIR:
		m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		m_ColLevel = m_TimeToLowerCollisionLevel != 0 ? AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM : AUD_INTERRUPT_LEVEL_COLLISION_NONE;
		break;
	case AUD_INTERRUPT_LEVEL_SLO_MO:
		m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		break;
	case AUD_INTERRUPT_LEVEL_SCRIPTED:
		if( m_Interrupter && m_Interrupter->GetSpeechAudioEntity())
		{
			speechAudioEntity = m_Interrupter->GetSpeechAudioEntity();
		}
		if(speechAudioEntity)
		{
			success = speechAudioEntity->Say(m_InterruptingContext, "SPEECH_PARAMS_INTERRUPT", m_InterruptingVoiceNameHash, 
				-1, NULL, 0, -1, 1.0f, true) != AUD_SPEECH_SAY_FAILED;
		}
#if !__FINAL
		if(!success && speechAudioEntity)
		{
			success = speechAudioEntity->Say(m_InterruptingContext, "SPEECH_PARAMS_INTERRUPT", m_InterruptingPlaceholderVoiceNameHash,
				-1, NULL, 0, -1, 1.0f, true) != AUD_SPEECH_SAY_FAILED;
		}
#endif
		if(!success)
		{
			m_TimeToRestartConversation = currentTime + g_TimeToPauseConvOnCollision;
		}
		m_ColLevel = m_TimeToLowerCollisionLevel != 0 ? AUD_INTERRUPT_LEVEL_COLLISION_MEDIUM : AUD_INTERRUPT_LEVEL_COLLISION_NONE;
		m_InterruptingContext[0] = '\0';
		m_InterruptingVoiceNameHash = 0;
		NOTFINAL_ONLY(m_InterruptingPlaceholderVoiceNameHash = 0;)
		break;
	default:
		m_SpeechRestartOffset = 0;
		m_LineCheckedForRestartOffset = -1;
		break;
	}

	if(success)
	{
		m_InterruptSpeechRequestTime = currentTime;
		m_CheckInterruptPreloading = true;
	}

	//kick off restarted line immediately.  We'll cache the predelay applied and use it to adjust the following line's trigger tiem.
	m_ScriptedConversationOverlapTriggerTime = m_TimeToRestartConversation;
	if(m_PlayingScriptedConversationLine < 0)
	{
		m_ScriptedConversationOverlapTriggerTime = 0;
		m_OverlapTriggerTimeAdjustedForPredelay = false;
	}
}

bool audScriptAudioEntity::IsScriptedConversationAMobileCall()
{
	return m_IsScriptedConversationAMobileCall;
}

bool audScriptAudioEntity::IsScriptBankLoaded(u32 bankID)
{
	for(u32 loop = 0; loop < AUD_MAX_SCRIPT_BANK_SLOTS; loop++)
	{
		if(m_BankSlots[loop].BankSlot->GetBankLoadingStatus(bankID) == audWaveSlot::LOADED)
		{
			return true;
		}
	}
	
	return false;
}

bool audScriptAudioEntity::IsScriptedConversationPaused()
{
	return m_PauseConversation;
}

bool audScriptAudioEntity::IsScriptedConversationLoaded()
{
	return m_HighestLoadedLine != -1;
}

s32	audScriptAudioEntity::GetCurrentScriptedConversationLine()
{
	// If we're not mid-conversation, do nothing
	if (!m_ScriptedConversationInProgress)
	{
		return -1;
	}

	return m_PlayingScriptedConversationLine;
}

s32	audScriptAudioEntity::GetCurrentUnresolvedScriptedConversationLine()
{
	// If we're not mid-conversation, do nothing
	if (!m_ScriptedConversationInProgress || m_PlayingScriptedConversationLine < 0 || m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES)
	{
		return -1;
	}

	return m_ScriptedConversationBuffer[m_PlayingScriptedConversationLine].UnresolvedLineNum;
}

s32 audScriptAudioEntity::AbortScriptedConversation(bool finishCurrentLine BANK_ONLY(,const char* reason))
{
	// If we're not mid-conversation, do nothing
	if (!m_ScriptedConversationInProgress)
	{
		return -1;
	}

	s32 currentLine = m_PlayingScriptedConversationLine;

	if (finishCurrentLine)
	{
		// We're going to abort asynchronously, after the current line.
		// On the first Process after we've finished the current line, this'll abort.
		m_AbortScriptedConversationRequested = true;
	}
	else
	{
		// We need to finish right now, and be available for another conversation request immediately.
		if (m_PlayingScriptedConversationLine>=0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
			m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0) // Check we've actually started the conversation
		{
			// See if they're still talking
			if (m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker] &&
				m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity())
			{
				m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity()->StopSpeech();
			}
			else
			{
				m_SpeechAudioEntity.StopSpeech();
			}

		}
		FinishScriptedConversation();
	}
#if __BANK
	if( g_ConversationDebugSpew )
	{
		naConvDisplayf("[Conv Spew] Aborting conversation %s Time: %u Reason :%s", m_ScriptedConversation[0].Context, g_AudioEngine.GetTimeInMilliseconds(),reason);
	}
#endif
	return currentLine;
}

bool audScriptAudioEntity::ShouldWeAbortScriptedConversation()
{
	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (playerVehicle)
	{
		if(playerVehicle->IsOnFire() && !g_AllowVehicleOnFireConversations)
		{
			naConvWarningf("Aborted conv: vehicle on fire");
			return true;
		}
		if(playerVehicle->m_nVehicleFlags.bIsDrowning && !g_AllowVehicleDrowningConversations)
		{
			if(playerVehicle->GetVehicleType() != VEHICLE_TYPE_PLANE &&
				playerVehicle->GetVehicleType() != VEHICLE_TYPE_HELI &&
				playerVehicle->GetVehicleType() != VEHICLE_TYPE_BLIMP)
			{
				naConvWarningf("Aborted conv: vehicle drowning");
				return true;
			}
			else
			{
				m_FramesVehicleIsInWater++;
			}
				
			if(m_FramesVehicleIsInWater > 15)
			{
				naConvWarningf("Aborted conv: vehicle drowning");
				return true;
			}
		}
		else
		{
			m_FramesVehicleIsInWater = 0;
		}
	}
	else
	{
		m_FramesVehicleIsInWater = 0;
	}

	// Check if all our peds are alive, none of them are on fire, we haven't been busted, and the car's not on fire
	naConvCErrorf(m_ScriptedConversationInProgress, "Trying to abort scripted conversion but there isn't one active");
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		if (m_Ped[i] && m_PedIsInvolvedInCurrentConversation[i])
		{
			// Check they're not dead or on fire, or injured
			if ( ((m_Ped[i]->GetIsDeadOrDying()|| m_Ped[i]->IsFatallyInjured() || m_Ped[i]->IsInjured()) && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisableAbortConversationForDeathAndInjury)) ||
				 (g_fireMan.IsEntityOnFire(m_Ped[i]) && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisableAbortConversationForOnFire)) || 
				 m_Ped[i]->m_nDEflags.bFrozen)
			{
				if(m_Ped[i]->m_nDEflags.bFrozen)
				{
					if(m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES &&
						i == m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker && 
						(m_ScriptedConversation[m_PlayingScriptedConversationLine].Volume != AUD_SPEECH_FRONTEND && !m_ScriptedConversationBuffer[i].Headset))
					{	
						naConvWarningf("Aborted conv. Ped frozen: voice: %s line: %s interiorFrozen: %s ", m_VoiceName[i], 
							m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES ? 
							m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "UNKNOWN",
							m_Ped[i]->m_nDEflags.bFrozenByInterior ? "TRUE" : "FALSE");
						return true;
					}
				}
				else
				{
					naConvWarningf("Aborted conv: voice: %s line: %s. Ped dead, dying, (fatally) injured, on fire, or ragdolling", m_VoiceName[i], 
					m_PlayingScriptedConversationLine >= 0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES ? 
					m_ScriptedConversation[m_PlayingScriptedConversationLine].Context : "UNKNOWN");

					return true;
				}
			}
		}
	}
	// Check we're not busted
	CPed* player = CGameWorld::FindLocalPlayer();
	if (player && player->GetIsArrested())
	{
		naConvWarningf("Aborted conv: player arrested");
		return true;
	}
	return false;
}

bool audScriptAudioEntity::ShouldWePauseScriptedConversation(u32 timeInMs, bool checkForInAir)
{
	naConvCErrorf(m_ScriptedConversationInProgress, "Asking if we can pause scripted conversation but there isn't one active");

	if(m_PlayingScriptedConversationLine >= AUD_MAX_SCRIPTED_CONVERSATION_LINES)
	{
		return false;
	}

	if(m_PlayingScriptedConversationLine < 0)
	{
		if(!m_PauseConversation || !m_PausedForRagdoll)
		{
			return false;
		}
	}

	bool isAnyoneRagdolling = false;
	//Not such an aptly named flag, now that we pause isntead of aborting, but you get the idea
	if(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisableAbortConversationForRagdoll) && m_ConversationIsInterruptible)
	{
		for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
		{
			if (m_Ped[i])
			{
				if(m_PedIsInvolvedInCurrentConversation[i])
				{
					//make sure we don't cancel thrown from vehicle until we're sure it's made it through once and called pause
					if(m_TimeThrownFromVehicle[i] != 0 && m_PausedForRagdoll)
						m_TimeThrownFromVehicle[i] = 0;

					if(m_Ped[i]->GetUsingRagdoll())
					{
						if(m_ConvPedRagDollTimer[i] == 0)
							m_ConvPedRagDollTimer[i] = timeInMs;

						//we've been ragdolling for a long time...we better just abort this.
						if(timeInMs - m_ConvPedRagDollTimer[i] > 5000)
						{
							AbortScriptedConversation(false BANK_ONLY(,"Ragdoling for a long time"));
						}

						m_ConvPedNotInRagDollTimer[i] = 0;
					}
					else
					{
						if(m_ConvPedRagDollTimer[i] != 0)
						{
							m_ConvPedNotInRagDollTimer[i] = timeInMs;
						}
						m_ConvPedRagDollTimer[i] = 0;
					}
				}

				bool thrownFromVehicle = m_TimeThrownFromVehicle[i] != 0 && m_TimeThrownFromVehicle[i] < timeInMs;
				bool hasBeenRagdollingLongEnough = m_ConvPedRagDollTimer[i] != 0 && timeInMs - m_ConvPedRagDollTimer[i] > g_MinRagdollTimeBeforeConvPause;
				bool recoveringFromRagdollPause = m_PausedForRagdoll && m_ConvPedNotInRagDollTimer[i] != timeInMs && timeInMs - m_ConvPedNotInRagDollTimer[i] < g_MinNotRagdollTimeBeforeConvRestart;
				if( thrownFromVehicle || hasBeenRagdollingLongEnough || recoveringFromRagdollPause)
				{
					isAnyoneRagdolling = true;
					if(m_PlayingScriptedConversationLine >= 0 &&
						(thrownFromVehicle || ( hasBeenRagdollingLongEnough && i == m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker ))
						)
					{
						// B* 1976594 - If we hit 2 interrupts in a row, then it's possible for m_PauseConversation to be set true before
						// m_PausedForRagdoll is set to true.  This causes the m_TimeThrownFromVehicle[i] above to never be reset to 0,
						// so the conversation never unpauses.
						m_PausedForRagdoll = true;
						if(!m_PauseConversation)
						{
							bool shouldRepeatForRagolling = false;
							if(m_Ped[i]->IsLocalPlayer())
							{
								shouldRepeatForRagolling = !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DontRepeatLineForPlayerRagdolling);
							}
							else
							{
								shouldRepeatForRagolling = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::RepeatLineForRagdolling);
							}

							if(shouldRepeatForRagolling)
							{
								if(m_ScriptedConversation[m_PlayingScriptedConversationLine].HasRepeatedForRagdoll)
								{
									shouldRepeatForRagolling = false;
								}
								else
								{
									m_ScriptedConversation[m_PlayingScriptedConversationLine].HasRepeatedForRagdoll = true;
								}
							}

							PauseScriptedConversation(false, shouldRepeatForRagolling);
						}
					}

					return true;
				}
			}
		}
	}

	//If currently playing line is -1 and we've made it this far, we've already checked that we are currently paused for ragdolling
	if(m_PlayingScriptedConversationLine < 0 && !isAnyoneRagdolling)
	{
		RestartScriptedConversation();
		m_PausedForRagdoll = false;
		return false;
	}

	CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
	if(playerVehicle)
	{
		if (playerVehicle->GetVehicleDamage()->GetIsStuck(VEH_STUCK_ON_ROOF|VEH_STUCK_ON_SIDE, 2000))
		{
			return true;
		}
		if (!playerVehicle->InheritsFromSubmarine() && !playerVehicle->InheritsFromSubmarineCar() && playerVehicle->GetVehicleAudioEntity() && 
			playerVehicle->GetVehicleAudioEntity()->GetDrowningFactor() > 0.0f)
		{
			return true;
		}
		
		//naDisplayf("time in air: %f", playerVehicle->GetTimeInAir());
		if(!g_DisableAirborneInterrupts && (checkForInAir || timeInMs - m_TimeOfLastCollision < 2000))
		{
			u32 timePlayersCarInAir = timeInMs - m_TimePlayersCarWasLastOnTheGround;
			u32 timePlayersCarOnGround = timeInMs - m_TimePlayersCarWasLastInTheAir;

			if(timePlayersCarInAir > g_MinTimeCarInAirForInterrupt && 
				m_TimeOfLastInAirInterrupt < timeInMs - g_MinTimeCarOnGroundForInterrupt &&
				playerVehicle->GetTransform().GetC().GetZf() <= g_MinCarRollForInterrupt)
			{
				if(g_DisableAirborneInterrupts && timeInMs - m_TimeOfLastCollision > 2000)
					m_TriggeredAirborneInterrupt = true;
				//trigger interrupt once, then just return true;
				if(!m_TriggeredAirborneInterrupt)
				{
					RegisterCarInAir(playerVehicle);
				}
				return true;
			}
			else if(m_TriggeredAirborneInterrupt && timePlayersCarInAir == 0)
			{
				u32 totalTimeAirborne = timeInMs - m_TimeLastRegisteredCarInAir;

				//swear and set variables this upon landing
				if(m_Interrupter)
				{
					m_Interrupter->NewSayWithParams(
#if __BANK
						g_szMediumInterruptContext
#else
						"CRASH_GENERIC_INTERRUPT"
#endif
						, "SPEECH_PARAMS_INTERRUPT");
					
					m_InterruptSpeechRequestTime = timeInMs;
				}

				//If we've been up for a while, just swear and then kill the conversation
				if(totalTimeAirborne > g_MinAirBorneTimeToKillConversation)
				{
					AbortScriptedConversation(true BANK_ONLY(,"totalTimeAirborne > g_MinAirBorneTimeToKillConversation"));
					m_TimeOfLastInAirInterrupt = timeInMs;
					m_TriggeredAirborneInterrupt = false;
					return false;
				}
				//if we're in the air a medium time, restart last line from the start
				else if(totalTimeAirborne >= g_MaxAirBorneTimeToApplyRestartOffset)
				{
					m_SpeechRestartOffset = 0;
				}

				m_TimeOfLastInAirInterrupt = timeInMs;
				m_TriggeredAirborneInterrupt = false;
			}

			if(timePlayersCarOnGround < g_MinTimeCarOnGroundToRestart)
			{
				return true;
			}
		}
	}
	
	if(m_PauseConversation && !m_PauseCalledFromScript && !m_InterruptTriggered)
		RestartScriptedConversation();

	m_PausedForRagdoll = false;
	return false;
}

void audScriptAudioEntity::PauseScriptedConversation(bool finishCurrentLine, bool repeatCurrentLine, bool fromScript)
{
	naConvDisplayf("Conversation pause called.  FinishLine: %s, repeatLine: %s fromScript: %s inProgress: %s", finishCurrentLine ? "TRUE" : "FALSE",
		repeatCurrentLine ? "TRUE" : "FALSE", fromScript ? "TRUE" : "FALSE", m_ScriptedConversationInProgress ? "TRUE" : "FALSE");

	// If we're not mid-conversation, do nothing
	if (!m_ScriptedConversationInProgress)
	{
		return;
	}

	if(!m_PauseCalledFromScript)
		m_PauseCalledFromScript = fromScript;

	if(m_PauseConversation && finishCurrentLine)
		return;


	if (finishCurrentLine)
	{
		// We're going to pause asynchronously, after the current line.
		m_PauseConversation = true;
		m_PauseWaitingOnCurrentLine = true;
	}
	else
	{
		naConvDisplayf("Pausing conversation %s %u", m_ScriptedConversation[0].Context, g_AudioEngine.GetTimeInMilliseconds());

		// We need to finish right now.
		if (m_PlayingScriptedConversationLine>=0 && m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0) // Check we've actually started the conversation
		{
			// See if they're still talking
			if (m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker] &&
				m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity())
			{
				m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity()->StopSpeech();
			}
			else
			{
				m_SpeechAudioEntity.StopSpeech();
			}
			// and cancel the current subtitle
			if (m_LastDisplayedTextBlock>0)
			{
#if __BANK
				sm_ScriptedLineDebugInfo[0] = '\0';
#endif
				CMessages::ClearAllDisplayedMessagesThatBelongToThisTextBlock(m_LastDisplayedTextBlock, false);
				m_LastDisplayedTextBlock = -1; // to avoid trying to cancel the same subtitle repeatedly. 
												// Don't think losing our memory of playing it will ever be a problem.
			}

			// higher level code decides whether or not to repeat the line.
			if (repeatCurrentLine && !m_PausedAndRepeatingLine)
			{
				naConvDebugf2("Decrementing playing conversation line 5. %u", g_AudioEngine.GetTimeInMilliseconds());
				m_PlayingScriptedConversationLine--;
			}
			m_PausedAndRepeatingLine = repeatCurrentLine;
		}

		if(m_ConversationDuckingScene)
			m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 0.f);
		m_PauseConversation = true;
		m_PauseWaitingOnCurrentLine = false;
	}

	SetAllPedsHavingAConversation(false);

	return;
}

void audScriptAudioEntity::RestartScriptedConversation(bool startAtMarker, bool fromScript)
{
	if(!m_PauseConversation)
	{
		m_TimeToRestartConversation = 0;
		return;
	}

	if(m_PauseCalledFromScript && !fromScript)
	{
#if !__FINAL
		if(g_AudioEngine.GetTimeInMilliseconds() - m_TimeLastWarnedAboutNonScriptRestart > 3000)
		{
			naConvWarningf("Restarting a conversation from code when it was paused from script.  Ignoring call to restart.");
			m_TimeLastWarnedAboutNonScriptRestart = g_AudioEngine.GetTimeInMilliseconds();
		}
#endif
		return;
	}

	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		m_ConvPedNotInRagDollTimer[i] = 0;
	}

	m_PauseConversation = false;
	m_PauseCalledFromScript = false;
#if !__FINAL
	m_TimeLastWarnedAboutNonScriptRestart = 0;
#endif
	m_ShouldRestartConvAtMarker = startAtMarker;
	SetAllPedsHavingAConversation(true);
	m_Interrupter = NULL;
	m_TimeToInterrupt = 0;
	m_TimeToRestartConversation = 0;
	m_InterruptTriggered = false;
	m_PausedAndRepeatingLine = false;
	m_InterruptSpeechRequestTime = 0;
	m_CheckInterruptPreloading = false;
	//This function seems to be called repeatedly when starting missions, so lets be careful about this.
	if(m_ScriptedConversationInProgress && m_ConversationDuckingScene && !m_ScriptSetNoDucking)
		m_ConversationDuckingScene->SetVariableValue(ATSTRINGHASH("apply", 0xe865cde8), 1.f);
	naConvDisplayf("Restarting conversation %s %u", m_ScriptedConversation[0].Context, g_AudioEngine.GetTimeInMilliseconds());
}

void audScriptAudioEntity::SkipToNextScriptedConversationLine()
{
	// If we're not mid-conversation, do nothing
	if (!m_ScriptedConversationInProgress)
	{
		return;
	}

	// Stop the current line
	if (m_PlayingScriptedConversationLine>=0 && m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker >= 0) // Check we've actually started the conversation
	{
		// See if they're still talking
		if (m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker] &&
			m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity())
		{
			m_Ped[m_ScriptedConversation[m_PlayingScriptedConversationLine].Speaker]->GetSpeechAudioEntity()->StopSpeech();
		}
		else
		{
			m_SpeechAudioEntity.StopSpeech();
		}
	}

	m_ScriptedConversationOverlapTriggerTime = g_AudioEngine.GetTimeInMilliseconds();
	// Don't touch m_PlayingScriptedConversationLine - our next update will trigger that.
}

void audScriptAudioEntity::SetAircraftWarningSpeechVoice(CVehicle* vehicle)
{
	if(!vehicle || !(vehicle->GetVehicleType() == VEHICLE_TYPE_HELI || vehicle->GetVehicleType() == VEHICLE_TYPE_PLANE) ||
		!vehicle->GetVehicleAudioEntity())
		return;

	m_AircraftWarningSpeechEnt.SetAmbientVoiceName(vehicle->GetVehicleAudioEntity()->AircraftWarningVoiceIsMale() ? g_AircraftWarningMale1Hash : g_AircraftWarningFemale1Hash, true);
}

void audScriptAudioEntity::TriggerAircraftWarningSpeech(audAircraftWarningTypes warningType)
{
	if(!g_EnableAircraftWarningSpeech)
		return;

	if(!FindPlayerPed() || !FindPlayerPed()->GetVehiclePedInside() || 
		(FindPlayerPed()->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_PLANE && FindPlayerPed()->GetVehiclePedInside()->GetVehicleType() != VEHICLE_TYPE_HELI) )
		return;

	if(naVerifyf(warningType >= 0 && warningType < AUD_AW_NUM_WARNINGS, "Invalid warning type passed to TriggerAircraftWarningSpeech"))
	{
		m_AircraftWarningInfo[warningType].ShouldPlay = true;
		m_AircraftWarningInfo[warningType].LastTimeTriggered = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		m_ShouldUpdateAircraftWarning = true;
	}
}


void audScriptAudioEntity::UpdateAircraftWarning()
{
	if(m_AircraftWarningSpeechEnt.IsAmbientSpeechPlaying())
		return;

#if __BANK
	if(PARAM_audiodesigner.Get())
		InitAircraftWarningRaveSettings();
#endif

	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	s8 mostRecent = -1;
	for(s8 i=0; i<AUD_AW_NUM_WARNINGS; i++)
	{
		audAircraftWarningStruct& info = m_AircraftWarningInfo[i];
		if(info.ShouldPlay)
		{
			if(currentTime - info.LastTimeTriggered > info.MaxTimeBetweenTriggerAndPlay || currentTime - info.LastTimePlayed < info.MinTimeBetweenPlay)
				info.ShouldPlay = false;
			else if(mostRecent == -1 || info.LastTimeTriggered < m_AircraftWarningInfo[mostRecent].LastTimeTriggered)
				mostRecent = i;
		}
	}

	if(mostRecent == -1)
		m_ShouldUpdateAircraftWarning = false;
	else
	{
		m_AircraftWarningSpeechEnt.SayWhenSafe(m_AircraftWarningInfo[mostRecent].Context, "SPEECH_PARAMS_FORCE_FRONTEND");
		m_AircraftWarningInfo[mostRecent].LastTimePlayed = currentTime;
		m_AircraftWarningInfo[mostRecent].ShouldPlay = false;
	}
}

s32 audScriptAudioEntity::GetIndexFromScriptThreadId(scrThreadId scriptThreadId)
{
	// Find our index, from the thread id
	for (s32 i=0; i<AUD_MAX_SCRIPTS; i++)
	{
		if (scriptThreadId == m_Scripts[i].ScriptThreadId)
		{
			return (s32)i;
		}
	}
	// We didn't find it!
	return -1;
}

s32 audScriptAudioEntity::AddScript()
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();

	naCErrorf(scriptThreadId, "Failed to get thread id");
	BANK_ONLY(m_CurrentScriptName = const_cast<char*>(CTheScripts::GetCurrentScriptName()));

	s32 index = GetIndexFromScriptThreadId(scriptThreadId);

	// DEBUG
	// Make sure we're not already in the list of scripts
	if (index >= 0)
	{
		naAssertf(!m_Scripts[index].m_ScriptisShuttingDown, "Trying to add a script that is shutting down, probably something is calling AddScript that doesn't want to");
		return index;
	}

	// Look through for a free slot
	s32 i=0;
	for (i=0; i<AUD_MAX_SCRIPTS; i++)
	{
		if (m_Scripts[i].ScriptThreadId == THREAD_INVALID)
		{
			// Free slot, so we take it

			// Invoke the constructor
			rage_placement_new(&m_Scripts[i]) audScript();

			m_Scripts[i].ScriptThreadId = scriptThreadId;
			index = i;
			break;
		}
	}
	// Makes sure we actually found a slot
	naAssertf(i<AUD_MAX_SCRIPTS, "Couldn't find a free slot to add script to! Please contact audio team.");

	return index;
}

void audScriptAudioEntity::SetScriptCleanupTime(u32 delay)
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();

	naCErrorf(scriptThreadId, "Failed to get thread id");

	s32 index = GetIndexFromScriptThreadId(scriptThreadId);

	if (index < 0)
	{
		return; 
	}

	m_Scripts[index].m_ScriptShutDownDelay = delay;
}

void audScriptAudioEntity::RemoveScript(scrThreadId scriptThreadId)
{
	naCErrorf(scriptThreadId, "Trying to remove script with invalid thread id");

	s32 scriptIndex = GetIndexFromScriptThreadId(scriptThreadId);
	if (scriptIndex < 0)
	{
		return; 
	}
	BANK_ONLY(m_CurrentScriptName = "");

	if(m_Scripts[scriptIndex].m_ScriptShutDownDelay > 0 && !audNorthAudioEngine::IsScreenFadedOut())
	{
		m_Scripts[scriptIndex].m_ScriptisShuttingDown = true;
		return;
	}

	// Tidy up.
	// If we're using any banks, dereference them, and stop any sounds that we're currently playing (we may want to reconsider that)
	bool hasNetworkBanks = false;
	for(int i=0; i< AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
	{
		hasNetworkBanks |= ((m_Scripts[scriptIndex].BankOverNetwork & BIT(i)) != 0);
		if(m_Scripts[scriptIndex].BankSlotIndex & BIT(i))
		{
			if(m_BankSlots[i].HintLoaded)
			{
				naAssertf(m_BankSlots[i].ReferenceCount == 0, "Hinted bankslot with a ref count, looks like something leaked.");
				m_BankSlots[i].HintLoaded = false;
			}
			else
			{
				naAssertf(m_BankSlots[i].ReferenceCount, "About to decrement a ref count of 0 - looks like something leaked");
				m_BankSlots[i].ReferenceCount--;
			}
			if(hasNetworkBanks)
			{
				for (s32 j=0; j<AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS; j++)
				{
					if(m_BankSlots[i].NetworkScripts[j].IsValid() && CTheScripts::GetCurrentGtaScriptHandler() && m_BankSlots[i].NetworkScripts[j] == static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()))
					{
						m_BankSlots[i].NetworkScripts[j].Invalidate();
						Warningf("Invalidating network script id due to script getting removed, this will hopefully fix B* 2355623");
					}
				}
			}
			m_Scripts[scriptIndex].BankSlotIndex &= ~BIT(i);
			m_Scripts[scriptIndex].BankOverNetwork &= ~BIT(i);
			BANK_ONLY(LogScriptBankRequest("RemoveScript",scriptIndex,m_BankSlots[i].BankId,i));
		}
	}

	for (s32 i=0; i<AUD_MAX_SCRIPT_SOUNDS; i++)
	{
		if (m_ScriptSounds[i].ScriptIndex == scriptIndex)
		{
			// This sound belongs to this script, so stop it (if it still exists).
			if (m_ScriptSounds[i].Sound)
			{
				// Stop the sound over the network.
				if(m_ScriptSounds[i].OverNetwork && NetworkInterface::IsGameInProgress())
				{
					CStopSoundEvent::Trigger(static_cast<u8>(i));
				}

				m_ScriptSounds[i].Sound->StopAndForget();
			}
			ResetScriptSound(i);
		}
	}
 
	// If we're in charge of audio, give that up, and reset anything
	if (m_ScriptInChargeOfAudio>-1 && m_ScriptInChargeOfAudio==scriptIndex)
	{
		ClearUpScriptSetVariables();
		m_ScriptInChargeOfAudio = -1;
	}

	m_Scripts[scriptIndex].Shutdown();
}

void audScriptAudioEntity::ClearUpScriptSetVariables()
{
	DontAbortCarConversationsWithScriptId(false, false, false, m_ScriptInChargeOfAudio);
}

bool audScriptAudioEntity::RequestScriptBank(const char* bankName, bool bOverNetwork, unsigned playerBits, bool hintRequest)
{
	if(naVerifyf(bankName, "NULL string passed to RequestScriptbank from script %s",const_cast<char*>(CTheScripts::GetCurrentScriptName())))
	{
		char validatedBankName[256];
		formatf(validatedBankName, "%s", bankName);
		// Make sure we have the right type of slash
		for (u32 i=0; i<strlen(bankName); i++)
		{
			if (validatedBankName[i] == '/')
			{
				validatedBankName[i] = '\\';
			}
		}

		u32 bankId = SOUNDFACTORY.GetBankIndexFromName(validatedBankName);
		//Displayf("%s %x", validatedBankName, atStringHash(validatedBankName) );

		if (bankId>=AUD_INVALID_BANK_ID)
		{
			char validatedBankNameWithScript[256];
			formatf(validatedBankNameWithScript, "SCRIPT\\%s", validatedBankName);
			bankId = SOUNDFACTORY.GetBankIndexFromName(validatedBankNameWithScript);
			//Displayf("%s %x", validatedBankNameWithScript, atStringHash(validatedBankNameWithScript) );
		}
		if(naVerifyf(bankId < AUD_INVALID_BANK_ID, "Audio: Script bank not found: %s: %s from script %s", bankName, validatedBankName,const_cast<char*>(CTheScripts::GetCurrentScriptName())))
		{
			if(!hintRequest)
			{
				return RequestScriptBank(bankId, bOverNetwork, playerBits);
			}
			else
			{
				HintScriptBank(bankId, bOverNetwork, playerBits);
			}
		}
	}
	return false;
}

void audScriptAudioEntity::HintScriptBank(const char* bankName, bool bOverNetwork, unsigned playerBits)
{
	RequestScriptBank(bankName, bOverNetwork, playerBits, true);
}

void audScriptAudioEntity::HintScriptBank(u32 bankId, bool bOverNetwork, unsigned playerBits)
{
	// Abort early if audio disabled
	if ( Unlikely(!g_AudioEngine.IsAudioEnabled()) )
		return;

	s32 scriptIndex = AddScript();

	s32 freeBankSlot = -1;
	//First of all look if the bank is loaded or already hinted.  We will get a freeBankSlot if there is one that isn't loaded or hinted.
	for(int i=0; i<AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
	{
		if (m_Scripts[scriptIndex].BankSlotIndex & BIT(i) && m_BankSlots[i].BankId == bankId)
		{			
			if( m_BankSlots[i].ReferenceCount != 0 || m_BankSlots[i].HintLoaded)
			{
				return;
			}
			else
			{
				// as a first pass only get the freeBankSlot if the bank isn't hinted.
				if (m_BankSlots[i].BankSlot)
				{
					if (m_BankSlots[i].BankSlot->GetReferenceCount() == 0  && !m_BankSlots[i].HintLoaded)
					{
						freeBankSlot = i;
					}
				}
			}
		}
		else if (m_BankSlots[i].BankId == bankId )
		{
			// already loaded or hinted.
			if((m_BankSlots[i].ReferenceCount != 0 || m_BankSlots[i].HintLoaded)) 
			{
				return;
			}
			else
			{
				// bank is free but still referencing to bankId, use it.
				freeBankSlot = i;
				break;
			}
		}
		else if (m_BankSlots[i].ReferenceCount == 0)
		{
			// as a first pass only get the freeBankSlot if the bank isn't hinted.
			if (m_BankSlots[i].BankSlot)
			{
				if (m_BankSlots[i].BankSlot->GetReferenceCount() == 0  && !m_BankSlots[i].HintLoaded)
				{					
					freeBankSlot = i;
				}
			}
		}
	}
	if (freeBankSlot == -1)
	{
		// If all the banks are loaded or hinted, look if we can kick off any hinted one
		for(int i=0; i<AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
		{
			if (m_BankSlots[i].ReferenceCount == 0)
			{
				if (m_BankSlots[i].BankSlot)
				{
					if (m_BankSlots[i].BankSlot->GetReferenceCount() == 0)
					{				
						ScriptBankNoLongerNeeded(m_BankSlots[i].BankId);
						freeBankSlot = i;
						break;
					}
				} 
			}
		}
		if (freeBankSlot == -1)
		{
			audWarningf("No free bank slots available");
			BANK_ONLY(LogScriptBanks());
			return;
		}
		else
		{
			audWarningf("No free bank slots avaliable, altough some are hinted.");
			BANK_ONLY(LogScriptBanks());
			return;
		}
	}
	// We have a free slot - load into it, up its ref count, and point our script at it.
	// We're free to load the bank into this slot.
	// Don't bother instantly seeing if it's loaded - it won't be.
	m_BankSlots[freeBankSlot].BankSlot->LoadBank(bankId);
	m_BankSlots[freeBankSlot].BankNameHash = atStringHash(audWaveSlot::GetBankName(bankId));
	Assign(m_BankSlots[freeBankSlot].BankId, bankId);
	m_BankSlots[freeBankSlot].HintLoaded = true;
	m_BankSlots[freeBankSlot].State = AUD_SCRIPT_BANK_LOADING;

	BANK_ONLY(LogScriptBankRequest("HINT",scriptIndex,bankId,freeBankSlot));
	if(bOverNetwork && NetworkInterface::IsGameInProgress())
	{
		CAudioBankRequestEvent::Trigger(atStringHash(audWaveSlot::GetBankName(bankId)), static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), true, playerBits);
	}

	// don't record hinted banks to replay
}

audWaveSlot *audScriptAudioEntity::AllocateScriptBank()
{
	for (int i=0; i<(AUD_USABLE_SCRIPT_BANK_SLOTS); i++)
	{
		// See if this slot's empty, and keep a note of it, in case we don't find the one we want already loaded
		if (m_BankSlots[i].ReferenceCount == 0)
		{
			Assign(m_BankSlots[i].BankId, AUD_INVALID_BANK_ID);
			m_BankSlots[i].BankNameHash = g_NullSoundHash;
			m_BankSlots[i].ReferenceCount = 1;
			m_BankSlots[i].State = AUD_SCRIPT_BANK_QUEUED;
			return m_BankSlots[i].BankSlot;
		}
	}
	return NULL;
}

bool audScriptAudioEntity::FreeUpScriptBank(audWaveSlot *slot)
{
	for(s32 i=0; i< AUD_MAX_SCRIPT_BANK_SLOTS; i++)
	{
		if(slot == m_BankSlots[i].BankSlot)
		{
			naAssertf(m_BankSlots[i].ReferenceCount, "About to decrement a ref count of 0 on slot %d - looks like something leaked", i);
			m_BankSlots[i].ReferenceCount--;
			return true;
		}
	}
	return false;
}

bool audScriptAudioEntity::RequestScriptBank(u32 bankId, bool bOverNetwork, unsigned playerBits)
{
	// Abort early if audio disabled
	if ( Unlikely(!g_AudioEngine.IsAudioEnabled()) )
		return true;

	s32 scriptIndex = AddScript();

	s32 freeBankSlot = -1;
	// We request this over and over again, until it returns true, so only want to up the ref count on the slot the first time.
	// After the first time, we have the slot index stored in our script array, so use that to determine if we need to up the count.
	for(int i=0; i<AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
	{
		if (m_Scripts[scriptIndex].BankSlotIndex & BIT(i) && m_BankSlots[i].BankId == bankId)
		{			
			// We've got a bank slot, so see if it's loaded
			if (m_BankSlots[i].State == AUD_SCRIPT_BANK_LOADED)
			{ 
				naAssertf(audWaveSlot::FindLoadedBankWaveSlot(m_BankSlots[i].BankId), "Loaded script bank: Id %u name %s points to a slot not found in the loaded bank waveslot table",bankId,m_BankSlots[i].BankSlot ? m_BankSlots[i].BankSlot->GetLoadedBankName() : "NULL BANK SLOT");
				BANK_ONLY(m_TimeLoadingScriptBank = 0);
				if(m_BankSlots[i].HintLoaded)
				{
					m_BankSlots[i].HintLoaded = false;
					m_BankSlots[i].ReferenceCount = 1;
				}
				//BANK_ONLY(LogScriptBankRequest("REQUEST WITH BANK SLOT LOADED",scriptIndex,bankId,i));
				return true;
			}
			else
			{
				// It's loading - see if it's done already
				audWaveSlot::audWaveSlotLoadStatus loadStatus = m_BankSlots[i].BankSlot->GetBankLoadingStatus(bankId);
				if (loadStatus == audWaveSlot::LOADED)
				{
					naAssertf(audWaveSlot::FindLoadedBankWaveSlot(bankId),
						"Loaded wave slot not found in the loaded bank waveslot table: Id: %u name %s",bankId,m_BankSlots[i].BankSlot ? m_BankSlots[i].BankSlot->GetLoadedBankName() : "NULL BANK SLOT");
					// It's loaded
					m_BankSlots[i].State = AUD_SCRIPT_BANK_LOADED;
					BANK_ONLY(m_TimeLoadingScriptBank = 0);
					if(m_BankSlots[i].HintLoaded)
					{
						m_BankSlots[i].HintLoaded = false;
						m_BankSlots[i].ReferenceCount = 1;
					}
					BANK_ONLY(LogScriptBankRequest("REQUEST WITH BANKSLOT LOADING",scriptIndex,bankId,i));
					return true;
				}
				else
				{
					// We should defo be loading, or we're in a state muddle
					naCErrorf(loadStatus == audWaveSlot::LOADING, "We should be loading, or we're in state middle, loadstatus: %d", loadStatus);
#if __BANK
					m_TimeLoadingScriptBank += fwTimer::GetTimeStepInMilliseconds();
					if(m_TimeLoadingScriptBank > 16000)
					{
						LogScriptBanks();
					}
#endif

					return false;
				}
			}
		}
	}

	// We don't already have a bank slot allocated.

	// Cycle through all our script slots 
	for (int i=0; i<(AUD_USABLE_SCRIPT_BANK_SLOTS); i++)
	{
		if (m_BankSlots[i].BankId == bankId)
		{
			// Something's already using it, up the ref count, point at it, and return depending on whether it's already loaded.
			m_BankSlots[i].ReferenceCount++;
			m_Scripts[scriptIndex].BankSlotIndex |= BIT(i);

			if(bOverNetwork && NetworkInterface::IsGameInProgress())
			{
				BANK_ONLY(LogScriptBankRequest("REQUEST OVER NETWORK:",scriptIndex,bankId,i));
				m_Scripts[scriptIndex].BankOverNetwork |= BIT(i);
				m_Scripts[scriptIndex].PlayerBits |= playerBits;
				CAudioBankRequestEvent::Trigger(atStringHash(audWaveSlot::GetBankName(bankId)), static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), true, playerBits);
			}
	
			if (m_BankSlots[i].State == AUD_SCRIPT_BANK_LOADED)
			{
				naAssertf(audWaveSlot::FindLoadedBankWaveSlot(m_BankSlots[i].BankId), "Loaded script bank: Id %u name %s points to a slot not found in the loaded bank waveslot table",bankId,m_BankSlots[i].BankSlot ? m_BankSlots[i].BankSlot->GetLoadedBankName() : "NULL BANK SLOT");
				BANK_ONLY(m_TimeLoadingScriptBank = 0);
				m_BankSlots[i].HintLoaded = false;
				BANK_ONLY(LogScriptBankRequest("REQUEST IN USE LOADED",scriptIndex,bankId,i));

				return true;
			}
			else
			{
				// It's loading - see if it's done already
				audWaveSlot::audWaveSlotLoadStatus loadStatus = m_BankSlots[i].BankSlot->GetBankLoadingStatus(bankId);
				if (loadStatus == audWaveSlot::LOADED)
				{
					naAssertf(audWaveSlot::FindLoadedBankWaveSlot(bankId),
						"Loaded wave slot not found in the loaded bank waveslot table: Id: %u name %s",bankId,m_BankSlots[i].BankSlot ? m_BankSlots[i].BankSlot->GetLoadedBankName() : "NULL BANK SLOT");
					// It's loaded
					m_BankSlots[i].State = AUD_SCRIPT_BANK_LOADED;
					BANK_ONLY(m_TimeLoadingScriptBank = 0);
					m_BankSlots[i].HintLoaded = false;
					BANK_ONLY(LogScriptBankRequest("REQUEST IN USE LOADING",scriptIndex,bankId,i));

					return true;
				}
				else
				{
					// We should defo be loading, or we're in a state muddle
					naCErrorf(loadStatus == audWaveSlot::LOADING, "We should be loading, or we're in state middle, loadstatus: %d", loadStatus);
#if __BANK
					m_TimeLoadingScriptBank += fwTimer::GetTimeStepInMilliseconds();
					if(m_TimeLoadingScriptBank > 16000)
					{
						LogScriptBanks();
					}
#endif
					return false;  
				}
			}
		}
		// See if this slot's empty, and keep a note of it, in case we don't find the one we want already loaded
		if (m_BankSlots[i].ReferenceCount == 0)
		{
			// Make sure nothing's still playing out of it - might take a few frames to free up, or even longer for a sound to release.
			if (m_BankSlots[i].BankSlot)
			{
				if (m_BankSlots[i].BankSlot->GetReferenceCount() == 0  && !m_BankSlots[i].HintLoaded)
				{
					freeBankSlot = i;
				}
			}
			else
			{
				// We're probably running -noaudiothread - pretend we're loaded already
				Assign(m_BankSlots[i].BankId, bankId);
				m_BankSlots[i].BankNameHash = atStringHash(audWaveSlot::GetBankName(bankId));
				m_BankSlots[i].ReferenceCount = 1;
				m_BankSlots[i].HintLoaded = false;
				m_BankSlots[i].State = AUD_SCRIPT_BANK_LOADED;
				m_Scripts[scriptIndex].BankSlotIndex |= BIT(i);
				if(bOverNetwork && NetworkInterface::IsGameInProgress())
				{
					BANK_ONLY(LogScriptBankRequest("REQUEST OVER NETWORK: NOAUDIO",scriptIndex,bankId,i));
					m_Scripts[scriptIndex].BankOverNetwork |= BIT(i);
					m_Scripts[scriptIndex].PlayerBits |= playerBits;
					CAudioBankRequestEvent::Trigger(atStringHash(audWaveSlot::GetBankName(bankId)), static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), true, playerBits);
				}
				BANK_ONLY(LogScriptBankRequest("REQUEST NOAUDIO",scriptIndex,bankId,i));

				BANK_ONLY(m_TimeLoadingScriptBank = 0);
				return true;
			}
		}
	}

	// We don't have the bank already loaded (or loading), so see if we found a free one while searching. 
	if (freeBankSlot == -1)
	{
		// Check if we have any slot loaded with a hint request, if so, use it.
		for (int i=0; i<(AUD_USABLE_SCRIPT_BANK_SLOTS); i++)
		{
			// See if this slot's empty, and keep a note of it, in case we don't find the one we want already loaded
			if (m_BankSlots[i].HintLoaded)
			{
				// Make sure nothing's still playing out of it - might take a few frames to free up, or even longer for a sound to release.
				if (m_BankSlots[i].BankSlot)
				{
					if (m_BankSlots[i].BankSlot->GetReferenceCount() == 0)
					{
						ScriptBankNoLongerNeeded(m_BankSlots[i].BankId);
						freeBankSlot = i;
					}
				}
			}
		}
		if (freeBankSlot == -1)
		{
			naAssertf(false,"No free bank slots available");
#if 0 // __BANK
			for (int i=0; i<(AUD_MAX_AMBIENT_BANK_SLOTS); i++)
			{
				Displayf("Ref Count for bank %d = %d", i, m_BankSlots[i].ReferenceCount);
				if (m_BankSlots[i].BankSlot)
				{
					Displayf("Sound Ref count %d", m_BankSlots[i].BankSlot->GetReferenceCount());
				}
			}
#endif
			BANK_ONLY(LogScriptBanks());
			return false;
		}
	}
	// We have a free slot - load into it, up its ref count, and point our script at it.
	// We're free to load the bank into this slot.
	// Don't bother instantly seeing if it's loaded - it won't be.
	m_BankSlots[freeBankSlot].BankSlot->LoadBank(bankId);
	Assign(m_BankSlots[freeBankSlot].BankId, bankId);
	m_BankSlots[freeBankSlot].BankNameHash = atStringHash(audWaveSlot::GetBankName(bankId));
	m_BankSlots[freeBankSlot].ReferenceCount = 1;
	m_BankSlots[freeBankSlot].State = AUD_SCRIPT_BANK_LOADING;
	m_Scripts[scriptIndex].BankSlotIndex |= BIT(freeBankSlot);
	BANK_ONLY(LogScriptBankRequest("REQUEST NEW",scriptIndex,bankId,freeBankSlot));

	if(bOverNetwork && NetworkInterface::IsGameInProgress())
	{
		BANK_ONLY(LogScriptBankRequest("REQUEST OVER NETWORK: NEW",scriptIndex,bankId,freeBankSlot));
		m_Scripts[scriptIndex].BankOverNetwork |= BIT(freeBankSlot);
		m_Scripts[scriptIndex].PlayerBits |= playerBits;
		CAudioBankRequestEvent::Trigger(atStringHash(audWaveSlot::GetBankName(bankId)), static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), true, playerBits);
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSndLoadScriptWaveBank>(CPacketSndLoadScriptWaveBank(freeBankSlot, m_BankSlots[freeBankSlot].BankNameHash));
	}
#endif

	return false; // as its not actually loaded yet.
}

#if GTA_REPLAY
bool audScriptAudioEntity::ReplayLoadScriptBank(u32 bankNameHash, u32 bankSlot)
{
	u32 bankId = SOUNDFACTORY.GetBankIndexFromName(bankNameHash);
	if(bankId == ~0U)
	{
		Displayf("Attempt to load script wave bank that does not exist %u(name hash)", bankNameHash);
		// we return true which stops the preload attempts
		return true;
	}

	if(m_BankSlots[bankSlot].ReferenceCount == 0)
	{
		// Make sure nothing's still playing out of it - might take a few frames to free up, or even longer for a sound to release.
		if (m_BankSlots[bankSlot].BankSlot)
		{
			if (m_BankSlots[bankSlot].BankSlot->GetReferenceCount() != 0)
			{
				Displayf("Stalling due to sounds playing from an existing script bank %u(name hash) ref count(%d)", bankNameHash, m_BankSlots[bankSlot].BankSlot->GetReferenceCount());
				return false;
			}
		}

		m_BankSlots[bankSlot].BankSlot->LoadBank(bankId);
		Assign(m_BankSlots[bankSlot].BankId, bankId);
		m_BankSlots[bankSlot].BankNameHash = atStringHash(audWaveSlot::GetBankName(bankId));
		m_BankSlots[bankSlot].ReferenceCount = 1;
		m_BankSlots[bankSlot].State = AUD_SCRIPT_BANK_LOADING;
		m_BankSlots[bankSlot].HintLoaded = false;

		// we won't have loaded yet
		return false;
	}
	else if(m_BankSlots[bankSlot].BankNameHash == bankNameHash)
	{
		audWaveSlot::audWaveSlotLoadStatus loadStatus = m_BankSlots[bankSlot].BankSlot->GetBankLoadingStatus(bankId);
		if(loadStatus == audWaveSlot::LOADED)
		{
			m_BankSlots[bankSlot].State = AUD_SCRIPT_BANK_LOADED;
			return true;
		}
		else if(loadStatus == audWaveSlot::FAILED)
		{
			return true;
		}
	}
	else if(m_BankSlots[bankSlot].ReferenceCount > 0 && m_BankSlots[bankSlot].BankNameHash != bankNameHash)
	{
		if (m_BankSlots[bankSlot].BankSlot)
		{
			if (m_BankSlots[bankSlot].BankSlot->GetReferenceCount() != 0)
			{
				// sound still ref bank so we can't load yet, return true for preload
				return false;
			}
		}
		// setup new bank load - only do this if the bank isn't already loaded, to prevent spamming load requests (which may block other legitimate loads)
		if(m_BankSlots[bankSlot].BankSlot->GetLoadedBankId() != bankId)
		{
			m_BankSlots[bankSlot].BankSlot->LoadBank(bankId);
		}

		Assign(m_BankSlots[bankSlot].BankId, bankId);
		m_BankSlots[bankSlot].BankNameHash = atStringHash(audWaveSlot::GetBankName(bankId));
		m_BankSlots[bankSlot].ReferenceCount = 1;
		m_BankSlots[bankSlot].State = AUD_SCRIPT_BANK_LOADING;
		m_BankSlots[bankSlot].HintLoaded = false;

		// we won't have loaded yet
		return false;

	}

	return false;
}

void audScriptAudioEntity::ClearRecentPedCollisions()
{
	m_TimeOfLastCarPedCollision = 0;
	m_TimeLastTriggeredPedCollisionScreams = 0;
	for(int i=0; i<m_CarPedCollisions.GetMaxCount(); i++)
	{
		m_CarPedCollisions[i].Victim = NULL;
		m_CarPedCollisions[i].TimeOfCollision = 0;
	}
}

void audScriptAudioEntity::ReplayFreeScriptBank(u32 bankSlot)
{
	if(!m_BankSlots[bankSlot].HintLoaded)
	{
		Assign(m_BankSlots[bankSlot].BankId, AUD_INVALID_BANK_ID);
		m_BankSlots[bankSlot].BankNameHash = g_NullSoundHash;
		m_BankSlots[bankSlot].ReferenceCount = 0;
		m_BankSlots[bankSlot].State = AUD_SCRIPT_BANK_LOADING;
		m_BankSlots[bankSlot].HintLoaded = false;
	}
}

void audScriptAudioEntity::CleanUpScriptAudioEntityForReplay()
{
	for(int i=0; i<AUD_MAX_SCRIPTS; i++)
	{
		m_Scripts[i].m_FlagResetState.Reset(); // clear out the flags here because Shutdown may assert
		m_Scripts[i].Shutdown();
	}
}

void audScriptAudioEntity::CleanUpScriptBankSlotsAfterReplay()
{
	for (int i=0; i<AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
	{
		ReplayFreeScriptBank(i);
	}

	// reset some time variables incase the time was messed up during the replay
	m_LastMobileInterferenceTime = 0;
	m_NextTimeScriptedConversationCanStart = 0;

	m_TimeOfLastCollisionReaction = 0;
	m_TimeOfLastCollisionTrigger = 0;
	m_TimeToLowerCollisionLevel = 0;

	m_TimePlayersCarWasLastOnTheGround = 0;
	m_TimePlayersCarWasLastInTheAir = 0;
	m_TimeOfLastInAirInterrupt = 0;
	m_TimeLastRegisteredCarInAir = 0;

	m_TimeOfLastCarPedCollision = 0;
	m_TimeLastTriggeredPedCollisionScreams = 0;

	m_TimeOfLastChopCollisionReaction = 0;
}
#endif

void audScriptAudioEntity::ScriptBankNoLongerNeeded(const char * bankName)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif
	// Abort early if audio disabled
	if (Unlikely(!g_AudioEngine.IsAudioEnabled()))
	{
		return;
	}

	char validatedBankName[256];
	formatf(validatedBankName, "%s", bankName);
	// Make sure we have the right type of slash
	for (u32 i=0; i<strlen(bankName); i++)
	{
		if (validatedBankName[i] == '/')
		{
			validatedBankName[i] = '\\';
		}
	}

	u32 bankId = SOUNDFACTORY.GetBankIndexFromName(validatedBankName);
	if (bankId>=AUD_INVALID_BANK_ID)
	{
		char validatedBankNameWithScript[256];
		formatf(validatedBankNameWithScript, "SCRIPT\\%s", validatedBankName);
		bankId = SOUNDFACTORY.GetBankIndexFromName(validatedBankNameWithScript);
	}

	ScriptBankNoLongerNeeded(bankId);
}

void audScriptAudioEntity::ScriptBankNoLongerNeeded(u32 bankId)
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif
	s32 scriptIndex = AddScript();
	if (naVerifyf(scriptIndex >= 0 && scriptIndex<AUD_MAX_SCRIPTS, "No valid scriptIndex in ScriptBankNoLongerNeeded: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		for(s32 i=0; i< AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
		{
			if(bankId == m_BankSlots[i].BankId)
			{
				if (m_BankSlots[i].HintLoaded)
				{
					naAssertf(m_BankSlots[i].ReferenceCount == 0,"Trying to free up a hint loaded bank with some references");
					m_BankSlots[i].HintLoaded = false;
				}
				else if ( m_Scripts[scriptIndex].BankSlotIndex & BIT(i) )
				{
					naAssertf(m_BankSlots[i].ReferenceCount, "About to decrement a ref count of 0 on slot %d - looks like something leaked", i);
					if(!m_BankSlots[i].ReferenceCount)
					{
						continue;
					}
					m_BankSlots[i].ReferenceCount--;
					m_Scripts[scriptIndex].BankSlotIndex &= ~BIT(i);
					if((m_Scripts[scriptIndex].BankOverNetwork & BIT(i)) && NetworkInterface::IsGameInProgress())
					{
						m_Scripts[scriptIndex].BankOverNetwork &= ~BIT(i);
						CAudioBankRequestEvent::Trigger(atStringHash(audWaveSlot::GetBankName(bankId)), static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), false, m_Scripts[scriptIndex].PlayerBits);
					}
					BANK_ONLY(LogScriptBankRequest("FREE BANK ID",scriptIndex,m_BankSlots[i].BankId,i));
				}
			}
		}
	}

}

void audScriptAudioEntity::ScriptBankNoLongerNeeded()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif

	// Abort early if audio disabled
	if ( Unlikely(!g_AudioEngine.IsAudioEnabled()) )
		return;

	s32 scriptIndex = AddScript();
	bool hasNetworkBanks = false;
	u32 playerBits = AUD_NET_ALL_PLAYERS;
	if (naVerifyf(scriptIndex >= 0 && scriptIndex<AUD_MAX_SCRIPTS, "No valid scriptIndex in ScriptBankNoLongerNeeded: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		playerBits = m_Scripts[scriptIndex].PlayerBits;

		for(int i=0; i< AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
		{
			hasNetworkBanks |= ((m_Scripts[scriptIndex].BankOverNetwork & BIT(i)) != 0);
			
			if(m_BankSlots[i].HintLoaded )
			{
				naAssertf(m_BankSlots[i].ReferenceCount == 0,"Trying to free up a hint loaded bank with some references");
				m_BankSlots[i].HintLoaded = false;
			}
			else if(m_Scripts[scriptIndex].BankSlotIndex & BIT(i))
			{
				naAssertf(m_BankSlots[i].ReferenceCount, "About to decrement a ref count of 0 on bank %d - looks like something leaked", i);
				if(!m_BankSlots[i].ReferenceCount)
				{
					continue;
				}
				m_BankSlots[i].ReferenceCount--;
				m_Scripts[scriptIndex].BankSlotIndex &= ~BIT(i);
				m_Scripts[scriptIndex].BankOverNetwork &= ~BIT(i);
				BANK_ONLY(LogScriptBankRequest("FREE",scriptIndex,m_BankSlots[i].BankId,i));
			}
		}
		naAssertf(m_Scripts[scriptIndex].BankSlotIndex == 0, "Have released all bank slots on script (%s , index: %d), but Bankslotindex is not 0", CTheScripts::GetCurrentScriptName(), scriptIndex);
		naAssertf(m_Scripts[scriptIndex].BankOverNetwork == 0, "Have released all bank slots on script (%s , index: %d), but BankOverNetwork is not 0", CTheScripts::GetCurrentScriptName(), scriptIndex);
	}

	if(hasNetworkBanks && NetworkInterface::IsGameInProgress())
	{
		CAudioBankRequestEvent::Trigger(0, static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId()), false, playerBits);
	}
}

bool audScriptAudioEntity::RequestScriptBankFromNetwork(u32 bankHash, CGameScriptId& scriptId)
{
	// Abort early if audio disabled
	if ( Unlikely(!g_AudioEngine.IsAudioEnabled()) )
		return true;

	s32 bankSlot = -1;
	bool hadBank = false;

	u32 bankId = g_AudioEngine.GetSoundManager().GetFactory().GetBankNameIndexFromHash(bankHash);

	// Cycle through all our bank slots
	for (int i=0; i<(AUD_USABLE_SCRIPT_BANK_SLOTS); i++)
	{
		if (m_BankSlots[i].BankId == bankId)
		{
			bankSlot = i;
			naDebugf3("RequestBank: Bank ID in use. BankSlot:%d, BankID:%d", i, bankId);
			BANK_ONLY(LogNetworkScriptBankRequest("REQUEST OVER NETWORK: IN USE",scriptId.GetScriptName(),bankId,i));
			hadBank = true;
			break;
		}
	}

	// no bank slot, so find one
	if(bankSlot == -1)
	{
		// See if this slot's empty, and keep a note of it, in case we don't find the one we want already loaded
		for (int i=0; i<(AUD_USABLE_SCRIPT_BANK_SLOTS); i++)
		{
			if (m_BankSlots[i].ReferenceCount == 0)
			{
				// Make sure nothing's still playing out of it - might take a few frames to free up, or even longer for a sound to release.
				if (m_BankSlots[i].BankSlot)
				{
					if (m_BankSlots[i].BankSlot->GetReferenceCount() == 0)
					{
						bankSlot = i;
						break;
					}
				}
				else
				{
					// We're probably running -noaudiothread - pretend we're loaded already
					Assign(m_BankSlots[i].BankId, bankId);
					m_BankSlots[i].BankNameHash = bankHash;
					m_BankSlots[i].ReferenceCount = 1;
					m_BankSlots[i].State = AUD_SCRIPT_BANK_LOADED;
					bankSlot = i;
					BANK_ONLY(LogNetworkScriptBankRequest("REQUEST NOAUDIO OVER NETWORk",scriptId.GetScriptName(),bankId,i));
					break;
				}
			}
		}
	}

	naDebugf3("RequestBank: Using Slot - BankSlot:%d, BankID:%d", bankSlot, bankId);
	BANK_ONLY(LogNetworkScriptBankRequest("REQUEST OVER NETWORK: USING SLOT",scriptId.GetScriptName(),bankId,bankSlot));

	// check that we have a valid bank slot
	if(!(bankSlot >= 0))
	{
		Warningf("Could not allocate a bank slot!");
		return false;
	}

	// if we didn't already have this bank, load it
	if(!hadBank)
	{
		m_BankSlots[bankSlot].BankSlot->LoadBank(bankId);
		Assign(m_BankSlots[bankSlot].BankId, bankId);
		m_BankSlots[bankSlot].BankNameHash = bankHash;
	}

	// check if we have a reference from this script - if not, add one
	bool bFound = false;
	for (int i=0; i<(AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS);i++)
	{
		if (m_BankSlots[bankSlot].NetworkScripts[i] == scriptId)
		{
			bFound = true;
			naDisplayf("RequestBank: Found Script Reference - NetSlot:%d", i);
			BANK_ONLY(LogNetworkScriptBankRequest("RequestBank: Found Script Reference",scriptId.GetScriptName(),bankId,bankSlot));
			break;
		}
	}

	if(!bFound)
	{
		// find an empty slot
		for (int i=0; i<(AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS);i++)
		{
			if (!m_BankSlots[bankSlot].NetworkScripts[i].IsValid())
			{
				m_BankSlots[bankSlot].ReferenceCount++;
				m_BankSlots[bankSlot].NetworkScripts[i] = scriptId;
				bFound = true;
				naDisplayf("RequestBank: Adding Script Reference - NetSlot:%d, Ref:%d", i, m_BankSlots[bankSlot].ReferenceCount);
				BANK_ONLY(LogNetworkScriptBankRequest("RequestBank: Adding Script Reference",scriptId.GetScriptName(),bankId,bankSlot));
				break;
			}
		}
	}

	// check that we have a valid network script slot
	if(!bFound)
	{
		BANK_ONLY(LogScriptBanks(););
		audAssertf(false, "Could not allocate a network script slot for bank slot %d! Check preceeding output for Info", bankSlot);		
		return false;
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSndLoadScriptWaveBank>(CPacketSndLoadScriptWaveBank(bankSlot, bankHash));
	}
#endif

	// success
	return true;
}

void audScriptAudioEntity::ReleaseScriptBankFromNetwork(u32 bankHash, CGameScriptId& scriptId)
{
	u32 bankId = g_AudioEngine.GetSoundManager().GetFactory().GetBankNameIndexFromHash(bankHash);

	// Cycle through all our bank slots
	for (int i=0; i<(AUD_USABLE_SCRIPT_BANK_SLOTS); i++)
	{
		// If we are wiping out all bank slots for this script or a particular bank slot
		if ((bankId == audScriptBankSlot::INVALID_BANK_ID) || (m_BankSlots[i].BankId == bankId))
		{
			for (s32 j=0; j<AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS; j++)
			{
				if(m_BankSlots[i].NetworkScripts[j].IsValid() && m_BankSlots[i].NetworkScripts[j] == scriptId)
				{
					naAssertf(m_BankSlots[i].ReferenceCount, "About to decrement a ref count of 0 - looks like something leaked");
					m_BankSlots[i].ReferenceCount--; 
					m_BankSlots[i].NetworkScripts[j].Invalidate();
					naDisplayf("ReleaseBank: BankSlot:%d, NetSlot:%d, BankID:%d, Ref:%d", i, j, m_BankSlots[i].BankId, m_BankSlots[i].ReferenceCount);
					BANK_ONLY(LogNetworkScriptBankRequest("ReleaseBank FROM NETWORK:",scriptId.GetScriptName(),bankId,i));
				}
			}
		}
	}
}

s32 audScriptAudioEntity::GetSoundId()
{
	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return -1;
	}	

	s32 freeSoundId = FindFreeSoundId();

	if (freeSoundId == -1)
	{
		naDisplayf("Script: %s index: %d requesting sound id (none free)",CTheScripts::GetCurrentScriptName(), scriptIndex);
		return -1;
	}
	else
	{
		naDisplayf("Script: %s index: %d requesting sound id (returned %d)",CTheScripts::GetCurrentScriptName(), scriptIndex, freeSoundId);
	}

	BANK_ONLY(formatf(m_ScriptSounds[freeSoundId].ScriptName, CTheScripts::GetCurrentScriptName());)
	BANK_ONLY(m_ScriptSounds[freeSoundId].TimeAllocated = fwTimer::GetTimeInMilliseconds());
	BANK_ONLY(m_ScriptSounds[freeSoundId].FrameAllocated = fwTimer::GetFrameCount());

	m_ScriptSounds[freeSoundId].ScriptIndex = scriptIndex;
	m_ScriptSounds[freeSoundId].ScriptHasReference = true;

	return freeSoundId;
}

s32 audScriptAudioEntity::FindFreeSoundId()
{
	// Trawl through for a free sound
	s32 i=0;
	for (i=0; i<AUD_MAX_SCRIPT_SOUNDS; i++)
	{
		// See if this one's free
		if (m_ScriptSounds[i].ScriptIndex == -1 && m_ScriptSounds[i].NetworkId == audScriptSound::INVALID_NETWORK_ID)
		{
			return i;
		}
	}
	// We didn't find a free one
	naAssertf(false,"No free sound id");
	return -1;
}

void audScriptAudioEntity::ReleaseSoundId(s32 soundId)
{
	if(soundId < 0)
	{
		return;
	}

	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}
	naDisplayf("Script: %s index: %d releasing sound id %d", CTheScripts::GetCurrentScriptName(), scriptIndex, soundId);

	// Make sure it's our sound to free, and that we had a reference to it
	naAssertf(m_ScriptSounds[soundId].ScriptHasReference, "%s:Level design: You appear to be calling RELEASE_SOUND_ID() on a sound id that has already been released", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	naAssertf(!m_ScriptSounds[soundId].ScriptHasReference || m_ScriptSounds[soundId].ScriptIndex == scriptIndex, "%s:Level design: Check you're calling RELEASE_SOUND_ID() on the correct SoundID. This script is %s (%u) but sound ID %d is owned by %s (%u)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), GetScriptName(scriptIndex), scriptIndex, soundId, GetScriptName(m_ScriptSounds[soundId].ScriptIndex), m_ScriptSounds[soundId].ScriptIndex);	

	m_ScriptSounds[soundId].ScriptHasReference = false;
}

audSound* audScriptAudioEntity::PlaySoundAndPopulateScriptSoundSlot(s32 soundId, const char* soundSetName, const char* soundName, audSoundInitParams* initParams, 
																	bool bOverNetwork, int nNetworkRange, naEnvironmentGroup* environmentGroup, const CEntity* pEntity, const bool ownsEnvGroup, bool enableOnReplay)
{
	return PlaySoundAndPopulateScriptSoundSlot(soundId, soundSetName ? atStringHash(soundSetName) : 0, soundName ? atStringHash(soundName) : g_NullSoundHash, initParams, bOverNetwork, nNetworkRange, environmentGroup, pEntity, ownsEnvGroup, enableOnReplay OUTPUT_ONLY(, soundSetName, soundName));
}

audSound* audScriptAudioEntity::PlaySoundAndPopulateScriptSoundSlot(s32 soundId, const u32 soundSetNameHash, const u32 soundNameHash, audSoundInitParams* initParams, 
																	bool bOverNetwork, int nNetworkRange, naEnvironmentGroup* environmentGroup, const CEntity* pEntity, const bool ownsEnvGroup ,bool enableOnReplay OUTPUT_ONLY(, const char* soundSetName, const char* soundName))
{
	audSoundSet soundSet;
	audMetadataRef soundRef = g_NullSoundRef;

	if(soundSetNameHash)
	{
		if(soundSet.Init(soundSetNameHash))
		{
#if __BANK
			if(!soundSetName)
			{
				soundSetName = SOUNDFACTORY.GetMetadataManager().GetObjectName(soundSetNameHash);
			}
#endif

			soundRef = soundSet.Find(soundNameHash);

			if(soundRef == g_NullSoundRef)
			{
				Displayf("[SFX] Script triggered sound request failed to find valid sound in soundset (Field %s(%u) - SoundSet %s(%u))", soundName, soundNameHash, soundSetName, soundSetNameHash);
				return NULL;
			}
		}		
		else
		{
			Displayf("[SFX] Script triggered sound request failed to find soundset (Field %s(%u) - SoundSet %s(%u))", soundName, soundNameHash, soundSetName, soundSetNameHash);
			return NULL;
		}
	}
	else
	{
		soundRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(soundNameHash);

		if(soundRef == g_NullSoundRef)
		{
			Displayf("[SFX] Script triggered sound request failed to find valid sound (Sound %s(%u))", soundName, soundNameHash);
			return NULL;
		}
		else
		{
#if __BANK
			if(!soundName)
			{
				soundName = SOUNDFACTORY.GetMetadataManager().GetObjectName(soundNameHash);
			}
#endif
		}
	}

	BANK_ONLY(audDisplayf("PlaySoundAndPopulateScriptSoundSlot soundId[%d] soundSetName[%s] soundSetNameHash[%u] soundName[%s] soundNameHash[%u] bOverNetwork[%d] nNetworkRange[%d]",soundId, soundSetName, soundSetNameHash, soundName, soundNameHash, bOverNetwork, nNetworkRange);)

	naAssertf(initParams, "initParams must be a valid pointer");
	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return NULL;
	}

	// If we're not passed in a soundId, we need to rustle one up
	if (soundId == -1)
	{
		s32 freeSoundId = FindFreeSoundId();
		if (freeSoundId == -1)
		{
			BANK_ONLY(naErrorf("Tried to play sound but no free sound id (Sound %s(%u) - SoundSet %s(%u))", soundName, soundNameHash, soundSetName, soundSetNameHash);)
			return NULL;
		}
		// Set up the sound slot
		m_ScriptSounds[freeSoundId].ScriptIndex = scriptIndex;
		m_ScriptSounds[freeSoundId].ScriptHasReference = false;
		// From here on, we do the same as if we're passing a soundId in, so set it to our free one.
		soundId = freeSoundId;
	}

	// To avoid cache misses, we're tracking which environmentGroups to update via a bitset
	if(ownsEnvGroup)
	{
		m_ScriptOwnedEnvGroup.Set(soundId);
	}
	else
	{
		m_ScriptOwnedEnvGroup.Clear(soundId);
	}
	m_ScriptSounds[soundId].EnvironmentGroup = environmentGroup;
	m_ScriptSounds[soundId].Entity = const_cast<CEntity*>(pEntity);

	// If we're currently playing something out of this slot, stop it.
	if (m_ScriptSounds[soundId].Sound)
	{
		// Stop the sound over the network.
		if(m_ScriptSounds[soundId].OverNetwork && NetworkInterface::IsGameInProgress())
		{
			CStopSoundEvent::Trigger(static_cast<u8>(soundId));
		}

		m_ScriptSounds[soundId].Sound->StopAndForget();
#if GTA_REPLAY
		m_ScriptSounds[soundId].SoundNameHash = g_NullSoundHash;
		m_ScriptSounds[soundId].SoundSetHash = g_NullSoundHash;
		m_ScriptSounds[soundId].ShouldRecord = enableOnReplay;
		m_ScriptSounds[soundId].ClearReplayVariableNameHashs();
#endif
	}

	m_ScriptSounds[soundId].InitParams = *initParams;
	m_ScriptSounds[soundId].SoundRef = soundRef;
    m_ScriptSounds[soundId].OverNetwork = bOverNetwork;
	
#if __BANK	
	if(soundSetName)
	{
		formatf(m_ScriptSounds[soundId].SoundSetTriggeredName, soundSetName);
	}
	else
	{
		formatf(m_ScriptSounds[soundId].SoundSetTriggeredName, "%u", soundSetNameHash);
	}

	if(soundName)
	{
		formatf(m_ScriptSounds[soundId].SoundTriggeredName, soundName);
	}
	else
	{
		formatf(m_ScriptSounds[soundId].SoundTriggeredName, "%u", soundNameHash);
	}
#endif 	

#if GTA_REPLAY
	m_ScriptSounds[soundId].SoundNameHash = soundNameHash;
	m_ScriptSounds[soundId].SoundSetHash = soundSetNameHash;
	m_ScriptSounds[soundId].ShouldRecord = enableOnReplay;
#endif

	// Play the sound over the network.
	if(m_ScriptSounds[soundId].OverNetwork && NetworkInterface::IsGameInProgress())
	{
        m_ScriptSounds[soundId].NetworkId = (NetworkInterface::GetLocalPhysicalPlayerIndex() << 16) | soundId;

		CGameScriptId& scriptId = static_cast<CGameScriptId&>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptId());
		if(pEntity != NULL)
		{
			CPlaySoundEvent::Trigger(pEntity, soundSetNameHash,soundNameHash, static_cast<u8>(soundId), scriptId, nNetworkRange);
		}
		else
		{
			CPlaySoundEvent::Trigger(initParams->Position, soundSetNameHash, soundNameHash, static_cast<u8>(soundId), scriptId, nNetworkRange);
		}
	}
	else
	{
		m_ScriptSounds[soundId].NetworkId = audScriptSound::INVALID_NETWORK_ID;
	}

	if(pEntity && pEntity->GetAudioEntity())
	{
		if(pEntity->GetIsTypeVehicle())
		{
			// Allocate a variable block in-case this sound wants to control fake revs/throttle via parameters
			((audVehicleAudioEntity*)pEntity->GetAudioEntity())->AllocateVehicleVariableBlock();
		}

		pEntity->GetAudioEntity()->CreateAndPlaySound_Persistent(m_ScriptSounds[soundId].SoundRef, &m_ScriptSounds[soundId].Sound, initParams);
	}
	else
	{
		CreateAndPlaySound_Persistent(m_ScriptSounds[soundId].SoundRef, &m_ScriptSounds[soundId].Sound, initParams);
	}
	
#if __BANK
	if (g_DebugPlayingScriptSounds)
	{
		if(m_ScriptSounds[soundId].Sound)
		{
			Displayf("Successfully played script sound %s", SOUNDFACTORY.GetMetadataManager().GetObjectName(soundNameHash));
		}
		else
		{
			Displayf("Failed to play script sound %s", SOUNDFACTORY.GetMetadataManager().GetObjectName(soundNameHash));
		}
	}
#endif
	return m_ScriptSounds[soundId].Sound;
}

bool audScriptAudioEntity::PlaySoundAndPopulateScriptSoundSlotFromNetwork(u32 setNameHash, u32 soundNameHash, audSoundInitParams* initParams, u32 nNetworkId, 
																		  CGameScriptId& scriptId, naEnvironmentGroup* environmentGroup, CEntity* entity, const bool ownsEnvGroup)
{
	networkAudioDebugf3("PlaySoundAndPopulateScriptSoundSlotFromNetwork nNetworkId[%u]",nNetworkId);

	audSoundSet soundSet;
	audMetadataRef soundRef = g_NullSoundRef;

	OUTPUT_ONLY(const char* soundSetName = "";)
	OUTPUT_ONLY(const char* soundName = "";)

	if(setNameHash)
	{
		if(soundSet.Init(setNameHash))
		{
#if __BANK
			if(!soundSetName)
			{
				soundSetName = SOUNDFACTORY.GetMetadataManager().GetObjectName(setNameHash);
			}
#endif

			soundRef = soundSet.Find(soundNameHash);

			if(soundRef == g_NullSoundRef)
			{
				Displayf("[SFX] Script triggered sound request failed to find valid sound in soundset (Field: %u - Sound Set: %s (%u))", soundNameHash, soundSetName, setNameHash);
				return false;
			}
		}		
		else
		{
			Displayf("[SFX] Script triggered sound request failed to find soundset (Field: %u - Sound Set: %u)", soundNameHash, setNameHash);
			return false;
		}
	}
	else
	{
		soundRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(soundNameHash);

		if(soundRef == g_NullSoundRef)
		{
			Displayf("[SFX] Script triggered sound request failed to find valid sound (Sound: %u)", soundNameHash);
			return false;
		}
		else
		{
#if __BANK
			if(!soundName)
			{
				soundName = SOUNDFACTORY.GetMetadataManager().GetObjectName(soundNameHash);
			}
#endif
		}
	}

	// check if we have any pending bank loads on this script
	for (s32 i=0; i<AUD_USABLE_SCRIPT_BANK_SLOTS; i++)
	{
		for (s32 j=0; j<AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS; j++)
		{
			if(m_BankSlots[i].NetworkScripts[j].IsValid() && m_BankSlots[i].NetworkScripts[j] == scriptId)
			{
				if (m_BankSlots[i].State != AUD_SCRIPT_BANK_LOADED)
				{
					// bank load still pending - hold off trying this sound until the bank has loaded
					naDebugf3("RequestSound: Bank Loading - BankSlot:%d, NetSlot:%d, Set:%u, Sound:%u, BankID:%d", i, j, setNameHash, soundNameHash, m_BankSlots[i].BankId);
					networkAudioDebugf3("RequestSound: Bank Loading - BankSlot:%d, NetSlot:%d, Set:%u, Sound:%u, BankID:%d", i, j, setNameHash, soundNameHash, m_BankSlots[i].BankId);
					return false;
				}
			}
		}
	}

	s32 soundId = FindFreeSoundId();
	if (soundId == -1)
	{
		naErrorf("Tried to play sound but no free sound id (Sound: %s (%u) - Sound Set: %s (%u)", soundName, soundNameHash, soundSetName, setNameHash);
		return false;
	}

	if (m_ScriptSounds[soundId].Sound)
	{
		naErrorf("Tried to play sound but 'free' sound id %d already has a sound playing from it! (Sound: %s (%u) - Sound Set: %s (%u)", soundId, soundName, soundNameHash, soundSetName, setNameHash);
		return false;
	}

	// Set up the sound slot
	m_ScriptSounds[soundId].ScriptIndex = -1;
	m_ScriptSounds[soundId].ScriptHasReference = false;
	m_ScriptSounds[soundId].SoundRef = soundRef;
	m_ScriptSounds[soundId].OverNetwork = false;
	m_ScriptSounds[soundId].NetworkId = nNetworkId;
	m_ScriptSounds[soundId].ScriptId = scriptId;
	m_ScriptSounds[soundId].EnvironmentGroup = environmentGroup;
	m_ScriptSounds[soundId].Entity = entity;
	m_ScriptSounds[soundId].ShouldRecord = true;

#if __BANK	
	if(soundSetName)
	{
		formatf(m_ScriptSounds[soundId].SoundSetTriggeredName, soundSetName);
	}
	else
	{
		formatf(m_ScriptSounds[soundId].SoundSetTriggeredName, "%u", setNameHash);
	}

	if(soundName)
	{
		formatf(m_ScriptSounds[soundId].SoundTriggeredName, soundName);
	}
	else
	{
		formatf(m_ScriptSounds[soundId].SoundTriggeredName, "%u", soundNameHash);
	}
#endif 	

#if GTA_REPLAY
	m_ScriptSounds[soundId].InitParams = *initParams;
	m_ScriptSounds[soundId].SoundNameHash = soundNameHash;
	m_ScriptSounds[soundId].SoundSetHash = setNameHash;
	Displayf("Recording sound hash %u and set %u over network", soundNameHash, setNameHash);
#endif
	// To avoid cache misses, we track which environmentGroups need to be updated via a bitset
	if(ownsEnvGroup)
	{
		m_ScriptOwnedEnvGroup.Set(soundId);
	}
	else
	{
		m_ScriptOwnedEnvGroup.Clear(soundId);
	}

	if(entity && entity->GetAudioEntity())
	{
		if (entity->GetIsTypeVehicle())
		{
			// Allocate a variable block in-case this sound wants to control fake revs/throttle via parameters
			((audVehicleAudioEntity*)entity->GetAudioEntity())->AllocateVehicleVariableBlock();
		}

		entity->GetAudioEntity()->CreateAndPlaySound_Persistent(m_ScriptSounds[soundId].SoundRef, &m_ScriptSounds[soundId].Sound, initParams);
	}
	else
	{
		CreateAndPlaySound_Persistent(m_ScriptSounds[soundId].SoundRef, &m_ScriptSounds[soundId].Sound, initParams);
	}

	if (g_DebugPlayingScriptSounds)
	{
		if(m_ScriptSounds[soundId].Sound)
		{
			Displayf("Successfully played script sound from network (Sound: %s (%u) - Sound Set: %s (%u)", soundName, soundNameHash, soundSetName, setNameHash);
		}
		else
		{
			Displayf("Failed to play script sound from network (Sound: %s (%u) - Sound Set: %s (%u)", soundName, soundNameHash, soundSetName, setNameHash);
		}
	}

	// doesn't matter if the sound played or not, that we tried is enough
	naDebugf3("RequestSound: Played - SoundID:%d, NetID:%d, Set:%u Sound:%u", soundId, nNetworkId, setNameHash, soundNameHash);
	networkAudioDebugf3("RequestSound: Played - SoundID:%d, NetID:%d, Set:%u Sound:%u", soundId, nNetworkId, setNameHash, soundNameHash);
	return true;
}

void audScriptAudioEntity::SetAmbientZoneNonPersistentState(u32 ambientZone, bool enabled, bool forceUpdate)
{
	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	s32 currentIndex = m_Scripts[scriptIndex].m_ScriptAmbientZoneChanges.Find(ambientZone);

	if(currentIndex >= 0)
	{
		TristateValue currentSetting = g_AmbientAudioEntity.GetZoneNonPersistentStatus(ambientZone);

		if((currentSetting == AUD_TRISTATE_TRUE && enabled) || 
			(currentSetting == AUD_TRISTATE_FALSE && !enabled))
		{
			// We've already set this zone to the desired value, nothing to do
			return;
		}

		// We can only set the status on zones with no status set, so we must clear the flag before resetting it
		g_AmbientAudioEntity.ClearZoneNonPersistentStatus(ambientZone, forceUpdate);
		m_Scripts[scriptIndex].m_ScriptAmbientZoneChanges.Delete(currentIndex);
	}

	if(g_AmbientAudioEntity.SetZoneNonPersistentStatus(ambientZone, enabled, forceUpdate))
	{
		m_Scripts[scriptIndex].m_ScriptAmbientZoneChanges.PushAndGrow(ambientZone);
	}
}

void audScriptAudioEntity::ClearAmbientZoneNonPersistentState(u32 ambientZone, bool forceUpdate)
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	naCErrorf(scriptThreadId, "Failed to get thread id");
	s32 scriptIndex = GetIndexFromScriptThreadId(scriptThreadId);

	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	s32 ambientZoneIndex = m_Scripts[scriptIndex].m_ScriptAmbientZoneChanges.Find(ambientZone);

	if(ambientZoneIndex >= 0)
	{
		if(g_AmbientAudioEntity.ClearZoneNonPersistentStatus(ambientZone, forceUpdate))
		{
			m_Scripts[scriptIndex].m_ScriptAmbientZoneChanges.Delete(ambientZoneIndex);
		}
	}
}

void audScriptAudioEntity::SetVehicleScriptPriority(audVehicleAudioEntity* vehicleEntity, audVehicleScriptPriority priorityLevel, scrThreadId scriptThreadId)
{
	s32 scriptIndex = GetIndexFromScriptThreadId(scriptThreadId);

	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	if(vehicleEntity)
	{
		if(vehicleEntity->SetScriptPriority(priorityLevel, scriptThreadId))
		{
			s32 currentIndex = m_Scripts[scriptIndex].m_PriorityVehicles.Find(vehicleEntity);

			if(currentIndex >= 0)
			{
				if(priorityLevel == AUD_VEHICLE_SCRIPT_PRIORITY_NONE)
				{
					m_Scripts[scriptIndex].m_PriorityVehicles.Delete(currentIndex);
				}
			}
			else if(priorityLevel > AUD_VEHICLE_SCRIPT_PRIORITY_NONE)
			{
				m_Scripts[scriptIndex].m_PriorityVehicles.PushAndGrow(vehicleEntity);
			}
		}
	}
}

void audScriptAudioEntity::SetVehicleScriptPriorityForCurrentScript(audVehicleAudioEntity* vehicleEntity, audVehicleScriptPriority priorityLevel)
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	naCErrorf(scriptThreadId, "Failed to get thread id");
	SetVehicleScriptPriority(vehicleEntity, priorityLevel, scriptThreadId);
}

void audScriptAudioEntity::SetPlayerAngry(audSpeechAudioEntity* speechEntity, bool isAngry)
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
	naCErrorf(scriptThreadId, "Failed to get thread id");
	s32 scriptIndex = GetIndexFromScriptThreadId(scriptThreadId);

	if (!naVerifyf(scriptIndex>=0 || !isAngry, "No valid scriptIndex: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	if(speechEntity)
	{
		speechEntity->SetPedIsAngry(isAngry);

		if(scriptIndex >= 0)
		{
			int index = m_Scripts[scriptIndex].m_PedsSetToAngry.Find(speechEntity);
			if(isAngry)
			{
				if(index == -1)
				{
					m_Scripts[scriptIndex].m_PedsSetToAngry.PushAndGrow(speechEntity);
				}
			}
			else
			{
				if(index != -1)
				{
					m_Scripts[scriptIndex].m_PedsSetToAngry.Delete(index);
				}
			}
		}
		else if(!isAngry)
		{
			for(int i=0; i<AUD_MAX_SCRIPTS; i++)
			{
				int index = m_Scripts[i].m_PedsSetToAngry.Find(speechEntity);
				if(index != -1)
				{
					m_Scripts[i].m_PedsSetToAngry.Delete(index);
				}
			}
		}
	}
}

void audScriptAudioEntity::SetPlayerAngryShortTime(audSpeechAudioEntity* speechEntity, u32 timeInMs)
{
	if(speechEntity)
		speechEntity->SetPedIsAngryShortTime(timeInMs);
}

void audScriptAudioEntity::PlaySoundFromEntity(s32 soundId, const char* soundName, const CEntity* entity, const char * setName, bool bOverNetwork, int nNetworkRange)
{
	PlaySoundFromEntity(soundId, soundName ? atStringHash(soundName) : g_NullSoundHash, entity, setName ? atStringHash(setName) : 0, bOverNetwork, nNetworkRange OUTPUT_ONLY(, setName, soundName));
}

void audScriptAudioEntity::PlaySoundFromEntity(s32 soundId, const u32 soundNameHash, const CEntity* entity, const u32 setNameHash, bool bOverNetwork, int nNetworkRange OUTPUT_ONLY(, const char* soundSetName, const char* soundName))
{
	// Grab an existing environmentGroup if we already have one (ped or vehicle) otherwise create a new one.
	bool ownsEnvGroup = false;
	naEnvironmentGroup* environmentGroup = (naEnvironmentGroup*)entity->GetAudioEnvironmentGroup();
	if(!environmentGroup && entity)
	{
		environmentGroup = CreateScriptEnvironmentGroup(entity, entity->GetTransform().GetPosition());
		ownsEnvGroup = true;
	}

	PlaySoundFromEntity(soundId, soundNameHash, entity, environmentGroup, setNameHash, bOverNetwork, nNetworkRange, ownsEnvGroup OUTPUT_ONLY(, soundSetName, soundName));

}


void audScriptAudioEntity::PlaySoundFromEntity(s32 soundId, const u32 soundNameHash, const CEntity* entity, naEnvironmentGroup* environmentGroup, 
												const u32 setNameHash, bool bOverNetwork, int nNetworkRange, const bool ownsEnvGroup OUTPUT_ONLY(, const char* soundSetName, const char* soundName))
{
	BANK_ONLY(networkAudioDebugf3("PlaySoundFromEntity soundId[%d] soundName[%s] entity[%p][%s] setName[%s] bOverNetwork[%d] nNetworkRange[%d] ownsEnvGroup[%d]",soundId,SOUNDFACTORY.GetMetadataManager().GetObjectName(soundNameHash),entity,entity ? entity->GetModelName() : "",SOUNDFACTORY.GetMetadataManager().GetObjectName(setNameHash),bOverNetwork,nNetworkRange,ownsEnvGroup);)

	naAssertf(entity, "entity must be a valid pointer");

	//if( entity->GetIsTypeObject() )
	//{
	//	if(!((CObject*)entity)->GetObjectAudioEntity()->HasRegisteredWithController())
	//	{
	//		((CObject*)entity)->GetObjectAudioEntity()->Init((CObject*)entity);
	//	}
	//}
	//naAssertf(((CObject*)entity)->GetObjectAudioEntity()->HasRegisteredWithController(), "trying to play a sound from an object that hasn't got a valid audio entity");

	audSound* sound = NULL;
	audSoundInitParams initParams;
	Vec3V position = entity->GetTransform().GetPosition();

	initParams.Tracker = entity->GetPlaceableTracker();
	initParams.EnvironmentGroup = environmentGroup;
	initParams.Position = VEC3V_TO_VECTOR3(position);
	
	sound = PlaySoundAndPopulateScriptSoundSlot(soundId, setNameHash, soundNameHash, &initParams, bOverNetwork, nNetworkRange, environmentGroup, entity, ownsEnvGroup, true OUTPUT_ONLY(, soundSetName, soundName));
	
#if GTA_REPLAY
	if(sound)
	{
		RecordScriptSlotSound(setNameHash, soundNameHash, &initParams, entity, sound);
	}
#endif

#if __BANK
	if(sound)
	{
		Displayf("[SFX] PLAY_SOUND_FROM_ENTITY - Sound: %s(%u) - Set:%s(%u): Played successfully (%s)", soundName, soundNameHash, soundSetName, setNameHash, CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		Displayf("[SFX] PLAY_SOUND_FROM_ENTITY - Sound: %s(%u) - Set:%s(%u): Failed to play (%s)", soundName, soundNameHash, soundSetName, setNameHash, CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
#endif
}

void audScriptAudioEntity::PlaySoundFromPosition(s32 soundId, const char* soundName, const Vector3* position, const char * setName, bool bOverNetwork, int nNetworkRange, bool isExteriorLoc)
{
	networkAudioDebugf3("PlaySoundFromPosition soundId[%d] soundName[%s] setName[%s] bOverNetwork[%d] nNetworkRange[%d] isExteriorLoc[%d]",soundId,soundName,setName,bOverNetwork,nNetworkRange,isExteriorLoc);

	// If it's an exterior location, then we can use an environmentGroup since we have accurate interior information
	naEnvironmentGroup* environmentGroup = NULL;
	bool ownsEnvGroup = false;
	if(isExteriorLoc)
	{
		environmentGroup = CreateScriptEnvironmentGroup(NULL, VECTOR3_TO_VEC3V(*position));
		ownsEnvGroup = true;
	}

	audSound* sound = NULL;
	audSoundInitParams initParams;
	initParams.Position = *position;
	initParams.EnvironmentGroup = environmentGroup;
	sound = PlaySoundAndPopulateScriptSoundSlot(soundId, setName, soundName, &initParams, bOverNetwork, nNetworkRange, environmentGroup, NULL, ownsEnvGroup, true);	

#if GTA_REPLAY
	if(sound && CReplayMgr::IsRecording())
	{
		if(soundId != -1)
		{
			if(setName)
			{
				m_ScriptSounds[soundId].SoundSetHash = atStringHash(setName);
			}
			else
			{
				m_ScriptSounds[soundId].SoundSetHash = g_NullSoundHash;
			}
			m_ScriptSounds[soundId].SoundNameHash = atStringHash(soundName);
			m_ScriptSounds[soundId].InitParams = initParams;
		}
		RecordScriptSlotSound(setName, soundName, &initParams, NULL, sound);
	}
#endif

#if __BANK
	if(sound)
	{
		Displayf("[SFX] PLAY_SOUND_FROM_COORD - Sound:%s - Set:%s: Played successfully (%s)", soundName, setName? setName : "null", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		Displayf("[SFX] PLAY_SOUND_FROM_COORD - Sound:%s - Set:%s: Failed to play (%s)", soundName, setName? setName : "null", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
#endif
}

void audScriptAudioEntity::UpdateSoundPosition(s32 soundId, const Vector3* position)
{
	if (soundId<0)
	{
		return;
	}

	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	// Make sure it's our sound ref to stop
	naCErrorf(m_ScriptSounds[soundId].ScriptIndex == scriptIndex, "%s:Level design: Check you're calling UPDATE_SOUND_POSITION(soundId, vector) on the correct SoundID", CTheScripts::GetCurrentScriptName());

	if (m_ScriptSounds[soundId].Sound && m_ScriptSounds[soundId].ScriptIndex == scriptIndex)
	{
		m_ScriptSounds[soundId].Sound->SetRequestedPosition(VECTOR3_TO_VEC3V(*position));
	}
}

void audScriptAudioEntity::PlayFireSoundFromPosition(s32 soundId, const Vector3* position)
{
	audSoundInitParams initParams;
	initParams.Position = *position;
	initParams.StartOffset = g_NextFireStartOffset;
	initParams.IsStartOffsetPercentage = true;
	g_NextFireStartOffset = (g_NextFireStartOffset+70)%100;

	audSound* sound = PlaySoundAndPopulateScriptSoundSlot(soundId, "", "SCRIPT_FIRE", &initParams, false, 0, NULL, NULL, false, true);

	if(sound)
	{
		REPLAY_ONLY(RecordScriptSlotSound(NULL, "SCRIPT_FIRE", &initParams, NULL, sound);)
	}
}

void audScriptAudioEntity::PlaySound(s32 soundId, const char* soundName, const char * setName, bool bOverNetwork, int nNetworkRange,bool  REPLAY_ONLY(enableOnReplay))
{
	audSound* sound = NULL;
	audSoundInitParams initParams;
	sound = PlaySoundAndPopulateScriptSoundSlot(soundId, setName, soundName, &initParams, bOverNetwork, nNetworkRange, NULL, NULL, false, enableOnReplay);

#if GTA_REPLAY
	if(sound && enableOnReplay)
	{
		if(soundId != -1)
		{
			if(setName)
			{
				m_ScriptSounds[soundId].SoundSetHash = atStringHash(setName);
			}
			else
			{
				m_ScriptSounds[soundId].SoundSetHash = g_NullSoundHash;
			}
			m_ScriptSounds[soundId].SoundNameHash = atStringHash(soundName);
			m_ScriptSounds[soundId].InitParams = initParams;
		}
		RecordScriptSlotSound(setName, soundName, &initParams, NULL, sound);
	}
#endif

#if __BANK
	if(sound)
	{
		Displayf("[SFX] PLAY_SOUND - Sound:%s - Set:%s: Played successfully (%s)", soundName, setName? setName : "null", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		Displayf("[SFX] PLAY_SOUND - Sound:%s - Set:%s: Failed to play (%s)", soundName, setName? setName : "null", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
#endif
}

void audScriptAudioEntity::PlaySoundFrontend(s32 soundId, const char* soundName, const char * setName, bool REPLAY_ONLY(enableOnReplay))
{
	audSound* sound = NULL;
	audSoundInitParams initParams;
	initParams.Pan = 0;
	if(setName)
	{		
		const u32 soundSetNameHash = atStringHash(setName);
		if( soundSetNameHash == ATSTRINGHASH("MissionFailedSounds", 0x5A23F3D5) ||
			soundSetNameHash == ATSTRINGHASH("BustedSounds", 0xDF84A53C) ||
			soundSetNameHash == ATSTRINGHASH("WastedSounds", 0xFD4C28))
		{
			initParams.TimerId = 1;
			// don't record sounds played from these sets.
			REPLAY_ONLY(enableOnReplay = false;)
		}
			
		if(	soundSetNameHash == ATSTRINGHASH("HUD_FRONTEND_CUSTOM_SOUNDSET", 0x832CAA0F) ||
			soundSetNameHash == ATSTRINGHASH("HUD_FRONTEND_DEFAULT_SOUNDSET", 0x17BD10F1) ||
			soundSetNameHash == ATSTRINGHASH("MP_LOBBY_SOUNDS", 0xFA4A5AA0) ||
			soundSetNameHash == ATSTRINGHASH("HUD_FRONTEND_MP_COLLECTABLE_SOUNDS", 0x2B8F97E3) ||
			soundSetNameHash == ATSTRINGHASH("HUD_FRONTEND_STANDARD_PICKUPS_NPC_SOUNDSET", 0x1D46A6A2) ||
			soundSetNameHash == ATSTRINGHASH("HUD_FRONTEND_VEHICLE_PICKUPS_NPC_SOUNDSET", 0xF5E3A26A) ||
			soundSetNameHash == ATSTRINGHASH("HUD_FRONTEND_WEAPONS_PICKUPS_NPC_SOUNDSET", 0xF35C567B) ||
			soundSetNameHash == ATSTRINGHASH("RESPAWN_ONLINE_SOUNDSET", 0x71F56AB4) ||
			soundSetNameHash == ATSTRINGHASH("MP_MISSION_COUNTDOWN_SOUNDSET", 0xC55C68A0) ||
			soundSetNameHash == ATSTRINGHASH("HUD_FREEMODE_SOUNDSET", 0x54C522AD) ||
			soundSetNameHash == ATSTRINGHASH("HUD_MINI_GAME_SOUNDSET", 0xD382DF7C) ||
			soundSetNameHash == ATSTRINGHASH("HUD_AWARDS", 0x2A508F9C) ||
			soundSetNameHash == ATSTRINGHASH("CELEBRATION_SOUNDSET", 0xE8F24AFD) ||
			soundSetNameHash == ATSTRINGHASH("LONG_PLAYER_SWITCH_SOUNDS", 0x8DDBFC96) ||
			soundSetNameHash == ATSTRINGHASH("PHONE_SOUNDSET_DEFAULT", 0x28C8633) ||
			soundSetNameHash == ATSTRINGHASH("PHONE_SOUNDSET_PROLOGUE", 0x596B8EBB) ||
			soundSetNameHash == ATSTRINGHASH("Phone_SoundSet_Franklin", 0x8A73028A) ||
			soundSetNameHash == ATSTRINGHASH("Phone_SoundSet_Michael", 0x578FE4D7) ||
			soundSetNameHash == ATSTRINGHASH("Phone_SoundSet_Trevor", 0xE52306DE) ||
			soundSetNameHash == ATSTRINGHASH("Phone_SoundSet_Glasses_Cam", 0x10109BEB))
		{
			// don't record sounds played from these sets.
			REPLAY_ONLY(enableOnReplay = false;)
		}
	}

	sound = PlaySoundAndPopulateScriptSoundSlot(soundId, setName, soundName, &initParams, false, 0, NULL, NULL, false, enableOnReplay);

#if GTA_REPLAY
	if(sound && enableOnReplay)
	{
		if(soundId != -1)
		{
			if(setName)
			{
				m_ScriptSounds[soundId].SoundSetHash = atStringHash(setName);
			}
			else
			{
				m_ScriptSounds[soundId].SoundSetHash = g_NullSoundHash;
			}
			m_ScriptSounds[soundId].SoundNameHash = atStringHash(soundName);
			m_ScriptSounds[soundId].InitParams = initParams;
		}
		RecordScriptSlotSound(setName, soundName, &initParams, NULL, sound);
	}
#endif

#if __BANK
	if(sound)
	{
		Displayf("[SFX] PLAY_SOUND_FRONTEND - Sound:%s - Set:%s: Played successfully (%s)", soundName, setName? setName : "null", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
	else
	{
		Displayf("[SFX] PLAY_SOUND_FRONTEND - Sound:%s - Set:%s: Failed to play (%s)", soundName, setName? setName : "null", CTheScripts::GetCurrentScriptNameAndProgramCounter());
	}
#endif
}

void audScriptAudioEntity::StopSound(s32 soundId)
{
	//Assertf(soundId >=0, "Level design: you're calling StopSound with soundId -1" );
	if (soundId<0)
	{
		return;
	}

    // Stop sounds started via the network from network function
    if (m_ScriptSounds[soundId].NetworkId != audScriptSound::INVALID_NETWORK_ID && !m_ScriptSounds[soundId].OverNetwork)
    {
        StopSoundFromNetwork(m_ScriptSounds[soundId].NetworkId);
        return;
    }

	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	// Make sure it's our sound ref to stop
	naCErrorf(m_ScriptSounds[soundId].ScriptIndex == scriptIndex, "%s:Level design: Check you're calling STOP_SOUND() on the correct SoundID", CTheScripts::GetCurrentScriptName());

	if (m_ScriptSounds[soundId].Sound && m_ScriptSounds[soundId].ScriptIndex == scriptIndex)
	{
		Displayf("[SFX] STOP_SOUND - Sound:%s - Id %u ScriptId: %u OverNetwork: %s", m_ScriptSounds[soundId].Sound->GetName(), soundId, m_ScriptSounds[soundId].ScriptIndex,m_ScriptSounds[soundId].OverNetwork ? "Yes" : "No");
		// Stop the sound over the network.
		if(m_ScriptSounds[soundId].OverNetwork && NetworkInterface::IsGameInProgress())
		{
			CStopSoundEvent::Trigger(static_cast<u8>(soundId));
		}

		m_ScriptSounds[soundId].Sound->StopAndForget();
		m_ScriptSounds[soundId].EnvironmentGroup = NULL;
		m_ScriptSounds[soundId].Entity = NULL;
		m_ScriptOwnedEnvGroup.Clear(soundId);

#if GTA_REPLAY
		m_ScriptSounds[soundId].SoundNameHash = g_NullSoundHash;
		m_ScriptSounds[soundId].SoundSetHash = g_NullSoundHash;
		m_ScriptSounds[soundId].ClearReplayVariableNameHashs();
#endif
	}
}

bool audScriptAudioEntity::PlaySoundFromNetwork(u32 nNetworkId, CGameScriptId& scriptId, CEntity* entity, u32 setNameHash, u32 soundNameHash)
{
	networkAudioDisplayf("PlaySoundFromNetwork nNetworkId[%u] entity[%p][%s] sound %u (set %u)",nNetworkId,entity,entity ? entity->GetModelName() : "", soundNameHash, setNameHash);

	if(naVerifyf(entity, "CEntity* must be a valid"))
	{
		audSoundInitParams initParams;
		initParams.Tracker = entity->GetPlaceableTracker();
		bool ownsEnvGroup = false;
		naEnvironmentGroup* environmentGroup = (naEnvironmentGroup*)entity->GetAudioEnvironmentGroup();
		if(!environmentGroup)
		{
			environmentGroup = CreateScriptEnvironmentGroup(entity, entity->GetTransform().GetPosition());
			ownsEnvGroup = true;
		}
		initParams.Position = VEC3V_TO_VECTOR3(entity->GetTransform().GetPosition());
		return PlaySoundAndPopulateScriptSoundSlotFromNetwork(setNameHash, soundNameHash, &initParams, nNetworkId, scriptId, environmentGroup, entity, ownsEnvGroup);
	}
	return false;
}

bool audScriptAudioEntity::PlaySoundFromNetwork(u32 nNetworkId, CGameScriptId& scriptId, const Vector3* position, u32 setNameHash, u32 soundNameHash)
{
	networkAudioDisplayf("PlaySoundFromNetwork nNetworkId[%u] sound %u (set %u)",nNetworkId, setNameHash, soundNameHash);

	audSoundInitParams initParams;
	initParams.Position = *position;
	return PlaySoundAndPopulateScriptSoundSlotFromNetwork(setNameHash, soundNameHash, &initParams, nNetworkId, scriptId, NULL, NULL, false);
}

void audScriptAudioEntity::StopSoundFromNetwork(u32 nNetworkId)
{
	networkAudioDisplayf("StopSoundFromNetwork nNetworkId[%u]",nNetworkId);

	if (nNetworkId == audScriptSound::INVALID_NETWORK_ID)
		return;

	for (s32 i=0; i<AUD_MAX_SCRIPT_SOUNDS; i++)
	{
		if (m_ScriptSounds[i].NetworkId == nNetworkId)
		{
			networkAudioDisplayf("StopSound: SoundID:%d, NetID:%d sound %u (set %u)", i, nNetworkId, m_ScriptSounds[i].SoundNameHash, m_ScriptSounds[i].SoundSetHash);
			if (m_ScriptSounds[i].Sound)
			{
				m_ScriptSounds[i].Sound->StopAndForget();
			}
			ResetScriptSound(i);
		}
	}
}

u32 audScriptAudioEntity::GetNetworkIdFromSoundId(s32 soundId)
{
    // bounds check
    if(!naVerifyf(soundId >= 0 && soundId < AUD_MAX_SCRIPT_SOUNDS, "GetNetworkIdFromSoundId - Invalid soundId: %d", soundId))
        return audScriptSound::INVALID_NETWORK_ID;

	return m_ScriptSounds[soundId].NetworkId;
}

s32 audScriptAudioEntity::GetSoundIdFromNetworkId(u32 nNetworkId)
{
    if (nNetworkId == audScriptSound::INVALID_NETWORK_ID)
    {
        return -1;
    }

	for (s32 i=0; i<AUD_MAX_SCRIPT_SOUNDS; i++)
	{
		if (m_ScriptSounds[i].NetworkId == nNetworkId)
		{
			return i;
		}
	}

	return -1;
}

void audScriptAudioEntity::SetVariableOnSound(s32 soundId, const char* variableName, f32 variableValue)
{
//	Assertf(soundId >=0, "Level design: you're calling StopSound with soundId -1" );
	if (soundId<0)
	{
		return;
	}

	s32 scriptIndex = AddScript();
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return;
	}

	// Make sure it's our sound ref to do stuff to
	naCErrorf(m_ScriptSounds[soundId].ScriptIndex == scriptIndex, "Level design: Check you're calling SET_VARIABLE on the correct SoundID");

	if (m_ScriptSounds[soundId].Sound && m_ScriptSounds[soundId].ScriptIndex == scriptIndex)
	{
		m_ScriptSounds[soundId].Sound->SetVariableValueDownHierarchyFromName(variableName, variableValue);
#if GTA_REPLAY
		u32 variableNameHash = atStringHash(variableName);
		m_ScriptSounds[soundId].StoreSoundVariable(variableNameHash, variableValue);
#endif
	}
}

void audScriptAudioEntity::SetVariableOnStream(const char * variableName, f32 variableValue)
{
	if(m_StreamSound)
	{
		m_StreamSound->SetVariableValueDownHierarchyFromName(variableName, variableValue);
#if GTA_REPLAY
		u32 variableNameHash = atStringHash(variableName);
		StoreSoundVariable(variableNameHash, variableValue);
#endif
	}
}

#if GTA_REPLAY
void audScriptAudioEntity::StoreSoundVariable(u32 variableNameHash, f32 value)
{
	for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
	{
		if(m_StreamVariableNameHash[i] == variableNameHash)
		{
			m_StreamVariableValue[i] = value;
			return;
		}
	}
	for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
	{
		if(m_StreamVariableNameHash[i] == g_NullSoundHash)
		{
			m_StreamVariableNameHash[i] = variableNameHash;
			m_StreamVariableValue[i] = value;
			return;
		}
	}
}
#endif

bool audScriptAudioEntity::HasSoundFinished(s32 soundId)
{
//	Assertf(soundId >=0, "Level design: you're calling StopSound with soundId -1" );
	if (soundId<0)
	{
		return true;
	}

	s32 scriptIndex = AddScript();
	
	if (!naVerifyf(scriptIndex>=0, "No valid scriptIndex, even with automated registering: %s; %d", CTheScripts::GetCurrentScriptName(), scriptIndex))
	{
		return true;
	}

    bool bCanStop = false;

    // can stop local sounds with valid script index
    bCanStop |= (m_ScriptSounds[soundId].ScriptIndex == scriptIndex);

    // can stop network sounds not started by the local player
    bCanStop |= (m_ScriptSounds[soundId].NetworkId != audScriptSound::INVALID_NETWORK_ID && !m_ScriptSounds[soundId].OverNetwork);

	if (!naVerifyf(bCanStop, "Level design: Check you're calling HAS_SOUND_FINISHED() on the correct SoundID. This script is %s (%d) but SoundID %d is owned by script %s (%d). Network ID = %d, OverNetwork = %s", GetScriptName(scriptIndex), scriptIndex, soundId, GetScriptName(m_ScriptSounds[soundId].ScriptIndex), m_ScriptSounds[soundId].ScriptIndex, m_ScriptSounds[soundId].NetworkId, m_ScriptSounds[soundId].OverNetwork ? "true" : "false"))
	{
		return true;
	}

	// Return that it's finished if we no longer have a ptr to it, or it's in the waiting to be deleted state
	if (!m_ScriptSounds[soundId].Sound)
	{
		return true;
	}
	if (m_ScriptSounds[soundId].Sound->GetPlayState() == AUD_SOUND_WAITING_TO_BE_DELETED)
	{
		return true;
	}
	// Else, we have a sound, and it's not done yet
	return false;
}  

void audScriptAudioEntity::ResetScriptSound(const s32 soundId)
{
	m_ScriptSounds[soundId].Reset();
	m_ScriptOwnedEnvGroup.Clear(soundId);
}

audScript * audScriptAudioEntity::GetScript(s32 index)
{
	if(index >= 0)
	{
		if(audVerifyf(index < m_Scripts.GetMaxCount(), "Invalid script index passed to audScriptAudioEntity::GetScript! (%d >= %d)", index, m_Scripts.GetMaxCount()))
		{
			return &m_Scripts[index];
		}
	}

	return NULL;
}

bool audScriptAudioEntity::StartScene(u32 sceneHash)
{
	s32 index = AddScript();
	if (!naVerifyf(index >= 0, "No valid script, even with automated registering: %s", CTheScripts::GetCurrentScriptName()))
	{
		return false;
	}

	audScript * script = GetScript(index);
	
	return script->StartScene(sceneHash);
}

void audScriptAudioEntity::StopScene(u32 sceneHash)
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(); 

	naAssertf(scriptThreadId, "Failed to get thread id");

	s32 index = GetIndexFromScriptThreadId(scriptThreadId);

	if(index < 0)
	{
		return;
	}

	audScript* script = GetScript(index);
	if(script)
	{
		script->StopScene(sceneHash);
	}
}

void audScriptAudioEntity::StopScenes()
{
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId(); 

	naAssertf(scriptThreadId, "Failed to get thread id");

	s32 index = GetIndexFromScriptThreadId(scriptThreadId);

	if(index < 0)
	{
		return;
	}

	audScript* script = GetScript(index);
	if(script)
	{
		script->StopScenes();
	}
}

void audScriptAudioEntity::SetSceneVariable(u32 sceneHash, u32 variableHash, f32 value)
{
	audScript* script = GetScript(AddScript());
	if(script)
	{
		script->SetSceneVariable(sceneHash, variableHash, value);
	}
}

bool audScriptAudioEntity::SceneHasFinishedFading(u32 sceneHash)
{
	audScript* script = GetScript(AddScript());
	if(script)
	{
		return script->SceneHasFinishedFade(sceneHash);
	}

	return true;
}

void audScriptAudioEntity::SetSceneVariable(const char * sceneName, const char * variableName, f32 value)
{
	SetSceneVariable(atStringHash(sceneName), atStringHash(variableName), value);
}

void audScriptAudioEntity::SetScriptVariable(const char * variableName, f32 value)
{
	audScript* script = GetScript(AddScript());

	if(script)
	{
		script->SetVariableValue(atStringHash(variableName), value);
	}
}

bool audScriptAudioEntity::IsSceneActive(const u32 sceneHash)
{
	audScript *script = GetScript(AddScript());

	if(script)
	{
		if((script->GetScene(sceneHash)) != NULL)
		{
			return true;
		}
	}

	return DYNAMICMIXER.GetActiveSceneFromHash(sceneHash) != NULL;
}

#if NA_POLICESCANNER_ENABLED
void audScriptAudioEntity::SetPoliceScannerAudioScene(const char* sceneName) 
{
	if(!Verifyf(m_PoliceScannerSceneHash == 0 && m_PoliceScannerSceneScriptIndex == -1, "Trying to set police scanner audio scene when on has already been set."))
		return;

	u32 sceneHash = atStringHash(sceneName);
	if(StartScene(sceneHash))
	{
		s32 index = AddScript();
		if (!naVerifyf(index >= 0, "No valid script, even with automated registering: %s", CTheScripts::GetCurrentScriptName()))
		{
			StopScene(sceneHash);
			return;
		}

		audScript* script = GetScript(index);
		if(Verifyf(script, "Failed to grab script pointer in spite of a valid index."))
		{
			audScene* scene = script->GetScene(sceneHash);
			if(Verifyf(scene, "Failed to grab audio scene despite successful creation.  Something is very wrong."))
			{
				scene->SetVariableValue("apply", 0.0f);
				m_PoliceScannerSceneHash = sceneHash;
				m_PoliceScannerSceneScriptIndex = index;
			}
			else
			{
				StopScene(sceneHash);
			}
		}
		else
		{
			StopScene(sceneHash);
		}
	}
}

void audScriptAudioEntity::StopAndRemovePoliceScannerAudioScene() 
{
	StopScene(m_PoliceScannerSceneHash);
	ResetPoliceScannerVariables();
}

void audScriptAudioEntity::ResetPoliceScannerVariables()
{
	m_PoliceScannerSceneHash = 0;
	m_PoliceScannerSceneScriptIndex = -1;
}

void audScriptAudioEntity::ApplyPoliceScannerAudioScene()
{
	if(m_PoliceScannerSceneHash == 0 || m_PoliceScannerSceneScriptIndex == -1 || m_PoliceScannerIsApplied)
		return;

	audScript* script = GetScript(m_PoliceScannerSceneScriptIndex);
	if(Verifyf(script, "Failed to grab script pointer in spite of a valid index."))
	{
		audScene* scene = script->GetScene(m_PoliceScannerSceneHash);
		if(Verifyf(scene, "Failed to grab audio scene despite successful creation.  Something is very wrong."))
		{
			scene->SetVariableValue("apply", g_AudioScannerManager.GetPoliceScannerApplyValue());
			m_PoliceScannerIsApplied = true;
		}
	}
}

void audScriptAudioEntity::DeactivatePoliceScannerAudioScene()
{
	if(m_PoliceScannerSceneHash == 0 || m_PoliceScannerSceneScriptIndex == -1)
		return;

	audScript* script = GetScript(m_PoliceScannerSceneScriptIndex);
	if(Verifyf(script, "Failed to grab script pointer in spite of a valid index."))
	{
		audScene* scene = script->GetScene(m_PoliceScannerSceneHash);
		if(Verifyf(scene, "Failed to grab audio scene despite successful creation.  Something is very wrong."))
		{
			scene->SetVariableValue("apply", SMALL_FLOAT);
			m_PoliceScannerIsApplied = false;
		}
	}
}
#endif

// Custom code handlers
void audScriptAudioEntity::GetSpeechForEmergencyServiceCall(char* RetString)
{
	CPopZone *pZone;

	Vector3	playerCoors = CGameWorld::FindLocalPlayerCoors();
	pZone = CPopZones::FindSmallestForPosition(&playerCoors, ZONECAT_ANY, ZONETYPE_SP);
    if(pZone)
    {
	    strncpy(RetString, pZone->m_associatedTextId.TryGetCStr(), 8);
    }
    else
    {
        strcpy(RetString, "");
    }
}

void audScriptAudioEntity::EnableChaseAudio(bool enable)
{
	audScript *script = GetScript();
	
	if(naVerifyf(script, "Failed to register script for audio"))
	{
		script->SetAudioFlag(audScriptAudioFlags::BoostPlayerCarVolume, enable);
		script->SetAudioFlag(audScriptAudioFlags::AggressiveHorns, enable);
		script->SetAudioFlag(audScriptAudioFlags::CarsStartFirstTime, enable);;
	}
}

void audScriptAudioEntity::EnableAggressiveHorns(bool enable)
{
	audScript *script = GetScript();

	if(naVerifyf(script, "Failed to register script for audio"))
	{
		script->SetAudioFlag(audScriptAudioFlags::AggressiveHorns, enable);
	}
}

void audScriptAudioEntity::PlayPreviewRingtone(const u32 ringtoneId)
{
	StopPreviewRingtone();

	char soundName[64];
	formatf(soundName, sizeof(soundName)-1, "MOBILE_PHONE_RING_%02d", ringtoneId);
	audSoundInitParams initParams;
	initParams.WaveSlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("PLAYER_RINGTONE", 0x0d2645e26));
	initParams.Pan = 0;
	initParams.AllowLoad = true;
	initParams.EffectRoute = EFFECT_ROUTE_FRONT_END;
	initParams.PrepareTimeLimit = 3500;
	CreateAndPlaySound_Persistent(soundName, &m_PreviewRingtoneSound, &initParams);
}

void audScriptAudioEntity::StopPreviewRingtone()
{
	if(m_PreviewRingtoneSound)
	{
		m_PreviewRingtoneSound->StopAndForget();
	}
}
void audScriptAudioEntity::DontAbortCarConversations(bool allowUpsideDown, bool allowOnFire, bool allowDrowning)
{
	s32 scriptIndex = AddScript(); 
	DontAbortCarConversationsWithScriptId(allowUpsideDown, allowOnFire, allowDrowning, scriptIndex);
}

void audScriptAudioEntity::DontAbortCarConversationsWithScriptId(bool allowUpsideDown, bool allowOnFire, bool allowDrowning, s32 scriptIndex)
{
	naConvDisplayf("DontAbortCarConversations() allowUpsideDown: %s allowOnFire: %s allowDrowning: %s", allowUpsideDown ? "True" : "False", allowOnFire ? "True" : "False", allowDrowning? "True" : "False");

	if(allowOnFire || allowUpsideDown || allowDrowning)
	{
		if (scriptIndex < 0 || m_ScriptInChargeOfAudio != -1)
		{	
			naErrorf("Couldn't set persistent car conversations...has another script already called this or have we failed to get an audScript? (scriptIndex: %d, scriptInchargeOfAudio: %d", scriptIndex, m_ScriptInChargeOfAudio);
			return;
		}
		m_ScriptInChargeOfAudio = scriptIndex;
		g_AllowUpsideDownConversations = allowUpsideDown;
		g_AllowVehicleOnFireConversations = allowOnFire;
		g_AllowVehicleDrowningConversations = allowDrowning;
	}
	else
	{
		m_ScriptInChargeOfAudio = -1;
		BANK_ONLY(m_CurrentScriptName = "");
		g_AllowUpsideDownConversations = allowUpsideDown;
		g_AllowVehicleOnFireConversations = allowOnFire;
		g_AllowVehicleDrowningConversations = allowDrowning;
	}

}

void audScriptAudioEntity::MuteGameWorldAndPositionedRadioForTV(bool mute)
{
	audNorthAudioEngine::MuteGameWorldAndPositionedRadioForTv(mute);
}

bool audScriptAudioEntity::RequestScriptWeaponAudio(const char * itemName)
{
	s32 scriptIndex = AddScript();
	audScript * script = GetScript(scriptIndex);
	if(!script)
	{
		return false;
	}

	return script->RequestScriptWeaponAudio(itemName);
}
void audScriptAudioEntity::ReleaseScriptWeaponAudio()
{
	s32 scriptIndex = AddScript();
	audScript * script = GetScript(scriptIndex);
	if(!script)
	{
		return;
	}

	script->ReleaseScriptWeaponAudio();
}
bool audScriptAudioEntity::RequestScriptVehicleAudio(u32 vehicleModelName)
{
	s32 scriptIndex = AddScript();
	audScript * script = GetScript(scriptIndex);
	if(!script)
	{
		return false;
	}

	return script->RequestScriptVehicleAudio(vehicleModelName);
}

#if !__FINAL
void audScriptAudioEntity::DebugScriptStream(s32 scriptIndex)
{
	GtaThread* currentThread = static_cast<GtaThread*>(scrThread::GetThread(m_Scripts[scriptIndex].ScriptThreadId));
	const char * currentScriptName = currentThread ? currentThread->GetScriptName() : "none"; 
	GtaThread* streamThread = static_cast<GtaThread*>(scrThread::GetThread(m_Scripts[sm_ScriptInChargeOfTheStream].ScriptThreadId));
	const char *streamScriptName = streamThread ? streamThread->GetScriptName() : "none"; 
	naWarningf("Attempting to preload as stream from script %s but another script (%s) is in charge of script stream", currentScriptName, streamScriptName);
}
#endif

bool audScriptAudioEntity::PreloadStream(const char *streamName, const u32 startOffset, const char * setName)
{
#if __BANK
	if(g_ScriptedStreamSpew)
	{
		audDebugf1("[audScriptedStream] Preload stream call, state: %s,  frame : %u",audScriptAudioEntity::GetStreamStateName(m_ScriptStreamState) , fwTimer::GetFrameCount());
	}
#endif 
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		safecpy(sm_CurrentStreamName, streamName);
		safecpy(sm_CurrentSetName, setName);
		sm_CurrentStartOffset = startOffset;
	}
#endif

	s32 scriptIndex = AddScript();
	if((sm_ScriptInChargeOfTheStream != -1 && sm_ScriptInChargeOfTheStream != scriptIndex))
	{
#if !__FINAL
	DebugScriptStream(scriptIndex);
#endif
		return true;
	}
	audScript *script = GetScript(scriptIndex);
	if(!script)
		return false;
	audScriptAudioEntity::sm_ScriptInChargeOfTheStream = scriptIndex;

	return PreloadStreamInternal(streamName, startOffset, setName);
}

bool audScriptAudioEntity::PreloadStreamInternal(const char *streamName, const u32 startOffsetMs, const char *setName)
{
	naCErrorf(m_ScriptStreamState != AUD_SCRIPT_STREAM_PLAYING, "Shouldn't try to preload while script stream state is set to playing");

	 audMetadataRef streamDataRef;

	if(setName)
	{
		audSoundSet soundSet;
		soundSet.Init(setName);

#if GTA_REPLAY
		if(CReplayMgr::IsPlaying())
		{
			char buf[64] = {0};
			formatf(buf, "%s_REPLAY", streamName);
			streamDataRef = soundSet.Find(buf);
		}
#endif

		if(!streamDataRef.IsValid())
		{
			streamDataRef = soundSet.Find(streamName);
		}
	}
	else
	{
#if GTA_REPLAY
		if(CReplayMgr::IsPlaying())
		{
			char buf[64] = {0};
			formatf(buf, "%s_REPLAY", streamName);
			streamDataRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(atStringHash(buf));
		}
#endif
		if(!streamDataRef.IsValid())
		{
			streamDataRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(atStringHash(streamName));
		}
	}


	if(m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARED && streamDataRef == m_LoadedStreamRef)
	{
#if __BANK
		if(g_ScriptedStreamSpew)
		{
			audDebugf1("[audScriptedStream] Stream %s successfully prepared",SOUNDFACTORY.GetMetadataManager().GetObjectName(streamDataRef));
		}
#endif 
		return true;
	}


	if(m_ScriptStreamState == AUD_SCRIPT_STREAM_IDLE) 
	{
		// grab a slot
		if(m_StreamSlot == NULL)
		{
			audStreamClientSettings settings;
			settings.priority = audStreamSlot::STREAM_PRIORITY_SCRIPT;
			settings.stopCallback = &OnStopCallback;
			settings.hasStoppedCallback = &HasStoppedCallback;
			settings.userData = 0;

			m_StreamSlot = audStreamSlot::AllocateSlot(&settings);
		}

		if(m_StreamSlot && (m_StreamSlot->GetState() == audStreamSlot::STREAM_SLOT_STATUS_ACTIVE))
		{
			audSoundInitParams initParams;
			initParams.StartOffset = startOffsetMs;
			initParams.IsStartOffsetPercentage = false;
			Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
			CreateSound_PersistentReference(streamDataRef, &m_StreamSound, &initParams);
			if(!m_StreamSound)
			{
#if __BANK
				if(g_ScriptedStreamSpew)
				{
					audDebugf1("[audScriptedStream] Failed to create the sound: %s",SOUNDFACTORY.GetMetadataManager().GetObjectName(streamDataRef));
				}
#endif 
				return false;
			}
			else
			{
				m_ScriptStreamState = AUD_SCRIPT_STREAM_PREPARING;
			}			
		}
	}

	if(m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARING)
	{
		naAssertf(m_StreamSlot, "About to access a null ptr....");
		naAssertf(m_StreamSound, "About to access a null ptr....");
		if(m_StreamSound)
		{
			audPrepareState ret = m_StreamSound->Prepare(m_StreamSlot->GetWaveSlot(), true);

			if(ret == AUD_PREPARED)
			{
				naDisplayf("Scripted stream %s prepared", streamName);
				m_LoadedStreamRef = streamDataRef;
				m_ScriptStreamState = AUD_SCRIPT_STREAM_PREPARED;
#if __BANK
				if(g_ScriptedStreamSpew)
				{
					audDebugf1("[audScriptedStream] Stream %s successfully prepared",SOUNDFACTORY.GetMetadataManager().GetObjectName(streamDataRef));
				}
#endif 
				return true;
			}
		}
		else
		{
#if __BANK
			if(g_ScriptedStreamSpew)
			{
				audDebugf1("[audScriptedStream] Preparing a stream with no sound: %s going into IDLE",SOUNDFACTORY.GetMetadataManager().GetObjectName(streamDataRef));
			}
#endif 
			m_ScriptStreamState = AUD_SCRIPT_STREAM_IDLE;	
		}
	}

#if GTA_REPLAY
	//During replay playback when scrubbing around the timeline its possible for the streamstate to get stuck
	//in the prepared state, if we're in this state and trying to preload a different stream then stop the current one.
	if(CReplayMgr::IsEditModeActive())
	{
		if( m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARED && streamDataRef != m_LoadedStreamRef )
		{
			StopStream();
			return false;
		}
	}
#endif //GTA_REPLAY

	return false;
}

void audScriptAudioEntity::PlayStreamFromEntity(CEntity *entity)
{
	s32 scriptIndex = AddScript();
	if((sm_ScriptInChargeOfTheStream != -1 && sm_ScriptInChargeOfTheStream != scriptIndex))
	{
#if !__FINAL
	DebugScriptStream(scriptIndex);
#endif
		return;
	}
	audScript *script = GetScript(scriptIndex);
	if(!script)
		return;

	PlayStreamFromEntityInternal(entity);

}


void audScriptAudioEntity::PlayStreamFrontend()
{
	s32 scriptIndex = AddScript();
	if((sm_ScriptInChargeOfTheStream != -1 && sm_ScriptInChargeOfTheStream != scriptIndex))
	{
#if !__FINAL
		DebugScriptStream(scriptIndex);
#endif
		return;
	}
	audScript *script = GetScript(scriptIndex);
	if(!script)
		return;

	PlayStreamFrontendInternal();
}

void audScriptAudioEntity::PlayStreamFromEntityInternal(CEntity *entity)
{
	if(naVerifyf(entity, "CEntity* must be valid, can't play stream"))
	{
		if(naVerifyf((m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARED), "Can't play a stream that hasn't been prepared, m_ScriptStreamState: %d", m_ScriptStreamState))
		{
			naAssertf(m_StreamSound, "About to access a null ptr...");
			naAssertf(m_StreamSlot, "About to access a null ptr...");
			if(!m_StreamSlot && g_DebugPlayingScriptSounds)
			{
				naWarningf("Must have a stream slot to play stream");
			}

			m_StreamTrackerEntity = entity;

			m_StreamEnvironmentGroup = (naEnvironmentGroup*)entity->GetAudioEnvironmentGroup();
			if(!m_StreamEnvironmentGroup)
			{
				m_StreamEnvironmentGroup = CreateScriptEnvironmentGroup(entity, entity->GetTransform().GetPosition());
			}
			if(m_StreamSound)
			{
				m_StreamSound->GetRequestedSettings()->SetEnvironmentGroup(m_StreamEnvironmentGroup);
				m_StreamSound->SetRequestedPosition(m_StreamTrackerEntity->GetTransform().GetPosition());
				m_StreamSound->Play();
			}

			m_StreamType = AUD_PLAYSTREAM_ENTITY;
			m_ScriptStreamState = AUD_SCRIPT_STREAM_PLAYING;
			m_LoadedStreamRef.SetInvalid();

#if GTA_REPLAY
			m_DisableReplayScriptStreamRecording = IsFlagSet(audScriptAudioFlags::DisableReplayScriptStreamRecording);
			if(!m_DisableReplayScriptStreamRecording && m_StreamSound)
			{
				REPLAY_ONLY(CReplayMgr::ReplayRecordPlayStreamFromEntity(entity, m_StreamSound, sm_CurrentStreamName, sm_CurrentStartOffset, sm_CurrentSetName);)
			}
#endif
#if __BANK
			if(g_ScriptedStreamSpew)
			{
				audDebugf1("[audScriptedStream] Playing stream: %s from entity",SOUNDFACTORY.GetMetadataManager().GetObjectNameFromNameTableOffset(m_StreamSound->GetBaseMetadata()->NameTableOffset));
			}
#endif 
		}
		else if(g_DebugPlayingScriptSounds)
		{
			naWarningf("Can't play a stream that hasn't been prepared, m_ScriptStreamState: %d", m_ScriptStreamState);
		}
	}
}

void audScriptAudioEntity::PlayStreamFrontendInternal()
{
	if(naVerifyf((m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARED), "Can't play a stream that hasn't been prepared, m_ScriptStreamState: %d", m_ScriptStreamState))
	{
		naAssertf(m_StreamSound, "Must have a stream sound to play: about to access a null ptr....");
		if(!m_StreamSound && g_DebugPlayingScriptSounds)
		{
			Displayf("Must have a stream sound to play: about to access a null ptr....");
		}

		naAssertf(m_StreamSlot, "Must have a stream slot");
		if(!m_StreamSlot && g_DebugPlayingScriptSounds)
		{
			naDisplayf("Must have a stream slot to play stream");
		}

		if(m_StreamTrackerEntity)
		{
			m_StreamTrackerEntity = NULL;
		}

		m_StreamEnvironmentGroup = NULL;

		if(m_StreamSound)
		{
			m_StreamSound->Play();
		}
		m_StreamType = AUD_PLAYSTREAM_FRONTEND;
		m_ScriptStreamState = AUD_SCRIPT_STREAM_PLAYING;
		m_LoadedStreamRef.SetInvalid();

#if GTA_REPLAY
		m_DisableReplayScriptStreamRecording = IsFlagSet(audScriptAudioFlags::DisableReplayScriptStreamRecording)  || IsFlagSet(audScriptAudioFlags::DisableReplayScriptFrontendStreamRecording);
		if(!m_DisableReplayScriptStreamRecording && m_StreamSound)
		{
			REPLAY_ONLY(CReplayMgr::ReplayRecordPlayStreamFrontend(m_StreamSound, sm_CurrentStreamName, sm_CurrentStartOffset, sm_CurrentSetName);)
		}
#endif
#if __BANK
		if(g_ScriptedStreamSpew)
		{
			audDebugf1("[audScriptedStream] Playing stream: %s frontend",SOUNDFACTORY.GetMetadataManager().GetObjectNameFromNameTableOffset(m_StreamSound->GetBaseMetadata()->NameTableOffset));
		}
#endif 
	}
	else if(g_DebugPlayingScriptSounds)
	{
		Displayf("Can't play a stream that hasn't been prepared, m_ScriptStreamState: %d", m_ScriptStreamState);
	}
}

const u32 audScriptAudioEntity::GetStreamPlayTime() const
{
	if(m_StreamSound && m_ScriptStreamState == AUD_SCRIPT_STREAM_PLAYING)
	{
		if(naVerifyf(m_StreamSound->GetSoundTypeID() == StreamingSound::TYPE_ID,"Script stream %s, with a parent sound that it's not a stream sound. Please bug default audio.",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_StreamSound->GetBaseMetadata()->NameTableOffset)))
		{
			return ((audStreamingSound*)m_StreamSound)->GetCurrentPlayTimeOfWave();				
		}
	}
	return 0;
}

bool audScriptAudioEntity::IsPlayingStream()
{
	bool isPlayingStream = false; 
	
	if(m_StreamSound && m_ScriptStreamState == AUD_SCRIPT_STREAM_PLAYING)
	{
		isPlayingStream = true;
	}

	return isPlayingStream;
}

void audScriptAudioEntity::PlayStreamFromPosition(const Vector3 &pos)
{
	s32 scriptIndex = AddScript();
	if((sm_ScriptInChargeOfTheStream != -1 && sm_ScriptInChargeOfTheStream != scriptIndex))
	{
#if !__FINAL
		DebugScriptStream(scriptIndex);
#endif
		return;
	}
	audScript *script = GetScript(scriptIndex);
	if(!script)
		return;

	PlayStreamFromPositionInternal(pos);
}

void audScriptAudioEntity::PlayStreamFromPositionInternal(const Vector3 &pos)
{
	if(naVerifyf((m_ScriptStreamState == AUD_SCRIPT_STREAM_PREPARED), "Can't play a stream that hasn't been prepared, m_ScriptStreamState: %d", m_ScriptStreamState))
	{
		naAssertf(m_StreamSound, "Must have a stream sound to play: about to access a null ptr....");
		if(!m_StreamSound && g_DebugPlayingScriptSounds)
		{
			Displayf("Must have a stream sound to play: about to access a null ptr....");
		}

		naAssertf(m_StreamSlot, "Must have a stream slot");
		if(!m_StreamSlot && g_DebugPlayingScriptSounds)
		{
			naDisplayf("Must have a stream slot to play stream");
		}

		if(m_StreamTrackerEntity)
		{
			m_StreamTrackerEntity = NULL;
		}

		m_StreamEnvironmentGroup = NULL;
		m_StreamPosition.Set(pos);

		if(m_StreamSound)
		{
			m_StreamSound->SetRequestedPosition(VECTOR3_TO_VEC3V(pos));
			m_StreamSound->Play();
		}
		m_StreamType = AUD_PLAYSTREAM_POSITION;
		m_ScriptStreamState = AUD_SCRIPT_STREAM_PLAYING;
		m_LoadedStreamRef.SetInvalid();

#if GTA_REPLAY
		m_DisableReplayScriptStreamRecording = IsFlagSet(audScriptAudioFlags::DisableReplayScriptStreamRecording);
		if(!m_DisableReplayScriptStreamRecording && m_StreamSound)
		{
			REPLAY_ONLY(CReplayMgr::ReplayRecordPlayStreamFromPosition(pos, m_StreamSound, sm_CurrentStreamName, sm_CurrentStartOffset, sm_CurrentSetName);)
		}
#endif
#if __BANK
		if(g_ScriptedStreamSpew)
		{
			audDebugf1("[audScriptedStream] Playing stream: %s from position",SOUNDFACTORY.GetMetadataManager().GetObjectNameFromNameTableOffset(m_StreamSound->GetBaseMetadata()->NameTableOffset));
		}
#endif 
	}
	else if(g_DebugPlayingScriptSounds)
	{
		Displayf("Can't play a stream that hasn't been prepared, m_ScriptStreamState: %d", m_ScriptStreamState);
	}
}

void audScriptAudioEntity::StopStream()
{
	if(m_StreamSound)
	{
		m_StreamSound->StopAndForget();
	}
	if(m_StreamSlot)
	{
		m_StreamSlot->Free();
		m_StreamSlot = NULL;
	}
	if(m_StreamTrackerEntity)
	{
		m_StreamTrackerEntity = NULL;
	}
	m_StreamType = AUD_PLAYSTREAM_UNKNOWN;
	m_StreamEnvironmentGroup = NULL;
	m_ScriptStreamState = AUD_SCRIPT_STREAM_IDLE;
	m_LoadedStreamRef.SetInvalid();
	audScriptAudioEntity::sm_ScriptInChargeOfTheStream = -1;
#if __BANK
	if(g_ScriptedStreamSpew)
	{
		audDebugf1("[audScriptedStream] StopStream called frame %u",fwTimer::GetFrameCount());
	}
#endif 
#if GTA_REPLAY
	for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
	{
		m_StreamVariableNameHash[i] = g_NullSoundHash;
		m_StreamVariableValue[i] = 0.0f;
	}
	CReplayMgr::RecordStopStream();
#endif
}

bool audScriptAudioEntity::OnStopCallback(u32 UNUSED_PARAM(userData))
{
	//We are losing our stream slot, so stop the scripted stream.
	if(g_ScriptAudioEntity.m_StreamSound)
	{
		g_ScriptAudioEntity.m_StreamSound->StopAndForget();
	}
	g_ScriptAudioEntity.m_StreamType = AUD_PLAYSTREAM_UNKNOWN;

#if GTA_REPLAY
	for(int i=0; i<AUD_MAX_REPLAY_SCRIPT_SOUND_VARIABLES; i++)
	{
		g_ScriptAudioEntity.m_StreamVariableValue[i] = 0.0f;
		g_ScriptAudioEntity.m_StreamVariableNameHash[i] = g_NullSoundHash;
	}
#endif
#if __BANK
	if(g_ScriptedStreamSpew)
	{
		audDebugf1("[audScriptedStream] OnStopCallback which mean we lost our stream slot, stop the sound and go into idle ");
	}
#endif 
	g_ScriptAudioEntity.m_ScriptStreamState = AUD_SCRIPT_STREAM_IDLE;
	g_ScriptAudioEntity.m_LoadedStreamRef.SetInvalid();
	g_ScriptAudioEntity.m_StreamEnvironmentGroup = NULL;
	return true;
}

bool audScriptAudioEntity::HasStoppedCallback(u32 UNUSED_PARAM(userData))
{
	bool hasStopped = true;
	audStreamSlot *streamSlot = g_ScriptAudioEntity.m_StreamSlot;
	if(streamSlot)
	{
		//Check if we are still loading or playing from our allocated wave slot
		audWaveSlot *waveSlot = streamSlot->GetWaveSlot();
		if(waveSlot && (waveSlot->GetIsLoading() || (waveSlot->GetReferenceCount() > 0)))
		{
			hasStopped = false;
		}
	}

	return hasStopped;
}

void audScriptAudioEntity::StopPedSpeaking(CPed *ped, bool shouldDisable )
{
	// for now, just flag them to be completely silent
	if (ped && ped->GetSpeechAudioEntity())
	{
		ped->GetSpeechAudioEntity()->DisableSpeaking(shouldDisable );
	}
}

void audScriptAudioEntity::BlockAllSpeechFromPed(CPed *ped, bool shouldDisable, bool shouldSuppressOutgoingNetworkSpeech)
{
	if (ped && ped->GetSpeechAudioEntity())
	{
		ped->GetSpeechAudioEntity()->BlockAllSpeechFromPed(shouldDisable, shouldSuppressOutgoingNetworkSpeech);
	}
}

void audScriptAudioEntity::StopPedSpeakingSynced(CPed *ped, bool shouldDisable)
{
	// for now, just flag them to be completely silent
	if (ped && ped->GetSpeechAudioEntity())
	{
		ped->GetSpeechAudioEntity()->DisableSpeakingSynced(shouldDisable );
	}
}

void audScriptAudioEntity::StopPedPainAudio(CPed *ped, bool shouldDisable )
{
	if (ped && ped->GetSpeechAudioEntity())
	{
		ped->GetSpeechAudioEntity()->DisablePainAudio(shouldDisable );
	}
}

void audScriptAudioEntity::MobilePreRingHandler(u32 UNUSED_PARAM(hash), void *UNUSED_PARAM(context))
{
	g_ScriptAudioEntity.PlayMobilePreRing(g_MobilePreRingHash, 0.3f);
}

void audScriptAudioEntity::PlayMobileGetSignal(f32 probability)
{
	g_ScriptAudioEntity.PlayMobilePreRing(g_MobileGetSignalHash, probability);
}

void audScriptAudioEntity::PlayMobilePreRing(u32 NA_RADIO_ENABLED_ONLY(mobileInterferenceHash), f32 NA_RADIO_ENABLED_ONLY(probability))
{
#if NA_RADIO_ENABLED
	u32 timeNow = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	// Purposefully leave a fair bit of a delay between triggers - don't want it constantly happening when popping in and out of tunnels
	if(timeNow - m_LastMobileInterferenceTime > 10000 BANK_ONLY(|| g_ForceMobileInterference))
	{
		// If the radio's playing frontend, and we're not already playing a prering, play one now
		if(g_RadioAudioEntity.IsVehicleRadioOn() && !m_MobileInterferenceSound && (audEngineUtil::ResolveProbability(probability) BANK_ONLY(|| g_ForceMobileInterference)))
		{
			CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
			if(playerVehicle && playerVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				CarAudioSettings* settings = ((audCarAudioEntity*)playerVehicle->GetVehicleAudioEntity())->GetCarAudioSettings();

				if(settings && AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_CARAUDIOSETTINGS_MOBILECAUSESRADIOINTERFERENCE) == AUD_TRISTATE_TRUE BANK_ONLY(|| g_ForceMobileInterference))
				{
					CreateAndPlaySound_Persistent(mobileInterferenceHash, &m_MobileInterferenceSound);
				}
			}
		}

		m_LastMobileInterferenceTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}
#endif
}

naEnvironmentGroup* audScriptAudioEntity::CreateScriptEnvironmentGroup(const CEntity* entity, const Vec3V& pos) const
{

	naEnvironmentGroup* environmentGroup = naEnvironmentGroup::Allocate("Script");
	if(environmentGroup)	// Need to check for NULL because we could run out of space in the naEnvironmentGroup pool
	{
		// NULL entity; will be deleted when the sound is deleted
		environmentGroup->Init(NULL, 20);

		// Force a source environment update giving it our entity and interior information, since we don't normally have one
		if(entity)
		{
			environmentGroup->ForceSourceEnvironmentUpdate(entity);
		}
		// We only create script environmentGroups without an entity if we know they're outside, so set that info on the group
		else
		{
			environmentGroup->SetInteriorInfoWithInteriorInstance(NULL, INTLOC_ROOMINDEX_LIMBO);
		}

		environmentGroup->SetSource(pos);
		environmentGroup->SetUsePortalOcclusion(true);
		environmentGroup->SetMaxPathDepth(audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());
	}
	return environmentGroup;
}


#if GTA_REPLAY
bool audScriptAudioEntity::HasAlreadyPlayed(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber)
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS; i++)
	{
		if( sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash == contextNameHash &&
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber == variationNumber &&
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash == voiceNameHash)
		{
			return true;
		}
	}
	return false;
}

void audScriptAudioEntity::AddToPlayedList(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber)
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS; i++)
	{
		if(sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber == -1)
		{
			//Displayf("AddToPlayedList %u, %u, %u", CReplayMgr::GetCurrentTimeRelativeMs(), contextNameHash, voiceNameHash);
			sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash = contextNameHash;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber = variationNumber;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash = voiceNameHash;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].TimeStamp = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
			return;
		}
	}
}

void audScriptAudioEntity::UpdateTimeStamp(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber)
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS; i++)
	{
		if(sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash == voiceNameHash &&
			sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash == contextNameHash &&
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber == variationNumber)
		{
			sm_ReplayScriptedSpeechAlreadyPlayed[i].TimeStamp = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
			return;
		}
	}
}

void audScriptAudioEntity::RemoveFromAreadyPlayedList(u32 voiceNameHash, u32 contextNameHash, s32 variationNumber)
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS; i++)
	{
		if( sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash == contextNameHash &&
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber == variationNumber &&
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash == voiceNameHash)
		{
			//Displayf("RemoveFromAreadyPlayedList %u, %u, %u", CReplayMgr::GetCurrentTimeRelativeMs(), contextNameHash, voiceNameHash);
			sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash = 0;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber = -1;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash = 0;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].TimeStamp = 0;
		}
	}
}
void audScriptAudioEntity::ClearPlayedList()
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS; i++)
	{
		sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash = 0;
		sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber = -1;
		sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash = 0;
		sm_ReplayScriptedSpeechAlreadyPlayed[i].TimeStamp = 0;
	}
	StopAllReplayScriptedSpeech();
}

void audScriptAudioEntity::StopAllReplayScriptedSpeech()
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_SLOTS; i++)
	{
		if(sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound)
		{
			sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound->StopAndForget();
			sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound = NULL;
		}
	}
	
	g_SpeechManager.FreeAllScriptedSpeechSlots();
}


ePreloadResult audScriptAudioEntity::PreloadReplayScriptedSpeech(u32 voiceNameHash, u32 contextHash, u32 variationNumber, s32 UNUSED_PARAM(waveSlotIndex ))
{
	//Displayf("PreloadReplayScriptedSpeech time:%d - %u - %d - %d", CReplayMgr::GetCurrentTimeRelativeMs(), m_uContextNameHash, m_sWaveSlotIndex, m_sPreDelay);

	//Displayf("PreloadReplayScriptedSpeech time:%u - %u - %d - evenet:%u, %u, %u game:%u", CReplayMgr::GetCurrentTimeRelativeMs(), m_uContextNameHash, m_sWaveSlotIndex, 
	//	eventInfo.GetReplayTime(), eventInfo.GetPreloadOffsetTime(), eventInfo.GetIsFirstFrame(),
	//	GetGameTime());

	if(CReplayMgr::IsCursorFastForwarding() || CReplayMgr::IsCursorRewinding() || CReplayMgr::IsScrubbing())
	{
		return PRELOAD_SUCCESSFUL; 
	}

	bool hasAlreadyPlayed = g_ScriptAudioEntity.HasAlreadyPlayed(voiceNameHash, contextHash, variationNumber);
	if(hasAlreadyPlayed)
	{
		return PRELOAD_SUCCESSFUL; 
	}

	s32 waveSlot = g_ScriptAudioEntity.LookupReplayPreloadScriptedSpeechSlot(voiceNameHash, contextHash, (s32)variationNumber);
	if( waveSlot == -1)
	{
		waveSlot = g_SpeechManager.GetScriptedSpeechSlot(NULL);
	}
	if(waveSlot == -1)
	{
		return PRELOAD_SUCCESSFUL;
	}

	s32 sSpeechSoundIndex = g_ScriptAudioEntity.GetFreeReplayScriptedSpeechSlot(voiceNameHash, contextHash, (s32)variationNumber, waveSlot);
	if(sSpeechSoundIndex < 0)
	{
		return PRELOAD_FAIL;
	}

	s32 sBaseWaveSlotIndex = audWaveSlot::GetWaveSlotIndex(audWaveSlot::FindWaveSlot("SCRIPT_SPEECH_0"));
	waveSlot += sBaseWaveSlotIndex;

	if(g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound == NULL && !hasAlreadyPlayed) 
	{
		audSoundInitParams initParams;
		g_ScriptAudioEntity.CreateSound_PersistentReference( "PRELOAD_SPEECH_SOUND", (audSound**)&g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound, &initParams );

		if( g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound )
		{
			bool bSuccess = g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound->InitSpeech( voiceNameHash, contextHash, variationNumber );
			if( !bSuccess )
			{
				// ditch this sound, as it failed to find a variation
				g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound->StopAndForget();
				return PRELOAD_SUCCESSFUL;
			}

			audWaveSlot* pWaveSlot = audWaveSlot::GetWaveSlotFromIndex(waveSlot);
			if( pWaveSlot ) 
			{
				g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].WaveSlot = pWaveSlot;
				g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].WaveSlotIndex = waveSlot;
			}
		}
	}
	if( g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound )
	{
		audWaveSlot* pWaveSlot = audWaveSlot::GetWaveSlotFromIndex(waveSlot);
		if(pWaveSlot)
		{
			audPrepareState prepareState  = g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound->Prepare(pWaveSlot, true, naWaveLoadPriority::Speech);
			if( prepareState == AUD_PREPARE_FAILED )
			{
				g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ScriptedSpeechSound->StopAndForget();
			}
			else
			{
				g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].VoiceNameHash = voiceNameHash;
				g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].ContextNameHash = contextHash;
				g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[sSpeechSoundIndex].VariationNumber = (s32)variationNumber;
				if(prepareState == AUD_PREPARING)
				{
					return PRELOAD_FAIL;
				}
			}
		}
	}

	return PRELOAD_SUCCESSFUL; 
}

int audScriptAudioEntity::LookupReplayPreloadScriptedSpeechSlot( u32 uVoiceNameHash, u32 uContextNameHash, s32 sVariationNumber)
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_SLOTS; i++)
	{
		if( sm_ReplayScriptedSpeechSlot[i].VariationNumber == sVariationNumber && 
			sm_ReplayScriptedSpeechSlot[i].ContextNameHash == uContextNameHash &&
			sm_ReplayScriptedSpeechSlot[i].VoiceNameHash == uVoiceNameHash )
		{
			return i;
		}
	}
	return -1;
}

int audScriptAudioEntity::GetFreeReplayScriptedSpeechSlot( u32 uVoiceNameHash, u32 uContextNameHash, s32 sVariationNumber, s32 speechSlotIndex)
{
	s32 sBaseWaveSlotIndex = audWaveSlot::GetWaveSlotIndex(audWaveSlot::FindWaveSlot("SCRIPT_SPEECH_0"));

	s32 i = speechSlotIndex;

	// check and see if we are already preloading
	if( sm_ReplayScriptedSpeechSlot[i].VariationNumber == sVariationNumber && 
		sm_ReplayScriptedSpeechSlot[i].ContextNameHash == uContextNameHash &&
		sm_ReplayScriptedSpeechSlot[i].VoiceNameHash == uVoiceNameHash &&
		sm_ReplayScriptedSpeechSlot[i].WaveSlot == audWaveSlot::GetWaveSlotFromIndex(sBaseWaveSlotIndex + speechSlotIndex) )
	{
		return i;
	}
	else
	{
		if(g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound)
		{
			return -1;
		}
	}
	return i;
}

void audScriptAudioEntity::UpdateReplayScriptedSpeechSlots()
{
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_SLOTS; i++)
	{
		if(sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound)
		{
			if(sm_ReplayScriptedSpeechSlot[i].WaveSlot)
			{
				audPrepareState state = sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound->Prepare(sm_ReplayScriptedSpeechSlot[i].WaveSlot, true, naWaveLoadPriority::Speech);
				if(state == AUD_PREPARE_FAILED || (state == AUD_PREPARED && g_ScriptAudioEntity.HasAlreadyPlayed(sm_ReplayScriptedSpeechSlot[i].VoiceNameHash, sm_ReplayScriptedSpeechSlot[i].ContextNameHash, sm_ReplayScriptedSpeechSlot[i].VariationNumber)))
				{
					sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound->StopAndForget();
					sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound = NULL;
				}
			}
		}
	}

	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(1);
	for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_PLAYED_SLOTS; i++)
	{
		if(sm_ReplayScriptedSpeechAlreadyPlayed[i].TimeStamp != 0 && currentTime > sm_ReplayScriptedSpeechAlreadyPlayed[i].TimeStamp + 10000) 
		{
			//Displayf("RemoveFromAreadyPlayedList auto %u, %u, %u", CReplayMgr::GetCurrentTimeRelativeMs(), sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash, sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash);
			sm_ReplayScriptedSpeechAlreadyPlayed[i].ContextNameHash = 0;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VariationNumber = -1;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].VoiceNameHash = 0;
			sm_ReplayScriptedSpeechAlreadyPlayed[i].TimeStamp = 0;
		}
	}
}

void audScriptAudioEntity::CleanUpReplayScriptedSpeechWaveSlot(s32 waveSlotIndex)
{
	s32 sBaseWaveSlotIndex = audWaveSlot::GetWaveSlotIndex(audWaveSlot::FindWaveSlot("SCRIPT_SPEECH_0"));
	replayAssert(waveSlotIndex >= sBaseWaveSlotIndex);
	s32 i = waveSlotIndex - sBaseWaveSlotIndex;
	CleanUpReplayScriptedSpeechSlot(i, true);
}

void audScriptAudioEntity::CleanUpReplayScriptedSpeechSlot(s32 slotIndex, bool force)
{
	if(sm_ReplayScriptedSpeechSlot[slotIndex].ScriptedSpeechSound)
	{
		if(force)
		{
			sm_ReplayScriptedSpeechSlot[slotIndex].ScriptedSpeechSound->StopAndForget();
			sm_ReplayScriptedSpeechSlot[slotIndex].ScriptedSpeechSound = NULL;
		}
		else if(sm_ReplayScriptedSpeechSlot[slotIndex].WaveSlot)
		{
			audWaveSlot* pWaveSlot = sm_ReplayScriptedSpeechSlot[slotIndex].WaveSlot;
			audPrepareState prepareState  = g_ScriptAudioEntity.sm_ReplayScriptedSpeechSlot[slotIndex].ScriptedSpeechSound->Prepare(pWaveSlot, true, naWaveLoadPriority::Speech);
			if( prepareState != AUD_PREPARING )
			{
				sm_ReplayScriptedSpeechSlot[slotIndex].ScriptedSpeechSound->StopAndForget();
				sm_ReplayScriptedSpeechSlot[slotIndex].ScriptedSpeechSound = NULL;
			}
		}
	}
	g_SpeechManager.FreeScriptedSpeechSlot(slotIndex);
}

naEnvironmentGroup* audScriptAudioEntity::CreateReplayScriptedEnvironmentGroup(const CEntity* entity, const Vec3V& pos) const
{
	return CreateScriptEnvironmentGroup(entity, pos);
}

bool audScriptAudioEntity::IsPlayerConversationActive(const CPed* pPlayer, s16& currentLine)
{
	//Check if the player is in the conversation
	if(IsPedInCurrentConversation(pPlayer))
	{
		// Check we've actually started the conversation
		if ( m_PlayingScriptedConversationLine>=0 && m_PlayingScriptedConversationLine < AUD_MAX_SCRIPTED_CONVERSATION_LINES 
			&& (currentLine == -1 || currentLine == m_PlayingScriptedConversationLine) ) 
		{
			currentLine = m_PlayingScriptedConversationLine;
			return true;
		}
	}

	currentLine = -1;
	return false;
}


void audScriptAudioEntity::RecordScriptSlotSound(const char* setName, const char* soundName, audSoundInitParams* initParams, const CEntity* pEntity, audSound* sound)
{
	RecordScriptSlotSound(setName ? atStringHash(setName) : 0, soundName ? atStringHash(soundName) : g_NullSoundHash,initParams,pEntity,sound);
}

void audScriptAudioEntity::RecordScriptSlotSound(const int setNameHash, const int soundNameHash, audSoundInitParams* initParams, const CEntity* pEntity, audSound* sound)
{
	if(pEntity && pEntity->GetAudioEntity())
	{
		if(setNameHash != 0)
		{
			CReplayMgr::ReplayRecordSoundPersistant(setNameHash,soundNameHash, initParams, sound, pEntity, eNoGlobalSoundEntity, ReplaySound::CONTEXT_INVALID);
		}
		else
		{
			CReplayMgr::ReplayRecordSoundPersistant(soundNameHash, initParams, sound, pEntity, eNoGlobalSoundEntity, ReplaySound::CONTEXT_INVALID);
		}
	}
	else
	{
		if(setNameHash != 0)
		{
			CReplayMgr::ReplayRecordSoundPersistant(setNameHash,soundNameHash, initParams, sound, pEntity, eScriptSoundEntity, ReplaySound::CONTEXT_INVALID);
		}
		else
		{
			CReplayMgr::ReplayRecordSoundPersistant(soundNameHash, initParams, sound, pEntity, eScriptSoundEntity, ReplaySound::CONTEXT_INVALID);
		}
	}	
}

void audScriptAudioEntity::RecordUpdateScriptSlotSound(const u32 setNameHash, const u32 soundNameHash, audSoundInitParams* initParams, const CEntity* pEntity, audSound* sound)
{
	if(pEntity && pEntity->GetAudioEntity())
	{
		if(setNameHash != g_NullSoundHash)
		{
			CReplayMgr::ReplayRecordSoundPersistant(setNameHash, soundNameHash, initParams, sound, pEntity, eNoGlobalSoundEntity, ReplaySound::CONTEXT_INVALID, true);
		}
		else
		{
			CReplayMgr::ReplayRecordSoundPersistant(soundNameHash, initParams, sound, pEntity, eNoGlobalSoundEntity, ReplaySound::CONTEXT_INVALID, true);
		}
	}
	else
	{
		if(setNameHash != g_NullSoundHash)
		{
			CReplayMgr::ReplayRecordSoundPersistant(setNameHash, soundNameHash, initParams, sound, pEntity, eScriptSoundEntity, ReplaySound::CONTEXT_INVALID, true);
		}
		else
		{
			CReplayMgr::ReplayRecordSoundPersistant(soundNameHash, initParams, sound, pEntity, eScriptSoundEntity, ReplaySound::CONTEXT_INVALID, true);
		}
	}	
}


#endif


#if __BANK
const char* audScriptAudioEntity::GetStreamStateName(audScriptStreamState state) 
{
	switch(state)
	{
	case AUD_SCRIPT_STREAM_IDLE:
		return "AUD_SCRIPT_STREAM_IDLE";
	case AUD_SCRIPT_STREAM_PREPARING:
		return "AUD_SCRIPT_STREAM_PREPARING";
	case AUD_SCRIPT_STREAM_PREPARED:
		return "AUD_SCRIPT_STREAM_PREPARED";
	case AUD_SCRIPT_STREAM_PLAYING:
		return "AUD_SCRIPT_STREAM_PLAYING";
	default:
		return "UNKNOWN";
	}
}

void audScriptAudioEntity::DebugScriptedConversation()
{
	naConvWarningf("Debugging scripted conversation:");
	if (m_ScriptedConversationInProgress)
	{
		naConvWarningf("Scripted conversation in progress");
	}
	if (m_IsScriptedConversationAMobileCall)
	{
		naConvWarningf("Scripted conversation is a mobile call");
	}
	if (m_WaitingForPreload)
	{
		naConvWarningf("WaitingForPreload");
	}
	if (m_WaitingForPreload)
	{
		naConvWarningf("WaitingForPreload");
	}
	if (m_PauseConversation)
	{
		naConvWarningf("PauseConversation");
	}
	naConvWarningf("PSCL:%d; HighestLoadedLine:%d; HLoadingL:%d; PLSL:%d", m_PlayingScriptedConversationLine, m_HighestLoadedLine, m_HighestLoadingLine, m_PreLoadingSpeechLine);
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_SPEAKERS; i++)
	{
		naConvWarningf("Voice %d: %s", i, m_VoiceName[i]);
	}
	for (s32 i=0; i<AUD_MAX_SCRIPTED_CONVERSATION_LINES; i++)
	{
		naConvWarningf("%d; sp:%d; ctx:%s", i, m_ScriptedConversation[i].Speaker, m_ScriptedConversation[i].Context);
	}
}

void audScriptAudioEntity::DebugScriptedConversationStatic()
{
	g_ScriptAudioEntity.DebugScriptedConversation();
}

//void audScriptAudioEntity::AddCallToLog(const char *scriptCommand)
//{
//
//}
void audScriptAudioEntity::LogScriptBanks()
{
	static const char* stateNames[3] = { "Queued", "Loading", "Loaded" };

	naDisplayf("Logging script bank info @ Time %u (frame %u)", fwTimer::GetTimeInMilliseconds(), fwTimer::GetFrameCount());
	naDisplayf("Currently active script: %s", m_CurrentScriptName);
	
	for(u32 i = 0; i < AUD_MAX_SCRIPT_BANK_SLOTS; i++)
	{
		naDisplayf("    Slot %u", i);
		naDisplayf("        Script Slot Bank: %s (%u)", audWaveSlot::GetBankName(m_BankSlots[i].BankId), m_BankSlots[i].BankId);
		naDisplayf("        Waveslot Bank: %s (%u)", m_BankSlots[i].BankSlot->GetLoadedBankName(), m_BankSlots[i].BankSlot->GetLoadedBankId());
		naDisplayf("        State: %s", stateNames[m_BankSlots[i].State]);
		naDisplayf("        Ref Count: %u", m_BankSlots[i].ReferenceCount);

		for(u32 j = 0; j < AUD_MAX_SCRIPT_NETWORK_BANK_SLOTS; j++)
		{
			if(m_BankSlots[i].NetworkScripts[j].IsValid())
			{
				naDisplayf("        Network Script %d: %s (%u)", j, m_BankSlots[i].NetworkScripts[j].GetScriptName(), (u32)m_BankSlots[i].NetworkScripts[j].GetScriptNameHash());
			}
			else
			{
				naDisplayf("        Network Script %d: none", j);
			}
		}
	}
}

const char* audScriptAudioEntity::GetScriptName(s32 scriptIndex)
{
	audScript * script = GetScript(scriptIndex);
	if (script)
	{
		GtaThread* pThread = static_cast<GtaThread*>(scrThread::GetThread(script->ScriptThreadId));

		if (pThread)
		{
			return pThread->GetScriptName();
		}
	}

	return "none";
}

void audScriptAudioEntity::LogScriptBankRequest(const char *request,const u32 scriptIndex,const u32 bankId, const u32 bankIndex)
{
	const char * scriptName = "none";
	audScript * script = GetScript(scriptIndex);
	if(script)
	{
		GtaThread* pThread = static_cast<GtaThread*>(scrThread::GetThread(script->ScriptThreadId));
		scriptName = pThread ? pThread->GetScriptName() : "none"; 
	}

	if(audVerifyf(bankIndex < m_BankSlots.GetMaxCount(), "Bank index %u exceeds slot count %u, Request %s, Script %s", bankIndex, m_BankSlots.GetMaxCount(), request, scriptName))
	{
		naDisplayf("%s [Script %s] [ScriptIdx %u] [Bank Idx %u] [Bank Name %s] [BankSlotIdx %u] [RefCount  %d]"
			,request,scriptName,scriptIndex,bankId,audWaveSlot::GetBankName(bankId),m_Scripts[scriptIndex].BankSlotIndex,m_BankSlots[bankIndex].ReferenceCount);
		if (m_BankSlots[bankIndex].BankSlot)
		{
			naDisplayf("Script bankSlot: [Id %u] [Bankslot: %s] [WaveSlot: %s]",m_BankSlots[bankIndex].BankId,m_BankSlots[bankIndex].BankSlot->GetLoadedBankName(),m_BankSlots[bankIndex].BankSlot->GetSlotName());
		}
		else
		{
			naDisplayf("Script bankSlot: Null");
		}
		const audWaveSlot* waveSlot = audWaveSlot::FindLoadedBankWaveSlot(m_BankSlots[bankIndex].BankId);
		if(waveSlot)
		{
			naDisplayf("WaveSlot: [Id %u] [Bankslot: %s] [WaveSlot: %s]",waveSlot->GetLoadedBankId(),waveSlot->GetCachedLoadedBankName(),waveSlot->GetSlotName());
		}
		else
		{
			naDisplayf("WaveSlot: Null");
		}
	}
}

void audScriptAudioEntity::LogNetworkScriptBankRequest(const char *request,const char* scriptName,const u32 bankId, const u32 bankIndex)
{
	if(audVerifyf(bankIndex < m_BankSlots.GetMaxCount(), "Bank index %u exceeds slot count %u, Request %s, Script %s", bankIndex, m_BankSlots.GetMaxCount(), request, scriptName))
	{
		naDisplayf("%s [Script %s] [Bank Idx %u] [Bank Name %s]" ,request,scriptName,bankId,audWaveSlot::GetBankName(bankId));

		if (m_BankSlots[bankIndex].BankSlot)
		{
			naDisplayf("Script bankSlot: [Id %u] [Bankslot: %s] [WaveSlot: %s]",m_BankSlots[bankIndex].BankId,m_BankSlots[bankIndex].BankSlot->GetLoadedBankName(),m_BankSlots[bankIndex].BankSlot->GetSlotName());
		}
		else
		{
			naDisplayf("Script bankSlot: Null");
		}
		const audWaveSlot* waveSlot = audWaveSlot::FindLoadedBankWaveSlot(m_BankSlots[bankIndex].BankId);
		if(waveSlot)
		{
			naDisplayf("WaveSlot: [Id %u] [Bankslot: %s] [WaveSlot: %s]",waveSlot->GetLoadedBankId(),waveSlot->GetCachedLoadedBankName(),waveSlot->GetSlotName());
		}
		else
		{
			naDisplayf("WaveSlot: Null");
		}
	}
}
void audScriptAudioEntity::DrawDebug()
{

	if(sm_DrawReplayScriptSpeechSlots)
	{
		char tempString[64];
		static bank_float lineInc = 0.015f;
		f32 lineBase = 0.1f;

#if GTA_REPLAY
		for(int i=0; i<AUD_REPLAY_SCRIPTED_SPEECH_SLOTS; i++)
		{
			s32 index = -1;
			if(sm_ReplayScriptedSpeechSlot[i].WaveSlot)
			{
				index = audWaveSlot::GetWaveSlotIndex(sm_ReplayScriptedSpeechSlot[i].WaveSlot);
			}
			audPrepareState state = AUD_PREPARING;
			if(sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound)
			{
				state = sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound->Prepare(sm_ReplayScriptedSpeechSlot[i].WaveSlot, true, naWaveLoadPriority::Speech);
			}
			bool played = false;
			played = g_ScriptAudioEntity.HasAlreadyPlayed(sm_ReplayScriptedSpeechSlot[i].VoiceNameHash, sm_ReplayScriptedSpeechSlot[i].ContextNameHash, sm_ReplayScriptedSpeechSlot[i].VariationNumber);
			sprintf(tempString, "context %u  voice %u  Speech %d  Waveslot %d  State %d  Played %d", sm_ReplayScriptedSpeechSlot[i].ContextNameHash, sm_ReplayScriptedSpeechSlot[i].VoiceNameHash, 
				sm_ReplayScriptedSpeechSlot[i].ScriptedSpeechSound ? 1 : 0, index, state, played); 		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString ); 	lineBase += lineInc;
		}
#endif // GTA_REPLAY

		lineBase += lineInc; lineBase += lineInc;
		for(int i=0; i<g_MaxScriptedSpeechSlots; i++)
		{
			sprintf(tempString, "ScripSlot : vac %d  refCount %d ", g_SpeechManager.IsScriptSpeechSlotVacant(i), g_SpeechManager.GetScriptSpeechSlotRefCount(i)); 		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString ); 	lineBase += lineInc;
		}
	}

	char buf[256];
	u32 soundHash =  atStringHash(sm_ScriptTriggerOverride);
	// On Screen script banks log
	f32 offset = 0.05f;
	if(sm_DrawScriptBanksLoaded)
	{
		for (int i=0; i<AUD_MAX_SCRIPT_BANK_SLOTS; i++)
		{
			char state[64];
			formatf(state, "LOADED :");
			if(m_BankSlots[i].BankId != AUD_INVALID_BANK_ID)
			{
				const char * scriptName = "none";
#if !__FINAL
				for(int j=0; j < AUD_MAX_SCRIPTS; j++)
				{
					audScript * script = GetScript(j);
					if(script)
					{
						if(script->BankSlotIndex & BIT(i)) 
						{
							GtaThread* pThread = static_cast<GtaThread*>(scrThread::GetThread(script->ScriptThreadId));
							scriptName = pThread ? pThread->GetScriptName() : "none"; 
						}
					}
				}
#endif
				bool isReleased = m_BankSlots[i].ReferenceCount == 0;
				char buf[128];
				Color32 color = Color_green;
				if(isReleased)
				{
					color = Color_blue;
					formatf(state,"FREE:");
				}
				else if( m_BankSlots[i].State == AUD_SCRIPT_BANK_LOADING )
				{
					color = Color_yellow;
					formatf(state,"LOADING...");
				}
				if (m_BankSlots[i].HintLoaded)
				{
					color = Color_LightBlue;
					formatf(state,"HINTED:");
				}
				formatf(buf, "%s [Script %s] [Script Bank %s] [Bank Idx %u] [Script bank slot %d]",state,scriptName,m_BankSlots[i].BankSlot->GetLoadedBankName(), m_BankSlots[i].BankSlot->GetLoadedBankId(), i);
				grcDebugDraw::Text(Vector2(0.05f, offset), color,buf,true,1.0f,1.0f,1);
				offset += 0.02f;
			}
		}
	}

	if(sm_DrawScriptStream && m_StreamSound)
	{
		GtaThread* streamThread = static_cast<GtaThread*>(scrThread::GetThread(m_Scripts[sm_ScriptInChargeOfTheStream].ScriptThreadId));
		const char *streamScriptName = streamThread ? streamThread->GetScriptName() : "none"; 
		formatf(buf, "Script: %s, Stream : %s, script idx : %d, type %d",streamScriptName,m_StreamSound->GetName(),sm_ScriptInChargeOfTheStream, m_StreamType);
		grcDebugDraw::Text(Vector2(0.05f, offset), Color32(255,255,255),buf,true,1.0f,1.0f,1);
		offset += 0.02f;
	}

	//Script sounds log
	if(sm_LogScriptTriggeredSounds && m_FileHandle == INVALID_FILE_HANDLE)
	{

		m_FileHandle = CFileMgr::OpenFileForWriting("common:/ScriptTriggeredSoundsLog.csv");

		if(m_FileHandle != INVALID_FILE_HANDLE)
		{
			const char *header = "Time,Name,Sound,Sound idx,script idx\r\n";
			CFileMgr::Write(m_FileHandle, header, istrlen(header));
			CFileMgr::CloseFile(m_FileHandle);
			m_FileHandle = CFileMgr::OpenFileForAppending("common:/ScriptTriggeredSoundsLog.csv");
		}
	}

	for(u32 soundIdx = 0; soundIdx < AUD_MAX_SCRIPT_SOUNDS; soundIdx++)
	{
		if(m_ScriptSounds[soundIdx].Sound)
		{
			const char * scriptName = "none";
			audScript * script = GetScript(m_ScriptSounds[soundIdx].ScriptIndex);
			if(script)
			{
				GtaThread* pThread = static_cast<GtaThread*>(scrThread::GetThread(script->ScriptThreadId));
				scriptName = pThread ? pThread->GetScriptName() : "none"; 
			}
			if(sm_DrawScriptTriggeredSounds)
			{
				audScriptSound scriptSound = m_ScriptSounds[soundIdx];

				if(scriptSound.SoundSetHash)
				{
					formatf(buf, "Script: %s, Soundset: %s, Field: %s, Sound: %s, Sound Idx: %d, Script Idx: %d",scriptName, m_ScriptSounds[soundIdx].SoundSetTriggeredName, m_ScriptSounds[soundIdx].SoundTriggeredName, m_ScriptSounds[soundIdx].Sound->GetName(),soundIdx, scriptSound.ScriptIndex);
				}
				else
				{
					formatf(buf, "Script: %s, Sound:%s, Sound Idx: %d, Script Idx: %d",scriptName, m_ScriptSounds[soundIdx].Sound->GetName(), soundIdx, scriptSound.ScriptIndex);
				}

				grcDebugDraw::Text(Vector2(0.05f, offset), Color32(255,255,255),buf,true,1.0f,1.0f,1);
				offset += 0.02f;
			}
			if(sm_InWorldDrawScriptTriggeredSounds)
			{
				bool found = true;
				if(sm_FilterScriptTriggeredSounds)
				{
					found = false;
					naDisplayf("sound name :%s text :%s ",m_ScriptSounds[soundIdx].Sound->GetName(),sm_ScriptTriggerOverride);
					if(atStringHash(m_ScriptSounds[soundIdx].Sound->GetName()) == soundHash)
					{
						found = true;
					}
					if (soundIdx == (u32)strtol(sm_ScriptTriggerOverride,NULL,0))
					{
						found = true;
					}
				}
				if(found)
				{
					audScriptSound scriptSound = m_ScriptSounds[soundIdx];
					formatf(buf, "Script: %s, SoundTriggeredName : %s, Soundset: %s , Sound: %s sound idx : %d, script idx : %d",scriptName, m_ScriptSounds[soundIdx].SoundTriggeredName, m_ScriptSounds[soundIdx].SoundSetTriggeredName,m_ScriptSounds[soundIdx].Sound->GetName(),soundIdx, scriptSound.ScriptIndex);
					grcDebugDraw::Text(m_ScriptSounds[soundIdx].Sound->GetRequestedPosition(), Color32(255, 255, 255, 255),buf,true,1);
				}
			}

			if(m_FileHandle != INVALID_FILE_HANDLE && m_ScriptSounds[soundIdx].Sound)
			{
				audScriptSound scriptSound = m_ScriptSounds[soundIdx];
				formatf(buf, "%f,%s,%s,%s,%s,%d,%d\r\n",fwTimer::GetTimeInMilliseconds(),scriptName
					,m_ScriptSounds[soundIdx].SoundTriggeredName, m_ScriptSounds[soundIdx].SoundSetTriggeredName,m_ScriptSounds[soundIdx].Sound->GetName(),soundIdx, scriptSound.ScriptIndex);

				CFileMgr::Write(m_FileHandle, buf, istrlen(buf));
			}
		}
	}	
	if(!sm_LogScriptTriggeredSounds && m_FileHandle != INVALID_FILE_HANDLE)
	{
		CFileMgr::CloseFile(m_FileHandle);
		m_FileHandle = INVALID_FILE_HANDLE;
	}	

	if(g_DisplayScriptedLineInfo && sm_ScriptedLineDebugInfo[0] != '\0')
	{
		grcDebugDraw::Text(Vector2(0.05f, 0.8f), Color32(255,255,255),sm_ScriptedLineDebugInfo,true,1.0f,1.0f,1);
	}	

	if(g_DisplayScriptSoundIdInfo)
	{
		f32 yCoord = 0.02f - g_ScriptSoundIdInfoScroll;
		grcDebugDraw::Text(Vector2(0.05f, yCoord), Color_white, "Slot", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.1f, yCoord), Color_white, "Owner", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.25f, yCoord), Color_white, "Sound", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.4f, yCoord), Color_white, "Sound State", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.55f, yCoord), Color_white, "Age", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.7f, yCoord), Color_white, "Frame Allocated", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.85f, yCoord), Color_white, "Script Has Ref", true, 1.f, 1.f, -1);
		yCoord += 0.04f;

		const char* soundStates[AUD_SOUND_STATE_MAX + 1] = {"Dormant","Preparing","Playing","Waiting to be Deleted","Max"};

		for (u32 scriptSoundIndex = 0; scriptSoundIndex < AUD_MAX_SCRIPT_SOUNDS; scriptSoundIndex++)
		{
			// This is the logic used in FindFreeSoundId, so invert it to find all used slots
			if (!(m_ScriptSounds[scriptSoundIndex].ScriptIndex == -1 && m_ScriptSounds[scriptSoundIndex].NetworkId == audScriptSound::INVALID_NETWORK_ID))
			{							
				formatf(buf, "%d", scriptSoundIndex);
				grcDebugDraw::Text(Vector2(0.05f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

				formatf(buf, "%s", m_ScriptSounds[scriptSoundIndex].ScriptName);
				grcDebugDraw::Text(Vector2(0.1f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

				formatf(buf, m_ScriptSounds[scriptSoundIndex].SoundRef.IsValid() ? g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_ScriptSounds[scriptSoundIndex].SoundRef) : "Invalid");				
				grcDebugDraw::Text(Vector2(0.25f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

				formatf(buf, "%s", m_ScriptSounds[scriptSoundIndex].Sound? soundStates[m_ScriptSounds[scriptSoundIndex].Sound->GetPlayState()] : "No Sound");
				grcDebugDraw::Text(Vector2(0.4f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

				formatf(buf, "%.02f", (fwTimer::GetTimeInMilliseconds() - m_ScriptSounds[scriptSoundIndex].TimeAllocated) * 0.001f);
				grcDebugDraw::Text(Vector2(0.55f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

				formatf(buf, "%u", m_ScriptSounds[scriptSoundIndex].FrameAllocated);
				grcDebugDraw::Text(Vector2(0.7f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

				formatf(buf, "%s", m_ScriptSounds[scriptSoundIndex].ScriptHasReference? "true" : "false");
				grcDebugDraw::Text(Vector2(0.85f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);
			}

			yCoord += 0.02f;
		}
	}

	if(g_DisplayScriptBankUsage)
	{
		f32 yCoord = 0.02f - g_ScriptSoundIdInfoScroll;

		grcDebugDraw::Text(Vector2(0.05f, yCoord), Color_white, "Slot", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.1f, yCoord), Color_white, "Bank", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.3f, yCoord), Color_white, "State", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.4f, yCoord), Color_white, "Ref Count", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.5f, yCoord), Color_white, "Network Script A", true, 1.f, 1.f, -1);
		grcDebugDraw::Text(Vector2(0.7f, yCoord), Color_white, "Network Script B", true, 1.f, 1.f, -1);
		yCoord += 0.04f;

		for (s32 i=0; i<AUD_MAX_SCRIPT_BANK_SLOTS; i++)
		{
			formatf(buf, "%d", i);
			grcDebugDraw::Text(Vector2(0.05f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

			if(m_BankSlots[i].BankId != AUD_INVALID_BANK_ID)
			{
				formatf(buf, "%s", audWaveSlot::GetBankName(m_BankSlots[i].BankId));
				grcDebugDraw::Text(Vector2(0.1f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

				static const char* stateNames[3] = { "Queued", "Loading", "Loaded" };
				formatf(buf, "%s", stateNames[m_BankSlots[i].State]);				
				grcDebugDraw::Text(Vector2(0.3f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);
			}

			formatf(buf, "%d", m_BankSlots[i].ReferenceCount);
			grcDebugDraw::Text(Vector2(0.4f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);

			if(m_BankSlots[i].NetworkScripts[0].IsValid())
			{
				formatf(buf, "%s (%u)", m_BankSlots[i].NetworkScripts[0].GetScriptName(), (u32)m_BankSlots[i].NetworkScripts[0].GetScriptNameHash());
				grcDebugDraw::Text(Vector2(0.5f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);
			}

			if(m_BankSlots[i].NetworkScripts[1].IsValid())
			{
				formatf(buf, "%s (%u)", m_BankSlots[i].NetworkScripts[1].GetScriptName(), (u32)m_BankSlots[i].NetworkScripts[1].GetScriptNameHash());
				grcDebugDraw::Text(Vector2(0.7f, yCoord), Color_white, buf, true, 1.f, 1.f, -1);
			}

			yCoord += 0.02f;
		}
	}

	if(g_DrawScriptSpeechSlots)
	{
		char str[1024];
		Color32 color;
		f32 yPos = 0.3f;
		f32 yInc = 0.025f;
		f32 xPos = 0.4f;

		formatf(str, sizeof(str), "SCRIPT SPEECH SLOTS");
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_purple, str);
		yPos += yInc;

		for (s32 currentPreloadSet=0; currentPreloadSet<2; currentPreloadSet++)
		{
			for (s32 i=0; i<AUD_MAX_SCRIPTED_SPEECH_SLOTS_TO_LOAD; i++)
			{
				{
					sprintf(str, "ScriptedSpeechSlotInfo %d %d Waveslot: %s, LoadedWave: %u, context: %s, voicename: %s",
						currentPreloadSet, i, 
						m_ScriptedSpeechSlot[i][currentPreloadSet].WaveSlot->GetSlotName(),
						m_ScriptedSpeechSlot[i][currentPreloadSet].WaveSlot->GetLoadedWaveNameHash(),
						m_ScriptedSpeechSlot[i][currentPreloadSet].Context,
						m_ScriptedSpeechSlot[i][currentPreloadSet].VoiceName
						);
					grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, str);
					yPos += yInc;

					if (m_ScriptedSpeechSlot[i][currentPreloadSet].ScriptedSpeechSound)
					{
						sprintf(str, "SoundInfo %d %d Waveslot: %p, WaveNameHash: %u BankId: %u",
							currentPreloadSet, i, 
							m_ScriptedSpeechSlot[i][currentPreloadSet].ScriptedSpeechSound->GetWaveSlot()->GetSlotName(), 
							m_ScriptedSpeechSlot[i][currentPreloadSet].ScriptedSpeechSound->GetWaveNameHash(),
							m_ScriptedSpeechSlot[i][currentPreloadSet].ScriptedSpeechSound->GetBankId());
						grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, str);
					}
					yPos += yInc;

				}
			}
		}
	}
}

void audScriptAudioEntity::DumpScriptSoundIdUsage()
{	
	char str[1024];
	FileHandle fileHandle = CFileMgr::OpenFileForWriting("common:/ScriptSoundIdUsage.csv");

	if(fileHandle != INVALID_FILE_HANDLE)
	{
		const char *header = "Slot,Owner,Sound,State,Age,Frame Allocated,Script Has Ref\r\n";
		CFileMgr::Write(fileHandle, header, istrlen(header));
		const char* soundStates[AUD_SOUND_STATE_MAX + 1] = {"Dormant","Preparing","Playing","Waiting to be Deleted","Max"};

		for (u32 scriptSoundIndex = 0; scriptSoundIndex < AUD_MAX_SCRIPT_SOUNDS; scriptSoundIndex++)
		{
			// This is the logic used in FindFreeSoundId, so invert it to find all used slots
			if (!(m_ScriptSounds[scriptSoundIndex].ScriptIndex == -1 && m_ScriptSounds[scriptSoundIndex].NetworkId == audScriptSound::INVALID_NETWORK_ID))
			{
				formatf(str, "%d,%s,%s,%s,%.02f,%u,%s\r\n", 
					scriptSoundIndex, 
					m_ScriptSounds[scriptSoundIndex].ScriptName, 
					m_ScriptSounds[scriptSoundIndex].SoundRef.IsValid() ? g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(m_ScriptSounds[scriptSoundIndex].SoundRef) : "Invalid",
					m_ScriptSounds[scriptSoundIndex].Sound? soundStates[m_ScriptSounds[scriptSoundIndex].Sound->GetPlayState()] : "No Sound",
					(fwTimer::GetTimeInMilliseconds() - m_ScriptSounds[scriptSoundIndex].TimeAllocated) * 0.001f,
					m_ScriptSounds[scriptSoundIndex].FrameAllocated,
					m_ScriptSounds[scriptSoundIndex].ScriptHasReference? "true" : "false"
					);

				CFileMgr::Write(fileHandle, str, istrlen(str));
			}
		}

		audDisplayf("Successfully dumped script sound usage to common:/ScriptSoundIdUsage.csv");
		CFileMgr::CloseFile(fileHandle);
	}
	else
	{
		audErrorf("Failed to open common:/ScriptSoundIdUsage.csv");
	}
}

void MissionCompleteCB()
{
	g_ScriptAudioEntity.TriggerMissionComplete("FRANKLIN_BIG_01");
}

void TriggerAmbientSpeechCB()
{
	g_ScriptAudioEntity.SayAmbientSpeechFromPosition(g_AmbientSpeechDebugContext, atStringHash(g_AmbientSpeechDebugVoice), 
													Vector3(g_AmbientSpeechDebugPosX, g_AmbientSpeechDebugPosY, g_AmbientSpeechDebugPosZ), 
													g_AmbientSpeechDebugParams);
}

void audScriptAudioEntity::ForceLowInterrupt()
{
	if(m_Ped[g_InterruptSpeakerNum])
	{
		RegisterScriptRequestedInterrupt(m_Ped[g_InterruptSpeakerNum],"", "",false, true);
	}
}

void audScriptAudioEntity::ForceMediumInterrupt()
{
	if(m_Ped[g_InterruptSpeakerNum])
	{
		RegisterScriptRequestedInterrupt(m_Ped[g_InterruptSpeakerNum],"", "",false, true);
	}
}

void audScriptAudioEntity::ForceHighInterrupt()
{
	if(m_Ped[g_InterruptSpeakerNum])
	{
		RegisterScriptRequestedInterrupt(m_Ped[g_InterruptSpeakerNum],"", "",false, true);
	}
}

void audScriptAudioEntity::DebugEnableFlag(const char *flagName)
{
	// Note - using the string version here only to test the map; code interaction with flags
	// should use the flag ids directly.
	const audScriptAudioFlags::FlagId flagId = audScriptAudioFlags::FindFlagId(flagName);
	g_ScriptAudioEntity.SetFlagState(flagId, true);
}

void audScriptAudioEntity::DebugDisableFlag(const char *flagName)
{
	// Note - using the string version here only to test the map; code interaction with flags
	// should use the flag ids directly.
	const audScriptAudioFlags::FlagId flagId = audScriptAudioFlags::FindFlagId(flagName);
	g_ScriptAudioEntity.SetFlagState(flagId, false);
}

void audScriptAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("audScriptAudioEntity",false);
		bank.AddToggle("Display Script Sound ID Usage", &g_DisplayScriptSoundIdInfo);
		bank.AddToggle("Display Script Bank Usage", &g_DisplayScriptBankUsage);
		bank.AddSlider("Sound ID Usage Display Scroll", &g_ScriptSoundIdInfoScroll, 0, 1.5f, 0.1f);	
		bank.AddButton("Dump Script Sound Id Usage (common:/ScriptSoundIdUsage.csv)", datCallback(MFA(audScriptAudioEntity::DumpScriptSoundIdUsage), &g_ScriptAudioEntity));
		bank.AddButton("Log Script Bank Usage", datCallback(MFA(audScriptAudioEntity::LogScriptBanks), &g_ScriptAudioEntity));
		bank.AddButton("TriggerMissionComplete", CFA(MissionCompleteCB));
		bank.AddToggle("Debug Replay Scripted Speech", &sm_DrawReplayScriptSpeechSlots);
		bank.AddButton("Debug Scripted Conv NOW", datCallback(CFA(DebugScriptedConversationStatic)));
		bank.AddToggle("Debug scripted convs", &g_DebugScriptedConvs);
		bank.AddToggle("g_DisplayScriptedLineInfo", &g_DisplayScriptedLineInfo);
		bank.AddSlider("g_MinRagdollTimeBeforeConvPause", &g_MinRagdollTimeBeforeConvPause, 0, 5000, 10);
		bank.AddSlider("g_MinNotRagdollTimeBeforeConvRestart", &g_MinNotRagdollTimeBeforeConvRestart, 0, 5000, 10);
		bank.AddToggle("g_AnimTriggersControllingConversation", &g_AnimTriggersControllingConversation);
		bank.AddToggle("g_FakeConvAnimTrigger", &g_FakeConvAnimTrigger);
		bank.AddToggle("g_DuckForConvAfterPreload", &g_DuckForConvAfterPreload);
		bank.AddToggle("g_ConversationDebugSpew", &g_ConversationDebugSpew);
		bank.AddToggle("Force Mobile Interference", &g_ForceMobileInterference);
		bank.AddToggle("g_AdjustOverlapForLipsyncPredelay", &g_AdjustOverlapForLipsyncPredelay);
		bank.AddToggle("g_AdjustTriggerTimeForZeroPlaytime", &g_AdjustTriggerTimeForZeroPlaytime);
		bank.AddToggle("g_AdjustTriggerForZeroEvenIfMarkedOverlap", &g_AdjustTriggerForZeroEvenIfMarkedOverlap);
		bank.AddSlider("g_AdditionalScriptedSpeechPause", &g_AdditionalScriptedSpeechPause, 0, 1000, 10);
		bank.AddToggle("g_TrackMissingDialogue", &g_TrackMissingDialogue);
		bank.AddToggle("g_TrackMissingDialogueMP", &g_TrackMissingDialogueMP);
		bank.AddSlider("g_MinLowPainBikeReaction", &g_MinLowPainBikeReaction, 0.0f, 100.0f, 0.01f);
		bank.AddToggle("g_EnableAircraftWarningSpeech", &g_EnableAircraftWarningSpeech);
		bank.PushGroup("Conversation Interrupts",true);
			bank.AddButton("Force Low Interrupt", datCallback(MFA(audScriptAudioEntity::ForceLowInterrupt), &g_ScriptAudioEntity));
			bank.AddButton("Force Medium Interrupt", datCallback(MFA(audScriptAudioEntity::ForceMediumInterrupt), &g_ScriptAudioEntity));
			bank.AddButton("Force High Interrupt", datCallback(MFA(audScriptAudioEntity::ForceHighInterrupt), &g_ScriptAudioEntity));
			bank.AddSlider("Player Speaker Number", &g_InterruptSpeakerNum, 0, 8, 1);
			bank.AddToggle("g_DisableInterrupts", &g_DisableInterrupts);
			bank.AddToggle("g_ForceScriptedInterrupts", &g_ForceScriptedInterrupts);
			bank.AddToggle("g_DisableAirborneInterrupts", &g_DisableAirborneInterrupts);
			bank.AddToggle("g_AllowMuliWantedInterrupts", &g_AllowMuliWantedInterrupts);
			bank.AddSlider("g_MinTimeCarInAirForInterrupt", &g_MinTimeCarInAirForInterrupt, 0, 20000, 10);
			bank.AddSlider("g_MinTimeCarOnGroundForInterrupt", &g_MinTimeCarOnGroundForInterrupt, 0, 20000, 10);
			bank.AddSlider("g_MinTimeCarOnGroundToRestart", &g_MinTimeCarOnGroundToRestart, 0, 20000, 10);
			bank.AddSlider("g_MinAirBorneTimeToKillConversation", &g_MinAirBorneTimeToKillConversation, 0, 20000, 10);
			bank.AddSlider("g_MaxAirBorneTimeToApplyRestartOffset", &g_MaxAirBorneTimeToApplyRestartOffset, 0, 20000, 10);
			bank.AddSlider("g_MinCarRollForInterrupt", &g_MinCarRollForInterrupt, -1.0f, 0.0f, 0.01f);
			bank.AddSlider("g_MinLowReactionDamage", &g_MinLowReactionDamage, 0.0f, 200.0f, 1.0f);
			bank.AddSlider("g_MinMediumReactionDamage", &g_MinMediumReactionDamage, 0.0f, 200.0f, 1.0f);
			bank.AddSlider("g_MinHighReactionDamage", &g_MinHighReactionDamage, 0.0f, 200.0f, 1.0f);
			bank.AddSlider("g_TimeToPauseConvOnCollision", &g_TimeToPauseConvOnCollision, 0, 10000, 50);
			bank.AddSlider("g_MinTimeBetweenCollisionReactions", &g_MinTimeBetweenCollisionReactions, 0, 10000, 50);
			bank.AddSlider("g_MinTimeBetweenCollisionTriggers", &g_MinTimeBetweenCollisionTriggers, 0, 10000, 50);
			bank.AddSlider("g_TimeToWaitForInterruptToPrepare", &g_TimeToWaitForInterruptToPrepare, 0, 5000, 50);
			bank.AddToggle("g_ForceNonPlayerInterrupt", &g_ForceNonPlayerInterrupt);
			bank.AddText("Low Interrupt Context", g_szLowInterruptContext, sizeof(g_szLowInterruptContext));
			bank.AddText("Medium Interrupt Context", g_szMediumInterruptContext, sizeof(g_szLowInterruptContext));
			bank.AddText("High Interrupt Context", g_szHighInterruptContext, sizeof(g_szLowInterruptContext));
		bank.PopGroup();
		bank.PushGroup("Ambient speech", true);
			bank.AddButton("TriggerMissionComplete", CFA(TriggerAmbientSpeechCB));
			bank.AddText("g_AmbientSpeechDebugContext", &g_AmbientSpeechDebugContext[0], sizeof(g_AmbientSpeechDebugContext));
			bank.AddText("g_AmbientSpeechDebugVoice", &g_AmbientSpeechDebugVoice[0], sizeof(g_AmbientSpeechDebugVoice));
			bank.AddText("g_AmbientSpeechDebugParams", &g_AmbientSpeechDebugParams[0], sizeof(g_AmbientSpeechDebugParams));
			bank.AddSlider("g_AmbientSpeechDebugPosX", &g_AmbientSpeechDebugPosX, -10000.0f, 10000.0f, 0.1f);
			bank.AddSlider("g_AmbientSpeechDebugPosY", &g_AmbientSpeechDebugPosY, -10000.0f, 10000.0f, 0.1f);
			bank.AddSlider("g_AmbientSpeechDebugPosZ", &g_AmbientSpeechDebugPosZ, -1000.0f, 1000.0f, 0.1f);
		bank.PopGroup();
		bank.PushGroup("Debug script sounds",true);
			bank.AddToggle("Screen debug playing script-triggered sounds", &sm_DrawScriptTriggeredSounds);
			bank.AddToggle("In world debug playing script-triggered sounds", &sm_InWorldDrawScriptTriggeredSounds);
			bank.AddToggle("Screen debug playing script-stream sound", &sm_DrawScriptStream);
			bank.AddToggle("Log script-triggered sounds playing.", &sm_LogScriptTriggeredSounds);
			bank.AddToggle("Show script banks loaded.", &sm_DrawScriptBanksLoaded);
			bank.AddToggle("Filter script-triggered sounds", &sm_FilterScriptTriggeredSounds);
			bank.AddText("By sound id or sound name", sm_ScriptTriggerOverride, 64, false);
		bank.PopGroup();

		bank.PushGroup("Script audio flags", true);
			for(s32 i = 0; i < audScriptAudioFlags::kNumScriptAudioFlags; i++)
			{
				char buttonName[128];
				const audScriptAudioFlags::FlagId flagId = (audScriptAudioFlags::FlagId)i;
				
				formatf(buttonName, "Enable %s", audScriptAudioFlags::GetName(flagId));
				bank.AddButton(buttonName, datCallback(CFA1(audScriptAudioEntity::DebugEnableFlag), (void*)audScriptAudioFlags::GetName(flagId)));
				formatf(buttonName, "Disable %s", audScriptAudioFlags::GetName(flagId));
				bank.AddButton(buttonName, datCallback(CFA1(audScriptAudioEntity::DebugDisableFlag), (void*)audScriptAudioFlags::GetName(flagId)));
			
			}
			bank.PushGroup("Enable counts");
			for(s32 i = 0; i < audScriptAudioFlags::kNumScriptAudioFlags; i++)
			{
				bank.AddText(audScriptAudioFlags::GetName((audScriptAudioFlags::FlagId)i), &g_ScriptAudioEntity.m_FlagState[i], true);
			}
			bank.PopGroup();
		bank.PopGroup();
	bank.PopGroup();
}
#endif

/******************************************************
 * audScript
 *****************************************************/

audScene* audScript::GetScene(u32 sceneHash)
{
	audScriptScene * scriptScene = scenes;

	while(scriptScene)
	{
		if(scriptScene->m_scene && scriptScene->m_scene->IsInstanceOf(sceneHash))
		{
			return scriptScene->m_scene;
		}

		scriptScene = scriptScene->m_next;
	}
	return NULL;
}

bool audScript::StartScene(u32 sceneHash) 
{
	MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(sceneHash);

	audScene * existingScene = GetScene(sceneHash);
	if(existingScene)
	{
		return true;
	}

	audScriptScene * sScene = scenes;
	audScriptScene * prevSS = NULL;

	while(sScene)
	{
		if(!sScene->m_scene)
		{
			//Found a slot that used to contain a scene but is vacant so use it
			break;
		}

		prevSS = sScene;
		sScene = sScene->m_next;
	}

	if(!sScene) //We've reached the end of the list so add a new slot on
	{
		sScene = rage_new audScriptScene(); 
	}

	if(prevSS)
	{
		prevSS->m_next = sScene;
	}
	else
	{
		//Must be at the head
		scenes = sScene;
	}

	//Tell the dynamic mixer to kick off this scene and pass through m_scene 
	//to pick up the reference

	bool ready = true;
	if(sceneSettings)
	{
		DYNAMICMIXER.StartScene(sceneSettings, &sScene->m_scene, this);
	
#if GTA_REPLAY
		if(sScene && sScene->m_scene)
		{
			sScene->m_scene->SetHash(sceneHash);

			if(CReplayMgr::ShouldRecord())
			{
				if(sceneHash == ATSTRINGHASH("DEATH_SCENE", 0x5A583A13) || sceneHash == ATSTRINGHASH("QUIET_SCENE", 0x85BA09E8))
				{
					CPed* pPlayerPed = FindPlayerPed();
					bool isPlayerDead = pPlayerPed && pPlayerPed->ShouldBeDead();
					bool isPlayerArrested = pPlayerPed && pPlayerPed->GetIsArrested();
					if(isPlayerDead || isPlayerArrested)
					{
						return ready;
					}
				}
				CReplayMgr::RecordPersistantFx<CPacketDynamicMixerScene>(
					CPacketDynamicMixerScene(sceneHash, false, 0),
					CTrackedEventInfo<tTrackedSceneType>(sScene->m_scene), NULL, true); 
			}
		}
#endif //GTA_REPLAY
	}
	
	return ready;
}

void audScript::StopScene(u32 sceneHash)
{
	audScene* scene = GetScene(sceneHash);
	
	if(scene)
	{
		scene->Stop();
	}
}

void audScript::StopScenes()
{
	while(scenes)
	{
		if(scenes->m_scene)
		{
			if(scenes->m_scene->IsInstanceOf(g_ScriptAudioEntity.GetPoliceScannerSceneHash()))
			{
				g_ScriptAudioEntity.ResetPoliceScannerVariables();
			}

			scenes->m_scene->Stop();
		}

		audScriptScene * next = scenes->m_next;
		delete scenes;
		scenes = next;
	}
}

void audScript::SetSceneVariable(u32 sceneHash, u32 variableHash, f32 value)
{
	audScene* scene = GetScene(sceneHash);

	if(scene)
	{
		scene->SetVariableValue(variableHash, value);
	}
}

bool audScript::SceneHasFinishedFade(u32 sceneHash)
{
	audScene * scene = GetScene(sceneHash);
	if(scene)
	{
		return scene->HasFinishedFade();
	}

	return true;
}

//Functions for manipulating script-scoped variables
const variableNameValuePair * audScript::GetVariableAddress(u32 nameHash) const
{
	//Loop through our own variable block to see if it's one of ours
	for(s32 i=0; i < g_MaxAudScriptVariables; i++)
	{
		if(m_Variables[i].hash == nameHash)
		{
			//We own the variable it's asking about, so return its address
			return &(m_Variables[i]);
		}
		else if(m_Variables[i].hash == 0)
		{
			return NULL;
		}
	}
	return NULL;
}

//const variableNameValuePair * audScript::GetVariableAddress(const char * name)
//{
//	return GetVariableAddress(atStringHash(name));
//}
//
void audScript::SetVariableValue(u32 nameHash, f32 value)
{
	//Look through our variable block to see if it exists already
	for(s32 i=0; i < g_MaxAudScriptVariables; i++)
	{
		if(m_Variables[i].hash == nameHash)
		{
			//the variable exists, so set it
			m_Variables[i].value = value;
			return;
		}
		else if (m_Variables[i].hash == 0)
		{
			//we've got the first empty one, so set this one to be the new variable
			//and bail out
			m_Variables[i].hash = nameHash;
			m_Variables[i]. value = value;
			return;
		}
	}
	naAssertf(0, "An audScript has run out of variables; consider using less variables or upping the max number of variables");
}

void audScript::SetVariableValue(const char * name, float value)
{
	SetVariableValue(atStringHash(name), value);
}

void audScript::Shutdown()
{
	ReleaseScriptWeaponAudio();
	StopScenes();

	g_AmbientAudioEntity.CancelScriptPedDensityChanges(ScriptThreadId);
	g_AmbientAudioEntity.CancelScriptWallaSoundSet(ScriptThreadId);

	for(s32 ambientZoneLoop = 0; ambientZoneLoop < m_ScriptAmbientZoneChanges.GetCount(); ambientZoneLoop++)
	{
		g_AmbientAudioEntity.ClearZoneNonPersistentStatus(m_ScriptAmbientZoneChanges[ambientZoneLoop], true);
	}

	m_ScriptAmbientZoneChanges.clear();

	for(int i=0; i<m_PedsSetToAngry.GetCount(); ++i)
	{
		if(m_PedsSetToAngry[i])
			m_PedsSetToAngry[i]->SetPedIsAngry(false);
	}
	m_PedsSetToAngry.clear();

	for(int i=0; i<m_PriorityVehicles.GetCount(); i++)
	{
		if(m_PriorityVehicles[i])
		{
			m_PriorityVehicles[i]->SetScriptPriority(AUD_VEHICLE_SCRIPT_PRIORITY_NONE, ScriptThreadId);
		}
	}
	m_PriorityVehicles.clear();

	audSpeechAudioEntity::RemoveContextBlocks(ScriptThreadId);

	s32 scriptIndex = g_ScriptAudioEntity.GetIndexFromScriptThreadId(ScriptThreadId);

	scenes = NULL;
	if(scriptIndex == audScriptAudioEntity::sm_ScriptInChargeOfTheStream)
	{
		g_ScriptAudioEntity.StopStream();
	}

	if(scriptIndex == g_SpeechManager.GetTennisScriptId())
	{
		g_SpeechManager.UnrequestTennisBanks();
	}

	if(m_FlagResetState.AreAnySet())
	{
		for(s32 i = 0; i < audScriptAudioFlags::kNumScriptAudioFlags; i++)
		{
			if(m_FlagResetState.IsSet(i))
			{
				g_ScriptAudioEntity.ResetFlagState((audScriptAudioFlags::FlagId)i);
			}
		}
	}
	
	if(m_SlowMoMode)
	{
		audWarningf("Automatically deactivating slow-mo mode %u for script", m_SlowMoMode);
		audNorthAudioEngine::DeactivateSlowMoMode(m_SlowMoMode);
		m_SlowMoMode = 0;
	}

	if(m_ScriptVehicleHash != 0u)
	{
		audVehicleAudioEntity::PreloadScriptVehicleAudio(0u, scriptIndex);
		m_ScriptVehicleHash = 0u;
	}

	// Ditch any pending music events owned by this script.  If this shows up in a profile then we'll need
	// to flag internally whether this is necessary for each script instance; there should only ever be a few
	// active events, so it will hopefully be cheap enough to ignore.
	g_InteractiveMusicManager.GetEventManager().CancelAllEvents(ScriptThreadId);
 
	// We're done - clear out the slot
	ScriptThreadId = THREAD_INVALID;
	BankSlotIndex = 0;
	BankOverNetwork = 0;
	PlayerBits = AUD_NET_ALL_PLAYERS;

	m_ScriptisShuttingDown = false;
}
extern audScannerManager g_AudioScannerManager;

void audScriptAudioEntity::OnFlagStateChange(audScriptAudioFlags::FlagId flagId, const bool state)
{
#if __BANK
	audDisplayf("Script audio flag %s now %s (from %s)", audScriptAudioFlags::GetName(flagId), state ? "enabled" : "disabled", CTheScripts::GetCurrentScriptNameAndProgramCounter());
#endif
	// If you want to kick off some custom logic when a script audio flag changes state, add a handler here.
	switch(flagId)
	{
	case audScriptAudioFlags::RadioOverlapDisabled:
		audRadioStation::DisableOverlappedTracks(state);
		break;
	case audScriptAudioFlags::PoliceScannerDisabled:
		g_AudioScannerManager.DisableScannerFlagChanged(state);
	default:
		// Not all flags will need to state changes - ie where code is happy to poll IsFlagSet()
		break;
	}
}

void audScriptAudioEntity::SetFlagState(const audScriptAudioFlags::FlagId flagId, const bool state)
{
	const bool prevState = IsFlagSet(flagId);
	if(state)
	{
		m_FlagState[flagId]++;
	}
	else
	{
		if(audVerifyf(prevState, "script trying to disable an audio flag (%d / %s) that is not currently set (from %s)", flagId, audScriptAudioFlags::GetName(flagId), CTheScripts::GetCurrentScriptNameAndProgramCounter()))
		{
			m_FlagState[flagId]--;
		}
	}

	const bool newState = IsFlagSet(flagId);
	if(newState != prevState)
	{
		Assert(state == newState);
		OnFlagStateChange(flagId, state);
	}
}

void audScript::SetAudioFlag(const char *flagName, const bool state)
{
	const audScriptAudioFlags::FlagId flagId = audScriptAudioFlags::FindFlagId(flagName);
	if(audScriptAudioFlags::IsValid(flagId))
	{
		return SetAudioFlag(flagId, state);
	}
}

void audScript::SetAudioFlag(const audScriptAudioFlags::FlagId flagId, const bool state)
{
	const bool prevState = m_FlagResetState.IsSet(flagId);
	// Silently ignore multiple requests for the same state
	if(state != prevState)
	{
		g_ScriptAudioEntity.SetFlagState(flagId, state);
		m_FlagResetState.Set(flagId, state);
	}
}

void audScript::ActivateSlowMoMode(const char *mode)
{
	const u32 newMode = atStringHash(mode);

	if(newMode == m_SlowMoMode)
	{
		return;
	}
	else if(!audVerifyf(m_SlowMoMode == 0, "Script activating a slow-mo mode %s without first deactivating %u", mode, m_SlowMoMode))
	{
		audNorthAudioEngine::DeactivateSlowMoMode(m_SlowMoMode);	
	}

	m_SlowMoMode = newMode;
	audNorthAudioEngine::ActivateSlowMoMode(newMode);
}

void audScript::DeactivateSlowMoMode(const char *mode)
{
	const u32 newMode = atStringHash(mode);

	if(newMode == m_SlowMoMode)
	{
		audNorthAudioEngine::DeactivateSlowMoMode(newMode);
		m_SlowMoMode = 0;
	}
	else
	{
		audWarningf("Script deactivating slow mo mode %s, but have set %u", mode, m_SlowMoMode);
		audNorthAudioEngine::DeactivateSlowMoMode(newMode);
	}
}
 
bool audScript::RequestScriptWeaponAudio(const char * itemName)
{
	const CItemInfo * info = CWeaponInfoManager::GetInfo(atStringHash(itemName));
	if(info)
	{
		m_ScriptWeaponAudioHash = info->GetAudioHash();

		return g_WeaponAudioEntity.AddScriptWeapon(info->GetAudioHash());
	}
	naAssertf(info, "SCRIPT: Couldn't find weapon %s in REQUEST_WEAPON_AUDIO", itemName);
	return true; //Can't find the weapon but return true to no block the script
}
void audScript::ReleaseScriptWeaponAudio()
{
	if(m_ScriptWeaponAudioHash)
	{
		g_WeaponAudioEntity.ReleaseScriptWeapon();
		m_ScriptWeaponAudioHash = 0;
	}
}

bool audScript::RequestScriptVehicleAudio(const u32 vehicleModelName)
{	
	if(audVehicleAudioEntity::PreloadScriptVehicleAudio(vehicleModelName, g_ScriptAudioEntity.GetIndexFromScriptThreadId(ScriptThreadId)))
	{
		m_ScriptVehicleHash = vehicleModelName;
		return true;
	}	
	else
	{
		return false;
	}
}
 
audScript::~audScript()
{
	Shutdown(); 
}



  
