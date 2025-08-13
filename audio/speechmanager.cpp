// 
// audio/speechmanager.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "northaudioengine.h"
#include "speechmanager.h"
#include "scriptaudioentity.h"
#include "control/replay/audio/SpeechAudioPacket.h"

#include "audiohardware/waveslot.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"

#include "speechaudioentity.h"
#include "virtcxtresolves.h"

#include "debug/debug.h"
#include "vehicles/vehicle.h"
#include "peds/ped.h"
#include "streaming/streamingengine.h"

#include "gameobjects.h"

#include "profile/profiler.h"

#include "debugaudio.h"

AUDIO_PEDS_OPTIMISATIONS()

audCurve g_VehicleOpennessToSpeechFilterCurve;
audCurve g_VehicleOpennessToSpeechVolumeCurve;

audCurve g_DistanceSqToPriorityScalarCurve;

#if __BANK
bool g_StressTestSpeech = false;
audSound *g_StressTestSpeechSound = NULL;
u32 g_StressTestSpeechVariation = 0;
char g_StressTestSpeechVoiceName[128] = {0};
char g_StressTestSpeechContext[128] = {0};
u32 g_StressTestMaxStartOffset = 0;

bool g_TestAnimalBankPriority = false;

bool g_DrawSpeechSlots = false;
bool g_DrawSpecificPainInfo = false;
bool g_DrawPainPlayedTotals = false;
bool g_DisplayAnimalBanks = false;
bool g_ForceAnimalBankUpdate = false;

bool g_OverrideSpecificPainLoaded = false;
u8 g_OverrideSpecificPainLevel = 0;
bool g_OverrideSpecificPainIsMale = false;
bool g_ForceSpecificPainBankSwap = false;

bool g_DebugTennisLoad = false;
bool g_DebugTennisUnload = false;

f32 g_LastRecordedPainValue;
atRangeArray<u32, AUD_PAIN_ID_NUM_TOTAL> g_DebugNumTimesPainTypePlayed;

extern bool g_UseTestPainBanks;
bool g_HavePreparedPainBanksForTesting = false;

bool g_PrintVoiceRefInfo = false;
#endif
u8 g_OverridePainVsAimScalar = 10;


audSpeechManager g_SpeechManager;

bool g_PrintUnknownContexts = false;

u32 g_NumPlayingAmbientSpeech = 0;
u32 g_StreamingIsBusy = 0;

u32 g_TimeBeforeWallaCanPlay = 1500;
u32 g_MaxWallaPlays = 15;
extern u32 g_WallaReuseTime;

//bool g_ChurnSpeech = true;
u32 g_LastTimeSpeechWasPlaying = 0;
u32 g_TimeBeforeChurningSpeech = 5000;
u32 g_LastTimeStreamingWasBusy = 0;
u32 g_StreamingBusyTime = 60;

u32 g_TimeBetweenSpecificPainBankSelection = 10000;
u32 g_TimeBetweenSpecificPainBankLoadAndSelection = 5000;
bool g_ActivatePainSpecificBankSwapping = true;

u32 g_MinTimeBetweenSameVariationOfPhoneCall = 180000;

extern bool g_StreamingSchmeaming;

extern audCategory *g_SpeechNormalCategory;

extern const u32 g_NoVoiceHash;

extern bool g_bSpeechAsserts;

extern const char* g_PainContexts[AUD_PAIN_ID_NUM_TOTAL];
extern const char* g_PlayerPainContexts[AUD_PAIN_ID_NUM_TOTAL];
extern u32 g_PainContextHashs[AUD_PAIN_ID_NUM_TOTAL];
extern u32 g_PainContextPHashs[AUD_PAIN_ID_NUM_TOTAL];
extern u32 g_PlayerPainContextHashs[AUD_PAIN_ID_NUM_TOTAL];
extern u32 g_PlayerPainContextPHashs[AUD_PAIN_ID_NUM_TOTAL];

f32 g_SpeechRolloffWhenInACar = 2.0f;

const u32 g_AnythingDullPHash = ATPARTIALSTRINGHASH("ANYTHING_DULL", 0x839929D9);

#define AUD_NUM_MICHAEL_PAIN_BANKS (4)
#define AUD_NUM_FRANKLIN_PAIN_BANKS (4)
#define AUD_NUM_TREVOR_PAIN_BANKS (4)
//#define AUD_NUM_MALE_PAIN_BANKS (3)
#define AUD_NUM_MALE_WEAK_PAIN_BANKS (4)
#define AUD_NUM_MALE_NORMAL_PAIN_BANKS (6)
#define AUD_NUM_MALE_TOUGH_PAIN_BANKS (5)
#define AUD_NUM_MALE_MIXED_PAIN_BANKS (9)
#define AUD_NUM_FEMALE_WEAK_PAIN_BANKS (0)
#define AUD_NUM_FEMALE_NORMAL_PAIN_BANKS (12)
#define AUD_NUM_FEMALE_MIXED_PAIN_BANKS (2)

const char* g_PainSlotType[4];
atRangeArray<const char*, AUD_PAIN_NUM_LEVELS> g_PainLevelString;
char g_PlayerPainRootBankName[32];

const char* g_BreathSlotType[AUD_BREATHING_TYPE_TOTAL];
u8 g_NumBreathingBanks[AUD_BREATHING_TYPE_TOTAL];
const char* g_BreathContexts[AUD_NUM_PLAYER_VFX_TYPES];

#define NUM_MINI_DRIVING_CONTEXTS 15
#define NUM_MINI_NON_DRIVING_CONTEXTS 9
#define NUM_GANG_CONTEXTS 52
#define NUM_LOUD_CONTEXTS 16 //25
#define NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS 1
u32 g_MiniDrivingContextHashes[NUM_MINI_DRIVING_CONTEXTS];
u32 g_MiniNonDrivingContextHashes[NUM_MINI_NON_DRIVING_CONTEXTS];
u32 g_GangContextHashes[NUM_GANG_CONTEXTS];
u32 g_LoudContextHashes[NUM_LOUD_CONTEXTS];
const char* g_MiniDrivingContexts[NUM_MINI_DRIVING_CONTEXTS] = {
	"RUN_FROM_FIGHT",
	"FIGHT",
	"DODGE",
	"BUMP",
	"GUN_RUN",
	"GUN_COOL",
	"TRAPPED",
	"VEHICLE_ATTACKED",
	"SURPRISED",
	"CRASH_CAR",
	"BLOCKED_VEHICLE",
	"BLOCKED_PED",
	"BLOCKED_GENERIC",
	"JACKED_IN_CAR",
	"JACK_CAR_BACK"
};

const char* g_MiniNonDrivingContexts[NUM_MINI_NON_DRIVING_CONTEXTS] = {
	"RUN_FROM_FIGHT",
	"FIGHT",
	"DODGE",
	"BUMP",
	"GUN_RUN",
	"GUN_COOL",
	"TRAPPED",
	"VEHICLE_ATTACKED",
	"SURPRISED"
};
const char* g_GangContexts[NUM_GANG_CONTEXTS] = {
	"ABUSE_DRIVER",
	"BEEN_SHOT_SUBMIT",
	"BLOCKED_PED",
	"BLOCKED_VEHICLE",
	"BUMP",
	"CONV_GANG_RESP",
	"CONV_GANG_STATE",
	"CONV_SMOKE_RESP",
	"CONV_SMOKE_STATE",
	"COVER_ME",
	"CRASH_BIKE",
	"CRASH_CAR",
	"CRASH_GENERIC",
	"DODGE",
	"DUCK",
	"FIGHT",
	"FOLLOWED_NO",
	"GANG_ASK_PLAYER_LEAVE",
	"GANG_ATTACK_WARNING",
	"GANG_BUMP",
	"GANG_CHASE",
	"GANG_DODGE_WARNING",
	"GANG_FIGHT_CHEER",
	"GANG_INTERVENE",
	"GANG_WATCH_THIS_GUY",
	"GANG_WATCH_THIS_GUY_RESP",
	"GANG_WATCH_THIS_GUY_SOLO",
	"GANG_WEAPON_WARNING",
	"GANG_YOU_DROP_WEAPON",
	"GENERIC_BYE",
	"GENERIC_FUCK_OFF",
	"GENERIC_HI",
	"GENERIC_NO",
	"GENERIC_YES",
	"INTIMIDATE_RESP",
	"JACKED_IN_CAR",
	"JACKED_ON_STREET",
	"JACKING_CAR_BACK",
	"JACKING_GENERIC_BACK",
	"MOVE_IN",
	"OVER_HANDLEBARS",
	"PLAYER_OVER_THERE",
	"PLAYER_UP_THERE",
	"SHIT",
	"SHOT_IN_LEG",
	"SHUT_UP_HORN",
	"SURROUNDED",
	"TAKE_COVER",
	"TARGET",
	"THANKS",
	"TRAPPED",
	"VEHICLE_ATTACKED"
};

const char* g_LoudContexts[] = {
	"GET_DOWN",
	"SURROUNDED" ,                            
	"MOVE_IN",
	"COVER_ME",
	"TARGET",
	"DUCK",
	"FALL_BACK",
	"TAKE_COVER",
	"ON_FIRE",
	"PANIC",
	"MEGAPHONE_FOOT_PURSUIT",
	"MEGAPHONE_PED_CLEAR_STREET",
	"PULL_OVER",
	"PULL_OVER_WARNING",
	"COP_HELI_MEGAPHONE",
	"COP_HELI_MEGAPHONE_WARNING"
};

u32 g_TimeLimitedAnimalFarContextsPHashes[NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS];
u32 g_TimeLimitedAnimalFarContextsLastPlayTime[NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS];
u32 g_TimeLimitedAnimalFarContextsMinTimeBetween[NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS] = {
	0
};
const char* g_TimeLimitedAnimalFarContexts[NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS] = {
	""
};

enum
{
	AUD_MINI_VOICE_TYPE_NORMAL,
	AUD_MINI_VOICE_TYPE_DRIVING,
	AUD_MINI_VOICE_TYPE_HOOKER,
	AUD_MINI_VOICE_TYPE_GANG
};

struct tVoice
{
	u32 VoiceName;
	u32 ReferenceCount;
	u32 RunningTab;
};

const char* g_AnimalBankNames[ANIMALTYPE_MAX] = {
	"NONE",
	"BOAR",
	"CHICKEN",
	"DOG",
	"DOG_ROTTWEILER",
	"HORSE",
	"COW",
	"COYOTE",
	"DEER",
	"EAGLE",
	"FISH",
	"HEN",
	"MONKEY",
	"MOUNTAIN_LION",
	"PIGEON",
	"RAT",
	"SEAGULL",
	"CROW",
	"PIG",
	"Chickenhawk",
	"Cormorant",
	"Rhesus",
	"Retriever",
	"Chimp",
	"Husky",
	"Shepherd",
	"Cat",
	"Whale",
	"Dolphin",
	"SmallDog",
	"Hammerhead",
	"Stingray",
	"Rabbit",
	"KillerWhale"
};

static SpeechContext* g_DefaultSpeechContext = NULL;

PF_PAGE(SpeechSoundsPage, "SpeechSounds");
PF_GROUP(SpeechSoundsGroup);
PF_LINK(SpeechSoundsPage, SpeechSoundsGroup);
PF_VALUE_INT(PlayingAmbientSpeech, SpeechSoundsGroup);
PF_VALUE_INT(StreamingIsBusy, SpeechSoundsGroup);

void audSpeechManager::Init()
{
	audEntity::Init();

	// Set up our slots
	for (s32 i=0; i<g_MaxAmbientSpeechSlots; i++)
	{
		m_AmbientSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_VACANT;
		m_AmbientSpeechSlots[i].SpeechAudioEntity = NULL;
		char slotName[32];
		sprintf(slotName, "SPEECH_%d", i);
		m_AmbientSpeechSlots[i].WaveSlot = audWaveSlot::FindWaveSlot(slotName);
		if(!m_AmbientSpeechSlots[i].WaveSlot)
		{
			naWarningf("Failed to find ambient speech slot %s", slotName);
		}
	}
	m_AmbientSpeechSlotToNextSearchFrom = 0;

	for (s32 i=0; i<g_MaxScriptedSpeechSlots; i++)
	{
		m_ScriptedSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_VACANT;
		m_ScriptedSpeechSlots[i].SpeechAudioEntity = NULL;
		char slotName[32];
		sprintf(slotName, "SCRIPT_SPEECH_%d", i);
		m_ScriptedSpeechSlots[i].WaveSlot = audWaveSlot::FindWaveSlot(slotName);
		if(!m_ScriptedSpeechSlots[i].WaveSlot)
		{
			naWarningf("Failed to find script speech slot %s", slotName);
		}
	}
	/*m_PlayerSpeechSlot.PlayState = AUD_SPEECH_SLOT_VACANT;
	m_PlayerSpeechSlot.SpeechAudioEntity = NULL;
	m_PlayerSpeechSlot.WaveSlot = audWaveSlot::FindWaveSlot("PLAYER_SPEECH");
	if(!m_PlayerSpeechSlot.WaveSlot)
	{
		naWarningf("Failed to find player_speech slot");
	}*/

	SetPlayerPainRootBankName("PAIN_MICHAEL");
	g_PainSlotType[0] = g_PlayerPainRootBankName;
	g_PainSlotType[1] = "PAIN_MALE";
	g_PainSlotType[2] = "PAIN_FEMALE";
	g_PainSlotType[3] = "BREATHING_TEST";
	g_PainLevelString[0] = "WEAK";
	g_PainLevelString[1] = "NORMAL";
	g_PainLevelString[2] = "TOUGH";
	g_PainLevelString[3] = "MIXED";
	for (u32 painType=0; painType<AUD_PAIN_VOICE_NUM_TYPES; painType++)
	{
		m_PainSlots[painType].BankId = 0;
		m_PainSlots[painType].SlotState = AUD_PAIN_SLOT_USED_UP;
		char slotName[32];
		sprintf(slotName, "PAIN_%d", painType);
		m_PainSlots[painType].WaveSlot = audWaveSlot::FindWaveSlot(slotName);
		if(!m_PainSlots[painType].WaveSlot)
		{
			naWarningf("Failed to find pain slot %s", slotName);
		}

		m_CurrentPainBank[painType] = 1;
		m_NextTimeToLoadPain[painType] = 0;
		if(painType != AUD_PAIN_VOICE_TYPE_BREATHING)
		{
			for (u32 i=0; i<AUD_PAIN_ID_NUM_BANK_LOADED; i++)
			{
				m_NextVariationForPain[painType][i] = 1;
			}
		}
		else
		{
			for (u32 i=0; i<AUD_NUM_PLAYER_VFX_TYPES; i++)
			{
				m_NextVariationForBreath[i] = 1;
			}
		}

		m_TimeLastLoaded[painType] = 0;
	}

	m_BreathingTypeCurrentlyLoaded = AUD_BREATHING_TYPE_NONE;

	m_PainSlots[AUD_PAIN_VOICE_NUM_TYPES].BankId = 0;
	m_PainSlots[AUD_PAIN_VOICE_NUM_TYPES].SlotState = AUD_PAIN_SLOT_READY;
	char slotName[32];
	sprintf(slotName, "PAIN_%d", AUD_PAIN_VOICE_NUM_TYPES);
	m_PainSlots[AUD_PAIN_VOICE_NUM_TYPES].WaveSlot = audWaveSlot::FindWaveSlot(slotName);
	if(!m_PainSlots[AUD_PAIN_VOICE_NUM_TYPES].WaveSlot)
	{
		naWarningf("Failed to find pain slot %s", slotName);
	}

	m_PainTypeLoading = -1;
	m_SpecificPainLevelToLoad = AUD_PAIN_LEVEL_NORMAL;
	m_SpecificPainLevelLoaded = AUD_PAIN_LEVEL_NORMAL;
	m_SpecificPainToLoadIsMale = true;
	m_SpecificPainLoadedIsMale = false;
	m_SpecificPainLoadingIsMale = false;

	m_TimeSpecificBankLastLoaded = 0;
	m_LastTimeSpecificPainBankSelected = 0;
	ResetSpecificPainBankTrackingVars();

	for(int i =0; i<AUD_PAIN_VOICE_NUM_TYPES; ++i)
	{
		for(int j=0; j<AUD_PAIN_ID_NUM_BANK_LOADED; ++j)
		{
			m_NumPainInCurrentBank[i][j] = 0;
		}
	}

	for(int i=0; i<AUD_NUM_PLAYER_VFX_TYPES; i++)
	{
		m_BreathVariationsInCurrentBank[i] = 0;
	}
	g_BreathSlotType[AUD_BREATHING_TYPE_MICHAEL] = "BREATHING_MICHAEL";
	g_BreathSlotType[AUD_BREATHING_TYPE_FRANKLIN] = "BREATHING_FRANKLIN";
	g_BreathSlotType[AUD_BREATHING_TYPE_TREVOR] = "BREATHING_TREVOR";
	g_BreathSlotType[AUD_BREATHING_TYPE_MP_MALE] = "";
	g_BreathSlotType[AUD_BREATHING_TYPE_MP_FEMALE] = "";

	g_NumBreathingBanks[AUD_BREATHING_TYPE_MICHAEL] = 1;
	g_NumBreathingBanks[AUD_BREATHING_TYPE_FRANKLIN] = 1;
	g_NumBreathingBanks[AUD_BREATHING_TYPE_TREVOR] = 1;
	g_NumBreathingBanks[AUD_BREATHING_TYPE_MP_MALE] = 0;
	g_NumBreathingBanks[AUD_BREATHING_TYPE_MP_FEMALE] = 0;


	g_BreathContexts[BREATH_EXHALE] = "EXHALE";
	g_BreathContexts[BREATH_EXHALE_CYCLING] = "EXHALE_CYCLING";
	g_BreathContexts[BREATH_INHALE] = "INHALE";
	g_BreathContexts[BREATH_SWIM_PRE_DIVE] = "SWIM_PRE_DIVE";

	for (u32 i=0; i<g_PhraseMemorySize; i++)
	{
		m_PhraseMemory[i][0] = 0;
		m_PhraseMemory[i][1] = 0;
		m_PhraseMemory[i][2] = 0;
	}
	m_NextPhraseMemoryWriteIndex = 0;

	for (u32 i=0; i<g_PhoneConvMemorySize; i++)
	{
		m_PhoneConversationMemory[i][0] = 0;
		m_PhoneConversationMemory[i][1] = 0;
		m_PhoneConversationMemory[i][2] = 0;
	}
	m_NextPhoneConvMemoryWriteIndex = 0;

	g_VehicleOpennessToSpeechFilterCurve.Init("VEHICLE_INTERIOR_OPEN_TO_SPEECH_CUTOFF");
	g_VehicleOpennessToSpeechVolumeCurve.Init("VEHICLE_INTERIOR_OPEN_TO_SPEECH_VOL");
	g_DistanceSqToPriorityScalarCurve.Init("SPEECH_DISTANCE_SQ_PRIORITY_CURVE");

	for (s32 i=0; i<NUM_MINI_DRIVING_CONTEXTS; i++)
	{
		g_MiniDrivingContextHashes[i] = atStringHash(g_MiniDrivingContexts[i]);
	}
	for (s32 i=0; i<NUM_MINI_NON_DRIVING_CONTEXTS; i++)
	{
		g_MiniNonDrivingContextHashes[i] = atStringHash(g_MiniNonDrivingContexts[i]);
	}
	for (s32 i=0; i<NUM_GANG_CONTEXTS; i++)
	{
		g_GangContextHashes[i] = atStringHash(g_GangContexts[i]);
	}
	for (s32 i=0; i<NUM_LOUD_CONTEXTS; i++)
	{
		g_LoudContextHashes[i] = atStringHash(g_LoudContexts[i]);
	}
	for (s32 i=0; i<NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS; i++)
	{
		g_TimeLimitedAnimalFarContextsPHashes[i] = atPartialStringHash(g_TimeLimitedAnimalFarContexts[i]);
		g_TimeLimitedAnimalFarContextsLastPlayTime[i] = 0;
	}

	m_ChurnSpeechSound = NULL;
	m_ChurnSlot = -1;

	m_ScriptOwningReservedSlot = THREAD_INVALID;
	m_ScriptReservedSlotNum = 0;

	g_DefaultSpeechContext =  static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr("DEFAULT_SPEECH_CONTEXT"));
	naAssertf(g_DefaultSpeechContext, "Failed to find default speech context object.  Any speech played through the ambient speech system without its own object will fail.");

	for(int i=0; i<ANIMALTYPE_MAX; ++i)
	{
		m_AnimalInfoArray[i].AnimalType = (AnimalType)i;
		char objName[64];
		formatf(objName, "ANIMAL_PARAMS_%s", g_AnimalBankNames[i]);
		m_AnimalInfoArray[i].Data = audNorthAudioEngine::GetObject<AnimalParams>(objName);

#if __BANK
		if(!m_AnimalInfoArray[i].Data)	
		{
			Warningf("Failed to find animal audio game object %s.  Alert audio.", objName);
		}
		else
		{
			Warningf("Found animal audio game object %s.", objName);
		}
#endif

		m_NextAnimalBankNumToLoad[i] = 1;
		m_NumAnimalAppearancesSinceLastUpdate[i] = 0;
		m_NearestAnimalAppearancesSinceLastUpdate[i] = -1.0f;

		m_NumAnimalBanks[i] = 0;
		bool bankFound = true;
		while(bankFound)
		{
			char bankName[64];
			formatf(bankName, "ANIMALS_NEAR\\%s_NEAR_%.2i", g_AnimalBankNames[i], m_NumAnimalBanks[i]+1);
			if(audWaveSlot::FindBankId(bankName) != ~0U)
			{
				Displayf("Found animal bank %s, index(%d)", bankName, i);
				m_NumAnimalBanks[i]++;
			}
			else
				bankFound = false;
		}
		m_NumAnimalsMakingSound[i] = 0;
	}

	m_MaxNumAnimalSlots = 3;
	m_AnimalBankSlots.Reset();
	m_AnimalBankSlots.Reserve(m_MaxNumAnimalSlots);
	for(u32 i = 0; i < m_MaxNumAnimalSlots; i++)
	{
		if(!Verifyf(m_AnimalBankSlots.GetCount() < ANIMALTYPE_MAX, "Audio is set up to allow more animal bank slots than there are animals.  This should be fixed."))
		{
			break;
		}

		formatf(slotName, sizeof(slotName), "ANIMAL_NEAR_%d", i);
		audWaveSlot* waveSlot = audWaveSlot::FindWaveSlot(slotName);
		if(waveSlot)
		{
			audAnimalBankSlot animalSlot;
			animalSlot.WaveSlot = waveSlot;
			m_AnimalBankSlots.PushAndGrow(animalSlot);
			//LoadAnimalBank((AnimalType)(s8)m_AnimalBankSlots.GetCount(), (s8)m_AnimalBankSlots.GetCount()-1);
		}
		else
		{
			Assertf(0, "Failed to find animal bank slot %s", slotName);
		}
	}
	m_TimeOfLastFullAnimalBankUpdate = 0;
	m_ForceAnimalBankUpdate = false;
	m_LoadingAfterAnimalUpdate = false;

	for(int i=0; i<2; i++)
	{
		for(int j=0; j<AUD_NUM_PLAYER_VFX_TYPES; j++)
		{
			m_NumTennisVariations[i][j] = 0;
			m_LastTennisVariations[i][j] = 0;
		}
	}
	m_TennisOpponent = NULL;
	m_TennisScriptIndex = -1;
	m_CurrentTennisSlotLoading = -1;
	m_TennisIsUsingAnimalBanks = false;
	m_TennisOpponentIsMale = false;
	m_TennisPlayerIsMichael = false;
	m_TennisPlayerIsFranklin = false;
	m_TennisPlayerIsTrevor = false;

#if GTA_REPLAY
	m_ReplayNextPainBank = AUD_PAIN_VOICE_NUM_TYPES;
	m_ReplayShouldSkipBankOnJump = false;
#endif // GTA_REPLAY
#if __BANK
	ResetSpecificPainDrawStats();
#endif
}

void audSpeechManager::Shutdown()
{
	audEntity::Shutdown();

	for(int i =0; i<m_AnimalBankSlots.GetCount(); ++i)
	{
		m_AnimalBankSlots[i].ContextInfo.clear();
		m_AnimalBankSlots[i].Owner = kAnimalNone;
	}
	for(int i=0; i<ANIMALTYPE_MAX; i++)
	{
		m_AnimalInfoArray[i].SlotNumber = -1;
	}
}


void audSpeechManager::SetPlayerPainRootBankName(const CPed& newPlayer)
{
	switch(newPlayer.GetPedType())
	{
	case PEDTYPE_PLAYER_0:
		SetPlayerPainRootBankName("PAIN_MICHAEL");
		break;
	case PEDTYPE_PLAYER_1:
		SetPlayerPainRootBankName("PAIN_FRANKLIN");
		break;
	case PEDTYPE_PLAYER_2:
		SetPlayerPainRootBankName("PAIN_TREVOR");
		break;
	default:
		SetPlayerPainRootBankName("PAIN_MICHAEL");
		break;
	}
}

void audSpeechManager::SetPlayerPainRootBankName(const char* bankName)
{
	if (bankName)
	{
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{	// If we could change this function to take in an ePedType instead of const char then
			// the CPacketSetPlayerPainRoot packet can lose about 62 bytes..... (!)
			CReplayMgr::RecordFx(CPacketSetPlayerPainRoot(g_PlayerPainRootBankName, bankName));
		}
#endif // GTA_REPLAY

		formatf(g_PlayerPainRootBankName, sizeof(g_PlayerPainRootBankName), bankName);
		// we also need to clear out the old pain slots and load up the new ones - just mark them as used up and the update will deal with loading
		m_PainSlots[AUD_PAIN_VOICE_TYPE_PLAYER].SlotState = AUD_PAIN_SLOT_USED_UP;
		m_PainSlots[AUD_PAIN_VOICE_TYPE_BREATHING].SlotState = AUD_PAIN_SLOT_USED_UP;
	}
}

void audSpeechManager::Update()
{
#if __DEV
	// See how much ambient speech we're playing
	
	g_NumPlayingAmbientSpeech = 0;
	for (s32 i=0; i<g_MaxAmbientSpeechSlots; i++)
	{
		if (m_AmbientSpeechSlots[i].PlayState == AUD_SPEECH_SLOT_ALLOCATED)
		{
			g_NumPlayingAmbientSpeech++;
		}
	}
	g_StreamingIsBusy = 0;
	if (IsStreamingBusyForContext(g_AnythingDullPHash))
	{
		g_StreamingIsBusy = 1;
	}
	PF_SET(PlayingAmbientSpeech, g_NumPlayingAmbientSpeech);
	PF_SET(StreamingIsBusy, g_StreamingIsBusy);
	
#endif

#if __BANK
	audSpeechAudioEntity::UpdateVFXDebugDrawStructs();
	audSpeechAudioEntity::UpdateAmbSpeechDebugDrawStructs();
	if(g_StressTestSpeech)
	{
		if(!g_StressTestSpeechSound)
		{
			audSoundInitParams initParams;
			initParams.Pan = 0;
			initParams.StartOffset = audEngineUtil::GetRandomNumberInRange(0, (s32)g_StressTestMaxStartOffset);
			CreateSound_PersistentReference("SPEECH_SCRIPTED_SPEECH", &g_StressTestSpeechSound, &initParams);
			if(g_StressTestSpeechSound)
			{
				u32 numVariations = audSpeechSound::FindNumVariationsForVoiceAndContext(g_StressTestSpeechVoiceName,g_StressTestSpeechContext);
				if(numVariations > 0)
				{
					audSpeechSound *speechSound = (audSpeechSound*)g_StressTestSpeechSound;
					naAssertf(speechSound->GetSoundTypeID() == SpeechSound::TYPE_ID, "Speech sound was expected to have type %d but instead had type %d", SpeechSound::TYPE_ID, speechSound->GetSoundTypeID());
					g_StressTestSpeechVariation = ((g_StressTestSpeechVariation + 1) % numVariations);
					if(speechSound->InitSpeech(g_StressTestSpeechVoiceName, g_StressTestSpeechContext, g_StressTestSpeechVariation+1))
					{
						speechSound->PrepareAndPlay(audWaveSlot::FindWaveSlot("SCRIPT_SPEECH_0"), true, -1);
					}
					else
					{
						speechSound->StopAndForget();
					}
				}
			}
		}
	}

	if(g_DrawSpeechSlots)
		DrawSpeechSlots();

	if(g_DrawSpecificPainInfo)
		DrawSpecificPainInfo();

	if(g_DrawPainPlayedTotals)
		DrawPainPlayedTotals();

	if(g_DisplayAnimalBanks)
		DrawAnimalBanks();
#endif

	// Go through our slots, and if any are marked as STOPPING, see if they're done, and free them up. 
	for (s32 i=0; i<g_MaxAmbientSpeechSlots; i++)
	{
		if (m_AmbientSpeechSlots[i].PlayState == AUD_SPEECH_SLOT_STOPPING)
		{
			naAssertf(m_AmbientSpeechSlots[i].WaveSlot, "Next ambient speech slot has an invalid WaveSlot");
			if (m_AmbientSpeechSlots[i].WaveSlot->GetReferenceCount() == 0)
			{
				// Slot's finished
				m_AmbientSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_VACANT;
			}
		}
		if(m_AmbientSpeechSlots[i].PlayState == AUD_SPEECH_SLOT_ALLOCATED && m_AmbientSpeechSlots[i].SpeechAudioEntity &&
			m_AmbientSpeechSlots[i].SpeechAudioEntity->GetParent() && m_AmbientSpeechSlots[i].SpeechAudioEntity->GetParent()->m_nDEflags.bFrozen)
		{
			m_AmbientSpeechSlots[i].SpeechAudioEntity->MarkToStopSpeech();
		}
	}
	for (s32 i=0; i<g_MaxScriptedSpeechSlots; i++)
	{
		if (m_ScriptedSpeechSlots[i].PlayState == AUD_SPEECH_SLOT_STOPPING)
		{
			naAssertf(m_ScriptedSpeechSlots[i].WaveSlot, "Scripted speech slot has invalid WaveSlot");
			if (m_ScriptedSpeechSlots[i].WaveSlot->GetReferenceCount() == 0)
			{
				// Slot's finished
				m_ScriptedSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_VACANT;
			}
		}
	}
	//if (m_PlayerSpeechSlot.PlayState == AUD_SPEECH_SLOT_STOPPING)
	//{
	//	naAssertf(m_PlayerSpeechSlot.WaveSlot, "Player speech slot has invalid WaveSlot");
	//	if (m_PlayerSpeechSlot.WaveSlot->GetReferenceCount() == 0)
	//	{
	//		// Slot's finished
	//		m_PlayerSpeechSlot.PlayState = AUD_SPEECH_SLOT_VACANT;
	//	}
	//}

	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(g_ActivatePainSpecificBankSwapping &&
		currentTime - m_LastTimeSpecificPainBankSelected > g_TimeBetweenSpecificPainBankSelection &&
		currentTime - m_TimeSpecificBankLastLoaded > g_TimeBetweenSpecificPainBankLoadAndSelection)
		SelectSpecificPainBank(currentTime);
#if __BANK
	else if(g_ForceSpecificPainBankSwap)
	{
		SelectSpecificPainBank(currentTime);
		g_ForceSpecificPainBankSwap = false;
	}
#endif

	UpdatePainSlots();

	UpdateBreathingBanks();

	//UpdateGlobalSpeechMetrics();

#if __BANK
	audSpeechAudioEntity::UpdateVFXDebugDrawStructs();
	audSpeechAudioEntity::UpdateAmbSpeechDebugDrawStructs();
#endif
}

void audSpeechManager::UpdateGlobalSpeechMetrics()
{
	// See how in a car we are, and scale up speech rolloffs accordingly
	f32 inACarRatio = 0.0f;
	bool isPlayerVehicleATrain = false;
	audNorthAudioEngine::GetGtaEnvironment()->IsPlayerInVehicle(&inACarRatio, &isPlayerVehicleATrain);
	f32 rolloff = (1.0f - inACarRatio) + (inACarRatio*g_SpeechRolloffWhenInACar);
	if (isPlayerVehicleATrain)
	{
		rolloff = 1.0f;
	}
	// Get our relevant categories - don't scale up loud or high-rolloff, as these are already really big
	if (g_SpeechNormalCategory)
	{
		g_SpeechNormalCategory->SetDistanceRollOffScale(rolloff);
	}
}

const char* audSpeechManager::GetRandomContextForChurn()
{
	return "BUMP";
}

void audSpeechManager::ResetSpecificPainBankTrackingVars()
{
	for(u8 i=0; i<3; ++i)
	{
		for(u8 j=0; j<2; ++j)
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[i][j] = 0;
	}
}

void audSpeechManager::RegisterPedAimedAtOrHurt(u8 toughness, bool shouldLowerToughness, bool hurt)
{
	if(!g_ActivatePainSpecificBankSwapping)
		return;
	
	u32 amtToInc = hurt ? g_OverridePainVsAimScalar : 1;

	if(shouldLowerToughness)
	{
		switch(toughness)
		{
		case AUD_TOUGHNESS_MALE_COWARDLY:
		case AUD_TOUGHNESS_MALE_NORMAL:
		case AUD_TOUGHNESS_MALE_BUM:
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[AUD_PAIN_LEVEL_WEAK][0] += amtToInc; 
			break;
		case AUD_TOUGHNESS_MALE_BRAVE:
		case AUD_TOUGHNESS_MALE_GANG:
		case AUD_TOUGHNESS_COP:
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[AUD_PAIN_LEVEL_NORMAL][0] += amtToInc; 
			break;
		case AUD_TOUGHNESS_FEMALE:
		case AUD_TOUGHNESS_FEMALE_BUM:
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[AUD_PAIN_LEVEL_NORMAL][1] += amtToInc; 
			break;
		default:
			break;
		}
	}
	else
	{
		switch(toughness)
		{
		case AUD_TOUGHNESS_MALE_COWARDLY:
		case AUD_TOUGHNESS_MALE_BUM:
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[AUD_PAIN_LEVEL_WEAK][0] += amtToInc; 
			break;
		case AUD_TOUGHNESS_MALE_NORMAL:
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[AUD_PAIN_LEVEL_NORMAL][0] += amtToInc; 
			break;
		case AUD_TOUGHNESS_MALE_BRAVE:
		case AUD_TOUGHNESS_MALE_GANG:
		case AUD_TOUGHNESS_COP:
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[AUD_PAIN_LEVEL_TOUGH][0] += amtToInc; 
			break;
		case AUD_TOUGHNESS_FEMALE:
		case AUD_TOUGHNESS_FEMALE_BUM:
			m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[AUD_PAIN_LEVEL_NORMAL][1] += amtToInc; 
			break;
		default:
			break;
		}
	}
}

void audSpeechManager::SelectSpecificPainBank(u32 currentTime)
{
	m_LastTimeSpecificPainBankSelected = currentTime;

#if __BANK
	if(g_OverrideSpecificPainLoaded)
	{
		m_SpecificPainLevelToLoad = g_OverrideSpecificPainLevel;
		m_SpecificPainToLoadIsMale = g_OverrideSpecificPainIsMale;
		m_NeedToLoad[AUD_PAIN_VOICE_TYPE_SPECIFIC] = true;
		ResetSpecificPainBankTrackingVars();
		return;
	}

#endif
	u32 highestCount = 0;
	u8 iForHighestCount = 0;
	u8 jForHighestCount = 0;
	for(u8 i=0; i<AUD_PAIN_LEVEL_MIXED; ++i)
	{
		for(u8 j=0; j<PAIN_DEBUG_NUM_GENDERS; ++j)
		{
			if(m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[i][j] > highestCount)
			{
				highestCount = m_PedsAimedAtOrHurtSinceLastSpecificPainBankSelected[i][j];
				iForHighestCount = i;
				jForHighestCount = j;
			}
		}
	}

	bool shouldLoadMale = jForHighestCount==0;
	if(highestCount != 0 && (m_SpecificPainLevelLoaded != iForHighestCount || m_SpecificPainLoadedIsMale != shouldLoadMale))
	{
		m_SpecificPainLevelToLoad = iForHighestCount;
		m_SpecificPainToLoadIsMale = shouldLoadMale;
		m_NeedToLoad[AUD_PAIN_VOICE_TYPE_SPECIFIC] = true;
	}
	//For now, we'll just keep whatever is loaded loaded.  Perhaps we'll want to fall back to MALE_NORMAL
	/*else
	{

	}*/
	ResetSpecificPainBankTrackingVars();
}

void audSpeechManager::UpdatePainSlots()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && (CReplayMgr::GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK) || CReplayMgr::GetCurrentPlayBackState().IsSet(REPLAY_CURSOR_JUMP)))
		return;
#endif // GTA_REPLAY

#if __BANK
	if(g_UseTestPainBanks && !g_HavePreparedPainBanksForTesting)
	{
		m_PainSlots[AUD_PAIN_VOICE_TYPE_MALE].SlotState = AUD_PAIN_SLOT_USED_UP;
		m_PainSlots[AUD_PAIN_VOICE_TYPE_FEMALE].SlotState = AUD_PAIN_SLOT_USED_UP;
		g_HavePreparedPainBanksForTesting = true;
	}
	else if(!g_UseTestPainBanks && g_HavePreparedPainBanksForTesting)
	{
		g_HavePreparedPainBanksForTesting = false;
	}
#endif

	// Check current state of unused one, and kick off load if required, and we're free to.
	for (u32 painVoiceType=0; painVoiceType<AUD_PAIN_VOICE_NUM_TYPES; painVoiceType++)
	{
		// If in-use one is done, move onto the other one if it's ready.

		switch (m_PainSlots[painVoiceType].SlotState)
		{
		case AUD_PAIN_SLOT_USED_UP:
			m_NeedToLoad[painVoiceType] = true;
			//m_PainSlots[painVoiceType].SlotState = AUD_PAIN_SLOT_LOADING;
			break;
		case AUD_PAIN_SLOT_LOADING:
			break;
		case AUD_PAIN_SLOT_READY:
			naErrorf("Audio: In-used pain slot claims to be ready. Type: %d", painVoiceType);
			m_PainSlots[painVoiceType].SlotState = AUD_PAIN_SLOT_USED_UP;
			break;
		case AUD_PAIN_SLOT_IN_USE:
			// See if we've played all of any of our lines, and if so, we're used up
			//	minus 1, as we deal with breathing elsewhere
			for (u32 i=0; i<AUD_PAIN_ID_NUM_BANK_LOADED-1; i++)
			{
				if( m_NumPainInCurrentBank[painVoiceType][i] != 0 &&
					m_NumTimesPainPlayed[painVoiceType][i] >= m_NumPainInCurrentBank[painVoiceType][i])
				{
					m_PainSlots[painVoiceType].SlotState = AUD_PAIN_SLOT_USED_UP;
					break;
				}
			}
			break;
		}
	}

	s32 nextPainTypeToLoad = AUD_PAIN_VOICE_NUM_TYPES;
	if(m_PainTypeLoading >= 0)
		nextPainTypeToLoad = m_PainTypeLoading;
	else
	{
		for(u32 painVoiceType=0; painVoiceType<AUD_PAIN_VOICE_NUM_TYPES; painVoiceType++)
		{
			if(m_NeedToLoad[painVoiceType])
			{
				if(m_NextTimeToLoadPain[painVoiceType]<audNorthAudioEngine::GetCurrentTimeInMs() &&
					(nextPainTypeToLoad == AUD_PAIN_VOICE_NUM_TYPES || m_TimeLastLoaded[painVoiceType] < m_TimeLastLoaded[nextPainTypeToLoad]))
					nextPainTypeToLoad = painVoiceType;
			}
		}
	}

	u8 numBanks = AUD_NUM_MICHAEL_PAIN_BANKS;
	u8 painLevel = AUD_PAIN_LEVEL_MIXED;

	if (nextPainTypeToLoad == AUD_PAIN_VOICE_TYPE_PLAYER)
	{
		switch(atStringHash(g_PlayerPainRootBankName))
		{
		case 0xAF868358: //PAIN_MICHAEL
			numBanks = AUD_NUM_MICHAEL_PAIN_BANKS;
			break;
		case 0xEBDE18F: //PAIN_FRANKLIN
			numBanks = AUD_NUM_FRANKLIN_PAIN_BANKS;
			break;
		case 0x983182FE: //PAIN_TREVOR
			numBanks = AUD_NUM_TREVOR_PAIN_BANKS;
			break;
		}
	}
	else if (nextPainTypeToLoad == AUD_PAIN_VOICE_TYPE_MALE)
	{
		numBanks = AUD_NUM_MALE_MIXED_PAIN_BANKS;
	}
	else if(nextPainTypeToLoad == AUD_PAIN_VOICE_TYPE_FEMALE)
	{
		numBanks = AUD_NUM_FEMALE_NORMAL_PAIN_BANKS;
	}
	else if (nextPainTypeToLoad == AUD_PAIN_VOICE_TYPE_SPECIFIC)
	{
		switch(m_SpecificPainLevelToLoad)
		{
		case AUD_PAIN_LEVEL_WEAK:
			numBanks = m_SpecificPainToLoadIsMale ? AUD_NUM_MALE_WEAK_PAIN_BANKS : AUD_NUM_FEMALE_WEAK_PAIN_BANKS;
			painLevel = AUD_PAIN_LEVEL_WEAK;
			break;
		case AUD_PAIN_LEVEL_NORMAL:
			numBanks = m_SpecificPainToLoadIsMale ? AUD_NUM_MALE_NORMAL_PAIN_BANKS : AUD_NUM_FEMALE_NORMAL_PAIN_BANKS;
			painLevel = AUD_PAIN_LEVEL_NORMAL;
			break;
		case AUD_PAIN_LEVEL_TOUGH:
			numBanks = m_SpecificPainToLoadIsMale ? AUD_NUM_MALE_TOUGH_PAIN_BANKS : AUD_NUM_FEMALE_NORMAL_PAIN_BANKS;
			painLevel = (u8)(m_SpecificPainToLoadIsMale ? AUD_PAIN_LEVEL_TOUGH : AUD_PAIN_LEVEL_NORMAL);
			break;
		}
	}
	else if (nextPainTypeToLoad == AUD_PAIN_VOICE_TYPE_BREATHING)
	{
		numBanks = g_NumBreathingBanks[m_BreathingTypeToLoad];
	}

	//Initialize these here to avoid "transfer of control bypasses..." compile errors.
	audWaveSlot::audWaveSlotLoadStatus loadStatus = audWaveSlot::NOT_REQUESTED;
	audWaveSlot* temp = NULL;

	audPainSlot& loadingSlot = m_PainSlots[AUD_PAIN_VOICE_NUM_TYPES];
	switch (loadingSlot.SlotState)
	{
	case AUD_PAIN_SLOT_READY:
		// See if it's ok to load the bank
		Assertf(loadingSlot.WaveSlot, "Failed to find valid waveslot pointer in backup pain slot.");
		naCErrorf(m_PainTypeLoading==-1, "Pain loading slot is set to READY but we have a valid PainTypeLoading set.");
		if (loadingSlot.WaveSlot && nextPainTypeToLoad != AUD_PAIN_VOICE_NUM_TYPES)
		{
			Assign(m_PainTypeLoading, nextPainTypeToLoad);
			if(loadingSlot.WaveSlot->GetReferenceCount() == 0)
			{
				//may as well cache this, in case we end up setting ToLoad from a different thread, though it will only be used
				//	if loading the specific bank
				bool loadingSpecificMale = m_SpecificPainToLoadIsMale;
				// Get the next bank name
				u8  nextPainBank = (m_CurrentPainBank[nextPainTypeToLoad] % numBanks) + 1;
#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx(CPacketPainWaveBankChange(m_CurrentPainBank[nextPainTypeToLoad], nextPainBank));
				}
#endif // GTA_REPLAY
				char voiceName[64] = "";
				if (nextPainTypeToLoad==AUD_PAIN_VOICE_TYPE_PLAYER)
				{
					formatf(voiceName, 64, "%s_%02d", g_PlayerPainRootBankName, nextPainBank);
				}
				else if (nextPainTypeToLoad==AUD_PAIN_VOICE_TYPE_FEMALE)
				{
					formatf(voiceName, 64, "PAIN_FEMALE_NORMAL_%02d", nextPainBank);
				}
				else if(nextPainTypeToLoad==AUD_PAIN_VOICE_TYPE_SPECIFIC)
				{
					//there are no specific female banks
					if(loadingSpecificMale)
					{
						formatf(voiceName, 64, "PAIN_%s_%s_%02d", "MALE", g_PainLevelString[painLevel], nextPainBank);
					}
					else
					{
						formatf(voiceName, 64, "PAIN_FEMALE_NORMAL_%02d", nextPainBank);
					}
				}
				else if(nextPainTypeToLoad==AUD_PAIN_VOICE_TYPE_BREATHING)
				{
					formatf(voiceName, 64, "%s_%02d", g_BreathSlotType[m_BreathingTypeToLoad], nextPainBank);
				}
				else
				{
#if __BANK
					if(g_UseTestPainBanks)
						nextPainBank = 1;
#endif
					formatf(voiceName, 64, "%s_MIXED_%02d", g_PainSlotType[nextPainTypeToLoad], nextPainBank);
				}
				
				u32 bankId = audSpeechSound::FindBankIdForVoiceAndContext(voiceName, "PAIN_WHEEZE");
				if(nextPainTypeToLoad==AUD_PAIN_VOICE_TYPE_BREATHING)
				{
					bankId = audSpeechSound::FindBankIdForVoiceAndContext(voiceName, "EXHALE");
				}
				else if(bankId == AUD_INVALID_BANK_ID)
				{
					bankId = audSpeechSound::FindBankIdForVoiceAndContext(voiceName, "PAIN_SHOVE");
					if(bankId == AUD_INVALID_BANK_ID)
					{
						bankId = audSpeechSound::FindBankIdForVoiceAndContext(voiceName, "PAIN_LOW");
					}
				}

				if (bankId < AUD_INVALID_BANK_ID)
				{
					Assign(loadingSlot.BankId, bankId);
					loadingSlot.WaveSlot->LoadBank(bankId);
					loadingSlot.SlotState = AUD_PAIN_SLOT_LOADING;
					loadingSlot.VoiceNameHash = atStringHash(voiceName);
					loadingSlot.PainLevel = painLevel;
#if __BANK
					safecpy(loadingSlot.VoiceName, voiceName);
#endif
					m_NextTimeToLoadPain[nextPainTypeToLoad] = audNorthAudioEngine::GetCurrentTimeInMs() + 1500;
					m_TimeLastLoaded[nextPainTypeToLoad] = audNorthAudioEngine::GetCurrentTimeInMs();
					if(nextPainTypeToLoad==AUD_PAIN_VOICE_TYPE_SPECIFIC)
						m_SpecificPainLoadingIsMale = loadingSpecificMale;
				}
				else
				{
					m_NextTimeToLoadPain[nextPainTypeToLoad] = audNorthAudioEngine::GetCurrentTimeInMs() + 60000;
					m_TimeLastLoaded[nextPainTypeToLoad] = audNorthAudioEngine::GetCurrentTimeInMs();
					Assign(m_PainTypeLoading, -1);
				}
			}
			else
			{
				Assign(m_PainTypeLoading, -1);
			}
		}
		break;
	case AUD_PAIN_SLOT_LOADING:
		// See if it's done
		loadStatus = loadingSlot.WaveSlot->GetBankLoadingStatus(loadingSlot.BankId);
		switch (loadStatus)
		{
		case audWaveSlot::NOT_REQUESTED:
			naErrorf("Pain bank not requested but thinks it has been");
			loadingSlot.SlotState = AUD_PAIN_SLOT_READY;
			break;
		case audWaveSlot::FAILED:
			//				Assertf(0, "Audio: Pain bank load failed. Bank: %s, Slot: %d", 
			//					audWaveSlot::GetBankName(m_PainSlots[painVoiceType][unusedSlot].BankId),	unusedSlot);
			naErrorf("Pain bank load load failed. Bank: %s", audWaveSlot::GetBankName(loadingSlot.BankId));
			loadingSlot.SlotState = AUD_PAIN_SLOT_READY;
			break;
		case audWaveSlot::LOADED:
			// change a few states, and point at that one
			m_NeedToLoad[m_PainTypeLoading] = false;
			m_CurrentPainBank[nextPainTypeToLoad] = (m_CurrentPainBank[nextPainTypeToLoad] % numBanks) + 1;
			if(nextPainTypeToLoad!=AUD_PAIN_VOICE_TYPE_BREATHING)
			{
				for (u32 i=0; i<AUD_PAIN_ID_NUM_BANK_LOADED; i++)
				{
					m_NumTimesPainPlayed[nextPainTypeToLoad][i] = 0;
					if(nextPainTypeToLoad == AUD_PAIN_VOICE_TYPE_PLAYER)
						m_NumPainInCurrentBank[m_PainTypeLoading][i] = (u8)audSpeechSound::FindNumVariationsForVoiceAndContext(loadingSlot.VoiceNameHash, g_PlayerPainContexts[i]);
					else
						m_NumPainInCurrentBank[m_PainTypeLoading][i] = (u8)audSpeechSound::FindNumVariationsForVoiceAndContext(loadingSlot.VoiceNameHash, g_PainContexts[i]);
					m_NextVariationForPain[m_PainTypeLoading][i] = 1;
				}
			}
			else
			{
				for(int i=0; i<AUD_NUM_PLAYER_VFX_TYPES; i++)
				{
					m_NextVariationForBreath[i] = 1;
					m_BreathVariationsInCurrentBank[i] = (u8)audSpeechSound::FindNumVariationsForVoiceAndContext(loadingSlot.VoiceNameHash, g_BreathContexts[i]);
				}
				m_BreathingTypeCurrentlyLoaded = m_BreathingTypeToLoad;
			}
			
			temp = m_PainSlots[m_PainTypeLoading].WaveSlot;
			m_PainSlots[m_PainTypeLoading].WaveSlot = loadingSlot.WaveSlot;
			loadingSlot.WaveSlot = temp;
			m_PainSlots[m_PainTypeLoading].BankId = loadingSlot.BankId;
			m_PainSlots[m_PainTypeLoading].SlotState = AUD_PAIN_SLOT_IN_USE;
			m_PainSlots[m_PainTypeLoading].VoiceNameHash = loadingSlot.VoiceNameHash;
#if __BANK
			safecpy(m_PainSlots[m_PainTypeLoading].VoiceName, loadingSlot.VoiceName);
#endif

			if(m_PainTypeLoading == AUD_PAIN_VOICE_TYPE_SPECIFIC)
			{
				m_SpecificPainLevelLoaded = loadingSlot.PainLevel;
				m_SpecificPainLoadedIsMale = m_SpecificPainLoadingIsMale;
				m_TimeSpecificBankLastLoaded = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
#if __BANK
				//int genderIndex = m_SpecificPainLoadedIsMale ? 0 : 1;
				int genderIndex = 0;
				m_DebugSpecPainBankLoadCount[m_SpecificPainLevelLoaded][genderIndex]++; 
				m_DebugSpecPainTimeLastLoaded[m_SpecificPainLevelLoaded][genderIndex] = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
				m_DebugSpecPainBankLoadCountTotal++;
#endif
			}
#if __BANK
			else
				m_DebugNonSpecPainBankLoadCountTotal++;
#endif

			m_PainTypeLoading = -1;
			loadingSlot.SlotState = AUD_PAIN_SLOT_READY;
			break;
		case audWaveSlot::LOADING:
		default:
			// All good, we'll wait for it
			break;
		}
		break;
	case AUD_PAIN_SLOT_IN_USE:
		naErrorf("Audio: Unused pain slot claims to be in use. Type: %d", nextPainTypeToLoad);
		loadingSlot.SlotState = AUD_PAIN_SLOT_READY;
		break;
	}
}

bool audSpeechManager::IsPainLoaded(u32 painVoiceType)
{
	// Make sure this pain type has a bank loaded - this won't be true for the first few seconds of a game
	if (painVoiceType<AUD_PAIN_VOICE_NUM_TYPES)
	{
		if (m_PainSlots[painVoiceType].SlotState == AUD_PAIN_SLOT_IN_USE)
		{
			return true;
		}
	}
	return false;
}

void audSpeechManager::PlayedPain(u32 painVoiceType, u32 painType)
{
#if __BANK
	if(painVoiceType == AUD_PAIN_VOICE_TYPE_SPECIFIC)
	{
		m_DebugSpecPainCountTotal++;
	}
	else if(painVoiceType == AUD_PAIN_VOICE_TYPE_FEMALE)
	{
		m_DebugNonSpecPainCountTotal++;
	}
	else if(painVoiceType == AUD_PAIN_VOICE_TYPE_MALE)
	{	
		m_DebugNonSpecPainCountTotal++;
	}

#endif
	if(m_NumPainInCurrentBank[painVoiceType][painType] != 0)
	{
		m_NumTimesPainPlayed[painVoiceType][painType]++; 
		m_NextVariationForPain[painVoiceType][painType] = 
			(m_NextVariationForPain[painVoiceType][painType] % m_NumPainInCurrentBank[painVoiceType][painType]) + 1;
		// also flag that we're not to swap this one out for a wee while - long enough that we'll defo have started playing any we're about to kick off
		m_NextTimeToLoadPain[painVoiceType] = Max(m_NextTimeToLoadPain[painVoiceType], audNorthAudioEngine::GetCurrentTimeInMs() + 500);
	}
}

#if GTA_REPLAY
void audSpeechManager::PlayedPainForReplay(u32 painVoiceType, u32 painType, u8 numTimes, u8 nextVariation, u32 nextTimeToLoad, bool isJumpingOrRwnd, bool isGoingForwards)
{
	if(!isJumpingOrRwnd)
	{	// We're just playing normally or fast forwarding so set the managers
		// variables directly and let is manage things properly
		m_NumTimesPainPlayed[painVoiceType][painType]			= numTimes; 
		m_NextVariationForPain[painVoiceType][painType]			= nextVariation;
		m_NextTimeToLoadPain[painVoiceType]						= nextTimeToLoad;
		m_ReplayNeedToReconcile									= false;
	}
	else
	{
		if(m_ReplayNeedToReconcile == false)
		{	// Replay hasn't started modifying vars yet so copy across what
			// they all should be
			for(int pvt = 0; pvt < AUD_PAIN_VOICE_NUM_TYPES; ++pvt)
			{
				for(int pt = 0; pt < AUD_PAIN_ID_NUM_BANK_LOADED; ++pt)
				{
					m_ReplayNumTimesPainPlayed[pvt][pt]			= m_NumTimesPainPlayed[pvt][pt];
					m_ReplayNextVariationForPain[pvt][pt]		= m_NextVariationForPain[pvt][pt];
				}
				m_ReplayNextTimeToLoadPain[pvt]					= m_NextTimeToLoadPain[pvt];
			}
		}

		// Set our modification from replay...
		m_ReplayNumTimesPainPlayed[painVoiceType][painType]		= numTimes; 
		m_ReplayNextVariationForPain[painVoiceType][painType]	= nextVariation;
		m_ReplayNextTimeToLoadPain[painVoiceType]				= nextTimeToLoad;
		m_ReplayNeedToReconcile									= true;		// These vars will need to be reconciled with the manager
	}

	m_ReplayWaveBankChangeWasLast								= false;	// Last action was to play a sound (not change bank)
	m_ReplayWasGoingForwards									= isGoingForwards;
}

void audSpeechManager::GetPlayedPainForReplayVars(u32 painVoiceType, u8* numTimes, u8* nextVariation, u32& nextTimeToLoad, u8& currentPainBank) const
{
	for(int i = 0; i < AUD_PAIN_ID_NUM_BANK_LOADED; ++i)
	{
		numTimes[i]			= m_NumTimesPainPlayed[painVoiceType][i];
		nextVariation[i]	= m_NextVariationForPain[painVoiceType][i];
	}
	nextTimeToLoad			= m_NextTimeToLoadPain[painVoiceType];
	currentPainBank			= m_CurrentPainBank[painVoiceType];
}

void audSpeechManager::SetPlayedPainForReplayVars(u32 painVoiceType, const u8* numTimes, const u8* nextVariation, const u32& nextTimeToLoad, const u8& currentPainBank)
{
	for(int i = 0; i < AUD_PAIN_ID_NUM_BANK_LOADED; ++i)
	{
		m_NumTimesPainPlayed[painVoiceType][i]		= numTimes[i];
		m_NextVariationForPain[painVoiceType][i]	= nextVariation[i];
	}
	m_NextTimeToLoadPain[painVoiceType]				= nextTimeToLoad;
	m_CurrentPainBank[painVoiceType]				= currentPainBank;
	replayDebugf1("Current pain bank set to %u", currentPainBank);
	m_CurrentPainBank[painVoiceType] = (m_CurrentPainBank[painVoiceType] % 2) + 1;
}
#endif // GTA_REPLAY

const char* audSpeechManager::GetPainLevelString(u8 toughness, bool shouldLowerToughness, bool& isSpecificPain) const
{
#if __BANK
	if(g_UseTestPainBanks)
		return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
#endif
	if(shouldLowerToughness)
	{
		switch(toughness)
		{
		case AUD_TOUGHNESS_MALE_COWARDLY:
		case AUD_TOUGHNESS_MALE_NORMAL:
		case AUD_TOUGHNESS_MALE_BUM:
			if(m_SpecificPainLoadedIsMale &&  m_SpecificPainLevelLoaded == AUD_PAIN_LEVEL_WEAK)
			{
				isSpecificPain = true;
				return g_PainLevelString[AUD_PAIN_LEVEL_WEAK];
			}
			else
				return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
		case AUD_TOUGHNESS_MALE_BRAVE:
		case AUD_TOUGHNESS_MALE_GANG:
		case AUD_TOUGHNESS_COP:
			if(m_SpecificPainLoadedIsMale &&  m_SpecificPainLevelLoaded == AUD_PAIN_LEVEL_NORMAL)
			{
				isSpecificPain = true;
				return g_PainLevelString[AUD_PAIN_LEVEL_NORMAL];
			}
			else
				return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
		case AUD_TOUGHNESS_FEMALE:
		case AUD_TOUGHNESS_FEMALE_BUM:
			return g_PainLevelString[AUD_PAIN_LEVEL_NORMAL];
		default:
			Assertf(0, "Bad toughness value passed to GetPainLevelString()");
			return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
		}
	}
	else
	{
		switch(toughness)
		{
		case AUD_TOUGHNESS_MALE_COWARDLY:
		case AUD_TOUGHNESS_MALE_BUM:
			/*if(m_SpecificPainLoadedIsMale &&  m_SpecificPainLevelLoaded == AUD_PAIN_LEVEL_WEAK)
			{
				isSpecificPain = true;
				return g_PainLevelString[AUD_PAIN_LEVEL_WEAK];
			}
			else*/
				return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
		case AUD_TOUGHNESS_MALE_NORMAL:
			if(m_SpecificPainLoadedIsMale &&  m_SpecificPainLevelLoaded == AUD_PAIN_LEVEL_NORMAL)
			{
				isSpecificPain = true;
				return g_PainLevelString[AUD_PAIN_LEVEL_NORMAL];
			}
			else
				return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
		case AUD_TOUGHNESS_MALE_BRAVE:
		case AUD_TOUGHNESS_MALE_GANG:
		case AUD_TOUGHNESS_COP:
			if(m_SpecificPainLoadedIsMale &&  m_SpecificPainLevelLoaded == AUD_PAIN_LEVEL_TOUGH)
			{
				isSpecificPain = true;
				return g_PainLevelString[AUD_PAIN_LEVEL_TOUGH];
			}
			else
				return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
		case AUD_TOUGHNESS_FEMALE:
		case AUD_TOUGHNESS_FEMALE_BUM:
			return g_PainLevelString[AUD_PAIN_LEVEL_NORMAL];
		default:
			Assertf(0, "Bad toughness value passed to GetPainLevelString()");
			return g_PainLevelString[AUD_PAIN_LEVEL_MIXED];
		}
	}
}

audWaveSlot* audSpeechManager::GetWaveSlotForPain(u32 painVoiceType)
{
#if __DEV
	naAssertf(painVoiceType<AUD_PAIN_VOICE_NUM_TYPES, "Pain voice type: %d", painVoiceType);
	naAssertf(m_PainSlots[painVoiceType].WaveSlot, "Missing waveslot for paintype %u", painVoiceType);
#endif
	return m_PainSlots[painVoiceType].WaveSlot;
}

u8 audSpeechManager::GetBankNumForPain(u32 painVoiceType)
{
#if __BANK
	if(g_UseTestPainBanks)
		return 1;
#endif
	return m_CurrentPainBank[painVoiceType];
}

#if GTA_REPLAY
void audSpeechManager::SetBankNumForPain(u32 painVoiceType, u8 bank)
{
	m_CurrentPainBank[painVoiceType] = bank;
}
#endif // GTA_REPLAY

u32 audSpeechManager::GetVariationForPain(u32 painVoiceType, u32 painType)
{
#if __BANK
	if(g_UseTestPainBanks)
		return 1;
#endif
	Assert(painType < AUD_PAIN_ID_NUM_BANK_LOADED);
	Assert(painVoiceType < AUD_PAIN_VOICE_NUM_TYPES);
	return m_NextVariationForPain[painVoiceType][painType];
}

#if GTA_REPLAY
void audSpeechManager::SetVariationForPain(u32 painVoiceType, u32 painType, u8 variation)
{
	if(Verifyf(painType < AUD_PAIN_ID_NUM_BANK_LOADED && painVoiceType < AUD_PAIN_VOICE_NUM_TYPES, "Invalid index into array"))
		m_NextVariationForPain[painVoiceType][painType] = variation;
}

u8 audSpeechManager::GetNumTimesPlayed(u32 painVoiceType, u32 painType) const
{
	if(Verifyf(painType < AUD_PAIN_ID_NUM_BANK_LOADED && painVoiceType < AUD_PAIN_VOICE_NUM_TYPES, "Invalid index into array"))
		return m_NumTimesPainPlayed[painVoiceType][painType];
	else
		return m_NumTimesPainPlayed[0][0];
}

u32 audSpeechManager::GetNextTimeToLoad(u32 painVoiceType) const
{
	if(Verifyf(painVoiceType < AUD_PAIN_VOICE_NUM_TYPES, "Invalid index into array"))
		return m_NextTimeToLoadPain[painVoiceType];
	else
		return m_NextTimeToLoadPain[0];
}
#endif // GTA_REPLAY

u32 audSpeechManager::GetWaveLoadedPainVoiceFromVoiceType(u32 painVoiceType, const CPed* ped)
{
	u32 voiceNameHash = 0;
	if(painVoiceType == AUD_PAIN_VOICE_TYPE_PLAYER && ped)
	{
		if(ped->GetPedType() == PEDTYPE_PLAYER_0)
		{
			voiceNameHash = ATSTRINGHASH("WAVELOAD_PAIN_MICHAEL", 0x6531A692);
		}
		else if(ped->GetPedType() == PEDTYPE_PLAYER_1)
		{
			voiceNameHash = ATSTRINGHASH("WAVELOAD_PAIN_FRANKLIN", 0x33F65FC3);
		}
		else if(ped->GetPedType() == PEDTYPE_PLAYER_2)
		{
			voiceNameHash = ATSTRINGHASH("WAVELOAD_PAIN_TREVOR", 0xCF83E9F);
		}
	}

	if(voiceNameHash == 0)
	{
		char voiceName[64];
		formatf(voiceName, "WAVELOAD_%s", g_PainSlotType[painVoiceType]);
		voiceNameHash = atStringHash(voiceName);
	}

	return voiceNameHash;
}

void audSpeechManager::UpdateBreathingBanks()
{
	if(!FindPlayerPed() || !FindPlayerPed()->GetSpeechAudioEntity() || FindPlayerPed()->GetSpeechAudioEntity()->IsAnimal())
		return;

	switch(FindPlayerPed()->GetPedType())
	{
	case PEDTYPE_PLAYER_0:
		m_BreathingTypeToLoad = AUD_BREATHING_TYPE_MICHAEL;
		break;
	case PEDTYPE_PLAYER_1:
		m_BreathingTypeToLoad = AUD_BREATHING_TYPE_FRANKLIN;
		break;
	case PEDTYPE_PLAYER_2:
		m_BreathingTypeToLoad = AUD_BREATHING_TYPE_TREVOR;
		break;
	default:
		m_BreathingTypeToLoad = AUD_BREATHING_TYPE_MICHAEL;
	}

	if(m_BreathingTypeToLoad != m_BreathingTypeCurrentlyLoaded)
	{
		m_NeedToLoad[AUD_PAIN_VOICE_TYPE_BREATHING] = true;
	}
}

const char* audSpeechManager::GetBreathingInfo(PlayerVocalEffectType eEffectType, u32& voiceNameHash, u8& variation)
{
	if(naVerifyf(eEffectType < AUD_NUM_PLAYER_VFX_TYPES, "Invalid vfxType passed into PlayedBreath()"))
	{
		variation = m_NextVariationForBreath[eEffectType];
		voiceNameHash = m_PainSlots[AUD_PAIN_VOICE_TYPE_BREATHING].VoiceNameHash;
		return g_BreathContexts[eEffectType];
	}
	return "";
}

void audSpeechManager::PlayedBreath(PlayerVocalEffectType eEffectType)
{
	if(naVerifyf(eEffectType < AUD_NUM_PLAYER_VFX_TYPES, "Invalid vfxType passed into PlayedBreath()"))
	{
		m_NextVariationForBreath[eEffectType]++;
		if(m_NextVariationForBreath[eEffectType] > m_BreathVariationsInCurrentBank[eEffectType])
		{
			m_NeedToLoad[AUD_PAIN_VOICE_TYPE_BREATHING] = true;
			m_NextVariationForBreath[eEffectType] = 1;
		}
	}
}

void audSpeechManager::UpdateAnimalBanks()
{
	static u32 sMinTimeBetweenFullUpdates = 4000;

#if __BANK
	if(g_DebugTennisLoad)
	{
		RequestTennisBanks(FindPlayerPed(), true);
		g_DebugTennisLoad = false;
	}

	if(g_DebugTennisUnload)
	{
		UnrequestTennisBanks();
		g_DebugTennisUnload = false;
	}
#endif

	if(m_TennisIsUsingAnimalBanks)
	{
		UpdateTennisBanks();
		return;
	}

	for(int i=0; i<m_AnimalBankSlots.GetCount(); ++i)
	{
		Assertf(m_AnimalBankSlots[i].WaveSlot, "Animal bank slot has gone NULL.  Please alert audio");
		if(m_AnimalBankSlots[i].IsLoading)
		{
			switch(m_AnimalBankSlots[i].WaveSlot->GetBankLoadingStatus(m_AnimalBankSlots[i].BankdID))
			{
				case audWaveSlot::FAILED:
					m_AnimalInfoArray[m_AnimalBankSlots[i].Owner].SlotNumber = -1;

					m_AnimalBankSlots[i].BankdID = ~0U;
					m_AnimalBankSlots[i].Owner = kAnimalNone;
					m_AnimalBankSlots[i].IsLoading = false;
					m_AnimalBankSlots[i].FailedLastTime = true;
					break;
				case audWaveSlot::LOADED:
					m_AnimalInfoArray[m_AnimalBankSlots[i].Owner].SlotNumber = (s8)i;
					m_AnimalBankSlots[i].IsLoading = false;
					break;
				default:
					break;
			}
		}
	}

	if(GetIsCurrentlyLoadingAnimalBank())
		return;

	if(m_LoadingAfterAnimalUpdate)
	{
		for(int i=0; i<m_AnimalBankSlots.GetCount(); ++i)
		{
			AnimalType animalType = m_SortedAnimalList[i].AnimalType;
			if(m_AnimalInfoArray[animalType].Priority <= 0.0f)
				m_LoadingAfterAnimalUpdate = false;
			else if(m_AnimalInfoArray[animalType].SlotNumber == -1)
			{
				LoadAnimalBank(animalType);
				if(i == m_AnimalBankSlots.GetCount() - 1)
					m_LoadingAfterAnimalUpdate = false;
				return;
			}
		}
	}
	

	for(int i=0; i<m_AnimalBankSlots.GetCount(); ++i)
	{
		for(int j=0; j<m_AnimalBankSlots[i].ContextInfo.GetCount(); ++j)
		{
			if(m_AnimalBankSlots[i].ContextInfo[j].UsedUp >= 0)
			{
				if(m_AnimalBankSlots[i].ContextInfo[j].UsedUp > 0)
					m_AnimalBankSlots[i].ContextInfo[j].UsedUp--;
				if(m_AnimalBankSlots[i].ContextInfo[j].UsedUp==0 && m_AnimalBankSlots[i].ContextInfo[j].ForcesSwap)
				{
					LoadAnimalBank(m_AnimalBankSlots[i].Owner, (s8)i);
					return;
				}
			}
		}
	}

	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(m_TimeOfLastFullAnimalBankUpdate + sMinTimeBetweenFullUpdates > currentTime && !m_ForceAnimalBankUpdate BANK_ONLY(&& !g_ForceAnimalBankUpdate))
		return;

	m_ForceAnimalBankUpdate = false;
	BANK_ONLY(g_ForceAnimalBankUpdate = false;)

	m_TimeOfLastFullAnimalBankUpdate = currentTime;

#if __BANK
	if(g_TestAnimalBankPriority)
	{
		for(int i=0; i<ANIMALTYPE_MAX; ++i)
		{
			m_NumAnimalAppearancesSinceLastUpdate[i] = fwRandom::GetRandomNumberInRange(0,10);
			m_NearestAnimalAppearancesSinceLastUpdate[i] = 1.0f;
		}
	}
#endif
	//safety valve to clear this value in case something wierd happens. Set the number of each type of animal making a sound
	//	to 0, if none of those animal are present.
	for(int i=0; i<ANIMALTYPE_MAX; ++i)
	{
		if(m_NumAnimalAppearancesSinceLastUpdate[i] == 0)
			m_NumAnimalsMakingSound[i] = 0;
	}

	SortAnimalList(&m_SortedAnimalList[0]);
	m_LoadingAfterAnimalUpdate = true;
}

void audSpeechManager::LoadAnimalBank(AnimalType animalId, s8 slotNum)
{
	u8 lowestPrioritySlotNumber = 0;

	for(int i=0; i<m_AnimalBankSlots.GetCount(); ++i)
	{
		if(m_AnimalBankSlots[i].Owner == animalId)
			slotNum = (s8)i;
	}

	if(slotNum < 0)
	{
		f32 lowestPriority = FLT_MAX;
	
		for(int i=0; i<m_AnimalBankSlots.GetCount(); ++i)
		{
			f32 priority = m_AnimalInfoArray[m_AnimalBankSlots[i].Owner].Priority;
			if(priority < lowestPriority)
			{
				lowestPriority = priority;
				lowestPrioritySlotNumber = (u8)i;
			}
		}
	}
	else
		lowestPrioritySlotNumber = slotNum;

	if(!Verifyf(animalId>kAnimalNone && animalId < ANIMALTYPE_MAX, "Invalid animal id %i passed to LoadAnimalBank", animalId))
		return;

	audAnimalBankSlot& animalSlot =  m_AnimalBankSlots[lowestPrioritySlotNumber];
	
	if(animalSlot.WaveSlot->GetReferenceCount() != 0)
		return;
	if(!Verifyf(!animalSlot.WaveSlot->GetIsLoading(), "Trying to load animal bank that is already currently loading."))
		return;

	//Set the animal info structs so that the animal getting kicked out takes a slot number of -1
	if(m_AnimalBankSlots[lowestPrioritySlotNumber].Owner != kAnimalNone)
		m_AnimalInfoArray[m_AnimalBankSlots[lowestPrioritySlotNumber].Owner].SlotNumber = -1;

	char voiceName[64];
	char bankName[64];
	formatf(voiceName, "%s_NEAR_%.2d", g_AnimalBankNames[animalId], m_NextAnimalBankNumToLoad[animalId]);
	formatf(bankName, "ANIMALS_NEAR\\%s", voiceName);

	Displayf("Loading animal bank : %s", bankName);

	animalSlot.BankdID = audWaveSlot::FindBankId(bankName);
	if(!animalSlot.WaveSlot->LoadBank(animalSlot.BankdID))
		return;

	m_NextAnimalBankNumToLoad[animalId] = (m_NextAnimalBankNumToLoad[animalId] % m_NumAnimalBanks[animalId]) + 1;

	const AnimalParams* params = m_AnimalInfoArray[animalId].Data;
	if(!params)
		return;

	animalSlot.ContextInfo.Reset();

	u32 voiceHash = atStringHash(voiceName);
	for(int i=0; i<params->numContexts; i++)
	{
			audAnimalContextInfo contextInfo;
			contextInfo.ContextPHash = atPartialStringHash(params->Context[i].ContextName);
			contextInfo.NumVars = audSpeechSound::FindNumVariationsForVoiceAndContext(voiceHash, params->Context[i].ContextName);
			if(contextInfo.NumVars != 0)
			{
				naAssertf(params->Context[i].MaxTimeBetween >= params->Context[i].MinTimeBetween, "Animal context has min time greater than its max time. Voicehash %u Context %s",
								voiceHash, params->Context[i].ContextName);
				contextInfo.NextTimeCanPlay = 0;
				contextInfo.MinTimeBetween = params->Context[i].MinTimeBetween;
				contextInfo.MaxTimeBetween = params->Context[i].MaxTimeBetween;
				contextInfo.LastVarUsed = 0;
				contextInfo.ForcesSwap = params->Context[i].ContextBits.BitFields.SwapWhenUsedUp;
				contextInfo.UsedUp = -1;
				animalSlot.ContextInfo.PushAndGrow(contextInfo);
			}
	}

	//Fill in info for the audAnimalBankSlot
	animalSlot.Owner = animalId;
	animalSlot.IsLoading = true;
	animalSlot.FailedLastTime = false;
	animalSlot.VoiceName = voiceHash;
}

bool audSpeechManager::GetIsCurrentlyLoadingAnimalBank()
{
	for(int i=0; i<m_AnimalBankSlots.GetCount(); ++i)
	{
		if(m_AnimalBankSlots[i].IsLoading)
			return true;
	}
	return false;
}

void audSpeechManager::UpdateTennisBanks()
{
	if(!m_TennisIsUsingAnimalBanks)
		return;

	if(m_CurrentTennisSlotLoading == -1)
	{
		m_CurrentTennisSlotLoading = 0;
		LoadTennisBanks();
	}
	if(m_CurrentTennisSlotLoading < 2)
	{
		audAnimalBankSlot& tennisSlot = m_AnimalBankSlots[m_CurrentTennisSlotLoading];
		audWaveSlot::audWaveSlotLoadStatus loadState = tennisSlot.WaveSlot->GetBankLoadingStatus(tennisSlot.BankdID);
		if(loadState == audWaveSlot::LOADING)
		{
			return;
		}
		else
		{
			if(loadState == audWaveSlot::FAILED)
			{
				naWarningf("Tennis audio bank failed to load");
			}
			m_CurrentTennisSlotLoading++;
			
			if(m_CurrentTennisSlotLoading >= 2)
			{
				return;
			}
			else
			{
				LoadTennisBanks();
			}
		}
	}
}

void audSpeechManager::RequestTennisBanks(CPed* opponent BANK_ONLY(, bool debuggingTennis))
{
	if(!opponent)
	{
		naWarningf("Not loading tennis audio bank.  Invalid opponent ped.");
		return;
	}

#if __BANK
	if(!debuggingTennis)
#endif
	{
		scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();
		naCErrorf(scriptThreadId, "Failed to get thread id");
		m_TennisScriptIndex = g_ScriptAudioEntity.GetIndexFromScriptThreadId(scriptThreadId);

	}
	
	if (!naVerifyf(m_TennisScriptIndex>=0 BANK_ONLY(|| debuggingTennis), "No valid scriptIndex: %s; %d", CTheScripts::GetCurrentScriptName(), m_TennisScriptIndex))
	{
		return;
	}

	m_CurrentTennisSlotLoading = -1;
	m_TennisIsUsingAnimalBanks = true;
	for(int i =0; i<m_AnimalBankSlots.GetCount(); ++i)
	{
		m_AnimalBankSlots[i].ContextInfo.clear();
		m_AnimalBankSlots[i].Owner = kAnimalNone;
	}
	for(int i=0; i<ANIMALTYPE_MAX; i++)
	{
		m_AnimalInfoArray[i].SlotNumber = -1;
	}

	m_TennisOpponent = opponent;
	m_TennisOpponentIsMale = opponent->IsMale();
	m_TennisPlayerIsMichael = false;
	m_TennisPlayerIsFranklin = false;
	m_TennisPlayerIsTrevor = false;
}

void audSpeechManager::UnrequestTennisBanks()
{
	m_CurrentTennisSlotLoading = -1;
	m_TennisIsUsingAnimalBanks = false;
	m_ForceAnimalBankUpdate = true;
	m_TennisOpponent = NULL;
}

void audSpeechManager::LoadTennisBanks()
{
	if(naVerifyf(m_AnimalBankSlots.GetCount() >= 2, "Fewer than 2 animal slots.  Not going to try and load tennis."))
	{
		if(m_CurrentTennisSlotLoading >= 2)
		{
			naWarningf("Trying to load tennis bank with invalid m_CurrentTennisSlotLoading.");
			return;
		}

		audAnimalBankSlot& tennisSlot = m_AnimalBankSlots[m_CurrentTennisSlotLoading];
		if(tennisSlot.WaveSlot->GetReferenceCount() != 0)
			return;
		if(!Verifyf(!tennisSlot.WaveSlot->GetIsLoading(), "Trying to load tennis bank that is already currently loading."))
			return;

		char voiceName[64];
		char bankName[64];

		//0th slot is for player
		if(m_CurrentTennisSlotLoading == 0)
		{
			if(!FindPlayerPed())
			{
				m_CurrentTennisSlotLoading = 1;
				naWarningf("Can't find player ped.  Won't be loading their tennis bank.");
				return;
			}

			switch(FindPlayerPed()->GetPedType())
			{
			case PEDTYPE_PLAYER_0: //MICHAEL
				safecpy(voiceName, "TENNIS_VFX_MICHAEL");
				safecpy(bankName, "ANIMALS_NEAR\\TENNIS_VFX_MICHAEL");		
				m_TennisPlayerIsMichael = true;
				break;
			case PEDTYPE_PLAYER_1: //FRANKLIN
				safecpy(voiceName, "TENNIS_VFX_FRANKLIN");
				safecpy(bankName, "ANIMALS_NEAR\\TENNIS_VFX_FRANKLIN");				
				m_TennisPlayerIsFranklin = true;
				break;
			case PEDTYPE_PLAYER_2: //TREVOR
				safecpy(voiceName, "TENNIS_VFX_TREVOR");
				safecpy(bankName, "ANIMALS_NEAR\\TENNIS_VFX_TREVOR");	
				m_TennisPlayerIsTrevor = true;
				break;
			default:
				break;
			}
		}
		else if(m_CurrentTennisSlotLoading == 1)
		{
			if(m_TennisOpponentIsMale)
			{
				safecpy(voiceName, "TENNIS_VFX_MALE1");
				safecpy(bankName, "ANIMALS_NEAR\\TENNIS_VFX_MALE1");	
			}
			else
			{
				safecpy(voiceName, "TENNIS_VFX_FEMALE1");
				safecpy(bankName, "ANIMALS_NEAR\\TENNIS_VFX_FEMALE1");
			}
		}
		else
		{
			return;
		}
		

		tennisSlot.BankdID = audWaveSlot::FindBankId(bankName);
		if(!tennisSlot.WaveSlot->LoadBank(tennisSlot.BankdID))
			return;

		u32 voiceHash = atStringHash(voiceName);

		//Fill in info for the audAnimalBankSlot
		tennisSlot.IsLoading = true;
		tennisSlot.FailedLastTime = false;
		tennisSlot.VoiceName = voiceHash;

		char context[64];
		formatf(context, "%s_LITE", voiceName);
		m_NumTennisContextPHashes[m_CurrentTennisSlotLoading][TENNIS_VFX_LITE] = atPartialStringHash(context);
		m_NumTennisVariations[m_CurrentTennisSlotLoading][TENNIS_VFX_LITE] = audSpeechSound::FindNumVariationsForVoiceAndContext(voiceHash, atPartialStringHash(context));
		m_LastTennisVariations[m_CurrentTennisSlotLoading][TENNIS_VFX_LITE] = 0;
		formatf(context, "%s_MED", voiceName);
		m_NumTennisContextPHashes[m_CurrentTennisSlotLoading][TENNIS_VFX_MED] = atPartialStringHash(context);
		m_NumTennisVariations[m_CurrentTennisSlotLoading][TENNIS_VFX_MED] = audSpeechSound::FindNumVariationsForVoiceAndContext(voiceHash, atPartialStringHash(context));
		m_LastTennisVariations[m_CurrentTennisSlotLoading][TENNIS_VFX_MED] = 0;
		formatf(context, "%s_STRONG", voiceName);
		m_NumTennisContextPHashes[m_CurrentTennisSlotLoading][TENNIS_VFX_STRONG] = atPartialStringHash(context);
		m_NumTennisVariations[m_CurrentTennisSlotLoading][TENNIS_VFX_STRONG] = audSpeechSound::FindNumVariationsForVoiceAndContext(voiceHash, atPartialStringHash(context));
		m_LastTennisVariations[m_CurrentTennisSlotLoading][TENNIS_VFX_STRONG] = 0;
	}
}

audWaveSlot* audSpeechManager::GetTennisVocalizationInfo(CPed* m_Parent, const TennisVocalEffectType& tennisVFXType, u32& contextPhash, u32& voiceHash, 
													u32& variation)
{
	if(!m_Parent || tennisVFXType >= AUD_NUM_TENNIS_VFX_TYPES || tennisVFXType < 0)
		return NULL;

	u8 speakerTypeIndex = 0; //0 for player
	switch(m_Parent->GetPedType())
	{
	case PEDTYPE_PLAYER_0: //MICHAEL	
		if(!m_TennisPlayerIsMichael)
			return NULL;
		break;
	case PEDTYPE_PLAYER_1: //FRANKLIN	
		if(!m_TennisPlayerIsFranklin)
			return NULL;
		break;
	case PEDTYPE_PLAYER_2: //TREVOR
		if(!m_TennisPlayerIsTrevor)
			return NULL;
		break;
	default:
		speakerTypeIndex = 1;
		if(m_Parent != m_TennisOpponent)
			return NULL;
		if( (m_Parent->IsMale() && !m_TennisOpponentIsMale) || (!m_Parent->IsMale() && m_TennisOpponentIsMale) )
			return NULL;
		break;
	}

	contextPhash = m_NumTennisContextPHashes[speakerTypeIndex][tennisVFXType];
	voiceHash = m_AnimalBankSlots[speakerTypeIndex].VoiceName;
	if(m_NumTennisVariations[speakerTypeIndex][tennisVFXType] > 1)
	{
		variation = audEngineUtil::GetRandomNumberInRange(1, m_NumTennisVariations[speakerTypeIndex][tennisVFXType]);
		if(variation == m_LastTennisVariations[speakerTypeIndex][tennisVFXType])
		{
			variation = (variation % m_NumTennisVariations[speakerTypeIndex][tennisVFXType]) + 1;
		}
	}
	else
	{
		variation = m_NumTennisVariations[speakerTypeIndex][tennisVFXType];
	}
	m_LastTennisVariations[speakerTypeIndex][tennisVFXType] = variation;

	return m_AnimalBankSlots[speakerTypeIndex].WaveSlot;
}

int CbCompareAnimalStructs(const void* pVoidA, const void* pVoidB)
{
	const audAnimalQSortStruct* pA = (const audAnimalQSortStruct*)pVoidA;
	const audAnimalQSortStruct* pB = (const audAnimalQSortStruct*)pVoidB;

	return pA->Priority > pB->Priority ? -1 : (pA->Priority == pB->Priority ? 0 : 1);
}

void audSpeechManager::SortAnimalList(audAnimalQSortStruct* outList)
{
	CPed* playerPed = FindPlayerPed();
	s32 playerAnimalIndex = ANIMALTYPE_MAX;
	if(playerPed && playerPed->GetSpeechAudioEntity() && playerPed->GetSpeechAudioEntity()->IsAnimal())
	{
		playerAnimalIndex = playerPed->GetSpeechAudioEntity()->GetAnimalType();
	}

	for(int i=0; i<ANIMALTYPE_MAX; ++i)
	{
		//Do whatever kind of priority calculations we want to here.  For now, just take the number of times this kind of
		//	animal has been seen since the last frame and multiply by its base priority.
		AnimalParams* params = m_AnimalInfoArray[i].Data;
		m_AnimalInfoArray[i].Priority = 0.0f;
		if(i == playerAnimalIndex)
		{
			m_AnimalInfoArray[i].Priority = 10000.0f; // boost the players animal bank to make sure their bank gets loaded
		}
		if(params)
		{
			f32 distanceScalar = m_NearestAnimalAppearancesSinceLastUpdate[i] < 0.0f ?
				0.0f :
				Clamp( 1.0f - (m_NearestAnimalAppearancesSinceLastUpdate[i]/ (params->MaxDistanceToBankLoad * params->MaxDistanceToBankLoad) ), 0.05f, 1.0f );
			m_AnimalInfoArray[i].Priority = Max(4.0f, (f32)m_NumAnimalAppearancesSinceLastUpdate[i]) * distanceScalar * (f32)params->BasePriority;
		}

		outList[i].AnimalType = (AnimalType)i;
		outList[i].Priority = m_AnimalInfoArray[i].Priority;

		m_NumAnimalAppearancesSinceLastUpdate[i] = 0;
		m_NearestAnimalAppearancesSinceLastUpdate[i] = -1.0f;
	}

	qsort(outList,ANIMALTYPE_MAX,sizeof(audAnimalQSortStruct),CbCompareAnimalStructs);
}

s32 audSpeechManager::GetVariationForNearAnimalContext(AnimalType animalType, const char* context)
{
	return GetVariationForNearAnimalContext(animalType, atPartialStringHash(context));
}

s32 audSpeechManager::GetVariationForNearAnimalContext(AnimalType animalType, u32 contextPHash)
{
	if(m_AnimalInfoArray[animalType].SlotNumber == -1)
		return -1;

	audAnimalBankSlot& bankSlot = m_AnimalBankSlots[m_AnimalInfoArray[animalType].SlotNumber];
	for(int i=0; i<bankSlot.ContextInfo.GetCount(); ++i)
	{
		if(bankSlot.ContextInfo[i].ContextPHash == contextPHash)
		{
			if(bankSlot.ContextInfo[i].MinTimeBetween >= 0 &&
				bankSlot.ContextInfo[i].NextTimeCanPlay > g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
				return -1;

			bankSlot.ContextInfo[i].LastVarUsed++;
			if(bankSlot.ContextInfo[i].LastVarUsed > bankSlot.ContextInfo[i].NumVars)
			{
				bankSlot.ContextInfo[i].LastVarUsed = 1;
				if(bankSlot.ContextInfo[i].UsedUp == -1)
					bankSlot.ContextInfo[i].UsedUp = 3;
			}
			return bankSlot.ContextInfo[i].LastVarUsed;
		}
	}

	return -1;
}

void audSpeechManager::NearAnimalContextPlayed(AnimalType animalType, u32 contextPHash)
{
	if(m_AnimalInfoArray[animalType].SlotNumber == -1)
		return;

	audAnimalBankSlot& bankSlot = m_AnimalBankSlots[m_AnimalInfoArray[animalType].SlotNumber];
	for(int i=0; i<bankSlot.ContextInfo.GetCount(); ++i)
	{
		if(bankSlot.ContextInfo[i].ContextPHash == contextPHash)
		{
			if(bankSlot.ContextInfo[i].MaxTimeBetween != -1)
			{
				s32 timeTilNext = audEngineUtil::GetRandomNumberInRange(bankSlot.ContextInfo[i].MinTimeBetween, bankSlot.ContextInfo[i].MaxTimeBetween);
				bankSlot.ContextInfo[i].NextTimeCanPlay = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + timeTilNext;
			}
			return;
		}
	}
}

void audSpeechManager::FarAnimalContextPlayed(u32 contextPHash, u32 timeInMs)
{
	s32 contextIndex = -1;
	for(int i=0; i<NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS && contextIndex<0; i++)
	{
		if(g_TimeLimitedAnimalFarContextsPHashes[i] == contextPHash)
			contextIndex = i;
	}

	if(contextIndex < 0)
		return;

	g_TimeLimitedAnimalFarContextsLastPlayTime[contextIndex] = timeInMs;
}

bool audSpeechManager::CanFarAnimalContextPlayed(u32 contextPHash, u32 timeInMs)
{
	s32 contextIndex = -1;
	for(int i=0; i<NUM_TIME_LIMITED_ANIMAL_FAR_CONTEXTS && contextIndex<0; i++)
	{
		if(g_TimeLimitedAnimalFarContextsPHashes[i] == contextPHash)
			contextIndex = i;
	}

	if(contextIndex < 0)
		return true;

	return g_TimeLimitedAnimalFarContextsLastPlayTime[contextIndex] + g_TimeLimitedAnimalFarContextsMinTimeBetween[contextIndex] < timeInMs;
}

void audSpeechManager::GetWaveSlotAndVoiceNameForAnimal(AnimalType animalType, audWaveSlot*& waveSlot, u32& voiceName)
{
	if(m_AnimalInfoArray[animalType].SlotNumber == -1 || m_AnimalBankSlots[m_AnimalInfoArray[animalType].SlotNumber].IsLoading ||
		 m_AnimalBankSlots[m_AnimalInfoArray[animalType].SlotNumber].Owner != animalType)
		return;

	waveSlot = m_AnimalBankSlots[m_AnimalInfoArray[animalType].SlotNumber].WaveSlot;
	voiceName = m_AnimalBankSlots[m_AnimalInfoArray[animalType].SlotNumber].VoiceName;
}

void audSpeechManager::SetIfNearestAnimalInstance(AnimalType animalType, Vec3V_In pos)
{
	float distSq = VEC3V_TO_VECTOR3((g_AudioEngine.GetEnvironment().GetVolumeListenerPosition() - pos)).Mag2();
	if(m_NearestAnimalAppearancesSinceLastUpdate[animalType] < 0.0f || 
	   m_NearestAnimalAppearancesSinceLastUpdate[animalType] > distSq)
	{
		m_NearestAnimalAppearancesSinceLastUpdate[animalType] = distSq;
	}
}

void audSpeechManager::IncrementAnimalsMakingSound(AnimalType animalType)
{
	Assert(animalType < ANIMALTYPE_MAX);
	m_NumAnimalsMakingSound[animalType]++;
}

void audSpeechManager::DecrementAnimalsMakingSound(AnimalType animalType)
{
	if(m_NumAnimalsMakingSound[animalType] == 0)
	{
		Warningf("Attempting to decrement m_NumAnimalsMakingSound when it's already 0.");
		return;
	}
	Assert(animalType < ANIMALTYPE_MAX);
	m_NumAnimalsMakingSound[animalType]--;
}

u8 audSpeechManager::GetNumAnimalsMakingSound(AnimalType animalType)
{
	Assert(animalType < ANIMALTYPE_MAX);
	return m_NumAnimalsMakingSound[animalType];
}

bool audSpeechManager::IsStreamingBusyForContext(u32 UNUSED_PARAM(contextPHash))
{
	if (g_StreamingSchmeaming)
	{
		return false;
	}
	// Don't play ambient speech if streaming stuff right in front of us.
	if (strStreamingEngine::GetIsLoadingPriorityObjects())
	{
		return true;
	}
	// What the hell, let's just play everything if streaming's not busy - no reason not to
	return false;
	/*
	// For now, just see how fast the player car's going, and don't allow anything other than DIVE or CRASH_CAR/etc if it's quick.
	if (!strcmp(context, "DIVE") || !strcmp(context, "CRASH_CAR") || !strcmp(context, "CRASH_BIKE") || !strcmp(context, "CRASH_GENERIC") ||
		!strcmp(context, "CRASH_CAR_DRIVEN"))
	{
		return false;
	}
	CVehicle* pVehicle = CGameWorld::FindLocalPlayerVehicle();
	if (pVehicle && pVehicle->GetMoveSpeed().Mag2() > 40.0f)
	{
		return true;
	}
	return false;
	*/
}

void audSpeechManager::ReserveAmbientSpeechSlotForScript()
{
	if(m_ScriptOwningReservedSlot != 0)
		return;

	scrThreadId scriptID = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();

	bool hadToStop;
	s32 slot = GetAmbientSpeechSlot(NULL, &hadToStop, -1.0f);
	if(!hadToStop)
	{
		Assign(m_ScriptReservedSlotNum, (u32)slot);
		m_ScriptOwningReservedSlot = scriptID;
		m_AmbientSpeechSlots[m_ScriptReservedSlotNum].PlayState = AUD_SPEECH_SLOT_VACANT;
		g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(slot, 0, 
#if __BANK
			"SCRIPT_RESERVED",
#endif
			0);
	}
	else
	{
		FreeAmbientSpeechSlot(slot, true);
	}
}

void audSpeechManager::ReleaseAmbientSpeechSlotForScript()
{
	scrThreadId scriptID = CTheScripts::GetCurrentGtaScriptThread()->GetThreadId();

	if(m_ScriptOwningReservedSlot == 0)
	{
#if !__FINAL
		GtaThread* pThread = static_cast<GtaThread*>(scrThread::GetThread(scriptID));
		Warningf("Script with id %u and name %s attempting to release reserved speech slot when no speech slot has been reserved.", 
			scriptID, pThread ? pThread->GetScriptName(): "unknown");	
#endif
		return;
	}

	if(scriptID != m_ScriptOwningReservedSlot)
	{
#if !__FINAL
		GtaThread* pThread1 = static_cast<GtaThread*>(scrThread::GetThread(scriptID));
		GtaThread* pThread2 = static_cast<GtaThread*>(scrThread::GetThread(m_ScriptOwningReservedSlot));
		Warningf("Script with id %u and name %s attempting to release reserved speech slot when it is actually reserved by script with id %u and name %s.", 
			scriptID, pThread1 ? pThread1->GetScriptName(): "unknown", m_ScriptOwningReservedSlot, pThread2 ? pThread2->GetScriptName(): "unknown");	
#endif
		return;
	}


	FreeAmbientSpeechSlot(m_ScriptReservedSlotNum, true);

	m_ScriptReservedSlotNum = 0;
	m_ScriptOwningReservedSlot = THREAD_INVALID;
}

#if GTA_REPLAY
void audSpeechManager::FreeAllAmbientSpeechSlots()
{
	if(m_ChurnSpeechSound)
	{
		m_ChurnSpeechSound->StopAndForget();
	}

	for (int i=0; i<g_MaxAmbientSpeechSlots; i++)
	{
		m_AmbientSpeechSlots[i].SpeechAudioEntity = NULL;
		m_AmbientSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_STOPPING;
	
		Assert(m_AmbientSpeechSlots[i].WaveSlot);
		if (m_AmbientSpeechSlots[i].WaveSlot->GetReferenceCount() == 0)
		{
			// Slot's finished
			m_AmbientSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_VACANT;
		}
	}
}
#endif

s32 audSpeechManager::GetAmbientSpeechSlot(audSpeechAudioEntity* requestingEntity, bool* hadToStopSpeech, f32 priority, scrThreadId scriptThreadId
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
											, const char* context
#endif
	, bool isHeli)
{

	if(hadToStopSpeech)
		*hadToStopSpeech = false;

	if(m_ScriptOwningReservedSlot != 0 && scriptThreadId == m_ScriptOwningReservedSlot && 
		m_AmbientSpeechSlots[m_ScriptReservedSlotNum].PlayState == AUD_SPEECH_SLOT_VACANT)
	{
		m_AmbientSpeechSlots[m_ScriptReservedSlotNum].PlayState = AUD_SPEECH_SLOT_ALLOCATED;
		m_AmbientSpeechSlots[m_ScriptReservedSlotNum].SpeechAudioEntity = requestingEntity;
		return m_ScriptReservedSlotNum;
	}

	f32 requestingPriority = GetRealAmbientSpeechPriority(requestingEntity, priority);
	f32 lowestAllocatedPriority = -1.0f;
	s32 lowestPrioritySlot = -1;
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
	atRangeArray<f32, g_MaxAmbientSpeechSlots+1> debugPriority;
	atRangeArray<f32, g_MaxAmbientSpeechSlots+1> debugRealPriority;
	atRangeArray<f32, g_MaxAmbientSpeechSlots+1> debugDist2;

	debugPriority[g_MaxAmbientSpeechSlots] = priority;
	debugRealPriority[g_MaxAmbientSpeechSlots] = requestingPriority;
	debugDist2[g_MaxAmbientSpeechSlots] = 0.0f;
	if(requestingEntity)
	{
		CPed* parent = requestingEntity->GetParent();
		if(parent)
			debugDist2[g_MaxAmbientSpeechSlots] = (VEC3V_TO_VECTOR3(parent->GetMatrix().d()) - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
	}
#endif

	// See if we've got a vacant slot. Don't allocate STOPPING ones for just now, for simplicity. And don't allow forcing out of less important ones.
	for (s32 index=0; index<g_MaxAmbientSpeechSlots; index++)
	{
		int i = (m_AmbientSpeechSlotToNextSearchFrom + index) % g_MaxAmbientSpeechSlots;
		if(m_AmbientSpeechSlots[i].PlayState == AUD_SPEECH_SLOT_VACANT)
		{
			//Allocate it, and return the slot number.
			m_AmbientSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_ALLOCATED;
			m_AmbientSpeechSlots[i].SpeechAudioEntity = requestingEntity;
			m_AmbientSpeechSlots[i].Priority = priority;
			m_AmbientSpeechSlotToNextSearchFrom = (u8)((i+1)%g_MaxAmbientSpeechSlots);
			return i;
		}

		f32 allocatedPriority = GetRealAmbientSpeechPriority(m_AmbientSpeechSlots[i].SpeechAudioEntity, m_AmbientSpeechSlots[i].Priority);
		//For now, only kick speech off if it has a valid entity.  Otherwise, we'll end up trying to call FreeSlot on the NULL entity and crashing.
		//Even if we just free the slot through the speech manager, this really only marks it as stopping.  We'll need a pointer to the sound
		//	or the ability to stop all sounds on a slot to really make it worth the effort of stealing slots with no entity.
		if(m_AmbientSpeechSlots[i].SpeechAudioEntity && (lowestAllocatedPriority==-1 || allocatedPriority<lowestAllocatedPriority))
		{
			lowestPrioritySlot = i;
			lowestAllocatedPriority = allocatedPriority;
		}
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
		debugPriority[i] = m_AmbientSpeechSlots[i].Priority;
		debugRealPriority[i] = allocatedPriority;
		debugDist2[i] = 0.0f;
		if(m_AmbientSpeechSlots[i].SpeechAudioEntity)
		{
			CPed* parent = m_AmbientSpeechSlots[i].SpeechAudioEntity->GetParent();
			if(parent)
				debugDist2[i] = (VEC3V_TO_VECTOR3(parent->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		}
#endif
	}

	//Some things, such as animal sounds or requests from script to reserve a slot, should never kick stuff out of slots.
	if(priority < 0.0f)
		return -1;

	// We don't have any free.  See if the lowest priority allocated slot is worth kicking out.
	// For now, don't kick out already playing speech for something that doesn't have a valid requestingEntity
	// Also, don't allow this to happen if we don't have a valid hadToStopSpeech pointer through which to notify our speechEntity that it
	//		should go into deferred speech mode.
	// The check for a valid SpeechAudioEntity shouldn't be required, as this is checked before we mark the slot as lowest priority.
	if(hadToStopSpeech && (requestingEntity || isHeli) && lowestPrioritySlot>-1 && requestingPriority > lowestAllocatedPriority
		&& m_AmbientSpeechSlots[lowestPrioritySlot].SpeechAudioEntity)
	{
		m_AmbientSpeechSlots[lowestPrioritySlot].SpeechAudioEntity->FreeSlot();

		*hadToStopSpeech = true;
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
		audWarningf("[DEFERRED SPEECH] Incoming: Context: %s, Prio: %.02f, Real Prio: %.02f, Dist2: %.02f", context, debugPriority[g_MaxAmbientSpeechSlots], debugRealPriority[g_MaxAmbientSpeechSlots], debugDist2[g_MaxAmbientSpeechSlots]);

		for (u32 i = 0; i < g_MaxAmbientSpeechSlots; i++)
		{
			char cxt[64]; 
			cxt[0] = '\0';

			if (m_AmbientSpeechSlots[i].SpeechAudioEntity)
			{
				m_AmbientSpeechSlots[i].SpeechAudioEntity->GetCurrentlyPlayingWaveName(cxt);
			}

			audWarningf("[DEFERRED SPEECH] Slot %d: Context: %s, Prio: %.02f, Real Prio: %.02f, Dist2: %.02f", i, cxt, debugPriority[i], debugRealPriority[i], debugDist2[i]);
		}
		
		audWarningf("[DEFERRED SPEECH] Slot Given: %i, from entity %p to entity %p", lowestPrioritySlot, m_AmbientSpeechSlots[lowestPrioritySlot].SpeechAudioEntity, requestingEntity);
#endif
		return lowestPrioritySlot;
	}
	

	return -1;
}

f32 audSpeechManager::GetRealAmbientSpeechPriority(audSpeechAudioEntity* requestingEntity, f32 priority)
{
	if(requestingEntity)
	{
		CPed* parent = requestingEntity->GetParent();
		if(parent)
		{
			f32 dist2 = (VEC3V_TO_VECTOR3(parent->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
			priority = g_DistanceSqToPriorityScalarCurve.CalculateValue(dist2) * priority;
		}
	}

	return priority;
}

void audSpeechManager::MarkSlotAsAllocated(int slotIndex, audSpeechAudioEntity* requestingEntity, f32 priority)
{
	naAssertf(m_AmbientSpeechSlots[slotIndex].PlayState==AUD_SPEECH_SLOT_VACANT || (m_AmbientSpeechSlots[slotIndex].PlayState==AUD_SPEECH_SLOT_ALLOCATED && m_AmbientSpeechSlots[slotIndex].SpeechAudioEntity == requestingEntity), "Trying to mark speech slot as allocated when it isn't vacant yet.  THIS IS VERY BAD!!!");
	m_AmbientSpeechSlots[slotIndex].PlayState = AUD_SPEECH_SLOT_ALLOCATED;
	m_AmbientSpeechSlots[slotIndex].SpeechAudioEntity = requestingEntity;
	m_AmbientSpeechSlots[slotIndex].Priority = priority;
}

void audSpeechManager::FreeAmbientSpeechSlot(s32 slot, bool BANK_ONLY(okForEntityToBeNull))
{
	// Remove the audio entity reference, and mark the slot as stopping or vacant, depending on the wave slot's state.
	// (smoother to just let the Update() do the STOPPING->VACANT change, but it'd be good to synchronously free the slot - bound to need that at some point)
	naAssertf(slot>=0 && slot<g_MaxAmbientSpeechSlots, "Slot index is out of bounds");
#if __BANK
	if(m_AmbientSpeechSlots[slot].SpeechAudioEntity == NULL && !okForEntityToBeNull)
	{
		REPLAY_ONLY(if(!CReplayMgr::IsEditModeActive()))
		{
			Warningf("Ambient speech slot has no speech audio entity and it's not ok for it to be null when attempting to free ambient speech");
		}
	}
#endif
	m_AmbientSpeechSlots[slot].SpeechAudioEntity = NULL;
	m_AmbientSpeechSlots[slot].Priority = 0;
	if (m_AmbientSpeechSlots[slot].WaveSlot->GetReferenceCount() == 0)
	{
		m_AmbientSpeechSlots[slot].PlayState = AUD_SPEECH_SLOT_VACANT;
	}
	else
	{
		m_AmbientSpeechSlots[slot].PlayState = AUD_SPEECH_SLOT_STOPPING;

		//RK - TODO: Deal with Walla
	}
}

void audSpeechManager::PopulateAmbientSpeechSlotWithPlayingSpeechInfo(s32 slot, u32 voiceName, 
#if __BANK
																	  const char* context, 
#endif
																	  s32 variation)
{
	naAssertf(slot>=0 && slot<g_MaxAmbientSpeechSlots, "Slot index passed into PopulateAmbientSpeechSlotWithPlayingSpeechInfo is out of bounds");
	m_AmbientSpeechSlots[slot].Voicename = voiceName;
#if __BANK
	sprintf(m_AmbientSpeechSlots[slot].Context, "%s", context);
#endif
	m_AmbientSpeechSlots[slot].VariationNumber = variation;	
}

bool audSpeechManager::IsSlotVacant(s32 slotIndex)
{
	naAssertf(slotIndex>=0 && slotIndex<g_MaxAmbientSpeechSlots, "Slot index passed into PopulateAmbientSpeechSlotWithPlayingSpeechInfo is out of bounds");
	return m_AmbientSpeechSlots[slotIndex].PlayState == AUD_SPEECH_SLOT_VACANT;
}

bool audSpeechManager::IsSafeToReuseSlot(s32 slotIndex, const audSpeechAudioEntity* entity)
{
	naAssertf(slotIndex >= 0 && slotIndex < g_MaxAmbientSpeechSlots, "Slot index passed into CanSlotBeReused is out of bounds");
	return m_AmbientSpeechSlots[slotIndex].PlayState == AUD_SPEECH_SLOT_ALLOCATED && m_AmbientSpeechSlots[slotIndex].SpeechAudioEntity == entity && m_AmbientSpeechSlots[slotIndex].WaveSlot->GetReferenceCount() == 0;
}

bool audSpeechManager::IsScriptSpeechSlotVacant(s32 slotIndex)
{
	naAssertf(slotIndex>=0 && slotIndex<g_MaxScriptedSpeechSlots, "Slot index passed into IsScriptSpeechSlotVacant is out of bounds");
	return m_ScriptedSpeechSlots[slotIndex].PlayState == AUD_SPEECH_SLOT_VACANT;
}

s32 audSpeechManager::GetScriptSpeechSlotRefCount(s32 slotIndex)
{
	if(m_ScriptedSpeechSlots[slotIndex].WaveSlot)
	{
		return m_ScriptedSpeechSlots[slotIndex].WaveSlot->GetReferenceCount();
	}
	return 0;
}


s32 audSpeechManager::GetScriptedSpeechSlot(audSpeechAudioEntity* requestingEntity)
{
	// See if we've got a vacant slot. Don't allocate STOPPING ones for just now, for simplicity. 
	// We may do this differently - is it ever acceptable to not give a script a slot when it thinks it needs one?
	for (s32 i=0; i<g_MaxScriptedSpeechSlots; i++)
	{
		if (m_ScriptedSpeechSlots[i].PlayState == AUD_SPEECH_SLOT_VACANT)
		{
			// Allocate it, and return the slot number.
			m_ScriptedSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_ALLOCATED;
			m_ScriptedSpeechSlots[i].SpeechAudioEntity = requestingEntity;
			return i;
		}
	}
	// We don't have any free.
	return -1;
}

u32 audSpeechManager::GetNumVacantAmbientSlots()
{
	u32 numVacant = 0;

	for(s32 loop = 0; loop < g_MaxAmbientSpeechSlots; loop++)
	{
		if(m_AmbientSpeechSlots[loop].PlayState == AUD_SPEECH_SLOT_VACANT)
		{
			numVacant++;
		}
	}

	return numVacant;
}

void audSpeechManager::FreeAllScriptedSpeechSlots()
{
	for(int i=0; i<g_MaxScriptedSpeechSlots; i++)
	{
		FreeScriptedSpeechSlot(i);
	}
}

void audSpeechManager::FreeScriptedSpeechSlot(s32 slot)
{
	// Remove the audio entity reference, and mark the slot as stopping or vacant, depending on the wave slot's state.
	// (smoother to just let the Update() do the STOPPING->VACANT change, but it'd be good to synchronously free the slot - bound to need that at some point)
	naAssertf(slot>=0 && slot<g_MaxScriptedSpeechSlots, "Slot index %d passed into FreeScriptedSpeechSlot is invalid", slot);
	m_ScriptedSpeechSlots[slot].SpeechAudioEntity = NULL;
	if (m_ScriptedSpeechSlots[slot].WaveSlot->GetReferenceCount() == 0)
	{
		m_ScriptedSpeechSlots[slot].PlayState = AUD_SPEECH_SLOT_VACANT;
	}
	else
	{
		m_ScriptedSpeechSlots[slot].PlayState = AUD_SPEECH_SLOT_STOPPING;
	}
}

s32 audSpeechManager::WhenWasVariationLastPlayed(u32 voiceHash, u32 contextHash, u32 variationNum, u32& timeInMs)
{
	// Search through the history, and see if the voice and variation combo are in it.
	// We're currently pointing at the next slot to write into, m_NextPhraseMemoryWriteIndex
	// Indices are correct but not obvious - we go from the previous index backwards (+g_PhraseMemorySize is there because I don't trust -ve modding)
	for (u32 i=1; i<=g_PhraseMemorySize; i++)
	{
		u32 phraseMemoryIndex = (m_NextPhraseMemoryWriteIndex - i + g_PhraseMemorySize) % g_PhraseMemorySize;
		if (m_PhraseMemory[phraseMemoryIndex][0] == voiceHash && 
			m_PhraseMemory[phraseMemoryIndex][1] == contextHash && 
			m_PhraseMemory[phraseMemoryIndex][2] == variationNum)
		{
			// This combo is in the history, return how long ago (in num phrases, not time) it was played.
			timeInMs = m_PhraseMemory[phraseMemoryIndex][3];
			return (i-1);
		}		
	}
	// We never found it, it's not in the history
	timeInMs = 0;
	return -1;
}

void audSpeechManager::BuildMemoryListForVoiceAndContext(u32 voiceHash, u32 contextHash, u32& memorySize, u32 aPhraseMemory[g_PhraseMemorySize][4])
{
	// Search through the history, and see if the voice and variation combo are in it.
	// We're currently pointing at the next slot to write into, m_NextPhraseMemoryWriteIndex
	// Indices are correct but not obvious - we go from the previous index backwards (+g_PhraseMemorySize is there because I don't trust -ve modding)
	for (u32 i=1; i<=g_PhraseMemorySize; i++)
	{
		u32 phraseMemoryIndex = (m_NextPhraseMemoryWriteIndex - i + g_PhraseMemorySize) % g_PhraseMemorySize;
		if (m_PhraseMemory[phraseMemoryIndex][0] == voiceHash && 
			m_PhraseMemory[phraseMemoryIndex][1] == contextHash )
		{
			 aPhraseMemory[memorySize][0] = m_PhraseMemory[phraseMemoryIndex][0];
			 aPhraseMemory[memorySize][1] = m_PhraseMemory[phraseMemoryIndex][1];
			 aPhraseMemory[memorySize][2] = m_PhraseMemory[phraseMemoryIndex][2];
			 aPhraseMemory[memorySize][3] = m_PhraseMemory[phraseMemoryIndex][3];
			 ++memorySize;
		}		
	}
}

void audSpeechManager::AddVariationToHistory(u32 voiceHash, u32 contextHash, u32 variationNum, u32 timeInMs)
{
	m_PhraseMemory[m_NextPhraseMemoryWriteIndex][0] = voiceHash;
	m_PhraseMemory[m_NextPhraseMemoryWriteIndex][1] = contextHash;
	m_PhraseMemory[m_NextPhraseMemoryWriteIndex][2] = variationNum;
	m_PhraseMemory[m_NextPhraseMemoryWriteIndex][3] = timeInMs;

	m_NextPhraseMemoryWriteIndex = (m_NextPhraseMemoryWriteIndex + 1) % g_PhraseMemorySize;	
	if (m_NextPhraseMemoryWriteIndex==0)
	{
		naWarningf("Burnt through all our speech memory. Time: %u", fwTimer::GetTimeInMilliseconds());
	}
}

void audSpeechManager::GetTimeWhenContextCanNextPlay(SpeechContext *speechContext, u32* timeCanNextPlay, bool* isControlledByContext)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into GetTimeWhenContextCanNextPlay");
	naAssertf(timeCanNextPlay || !g_bSpeechAsserts, "Invalid u32* passed into GetTimeWhenContextCanNextPlay");
	naAssertf(isControlledByContext || !g_bSpeechAsserts, "Invalid bool* parameter passed into GetTimeWhenContextCanNextPlay");

	if (speechContext && speechContext->RepeatTime > -1)
	{
		*timeCanNextPlay = speechContext->TimeCanNextPlay;
		*isControlledByContext = true;
	}
	else
	{
		*timeCanNextPlay = 0;
		*isControlledByContext = false;
	}
}

void audSpeechManager::GetTimeWhenTriggeredPrimaryContextCanNextPlay(TriggeredSpeechContext *speechContext, u32* timeCanNextPlay, bool* isControlledByContext)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into GetTimeWhenContextCanNextPlay");
	naAssertf(timeCanNextPlay || !g_bSpeechAsserts, "Invalid u32* passed into GetTimeWhenContextCanNextPlay");
	naAssertf(isControlledByContext || !g_bSpeechAsserts, "Invalid bool* parameter passed into GetTimeWhenContextCanNextPlay");

	if (speechContext && speechContext->PrimaryRepeatTime > -1)
	{
		*timeCanNextPlay = speechContext->TimeCanNextPlayPrimary;
		*isControlledByContext = true;
	}
	else
	{
		*timeCanNextPlay = 0;
		*isControlledByContext = false;
	}
}

void audSpeechManager::GetTimeWhenTriggeredBackupContextCanNextPlay(TriggeredSpeechContext *speechContext, u32* timeCanNextPlay, bool* isControlledByContext)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into GetTimeWhenContextCanNextPlay");
	naAssertf(timeCanNextPlay || !g_bSpeechAsserts, "Invalid u32* passed into GetTimeWhenContextCanNextPlay");
	naAssertf(isControlledByContext || !g_bSpeechAsserts, "Invalid bool* parameter passed into GetTimeWhenContextCanNextPlay");

	if (speechContext && speechContext->BackupRepeatTime > -1)
	{
		*timeCanNextPlay = speechContext->TimeCanNextPlayBackup;
		*isControlledByContext = true;
	}
	else
	{
		*timeCanNextPlay = 0;
		*isControlledByContext = false;
	}
}

u32 audSpeechManager::GetRepeatTimeContextCanPlayOnVoice(SpeechContext *speechContext)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into GetRepeatTimeContextCanPlayOnVoice");
	if (speechContext && speechContext->RepeatTimeOnSameVoice > -1)
	{
		f32 repeatTime = (f32)speechContext->RepeatTimeOnSameVoice;
		u32 randomRepeatTime = (u32)(repeatTime * audEngineUtil::GetRandomNumberInRange(0.8f, 1.2f));
		if (randomRepeatTime>50000)
		{
			randomRepeatTime = 50000;
		}
		return randomRepeatTime;
	}
	return audEngineUtil::GetRandomNumberInRange(6500, 10000);
}

u32 audSpeechManager::GetRepeatTimePrimaryTriggeredContextCanPlayOnVoice(TriggeredSpeechContext *speechContext)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into GetRepeatTimeContextCanPlayOnVoice");
	if (speechContext && speechContext->PrimaryRepeatTimeOnSameVoice > -1)
	{
		f32 repeatTime = (f32)speechContext->PrimaryRepeatTimeOnSameVoice;
		u32 randomRepeatTime = (u32)(repeatTime * audEngineUtil::GetRandomNumberInRange(0.8f, 1.2f));
		if (randomRepeatTime>50000)
		{
			randomRepeatTime = 50000;
		}
		return randomRepeatTime;
	}
	return audEngineUtil::GetRandomNumberInRange(6500, 10000);
}

u32 audSpeechManager::GetRepeatTimeBackupTriggeredContextCanPlayOnVoice(TriggeredSpeechContext *speechContext)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into GetRepeatTimeContextCanPlayOnVoice");
	if (speechContext && speechContext->BackupRepeatTimeOnSameVoice > -1)
	{
		f32 repeatTime = (f32)speechContext->BackupRepeatTimeOnSameVoice;
		u32 randomRepeatTime = (u32)(repeatTime * audEngineUtil::GetRandomNumberInRange(0.8f, 1.2f));
		if (randomRepeatTime>50000)
		{
			randomRepeatTime = 50000;
		}
		return randomRepeatTime;
	}
	return audEngineUtil::GetRandomNumberInRange(6500, 10000);
}

u32 audSpeechManager::GetTimeLastPlayedForContext(u32 contextPHash)
{
	u32 ret = 0;
	for (u32 i=0; i<g_PhraseMemorySize; i++)
	{
		if(m_PhraseMemory[i][1] == contextPHash && ret < m_PhraseMemory[i][3])
			ret = m_PhraseMemory[i][3];
	}

	return ret;
}

void audSpeechManager::UpdateTimeContextWasLastUsed(const char *contextName, CPed* speaker, u32 timeInMs)
{
	SpeechContext *speechContext = GetSpeechContext(contextName, speaker, NULL, NULL);
	UpdateTimeContextWasLastUsed(speechContext, timeInMs);
}

void audSpeechManager::UpdateTimeContextWasLastUsed(SpeechContext *speechContext, u32 timeInMs)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into UpdateTimeContextWasLastUsed");
	if (speechContext && speechContext->RepeatTime > -1)
	{
		// To avoid things getting all in-sync with each other - our repeat time is randomised between 100% and 125%
		s32 repeatTime = audEngineUtil::GetRandomNumberInRange(speechContext->RepeatTime, (s32)(((f32)speechContext->RepeatTime)*1.25f));
		speechContext->TimeCanNextPlay = timeInMs + repeatTime;
	}
}

void audSpeechManager::UpdateTimeTriggeredContextWasLastUsed(TriggeredSpeechContext* speechContext, u32 timeInMs)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid TriggeredSpeechContext passed into UpdateTimeTriggeredContextWasLastUsed");
	if (speechContext && speechContext->TriggeredContextRepeatTime > -1)
	{
		// To avoid things getting all in-sync with each other - our repeat time is randomised between 100% and 125%
		s32 repeatTime = audEngineUtil::GetRandomNumberInRange(speechContext->TriggeredContextRepeatTime, (s32)(((f32)speechContext->TriggeredContextRepeatTime)*1.25f));
		speechContext->TimeCanNextUseTriggeredContext= timeInMs + repeatTime;
	}
}

void audSpeechManager::UpdateTimePrimaryTriggeredContextWasLastUsed(TriggeredSpeechContext* speechContext, u32 timeInMs)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid TriggeredSpeechContext passed into UpdateTimePrimaryTriggeredContextWasLastUsed");
	if (speechContext && speechContext->PrimaryRepeatTime > -1)
	{
		// To avoid things getting all in-sync with each other - our repeat time is randomised between 100% and 125%
		s32 repeatTime = audEngineUtil::GetRandomNumberInRange(speechContext->PrimaryRepeatTime, (s32)(((f32)speechContext->PrimaryRepeatTime)*1.25f));
		speechContext->TimeCanNextPlayPrimary = timeInMs + repeatTime;
	}
}

void audSpeechManager::UpdateTimeBackupTriggeredContextWasLastUsed(TriggeredSpeechContext* speechContext, u32 timeInMs)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid TriggeredSpeechContext passed into UpdateTimeBackupTriggeredContextWasLastUsed");
	if (speechContext && speechContext->BackupRepeatTime > -1)
	{
		// To avoid things getting all in-sync with each other - our repeat time is randomised between 100% and 125%
		s32 repeatTime = audEngineUtil::GetRandomNumberInRange(speechContext->BackupRepeatTime, (s32)(((f32)speechContext->BackupRepeatTime)*1.25f));
		speechContext->TimeCanNextPlayBackup = timeInMs + repeatTime;
	}
}

u32 audSpeechManager::GetVoiceForPhoneConversation(u32 pedVoiceGroup)
{
	PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup);
	if (pedVoiceGroupObject)
	{
		//0 is the voiceHash, 1 is the num of refs
		atMultiRangeArray<u32, g_PhoneConvMemorySize, 2> NumRefsPerVoiceInPhoneHistory;
		for(int i=0; i<g_PhoneConvMemorySize; i++)
			NumRefsPerVoiceInPhoneHistory[i][0] = 0;

		u32 numPedsFound = 0;
		for(int i=0; i<pedVoiceGroupObject->numPrimaryVoices; ++i)
		{
			if(audSpeechSound::FindNumVariationsForVoiceAndContext(pedVoiceGroupObject->PrimaryVoice[i].VoiceName,ATPARTIALSTRINGHASH("PHONE_CONV1_INTRO", 0x8AADB637)) != 0)
			{
				u32 numRef = 0;
				for(int j=0; j<g_PhoneConvMemorySize; ++j)
				{
					//exit early, as the rest of the history is blank
					if(m_PhoneConversationMemory[j][0] == 0)
						break;
					if(pedVoiceGroupObject->PrimaryVoice[i].VoiceName == m_PhoneConversationMemory[j][0])
						numRef++;
				}
				//if this voice doesn't appear, return it
				if(numRef == 0)
				{
					return pedVoiceGroupObject->PrimaryVoice[i].VoiceName;
				}
				//otherwise, store it
				NumRefsPerVoiceInPhoneHistory[numPedsFound][0] = pedVoiceGroupObject->PrimaryVoice[i].VoiceName;
				NumRefsPerVoiceInPhoneHistory[numPedsFound++][1] = numRef;
			}
		}

		u8* p = (u8*)&pedVoiceGroupObject->PrimaryVoice[pedVoiceGroupObject->numPrimaryVoices];
		u8 numMiniVoices = *p;
		p++;
		tVoice* miniVoices = (tVoice*)p;

		for(int i=0; i<numMiniVoices; ++i)
		{
			if(audSpeechSound::FindNumVariationsForVoiceAndContext(miniVoices[i].VoiceName,ATPARTIALSTRINGHASH("PHONE_CONV1_INTRO", 0x8AADB637)) != 0)
			{
				u32 numRef = 0;
				for(int j=0; j<g_PhoneConvMemorySize; ++j)
				{
					//exit early, as the rest of the history is blank
					if(m_PhoneConversationMemory[j][0] == 0)
						break;
					if(miniVoices[i].VoiceName == m_PhoneConversationMemory[j][0])
						numRef++;
				}
				//if this voice doesn't appear, return it
				if(numRef == 0)
				{
					return miniVoices[i].VoiceName;
				}
				//otherwise, store it
				NumRefsPerVoiceInPhoneHistory[numPedsFound][0] = miniVoices[i].VoiceName;
				NumRefsPerVoiceInPhoneHistory[numPedsFound++][1] = numRef;
			}
		}

		p = (u8*)&miniVoices[numMiniVoices];
		u8 numGangVoices = *p;
		p++;
		tVoice* gangVoices = (tVoice*)p;

		for(int i=0; i<numGangVoices; ++i)
		{
			if(audSpeechSound::FindNumVariationsForVoiceAndContext(gangVoices[i].VoiceName,ATPARTIALSTRINGHASH("PHONE_CONV1_INTRO", 0x8AADB637)) != 0)
			{
				u32 numRef = 0;
				for(int j=0; j<g_PhoneConvMemorySize; ++j)
				{
					//exit early, as the rest of the history is blank
					if(m_PhoneConversationMemory[j][0] == 0)
						break;
					if(gangVoices[i].VoiceName == m_PhoneConversationMemory[j][0])
						numRef++;
				}
				//if this voice doesn't appear, return it
				if(numRef == 0)
				{
					return gangVoices[i].VoiceName;
				}
				//otherwise, store it
				NumRefsPerVoiceInPhoneHistory[numPedsFound][0] = gangVoices[i].VoiceName;
				NumRefsPerVoiceInPhoneHistory[numPedsFound++][1] = numRef;
			}
		}

		//All these voices are present in the history, so lets find the one with the lowest ref count, giving priority to the
		//   earlier ones as they are full ped voices
		u32 indexWithLowestRef = 0;
		u32 lowestRef = NumRefsPerVoiceInPhoneHistory[0][1];
		for(int i=1; i<numPedsFound; i++)
		{
			if(NumRefsPerVoiceInPhoneHistory[i][1] < lowestRef)
			{
				lowestRef = NumRefsPerVoiceInPhoneHistory[i][1];
				indexWithLowestRef = i;
			}
		}

		return NumRefsPerVoiceInPhoneHistory[indexWithLowestRef][0];
	}
	return 0;
}


u8 audSpeechManager::GetPhoneConversationVariationFromVoiceHash(u32 voiceHash)
{
	if(!Verifyf(voiceHash != 0, "Trying to get a phone conv variation with voiceHash of 0."))
	{
		return 0;
	}
	char contextString[32];
	safecpy(contextString, "PHONE_CONV1_INTRO");
	if(audSpeechSound::FindNumVariationsForVoiceAndContext(voiceHash,atPartialStringHash(contextString)) == 0)
	{
		naAssertf(0, "voiceHash %i attempting to get phone conversation variation when it has none.", voiceHash);
		return 0;
	}

	u32 numVariations=0;
	while(numVariations<AUD_MAX_PHONE_CONVERSATIONS_PER_VOICE)
	{
		numVariations++;
		formatf(contextString, "PHONE_CONV%i_INTRO", numVariations);
		if(audSpeechSound::FindNumVariationsForVoiceAndContext(voiceHash,atPartialStringHash(contextString)) == 0)
			break;
	}
	//numVariations is now one higher than the highest valid variation we have, so decrement
	numVariations--;

	atArray<bool> varFound;
	varFound.Reset();
	varFound.Resize(numVariations);
	for(int i=0; i<numVariations; ++i)
		varFound[i] = false;

	u8 variationPlayedLongestAgo = 0;
	u32 timeLongestVariationAgoPlayed = 0;
	u8 numVariationsNotInHistory = static_cast<u8>(numVariations);

	for(int i = 0; i< g_PhoneConvMemorySize; ++i)
	{
		if(m_PhoneConversationMemory[i][0] == 0)
		{
			//exit early...the rest of the history is blank
			break;
		}
		if(voiceHash == m_PhoneConversationMemory[i][0])
		{
			numVariationsNotInHistory--;
			varFound[m_PhoneConversationMemory[i][1]-1] = true; //minus 1, as conversation variations aren't 0 indexed
			if(variationPlayedLongestAgo==0 ||  timeLongestVariationAgoPlayed > m_PhoneConversationMemory[i][2])
			{
				variationPlayedLongestAgo = static_cast<u8>(m_PhoneConversationMemory[i][1]);
				timeLongestVariationAgoPlayed = m_PhoneConversationMemory[i][2];
			}
		}
	}

	//If the history holds no conversation for this voice, return a randome one
	if(numVariationsNotInHistory == numVariations)
		return static_cast<u8>((audEngineUtil::GetRandomInteger() % numVariations) + 1);

	//If there are variations not in the history, pick a random one and return that
	if(numVariationsNotInHistory != 0)
	{
		u8 startPos = static_cast<u8>(audEngineUtil::GetRandomInteger() % numVariations);
		for(u8 i=0; i<numVariations; i++)
		{
			u8 index = (startPos + i) % numVariations;
			if(!varFound[index])
				return index+1; //plus 1, as conversation variations aren't 0 indexed
		}
	}

	//otherwise, return the oldest variation we found if it's been long enough since it last played
	if(fwTimer::GetTimeInMilliseconds() - timeLongestVariationAgoPlayed < g_MinTimeBetweenSameVariationOfPhoneCall)
		return 0;

	return variationPlayedLongestAgo;
}

void audSpeechManager::AddPhoneConversationVariationToHistory(u32 voiceHash, u32 variation, u32 timeInMs)
{
	for(int i = 0; i< g_PhoneConvMemorySize; ++i)
	{
		if(m_PhoneConversationMemory[i][0] == voiceHash && m_PhoneConversationMemory[i][1] == variation)
		{
			m_PhoneConversationMemory[i][2] = timeInMs;
			return;
		}
	}

	m_PhoneConversationMemory[m_NextPhoneConvMemoryWriteIndex][0] = voiceHash;
	m_PhoneConversationMemory[m_NextPhoneConvMemoryWriteIndex][1] = variation;
	m_PhoneConversationMemory[m_NextPhoneConvMemoryWriteIndex][2] = timeInMs;

	m_NextPhoneConvMemoryWriteIndex = (m_NextPhoneConvMemoryWriteIndex + 1) % g_PhoneConvMemorySize;
}

s32 audSpeechManager::WhenWasPhoneConversationLastPlayed(u32 voiceHash, u32 variation, u32& timeInMs)
{
	for(int i = 0; i< g_PhoneConvMemorySize; ++i)
	{
		if(m_PhoneConversationMemory[i][0] == voiceHash && m_PhoneConversationMemory[i][1] == variation)
		{
			timeInMs = m_PhoneConversationMemory[i][2];
			return i;
		}
	}

	timeInMs = 0u;
	return -1;
}

SpeechContext *audSpeechManager::GetSpeechContext(const char* contextName, CPed* speaker, bool* returningDefault)
{
	u32 contextHash = atStringHash(contextName);
	return GetSpeechContext(contextHash, speaker, NULL, returningDefault);
}

SpeechContext *audSpeechManager::GetSpeechContext(const char* contextName, CPed* speaker, CPed* replyingPed, bool* returningDefault)
{
	u32 contextHash = atStringHash(contextName);
	return GetSpeechContext(contextHash, speaker, replyingPed, returningDefault);
}

SpeechContext *audSpeechManager::GetSpeechContext(u32 contextHash, CPed* speaker, bool* returningDefault)
{
	return GetSpeechContext(contextHash, speaker, NULL, returningDefault);
}

SpeechContext *audSpeechManager::GetSpeechContext(u32 contextHash, CPed* speaker, CPed* replyingPed, bool* returningDefault)
{
	//Match the speech context with the ContextName field in the objects themselves, because the context object might have a different name
	//if there was already a gameobject with the same hash, or the speechcontext may already be referenced by a bunch of other gameobjects.
			//not useing GetObject because it checks against TYPE_ID, not BASE_TYPE_ID.  Since I've added virtual context objects that inherit from
			//	regular speech context objects, this would end up throwing pointless asserts
	AudBaseObject *obj = static_cast<AudBaseObject*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(contextHash));
	naAssertf(obj || !g_bSpeechAsserts, "No SpeechContext object for hash %u", contextHash);
	if(obj && (obj->ClassId==SpeechContext::TYPE_ID || obj->ClassId==SpeechContextVirtual::TYPE_ID) )
	{
		SpeechContext* speechContext = static_cast<SpeechContext*>(obj);
		if(returningDefault)
			*returningDefault = false;
		return ResolveVirtualSpeechContext(speechContext, speaker, replyingPed);
	}
	else if(obj && obj->ClassId==TriggeredSpeechContext::TYPE_ID)
	{
		return NULL;
	}
	else
	{
		if(returningDefault)
			*returningDefault = true;
		return g_DefaultSpeechContext;
	}
}

SpeechContext* audSpeechManager::ResolveVirtualSpeechContext(const SpeechContext* speechContext, CPed* speaker, CPed* replyingPed)
{
	if(!speechContext)
		return NULL;

	if(!Verifyf(speechContext->ClassId == SpeechContextVirtual::TYPE_ID || speechContext->ClassId == SpeechContext::TYPE_ID, 
				"Argument passed into ResolveVirtualSpeechContext isn't a speech context. TYPE_ID %u Name %s", speechContext->ClassId,
				audNorthAudioEngine::GetMetadataManager().GetObjectName(speechContext->NameTableOffset, 0)))
		return NULL;

	if(speechContext->ClassId == SpeechContext::TYPE_ID)
		return const_cast<SpeechContext*>(speechContext);

	SpeechContextVirtual* speechContextVirt = audNorthAudioEngine::GetObject<SpeechContextVirtual>(atFinalizeHash(speechContext->ContextNamePHash));//reinterpret_cast<SpeechContextVirtual*>(speechContext);
	if(!Verifyf(speechContextVirt, "Attempting to resolve a speech context with no context name partial hash."))
		return NULL;

	u8* resolvingFunction = (u8*)(&(speechContext->FakeGesture[speechContext->numFakeGestures]));
	if(!resolvingFunction)
		return NULL;

	u8* numSpeechContexts = resolvingFunction;
	numSpeechContexts++;
	u8* justBeforeSpeechContexts = numSpeechContexts;
	justBeforeSpeechContexts++;
	u32* speechContexts = (u32*)justBeforeSpeechContexts;
	switch(*resolvingFunction)
	{
	case SRF_TIME_OF_DAY:
		return ResolveVirtualSpeechContext(audVirtCxtResolves::ResolveTimeOfDay(*numSpeechContexts, speechContexts), speaker);
	case SRF_PED_TOUGHNESS:
		return ResolveVirtualSpeechContext(audVirtCxtResolves::ResolveSpeakerToughness(*numSpeechContexts, speechContexts, speaker), speaker);
	case SRF_TARGET_GENDER:
		return ResolveVirtualSpeechContext(audVirtCxtResolves::ResolveTargetGender(*numSpeechContexts, speechContexts, speaker, replyingPed), speaker, replyingPed);
	case SRF_DEFAULT:
	default:
		if(speechContextVirt->numSpeechContexts > 0)
			return ResolveVirtualSpeechContext(static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(speechContextVirt->SpeechContexts[0].SpeechContext)), speaker);
		return NULL;
	}
}

bool audSpeechManager::DoesContextHaveANoGenderVersion(SpeechContext *speechContext)
{
	naAssertf(speechContext || !g_bSpeechAsserts, "Invalid SpeechContext passed into DoesContextHaveADrunkVersion");
	if (speechContext && speechContext->GenderNonSpecificVersion != 0)
	{
		return true;
	}
	return false;
}

audWaveSlot* audSpeechManager::GetAmbientWaveSlotFromId(s32 slotId)
{
	naAssertf(slotId>=0 && slotId<g_MaxAmbientSpeechSlots, "Slot id passed into GetAmbientWaveSlotFromId is out of bounds");
	return m_AmbientSpeechSlots[slotId].WaveSlot;
}

audWaveSlot* audSpeechManager::GetScriptedWaveSlotFromId(s32 slotId)
{
	naAssertf(slotId>=0 && slotId<g_MaxScriptedSpeechSlots, "Slot id passed into GetScriptedWaveSlotFromid is out of bounds");
	return m_ScriptedSpeechSlots[slotId].WaveSlot;
}

void audSpeechManager::SpeechAudioEntityBeingDeleted(audSpeechAudioEntity* audioEntity)
{
	// Search through our slots, to check none of them are associated with our soon-to-be-deleted entity.
	// For now, let's assert, as although it could be valid usage, it's more likely to be a bug.
	for (s32 i=0; i<g_MaxAmbientSpeechSlots; i++)
	{
		if (m_AmbientSpeechSlots[i].SpeechAudioEntity == audioEntity)
		{	
			naWarningf("Ambient speech slot is associated with speech audio entity that is being deleted");
			m_AmbientSpeechSlots[i].SpeechAudioEntity = NULL;
			m_AmbientSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_STOPPING;
		}
	}
	for (s32 i=0; i<g_MaxScriptedSpeechSlots; i++)
	{
		if (m_ScriptedSpeechSlots[i].SpeechAudioEntity == audioEntity)
		{	
			naWarningf("Scripted speech slot is associated with speech audio entity that is being deleted");
			m_ScriptedSpeechSlots[i].SpeechAudioEntity = NULL;
			m_ScriptedSpeechSlots[i].PlayState = AUD_SPEECH_SLOT_STOPPING;
		}
	}
}

u32 audSpeechManager::GetPrimaryVoice(u32 pedVoiceGroup)
{
	// rustle up that game object, and pull the first primary voice out of it
	PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup);
	if (pedVoiceGroupObject && pedVoiceGroupObject->numPrimaryVoices>0)
	{
		return pedVoiceGroupObject->PrimaryVoice[0].VoiceName;
	}
	return 0;
}

u32 audSpeechManager::GetGangVoice(u32 pedVoiceGroup)
{
	u8 numGangVoices = 0;
	tVoice* gangVoices = GetGangVoices(pedVoiceGroup, numGangVoices);
	if (numGangVoices>0)
	{
		return gangVoices[0].VoiceName;
	}
	return 0;
}

u32 audSpeechManager::GetBestPrimaryVoice(PedVoiceGroups* pedVoiceGroupObject, s32& minReferences, u32 contextPartialHash)
{
	if (pedVoiceGroupObject && pedVoiceGroupObject->numPrimaryVoices>0)
	{
		// Go through all the primary voices and return the one with fewest references
		// Don't start at the beginning - it meant when the game  cleared everyone out, you'd always hear the same
		// first voice immediately - specifically a problem with M_M_TAXIDRIVER, with 11 different voices.
		s32 bestVoiceIndex = -1;
		s32 fewestReferences = -1;
		s32 lowestRunningTab = -1;

		s32 startingIndex = audEngineUtil::GetRandomNumberInRange(0, pedVoiceGroupObject->numPrimaryVoices-1);
		for (s32 i=0; i<pedVoiceGroupObject->numPrimaryVoices; i++)
		{
			s32 currentIndex = (startingIndex+i) % pedVoiceGroupObject->numPrimaryVoices;
			if (fewestReferences==-1 || (s32)pedVoiceGroupObject->PrimaryVoice[currentIndex].ReferenceCount < fewestReferences ||
				   ( (s32)pedVoiceGroupObject->PrimaryVoice[currentIndex].ReferenceCount == fewestReferences && pedVoiceGroupObject->PrimaryVoice[currentIndex].RunningTab < lowestRunningTab)
				)
			{
				if (audSpeechSound::FindNumVariationsForVoiceAndContext(pedVoiceGroupObject->PrimaryVoice[currentIndex].VoiceName, contextPartialHash) != 0 ||
					contextPartialHash == 0)
				{
					bestVoiceIndex = currentIndex;
					fewestReferences = pedVoiceGroupObject->PrimaryVoice[currentIndex].ReferenceCount;
					lowestRunningTab = pedVoiceGroupObject->PrimaryVoice[currentIndex].RunningTab;
					if(fewestReferences == 0 && lowestRunningTab == 0 && pedVoiceGroupObject->PrimaryVoice[bestVoiceIndex].VoiceName != 0)
					{
						minReferences = fewestReferences;
						return pedVoiceGroupObject->PrimaryVoice[bestVoiceIndex].VoiceName;
					}
				}
			}
		}
		if (bestVoiceIndex>-1)
		{
			minReferences = (s32)pedVoiceGroupObject->PrimaryVoice[bestVoiceIndex].ReferenceCount;
			return pedVoiceGroupObject->PrimaryVoice[bestVoiceIndex].VoiceName;
		}
	}
	minReferences = -1;
	return 0;
}

u32 audSpeechManager::GetBestMiniVoice(PedVoiceGroups* pedVoiceGroupObject, s32& minReferences, u32 contextPartialHash)
{
	// Go through all the mini voices and return the one with fewest references - we allow driving ones to be returned, if fewer refs.
	// We need to skip over the variable length primary voice array.
	if (pedVoiceGroupObject)
	{
		u8* p = (u8*)&pedVoiceGroupObject->PrimaryVoice[pedVoiceGroupObject->numPrimaryVoices];
		u8 numMiniVoices = *p;
		p++;
		tVoice* miniVoices = (tVoice*)p;

		// Go through all the mini voices and return the one with fewest references - checking it's a non-driving voice
		s32 bestVoiceIndex = -1;
		s32 fewestReferences = -1;
		s32 lowestRunningTab = -1;
		if (numMiniVoices>0)
		{
			s32 startingIndex = audEngineUtil::GetRandomNumberInRange(0, numMiniVoices-1);
			for (s32 i=0; i<numMiniVoices; i++)
			{
				s32 currentIndex = (startingIndex+i) % numMiniVoices;
				if (fewestReferences==-1 || (s32)miniVoices[currentIndex].ReferenceCount < fewestReferences ||
					  ( (s32)miniVoices[currentIndex].ReferenceCount == fewestReferences &&  miniVoices[currentIndex].RunningTab < lowestRunningTab)
					)
				{
					if (contextPartialHash==0 || audSpeechSound::FindNumVariationsForVoiceAndContext(miniVoices[currentIndex].VoiceName, contextPartialHash) != 0)
					{
						bestVoiceIndex = currentIndex;
						fewestReferences = miniVoices[currentIndex].ReferenceCount;
						lowestRunningTab = miniVoices[currentIndex].RunningTab;
						if(fewestReferences == 0 && lowestRunningTab == 0  && miniVoices[bestVoiceIndex].VoiceName != 0)
						{
							minReferences = fewestReferences;
							return miniVoices[bestVoiceIndex].VoiceName;;
						}
					}
				}
			}
		}
		// see if we found a valid one
		if (bestVoiceIndex>-1)
		{
			minReferences = (s32)miniVoices[bestVoiceIndex].ReferenceCount;
			return miniVoices[bestVoiceIndex].VoiceName;
		}
	}
	minReferences = -1;
	return 0;
}

u32 audSpeechManager::GetBestGangVoice(PedVoiceGroups* pedVoiceGroupObject, s32& minReferences, u32 contextPartialHash)
{
	// Go through all the gang voices and return the one with fewest references.
	// We need to skip over the variable length primary voice array, and the variable length mini voice array.
	u8 numGangVoices = 0;
	tVoice* gangVoices = GetGangVoices(pedVoiceGroupObject, numGangVoices);
	if (gangVoices && numGangVoices>0)
	{
		// Go through all the gang voices and return the one with fewest references
		s32 bestVoiceIndex = -1;
		s32 fewestReferences = -1;
		s32 lowestRunningTab = -1;

		s32 startingIndex = audEngineUtil::GetRandomNumberInRange(0, numGangVoices-1);
		for (s32 i=0; i<numGangVoices; i++)
		{
			s32 currentIndex = (startingIndex+i) % numGangVoices;
			if (fewestReferences==-1 || (s32)gangVoices[currentIndex].ReferenceCount < fewestReferences ||
					( (s32)gangVoices[currentIndex].ReferenceCount == fewestReferences &&  gangVoices[currentIndex].RunningTab < lowestRunningTab)
				)
			{
				if (contextPartialHash==0 || audSpeechSound::FindNumVariationsForVoiceAndContext(gangVoices[currentIndex].VoiceName, contextPartialHash) != 0)
					{
					bestVoiceIndex = currentIndex;
					fewestReferences = gangVoices[currentIndex].ReferenceCount;
					lowestRunningTab = gangVoices[currentIndex].RunningTab;
					if(fewestReferences == 0 && lowestRunningTab == 0 && gangVoices[bestVoiceIndex].VoiceName != 0)
					{
						minReferences = fewestReferences;
						return gangVoices[bestVoiceIndex].VoiceName;
					}
				}
			}
		}
		if (bestVoiceIndex>-1)
		{
			minReferences = (s32)gangVoices[bestVoiceIndex].ReferenceCount;
			return gangVoices[bestVoiceIndex].VoiceName;
		}
	}
	minReferences = -1;
	return 0;
}

u32 audSpeechManager::GetBestVoice(u32 pedVoiceGroup, u32 contextPHash, CPed* speaker, bool allowBackupPVG, s32* overallMinReferenceArg)
{
	PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup);
	if(!pedVoiceGroupObject)
	{
		Warningf("Invalid PVG hash passed to GetBestVoice: %u. Setting to MALE_BRAVE", pedVoiceGroup);
		if(speaker && speaker->GetSpeechAudioEntity())
		{
			switch(speaker->GetSpeechAudioEntity()->GetPedToughnessWithoutSetting())
			{
			case AUD_TOUGHNESS_MALE_NORMAL:
			case AUD_TOUGHNESS_MALE_COWARDLY:
			case AUD_TOUGHNESS_MALE_BUM:
				pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(ATSTRINGHASH("MALE_COWARDLY_PVG", 0x1F6CDCC4));
				break;
			case AUD_TOUGHNESS_MALE_BRAVE:
			case AUD_TOUGHNESS_MALE_GANG:
				pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(ATSTRINGHASH("MALE_BRAVE_PVG", 0x0087d1fc0));
				break;
			case AUD_TOUGHNESS_COP:
				pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(ATSTRINGHASH("COP_PVG", 0xA2468C11));
				break;
			case AUD_TOUGHNESS_FEMALE:
			case AUD_TOUGHNESS_FEMALE_BUM:
				pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(ATSTRINGHASH("FEMALE_PVG", 0x84FE566E));
				break;
			default:
				if(speaker->IsMale())
					pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(ATSTRINGHASH("MALE_BRAVE_PVG", 0x0087d1fc0));
				else
					pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(ATSTRINGHASH("FEMALE_PVG", 0x84FE566E));
				break;
			}
		}
		if(!pedVoiceGroupObject)
			return 0;
	}

	//Displayf("%s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pedVoiceGroupObject->NameTableOffset));

	u32 bestVoice[3];
	s32 minReferences[3];
	for (s32 i=0; i<3; i++)
	{
		bestVoice[i] = 0;
		minReferences[i] = -1;
	}

	if(audSpeechAudioEntity::IsMultipartContext(contextPHash))
	{
		contextPHash = atPartialStringHash("_01", contextPHash);
	}

	bestVoice[0] = GetBestMiniVoice(pedVoiceGroupObject, minReferences[0], contextPHash);
	if(minReferences[0] == 0 && bestVoice[0] != 0)
		return bestVoice[0];
	bestVoice[1] = GetBestGangVoice(pedVoiceGroupObject, minReferences[1], contextPHash);
	if(minReferences[1] == 0 && bestVoice[1] != 0)
		return bestVoice[1];
	bestVoice[2] = GetBestPrimaryVoice(pedVoiceGroupObject, minReferences[2], contextPHash);
	if(minReferences[2] == 0 && bestVoice[2] != 0)
		return bestVoice[2];

	// Now see which has the least references, and go with that one
	u32 overallBestVoice = 0;
	s32 overallMinReference = -1;
	for (s32 i=0; i<3; i++)
	{
		if (minReferences[i] > -1 &&  (minReferences[i] < overallMinReference || overallMinReference == -1))
		{
			overallBestVoice = bestVoice[i];
			overallMinReference = minReferences[i];
		}
	}

	AudBaseObject* obj =(rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(atFinalizeHash(contextPHash));
	if(!obj)
	{
		//could be a situation where we've been handed a full hash instead of a partial, so let's look using a hash of 0
		bestVoice[0] = GetBestMiniVoice(pedVoiceGroupObject, minReferences[0], 0);
		if(minReferences[0] == 0 && bestVoice[0] != 0)
			return bestVoice[0];
		bestVoice[1] = GetBestGangVoice(pedVoiceGroupObject, minReferences[1], 0);
		if(minReferences[1] == 0 && bestVoice[1] != 0)
			return bestVoice[1];
		bestVoice[2] = GetBestPrimaryVoice(pedVoiceGroupObject, minReferences[2], 0);
		if(minReferences[2] == 0 && bestVoice[2] != 0)
			return bestVoice[2];

		// Now see which has the least references, and go with that one
		overallBestVoice = 0;
		overallMinReference = -1;
		for (s32 i=0; i<3; i++)
		{
			if (minReferences[i] > -1 &&  (minReferences[i] < overallMinReference || overallMinReference == -1))
			{
				overallBestVoice = bestVoice[i];
				overallMinReference = minReferences[i];
			}
		}
	}
	else if(overallBestVoice == 0 || overallMinReference>0)
	{
		TriggeredSpeechContext* triggeredSpeechContext = (obj && obj->ClassId == TriggeredSpeechContext::TYPE_ID) ? (TriggeredSpeechContext*)obj : NULL;
		//see if the speech context has any backup contexts we can use
		if(triggeredSpeechContext && triggeredSpeechContext->numBackupSpeechContexts > 0)
		{
			for(int i=0; i< triggeredSpeechContext->numBackupSpeechContexts; i++)
			{
				if(triggeredSpeechContext->BackupSpeechContext[i].SpeechContext == 0)
					continue;
				SpeechContext* backupObj =(rage::SpeechContext*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(triggeredSpeechContext->BackupSpeechContext[i].SpeechContext);
				SpeechContext* backupContextObj = ResolveVirtualSpeechContext(backupObj, speaker);
				if(backupContextObj)
				{
					contextPHash = backupContextObj->ContextNamePHash;
					//Choose full voice before mini, if we're looking to backup context
					bestVoice[0] = GetBestPrimaryVoice(pedVoiceGroupObject, minReferences[0], contextPHash);
					if(minReferences[0] == 0 && bestVoice[0] != 0)
						return bestVoice[0];
					bestVoice[1] = GetBestMiniVoice(pedVoiceGroupObject, minReferences[1], contextPHash);
					if(minReferences[1] == 0 && bestVoice[1] != 0)
						return bestVoice[1];
					bestVoice[2] = GetBestGangVoice(pedVoiceGroupObject, minReferences[2], contextPHash);
					if(minReferences[2] == 0 && bestVoice[2] != 0)
						return bestVoice[2];

					for (s32 i=0; i<3; i++)
					{
						if (minReferences[i] > -1 &&  (minReferences[i] < overallMinReference || overallMinReference == -1))
						{
							overallBestVoice = bestVoice[i];
							overallMinReference = minReferences[i];
						}
					}
				}
				if(!(overallBestVoice == 0 || overallMinReference>0))
					break;
			}
		}
	}

	if(overallMinReferenceArg)
		*overallMinReferenceArg = overallMinReference;

	u32 overallBestBackupVoice = 0;
	s32 overallMinBackupReference = -1;
	if( (overallBestVoice == 0 || overallMinReference>0) && allowBackupPVG)
	{
		u8* p = (u8*)&pedVoiceGroupObject->PrimaryVoice[pedVoiceGroupObject->numPrimaryVoices];
		u8 numMiniVoices = *p;
		p++;
		tVoice* miniVoices = (tVoice*)p;
		p = (u8*)&miniVoices[numMiniVoices];
		u8 numGangVoices = *p;
		p++;
		tVoice* gangVoices = (tVoice*)p;
		p = (u8*)&gangVoices[numGangVoices];
		u8 numBackupPVGs = *p;
		p++;
		u32* BackupPVGHashes = (u32*)p;

		if(numBackupPVGs > 0)
		{
			u8 PVGStartIndex = audEngineUtil::GetRandomInteger() % numBackupPVGs;
			for(int i=0; i<numBackupPVGs; i++)
			{
				s32 overallMinRefIn = -1;
				u8 index = (PVGStartIndex + i) % numBackupPVGs;
				//don't allow us to go any deeper...don't want to fall into some crazy infinite recursion or end up with a 
				//	really inappropriate voice
				bestVoice[0] = GetBestVoice(BackupPVGHashes[index], contextPHash, speaker, false, &overallMinRefIn);
				//if we find an unreferenced voice, return it immediately
				if(overallMinRefIn == 0 && overallBestBackupVoice != 0)
					return overallBestBackupVoice;
				if(overallMinBackupReference == -1 || overallMinBackupReference > overallMinRefIn)
				{
					overallMinBackupReference = overallMinRefIn;
					overallBestBackupVoice = bestVoice[0];
				}
			}
		}
	}

	if(overallBestBackupVoice == 0 || overallMinBackupReference >= overallMinReference)
		return overallBestVoice;

	return overallBestBackupVoice;
}

void audSpeechManager::AddReferenceToVoice(u32 pedVoiceGroup, u32 voiceNameHash BANK_ONLY(, audSpeechAudioEntity* speechEnt))
{
	if (voiceNameHash==0)
	{
		return;
	}
	// go through our primary, mini and gang voices - return as soon as we find one
	u8 numVoices = 0;
	tVoice* voices = GetPrimaryVoices(pedVoiceGroup, numVoices);
	for (u32 i=0; i<numVoices; i++)
	{
		if (voices[i].VoiceName == voiceNameHash)
		{
			voices[i].ReferenceCount++;
			voices[i].RunningTab++;
#if __BANK
			if(g_PrintVoiceRefInfo)
			{
				naDisplayf("[VoiceRef] Adding ref to primary voice %u. PVG: %s EntityPtr: %p Current: %u  Total: %u", voiceNameHash, 
					audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(((PedVoiceGroups*)audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup))->NameTableOffset),
					speechEnt, voices[i].ReferenceCount, voices[i].RunningTab);
			}
#endif
			return;
		}
	}
	numVoices = 0;
	voices = GetMiniVoices(pedVoiceGroup, numVoices);
	for (u32 i=0; i<numVoices; i++)
	{
		if (voices[i].VoiceName == voiceNameHash)
		{
			voices[i].ReferenceCount++;
			voices[i].RunningTab++;
#if __BANK
			if(g_PrintVoiceRefInfo)
			{
				naDisplayf("[VoiceRef] Adding ref to mini voice %u. PVG: %s EntityPtr: %p Current: %u  Total: %u", voiceNameHash, 
					audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(((PedVoiceGroups*)audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup))->NameTableOffset),
					speechEnt, voices[i].ReferenceCount, voices[i].RunningTab);
			}
#endif
			return;
		}
	}
	numVoices = 0;
	voices = GetGangVoices(pedVoiceGroup, numVoices);
	for (u32 i=0; i<numVoices; i++)
	{
		if (voices[i].VoiceName == voiceNameHash)
		{
			voices[i].ReferenceCount++;
			voices[i].RunningTab++;
#if __BANK
			if(g_PrintVoiceRefInfo)
			{
				naDisplayf("[VoiceRef] Adding ref to gang voice %u. PVG: %s EntityPtr: %p Current: %u  Total: %u", voiceNameHash, 
					audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(((PedVoiceGroups*)audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup))->NameTableOffset),
					speechEnt, voices[i].ReferenceCount, voices[i].RunningTab);
			}
#endif
			return;
		}
	}
}

void audSpeechManager::RemoveReferenceToVoice(u32 pedVoiceGroup, u32 voiceNameHash)
{
	if (voiceNameHash==0)
	{
		return;
	}
	// go through our primary, mini and gang voices - return as soon as we find one
	u8 numVoices = 0;
	tVoice* voices = GetPrimaryVoices(pedVoiceGroup, numVoices);
	for (u32 i=0; i<numVoices; i++)
	{
		if (voices[i].VoiceName == voiceNameHash)
		{
			voices[i].ReferenceCount--;
			return;
		}
	}
	numVoices = 0;
	voices = GetMiniVoices(pedVoiceGroup, numVoices);
	for (u32 i=0; i<numVoices; i++)
	{
		if (voices[i].VoiceName == voiceNameHash)
		{
			voices[i].ReferenceCount--;
			return;
		}
	}
	numVoices = 0;
	voices = GetGangVoices(pedVoiceGroup, numVoices);
	for (u32 i=0; i<numVoices; i++)
	{
		if (voices[i].VoiceName == voiceNameHash)
		{
			voices[i].ReferenceCount--;
			return;
		}
	}
}

u16 audSpeechManager::GetPossibleConversationTopicsForPvg(u32 pvg)
{
	//we could be getting a voice name passed in, not a pvg, so check that first.
	u32 voiceNameHash = pvg;
	if(!audSpeechSound::DoesVoiceExist(voiceNameHash))
	{
		voiceNameHash = GetPrimaryVoice(pvg);
		// if it doesn't exist yet, give a gang voice a whirl
		if (!audSpeechSound::DoesVoiceExist(voiceNameHash))
		{
			voiceNameHash = g_SpeechManager.GetGangVoice(pvg);
		}

		if (voiceNameHash == 0 || voiceNameHash == g_NoVoiceHash)
		{
			return 0;
		}
	}
	
	// Cycle through the various conversation types, and see if we have valid lines of dialogue for that topic.
	const char* conversationLines[] = 
	{
		"CONV_SMOKE_STATE",
		"CONV_BIZ_STATE",
		"CONV_BUM_STATE",
		"CONV_CONSTRUCTION_STATE",
		"CONV_PIMP_STATE_A",
		"CONV_HOOKER_STATE_A",
		"MOBILE_CHAT",
		"TWO_WAY_PHONE_CHAT",
		"CONV_GANG_STATE"
	};
	const u32 conversationTopics[] = 
	{
		AUD_CONVERSATION_TOPIC_SMOKER,
		AUD_CONVERSATION_TOPIC_BUSINESS,
		AUD_CONVERSATION_TOPIC_BUM,
		AUD_CONVERSATION_TOPIC_CONSTRUCTION,
		AUD_CONVERSATION_TOPIC_PIMP,
		AUD_CONVERSATION_TOPIC_HOOKER,
		AUD_CONVERSATION_TOPIC_MOBILE,
		AUD_CONVERSATION_TOPIC_TWO_WAY,
		AUD_CONVERSATION_TOPIC_GANG
	};

	u16 validConversationTopics = 0;

	for (u32 i=0; i<(sizeof(conversationTopics)/sizeof(u32)); i++)
	{
		if (audSpeechSound::FindNumVariationsForVoiceAndContext(voiceNameHash, const_cast<char*>(conversationLines[i])) > 0)
		{
			// It's valid for them to have this type of conversation
			validConversationTopics = validConversationTopics | ((u16)conversationTopics[i]);
		}
	}

	return validConversationTopics;
}

u8 audSpeechManager::GetMiniVoiceType(const u32 voiceNameHash)
{
	// This is a bit shit - we shouldn't do FindVariationInfoForVoiceAndContext() every time - tag them at a higher level.
	// First, see if it's a gang mini, then if it's a hooker, then if it's driving, then it must (check) be a non-driving.
	u32 numVariations = 0;
	const u32 CONV_GANG_STATE = ATPARTIALSTRINGHASH("CONV_GANG_STATE", 0x6E6D0E76);
	const u32 HOOKER_SEX = ATPARTIALSTRINGHASH("HOOKER_SEX", 0x3D386278);
	const u32 CRASH_CAR = ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
	if (audSpeechSound::FindVariationInfoForVoiceAndContext(voiceNameHash, CONV_GANG_STATE, numVariations))
	{
		return AUD_MINI_VOICE_TYPE_GANG;
	}
	else if (audSpeechSound::FindVariationInfoForVoiceAndContext(voiceNameHash, HOOKER_SEX, numVariations))
	{
		return AUD_MINI_VOICE_TYPE_HOOKER;
	}
	else if (audSpeechSound::FindVariationInfoForVoiceAndContext(voiceNameHash, CRASH_CAR, numVariations))
	{
		return AUD_MINI_VOICE_TYPE_DRIVING;
	}
	else
	{
		// Once all voices are in-game, start doing this again
		//		Assert(audSpeechSound::FindVariationInfoForVoiceAndContext(voiceNameHash, "BUMP", numVariations));
		return AUD_MINI_VOICE_TYPE_NORMAL;
	}
}

bool audSpeechManager::IsAMiniNonDrivingContext(char* context)
{
	u32 contextHash = atStringHash(context);
	// See if it's in our list of mini non-driving contexts
	for (s32 i=0; i<NUM_MINI_NON_DRIVING_CONTEXTS; i++)
	{
		if (g_MiniNonDrivingContextHashes[i] == contextHash)
		{
			return true;
		}
	}
	return false;
}

bool audSpeechManager::IsAMiniDrivingContext(char* context)
{
	u32 contextHash = atStringHash(context);
	// See if it's in our list of mini non-driving contexts
	for (s32 i=0; i<NUM_MINI_DRIVING_CONTEXTS; i++)
	{
		if (g_MiniDrivingContextHashes[i] == contextHash)
		{
			return true;
		}
	}
	return false;
}

bool audSpeechManager::IsAGangContext(char* context)
{
	u32 contextHash = atStringHash(context);
	// See if it's in our list of mini non-driving contexts
	for (s32 i=0; i<NUM_GANG_CONTEXTS; i++)
	{
		if (g_GangContextHashes[i] == contextHash)
		{
			return true;
		}
	}
	return false;
}

tVoice* audSpeechManager::GetPrimaryVoices(u32 pedVoiceGroup, u8& numPrimaryVoices)
{
	PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup);
	if (pedVoiceGroupObject)
	{
		numPrimaryVoices = pedVoiceGroupObject->numPrimaryVoices;
		if (numPrimaryVoices>0)
		{
			return (tVoice*)&pedVoiceGroupObject->PrimaryVoice[0];
		}
	}
	return NULL;
}

tVoice* audSpeechManager::GetMiniVoices(u32 pedVoiceGroup, u8& numMiniVoices)
{
	PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup);
	if (pedVoiceGroupObject)
	{
		u8* p = (u8*)&pedVoiceGroupObject->PrimaryVoice[pedVoiceGroupObject->numPrimaryVoices];
		numMiniVoices = *p;
		if (numMiniVoices>0)
		{
			p++;
			return (tVoice*)p;
		}
	}
	return NULL;
}

tVoice* audSpeechManager::GetGangVoices(u32 pedVoiceGroup, u8& numGangVoices)
{
	PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(pedVoiceGroup);
	if (pedVoiceGroupObject)
	{
		u8* p = (u8*)&pedVoiceGroupObject->PrimaryVoice[pedVoiceGroupObject->numPrimaryVoices];
		u8 numMiniVoices = *p;
		p++;
		tVoice* miniVoices = (tVoice*)p;
		p = (u8*)&miniVoices[numMiniVoices];
		numGangVoices = *p;
		if (numGangVoices>0)
		{
			p++;
			return (tVoice*)p;
		}
	}
	return NULL;
}

tVoice* audSpeechManager::GetGangVoices(PedVoiceGroups* pedVoiceGroupObject, u8& numGangVoices)
{
	if (pedVoiceGroupObject)
	{
		u8* p = (u8*)&pedVoiceGroupObject->PrimaryVoice[pedVoiceGroupObject->numPrimaryVoices];
		u8 numMiniVoices = *p;
		p++;
		tVoice* miniVoices = (tVoice*)p;
		p = (u8*)&miniVoices[numMiniVoices];
		numGangVoices = *p;
		if (numGangVoices>0)
		{
			p++;
			return (tVoice*)p;
		}
	}
	return NULL;
}

#if __BANK
void audSpeechManager::ResetSpecificPainDrawStats()
{
	for(int i=0; i<AUD_PAIN_LEVEL_MIXED; i++)
	{
		for(int j=0; j<PAIN_DEBUG_NUM_GENDERS; j++)
		{
			m_DebugSpecPainTimeLastLoaded[i][j] = 0;
			m_DebugNonSpecPainCount[i][j] = 0;
			m_DebugSpecPainBankLoadCount[i][j] = 0;
		}
	}
	m_DebugSpecPainCountTotal = 0; 
	m_DebugNonSpecPainCountTotal = 0;
	m_DebugSpecPainBankLoadCountTotal = 0; 
	m_DebugNonSpecPainBankLoadCountTotal = 0;

	for(int i=0; i<AUD_PAIN_ID_NUM_TOTAL; i++)
		g_DebugNumTimesPainTypePlayed[i] = 0;
}

void audSpeechManager::DrawSpeechSlots()
{
	char str[1024];
	Color32 color;
	f32 yPos = 0.5f;
	f32 yInc = 0.025f;

	formatf(str, sizeof(str), "AMBIENT SLOTS");
	grcDebugDraw::Text(Vector2(0.05f, yPos), Color_purple, str);
	yPos += yInc;

	for (s32 i = 0; i < g_MaxAmbientSpeechSlots; i++)
	{
		formatf(str, sizeof(str), "Slot %d  ", i);
		switch(m_AmbientSpeechSlots[i].PlayState)
		{
		case AUD_SPEECH_SLOT_VACANT:
			safecat(str, "VACANT", sizeof(str));
			color = Color_white;
			break;
		case AUD_SPEECH_SLOT_ALLOCATED:
			safecatf(str, sizeof(str), "ALLOCATED Voice: %u Sound: %s_%02d entity: %p", m_AmbientSpeechSlots[i].Voicename, m_AmbientSpeechSlots[i].Context, m_AmbientSpeechSlots[i].VariationNumber, m_AmbientSpeechSlots[i].SpeechAudioEntity);
			color = Color_green;
			break;
		case AUD_SPEECH_SLOT_WALLA:
			safecatf(str, sizeof(str), "WALLA Voice: %u Sound: %s_%02d entity: %p", m_AmbientSpeechSlots[i].Voicename, m_AmbientSpeechSlots[i].Context, m_AmbientSpeechSlots[i].VariationNumber, m_AmbientSpeechSlots[i].SpeechAudioEntity);
			color = Color_green;
			break;
		case AUD_SPEECH_SLOT_STOPPING:
			safecatf(str, sizeof(str), "STOPPING Voice: %u Sound: %s_%02d bank: %s", m_AmbientSpeechSlots[i].Voicename, m_AmbientSpeechSlots[i].Context, m_AmbientSpeechSlots[i].VariationNumber, 
				m_AmbientSpeechSlots[i].WaveSlot ? m_AmbientSpeechSlots[i].WaveSlot->GetLoadedBankName() : "NULL");
			color = Color_yellow;
			break;
		default:
			safecat(str, "UNKNOWN", sizeof(str));
			color = Color_red;
			break;
		}
		grcDebugDraw::Text(Vector2(0.05f, yPos), color, str);
		yPos += yInc;
	}
}

void audSpeechManager::DrawPainPlayedTotals()
{
	char str[1024];
	Color32 color;
	f32 yPos = 0.05f;
	f32 yInc = 0.025f;
	f32 xPos = 0.05f;

	formatf(str, "Pain Played Totals");
	grcDebugDraw::Text(Vector2(xPos, yPos), Color_purple, str);
	yPos += yInc;

	for(int i=0; i<AUD_PAIN_ID_NUM_TOTAL; ++i)
	{
		if(i == AUD_PAIN_ID_NUM_BANK_LOADED)
		{
			yPos += yInc;
			continue;
		}

		formatf(str, sizeof(str), "%s  %u", g_PainContexts[i], g_DebugNumTimesPainTypePlayed[i]);
		grcDebugDraw::Text(Vector2(xPos, yPos), color, str);
		yPos += yInc;
	}

	formatf(str, sizeof(str), "Last Pain Value  %f", g_LastRecordedPainValue);
	grcDebugDraw::Text(Vector2(xPos, yPos), color, str);
	yPos += yInc;
}

void audSpeechManager::DrawSpecificPainInfo()
{
	char str[1024];
	Color32 color;
	f32 yPos = 0.05f;
	f32 yInc = 0.025f;
	f32 xPos = 0.05f;

	formatf(str, "Specific Pain Info");
	grcDebugDraw::Text(Vector2(xPos, yPos), Color_purple, str);
	yPos += yInc;

	formatf(str, sizeof(str), "Pct. of Pain That's Specific  %f", m_DebugSpecPainCountTotal + m_DebugNonSpecPainCountTotal == 0 ? 0.0f : ((f32)m_DebugSpecPainCountTotal)/((f32)(m_DebugSpecPainCountTotal + m_DebugNonSpecPainCountTotal)));
	color = Color_green;
	grcDebugDraw::Text(Vector2(xPos, yPos), color, str);
	yPos += yInc;

	formatf(str, sizeof(str), "Specific Pain Instance Played Per Bank Swap  %f", m_DebugSpecPainBankLoadCountTotal == 0 ? 0.0f : ((f32)m_DebugSpecPainCountTotal)/((f32) m_DebugSpecPainBankLoadCountTotal));
	color = Color_green;
	grcDebugDraw::Text(Vector2(xPos, yPos), color, str);
	yPos += yInc;

	formatf(str, sizeof(str), "Pct. of Pain Swaps That Are Specific Pain  %f", m_DebugSpecPainBankLoadCountTotal + m_DebugNonSpecPainBankLoadCountTotal == 0 ? 0.0f : ((f32)m_DebugSpecPainBankLoadCountTotal)/((f32)(m_DebugSpecPainBankLoadCountTotal + m_DebugNonSpecPainBankLoadCountTotal)));
	color = Color_green;
	grcDebugDraw::Text(Vector2(xPos, yPos), color, str);
	yPos += yInc;

	yPos = 0.05f;
	xPos = 0.6f;

	formatf(str, sizeof(str), "Specific Bank Loaded  %s_%s", m_SpecificPainLoadedIsMale ? "PAIN_MALE" : "PAIN_FEMALE", g_PainLevelString[m_SpecificPainLevelLoaded]);
	color = Color_green;
	grcDebugDraw::Text(Vector2(xPos, yPos), color, str);
	yPos += yInc;

	for(int i=0; i<3; i++)
	{
		for(int j=0; j<PAIN_DEBUG_NUM_GENDERS; j++)
		{
			formatf(str, sizeof(str), "PAIN_%s_%s %u %u", j==0 ? "MALE" : "FEMALE", g_PainLevelString[i], 
							m_DebugSpecPainBankLoadCount[i][j], m_DebugSpecPainTimeLastLoaded[i][j]);
			color = Color_green;
			grcDebugDraw::Text(Vector2(xPos, yPos), color, str);
			yPos += yInc;
		}
	}
}

void audSpeechManager::DrawAnimalBanks()
{
	char str[1024];
	Color32 color;
	f32 yPos = 0.05f;
	f32 yInc = 0.025f;
	f32 xPos = 0.05f;

	formatf(str, "Animal Banks");
	grcDebugDraw::Text(Vector2(xPos, yPos), Color_purple, str);
	yPos += yInc;

	for(int i=0; i<m_AnimalBankSlots.GetCount(); i++)
	{
		formatf(str, (m_AnimalBankSlots[i].WaveSlot ? m_AnimalBankSlots[i].WaveSlot->GetLoadedBankName() : "NULL"));
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_purple, str);
		yPos += yInc;
	}
}
#endif

