// 
// audio/speechaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "speechaudioentity.h"
#include "audiodefines.h"
#include "caraudioentity.h"
#include "speechmanager.h"
#include "northaudioengine.h"
#include "cutsceneaudioentity.h"
#include "scriptaudioentity.h"
#include "frontendaudioentity.h"

#include "audio/weaponaudioentity.h"
#include "audiosoundtypes/environmentsound.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiohardware/device.h"
#include "audiohardware/driver.h"
#include "audiohardware/driverdefs.h"
#include "audiohardware/submix.h"
#include "audiohardware/syncsource.h"
#include "audiohardware/waveref.h"
#include "audiosynth/synthcore.h"
#include "audiohardware/waveslot.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "camera/cutscene/CutsceneDirector.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/debug/FreeCamera.h"
#include "camera/replay/ReplayRecordedCamera.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "control/replay/Audio/SpeechAudioPacket.h"
#include "cutscene/CutSceneManagerNew.h"
#include "frontend/MiniMap.h"
#include "fwdebug/picker.h"
#include "grcore/debugdraw.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "peds/pedfactory.h"
#include "peds/HealthConfig.h"
#include "scene/ExtraContent.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/physical.h"
#include "system/memory.h"
#include "event/EventDamage.h"
#include "network/NetworkInterface.h"
#include "game/Clock.h"
#include "game/localisation.h"
#include "game/ModelIndices.h"
#include "game/weather.h"
#include "vfx/misc/TVPlaylistManager.h"
#include "vehicles/vehicle.h"
#include "task/motion/taskmotionbase.h"
#include "task/Movement/TaskParachute.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "camera/debug/debugdirector.h"
#include "audio/ambience/ambientaudioentity.h"
#include "debug/vectormap.h"

#include "audio/debugaudio.h"

AUDIO_PEDS_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(Audio,SpeechEntity)

const char *g_SpeechWaveSlotName = "SCRIPT_SPEECH_0";

BANK_ONLY(static const char *g_AnimTestVoiceName = "MICHAEL");
BANK_ONLY(static const char *g_AnimTestContextName = "JH_RJAE");

PARAM(disableAmbientSpeech, "Disable Ambient Speech");
PARAM(bypassSpeechStreamCheck, "Bypass Streaming Check For Speech");
PARAM(phoneConversationHistoryTest, "Run the phone conversation history test.");
PARAM(stopForUrgentSpeech, "Stop scripted speech early for urgent speech.");
PARAM(audNoUseStreamingBucketForSpeech, "Do not use the streaming bucket for scripted speech, and ambient speech triggered via script");
//PARAM(audPhoneConvThroughPadSpeaker, "Play phone conversations through the ps4 controller speaker when in the car");

#if RSG_ORBIS
const bool g_RoutePhoneCallsThroughPadSpeaker = true;
#else
const bool g_RoutePhoneCallsThroughPadSpeaker = false;
#endif
bool g_DisableAmbientSpeech = false;
bool g_BypassStreamingCheckForSpeech = true;
bool g_AllowPlayerToSpeak = false;
bool g_ShortenCompleteSay = true;
f32 g_MaxDistanceToPlaySpeechIfFewSlotsInUse = 50.0f;
f32 g_MaxDistanceToPlaySpeech = 35.0f;
f32 g_MaxDistanceToPlayCopSpeech = 60.0f;	
f32 g_MaxDistanceToPlayPain = 100.0f;
f32 g_MaxDistanceSqToPlayPain = g_MaxDistanceToPlayPain * g_MaxDistanceToPlayPain;
f32 g_MinDistForLongDeath = 40.0f;
f32 g_MaxDistForShortDeath = 40.0f;
f32 g_HeadshotDeathGargleProb = 0.5f;
u32 g_FallingScreamReleaseTime = 300;
f32 g_FallingScreamVelocityThresh = 1.0f;
f32 g_FallingScreamMinVelocityWithNoGroundProbe = 40.0f;
f32 g_SpeakerConeVolAttenLin = 1.0f;
f32 g_SpeakerConeLPF = 3000.0f;
f32 g_SpeakerConeAngleInner = 65.0f;
f32 g_SpeakerConeAngleOuter = 135.0f;
f32 g_SpeakerConeDistForZeroStrength = 2.0f;
f32 g_SpeakerConeDistForFullStrength = 5.0f;
f32 g_ListenerConeVolAttenLin = 0.63f;
f32 g_ListenerConeLPF = 3000.0f;
f32 g_ListenerConeAngleInner = 45.0f;
f32 g_ListenerConeAngleOuter = 135.0f;
f32 g_SpeechVolumeSmoothRate = 0.45f;
f32 g_SpeechLinVolumeSmoothRate = 0.0008f;
f32 g_SpeechFilterSmoothRate = 0.05f;
f32 g_HeadsetDistortionDefault = 0.423f;
f32 g_HeadsetDistortionReduced = 0.25f;
bool g_GiveEveryoneConversationVoices = false;
bool g_UseSpeedGesturingVoices = false;
bool g_StreamingSchmeaming = false;
bool g_IgnoreBeingInPlayerCar = false;
f32 g_MaxDepthToHearSpeech = 7.0f;
f32 g_MinUnderwaterLinLPF = 0.4f;
f32 g_UnderwaterReverbWet = 1.0f;
bank_bool g_ForceToughCopVoice = false;
u32 g_MaxTimeAfterThrownFromCarToSpeak = 5000;
f32 g_ScriptedSpeechAmbientSlotPriority = 5.0f;

bool g_EnableEarlyStoppingForUrgentSpeech = false;
u32 g_TimeToStopEarlyForUrgentSpeech = 100;

// New speech volumes and rolloffs
f32 g_AudibilityVolOffset[AUD_NUM_AUDIBILITIES] = { 
	-1.5f,	// AUD_AUDIBILITY_NORMAL						
	-0.75f,	// AUD_AUDIBILITY_CLEAR
	0.0f,	// AUD_AUDIBILITY_CRITICAL
	0.0f	// AUD_AUDIBILITY_LEAD_IN
};
f32 g_VolTypeVolOffset[AUD_NUM_SPEECH_VOLUMES] = {
	0.0f,	// AUD_SPEECH_NORMAL
	0.0f,	// AUD_SPEECH_SHOUTED
	0.0f,	// AUD_SPEECH_FRONTEND
	3.0f	// AUD_SPEECH_MEGAPHONE
};
f32 g_VolTypeEnvLoudness[AUD_NUM_SPEECH_VOLUMES] = {
	0.0f,	// AUD_SPEECH_NORMAL
	0.0f,	// AUD_SPEECH_SHOUTED
	0.0f,	// AUD_SPEECH_FRONTEND
	0.75f	// AUD_SPEECH_MEGAPHONE
};
f32 g_PlayerInCarVolumeOffset = 0.0f;
f32 g_PlayerOnFootAmbientVolumeOffset = 1.5f;
f32 g_BreathingRolloff = 3.0f;
f32 g_BreathingRolloffGunfire = 6.0f;

bool g_PedsAreBlindRagingOverride = false;
NOTFINAL_ONLY(f32 g_VolumeIncreaseForPlaceholder = 6.0f;)

bool g_DisplayVoiceInfo = false;
bool g_DisplayVoiceInfoForAllPeds = false;
f32 g_DisplayVoiceInfoDistance = 15.0f;

u32 g_GlobalMinTimeBetweenCoughSpeech = 1000;
u32 g_PerPedMinTimeBetweenCoughSpeech = 10000;
//f32 g_LaughProb = 30.0f;//1.0f/(30.0f*30.0f); // once every 30 seconds
//f32 g_CoughProb = 30.0f;//1.0f/(30.0f*30.0f); // once every 30 seconds
//Replacing prob with time so we don't have to check prop every frame
s32 g_MinTimeBetweenLaughs = 45000;
s32 g_MaxTimeBetweenLaughs = 135000;
s32 g_MinTimeBetweenCoughs = 45000;
s32 g_MaxTimeBetweenCoughs = 135000;
f32 g_LaughCoughMinDistSq = 35.0f * 35.0f;
f32 g_LaughCoughMaxDistSq = 60.0f * 60.0f;

f32 g_MinDistanceSqForRunnedOverScream = 25.0f * 25.0f;
f32 g_MaxDistanceSqForRunnedOverScream = 40.0f * 40.0f;
u32 g_NumScreamsForRunnedOverReaction = 2;
f32 g_RunnedOverScreamPredelayVariance = 5000.0f;
extern u32 g_MinPedsHitForRunnedOverReaction;
extern u32 g_TimespanToHitPedForRunnedOverReaction;
extern u32 g_MinTimeBetweenLastCollAndRunnedOverReaction;
extern u32 g_TimeToWaitBetweenRunnedOverReactions;

bool g_DoesLandProbeWaitCurveExist = false;
bool g_UseDelayedFallProbe = false;

f32 g_EchoDryLevel = 1.0f;
f32 g_SpeechEchoPredelayScalar = 0.3f;
f32 g_SpeechEchoVolShouted = -18.0f;
f32 g_SpeechEchoVolMegaphone = -6.0f;
s32 g_SpeechEchoMinPredelayDiff = 100;

u32 g_SpeechPlayedEnoughNotToRepeatTime = 250;

u32 g_AnimalPainPredelay = 300;

u32 g_MinTimeBetweenPlayerCarHitPedSpeech = 15000;

bool g_DisablePanting = false;

// stuff for speech manager
extern u32 g_LastTimeSpeechWasPlaying;
extern u32 g_TimeBeforeChurningSpeech;
extern u32 g_LastTimeStreamingWasBusy;
extern u32 g_StreamingBusyTime;
extern f32 g_SpeechRolloffWhenInACar;
extern bool g_PrintUnknownContexts;
extern bool g_ActivatePainSpecificBankSwapping;

extern bool g_TestAnimalBankPriority;
extern bool g_DisplayAnimalBanks;
extern bool g_ForceAnimalBankUpdate;
bool g_ForceAnimalMoodAngry = false;
bool g_ForceAnimalMoodPlayful = false;

extern bank_u32 g_SpeechOffsetForVisemesMs;

const u32 g_MaleCowardlyPVGHash = ATSTRINGHASH("MALE_COWARDLY_PVG", 0x01f6cdcc4);
const u32 g_MaleBravePVGHash = ATSTRINGHASH("MALE_BRAVE_PVG", 0x0087d1fc0);
const u32 g_MaleGangPVGHash = ATSTRINGHASH("MALE_GANG_PVG", 0x0b616cbfb);
const u32 g_FemalePVGHash = ATSTRINGHASH("FEMALE_PVG", 0x084fe566e);
const u32 g_CopPVGHash = ATSTRINGHASH("COP_PVG", 0x0a2468c11);
const u32 g_SilentPVGHash = ATSTRINGHASH("SILENT_PVG", 0x019459f1e);

const u32 g_SilentVoiceHash = ATSTRINGHASH("SILENT_VOICE", 0x0380bcb73);


const u8 g_MaleCowardlyPainHash = (u8)(ATSTRINGHASH("MaleCowardly", 0x03567c7b2) & 0xff);
const u8 g_MaleBumPainHash = (u8)(ATSTRINGHASH("MaleBum", 0x6b107822) & 0xff);
const u8 g_MaleBravePainHash = (u8)(ATSTRINGHASH("MaleBrave", 0x0eed7bf22) & 0xff);
const u8 g_MaleGangPainHash = (u8)(ATSTRINGHASH("MaleGang", 0x0a8293719) & 0xff);
const u8 g_FemalePainHash = (u8)(ATSTRINGHASH("Female", 0x06d155a1b) & 0xff);
const u8 g_FemaleBumPainHash = (u8)(ATSTRINGHASH("FemaleBum", 0x58ce915e) & 0xff);
const u8 g_CopPainHash = (u8)(ATSTRINGHASH("Cop", 0x0a49e591c) & 0xff);

const u32 g_HeadsetSynthPatchHash = ATSTRINGHASH("SYNTH_SPEECH_HEADSET_DSP", 0x9B8EBB30);
const u32 g_HeadsetSubmixControlHash = ATSTRINGHASH("HEADSET_SUBMIX_CONTROL", 0x6DD3418);

const u32 g_TennisVocalSoundHash = ATSTRINGHASH("TENNIS_VOCALIZATION", 0xBABEFB42);

#if !__FINAL 
PARAM(noplaceholderdialogue, "Disable placeholder dialogue");
extern bool g_EnablePlaceholderDialogue;
#endif

#if __BANK
extern bool g_ConversationDebugSpew;

PARAM(fullspeechPVG, "Force all peds to have a full speech PVG");
bool g_FullSpeechPVG = false;
PARAM(silentspeech, "Allow silent speech assets to trigger.");
bool g_EnableSilentSpeech = false;
PARAM(ForceFakeGestures, "Allow fake speech gestures..");
bool g_ForceFakeGestures = true;

bool g_ShowSpeechRoute = false;
bool g_ShowSpeechPositions = false;

atHashString g_FakeGestureName;
u32 g_FakeGestureMarkerTime = 0;
f32 g_FakeGestureStartPhase = 0.0f;
f32 g_FakeGestureEndPhase = 1.0f;
f32 g_FakeGestureRate = 1.0f;
f32 g_FakeGestureMaxWeight = 1.0f;
f32 g_FakeGestureBlendInTime = 0.3f;
f32 g_FakeGestureBlendOutTime = 0.3f;
bool g_FakeGestureIsSpeaker = true;

extern bool g_DrawSpeechSlots;
extern bool g_DrawSpecificPainInfo;
extern bool g_DrawPainPlayedTotals;

extern bool g_OverrideSpecificPainLoaded;
extern u8 g_OverrideSpecificPainLevel;
extern bool g_OverrideSpecificPainIsMale;
extern u8 g_OverridePainVsAimScalar;
extern bool g_ForceSpecificPainBankSwap;

extern bool g_DebugTennisLoad;
extern bool g_DebugTennisUnload;

atRangeArray<VFXDebugStruct, 64> audSpeechAudioEntity::sm_VFXDebugDrawStructs;
atRangeArray<VFXDebugStruct, 64> audSpeechAudioEntity::sm_AmbSpeechDebugDrawStructs;
CPed* audSpeechAudioEntity::sm_CanHaveConvTestPed = NULL;
char audSpeechAudioEntity::sm_VisemeTimingingInfoString[256];

bool g_TurnOnVFXInfo = false;
bool g_TurnOnAmbSpeechInfo = false;
bool g_IsPlayerAngry = false;
bool g_DisplayLipsyncTimingInfo = false;

extern f32 g_LastRecordedPainValue;
extern atRangeArray<u32, AUD_PAIN_ID_NUM_TOTAL> g_DebugNumTimesPainTypePlayed;

bool g_ListPlayedAmbientContexts = false;
bool g_ListPlayedScriptedContexts = false;
bool g_ListPlayedPainContexts = false;
bool g_ListPlayedAnimalContexts = false;
#endif

#if !__FINAL
bool g_HasPrintedSoundPoolForCancelledPlayback = false;
#endif

bool g_DisableFESpeechBubble = false;

bool g_UseHPFOnLeadIn = true;

bool g_PretendThePlayerIsPissedOff = false;
extern s32 g_RomansMood;
extern s32 g_BriansMood;

u32 audSpeechAudioEntity::sm_LocalPlayerVoice;
u32 audSpeechAudioEntity::sm_LocalPlayerPainVoice;
char audSpeechAudioEntity::sm_TrevorRageContextOverride[64];
u32 audSpeechAudioEntity::sm_TrevorRageContextOverridePHash = 0;
bool audSpeechAudioEntity::sm_TrevorRageOverrideIsScriptSet = false;


u32 audSpeechAudioEntity::sm_NumExplosionPainsPlayedThisFrame = 0;
u32 audSpeechAudioEntity::sm_NumExplosionPainsPlayedLastFrame = 0;
f32 g_ProbOfExplosionDeathScream = 0.1f;

float g_MinHeightForHighFallContext = 3.0f;
float g_MinHeightForSuperHighFallContext = 15.0f;
bool g_TestFallingVoice = false;

const u32 g_PainFemale = ATSTRINGHASH("PAIN_FEMALE_EXTRAS", 0x02e6addc9);
const u32 g_PainMale = ATSTRINGHASH("PAIN_MALE_EXTRAS", 0x053b40edd);
extern const u32 g_PainVoice = ATSTRINGHASH("PAIN_VOICE", 0x048571d8d);

const u32 g_OnFireContextPHash = ATPARTIALSTRINGHASH("ON_FIRE", 0xC6E0C121);
const u32 g_HighFallContextPHash = ATPARTIALSTRINGHASH("HIGH_FALL", 0x15C3D535);
const u32 g_SuperHighFallContextPHash = ATPARTIALSTRINGHASH("SUPER_HIGH_FALL", 0xF753E8FD);
const u32 g_HighFallNewContextPHash = ATPARTIALSTRINGHASH("HIGH_FALL_NEW", 0x7057984F);
const u32 g_DrowningContextPHash = ATPARTIALSTRINGHASH("DROWNING", 0xD1905115);
const u32 g_DrowningDeathContextPHash = ATPARTIALSTRINGHASH("DEATH_UNDERWATER", 0x3BD03BA7);
const u32 g_ComeUpForAirContextPHash = ATPARTIALSTRINGHASH("SWIMMING_COME_UP_FOR_AIR", 0x46D94EBB);
const u32 g_CoughContextPHash = ATPARTIALSTRINGHASH("COUGH", 0x6E9A759);
const u32 g_PanicContextPHash = ATPARTIALSTRINGHASH("PANIC", 0x82048599);
const u32 g_PanicShortContextPHash = ATPARTIALSTRINGHASH("PANIC_SHORT", 0x543E9052);
const u32 g_FuckFallContextPHash = ATPARTIALSTRINGHASH("FUCK_FALL", 0xF11E2F2C);
const u32 g_InhaleContextPHash = ATPARTIALSTRINGHASH("INHALE", 0x89C41C1D);
const u32 g_SharkScreamContextPHash = ATPARTIALSTRINGHASH("SHARK_SCREAM", 0x11E636FA);
const u32 g_SharkAttackContextPHash = ATPARTIALSTRINGHASH("SHARK_ATTACK", 0x5BB745F5);
const u32 g_SharkAwayContextPHash = ATPARTIALSTRINGHASH("SHARK_AWAY", 0x20D013A);

const u32 g_ShootoutSpecialContextPHash = ATPARTIALSTRINGHASH("SHOOTOUT_SPECIAL", 0xEA6FB40A);
const u32 g_ShootoutSpecialAloneContextPHash = ATPARTIALSTRINGHASH("SHOOTOUT_SPECIAL_TO_NOBODY", 0xFC1DC1D2);

const u32 g_GetWantedLevelContextHash = ATSTRINGHASH("GET_WANTED_LEVEL", 0xE93A04CB);

const u32 g_MichaelVoice = ATPARTIALSTRINGHASH("MICHAEL", 0x86E0105A);
const u32 g_FranklinVoice = ATPARTIALSTRINGHASH("FRANKLIN", 0x202A4491);
const u32 g_TrevorVoice = ATPARTIALSTRINGHASH("TREVOR", 0x6B609EF);

const u32 g_ZombieVoice = ATSTRINGHASH("M_ZOMBIE", 0x061594bf6);
const u32 g_SharkVoiceHash = ATSTRINGHASH("SHARK", 0x229503C8);

extern const u32 g_SpeechContextBumpHash = ATSTRINGHASH("BUMP", 0x9912B1F5);
extern const u32 g_SpeechContextBumpPHash = ATPARTIALSTRINGHASH("BUMP", 0x9C72B01B);
extern const u32 g_SpeechContextGangBumpPHash = ATPARTIALSTRINGHASH("GANG_BUMP", 0x154EDF52);
const u32 g_SpeechContextJeeringRespPHash = ATPARTIALSTRINGHASH("JEERING_RESP", 0xB777CF58);
const u32 g_SpeechContextIntimidateRespPHash = ATPARTIALSTRINGHASH("INTIMIDATE_RESP", 0x25C702A5);
const u32 g_SpeechContextKnockOverPedPHash = ATPARTIALSTRINGHASH("KNOCK_OVER_PED", 0x6D0513D0);
const u32 g_SpeechContextMissionFailRagePHash = ATPARTIALSTRINGHASH("MISSION_FAIL_RAGE", 0x14F79DA);
const u32 g_SpeechContextMissionBlindRagePHash = ATPARTIALSTRINGHASH("MISSION_BLIND_RAGE", 0x8ABF1540);
const u32 g_SpeechContextKilledAllPHash = ATPARTIALSTRINGHASH("KILLED_ALL", 0x62EB08D6);
const u32 g_SpeechContextMessingWithPhonePHash = ATPARTIALSTRINGHASH("MESSING_WITH_PHONE", 0xA1CC7F1B);
const u32 g_SpeechContextJackedSoftPHash = ATPARTIALSTRINGHASH("JACKED_SOFT", 0xA36ED157);
const u32 g_SpeechContextJackedInCarPHash = ATPARTIALSTRINGHASH("JACKED_IN_CAR", 0x5EC814B3);
const u32 g_SpeechContextJackedCarPHash = ATPARTIALSTRINGHASH("JACKED_CAR", 0x9D6E8438);
const u32 g_SpeechContextJackedGenericPHash = ATPARTIALSTRINGHASH("JACKED_GENERIC", 0xE6407BDA);
const u32 g_SpeechContextArrestedPHash = ATPARTIALSTRINGHASH("ARRESTED", 0x35344AC3);
const u32 g_SpeechContextChasedPHash = ATPARTIALSTRINGHASH("CHASED", 0x6FE8EB2D);
const u32 g_SpeechContextDirectionsNoPHash = ATPARTIALSTRINGHASH("DIRECTIONS_NO", 0xA40580);
const u32 g_SpeechContextGenericWarCryPHash = ATPARTIALSTRINGHASH("GENERIC_WAR_CRY", 0x7F43037E);

const u32 g_SpeechContextDodgePHash = ATPARTIALSTRINGHASH("DODGE", 0x868834D7);
const u32 g_SpeechContextGangDodgeWarningPHash = ATPARTIALSTRINGHASH("GANG_DODGE_WARNING", 0xFF784581);
const u32 g_SpeechContextBeenShotPHash = ATPARTIALSTRINGHASH("BEEN_SHOT", 0x8950E6B6);
const u32 g_SpeechContextShotInLegPHash = ATPARTIALSTRINGHASH("SHOT_IN_LEG", 0x43E26A29);

const u32 g_SpeechContextSurprisedPHash = ATPARTIALSTRINGHASH("SURPRISED", 0xF30443A6);
const u32 g_SpeechContextInsultHighPHash = ATPARTIALSTRINGHASH("GENERIC_INSULT_HIGH", 0x5B2027A1);
const u32 g_SpeechContextInsultMedPHash = ATPARTIALSTRINGHASH("GENERIC_INSULT_MED", 0x535A7DD8);
const u32 g_SpeechContextShockedHighPHash = ATPARTIALSTRINGHASH("GENERIC_SHOCKED_HIGH", 0x3780E306);
const u32 g_SpeechContextShockedMedPHash = ATPARTIALSTRINGHASH("GENERIC_SHOCKED_MED", 0x7F1A575A);
const u32 g_SpeechContextCurseHighPHash = ATPARTIALSTRINGHASH("GENERIC_CURSE_HIGH", 0xD24FF087);
const u32 g_SpeechContextCurseMedPHash = ATPARTIALSTRINGHASH("GENERIC_CURSE_MED", 0xE49E566B);
const u32 g_SpeechContextFrightenedHighPHash = ATPARTIALSTRINGHASH("GENERIC_FRIGHTENED_HIGH", 0xCC882343);
const u32 g_SpeechContextFrightenedMedPHash = ATPARTIALSTRINGHASH("GENERIC_FRIGHTENED_MED", 0x5826C8E7);

const u32 g_SpeechContextGenericHiHash = ATSTRINGHASH("GENERIC_HI", 0x125114D5);

const u32 g_SpeechContextSeeWeirdoHash = ATSTRINGHASH("SEE_WEIRDO", 0xE1DEA76F);
const u32 g_SpeechContextSeeWeirdoPHash = ATPARTIALSTRINGHASH("SEE_WEIRDO", 0x96E8C3DB);
// Guard
extern const u32 g_SpeechContextGuardResponsePHash = ATPARTIALSTRINGHASH("CONV_GENERIC_RESP", 0x512B69B0);

// megaphone categories
const u32 g_SpeechContextCopHeliMegaphonePHash = ATPARTIALSTRINGHASH("COP_HELI_MEGAPHONE", 0x86FF6B4);
const u32 g_SpeechContextCopHeliMegaphoneWarningPHash = ATPARTIALSTRINGHASH("COP_HELI_MEGAPHONE_WARNING", 0x7C4BC571);
const u32 g_SpeechContextPullOverPHash = ATPARTIALSTRINGHASH("PULL_OVER", 0x1D70DAF3);
const u32 g_SpeechContextPullOverWarningPHash = ATPARTIALSTRINGHASH("PULL_OVER_WARNING", 0xEC97ACA3);
const u32 g_SpeechContextMegaphoneFootPursuitPHash = ATPARTIALSTRINGHASH("MEGAPHONE_FOOT_PURSUIT", 0x2D2F9416);
const u32 g_SpeechContextMegaphonePedClearStreetPHash = ATPARTIALSTRINGHASH("MEGAPHONE_PED_CLEAR_STREET", 0x1E6EBC8A);

const u32 g_SpeechContextMobileIntroPHash = ATPARTIALSTRINGHASH("MOBILE_INTRO", 0xE1D61F75);
const u32 g_SpeechContextGenericHiPHash = ATPARTIALSTRINGHASH("GENERIC_HI", 0xDD552858);
const u32 g_SpeechContextKifflomGreetPHash = ATPARTIALSTRINGHASH("KIFFLOM_GREET", 0x290F5D37);

//const u32 g_SpeechContextGreetAttractiveFemalePHash = ATPARTIALSTRINGHASH("GREET_ATTRACTIVE_F", 0x4D09269D);
const u32 g_SpeechContextGreetBumPHash = ATPARTIALSTRINGHASH("GREET_BUM", 0xF66A5F74);
const u32 g_SpeechContextGreetHillbillyMalePHash = ATPARTIALSTRINGHASH("GREET_HILLBILLY_M", 0xB1517394);
//const u32 g_SpeechContextGreetJunkiePHash = ATPARTIALSTRINGHASH("GREET_JUNKIE", 0xCB529024);
const u32 g_SpeechContextGreetCopPHash = ATPARTIALSTRINGHASH("GREET_COP", 0x308C90C0);
const u32 g_SpeechContextGreetBallasMalePHash = ATPARTIALSTRINGHASH("GREET_GANG_BALLAS_M", 0x2D0675C4);
const u32 g_SpeechContextGreetBallasFemalePHash = ATPARTIALSTRINGHASH("GREET_GANG_BALLAS_F", 0x2D06597D);
const u32 g_SpeechContextGreetFamiliesMalePHash = ATPARTIALSTRINGHASH("GREET_GANG_FAMILIES_M", 0x337E42B0);
const u32 g_SpeechContextGreetFamiliesFemalePHash = ATPARTIALSTRINGHASH("GREET_GANG_FAMILIES_F", 0x337EA539);
const u32 g_SpeechContextGreetVagosMalePHash = ATPARTIALSTRINGHASH("GREET_GANG_VAGOS_M", 0x9DA6CAD0);
const u32 g_SpeechContextGreetVagosFemalePHash = ATPARTIALSTRINGHASH("GREET_GANG_VAGOS_F", 0x9DA6F61B);
const u32 g_SpeechContextGreetHippyMalePHash = ATPARTIALSTRINGHASH("GREET_HIPPY_M", 0xA6F15C17);
//const u32 g_SpeechContextGreetHippyFemalePHash = ATPARTIALSTRINGHASH("GREET_HIPPY_F", 0xA6F1406E);
const u32 g_SpeechContextGreetHipsterMalePHash = ATPARTIALSTRINGHASH("GREET_HIPSTER_M", 0x76B917CB);
const u32 g_SpeechContextGreetHipsterFemalePHash = ATPARTIALSTRINGHASH("GREET_HIPSTER_F", 0x76B9F872);
const u32 g_SpeechContextGreetStrongPHash = ATPARTIALSTRINGHASH("GREET_STRONG_M", 0x6AA4A863);
const u32 g_SpeechContextGreetTransvestitePHash = ATPARTIALSTRINGHASH("GREET_TRANSVESTITE", 0x59904A18);

const u32 g_SpeechContextChatStatePHash = ATPARTIALSTRINGHASH("CHAT_STATE", 0xF29859A7);
const u32 g_SpeechContextChatRespPHash = ATPARTIALSTRINGHASH("CHAT_RESP", 0xAD55B8C0);
const u32 g_SpeechContextChatAcrossStreetStatePHash = ATPARTIALSTRINGHASH("CHAT_ACROSS_STREET_STATE", 0x6567FC3B);
const u32 g_SpeechContextChatAcrossStreetRespPHash = ATPARTIALSTRINGHASH("CHAT_ACROSS_STREET_RESP", 0x659CF2);
const u32 g_SpeechContextPedRantPHash = ATPARTIALSTRINGHASH("PED_RANT", 0x4EE60A0E);
const u32 g_SpeechContextHookerStoryPHash = ATPARTIALSTRINGHASH("HOOKER_STORY", 0x369ED149);
const u32 g_SpeechContextGenericHowsItGoingPHash = ATPARTIALSTRINGHASH("GENERIC_HOWS_IT_GOING", 0x4A71E319);

const u32 g_JackingBikeContextPHash = ATPARTIALSTRINGHASH("JACKING_BIKE", 0x2A4D86AD);
const u32 g_JackingCarMaleContextPHash = ATPARTIALSTRINGHASH("JACKING_CAR_MALE", 0xA1348DB1);
const u32 g_JackingCarFemaleContextPHash = ATPARTIALSTRINGHASH("JACKING_CAR_FEMALE", 0xCCC77EC9);
const u32 g_JackingGenericContextPHash = ATPARTIALSTRINGHASH("JACKING_GENERIC", 0x6C72F004);
const u32 g_JackingDeadPedContextPHash = ATPARTIALSTRINGHASH("JACKING_DEAD_PED", 0x85FF0079);
const u32 g_JackedGenericContextPHash = ATPARTIALSTRINGHASH("JACKED_GENERIC", 0xE6407BDA);
const u32 g_JackedCarContextPHash = ATPARTIALSTRINGHASH("JACKED_CAR", 0x9D6E8438);
const u32 g_JackVehicleBackContextPHash = ATPARTIALSTRINGHASH("JACK_VEHICLE_BACK", 0x4103E4C8);
const u32 g_AngryWithPlayerMichaelContextPHash = ATPARTIALSTRINGHASH("ANGRY_WITH_PLAYER_MICHAEL", 0xFE185EB4);
const u32 g_AngryWithPlayerFranklinContextPHash = ATPARTIALSTRINGHASH("ANGRY_WITH_PLAYER_FRANKLIN", 0xBFA85B05);
const u32 g_AngryWithPlayerTrevorContextPHash = ATPARTIALSTRINGHASH("ANGRY_WITH_PLAYER_TREVOR", 0x9C698C5D);

s32 g_JackingPedsResponseDelay = -1;
s32 g_JackingPedsFirstLineDelay = -1;
s32 g_JackingPedsFirstLineDelayPassSide = 750;
s32 g_JackingVictimResponseDelay = -1;
s32 g_JackingVictimFirstLineDelay = 250;
s32 g_JackingVictimFirstLineDelayPassSide = 1000;
s32 g_MinTimeForJackingReply = 2000;
u32 g_MinTimeBetweenJackingSpeechTriggers = 4000;
u32 g_MaxTimeBetweenJackingInfoSetAndDoorOpening = 1500;
BANK_ONLY(bool g_ForcePlayerThenPedJackingSpeech = false;)
BANK_ONLY(bool g_ForcePedThenPlayerJackingSpeech = true;)

u32 g_TimeBetweenShotScreams = 10000;

char g_AlternateCryingContext[128] = "";
u32 g_MinTimeBetweenCries = 2000;
u32 g_MaxTimeBetweenCries = 5000;
u8 g_MaxPedsCrying = 4;

s32 g_MinPanicSpeechPredelay = 0;
s32 g_MaxPanicSpeechPredelay = 1500;

audCategory* g_SpeechNormalCategory;
audCategory* g_SpeechScriptedCategory;

audSoundSet g_AmbientSoundSet;
audSoundSet g_ScriptedSoundSet;
audSoundSet g_AnimalNearSoundSet;
audSoundSet g_AnimalFarSoundSet;
audSoundSet g_AnimalPainSoundSet;
audSoundSet g_WhaleNearSoundSet;
audSoundSet g_WhaleFarSoundSet;
audSoundSet g_WhalePainSoundSet;

audCurve audSpeechAudioEntity::sm_StartHeightToLandProbeWaitTime;
audCurve audSpeechAudioEntity::sm_HeadsetSpeechDistToVol;
audCurve audSpeechAudioEntity::sm_DistToListenerConeStrengthCurve;
audCurve audSpeechAudioEntity::sm_FEBubbleAngleToDistanceInner;
audCurve audSpeechAudioEntity::sm_FEBubbleAngleToDistanceOuter;
audCurve audSpeechAudioEntity::sm_EqualPowerRiseCrossFade;
audCurve audSpeechAudioEntity::sm_EqualPowerFallCrossFade;
audCurve audSpeechAudioEntity::sm_TimeSinceDamageToCoughProbability;
audCurve audSpeechAudioEntity::sm_BuiltUpToEchoWetMedium;
audCurve audSpeechAudioEntity::sm_BuiltUpToEchoWetLarge;
audCurve audSpeechAudioEntity::sm_BuiltUpToEchoVolLinShouted;
audCurve audSpeechAudioEntity::sm_BuiltUpToEchoVolLinMegaphone;
audCurve audSpeechAudioEntity::sm_EchoShoutedDistToVolLinCurve;
audCurve audSpeechAudioEntity::sm_EchoMegaphoneDistToVolLinCurve;
audCurve audSpeechAudioEntity::sm_VehicleOpennessToReverbVol;
audCurve audSpeechAudioEntity::sm_FallVelToStopHeightCurve;

const u32 g_NumEpsilonVoices = 9;
u32 g_EpsilonVoiceHashes[g_NumEpsilonVoices] = 
{
	ATSTRINGHASH("A_F_Y_EPSILON_01_WHITE_MINI_01", 0x1B66BF81),
	ATSTRINGHASH("A_M_Y_EPSILON_01_BLACK_FULL_01", 0x3C737DA3),
	ATSTRINGHASH("A_M_Y_EPSILON_01_KOREAN_FULL_01", 0xC5901506),
	ATSTRINGHASH("A_M_Y_EPSILON_01_WHITE_FULL_01", 0x8B5E6BA1),
	ATSTRINGHASH("A_M_Y_EPSILON_02_WHITE_MINI_01", 0x5929B31B),
	ATSTRINGHASH("A_M_Y_Epsilon_01_White_FULL_01", 0x8B5E6BA1),
	ATSTRINGHASH("A_M_Y_Epsilon_02_WHITE_MINI_01", 0x5929B31B),
	ATSTRINGHASH("A_M_Y_Epsilon_01_Black_FULL_01", 0x3C737DA3),
	ATSTRINGHASH("A_M_Y_Epsilon_01_Korean_FULL_01", 0xC5901506)
};

#if __BANK
const char* g_SpeechTypeNames[] =
{
	"AUD_SPEECH_TYPE_INVALID",
	"AUD_SPEECH_TYPE_AMBIENT",
	"AUD_SPEECH_TYPE_SCRIPTED",
	"AUD_SPEECH_TYPE_PAIN",
	"AUD_SPEECH_TYPE_ANIMAL_PAIN",
	"AUD_SPEECH_TYPE_ANIMAL_NEAR",
	"AUD_SPEECH_TYPE_ANIMAL_FAR",
	"AUD_SPEECH_TYPE_BREATHING"
};
CompileTimeAssert(NELEM(g_SpeechTypeNames) == AUD_NUM_SPEECH_TYPES);
#endif

const char* g_SpeechVolNames[AUD_NUM_SPEECH_VOLUMES] = {
	"SPOKEN",
	"SHOUTED",
	"FRONTEND",
	"MEGAPHONE"
};

const char* g_ConvAudNames[AUD_NUM_AUDIBILITIES] = {
	"NORMAL",
	"CLEAR",
	"CRITICAL",
	"LEAD_IN"
};

audCurve audSpeechAudioEntity::sm_SpeechDistanceToHPFCurve;
atMultiRangeArray<audCurve, AUD_NUM_SPEECH_VOLUMES, AUD_NUM_AUDIBILITIES> audSpeechAudioEntity::sm_ScriptedDistToVolCurve;
atMultiRangeArray<audCurve, AUD_NUM_SPEECH_VOLUMES, AUD_NUM_AUDIBILITIES> audSpeechAudioEntity::sm_AmbientDistToVolCurve;
atMultiRangeArray<audCurve, AUD_NUM_SPEECH_VOLUMES, AUD_NUM_AUDIBILITIES> audSpeechAudioEntity::sm_PainDistToVolCurve;

u32 audSpeechAudioEntity::sm_LastTimeCoughCheckWasReset = 0;
u32	audSpeechAudioEntity::sm_TimeOfNextCoughSpeech = 0;
bool audSpeechAudioEntity::sm_PerformedCoughCheckThisFrame = false;

atRangeArray<u32, AUD_AMB_MOD_TOT> audSpeechAudioEntity::sm_LastTimeAmbSpeechModsUsed;
atRangeArray<audBlockedContextStruct, AUD_MAX_BLOCKED_CONTEXT_LISTS> audSpeechAudioEntity::sm_BlockedContextLists;

u8 audSpeechAudioEntity::sm_NumCollisionScreamsToTrigger = 0;

ePedType audSpeechAudioEntity::sm_PlayerPedType = PEDTYPE_PLAYER_0;

u32 audSpeechAudioEntity::sm_TimeToNextPlayShotScream;

u32 audSpeechAudioEntity::sm_NumPedsCrying = 0;

f32 audSpeechAudioEntity::sm_UnderwaterLPF = kVoiceFilterLPFMaxCutoff;
f32 audSpeechAudioEntity::sm_UnderwaterReverb = 0.0f;

u32 audSpeechAudioEntity::sm_TimeOfLastPlayerCarHitPedSpeech = 0;
u32 audSpeechAudioEntity::sm_TimeOfLastChat = 0;

u32 audSpeechAudioEntity::sm_TimeOfLastSaySpaceRanger = 0;

RegdPed audSpeechAudioEntity::sm_PedToPlayGetWantedLevel;

#if __BANK
bkCombo * g_pSpeechCombo = NULL;
bool g_MakePedSpeak = false;
bool g_ForceAllowGestures = false;
bool g_PreloadOnly = false;
bool g_TriggerPreload = false;
bool g_MakePedDoMegaphone = false;
bool g_MakePedDoPanic = false;
bool g_MakePedDoBlocked = false;
bool g_MakePedDoCoverMe = false;
bool g_MakePedDoHeli = false;
bool g_TestWallaSound = false;
bool g_DebugDrawLaughterTrack = false;
bool g_LaughterTrackEditorEnabled = false;
s32 g_LaughterTrackChannelSelection = 0;
f32 g_LaughterTrackEditorScroll = 0.0f;
const char* g_LaughterTrackTVShowName = NULL;
atArray<RadioTrackTextIds::tTextId> g_LaughterTrackEditorTrackIDs;

audSound* g_WallaTestSoundPtr = NULL;
s32 g_WallaTestSlot = -1;

static char g_TestVoice[128] = "MICHAEL";
static char g_TestContext[128] = "JH_RJAB";

bool g_RunCanChatTest = false;
bool g_TriggeredContextTest = false;
CPed* g_ChatTestOtherPed = NULL;

bool g_UseTestPainBanks = false;

bool g_MakeFocusPedCry = false;

int g_SpeechTypeOverride = AUD_SPEECH_TYPE_AMBIENT;
int g_SpeechVolumeTypeOverride = DEBUG_SPEECH_VOL_AMBIENT_NORMAL;
int g_SpeechAudibilityOverride = AUD_AUDIBILITY_NORMAL;
bool g_ShouldOverrideSpeechType = false;
bool g_ShouldOverrideSpeechVolumeType = false;
bool g_ShouldOverrideSpeechAudibility = false;
bool g_DebugIsInCarFEScene = false;
bool g_DrawFESpeechBubble = false;
bool g_DebugVolumeMetrics = false;
bool g_EchoSolo = false;
bool g_DebugEchoes = false;
#endif

extern audCurve g_VehicleOpennessToSpeechFilterCurve;
extern audCurve g_VehicleOpennessToSpeechVolumeCurve;

const u32 g_PlayerInhaleSound = ATSTRINGHASH("PLAYER_INHALE", 0xE5C67BEC);
const u32 g_PlayerExhaleSound = ATSTRINGHASH("PLAYER_EXHALE", 0x6526DD4F);
const u32 g_PlayerBikeInhaleSound = ATSTRINGHASH("PLAYER_BIKE_INHALE", 0x56A6B83B);
const u32 g_PlayerBikeExhaleSound = ATSTRINGHASH("PLAYER_BIKE_EXHALE", 0xA3D2403);
const u32 g_PlayerBikeCyclingExhale = ATSTRINGHASH("PLAYER_BIKE_CYCLING_EXHALE", 0x7633B049);
const u32 g_PainDeathPHash = ATPARTIALSTRINGHASH("PAIN_DEATH", 0x5E666895);
const u32 g_DeathGarglePHash = ATPARTIALSTRINGHASH("DEATH_GARGLE", 0x663765A1);
const u32 g_DeathHighLongPHash = ATPARTIALSTRINGHASH("DEATH_HIGH_LONG", 0xF9B7C1B1);
const u32 g_DeathHighMediumPHash = ATPARTIALSTRINGHASH("DEATH_HIGH_MEDIUM", 0xB959905D);
const u32 g_DeathHighShortPHash = ATPARTIALSTRINGHASH("DEATH_HIGH_SHORT", 0x275DD821);
const u32 g_DeathUnderwaterPHash = ATPARTIALSTRINGHASH("DEATH_UNDERWATER", 0x3BD03BA7);
const char* g_PainContexts[AUD_PAIN_ID_NUM_TOTAL];
const char* g_PlayerPainContexts[AUD_PAIN_ID_NUM_TOTAL];
u32 g_PainContextHashs[AUD_PAIN_ID_NUM_TOTAL];
u32 g_PainContextPHashs[AUD_PAIN_ID_NUM_TOTAL];
u32 g_PlayerPainContextHashs[AUD_PAIN_ID_NUM_TOTAL];
u32 g_PlayerPainContextPHashs[AUD_PAIN_ID_NUM_TOTAL];
audSoundSet g_PainSoundSet[AUD_PAIN_ID_NUM_TOTAL];
bool g_DisablePain = false;
f32 g_HealthLostForMediumPain = 27.0f;
f32 g_HealthLostForHighPain = 31.0f;
f32 g_HealthLostForHighDeath = 27.0f;
f32 g_HealthLostForLowPain = 0.35f;
u32 g_TimeBetweenPain = 0;
u32 g_TimeBetweenDyingMoans = 4000;
f32 g_DamageScalarDiffMax = 0.01f;
f32 g_ProbabilityOfDeathDrop = 0.15f;
f32 g_ProbabilityOfDeathRise = 0.15f;
f32 g_ProbabilityOfPainDrop = 0.15f;
f32 g_ProbabilityOfPainRise = 0.15f;
f32 g_RunOverByVehicleDamageScalar = 3.0f;
s32 g_MinTimeBetweenLungDamageCough = 500;
s32 g_MaxTimeBetweenLungDamageCough = 1500;

bool g_EnablePainedBreathing = false;
u32 g_TimeBetweenPainInhaleAndExhale = 1000;
u32 g_TimeBetweenPainExhaleAndInhale = 2500;

bool g_DisablePlayerBreathing = false;
BANK_ONLY(bool g_EnableBreathingSpew = false;)

u32 g_TimeRunningBeforeBreathing = 5000;
f32 g_RunStartVolRampUpTimeMs = 8000.0f;
f32 g_JogVolRampDownTimeMs1 = 1000.0f;
f32 g_JogVolRampDownTimeMs2 = 7000.0f;
f32 g_RunStartBreathVolInc = 30.0f/g_RunStartVolRampUpTimeMs;
f32 g_JogHighBreathVolDec = -30.0f/g_JogVolRampDownTimeMs1;
f32 g_JogRampDownVolDec  = -30.0f/g_JogVolRampDownTimeMs2;
s32 g_MinTimeBetweenJogLowBreath = 3000;
s32 g_MaxTimeBetweenJogLowBreath = 5500;
s32 g_MinPredelayOnBreath = 75;
s32 g_MaxPredelayOnBreath = 200;
u32 g_TimeJoggingBeforeLeavingRunBreathing = 1000;

u32 g_TimePedalingBeforeBreathing = 200;
f32 g_BikeStartVolRampUpTimeMs = 4500.0f;
f32 g_BikeStartBreathVolInc = 30.0f/g_BikeStartVolRampUpTimeMs;
s32 g_MinPredelayOnBikeBreath = 200;
s32 g_MaxPredelayOnBikeBreath = 350;
u32 g_TimeNotPedalingBeforeLeavingBikeBreathing = 500;
u32 g_TimeToPedalBeforeCycleExhale = 30000;
f32 g_BikeSlopeAngle = 15.0f;

u32 g_TimeBetweenScubaBreaths = 2000;

u32 g_TimeToSmoothDownFromHighAudibility = 10000;

bool audSpeechAudioEntity::ms_ShouldProcessVisemes = true;

u32 audSpeechAudioEntity::sm_TimeLastPlayedJackingSpeech = 0;
u32 audSpeechAudioEntity::sm_TimeLastPlayedHighFall = 0;

u32 audSpeechAudioEntity::sm_TimeLastPlayedCrashSpeechFromPlayerCar = 0;

extern const char* g_PainSlotType[4];

extern const u32 g_NoVoiceHash = ATSTRINGHASH("NO_VOICE", 0x087bff09a);

#if __BANK
s32 g_AnimTestSpeechVariation = 0;
#endif // __BANK

PARAM(speechasserts, "Enable asserts due to NULL SpeechContext");
bool g_bSpeechAsserts = false;

static const char *g_SpeechMetadataCategoryNames[NUM_AUD_SPEECH_METADATA_CATEGORIES] =
{
	"G_S",	//AUD_SPEECH_METADATA_CATEGORY_GESTURE_SPEAKER
	"G_L",	//AUD_SPEECH_METADATA_CATEGORY_GESTURE_LISTENER
	"F_S",	//AUD_SPEECH_METADATA_CATEGORY_FACIAL_SPEAKER
	"F_L",	//AUD_SPEECH_METADATA_CATEGORY_FACIAL_LISTENER
	"R_P"	//AUD_SPEECH_METADATA_CATEGORY_RESTART_POINT
};

extern const char* g_AnimalBankNames[ANIMALTYPE_MAX];

static const char *g_AmbSpeechModifiers[AUD_AMB_MOD_TOT] =
{
	"_MORNING",
	"_EVENING",
	"_NIGHT",
	"_RAINING",
};

void audSpeechAudioEntity::InitClass()
{
	g_AmbientSoundSet.Init(ATSTRINGHASH("SPEECH_AMBIENT_SS", 0xf5e522c0));
	g_ScriptedSoundSet.Init(ATSTRINGHASH("SPEECH_SCRIPTED_SS", 0x2e7561b2));
	g_AnimalNearSoundSet.Init(ATSTRINGHASH("ANIMAL_VOCAL_NEAR_SS", 0x626f6467));
	g_AnimalFarSoundSet.Init(ATSTRINGHASH("ANIMAL_VOCAL_FAR_SS", 0x826e6115));
	g_AnimalPainSoundSet.Init(ATSTRINGHASH("ANIMAL_VOCAL_PAIN_SS", 0x2cce325f));

	g_WhaleNearSoundSet.Init(ATSTRINGHASH("WHALE_VOCAL_NEAR_SS", 0xCFF03D81));
	g_WhaleFarSoundSet.Init(ATSTRINGHASH("WHALE_VOCAL_FAR_SS", 0xCC955151));
	g_WhalePainSoundSet.Init(ATSTRINGHASH("WHALE_VOCAL_PAIN_SS", 0x82EF4181));


	g_PainSoundSet[AUD_PAIN_ID_PAIN_LOW_WEAK].Init(ATSTRINGHASH("PAIN_LOW_SS", 0x660635d1));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_LOW_GENERIC].Init(ATSTRINGHASH("PAIN_LOW_SS", 0x660635d1));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_LOW_TOUGH].Init(ATSTRINGHASH("PAIN_LOW_SS", 0x660635d1));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_MEDIUM_WEAK].Init(ATSTRINGHASH("PAIN_MEDIUM_SS", 0x163a44f9));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_MEDIUM_GENERIC].Init(ATSTRINGHASH("PAIN_MEDIUM_SS", 0x163a44f9));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_MEDIUM_TOUGH].Init(ATSTRINGHASH("PAIN_MEDIUM_SS", 0x163a44f9));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_LOW_WEAK].Init(ATSTRINGHASH("PAIN_DEATH_LOW_SS", 0x85cbaa23));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_LOW_GENERIC].Init(ATSTRINGHASH("PAIN_DEATH_LOW_SS", 0x85cbaa23));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_LOW_TOUGH].Init(ATSTRINGHASH("PAIN_DEATH_LOW_SS", 0x85cbaa23));
	g_PainSoundSet[AUD_PAIN_ID_HIGH_FALL_DEATH].Init(ATSTRINGHASH("PAIN_HIGH_FALL_DEATH_SS", 0xadaedd3c));
	g_PainSoundSet[AUD_PAIN_ID_CYCLING_EXHALE].Init(ATSTRINGHASH("PAIN_CYCLING_EXHALE_SS", 0x4588e815));
	g_PainSoundSet[AUD_PAIN_ID_EXHALE].Init(ATSTRINGHASH("PAIN_EXHALE_SS", 0xfcd68255));
	g_PainSoundSet[AUD_PAIN_ID_INHALE].Init(ATSTRINGHASH("PAIN_INHALE_SS", 0x113b5b1f));
	g_PainSoundSet[AUD_PAIN_ID_SHOVE].Init(ATSTRINGHASH("PAIN_SHOVE_SS", 0xa047d358));
	g_PainSoundSet[AUD_PAIN_ID_WHEEZE].Init(ATSTRINGHASH("PAIN_WHEEZE_SS", 0x607f106d));
	g_PainSoundSet[AUD_PAIN_ID_CLIMB_LARGE].Init(ATSTRINGHASH("CLIMB_LARGE_SS", 0x332d989));
	g_PainSoundSet[AUD_PAIN_ID_CLIMB_SMALL].Init(ATSTRINGHASH("CLIMB_SMALL_SS", 0x6962bba3));
	g_PainSoundSet[AUD_PAIN_ID_JUMP].Init(ATSTRINGHASH("JUMP_SS", 0x35a9fbf8));
	g_PainSoundSet[AUD_PAIN_ID_MELEE_SMALL_GRUNT].Init(ATSTRINGHASH("PAIN_MELEE_SMALL_GRUNT_SS", 0xB9F807D6));
	g_PainSoundSet[AUD_PAIN_ID_MELEE_LARGE_GRUNT].Init(ATSTRINGHASH("PAIN_MELEE_LARGE_GRUNT_SS", 0xF4785CB3));
	g_PainSoundSet[AUD_PAIN_ID_COWER].Init(ATSTRINGHASH("COWER_SS", 0xc772439d));
	g_PainSoundSet[AUD_PAIN_ID_WHIMPER].Init(ATSTRINGHASH("WHIMPER_SS", 0x1f4cdf81));
	g_PainSoundSet[AUD_PAIN_ID_DYING_MOAN].Init(ATSTRINGHASH("DYING_MOAN_SS", 0xee68f504));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_RAPIDS].Init(ATSTRINGHASH("PAIN_EXHALE_SS", 0xfcd68255));

	g_PainSoundSet[AUD_PAIN_ID_PAIN_HIGH_WEAK].Init(ATSTRINGHASH("PAIN_HIGH_SS", 0x97bef04e));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_HIGH_GENERIC].Init(ATSTRINGHASH("PAIN_HIGH_SS", 0x97bef04e));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_HIGH_TOUGH].Init(ATSTRINGHASH("PAIN_HIGH_SS", 0x97bef04e));
	g_PainSoundSet[AUD_PAIN_ID_PAIN_GARGLE].Init(ATSTRINGHASH("PAIN_GARGLE_SOUND_SS", 0x9d4bb420));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_GARGLE].Init(ATSTRINGHASH("DEATH_GARGLE_SOUND_SS", 0x72cc6066));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_HIGH_SHORT].Init(ATSTRINGHASH("PAIN_DEATH_HIGH_SHORT_SS", 0x9610210b));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_HIGH_MEDIUM].Init(ATSTRINGHASH("PAIN_DEATH_HIGH_MEDIUM_SS", 0xc0cb4ce8));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_HIGH_LONG].Init(ATSTRINGHASH("PAIN_DEATH_HIGH_LONG_SS", 0x6f6e952));
	g_PainSoundSet[AUD_PAIN_ID_COUGH].Init(ATSTRINGHASH("PAIN_COUGH_SS", 0xbe320b08));
	g_PainSoundSet[AUD_PAIN_ID_SNEEZE].Init(ATSTRINGHASH("PAIN_COUGH_SS", 0xbe320b08));
	g_PainSoundSet[AUD_PAIN_ID_ON_FIRE].Init(ATSTRINGHASH("PAIN_ON_FIRE_SS", 0xd273560c));
	g_PainSoundSet[AUD_PAIN_ID_HIGH_FALL].Init(ATSTRINGHASH("PAIN_HIGH_FALL_SS", 0x7ece8184));
	g_PainSoundSet[AUD_PAIN_ID_SUPER_HIGH_FALL].Init(ATSTRINGHASH("PAIN_SUPER_HIGH_FALL_SS", 0xac3944e3));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_PANIC].Init(ATSTRINGHASH("PAIN_SCREAM_PANIC_SS", 0x279dc5f4));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_PANIC_SHORT].Init(ATSTRINGHASH("PAIN_SCREAM_PANIC_SHORT_SS", 0xC51B4EB3));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_SCARED].Init(ATSTRINGHASH("PAIN_SCREAM_SCARED_SS", 0x7d20b535));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_SHOCKED_WEAK].Init(ATSTRINGHASH("PAIN_SCREAM_SHOCKED_SS", 0xfa6f74a6));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC].Init(ATSTRINGHASH("PAIN_SCREAM_SHOCKED_SS", 0xfa6f74a6));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_SHOCKED_TOUGH].Init(ATSTRINGHASH("PAIN_SCREAM_SHOCKED_SS", 0xfa6f74a6));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_TERROR_WEAK].Init(ATSTRINGHASH("PAIN_SCREAM_TERROR_SS", 0xda4ec54c));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_TERROR_GENERIC].Init(ATSTRINGHASH("PAIN_SCREAM_TERROR_SS", 0xda4ec54c));
	g_PainSoundSet[AUD_PAIN_ID_SCREAM_TERROR_TOUGH].Init(ATSTRINGHASH("PAIN_SCREAM_TERROR_SS", 0xda4ec54c));
	g_PainSoundSet[AUD_PAIN_ID_TAZER].Init(ATSTRINGHASH("PAIN_TAZER_SOUND_SS", 0x1e4a1f71));
	g_PainSoundSet[AUD_PAIN_ID_DEATH_UNDERWATER].Init(ATSTRINGHASH("PAIN_DEATH_UNDERWATER_SS", 0xaf2ea409));

	g_PainContexts[AUD_PAIN_ID_PAIN_LOW_WEAK] = "PAIN_LOW_WEAK";
	g_PainContexts[AUD_PAIN_ID_PAIN_LOW_GENERIC] = "PAIN_LOW_GENERIC";
	g_PainContexts[AUD_PAIN_ID_PAIN_LOW_TOUGH] = "PAIN_LOW_TOUGH";
	g_PainContexts[AUD_PAIN_ID_PAIN_MEDIUM_WEAK] = "PAIN_MEDIUM_WEAK";
	g_PainContexts[AUD_PAIN_ID_PAIN_MEDIUM_GENERIC] = "PAIN_MEDIUM_GENERIC";
	g_PainContexts[AUD_PAIN_ID_PAIN_MEDIUM_TOUGH] = "PAIN_MEDIUM_TOUGH";
	g_PainContexts[AUD_PAIN_ID_DEATH_LOW_WEAK] = "DEATH_LOW_WEAK";
	g_PainContexts[AUD_PAIN_ID_DEATH_LOW_GENERIC] = "DEATH_LOW_GENERIC";
	g_PainContexts[AUD_PAIN_ID_DEATH_LOW_TOUGH] = "DEATH_LOW_TOUGH";
	g_PainContexts[AUD_PAIN_ID_CYCLING_EXHALE] = "EXHALE_CYCLING";
	g_PainContexts[AUD_PAIN_ID_EXHALE] = "EXHALE";
	g_PainContexts[AUD_PAIN_ID_INHALE] = "INHALE";
	g_PainContexts[AUD_PAIN_ID_SHOVE] = "PAIN_SHOVE";
	g_PainContexts[AUD_PAIN_ID_WHEEZE] = "PAIN_WHEEZE";
	g_PainContexts[AUD_PAIN_ID_DEATH_GARGLE] = "DEATH_GARGLE";
	g_PainContexts[AUD_PAIN_ID_CLIMB_LARGE] = "CLIMB_LARGE";
	g_PainContexts[AUD_PAIN_ID_CLIMB_SMALL] = "CLIMB_SMALL";
	g_PainContexts[AUD_PAIN_ID_JUMP] = "JUMP";
	g_PainContexts[AUD_PAIN_ID_MELEE_SMALL_GRUNT] = "MELEE_SMALL_GRUNT";
	g_PainContexts[AUD_PAIN_ID_MELEE_LARGE_GRUNT] = "MELEE_LARGE_GRUNT";

	g_PainContexts[AUD_PAIN_ID_PAIN_HIGH_WEAK] = "PAIN_HIGH_WEAK";
	g_PainContexts[AUD_PAIN_ID_PAIN_HIGH_GENERIC] = "PAIN_HIGH_GENERIC";
	g_PainContexts[AUD_PAIN_ID_PAIN_HIGH_TOUGH] = "PAIN_HIGH_TOUGH";
	g_PainContexts[AUD_PAIN_ID_PAIN_GARGLE] = "PAIN_GARGLE";
	g_PainContexts[AUD_PAIN_ID_DEATH_HIGH_SHORT] = "DEATH_HIGH_SHORT";
	g_PainContexts[AUD_PAIN_ID_DEATH_HIGH_MEDIUM] = "DEATH_HIGH_MEDIUM";
	g_PainContexts[AUD_PAIN_ID_DEATH_HIGH_LONG] = "DEATH_HIGH_LONG";
	g_PainContexts[AUD_PAIN_ID_HIGH_FALL_DEATH] = "HIGH_FALL_DEATH";
	g_PainContexts[AUD_PAIN_ID_COUGH] = "COUGH";
	g_PainContexts[AUD_PAIN_ID_SNEEZE] = "SNEEZE";
	g_PainContexts[AUD_PAIN_ID_ON_FIRE] = "ON_FIRE";
	g_PainContexts[AUD_PAIN_ID_HIGH_FALL] = "HIGH_FALL";
	g_PainContexts[AUD_PAIN_ID_SUPER_HIGH_FALL] = "HIGH_FALL";
	g_PainContexts[AUD_PAIN_ID_SCREAM_PANIC] = "SCREAM_PANIC";
	g_PainContexts[AUD_PAIN_ID_SCREAM_PANIC_SHORT] = "SCREAM_PANIC_SHORT";
	g_PainContexts[AUD_PAIN_ID_SCREAM_SCARED] = "SCREAM_SCARED";
	g_PainContexts[AUD_PAIN_ID_SCREAM_SHOCKED_WEAK] = "SCREAM_SHOCKED";
	g_PainContexts[AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC] = "SCREAM_SHOCKED";
	g_PainContexts[AUD_PAIN_ID_SCREAM_SHOCKED_TOUGH] = "SCREAM_SHOCKED";
	g_PainContexts[AUD_PAIN_ID_SCREAM_TERROR_WEAK] = "SCREAM_TERROR";
	g_PainContexts[AUD_PAIN_ID_SCREAM_TERROR_GENERIC] = "SCREAM_TERROR";
	g_PainContexts[AUD_PAIN_ID_SCREAM_TERROR_TOUGH] = "SCREAM_TERROR";
	g_PainContexts[AUD_PAIN_ID_TAZER] = "PAIN_TAZER";
	g_PainContexts[AUD_PAIN_ID_DEATH_UNDERWATER] = "DEATH_UNDERWATER";
	g_PainContexts[AUD_PAIN_ID_COWER] = "COWER";
	g_PainContexts[AUD_PAIN_ID_WHIMPER] = "WHIMPER";
	g_PainContexts[AUD_PAIN_ID_DYING_MOAN] = "DYING_MOAN";
	g_PainContexts[AUD_PAIN_ID_PAIN_RAPIDS] = "PAIN_RAPIDS";

	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_LOW_WEAK] = "PAIN_LOW";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_LOW_GENERIC] = "PAIN_LOW";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_LOW_TOUGH] = "PAIN_LOW";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_MEDIUM_WEAK] = "PAIN_MED";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_MEDIUM_GENERIC] = "PAIN_MED";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_MEDIUM_TOUGH] = "PAIN_MED";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_LOW_WEAK] = "DEATH_LOW";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_LOW_GENERIC] = "DEATH_LOW";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_LOW_TOUGH] = "DEATH_LOW";
	g_PlayerPainContexts[AUD_PAIN_ID_CYCLING_EXHALE] = "EXHALE_CYCLING";
	g_PlayerPainContexts[AUD_PAIN_ID_EXHALE] = "EXHALE";
	g_PlayerPainContexts[AUD_PAIN_ID_INHALE] = "INHALE";
	g_PlayerPainContexts[AUD_PAIN_ID_SHOVE] = "PAIN_SHOVE";
	g_PlayerPainContexts[AUD_PAIN_ID_WHEEZE] = "PAIN_WHEEZE";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_GARGLE] = "DEATH_GARGLE";
	g_PlayerPainContexts[AUD_PAIN_ID_CLIMB_LARGE] = "CLIMB_LARGE";
	g_PlayerPainContexts[AUD_PAIN_ID_CLIMB_SMALL] = "CLIMB_SMALL";
	g_PlayerPainContexts[AUD_PAIN_ID_JUMP] = "JUMP";
	g_PlayerPainContexts[AUD_PAIN_ID_MELEE_SMALL_GRUNT] = "MELEE_SMALL_GRUNT";
	g_PlayerPainContexts[AUD_PAIN_ID_MELEE_LARGE_GRUNT] = "MELEE_LARGE_GRUNT";

	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_HIGH_WEAK] = "PAIN_HIGH";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_HIGH_GENERIC] = "PAIN_HIGH";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_HIGH_TOUGH] = "PAIN_HIGH";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_GARGLE] = "PAIN_GARGLE";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_HIGH_SHORT] = "DEATH_HIGH_SHORT";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_HIGH_MEDIUM] = "DEATH_HIGH_MEDIUM";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_HIGH_LONG] = "DEATH_HIGH_LONG";
	g_PlayerPainContexts[AUD_PAIN_ID_HIGH_FALL_DEATH] = "HIGH_FALL_DEATH";
	g_PlayerPainContexts[AUD_PAIN_ID_COUGH] = "COUGH";
	g_PlayerPainContexts[AUD_PAIN_ID_SNEEZE] = "SNEEZE";
	g_PlayerPainContexts[AUD_PAIN_ID_ON_FIRE] = "ON_FIRE";
	g_PlayerPainContexts[AUD_PAIN_ID_HIGH_FALL] = "HIGH_FALL";
	g_PlayerPainContexts[AUD_PAIN_ID_SUPER_HIGH_FALL] = "SUPER_HIGH_FALL";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_PANIC] = "SCREAM_PANIC";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_PANIC_SHORT] = "SCREAM_PANIC_SHORT";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_SCARED] = "SCREAM_SCARED";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_SHOCKED_WEAK] = "SCREAM_SHOCKED_WEAK";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC] = "SCREAM_SHOCKED";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_SHOCKED_TOUGH] = "SCREAM_SHOCKED_TOUGH";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_TERROR_WEAK] = "SCREAM_TERROR_WEAK";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_TERROR_GENERIC] = "SCREAM_TERROR";
	g_PlayerPainContexts[AUD_PAIN_ID_SCREAM_TERROR_TOUGH] = "SCREAM_TERROR_TOUGH";
	g_PlayerPainContexts[AUD_PAIN_ID_TAZER] = "PAIN_TAZER";
	g_PlayerPainContexts[AUD_PAIN_ID_DEATH_UNDERWATER] = "DEATH_UNDERWATER";
	g_PlayerPainContexts[AUD_PAIN_ID_COWER] = "COWER";
	g_PlayerPainContexts[AUD_PAIN_ID_WHIMPER] = "WHIMPER";
	g_PlayerPainContexts[AUD_PAIN_ID_DYING_MOAN] = "DYING_MOAN";
	g_PlayerPainContexts[AUD_PAIN_ID_PAIN_RAPIDS] = "PAIN_RAPIDS";

	for(int i=0; i<AUD_PAIN_ID_NUM_TOTAL; i++)
	{
		g_PainContextHashs[i] = g_PainContexts[i] ? atStringHash(g_PainContexts[i]) : 0;
		g_PainContextPHashs[i] = g_PainContexts[i] ? atPartialStringHash(g_PainContexts[i]) : 0;
		g_PlayerPainContextHashs[i] = g_PlayerPainContexts[i] ? atStringHash(g_PlayerPainContexts[i]) : 0;
		g_PlayerPainContextPHashs[i] = g_PlayerPainContexts[i] ? atPartialStringHash(g_PlayerPainContexts[i]) : 0;
	}

	g_SpeechNormalCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("SPEECH", 0x009bbf7c4));
	g_SpeechScriptedCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("SPEECH_SCRIPTED", 0xB77A4E4D));

	g_DoesLandProbeWaitCurveExist = sm_StartHeightToLandProbeWaitTime.Init("START_HEIGHT_TO_LAND_PROBE_WAIT_TIME");
	sm_HeadsetSpeechDistToVol.Init("HEADSET_SPEECH_DIST_SQ_TO_VOLUME");
	sm_DistToListenerConeStrengthCurve.Init("DIST_TO_LISTENER_CONE_STRENGTH");
	sm_FEBubbleAngleToDistanceInner.Init("SPEECH_FE_BUBBLE_ANGLE_TO_DIST_INNER");
	sm_FEBubbleAngleToDistanceOuter.Init("SPEECH_FE_BUBBLE_ANGLE_TO_DIST_OUTER");
	sm_EqualPowerFallCrossFade.Init("EQUAL_POWER_FALL_CROSSFADE");
	sm_EqualPowerRiseCrossFade.Init("EQUAL_POWER_RISE_CROSSFADE");
	sm_TimeSinceDamageToCoughProbability.Init("SPEECH_TIME_SINCE_DAMAGE_TO_COUGH_PROB");
	sm_BuiltUpToEchoWetMedium.Init("SPEECH_BUILT_UP_TO_ECHO_WET_MED");
	sm_BuiltUpToEchoWetLarge.Init("SPEECH_BUILT_UP_TO_ECHO_WET_LARGE");
	sm_BuiltUpToEchoVolLinShouted.Init("SPEECH_BUILT_UP_TO_ECHO_VOL_SHOUTED");
	sm_BuiltUpToEchoVolLinMegaphone.Init("SPEECH_BUILT_UP_TO_ECHO_VOL_MEGAPHONE");
	sm_EchoShoutedDistToVolLinCurve.Init("SPEECH_ECHO_SHOUTED_DIST_TO_VOL_LIN");
	sm_EchoMegaphoneDistToVolLinCurve.Init("SPEECH_ECHO_MEGAPHONE_DIST_TO_VOL_LIN");
	sm_SpeechDistanceToHPFCurve.Init("SPEECH_DIST_TO_HPF");
	sm_VehicleOpennessToReverbVol.Init("SPEECH_VEH_OPEN_TO_REVERB_VOL_LIN");
	sm_FallVelToStopHeightCurve.Init("FALL_VEL_TO_STOP_HEIGHT");

	char buf[64];
	for(u32 i = 0; i < AUD_NUM_SPEECH_VOLUMES; i++)
	{
		for(u32 j = 0; j < AUD_NUM_AUDIBILITIES; j++)
		{
			formatf(buf, "SCRIPT_%s_%s_VOL", g_SpeechVolNames[i], g_ConvAudNames[j]);
			sm_ScriptedDistToVolCurve[i][j].Init(buf);
			formatf(buf, "AMB_%s_%s_VOL", g_SpeechVolNames[i], g_ConvAudNames[j]);
			sm_AmbientDistToVolCurve[i][j].Init(buf);
			formatf(buf, "PAIN_%s_%s_VOL", g_SpeechVolNames[i], g_ConvAudNames[j]);
			sm_PainDistToVolCurve[i][j].Init(buf);
		}
	}

	sm_TimeToNextPlayShotScream = 0;
	sm_TimeLastPlayedHighFall = 0;

	sm_LocalPlayerVoice = g_MichaelVoice;
	//sm_LocalPlayerPainVoice = g_PainNiko;

	sm_TrevorRageContextOverride[0] = '\0';
	BANK_ONLY(sm_VisemeTimingingInfoString[0] = '\0';)

	for(int i=0; i<AUD_MAX_BLOCKED_CONTEXT_LISTS; i++)
	{
		sm_BlockedContextLists[i].ScriptThreadId = THREAD_INVALID;
		sm_BlockedContextLists[i].ContextList = NULL;
	}

	sm_PedToPlayGetWantedLevel = NULL;

	NOTFINAL_ONLY(g_EnablePlaceholderDialogue = !PARAM_noplaceholderdialogue.Get());

#if __BANK
		
	if(PARAM_fullspeechPVG.Get())
		g_FullSpeechPVG = true;
	if(PARAM_silentspeech.Get())
		g_EnableSilentSpeech = true;
	if(PARAM_ForceFakeGestures.Get())
		g_ForceFakeGestures = true;

	//Uncomment to get a printout of voice name hashes upon bootup.  Found this useful for tracking down some issues.
	//DisplayVoiceNameHashes();
#endif

	if(PARAM_speechasserts.Get())
		g_bSpeechAsserts = true;

	if(PARAM_disableAmbientSpeech.Get())
		g_DisableAmbientSpeech = true;

	if(PARAM_bypassSpeechStreamCheck.Get())
		g_BypassStreamingCheckForSpeech = true;

	if(PARAM_stopForUrgentSpeech.Get())
		g_EnableEarlyStoppingForUrgentSpeech = true;
}

audSpeechAudioEntity::~audSpeechAudioEntity()
{
	naSpeechEntDebugf2(this, "~audSpeechAudioEntity");

	// Stop any speech we're playing
	// Don't do this if we've never been initialised - see if we've got a controller
	if (GetControllerId()!=AUD_INVALID_CONTROLLER_ENTITY_ID)
	{
		// Pass in false to the GetPedVoiceGroup here to make sure we don't try and create the voice
		// if its unassigned, the data we base that decision on may be deleted as this is called
		// from the ped destructor, if a voice hasn't been decided the remove reference will do nothing anyway
		OrphanSpeech();
		g_SpeechManager.RemoveReferenceToVoice(GetPedVoiceGroup(false), m_AmbientVoiceNameHash);
		FreeSlot(false);
		g_SpeechManager.SpeechAudioEntityBeingDeleted(this);
	}
	if(m_ReplyingPed)
	{
		m_ReplyingPed = NULL;
		m_DelayJackingReply = false;
		m_TimeToPlayJackedReply = 0;
	}

	if(m_IsAngry)
		g_ScriptAudioEntity.SetPlayerAngry(this, false);

	StopCrying();

	if(g_ScriptAudioEntity.IsPedInCurrentConversation(m_Parent))
		g_ScriptAudioEntity.AbortScriptedConversation(false BANK_ONLY(,"destructor"));

	if(m_AnimalParams && m_WasMakingAnimalSoundLastFrame)
		g_SpeechManager.DecrementAnimalsMakingSound(m_AnimalType);

	if(m_SpeechSyncId != audMixerSyncManager::InvalidId && audMixerSyncManager::Get())
	{
		audMixerSyncManager::Get()->Release(m_SpeechSyncId);
		m_SpeechSyncId = audMixerSyncManager::InvalidId;
	}
#if __BANK
	if(m_Parent)
	{
		RemoveVFXDebugDrawStruct(m_Parent);
		RemoveAmbSpeechDebugDrawStruct(m_Parent);
	}
#endif
}

void audSpeechAudioEntity::Init(CPed *parent)
{
	naAudioEntity::Init();
	audEntity::SetName("audSpeechAudioEntity");
	m_Parent = parent;

#if ENABLE_VISEMES
	if (!parent)
		m_IgnoreVisemeData = true;
#endif

	if (m_Parent && m_Parent->IsLocalPlayer())
		g_SpeechManager.SetPlayerPainRootBankName(*m_Parent);

	if (m_Parent && PARAM_phoneConversationHistoryTest.Get())
	{
		m_AmbientVoiceNameHash = g_SpeechManager.GetVoiceForPhoneConversation(GetPedVoiceGroup());
		s32 convVar = g_SpeechManager.GetPhoneConversationVariationFromVoiceHash(m_AmbientVoiceNameHash);
		if (convVar > 0)
			g_SpeechManager.AddPhoneConversationVariationToHistory(m_AmbientVoiceNameHash, convVar, fwTimer::GetTimeInMilliseconds());
	}
}

audSpeechAudioEntity::audSpeechAudioEntity()
{
	m_IsHavingAConversation = false;
	m_LastScriptedConversationTime = 0;	
	m_ScriptedSpeechSound = NULL;
	m_HasPreparedAndPlayed = false;
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		m_ScriptedShadowSound[i] = NULL;
	}
	m_ScriptedSpeechSoundEcho = NULL;
	m_ScriptedSpeechSoundEcho2 = NULL;
	m_HeadSetSubmixEnvSnd = NULL;
	m_ActiveSubmixIndex = -1;
	m_VariationNum = -1;
	m_SpeechSyncId = audMixerSyncManager::InvalidId;
	m_TimeCanBeInConversation = 0;

	for(u32 i=0; i<NUM_AUD_SPEECH_METADATA_CATEGORIES; i++)
	{
		m_SpeechMetadataCategoryHashes[i] = atStringHash(g_SpeechMetadataCategoryNames[i]);
		m_LastQueriedMarkerIdForCategory[i] = -1;
		m_LastQueriedGestureIdForCategory[i] = -1;
		m_GesturesHaveBeenParsed = false;
	}

	m_GestureData = NULL;
	m_NumGestures = 0;
	m_WasPlayingAcitveSoundLastFrame = false;

	m_AmbientSpeechSound = NULL;
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		m_AmbientShadowSound[i] = NULL;
	}
	m_AmbientSpeechSoundEcho = NULL;
	m_AmbientSpeechSoundEcho2 = NULL;
	m_AmbientSlotId = -1;
	m_ScriptedSlot = NULL;

	m_ActiveSpeechIsHeadset = false;
	m_ActiveSpeechIsPadSpeaker = false;

#if __BANK
	safecpy(m_CurrentlyPlayingSpeechContext, "");
#endif
	m_CurrentlyPlayingSoundSet.Reset();
	m_CurrentlyPlayingPredelay1 = 0;
	m_TotalPredelayForLipsync = 0;
	m_CurrentlyPlayingVoiceNameHash = 0;

	m_AmbientVoiceNameHash = 0;
	m_AmbientVoicePriority = 0;
	m_LastPlayedWaveSlot = NULL;
	m_LastPlayedSpeechContextPHash = 0;
	m_LastPlayedSpeechVariationNumber = 0;

	m_HasContextToPlaySafely = false;
	m_ContextToPlaySafely[0] = '\0';
	m_PlaySafelySpeechParams[0] = '\0';
	m_AlternateCryingContext[0] = '\0';
	m_IsCrying = false;

	//m_BackupSpeechContexts.Reset();
	//m_AllowedToFillBackupSpeechArray = true;
	m_TriggeredSpeechContext = NULL;
	m_IsAttemptingTriggeredPrimary = false;
	m_IsAttemptingTriggeredBackup = false;

	m_PossibleConversationTopics = 0;

	m_PedHeadMatrix.Identity();
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		m_SpeechVolSmoother[i].Init(g_SpeechLinVolumeSmoothRate, true);
	}
	m_SpeechLinLPFSmoother.Init(g_SpeechFilterSmoothRate, true);
	m_SpeechLinHPFSmoother.Init(g_SpeechFilterSmoothRate, true);
	m_PositionForNullSpeaker.Set(ORIGIN);
	m_EntityForNullSpeaker = NULL;

	m_PainSpeechSound = NULL;
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		m_PainShadowSound[i] = NULL;
	}
	m_PainEchoSound = NULL;
	m_LastPainLevel = AUD_PAIN_LOW;
	m_LastPainTimes[0] = m_LastPainTimes[1] = 0;
	m_PedHasDied = false;
	m_PedHasGargled = false;
	m_DropToughnessLevel = false;
	m_ShouldUpdatePain = false;
	m_ShouldStopSpeech = false;
	m_ShouldSaySpaceRanger = false;
	m_ShouldPlayCycleBreath = false;
	m_IsUnderwater = false;

	m_AnimalParams = NULL;
	m_AnimalType = kAnimalNone;
	m_AnimalMood = AUD_ANIMAL_MOOD_ANGRY;
	m_WasMakingAnimalSoundLastFrame = false;

	m_AnimalVocalSound = NULL;
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		m_AnimalVocalShadowSound[i] = NULL;
	}
	m_AnimalVocalEchoSound = NULL;
	for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS; ++i)
		m_AnimalOverlappableSound[i] = NULL;

	m_BreathSound = NULL;
	m_BreathState = kBreathingIdle;
	m_LastTimeInVehicle = 0;
	m_LastTimePedaling = 0;
	m_LastTimeNotPedaling = 0;
	m_LastBreathWasInhale = false;
	m_TimeToNextPlayJogLowBreath = 0;
	m_LastTimeRunning = 0;
	m_LastTimeSprinting = 0;
	m_LastTimeNotRunningOrSprinting = 0;
	m_BreathingVolumeLin = 0.0f;
	m_BreathingOnBike = false;

	m_PantState = kPantIdle;
	m_PantStateChangeTime = 0;
	
	m_LastScubaBreathWasInhale = false;
	m_LastTimeScubaBreathPlayed = 0;

	m_PedIsDrunk = false;
	m_PedIsBlindRaging = false;

	m_VisemeAnimationData = NULL;
	m_VisemeType = AUD_VISEME_TYPE_FACEFX;

	m_EchoVolume = g_SilenceVolume;
	m_BlipNoise = 0.0f;
	m_TimeLastDrewSpeechBlip = 0;
	m_DontAddBlip = false;
	m_IsUrgent = false;

	m_CachedSpeechStartOffset = 0;
	m_CachedSpeechType = AUD_SPEECH_TYPE_INVALID;
	m_CachedVolType = AUD_SPEECH_NORMAL;
	m_CachedAudibility = AUD_AUDIBILITY_NORMAL;
	m_CachedRolloff = 1.0f;
	m_IsFromScript = false;

	m_GreatestAudibility = AUD_AUDIBILITY_NORMAL;
	m_LastHighAudibilityLineTime = 0;
	m_AmountToUsePreviousHighAudibility = 0.0f;

	m_SpeechVolumeOffset = 0.0f;

	m_SpeakingDisabled = false;
	m_SpeakingDisabledSynced = false;
	m_AllSpeechBlocked = false;
	m_SpeechBlockAffectsOutgoingNetworkSpeech = false;
	m_PainAudioDisabled = false;

	m_ReplyingPed = NULL;
	m_ReplyingContext = 0;
	m_ReplyingDelay = -1;
	m_ShouldResetReplyingPed = false;

	m_WaitingForAmbientSlot = false;

	m_Preloading = false;
	m_PreloadFreeTime = 0;
	m_PreloadedSpeechIsPlaying = false;

	m_FirstUpdate = true;

	m_TimeToStopBeingAngry = 0;
	m_IsAngry = false;
	m_IsBuddy = false;
	m_IsCodeSetAngry = false;
	m_IsInRapids = false;
#if ENABLE_VISEMES
	m_IgnoreVisemeData = false;	
#else
	m_IgnoreVisemeData = true;
#endif

	m_AmbientVoicePriority= 0xff;

	m_LungDamageStartTime = 0;
	m_LungDamageNextCoughTime = 0;
	m_IsCoughing = false;
	m_TimeOfLastCoughCheck = 0;
	m_TimeToPlayCollisionScream = 0;
	m_WaitingToPlayCollisionScream = false;
	m_TimeToNextCry = 0;

	m_IsMale = false;
	m_isMaleSet = false;

	m_IsFalling = false;
	m_IsHighFall = false;
	m_bIsFallScreaming = false;
	m_HasPlayedInitialHighFallScream = false;
	m_FramesUntilNextFallingCheck = 0;
	m_TimeToStartLandProbe = 0;
	m_LastTimeFallingVelocityCheckWasSet = 0;
	m_WaveLoadedPainType = AUD_PAIN_ID_PAIN_LOW_GENERIC;
	m_DeathIsFromEnteringRagdoll = false;
	m_WaitingToPlayFallDeath = false;
	m_LastTimeFalling = 0;

	m_LastTimeThrownFromVehicle = 0;
	m_DelayJackingReply = false;
	m_TimeToPlayJackedReply = 0;
	m_TimeJackedInforWasSet = 0;
	m_LastTimeOpeningDoor = 0;
	m_JackedPed = NULL;
	m_JackedVehicle = NULL;
	m_JackedFromPassengerSide = false;
	m_VanJackClipUsed = false;

	m_MultipartVersion = 0;
	m_MultipartContextCounter = 0;
	m_MultipartContextPHash = 0;
	m_OriginalMultipartContextPHash = 0;	

	m_PlaySpeechWhenFinishedPreparing = false;
	m_UseShadowSounds = false;

#if __DEV
	formatf(m_LastPlayedWaveName, 64, "");
	formatf(m_LastPlayedScriptedSpeechVoice, 64, "");
#endif

#if __BANK
	m_UsingFakeFullPVG = false;
	m_ContextToPlaySilentPHash = 0;

	m_Toughness = AUD_TOUGHNESS_MALE_NORMAL;
	m_ToughnessIsSet = false;

	m_TimeOfFakeGesture = -1;
	m_SoundLengthForFakeGesture = -1;
	m_FakeGestureNameHash = 0;
	m_FakeMarkerReturned = false;

	m_LastTimePlayingAnySpeech = 0;
	m_TimeScriptedSpeechStarted = 0;

	m_HasPreloadedSurfacingBreath = false;
	m_ShouldPlaySurfacingBreath = false;
	m_UsingStreamingBucking = false;

	m_bIsGettingUpAfterVehicleEjection = false;
	m_SuccessfullyUsedAmbientVoice = false;
	m_ForceSetAmbientVoice = false;
	m_PlayingSharkPain = false;
	m_IsDebugEntity = false;
	m_IsMutedDuringSlowMo = true;
	m_IsPitchedDuringSlowMo = true;
#endif
#if !__FINAL
	m_AlertConversationEntityWhenSoundBegins = false;
	m_IsPlaceholder = false;
#endif

#if GTA_REPLAY
	m_LastTimePlayingReplayAmbientSpeech = 0;
	m_ActiveSoundStartTime = 0;
	m_ContextNameStringHash = 0;	
	m_ReplayPreDelay = -1;
	m_ReplayWaveSlotIndex = -1;
#endif	

	naSpeechEntDebugf3(this, "Init()");
}

void audSpeechAudioEntity::InitAnimalParams()
{
	if(m_Parent && m_Parent->GetPedModelInfo())
	{
		m_AnimalParams = audNorthAudioEngine::GetObject<AnimalParams>(m_Parent->GetPedModelInfo()->GetAnimalAudioObject());
		if(m_AnimalParams)
		{
			m_AnimalType = (AnimalType)(m_AnimalParams->Type);
			g_SpeechManager.IncrementAnimalAppearance(m_AnimalType);
			if(m_Parent)
				g_SpeechManager.SetIfNearestAnimalInstance(m_AnimalType, m_Parent->GetMatrix().d());
			g_SpeechManager.ForceAnimalBankUpdate();

			m_DeferredAnimalVocalAnimTriggers.Reset();
		}
	}
}

void audSpeechAudioEntity::Resurrect()
{
	naSpeechEntDebugf3(this, "Resurrect()");

	StopSpeech();
	FreeSlot();
	ResetFalling();
	m_PedHasDied = false;
	m_PedHasGargled = false;
}

void audSpeechAudioEntity::SetIsHavingAConversation(bool isHavingAConversation) 
{
	m_IsHavingAConversation = isHavingAConversation;
	// If false, we're no longer having a conv, which is only called after we have been, so set this as the last time we had one.
	// This is vaguely sensible, as calling this with true doesn't happen every frame.
	if(!isHavingAConversation)
	{
		m_LastScriptedConversationTime = fwTimer::GetTimeInMilliseconds();
	}
}

bool audSpeechAudioEntity::GetIsHavingANonPausedConversation()
{
	return (m_IsHavingAConversation && !g_ScriptAudioEntity.IsScriptedConversationPaused());
}

bool audSpeechAudioEntity::GetIsOkayTimeToStartConversation()
{
	return m_TimeCanBeInConversation < g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audSpeechAudioEntity::SetAmbientVoiceName(const char* context, bool allowBackupPVG )
{
	u32 voiceNameHash = g_SpeechManager.GetBestVoice(GetPedVoiceGroup(), atPartialStringHash(context), m_Parent, allowBackupPVG);
	SetAmbientVoiceName(voiceNameHash, true);
}

void audSpeechAudioEntity::SetAmbientVoiceName(char* voiceName, bool forceSet)
{
	SetAmbientVoiceName(atStringHash(voiceName), forceSet);
}

void audSpeechAudioEntity::SetAmbientVoiceName(u32 voiceNameHash, bool forceSet)
{
#if __ASSERT 
	if(m_Parent && voiceNameHash != 0)
	{
		if (m_Parent->GetPedType() == PEDTYPE_PLAYER_0)
		{
			naAssertf(voiceNameHash == g_MichaelVoice,"Setting the wrong voice for Micahel, Hash :%u ",voiceNameHash);
		}
		else if (m_Parent->GetPedType() == PEDTYPE_PLAYER_1)
		{
			naAssertf(voiceNameHash == g_FranklinVoice,"Setting the wrong voice for Franklin, Hash :%u ",voiceNameHash);
		}
		else if (m_Parent->GetPedType() == PEDTYPE_PLAYER_2)
		{
			naAssertf(voiceNameHash == g_TrevorVoice,"Setting the wrong voice for Trevor, Hash :%u ",voiceNameHash);
		}
	}
#endif

	if(m_ForceSetAmbientVoice)
		naWarningf("Setting an ambient voice that has already been force set once.  Oldhash: %u Newhash: %u", m_AmbientVoiceNameHash, voiceNameHash);

	m_ForceSetAmbientVoice = forceSet;
	// Do this here and only here, so we ref-count no matter how our voice is set.
	if (m_AmbientVoiceNameHash!=voiceNameHash)
	{
		g_SpeechManager.AddReferenceToVoice(GetPedVoiceGroup(), voiceNameHash BANK_ONLY(, this));
		// and remove any old reference (if we already had a voice name hash)
		if (m_AmbientVoiceNameHash!=0)
		{
			g_SpeechManager.RemoveReferenceToVoice(GetPedVoiceGroup(), m_AmbientVoiceNameHash);
		}
	}
	m_AmbientVoiceNameHash = voiceNameHash;
	if(!audSpeechSound::DoesVoiceExist(m_AmbientVoiceNameHash))
	{
		m_AmbientVoiceNameHash = g_NoVoiceHash;
		m_AmbientVoicePriority = 0;
		m_Toughness = m_IsMale ? (u8)AUD_TOUGHNESS_MALE_NORMAL : (u8)AUD_TOUGHNESS_FEMALE;
	}
	else
	{
		u8 toughness = audSpeechSound::FindPainType(m_AmbientVoiceNameHash);
		if(toughness == g_MaleCowardlyPainHash)
			m_Toughness = AUD_TOUGHNESS_MALE_COWARDLY;
		if(toughness == g_MaleBumPainHash)
			m_Toughness = AUD_TOUGHNESS_MALE_BUM;
		else if(toughness == g_MaleBravePainHash)
			m_Toughness = AUD_TOUGHNESS_MALE_BRAVE;
		else if(toughness == g_MaleGangPainHash)
			m_Toughness = AUD_TOUGHNESS_MALE_GANG;
		else if(toughness == g_FemalePainHash)
			m_Toughness = AUD_TOUGHNESS_FEMALE;
		else if(toughness == g_FemaleBumPainHash)
			m_Toughness = AUD_TOUGHNESS_FEMALE_BUM;
		else if(toughness == g_CopPainHash)
		{
			m_Toughness = AUD_TOUGHNESS_COP;
#if __BANK
			if(g_ForceToughCopVoice)
			{
				m_AmbientVoiceNameHash = audEngineUtil::ResolveProbability(0.5f) ? ATSTRINGHASH("S_AGGROCOP_01", 0xAFDD9368) : 
																					ATSTRINGHASH("S_AGGROCOP_02", 0xE1C37733);
			}
#endif
		}
		else
			m_Toughness = m_IsMale ? (u8)AUD_TOUGHNESS_MALE_NORMAL : (u8)AUD_TOUGHNESS_FEMALE;
	}
	m_ToughnessIsSet = true;
	m_PossibleConversationTopics = g_SpeechManager.GetPossibleConversationTopicsForPvg(m_AmbientVoiceNameHash);
}

//void audSpeechAudioEntity::ChooseFullVoiceIfUndecided()
//{
//	if (m_AmbientVoiceNameHash==0)
//	{
//		naCErrorf(!m_Parent->IsLocalPlayer(), "Wasn't expecting ChooseFullVoiceUndecided() to be called on a local player");
//		s32 minReferences = 0;//unused
//		u32 voiceHash = g_SpeechManager.GetBestPrimaryVoice(GetPedVoiceGroup(), minReferences);
//		if (voiceHash!=0)
//		{
//			SetAmbientVoiceName(voiceHash);
//		}
//		else
//		{
//#if __DEV
//			fwModelId modelId;
//			u32 modelInfoHash = 0;
//			u32 pedVoiceGroup = 0;
//			if (m_Parent)
//			{
//				modelId = m_Parent->GetModelId();
//				CPedModelInfo* pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);
//				if (pModelInfo)
//				{
//					modelInfoHash = pModelInfo->GetHashKey();
//					pedVoiceGroup = pModelInfo->GetPedVoiceGroup();
//				}
//			}
//			naWarningf("No voice for modelId: %u; modelHash: %u; pvg: %u", modelId.GetModelIndex(), modelInfoHash, pedVoiceGroup);
//			naErrorf("No voice found for ped");
//#endif
//			/*
//			// Hack for now - if we don't find one, use the ped name directly if it exists
//			if (m_Parent->GetPedType() == PEDTYPE_COP)
//			{
//				m_AmbientVoiceNameHash = GetRandomCopVoiceNameHash();
//			}
//			else if (m_Parent->GetPedType() == PEDTYPE_MEDIC)
//			{
//				m_AmbientVoiceNameHash = ATSTRINGHASH("M_Y_PMEDIC_BLACK", 0xBAC7A765);
//			}
//			else if (m_Parent->GetPedType() == PEDTYPE_FIRE)
//			{
//				m_AmbientVoiceNameHash = ATSTRINGHASH("M_Y_FIREMAN_BLACK", 0x4C3F3ED1);
//			}
//			else
//			{
//				fwModelId modelId = m_Parent->GetModelId();
//				m_AmbientVoiceNameHash = g_NoVoiceHash;
//				if (modelId>=0)
//				{
//					CPedModelInfo* pModelInfo = (CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId);
//					if (pModelInfo)
//					{
//						m_AmbientVoiceNameHash = pModelInfo->GetHashKey();
//					}
//				}
//			}
//			*/
//			if (!audSpeechSound::DoesVoiceExist(m_AmbientVoiceNameHash))
//			{
//#if __DEV
//				u32 modelInfoHash = 0;
//				u32 pedVoiceGroup = 0;
//				CPedModelInfo* pModelInfo = NULL;
//				if (m_Parent)
//				{
//					pModelInfo = m_Parent->GetPedModelInfo();
//					if (pModelInfo)
//					{
//						modelInfoHash = pModelInfo->GetHashKey();
//						pedVoiceGroup = pModelInfo->GetPedVoiceGroup();
//					}
//				}
//				naWarningf("No voice for modelId: %s; modelHash: %u; pvg: %u", pModelInfo->GetModelName(), modelInfoHash, pedVoiceGroup);
//				//Assertf(0, "No voice found for ped");
//#endif
//				m_AmbientVoiceNameHash = g_NoVoiceHash;
//				m_AmbientVoicePriority = 0;
//			}
//		}
//	}
//}

void audSpeechAudioEntity::ChooseVoiceIfUndecided(u32 contextPHash)
{
	if (!m_Parent)
		return;

	if (m_AmbientVoiceNameHash == 0 || m_AmbientVoiceNameHash == g_NoVoiceHash)
	{
		if (m_Parent->GetPedType() == PEDTYPE_PLAYER_0)
		{
			m_AmbientVoiceNameHash = g_MichaelVoice;
			SetLocalPlayerVoice(m_AmbientVoiceNameHash);
			m_AmbientVoicePriority = 10;
			return;
		}
		else if (m_Parent->GetPedType() == PEDTYPE_PLAYER_1)
		{
			m_AmbientVoiceNameHash = g_FranklinVoice;
			SetLocalPlayerVoice(m_AmbientVoiceNameHash);
			m_AmbientVoicePriority = 10;
			return;
		}
		else if (m_Parent->GetPedType() == PEDTYPE_PLAYER_2)
		{
			m_AmbientVoiceNameHash = g_TrevorVoice;
			SetLocalPlayerVoice(m_AmbientVoiceNameHash);
			m_AmbientVoicePriority = 10;
			return;
		}

		u32 voiceHash = g_SpeechManager.GetBestVoice(GetPedVoiceGroup(), contextPHash, m_Parent);
		//u32 voiceHash = g_SpeechManager.GetBestVoice(ATSTRINGHASH("COP_PVG", 0xA2468C11), context);
		if (voiceHash != 0)
		{
			SetAmbientVoiceName(voiceHash);
		}
		else
		{
#if __DEV
			u32 modelInfoHash = 0;
			u32 pedVoiceGroup = 0;
			CPedModelInfo* pModelInfo = NULL;
			if (m_Parent)
			{
				pModelInfo = m_Parent->GetPedModelInfo();
				if (pModelInfo)
				{
					modelInfoHash = pModelInfo->GetHashKey();
					pedVoiceGroup = pModelInfo->GetPedVoiceGroup();
				}
			}
			//naWarningf("No voice for modelId: %s; modelHash: %u; pvg: %u", pModelInfo->GetModelName(), modelInfoHash, pedVoiceGroup);
			//Assertf(0, "No voice found for ped");
#endif
			//Getting rid of this.  If someone tries to say a line with no speechContext GO before their voice is assigned, we end up here
			//	and they end up silent for the rest of eternity.  RAK 21/11/12
			/*if (!audSpeechSound::DoesVoiceExist(m_AmbientVoiceNameHash))
			{
				m_AmbientVoiceNameHash = g_NoVoiceHash;
				m_AmbientVoicePriority = 0;
			}*/
		}
	}
}

bool audSpeechAudioEntity::IsMPPlayerPedAllowedToSpeak(const CPed* pPed, u32 voiceHash)
{
	// Exception required for GTA:Online missions where you take control of Franklin/Lamar, as we want them to behave as in SP
	if (pPed && (pPed->GetModelNameHash() == ATSTRINGHASH("p_franklin_02", 0xAF10BD56) || pPed->GetModelNameHash() == ATSTRINGHASH("ig_lamardavis_02", 0x1924A05E)))
	{
		// url:bugstar:7410801 - Preventing ped form commentng if they're involved in a conversation
		if (!g_ScriptAudioEntity.IsScriptedConversationOngoing() || !g_ScriptAudioEntity.IsPedInCurrentConversation(pPed))
		{
			return true;
		}		
	}

	if (voiceHash == ATSTRINGHASH("Mask_SFX", 0x36179520))
	{
		return true;
	}

	return false;
}

void audSpeechAudioEntity::SetPedGender(bool isMale)
{
	m_IsMale = isMale; 
	m_isMaleSet = true;
}

void audSpeechAudioEntity::ForceFullVoice(s32 PVGHash,s32 pedRace)
{
	PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(PVGHash != 0 ? PVGHash : GetPedVoiceGroup(true,pedRace));
	s32 minReferences = 0;//unused
	u32 voiceHash = g_SpeechManager.GetBestPrimaryVoice(pedVoiceGroupObject, minReferences);
	if (voiceHash!=0)
	{
		SetAmbientVoiceName(voiceHash,true);
	}
	else
	{
		voiceHash = g_SpeechManager.GetBestMiniVoice(pedVoiceGroupObject, minReferences,0);
		SetAmbientVoiceName(voiceHash,true);
	}
}

u16 audSpeechAudioEntity::GetPossibleConversationTopics()
{
	// If we don't have a voice, go get conv topics for the full voices in our ped voice group. If we do, return the cached one.
	if (m_AmbientVoiceNameHash!=0)
	{
		return m_PossibleConversationTopics;
	}

	return g_SpeechManager.GetPossibleConversationTopicsForPvg(GetPedVoiceGroup());
}

u32 audSpeechAudioEntity::GetPVGHashFromRace(const PedRaceToPVG* raceToPVG,s32 pedRace) const
{
	u32 ret = g_NullSoundHash;
	if(!m_Parent || !raceToPVG)
	{
		return g_NullSoundHash;
	}

	ePVRaceType race = m_Parent->GetPedRace();
	// check if script is overriding the race.
	if(pedRace != -1 && naVerifyf(pedRace < PV_RACE_MAX_TYPES, "Wrong ped race: %u", pedRace))
	{
		race = (ePVRaceType)pedRace;
	}
	switch(race)
	{
	case PV_RACE_UNIVERSAL:
		ret = raceToPVG->Universal;
		break;
	case PV_RACE_WHITE:
		ret = raceToPVG->White;
		break;
	case PV_RACE_BLACK:
		ret = raceToPVG->Black;
		break;
	case PV_RACE_CHINESE:
		ret = raceToPVG->Chinese;
		break;
	case PV_RACE_LATINO:
		ret = raceToPVG->Latino;
		break;
	case PV_RACE_ARAB:
		ret = raceToPVG->Arab;
		break;
	case PV_RACE_BALKAN:
		ret = raceToPVG->Balkan;
		break;
	case PV_RACE_JAMAICAN:
		ret = raceToPVG->Jamaican;
		break;
	case PV_RACE_KOREAN:
		ret = raceToPVG->Korean;
		break;
	case PV_RACE_ITALIAN:
		ret = raceToPVG->Italian;
		break;
	case PV_RACE_PAKISTANI:
		ret = raceToPVG->Pakistani;
		break;
	default:
		break;
	}

#if __BANK
	//TODO: add an assert...we don't want this happening.  But until everything is set up, this could definitley happen, so lets not assert yet.
	if(ret == 0 || ret == g_NullSoundHash)
	{
		if(raceToPVG->Universal != g_NullSoundHash && raceToPVG->Universal != 0)
			return raceToPVG->Universal;
		if(raceToPVG->White != g_NullSoundHash && raceToPVG->White != 0)
			return raceToPVG->White;
		if(raceToPVG->Black != g_NullSoundHash && raceToPVG->Black != 0)
			return raceToPVG->Black;
		if(raceToPVG->Chinese != g_NullSoundHash && raceToPVG->Chinese != 0)
			return raceToPVG->Chinese;
		if(raceToPVG->Latino != g_NullSoundHash && raceToPVG->Latino != 0)
			return raceToPVG->Latino;
		if(raceToPVG->Arab != g_NullSoundHash && raceToPVG->Arab != 0)
			return raceToPVG->Arab;
		if(raceToPVG->Balkan != g_NullSoundHash && raceToPVG->Balkan != 0)
			return raceToPVG->Balkan;
		if(raceToPVG->Jamaican != g_NullSoundHash && raceToPVG->Jamaican != 0)
			return raceToPVG->Jamaican;
		if(raceToPVG->Korean != g_NullSoundHash && raceToPVG->Korean != 0)
			return raceToPVG->Korean;
		if(raceToPVG->Italian != g_NullSoundHash && raceToPVG->Italian != 0)
			return raceToPVG->Italian;
		if(raceToPVG->Pakistani != g_NullSoundHash && raceToPVG->Pakistani != 0)
			return raceToPVG->Pakistani;
	}
#endif

	return ret;
}

u32 audSpeechAudioEntity::GetPedVoiceGroup(bool bCreateIfUnassigned,s32 pedRace) const
{
	if(m_AnimalParams)
		return g_SilentPVGHash;

	if(m_Parent)
	{
		u32 PVGHash = m_Parent->GetPedVoiceGroup(bCreateIfUnassigned);

		if(PVGHash == 0)
			return 0;

		audMetadataObjectInfo objectInfo;
		if(audNorthAudioEngine::GetMetadataManager().GetObjectInfo(PVGHash, objectInfo))
		{
			switch(objectInfo.GetType())
			{
			case PedRaceToPVG::TYPE_ID:
				PVGHash = GetPVGHashFromRace(objectInfo.GetObject<PedRaceToPVG>(),pedRace);
				break;
			case PedVoiceGroups::TYPE_ID:
				break;
			default:
				NOTFINAL_ONLY(audWarningf("Bad PVGHash %u passed to GetPedVoiceGroup; incorrect object type %s (%u).  Setting to Silent.", PVGHash, objectInfo.GetName_Debug(), objectInfo.GetType());)
				return g_SilentPVGHash;
			}
		}	
		else
		{
			audWarningf("Bad PVGHash %u passed to GetPedVoiceGroup (object not found).  Setting to Silent.", PVGHash);
			return g_SilentPVGHash;
		}
		return PVGHash;
	}
	return 0;
}

bool audSpeechAudioEntity::DoesContextExistForThisPed(const char* context, bool allowBackupPVG) const
{
	return DoesContextExistForThisPed(atPartialStringHash(context), allowBackupPVG);
}

bool audSpeechAudioEntity::DoesContextExistForThisPed(u32 contextPHash, bool allowBackupPVG) const
{
	u32 voiceName = m_AmbientVoiceNameHash;
	if(m_Parent)
	{
		u32 playerVoiceName = 0;
		if (m_Parent->GetPedType() == PEDTYPE_PLAYER_0)
			playerVoiceName = g_MichaelVoice;
		else if (m_Parent->GetPedType() == PEDTYPE_PLAYER_1)
			playerVoiceName = g_FranklinVoice;
		else if (m_Parent->GetPedType() == PEDTYPE_PLAYER_2)
			playerVoiceName = g_TrevorVoice;

		if(playerVoiceName != 0)	
			voiceName = !m_PedIsDrunk && (m_IsAngry || ShouldBeAngry()) ? atStringHash("_ANGRY", playerVoiceName) : atStringHash("_NORMAL", playerVoiceName);
	}

	if (voiceName==0)
	{
		voiceName = g_SpeechManager.GetBestVoice(GetPedVoiceGroup(), contextPHash, m_Parent, allowBackupPVG);
	}

	if(audSpeechAudioEntity::IsMultipartContext(contextPHash))
	{
		contextPHash = atPartialStringHash("_01", contextPHash);
	}

	s32 numVariations = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(voiceName, contextPHash);

	if (numVariations>0)
	{
		return true;
	}
	return false;
}

bool audSpeechAudioEntity::SetPedVoiceForContext(const char* context, bool allowBackupPVG)
{
	return SetPedVoiceForContext(atPartialStringHash(context), allowBackupPVG);
}

bool audSpeechAudioEntity::SetPedVoiceForContext(u32 contextPHash, bool allowBackupPVG)
{
	u32 voiceName = m_AmbientVoiceNameHash;
	if (voiceName==0)
	{
		voiceName = g_SpeechManager.GetBestVoice(GetPedVoiceGroup(), contextPHash, m_Parent, allowBackupPVG);
	}
	
	if(audSpeechAudioEntity::IsMultipartContext(contextPHash))
	{
		contextPHash = atPartialStringHash("_01", contextPHash);
	}

	s32 numVariations = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(voiceName, contextPHash);

	if (numVariations>0)
	{
		if(voiceName != 0)
			SetAmbientVoiceName(voiceName, true);
		return true;
	}
	return false;
}

bool audSpeechAudioEntity::DoesPhoneConversationExistForThisPed() const
{
	if (m_AmbientVoiceNameHash==0)
	{
		u32 voiceName = g_SpeechManager.GetVoiceForPhoneConversation(GetPedVoiceGroup());
		if(voiceName != 0)
		{
			return true;
		}
		else
			return false;
	}
	else if(audSpeechSound::FindNumVariationsForVoiceAndContext(m_AmbientVoiceNameHash,"PHONE_CONV1_INTRO") != 0)
	{
		return true;
	}

	return false;
}

u8 audSpeechAudioEntity::SetAmbientVoiceNameAndGetVariationForPhoneConversation(bool automaticallyAddToHistory)
{
	u8 variation = 0;
	if (m_AmbientVoiceNameHash==0)
	{
		u32 voiceName = g_SpeechManager.GetVoiceForPhoneConversation(GetPedVoiceGroup());
		if(voiceName != 0)
		{
			SetAmbientVoiceName(voiceName);
		}
		else
		{
			return 0;
		}
	}
	else if(audSpeechSound::FindNumVariationsForVoiceAndContext(m_AmbientVoiceNameHash,"PHONE_CONV1_INTRO") == 0)
	{
		return 0;
	}

	variation = g_SpeechManager.GetPhoneConversationVariationFromVoiceHash(m_AmbientVoiceNameHash);
	if(variation<=0)
	{
		return 0;
	}

	if(automaticallyAddToHistory)
	{
		g_SpeechManager.AddPhoneConversationVariationToHistory(m_AmbientVoiceNameHash, variation, fwTimer::GetTimeInMilliseconds());
	}
	return variation;
}

bool audSpeechAudioEntity::CanPedHaveChatConversationWithPed(CPed* ped, u32 statePHash, u32 respPHash, bool allowBackupPVG)
{
	if(ped == m_Parent)
		return false;

	if(Verifyf(ped, "NULL ped passed into CanPedHaveChatConversationWithPed()") &&
	   Verifyf(m_Parent, "NULL parent in CanPedHaveChatConversationWithPed()") &&
	   Verifyf(ped->GetSpeechAudioEntity(), "Ped with NULL speechEntity passed into CanPedHaveChatConversationWithPed()")
		)
	{
		u32 thisPVGHash = m_Parent->GetPedVoiceGroup(true);
		u32 otherPVGHash = ped->GetPedVoiceGroup(true);

		if(thisPVGHash == otherPVGHash)
		{
			return SetPedVoiceForContext(statePHash, allowBackupPVG) && 
				ped->GetSpeechAudioEntity()->SetPedVoiceForContext(respPHash, allowBackupPVG);
		}

		PedRaceToPVG* thisPRtoPVG = audNorthAudioEngine::GetObject<PedRaceToPVG>(thisPVGHash);
		PedRaceToPVG* otherPRtoPVG = audNorthAudioEngine::GetObject<PedRaceToPVG>(otherPVGHash);

		if(!thisPRtoPVG || !otherPRtoPVG)
		{
			Assertf(0, "One of the peds in a call to CanPedHaveChatConversationWithPed doesn't have their PVG set to a PedRaveToPVG object.  Returning false.");
			return false;			
		}

		for(int i=0; i<thisPRtoPVG->numFriendPVGs; ++i)
		{
			for(int j=0; j<otherPRtoPVG->numFriendPVGs; j++)
			{
				if(thisPRtoPVG->FriendPVGs[i].PVG == otherPRtoPVG->FriendPVGs[j].PVG)
					return true;
			}
		}
	}
	return false;
}

void audSpeechAudioEntity::PlayScriptedSpeech(const char *voiceName, const char *contextName, s32 variationNum, audWaveSlot* waveSlot,
											  audSpeechVolume volType, bool allowLoad, u32 startOffset, const Vector3& posForNullSpeaker, CEntity* entForNullSpeaker,
											  u32 setVoiceNameHash, audConversationAudibility audibility, bool headset, crClip* visemeData, bool isPadSpeakerRoute, s32 predelay)
{
	naSpeechEntDebugf1(this, "PlayScriptedSpeech() voiceName: %s contextName: %s variationNum: %d volType: %u audibility: %u isHeadSet: %s",
		voiceName, contextName, variationNum, volType, audibility, headset ? "True" : "False");

	m_PositionForNullSpeaker.Set(ORIGIN);
	m_EntityForNullSpeaker = NULL;
	m_DontAddBlip = true;
	m_ActiveSpeechIsHeadset = headset;
	m_ActiveSpeechIsPadSpeaker = isPadSpeakerRoute;
	m_PlaySpeechWhenFinishedPreparing = false;
	m_CurrentlyPlayingPredelay1 = 0;

	Displayf("PlayScriptedSpeech...voice(%s) Context(%s) headset(%d) isPad(%d)", voiceName, contextName, headset, isPadSpeakerRoute);

	if(IsAmbientSpeechPlaying())
	{
		KillPreloadedSpeech();
	}

	// We may not have a parent! *******************
	// This is because we may be a speech entity owned by something other than a ped - eg the mobile
	//  phone script entity. If we don't belong to a ped, we play front-end. This might be best being
	//  subclassed rather than all in together, but most of this needs to change at some point anyway.

	bool IgnoreVisemeData = false;

#if __BANK
	if(m_IsDebugEntity)
	{
		if(g_ShouldOverrideSpeechVolumeType && g_SpeechVolumeTypeOverride < AUD_NUM_SPEECH_VOLUMES)
			volType = (audSpeechVolume)g_SpeechVolumeTypeOverride;
		if(g_ShouldOverrideSpeechAudibility && g_SpeechAudibilityOverride < AUD_NUM_AUDIBILITIES)
			audibility = (audConversationAudibility)g_SpeechAudibilityOverride;
	}
#endif

	if(IsScriptedSpeechPlaying())
	{
#if __BANK
		if(NetworkInterface::IsGameInProgress())
		{
			Displayf("Can't Speak - Scripted speech already playing %s", contextName);
		}
#endif
		//We are already playing speech for this entity.
		naSpeechEntDebugf2(this, "PlayScriptedSpeech() FAILED - We are already playing speech for this entity");
		return;
	}

	// Need to clear this out each time we play something
	for(u32 i=0; i<NUM_AUD_SPEECH_METADATA_CATEGORIES; i++)
	{
		m_SpeechMetadataCategoryHashes[i] = atStringHash(g_SpeechMetadataCategoryNames[i]);
		m_LastQueriedMarkerIdForCategory[i] = -1;
		m_LastQueriedGestureIdForCategory[i] = -1;
	}
	
	m_GesturesHaveBeenParsed = false;
	m_GestureData = NULL;
	m_NumGestures = 0;

#if __BANK
	m_TimeOfFakeGesture = -1;
	m_SoundLengthForFakeGesture = -1;
	m_FakeMarkerReturned = false;
#endif

	u32 voiceNameHash = setVoiceNameHash ? setVoiceNameHash : atStringHash(voiceName);

#if !__FINAL
	m_IsPlaceholder = false;
	char placeholderVoiceName[128] = "";
	if (g_EnablePlaceholderDialogue && voiceName)
	{
		s32 numVars = GetRandomVariation(voiceNameHash, contextName, NULL, NULL);
		if (numVars < 0)
		{
			formatf(placeholderVoiceName, sizeof(placeholderVoiceName), "%s_PLACEHOLDER", voiceName);
			voiceName = placeholderVoiceName;
			voiceNameHash = atStringHash(voiceName);
			m_IsPlaceholder = true;	// We'll apply a volume boost if so when getting SpeechAudibilityMetrics
			// play all placeholder speech as frontend
			volType = AUD_SPEECH_FRONTEND;
		}
	}
#endif

	if(variationNum <= 0)
	{
		//Pick a random variation. Do this via our proper managed non-repeating code, then register the one we've used.
		variationNum = GetRandomVariation(voiceNameHash, contextName, NULL, NULL);//, const_cast<char*>(voiceName));
		//if (!naVerifyf(variationNum>0, "Missing scripted speech asset. Check context and voice name. (context: %s; voice: %s; variation: %d)\n", contextName, voiceName, variationNum)) // ==0 is also wrong - variation numbers go from 1 up
		if (variationNum<=0) // ==0 is also wrong - variation numbers go from 1 up
		{
			naSpeechEntDebugf2(this, "PlayScriptedSpeech() FAILED - variationNum<=0");
#if __BANK
			if(NetworkInterface::IsGameInProgress())
			{
				Displayf("Can't Speak - no variation %s", contextName);
			}
#endif
			return;
		}
	}

	const audMetadataRef soundRef = g_ScriptedSoundSet.Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));
	naAssertf(soundRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");

	// Set up our speech sound and init params
	audSoundInitParams initParams;
	initParams.StartOffset = startOffset;
	initParams.RemoveHierarchy = false;
	initParams.UpdateEntity = true;
	initParams.TimerId = 4;
	initParams.Predelay = predelay;
	// In order to make sure we don't drop scripted speech which could end up breaking mission scripts, use the streaming bucket.
	// Each bucket has 192 slots, and the unlikely worst case scenario for the streaming bucket 
	// is something like 120 sounds so there should always be plenty room for scripted speech and ambient speech triggered from script
	BANK_ONLY(m_UsingStreamingBucking = false;)
	if(!PARAM_audNoUseStreamingBucketForSpeech.Get())
	{
		BANK_ONLY(m_UsingStreamingBucking = true;)
		Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
	}

	m_HasPreparedAndPlayed = false;
	CreateSound_PersistentReference(soundRef, (audSound**)&m_ScriptedSpeechSound, &initParams);
	if(!m_ScriptedSpeechSound)
	{
		naAssertf(0, "Unable to create a sound reference in PlayScriptedSound");
		naSpeechEntDebugf2(this, "PlayScriptedSpeech() FAILED - !m_ScriptedSpeechSound");
		return;
	}

	if(m_ScriptedSpeechSound->GetSoundTypeID() != SpeechSound::TYPE_ID)
	{
		naAssertf(0, "Created a sound in PlayScriptedSpeech but it wasn't a speech sound");
		naSpeechEntDebugf2(this, "PlayScriptedSpeech() FAILED - m_ScriptedSpeechSound->GetSoundTypeID() != SpeechSound::TYPE_ID");
		m_ScriptedSpeechSound->StopAndForget();
		return;
	}
	
	bool bInitSpeech = false;
	if (setVoiceNameHash)
		bInitSpeech = m_ScriptedSpeechSound->InitSpeech(setVoiceNameHash, (char *)contextName, variationNum);
	else
		bInitSpeech = m_ScriptedSpeechSound->InitSpeech((char *)voiceName, (char *)contextName, variationNum);

	if(!bInitSpeech)
	{
		naAssertf(0, "Unable to initialise speech sound in PlayScriptedSpeech");
		naSpeechEntDebugf2(this, "PlayScriptedSpeech() FAILED - Unable to initialise speech sound in PlayScriptedSpeech");
		m_ScriptedSpeechSound->StopAndForget();
		return;
	}

	if(!posForNullSpeaker.IsEqual(ORIGIN))
	{
		m_PositionForNullSpeaker.Set(posForNullSpeaker);
		m_EntityForNullSpeaker = NULL;
	}
	else if(entForNullSpeaker)
	{
		m_EntityForNullSpeaker = entForNullSpeaker;
		m_PositionForNullSpeaker.Set(ORIGIN);
	}
	else if(!m_Parent)
	{
		volType = AUD_SPEECH_FRONTEND;
	}

	if(g_ScriptAudioEntity.IsScriptedConversationAMobileCall() 
		|| (m_Parent && m_Parent->IsLocalPlayer() && m_Parent->GetIsParachuting())
		|| isPadSpeakerRoute)
	{
		volType = AUD_SPEECH_FRONTEND;
	}

	const audSpeechType speechType = AUD_SPEECH_TYPE_SCRIPTED;

	// if we're in a scripted cutscene, play it as high audibility
	if(gVpMan.AreWidescreenBordersActive())
	{
		audibility = AUD_AUDIBILITY_CRITICAL;
	}

	m_VisemeAnimationData = NULL;

	// Cache the volType and audibility so that in the ::Update() we know what kind of speech it is when calculating GetSpeechAudibilityMetrics
	m_CachedSpeechStartOffset = startOffset;
	m_CachedSpeechType = speechType;
	m_CachedVolType = volType;
	m_CachedAudibility = audibility;
	m_SpeechVolumeOffset = 0.0f;
	m_IsFromScript = true;
	m_CurrentlyPlayingSoundSet = g_ScriptedSoundSet;
	m_UseShadowSounds = true;

#if GTA_REPLAY
	// Mute/pitch flags are stored in the replay packet during replay playback
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		m_IsMutedDuringSlowMo = !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowScriptedSpeechInSlowMo);
		m_IsPitchedDuringSlowMo = !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowScriptedSpeechInSlowMo);
	}

	// if we don't have a waveslot passed-in, use the global one
	audWaveSlot* waveSlotToUse = waveSlot;
	if (!waveSlotToUse)
	{
		waveSlotToUse = audWaveSlot::FindWaveSlot((char *)g_SpeechWaveSlotName);
	}

	m_ScriptedSlot = waveSlotToUse;

	bool tellAnimationToGetLipsync = true;
	if (allowLoad || IgnoreVisemeData || m_IgnoreVisemeData || !m_Parent || m_Parent->m_nDEflags.bFrozen ||
		m_Parent->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEventScanning))
	{
		m_ScriptedSpeechSound->Prepare(waveSlotToUse, true, naWaveLoadPriority::Speech);
		tellAnimationToGetLipsync = false;
		m_PlaySpeechWhenFinishedPreparing = true;
	}
	else
	{
		audPrepareState prepared = m_ScriptedSpeechSound->Prepare(waveSlotToUse, true, naWaveLoadPriority::Speech);
		if(prepared!=AUD_PREPARED)
		{
			naErrorf("Failed to prepare active scripted speech");
		}
	}

	BANK_ONLY(safecpy(m_CurrentlyPlayingSpeechContext, contextName);)
	m_LastPlayedSpeechContextPHash = atPartialStringHash(contextName);
	m_CurrentlyPlayingVoiceNameHash = voiceNameHash;

	// We've played this line, so register it as having been used, so we don't repeat it too soon.
	naCErrorf(variationNum>=0, "Variation num for scripted speech is invalid");
#if __BANK
	// Since voices are being reused, use a ped identifier rather than voice name for history
	const u32 pedIdentifier = m_Parent ? static_cast<u32>(CPed::GetPool()->GetIndex(m_Parent)) : 0U;
	g_SpeechManager.AddVariationToHistory( (m_UsingFakeFullPVG || voiceNameHash==g_SilentVoiceHash) ? pedIdentifier : voiceNameHash, atPartialStringHash(contextName), (u32)variationNum, fwTimer::GetTimeInMilliseconds());

#else
	g_SpeechManager.AddVariationToHistory(voiceNameHash, atPartialStringHash(contextName), (u32)variationNum, fwTimer::GetTimeInMilliseconds());
#endif

	char variationBuf[4];
	formatf(variationBuf, "_%02u", variationNum);

	m_PreloadedSpeechIsPlaying = false;

	m_VariationNum = variationNum;

	m_VisemeAnimationData = visemeData;
	if(tellAnimationToGetLipsync && m_Parent)
		m_Parent->SetIsWaitingForSpeechToPreload(true);

	m_IsUrgent = g_ScriptAudioEntity.ShouldTreatNextLineAsUrgent();

#if __DEV
	formatf(m_LastPlayedWaveName, 64, "%s_%02d", contextName, variationNum);
	formatf(m_LastPlayedScriptedSpeechVoice, 64, "%s", voiceName);
#endif

#if GTA_REPLAY
	bool shouldRecord = CReplayMgr::ShouldRecord();
		
	// HL: GTAV hacky fix for B*2066416. We don't want to record triathlon news speech into the replay since the 'news feed'
	// camera angle isn't selectable in the replay and the disembodied voice makes no sense. Ideally for future
	// projects we could flag up conversations in the metadata as being replay-recordable or not.
	// added fix for B*2861491
	if((voiceNameHash == ATSTRINGHASH("TRINEWS", 0xA0CF7479)) || (voiceNameHash == ATSTRINGHASH("EXEC1_POWERANN", 0x4C49ABC3)))
	{
		shouldRecord = false;
	}
	// End hacky fix

	m_ActiveSoundStartTime = fwTimer::GetReplayTimeInMilliseconds();
	m_ContextNameStringHash = atPartialStringHash(contextName);
	if(shouldRecord)
	{
		CReplayMgr::RecordFx<CPacketScriptedSpeech>(
			CPacketScriptedSpeech(m_LastPlayedSpeechContextPHash, m_ContextNameStringHash, m_CurrentlyPlayingVoiceNameHash, (s32)predelay, m_CachedVolType,m_CachedAudibility, m_VariationNum, waveSlotToUse->GetSlotIndex(), m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, m_PlaySpeechWhenFinishedPreparing), m_Parent);
	}
#endif // GTA_REPLAY

}

#if GTA_REPLAY

void audSpeechAudioEntity::ReplaySay(u32 contextHash, u32 voiceNameHash, s32 preDelay, u32 requestedVolume, u32 variationNum, f32 priority, u32 offsetMs, SpeechAudibilityType requestedAudibility, bool preloadOnly, CPed* replyingPed, bool checkSlotVacant, Vector3 positionForNullSpeaker, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo)
{
	bool wasPreloading = m_DeferredSayStruct.preloadOnly;

	m_DeferredSayStruct.preDelay = preDelay + (contextHash == g_SharkScreamContextPHash ? 1000 : 0);
	m_DeferredSayStruct.startOffset = offsetMs;
	m_DeferredSayStruct.requestedVolume = requestedVolume;
	m_DeferredSayStruct.voiceNameHash = voiceNameHash;
	m_DeferredSayStruct.contextPHash = contextHash;
	m_DeferredSayStruct.variationNumber = variationNum;

	bool returningDefault = false;
	SpeechContext* speechContext = g_SpeechManager.GetSpeechContext(atFinalizeHash(atPartialStringHash("_SC", contextHash)), m_Parent, replyingPed, &returningDefault);

	if(returningDefault)
	{
		audWarningf("Could not find speech context for recorded speech %u, using default", contextHash);
	}	

	m_DeferredSayStruct.speechContext = speechContext;
	m_DeferredSayStruct.preloadOnly = preloadOnly;
	m_DeferredSayStruct.priority = priority;
	m_DeferredSayStruct.requestedAudibility = requestedAudibility;
	m_PositionForNullSpeaker = positionForNullSpeaker;

	bool hadToStopSpeech = false;
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread() ? CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() : THREAD_INVALID;
	if(m_AmbientSlotId < 0)
	{
		m_AmbientSlotId = g_SpeechManager.GetAmbientSpeechSlot(this, &hadToStopSpeech, priority, scriptThreadId);
		if(m_AmbientSlotId < 0)
		{
			hadToStopSpeech = true;
		}
	}
	else
	{
		if(!g_SpeechManager.IsSlotVacant(m_AmbientSlotId) && checkSlotVacant && !wasPreloading)
		{
			hadToStopSpeech = true;
		}
	}

	if(hadToStopSpeech)
	{
 		m_WaitingForAmbientSlot = true;
 		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_DEFERRED;
		return;
	}

	m_IgnoreVisemeData = true;
	m_WaitingForAmbientSlot = false;

	CompleteSay();

	m_IsMutedDuringSlowMo = isMutedDuringSlowMo;
	m_IsPitchedDuringSlowMo = isPitchedDuringSlowMo;

	m_LastTimePlayingReplayAmbientSpeech = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
#if GTA_REPLAY
	// Ensure ambient replay speech gets positioned correctly
	if(CReplayMgr::IsEditModeActive() && !m_Parent)
	{
		m_UseShadowSounds = false;
	}
#endif
}

void audSpeechAudioEntity::ReplayPlayScriptedSpeech(u32 contextHash, u32 contextNameHash, u32 voiceNameHash, s32 preDelay, audSpeechVolume requestedVolume, audConversationAudibility audibility, s32 uVariationNumber, u32 uMsOffset, s32 waveSlot, bool activeSpeechIsHeadset, bool isPitchedDuringSlowMo, bool isMutedDuringSlowMo, bool isUrgent, bool playSpeechWhenFinishedPreparing)
{
	//Displayf("audSpeechAudioEntity::ReplayPlayScriptedSpeech time:%d hash %u, voice %u", CReplayMgr::GetCurrentTimeRelativeMs(), contextNameHash, voiceNameHash);

	s32 sBaseWaveSlotIndex = audWaveSlot::GetWaveSlotIndex(audWaveSlot::FindWaveSlot("SCRIPT_SPEECH_0"));
	s32 WaveSlotIndex = waveSlot + sBaseWaveSlotIndex;

	if(m_ScriptedSpeechSound || m_ReplayWaveSlotIndex >= 0)
	{
		return;
	}

	if(IsAmbientSpeechPlaying())
	{
		KillPreloadedSpeech();
	}

	// Need to clear this out each time we play something
	for(u32 i=0; i<NUM_AUD_SPEECH_METADATA_CATEGORIES; i++)
	{
		m_SpeechMetadataCategoryHashes[i] = atStringHash(g_SpeechMetadataCategoryNames[i]);
		m_LastQueriedMarkerIdForCategory[i] = -1;
		m_LastQueriedGestureIdForCategory[i] = -1;
	}

	m_GesturesHaveBeenParsed = false;
	m_GestureData = NULL;
	m_NumGestures = 0;
	m_ActiveSpeechIsHeadset = activeSpeechIsHeadset;

#if __BANK
	m_TimeOfFakeGesture = -1;
	m_SoundLengthForFakeGesture = -1;
	m_FakeMarkerReturned = false;
#endif

	if(uVariationNumber <= 0)
	{
		return;
	}

	const audMetadataRef soundRef = g_ScriptedSoundSet.Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));
	naAssertf(soundRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");

	// Set up our speech sound and init params
	audSoundInitParams initParams;
	initParams.StartOffset = uMsOffset;
	initParams.RemoveHierarchy = false;
	initParams.UpdateEntity = true;
	initParams.TimerId = 4;

	// In order to make sure we don't drop scripted speech which could end up breaking mission scripts, use the streaming bucket.
	// Each bucket has 192 slots, and the unlikely worst case scenario for the streaming bucket 
	// is something like 120 sounds so there should always be plenty room for scripted speech and ambient speech triggered from script
	BANK_ONLY(m_UsingStreamingBucking = false;)
	if(!PARAM_audNoUseStreamingBucketForSpeech.Get())
	{
		BANK_ONLY(m_UsingStreamingBucking = true;)
			Assign(initParams.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
	}
	if(!m_ScriptedSpeechSound)
	{
		CreateSound_PersistentReference(soundRef, (audSound**)&m_ScriptedSpeechSound, &initParams);
	}
	if(!m_ScriptedSpeechSound)
	{
		Assertf(0, "Unable to create a sound reference in ReplayPlayScriptedSound");
		return;
	}

	if(m_ScriptedSpeechSound->GetSoundTypeID() != SpeechSound::TYPE_ID)
	{
		Assertf(0, "Created a sound in PlayScriptedSpeech but it wasn't a speech sound");
		g_ScriptAudioEntity.CleanUpReplayScriptedSpeechWaveSlot(WaveSlotIndex);
		m_ReplayWaveSlotIndex = -1;
		m_ScriptedSpeechSound->StopAndForget();
		return;
	}

	bool bInitSpeech = m_ScriptedSpeechSound->InitSpeech(voiceNameHash, contextNameHash, uVariationNumber);

	if(!bInitSpeech)
	{
		naAssertf(0, "Unable to initialise speech sound in PlayScriptedSpeech");
		g_ScriptAudioEntity.CleanUpReplayScriptedSpeechWaveSlot(WaveSlotIndex);
		m_ReplayWaveSlotIndex = -1;
		m_ScriptedSpeechSound->StopAndForget();
		return;
	}

	const audSpeechType speechType = AUD_SPEECH_TYPE_SCRIPTED;

	m_VisemeAnimationData = NULL;

	// Cache the volType and audibility so that in the ::Update() we know what kind of speech it is when calculating GetSpeechAudibilityMetrics
	m_CachedSpeechStartOffset = uMsOffset;
	m_CachedSpeechType = speechType;
	m_CachedVolType = requestedVolume;
	m_CachedAudibility = audibility;
	m_SpeechVolumeOffset = 0.0f;
	m_IsFromScript = true;
	m_CurrentlyPlayingSoundSet = g_ScriptedSoundSet;
	m_UseShadowSounds = true;
	m_CurrentlyPlayingPredelay1 = preDelay;

	// if we don't have a waveslot passed-in, use the global one
	audWaveSlot* waveSlotToUse = audWaveSlot::GetWaveSlotFromIndex(WaveSlotIndex);
	if (!waveSlotToUse)
	{
		waveSlotToUse = audWaveSlot::FindWaveSlot((char *)g_SpeechWaveSlotName);
	}

	m_ScriptedSlot = waveSlotToUse;
	m_ReplayWaveSlotIndex = audWaveSlot::GetWaveSlotIndex(m_ScriptedSlot);

	m_ScriptedSpeechSound->Prepare(waveSlotToUse, true, naWaveLoadPriority::Speech);
	m_PlaySpeechWhenFinishedPreparing = playSpeechWhenFinishedPreparing;

	//BANK_ONLY(safecpy(m_CurrentlyPlayingSpeechContext, context);)
	m_LastPlayedSpeechContextPHash = contextHash;
	m_CurrentlyPlayingVoiceNameHash = voiceNameHash;

	// We've played this line, so register it as having been used, so we don't repeat it too soon.
	naCErrorf(uVariationNumber>=0, "Variation num for scripted speech is invalid");
#if __BANK
	// Since voices are being reused, use a ped identifier rather than voice name for history
	const u32 pedIdentifier = m_Parent ? static_cast<u32>(CPed::GetPool()->GetIndex(m_Parent)) : 0U;
	g_SpeechManager.AddVariationToHistory( (m_UsingFakeFullPVG || voiceNameHash==g_SilentVoiceHash) ? pedIdentifier : voiceNameHash, contextNameHash, (u32)uVariationNumber, fwTimer::GetTimeInMilliseconds());

#else
	g_SpeechManager.AddVariationToHistory(voiceNameHash, contextNameHash, (u32)uVariationNumber, fwTimer::GetTimeInMilliseconds());
#endif

	char variationBuf[4];
	formatf(variationBuf, "_%02u", uVariationNumber);

	m_PreloadedSpeechIsPlaying = false;

	m_VariationNum = uVariationNumber;

	m_VisemeAnimationData = NULL;

	m_IsUrgent = isUrgent;

	g_ScriptAudioEntity.AddToPlayedList(voiceNameHash, contextHash, uVariationNumber);

	m_IsPitchedDuringSlowMo = isPitchedDuringSlowMo;
	m_IsMutedDuringSlowMo = isMutedDuringSlowMo;
}
#endif

bool audSpeechAudioEntity::IsMegaphoneSpeechPlaying() const
{
	if(IsAmbientSpeechPlaying())
	{
		return m_CachedVolType == AUD_SPEECH_MEGAPHONE;
	}
	return false;
}

bool audSpeechAudioEntity::IsAmbientSpeechPlaying() const
{
	return m_AmbientSpeechSound || m_AmbientSpeechSoundEcho || m_AmbientSpeechSoundEcho2;
}

bool audSpeechAudioEntity::IsAnimalVocalizationPlaying() const
{
	return m_AnimalVocalSound || m_AnimalVocalEchoSound;
}

bool audSpeechAudioEntity::IsScriptedSpeechPlaying() const
{
	return m_ScriptedSpeechSound || m_ScriptedSpeechSoundEcho || m_ScriptedSpeechSoundEcho2;
}

bool audSpeechAudioEntity::IsScriptedSpeechPlaying(s32* length, s32* playTime) const
{
	if(m_ScriptedSpeechSound)
	{
		if (length && playTime && m_ScriptedSpeechSound->GetSoundTypeID() == SpeechSound::TYPE_ID && m_ScriptedSpeechSound->GetEnvironmentSound() != NULL)
		{
			bool looping = false;
			*playTime = m_ScriptedSpeechSound->GetWavePlaytime();
			*length = m_ScriptedSpeechSound->ComputeDurationMsIncludingStartOffsetAndPredelay(&looping);
		}
		return true;
	}
	else if (m_ScriptedSpeechSoundEcho)
	{
		if (length && playTime && m_ScriptedSpeechSoundEcho->GetSoundTypeID() == SpeechSound::TYPE_ID && m_ScriptedSpeechSoundEcho->GetEnvironmentSound() != NULL)
		{
			bool looping = false;
			*playTime = m_ScriptedSpeechSoundEcho->GetWavePlaytime();
			*length = m_ScriptedSpeechSoundEcho->ComputeDurationMsIncludingStartOffsetAndPredelay(&looping);
		}
		return true;
	}
	else if (m_ScriptedSpeechSoundEcho2)
	{
		if (length && playTime && m_ScriptedSpeechSoundEcho2->GetSoundTypeID() == SpeechSound::TYPE_ID && m_ScriptedSpeechSoundEcho2->GetEnvironmentSound() != NULL)
		{
			bool looping = false;
			*playTime = m_ScriptedSpeechSoundEcho2->GetWavePlaytime();
			*length = m_ScriptedSpeechSoundEcho2->ComputeDurationMsIncludingStartOffsetAndPredelay(&looping);
		}
		return true;
	}

	return false;

}

const audSpeechSound* audSpeechAudioEntity::GetEchoPrimary() const
{
	if(m_AmbientSpeechSoundEcho)
		return m_AmbientSpeechSoundEcho;
	else if(m_ScriptedSpeechSoundEcho)
		return m_ScriptedSpeechSoundEcho;
	else if(m_PainEchoSound)
		return m_PainEchoSound;
	else if(m_AnimalVocalEchoSound)
		return m_AnimalVocalEchoSound;
	else
		return NULL;
}

const audSpeechSound* audSpeechAudioEntity::GetEchoSecondary() const
{
	if(m_AmbientSpeechSoundEcho2)
		return m_AmbientSpeechSoundEcho2;
	else if(m_ScriptedSpeechSoundEcho2)
		return m_ScriptedSpeechSoundEcho2;
	else
		return NULL;
}

void audSpeechAudioEntity::StopPlayingScriptedSpeech()
{
	naSpeechEntDebugf3(this, "StopPlayingScriptedSpeech()");

#if GTA_REPLAY
	// Only record a stop packet if there is something to stop otherwise you 
	// are possibly going to cut off the ends of scripted speech during playback.
	// There is an stop called in FinishScriptedConversation, but the speech 
	// has normally ended by then so we don't want to record those stop cases
	if(CReplayMgr::ShouldRecord() && m_Parent && m_ScriptedSpeechSound)
	{
		CReplayMgr::RecordFx<CPacketSpeechStop>(
			CPacketSpeechStop(SPEECH_STOP_SCRIPTED), m_Parent);
	}
#endif

	//for testing strange conversation issues.
	//#if __BANK
	//	if(m_TimeScriptedSpeechStarted != 0)
	//	{
	//		if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_TimeScriptedSpeechStarted < 60)
	//			naDisplayf("Stopped");
	//		m_TimeScriptedSpeechStarted = 0;
	//	}
	//#endif
	if (m_ScriptedSpeechSound)
	{
		//naDisplayf("Stopping scripted speech %s", m_CurrentlyPlayingSpeechContext);
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			if(m_ScriptedSlot)
			{
				u32 index = m_ScriptedSlot->GetSlotIndex();
				g_ScriptAudioEntity.CleanUpReplayScriptedSpeechWaveSlot(index);
			}
		}
		m_ReplayWaveSlotIndex = -1;
#endif
		m_ScriptedSpeechSound->StopAndForget();
	}
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if (m_ScriptedShadowSound[i])
		{
			m_ScriptedShadowSound[i]->StopAndForget();
		}
	}
	if (m_ScriptedSpeechSoundEcho)
	{
		m_ScriptedSpeechSoundEcho->StopAndForget();
	}
	if (m_ScriptedSpeechSoundEcho2)
	{
		m_ScriptedSpeechSoundEcho2->StopAndForget();
	}

	if(m_ActiveSubmixIndex != -1)
	{
		g_AudioEngine.GetEnvironment().FreeConversationSpeechSubmix(m_ActiveSubmixIndex);
		m_ActiveSubmixIndex = -1;
	}
	if(m_HeadSetSubmixEnvSnd)
	{
		m_HeadSetSubmixEnvSnd->StopAndForget();
	}

	m_VisemeAnimationData = NULL;
	m_HasPreparedAndPlayed = false;

	if(m_Parent && !IsAmbientSpeechPlaying())
		m_Parent->SetIsWaitingForSpeechToPreload(false);
}

bool audSpeechAudioEntity::IsPainPlaying() const
{
	return m_PainSpeechSound || m_PainEchoSound;
}

void audSpeechAudioEntity::StopPain()
{
	naSpeechEntDebugf3(this, "StopPain()");

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSpeechStop>(
			CPacketSpeechStop(SPEECH_STOP_PAIN), m_Parent);
	}
#endif

	if(m_PainSpeechSound)
		m_PainSpeechSound->StopAndForget();
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if(m_PainShadowSound[i])
			m_PainShadowSound[i]->StopAndForget();
	}
	if(m_PainEchoSound)
		m_PainEchoSound->StopAndForget();
}

void audSpeechAudioEntity::StopAmbientSpeech()
{
	naSpeechEntDebugf3(this, "StopAmbientSpeech()");

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSpeechStop>(
			CPacketSpeechStop(SPEECH_STOP_AMBIENT), m_Parent);
	}
#endif

	if (m_AmbientSpeechSound)
	{
		m_AmbientSpeechSound->StopAndForget();
		if (m_Preloading)
		{
			m_Preloading = false;
			m_PreloadFreeTime = 0;
		}
		if (m_ReplyingPed)
		{
			m_ReplyingPed = NULL;
			m_DelayJackingReply = false;
			m_TimeToPlayJackedReply = 0;
		}
	}
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if(m_AmbientShadowSound[i])
		{
			m_AmbientShadowSound[i]->StopAndForget();
		}
	}
	if (m_AmbientSpeechSoundEcho)
	{
		m_AmbientSpeechSoundEcho->StopAndForget();
	}
	if (m_AmbientSpeechSoundEcho2)
	{
		m_AmbientSpeechSoundEcho2->StopAndForget();
	}

	if (m_AmbientSlotId>=0)
	{
		if(m_SuccessValueOfLastSay != AUD_SPEECH_SAY_DEFERRED)
			g_SpeechManager.FreeAmbientSpeechSlot(m_AmbientSlotId);
		m_AmbientSlotId = -1;
	}

	if(m_WaitingForAmbientSlot)
	{
		m_WaitingForAmbientSlot = false;
		m_AmbientSlotId = -1;
	}

	if(m_Parent && !IsScriptedSpeechPlaying())
		m_Parent->SetIsWaitingForSpeechToPreload(false);

	m_VisemeAnimationData = NULL;

	m_MultipartVersion = 0;
	m_MultipartContextCounter = 0;
	m_MultipartContextPHash = 0;
	m_OriginalMultipartContextPHash = 0;
}

void audSpeechAudioEntity::StopSpeech()
{
	naSpeechEntDebugf2(this, "StopSpeech()");

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSpeechStop>(
			CPacketSpeechStop(SPEECH_STOP_ALL), m_Parent);
	}
#endif

	StopPlayingScriptedSpeech();
	StopAmbientSpeech();
	StopPain();
}

void audSpeechAudioEntity::StopSpeechForinterruptingAmbientSpeech()
{
	naSpeechEntDebugf3(this, "StopSpeechForinterruptingAmbientSpeech()");

	StopPlayingScriptedSpeech();
	StopPain();

	if (m_AmbientSpeechSound)
	{
		m_AmbientSpeechSound->StopAndForget();
	}
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if (m_AmbientShadowSound[i])
		{
			m_AmbientShadowSound[i]->StopAndForget();
		}
	}
	if (m_AmbientSpeechSoundEcho)
	{
		m_AmbientSpeechSoundEcho->StopAndForget();
	}
	if (m_AmbientSpeechSoundEcho2)
	{
		m_AmbientSpeechSoundEcho2->StopAndForget();
	}
}

bool audSpeechAudioEntity::GetIsAnySpeechPlaying()
{
	for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS; i++)
	{
		if(m_AnimalOverlappableSound[i] != NULL)
			return true;
	}

	return IsAmbientSpeechPlaying() || IsScriptedSpeechPlaying() || IsPainPlaying() || IsAnimalVocalizationPlaying();
};

void audSpeechAudioEntity::MarkToStopSpeech()
{
	m_ShouldStopSpeech = true;
}

void audSpeechAudioEntity::MarkToSaySpaceRanger()
{
	m_ShouldSaySpaceRanger = true;
}

void audSpeechAudioEntity::MarkToPlayCyclingBreath()
{
	m_ShouldPlayCycleBreath = true;
}

void audSpeechAudioEntity::MakeTrevorRage()
{
	if(!CPedFactory::GetFactory())
		return;

	CPed* player = CPedFactory::GetFactory()->GetLocalPlayer();
	if(!player || !player->GetSpeechAudioEntity())
		return;

	if(player->GetPedType() != PEDTYPE_PLAYER_2)
	{
		audDisplayf("Attempting to play TrevorRage on non-Trevor player character, ignoring");
		return;
	}		

	bool customLinePlayed = false;
	if(sm_TrevorRageContextOverride[0]!='\0' && (!sm_TrevorRageOverrideIsScriptSet || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::TrevorRageIsOverriden)))
	{
		customLinePlayed = player->GetSpeechAudioEntity()->Say(sm_TrevorRageContextOverride,"SPEECH_PARAMS_FORCE", 0, -1, NULL, 0, -1, 1.0f, true) != AUD_SPEECH_SAY_FAILED;
	}
	if(!customLinePlayed)
	{
		player->GetSpeechAudioEntity()->Say("SHOOTOUT_SPECIAL", "SPEECH_PARAMS_FORCE", 0, -1, NULL, 0, -1, 1.0f, true);
	}
}

void audSpeechAudioEntity::OverrideTrevorRageContext(const char* context, bool fromScript)
{
	safecpy(sm_TrevorRageContextOverride, context);
	sm_TrevorRageContextOverridePHash = atPartialStringHash(context);
	sm_TrevorRageOverrideIsScriptSet = fromScript;
	if(fromScript)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "TrevorRageIsOverriden", CTheScripts::GetCurrentScriptName()))
		{
			script->SetAudioFlag("TrevorRageIsOverriden", true);
		}
	}
}

void audSpeechAudioEntity::ResetTrevorRageContext(bool fromScript)
{
	sm_TrevorRageContextOverride[0] = '\0';
	sm_TrevorRageContextOverridePHash = 0;
	if(fromScript)
	{
		audScript *script = g_ScriptAudioEntity.GetScript();
		if (naVerifyf(script, "Failed to set script audio flag %s; unable to get script %s", "TrevorRageIsOverriden", CTheScripts::GetCurrentScriptName()))
		{
			script->SetAudioFlag("TrevorRageIsOverriden", false);
		}
	}
}

void audSpeechAudioEntity::SetPedIsAngry(bool isAngry)
{
	m_IsAngry = isAngry;
	m_TimeToStopBeingAngry = 0;
}

void audSpeechAudioEntity::SetPedIsAngryShortTime(u32 timeInMs)
{
	m_IsAngry = true;
	m_TimeToStopBeingAngry = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + timeInMs;
}

bool audSpeechAudioEntity::ShouldBeAngry() const
{
	if(!m_Parent->IsLocalPlayer())
		return false;

	return m_IsCodeSetAngry ||
		(FindPlayerPed() && FindPlayerPed()->GetPlayerWanted() && FindPlayerPed()->GetPlayerWanted()->GetWantedLevel() >= WANTED_LEVEL3);
}

void audSpeechAudioEntity::SetPedIsBuddy(bool isBuddy)
{
	m_IsBuddy = isBuddy;
}

void audSpeechAudioEntity::RemoveContextBlocks(scrThreadId ScriptThreadId)
{
	for(int i=0; i<AUD_MAX_BLOCKED_CONTEXT_LISTS; i++)
	{
		if(sm_BlockedContextLists[i].ScriptThreadId == ScriptThreadId)
		{
			sm_BlockedContextLists[i].ScriptThreadId = THREAD_INVALID;
			sm_BlockedContextLists[i].ContextList = NULL;
		}
	}
}

void audSpeechAudioEntity::RemoveAllContextBlocks()
{
	for(int i=0; i<AUD_MAX_BLOCKED_CONTEXT_LISTS; i++)
	{
		sm_BlockedContextLists[i].ScriptThreadId = THREAD_INVALID;
		sm_BlockedContextLists[i].ContextList = NULL;
	}
}


void audSpeechAudioEntity::AddContextBlock(const char* contextListName, audContextBlockTarget target, scrThreadId ScriptThreadId)
{
	SpeechContextList* contextList = audNorthAudioEngine::GetObject<SpeechContextList>(contextListName);
	if(naVerifyf(contextList, "Attempting to block invalid contextList %s", contextListName))
	{
		s32 firstFreeContextBlockIndex = -1;

		for(int i=0; i<AUD_MAX_BLOCKED_CONTEXT_LISTS; i++)
		{
			if(sm_BlockedContextLists[i].ScriptThreadId == THREAD_INVALID)
			{
				if (firstFreeContextBlockIndex == -1)
				{
					firstFreeContextBlockIndex = i;
				}
			}
			else
			{	
				// This script has already requested this context block, just ignore
				if(sm_BlockedContextLists[i].ScriptThreadId == ScriptThreadId && sm_BlockedContextLists[i].ContextList == contextList && sm_BlockedContextLists[i].Target == target)
				{
					return;
				}
			}
		}

		if (firstFreeContextBlockIndex >= 0)
		{
			sm_BlockedContextLists[firstFreeContextBlockIndex].ScriptThreadId = ScriptThreadId;
			sm_BlockedContextLists[firstFreeContextBlockIndex].ContextList = contextList;
			sm_BlockedContextLists[firstFreeContextBlockIndex].Target = target;
		}
		else
		{
			naAssertf(0, "We've run out BlockedContextLists.  Unable to block using list %s. Alert audio.", contextListName);
		}
	}
}

void audSpeechAudioEntity::RemoveContextBlock(const char* contextListName)
{
	SpeechContextList* contextList = audNorthAudioEngine::GetObject<SpeechContextList>(contextListName);
	if(naVerifyf(contextList, "Attempting to remove invalid contextList %s", contextListName))
	{
		for(int i=0; i<AUD_MAX_BLOCKED_CONTEXT_LISTS; i++)
		{
			if(sm_BlockedContextLists[i].ContextList == contextList)
			{
				sm_BlockedContextLists[i].ScriptThreadId = THREAD_INVALID;
				sm_BlockedContextLists[i].ContextList = NULL;
				//Don't return...remove all instances of this block
			}
		}
	}
}

bool audSpeechAudioEntity::ShouldBlockContext(u32 contextHash)
{
	if(!m_Parent)
		return false;

	for(int i=0; i<AUD_MAX_BLOCKED_CONTEXT_LISTS; i++)
	{
		if(sm_BlockedContextLists[i].ScriptThreadId != THREAD_INVALID && sm_BlockedContextLists[i].ContextList)
		{
			bool shouldCheck = false;
			switch(sm_BlockedContextLists[i].Target)
			{
			case AUD_CONTEXT_BLOCK_PLAYER:
				shouldCheck = m_Parent->IsLocalPlayer();
				break;
			case AUD_CONTEXT_BLOCK_NPCS:
				shouldCheck = !m_Parent->IsLocalPlayer();
				break;
			case AUD_CONTEXT_BLOCK_BUDDYS:
				shouldCheck = m_IsBuddy;
				break;
			case AUD_CONTEXT_BLOCK_EVERYONE:
				shouldCheck = true;
				break;
			default:
				break;
			}

			if(shouldCheck)
			{
				for(int j=0; j<sm_BlockedContextLists[i].ContextList->numSpeechContexts; j++)
				{
					if(sm_BlockedContextLists[i].ContextList->SpeechContexts[j].SpeechContext == contextHash)
						return true;
				}
			}
		}
	}

	return false;
}

void audSpeechAudioEntity::TriggerCollisionScreams()
{
	sm_NumCollisionScreamsToTrigger = (u8)g_NumScreamsForRunnedOverReaction;
}

u32 audSpeechAudioEntity::GetActiveScriptedSpeechPlayTimeMs(void)
{
	u32 playTimeMs = 0;

	if(m_ScriptedSpeechSound)
	{
		playTimeMs = m_ScriptedSpeechSound->GetWavePlaytime();
	}
	else if(m_ScriptedSpeechSoundEcho)
	{
		playTimeMs = m_ScriptedSpeechSoundEcho->GetWavePlaytime();
	}
	else if(m_ScriptedSpeechSoundEcho2)
	{
		playTimeMs = m_ScriptedSpeechSoundEcho2->GetWavePlaytime();
	}

	return playTimeMs;
}

u32 audSpeechAudioEntity::GetActiveSpeechPlayTimeMs(u32& predelay, u32& remainingPredelay)
{
	u32 playTimeMs = 0;
	audSpeechSound *activeSound = (m_AmbientSpeechSound?m_AmbientSpeechSound:m_ScriptedSpeechSound);
	audSpeechSound *activeEchoSound = (m_AmbientSpeechSoundEcho?m_AmbientSpeechSoundEcho:m_ScriptedSpeechSoundEcho);
	audSpeechSound *activeEchoSound2 = (m_AmbientSpeechSoundEcho2?m_AmbientSpeechSoundEcho2:m_ScriptedSpeechSoundEcho2);
	predelay = m_TotalPredelayForLipsync;
	if(activeSound != NULL)
	{
		s32 playTime = -1;
		playTime = activeSound->GetWavePlaytime();

		u32 currentPlayTime = activeSound->GetCurrentPlayTime();
		u32 additionalPredelay = activeSound->GetAdditionalPredelay();
		remainingPredelay = currentPlayTime < additionalPredelay ? additionalPredelay - currentPlayTime : 0;
		
		if (playTime>=0)
		{
			playTimeMs = (u32)playTime;
		}
		else
		{
			remainingPredelay = predelay;
		}
	}
	else if(activeEchoSound != NULL)
	{
		s32 playTime = -1;
		playTime = activeEchoSound->GetWavePlaytime();
		remainingPredelay = activeEchoSound->GetPredelayRemainingMs();

		if (playTime>=0)
		{
			playTimeMs = (u32)playTime;
		}
		else
		{
			remainingPredelay = predelay;
		}
	}
	else if(activeEchoSound2 != NULL)
	{
		s32 playTime = -1;
		playTime = activeEchoSound2->GetWavePlaytime();
		remainingPredelay = activeEchoSound2->GetPredelayRemainingMs();

		if (playTime>=0)
		{
			playTimeMs = (u32)playTime;
		}
		else
		{
			remainingPredelay = predelay;
		}
	}

#if __BANK
	if(g_DisplayLipsyncTimingInfo)
	{
		fwEntity *pSelectedEntity = g_PickerManager.GetSelectedEntity();
		if(pSelectedEntity == NULL)
		{
			pSelectedEntity = CGameWorld::FindLocalPlayer();
		}
		if(g_DisplayLipsyncTimingInfo && m_Parent && pSelectedEntity == m_Parent)
		{
			formatf(sm_VisemeTimingingInfoString, "Playtime: %u Predelay: %u Remaining Predelay: %u Duration: %f", playTimeMs, predelay, remainingPredelay,
										m_Parent->GetVisemeDuration());
		}
	}

#endif

	return playTimeMs;
}

bool audSpeechAudioEntity::GetLastMetadataHash(u32 categoryId, u32 &hash)
{
	const audWaveMarker* marker = GetLastMarkerForCategoryId(categoryId);
	if(marker)
	{
		hash = marker->nameHash;
		return true;
	}
	else
	{
		return false;
	}
}

bool audSpeechAudioEntity::GetLastMetadataHash(u32 categoryId, u32 &hash, u32 &data)
{
	const audWaveMarker* marker = GetLastMarkerForCategoryId(categoryId);
	if(marker)
	{
		hash = marker->nameHash;
		data = marker->data;
		return true;
	}
	else
	{
		return false;
	}
}

bool audSpeechAudioEntity::GetLastMetadataValue(u32 categoryId, f32 &value)
{
	const audWaveMarker* marker = GetLastMarkerForCategoryId(categoryId);
	if(marker)
	{
		value = marker->value;
		return true;
	}
	else
	{
		return false;
	}
}

const audWaveMarker *audSpeechAudioEntity::GetLastMarkerForCategoryId(u32 categoryId)
{
	audSpeechSound *activeSound = (m_AmbientSpeechSound?m_AmbientSpeechSound:m_ScriptedSpeechSound);
	if(activeSound != NULL)
	{
		if(activeSound && activeSound->GetSoundTypeID() == SpeechSound::TYPE_ID && activeSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			audWaveSlot *slot = activeSound->GetWaveSlot();
			naAssertf(slot, "Unable to get a wave slot for the active speech sound, will access a null ptr...");

			audWaveRef waveRef;
			if(waveRef.Init(slot,activeSound->GetWaveNameHash()))
			{			
				audWaveMarkerList markers;
				waveRef.FindMarkerData(markers);
				const audWaveFormat *format = waveRef.FindFormat();
				Assert(format);

				const u32 playTime = (u32)activeSound->GetWavePlaytime();
				if(playTime != ~0U && playTime != 0x07FFFFF0)
				{
#if __BANK
					if(markers.GetCount() == 0 && g_ForceFakeGestures && categoryId==AUD_SPEECH_METADATA_CATEGORY_GESTURE_SPEAKER)
					{
						if(m_TimeOfFakeGesture == -1)
						{
							bool unused = false;
							m_SoundLengthForFakeGesture = activeSound->ComputeDurationMsExcludingStartOffsetAndPredelay(&unused);
							m_TimeOfFakeGesture = (s32)(fwRandom::GetRandomNumberInRange(0.1f, 0.3f) * m_SoundLengthForFakeGesture);
						}
						else if(m_TimeOfFakeGesture < playTime && !m_FakeMarkerReturned)
						{
							m_FakeMarker.nameHash = m_FakeGestureNameHash;
							m_FakeMarker.data = m_SoundLengthForFakeGesture - playTime;
							m_FakeMarkerReturned = true;
							return &m_FakeMarker;
						}
					}
#endif

					for(s32 i=0; i< markers.GetCount(); i++)
					{
						u32 markerTime = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, format->SampleRate);
						//Look for most recent one, then check we didn't return it last time
						if(markers[i].categoryNameHash == m_SpeechMetadataCategoryHashes[categoryId] &&
							playTime >= markerTime && 
							i > m_LastQueriedMarkerIdForCategory[categoryId])
						{
							// We found one, and it wasn't the same one as the last time we were asked
							m_LastQueriedMarkerIdForCategory[categoryId] = i;
							return &(markers[i]); 
						}
					}
				}
			}
		}
	}
	return NULL;
}

u32 audSpeechAudioEntity::GetConversationInterruptionOffset(u32& playTime, bool& hasNoMarkers)
{
	u32 uLastTimeOffsetMs = ~0U; 
	u32 uNextTimeOffsetMs = ~0U;
	playTime = ~0U;
	hasNoMarkers = true;
	audSpeechSound *activeSound = (m_AmbientSpeechSound?m_AmbientSpeechSound:m_ScriptedSpeechSound);
	if(activeSound != NULL)
	{
		if(activeSound && activeSound->GetSoundTypeID() == SpeechSound::TYPE_ID && activeSound->GetPlayState() == AUD_SOUND_PLAYING)
		{
			audWaveSlot *slot = activeSound->GetWaveSlot();
			naAssertf(slot, "Unable to get a wave slot for the active speech sound, will access a null ptr...");

			audWaveRef waveRef;
			if(waveRef.Init(slot,activeSound->GetWaveNameHash()))
			{			
				audWaveMarkerList markers;
				waveRef.FindMarkerData(markers);
				const audWaveFormat *format = waveRef.FindFormat();
				Assert(format);

				playTime = (u32)activeSound->GetWavePlaytime();
				if(playTime != ~0U && playTime != 0x07FFFFF0)
				{
					for(s32 i=0; i< markers.GetCount(); i++)
					{
						u32 markerTime = audDriverUtil::ConvertSamplesToMs(markers[i].positionSamples, format->SampleRate);
						if(markers[i].categoryNameHash == m_SpeechMetadataCategoryHashes[AUD_SPEECH_METADATA_CATEGORY_RESTART_POINT] &&
							playTime >= markerTime)
						{
							hasNoMarkers = false;
							if(waveRef.FindFormat())
							{
								u32 timeOffsetMs = u32((float)markers[i].positionSamples / ((float)waveRef.FindFormat()->SampleRate*0.001f));
								if(uLastTimeOffsetMs == ~0U || playTime - timeOffsetMs < playTime - uLastTimeOffsetMs)
									uLastTimeOffsetMs = timeOffsetMs;
							}
						}
						else if(markers[i].categoryNameHash == m_SpeechMetadataCategoryHashes[AUD_SPEECH_METADATA_CATEGORY_RESTART_POINT] &&
							playTime < markerTime)
						{
							hasNoMarkers = false;
							if(waveRef.FindFormat())
							{
								u32 timeOffsetMs = u32((float)markers[i].positionSamples / ((float)waveRef.FindFormat()->SampleRate*0.001f));
								if(uNextTimeOffsetMs == ~0U || timeOffsetMs - playTime < uNextTimeOffsetMs - playTime)
									uNextTimeOffsetMs = timeOffsetMs;
							}
						}
					}
				}
			}
		}
	}

	if(playTime == ~0U)
	{
		return 0;
	}
	else if(uNextTimeOffsetMs != ~0U && uNextTimeOffsetMs - playTime < 500)
	{
		return uNextTimeOffsetMs;
	}
	else if(uLastTimeOffsetMs != ~0U)
	{
		return uLastTimeOffsetMs;
	}
		
	return 0;
}

#if __BANK

u32 audSpeechAudioEntity::GetFakeGestureHashForContext(SpeechContext* context)
{
	if(!context || context->numFakeGestures==0)
		return g_NullSoundHash;

	static const atHashString s_AbsolutelyGesture("Gesture_Displeased",0xCEDA50BE);
	static const atHashString s_AgreeGesture("Gesture_Nod_Yes_Soft",0x8D58D3C);
	static const atHashString s_ComeHereFrontGesture("Gesture_Come_Here_Soft",0xC6D84065);
	static const atHashString s_ComeHereLeftGesture("Gesture_Come_Here_Soft",0xC6D84065);
	static const atHashString s_DamnGesture("Gesture_Damn",0x32B4ABF4);
	static const atHashString s_DontKnowGesture("Gesture_Shrug_Soft",0xCEF8B5A5);
	static const atHashString s_EasyNowGesture("Gesture_Easy_Now",0x32F8D7A7);
	static const atHashString s_ExactlyGesture("Gesture_Nod_Yes_Hard",0xF74B4207);
	static const atHashString s_ForgetItGesture("Gesture_Displeased",0xCEDA50BE);
	static const atHashString s_GoodGesture("Gesture_Nod_Yes_Soft",0x8D58D3C);
	static const atHashString s_HelloGesture("Gesture_Hello",0xFEC7EB20);
	static const atHashString s_HeyGesture("Gesture_Hello",0xFEC7EB20);
	static const atHashString s_IDontThinkSoGesture("Gesture_No_Way",0x981A4DA0);
	static const atHashString s_IfYouSaySoGesture("Gesture_Shrug_Soft",0xCEF8B5A5);
	static const atHashString s_IllDoItGesture("Gesture_Nod_Yes_Hard",0xF74B4207);
	static const atHashString s_ImNotSureGesture("Gesture_What_Soft",0x89C16359);
	static const atHashString s_ImSorryGesture("Gesture_Nod_No_Soft",0x3EFCDD04);
	static const atHashString s_IndicateBackGesture("Gesture_Me",0x788A8ABD);
	static const atHashString s_IndicateLeftGesture("Gesture_Hand_Left",0x3981B224);
	static const atHashString s_IndicateRightGesture("Gesture_Hand_Right",0xF6C14374);
	static const atHashString s_NoChanceGesture("Gesture_No_Way",0x981A4DA0);
	static const atHashString s_YesGesture("Gesture_Nod_Yes_Hard",0xF74B4207);
	static const atHashString s_YouUnderstandGesture("Gesture_You_Soft",0xD8884A6B);

	u32 index = fwRandom::GetRandomNumber() % context->numFakeGestures;
	FakeGesture fakeGesture = static_cast< FakeGesture >(context->FakeGesture[index].GestureEnum);
	switch(fakeGesture)
	{
	case kAbsolutelyGesture: return s_AbsolutelyGesture;
	case kAgreeGesture: return s_AgreeGesture;
	case kComeHereFrontGesture: return s_ComeHereFrontGesture;
	case kComeHereLeftGesture: return s_ComeHereLeftGesture;
	case kDamnGesture: return s_DamnGesture;
	case kDontKnowGesture: return s_DontKnowGesture;
	case kEasyNowGesture: return s_EasyNowGesture;
	case kExactlyGesture: return s_ExactlyGesture;
	case kForgetItGesture: return s_ForgetItGesture;
	case kGoodGesture: return s_GoodGesture;
	case kHelloGesture: return s_HelloGesture;
	case kHeyGesture: return s_HeyGesture;
	case kIDontThinkSoGesture: return s_IDontThinkSoGesture;
	case kIfYouSaySoGesture: return s_IfYouSaySoGesture;
	case kIllDoItGesture: return s_IllDoItGesture;
	case kImNotSureGesture: return s_ImNotSureGesture;
	case kImSorryGesture: return s_ImSorryGesture;
	case kIndicateBackGesture: return s_IndicateBackGesture;
	case kIndicateLeftGesture: return s_IndicateLeftGesture;
	case kIndicateRightGesture: return s_IndicateRightGesture;
	case kNoChanceGesture: return s_NoChanceGesture;
	case kYesGesture: return s_YesGesture;
	case kYouUnderstandGesture: return s_YouUnderstandGesture;
	default: return g_NullSoundHash;
	}
}

#endif

//struct datResourceFileHeaderOld {
//	size_t Magic;				// c_MAGIC above (not kept in an archive)
//	intptr_t Version;			// Application-defined version information.  (only eight lsb's kept in an archive)
//
//	datResourceInfoOld Info;	// Size information (kept separately in an archive)
//};

// this function copies the crAnimation (in resource format) from the chunk data, and uses the resource header at the beginning of the chunk data to call place on the animation and fixup the pointers
crClipDictionary* audSpeechAudioEntity::ProcessClipDictionaryResource(void* chunkData, u32 /*numBytes*/) 
{
	datResourceFileHeader *dictionaryResourceHeader = static_cast<datResourceFileHeader*>(chunkData);

	naCErrorf(dictionaryResourceHeader->Magic == datResourceFileHeader::c_MAGIC,
		"Invalid magic number for viseme data (found %u, expected %u.) The speech audio data needs to be rebuilt.", 
		dictionaryResourceHeader->Magic, datResourceFileHeader::c_MAGIC);
	
	naCErrorf((dictionaryResourceHeader->Version & 0xFF) == crClipDictionary::RORC_VERSION,
		"Invalid crAnimation version number for viseme data (found %u, expected %u.) The speech audio data needs to be rebuilt.", 
		(dictionaryResourceHeader->Version & 0xFF), crClipDictionary::RORC_VERSION);

	crClipDictionary *pVisemeClipDataDictionary = NULL;

	// Skip the header
	chunkData = (void*)((char*)chunkData + sizeof(datResourceFileHeader));
	TrapZ((((pgBase*)chunkData)->HasFirstNode()));

	if ((((pgBase*)chunkData)->HasFirstNode()) && (dictionaryResourceHeader->Magic == datResourceFileHeader::c_MAGIC) && ((dictionaryResourceHeader->Version & 0xFF) == crClipDictionary::RORC_VERSION))
	{
		datResourceMap map;
		pgRscBuilder::GenerateMap(dictionaryResourceHeader->Info, map);

		strStreamingAllocator& allocator = strStreamingEngine::GetAllocator();
		if (allocator.StreamingAllocate(map, strIndex(), MEMBUCKET_RESOURCE, MEMUSERDATA_VISEME))
		{
			// Copy data to resource memory blocks (may be more than one now)
			for (int i=0; i<map.VirtualCount; i++) 
			{
				sysMemCpy(map.Chunks[i].DestAddr, chunkData, map.Chunks[i].Size);
				chunkData = (void*)((char*)chunkData + map.Chunks[i].Size);
			}

			// Note this memory is not marked defragmentable.  It won't be around for long.
			datResource rsc(map, "Viseme animation data");
			pVisemeClipDataDictionary = (crClipDictionary *)map.Chunks[0].DestAddr;
			AudioClipDictionary::Place((AudioClipDictionary*)pVisemeClipDataDictionary, rsc);
			TrapZ(pVisemeClipDataDictionary->HasPageMap());
		}
	}
	else
	{
		//We have found an incompatible animation version, so disable further viseme processing to prevent a string of additional asserts.
		//We might also have bad data too so ignore it to avoid a crash on teardown
		//ms_ShouldProcessVisemes = false;
	}

	return pVisemeClipDataDictionary;
}

void audSpeechAudioEntity::GetVisemeData(void **data)
{
	audSpeechSound *activeSound = ( (m_AmbientSpeechSound && m_AmbientSpeechSound->GetWaveSlot()) ?m_AmbientSpeechSound:m_ScriptedSpeechSound);
	if(!m_IgnoreVisemeData && ms_ShouldProcessVisemes && (activeSound != NULL))
	{
		if(m_VisemeAnimationData == NULL)
		{
			if(activeSound && activeSound->GetSoundTypeID() == SpeechSound::TYPE_ID /*&& activeSound->GetPlayState() == AUD_SOUND_PLAYING*/)
			{
				if(activeSound->GetWaveSlot() == NULL)
				{
#if __BANK
					Displayf("Current context %s", m_CurrentlyPlayingSpeechContext);
					Displayf("Current Ambient Slot %i", m_AmbientSlotId);
					Displayf("Current scripted slot address %p", m_ScriptedSlot);
					Displayf("Current active sound address %p", activeSound);
					Assertf(0, "Attempting to get viseme data from valid sound with NULL waveslot.  This is very wrong.  Alert Rob Katz and send output.");
#endif
					StopSpeech();
					return;
				}
				audWaveRef waveRef;
				if(waveRef.Init(activeSound->GetWaveSlot(), activeSound->GetWaveNameHash()))
				{
//#if USE_DEBUG_FILE
//					s_DebugAnimationFile = ASSET.Open("x:/tmp/face/output/flee_01",__PS3?"can":"xan");
//					bool dataFound = s_DebugAnimationFile!=NULL;
//					const void *chunkData = NULL;
//#else
					// the chunk data is an animation resource
#if __64BIT
					const u32 visemeChunkNameHash = ATSTRINGHASH("lipsync64", 0x0938c925c);
#else
					const u32 visemeChunkNameHash = ATSTRINGHASH("lipsync", 0x013d9f241);
#endif	// __64BIT
					u32 numBytes = 0;
					const void *chunkData = waveRef.FindChunk(visemeChunkNameHash, numBytes);
					//AlignedAssert(chunkData, 16);
					bool dataFound = chunkData!=NULL;


#if __BANK && AUD_DEBUG_CUSTOM_LIPSYNC
					if(waveRef.FindFormat() && waveRef.FindFormat()->padding != 0)
					{
						naDisplayf("%s: Found custom lipsync", m_LastPlayedWaveName);
					}
					else
					{
						const u32 custVisemeChunkNameHash = ATSTRINGHASH("custom_lipsync", 0x00693333e);
						u32 custNumBytes = 0;
						const void *custChunkData = waveRef.FindChunk(custVisemeChunkNameHash, custNumBytes);
						//AlignedAssert(custChunkData, 16);
						bool custDataFound = custChunkData!=NULL;
						if(custDataFound)
						{
							naDisplayf("%s: Found custom lipsync", m_LastPlayedWaveName);
						}
					}
#endif

//
////#endif
//
//					
//					m_VisemeType = AUD_VISEME_TYPE_FACEFX;
//					if(custDataFound)
//					{
//						//TODO: Get custom anims playing properly.  Probalby for Musson to do.
//						return;
//						//m_VisemeType = AUD_VISEME_TYPE_IM;
//					}

					if(dataFound)
					{
						// Displayf(">> found lipsync data for 0x%x",activeSound->GetWaveNameHash());
						crClipDictionary* visemeClipDictionary = ProcessClipDictionaryResource(const_cast<void*>(chunkData), numBytes);
						if(visemeClipDictionary)
						{
							crClipAnimation* clip = static_cast<crClipAnimation*>(visemeClipDictionary->FindClipByIndex(0));  // There's only 1 clip in this dictionary
							const crAnimation* anim = clip->GetAnimation();
							if(anim && anim->GetNumBlocks())
							{
								m_VisemeAnimationData = clip;
							}
						}
/*
						crClipAnimation* pVisemeAnimationData = ProcessAnimationResource(const_cast<void*>(chunkData), numBytes);
						if(pVisemeAnimationData)
						{
							// we're going to need a custom release strategy because our animation resource is a single block allocation, but since it's not in the resource heap, then the
							// ~crAnimation is going to choke when it tries to delete all of its contained allocations (normally the resource heap knows to discard these delete calls)
							m_VisemeAnimationData = rage_new crClipAnimationCustomAnimationAllocation; // crClipAnimation::AllocateAndCreate(*pVisemeAnimationData);
							((crClipAnimationCustomAnimationAllocation*)m_VisemeAnimationData)->Create(*pVisemeAnimationData);
							pVisemeAnimationData->Release();
						}
*/
					}
				}
			}
		}
	}
	*data = m_VisemeAnimationData;
}

crClipDictionary* audSpeechAudioEntity::GetVisemeData(void **data, audSpeechSound* sound
#if !__FINAL
													  , bool& failedToInitWaveRef
#endif	
													  )
{
	crClipDictionary* pDictionary = NULL;

	if(ms_ShouldProcessVisemes && sound)
	{
		if(sound && sound->GetSoundTypeID() == SpeechSound::TYPE_ID /*&& activeSound->GetPlayState() == AUD_SOUND_PLAYING*/)
		{
			if(sound->GetWaveSlot() == NULL)
			{
				return NULL;
			}
#if !__FINAL
			failedToInitWaveRef = false;
#endif
			audWaveRef waveRef;
			if(waveRef.Init(sound->GetWaveSlot(), sound->GetWaveNameHash()))
			{
				// the chunk data is an animation resource
#if __64BIT
				const u32 visemeChunkNameHash = ATSTRINGHASH("lipsync64", 0x0938c925c);
#else
				const u32 visemeChunkNameHash = ATSTRINGHASH("lipsync", 0x013d9f241);
#endif	// __64BIT
				u32 numBytes = 0;
				const void *chunkData = waveRef.FindChunk(visemeChunkNameHash, numBytes);
				bool dataFound = chunkData!=NULL;

#if !__FINAL && AUD_DEBUG_CUSTOM_LIPSYNC
				if(waveRef.FindFormat() && waveRef.FindFormat()->padding != 0)
				{
					naDisplayf("Found custom lipsync");
				}
				else
				{
					const u32 custVisemeChunkNameHash = ATSTRINGHASH("custom_lipsync", 0x00693333e);
					u32 custNumBytes = 0;
					const void *custChunkData = waveRef.FindChunk(custVisemeChunkNameHash, custNumBytes);
					//AlignedAssert(custChunkData, 16);
					bool custDataFound = custChunkData!=NULL;
					if(custDataFound)
					{
						naDisplayf("Found custom lipsync");
					}
				}
#endif

				if(dataFound)
				{
					// Displayf(">> found lipsync data for 0x%x",activeSound->GetWaveNameHash());
					pDictionary = ProcessClipDictionaryResource(const_cast<void*>(chunkData), numBytes);
					if(pDictionary)
					{
						pDictionary->AddRef();
						crClipAnimation* clip = static_cast<crClipAnimation*>(pDictionary->FindClipByIndex(0));  // There's only 1 clip in this dictionary
						const crAnimation* anim = clip->GetAnimation();
						if(anim && anim->GetNumBlocks())
						{
							*data = clip;
						}
					}
				}
			}
#if !__FINAL
			else
			{
				failedToInitWaveRef = true;
			}
#endif
		}
	}

	return pDictionary;
}

f32 audSpeechAudioEntity::GetSpeechHeadroom()
{
	audSpeechSound *activeSound = (m_AmbientSpeechSound ? m_AmbientSpeechSound : m_ScriptedSpeechSound);
	if(activeSound)
		return activeSound->GetWaveHeadroom();

	return -1.0f;
}


audWaveSlot* audSpeechAudioEntity::AddRefToActiveSoundSlot()
{
	audSpeechSound *activeSound = (m_AmbientSpeechSound ? m_AmbientSpeechSound : m_ScriptedSpeechSound);
	if(activeSound && activeSound->GetWaveSlot())
	{
		activeSound->GetWaveSlot()->AddReference();
		return activeSound->GetWaveSlot();
	}
	
	return NULL;
}

void audSpeechAudioEntity::GetAndParseGestureData()
{
	audSpeechSound *activeSound = (m_AmbientSpeechSound ? m_AmbientSpeechSound : m_ScriptedSpeechSound);
	if(activeSound)
	{
		if(activeSound && activeSound->GetSoundTypeID() == SpeechSound::TYPE_ID /* && activeSound->GetPlayState() == AUD_SOUND_PLAYING */)
		{
			if(activeSound->GetWaveSlot() == NULL)
			{
#if __BANK
				Displayf("Current context %s", m_CurrentlyPlayingSpeechContext);
				Displayf("Current Ambient Slot %i", m_AmbientSlotId);
				Displayf("Current scripted slot address %p", m_ScriptedSlot);
				Displayf("Current active sound address %p", activeSound);
#endif
				Assertf(0, "Attempting to get gesture data from valid sound with NULL waveslot.  This is very wrong.  Alert Rob Katz and send output.");
				StopSpeech();
				return;
			}

			audWaveRef waveRef;
			if(waveRef.Init(activeSound->GetWaveSlot(), activeSound->GetWaveNameHash()))
			{
				const u32 gestureChunkNameHash = ATSTRINGHASH("gesture", 0x023097a2b);

				u32 numBytes = 0;
				const void *chunkData = waveRef.FindChunk(gestureChunkNameHash, numBytes);
				if(chunkData)
				{
					// Parse gesture data

					m_GestureData = reinterpret_cast< const audGestureData * >(chunkData);
					const u32 audGestureDataSize = sizeof(audGestureData);
#if __ASSERT
					if(numBytes % audGestureDataSize != 0)
					{
						naErrorf("Bad gesture data packet. ped: %s context: %s",
							m_Parent && m_Parent->GetPedModelInfo() ? m_Parent->GetPedModelInfo()->GetModelName() : "UNKNOWN", m_CurrentlyPlayingSpeechContext);
						naAssertf(0, "numBytes should be an exact multiple of sizeof(audGestureData)!");
					}
#endif
					m_NumGestures = numBytes / audGestureDataSize;
				}
#if __BANK
				if(g_ForceFakeGestures && !chunkData)
				{
					m_GestureData = &m_FakeGestureData;
					m_NumGestures = 1;
					m_FakeGestureData.ClipNameHash = g_FakeGestureName.IsNull() ? m_FakeGestureNameHash : g_FakeGestureName.GetHash();
					m_FakeGestureData.MarkerTime = g_FakeGestureMarkerTime;
					m_FakeGestureData.StartPhase = g_FakeGestureStartPhase;
					m_FakeGestureData.EndPhase = g_FakeGestureEndPhase;
					m_FakeGestureData.Rate = g_FakeGestureRate;
					m_FakeGestureData.MaxWeight = g_FakeGestureMaxWeight;
					m_FakeGestureData.BlendInTime = g_FakeGestureBlendInTime;
					m_FakeGestureData.BlendOutTime = g_FakeGestureBlendOutTime;
					m_FakeGestureData.IsSpeaker = g_FakeGestureIsSpeaker;
				}
#endif
			}
		}
	}
}


const audGestureData* audSpeechAudioEntity::GetLastGestureDataForCategoryID(u32 categoryId)
{
	if(!Verifyf(categoryId == AUD_SPEECH_METADATA_CATEGORY_GESTURE_SPEAKER || categoryId == AUD_SPEECH_METADATA_CATEGORY_GESTURE_LISTENER,
		"Attempting to get gesture data for a currently unsupported category type."))
	{
		return NULL;
	}

	if(!m_GestureData)
	{
		return NULL;
	}

	audSpeechSound *activeSound = (m_AmbientSpeechSound ? m_AmbientSpeechSound : m_ScriptedSpeechSound);

	if(activeSound && activeSound->GetSoundTypeID() == SpeechSound::TYPE_ID && activeSound->GetPlayState() == AUD_SOUND_PLAYING)
	{
		audWaveSlot *slot = activeSound->GetWaveSlot();
		naAssertf(slot, "Unable to get a wave slot for the active speech sound, will access a null ptr...");

		audWaveRef waveRef;
		if(waveRef.Init(slot,activeSound->GetWaveNameHash()))
		{	
			const u32 playTime = (u32)activeSound->GetWavePlaytime();
			if(playTime != ~0U && playTime != 0x07FFFFF0)
			{
				for(s32 i = m_LastQueriedGestureIdForCategory[categoryId]+1; i< m_NumGestures; i++)
				{
					const audWaveFormat *format = waveRef.FindFormat();
					Assert(format);
					u32 markerTime = audDriverUtil::ConvertSamplesToMs(m_GestureData[i].MarkerTime, format->SampleRate);

					if( ((categoryId == AUD_SPEECH_METADATA_CATEGORY_GESTURE_SPEAKER && m_GestureData[i].IsSpeaker) ||
						(categoryId == AUD_SPEECH_METADATA_CATEGORY_GESTURE_LISTENER && !m_GestureData[i].IsSpeaker)) &&
						m_GestureData[i].ClipNameHash != 0 && m_GestureData[i].ClipNameHash != g_NullSoundHash &&
						playTime >= markerTime
						)
					{
						// We found one, and it wasn't the same one as the last time we were asked
						m_LastQueriedGestureIdForCategory[categoryId] = i;
						return &(m_GestureData[i]); 
					}
				}
			}
		}
	}
	return NULL;
}



//bool audSpeechAudioEntity::AttemptGesture(sagActor* pTarget)
//{
//	sagActor* pParentActor = GetParentActor();
//	if(!pParentActor)
//		return false;
//
//	if(PARAM_nogestures.Get())
//		return false;
//
//	animAnimatorComponent * ac = GET_COMPONENT_SIMPLE(*pParentActor, animAnimatorComponent);
//	if(ac)
//	{
//		aniActionSet* pActionSet = ac->GetActionSet();
//
//		//  make sure gesture is not already playing
//		aniGesture* gesture         = pActionSet ? pActionSet->GetGesture():  NULL;
//		const bool  gestureInuse    = gesture ? gesture->IsGestureOn():  false;
//		const bool	canPlayGesture	= gesture ? gesture->CanPlayGesture(pParentActor->GetGuid()) : false;
//
//		sagGuid gestureTarget = sagGuid();
//
//		//just create a gesture request; need to wait for wave to load to know if there gesture metadata
//		if( gesture && !gestureInuse && canPlayGesture) 
//		{
//			// should we consider anything on the target?
//			if(pTarget)
//			{
//				animAnimatorComponent * tac = GET_COMPONENT_SIMPLE(*pTarget, animAnimatorComponent);
//				if(tac)
//				{		
//					//  make sure gesture is not already playing
//					aniGesture* tGesture          = tac->GetActionSet()->GetGesture();
//					const bool  tGestureInuse     = tGesture ? tGesture->IsGestureOn():  false;
//					const bool	tCanPlayGesture	  = tGesture ? tGesture->CanPlayGesture(pTarget->GetGuid()) : false;
//					
//					if(tGesture && !tGestureInuse && tCanPlayGesture) 
		//					{
//						gestureTarget = pTarget->GetGuid();
		//					}
		//							}
		//						}
//
//			// start the gesture, with or without a target
//			gesture->Start( *this, pParentActor->GetGuid(), gestureTarget );	
//			return true;
		//						}
		//					}
//
//	return false;
		//}

void audSpeechAudioEntity::Update()
{
	u32 timeOnAudioTimer0 = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	m_SpectatorPedInVehicle = false;
	m_SpectatorPedVehicleIsBus = false;
	CPed *spectatorPed = NULL;
	if(NetworkInterface::IsInSpectatorMode() && m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED)
	{
		spectatorPed = CGameWorld::FindFollowPlayer();
		if(spectatorPed && spectatorPed->GetVehiclePedInside())
		{
			m_SpectatorPedInVehicle = spectatorPed->GetVehiclePedInside() == CGameWorld::FindFollowPlayerVehicle();
			m_SpectatorPedVehicleIsBus =  spectatorPed->GetVehiclePedInside()->m_nVehicleFlags.bIsBus;
		}
	}

	bool anyAnimalVocalSoundPlaying = false;
	bool overlappingAnimalSoundPlaying = false;
	if(m_AnimalParams)
	{
		if(m_Parent)
		{
			if(m_Parent->GetDeathState() != DeathState_Dead)
				g_SpeechManager.IncrementAnimalAppearance(m_AnimalType);
			g_SpeechManager.SetIfNearestAnimalInstance(m_AnimalType, m_Parent->GetMatrix().d());
		}

		if(m_DeferredAnimalVocalAnimTriggers.GetCount() > 0)
		{
			const AnimalVocalAnimTrigger* trigger = m_DeferredAnimalVocalAnimTriggers.Pop();
			ReallyPlayAnimalVocalization(trigger);
		}

		if(m_AnimalType == kAnimalRottweiler)
		{
			UpdatePanting(timeOnAudioTimer0);
		}

		anyAnimalVocalSoundPlaying = m_AnimalVocalSound || m_AnimalVocalEchoSound;
		//See if any overlappable animal sounds need updating
		for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS && !overlappingAnimalSoundPlaying; i++)
		{
			if(m_AnimalOverlappableSound[i] != NULL)
				overlappingAnimalSoundPlaying = true;
		}
	}

	//This is our first update after the ped has been fully init'ed, so take care of this stuff.
	if(m_FirstUpdate)
	{
		if(m_Parent)
		{
			PedVoiceGroups* pedVoiceGroupObject = audNorthAudioEngine::GetObject<PedVoiceGroups>(GetPedVoiceGroup());
			m_AmbientVoicePriority = pedVoiceGroupObject ? pedVoiceGroupObject->VoicePriority : 0;
		}
		else
		{
			m_AmbientVoicePriority = 0;
		}
		m_FirstUpdate = false;
	}

	if(m_ShouldResetReplyingPed)
	{
		m_ReplyingPed = NULL;
		m_ShouldResetReplyingPed = false;
	}

	m_IsUnderwater = m_Parent && m_Parent->GetCurrentMotionTask() && m_Parent->GetCurrentMotionTask()->IsUnderWater();

	if(m_IsAngry && m_TimeToStopBeingAngry != 0 && timeOnAudioTimer0 >= m_TimeToStopBeingAngry)
	{
		m_IsAngry = false;
		m_TimeToStopBeingAngry = 0;
	}

	if(m_HasContextToPlaySafely)
	{
		if(atStringHash(m_ContextToPlaySafely) == g_GetWantedLevelContextHash)
			sm_PedToPlayGetWantedLevel = NULL;

		Say(m_ContextToPlaySafely, m_PlaySafelySpeechParams);
		m_ContextToPlaySafely[0] = '\0';
		m_PlaySafelySpeechParams[0] = '\0';
		m_HasContextToPlaySafely = false;
	}

	UpdateFalling(timeOnAudioTimer0);

	if(!IsPainPlaying())
	{
		if(m_IsCoughing)
		{
			m_LungDamageNextCoughTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + (u32)audEngineUtil::GetRandomNumberInRange(g_MinTimeBetweenLungDamageCough, g_MaxTimeBetweenLungDamageCough);
		}
		m_IsCoughing = false;
	}
	
	if(m_Parent && m_Parent->IsLocalPlayer())
	{
		if(sm_PlayerPedType != m_Parent->GetPedType())
		{
			g_SpeechManager.SetPlayerPainRootBankName(*m_Parent);
			sm_PlayerPedType = m_Parent->GetPedType();
		}
		if(m_Parent->GetPlayerInfo())
		{
			const CPlayerPedTargeting& targeting = m_Parent->GetPlayerInfo()->GetTargeting();
			if(targeting.GetTarget() && targeting.GetTarget()->GetType() == ENTITY_TYPE_PED)
			{
				const CPed* targetPed = static_cast<const CPed*>(targeting.GetTarget());
				if(targetPed && targetPed->GetSpeechAudioEntity())
				{
					bool lowHealth = false;
					if(targetPed->GetPedHealthInfo() && targetPed->GetPedHealthInfo()->GetInjuredHealthThreshold() > targetPed->GetHealth())
						lowHealth = true;
					s8 toughness = targetPed->GetSpeechAudioEntity()->GetPedToughnessWithoutSetting();
					if(toughness >= 0)
						g_SpeechManager.RegisterPedAimedAtOrHurt((u8)toughness, lowHealth, false);
				}
			}
		}

		//seems a good place to stick something that should happen once a frame
		sm_NumExplosionPainsPlayedThisFrame = 0;
		sm_NumExplosionPainsPlayedLastFrame = sm_NumExplosionPainsPlayedThisFrame;

		//we're going to only allow 1 ped per frame to check if they are valid to produce a cough/giggle, etc when in the valid time range
		sm_PerformedCoughCheckThisFrame = false;
		if(timeOnAudioTimer0 - sm_LastTimeCoughCheckWasReset > 10000)
		{
			sm_LastTimeCoughCheckWasReset = timeOnAudioTimer0;
		}

		if(g_PlayerSwitch.IsActive())
		{
			m_IsAngry = false;
			m_IsCodeSetAngry = false;
		}

		BANK_ONLY(g_IsPlayerAngry = m_IsAngry || ShouldBeAngry();)

		if(	m_Parent->GetPedIntelligence() &&
			m_Parent->GetPedIntelligence()->GetEventScanner())
		{
			const CInWaterEventScanner& waterScanner = m_Parent->GetPedIntelligence()->GetEventScanner()->GetInWaterEventScanner();
			f32 timeSubmerged = waterScanner.GetTimeSubmerged();

			if(!m_IsUnderwater)
			{
				if(m_HasPreloadedSurfacingBreath)
				{
					if(m_ShouldPlaySurfacingBreath && !m_Parent->IsDead() && m_AmbientSpeechSound)
					{
						//Stop any drowning sounds that might be playing
						g_ScriptAudioEntity.GetSpeechAudioEntity().StopAmbientSpeech();
						//Okay, not technically from script, but this will handle letting anim make the call, as we want lipsync
						PlayPreloadedSpeech_FromScript();
					}
					else if(m_LastPlayedSpeechContextPHash != g_DeathUnderwaterPHash || m_AmbientSpeechSound)
					{
						KillPreloadedSpeech(false);
					}
				}
				
				m_HasPreloadedSurfacingBreath = false;
				m_ShouldPlaySurfacingBreath = false;

				if (m_IsInRapids && timeSubmerged==0 && !GetIsAnySpeechPlaying())
				{
					TriggerPainFromPainId(AUD_PAIN_ID_PAIN_RAPIDS, 0);
				}
			}
			else if(!m_HasPreloadedSurfacingBreath && 
				!(m_Parent && m_Parent->GetPedAudioEntity() && m_Parent->GetPedAudioEntity()->GetFootStepAudio().IsWearingScubaMask())
				&& m_IsMale)
			{
				m_HasPreloadedSurfacingBreath = AUD_SPEECH_SAY_FAILED != Say(g_ComeUpForAirContextPHash, "SPEECH_PARAMS_FORCE_PRELOAD_ONLY",
								g_SpeechManager.GetWaveLoadedPainVoiceFromVoiceType(AUD_PAIN_VOICE_TYPE_PLAYER, m_Parent));
			}
			m_ShouldPlaySurfacingBreath = timeSubmerged > 4.0f;

		}

		UpdatePlayerBreathing();

		// When the player is underwater, we want to filter and attenuate other peds speech provided it's not scripted or frontend.
		// We need to update that in ::UpdateSound() otherwise the game environment metrics will overwrite those values.
		// So grab the LPF and reverb values once here in the IsLocalPlayer ::Update, and then apply in ::UpdateSound for all other speechaudioentities.
		// Due to the SourceReverbDamping, we won't be able to completely saturate with verb, but at least put on what we can.
		UpdateUnderwaterMetrics();
	}

	//for testing strange conversation issues.
	//#if __BANK
	//	if(m_TimeScriptedSpeechStarted != 0 && !m_ScriptedSpeechSound)
	//	{
	//		if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_TimeScriptedSpeechStarted < 60)
	//			naDisplayf("Stopped");
	//		m_TimeScriptedSpeechStarted = 0;
	//	}
	//#endif

#if __BANK
	UpdateDebug();
#endif
	
	//u32 timeInMs = audNorthAudioEngine::GetCurrentTimeInMs();

	if(!m_isMaleSet)
	{
		m_IsMale = (m_Parent && m_Parent->IsMale());
		m_isMaleSet = true;
	}

	if(m_ShouldStopSpeech)
	{
		naSpeechEntDebugf2(this, "Calling StopSpeech() - m_ShouldStopSpeech == true");
		if(GetIsHavingAConversation())
		{
			if(IsScriptedSpeechPlaying())
				g_ScriptAudioEntity.AbortScriptedConversation(false BANK_ONLY(,"should stop flag set to true"));
			else
				g_ScriptAudioEntity.AbortScriptedConversation(true BANK_ONLY(,"should stop flag set to true"));
		}
		StopSpeech();
		m_ShouldStopSpeech = false;
		m_TimeCanBeInConversation = timeOnAudioTimer0 + 500;
	}

	if(m_ShouldSaySpaceRanger)
	{
		if(timeOnAudioTimer0 - sm_TimeOfLastSaySpaceRanger > 25000)
		{
			//Jeff wants the second variation to play more than the first
			s32 variation = audEngineUtil::ResolveProbability(0.75) ? 2 : 1; 
			Say("SPACE_RANGER", "SPEECH_PARAMS_ALLOW_REPEAT", ATSTRINGHASH("SPACE_RANGER", 0x21D80107), -1, NULL, 0, -1, 0.0f, false, &variation);
			sm_TimeOfLastSaySpaceRanger = timeOnAudioTimer0;
		}
		m_ShouldSaySpaceRanger = false;
	}

	if(m_ShouldPlayCycleBreath)
	{
		PlayPlayerBreathing(BREATH_EXHALE_CYCLING);
		m_ShouldPlayCycleBreath = false;
	}

	m_DeathIsFromEnteringRagdoll = false;

	/*if(m_Parent &&
	!m_PainSpeechSound && 
	m_Parent->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir) &&
	m_Parent->GetFallingHeight() > 3.0f &&
	!m_Parent->IsPlayer()
	)
	{
	audDamageStats damageStats;
	damageStats.RawDamage = 0.0f;
	damageStats.Fatal = false;
	damageStats.DamageReason = m_Parent->GetFallingHeight() > g_MinHeightForSuperHighFallContext ? AUD_DAMAGE_REASON_SUPER_FALLING : AUD_DAMAGE_REASON_FALLING;
	InflictPain(damageStats);
	}*/

	if(m_ShouldUpdatePain)
	{
		ReallyInflictPain(m_DamageStats);
	}

	// if we've passed our preload time limit, bin it, so we free up the slot.
	if ( (m_WaitingForAmbientSlot || m_AmbientSpeechSound) && m_PreloadFreeTime != 0 && m_PreloadFreeTime < timeOnAudioTimer0)
	{
		naSpeechEntDebugf2(this, "Calling StopAmbientSpeech() - we've passed our preload time limit");
		StopAmbientSpeech();
	}

#if __BANK
	if(IsScriptedSpeechPlaying() || IsAmbientSpeechPlaying())
		m_LastTimePlayingAnySpeech = timeOnAudioTimer0;
#endif

	if(m_WaitingToPlayCollisionScream && m_TimeToPlayCollisionScream < timeOnAudioTimer0)
	{
		audDamageStats damageStats;
		damageStats.DamageReason = audEngineUtil::ResolveProbability(0.5f) ? AUD_DAMAGE_REASON_SCREAM_TERROR : AUD_DAMAGE_REASON_SCREAM_PANIC;
		ReallyInflictPain(damageStats);
		m_TimeToPlayCollisionScream = 0;
		m_WaitingToPlayCollisionScream = false;
	}

#if GTA_REPLAY
	if(!m_ScriptedSpeechSound && m_ReplayWaveSlotIndex >= 0)
	{
		// clean up replay scripted speech waveslot
		g_ScriptAudioEntity.CleanUpReplayScriptedSpeechWaveSlot(m_ReplayWaveSlotIndex);
		m_ReplayWaveSlotIndex = -1;
	}
#endif

	// Don't update anything if there's nothing playing - esp the GetSpeechAudibilityMetrics(), as this appears to be expensive
	if (m_AmbientSlotId<0 && !GetIsAnySpeechPlaying() && !m_BreathSound)
	{
		m_PlayingSharkPain = false;

		if(m_SpeechSyncId != audMixerSyncManager::InvalidId && audMixerSyncManager::Get())
		{
			audMixerSyncManager::Get()->Release(m_SpeechSyncId);
			m_SpeechSyncId = audMixerSyncManager::InvalidId;
		}
		if(m_ActiveSubmixIndex != -1)
		{
			g_AudioEngine.GetEnvironment().FreeConversationSpeechSubmix(m_ActiveSubmixIndex);
			m_ActiveSubmixIndex = -1;
		}
		if(m_HeadSetSubmixEnvSnd)
		{
			m_HeadSetSubmixEnvSnd->StopAndForget();
		}

		if(m_WasMakingAnimalSoundLastFrame)
			g_SpeechManager.DecrementAnimalsMakingSound(m_AnimalType);

		m_WasMakingAnimalSoundLastFrame = false;

		if(timeOnAudioTimer0 - m_LastTimeFallingVelocityCheckWasSet > 1500)
			m_LastTimeFallingVelocityCheckWasSet = 0;
#if __BANK
		m_FakeGestureNameHash = 0;
#endif
		if(m_Parent)
		{
			if(m_DelayJackingReply)
			{
				if(!m_ReplyingPed || !m_ReplyingPed->GetPedIntelligence())
				{
					m_ReplyingPed = NULL;
					m_DelayJackingReply = false;
				}
				else
				{
					if(m_TimeToPlayJackedReply == 0)
						m_TimeToPlayJackedReply = timeOnAudioTimer0 + g_MinTimeForJackingReply;

					bool isNotBeingJacked = !m_ReplyingPed->IsBeingJackedFromVehicle();
					bool isNotJumpRoll =  !m_ReplyingPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_NM_JUMP_ROLL_FROM_ROAD_VEHICLE);
					bool isNotRagdolling = !m_Parent || !m_Parent->GetUsingRagdoll();

					if(/*m_ReplyingPed->IsLocalPlayer() ||*/ 
						(isNotBeingJacked && isNotRagdolling && isNotJumpRoll && m_TimeToPlayJackedReply < timeOnAudioTimer0)
						)
					{
						TriggerReplySpeech(m_ReplyingPed, m_ReplyingContext, m_ReplyingDelay);
						m_ReplyingPed = NULL;
						m_DelayJackingReply = false;
						m_TimeToPlayJackedReply = 0;
					}
				}
			}

			if(m_Parent->GetPedIntelligence() && m_LastTimeThrownFromVehicle != 0)
			{
				if(timeOnAudioTimer0 - m_LastTimeThrownFromVehicle > g_MaxTimeAfterThrownFromCarToSpeak)
				{
					m_LastTimeThrownFromVehicle = 0;
					m_bIsGettingUpAfterVehicleEjection = false;
				}
				else
				{
					CTask *pTaskSimplest = m_Parent->GetPedIntelligence()->GetTaskActiveSimplest();
					if(!pTaskSimplest)
					{
						m_bIsGettingUpAfterVehicleEjection = false;
					}
					else
					{
						if(pTaskSimplest->GetTaskType()==CTaskTypes::TASK_GET_UP)
							m_bIsGettingUpAfterVehicleEjection = true;
						else if(m_bIsGettingUpAfterVehicleEjection && !m_ReplyingPed && 
								timeOnAudioTimer0 - m_TimeJackedInforWasSet > 10000)
						{
							Say("GENERIC_CURSE_MED");
							m_bIsGettingUpAfterVehicleEjection = false;
						}
					}
					m_LastTimeThrownFromVehicle = 0;
				}
			}

		}
		// If we don't have sounds, we can't be preloading. This should catch all other ways the speech sounds could be killed
		// without explicitly resetting the preload flag
		if (m_Preloading)
		{
			m_Preloading = false;
			m_PreloadFreeTime = 0;
		}
		if(m_WaitingForAmbientSlot)
			m_WaitingForAmbientSlot = false;
		// We're not playing anything, so we're a candidate for laughing, coughing
		if(m_Parent && !m_Parent->IsPlayer() && !m_AnimalParams)
		{
			if(m_IsCrying)
			{
				if(m_TimeToNextCry < timeOnAudioTimer0)
				{
					TriggerSingleCry();
					m_TimeToNextCry = timeOnAudioTimer0 + (u32)audEngineUtil::GetRandomNumberInRange((f32)g_MinTimeBetweenCries, (f32)g_MaxTimeBetweenCries);
				}
			}
			else
			{
				f32 distanceSqToNearestListener = (g_AudioEngine.GetEnvironment().ComputeSqdDistanceRelativeToVolumeListenerV(m_Parent->GetTransform().GetPosition())).Getf();

				if(sm_NumCollisionScreamsToTrigger > 0 && distanceSqToNearestListener > g_MinDistanceSqForRunnedOverScream && 
					distanceSqToNearestListener < g_MaxDistanceSqForRunnedOverScream)
				{
					if(audEngineUtil::ResolveProbability(0.05f))
					{
						m_TimeToPlayCollisionScream = timeOnAudioTimer0 + (u32)audEngineUtil::GetRandomNumberInRange(0.0f, g_RunnedOverScreamPredelayVariance);
						m_WaitingToPlayCollisionScream = true;
						sm_NumCollisionScreamsToTrigger--;
					}
				}
				else if(timeOnAudioTimer0 > sm_TimeOfNextCoughSpeech)
				{
					f32 probabilityOfCoughOrLaugh = 0.05f;
					bool wasPedToughnessSet = m_ToughnessIsSet;
					u8 toughness = GetPedToughness();
					//GetToughness sets the peds voice if it hasn't already been set, as toughness is tagged per voice.  However, we could loop
					//	through this a number of times, never actually using the voice.  This could lead to more voice repetition, so lets go ahead and
					//	set the ambient voice back to 0.  this call will also decrement the reference count on the voice.
					if(!wasPedToughnessSet)
						SetAmbientVoiceName((u32)0);

					if(toughness == AUD_TOUGHNESS_MALE_BUM || toughness == AUD_TOUGHNESS_FEMALE_BUM)
						probabilityOfCoughOrLaugh = 0.2f;

					// Don't do laughs or coughs for peds that are in the same vehicle as us.  Distance checks will fail for large vehicles where camera is far away.
					if(!IsInCarFrontendScene() && audEngineUtil::ResolveProbability(probabilityOfCoughOrLaugh))
					{
						if(!sm_PerformedCoughCheckThisFrame && m_TimeOfLastCoughCheck < sm_LastTimeCoughCheckWasReset)
						{
							sm_PerformedCoughCheckThisFrame = true;
							m_TimeOfLastCoughCheck = timeOnAudioTimer0;

							if (!m_IsMale)
							{
								if (audEngineUtil::ResolveProbability(0.5f))
								{
									// Only do it if they're not right next to us
									if (distanceSqToNearestListener > g_LaughCoughMinDistSq && distanceSqToNearestListener < g_LaughCoughMaxDistSq)
									{
										Say("LAUGH", "SPEECH_PARAMS_ALLOW_REPEAT", ATSTRINGHASH("WAVELOAD_PAIN_FEMALE", 0x0332128ab));
										sm_TimeOfNextCoughSpeech = timeOnAudioTimer0 + audEngineUtil::GetRandomNumberInRange(g_MinTimeBetweenLaughs, g_MaxTimeBetweenLaughs);
									}
								}
								else
								{
									if (distanceSqToNearestListener > g_LaughCoughMinDistSq && distanceSqToNearestListener < g_LaughCoughMaxDistSq)
									{
										audDamageStats damageStats;
										damageStats.DamageReason = AUD_DAMAGE_REASON_COUGH;
										ReallyInflictPain(damageStats);
										sm_TimeOfNextCoughSpeech = timeOnAudioTimer0 + audEngineUtil::GetRandomNumberInRange(g_MinTimeBetweenCoughs, g_MaxTimeBetweenCoughs);
									}
								}
							}
						}
						else
						{
							// Only do it if they're not right next to us
							if (distanceSqToNearestListener > g_LaughCoughMinDistSq && distanceSqToNearestListener < g_LaughCoughMaxDistSq)
							{
								audDamageStats damageStats;
								damageStats.DamageReason = AUD_DAMAGE_REASON_COUGH;
								ReallyInflictPain(damageStats);
								sm_TimeOfNextCoughSpeech = timeOnAudioTimer0 + audEngineUtil::GetRandomNumberInRange(g_MinTimeBetweenCoughs, g_MaxTimeBetweenCoughs);
							}
						}
					}
				}
			}
		}

		return;
	}

	/*if (m_AmbientSlotId>=0 && m_Parent && !m_Parent->GetIsCurrentlyHD())
	{
	FreeSlot();
	return;
	}*/

	if((camInterface::GetCutsceneDirector().IsCutScenePlaying() || g_PlayerSwitch.IsActive()) && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowPainAndAmbientSpeechToPlayDuringCutscene))
	{
		naSpeechEntDebugf2(this, "Calling StopAmbientSpeech() and StopPain() - amInterface::GetCutsceneDirector().IsCutScenePlaying() && !audScriptAudioFlags::AllowPainAndAmbientSpeechToPlayDuringCutscene");
		if(IsAmbientSpeechPlaying())
			StopAmbientSpeech();
		if(IsPainPlaying())
			StopPain();
	}

	bool bInVehicle = m_Parent && m_Parent->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle );
	if(	!m_HasPreloadedSurfacingBreath && m_IsUnderwater && !m_PlayingSharkPain && m_LastPlayedSpeechContextPHash != g_DeathUnderwaterPHash &&
		m_Parent && m_Parent->GetPedIntelligence() &&
		( (m_Parent->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInWater ) && !bInVehicle) || 
		(m_Parent->GetPedConfigFlag( CPED_CONFIG_FLAG_DrownsInSinkingVehicle ) && bInVehicle) )
		)
	{
		FreeSlot(true, true);
	}

	if(m_AnimalParams)
		m_WasMakingAnimalSoundLastFrame = true;

#if !__FINAL
	if(m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED)
	{
		if(m_AlertConversationEntityWhenSoundBegins)
		{
			if(GetActiveScriptedSpeechPlayTimeMs() > 0)
			{
				g_ScriptAudioEntity.SetTimeTriggeredSpeechPlayed(g_AudioEngine.GetTimeInMilliseconds());
				m_AlertConversationEntityWhenSoundBegins = false;
			}
		}
	}
#endif

	audSpeechSound *activeSound = (m_AmbientSpeechSound?m_AmbientSpeechSound:m_ScriptedSpeechSound);	
	if(activeSound && activeSound->GetPlayState() != AUD_SOUND_WAITING_TO_BE_DELETED)
	{
		bool speechIsPreloadedOrPlaying =  activeSound->GetPlayState() == AUD_SOUND_PLAYING || IsPreloadedSpeechReady();
		audWaveSlot* activeSoundWaveSlot = (activeSound == m_AmbientSpeechSound?m_LastPlayedWaveSlot:m_ScriptedSlot);

		if(CheckAndPrepareSpeechSound(activeSound, activeSoundWaveSlot) && !m_GesturesHaveBeenParsed)
		{
			GetAndParseGestureData();
			m_GesturesHaveBeenParsed = true;
		}

		if(m_Parent && m_Parent->GetIsWaitingForSpeechToPreload() && speechIsPreloadedOrPlaying &&
			(m_Parent->m_nDEflags.bFrozen || m_Parent->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEventScanning)))
		{
			PlayPreloadedSpeech(0);
			m_Parent->SetIsWaitingForSpeechToPreload(false);
		}
	}

	// Kick off prepared speech that's not waiting to be triggered via script
	if(m_PlaySpeechWhenFinishedPreparing && IsPreloadedSpeechReady())
	{
		PlayPreloadedSpeech(0);
	}

	if(m_PainSpeechSound && m_CachedSpeechType != AUD_SPEECH_TYPE_PAIN && m_CachedSpeechType != AUD_SPEECH_TYPE_ANIMAL_PAIN)
	{
		StopPain();
	}

	// If we're currently playing a sound, see if it's finished, and free the slot if it has
	if (m_AmbientSlotId>=0)
	{
		const bool hasAllSpeechFinished = !IsAmbientSpeechPlaying() && !IsAnimalVocalizationPlaying() && !IsPainPlaying();

		if (hasAllSpeechFinished && (!m_WaitingForAmbientSlot REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && (m_SuccessValueOfLastSay == AUD_SPEECH_SAY_DEFERRED)))))
		{
			// if we've got some reply speech, synchronously clear out the slot, as we'll get to use it for the reply,
			// and with only three slots, a lot of reply stuff wasn't getting played. Note that the slot will still need to have
			// no waves referencing it. Which, if I check this in, I'll have made sure is usually happening.
			//bool clearOutInstantlyForReplySpeech = false;
			/*if (m_ReplyingPed)
			{
				clearOutInstantlyForReplySpeech = true;
			}*/
			g_SpeechManager.FreeAmbientSpeechSlot(m_AmbientSlotId);
			m_AmbientSlotId = -1;
			m_WaitingForAmbientSlot = false;

			if(m_MultipartContextPHash != 0)
			{
				if(m_MultipartContextCounter > m_MultipartContextNumLines)
				{
					m_MultipartContextCounter = 0;
					m_MultipartContextPHash = 0;
					m_OriginalMultipartContextPHash = 0;
					m_MultipartContextNumLines = 0;
				}
				else
					TriggerReplySpeech(m_Parent, atFinalizeHash(m_MultipartContextPHash), 0);
				
			}
			// See if we're meant to play a reply on another ped
			// Handle jacking up above
			else if (m_ReplyingPed && !m_DelayJackingReply)
			{
				TriggerReplySpeech(m_ReplyingPed, m_ReplyingContext, m_ReplyingDelay);
				m_ReplyingPed = NULL;
			}
			else if(m_LastPlayedSpeechContextPHash == g_SharkAttackContextPHash)
			{
				TriggerSharkAttackSound(NULL, true);
			}
		}

		// HL: Fix for url:bugstar:5850444 - Blackjack - Dealer dialogue went missing after two games for the local player in the Penthouse
		// In the event that we trigger some speech while this audSpeechAudioEntity already has an ambient speech slot allocated, the code 
		// will defer the speech, set the m_WaitingForAmbientSlot flag, and then wait for the same speech slot to become vacant... but it will 
		// never become vacant because we have already allocated it, and it'll never release the slot because the m_WaitingForAmbientSlot flag 
		// is set. This circular logic results in the entity getting stuck and never speaking again. Fix is to add this additional IsSafeToReuseSlot 
		// check, which  will allow deferred speech to play a) if we own the slot already and b) it has no active wave slot references. Also restricting 
		// this to online, non-replay speech to be extra cautious.
		if(m_WaitingForAmbientSlot && (g_SpeechManager.IsSlotVacant(m_AmbientSlotId) || (NetworkInterface::IsGameInProgress() && !CReplayMgr::IsEditModeActive() && hasAllSpeechFinished && m_SuccessValueOfLastSay == AUD_SPEECH_SAY_DEFERRED && g_SpeechManager.IsSafeToReuseSlot(m_AmbientSlotId, this))))
		{
			m_SuccessValueOfLastSay = CompleteSay();

#if GTA_REPLAY
			if(m_SuccessValueOfLastSay != AUD_SPEECH_SAY_FAILED)
			{
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketSpeech>(
						CPacketSpeech(&m_DeferredSayStruct, true, m_PositionForNullSpeaker), m_Parent);
				}
			}
#endif // GTA_REPLAY

			m_WaitingForAmbientSlot = false;
		}

		const bool allowWhileInWater = m_LastPlayedSpeechContextPHash == g_DrowningContextPHash || 
										m_LastPlayedSpeechContextPHash == g_DrowningDeathContextPHash ||
										m_LastPlayedSpeechContextPHash == g_ComeUpForAirContextPHash;
		if(m_Parent && (m_Parent->IsFatallyInjured() || (!allowWhileInWater && !m_PlayingSharkPain && m_IsUnderwater)))
		{
			if(m_AmbientSpeechSound)
				m_AmbientSpeechSound->StopAndForget();
			for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
			{
				if(m_AmbientShadowSound[i])
					m_AmbientShadowSound[i]->StopAndForget();
			}
			if(m_AmbientSpeechSoundEcho)
				m_AmbientSpeechSoundEcho->StopAndForget();
			if(m_AmbientSpeechSoundEcho2)
				m_AmbientSpeechSoundEcho2->StopAndForget();
		}
	}

	const bool inCarAndSceneSaysFrontend = IsInCarFrontendScene();

	// If we're playing a sound that uses the audibility curves, then update our greatest audibility
	if(IsAmbientSpeechPlaying() || IsScriptedSpeechPlaying() || IsPainPlaying())
	{
		const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		const u32 timeSinceHighAudibiliyLine = timeInMs - m_LastHighAudibilityLineTime;
		if(m_CachedVolType != AUD_SPEECH_FRONTEND && !inCarAndSceneSaysFrontend && (m_CachedAudibility >= m_GreatestAudibility || timeSinceHighAudibiliyLine >= g_TimeToSmoothDownFromHighAudibility))
		{
			m_GreatestAudibility = m_CachedAudibility;
			m_LastHighAudibilityLineTime = timeInMs;
		}
	}

	Vector3 position;
	if(!GetPositionForSpeechSound(position, m_CachedVolType))
	{
		naAssertf(false, "Unabled to get a position for non-frontend positioned speech, aborting");
		return;
	}

	if(m_Parent && m_Parent->IsLocalPlayer() && timeOnAudioTimer0 - m_TimeLastDrewSpeechBlip > 1000)
	{
		CMiniMap::CreateSonarBlip(position, m_BlipNoise, HUD_COLOUR_BLUEDARK, m_Parent);
		m_TimeLastDrewSpeechBlip = timeOnAudioTimer0;
	}

	audEchoMetrics echo1, echo2;
	const audSpeechSound* sndEcho1 = GetEchoPrimary();
	echo1.useEcho = sndEcho1 != NULL;
	echo1.pos = sndEcho1 ? sndEcho1->GetRequestedPosition() : Vec3V(V_ZERO);
	const audSpeechSound* sndEcho2 = GetEchoSecondary();
	echo2.useEcho = sndEcho2 != NULL;
	echo2.pos = sndEcho2 ? sndEcho2->GetRequestedPosition() : Vec3V(V_ZERO);

	audSpeechMetrics metrics;
	const bool metricsProcessed = GetSpeechAudibilityMetrics(VECTOR3_TO_VEC3V(position), m_CachedSpeechType, m_CachedVolType, 
															inCarAndSceneSaysFrontend, m_CachedAudibility, metrics, echo1, echo2);
	if(!metricsProcessed)
	{
		naAssertf(false, "Unabled to get speech audibility metrics in ::Update for speech, aborting");
		return;
	}

	// Smoothers work better with linear values since they're closer to a proper x-fade curve than a dB smoother.
	// Grab the smoothed positional volume now because we use it several places
	const f32 volPosLin = audDriverUtil::ComputeLinearVolumeFromDb(metrics.vol[AUD_SPEECH_ROUTE_POS]);
	const f32 smoothedVolPosLin = m_SpeechVolSmoother[AUD_SPEECH_ROUTE_POS].CalculateValue(volPosLin, timeOnAudioTimer0);
	metrics.vol[AUD_SPEECH_ROUTE_POS] = audDriverUtil::ComputeDbVolumeFromLinear(smoothedVolPosLin);

	if(m_UseShadowSounds)
	{
		bool killedSpeech = false;
		UpdateSpeechAndShadowSounds(m_CachedSpeechType, metrics, position, killedSpeech, timeOnAudioTimer0);
		if(killedSpeech)
			return;
	}
	else
	{
		UpdateSpeechSound(m_CachedSpeechType, metrics, position);
	}

	UpdateSpeechEchoes(m_CachedSpeechType, echo1, echo2);

	if(m_AnimalParams)
	{
		for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS; ++i)
		{
			if(m_AnimalOverlappableSound[i])
			{
				m_AnimalOverlappableSound[i]->SetRequestedPosition(position);
				m_AnimalOverlappableSound[i]->SetRequestedHPFCutoff(metrics.hpf);
				m_AnimalOverlappableSound[i]->SetRequestedLPFCutoff(metrics.lpf);
				m_AnimalOverlappableSound[i]->SetRequestedVolume(metrics.vol[AUD_SPEECH_ROUTE_POS]);
			}
		}
	}

	if(m_BreathSound)
	{
		m_BreathSound->SetRequestedHPFCutoff(metrics.hpf);
		m_BreathSound->SetRequestedLPFCutoff(metrics.lpf);
		m_BreathSound->SetRequestedVolume(GetIsAnySpeechPlaying() ? -100.0f : 
			(audDriverUtil::ComputeDbVolumeFromLinear(m_BreathingVolumeLin) + (!m_BreathingOnBike ? metrics.vol[AUD_SPEECH_ROUTE_POS] : metrics.vol[AUD_SPEECH_ROUTE_FE])) );
		m_BreathSound->SetRequestedPosition(position);
	}
}

f32 audSpeechAudioEntity::GetSpeechPitchRatio()
{
	f32 pitchRatio = fwTimer::GetTimeWarpActive();
	// replay marker scaled pitch is handled in northaudioengine on the entire GAME_WORLD cat
	return pitchRatio;
}

bool audSpeechAudioEntity::IsPitchedDuringSlowMo()
{	
	bool isPitched = m_IsPitchedDuringSlowMo;

#if GTA_REPLAY
	// We always pitch speech when a replay marker is affecting speed
	if(CReplayMgr::IsEditModeActive())
	{
		if(CReplayMgr::GetMarkerSpeed() != 1.0f)
		{
			isPitched = true;
		}
	}
#endif

	return isPitched;
}


bool audSpeechAudioEntity::IsMutedDuringSlowMo()
{
	bool isMuted = m_IsMutedDuringSlowMo;

#if GTA_REPLAY
	if(isMuted)
	{
		if(CReplayMgr::IsEditModeActive())
		{
			// We only forcibly unmute speech during replay if a marker is affecting playback speed AND the original 
			// clip was recorded at normal speed. This ensures that we don't get speech appearing in a replay clip
			// that wasn't audible in the original recording (eg. it was muted due to a player ped special ability being active)			
			if(CReplayMgr::GetMarkerSpeed() != 1.0f && fwTimer::GetTimeWarpActive() == 1.0f)          
			{
				isMuted = false;
			}
		}
	}	
#endif

	return isMuted;
}

void audSpeechAudioEntity::UpdateSpeechAndShadowSounds(const audSpeechType speechType, audSpeechMetrics& metrics, const Vector3& pos, bool& killedSpeech, const u32 timeOnAudioTimer0)
{
	audSpeechSound* speechSound = NULL;
	audSound* shadowSound[AUD_NUM_SPEECH_ROUTES];
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		shadowSound[i] = NULL;
	}
	f32 pitchRatio = GetSpeechPitchRatio();

	audSound** srcArray = NULL;
	switch(speechType)
	{
	case AUD_SPEECH_TYPE_AMBIENT:
		{
			speechSound = m_AmbientSpeechSound;
			srcArray = &m_AmbientShadowSound[0];
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && speechSound)
			{
				s32 wavePlayTime = speechSound->GetWavePlaytime();
				// it takes time for the wave to actually start returning valid play positions so we'll use the cached start offset (we are most interested in the first frame so we know what the start offset is for interrupted speech)
				if(wavePlayTime == 0 && m_CachedSpeechStartOffset != 0) 
				{
					wavePlayTime = m_CachedSpeechStartOffset;
				}
				s32 startOffset = m_HasPreparedAndPlayed ? wavePlayTime : -1;
				CReplayMgr::RecordFx<CPacketSpeech>(CPacketSpeech(&m_DeferredSayStruct, true, m_PositionForNullSpeaker,startOffset), m_Parent, m_ReplyingPed);
			}
#endif // GTA_REPLAY
			break;
		}
	case AUD_SPEECH_TYPE_SCRIPTED:
		{
			if(g_ScriptAudioEntity.ShouldTreatNextLineAsUrgent() && g_EnableEarlyStoppingForUrgentSpeech && m_ScriptedSpeechSound)
			{
				u32 playtime = m_ScriptedSpeechSound->GetWavePlaytime();
				u32 length =  m_ScriptedSpeechSound->ComputeDurationMsExcludingStartOffsetAndPredelay(NULL) - m_ScriptedSpeechSound->GetPredelay();
				if(playtime >= length - g_TimeToStopEarlyForUrgentSpeech)
				{
					StopPlayingScriptedSpeech();
					killedSpeech = true;
					return;
				}
			}
			if(!g_ScriptAudioEntity.GetIsConversationControlledByAnimTriggers())
			{
				pitchRatio = 1.0f;
			}
			speechSound = m_ScriptedSpeechSound;
			srcArray = &m_ScriptedShadowSound[0];
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && speechSound)
			{
				if(m_CurrentlyPlayingVoiceNameHash != ATSTRINGHASH("EXEC1_POWERANN", 0x4C49ABC3))
				{
					s32 wavePlayTime = speechSound->GetWavePlaytime();
					// it takes time for the wave to actually start returning valid play positions so we'll use the cached start offset (we are most interested in the first frame so we know what the start offset is for interrupted speech)
					if(wavePlayTime == 0 && m_CachedSpeechStartOffset != 0) 
					{
						wavePlayTime = m_CachedSpeechStartOffset;
					}
					s32 startOffset = m_HasPreparedAndPlayed ? wavePlayTime : -1;
					CReplayMgr::RecordFx<CPacketScriptedSpeechUpdate>(
						CPacketScriptedSpeechUpdate(m_LastPlayedSpeechContextPHash, m_ContextNameStringHash, m_CurrentlyPlayingVoiceNameHash, m_CachedVolType,m_CachedAudibility, m_VariationNum, startOffset, m_ActiveSoundStartTime, fwTimer::GetReplayTimeInMilliseconds(), m_ScriptedSlot->GetSlotIndex(), m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, m_PlaySpeechWhenFinishedPreparing), m_Parent);
				}
			} 
#endif // GTA_REPLAY
			break;
		}
	case AUD_SPEECH_TYPE_PAIN:
	case AUD_SPEECH_TYPE_ANIMAL_PAIN:
		{
			speechSound = m_PainSpeechSound;
			srcArray = &m_PainShadowSound[0];
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && speechSound)
			{
				s32 wavePlayTime = speechSound->GetWavePlaytime();
				// it takes time for the wave to actually start returning valid play positions so we'll use the cached start offset (we are most interested in the first frame so we know what the start offset is for interrupted speech)
				if(wavePlayTime == 0 && m_CachedSpeechStartOffset != 0) 
				{
					wavePlayTime = m_CachedSpeechStartOffset;
				}
				s32 startOffset = m_HasPreparedAndPlayed ? wavePlayTime : -1;
				if(m_DeferredSayStruct.contextPHash != 0 && m_DeferredSayStruct.voiceNameHash != 0)			
				{
					CReplayMgr::RecordFx<CPacketSpeech>(CPacketSpeech(&m_DeferredSayStruct, true, m_PositionForNullSpeaker,startOffset), m_Parent, m_ReplyingPed);
				}
			}
#endif // GTA_REPLAY
			break;
		}
	case AUD_SPEECH_TYPE_ANIMAL_NEAR:
		{
			speechSound = m_AnimalVocalSound;
			srcArray = &m_AnimalVocalShadowSound[0];
			break;
		}
	default:
		{
			break;
		}
	}

	if(speechSound)
	{
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			shadowSound[i] = srcArray[i];
		}

		speechSound->SetRequestedVolume(g_SilenceVolume);
		if(IsPitchedDuringSlowMo())
			speechSound->SetRequestedPitch(audDriverUtil::ConvertRatioToPitch(pitchRatio));
	}

	// Get the smoothed volumes.  We've already done the positional one as we used that elsewhere
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if(i != AUD_SPEECH_ROUTE_POS && shadowSound[i])
		{
			const f32 volLin = audDriverUtil::ComputeLinearVolumeFromDb(metrics.vol[i]);
			const f32 volLinSmoothed = m_SpeechVolSmoother[i].CalculateValue(volLin, timeOnAudioTimer0);
			metrics.vol[i] = audDriverUtil::ComputeDbVolumeFromLinear(volLinSmoothed);
		}
	}

	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if(shadowSound[i])
		{
#if __BANK
			if(g_ShowSpeechPositions)
			{
				CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
				if(pPlayer)
				{
					const Vector3 &playerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
					CVectorMap::DrawLine(playerPosition, pos, Color_yellow, Color_yellow); 
				}
			}
#endif

			f32 volume = 0.0f;
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive() && i==AUD_SPEECH_ROUTE_HS)
			{
				volume = g_FrontendAudioEntity.GetReplayDialogVolume();
			}
#endif
			shadowSound[i]->SetRequestedPosition(pos);
			shadowSound[i]->SetRequestedHPFCutoff(metrics.hpf);
			shadowSound[i]->SetRequestedLPFCutoff(metrics.lpf);
			if(pitchRatio == 1.0f || !IsMutedDuringSlowMo())
				shadowSound[i]->SetRequestedVolume(metrics.vol[i] +volume);
			else
				shadowSound[i]->SetRequestedVolume(g_SilenceVolume);
		}
	}
}

void audSpeechAudioEntity::UpdateSpeechSound(const audSpeechType speechType, const audSpeechMetrics& metrics, const Vector3& pos)
{
	audSpeechSound* speechSound = NULL;
	f32 pitchRatio = GetSpeechPitchRatio();

	switch(speechType)
	{
	case AUD_SPEECH_TYPE_AMBIENT:
		{
			speechSound = m_AmbientSpeechSound;
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && speechSound)
			{
				s32 wavePlayTime = speechSound->GetWavePlaytime();
				// it takes time for the wave to actually start returning valid play positions so we'll use the cached start offset (we are most interested in the first frame so we know what the start offset is for interrupted speech)
				if(wavePlayTime == 0 && m_CachedSpeechStartOffset != 0) 
				{
					wavePlayTime = m_CachedSpeechStartOffset;
				}
				s32 startOffset = m_HasPreparedAndPlayed ? wavePlayTime : -1;
				CReplayMgr::RecordFx<CPacketSpeech>(CPacketSpeech(&m_DeferredSayStruct, true, m_PositionForNullSpeaker,startOffset), m_Parent, m_ReplyingPed);
			}
#endif // GTA_REPLAY
			break;
		}
	case AUD_SPEECH_TYPE_SCRIPTED:
		{
			if(!g_ScriptAudioEntity.GetIsConversationControlledByAnimTriggers())
			{
				pitchRatio = 1.0f;
			}
			speechSound = m_ScriptedSpeechSound;
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && speechSound)
			{
				if(m_CurrentlyPlayingVoiceNameHash != ATSTRINGHASH("EXEC1_POWERANN", 0x4C49ABC3))
				{
					s32 wavePlayTime = speechSound->GetWavePlaytime();
					// it takes time for the wave to actually start returning valid play positions so we'll use the cached start offset (we are most interested in the first frame so we know what the start offset is for interrupted speech)
					if(wavePlayTime == 0 && m_CachedSpeechStartOffset != 0) 
					{
						wavePlayTime = m_CachedSpeechStartOffset;
					}
					s32 startOffset = m_HasPreparedAndPlayed ? wavePlayTime : -1;
					CReplayMgr::RecordFx<CPacketScriptedSpeechUpdate>(
						CPacketScriptedSpeechUpdate(m_LastPlayedSpeechContextPHash, m_ContextNameStringHash, m_CurrentlyPlayingVoiceNameHash, m_CachedVolType,m_CachedAudibility, m_VariationNum, startOffset, m_ActiveSoundStartTime, fwTimer::GetReplayTimeInMilliseconds(), m_ScriptedSlot->GetSlotIndex(), m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, m_PlaySpeechWhenFinishedPreparing), m_Parent);
				}
			}
#endif // GTA_REPLAY
			break;
		}
	case AUD_SPEECH_TYPE_PAIN:
	case AUD_SPEECH_TYPE_ANIMAL_PAIN:
		{
			speechSound = m_PainSpeechSound;
#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord() && speechSound)
			{
				s32 wavePlayTime = speechSound->GetWavePlaytime();
				// it takes time for the wave to actually start returning valid play positions so we'll use the cached start offset (we are most interested in the first frame so we know what the start offset is for interrupted speech)
				if(wavePlayTime == 0 && m_CachedSpeechStartOffset != 0) 
				{
					wavePlayTime = m_CachedSpeechStartOffset;
				}
				s32 startOffset = m_HasPreparedAndPlayed ? wavePlayTime : -1;
				CReplayMgr::RecordFx<CPacketSpeech>(CPacketSpeech(&m_DeferredSayStruct, true, m_PositionForNullSpeaker,startOffset), m_Parent, m_ReplyingPed);
			}
#endif // GTA_REPLAY
			break;
		}
	case AUD_SPEECH_TYPE_ANIMAL_NEAR:
	case AUD_SPEECH_TYPE_ANIMAL_FAR:
		{
			speechSound = m_AnimalVocalSound;
			break;
		}
	default:
		{
			break;
		}
	}

	if(speechSound)
	{
		speechSound->SetRequestedPosition(pos);
		speechSound->SetRequestedHPFCutoff(metrics.hpf);
		speechSound->SetRequestedLPFCutoff(metrics.lpf);
		if(pitchRatio == 1.0f || !IsMutedDuringSlowMo())
			speechSound->SetRequestedVolume(metrics.vol[AUD_SPEECH_ROUTE_POS]);
		else
			speechSound->SetRequestedVolume(g_SilenceVolume);
		if(IsPitchedDuringSlowMo())
			speechSound->SetRequestedPitch(audDriverUtil::ConvertRatioToPitch(pitchRatio));

#if __BANK
		if(g_ShowSpeechPositions)
		{
			CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
			if(pPlayer)
			{
				const Vector3 &playerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
				CVectorMap::DrawLine(playerPosition, pos, Color_blue, Color_blue); 
			}
		}
#endif

	}
}

void audSpeechAudioEntity::UpdateSpeechEchoes(const audSpeechType speechType, const audEchoMetrics& echo1, const audEchoMetrics& echo2)
{
	audSpeechSound* speechEchoSound = NULL;
	audSpeechSound* speechEchoSound2 = NULL;
	f32 pitchRatio = GetSpeechPitchRatio();

	switch(speechType)
	{
	case AUD_SPEECH_TYPE_AMBIENT:
		{
			speechEchoSound = m_AmbientSpeechSoundEcho;
			speechEchoSound2 = m_AmbientSpeechSoundEcho2;
			break;
		}
	case AUD_SPEECH_TYPE_SCRIPTED:
		{
			if(!g_ScriptAudioEntity.GetIsConversationControlledByAnimTriggers())
			{
				pitchRatio = 1.0f;
			}
			speechEchoSound = m_ScriptedSpeechSoundEcho;
			speechEchoSound2 = m_ScriptedSpeechSoundEcho2;
			break;
		}
	case AUD_SPEECH_TYPE_PAIN:
	case AUD_SPEECH_TYPE_ANIMAL_PAIN:
		{
			speechEchoSound = m_PainEchoSound;
			break;
		}
	case AUD_SPEECH_TYPE_ANIMAL_NEAR:
	case AUD_SPEECH_TYPE_ANIMAL_FAR:
		{
			speechEchoSound = m_AnimalVocalEchoSound;
			break;
		}
	default:
		{
			break;
		}
	}

	if(speechEchoSound && naVerifyf(echo1.useEcho, "We have a primary echo sound but we're not set to use it"))
	{
		if(pitchRatio == 1.0f || !IsMutedDuringSlowMo())
			speechEchoSound->SetRequestedVolume(echo1.vol);
		else
			speechEchoSound->SetRequestedVolume(g_SilenceVolume);
		speechEchoSound->SetRequestedLPFCutoff(echo1.lpf);
		speechEchoSound->SetRequestedHPFCutoff(echo1.hpf);
		speechEchoSound->SetRequestedPitch(audDriverUtil::ConvertRatioToPitch(pitchRatio));
	}

	if(speechEchoSound2 && naVerifyf(echo2.useEcho, "We have a secondary echo sound but we're not set to use it"))
	{
		if(pitchRatio == 1.0f || !IsMutedDuringSlowMo())
			speechEchoSound2->SetRequestedVolume(echo2.vol);
		else
			speechEchoSound2->SetRequestedVolume(g_SilenceVolume);
		speechEchoSound2->SetRequestedLPFCutoff(echo2.lpf);
		speechEchoSound2->SetRequestedHPFCutoff(echo2.hpf);
		speechEchoSound2->SetRequestedPitch(audDriverUtil::ConvertRatioToPitch(pitchRatio));
	}
}

void audSpeechAudioEntity::UpdateSound(audSound* sound, audRequestedSettings* reqSets, u32 UNUSED_PARAM(timeInMs))
{
	if(sound && reqSets)
	{
		const bool inCarAndSceneSaysFrontend = IsInCarFrontendScene();
		if(m_Parent && !m_Parent->IsLocalPlayer() && m_CachedSpeechType != AUD_SPEECH_TYPE_SCRIPTED && m_CachedVolType != AUD_SPEECH_FRONTEND && !inCarAndSceneSaysFrontend && m_AnimalType != kAnimalWhale && m_AnimalType != kAnimalDolphin && m_AnimalType != kAnimalOrca)
		{
			const u32 currentLPF = reqSets->GetLowPassFilterCutoff();
			reqSets->SetLowPassFilterCutoff(Min(currentLPF, (u32)sm_UnderwaterLPF));
			const f32 currentReverbSmall = reqSets->GetEnvironmentGameMetric().GetReverbSmall();
			reqSets->GetEnvironmentGameMetric().SetReverbSmall(Max(currentReverbSmall, sm_UnderwaterReverb));
		}

		if(sound == m_ScriptedSpeechSoundEcho || sound == m_ScriptedSpeechSoundEcho2 
			|| sound == m_AmbientSpeechSoundEcho || sound == m_AmbientSpeechSoundEcho2
			|| sound == m_PainEchoSound
			|| sound == m_AnimalVocalEchoSound)
		{
			const f32 builtUpFactor = audNorthAudioEngine::GetGtaEnvironment()->GetBuiltUpFactorForDirection(AUD_AMB_DIR_LOCAL);
			const f32 reverbWetMed = sm_BuiltUpToEchoWetMedium.CalculateValue(builtUpFactor);
			const f32 reverbWetLarge = sm_BuiltUpToEchoWetLarge.CalculateValue(builtUpFactor);
			reqSets->GetEnvironmentGameMetric().SetReverbMedium(reverbWetMed);
			reqSets->GetEnvironmentGameMetric().SetReverbLarge(reverbWetLarge);
			reqSets->GetEnvironmentGameMetric().SetDryLevel(g_EchoDryLevel);
		}
#if __BANK
		else if(g_EchoSolo)
		{
			sound->SetRequestedVolume(g_SilenceVolume);
		}
#endif

		// We do this here because ::UpdateSound comes after UpdateEnvironmentGameMetric(), so we can override per sound environmental metrics
		if(sound == m_ScriptedShadowSound[AUD_SPEECH_ROUTE_VERB_ONLY]
			|| sound == m_AmbientShadowSound[AUD_SPEECH_ROUTE_VERB_ONLY]
			|| sound == m_PainShadowSound[AUD_SPEECH_ROUTE_VERB_ONLY]
			|| sound == m_AnimalVocalShadowSound[AUD_SPEECH_ROUTE_VERB_ONLY])
		{
			reqSets->GetEnvironmentGameMetric().SetDryLevel(0.0f);
		}
	}
}

void audSpeechAudioEntity::TriggerReplySpeech(CPed* replyingPed, u32 replyingContext, s32 delay)
{
	naSpeechEntDebugf2(this, "TriggerReplySpeech() replyingContext: %u", replyingContext);

	naCErrorf(replyingPed, "Null Cped* passed through as parameter to TriggerReplySpeech: a replying ped must be specified");
	if (!replyingPed)
	{
		return;
	}

	replyingPed->NewSay(replyingContext, 0, true, false, delay, m_Parent);
}

const audSoundSet* audSpeechAudioEntity::GetSoundSetForAmbientSpeech(const bool isPain) const
{
	const audSoundSet* soundSet = NULL;
	if(isPain)
	{
		soundSet = &g_PainSoundSet[m_WaveLoadedPainType];
	}
	else
	{
		soundSet = &g_AmbientSoundSet;
	}

#if __BANK
	if(g_ShouldOverrideSpeechType && m_IsDebugEntity && naVerifyf(g_SpeechTypeOverride < AUD_NUM_SPEECH_TYPES, "Invalid g_SpeechTypeOverride - %u", g_SpeechTypeOverride))
	{
		switch(g_SpeechTypeOverride)
		{
		case AUD_SPEECH_TYPE_INVALID:
		case AUD_SPEECH_TYPE_BREATHING:
		case AUD_SPEECH_TYPE_AMBIENT:
			soundSet = &g_AmbientSoundSet;
			break;
		case AUD_SPEECH_TYPE_SCRIPTED:
			soundSet = &g_ScriptedSoundSet;
			break;
		case AUD_SPEECH_TYPE_PAIN:
			soundSet = &g_PainSoundSet[AUD_PAIN_ID_PAIN_MEDIUM_GENERIC];
			break;
		case AUD_SPEECH_TYPE_ANIMAL_PAIN:
			soundSet = &g_AnimalPainSoundSet;
			break;
		case AUD_SPEECH_TYPE_ANIMAL_NEAR:
			soundSet = &g_AnimalNearSoundSet;
			break;
		case AUD_SPEECH_TYPE_ANIMAL_FAR:
			soundSet = &g_AnimalFarSoundSet;
			break;
		default:
			naAssertf(false, "Shouldn't have a default case when debugging speech - %u", g_SpeechVolumeTypeOverride);
			break;
		}
	}
#endif

	return soundSet;
}

void audSpeechAudioEntity::ComputeAmountToUsePreviousHighAudibility(const audSpeechVolume volType, const audConversationAudibility audibility)
{
	m_AmountToUsePreviousHighAudibility = 0.0f;
	if(volType != AUD_SPEECH_FRONTEND && audibility < m_GreatestAudibility)
	{
		const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		const u32 timeSinceHighAudibiliyLine = timeInMs - m_LastHighAudibilityLineTime;
		if(timeSinceHighAudibiliyLine < g_TimeToSmoothDownFromHighAudibility)
		{
			m_AmountToUsePreviousHighAudibility = 1.0f - ((f32)timeSinceHighAudibiliyLine / (f32)g_TimeToSmoothDownFromHighAudibility);
		}
	}
}

bool audSpeechAudioEntity::GetSpeechAudibilityMetrics(const Vec3V& speakerPos, const audSpeechType speechType, const audSpeechVolume volType, const bool isInCarFEScene, 
														const audConversationAudibility audibility, audSpeechMetrics& metrics, audEchoMetrics& echo1, audEchoMetrics& echo2)
{
#if __BANK
	if (g_DebugVolumeMetrics)
	{
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			m_SpeechVolSmoother[i].SetRate(g_SpeechLinVolumeSmoothRate);
		}
		m_SpeechLinLPFSmoother.SetRate(g_SpeechFilterSmoothRate);
		m_SpeechLinHPFSmoother.SetRate(g_SpeechFilterSmoothRate);
	}
#endif

	if(naVerifyf(speechType < AUD_NUM_SPEECH_TYPES, "Passed invalid audSpeechType (%u) when getting audibility metrics", speechType)
		&& naVerifyf(volType < AUD_NUM_SPEECH_VOLUMES, "Passed invalid audSpeechVolume (%u) when getting audibility metrics", volType)
		&& naVerifyf(audibility < AUD_NUM_AUDIBILITIES, "Passed invalid audConversationAudibility (%u) when getting audibility metrics", audibility))
	{

		bool inCachedBJVehicle = false;
		CVehicle* cachedBJVehicle = audPedAudioEntity::GetBJVehicle();
		if(!isInCarFEScene && cachedBJVehicle)
		{
			inCachedBJVehicle = true;
		}

		if(volType != AUD_SPEECH_FRONTEND && !isInCarFEScene && !inCachedBJVehicle)
		{
			// We need the listSpeaker and speakerList normal in a few spots, so just grab it once here to avoid unneeded sqrt's
			Vec3V listToSpeaker = speakerPos - g_AudioEngine.GetEnvironment().GetPanningListenerPosition();
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				listToSpeaker = speakerPos - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(1);
			}
#endif
			const f32 distanceToPed = Mag(listToSpeaker).Getf();

			Vec3V listToSpeakerNorm(V_ZERO);
			if(distanceToPed != 0.0f)
			{
				listToSpeakerNorm = InvScale(listToSpeaker, ScalarV(distanceToPed));
			}		

			// See if we're in a vehicle, and adjust filter params accordingly.
			f32 opennessVol;
			f32 opennessLPF;
			ComputeVehicleOpennessMetrics(volType, opennessVol, opennessLPF);

			// Now check for the speaking ped's orientation, and cone their voice accordingly.  So attenuate/filter if they're facing away from you.
			f32 speakerConeVol;
			f32 speakerConeLPF;
			ComputeSpeakerVolumeConeMetrics(listToSpeakerNorm, volType, distanceToPed, speakerConeVol, speakerConeLPF);

			// Attenuate ped speech when they're right behind the microphone, otherwise it sounds strange. So attenuate/filter if the speaking ped is behind you.
			f32 listenerConeVol;
			f32 listenerConeLPF;
			ComputeListenerVolumeConeMetrics(listToSpeakerNorm, speechType, volType, distanceToPed, listenerConeVol, listenerConeLPF);

			// Find the volume and filtering due to distanceToPed based on the volumeType and audibility custom curves
			// as we don't do any distance attenuation via rolloff in the environment_game update
			f32 distanceVol;
			f32 distanceHPFLin;
			ComputeDistanceMetrics(speechType, volType, audibility, distanceToPed, distanceVol, distanceHPFLin);

			// Do volume offsets based on the audibility and the volume type
			const f32 audibilityVolOffset = g_AudibilityVolOffset[audibility];
			const f32 volTypeVolOffset = g_VolTypeVolOffset[volType];

			// We want the player's ambient VO to be louder when he's on foot
			f32 playerOnFootVol = 0.0f;
			if(speechType == AUD_SPEECH_TYPE_AMBIENT && m_Parent && m_Parent->IsLocalPlayer())
			{
				playerOnFootVol += g_PlayerOnFootAmbientVolumeOffset;
			}

			// Get a total volume for the sound, which we'll then split into positional and front end with the bubble metrics
			const f32 volTotal = opennessVol + speakerConeVol + listenerConeVol + distanceVol + audibilityVolOffset + volTypeVolOffset + playerOnFootVol;

			// We transition between positional and frontend based on a "bubble" around the listener position
			// So take the volTotal, and split that into a positioned and frontend volume.
			// Use an equal power crossfade curve when splitting so we retain the perceived volume (volTotal) from our previous metrics.
			ComputeSpeechFEBubbleMetrics(listToSpeakerNorm, speechType, volType, distanceToPed, volTotal, 
										metrics.vol[AUD_SPEECH_ROUTE_POS], metrics.vol[AUD_SPEECH_ROUTE_FE]);

			// For now, if the sound is positioned, use reverb for the FE sound so we're not dropping the reverb just because you're in the bubble
			metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY] = metrics.vol[AUD_SPEECH_ROUTE_FE];

			// Fade out the the headset volume with the positional volume as you get closer to the ped that's speaking
			metrics.vol[AUD_SPEECH_ROUTE_HS] = ComputePositionedHeadsetVolume(m_Parent, speechType, (m_Parent && m_Parent->m_nDEflags.bFrozen ? 1000.f : distanceToPed));

			const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

			// Convert to pitch-linear from hZ freq when smoothing otherwise we'll be linearly smoothing an exponential value.
			const f32 linLPF = audDriverUtil::ComputeLinearFromHzFrequency(Min(opennessLPF, speakerConeLPF, listenerConeLPF));
			const f32 smoothedLinLPF = m_SpeechLinLPFSmoother.CalculateValue(linLPF, timeInMs);
			metrics.lpf = audDriverUtil::ComputeHzFrequencyFromLinear(smoothedLinLPF);

			const f32 smoothedLinHPF = m_SpeechLinHPFSmoother.CalculateValue(distanceHPFLin);
			metrics.hpf = audDriverUtil::ComputeHzFrequencyFromLinear(smoothedLinHPF);

			// Get a base echo volume so we can keep keep the echoes' volume proportional to the source before doing speaker->echo->list distance metrics
			// Don't use the coning/speakerPos metrics as we'll get those again with the echo's position
			// Also factor in how much volume we've lost due to the speech FE bubble
			const f32 volPosSpeechBubbleAtten = metrics.vol[AUD_SPEECH_ROUTE_POS] - volTotal;
			const f32 volEchoBase = opennessVol + distanceVol + audibilityVolOffset + volTypeVolOffset + volPosSpeechBubbleAtten;
			if(echo1.useEcho)
			{
				ComputeEchoMetrics(speakerPos, speechType, volType, volEchoBase, metrics.hpf, echo1);
			}
			if(echo2.useEcho)
			{
				ComputeEchoMetrics(speakerPos, speechType, volType, volEchoBase, metrics.hpf, echo2);
			}
#if __BANK
			if(g_ShowSpeechRoute && m_ActiveSpeechIsHeadset)
			{
				naDisplayf("%s POSITIONED AUDIO dist(%f) incar(%d) headset(%d) headsetvol(%f) ismobile(%d)", g_SpeechTypeNames[speechType], distanceToPed, isInCarFEScene, m_ActiveSpeechIsHeadset, metrics.vol[AUD_SPEECH_ROUTE_HS], g_ScriptAudioEntity.IsScriptedConversationAMobileCall());
			}
#endif

#if __BANK
			if(g_DebugVolumeMetrics && m_IsDebugEntity)
			{
				char buf[255];
				s32 line = 0;
				formatf(buf, "SpeechType - %s", g_SpeechTypeNames[speechType]);
				grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
				formatf(buf, "VolType - %s", g_SpeechVolNames[volType]);
				grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
				formatf(buf, "Audibility - %s", g_ConvAudNames[audibility]);
				grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
				formatf(buf, "Openness - Vol: %.02f LPF: %.0f", opennessVol, opennessLPF);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Speaker Cone - Vol: %.02f LPF: %.0f", speakerConeVol, speakerConeLPF);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Listener Cone - Vol: %.02f LPF: %.0f", listenerConeVol, listenerConeLPF);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Distance - Vol: %.02f HPF: %.0f", distanceVol, audDriverUtil::ComputeHzFrequencyFromLinear(distanceHPFLin));
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Vol Offsets - audibility: %.02f volType: %.02f", audibilityVolOffset, volTypeVolOffset);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Vol Final: volTotal: %.02f volPos: %.02f volFE: %.02f volFEVerb: %.02f volHS: %.02f", 
						volTotal, metrics.vol[AUD_SPEECH_ROUTE_POS], metrics.vol[AUD_SPEECH_ROUTE_FE], metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY], metrics.vol[AUD_SPEECH_ROUTE_HS]);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Filter Final - lpf: %.0f hpf: %.0f", metrics.lpf, metrics.hpf);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Echo1 - %s vol: %.02f lpf: %.0f hpf: %.0f", echo1.useEcho ? "OFF" : "ON", echo1.vol, echo1.lpf, echo1.hpf);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
				formatf(buf, "Echo2 - %s vol: %.02f lpf: %.0f hpf: %.0f", echo2.useEcho ? "OFF" : "ON", echo2.vol, echo2.lpf, echo2.hpf);
				grcDebugDraw::Text(speakerPos, Color_white, 0, line += 10, buf);
			}
#endif
		}
		else
		{
			for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
			{
				metrics.vol[i] = g_SilenceVolume;
			}

			// If we're playing the speech through the headset, then silence everything else as we don't want reverb
			if(speechType == AUD_SPEECH_TYPE_SCRIPTED && m_ActiveSpeechIsHeadset)
			{
				if(isInCarFEScene) 
				{
					metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY] = ComputeAmountToUseEnvironmentForFESpeech();
					BANK_ONLY(if(g_ShowSpeechRoute) naDisplayf("%s AUD_SPEECH_ROUTE_FE1 incar(%d) headset(%d) ismobile(%d)", g_SpeechTypeNames[speechType], isInCarFEScene, m_ActiveSpeechIsHeadset, g_ScriptAudioEntity.IsScriptedConversationAMobileCall()));
					metrics.vol[AUD_SPEECH_ROUTE_FE] = 0.0f;
				}
				else
				{
					BANK_ONLY(if(g_ShowSpeechRoute) naDisplayf("%s AUD_SPEECH_ROUTE_HS headset(%d) ismobile(%d)", g_SpeechTypeNames[speechType], m_ActiveSpeechIsHeadset, g_ScriptAudioEntity.IsScriptedConversationAMobileCall()));
					metrics.vol[AUD_SPEECH_ROUTE_HS] = 0.0f;
				}

			}
			else if(isInCarFEScene || inCachedBJVehicle)
			{
				// If it's an inCarFEScene, then figure out how much to apply to the positional reverb only route so we can have reverb in tunnels
				// while still playing the speech front end
				if(g_RoutePhoneCallsThroughPadSpeaker
					&& g_AudioEngine.GetEnvironment().GetPadSpeakerEnabled()
					REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) // No pad speaker in replay
					&& ( g_ScriptAudioEntity.IsScriptedConversationAMobileCall() || m_ActiveSpeechIsPadSpeaker )
					&& !(m_Parent && m_Parent->IsLocalPlayer() ? true : false) && !IsPedInPlayerCar(m_Parent))
				{
					BANK_ONLY(if(g_ShowSpeechRoute) naDisplayf("%s AUD_SPEECH_ROUTE_PAD2 incar(%d) ismobile(%d)", g_SpeechTypeNames[speechType], isInCarFEScene, g_ScriptAudioEntity.IsScriptedConversationAMobileCall()));
					metrics.vol[AUD_SPEECH_ROUTE_PAD] = 0.0f;
				}
				else
				{
					BANK_ONLY(if(g_ShowSpeechRoute) naDisplayf("%s AUD_SPEECH_ROUTE_FE2 incar(%d) ismobile(%d) isPlayer(%d) IsInPlayerCar(%d)", g_SpeechTypeNames[speechType], isInCarFEScene, g_ScriptAudioEntity.IsScriptedConversationAMobileCall(),	m_Parent->IsPlayer(), IsPedInPlayerCar(m_Parent)));
					metrics.vol[AUD_SPEECH_ROUTE_FE] = 0.0f;
					metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY] = ComputeAmountToUseEnvironmentForFESpeech();
				}
				
			}
			else
			{
				// If ped is on the phone and not in players car, then play through the pad speaker
				if(g_RoutePhoneCallsThroughPadSpeaker
					&& g_AudioEngine.GetEnvironment().GetPadSpeakerEnabled()
					REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) // No pad speaker in replay
					&& speechType == AUD_SPEECH_TYPE_SCRIPTED 
					&& (g_ScriptAudioEntity.IsScriptedConversationAMobileCall() || m_ActiveSpeechIsPadSpeaker)
					/*&& IsEqualAll(speakerPos, Vec3V(V_ZERO))*/
					&& (!IsPedInPlayerCar(m_Parent) || m_ActiveSpeechIsPadSpeaker)
					&& !(m_Parent && m_Parent->IsLocalPlayer() ? true : false))
				{
					// all phone conv now goes through pad, but not for player
					BANK_ONLY(if(g_ShowSpeechRoute) naDisplayf("%s AUD_SPEECH_ROUTE_PAD3 ismobile(%d)", g_SpeechTypeNames[speechType], g_ScriptAudioEntity.IsScriptedConversationAMobileCall()));
					metrics.vol[AUD_SPEECH_ROUTE_PAD] = 0.0f;
					metrics.vol[AUD_SPEECH_ROUTE_FE] = g_SilenceVolume;
				}
				else
				{
					BANK_ONLY(if(g_ShowSpeechRoute) naDisplayf("%s AUD_SPEECH_ROUTE_FE4 ismobile(%d)", g_SpeechTypeNames[speechType], g_ScriptAudioEntity.IsScriptedConversationAMobileCall()));
					metrics.vol[AUD_SPEECH_ROUTE_PAD] = g_SilenceVolume;
					metrics.vol[AUD_SPEECH_ROUTE_FE] = 0.0f;
				}
			}

#if !__FINAL
			if(m_IsPlaceholder)
			{
				metrics.vol[AUD_SPEECH_ROUTE_FE] += metrics.vol[AUD_SPEECH_ROUTE_FE] == g_SilenceVolume ? 0.0f : g_VolumeIncreaseForPlaceholder;
				metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY] += metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY] == g_SilenceVolume ? 0.0f : g_VolumeIncreaseForPlaceholder;
				metrics.vol[AUD_SPEECH_ROUTE_HS] += metrics.vol[AUD_SPEECH_ROUTE_HS] == g_SilenceVolume ? 0.0f : g_VolumeIncreaseForPlaceholder;
			}
#endif

#if __BANK
			if(g_DebugVolumeMetrics && m_IsDebugEntity)
			{
				char buf[255];
				s32 line = 0;
				formatf(buf, "SpeechType - %s", g_SpeechTypeNames[speechType]);
				grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
				formatf(buf, "VolType - %s", g_SpeechVolNames[volType]);
				grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
				formatf(buf, "Audibility - %s", g_ConvAudNames[audibility]);
				grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
				formatf(buf, "Final - volPos: %.02f volFE: %.02f volFEVerb: %.02f volHS: %.02f lpf: %.0f hpf: %.0f", 
					metrics.vol[AUD_SPEECH_ROUTE_POS], metrics.vol[AUD_SPEECH_ROUTE_FE], metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY], metrics.vol[AUD_SPEECH_ROUTE_HS], metrics.lpf, metrics.hpf);
				grcDebugDraw::Text(speakerPos, Color_green, 0, line += 10, buf);
			}
#endif
		}

		if(g_PlayerSwitch.IsActive() && 
			(speechType == AUD_SPEECH_TYPE_AMBIENT ||
			speechType == AUD_SPEECH_TYPE_PAIN ||
			speechType == AUD_SPEECH_TYPE_ANIMAL_PAIN ||
			speechType == AUD_SPEECH_TYPE_ANIMAL_NEAR ||
			speechType == AUD_SPEECH_TYPE_ANIMAL_FAR))
		{
			metrics.vol[AUD_SPEECH_ROUTE_POS] = g_SilenceVolume;
			metrics.vol[AUD_SPEECH_ROUTE_FE] = g_SilenceVolume;
			metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY] = g_SilenceVolume;
			metrics.vol[AUD_SPEECH_ROUTE_HS] = g_SilenceVolume;
		}

#if __BANK
		static bool showDebugMetrics = false;
		if(showDebugMetrics)
		{
			char buf[255];
			s32 line = 0;
			formatf(buf, "SpeechType - %s", g_SpeechTypeNames[speechType]);
			grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
			formatf(buf, "VolType - %s", g_SpeechVolNames[volType]);
			grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
			formatf(buf, "Audibility - %s", g_ConvAudNames[audibility]);
			grcDebugDraw::Text(speakerPos, Color_blue, 0, line += 10, buf);
			formatf(buf, "Final - POS: %.02f FE: %.02f VERB: %.02f HS: %.02f PAD: %.02f lpf: %.0f hpf: %.0f", 
				metrics.vol[AUD_SPEECH_ROUTE_POS], metrics.vol[AUD_SPEECH_ROUTE_FE], metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY], metrics.vol[AUD_SPEECH_ROUTE_HS], metrics.vol[AUD_SPEECH_ROUTE_PAD], metrics.lpf, metrics.hpf);
			grcDebugDraw::Text(speakerPos, Color_green, 0, line += 10, buf);
		}
#endif

		return true;
	}

	return false;
}

bool audSpeechAudioEntity::GetPositionForSpeechSound(Vector3& position, audSpeechVolume volType, const Vector3& posForNullSpeaker, CEntity* entForNullSpeaker)
{
	position.Zero();

	if(!posForNullSpeaker.IsEqual(ORIGIN))
	{
		m_PositionForNullSpeaker.Set(posForNullSpeaker);
		m_EntityForNullSpeaker = NULL;
	}
	else if(entForNullSpeaker)
	{
		m_EntityForNullSpeaker = entForNullSpeaker;
		m_PositionForNullSpeaker.Set(ORIGIN);
	}
	else if(!m_Parent)
	{
		volType = AUD_SPEECH_FRONTEND;
	}

	if(!m_PositionForNullSpeaker.IsEqual(ORIGIN))
	{
		position.Set(m_PositionForNullSpeaker);
	}
	else if(m_EntityForNullSpeaker)
	{
		position.Set(VEC3V_TO_VECTOR3(m_EntityForNullSpeaker->GetMatrix().d()));
	}
	else if(m_Parent)
	{
		// Update the head matrix regardless of whether we're using the occlusion path data because we still want a forward vector for coning
		const bool matrixUpdated = UpdatePedHeadMatrix();
		if(m_Parent->GetAudioEnvironmentGroup() && m_Parent->GetAudioEnvironmentGroup()->GetEnvironmentGameMetric().GetUseOcclusionPath())
		{
			position = VEC3V_TO_VECTOR3(m_Parent->GetAudioEnvironmentGroup()->GetEnvironmentGameMetric().GetOcclusionPathPosition());
		}
		else if(matrixUpdated)
		{
			position.Set(m_PedHeadMatrix.d);
		}
	}

	if(position.IsZero() && volType != AUD_SPEECH_FRONTEND)
	{
		naAssertf(false, "Unable to get a position for non-frontend positioned speech, aborting");
		return false;
	}

	return true;
}

bool audSpeechAudioEntity::UpdatePedHeadMatrix()
{
	if(m_Parent)
	{
		// Find the ped's head matrix
		if(m_Parent->GetIsVisibleInSomeViewportThisFrame())
		{
			m_Parent->GetBoneMatrixCached(m_PedHeadMatrix, BONETAG_HEAD);
		}
		else
		{
			// Got a couple reports of FPIsFinite assert.  Callstack and .cod seem to point here.  Maybe a little spew will help.
			// We don't want to play speech from a ped without a finite matrix anyway, so gracefully early out.
			const bool isFinite = (FPIsFinite(m_Parent->GetMatrix().GetCol0().GetXf()) & FPIsFinite(m_Parent->GetMatrix().GetCol0().GetYf()) & FPIsFinite(m_Parent->GetMatrix().GetCol0().GetZf()) &
				FPIsFinite(m_Parent->GetMatrix().GetCol1().GetXf()) & FPIsFinite(m_Parent->GetMatrix().GetCol1().GetYf()) & FPIsFinite(m_Parent->GetMatrix().GetCol1().GetZf()) &
				FPIsFinite(m_Parent->GetMatrix().GetCol2().GetXf()) & FPIsFinite(m_Parent->GetMatrix().GetCol2().GetYf()) & FPIsFinite(m_Parent->GetMatrix().GetCol2().GetZf()) &
				FPIsFinite(m_Parent->GetMatrix().GetCol3().GetXf()) & FPIsFinite(m_Parent->GetMatrix().GetCol3().GetYf()) & FPIsFinite(m_Parent->GetMatrix().GetCol3().GetZf()) );

			if(!isFinite)
			{
				ASSERT_ONLY(const PedVoiceGroups* pvg = audNorthAudioEngine::GetObject<PedVoiceGroups>(GetPedVoiceGroup());)
				Assertf(0, "Non-finite matrix handed to GetSpeechAudibilityMetrics.");
				Assertf(0, "m_CurrentlyPlayingSpeechContext %s", m_CurrentlyPlayingSpeechContext);
				Assertf(0, "PVG %s", pvg ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(pvg->NameTableOffset) : "NULL");
				Assertf(0, "m_AmbientVoiceNameHash %u", m_AmbientVoiceNameHash);
				return false;
			}

			m_PedHeadMatrix = MAT34V_TO_MATRIX34(m_Parent->GetMatrix());
		}
	}

	return true;
}

void audSpeechAudioEntity::ComputeSpeechFEBubbleMetrics(const Vec3V& listToSpeakerNorm, const audSpeechType speechType, const audSpeechVolume volType, 
														const f32 distanceToPed, const f32 volTotal, f32& volPos, f32& volFE) const
{
	volPos = volTotal;
	volFE = g_SilenceVolume;

	// Based on how much the speaker is within the bubble, use an equal power curve to fade the speech from positional to front end
	// "Ambient/normal" volume on scripted speech means it's probably replacing AI context VO, which sounds bad FE,
	// as it can affect gameplay (need to know location of the guy shooting you)
	if(!g_DisableFESpeechBubble 
		&& speechType == AUD_SPEECH_TYPE_SCRIPTED
		&& volType != AUD_SPEECH_FRONTEND 
		&& volType != AUD_SPEECH_NORMAL
		&& !IsEqualAll(listToSpeakerNorm, Vec3V(V_ZERO)))
	{
		const Vec3V forward = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1();
		const f32 cosAngle = Dot(forward, listToSpeakerNorm).Getf();
		const f32 angle = AcosfSafe(cosAngle);
		const f32 angleDegrees = angle * RtoD;

		// This curve allows us to skew how much x-fading to front end we do when the speaker is in front of us.
		// So we can make any sort of oval-shape etc. that we want with these curves.
		const f32 distInner = sm_FEBubbleAngleToDistanceInner.CalculateValue(angleDegrees);
		const f32 distOuter = sm_FEBubbleAngleToDistanceOuter.CalculateValue(angleDegrees);
		const f32 bubbleRatio = RampValueSafe(distanceToPed, Min(distInner, distOuter), Max(distInner, distOuter), 1.0f, 0.0f);

		const f32 volPosStrength = sm_EqualPowerFallCrossFade.CalculateValue(bubbleRatio);
		const f32 volFEStrength = sm_EqualPowerRiseCrossFade.CalculateValue(bubbleRatio);

		const f32 volTotalLin = audDriverUtil::ComputeLinearVolumeFromDb(volTotal);

		volPos = audDriverUtil::ComputeDbVolumeFromLinear(volTotalLin * volPosStrength);
		volFE = audDriverUtil::ComputeDbVolumeFromLinear(volTotalLin * volFEStrength);
	}

#if __BANK
	if(g_DrawFESpeechBubble)
	{
		Vec3V forward;
		Vec3V listPos;
		const CPed* player = FindPlayerPed();
		if(player)
		{
			forward = player->GetTransform().GetForward();
			listPos = player->GetTransform().GetPosition();
		}
		else
		{
			forward = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1();
			listPos = Add(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(), Vec3V(V_NEGHALF));
		}

		for(s32 dir = 0; dir < 2; dir++)
		{
			for(s32 degrees = 0; degrees < 180; degrees+=5)
			{
				const f32 degreesFloat = static_cast<f32>(degrees);
				const f32 pointARad = degreesFloat * PI/180.0f * (dir == 0 ? 1.0f : -1.0f); 
				const f32 pointBRad = (degreesFloat + 5.0f) * PI/180.0f * (dir == 0 ? 1.0f : -1.0f);

				const f32 distInnerA = sm_FEBubbleAngleToDistanceInner.CalculateValue(degreesFloat);
				const f32 distInnerB = sm_FEBubbleAngleToDistanceInner.CalculateValue(degreesFloat + 5.0f);
				const f32 distOuterA = sm_FEBubbleAngleToDistanceOuter.CalculateValue(degreesFloat);
				const f32 distOuterB = sm_FEBubbleAngleToDistanceOuter.CalculateValue(degreesFloat + 5.0f);

				Vec3V pointAInner = forward;
				pointAInner = RotateAboutZAxis(pointAInner, ScalarV(pointARad));
				pointAInner = Scale(pointAInner, ScalarV(distInnerA));
				pointAInner = Add(pointAInner, listPos);

				Vec3V pointBInner = forward;
				pointBInner = RotateAboutZAxis(pointBInner, ScalarV(pointBRad));
				pointBInner = Scale(pointBInner, ScalarV(distInnerB));
				pointBInner = Add(pointBInner, listPos);

				Vec3V pointAOuter = forward;
				pointAOuter = RotateAboutZAxis(pointAOuter, ScalarV(pointARad));
				pointAOuter = Scale(pointAOuter, ScalarV(distOuterA));
				pointAOuter = Add(pointAOuter, listPos);

				Vec3V pointBOuter = forward;
				pointBOuter = RotateAboutZAxis(pointBOuter, ScalarV(pointBRad));
				pointBOuter = Scale(pointBOuter, ScalarV(distOuterB));
				pointBOuter = Add(pointBOuter, listPos);

				grcDebugDraw::Line(pointAInner, pointBInner, Color_DarkSeaGreen);
				grcDebugDraw::Line(pointAOuter, pointBOuter, Color_purple);
				grcDebugDraw::Line(pointAInner, pointAOuter, Color_DarkSeaGreen, Color_purple);
			}
		}
	}
#endif

}

void audSpeechAudioEntity::ComputeVehicleOpennessMetrics(const audSpeechVolume volType, f32& vol, f32& lpf) const
{
	vol = 0.0f;
	lpf = kVoiceFilterLPFMaxCutoff;

	if (volType != AUD_SPEECH_FRONTEND 
		&& m_Parent 
		&& m_Parent->GetVehiclePedInside() 
		&& m_Parent->GetVehiclePedInside()->GetVehicleAudioEntity() 
		&& (!IsPedInPlayerCar(m_Parent) || g_IgnoreBeingInPlayerCar))
	{
		const f32 vehicleOpenness = m_Parent->GetVehiclePedInside()->GetVehicleAudioEntity()->GetOpenness();
		lpf = g_VehicleOpennessToSpeechFilterCurve.CalculateValue(vehicleOpenness);
		vol = g_VehicleOpennessToSpeechVolumeCurve.CalculateValue(vehicleOpenness);
	}
}

void audSpeechAudioEntity::ComputeSpeakerVolumeConeMetrics(const Vec3V& listToSpeakerNorm, const audSpeechVolume volType, const f32 distanceToPed, f32& vol, f32& lpf) const
{
	vol = 0.0f;
	lpf = kVoiceFilterLPFMaxCutoff;

	if (m_Parent && volType != AUD_SPEECH_FRONTEND)
	{
		// We don't cone speech up close, and have a band where we transition to coned
		if((!IsPedInPlayerCar(m_Parent) || g_IgnoreBeingInPlayerCar) && !m_Parent->IsLocalPlayer() && !IsEqualAll(listToSpeakerNorm, Vec3V(V_ZERO)))
		{
			const Vec3V forward = m_AnimalParams ? VECTOR3_TO_VEC3V(m_PedHeadMatrix.a) : VECTOR3_TO_VEC3V(m_PedHeadMatrix.b);
			const Vec3V speakerToListNorm = Scale(listToSpeakerNorm, ScalarV(-1.0f));
			const f32 cosAngle = Dot(forward, speakerToListNorm).Getf();
			const f32 angle = AcosfSafe(cosAngle);
			const f32 angleDegrees = angle * RtoD;
			const f32 coneAngleStrength = RampValueSafe(angleDegrees, g_SpeakerConeAngleInner, g_SpeakerConeAngleOuter, 0.0f, 1.0f);
			const f32 coneDistanceStrength = audCurveRepository::GetLinearInterpolatedValue(0.0f, 1.0f, 
				g_SpeakerConeDistForZeroStrength, g_SpeakerConeDistForFullStrength, distanceToPed);

			const f32 volLin = Lerp(coneAngleStrength * coneDistanceStrength, 1.0f, g_SpeakerConeVolAttenLin);
			vol = audDriverUtil::ComputeDbVolumeFromLinear(volLin);

			// Also filter the speech if the ped is facing away from you
			const f32 maxRearFilterLin = audDriverUtil::ComputeLinearFromHzFrequency(g_SpeakerConeLPF);
			const f32 rearFilterLin = Lerp(coneAngleStrength * coneDistanceStrength, 1.0f, maxRearFilterLin);
			lpf = audDriverUtil::ComputeHzFrequencyFromLinear(rearFilterLin);
		}
	}
}

void audSpeechAudioEntity::ComputeListenerVolumeConeMetrics(const Vec3V& listToSpeakerNorm, const audSpeechType speechType, const audSpeechVolume volType, const f32 distanceToPed, f32& vol, f32& lpf) const
{
	vol = 0.0f;
	lpf = kVoiceFilterLPFMaxCutoff;

	// Make sure we're only doing coning on speech that has no distance attenuation, otherwise we'll be doing it twice,
	// once here and once in environment_game
	if(speechType != AUD_SPEECH_TYPE_ANIMAL_NEAR && speechType != AUD_SPEECH_TYPE_ANIMAL_FAR && speechType != AUD_SPEECH_TYPE_ANIMAL_PAIN && speechType != AUD_SPEECH_TYPE_SCRIPTED
		&& volType != AUD_SPEECH_FRONTEND && !IsEqualAll(listToSpeakerNorm, Vec3V(V_ZERO)))
	{
		const Vec3V forward = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1();
		const f32 cosAngle = Dot(forward, listToSpeakerNorm).Getf();
		const f32 angle = AcosfSafe(cosAngle);
		const f32 angleDegrees = angle * RtoD;
		const f32 coneAngleStrength = RampValueSafe(angleDegrees, g_ListenerConeAngleInner, g_ListenerConeAngleOuter, 0.0f, 1.0f);
		const f32 coneDistanceStrength = sm_DistToListenerConeStrengthCurve.CalculateValue(distanceToPed);
		const f32 volLin = Lerp(coneAngleStrength * coneDistanceStrength, 1.0f, g_ListenerConeVolAttenLin);
		vol = audDriverUtil::ComputeDbVolumeFromLinear(volLin);

		// Filter speech that's behind you when in stereo
		if(audDriver::GetDownmixOutputMode() <= AUD_OUTPUT_STEREO)
		{
			const f32 maxRearFilterLin = audDriverUtil::ComputeLinearFromHzFrequency(g_ListenerConeLPF);
			const f32 rearFilterLin = Lerp(coneAngleStrength * coneDistanceStrength, 1.0f, maxRearFilterLin);
			lpf = audDriverUtil::ComputeHzFrequencyFromLinear(rearFilterLin);
		}
	}
}

void audSpeechAudioEntity::ComputeDistanceMetrics(const audSpeechType speechType, const audSpeechVolume volType, const audConversationAudibility audibility, 
												 f32 distanceToPed, f32& vol, f32& linHPF) const
{
	vol = 0.0f;
	linHPF = kVoiceFilterHPFMinCutoff;

	if(naVerifyf(speechType < AUD_NUM_SPEECH_TYPES, "Passed invalid audSpeechType (%u) when getting custom speech distance metrics", speechType)
		&& naVerifyf(volType < AUD_NUM_SPEECH_VOLUMES, "Passed invalid audSpeechVolume (%u) when getting custom speech distance metrics", volType)
		&& naVerifyf(audibility < AUD_NUM_AUDIBILITIES, "Passed invalid audConversationAudibility (%u) when getting custom speech distance metrics", audibility))
	{
		f32 linVol = 1.0f;
		f32 greatestAudLinVol = 1.0f;
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			// Remap the distance to ped to a smaller interval since the camera can only be 30 meters away from anyped in replay.
			distanceToPed = ClampRange(distanceToPed,0.f,30.f) * (volType == AUD_SPEECH_SHOUT ? 80.f : 40.f);
		}
#endif
		switch(speechType)
		{
		case AUD_SPEECH_TYPE_AMBIENT:
			linVol = sm_AmbientDistToVolCurve[volType][audibility].CalculateValue(distanceToPed);
			greatestAudLinVol = sm_AmbientDistToVolCurve[volType][m_GreatestAudibility].CalculateValue(distanceToPed);
			break;
		case AUD_SPEECH_TYPE_SCRIPTED:
			linVol = sm_ScriptedDistToVolCurve[volType][audibility].CalculateValue(distanceToPed);
			greatestAudLinVol = sm_ScriptedDistToVolCurve[volType][m_GreatestAudibility].CalculateValue(distanceToPed);
			break;
		case AUD_SPEECH_TYPE_PAIN:
			linVol = sm_PainDistToVolCurve[volType][audibility].CalculateValue(distanceToPed);
			greatestAudLinVol = sm_PainDistToVolCurve[volType][m_GreatestAudibility].CalculateValue(distanceToPed);
			break;
		case AUD_SPEECH_TYPE_ANIMAL_PAIN:
		case AUD_SPEECH_TYPE_ANIMAL_NEAR:
		case AUD_SPEECH_TYPE_ANIMAL_FAR:
		case AUD_SPEECH_TYPE_BREATHING:
		case AUD_SPEECH_TYPE_INVALID:
			break;
		default:
			naAssertf(false, "Bad audSpeechType passed in when getting speech distance volume metrics");
			break;
		}

		const f32 interpolatedLinVol = Lerp(m_AmountToUsePreviousHighAudibility, linVol, greatestAudLinVol);
		vol = audDriverUtil::ComputeDbVolumeFromLinear(interpolatedLinVol);

		if(g_UseHPFOnLeadIn || audibility != AUD_AUDIBILITY_LEAD_IN)	
		{
			linHPF = sm_SpeechDistanceToHPFCurve.CalculateValue(distanceToPed);
		}
	}
}

f32 audSpeechAudioEntity::ComputePositionedHeadsetVolume(const CPed* pSpeakingPed, const audSpeechType speechType, const f32 distanceToPed, bool forceHeadset) const
{
	f32 vol = g_SilenceVolume;
	if(speechType == AUD_SPEECH_TYPE_SCRIPTED && (m_ActiveSpeechIsHeadset || forceHeadset))
	{
		vol = 0.0f;

		// Hack to allow LES1A_BRAC to play in SP with no headset, and MP with headset processing
		if(m_LastPlayedSpeechContextPHash == 1782767579 && NetworkInterface::IsGameInProgress() && m_CurrentlyPlayingVoiceNameHash == 2377353286)
		{
			vol = 1.0f;
		}
		else if(m_Parent && sm_HeadsetSpeechDistToVol.IsValid())
		{
			vol = sm_HeadsetSpeechDistToVol.CalculateValue(distanceToPed);
		}

		// If the ForceNPCHeadsetSpeechEffect flag is set, we force the headset effect on NPC peds unless
		// they're in the same vehicle as the player. This can be used to fix issues with peds driving close
		// to each other and chatting on mission, and fading in and out of headset mode (which sounds odd)
		if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisableNPCHeadsetSpeechAttenuation) && pSpeakingPed && !pSpeakingPed->IsLocalPlayer())
		{
			const CPed* localPlayer = FindPlayerPed();
			const CVehicle* localPlayerVehicle = localPlayer ? localPlayer->GetVehiclePedInside() : nullptr;
			const CVehicle* speakingPedVehicle = pSpeakingPed->GetVehiclePedInside();

			if((localPlayerVehicle || speakingPedVehicle) && (speakingPedVehicle != localPlayerVehicle))
			{
				vol = 1.f;
			}
		}

	}
	return vol;
}

f32 audSpeechAudioEntity::ComputeAmountToUseEnvironmentForFESpeech() const
{
	f32 volVerb = g_SilenceVolume;

	// Figure out how open the car is and increase the volume of the reverb only channel accordingly
	if(g_AudioEngine.GetEnvironment().GetSpecialEffectMode() == kSpecialEffectModeStoned)
	{
		volVerb = 0.f;
	}
	else if(m_Parent && m_Parent->GetVehiclePedInside() && m_Parent->GetVehiclePedInside()->GetVehicleAudioEntity())
	{
		const f32 vehicleOpenness = m_Parent->GetVehiclePedInside()->GetVehicleAudioEntity()->GetOpenness();
		const f32 volVerbLin = sm_VehicleOpennessToReverbVol.CalculateValue(vehicleOpenness);
		volVerb = audDriverUtil::ComputeDbVolumeFromLinear(volVerbLin);
	}

	return volVerb;
}

void audSpeechAudioEntity::UpdateUnderwaterMetrics()
{
	sm_UnderwaterLPF = kVoiceFilterLPFMaxCutoff;
	sm_UnderwaterReverb = 0.0f;

	if(Water::IsCameraUnderwater())
	{
		const f32 depth = Water::GetCameraWaterDepth();
		const f32 depthRatio = RampValueSafe(depth, 0.0f, g_MaxDepthToHearSpeech, g_MinUnderwaterLinLPF, 0.0f);
		sm_UnderwaterLPF = audDriverUtil::ComputeHzFrequencyFromLinear(depthRatio);

		sm_UnderwaterReverb = g_UnderwaterReverbWet;
	}
}

f32 audSpeechAudioEntity::ComputePostSubmixVolume(const audSpeechType speechType, const audSpeechVolume volType, const bool inCarAndSceneSaysFrontend, const f32 volOffset)
{
	naAssertf(speechType < AUD_NUM_SPEECH_TYPES, "Passed invalid audSpeechType (%u) when getting post submix volPos", speechType);
	naAssertf(volType < AUD_NUM_SPEECH_VOLUMES, "Passed invalid audSpeechVolume (%u) when getting post submix volPos", volType);

	// Drop the volume of any non-script speech if there's a scripted conversation going on.
	f32 convVolAtten = 0.0f;
	if(speechType != AUD_SPEECH_TYPE_SCRIPTED
		&& volType != AUD_SPEECH_FRONTEND
		&& !inCarAndSceneSaysFrontend
		&& g_ScriptAudioEntity.IsScriptedConversationOngoing() 
		&& !g_ScriptAudioEntity.IsScriptedConversationPaused())
	{
		convVolAtten = -3.0f;
	}

	// Boost the volPos of speech if the player is in a car
	const f32 playerInCarVolOffset = ComputeVolumeOffsetFromPlayerInCar();

	// Combine
	const f32 postSubmixVol = convVolAtten + playerInCarVolOffset + volOffset;

	return postSubmixVol;
}

f32 audSpeechAudioEntity::ComputeVolumeOffsetFromPlayerInCar()
{
	f32 volOffset = 0.0f;
	f32 inACarRatio = 0.0f;
	bool isPlayerVehicleATrain = false;
	audNorthAudioEngine::GetGtaEnvironment()->IsPlayerInVehicle(&inACarRatio, &isPlayerVehicleATrain);
	if (!isPlayerVehicleATrain)
	{
		volOffset += Lerp(inACarRatio, g_PlayerInCarVolumeOffset, 0.0f);
	}
	return volOffset;
}

void audSpeechAudioEntity::ComputeEchoInitMetrics(const Vector3& posSpeaker, const audSpeechVolume volType, const bool useEcho1, const bool useEcho2, 
												audEchoMetrics& echo1, audEchoMetrics& echo2) const
{
	echo1.useEcho = useEcho1 && (volType == AUD_SPEECH_SHOUT || volType == AUD_SPEECH_MEGAPHONE);
	echo2.useEcho = useEcho2 && volType == AUD_SPEECH_MEGAPHONE;

	if(echo1.useEcho)
	{
		s32 echoIdx1 = -1;
		s32 echoIdx2 = -1;
		echoIdx1 = audNorthAudioEngine::GetGtaEnvironment()->GetBestEchoForSourcePosition(posSpeaker, &echoIdx2);

		if(echoIdx1 == -1)
			echo1.useEcho = false;
		if(echoIdx2 == -1)
			echo2.useEcho = false;

		if(echo1.useEcho)
		{
			// Adjust the volume based on the audSpeechVolume, so megaphone echoes are louder than shouted
			const f32 volEchoOffset = volType == AUD_SPEECH_SHOUT ? g_SpeechEchoVolShouted : g_SpeechEchoVolMegaphone;

			// It sounds silly when you have lots of reflections happening in a trailer park when shouting, so take the volume down with the built-up
			// Probably a good idea to have separate curves for megaphone vs scripted so we can actually still delay out in trailer parks with the megaphone
			const f32 builtUpFactor = audNorthAudioEngine::GetGtaEnvironment()->GetBuiltUpFactorForDirection(AUD_AMB_DIR_LOCAL);
			f32 volLinBuiltUp = 0.0f;
			if(volType == AUD_SPEECH_SHOUT)
			{
				volLinBuiltUp = sm_BuiltUpToEchoVolLinShouted.CalculateValue(builtUpFactor);
			}
			else if(volType == AUD_SPEECH_MEGAPHONE)
			{
				volLinBuiltUp = sm_BuiltUpToEchoVolLinMegaphone.CalculateValue(builtUpFactor);
			}
			const f32 volBuiltUp = audDriverUtil::ComputeDbVolumeFromLinear(volLinBuiltUp);

			const f32 volEchoPostSubmix = volEchoOffset + volBuiltUp;

			// Don't use the volume we get here as we're not terribly concerned with the accuracy of the echo, we just want the pos and predelay
			f32 volUnused = 0.0f;

			Vector3 posEcho1;
			u32 predelayEcho1 = 0;
			audNorthAudioEngine::GetGtaEnvironment()->CalculateEchoDelayAndAttenuation(echoIdx1, posSpeaker, &predelayEcho1, &volUnused, &posEcho1);

			// Scale the predelay down a bit as the metrics we get for the echoes are too large
			predelayEcho1 = (u32)((f32)predelayEcho1 * g_SpeechEchoPredelayScalar);

			// Somewhat expensive, so only do this if we need to
			if(echo2.useEcho)
			{
				Vector3 posEcho2;
				u32 predelayEcho2 = 0;
				audNorthAudioEngine::GetGtaEnvironment()->CalculateEchoDelayAndAttenuation(echoIdx2, posSpeaker, &predelayEcho2, &volUnused, &posEcho2);
				predelayEcho2 = (u32)((f32)predelayEcho2 * g_SpeechEchoPredelayScalar);

				// Separate the predelays a bit, as it sounds better the more distinct they are
				const u32 preDelayDiff = Max(predelayEcho1, predelayEcho2) - Min(predelayEcho1, predelayEcho2);
				if(preDelayDiff < g_SpeechEchoMinPredelayDiff)
				{
					if(predelayEcho1 >= predelayEcho2)
					{
						predelayEcho1 += g_SpeechEchoMinPredelayDiff - preDelayDiff;
					}
					else
					{
						predelayEcho2 += g_SpeechEchoMinPredelayDiff - preDelayDiff;
					}
				}

				echo2.volPostSubmix = volEchoPostSubmix;
				echo2.predelay = predelayEcho2;
				echo2.pos = VECTOR3_TO_VEC3V(posEcho2);
			}

			echo1.volPostSubmix = volEchoPostSubmix;
			echo1.predelay = predelayEcho1;
			echo1.pos = VECTOR3_TO_VEC3V(posEcho1);
		}
	}
}

void audSpeechAudioEntity::ComputeEchoMetrics(const Vec3V& posSpeaker, const audSpeechType speechType, const audSpeechVolume volType, const f32 volEchoBase, const f32 hpf, audEchoMetrics& echo) const
{
	const Vec3V listToEcho = echo.pos - g_AudioEngine.GetEnvironment().GetPanningListenerPosition();
	const Vec3V speakerToEcho = echo.pos - posSpeaker;

	const ScalarV distListToEcho = Mag(listToEcho);
	Vec3V listToEchoNorm(V_ZERO);
	if(!IsEqualAll(distListToEcho, ScalarV(V_ZERO)))
	{
		listToEchoNorm = InvScale(listToEcho, distListToEcho);
	}
	const ScalarV distSpeakerToEcho = Mag(speakerToEcho);
	const ScalarV distSpeakerToEchoToList = distSpeakerToEcho + distListToEcho;

	// Do the same sort of rear attenuation as normal speech if the echo is behind you.
	f32 listenerConeVol;
	f32 listenerConeLPF;
	ComputeListenerVolumeConeMetrics(listToEchoNorm, speechType, volType, distListToEcho.Getf(), listenerConeVol, listenerConeLPF);

	// Attenuate the volume based on the Speaker->Echo->Listener distance so we can make closer echoes louder.
	// We use custom curves instead of the normal speech distance curves here because there is a large variance in distances and we don't want a large variance in volume
	f32 volLinEchoDist = 0.0f;
	if(volType == AUD_SPEECH_SHOUT)
	{
		volLinEchoDist = sm_EchoShoutedDistToVolLinCurve.CalculateValue(distSpeakerToEchoToList.Getf());
	}
	else if(volType == AUD_SPEECH_MEGAPHONE)
	{
		volLinEchoDist = sm_EchoMegaphoneDistToVolLinCurve.CalculateValue(distSpeakerToEchoToList.Getf());
	}
	const f32 volEchoDist = audDriverUtil::ComputeDbVolumeFromLinear(volLinEchoDist);

	echo.vol = volEchoBase + listenerConeVol + volEchoDist;
	echo.lpf = listenerConeLPF;
	echo.hpf = hpf;

#if __BANK
	if(m_IsDebugEntity && g_DebugEchoes)
	{
		s32 line = 0;
		char buf[255];
		formatf(buf, "ECHO: Vol: %.02f LPF: %.0f HPF: %.0f", echo.vol, echo.lpf, echo.hpf);
		grcDebugDraw::Text(echo.pos, Color_white, 0, line, buf);
		line += 10;
		formatf(buf, "DIST: List->Echo: %.02f Speaker->Echo: %.02f Total: %.02f volDistAtten: %.02f", 
				distListToEcho.Getf(), distSpeakerToEcho.Getf(), distSpeakerToEchoToList.Getf(), volEchoDist);
		grcDebugDraw::Text(echo.pos, Color_white, 0, line, buf);
		grcDebugDraw::Sphere(echo.pos, 2.0f, Color_orange);
		const CPed* player = FindPlayerPed();
		if(player)
		{
			grcDebugDraw::Line(echo.pos, player->GetTransform().GetPosition(), Color_blue);
		}
	}
#endif
}

void audSpeechAudioEntity::ResetSpeechSmoothers()
{
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		m_SpeechVolSmoother[i].Reset();
	}
	m_SpeechLinLPFSmoother.Reset();
	m_SpeechLinHPFSmoother.Reset();
}

bool audSpeechAudioEntity::IsInCarFrontendScene() const
{
	bool isBus = false;
	const bool animalInCar = m_AnimalParams && m_Parent && m_Parent->GetAttachParent() && 
		m_Parent->GetAttachParent()->GetType() == ENTITY_TYPE_VEHICLE && 
		m_Parent->GetAttachParent() == FindPlayerPed()->GetVehiclePedInside();
	const bool isInPlayerCar = IsPedInPlayerCar(m_Parent, &isBus);

#if GTA_REPLAY
	const camBaseCamera* theActiveRenderedCamera = camInterface::GetDominantRenderedCamera();
	const bool isInReplayFreeCam = CReplayMgr::IsEditModeActive() && theActiveRenderedCamera && theActiveRenderedCamera->GetClassId() != camReplayRecordedCamera::GetStaticClassId();
#endif
	
	const bool inCarAndSceneSaysFrontend = m_Parent && (isInPlayerCar || animalInCar) && (!isBus || m_Parent->IsLocalPlayer()) &&
		audDynamicMixer::IsPlayerFrontend() && audNorthAudioEngine::GetMicrophones().IsPlayerFrontend() REPLAY_ONLY(&& !isInReplayFreeCam);
	return inCarAndSceneSaysFrontend;
}

bool audSpeechAudioEntity::IsPedInPlayerCar(CPed* ped, bool* isBus) const
{
	bool pedInVehicle = ped && ped->GetVehiclePedInside() && (ped->GetVehiclePedInside() == CGameWorld::FindLocalPlayerVehicle());
	if (pedInVehicle || m_SpectatorPedInVehicle)
	{
		if(isBus)
		{
			(*isBus) = ((pedInVehicle && (ped->GetVehiclePedInside()->m_nVehicleFlags.bIsBus || (ped->GetVehiclePedInside()->GetVehicleAudioEntity()->GetVehicleModelNameHash() == ATSTRINGHASH("TOURBUS", 0x73B1C3CB)))) || (m_SpectatorPedInVehicle && m_SpectatorPedVehicleIsBus));
		}
		return true;
	}
	return false;
}

audSaySuccessValue audSpeechAudioEntity::CompleteSay()
{
	naSpeechEntDebugf1(this, "CompleteSay()");

#if __BANK
	Color32 ambDebugMsgColor(255,0,0);

	char context[255];
	safecpy(context, m_DeferredSayStruct.context);
#endif
	f32 priority = m_DeferredSayStruct.priority;
	u32 requestedVolume = m_DeferredSayStruct.requestedVolume;
	SpeechAudibilityType requestedAudibility = m_DeferredSayStruct.requestedAudibility;
	s32 preDelay = m_DeferredSayStruct.preDelay;
	u32 startOffset = m_DeferredSayStruct.startOffset;
	u32 voiceNameHash = m_DeferredSayStruct.voiceNameHash;
	u32 contextPHash = m_DeferredSayStruct.contextPHash;
	s32 variationNum = m_DeferredSayStruct.variationNumber;
	u32 preloadTimeoutInMilliseconds = m_DeferredSayStruct.preloadTimeOutInMilliseconds;
	SpeechContext* speechContext = m_DeferredSayStruct.speechContext;
	bool preloadOnly = m_DeferredSayStruct.preloadOnly;
	bool fromScript = m_DeferredSayStruct.fromScript;
	bool isMultipartContext = m_DeferredSayStruct.isMultipartContext;
	m_DontAddBlip = !m_DeferredSayStruct.addBlip;
	m_IsUrgent = m_DeferredSayStruct.isUrgent;

#if GTA_REPLAY
	// Mute/pitch flags are stored in the replay packet during replay playback
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		m_IsMutedDuringSlowMo = (m_DeferredSayStruct.isMutedDuringSlowMo)
			&& (speechContext ? (AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISMUTEDDURINGSLOWMO)!=AUD_TRISTATE_FALSE) : true)
			&& (!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowAmbientSpeechInSlowMo));
		m_IsPitchedDuringSlowMo = (m_DeferredSayStruct.isPitchedDuringSlowMo) 
			&& (speechContext ? (AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISPITCHEDDURINGSLOWMO)!=AUD_TRISTATE_FALSE) : true)
			&& (!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowAmbientSpeechInSlowMo));
	}

	m_ActiveSpeechIsHeadset = false;
	m_PlaySpeechWhenFinishedPreparing = false;

	bool isPain = speechContext ? AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISPAIN)==AUD_TRISTATE_TRUE : false;
#if __BANK
	if(contextPHash == atPartialStringHash("HIGH_FALL_NEW"))
		isPain = true;
#endif
	StopSpeechForinterruptingAmbientSpeech();

	audSpeechSound** snd = isPain ? &m_PainSpeechSound : &m_AmbientSpeechSound;

#if __BANK
	if(g_UseTestPainBanks && isPain)
		variationNum = 1;
#endif

	//if this is part of a deferred Say, we need to mark the slot as allocated and set its entity and priority
	if(m_WaitingForAmbientSlot)
	{
		g_SpeechManager.MarkSlotAsAllocated(m_AmbientSlotId, this, priority);
	}

	// Map the requestedVolume and contextVolume to the audSpeechVolume and use whichever is more audible
	const audSpeechVolume contextVolType = GetVolTypeForSpeechContext(speechContext, contextPHash);
	const audSpeechVolume paramVolType = GetVolTypeForSpeechParamsVolume((audAmbientSpeechVolume)requestedVolume);
	const audSpeechVolume volType = FindVolTypeToUseForAmbientSpeech(contextVolType, paramVolType);

	const audConversationAudibility contextAudibility = GetAudibilityForSpeechContext(speechContext);
	const audConversationAudibility paramAudibility = GetAudibilityForSpeechAudibilityType(requestedAudibility);
	const audConversationAudibility audibility = Max(contextAudibility, paramAudibility);

	const audSoundSet* soundSet = GetSoundSetForAmbientSpeech(isPain);

	if(!soundSet)
	{
		FreeSlot();
		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(m_DeferredSayStruct.debugDrawMsg, m_Parent, ambDebugMsgColor);)
		naAssertf(false, "Unable to find the audSoundSet for context %s", 
			speechContext ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(speechContext->NameTableOffset) : "unknown");
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED !soundSet");
		return AUD_SPEECH_SAY_FAILED;
	}

	const audMetadataRef soundRef = soundSet->Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));
	naAssertf(soundRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");
	
	// Set up our speech sound and init params
	audSoundInitParams initParam;
	initParam.RemoveHierarchy = false;
	initParam.AllowOrphaned = true;
	initParam.Predelay = preDelay;
	initParam.EnvironmentGroup = m_Parent ? m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true) : NULL;
	initParam.UpdateEntity = true;
	initParam.StartOffset = startOffset;
	// In order to make sure we don't drop scripted speech which could end up breaking mission scripts, use the streaming bucket.
	// Each bucket has 192 slots, and the unlikely worst case scenario for the streaming bucket 
	// is something like 120 sounds so there should always be plenty room for scripted speech and ambient speech triggered from script
	BANK_ONLY(m_UsingStreamingBucking = false;)
	if(!PARAM_audNoUseStreamingBucketForSpeech.Get() && fromScript)
	{
		BANK_ONLY(m_UsingStreamingBucking = true;)
		Assign(initParam.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
	}
	// We want interrupts, and in-car player ambient speech to be the same volume as scripted speech
	// We mute scripted speech and allow ambient for Trevor's rage stuff, so don't send the ambient speech to the script category
	// if the scripted category is muted ( or muted give or take )
	if((IsInCarFrontendScene() || g_ScriptAudioEntity.IsPedInCurrentConversation(m_Parent))
		&& (g_SpeechScriptedCategory && g_SpeechScriptedCategory->GetRequestedVolume() > -70.0f))
	{
		initParam.Category = g_SpeechScriptedCategory;
	}
	//initParam.IsAuditioning = true;
	naCErrorf(m_Parent && m_Parent->GetPlaceableTracker(), "Unable to get tracker from ped parent in CompleteSay()");

	m_HasPreparedAndPlayed = false;
	CreateSound_PersistentReference(soundRef, (audSound**)snd, &initParam);

	if (!(*snd))
	{
		FreeSlot();
		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(m_DeferredSayStruct.debugDrawMsg, m_Parent, ambDebugMsgColor););
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - !(*snd)");
		return AUD_SPEECH_SAY_FAILED;
	}

	if((*snd)->GetSoundTypeID() != SpeechSound::TYPE_ID)
	{
		naErrorf("In 'Say(...)', have successfully created a sound reference but it's not a speech sound");
		(*snd)->StopAndForget();
		FreeSlot();
		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(m_DeferredSayStruct.debugDrawMsg, m_Parent, ambDebugMsgColor););
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - (*snd)->GetSoundTypeID() != SpeechSound::TYPE_ID");
		return AUD_SPEECH_SAY_FAILED;
	}

	if(!(*snd)->InitSpeech(voiceNameHash, contextPHash, variationNum))
	{
#if __BANK
		//Probably never going to be hit, but what the heck.
		if(g_SilenceVolume)
		{
			if(!(*snd)->InitSpeech(g_SilentVoiceHash, contextPHash, 1))
			{
#endif
				//		Assert(0);
				(*snd)->StopAndForget();
				FreeSlot();
				ResetTriggeredContextVariables();
				BANK_ONLY(AddAmbSpeechDebugDrawStruct(m_DeferredSayStruct.debugDrawMsg, m_Parent, ambDebugMsgColor););
				naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - !(*snd)->InitSpeech(voiceNameHash, contextPHash, variationNum)");
				return AUD_SPEECH_SAY_FAILED;
#if __BANK
			}
		}
#endif
	}

	// Reset and cache our volType and audibility so during the updates we can still use the custom curves for volPos and filter.
	m_CachedSpeechStartOffset = 0;
	m_CachedSpeechType = isPain ? AUD_SPEECH_TYPE_PAIN : AUD_SPEECH_TYPE_AMBIENT;
	m_CachedVolType = volType;
	m_CachedAudibility = audibility;
	m_SpeechVolumeOffset = 0.0f;
	m_IsFromScript = fromScript;
	m_UseShadowSounds = true;

	audWaveSlot* waveSlot = g_SpeechManager.GetAmbientWaveSlotFromId(m_AmbientSlotId);
	m_VisemeAnimationData = NULL;
	const bool ignoreViseme = m_Parent && (m_Parent->m_nDEflags.bFrozen || m_Parent->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodEventScanning));

	(*snd)->Prepare(waveSlot, true, naWaveLoadPriority::Speech);
	if (preloadOnly)
	{
		m_Preloading = true;
	}
	else
	{
		m_Preloading = false;
	}

	m_PreloadFreeTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + preloadTimeoutInMilliseconds;

	m_PreloadedSpeechIsPlaying = false;

	// We can actually play the speech, so update everything
	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	naCErrorf(variationNum>=0, "In 'Say(...)' invalid variationNum when about to update sound we just played");

	//only want to do this for the first line of a multipart context, as we store the conversation version.  Also, we've already
	//	incremented the counter after setting the variation for the first line, hence checking against 2
	if(isMultipartContext && m_MultipartContextCounter == 2)
	{
		g_SpeechManager.AddVariationToHistory(voiceNameHash, m_OriginalMultipartContextPHash, (u32)m_MultipartVersion, currentTime);
#if GTA_REPLAY
		if(!m_Preloading)
		{
			g_ScriptAudioEntity.AddToPlayedList(voiceNameHash, contextPHash, variationNum);
		}
#endif
	}
	else
	{
#if __BANK
		// Since voices are being reused, use a ped identifier rather than voice name for history
		const u32 pedIdentifier = m_Parent ? static_cast<u32>(CPed::GetPool()->GetIndex(m_Parent)) : 0U;
		g_SpeechManager.AddVariationToHistory( (m_UsingFakeFullPVG || voiceNameHash==g_SilentVoiceHash) ? pedIdentifier : voiceNameHash, contextPHash, (u32)variationNum, currentTime);
#else
		g_SpeechManager.AddVariationToHistory(voiceNameHash, contextPHash, (u32)variationNum, currentTime);
#endif
#if GTA_REPLAY
		if(!m_Preloading)
		{
			g_ScriptAudioEntity.AddToPlayedList(voiceNameHash, contextPHash, variationNum);
		}
#endif
	}

	g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(m_AmbientSlotId, voiceNameHash, 
#if __BANK
		(context[0] != '\0') ? context : speechContext ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(speechContext->NameTableOffset) : "unknown",
#endif
		variationNum);

	// Store what we're currently playing
#if __BANK
	safecpy(m_CurrentlyPlayingSpeechContext, (context[0] != '\0') ? context : speechContext ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(speechContext->NameTableOffset) : "unknown");
#endif
	m_LastPlayedWaveSlot = waveSlot;
	m_LastPlayedSpeechContextPHash = contextPHash;
	m_LastPlayedSpeechVariationNumber = variationNum;
	m_CurrentlyPlayingSoundSet = *soundSet;
	m_CurrentlyPlayingPredelay1 = (u32)(preDelay >= 0 ? preDelay : 0);
	m_CurrentlyPlayingVoiceNameHash = voiceNameHash;
	// If it's HIGH_FALL, and we're the player, make a note of that, so we can do custom interuption stuff
	if (contextPHash==g_HighFallContextPHash && m_Parent && m_Parent->IsLocalPlayer())
	{
		sm_TimeLastPlayedHighFall = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}

	// Need to clear this out each time we play something
	for(u32 i=0; i<NUM_AUD_SPEECH_METADATA_CATEGORIES; i++)
	{
		m_SpeechMetadataCategoryHashes[i] = atStringHash(g_SpeechMetadataCategoryNames[i]);
		m_LastQueriedMarkerIdForCategory[i] = -1;
		m_LastQueriedGestureIdForCategory[i] = -1;
		m_GesturesHaveBeenParsed = false;
	}

	m_GestureData = NULL;
	m_NumGestures = 0;

#if __BANK
	m_TimeOfFakeGesture = -1;
	m_SoundLengthForFakeGesture = -1;
	m_FakeGestureNameHash = GetFakeGestureHashForContext(speechContext);
	m_FakeMarkerReturned = false;
#endif
#if __DEV
	formatf(m_LastPlayedWaveName, 64, "%s_%02d", speechContext ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(speechContext->NameTableOffset) : "", 
		variationNum);
#endif
	if(m_Parent && !m_IgnoreVisemeData && !ignoreViseme && !isPain)
		m_Parent->SetIsWaitingForSpeechToPreload(true);
	else
		m_PlaySpeechWhenFinishedPreparing = true;

	ResetTriggeredContextVariables();
#if __BANK
	ambDebugMsgColor.Set(0,255,0);
	AddAmbSpeechDebugDrawStruct(m_DeferredSayStruct.debugDrawMsg, m_Parent, ambDebugMsgColor);
	if(g_TriggeredContextTest)
	{
		AudBaseObject* obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(atFinalizeHash(contextPHash)) ;
		Displayf("TC Played. Context:%s Name:%s Voice:%u Time:%u", context, 
			obj ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(obj->NameTableOffset) : "",
			voiceNameHash, currentTime);
	}
#endif

#if __BANK
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_SUCCEEDED contextPHash: %u contextString: %s", contextPHash, context);
#else
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_SUCCEEDED contextPHash: %u contextString: %s", contextPHash, "__BANK only");
#endif

	return AUD_SPEECH_SAY_SUCCEEDED;
}

bool audSpeechAudioEntity::IsMultipartContext(u32 contextPHash)
{
	if(contextPHash == g_SpeechContextPedRantPHash ||
	   contextPHash == g_SpeechContextHookerStoryPHash 
		)
		return true;

	return false;
}


u32 audSpeechAudioEntity::CheckForModifiedVersion(u32 contextPHash, u32 voiceHash, u32& modVariation)
{
	const int eveningStart = 19;
	const int eveningEnd = 21;
	const int morningStart = 7;
	const int morningEnd = 11;
	const int minMSBetweenModUse = 120000;

	int currTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	int hours = CClock::GetHour();
	u32 modHash = 0;
	if(hours>eveningStart && hours<eveningEnd && sm_LastTimeAmbSpeechModsUsed[AUD_AMB_MOD_EVENING] < currTime + minMSBetweenModUse)
	{
		modHash = atPartialStringHash(g_AmbSpeechModifiers[AUD_AMB_MOD_EVENING], contextPHash);
	}
	else if( (hours>eveningEnd || hours<morningStart) && sm_LastTimeAmbSpeechModsUsed[AUD_AMB_MOD_NIGHT] < currTime + minMSBetweenModUse)
	{
		modHash = atPartialStringHash(g_AmbSpeechModifiers[AUD_AMB_MOD_NIGHT], contextPHash);
	}
	else if(hours>morningStart && hours<morningEnd && sm_LastTimeAmbSpeechModsUsed[AUD_AMB_MOD_MORNING] < currTime + minMSBetweenModUse)
	{
		modHash = atPartialStringHash(g_AmbSpeechModifiers[AUD_AMB_MOD_EVENING], contextPHash);
	}
	else if(g_weather.IsRaining() && sm_LastTimeAmbSpeechModsUsed[AUD_AMB_MOD_RAINING] < currTime + minMSBetweenModUse)
	{
		modHash = atPartialStringHash(g_AmbSpeechModifiers[AUD_AMB_MOD_RAINING], contextPHash);
	}

	if(modHash == 0)
		return contextPHash;

	modVariation = audSpeechSound::FindNumVariationsForVoiceAndContext(voiceHash, modHash);
	if(modVariation != 0)
	{
		modVariation = audEngineUtil::GetRandomInteger() % modVariation;
		return modHash;
	}

	return contextPHash;
}

audSaySuccessValue audSpeechAudioEntity::Say(const char* context, audSpeechInitParams speechInitParams, u32 voiceNameHash, s32 preDelay, CPed* replyingPed, 
											 u32 replyingContext, s32 replyingDelay, f32 replyProbability, s32* const variationNumberOverride, const Vector3& posForNullSpeaker)
{
#if __BANK
	if(NetworkInterface::IsGameInProgress())
	{
		naDisplayf("Say : %s", context);
	}
#endif

	return Say(atPartialStringHash(context), speechInitParams, voiceNameHash, preDelay, replyingPed, 
				replyingContext, replyingDelay, replyProbability, variationNumberOverride, posForNullSpeaker
#if __BANK
					, context
#endif
				);
}

audSaySuccessValue audSpeechAudioEntity::Say(u32 contextPHash, audSpeechInitParams speechInitParams, u32 voiceNameHash, s32 preDelay, CPed* replyingPed, 
								  u32 replyingContext, s32 replyingDelay, f32 replyProbability, s32* const variationNumberOverride, const Vector3& posForNullSpeaker
#if __BANK
									, const char* context
#endif
								  )
{
#if __BANK
	naSpeechEntDebugf1(this, "Say() with contextPHash: %u contextString: %s", contextPHash, context);
#else
	naSpeechEntDebugf1(this, "Say() with contextPHash: %u contextString: %s", contextPHash,	"__BANK only");
#endif

#if __BANK
	char debugDrawMsg[255];
	safecpy(debugDrawMsg, context);

	if(contextPHash == atPartialStringHash("SCREAM_SHOCKED") || contextPHash == atStringHash("SCREAM_SHOCKED"))
		naAssertf(0, "Trying to play SCREAM_SHOCKED as ambient speech.  Send callstack to audio.");

#endif

	if(!naVerifyf(!(sysThreadType::IsUpdateThread() && audNorthAudioEngine::IsAudioUpdateCurrentlyRunning()), "Say() called while update thread is running"))
	{
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - fwThreadType::IsUpdateThread() && audNorthAudioEngine::IsAudioUpdateCurrentlyRunning()");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	if(CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack())
	{
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack()");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if( (contextPHash == g_SpeechContextSeeWeirdoHash || contextPHash == g_SpeechContextSeeWeirdoPHash) &&
		currentTime - sm_TimeOfLastChat < 8000)
	{
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - contextPHash == g_SpeechContextSeeWeirdoHash && currentTime - sm_TimeOfLastChat < 8000");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	bool isPlayer = false;
	if(m_Parent)
	{
		isPlayer = m_Parent->IsPlayer();
		if(m_Parent->GetPedAudioEntity() && m_Parent->GetPedAudioEntity()->GetIsWhistling())
		{
			naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - m_Parent->GetPedAudioEntity() && m_Parent->GetPedAudioEntity()->GetIsWhistling()");
			Displayf("Can't speak - whistling %x", atFinalizeHash(contextPHash));
			BANK_ONLY(Displayf("Failed to say context : %s", context);)

			m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
			return AUD_SPEECH_SAY_FAILED;
		}
		if(CNetwork::IsGameInProgress() && isPlayer BANK_ONLY(&& !g_MakePedSpeak))
		{			
			if(!IsMPPlayerPedAllowedToSpeak(m_Parent, voiceNameHash))
			{
				naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - CNetwork::IsGameInProgress() && isPlayer BANK_ONLY(&& !g_MakePedSpeak)");
				m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
				return AUD_SPEECH_SAY_FAILED;
			}
		}
		if(contextPHash == g_SpeechContextGenericWarCryPHash)
		{
			const CPedWeaponManager* pWeaponManager = m_Parent->GetWeaponManager();
			if(!pWeaponManager || !pWeaponManager->GetIsArmedGun())
			{
				{
					naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - contextPHash == g_SpeechContextGenericWarCryPHash && !pWeaponManager || !pWeaponManager->GetIsArmedGun()");
					m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
					return AUD_SPEECH_SAY_FAILED;
				}
			}
		}
	}

	bool isBankLoadedPain = false;
	bool isPartialHash = false;
	int painIdNum = LookForPainContextInSay(contextPHash, isPlayer, isBankLoadedPain, isPartialHash);

	if(painIdNum >= 0)
	{
		if(isBankLoadedPain)
		{
			TriggerPainFromPainId(painIdNum, 0);
			naSpeechEntDebugf2(this, "AUD_SPEECH_SAY_SUCCEEDED - Triggering Pain from Pain ID (%d)", painIdNum);
			m_SuccessValueOfLastSay = AUD_SPEECH_SAY_SUCCEEDED;
			return AUD_SPEECH_SAY_SUCCEEDED;
		}
		else if (voiceNameHash == 0)
		{
			if(isPlayer)
				voiceNameHash = g_SpeechManager.GetWaveLoadedPainVoiceFromVoiceType(AUD_PAIN_VOICE_TYPE_PLAYER, m_Parent);
			else if(m_Parent)
				voiceNameHash = g_SpeechManager.GetWaveLoadedPainVoiceFromVoiceType(m_Parent->IsMale() ? AUD_PAIN_VOICE_TYPE_MALE : AUD_PAIN_VOICE_TYPE_FEMALE, m_Parent);
			else
			{	
				naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - voiceNameHash == 0");
				m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
				return AUD_SPEECH_SAY_FAILED;
			}
		}
	}
	

	BANK_ONLY(Color32 ambDebugMsgColor(255, 0, 0));

	//animals shouldn't speak
	if(m_AnimalParams && voiceNameHash != ATSTRINGHASH("reindeer", 0x6288E279))
	{
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - m_AnimalParams && voiceNameHash != ATSTRINGHASH(reindeer, 0x6288E279)");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	bool isDrowningContext = contextPHash == g_DrowningContextPHash || contextPHash == g_DrowningDeathContextPHash ||
							contextPHash == g_ComeUpForAirContextPHash || contextPHash == g_SharkScreamContextPHash;

	//Don't speak if underwater
	if(m_Parent && !isDrowningContext && m_IsUnderwater)
	{
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - m_Parent && !isDrowningContext && m_IsUnderwater");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	if(contextPHash == g_SharkScreamContextPHash)
	{
		voiceNameHash = g_SharkVoiceHash;
		contextPHash = atPartialStringHash(m_IsMale ? "_MALE" : "_FEMALE", contextPHash);
	}

	CPed* playerPed = FindPlayerPed();
	if(playerPed && m_Parent != playerPed)
	{
		//  player isn't in switch scene
		if(!speechInitParams.fromScript &&
			contextPHash != g_HighFallContextPHash && contextPHash != g_SuperHighFallContextPHash && contextPHash != g_HighFallNewContextPHash &&
			playerPed->GetPedIntelligence() &&
			playerPed->GetPedIntelligence()->GetQueriableInterface() &&
			playerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_SYNCHRONIZED_SCENE ))
		{
			naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - contextPHash != g_HighFallContextPHash && CTaskTypes::TASK_SYNCHRONIZED_SCENE");
			m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
			return AUD_SPEECH_SAY_FAILED;
		}

		// If you are in a bonnet cam and the player is in the same car as the ped then play dialog frontend(Armenian 3, Michael talking to Franklin with AUD_INTERRUPT_LEVEL_SCRIPTED)
		if(speechInitParams.isConversationInterrupt && speechInitParams.requestedVolume == AUD_AMBIENT_SPEECH_VOLUME_UNSPECIFIED && playerPed->GetVehiclePedInside()  && (playerPed->GetVehiclePedInside() == m_Parent->GetVehiclePedInside()))
		{
			speechInitParams.requestedVolume = AUD_AMBIENT_SPEECH_VOLUME_FRONTEND;
		}
	}

	//TODO: Pull this out and make it a __BANK only function that checks any bank-loaded speech contexts that might accidentally 
	// come thorugh here
	/*for(int i=AUD_PAIN_ID_PAIN_LOW_WEAK; i<AUD_PAIN_ID_NUM_BANK_LOADED; ++i)
	{
		if(contextPHash == atPartialStringHash(g_PainContexts[i]))
		{
			naAssertf(0, "Attempting to play bank-loaded pain through Say().  Context:%u Voice:%i", contextPHash, voiceNameHash);
			BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
			return AUD_SPEECH_SAY_FAILED;
		}
	}*/

	m_PositionForNullSpeaker = posForNullSpeaker;
	m_EntityForNullSpeaker = NULL;

	if(g_DisableAmbientSpeech)
	{
		Displayf("Can't Speak - Ambient speech disabled %x", atFinalizeHash(contextPHash));
		BANK_ONLY(Displayf("Failed to say context : %s", context);)

		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - g_DisableAmbientSpeech");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	if( (contextPHash == g_SpeechContextBumpHash || contextPHash == g_SpeechContextBumpPHash) && 
			!(m_LastPlayedSpeechContextPHash == g_SpeechContextBumpHash || m_LastPlayedSpeechContextPHash == g_SpeechContextBumpPHash) 
	  )
	{
		speechInitParams.interruptAmbientSpeech = true;
	}
	// for now, just don't allow any interrupting
	if(!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ForceConversationInterrupt))
	{
		if(!speechInitParams.interruptAmbientSpeech)
		{
			if (IsAmbientSpeechPlaying() || IsScriptedSpeechPlaying() || IsPainPlaying() || IsAnimalVocalizationPlaying())
			{
				Displayf("Can't Speak - 1.Speech already playing %x", atFinalizeHash(contextPHash));
				BANK_ONLY(Displayf("Failed to say context : %s", context);)

				ResetTriggeredContextVariables();
				BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
				naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - !speechInitParams.interruptAmbientSpeech || !audScriptAudioFlags::ForceConversationInterrupt && speech is playing");
				m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
				return AUD_SPEECH_SAY_FAILED;
			}
		}
		else 
		{
			if (IsScriptedSpeechPlaying() || IsPainPlaying() || IsAnimalVocalizationPlaying())
			{
				Displayf("Can't Speak - 2.Speech already playing %x", atFinalizeHash(contextPHash));
				BANK_ONLY(Displayf("Failed to say context : %s", context);)

				ResetTriggeredContextVariables();
				BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
				naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - !audScriptAudioFlags::ForceConversationInterrupt && Non-Ambient speech is playing");
				m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
				return AUD_SPEECH_SAY_FAILED;
			}
		}
	}

	if(!speechInitParams.fromScript && ShouldBlockContext(contextPHash))
	{
		Displayf("Can't Speak - Context blocked %x", contextPHash);
		BANK_ONLY(Displayf("Failed to say context : %s", context);)

		naDisplayf("Context blocked by script-set context block.");
		{
			naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - !speechInitParams.fromScript && ShouldBlockContext(atFinalizeHash(contextPHash))");
			m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
			return AUD_SPEECH_SAY_FAILED;
		}
	}

	//PED_RANT final hash is passed in from NewSay...usually handled later, but in this case we need to know its a multipart, so we have to set
	//	it to the propre partial hash
	if(contextPHash == 0x126918B5)
		contextPHash = 0x4EE60A0E;

	bool isMultipartContext = IsMultipartContext(contextPHash);
	if(isMultipartContext)
	{
		m_OriginalMultipartContextPHash = contextPHash;
		contextPHash = atPartialStringHash("_01", contextPHash); //we'll sort out which to actually use later
		m_MultipartContextCounter = 1;
	}

	//reset voice name if it was set on a previous call to Say that failed for some reason
	if(!m_SuccessfullyUsedAmbientVoice && !m_ForceSetAmbientVoice)
	{
		SetAmbientVoiceName((u32)0);
	}
	// We might be about to really play something, so we need to actually pick a voice if we've not already. (Unless we're overridden)
	if ( (m_AmbientVoiceNameHash==0 || m_AmbientVoiceNameHash==g_NoVoiceHash) && voiceNameHash==0)
	{
		ChooseVoiceIfUndecided(isMultipartContext ? m_OriginalMultipartContextPHash : contextPHash);
	}

	//change GENERIC_HIs from peds to KIFFLOM_GREETs if the player is the target and wearing epsilon robes
	if((contextPHash == g_SpeechContextGenericHiPHash || contextPHash == g_SpeechContextGenericHiHash) && playerPed)
	{
		if(replyingPed == playerPed || ( m_Parent && m_Parent->GetPedResetFlag(CPED_RESET_FLAG_TalkingToPlayer)))
		{
			if( (FindPlayerPed()->GetPedAudioEntity() && FindPlayerPed()->GetPedAudioEntity()->GetFootStepAudio().IsWearingKifflomSuit()) ||
				IsEpsilonPed(voiceNameHash))
			{
				contextPHash = g_SpeechContextKifflomGreetPHash;
			}
		}
		else if(m_Parent == playerPed && replyingPed)
		{
			contextPHash = GetSpecificGreetForPlayer(replyingPed);
		}
	}
	//Let's hold on to a copy of the original in case we end up trying to use a backup context.
	audSpeechInitParams backupSpeechInitParams(speechInitParams);

	SpeechContext* primaryContext = NULL;
	if(!m_TriggeredSpeechContext)
	{
		if (CNetwork::IsGameInProgress() && m_AmbientVoiceNameHash == ATSTRINGHASH("LESTER", 0x8DB38846) && contextPHash == ATPARTIALSTRINGHASH("HIT_BY_PLAYER", 0xE28EF7AE))
		{
			contextPHash = ATPARTIALSTRINGHASH("HIT_BY_PLAYER_LESTER", 0x2C657921);
		}
		//TriggeredContexts pass in finalized hash
		AudBaseObject* obj =(rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(contextPHash);
		m_TriggeredSpeechContext = (obj && obj->ClassId == TriggeredSpeechContext::TYPE_ID) ? (TriggeredSpeechContext*)obj : NULL;
		//errrrrr....unless they don't.  Seems alot of AI systems pass in finalized, but Say(const char*) turns it into a partial...seems
		//	easiest to fix this by just checking both
#if __ASSERT
		//look for collision and assert
		if(m_TriggeredSpeechContext)
		{
			rage::AudBaseObject* collisionCheckObj =(rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(atFinalizeHash(contextPHash));
			TriggeredSpeechContext* collisionCheckContext = (collisionCheckObj && collisionCheckObj->ClassId == TriggeredSpeechContext::TYPE_ID) ? (TriggeredSpeechContext*)collisionCheckObj : NULL;
			naAssertf(!collisionCheckContext, "Found a triggered context hash that works as both partial and full!! partial %s full %s",
				audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_TriggeredSpeechContext->NameTableOffset),
				audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(collisionCheckContext->NameTableOffset)
			);
		}
#endif
		if(!m_TriggeredSpeechContext)
		{
			obj =(rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(atFinalizeHash(contextPHash));
			m_TriggeredSpeechContext = (obj && obj->ClassId == TriggeredSpeechContext::TYPE_ID) ? (TriggeredSpeechContext*)obj : NULL;
		}
		if(m_TriggeredSpeechContext)
		{
			m_IsAttemptingTriggeredPrimary = true;
			primaryContext = 
				static_cast<SpeechContext*>(audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(m_TriggeredSpeechContext->PrimarySpeechContext));
			primaryContext = g_SpeechManager.ResolveVirtualSpeechContext(primaryContext, m_Parent, replyingPed);
			if(primaryContext)
				contextPHash = primaryContext->ContextNamePHash;
			else
			{
				naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - No PrimarySpeechContext set on TriggeredSpeechContext: %s", 
					audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_TriggeredSpeechContext->NameTableOffset));
				return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, 
														replyingContext, replyingDelay, replyProbability);
			}

		}
	}

	if (m_AmbientVoiceNameHash == g_ZombieVoice)
	{
		contextPHash = ATPARTIALSTRINGHASH("MOAN", 0x62AD4F30);
		voiceNameHash = 0;
	}

	// avoid doing the hash every time, just when speaking's disabled. Allow pain through
	if ((m_SpeakingDisabled || m_AllSpeechBlocked || m_SpeakingDisabledSynced) && voiceNameHash!=g_PainVoice && (!speechInitParams.fromScript || m_AllSpeechBlocked))
	{
		if (contextPHash!=g_SpeechContextMissionFailRagePHash && contextPHash!=g_SpeechContextMissionBlindRagePHash && 
			contextPHash!=g_SpeechContextKilledAllPHash && contextPHash!=g_SpeechContextMessingWithPhonePHash &&
			contextPHash!=g_HighFallContextPHash && contextPHash!=g_SuperHighFallContextPHash && contextPHash!=g_HighFallNewContextPHash)
		{
			ResetTriggeredContextVariables();
			BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
			naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - m_SpeakingDisabled && voiceNameHash!=g_PainVoice && !speechInitParams.fromScript");
			m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;

			Displayf("Can't Speak - Speaking Disabled %x", atFinalizeHash(contextPHash));
			BANK_ONLY(Displayf("Failed to say context : %s", context);)

			return AUD_SPEECH_SAY_FAILED;
		}
	}

	// Dead peds don't talk - may need to special case some contexts
	if(m_Parent && m_Parent->ShouldBeDead() && contextPHash!=g_DeathGarglePHash && contextPHash!=g_DeathHighLongPHash && 
		contextPHash!=g_DeathHighMediumPHash && contextPHash!=g_DeathHighShortPHash && contextPHash!=g_DeathUnderwaterPHash /*&& voiceNameHash != ATSTRINGHASH("reindeer", 0x6288E279)*/)
	{
		Displayf("Can't Speak - Dead %x", atFinalizeHash(contextPHash));
		BANK_ONLY(Displayf("Failed to say context : %s", context);)

		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - m_Parent && m_Parent->ShouldBeDead()");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

#if __BANK
	u32 uTestContext = atPartialStringHash(g_TestContext);
	if(contextPHash == uTestContext)
	{
		Displayf("Playing the context you're looking for.");
	}
#endif

	//SO: removed 
	// Don't let people speak in different interiors to us.
	//if (m_Parent && m_Parent->GetAudioOcclusionGroup() && ((audGtaOcclusionGroup*)m_Parent->GetAudioOcclusionGroup())->IsInADifferentInteriorToListener())
	//{
	//	return AUD_SPEECH_SAY_FAILED;
	//}

	//SO: removed 
	// Quick and dirty hack to stop taxi drivers talking during mission speech
	//if (g_ScriptAudioEntity.IsScriptedConversationOngoing() && !g_ScriptAudioEntity.IsScriptedConversationPaused() && IsPedInPlayerCar(m_Parent) &&
	//	(audNorthAudioEngine::GetGtaEnvironment()->GetLastTimeWeWereAPassengerInATaxi()+100 > CTimer::GetTimeInMilliseconds()))
	//{
	//	return AUD_SPEECH_SAY_FAILED;
	//}

	bool isPlayerCharacterVoice = m_Parent &&
		(m_Parent->GetPedType() == PEDTYPE_PLAYER_0 ||
		m_Parent->GetPedType() == PEDTYPE_PLAYER_1 ||
		m_Parent->GetPedType() == PEDTYPE_PLAYER_2);

	if(isPlayerCharacterVoice && voiceNameHash==0)
	{
		voiceNameHash = !m_PedIsDrunk && (m_IsAngry || ShouldBeAngry()) ? atStringHash("_ANGRY", m_AmbientVoiceNameHash) : atStringHash("_NORMAL", m_AmbientVoiceNameHash);
	}

	// Don't play anything if we've got NO_VOICE - ie. we've tried to pick, but we don't have one for this ped, and we're not overridden.
	if (m_AmbientVoiceNameHash==g_NoVoiceHash  && voiceNameHash==0)
	{
		Displayf("Can't Speak - No voice hash %x", atFinalizeHash(contextPHash));
		BANK_ONLY(Displayf("Failed to say context : %s", context);)

		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - m_AmbientVoiceNameHash==g_NoVoiceHash  && voiceNameHash==0");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

#if __BANK
	if(g_ForceToughCopVoice && 
		(m_AmbientVoiceNameHash==ATSTRINGHASH("S_AGGROCOP_01", 0xAFDD9368) || m_AmbientVoiceNameHash==ATSTRINGHASH("S_AGGROCOP_02", 0xE1C37733)))
	{
		speechInitParams.forcePlay = true;
		speechInitParams.allowRecentRepeat = true;
	}	
#endif

	// Use the overridden voice if one's passed in, otherwise use the usual.
	if (voiceNameHash == 0)
	{
		voiceNameHash = m_AmbientVoiceNameHash;
	}

	SpeechContext *speechContext = g_SpeechManager.GetSpeechContext(atFinalizeHash(atPartialStringHash("_SC", contextPHash)), m_Parent, replyingPed);
	naAssertf(speechContext || !g_bSpeechAsserts, "We do not have a valid SpeechContext game object for context: %u", contextPHash);

#if __BANK
	if(speechContext)
	{
		safecpy(debugDrawMsg, audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(speechContext->NameTableOffset));
		int i = 0;
		for(; i<255 && debugDrawMsg[i] != '\0'; i++);
		if(i >=3)
			debugDrawMsg[i-3] = '\0';
	}
#endif

	bool allowPlayerAISpeech = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowPlayerAIOnMission);
	bool allowBuddyAISpeech = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowBuddyAIOnMission);
	//For now, don't allow player AI speech during missions
	if((CTheScripts::GetPlayerIsOnAMission() || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::IsPlayerOnMissionForSpeech)) && 
		( (isPlayerCharacterVoice && !allowPlayerAISpeech) || (m_IsBuddy && !allowBuddyAISpeech) )&& 
		!speechInitParams.isConversationInterrupt  && !speechInitParams.fromScript &&
		(!speechContext || (AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISPAIN) == AUD_TRISTATE_FALSE &&
			AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISOKAYONMISSION) == AUD_TRISTATE_FALSE))
		)
	{
		Displayf("Can't Speak - On a mission fail case %x", atFinalizeHash(contextPHash));
		BANK_ONLY(Displayf("Failed to say context : %s", context);)

		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - CTheScripts::GetPlayerIsOnAMission() || g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::IsPlayerOnMissionForSpeech)");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	/*if(m_AllowedToFillBackupSpeechArray && speechContext)
	{
		SetupBackupContextArray(speechContext);
	}*/

	if(speechContext)
	{
		if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowCombatSay) && AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISCOMBAT) != AUD_TRISTATE_TRUE)
		{
			naWarningf("Attempting to play non-combat speech context when script has set OnlyAllowCombatSay.");
			BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor));
			naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OnlyAllowCombatSay) && !FLAG_ID_SPEECHCONTEXT_ISCOMBAT");
			m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
			return AUD_SPEECH_SAY_FAILED;
		}
		AlterSpeechInitParamsPassedAsArg(speechInitParams, speechContext);
	}

	BANK_ONLY(m_ContextToPlaySilentPHash = contextPHash;)

	if (BANK_ONLY(!g_BypassStreamingCheckForSpeech &&) g_SpeechManager.IsStreamingBusyForContext(contextPHash) && !speechInitParams.forcePlay)
	{
		naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - g_BypassStreamingCheckForSpeech && g_SpeechManager.IsStreamingBusyForContext(contextPHash) && !speechInitParams.forcePlay");
		return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingContext, replyingDelay, replyProbability);
	}

	// Do an initial distanceToPed cull, then make another call later, based on the volume of the line
	f32 distanceSqToNearestListener = m_Parent ? (g_AudioEngine.GetEnvironment().ComputeSqdDistanceRelativeToVolumeListenerV(m_Parent->GetMatrix().d())).Getf() : 0.0f;
	const f32 maxDistSqToPlayCopSpeech = g_MaxDistanceToPlayCopSpeech * g_MaxDistanceToPlayCopSpeech;
	if(distanceSqToNearestListener > maxDistSqToPlayCopSpeech && !speechInitParams.forcePlay)
	{
		naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - distanceSqToNearestListener > maxDistSqToPlayCopSpeech && !speechInitParams.forcePlay");
		return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingContext, replyingDelay, replyProbability);
	}

	// Do another distanceToPed check, because if it's not a shouted context, we don't play if it's a way off
	u32 contextVolume = speechContext ? speechContext->VolumeType : CONTEXT_VOLUME_NORMAL;
	if (contextVolume == CONTEXT_VOLUME_NORMAL && speechInitParams.requestedVolume == AUD_AMBIENT_SPEECH_VOLUME_UNSPECIFIED)
	{
		const f32 maxDistSqToPlaySpeech = g_SpeechManager.GetNumVacantAmbientSlots() > 3 ?
			(g_MaxDistanceToPlaySpeechIfFewSlotsInUse * g_MaxDistanceToPlaySpeechIfFewSlotsInUse) :
			(g_MaxDistanceToPlaySpeech * g_MaxDistanceToPlaySpeech);
		if (distanceSqToNearestListener > maxDistSqToPlaySpeech)	
		{
			naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - distanceSqToNearestListener > maxDistSqToPlaySpeech");
			return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingContext, replyingDelay, replyProbability);
		}
	}

	// Pick the actual context we want to use - we swap for alternatives if gender-sepcific in multiplayer, or if friend is drunk
	u32 alternateContextNamePHash = 0;
	if (m_PedIsDrunk)
	{
		alternateContextNamePHash = atPartialStringHash("_DRUNK", contextPHash);
		// don't use this, if the ped doesn't have it - quite a few don't, and sober's better than silent
		s32 numVariationsForDrunkVersion = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(voiceNameHash, alternateContextNamePHash);
		if (numVariationsForDrunkVersion>0)
		{
			contextPHash = alternateContextNamePHash;
			speechContext = g_SpeechManager.GetSpeechContext(atFinalizeHash(atPartialStringHash("_SC", contextPHash)), m_Parent, replyingPed);
			naAssertf(speechContext || !g_bSpeechAsserts, "TriggeredSpeechContext found when looking for drunk version SpeechContext: %u", contextPHash);
		}
	}

	// Don't let Player talk when deafening's on
	if (m_Parent && m_Parent->IsLocalPlayer() && contextPHash != g_OnFireContextPHash && 
		contextPHash != g_ShootoutSpecialContextPHash && contextPHash != g_ShootoutSpecialAloneContextPHash &&
		audNorthAudioEngine::GetGtaEnvironment()->IsDeafeningOn())
	{
		Displayf("Can't Speak - Deafening %x", atFinalizeHash(contextPHash));
		BANK_ONLY(Displayf("Failed to say context : %s", context);)

		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor););
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - audNorthAudioEngine::GetGtaEnvironment()->IsDeafeningOn()");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}
	// Pick the actual voice we want to use - this will be different for Niko, Roman, Brian, and pain
	voiceNameHash = SelectActualVoice(voiceNameHash);

	// If we're having a conversation, don't allow any ambient speech 
	if ( GetIsHavingANonPausedConversation() && !speechInitParams.isConversationInterrupt &&
		 g_ScriptAudioEntity.IsPedInCurrentConversation(m_Parent) && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::ScriptedConvListenerMaySpeak))
	{
		Displayf("Can't Speak - Conversation on going %x", atFinalizeHash(contextPHash));
		BANK_ONLY(Displayf("Failed to say context : %s", context);)

		ResetTriggeredContextVariables();
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor););
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - GetIsHavingANonPausedConversation() && !speechInitParams.isConversationInterrupt");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	// Can we play this context at this time
	u32 timeCanPlay = 0;
	bool timeControlledByContext = false;

	if(m_IsAttemptingTriggeredPrimary)
	{
		if(Verifyf(m_TriggeredSpeechContext, "Attempting a triggered primary when the m_TriggeredSpeechContext is NULL."))
		{
			//Check against the context, regardless of which TriggeredContext triggered it, but use the times in the TriggeredContext
			if(Verifyf(primaryContext, "Attempting a triggered primary when the primaryContext object is NULL."))
			{
				if(m_TriggeredSpeechContext->TimeCanNextUseTriggeredContext > currentTime && !speechInitParams.forcePlay)
				{
					//Displayf("Can't Speak - Some time thing %x", atFinalizeHash(contextPHash));
					//BANK_ONLY(Displayf("Failed to say context : %s", context);)

					naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - (m_TriggeredSpeechContext->TimeCanNextUseTriggeredContext (%u) > currentTime (%u) && !speechInitParams.forcePlay",
						m_TriggeredSpeechContext->TimeCanNextUseTriggeredContext, currentTime);
					ResetTriggeredContextVariables();
					BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor););
					m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
					return AUD_SPEECH_SAY_FAILED;
				}

				//g_SpeechManager.GetTimeWhenContextCanNextPlay(primaryContext, &timeCanPlay, &timeControlledByContext);
				u32 timeLastPlayed = g_SpeechManager.GetTimeLastPlayedForContext(primaryContext->ContextNamePHash);
				if(m_TriggeredSpeechContext->PrimaryRepeatTime > -1 && currentTime - timeLastPlayed < m_TriggeredSpeechContext->PrimaryRepeatTime 
					&& (!speechInitParams.forcePlay || !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::IsDirectorModeActive)))
				{
					naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - currentTime (%u) - timeLastPlayed (%u) < m_TriggeredSpeechContext->PrimaryRepeatTime (%d)",
						currentTime, timeLastPlayed, m_TriggeredSpeechContext->PrimaryRepeatTime);
					return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, 
						replyingContext, replyingDelay, replyProbability);
				}
			}
		}
	}
	
	if (m_IsAttemptingTriggeredPrimary || m_IsAttemptingTriggeredBackup)
	{
		u32 timePrimaryCanPlay = 0;
		u32 timeBackupCanPlay = 0;

		g_SpeechManager.GetTimeWhenTriggeredPrimaryContextCanNextPlay(m_TriggeredSpeechContext, &timePrimaryCanPlay, &timeControlledByContext);
		g_SpeechManager.GetTimeWhenTriggeredBackupContextCanNextPlay(m_TriggeredSpeechContext, &timeBackupCanPlay, &timeControlledByContext);

		
		timeCanPlay = m_IsAttemptingTriggeredPrimary ? Max(timePrimaryCanPlay, timeBackupCanPlay) : timeBackupCanPlay;
	}
	else
		g_SpeechManager.GetTimeWhenContextCanNextPlay(speechContext, &timeCanPlay, &timeControlledByContext);

	if (timeControlledByContext && timeCanPlay>currentTime DEV_ONLY(&& !g_MakePedSpeak && !(g_MakePedDoMegaphone||g_MakePedDoPanic||g_MakePedDoBlocked||g_MakePedDoHeli||g_MakePedDoCoverMe)) && !speechInitParams.forcePlay)
	{
		naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - timeControlledByContext && timeCanPlay (%u) > currentTime (%u)", timeCanPlay, currentTime);
		return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingDelay, replyingContext, replyProbability);
	}

	//Pick a random variation. Do this via our proper managed non-repeating code, then register the one we've use.
	u32 timeContextLastPlayedForThisVoice = 0; // ped would be better, but never mind
	bool hasBeenPlayedRecently = false;
	
	// The network needs to know which variation we played so 
	// we can sync it correctly so if we've passed a pointer in use that,
	s32 variationNum = -1;
	s32* pVariationNum = &variationNum;
	if(variationNumberOverride)
	{	
		pVariationNum = variationNumberOverride;
	}

	if(isMultipartContext)
	{
		naCErrorf(m_MultipartContextPHash == 0, "Triggering a multipart context when it appears we're already playing one.");

		m_MultipartVersion = GetRandomVariation(voiceNameHash, m_OriginalMultipartContextPHash, &timeContextLastPlayedForThisVoice,  &hasBeenPlayedRecently, true);
		//if it's a PED_RANT we've play recently, fall back to generic hi
		if(!hasBeenPlayedRecently && m_OriginalMultipartContextPHash == 0x4EE60A0E)
		{
			char versionBuffer[4];
			formatf(versionBuffer, 4, "_%02u", m_MultipartVersion);
			m_MultipartContextPHash = atPartialStringHash(versionBuffer, m_OriginalMultipartContextPHash);
			m_MultipartContextNumLines = audSpeechSound::FindNumVariationsForVoiceAndContext(voiceNameHash, m_MultipartContextPHash);
		}
		else
		{
			contextPHash = 0xA6B1F3DE; //GENERIC_WHATEVER
			isMultipartContext = false;
			m_MultipartContextPHash = 0;
		}
	}

	timeContextLastPlayedForThisVoice = 0; // ped would be better, but never mind
	hasBeenPlayedRecently = false;
	if( ( variationNumberOverride!=NULL ) && ( (*variationNumberOverride) > 0 ) )
	{
		// sanity check that this is a valid variation for this voice and context, or something's gone tits up.
		// note that our var num can be == the num vars
		s32 numVariationsCheck = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(voiceNameHash, contextPHash);
		naCErrorf(numVariationsCheck>=((s32)(*variationNumberOverride)), "Overriden speech variation number out of valid range - probably a taunt bug");
		if (numVariationsCheck<((s32)(*variationNumberOverride)))
		{
			naWarningf("Overriden var num: %d; max vars: %d; context: %u; vnh: %u", *variationNumberOverride, numVariationsCheck, contextPHash, voiceNameHash);
			naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - numVariationsCheck (%d) < *variationNumberOverride (%d)", numVariationsCheck, (s32)(*variationNumberOverride));
			return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, 
				replyingContext, replyingDelay, replyProbability);
		}

		(*pVariationNum) = (*variationNumberOverride);
	}
	else 
	{
		(*pVariationNum) = GetRandomVariation(voiceNameHash, contextPHash, &timeContextLastPlayedForThisVoice, &hasBeenPlayedRecently);//, m_VoiceName);
	}

	if ((*pVariationNum) < 0)
	{
		if(isPlayerCharacterVoice)
		{
			u32 finalAmbVoiceNameHash = atFinalizeHash(m_AmbientVoiceNameHash);
			(*pVariationNum) = GetRandomVariation(finalAmbVoiceNameHash, contextPHash, &timeContextLastPlayedForThisVoice, &hasBeenPlayedRecently);//, m_VoiceName);
			if((*pVariationNum) > 0)
				voiceNameHash = finalAmbVoiceNameHash;
			else
			{
				naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - (*pVariationNum) < 0 && isPlayerCharacterVoice");
				return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingDelay, replyingContext, replyProbability);
			}
		}
		else
		{
			naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - (*pVariationNum) < 0");
			return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingDelay, replyingContext, replyProbability);
		}
	}

#if __BANK
	if(speechContext)
	{
		char buf[255];
		safecpy(buf, audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(speechContext->NameTableOffset));
		int i = 0;
		for(; i<255 && buf[i] != '\0'; i++);
		if(i >=3)
			buf[i-3] = '\0';
		formatf(debugDrawMsg, 255, "%s_%.2i", buf, (*pVariationNum));
	}
	else
		formatf(debugDrawMsg, 255, "%s_%.2i", context, (*pVariationNum));
#endif
	
	if(m_MultipartContextPHash != 0)
		(*pVariationNum) = m_MultipartContextCounter++;

	// See if we want to play it. We don't let peds repeat contexts too often, and we don't let them repeat actual phrases at all.
	if (hasBeenPlayedRecently DEV_ONLY(&& !g_MakePedSpeak && !g_MakePedDoMegaphone) && !(speechInitParams.forcePlay||speechInitParams.allowRecentRepeat))
	{
		naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - hasBeenPlayedRecently");
		return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingDelay, replyingContext, replyProbability);
	}
	// custom thing - if we've already used the MOBILE_INTRO phrase, use the GENERIC_HI one instead, as we force this context. What are you calling me for, detective?
	if (hasBeenPlayedRecently && contextPHash==g_SpeechContextMobileIntroPHash)
	{
		contextPHash = g_SpeechContextGenericHiPHash;
		(*pVariationNum) = GetRandomVariation(voiceNameHash, contextPHash, &timeContextLastPlayedForThisVoice, &hasBeenPlayedRecently);
		if ((*pVariationNum)<0)
		{
			naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - hasBeenPlayedRecently && contextPHash==g_SpeechContextMobileIntroPHash");
			return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, 
				replyingContext, replyingDelay, replyProbability);
		}
	}


	u32 repeatTimeContextCanPlayOnSameVoice;
	if(m_IsAttemptingTriggeredPrimary)
		repeatTimeContextCanPlayOnSameVoice = g_SpeechManager.GetRepeatTimePrimaryTriggeredContextCanPlayOnVoice(m_TriggeredSpeechContext);
	else if (m_IsAttemptingTriggeredBackup)
		repeatTimeContextCanPlayOnSameVoice = g_SpeechManager.GetRepeatTimeBackupTriggeredContextCanPlayOnVoice(m_TriggeredSpeechContext);
	else
		repeatTimeContextCanPlayOnSameVoice = g_SpeechManager.GetRepeatTimeContextCanPlayOnVoice(speechContext);

	if ((timeContextLastPlayedForThisVoice + repeatTimeContextCanPlayOnSameVoice)>currentTime DEV_ONLY(&& !g_MakePedSpeak && !g_MakePedDoMegaphone) && !speechInitParams.forcePlay)
	{
		m_MultipartVersion = 0;
		m_MultipartContextCounter = 0;
		m_MultipartContextPHash = 0;
		m_OriginalMultipartContextPHash = 0;
		naSpeechEntDebugf2(this, "AttemptSayWithBackupContext - timeContextLastPlayedForThisVoice (%u) + repeatTimeContextCanPlayOnSameVoice (%u) > currentTime (%u)",
			timeContextLastPlayedForThisVoice, repeatTimeContextCanPlayOnSameVoice, currentTime);
		return AttemptSayWithBackupContext(backupSpeechInitParams, voiceNameHash, preDelay, replyingPed, replyingContext, replyingDelay, replyProbability);
	}

	
	// Can we get a free speech slot
	f32 priority = Max((f32)m_AmbientVoicePriority, speechInitParams.fromScript ? g_ScriptedSpeechAmbientSlotPriority : 0.0f);
	if(speechContext)
	{
		priority += speechContext->Priority;
		priority = priority/2.0f;
	}
	bool hadToStopSpeech = false;
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread() ? CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() : THREAD_INVALID;
	s32 slot = m_AmbientSlotId;
	if(m_AmbientSlotId < 0)
	{
		slot = g_SpeechManager.GetAmbientSpeechSlot(this, &hadToStopSpeech, priority, scriptThreadId
#if __BANK && AUD_DEFERRED_SPEECH_DEBUG_PRINT
			, context
#endif
			);
	}
	else
	{
		if(!g_SpeechManager.IsSlotVacant(m_AmbientSlotId))
		{
			hadToStopSpeech = true;
		}
	}
	
	if (slot<0)
	{
		if(NetworkInterface::IsGameInProgress())
		{
			Displayf("Can't Speak - No speech slot %x", atFinalizeHash(contextPHash));
			BANK_ONLY(Displayf("Failed to say context : %s", context);)
		}

		ResetTriggeredContextVariables();
		m_MultipartVersion = 0;
		m_MultipartContextCounter = 0;
		m_MultipartContextPHash = 0;
		m_OriginalMultipartContextPHash = 0;
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor););
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - Couldn't find ambient slot");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_FAILED;
		return AUD_SPEECH_SAY_FAILED;
	}

	// We've got a slot, so create our speech sound, and get variation info
	// if we've got a parent, go get the occlusion group off of its pedAudioEntity
	m_AmbientSlotId = slot;

	// We can't clear or assign if the call is coming from the audio thread as these are RegdEnt's.  So instead set a bool letting us know to NULL them out
	// once in the ::Update which is on the main thread.
	if(!sysThreadType::IsUpdateThread() && audNorthAudioEngine::IsAudioUpdateCurrentlyRunning())
	{
		if(m_ReplyingPed)
		{
			m_ShouldResetReplyingPed = true;
		}
	}
	else
	{
		// if we've got any reply speech to play, regref the other ped and store the context. Otherwise null it out, so we don't play the last one.
		if (m_ReplyingPed)
		{
			m_ReplyingPed = NULL;
		}
		// don't always play the reply, if we have an associated probability - particularly useful for Nico replies, as we force replies to play
		if (replyingPed && audEngineUtil::ResolveProbability(replyProbability))
		{
			m_ReplyingPed = replyingPed;
			AssertEntityPointerValid( ((CEntity*)m_ReplyingPed) );
			m_ReplyingContext = replyingContext;
			m_ReplyingDelay = replyingDelay;
		}
		
		m_ShouldResetReplyingPed = false;
	}

	if(m_TriggeredSpeechContext)
	{
		g_SpeechManager.UpdateTimeTriggeredContextWasLastUsed(m_TriggeredSpeechContext, currentTime);
	}
	if(m_IsAttemptingTriggeredPrimary)
	{
		g_SpeechManager.UpdateTimePrimaryTriggeredContextWasLastUsed(m_TriggeredSpeechContext, currentTime);
		//Do not allow backups to play until "BackupTime" ms after the primary plays
		g_SpeechManager.UpdateTimeBackupTriggeredContextWasLastUsed(m_TriggeredSpeechContext, currentTime);
	}
	else if(m_IsAttemptingTriggeredBackup)
	{
		g_SpeechManager.UpdateTimeBackupTriggeredContextWasLastUsed(m_TriggeredSpeechContext, currentTime);
	}
	else
	{
		g_SpeechManager.UpdateTimeContextWasLastUsed(speechContext, currentTime);
	}
	
	//We load the deferredSayStruct to be used in CompleteSay(), even if the speech isn't deferred and we call CompleteSay immediately
	u32 modVariation = 0;
#if __BANK
	safecpy(m_DeferredSayStruct.context, context);
	safecpy(m_DeferredSayStruct.debugDrawMsg, debugDrawMsg);
#endif
	m_DeferredSayStruct.preDelay = preDelay + (contextPHash == g_SharkScreamContextPHash ? 1000 : 0);
	m_DeferredSayStruct.requestedVolume = speechInitParams.requestedVolume;
	m_DeferredSayStruct.requestedAudibility = speechInitParams.requestedAudibility;
	m_DeferredSayStruct.voiceNameHash = voiceNameHash;
	m_DeferredSayStruct.contextPHash = CheckForModifiedVersion(contextPHash, voiceNameHash, modVariation);
	m_DeferredSayStruct.variationNumber = modVariation == 0 ? (*pVariationNum) : modVariation;
	m_DeferredSayStruct.preloadTimeOutInMilliseconds = speechInitParams.preloadTimeoutInMilliseconds;
	m_DeferredSayStruct.speechContext = speechContext;
	m_DeferredSayStruct.preloadOnly = speechInitParams.preloadOnly;
	m_DeferredSayStruct.priority = priority;
	m_DeferredSayStruct.fromScript = speechInitParams.fromScript;
	m_DeferredSayStruct.isMultipartContext = isMultipartContext;
	m_DeferredSayStruct.addBlip = speechInitParams.addBlip;
	m_DeferredSayStruct.isUrgent = speechInitParams.isUrgent;
	m_DeferredSayStruct.isMutedDuringSlowMo = speechInitParams.isMutedDuringSlowMo && contextPHash != sm_TrevorRageContextOverridePHash;
	m_DeferredSayStruct.isPitchedDuringSlowMo = speechInitParams.isPitchedDuringSlowMo && contextPHash != sm_TrevorRageContextOverridePHash;

	(*pVariationNum) = m_DeferredSayStruct.variationNumber;

	if(contextPHash == g_SpeechContextChatStatePHash || contextPHash == g_SpeechContextGenericHowsItGoingPHash || 
		contextPHash == g_SpeechContextChatAcrossStreetStatePHash)
		sm_TimeOfLastChat = currentTime;

	m_SuccessfullyUsedAmbientVoice = true;

	REPLAY_ONLY(if(CReplayMgr::IsEditModeActive()) { replayWarningf("audSpeechAudioEntity::Say shouldn't be called on replay playback"); } )

	if(hadToStopSpeech)
	{
		m_WaitingForAmbientSlot = true;
		naSpeechEntDebugf2(this, "AUD_SPEECH_SAY_DEFERRED");
		m_SuccessValueOfLastSay = AUD_SPEECH_SAY_DEFERRED;
		m_PreloadFreeTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + speechInitParams.preloadTimeoutInMilliseconds;
		ResetTriggeredContextVariables();
#if __BANK
		ambDebugMsgColor.Set(0,0,255);
		AddAmbSpeechDebugDrawStruct(debugDrawMsg, m_Parent, ambDebugMsgColor);

		if(NetworkInterface::IsGameInProgress())
		{
			audDisplayf("AUD_SPEECH_SAY_DEFERRED %x", atFinalizeHash(contextPHash));
			BANK_ONLY(audDisplayf("AUD_SPEECH_SAY_DEFERRED context : %s", context);)
		}
#endif

		return AUD_SPEECH_SAY_DEFERRED;
	}

	m_SuccessValueOfLastSay = CompleteSay();
	
#if GTA_REPLAY
	if(m_SuccessValueOfLastSay != AUD_SPEECH_SAY_FAILED)
	{
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketSpeech>(
				CPacketSpeech(&m_DeferredSayStruct, true, posForNullSpeaker), m_Parent, replyingPed);
		}
	}
#endif // GTA_REPLAY

	return m_SuccessValueOfLastSay;
}

audSaySuccessValue audSpeechAudioEntity::Say(const char* context, const char *speechParamsObject, u32 voiceNameHash, s32 preDelay, CPed* replyingPed, 
											 u32 replyingContext, s32 replyingDelay, f32 replyProbability, bool fromScript, s32 *const variationNumberOverride, const Vector3& posForNullSpeaker)
{

#if __BANK
	if(NetworkInterface::IsGameInProgress())
	{
		naDisplayf("Say : %s", context);
	}
#endif

	return Say(atPartialStringHash(context), speechParamsObject, voiceNameHash, preDelay, replyingPed, 
		             replyingContext, replyingDelay, replyProbability, fromScript, variationNumberOverride, posForNullSpeaker
#if __BANK
						, context				 
#endif
					 );
}

audSaySuccessValue audSpeechAudioEntity::Say(u32 contextPHash, const char *speechParamsObject, u32 voiceNameHash, s32 preDelay, CPed* replyingPed, 
							   u32 replyingContext, s32 replyingDelay, f32 replyProbability, bool fromScript, s32 *const variationNumberOverride, const Vector3& posForNullSpeaker
#if __BANK
									, const char* context
#endif
							   )
{	
	SpeechParams* pSpeechParams = audNorthAudioEngine::GetObject<SpeechParams>(speechParamsObject);
	if(Verifyf(pSpeechParams, "We do not have a valid SpeechParams game object with name: %s", speechParamsObject))
	{
#if __BANK
		naSpeechEntDebugf1(this, "Say() with contextPHash: %u contextString: %s params: %s", contextPHash, context, speechParamsObject);
#else
		naSpeechEntDebugf1(this, "Say() with contextPHash: %u contextString: %s params %s", contextPHash, "__BANK only", speechParamsObject);
#endif

		audSpeechInitParams speechInitParams;
		FillSpeechInitParams(speechInitParams, pSpeechParams);
		speechInitParams.fromScript = fromScript;

		return Say(contextPHash, speechInitParams, voiceNameHash, preDelay, replyingPed, replyingContext, replyingDelay, replyProbability,
			variationNumberOverride, posForNullSpeaker
#if __BANK
			, context
#endif
			);
	}

	naSpeechEntDebugf2(this, "!SpeechParams for %s", speechParamsObject);
	return AUD_SPEECH_SAY_FAILED;
}

void audSpeechAudioEntity::SayWhenSafe(const char* context, const char* speechParams)
{
	naSpeechEntDebugf2(this, "SayWhenSafe(%s, %s)", context, speechParams);

#if __ASSERT
	if(atPartialStringHash(context) == atPartialStringHash("SCREAM_SHOCKED"))
		naAssertf(0, "Trying to play SCREAM_SHOCKED as ambient speech.  Send callstack to audio.");
#endif

	safecpy(m_ContextToPlaySafely, context);
	safecpy(m_PlaySafelySpeechParams, speechParams);
	m_HasContextToPlaySafely = true;
}

void audSpeechAudioEntity::ResetTriggeredContextVariables()
{
	m_TriggeredSpeechContext = NULL;
	m_IsAttemptingTriggeredPrimary = false;
	m_IsAttemptingTriggeredBackup = false;
}

void audSpeechAudioEntity::FillSpeechInitParams(audSpeechInitParams& speechInitParams, const SpeechParams* pSpeechParams)
{
	if (!Verifyf(pSpeechParams, "Missing speechParams object.  Using default speech params settings."))
	{
		return;
	}

	speechInitParams.forcePlay = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_FORCEPLAY) == AUD_TRISTATE_TRUE;

	speechInitParams.allowRecentRepeat = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_ALLOWREPEAT) == AUD_TRISTATE_TRUE;

	speechInitParams.repeatTime = pSpeechParams->RepeatTime;
	speechInitParams.repeatTimeOnSameVoice = pSpeechParams->RepeatTimeOnSameVoice;

	speechInitParams.requestedVolume = pSpeechParams->RequestedVolume;
	speechInitParams.requestedAudibility = (SpeechAudibilityType)pSpeechParams->Audibility;

	speechInitParams.isConversation = false;
	speechInitParams.sayOverPain = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_SAYOVERPAIN) == AUD_TRISTATE_TRUE;
	speechInitParams.preloadOnly = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_PRELOADONLY) == AUD_TRISTATE_TRUE;
	speechInitParams.preloadTimeoutInMilliseconds = pSpeechParams->PreloadTimeoutInMs;
	speechInitParams.isConversationInterrupt = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_ISCONVERSATIONINTERRUPT) == AUD_TRISTATE_TRUE;
	speechInitParams.addBlip = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_ADDBLIP) == AUD_TRISTATE_TRUE;
	speechInitParams.isUrgent = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_ISURGENT) == AUD_TRISTATE_TRUE;
	speechInitParams.interruptAmbientSpeech = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_INTERRUPTAMBIENTSPEECH) == AUD_TRISTATE_TRUE;
	speechInitParams.isMutedDuringSlowMo = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_ISMUTEDDURINGSLOWMO) != AUD_TRISTATE_FALSE;
	speechInitParams.isPitchedDuringSlowMo = AUD_GET_TRISTATE_VALUE(pSpeechParams->Flags, FLAG_ID_SPEECHPARAMS_ISPITCHEDDURINGSLOWMO) != AUD_TRISTATE_FALSE;
}

void audSpeechAudioEntity::AlterSpeechInitParamsPassedAsArg(audSpeechInitParams& speechInitParams, const SpeechContext* speechContext)
{
	if(speechInitParams.allowContextToOverride)
	{
		speechInitParams.forcePlay |= AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_FORCEPLAY) == AUD_TRISTATE_TRUE;
		
		// HL - I think this is a bug, we should be a doing an |= as with the forcePlay. As it is, if we trigger any speech that doesn't have a bespoke
		// speech context, we fall back to using DEFAULT_SPEECH_CONTEXT which does not have ALLOWREPEAT set, and therefore just clears the flag. This basically
		// makes SPEECH_PARAMS_ALLOW_REPEAT completely useless unless you also provide a custom SpeechContext.
		speechInitParams.allowRecentRepeat = AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ALLOWREPEAT) == AUD_TRISTATE_TRUE;
		speechInitParams.repeatTime = speechContext->RepeatTime;
		speechInitParams.repeatTimeOnSameVoice = speechContext->RepeatTimeOnSameVoice;
	}
	
	speechInitParams.isConversation = AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISCONVERSATION) == AUD_TRISTATE_TRUE;
	speechInitParams.isUrgent |= AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISURGENT) == AUD_TRISTATE_TRUE;
}

u32 audSpeechAudioEntity::SelectBackupContext(TriggeredSpeechContext* context)
{
	if(!context || context->numBackupSpeechContexts == 0)
		return 0;

	f32 fTotalWeight = 0.0f;
	for(int i=0; i < context->numBackupSpeechContexts; ++i)
	{
		fTotalWeight += context->BackupSpeechContext[i].Weight;
	}

	if(fTotalWeight==0.0f)
		return 0;

	f32 fRandomValue = audEngineUtil::GetRandomNumberInRange(0.0f, fTotalWeight);
	f32 fRunningSum = 0.0f;

	for(int i=0; i < context->numBackupSpeechContexts; ++i)
	{
		fRunningSum += context->BackupSpeechContext[i].Weight;
		if(fRunningSum >= fRandomValue)
		{
			return context->BackupSpeechContext[i].SpeechContext;
		}
	}

	return 0;
}

audSaySuccessValue audSpeechAudioEntity::AttemptSayWithBackupContext(audSpeechInitParams speechInitParams, u32 voiceNameHash, 
												s32 preDelay, CPed* replyingPed, u32 replyingContext, s32 replyingDelay, f32 replyProbability)
{
#if __BANK
	Color32 ambDebugMsgColor(255, 0, 0);
	const char* debugMsg = m_TriggeredSpeechContext ? audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_TriggeredSpeechContext->NameTableOffset) : "UNKNOWN";
#endif

	if(m_IsAttemptingTriggeredBackup || !m_TriggeredSpeechContext || m_TriggeredSpeechContext->numBackupSpeechContexts == 0)
	{
#if !__FINAL
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - No backup speech contexts for TriggeredSpeechContext: %s", 
			m_TriggeredSpeechContext ? audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_TriggeredSpeechContext->NameTableOffset) : "NULL");
#else
		naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - No backup speech contexts for TriggeredSpeechContext: %s", "");
#endif
		BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugMsg, m_Parent, ambDebugMsgColor);)
		ResetTriggeredContextVariables();

		return AUD_SPEECH_SAY_FAILED;
	}

	m_IsAttemptingTriggeredPrimary = false;
	m_IsAttemptingTriggeredBackup = true;

	u32 backupHash = SelectBackupContext(m_TriggeredSpeechContext);
	SpeechContext* backupContext = g_SpeechManager.GetSpeechContext(backupHash, m_Parent, replyingPed);
	if(backupContext)
	{
#if !__FINAL
		naSpeechEntDebugf2(this, "Attempting Backup Say() with contextPHash: %u contextString: %s", backupContext->ContextNamePHash, 
			audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(backupContext->NameTableOffset));
#else
		naSpeechEntDebugf2(this, "Attempting Backup Say() with contextPHash: %u contextString: %s", backupContext->ContextNamePHash, "");
#endif

		return Say(backupContext->ContextNamePHash, speechInitParams, voiceNameHash, preDelay, replyingPed, replyingContext, replyingDelay, replyProbability, 0, ORIGIN
#if __BANK
			, audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_TriggeredSpeechContext->NameTableOffset)
#endif
			);
	}

#if !__FINAL
	naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - Could not find backup SpeechContext* from TriggeredSpeechContext: %s with backupHash: %u", 
		m_TriggeredSpeechContext ? audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(m_TriggeredSpeechContext->NameTableOffset) : "",
		backupHash);
#else
	naSpeechEntDebugf1(this, "AUD_SPEECH_SAY_FAILED - Could not find backup SpeechContext* from TriggeredSpeechContext: %s with backupHash: %u", "", backupHash);
#endif
	BANK_ONLY(AddAmbSpeechDebugDrawStruct(debugMsg, m_Parent, ambDebugMsgColor);)
	ResetTriggeredContextVariables();

	return AUD_SPEECH_SAY_FAILED;
}

bool audSpeechAudioEntity::CanSayAngryLineToPlayer(CPed* jackedPed, u32& angryLineContext)
{
	static const u32 gangHash = ATSTRINGHASH("GANG", 0xBC066B98);
	CPed* player = FindPlayerPed();
	if(!player || !jackedPed)
		return false;

	//let all gang types play angry lines
	if(jackedPed->GetPedModelInfo() && jackedPed->GetPedModelInfo()->GetCombatInfoHash() == gangHash)
		return true;

	s8 toughness = GetPedToughness();

	if (player->GetPedType() == PEDTYPE_PLAYER_0)
	{
		//any normal or tough ped will talk back to michael
		angryLineContext = g_AngryWithPlayerMichaelContextPHash;
		return toughness != AUD_TOUGHNESS_MALE_COWARDLY;
	}
	else if (player->GetPedType() == PEDTYPE_PLAYER_1)
	{
		//only tough and gang peds will talk back to Franklin
		angryLineContext = g_AngryWithPlayerFranklinContextPHash;
		return toughness == AUD_TOUGHNESS_MALE_BRAVE || toughness == AUD_TOUGHNESS_COP || toughness == AUD_TOUGHNESS_MALE_GANG;
	}
	else if (player->GetPedType() == PEDTYPE_PLAYER_2)
	{
		//everyone talks back to Trevor
		angryLineContext = g_AngryWithPlayerTrevorContextPHash;
		return true;
	}

	return false;
}

int audSpeechAudioEntity::LookForPainContextInSay(u32 contextPHash, bool isPlayer, bool& isBankLoadedPain, bool& isPartialHash)
{
	isBankLoadedPain = false;
	isPartialHash = false;

	for(int i=0; i<AUD_PAIN_ID_NUM_BANK_LOADED; ++i)
	{
		if(contextPHash == (isPlayer ? g_PlayerPainContextPHashs[i] : g_PainContextPHashs[i]))
		{
			isBankLoadedPain = true;
			isPartialHash = true;
			return i;
		}
		else if(contextPHash == (isPlayer ? g_PlayerPainContextHashs[i] : g_PainContextHashs[i]))
		{
			isBankLoadedPain = true;
			return i;
		}
	}

	for(int i=AUD_PAIN_ID_NUM_BANK_LOADED + 1; i<AUD_PAIN_ID_NUM_TOTAL ;i++)
	{
		if(contextPHash == (isPlayer ? g_PlayerPainContextPHashs[i] : g_PainContextPHashs[i]))
		{
			isPartialHash = true;
			return i;
		}
		else if(contextPHash == (isPlayer ? g_PlayerPainContextHashs[i] : g_PainContextHashs[i]))
		{
			return i;
		}
	}

	return -1;
}

bool audSpeechAudioEntity::IsEpsilonPed(u32 voiceNameHash)
{
	for(int i=0; i<g_NumEpsilonVoices; i++)
	{
		if(m_AmbientVoiceNameHash == g_EpsilonVoiceHashes[i] || voiceNameHash == g_EpsilonVoiceHashes[i])
			return true;
	}

	return false;
}

u32 audSpeechAudioEntity::GetSpecificGreetForPlayer(CPed* replyingPed)
{
	if(replyingPed)
	{
		if(replyingPed->GetSpeechAudioEntity())
		{
			u32 pvg = replyingPed->GetSpeechAudioEntity()->GetPedVoiceGroup(true);

			s8 toughness = replyingPed->GetSpeechAudioEntity()->GetPedToughnessWithoutSetting();
			switch(toughness)
			{
			case AUD_TOUGHNESS_MALE_BUM:
			case AUD_TOUGHNESS_FEMALE_BUM:
				return g_SpeechContextGreetBumPHash;
			case AUD_TOUGHNESS_COP:
				return g_SpeechContextGreetCopPHash;
			default:
				break;
			}

			if(pvg == ATSTRINGHASH("G_M_Y_BALLAEAST_01_BLACK_PVG", 0x466DD548) ||
				pvg == ATSTRINGHASH("G_M_Y_BALLAORIG_01_BLACK_PVG", 0x72F6EF48) ||
				pvg == ATSTRINGHASH("G_M_Y_BALLASOUT_01_BLACK_PVG", 0x6CDAB80))
				return g_SpeechContextGreetBallasMalePHash;
			else if(pvg == ATSTRINGHASH("G_F_Y_BALLAS_01_BLACK_PVG", 0xF387D1B5))
				return g_SpeechContextGreetBallasFemalePHash;
			else if(pvg == ATSTRINGHASH("G_M_Y_FAMDNF_01_BLACK_PVG", 0x98AD3055) ||
				pvg == ATSTRINGHASH("G_M_Y_FAMCA_01_BLACK_PVG", 0xE78C7A5F) ||
				pvg == ATSTRINGHASH("G_M_Y_FamFor_01_BLACK_PVG", 0x135B9067))
				return g_SpeechContextGreetFamiliesMalePHash;
			else if(pvg == ATSTRINGHASH("G_F_Y_FAMILIES_01_BLACK_PVG", 0xFBED65C9))
				return g_SpeechContextGreetFamiliesFemalePHash;
			else if(pvg == ATSTRINGHASH("G_M_Y_SALVAGOON_01_SALVADORIAN_PVG", 0xEF10B5E) ||
				pvg == ATSTRINGHASH("G_M_Y_SALVAGOON_02_SALVADORIAN_PVG", 0xB9C1880C) ||
				pvg == ATSTRINGHASH("G_M_Y_SALVAGOON_03_SALVADORIAN_PVG", 0xA327DE8))
				return g_SpeechContextGreetVagosMalePHash;
			else if(pvg == ATSTRINGHASH("G_F_Y_VAGOS_01_LATINO_PVG", 0xE3BFCEF4))
				return g_SpeechContextGreetVagosFemalePHash;
			else if(pvg == ATSTRINGHASH("A_M_M_HILLBILLY_01_WHITE_PVG", 0xFB2EF39E) ||
			   pvg == ATSTRINGHASH("A_M_M_HILLBILLY_02_WHITE_PVG", 0x9A36C092) )
			   return g_SpeechContextGreetHillbillyMalePHash;
			else if(pvg == ATSTRINGHASH("A_M_Y_HIPPY_01_WHITE_PVG", 0xF3B149E9))
				return g_SpeechContextGreetHippyMalePHash;
			else if(pvg == ATSTRINGHASH("A_M_Y_HIPSTER_01_BLACK_PVG", 0x9A287536) ||
				pvg == ATSTRINGHASH("A_M_Y_HIPSTER_01_WHITE_PVG", 0x28BE7D3A) ||
				pvg == ATSTRINGHASH("A_M_Y_HIPSTER_02_WHITE_PVG", 0x81BE02E9) ||
				pvg == ATSTRINGHASH("A_M_Y_HIPSTER_03_WHITE_PVG", 0xC1EF29FD) ||
				pvg == ATSTRINGHASH("MALE_HIPSTERVINEWOOD_BLACK_PVG", 0x371549F1) ||
				pvg == ATSTRINGHASH("MALE_HIPSTERVINEWOOD_WHITE_PVG", 0x55A8E134))
					return g_SpeechContextGreetHipsterMalePHash;
			else if(pvg == ATSTRINGHASH("A_F_Y_HIPSTER_01_WHITE_PVG", 0x4B64AC97) ||
				pvg == ATSTRINGHASH("A_F_Y_HIPSTER_02_WHITE_PVG", 0x123A03A3) ||
				pvg == ATSTRINGHASH("A_F_Y_HIPSTER_03_WHITE_PVG", 0x1420226E) ||
				pvg == ATSTRINGHASH("A_F_Y_HIPSTER_04_WHITE_PVG", 0x1563B019) ||
				pvg == ATSTRINGHASH("FEMALE_HIPSTER_PVG", 0x6523CE73))
					return g_SpeechContextGreetHipsterFemalePHash;
			else if(pvg == ATSTRINGHASH("A_M_M_TRANVEST_02_LATINO_PVG", 0x20190E12) ||
					pvg == ATSTRINGHASH("A_M_M_TranVest_01_WHITE_PVG", 0x888169C8))
						return g_SpeechContextGreetTransvestitePHash;
			else if(toughness == AUD_TOUGHNESS_MALE_BRAVE)
				return g_SpeechContextGreetStrongPHash;
		}
	}

	return g_SpeechContextGenericHiPHash;
}

void audSpeechAudioEntity::SetJackingSpeechInfo(CPed* jackedPed, CVehicle* vehicle, bool jackFromPassengerSide, bool vanJackClipUsed)
{
	m_TimeJackedInforWasSet = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	m_JackedPed = jackedPed;
	m_JackedVehicle = vehicle;
	m_JackedFromPassengerSide = jackFromPassengerSide;
	m_VanJackClipUsed = vanJackClipUsed;

	if(m_Parent)
	{
		if(!m_Parent->IsLocalPlayer() || 
			m_JackedVehicle->InheritsFromBike() || 
			m_JackedVehicle->GetIsJetSki() ||
			g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_LastTimeOpeningDoor < 1500)
				TriggerJackingSpeech();
	}
	
}

void audSpeechAudioEntity::TriggerJackingSpeech()
{
	if(!m_Parent || !m_JackedPed || !m_JackedVehicle)
		return;

	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(currentTime - sm_TimeLastPlayedJackingSpeech < g_MinTimeBetweenJackingSpeechTriggers ||
			m_TimeJackedInforWasSet == 0 ||
			currentTime - m_TimeJackedInforWasSet > g_MaxTimeBetweenJackingInfoSetAndDoorOpening
		)
			return;
	sm_TimeLastPlayedJackingSpeech = currentTime;

	bool dontDelayResponse = m_JackedFromPassengerSide && m_JackedPed == m_JackedVehicle->GetDriver();
	bool isBig = m_JackedVehicle->m_nVehicleFlags.bIsBig;
	bool jackerSpeaks = false;
	bool otherSpeaks = false;
	bool jackerSpeaksFirst = false;
	if(m_Parent->GetPedType() == PEDTYPE_PLAYER_0)
	{
		//with Michael's custom jacking move, we always want him talking and always talking first.
		jackerSpeaks = true;
		jackerSpeaksFirst = true;
		if(fwRandom::GetRandomTrueFalse())
			otherSpeaks = true;
	}
	else if(m_JackedPed->GetSpeechAudioEntity() && m_JackedPed->GetSpeechAudioEntity()->GetIsAnySpeechPlaying())
	{
		jackerSpeaks = true;
		otherSpeaks = false;
	}
	else if(fwRandom::GetRandomTrueFalse())
	{
		jackerSpeaks = true;
		otherSpeaks = true;
		jackerSpeaksFirst = fwRandom::GetRandomTrueFalse() && m_JackedPed->GetSpeechAudioEntity();
	}
	else if(fwRandom::GetRandomTrueFalse())
		jackerSpeaks = true;
	else
		otherSpeaks = true;

	bool isGeneric = false;
	if(m_JackedVehicle->GetVehicleType() == VEHICLE_TYPE_CAR && m_JackedVehicle->GetVehicleAudioEntity())
	{
		audCarAudioEntity* carAudEnt = static_cast<audCarAudioEntity*>(m_JackedVehicle->GetVehicleAudioEntity());
		isGeneric = carAudEnt && carAudEnt->GetCarAudioSettings() &&
				AUD_GET_TRISTATE_VALUE(carAudEnt->GetCarAudioSettings()->Flags, FLAG_ID_CARAUDIOSETTINGS_IAMNOTACAR) == AUD_TRISTATE_TRUE;
	}

	if(m_Parent->IsLocalPlayer())
	{
		if(m_JackedPed->GetIsDeadOrDying())
		{
			Say(g_JackingDeadPedContextPHash, "SPEECH_PARAMS_FORCE", 0, g_JackingPedsFirstLineDelay);
			return;
		}

		u32 victimResponseContextPHash, jackingPedContextPHash;
		u32 angryLineContext = 0;
		bool victimShouldPlayAngryLine = CanSayAngryLineToPlayer(m_JackedPed, angryLineContext) &&  audEngineUtil::ResolveProbability(0.4f);
		victimResponseContextPHash = victimShouldPlayAngryLine ? angryLineContext : g_JackedGenericContextPHash;
		
		switch(m_JackedVehicle->GetVehicleType())
		{ 
		case VEHICLE_TYPE_BIKE:
		case VEHICLE_TYPE_BICYCLE:
			jackingPedContextPHash = g_JackingBikeContextPHash;
			break;
		case VEHICLE_TYPE_CAR:
			//skip break; if generic so we fall through
			if(!isGeneric)
			{
				jackingPedContextPHash = m_JackedPed->IsMale() ? g_JackingCarMaleContextPHash : g_JackingCarFemaleContextPHash;
				break;
			}
		default:
			jackingPedContextPHash = g_JackingGenericContextPHash;
			break;
		}

#if __BANK
	if(g_ForcePlayerThenPedJackingSpeech)
	{
		jackerSpeaks = true;
		otherSpeaks = true;
		jackerSpeaksFirst = true;
	}
	else if(g_ForcePedThenPlayerJackingSpeech)
	{
		jackerSpeaks = true;
		otherSpeaks = true;
		jackerSpeaksFirst = false;
	}
#endif

		if(jackerSpeaks && otherSpeaks)
		{
			if(jackerSpeaksFirst || !m_JackedPed->GetSpeechAudioEntity())
			{
				Say(jackingPedContextPHash, "SPEECH_PARAMS_FORCE", 0, 
					m_JackedFromPassengerSide || isBig ? g_JackingPedsFirstLineDelayPassSide : g_JackingPedsFirstLineDelay, 
					m_JackedPed, atFinalizeHash(victimResponseContextPHash), g_JackingVictimResponseDelay);

				//due to longer animation, treat van jacking reply as regular speech reply unless its trevor, as his anim
				//	involves a punch
				if(!dontDelayResponse && !m_VanJackClipUsed && m_Parent->GetPedType() != PEDTYPE_PLAYER_2)
					SetJackingReplyShouldDelay();
			}
			else
			{
				m_JackedPed->GetSpeechAudioEntity()->Say("GENERIC_FRIGHTENED_HIGH", "SPEECH_PARAMS_FORCE", 0, 
						m_JackedFromPassengerSide || isBig ? g_JackingVictimFirstLineDelayPassSide : g_JackingVictimFirstLineDelay, m_Parent,
						atFinalizeHash(jackingPedContextPHash), g_JackingPedsResponseDelay);
				//player is jacking and responding, so no pause sounds best
				/*if(!dontDelayResponse)
					m_JackedPed->GetSpeechAudioEntity()->SetJackingReplyShouldDelay();*/
			}
		}
		else if(jackerSpeaks)
			Say(jackingPedContextPHash, "SPEECH_PARAMS_FORCE", 0, m_JackedFromPassengerSide ? g_JackingPedsFirstLineDelayPassSide : g_JackingPedsFirstLineDelay);
		else if(m_JackedPed->GetSpeechAudioEntity())
			m_JackedPed->GetSpeechAudioEntity()->Say("GENERIC_FRIGHTENED_HIGH", "SPEECH_PARAMS_FORCE", 0, 
					m_JackedFromPassengerSide || isBig ? g_JackingVictimFirstLineDelayPassSide : g_JackingVictimFirstLineDelay);
	}
	else
	{
		//if(m_Parent && m_Parent->GetPedIntelligence() && m_Parent->GetPedIntelligence()->GetCurrentEvent() &&
			//m_Parent->GetPedIntelligence()->GetCurrentEvent()->GetEventType() == EVENT_DRAGGED_OUT_CAR)
		if(otherSpeaks)
		{
			//Looks shitty unless the player speaks second
			Say(g_JackVehicleBackContextPHash, "SPEECH_PARAMS_FORCE", 0, g_JackingPedsFirstLineDelay, m_JackedPed, 
				(m_JackedVehicle->GetType() == VEHICLE_TYPE_CAR && !isGeneric) ? atFinalizeHash(g_JackedCarContextPHash) : atFinalizeHash(g_JackedGenericContextPHash), g_JackingVictimResponseDelay);
			if(!dontDelayResponse)
				SetJackingReplyShouldDelay();
		}
		else
			Say(g_JackVehicleBackContextPHash, "SPEECH_PARAMS_FORCE", 0, g_JackingPedsFirstLineDelay);
	}
}

void audSpeechAudioEntity::SetJackingReplyShouldDelay() 
{
	m_DelayJackingReply = true; 
	m_TimeToPlayJackedReply = 0; 
}

void audSpeechAudioEntity::SetBlockFallingVelocityCheck()
{
	m_LastTimeFallingVelocityCheckWasSet = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}


void audSpeechAudioEntity::Panic(const s32 eventType, const bool isFirstReaction)
{
	if(m_Parent && !m_Parent->IsPlayer())
	{
		const AudTougnessType toughness = (AudTougnessType)GetPedToughness();
		const bool isTough = toughness == AUD_TOUGHNESS_MALE_BRAVE || toughness == AUD_TOUGHNESS_MALE_GANG || toughness == AUD_TOUGHNESS_COP;
		const bool isFemale = !m_Parent->IsMale();
		const s32 preDelay =  eventType == EVENT_GUN_AIMED_AT ? 0 : audEngineUtil::GetRandomNumberInRange(g_MinPanicSpeechPredelay, g_MaxPanicSpeechPredelay);

		const audSpeechPanicType panicType = GetPanicType(eventType); 
		switch(panicType)
		{
		case AUD_SPEECH_PANIC_JACKED:
			{
				Say("JACKED_FLEE");
				break;
			}
		case AUD_SPEECH_PANIC_LOW:
			{
				if(isFirstReaction)
				{
					if(!isTough && audEngineUtil::ResolveProbability(0.5f))
					{
						TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC, preDelay);
					}
					else
					{
						Say(g_SpeechContextShockedMedPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
					}
				}
				else
				{
					Say(g_SpeechContextFrightenedMedPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
				}
				break;
			}
		case AUD_SPEECH_PANIC_MEDIUM:
			{
				if(isFirstReaction)
				{
					if(isFemale)
					{
						if(!isTough && audEngineUtil::ResolveProbability(0.1f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_TERROR_GENERIC, preDelay);
						}
						else if(audEngineUtil::ResolveProbability(0.5f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC, preDelay);
						}
						else
						{
							Say(g_SpeechContextShockedMedPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
					else
					{
						if(!isTough && audEngineUtil::ResolveProbability(0.5f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC, preDelay);
						}
						else
						{
							Say(g_SpeechContextShockedMedPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
				}
				else
				{
					if(isFemale)
					{
						if(!isTough && audEngineUtil::ResolveProbability(0.5f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_SCARED, preDelay);
						}
						else
						{
							Say(g_SpeechContextFrightenedMedPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
					else
					{
						if(!isTough && audEngineUtil::ResolveProbability(0.25f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_PANIC, preDelay);
						}
						else
						{
							Say(g_SpeechContextFrightenedMedPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
				}
				break;
			}
		case AUD_SPEECH_PANIC_HIGH:
			{
				if(isFirstReaction)
				{
					if(isFemale)
					{
						if(audEngineUtil::ResolveProbability(0.5f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_TERROR_GENERIC, preDelay);
						}
						else if(audEngineUtil::ResolveProbability(0.5f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC, preDelay);
						}
						else
						{
							Say(g_SpeechContextShockedHighPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
					else
					{
						if(!isTough && audEngineUtil::ResolveProbability(0.15f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_TERROR_GENERIC, preDelay);
						}
						else if(audEngineUtil::ResolveProbability(0.5f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC, preDelay);
						}
						else
						{
							Say(g_SpeechContextShockedHighPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
				}
				else
				{
					if(isFemale)
					{
						if(audEngineUtil::ResolveProbability(0.25f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_TERROR_GENERIC, preDelay);
						}
						else if(audEngineUtil::ResolveProbability(0.25f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_SCARED, preDelay);
						}
						else
						{
							Say(g_SpeechContextFrightenedHighPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
					else
					{
						if(audEngineUtil::ResolveProbability(0.2f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_TERROR_GENERIC, preDelay);
						}
						else if(audEngineUtil::ResolveProbability(0.2f))
						{
							TriggerPainFromPainId(AUD_PAIN_ID_SCREAM_PANIC, preDelay);
						}
						else
						{
							Say(g_SpeechContextFrightenedHighPHash, "SPEECH_PARAMS_STANDARD", 0, preDelay);
						}
					}
				}
				break;
			}
		case AUD_SPEECH_PANIC_NONE:
		default:
			{
				break;
			}
		}
	}
}

audSpeechPanicType audSpeechAudioEntity::GetPanicType(const s32 eventType)
{
	audSpeechPanicType panicType = AUD_SPEECH_PANIC_NONE;
	switch(eventType)
	{
	case EVENT_DRAGGED_OUT_CAR:
		{
			panicType = AUD_SPEECH_PANIC_JACKED;
			break;
		}
	case EVENT_CRIME_CRY_FOR_HELP:
	case EVENT_DAMAGE:
	case EVENT_MELEE_ACTION:
	case EVENT_EXPLOSION:
	case EVENT_EXPLOSION_HEARD:
	case EVENT_GUN_AIMED_AT:
	case EVENT_RESPONDED_TO_THREAT:
	case EVENT_SHOCKING_EXPLOSION:
	case EVENT_SHOCKING_GUN_FIGHT:
	case EVENT_SHOCKING_GUNSHOT_FIRED:
	case EVENT_SHOCKING_PED_SHOT:
	case EVENT_SHOCKING_POTENTIAL_BLAST:
	case EVENT_SHOCKING_SEEN_PED_KILLED:
	case EVENT_SHOCKING_STUDIO_BOMB:
	case EVENT_SHOT_FIRED:
	case EVENT_SHOT_FIRED_BULLET_IMPACT:
	case EVENT_SHOT_FIRED_WHIZZED_BY:
	case EVENT_VEHICLE_DAMAGE_WEAPON:
	case EVENT_INJURED_CRY_FOR_HELP:
		{
			panicType = AUD_SPEECH_PANIC_HIGH;
			break;
		}
	case EVENT_SHOCKING_IN_DANGEROUS_VEHICLE:
	case EVENT_SHOCKING_CAR_CRASH:
	case EVENT_SHOCKING_CAR_CHASE:
	case EVENT_SHOCKING_CAR_ON_CAR:
	case EVENT_SHOCKING_CAR_PILE_UP:
	case EVENT_SHOCKING_DEAD_BODY:
	case EVENT_SHOCKING_INJURED_PED:
	case EVENT_SHOCKING_DRIVING_ON_PAVEMENT:
	case EVENT_SHOCKING_FIRE:
	case EVENT_SHOCKING_PED_RUN_OVER:
	case EVENT_SHOCKING_SEEN_GANG_FIGHT:
	case EVENT_SHOCKING_SEEN_MELEE_ACTION:
	case EVENT_SHOCKING_NON_VIOLENT_WEAPON_AIMED_AT:
	case EVENT_SHOCKING_VISIBLE_WEAPON:
	case EVENT_SHOCKING_RUNNING_STAMPEDE:
	case EVENT_VEHICLE_ON_FIRE:
	case EVENT_PED_ENTERED_MY_VEHICLE:
	case EVENT_PED_JACKING_MY_VEHICLE:
		{
			panicType = AUD_SPEECH_PANIC_MEDIUM;
			break;
		}
	case EVENT_ACQUAINTANCE_PED_HATE:
	case EVENT_ACQUAINTANCE_PED_WANTED:
	case EVENT_SHOCKING_SEEN_CAR_STOLEN:
	case EVENT_SHOCKING_MUGGING:
	case EVENT_SHOCKING_SEEN_WEAPON_THREAT:
	case EVENT_SHOCKING_SEEN_CONFRONTATION:
	case EVENT_AGITATED_ACTION:
	case EVENT_SHOCKING_DANGEROUS_ANIMAL:
	case EVENT_SHOCKING_HELICOPTER_OVERHEAD:
		{
			panicType = AUD_SPEECH_PANIC_LOW;
			break;
		}	
	case EVENT_FRIENDLY_FIRE_NEAR_MISS:
	case EVENT_SHOUT_TARGET_POSITION:
	case EVENT_SHOCKING_SEEN_VEHICLE_TOWED:
	case EVENT_SCENARIO_FORCE_ACTION:
	case EVENT_FOOT_STEP_HEARD:
	case EVENT_HELP_AMBIENT_FRIEND:
		{
			panicType = AUD_SPEECH_PANIC_NONE;
			break;
		}
	default:
		{
			audWarningf("No handler for eventType type %d when deciding on an audio panic type", eventType);
			break;
		}
	}
	return panicType;
}

void audSpeechAudioEntity::TriggerPainFromPainId(int painIdNum, const s32 preDelay)
{
	if(painIdNum == AUD_PAIN_ID_NUM_BANK_LOADED || painIdNum < 0 || painIdNum >= AUD_PAIN_ID_NUM_TOTAL)
	{
		return;
	}

	audDamageStats damageStats;
	damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
	damageStats.IsFromAnim = true; //keeps us from varying damage levels
	damageStats.PreDelay = preDelay;

	switch(painIdNum)
	{
	case AUD_PAIN_ID_PAIN_LOW_WEAK:
	case AUD_PAIN_ID_PAIN_LOW_GENERIC:
	case AUD_PAIN_ID_PAIN_LOW_TOUGH:
		damageStats.RawDamage = g_HealthLostForMediumPain - 1.0f;
		break;
	case AUD_PAIN_ID_PAIN_MEDIUM_WEAK:
	case AUD_PAIN_ID_PAIN_MEDIUM_GENERIC:
	case AUD_PAIN_ID_PAIN_MEDIUM_TOUGH:
		damageStats.RawDamage = g_HealthLostForMediumPain + 0.01f;
		break;
	case AUD_PAIN_ID_DEATH_LOW_WEAK:
	case AUD_PAIN_ID_DEATH_LOW_GENERIC:
	case AUD_PAIN_ID_DEATH_LOW_TOUGH:
		damageStats.Fatal = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE;
		break;
	case AUD_PAIN_ID_HIGH_FALL_DEATH:
		damageStats.DamageReason = AUD_DAMAGE_REASON_POST_FALL_GRUNT;
		break;
	case AUD_PAIN_ID_CYCLING_EXHALE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_CYCLING_EXHALE;
		break;
	case AUD_PAIN_ID_EXHALE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_EXHALE;
		break;
	case AUD_PAIN_ID_INHALE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_INHALE;
		break;
	case AUD_PAIN_ID_SHOVE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_SHOVE;
		break;
	case AUD_PAIN_ID_WHEEZE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_WHEEZE;
		break;
	case AUD_PAIN_ID_DEATH_HIGH_SHORT:
		damageStats.Fatal = true;
		damageStats.ForceDeathShort = true;
		break;
	case AUD_PAIN_ID_PAIN_HIGH_WEAK:
	case AUD_PAIN_ID_PAIN_HIGH_GENERIC:
	case AUD_PAIN_ID_PAIN_HIGH_TOUGH:
		damageStats.RawDamage = g_HealthLostForHighPain + 0.01f;
		break;
	case AUD_PAIN_ID_SCREAM_SHOCKED_WEAK:
	case AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC:
	case AUD_PAIN_ID_SCREAM_SHOCKED_TOUGH:
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_SHOCKED;
		break;
	case AUD_PAIN_ID_DEATH_GARGLE:
		damageStats.Fatal = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PlayGargle = true;
		break;
	case AUD_PAIN_ID_CLIMB_LARGE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_CLIMB_LARGE;
		break;
	case AUD_PAIN_ID_CLIMB_SMALL:
		damageStats.DamageReason = AUD_DAMAGE_REASON_CLIMB_SMALL;
		break;
	case AUD_PAIN_ID_JUMP:
		damageStats.DamageReason = AUD_DAMAGE_REASON_JUMP;
		break;
	case AUD_PAIN_ID_MELEE_SMALL_GRUNT:
		damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE_SMALL_GRUNT;
		break;
	case AUD_PAIN_ID_MELEE_LARGE_GRUNT:
		damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE_LARGE_GRUNT;
		break;
	case AUD_PAIN_ID_PAIN_GARGLE:
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PlayGargle = true;
		break;
	case AUD_PAIN_ID_DEATH_HIGH_MEDIUM:
		damageStats.Fatal = true;
		damageStats.ForceDeathMedium = true;
		break;
	case AUD_PAIN_ID_DEATH_HIGH_LONG:
		damageStats.Fatal = true;
		damageStats.ForceDeathLong = true;
		break;
	case AUD_PAIN_ID_COUGH:
		damageStats.DamageReason = AUD_DAMAGE_REASON_COUGH;
		break;
	case AUD_PAIN_ID_SNEEZE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_SNEEZE;
		break;
	case AUD_PAIN_ID_ON_FIRE:
		damageStats.DamageReason = AUD_DAMAGE_REASON_ON_FIRE;
		break;
	case AUD_PAIN_ID_HIGH_FALL:
	case AUD_PAIN_ID_SUPER_HIGH_FALL:
		damageStats.DamageReason = AUD_DAMAGE_REASON_FALLING;
		break;
	case AUD_PAIN_ID_SCREAM_PANIC:
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_PANIC;
		break;
	case AUD_PAIN_ID_SCREAM_PANIC_SHORT:
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_PANIC_SHORT;
		break;
	case AUD_PAIN_ID_SCREAM_SCARED:
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_SCARED;
		break;
	case AUD_PAIN_ID_SCREAM_TERROR_WEAK:
	case AUD_PAIN_ID_SCREAM_TERROR_GENERIC:
	case AUD_PAIN_ID_SCREAM_TERROR_TOUGH:
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_TERROR;
		break;
	case AUD_PAIN_ID_TAZER:
		damageStats.DamageReason = AUD_DAMAGE_REASON_TAZER;
		break;
	case AUD_PAIN_ID_DEATH_UNDERWATER:
		damageStats.Fatal = true;
		break;
	case AUD_PAIN_ID_COWER:
		damageStats.DamageReason = AUD_DAMAGE_REASON_COWER;
		break;
	case AUD_PAIN_ID_WHIMPER:
		damageStats.DamageReason = AUD_DAMAGE_REASON_WHIMPER;
		break;
	case AUD_PAIN_ID_DYING_MOAN:
		damageStats.DamageReason = AUD_DAMAGE_REASON_DYING_MOAN;
		break;
	case AUD_PAIN_ID_PAIN_RAPIDS:
		damageStats.DamageReason = AUD_DAMAGE_REASON_PAIN_RAPIDS;
		break;
	}

	InflictPain(damageStats);
}

void audSpeechAudioEntity::SetOpenedDoor()
{ 
	m_LastTimeOpeningDoor = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0); 
}

void audSpeechAudioEntity::SetHasBeenThrownFromVehicle()
{
	m_LastTimeThrownFromVehicle = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0); 
}

void audSpeechAudioEntity::SetToPlayGetWantedLevel()
{
	sm_PedToPlayGetWantedLevel = m_Parent; 
}

void audSpeechAudioEntity::PlayGetWantedLevel()
{
	if(sm_PedToPlayGetWantedLevel && sm_PedToPlayGetWantedLevel->GetSpeechAudioEntity())
		sm_PedToPlayGetWantedLevel->GetSpeechAudioEntity()->SayWhenSafe("GET_WANTED_LEVEL");
}

s32 audSpeechAudioEntity::GetNextLaughterTrackMarkerIndex(const RadioTrackTextIds *textIds, u32 currentPlayTimeMs)
{
	s32 result = -1;

	if(textIds && textIds->numTextIds > 0)
	{
		u32 startIndex = 0;
		u32 endIndex = textIds->numTextIds - 1;

		while(endIndex >= startIndex)
		{
			u32 midPoint = (endIndex + startIndex)/2;
			u32 timeBelow = 0;

			if(midPoint > 0)
			{
				timeBelow = textIds->TextId[midPoint - 1].OffsetMs;
			}
				
			u32 time = textIds->TextId[midPoint].OffsetMs;

			if(time > currentPlayTimeMs && timeBelow > currentPlayTimeMs)
			{
				endIndex = midPoint - 1;
			}
			else if(time < currentPlayTimeMs && timeBelow < currentPlayTimeMs)
			{
				startIndex = midPoint + 1;
			}
			else if(time >= currentPlayTimeMs && timeBelow < currentPlayTimeMs)
			{
				result = midPoint;
				break;
			}
		}
	}

	return result;
}

void audSpeechAudioEntity::PlayTrevorSpecialAbilitySpeech()
{
	if(naVerifyf(m_Parent && m_Parent->GetPedType() == PEDTYPE_PLAYER_2, "Attempting to play Trevor's special ability scream on a ped that isn't Trevor."))
	{
		if(m_Parent->GetPedIntelligence() && m_Parent->GetPedIntelligence()->IsBattleAware())
		{
			SayWhenSafe("SHOOTOUT_SPECIAL");
		}
		else
		{
			SayWhenSafe("SHOOTOUT_SPECIAL_TO_NOBODY");
		}
	}
}

void audSpeechAudioEntity::PlayCarCrashSpeech(CPed* otherDriver, CVehicle* myVehicle, CVehicle* otherVehicle)
{
	u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(!m_Parent || ! otherDriver || !myVehicle || !otherVehicle || 
		timeInMs - g_ScriptAudioEntity.GetTimeOfLastCollisionReaction() < 3000)
		return;

	bool isPlayer = m_Parent->IsLocalPlayer();
	bool otherIsPlayer = otherDriver->IsLocalPlayer();
	bool containsPlayer = myVehicle->ContainsLocalPlayer();
	bool otherContainsPlayer = otherVehicle->ContainsLocalPlayer();
	u32 contextPHash = 0;
	bool success = false;

	bool dontUsePlayer = false;
	if(timeInMs - sm_TimeLastPlayedCrashSpeechFromPlayerCar < 15000)
		dontUsePlayer = true;

	audCarAudioEntity* playerVehCarSettings = NULL;
	audCarAudioEntity* myVehCarSettings = NULL;
	audCarAudioEntity* otherVehCarSettings = NULL;
	if(myVehicle->GetVehicleAudioEntity() && myVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		myVehCarSettings = (audCarAudioEntity*)myVehicle->GetVehicleAudioEntity();
	if(otherVehicle->GetVehicleAudioEntity() && otherVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
		otherVehCarSettings = (audCarAudioEntity*)otherVehicle->GetVehicleAudioEntity();

	playerVehCarSettings = (isPlayer || containsPlayer) ? myVehCarSettings : otherVehCarSettings;


	if(isPlayer || otherIsPlayer)
	{
		CVehicle* playerVehicle = isPlayer ? myVehicle : otherVehicle;
		if(!playerVehicle)
			return; //this shouldn't happen

		//if player isn't alone, 50% chance of passenger speaking.
		//Otherwise, 50% chance of player speaking, 50% chance of other driver speaking
		if( playerVehicle->GetNumberOfPassenger() == 0 || audEngineUtil::ResolveProbability(0.5f))
		{
			bool tryThisParentFirst = audEngineUtil::ResolveProbability(0.5f);
			if(tryThisParentFirst && (!isPlayer || !dontUsePlayer))
			{
				switch(myVehicle->GetVehicleType())
				{
				case VEHICLE_TYPE_CAR:
					contextPHash = !myVehCarSettings || !myVehCarSettings->CanBeDescribedAsACar() ? ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2) : 
								ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
					break;
				case VEHICLE_TYPE_BIKE:
					contextPHash = ATPARTIALSTRINGHASH("CRASH_MOTERCYCLE", 0xEC3EB250);
					break;
				default:
					contextPHash = ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2);
				}
				
				success = (AUD_SPEECH_SAY_FAILED != Say(contextPHash, isPlayer ? "SPEECH_PARAMS_STANDARD_SHORT_LOAD" : "SPEECH_PARAMS_STANDARD"));
				if(success && isPlayer)
				{
					sm_TimeLastPlayedCrashSpeechFromPlayerCar = timeInMs;

					if(contextPHash == ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02)) 
					{
						m_Parent->SetPedResetFlag(CPED_RESET_FLAG_TriggerRoadRageAnim, true);
					}
				}
			}
			else if(otherDriver->GetSpeechAudioEntity() && (!otherIsPlayer || !dontUsePlayer))
			{
				switch(otherVehicle->GetVehicleType())
				{
				case VEHICLE_TYPE_CAR:
					contextPHash = !otherVehCarSettings || !otherVehCarSettings->CanBeDescribedAsACar() ? ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2) : 
						ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
					break;
				case VEHICLE_TYPE_BIKE:
					contextPHash = ATPARTIALSTRINGHASH("CRASH_MOTERCYCLE", 0xEC3EB250);
					break;
				default:
					contextPHash = ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2);
				}

				success = (AUD_SPEECH_SAY_FAILED != otherDriver->GetSpeechAudioEntity()->Say(contextPHash, otherIsPlayer ? "SPEECH_PARAMS_STANDARD_SHORT_LOAD" : "SPEECH_PARAMS_STANDARD"));
				if(success && otherIsPlayer)
				{
					sm_TimeLastPlayedCrashSpeechFromPlayerCar = timeInMs;
					
					if(contextPHash == ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02)) 
					{
						otherDriver->SetPedResetFlag(CPED_RESET_FLAG_TriggerRoadRageAnim, true);
					}
				}
			}
			
			if(!success)
			{
				if(!tryThisParentFirst  && (!isPlayer || !dontUsePlayer))
				{
					switch(myVehicle->GetVehicleType())
					{
					case VEHICLE_TYPE_CAR:
						contextPHash = !myVehCarSettings || !myVehCarSettings->CanBeDescribedAsACar() ? ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2) : 
							ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
						break;
					case VEHICLE_TYPE_BIKE:
						contextPHash = ATPARTIALSTRINGHASH("CRASH_MOTERCYCLE", 0xEC3EB250);
						break;
					default:
						contextPHash = ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2);
					}

					success = (AUD_SPEECH_SAY_FAILED != Say(contextPHash, isPlayer ? "SPEECH_PARAMS_STANDARD_SHORT_LOAD" : "SPEECH_PARAMS_STANDARD"));
					if(success && isPlayer)
					{
						sm_TimeLastPlayedCrashSpeechFromPlayerCar = timeInMs;
					
						if(contextPHash == ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02)) 
						{
							m_Parent->SetPedResetFlag(CPED_RESET_FLAG_TriggerRoadRageAnim, true);
						}
					}
				}
				else if(otherDriver->GetSpeechAudioEntity()  && (!otherIsPlayer || !dontUsePlayer))
				{
					switch(otherVehicle->GetVehicleType())
					{
					case VEHICLE_TYPE_CAR:
						contextPHash = !otherVehCarSettings || !otherVehCarSettings->CanBeDescribedAsACar() ? ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2) : 
							ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
						break;
					case VEHICLE_TYPE_BIKE:
						contextPHash = ATPARTIALSTRINGHASH("CRASH_MOTERCYCLE", 0xEC3EB250);
						break;
					default:
						contextPHash = ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2);
					}

					success = (AUD_SPEECH_SAY_FAILED != otherDriver->GetSpeechAudioEntity()->Say(contextPHash,
									otherIsPlayer ? "SPEECH_PARAMS_STANDARD_SHORT_LOAD" : "SPEECH_PARAMS_STANDARD"));
					if(success && otherIsPlayer)
					{
						sm_TimeLastPlayedCrashSpeechFromPlayerCar = timeInMs;
					
						if(contextPHash == ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02)) 
						{
							otherDriver->SetPedResetFlag(CPED_RESET_FLAG_TriggerRoadRageAnim, true);
						}
					}
				}
			}
		}
		else if(playerVehicle->GetSeatManager() && !dontUsePlayer) //passenger speaks
		{
			s32 maxSeats = playerVehicle->GetSeatManager()->GetMaxSeats();
			int startSeat = audEngineUtil::GetRandomInteger() % maxSeats;
			for(int i =0; i<maxSeats; ++i)
			{
				int index = (i + startSeat) % maxSeats;
				CPed* passenger = playerVehicle->GetSeatManager()->GetPedInSeat(index);
				if(passenger && !passenger->IsLocalPlayer() && passenger->GetSpeechAudioEntity())
				{
					contextPHash = !playerVehCarSettings || !playerVehCarSettings->CanBeDescribedAsACar()?
						ATPARTIALSTRINGHASH("CRASH_GENERIC_DRIVEN", 0xF276729D) :
						ATPARTIALSTRINGHASH("CRASH_CAR_DRIVEN", 0x76566BD2);
					
					if(AUD_SPEECH_SAY_FAILED != passenger->GetSpeechAudioEntity()->Say(contextPHash, "SPEECH_PARAMS_STANDARD"))
					{
						sm_TimeLastPlayedCrashSpeechFromPlayerCar = timeInMs;
						return;
					}
				}
			}
		}
	}
	//if the player is being driven
	else if(containsPlayer || otherContainsPlayer)
	{
		//50% chance player speaks, 50% chance the other driver speaks
		if(audEngineUtil::ResolveProbability(0.5f)  && !dontUsePlayer && 
			FindPlayerPed() && FindPlayerPed()->GetSpeechAudioEntity())
		{
			//player speaks
			CVehicle* playerVehicle = containsPlayer ? myVehicle : otherVehicle;
			if(!playerVehicle)
				return;

			contextPHash = !playerVehCarSettings || !playerVehCarSettings->CanBeDescribedAsACar() ? 
				ATPARTIALSTRINGHASH("CRASH_GENERIC_DRIVEN", 0xF276729D) :
				ATPARTIALSTRINGHASH("CRASH_CAR_DRIVEN", 0x76566BD2);

			success = (AUD_SPEECH_SAY_FAILED != FindPlayerPed()->GetSpeechAudioEntity()->Say(contextPHash, "SPEECH_PARAMS_STANDARD_SHORT_LOAD"));
		}
		if(!success)
		{
			//other driver speaks
			CVehicle* notPlayersVehicle = containsPlayer ? otherVehicle : myVehicle;
			CPed* driver = notPlayersVehicle ? notPlayersVehicle->GetDriver() : NULL;
			if(driver && driver->GetSpeechAudioEntity())
			{
				switch(notPlayersVehicle->GetVehicleType())
				{
				case VEHICLE_TYPE_CAR:
					contextPHash = !myVehCarSettings || !myVehCarSettings->CanBeDescribedAsACar() ? ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2) : 
						ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
					break;
				case VEHICLE_TYPE_BIKE:
					contextPHash = ATPARTIALSTRINGHASH("CRASH_MOTERCYCLE", 0xEC3EB250);
					break;
				default:
					contextPHash = ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2);
				}
				driver->GetSpeechAudioEntity()->Say(contextPHash, "SPEECH_PARAMS_STANDARD");
			}
		}
	}
	//if the player is in neither car
	else
	{
		switch(myVehicle->GetVehicleType())
		{
		case VEHICLE_TYPE_CAR:
			contextPHash = !myVehCarSettings || !myVehCarSettings->CanBeDescribedAsACar() ? ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2) : 
				ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
			break;
		case VEHICLE_TYPE_BIKE:
			contextPHash = ATPARTIALSTRINGHASH("CRASH_MOTERCYCLE", 0xEC3EB250);
			break;
		default:
			contextPHash = ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2);
		}
		success = (AUD_SPEECH_SAY_FAILED != Say(contextPHash, isPlayer ? "SPEECH_PARAMS_STANDARD_SHORT_LOAD" : "SPEECH_PARAMS_STANDARD"));

		if(!success && otherDriver->GetSpeechAudioEntity())
		{
			switch(otherVehicle->GetVehicleType())
			{
			case VEHICLE_TYPE_CAR:
				contextPHash = !otherVehCarSettings || !otherVehCarSettings->CanBeDescribedAsACar() ? ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2) : 
					ATPARTIALSTRINGHASH("CRASH_CAR", 0x251FCF02);
				break;
			case VEHICLE_TYPE_BIKE:
				contextPHash = ATPARTIALSTRINGHASH("CRASH_MOTERCYCLE", 0xEC3EB250);
				break;
			default:
				contextPHash = ATPARTIALSTRINGHASH("CRASH_GENERIC", 0x632864D2);
			}
			otherDriver->GetSpeechAudioEntity()->Say(contextPHash, otherIsPlayer ? "SPEECH_PARAMS_STANDARD_SHORT_LOAD" : "SPEECH_PARAMS_STANDARD");
		}
	}
}

bool audSpeechAudioEntity::IsPreloadedSpeechReady()
{
	audSpeechSound* snd = NULL;
	audWaveSlot* waveSlot = NULL;

	switch(m_CachedSpeechType)
	{
	case AUD_SPEECH_TYPE_SCRIPTED:
		snd = m_ScriptedSpeechSound;
		waveSlot = m_ScriptedSlot;
		break;
	case AUD_SPEECH_TYPE_AMBIENT:
		snd = m_AmbientSpeechSound;
		waveSlot = m_LastPlayedWaveSlot;
		break;
	case AUD_SPEECH_TYPE_PAIN:
	case AUD_SPEECH_TYPE_ANIMAL_PAIN:
		snd = m_PainSpeechSound;
		waveSlot = m_LastPlayedWaveSlot;
		break;
	case AUD_SPEECH_TYPE_ANIMAL_NEAR:
		snd = m_AnimalVocalSound;
		waveSlot = m_LastPlayedWaveSlot;
		break;
	default:
		return false;
	}

	if(!snd || !waveSlot)
	{
		KillPreloadedSpeech();
		return false;
	}

	if(m_Preloading)
		return false;

	if(snd->GetPlayState() == AUD_SOUND_PLAYING)
	{
		return true;
	}
	if(snd->GetPlayState() == AUD_SOUND_WAITING_TO_BE_DELETED)
	{
		KillPreloadedSpeech();
		return false;
	}

	if(snd->Prepare(waveSlot, true, naWaveLoadPriority::Speech) == AUD_PREPARED && snd->GetWaveSlot() && snd->GetWavePlayerId() != -1)
		return true;
	else if(snd->Prepare(waveSlot, true, naWaveLoadPriority::Speech) == AUD_PREPARE_FAILED)
	{
		KillPreloadedSpeech();
		return false;
	}
	else
		return false;
}

bool audSpeechAudioEntity::CheckAndPrepareSpeechSound(audSpeechSound* snd, audWaveSlot* waveSlot)
{
	if(!snd || !waveSlot)
	{
		return false;
	}
	
	if(snd->GetPlayState() == AUD_SOUND_PLAYING)
	{
		return true;
	}
	if(snd->GetPlayState() == AUD_SOUND_WAITING_TO_BE_DELETED)
	{
		return false;
	}

	if(snd->Prepare(waveSlot, true, naWaveLoadPriority::Speech) == AUD_PREPARED && snd->GetWaveSlot() && snd->GetWavePlayerId() != -1)
		return true;
	else if(snd->Prepare(waveSlot, true, naWaveLoadPriority::Speech) == AUD_PREPARE_FAILED)
	{
		return false;
	}
	else
		return false;
}


void audSpeechAudioEntity::KillPreloadedSpeech(bool bFromReset)
{
	naSpeechEntDebugf2(this, "KillPreloadedSpeech()");

	bool bIsScripted = (m_ScriptedSpeechSound != NULL);

	if(!bIsScripted)
	{
		FreeSlot();
	}
	else
	{
		if(m_ScriptedSpeechSound)
		{
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				if(m_ScriptedSlot)
				{
					u32 index = m_ScriptedSlot->GetSlotIndex();
					g_ScriptAudioEntity.CleanUpReplayScriptedSpeechWaveSlot(index);
				}
			}
			m_ReplayWaveSlotIndex = -1;
#endif
			m_ScriptedSpeechSound->StopAndForget();
			//naDisplayf("Killing preloaded scripted speech %s", m_CurrentlyPlayingSpeechContext);
		}
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			if(m_ScriptedShadowSound[i])
			{
				m_ScriptedShadowSound[i]->StopAndForget();
			}
		}
		if(m_ScriptedSpeechSoundEcho)
			m_ScriptedSpeechSoundEcho->StopAndForget();
		if(m_ScriptedSpeechSoundEcho2)
			m_ScriptedSpeechSoundEcho2->StopAndForget();
		if(m_ActiveSubmixIndex != -1)
		{
			g_AudioEngine.GetEnvironment().FreeConversationSpeechSubmix(m_ActiveSubmixIndex);
			m_ActiveSubmixIndex = -1;
		}
		if(m_HeadSetSubmixEnvSnd)
		{
			m_HeadSetSubmixEnvSnd->StopAndForget();
		}
		g_ScriptAudioEntity.AbortScriptedConversation(true BANK_ONLY(,"Killing preloaded speech"));
	}

	if(!bFromReset && m_Parent)
	{
		m_Parent->SetIsWaitingForSpeechToPreload(false);
	}

	m_Preloading  = false;
	m_PlaySpeechWhenFinishedPreparing = false;
	m_HasPreparedAndPlayed = false;
}

bool audSpeechAudioEntity::IsPreloadedSpeechPlaying(bool* playTimeOverZero) const {

	audSpeechSound * activeSound = (m_AmbientSpeechSound?m_AmbientSpeechSound:m_ScriptedSpeechSound);
	if( activeSound && activeSound->GetEnvironmentSound() ) {

		const u32 magic = 0xf0000000;  // HACK 
		const u32 playTime = (u32)activeSound->GetCurrentPlayTime(0);

		if( m_PreloadedSpeechIsPlaying && playTime < magic ) {
			if(playTimeOverZero)
				(*playTimeOverZero) = true;
			return true;
		}
	}

	return false;
}

void audSpeechAudioEntity::PlayPreloadedSpeech_FromScript()
{
	naSpeechEntDebugf1(this, "PlayPreloadedSpeech_FromScript()");

	Assert(m_AmbientSpeechSound);
	Assert(m_AmbientSlotId >= 0);
	Assert(m_Preloading);
	
	if(m_Parent && m_Parent->GetIsWaitingForSpeechToPreload())
	{
		m_Preloading = false;
		m_PreloadFreeTime = 0;
	}
	else
	{
		if (m_AmbientSpeechSound && m_AmbientSlotId >=0 && m_Preloading)
		{
			audWaveSlot* waveSlot = g_SpeechManager.GetAmbientWaveSlotFromId(m_AmbientSlotId);
			m_AmbientSpeechSound->Prepare(waveSlot, true, naWaveLoadPriority::Speech);
			m_Preloading = false;
			m_PreloadFreeTime = 0;
			m_PlaySpeechWhenFinishedPreparing = true;
		}
	}
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		//Replay needs to know that this is playing the audio, not pre-loading. 
		bool cache = m_DeferredSayStruct.preloadOnly;
		m_DeferredSayStruct.preloadOnly = false;
		CReplayMgr::RecordFx<CPacketSpeech>(
			CPacketSpeech(&m_DeferredSayStruct, false, m_PositionForNullSpeaker), m_Parent);
		m_DeferredSayStruct.preloadOnly = cache;
	}		
#endif //GTA_REPLAY
}

void audSpeechAudioEntity::PlayPreloadedSpeech(s32 uTimeOfPredelay)
{
	naSpeechEntDebugf1(this, "PlayPreloadedSpeech()");

	m_PlaySpeechWhenFinishedPreparing = false;

	audSpeechSound** snd = NULL;
	audSound** sndShadow[AUD_NUM_SPEECH_ROUTES];
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		sndShadow[i] = NULL;
	}
	audSpeechSound** sndEcho = NULL;
	audSpeechSound** sndEcho2 = NULL;
	audWaveSlot* waveSlot = m_LastPlayedWaveSlot;
	u32 contextPHash = m_LastPlayedSpeechContextPHash;
	u32 variation = m_LastPlayedSpeechVariationNumber;

	BANK_ONLY(const char* context = m_CurrentlyPlayingSpeechContext);

	switch(m_CachedSpeechType)
	{
	case AUD_SPEECH_TYPE_SCRIPTED:
		//Displayf("audSpeechAudioEntity::PlayPreloadedSpeech time:%d hash %u, voice %s", CReplayMgr::GetCurrentTimeRelativeMs(), m_LastPlayedSpeechContextPHash, m_LastPlayedScriptedSpeechVoice);
		snd = &m_ScriptedSpeechSound;
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			sndShadow[i] = &m_ScriptedShadowSound[i];
		}
		//sndEcho = &m_ScriptedSpeechSoundEcho;
		//sndEcho2 = &m_ScriptedSpeechSoundEcho2;
		waveSlot = m_ScriptedSlot;
		variation = m_VariationNum;

		m_HasPreparedAndPlayed = true;
		break;
	case AUD_SPEECH_TYPE_AMBIENT:
		snd = &m_AmbientSpeechSound;
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			sndShadow[i] = &m_AmbientShadowSound[i];
		}
		sndEcho = &m_AmbientSpeechSoundEcho;
		sndEcho2 = &m_AmbientSpeechSoundEcho2;
		m_HasPreparedAndPlayed = true;
		break;
	case AUD_SPEECH_TYPE_PAIN:
	case AUD_SPEECH_TYPE_ANIMAL_PAIN:
		snd = &m_PainSpeechSound;
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			sndShadow[i] = &m_PainShadowSound[i];
		}
		sndEcho = &m_PainEchoSound;	
		m_HasPreparedAndPlayed = true;
		break;
	case AUD_SPEECH_TYPE_ANIMAL_NEAR:
		snd = &m_AnimalVocalSound;
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			sndShadow[i] = &m_AnimalVocalShadowSound[i];
		}
		sndEcho = &m_AnimalVocalEchoSound;
		break;
	default:
		naAssertf(false, "Invalid audSpeechType for sound in PlayPreloadedSpeech() - %u", m_CachedSpeechType);
		break;
	}

	//Grab this before resetting predelay for urgent lines
	m_TotalPredelayForLipsync = m_CurrentlyPlayingPredelay1+uTimeOfPredelay;

	if(m_IsUrgent)
		uTimeOfPredelay = 0;

	m_PreloadFreeTime = 0;
	m_IsUrgent = false;

	if(!*snd || !waveSlot || (m_CachedSpeechType == AUD_SPEECH_TYPE_AMBIENT && m_AmbientSlotId<0))
	{
		naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - !*snd || !waveSlot || (m_CachedSpeechType == AUD_SPEECH_TYPE_AMBIENT && m_AmbientSlotId<0)");
		KillPreloadedSpeech();
		return;
	}

	if(m_Parent && m_Parent->GetPedAudioEntity())
		m_Parent->GetPedAudioEntity()->StopScubaSound();

	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if(sndShadow[i] && *sndShadow[i])
			(*sndShadow[i])->StopAndForget();
	}
	if(sndEcho && *sndEcho)
		(*sndEcho)->StopAndForget();
	if(sndEcho2 && *sndEcho2)
		(*sndEcho2)->StopAndForget();
	if (m_HeadSetSubmixEnvSnd)
		m_HeadSetSubmixEnvSnd->StopAndForget();

	naAssertf(m_CurrentlyPlayingSoundSet.IsInitialised(), "We didn't properly set the audSoundSet prior to playing preloaded speech");
	const audMetadataRef soundSpeechRef = m_CurrentlyPlayingSoundSet.Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));
	const audMetadataRef soundShadowFERef = m_CurrentlyPlayingSoundSet.Find(ATSTRINGHASH("SpeechShadowFE", 0x623b8117));
	const audMetadataRef soundShadowPosRef = m_CurrentlyPlayingSoundSet.Find(ATSTRINGHASH("SpeechShadowPos", 0x52f58009));
	const audMetadataRef soundShadowHSRef = m_CurrentlyPlayingSoundSet.Find(ATSTRINGHASH("SpeechShadowHS", 0xfdf5a1c));
	const audMetadataRef soundShadowPadRef = m_CurrentlyPlayingSoundSet.Find(ATSTRINGHASH("SpeechShadowPad", 0xd351f0b4));
	const audMetadataRef soundEchoRef = m_CurrentlyPlayingSoundSet.Find(ATSTRINGHASH("SpeechEchoSound", 0x4bec8264));

	naAssertf(soundSpeechRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");
	naAssertf(soundShadowFERef.IsValid(), "Invalid 'SpeechShadowFE' audMetadataRef for speech audSoundSet");
	naAssertf(soundShadowPosRef.IsValid(), "Invalid 'SpeechShadowPos' audMetadataRef for speech audSoundSet");
	naAssertf(soundShadowHSRef.IsValid(), "Invalid 'SpeechShadowHS' audMetadataRef for speech audSoundSet");
	naAssertf(soundShadowPadRef.IsValid(), "Invalid 'SpeechShadowPad' audMetadataRef for speech audSoundSet");
	naAssertf(soundEchoRef.IsValid(), "Invalid 'SpeechEchoSound' audMetadataRef for speech audSoundSet");

	u32 totalPredelay = m_CurrentlyPlayingPredelay1+uTimeOfPredelay;

	// We push the additional predelay from the viseme's to the SpeechSound, because we can't create a new sound with the correct
	// predelay and prepare without losing the WavePlayerSlotId we need to create and play the shadows.
	(*snd)->SetAdditionalPredelay(totalPredelay);
	// The main speech sound is just so we can create shadow sounds from it, so set it's volume to silence
	(*snd)->SetRequestedVolume(g_SilenceVolume);
	(*snd)->SetRequestedDopplerFactor(0.0f);
	
	m_PreloadedSpeechIsPlaying = true;

	// Figure out volumes and position for the shadows
	// Get the position to play positioned speech, and abort if we can't find a place to play it
	Vector3 position;
	if(!GetPositionForSpeechSound(position, m_CachedVolType))
	{
		naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - !GetPositionForSpeechSound(position, m_CachedVolType)");
		(*snd)->StopAndForget();
		KillPreloadedSpeech();
		naAssertf(false, "PlayPreloadedSpeech failed: Unable to get a position for non-frontend speech.");
		return;
	}
	
	// As we're about to play new speech, reset the smoothers (I hope this is what's causing a slight ramping on the occasional line)
	ResetSpeechSmoothers();

	const bool inCarAndSceneSaysFrontend = IsInCarFrontendScene();

	// See how much we should use our current audibility, and how much we should use our previous high audibility lines
	// We want to ease back into low priority speech after playing a high priority line, so we don't follow up a near-inaudible line
	// with something that just played loudly, as that sounds bad.
	ComputeAmountToUsePreviousHighAudibility(m_CachedVolType, m_CachedAudibility);

	// Get the initial echo data we need to process their volumes and filters in GetSpeechAudibilityMetrics
	audEchoMetrics echo1, echo2;
	ComputeEchoInitMetrics(position, m_CachedVolType, sndEcho != NULL, sndEcho2 != NULL, echo1, echo2);


	// Check for success, because there's the possibility of getting a bad ped head matrix in which case we don't want to play the speech
	audSpeechMetrics metrics;
	const bool metricsProcessed = GetSpeechAudibilityMetrics(VECTOR3_TO_VEC3V(position), m_CachedSpeechType, m_CachedVolType, inCarAndSceneSaysFrontend, m_CachedAudibility, 
															metrics, echo1, echo2);
	if(!metricsProcessed)
	{
		naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - !metricsProcessed");
		//		Assert(0);
		(*snd)->StopAndForget();
		KillPreloadedSpeech();
		Warningf("PlayPreloadedSpeech failed: GetSpeechAudibilityMetrics failed.");
		return;
	}

	// Apply the volume on the main speech sound to all the shadows
	Sound uncompressedMetadata; 
	SOUNDFACTORY.DecompressMetadata<SpeechSound>(soundSpeechRef, uncompressedMetadata); 
	const f32 speechMetadataVol = uncompressedMetadata.Volume * 0.01f;

	const f32 postSubmixVol = ComputePostSubmixVolume(m_CachedSpeechType, m_CachedVolType, inCarAndSceneSaysFrontend, m_SpeechVolumeOffset + speechMetadataVol);

	// Create the shadow sounds
	audSoundInitParams initParam;
	initParam.RemoveHierarchy = false;
	initParam.AllowOrphaned = true;
	initParam.Predelay = totalPredelay;
	initParam.StartOffset = m_CachedSpeechStartOffset;
	initParam.EnvironmentGroup = m_Parent ? m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true) : NULL;
	initParam.UpdateEntity = true;
	// In order to make sure we don't drop scripted speech which could end up breaking mission scripts, use the streaming bucket.
	// Each bucket has 192 slots, and the unlikely worst case scenario for the streaming bucket 
	// is something like 120 sounds so there should always be plenty room for scripted speech and ambient speech triggered from script
	BANK_ONLY(m_UsingStreamingBucking = false;)
	if(!PARAM_audNoUseStreamingBucketForSpeech.Get() && (m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED || m_IsFromScript))
	{
		BANK_ONLY(m_UsingStreamingBucking = true;)
		Assign(initParam.BucketId, audNorthAudioEngine::GetBucketIdForStreamingSounds());
	}
	//initParam.IsAuditioning = true;
	naAssertf((*snd)->GetWavePlayerId() != -1, "Invalid ShadowPcmSourceId on m_AmbientSpeechSound");
	initParam.ShadowPcmSourceId = (*snd)->GetWavePlayerId();
	// The shadow predelays will get off from the main sound predelay if they're on different timers and in slow mo, as one's pausing and ones just slowing down
	// So set the shadows to the same timer as the main scripted speech sound if it's scripted speech.
	if(m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED)
	{
		initParam.TimerId = 4;
	}

	// We always need a front end sound and it's reverb, either because we specified AUD_SPEECH_FRONTEND volType, or it's positioned
	// in which case we need a FE sound to play if they're in a car and we want to switch to front end
	if(!CreateShadow(soundShadowFERef, sndShadow[AUD_SPEECH_ROUTE_FE], initParam  ASSERT_ONLY(, context, contextPHash)))
	{
		naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - !CreateShadow(soundShadowFERef, sndShadow[AUD_SPEECH_ROUTE_FE], initParam  ASSERT_ONLY(, context, contextPHash))");
		KillPreloadedSpeech();
		return;
	}

	// We need a position to play this otherwise we can't get valid game metrics for the reverb
	if(!position.IsZero())
	{
		if(!CreateShadow(soundShadowPosRef, sndShadow[AUD_SPEECH_ROUTE_VERB_ONLY], initParam  ASSERT_ONLY(, context, contextPHash)))
		{
			naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - !CreateShadow(soundShadowPosRef, sndShadow[AUD_SPEECH_ROUTE_VERB_ONLY], initParam  ASSERT_ONLY(, context, contextPHash))");
			KillPreloadedSpeech();
			return;
		}
	}

	// We don't need a positioned shadow or echo if the volType is AUD_SPEECH_FRONTEND
	if(m_CachedVolType != AUD_SPEECH_FRONTEND)
	{
		if(!CreateShadow(soundShadowPosRef, sndShadow[AUD_SPEECH_ROUTE_POS], initParam  ASSERT_ONLY(, context, contextPHash)))
		{
			naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - !CreateShadow(soundShadowPosRef, sndShadow[AUD_SPEECH_ROUTE_POS], initParam  ASSERT_ONLY(, context, contextPHash))");
			KillPreloadedSpeech();
			return;
		}

		if(echo1.useEcho)
		{
			CreateEcho(echo1, m_CachedSpeechType, m_CachedVolType, postSubmixVol, totalPredelay, m_CachedRolloff, 
						m_CurrentlyPlayingVoiceNameHash, contextPHash, variation, soundEchoRef, sndEcho, initParam);
		}
		if(echo2.useEcho)
		{
			CreateEcho(echo2, m_CachedSpeechType, m_CachedVolType, postSubmixVol, totalPredelay, m_CachedRolloff, 
						m_CurrentlyPlayingVoiceNameHash, contextPHash, variation, soundEchoRef, sndEcho2, initParam);
		}
	}
	
	if(sndShadow[AUD_SPEECH_ROUTE_HS] && m_ActiveSpeechIsHeadset && m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED)
	{
		if(m_ActiveSubmixIndex == -1)
		{
			m_ActiveSubmixIndex = static_cast<s8>(g_AudioEngine.GetEnvironment().AssignConversationSpeechSubmix());
		}
		if(m_ActiveSubmixIndex != -1)
		{
			audSoundInitParams headsetParams = initParam;

			// Create the sound that controls the submix
			const s32 speechSubmixId = audDriver::GetMixer()->GetSubmixIndex(g_AudioEngine.GetEnvironment().GetConversationSpeechSubmix(m_ActiveSubmixIndex));
			g_AudioEngine.GetEnvironment().GetConversationSpeechSubmix(m_ActiveSubmixIndex)->SetEffectParam(0, synthCorePcmSource::Params::CompiledSynthNameHash, g_HeadsetSynthPatchHash);

			// url:bugstar:7816601 - Can we reduce the distortion amount on UNITEDPAPER's voice a little to aid clarity/audibility of this dialogue
			g_AudioEngine.GetEnvironment().GetConversationSpeechSubmix(m_ActiveSubmixIndex)->SetEffectParam(0, ATSTRINGHASH("Distortion", 0xD767E48B), 
				m_CurrentlyPlayingVoiceNameHash == ATSTRINGHASH("UNITEDPAPER", 0xC04D7C00) ? g_HeadsetDistortionReduced : g_HeadsetDistortionDefault);			

			headsetParams.SourceEffectSubmixId = (s16) speechSubmixId;
			headsetParams.ShadowPcmSourceId = -1;
			CreateSound_PersistentReference(g_HeadsetSubmixControlHash, &m_HeadSetSubmixEnvSnd, &headsetParams);
			if(m_HeadSetSubmixEnvSnd)
			{
				m_HeadSetSubmixEnvSnd->SetRequestedSourceEffectMix(1.0f, 1.0f);
			}

			// Create the HS Shadow sound
			headsetParams.SourceEffectSubmixId = -1;
			headsetParams.ShadowPcmSourceId = (*snd)->GetWavePlayerId();
			headsetParams.EffectRoute = EFFECT_ROUTE_CONVERSATION_SPEECH_MIN + m_ActiveSubmixIndex;
			if(!CreateShadow(soundShadowHSRef, sndShadow[AUD_SPEECH_ROUTE_HS], headsetParams  ASSERT_ONLY(, context, contextPHash)))
			{
				naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - !CreateShadow(soundShadowHSRef, sndShadow[AUD_SPEECH_ROUTE_HS], headsetParams  ASSERT_ONLY(, context, contextPHash))");
				KillPreloadedSpeech();
				return;
			}
		}
	}

	if(sndShadow[AUD_SPEECH_ROUTE_PAD] 
		&& m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED
		REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) // No pad speaker in replay
		&& (m_ActiveSpeechIsPadSpeaker || (g_RoutePhoneCallsThroughPadSpeaker && g_AudioEngine.GetEnvironment().GetPadSpeakerEnabled() && g_ScriptAudioEntity.IsScriptedConversationAMobileCall())))
	{
		if(!CreateShadow(soundShadowPadRef, sndShadow[AUD_SPEECH_ROUTE_PAD], initParam  ASSERT_ONLY(, context, contextPHash)))
		{
			KillPreloadedSpeech();
			return;
		}
	}

#if __BANK
	naSpeechEntDebugf1(this, "PlayPreloadedSpeech() Success.  context: %s totalPredelay: %u m_CurrentlyPlayingPredelay1: %u uTimeOfPredelay: %d m_CachedSpeechStartOffset: %u",
		m_CurrentlyPlayingSpeechContext, totalPredelay, m_CurrentlyPlayingPredelay1, uTimeOfPredelay, m_CachedSpeechStartOffset);
#endif

#if __BANK
	safecpy(m_DeferredSayStruct.context, context);
#endif

	m_DeferredSayStruct.contextPHash = m_LastPlayedSpeechContextPHash;
	m_DeferredSayStruct.variationNumber = m_LastPlayedSpeechVariationNumber;

	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if(*sndShadow[i])
		{
#if __BANK
			if(g_ShowSpeechPositions)
			{
				CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
				if(pPlayer)
				{
					const Vector3 &playerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
					CVectorMap::DrawLine(playerPosition, position, Color_yellow, Color_yellow); 
				}
			}
#endif

#if GTA_REPLAY
			f32 volume = 0.0f;
			if(CReplayMgr::IsEditModeActive() && i == AUD_SPEECH_ROUTE_HS)
			{
				volume = g_FrontendAudioEntity.GetReplayDialogVolume();
			}
#endif

			(*sndShadow[i])->SetRequestedPosition(position);
			(*sndShadow[i])->SetRequestedLPFCutoff(metrics.lpf);
			(*sndShadow[i])->SetRequestedHPFCutoff(metrics.hpf);
			(*sndShadow[i])->SetRequestedVolume(metrics.vol[i]+volume);
			(*sndShadow[i])->SetRequestedPostSubmixVolumeAttenuation(postSubmixVol);
			(*sndShadow[i])->SetRequestedDopplerFactor(0.0f);
			(*sndShadow[i])->SetRequestedEnvironmentalLoudnessFloat(g_VolTypeEnvLoudness[m_CachedVolType]);
			if(m_CachedSpeechType == AUD_SPEECH_TYPE_ANIMAL_NEAR || m_CachedSpeechType == AUD_SPEECH_TYPE_ANIMAL_FAR || m_CachedSpeechType == AUD_SPEECH_TYPE_ANIMAL_PAIN)
			{
				(*sndShadow[i])->SetRequestedVolumeCurveScale(m_CachedRolloff);
			}
			else
			{
				(*sndShadow[i])->SetRequestedShouldAttenuateOverDistance(AUD_TRISTATE_FALSE);
			}
		}
	}

#if GTA_REPLAY
	switch(m_CachedSpeechType)
	{
	case AUD_SPEECH_TYPE_SCRIPTED:
		if(CReplayMgr::ShouldRecord())
		{
			if(m_CurrentlyPlayingVoiceNameHash != ATSTRINGHASH("EXEC1_POWERANN", 0x4C49ABC3))
			{
				// If the startoffset is 0 then we will record a -2 as an indicator that this is the actual play sound packet(The sound will be started when played back and start offset is -2 or greater than 0). 
				// -1 is preloading.
				// Using the play position has an inaccurate start time.
				s32 startOffset = -2;
				if(m_CachedSpeechStartOffset != 0)
				{
					startOffset = m_CachedSpeechStartOffset;
				}
				CReplayMgr::RecordFx<CPacketScriptedSpeechUpdate>(
					CPacketScriptedSpeechUpdate(m_LastPlayedSpeechContextPHash, m_ContextNameStringHash, m_CurrentlyPlayingVoiceNameHash, m_CachedVolType,m_CachedAudibility, m_VariationNum, startOffset, m_ActiveSoundStartTime, fwTimer::GetReplayTimeInMilliseconds(), m_ScriptedSlot->GetSlotIndex(), m_ActiveSpeechIsHeadset, m_IsPitchedDuringSlowMo, m_IsMutedDuringSlowMo, m_IsUrgent, m_PlaySpeechWhenFinishedPreparing), m_Parent);
			}
		}
		break;
	default:
		break;
	}
#endif

	(*snd)->PrepareAndPlay(waveSlot, false);
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if(sndShadow[i] && *sndShadow[i])
			(*sndShadow[i])->PrepareAndPlay(waveSlot,false);
	}
	if (sndEcho && *sndEcho)
		(*sndEcho)->PrepareAndPlay(waveSlot,false);
	if (sndEcho2 && *sndEcho2)
		(*sndEcho2)->PrepareAndPlay(waveSlot,false);
	if (m_HeadSetSubmixEnvSnd)
		m_HeadSetSubmixEnvSnd->PrepareAndPlay(waveSlot,false);

#if __BANK
	if(m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED) 
	{
		m_TimeScriptedSpeechStarted = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);	
		if(g_ConversationDebugSpew)
			naDisplayf("Playing preloaded scripted line %u", g_AudioEngine.GetTimeInMilliseconds());
#if !__FINAL
		m_AlertConversationEntityWhenSoundBegins = true;
#endif
	}
#endif

	if((*snd)->Prepare(waveSlot, true, naWaveLoadPriority::Speech) != AUD_PREPARED)
	{
		naSpeechEntDebugf2(this, "PlayPreloadedSpeech() FAILED - (*snd)->Prepare(waveSlot, true, naWaveLoadPriority::Speech) != AUD_PREPARED");
		KillPreloadedSpeech();
	}

	if(m_Parent && !m_DontAddBlip)
	{
		//Calculate the distanceToPed.
		if(m_CachedVolType == AUD_SPEECH_MEGAPHONE)
		{
			m_BlipNoise = CMiniMap::sm_Tunables.Sonar.fSoundRange_Megaphone;
		}
		else if(m_CachedVolType == AUD_SPEECH_SHOUT)
		{
			m_BlipNoise = CMiniMap::sm_Tunables.Sonar.fSoundRange_Shouting;
		}
		else
		{
			m_BlipNoise = CMiniMap::sm_Tunables.Sonar.fSoundRange_Talking;
		}

		//Report a stealth noise.
		const bool bCreateSonarBlip = true;

		if(m_Parent->IsLocalPlayer())
			m_BlipNoise = CMiniMap::CreateSonarBlipAndReportStealthNoise(m_Parent, m_Parent->GetTransform().GetPosition(), m_BlipNoise, HUD_COLOUR_BLUEDARK, bCreateSonarBlip);
		else
			CMiniMap::CreateSonarBlip(VEC3V_TO_VECTOR3(m_Parent->GetTransform().GetPosition()), m_BlipNoise, HUD_COLOUR_BLUEDARK, m_Parent);
	}

#if __BANK
	if ((g_ListPlayedAmbientContexts && m_CachedSpeechType == AUD_SPEECH_TYPE_AMBIENT) 
		|| (g_ListPlayedScriptedContexts && m_CachedSpeechType == AUD_SPEECH_TYPE_SCRIPTED)
		|| (g_ListPlayedPainContexts && (m_CachedSpeechType == AUD_SPEECH_TYPE_PAIN || m_CachedSpeechType == AUD_SPEECH_TYPE_ANIMAL_PAIN))
		|| (g_ListPlayedAnimalContexts && (m_CachedSpeechType == AUD_SPEECH_TYPE_ANIMAL_NEAR || m_CachedSpeechType == AUD_SPEECH_TYPE_ANIMAL_PAIN)))
	{
		const char* speechTypeString = m_CachedSpeechType < AUD_NUM_SPEECH_TYPES ? g_SpeechTypeNames[m_CachedSpeechType] : "Unknown speech type";
		f32 distanceToPed = 0.0f;
		if(m_Parent)
		{
			distanceToPed = Mag(g_AudioEngine.GetEnvironment().GetPanningListenerPosition() - m_Parent->GetTransform().GetPosition()).Getf();
		}

		naWarningf("***%s Speech - Dist: %f; Time: %u;", speechTypeString, distanceToPed, fwTimer::GetTimeInMilliseconds());
		naWarningf("Context: %s; volType: %s; aud: %s; prevAudAmount: %f;", context, g_SpeechVolNames[m_CachedVolType], g_ConvAudNames[m_CachedAudibility], m_AmountToUsePreviousHighAudibility);
		naWarningf("volPos: %f; volFE: %f; volFEVerb: %f volHS: %f; volPostSubmix: %f; volSpeechOffset: %f;", 
			metrics.vol[AUD_SPEECH_ROUTE_POS], metrics.vol[AUD_SPEECH_ROUTE_FE], metrics.vol[AUD_SPEECH_ROUTE_VERB_ONLY], metrics.vol[AUD_SPEECH_ROUTE_HS], postSubmixVol, m_SpeechVolumeOffset);
		naWarningf("LPF: %f; HPF: %f;", metrics.lpf, metrics.hpf);
		if(echo1.useEcho)
			naWarningf("Echo1: ON vol: %f; volPostSubmix: %f; lpf: %f; hpf: %f;", echo1.vol, echo1.volPostSubmix, echo1.lpf, echo1.hpf);
		else
			naWarningf("Echo1: OFF");
		if(echo2.useEcho)
			naWarningf("Echo2: ON vol: %f; volPostSubmix: %f; lpf: %f; hpf: %f;", echo2.vol, echo2.volPostSubmix, echo2.lpf, echo2.hpf);
		else
			naWarningf("Echo2: OFF");
	}
#endif
}

bool audSpeechAudioEntity::CreateShadow(const audMetadataRef metaRef, audSound** sound, audSoundInitParams& initParam  
										ASSERT_ONLY(, const char* context, const u32 contextPHash))
{
	if(naVerifyf(sound, "Passed NULL audSpeechSound** when creating a shadow"))
	{
		CreateSound_PersistentReference(metaRef, sound, &initParam);
		if(!*sound)
		{
#if __BANK
			PrintSoundPool(ASSERT_ONLY(context, contextPHash, metaRef));
#endif
			return false;
		}

		if((*sound)->GetSoundTypeID() != EnvironmentSound::TYPE_ID)
		{
			naAssertf(false, "Shadow sound is not an EnvironmentSound::TYPE_ID");
			return false;
		}

		return true;
	}
	return false;
}

void audSpeechAudioEntity::CreateEcho(const audEchoMetrics& echo, const audSpeechType speechType, const audSpeechVolume volType, const f32 volPostSubmix, const u32 predelay, const f32 rolloff, 
										const u32 voiceHash, const u32 contextPHash, const s32 variation, const audMetadataRef metaRef, audSpeechSound** sndEcho, audSoundInitParams& initParam)
{
	if(naVerifyf(sndEcho, "Passed NULL audSpeechSound**") && echo.useEcho)
	{
		// Copy the init params so we're not unintentionally changing sounds outside this function
		audSoundInitParams echoInitParams = initParam;
		echoInitParams.BucketId = 0xff;
		echoInitParams.ShadowPcmSourceId = -1;
		echoInitParams.Predelay = echo.predelay + predelay;
		CreateSound_PersistentReference(metaRef, (audSound**)sndEcho, &echoInitParams);
		if (*sndEcho)
		{
			const bool success = ((audSpeechSound*)(*sndEcho))->InitSpeech(voiceHash, contextPHash, variation);
			if (success)
			{
				(*sndEcho)->SetRequestedVolume(echo.vol);
				(*sndEcho)->SetRequestedLPFCutoff(echo.lpf);
				(*sndEcho)->SetRequestedHPFCutoff(echo.hpf);
				(*sndEcho)->SetRequestedPosition(echo.pos);
				(*sndEcho)->SetRequestedDopplerFactor(0.0f);
				(*sndEcho)->SetRequestedEnvironmentalLoudnessFloat(g_VolTypeEnvLoudness[volType]);
				(*sndEcho)->SetRequestedPostSubmixVolumeAttenuation(echo.volPostSubmix + volPostSubmix);
				if(speechType == AUD_SPEECH_TYPE_ANIMAL_NEAR || speechType == AUD_SPEECH_TYPE_ANIMAL_FAR || speechType == AUD_SPEECH_TYPE_ANIMAL_PAIN)
				{
					(*sndEcho)->SetRequestedVolumeCurveScale(rolloff);
				}
				else
				{
					(*sndEcho)->SetRequestedShouldAttenuateOverDistance(AUD_TRISTATE_FALSE);
				}
			}
			else
			{
				(*sndEcho)->StopAndForget();
			}
		}
	}
}

u32 audSpeechAudioEntity::SelectActualVoice(u32 voiceNameHash)
{
	// see if we're one of a few special-case voices, and pick from the options of actual voices to use. Primarily pain, Nico, Roman and Brian
	u32 actualVoiceNameHash = voiceNameHash;
	// If PAIN_VOICE is the voice, pick the appropriate one for the sex of the ped, or special for Nico
	if (voiceNameHash==g_PainVoice)
	{
		if (m_Parent->IsLocalPlayer() && !NetworkInterface::IsGameInProgress())
		{
			actualVoiceNameHash = sm_LocalPlayerPainVoice;
		}
		else if (!m_IsMale)
		{
			actualVoiceNameHash = g_PainFemale;
		}
		else
		{
			actualVoiceNameHash = g_PainMale;
		}
	}

	return actualVoiceNameHash;
}

void audSpeechAudioEntity::FreeSlot(bool UNUSED_PARAM(freeInstantly), bool stopPain)
{
	naSpeechEntDebugf2(this, "FreeSlot() stopPain: %s", stopPain ? "True" : "False");

	if (m_AmbientSpeechSound)
	{
		m_AmbientSpeechSound->StopAndForget();
		m_AmbientSpeechSound = NULL;
	}
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if (m_AmbientShadowSound[i])
		{
			m_AmbientShadowSound[i]->StopAndForget();
			m_AmbientShadowSound[i] = NULL;
		}
	}
	if (m_AmbientSpeechSoundEcho)
	{
		m_AmbientSpeechSoundEcho->StopAndForget();
		m_AmbientSpeechSoundEcho = NULL;
	}
	if (m_AmbientSpeechSoundEcho2)
	{
		m_AmbientSpeechSoundEcho2->StopAndForget();
		m_AmbientSpeechSoundEcho2 = NULL;
	}

	if (stopPain && m_PainSpeechSound)
	{
		m_PainSpeechSound->StopAndForget();
		m_PainSpeechSound = NULL;
	}
	for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
	{
		if (stopPain && m_PainShadowSound[i])
		{
			m_PainShadowSound[i]->StopAndForget();
			m_PainShadowSound[i] = NULL;
		}
	}
	if (stopPain && m_PainEchoSound)
	{
		m_PainEchoSound->StopAndForget();
		m_PainEchoSound = NULL;
	}
	if (m_AnimalVocalSound)
	{
		m_AnimalVocalSound->StopAndForget();
		m_AnimalVocalSound = NULL;
	}
	if (m_AnimalVocalEchoSound)
	{
		m_AnimalVocalEchoSound->StopAndForget();
		m_AnimalVocalEchoSound = NULL;
	}
	for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS; ++i)
	{
		if (m_AnimalOverlappableSound[i])
		{
			m_AnimalOverlappableSound[i]->StopAndForget();
			m_AnimalOverlappableSound[i] = NULL;
		}
	}
	if (m_AmbientSlotId>=0)
	{
		if(m_SuccessValueOfLastSay != AUD_SPEECH_SAY_DEFERRED)
			g_SpeechManager.FreeAmbientSpeechSlot(m_AmbientSlotId);
		m_AmbientSlotId = -1;
	}

	m_WaitingForAmbientSlot = false;
	ResetTriggeredContextVariables();

	m_PositionForNullSpeaker.Set(ORIGIN);
	m_EntityForNullSpeaker = NULL;
	m_PreloadedSpeechIsPlaying = false;
	m_VisemeAnimationData = NULL;

	m_MultipartVersion = 0;
	m_MultipartContextCounter = 0;
	m_MultipartContextPHash = 0;
	m_OriginalMultipartContextPHash = 0;
}

void audSpeechAudioEntity::OrphanSpeech()
{
	if(camInterface::IsFadedOut())
	{
		FreeSlot();
	}
	else
	{
		if (m_AmbientSpeechSound)
		{
			if(m_AmbientSpeechSound->GetPlayState() == AUD_SOUND_PLAYING)
			{
				m_AmbientSpeechSound->InvalidateEntitySoundReference();
				m_AmbientSpeechSound = NULL;
			}
			else
			{
				m_AmbientSpeechSound->StopAndForget();
			}
		}
		if (m_AmbientSpeechSoundEcho)
		{
			if(m_AmbientSpeechSoundEcho->GetPlayState() == AUD_SOUND_PLAYING)
			{
				m_AmbientSpeechSoundEcho->InvalidateEntitySoundReference();
				m_AmbientSpeechSoundEcho = NULL;
			}
			else
			{
				m_AmbientSpeechSoundEcho->StopAndForget();
			}
		}
		if (m_AmbientSpeechSoundEcho2)
		{
			if(m_AmbientSpeechSoundEcho2->GetPlayState() == AUD_SOUND_PLAYING)
			{
				m_AmbientSpeechSoundEcho2->InvalidateEntitySoundReference();
				m_AmbientSpeechSoundEcho2 = NULL;
			}
			else
			{
				m_AmbientSpeechSoundEcho2->StopAndForget();
			}
		}
		if (m_PainSpeechSound)
		{
			if(m_PainSpeechSound->GetPlayState() == AUD_SOUND_PLAYING)
			{
				m_PainSpeechSound->InvalidateEntitySoundReference();
				m_PainSpeechSound = NULL;
			}
			else
			{
				m_PainSpeechSound->StopAndForget();
			}
		}
		if (m_PainEchoSound)
		{
			if(m_PainEchoSound->GetPlayState() == AUD_SOUND_PLAYING)
			{
				m_PainEchoSound->InvalidateEntitySoundReference();
				m_PainEchoSound = NULL;
			}
			else
			{
				m_PainEchoSound->StopAndForget();
			}
		}
		if (m_AnimalVocalSound)
		{
			if(m_AnimalVocalSound->GetPlayState() == AUD_SOUND_PLAYING)
			{
				m_AnimalVocalSound->InvalidateEntitySoundReference();
				m_AnimalVocalSound = NULL;
			}
			else
			{
				m_AnimalVocalSound->StopAndForget();
			}
		}
		for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS; ++i)
		{
			if (m_AnimalOverlappableSound[i])
			{
				if(m_AnimalOverlappableSound[i]->GetPlayState() == AUD_SOUND_PLAYING)
				{
					m_AnimalOverlappableSound[i]->InvalidateEntitySoundReference();
					m_AnimalOverlappableSound[i] = NULL;
				}
				else
				{
					m_AnimalOverlappableSound[i]->StopAndForget();
				}
			}
		}
		if (m_AnimalVocalEchoSound)
		{
			if(m_AnimalVocalEchoSound->GetPlayState() == AUD_SOUND_PLAYING)
			{
				m_AnimalVocalEchoSound->InvalidateEntitySoundReference();
				m_AnimalVocalEchoSound = NULL;
			}
			else
			{
				m_AnimalVocalEchoSound->StopAndForget();
			}
		}
	}
}

void audSpeechAudioEntity::OrphanScriptedSpeechEcho()
{
	if (m_ScriptedSpeechSoundEcho)
	{
		if(m_ScriptedSpeechSoundEcho->GetPlayState() == AUD_SOUND_PLAYING)
		{
			m_ScriptedSpeechSoundEcho->InvalidateEntitySoundReference();
			m_ScriptedSpeechSoundEcho = NULL;
		}
		else
		{
			m_ScriptedSpeechSoundEcho->StopAndForget();
		}
	}

	if (m_ScriptedSpeechSoundEcho)
	{
		if(m_ScriptedSpeechSoundEcho2->GetPlayState() == AUD_SOUND_PLAYING)
		{
			m_ScriptedSpeechSoundEcho2->InvalidateEntitySoundReference();
			m_ScriptedSpeechSoundEcho2 = NULL;
		}
		else
		{
			m_ScriptedSpeechSoundEcho2->StopAndForget();
		}
	}
}

s32 audSpeechAudioEntity::GetRandomVariation(u32 voiceNameHash, const char* context, u32 *timeContextLastPlayedForThisVoice, bool *hasBeenPlayedRecently, bool isMultipart)
{
	return GetRandomVariation(voiceNameHash, atPartialStringHash(context), timeContextLastPlayedForThisVoice, hasBeenPlayedRecently, isMultipart);
}

s32 audSpeechAudioEntity::GetRandomVariation(u32 voiceNameHash, u32 contextPHash, u32 *timeContextLastPlayedForThisVoice, bool *hasBeenPlayedRecently, bool isMultipart)
{
	// We've got valid variations.
	// Here's how we pick one (as the randomness of this'll get argued over endlessly...):
	// Pick a random starting point. Work from there, sequentially through them all. If we find one that's not 
	// in phrase memory (last g_PhraseMemorySize = 250 lines used) go with it. Also update the oldest used, and if
	// we don't find an unused one, use the oldest. Enhance: perhaps don't say anything, if it's used really recently,
	// though this'll depend on context.
	s32 oldestVariation = -1; // we use -1 to mean it's an unused one
	s32 oldestVariationTime = -2; // we use -1 to mean it's an unused one
	u32 newestTime = 0;
	s32 variationNum = -1;
	s32 variationCount = 0;
	u32 contextHash = contextPHash;
	s32 numVariations = 0;

	if(isMultipart)
	{
		bool foundAllVariations = false;
		char versionBuffer[4];
		while(!foundAllVariations)
		{
			numVariations++;
			formatf(versionBuffer, 4, "_%02u", numVariations);
			u32 contextVersionPHash = atPartialStringHash(versionBuffer, m_OriginalMultipartContextPHash);
			foundAllVariations = audSpeechSound::FindNumVariationsForVoiceAndContext(voiceNameHash, contextVersionPHash) == 0;

			if(!Verifyf(numVariations < 10, "Too many variations found for multipart context. PHash: %u", m_OriginalMultipartContextPHash))
				foundAllVariations = true;
		}
		numVariations--;
	}
	else
	{
		// Are there variations for this context and voice
		numVariations = (s32)audSpeechSound::FindNumVariationsForVoiceAndContext(voiceNameHash, contextPHash);
		//	Assert(numVariations > 0);
	}

	if (numVariations<=0)
	{
		//***		Warningf("No speech asset for context: %s; voice: %s\n", context, voiceName);
		return -1;
	}

	// Build a list of variations for this voice and context that, so we don't have to search through the whole list for each variation
	u32 memorySize = 0;
	u32 aPhraseMemory[g_PhraseMemorySize][4];
	g_SpeechManager.BuildMemoryListForVoiceAndContext(voiceNameHash, contextHash, memorySize, aPhraseMemory);

	s32 startVariationNum = audEngineUtil::GetRandomNumberInRange(1, numVariations);
//	Warningf("new speech trigger. %d variations", numVariations);
	for (variationCount=0; variationCount<numVariations; variationCount++)
	{
		// indexing of variations goes from 1-numVariations, hence the +1
		variationNum = ((startVariationNum + variationCount) % numVariations) + 1;
		u32 timeInMs = 0;
		s32 timeLastUsed = -1;

		// Go through the mini-phrase history generated for this voice and context
		for( u32 iMem = 0; iMem < memorySize; iMem++ )
		{
			if( aPhraseMemory[iMem][2] == (u32)variationNum )
			{
				timeInMs = aPhraseMemory[iMem][3];
				timeLastUsed = iMem;
				break;
			}
		}
//		Warningf("varNum: %d;  timeUsed: %d", variationNum, timeLastUsed);
		// is this variation in the history? If not, use it.
		if (timeLastUsed == -1)
		{
//			It's an unused variation
			oldestVariationTime = -1;
			oldestVariation = variationNum;
//			break;
		}
		else
		{
			// It's in the history. Is it the oldest one? Don't store if we found an unused one. Store time if it's the newest one
			if (timeInMs>newestTime)
			{
				newestTime = timeInMs;
			}
			if (timeLastUsed>oldestVariationTime && oldestVariationTime!=-1)
			{
				oldestVariationTime = timeLastUsed;
				oldestVariation = variationNum;
			}
		}
	}
	// We've either found an unused one, or we need to use the oldest. Did we break out, out fall through?
//	Warningf("variationCount: %d;  numVariations: %d", variationCount, numVariations);
	variationNum = oldestVariation;

	if (timeContextLastPlayedForThisVoice)
	{
		*timeContextLastPlayedForThisVoice = newestTime;
	}
	if (hasBeenPlayedRecently)
	{
		if (oldestVariationTime==-1)
		{
			*hasBeenPlayedRecently = false;
		}
		else
		{
			*hasBeenPlayedRecently = true;
		}
	}
	return variationNum;
}

bool audSpeechAudioEntity::GetIsMegaphoneContext(u32 c)
{
	if (c==g_SpeechContextMegaphonePedClearStreetPHash || c==g_SpeechContextMegaphoneFootPursuitPHash || c==g_SpeechContextPullOverWarningPHash ||
		c==g_SpeechContextPullOverPHash || c==g_SpeechContextCopHeliMegaphoneWarningPHash || c==g_SpeechContextCopHeliMegaphonePHash)
	{
		return true;
	}
	return false;
}

void audSpeechAudioEntity::RegisterFallingStart()
{
	naSpeechEntDebugf1(this, "RegisterFallingStart()");

	m_IsFalling = true;
	m_LastTimeFalling = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	// Update falling right away because sometimes TaskNMHighFall tasks are being triggered when you're already on the ground
	m_FramesUntilNextFallingCheck = 0;
	m_TimeToStartLandProbe = 0;
	if(g_DoesLandProbeWaitCurveExist && g_UseDelayedFallProbe)
	{
		const Vector3& pos = VEC3V_TO_VECTOR3(m_Parent->GetMatrix().d());
		WorldProbe::CShapeTestHitPoint hitPoint;
		WorldProbe::CShapeTestResults probeResults(hitPoint);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(pos, Vector3(pos.x, pos.y, pos.z - 100.0f));
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
		probeDesc.SetExcludeEntity(m_Parent);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			f32 height = pos.z - hitPoint.GetHitPosition().z;
			u32 timeToDelayProbing = (u32) Max((f32)sm_StartHeightToLandProbeWaitTime.CalculateValue(height), 0.0f);
			m_TimeToStartLandProbe = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) + timeToDelayProbing;
		}
	}
}

void audSpeechAudioEntity::UpdateFalling(const u32 timeOnAudioTimer0)
{
	if(m_IsFalling)
	{
		m_LastTimeFalling = timeOnAudioTimer0;
		m_FramesUntilNextFallingCheck--;
		if(m_FramesUntilNextFallingCheck <= 0)
		{
			m_FramesUntilNextFallingCheck = 0;

			if(m_Parent && !m_Parent->GetIsDeadOrDying())
			{
				if(!m_PainSpeechSound && !m_PainEchoSound)
				{
					m_bIsFallScreaming = false;
				}
	
				//safety check
				const Vector3& vel = m_Parent->GetVelocity();
				const bool isSwimming = m_Parent->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwimming) || m_Parent->GetIsInWater();
				const bool isParachuting = m_Parent->GetIsParachuting() 
					&& m_Parent->GetPedIntelligence() 
					&& !m_Parent->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_NM_HIGH_FALL, true);
				bool fallScreamStop = isSwimming 
					|| (m_LastTimeFallingVelocityCheckWasSet != 0 ? false : vel.GetZ() > -1.0f * g_FallingScreamVelocityThresh)
					|| (m_LastTimeFallingVelocityCheckWasSet != 0 ? false : m_Parent->IsOnGround())
					|| isParachuting;

				if(!fallScreamStop)
				{
					const Vector3& pos = VEC3V_TO_VECTOR3(m_Parent->GetMatrix().d());
					WorldProbe::CShapeTestHitPoint hitPoint;
					WorldProbe::CShapeTestResults probeResults(hitPoint);
					WorldProbe::CShapeTestProbeDesc probeDesc;
					probeDesc.SetResultsStructure(&probeResults);
					probeDesc.SetStartAndEnd(pos, Vector3(pos.x, pos.y, pos.z - 100.0f));
					probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_TYPES_MOVER);
					probeDesc.SetExcludeEntity(m_Parent);
					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
					{
						const f32 height = pos.z - hitPoint.GetHitPosition().z;
						const f32 heightToStop = sm_FallVelToStopHeightCurve.CalculateValue(vel.GetZ() * -1.0f);
						if(height <= heightToStop)
						{
							fallScreamStop = true;
						}
					}
				}

				if(fallScreamStop)
				{
					if(m_bIsFallScreaming)
					{						
						if(m_PainSpeechSound)
						{
							m_PainSpeechSound->SetReleaseTime(g_FallingScreamReleaseTime);
						}
						for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
						{
							if(m_PainShadowSound[i])
							{
								m_PainShadowSound[i]->SetReleaseTime(g_FallingScreamReleaseTime);
							}
						}
						if(m_PainEchoSound)
						{
							m_PainEchoSound->SetReleaseTime(g_FallingScreamReleaseTime);
						}

						StopPain();
						FreeSlot();
					}

					if(!(isSwimming || isParachuting))
					{
						audDamageStats damageStats;
						if(m_IsHighFall)
						{
							damageStats.DamageReason = AUD_DAMAGE_REASON_POST_FALL_GRUNT;
						}
						else
						{
							damageStats.DamageReason = AUD_DAMAGE_REASON_POST_FALL_GRUNT_LOW;
						}
						ReallyInflictPain(damageStats);
					}

					ResetFalling();
				}
			}
			else
			{
				ResetFalling();
				StopPain();
			}
		}
	}
}

void audSpeechAudioEntity::ResetFalling()
{
	m_IsFalling = false;
	m_IsHighFall = false;
	m_bIsFallScreaming = false;
	m_HasPlayedInitialHighFallScream = false;
	m_LastTimeFallingVelocityCheckWasSet = 0;
}

void audSpeechAudioEntity::InflictPain(audDamageStats& damageStats)
{
	naSpeechEntDebugf1(this, "InflictPain()");

	if(m_Parent && m_Parent->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ))
	{
		naSpeechEntDebugf2(this, "Stopping speech due as we're playing pain and ped was killed by stealth");
		if(GetIsHavingAConversation())
		{
			if(IsScriptedSpeechPlaying())
				g_ScriptAudioEntity.AbortScriptedConversation(false BANK_ONLY(,"killed by stealth"));
			else
				g_ScriptAudioEntity.AbortScriptedConversation(true BANK_ONLY(,"killed by stealth"));
		}
		StopSpeech();
		return;
	}
	if(damageStats.DamageReason == AUD_DAMAGE_REASON_FALLING && camInterface::IsFadedOut())
		return;
	//this seems to happen at times when the game decides a person should be dead, but it doesn't really make for a good
	//	audio trigger point
	if(damageStats.DamageReason == AUD_DAMAGE_REASON_MELEE && damageStats.RawDamage > 200.0f)
		return;

	m_ShouldUpdatePain = true;
	m_DamageStats = damageStats;
}

void audSpeechAudioEntity::ReallyInflictPain(audDamageStats& damageStats)
{
	naSpeechEntDebugf1(this, "ReallyInflictPain()");

	m_ShouldUpdatePain = false;

	if (m_PedHasGargled || m_PlayingSharkPain)
	{
		naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - m_PedHasGargled || m_PlayingSharkPain");
		return;
	}

	if( !m_Parent ||
		(damageStats.DamageReason != AUD_DAMAGE_REASON_POST_FALL_GRUNT && !damageStats.IsFromAnim && damageStats.PedWasAlreadyDead) || 
		m_PainAudioDisabled || 
		damageStats.DamageReason == AUD_DAMAGE_REASON_ENTERING_RAGDOLL_DEATH ||
		(CNetwork::IsGameInProgress() && m_Parent->IsPlayer())
		)
	{
		naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - !m_Parent || m_PainAudioDisabled || CNetwork::IsGameInProgress() && m_Parent->IsPlayer() || POST_FALL_GRUNT and already dead");
		return;
	}

	// Don't play grunts when the player's moving around in stealth mode
	if(m_Parent 
		&& m_Parent->IsPlayer() 
		&& (DYNAMICMIXER.IsStealthScene() || m_Parent->GetMotionData()->GetUsingStealth())
		&& (damageStats.DamageReason == AUD_DAMAGE_REASON_CLIMB_LARGE || damageStats.DamageReason == AUD_DAMAGE_REASON_CLIMB_SMALL || damageStats.DamageReason == AUD_DAMAGE_REASON_JUMP))
	{
		naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - Don't play grunts when the player's moving around in stealth mode");
		return;
	}

	//handle animal pain
	if(m_AnimalParams)
	{
		if(m_PainSpeechSound || m_PainEchoSound)
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - m_AnimalParams && (m_PainSpeechSound || m_PainEchoSound)");
			return;
		}
		if(m_AnimalVocalSound)
			m_AnimalVocalSound->StopAndForget();
		for(u32 i = 0; i < AUD_NUM_SPEECH_ROUTES; i++)
		{
			if(m_AnimalVocalShadowSound[i])
			{
				m_AnimalVocalShadowSound[i]->StopAndForget();
			}
		}
		for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS; ++i)
		{
			if (m_AnimalOverlappableSound[i])
			{
				m_AnimalOverlappableSound[i]->StopAndForget();
			}
		}

		PlayAnimalVocalization(m_AnimalType, "PAIN_DEATH");
		return;
	}

	//Don't play pain if underwater
	if(m_IsUnderwater && 
		damageStats.DamageReason != AUD_DAMAGE_REASON_SURFACE_DROWNING &&
		damageStats.DamageReason != AUD_DAMAGE_REASON_DROWNING
		)
	{
		naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - Don't play pain if underwater");
		return;
	}

	if(damageStats.DamageReason == AUD_DAMAGE_REASON_EXHAUSTION)
	{
		if(m_Parent->GetPedAudioEntity())
			m_Parent->GetPedAudioEntity()->StartHeartbeat();
		return;
	}

	bool isSniperScope = audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings() && audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings()->MicType == MIC_SNIPER_RIFLE;

	bool isMelee = damageStats.DamageReason == AUD_DAMAGE_REASON_MELEE;
	f32 distanceSqToNearestListener = (g_AudioEngine.GetEnvironment().ComputeSqdDistanceRelativeToVolumeListenerV(m_Parent->GetMatrix().d())).Getf();
	if(damageStats.IsHeadshot && !isMelee)
	{
		if(isSniperScope)
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - damageStats.IsHeadshot && !isMelee && isSniperScope");
			return;
		}

		// Don't play a sound for the headshot, except for every once in a while and play a death gargle for the headshot		
		if(audEngineUtil::ResolveProbability(g_HeadshotDeathGargleProb))
			damageStats.PlayGargle = true;
		else
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - damageStats.IsHeadshot && !isMelee && distanceSqToNearestListener <= g_MinDistanceSqForHeadshotPainSound");
			return;
		}
	}

	// If we're in stealth and doing stealth like things, then we don't want to playing loud screams, so instead play a gargle
	const CPed* player = FindPlayerPed();
	const bool isPlayerUsingStealth = DYNAMICMIXER.IsStealthScene() || (player && player->GetMotionData()->GetUsingStealth());
	if(isPlayerUsingStealth && damageStats.Fatal && (damageStats.IsSilenced || damageStats.IsSniper || isSniperScope || isMelee))
	{
		damageStats.PlayGargle = true;
	}

	if(m_DeathIsFromEnteringRagdoll)
	{
		damageStats.PlayGargle = true;
	}

	if(damageStats.DamageReason == AUD_DAMAGE_REASON_EXPLOSION)
	{
		if(!audEngineUtil::ResolveProbability(g_ProbOfExplosionDeathScream))
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_EXPLOSION && !audEngineUtil::ResolveProbability(g_ProbOfExplosionDeathScream");
			return;
		}
		else if(sm_NumExplosionPainsPlayedThisFrame + sm_NumExplosionPainsPlayedLastFrame >= 2)
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_EXPLOSION && sm_NumExplosionPainsPlayedThisFrame + sm_NumExplosionPainsPlayedLastFrame >= 2");
			return;
		}

		sm_NumExplosionPainsPlayedThisFrame++;
	}

	if(damageStats.Inflictor && damageStats.Inflictor->GetIsTypeVehicle())
	{
		CVehicle* vehicle = static_cast<CVehicle*>(damageStats.Inflictor.Get());
		if(vehicle && vehicle->ContainsLocalPlayer())
			g_ScriptAudioEntity.RegisterCarPedCollision(vehicle, damageStats.RawDamage, m_Parent);

		// Also scale the damage, as getting hit by a vehicle traveling 60 mph is barely enough to do medium pain
		m_DamageStats.RawDamage *= g_RunOverByVehicleDamageScalar;
	}

	//As damage tends to be weapon-dependent, our pain sounds are highly weapon-dependent as well.  We'll swap banks less and get
	//	greater variety if we randomly vary our damage a bit
	if(damageStats.RawDamage > g_HealthLostForLowPain && !damageStats.IsFromAnim)
	{
		f32 randomDamageScalar = audEngineUtil::GetRandomNumberInRange(1.0f - g_DamageScalarDiffMax, 1.0f + g_DamageScalarDiffMax);
		damageStats.RawDamage *= randomDamageScalar;
	}

	// Player suffers less damage than anyone else - compensate for that here (I presume it's the same for all players, so don't just want LocalPlayer)
	// (actually, for minor punches, I see 4.0 for peds, and 1.0 for the player, with 1.155 for bigger punches - 
	//   will go with *3 and a limit of 3, and accept the player grunts a wee bit less)
	if (m_Parent->IsAPlayerPed() && !damageStats.IsFromAnim)
	{
		damageStats.RawDamage *= 3.0f;
	}

	// Decide if this is worth playing a pain sample for, which level, and whether we should stop any currently playing speech
	// In a further attempt to increase variety and decrease bank swapping, we'll randomly move up or down a level of pain here as well.
	u32 painLevel = AUD_PAIN_NONE;
	if (damageStats.Fatal)                 
	{
		if(damageStats.DamageReason == AUD_DAMAGE_REASON_COUGH)
			audEngineUtil::ResolveProbability(0.5f) ? damageStats.PlayGargle = true : painLevel = AUD_PAIN_COUGH;
		else if(m_IsUnderwater)
			painLevel = AUD_DEATH_UNDERWATER;
		else if(damageStats.PlayGargle)
			painLevel = AUD_DEATH_HIGH;
		else if(!isMelee && !isSniperScope)
			painLevel = AUD_DEATH_HIGH;
		else
			painLevel = AUD_DEATH_LOW;
	}
	else if (damageStats.DamageReason == AUD_DAMAGE_REASON_DROWNING || damageStats.DamageReason == AUD_DAMAGE_REASON_SURFACE_DROWNING)
	{
		// depends if we're the player or not. If so, do stuff, otherwise keep schtum
		if (m_Parent && !m_Parent->IsLocalPlayer())
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_DROWNING || AUD_DAMAGE_REASON_SURFACE_DROWNING && !m_Parent->IsLocalPlayer()");
			return;
		}
	}
	else
	{
		if (damageStats.DamageReason == AUD_DAMAGE_REASON_ON_FIRE)
		{
			painLevel = AUD_ON_FIRE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_INHALE)
		{
			painLevel = AUD_PAIN_INHALE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_EXHALE)
		{
			painLevel = AUD_PAIN_EXHALE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_CLIMB_LARGE)
		{
			painLevel = AUD_CLIMB_LARGE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_CLIMB_SMALL)
		{
			painLevel = AUD_CLIMB_SMALL;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_JUMP)
		{
			painLevel = AUD_JUMP;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_MELEE_SMALL_GRUNT)
		{
			painLevel = AUD_MELEE_SMALL_GRUNT;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_MELEE_LARGE_GRUNT)
		{
			painLevel = AUD_MELEE_LARGE_GRUNT;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_COWER || damageStats.DamageReason == AUD_DAMAGE_REASON_WHIMPER)
		{
			// Don't play these contexts if the ped is in a vehicle
			if(m_Parent && m_Parent->GetIsInVehicle())
			{
				naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_COWER || AUD_DAMAGE_REASON_WHIMPER && m_Parent->GetIsInVehicle()");
				return;
			}

			if(m_AlternateCryingContext[0] != '\0')
			{
				naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - Calling Say(m_AlternateCryContext)");
				Say(m_AlternateCryingContext);
				return;
			}
			else
				painLevel = damageStats.DamageReason == AUD_DAMAGE_REASON_COWER && m_IsMale ? AUD_COWER : AUD_WHIMPER;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_DYING_MOAN)
		{
			painLevel = AUD_DYING_MOAN;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_CYCLING_EXHALE)
		{
			painLevel = AUD_CYCLING_EXHALE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_PAIN_RAPIDS)
		{
			painLevel = AUD_PAIN_RAPIDS;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_FALLING || damageStats.DamageReason == AUD_DAMAGE_REASON_SUPER_FALLING)
		{
			Vector3 pos = VEC3V_TO_VECTOR3(m_Parent->GetTransform().GetPosition());

			const float fSecondSurfaceInterp=0.0f;
			bool hitGround = false;
			float ground = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp, pos.x, pos.y, pos.z, &hitGround);

			painLevel = AUD_SUPER_HIGH_FALL;
			if(hitGround)
			{
				f32 height = pos.z - ground;
				if(height < g_MinHeightForHighFallContext)
				{
					naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_FALLING || AUD_DAMAGE_REASON_SUPER_FALLING && height < g_MinHeightForHighFallContext");
					return;
				}
				else if(height < g_MinHeightForSuperHighFallContext)
					painLevel = AUD_HIGH_FALL;
			}
			else
			{
				// If we don't hit the ground with the probe, then it's for 2 reasons.  We're really high, or spawned, so the collision hasn't had a chance to load in.
				// Do a velocity check so that we can get peds that are falling from great distance to scream, but not scream from 1 foot above the ground when spawning.
				// Also it sounds bad if we play screams the whole way down, so only play 1 for the initial fall, and then when the ground loads in then play the rest.
				if(m_HasPlayedInitialHighFallScream || m_Parent->GetVelocity().z < -1.0f * g_FallingScreamMinVelocityWithNoGroundProbe)
				{
					naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_FALLING || AUD_DAMAGE_REASON_SUPER_FALLING"
						"&& m_HasPlayedInitialHighFallScream || m_Parent->GetVelocity().z < -1.0f * g_FallingScreamMinVelocityWithNoGroundProbe");
					return;
				}
			}

			m_LastTimeFalling = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
			m_IsHighFall = true;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_PANIC)
		{
			if(m_Parent && m_Parent->GetIsInVehicle())
			{
				naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_SCREAM_PANIC && m_Parent->GetIsInVehicle()");
				return;
			}
			painLevel = AUD_SCREAM_PANIC;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_PANIC_SHORT)
		{
			if(m_Parent && m_Parent->GetIsInVehicle())
			{
				naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - AUD_DAMAGE_REASON_SCREAM_PANIC_SHORT && m_Parent->GetIsInVehicle()");
				return;
			}
			painLevel = AUD_SCREAM_PANIC_SHORT;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_SCARED)
		{
			painLevel = AUD_SCREAM_SCARED;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_SHOCKED)
		{
			painLevel = AUD_SCREAM_SHOCKED;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_TERROR)
		{
			painLevel = AUD_SCREAM_TERROR;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_POST_FALL_GRUNT)
		{
			painLevel = AUD_HIGH_FALL_DEATH;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_POST_FALL_GRUNT_LOW)
		{
			painLevel = audEngineUtil::ResolveProbability(0.5f) ? AUD_PAIN_LOW : AUD_PAIN_MEDIUM;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_SHOVE)
		{
			painLevel = AUD_PAIN_SHOVE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_WHEEZE)
		{
			painLevel = AUD_PAIN_WHEEZE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_COUGH)
		{
			painLevel = AUD_PAIN_COUGH;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_SNEEZE)
		{
			painLevel = AUD_PAIN_SNEEZE;
		}
		else if(damageStats.DamageReason == AUD_DAMAGE_REASON_TAZER)
		{
			painLevel = AUD_PAIN_TAZER;
		}
		else if(damageStats.RawDamage > g_HealthLostForHighPain)
		{
			painLevel = audEngineUtil::ResolveProbability(g_ProbabilityOfPainDrop) ? AUD_PAIN_MEDIUM : AUD_PAIN_HIGH;
		}
		else if(damageStats.RawDamage > g_HealthLostForMediumPain)
		{
			if(isMelee && audEngineUtil::ResolveProbability(g_ProbabilityOfPainDrop))
				painLevel = AUD_PAIN_LOW;
			else if(audEngineUtil::ResolveProbability(g_ProbabilityOfPainRise))
				painLevel = AUD_PAIN_HIGH;
			else
				painLevel = AUD_PAIN_MEDIUM;
		}
		// we want to make sure Niko's HIGH_FALL is interrupted for new falls, even if they're pretty low damage
		else if ( (damageStats.DamageReason == AUD_DAMAGE_REASON_FALLING || damageStats.DamageReason == AUD_DAMAGE_REASON_SUPER_FALLING) && 
			m_Parent && m_Parent->IsLocalPlayer() &&
			m_PainSpeechSound && m_LastPlayedSpeechContextPHash == g_HighFallContextPHash)
		{
			painLevel = AUD_PAIN_LOW;
		}
		else if (damageStats.RawDamage < g_HealthLostForLowPain)
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - damageStats.RawDamage < g_HealthLostForLowPain");
			return;
		}
		else
		{
			if(!isMelee || audEngineUtil::ResolveProbability(g_ProbabilityOfPainRise))
				painLevel = AUD_PAIN_MEDIUM;
			else
				painLevel = AUD_PAIN_LOW;
		}
	}
	// Are we playing anything - separate override levels for scripted and ambient speech - will doubtless also need
	// to special case different contexts at some point.
	/*
	if (GetIsHavingAConversation() && painLevel < AUD_ON_FIRE)
	{
		return;
	}
	*/

	if (m_ScriptedSpeechSound) // scripted speech
	{
		// Only interrupt it for death or being on fire, and have the script audio entity check independently so as to not kick anything else off
		// (21/01/08) We now interrupt for any pain, so that punching peds will interrupt them - we do this by pausing and unpausing the conv,
		// as this will cause the line to be played again - probably best. 
		// ScriptAudioEntity will notice a ped's playing pain, and if it's the next speaker, wait X seconds before allowing the next line.
		if (painLevel >= AUD_ON_FIRE && painLevel < AUD_COWER)
		{
			// do we want to repeat this line
			bool repeat = true;
			u32 playedTime = GetActiveScriptedSpeechPlayTimeMs();
			if (playedTime>g_SpeechPlayedEnoughNotToRepeatTime)
			{
				repeat = false;
			}
			g_ScriptAudioEntity.PauseScriptedConversation(false, repeat);
			g_ScriptAudioEntity.RestartScriptedConversation();
		}
		else
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - m_ScriptedSpeechSound && painLevel < AUD_ON_FIRE");
			return;
		}
	}
	else if (m_AmbientSpeechSound)
	{
		// Don't interrupt on-fire/high-fall for more on-fire/high-fall - don't interrupt on-fire for non-death pain
		if ((m_LastPlayedSpeechContextPHash == g_OnFireContextPHash && painLevel <= AUD_ON_FIRE) || 
			(m_LastPlayedSpeechContextPHash == g_HighFallContextPHash && damageStats.DamageReason == AUD_DAMAGE_REASON_FALLING) ||
			(m_LastPlayedSpeechContextPHash == g_SuperHighFallContextPHash && damageStats.DamageReason == AUD_DAMAGE_REASON_SUPER_FALLING) ||
			(m_LastPlayedSpeechContextPHash == g_DrowningContextPHash && damageStats.DamageReason == AUD_DAMAGE_REASON_DROWNING && !damageStats.Fatal) ||
			(m_LastPlayedSpeechContextPHash == g_CoughContextPHash && damageStats.DamageReason == AUD_DAMAGE_REASON_SURFACE_DROWNING && !damageStats.Fatal))
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - Don't interrupt on-fire/high-fall for more on-fire/high-fall - don't interrupt on-fire for non-death pain");
			return;
		}
		// DODGE is weird, because you so often injure the ped at the same time. If it doesn't kill them, let's not interrupt.
		// Same for a few others - and let's interrupt 1/20, so if you riddle them with bullets it'll shut them up
		else if (painLevel<=AUD_PAIN_HIGH &&
			(m_LastPlayedSpeechContextPHash == g_SpeechContextDodgePHash ||
			 m_LastPlayedSpeechContextPHash == g_SpeechContextGangDodgeWarningPHash ||
			 m_LastPlayedSpeechContextPHash == g_SpeechContextBeenShotPHash ||
			 (m_LastPlayedSpeechContextPHash == g_HighFallContextPHash && damageStats.DamageReason != AUD_DAMAGE_REASON_FALLING)||
			 (m_LastPlayedSpeechContextPHash == g_SuperHighFallContextPHash && damageStats.DamageReason != AUD_DAMAGE_REASON_SUPER_FALLING)||
			 m_LastPlayedSpeechContextPHash == g_OnFireContextPHash ||
			 m_LastPlayedSpeechContextPHash == g_PanicContextPHash ||
			 m_LastPlayedSpeechContextPHash == g_PanicShortContextPHash ||
			 m_LastPlayedSpeechContextPHash == g_SpeechContextShotInLegPHash) &&
			 audEngineUtil::ResolveProbability(0.95f))
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - DODGE is weird, because you so often injure the ped at the same time. If it doesn't kill them, let's not interrupt.");
			return;
		}
		//Try not to interrupt bump or pissed off lines when damage is just from a little fall or bump
		else if(painLevel <= AUD_PAIN_LOW && 
			    (damageStats.DamageReason == AUD_DAMAGE_REASON_FALLING || damageStats.DamageReason == AUD_DAMAGE_REASON_SUPER_FALLING) &&
				(m_LastPlayedSpeechContextPHash == g_SpeechContextBumpPHash ||
				 m_LastPlayedSpeechContextPHash == g_SpeechContextSurprisedPHash ||
				 m_LastPlayedSpeechContextPHash == g_SpeechContextInsultHighPHash ||
				 m_LastPlayedSpeechContextPHash == g_SpeechContextInsultMedPHash ||
				 m_LastPlayedSpeechContextPHash == g_SpeechContextShockedHighPHash ||
				 m_LastPlayedSpeechContextPHash == g_SpeechContextShockedMedPHash ||
				 m_LastPlayedSpeechContextPHash == g_SpeechContextCurseMedPHash ||
				 m_LastPlayedSpeechContextPHash == g_SpeechContextCurseMedPHash
				)
			   )
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - Try not to interrupt bump or pissed off lines when damage is just from a little fall or bump");
			return;
		}
		// If we're playing flee type ambient VO don't cancel it out for flee type pain
		else if ((damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_SHOCKED 
					|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_TERROR 
					|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_PANIC
					|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_PANIC_SHORT
					|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_SCARED)
				&& 
				(m_LastPlayedSpeechContextPHash == g_SpeechContextFrightenedHighPHash
				 	|| m_LastPlayedSpeechContextPHash == g_SpeechContextShockedHighPHash))
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - If we're playing flee type ambient VO don't cancel it out for flee type pain");
			return;
		}
		// If we're playing ambient speech triggered from script make sure we're not stopping it for a panic reason.
		else if((damageStats.DamageReason == AUD_DAMAGE_REASON_COWER 
				|| damageStats.DamageReason == AUD_DAMAGE_REASON_WHIMPER
				|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_SHOCKED 
				|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_TERROR 
				|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_PANIC
				|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_PANIC_SHORT
				|| damageStats.DamageReason == AUD_DAMAGE_REASON_SCREAM_SCARED)
			&& m_IsFromScript)
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - If we're playing ambient speech triggered from script make sure we're not stopping it for a panic reason.");
			return;
		}
	}

	//I think this is the right move.  It will get rid of some asserts we've been seeing, and it seems to me that if speech hasn't
	//	started by now, we may as well give pain the opportunity.
	if(m_WaitingForAmbientSlot)
	{
		m_WaitingForAmbientSlot = false;
		m_AmbientSlotId = -1;
	}

	// We do Fire and High Fall as regular speech
	bool actuallyPlayedSomething = false;
	if (painLevel == AUD_ON_FIRE && CLocalisation::GermanGame())
	{
		actuallyPlayedSomething = PlayPain(AUD_PAIN_LOW, damageStats, distanceSqToNearestListener);
	}
	else if (damageStats.DamageReason == AUD_DAMAGE_REASON_DROWNING && !damageStats.Fatal)
	{
		//only works for the player
		if(!m_Parent || !m_Parent->IsLocalPlayer())
		{
			naSpeechEntDebugf2(this, "ReallyInflictPain() FAILED - damageStats.DamageReason == AUD_DAMAGE_REASON_DROWNING && !damageStats.Fatal && !m_Parent->IsLocalPlayer()");
			return;
		}
		g_ScriptAudioEntity.GetSpeechAudioEntity().Say("DROWNING", "SPEECH_PARAMS_STANDARD", g_SpeechManager.GetWaveLoadedPainVoiceFromVoiceType(AUD_PAIN_VOICE_TYPE_PLAYER, m_Parent));
	}
	else if (damageStats.DamageReason == AUD_DAMAGE_REASON_SURFACE_DROWNING && !damageStats.Fatal)
	{
		actuallyPlayedSomething = PlayPain(AUD_PAIN_COUGH, damageStats, distanceSqToNearestListener);
	}
	// if it's the player, and they recent did HIGH_FALL, do FUCK_FALL instead of regular pain
	/*else if (m_Parent && m_Parent->IsLocalPlayer() && painLevel<=AUD_PAIN_HIGH && (sm_TimeLastPlayedHighFall+5000)>fwTimer::GetTimeInMilliseconds())
	{
		bool sayFuckFall = Say("FUCK_FALL", "SPEECH_PARAMS_STANDARD", g_PainVoice) != AUD_SPEECH_SAY_FAILED;
		if (!sayFuckFall && !(m_AmbientSpeechSound && m_LastPlayedSpeechContextPHash==g_FuckFallContextPHash))
		{
			actuallyPlayedSomething = PlayPain(painLevel, damageStats.RawDamage, damageStats.PlayGargle, damageStats.Inflictor);
		}
	}*/
	else
	{
		actuallyPlayedSomething = PlayPain(painLevel, damageStats, distanceSqToNearestListener);
	}
	// If we played something, cancel any ambient speech
	if(actuallyPlayedSomething)
	{		
		FreeSlot(false, false);
		if(m_Parent)
			m_Parent->SetIsWaitingForSpeechToPreload(false);
		m_Preloading = false;
	}
}

bool audSpeechAudioEntity::PlayPain(u32 painLevel, audDamageStats damageStats, f32 distSqToVolListener)
{
	naSpeechEntDebugf1(this, "PlayPain()");

	m_PositionForNullSpeaker.Set(ORIGIN);
	m_EntityForNullSpeaker = NULL;

	if(!m_Parent)
	{
		naAssertf(false, "speechaudioentity has no parent (Cped) assigned during PlayPain, aborting");
		naSpeechEntDebugf2(this, "PlayPain() FAILED - !m_Parent");
		return false;
	}

	if(!g_AudioEngine.IsAudioEnabled())
	{
		naSpeechEntDebugf2(this, "PlayPain() FAILED - !g_AudioEngine.IsAudioEnabled()");
		return false;
	}

	if(g_DisablePain || g_PlayerSwitch.IsActive())
	{
		naSpeechEntDebugf2(this, "PlayPain() FAILED - g_DisablePain");
		return false;
	}

	/*
	if ((m_SpeakingDisabled || m_SpeakingDisabledSynced) && m_Parent && !m_Parent->IsLocalPlayer())
	{
		return false;
	}
	*/

	u32 currentTime = audNorthAudioEngine::GetCurrentTimeInMs();
	if(painLevel == AUD_DYING_MOAN && painLevel==m_LastPainLevel)
	{
		if(m_LastPainTimes[0]+g_TimeBetweenDyingMoans > currentTime)
		{
			naSpeechEntDebugf2(this, "PlayPain() FAILED - m_LastPainTimes[0] (%u) + g_TimeBetweenDyingMoans (%u) > currentTime (%u)",
				m_LastPainTimes[0], g_TimeBetweenDyingMoans, currentTime);
			return false;
		}
	}
	// Don't play pain for the same ped too quickly, unless it's a higher level of pain (ignore for painLevels below NONE)
	else if (painLevel > AUD_PAIN_NONE && painLevel != AUD_COWER && painLevel != AUD_WHIMPER &&
		painLevel<=m_LastPainLevel && m_LastPainTimes[0]+g_TimeBetweenPain > currentTime)
	{
		naSpeechEntDebugf2(this, "PlayPain() FAILED - m_LastPainTimes[0] (%u) + g_TimeBetweenPain (%u) > currentTime (%u)",
			m_LastPainTimes[0], g_TimeBetweenPain, currentTime);
		return false;
	}

	// We want a half second or so of time in between coughs where they'd be breathing.
	if(painLevel == AUD_PAIN_COUGH && !damageStats.Fatal && !damageStats.IsFromAnim && g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) < m_LungDamageNextCoughTime)
	{
		naSpeechEntDebugf2(this, "PlayPain() FAILED - We want a half second or so of time in between coughs where they'd be breathing.");
		return false;
	}

	// Don't play if we're already playing pain - unless it's to interupt pain for death
	//	or the already playing pain is a scream that hasn't started yet
	if (m_PainSpeechSound)
	{
		if ( (m_LastPainLevel <= AUD_ON_FIRE && painLevel >= AUD_DEATH_LOW) ||
			(m_LastPainLevel >= AUD_SCREAM_PANIC_SHORT && m_LastPainLevel <= AUD_SCREAM_TERROR && m_PainSpeechSound->GetPlayState() != AUD_SOUND_PLAYING )
			)
		{
			naSpeechEntDebugf2(this, "FreeSlot() Interrupting pain for death or a scream that hasn't started yet");
			FreeSlot();
		}
	}
	if (m_PainEchoSound)
	{
		if ( (m_LastPainLevel <= AUD_ON_FIRE && painLevel >= AUD_DEATH_LOW) ||
			(m_LastPainLevel >= AUD_SCREAM_PANIC_SHORT && m_LastPainLevel <= AUD_SCREAM_TERROR && m_PainEchoSound->GetPlayState() != AUD_SOUND_PLAYING )
			)
		{
			naSpeechEntDebugf2(this, "FreeSlot() Interrupting pain for death or a scream that hasn't started yet");
			FreeSlot();
		}
	}
	if (!m_PainSpeechSound && !m_PainEchoSound)
	{
		m_PlaySpeechWhenFinishedPreparing = false;

		BANK_ONLY(g_MaxDistanceSqToPlayPain = g_MaxDistanceToPlayPain * g_MaxDistanceToPlayPain;)
		// Don't play anything for peds further than 40m away from the camera (need to later decide which camera, etc)
		if (distSqToVolListener > g_MaxDistanceSqToPlayPain)
		{
			naSpeechEntDebugf2(this, "PlayPain() FAILED - distSqToVolListener > g_MaxDistanceSqToPlayPain");
			return false;
		}

		// Get the wave slot and variation to use
		bool isPlayer = false;
		u32 painVoiceType = AUD_PAIN_VOICE_TYPE_MALE;
		if (!NetworkInterface::IsGameInProgress() && 
			!g_TestFallingVoice &&
			(m_Parent->GetPedType() == PEDTYPE_PLAYER_0 || m_Parent->GetPedType() == PEDTYPE_PLAYER_1 || m_Parent->GetPedType() == PEDTYPE_PLAYER_2)
			)
		{
			isPlayer = true;
			painVoiceType = AUD_PAIN_VOICE_TYPE_PLAYER;
		}
		else if (!m_IsMale)
		{
			painVoiceType = AUD_PAIN_VOICE_TYPE_FEMALE;
		}

		u32 painType = AUD_PAIN_ID_PAIN_LOW_GENERIC;

		bool lowHealth = false;
		if(m_Parent && m_Parent->GetPedHealthInfo())
		{
			if(m_Parent->GetPedHealthInfo()->GetInjuredHealthThreshold() > m_Parent->GetHealth())
				lowHealth = true;
		}
		//if the last pain was more than 3 seconds ago, we don't drop toughness level.  If it was less AND the one before that was less as well
		//	then drop level
		if(currentTime - m_LastPainTimes[0] > 3000)
			m_DropToughnessLevel = false;
		else if(currentTime - m_LastPainTimes[1] < 3000)
			m_DropToughnessLevel = true;

		lowHealth |= m_DropToughnessLevel;
		lowHealth |= GetPedToughness() == AUD_TOUGHNESS_MALE_COWARDLY;

		if(damageStats.PlayGargle)
		{
			painType = AUD_PAIN_ID_DEATH_GARGLE;
			m_PedHasGargled = !damageStats.IsFromAnim;
		}
		else if(painLevel == AUD_CLIMB_LARGE)
		{
			painType = AUD_PAIN_ID_CLIMB_LARGE;
		}
		else if(painLevel == AUD_CLIMB_SMALL)
		{
			painType = AUD_PAIN_ID_CLIMB_SMALL;
		}
		else if(painLevel == AUD_JUMP)
		{
			painType = AUD_PAIN_ID_JUMP;
		}
		else if(painLevel == AUD_MELEE_SMALL_GRUNT)
		{
			painType = AUD_PAIN_ID_MELEE_SMALL_GRUNT;
		}
		else if(painLevel == AUD_MELEE_LARGE_GRUNT)
		{
			painType = AUD_PAIN_ID_MELEE_LARGE_GRUNT;
		}
		else if(painLevel == AUD_COWER)
		{
			painType = AUD_PAIN_ID_COWER;
		}
		else if(painLevel == AUD_WHIMPER)
		{
			painType = AUD_PAIN_ID_WHIMPER;
		}
		else if(painLevel == AUD_DYING_MOAN)
		{
			painType = AUD_PAIN_ID_DYING_MOAN;
		}
		else if(painLevel == AUD_PAIN_RAPIDS)
		{
			painType = AUD_PAIN_ID_PAIN_RAPIDS;
		}
		else if(painLevel == AUD_CYCLING_EXHALE)
		{
			painType = AUD_PAIN_ID_CYCLING_EXHALE;
		}
		else if(painLevel == AUD_PAIN_EXHALE)
		{
			painType = AUD_PAIN_ID_EXHALE;
		}
		else if(painLevel == AUD_PAIN_INHALE)
		{
			painType = AUD_PAIN_ID_INHALE;
		}
		else if(painLevel == AUD_PAIN_SHOVE)
		{
			painType = AUD_PAIN_ID_SHOVE;
		}
		else if(painLevel == AUD_PAIN_WHEEZE)
		{
			painType = AUD_PAIN_ID_WHEEZE;
		}
		else if(painLevel == AUD_PAIN_COUGH)
		{
			painType = AUD_PAIN_ID_COUGH;
		}
		else if(painLevel == AUD_PAIN_SNEEZE)
		{
			painType = AUD_PAIN_ID_SNEEZE;
		}
		else if(painLevel == AUD_HIGH_FALL)
		{
			painType = AUD_PAIN_ID_HIGH_FALL;
		}
		else if(painLevel == AUD_SUPER_HIGH_FALL)
		{
			painType = AUD_PAIN_ID_SUPER_HIGH_FALL;
		}
		else if(painLevel == AUD_HIGH_FALL_DEATH)
		{
			painType = AUD_PAIN_ID_DEATH_HIGH_SHORT;
		}
		else if(painLevel == AUD_ON_FIRE)
		{
			painType = AUD_PAIN_ID_ON_FIRE;
		}
		else if(painLevel == AUD_SCREAM_PANIC)
		{
			painType = AUD_PAIN_ID_SCREAM_PANIC;
		}
		else if(painLevel == AUD_SCREAM_PANIC_SHORT)
		{
			painType = AUD_PAIN_ID_SCREAM_PANIC_SHORT;
		}
		else if(painLevel == AUD_SCREAM_SCARED)
		{
			painType = AUD_PAIN_ID_SCREAM_SCARED;
		}
		else if(painLevel == AUD_PAIN_TAZER)
		{
			painType = AUD_PAIN_ID_TAZER;
		}
#if __BANK
		else if(g_UseTestPainBanks)
		{
			switch (painLevel)
			{
			case AUD_PAIN_LOW:
				painType = AUD_PAIN_ID_PAIN_LOW_GENERIC;
				break;
			case AUD_PAIN_MEDIUM:
				painType = AUD_PAIN_ID_PAIN_MEDIUM_GENERIC;
				break;
			case AUD_PAIN_HIGH:
				painType = AUD_PAIN_ID_PAIN_HIGH_GENERIC;
				break;
			case AUD_DEATH_LOW:
				painType = AUD_PAIN_ID_DEATH_LOW_GENERIC;
				break;
			case AUD_DEATH_HIGH:
				if(damageStats.ForceDeathShort)
					painType = AUD_PAIN_ID_DEATH_HIGH_SHORT;
				else if(damageStats.ForceDeathMedium)
					painType = AUD_PAIN_ID_DEATH_HIGH_MEDIUM;
				else if(damageStats.ForceDeathLong)
					painType = AUD_PAIN_ID_DEATH_HIGH_LONG;
				else
					painType = GetHighDeathPainTypeFromDistance(Sqrtf(distSqToVolListener));
				break;
			case AUD_DEATH_UNDERWATER:
				painType = AUD_PAIN_ID_DEATH_UNDERWATER;
				break;
			case AUD_SCREAM_SHOCKED:
				painType = AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC;
				break;
			case AUD_SCREAM_TERROR:
				painType = AUD_PAIN_ID_SCREAM_TERROR_GENERIC;
				break;
			}
		}
#endif
		else
		{
			switch(GetPedToughness())
			{
			case AUD_TOUGHNESS_MALE_COWARDLY:
			case AUD_TOUGHNESS_FEMALE:
			case AUD_TOUGHNESS_FEMALE_BUM:
			case AUD_TOUGHNESS_MALE_NORMAL:
			case AUD_TOUGHNESS_MALE_BUM:
				switch (painLevel)
				{
				case AUD_PAIN_LOW:
					painType = lowHealth ? AUD_PAIN_ID_PAIN_LOW_WEAK : AUD_PAIN_ID_PAIN_LOW_GENERIC;
					break;
				case AUD_PAIN_MEDIUM:
					painType = lowHealth ? AUD_PAIN_ID_PAIN_MEDIUM_WEAK : AUD_PAIN_ID_PAIN_MEDIUM_GENERIC;
					break;
				case AUD_PAIN_HIGH:
					painType = lowHealth ? AUD_PAIN_ID_PAIN_HIGH_WEAK : AUD_PAIN_ID_PAIN_HIGH_GENERIC;
					break;
				case AUD_DEATH_LOW:
					painType = (lowHealth && m_IsMale) ? AUD_PAIN_ID_DEATH_LOW_WEAK : AUD_PAIN_ID_DEATH_LOW_GENERIC;
					break;
				case AUD_DEATH_HIGH:
					if(damageStats.ForceDeathShort)
						painType = AUD_PAIN_ID_DEATH_HIGH_SHORT;
					else if(damageStats.ForceDeathMedium)
						painType = AUD_PAIN_ID_DEATH_HIGH_MEDIUM;
					else if(damageStats.ForceDeathLong)
						painType = AUD_PAIN_ID_DEATH_HIGH_LONG;
					else
						painType = GetHighDeathPainTypeFromDistance(Sqrtf(distSqToVolListener));
					break;
				case AUD_DEATH_UNDERWATER:
					painType = AUD_PAIN_ID_DEATH_UNDERWATER;
					break;
				case AUD_SCREAM_SHOCKED:
					painType = lowHealth ? AUD_PAIN_ID_SCREAM_SHOCKED_WEAK : AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC;
					break;
				case AUD_SCREAM_TERROR:
					painType = lowHealth ? AUD_PAIN_ID_SCREAM_TERROR_WEAK : AUD_PAIN_ID_SCREAM_TERROR_GENERIC;
					break;
				}
				break;
			case AUD_TOUGHNESS_MALE_BRAVE:
			case AUD_TOUGHNESS_MALE_GANG:
			case AUD_TOUGHNESS_COP:
				switch (painLevel)
				{
				case AUD_PAIN_LOW:
					painType = lowHealth ? AUD_PAIN_ID_PAIN_LOW_GENERIC : AUD_PAIN_ID_PAIN_LOW_TOUGH;
					break;
				case AUD_PAIN_MEDIUM:
					painType = lowHealth ? AUD_PAIN_ID_PAIN_MEDIUM_GENERIC : AUD_PAIN_ID_PAIN_MEDIUM_TOUGH;
					break;
				case AUD_PAIN_HIGH:
					painType = lowHealth ? AUD_PAIN_ID_PAIN_HIGH_GENERIC : AUD_PAIN_ID_PAIN_HIGH_TOUGH;
					break;
				case AUD_DEATH_LOW:
					painType = lowHealth ? AUD_PAIN_ID_DEATH_LOW_GENERIC : AUD_PAIN_ID_DEATH_LOW_TOUGH;
					break;
				case AUD_DEATH_HIGH:
					if(damageStats.ForceDeathShort)
						painType = AUD_PAIN_ID_DEATH_HIGH_SHORT;
					else if(damageStats.ForceDeathMedium)
						painType = AUD_PAIN_ID_DEATH_HIGH_MEDIUM;
					else if(damageStats.ForceDeathLong)
						painType = AUD_PAIN_ID_DEATH_HIGH_LONG;
					else
						painType = GetHighDeathPainTypeFromDistance(Sqrtf(distSqToVolListener));
					break;
				case AUD_DEATH_UNDERWATER:
					painType = AUD_PAIN_ID_DEATH_UNDERWATER;
					break;
				case AUD_SCREAM_SHOCKED:
					painType = lowHealth ? AUD_PAIN_ID_SCREAM_SHOCKED_GENERIC : AUD_PAIN_ID_SCREAM_SHOCKED_TOUGH;
					break;
				case AUD_SCREAM_TERROR:
					painType = lowHealth ? AUD_PAIN_ID_SCREAM_TERROR_GENERIC : AUD_PAIN_ID_SCREAM_TERROR_TOUGH;
					break;
				}
				break;
			}
		}
		
		if(damageStats.Inflictor && damageStats.Inflictor->GetType() == ENTITY_TYPE_PED)
		{
			CPed* inflictorPed = static_cast<CPed*>(damageStats.Inflictor.Get());
			if(inflictorPed && inflictorPed->IsLocalPlayer())
				g_SpeechManager.RegisterPedAimedAtOrHurt(GetPedToughness(), lowHealth, true);
		}

		if(painType > AUD_PAIN_ID_NUM_BANK_LOADED)
		{
			naSpeechEntDebugf2(this, "PlayPain() FAILED - Calling PlayWaveLoadedPain()");

#if __BANK
			g_LastRecordedPainValue = damageStats.RawDamage;
			g_DebugNumTimesPainTypePlayed[painType]++;
#endif
			PlayWaveLoadedPain(painType, painVoiceType, damageStats.PreDelay);
			//Let's not track this for waveloaded non-pain pain (if you know what I mean), at least for now
			if(painLevel > AUD_PAIN_NONE)
			{
				m_LastPainLevel = painLevel;
				m_LastPainTimes[1] = m_LastPainTimes[0];
				m_LastPainTimes[0] = currentTime;
			}
			if (painLevel>=AUD_DEATH_LOW)
			{
				m_PedHasDied = true;
			}
#if __BANK
			char msg[128] = "";
			formatf(msg, 128, "%s %s", g_PainSlotType[painVoiceType], g_PainContexts[painType]);
			AddVFXDebugDrawStruct(msg,m_Parent);
#endif
			//seems this return value is only used to stop ambient speech.  We'll take care of this in PlayWaveLoadedPain()
			return false;
		}

		bool isSpecificPain = false;
		const char* painLevelString = g_SpeechManager.GetPainLevelString(GetPedToughness(), lowHealth, isSpecificPain);
		//we need to hold on to this value for indexing g_PainSlotType
		u32 nonSpecificPainVoiceType = painVoiceType; 
		painVoiceType = isSpecificPain && !isPlayer ? AUD_PAIN_VOICE_TYPE_SPECIFIC : painVoiceType;
		// Make sure we have at least some pain loaded - this won't be true for the first few seconds of a game (or more likely a NewGame)
		if (!g_SpeechManager.IsPainLoaded(painVoiceType))
		{
			naSpeechEntDebugf2(this, "PlayPain() FAILED - !g_SpeechManager.IsPainLoaded(painVoiceType)");
			return false;
		}

		audWaveSlot* waveSlot = g_SpeechManager.GetWaveSlotForPain(painVoiceType);
		u32 variationNum = g_SpeechManager.GetVariationForPain(painVoiceType, painType);
		u8 bankNum = g_SpeechManager.GetBankNumForPain(painVoiceType);

		// Work out our voice name
		char voiceName[64] = "";
		if(isPlayer)
		{
			formatf(voiceName, 64, "%s_%02d", g_PainSlotType[nonSpecificPainVoiceType], bankNum);
		}
		else
			formatf(voiceName, 64, "%s_%s_%02d", g_PainSlotType[nonSpecificPainVoiceType], painLevelString, bankNum);


		audSoundInitParams initParam;
		initParam.WaveSlot = waveSlot;
		initParam.AllowLoad = false;
		initParam.AllowOrphaned = true;
		initParam.UpdateEntity = true;
		initParam.RemoveHierarchy = false;
		initParam.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
		initParam.Predelay = damageStats.PreDelay;
		naCErrorf(m_Parent->GetPlaceableTracker(), "Unable to get tracker from CPed parent in PlayPain");

		const u32 contextPHash = atPartialStringHash(GetPainContext(painType));
		const SpeechContext* speechContext = g_SpeechManager.GetSpeechContext(atFinalizeHash(atPartialStringHash("_SC", contextPHash)), m_Parent, NULL, NULL);

		audSpeechVolume volType = AUD_SPEECH_NORMAL;
		if(naVerifyf(speechContext, "We do not have a valid SpeechContext game object for pain context: %s", GetPainContext(painType)))
		{
			volType = GetVolTypeForSpeechContext(speechContext, contextPHash);
		}

		// Are we in the same vehicle as the player? If so, play speech frontend
		if (IsPedInPlayerCar(m_Parent) || damageStats.IsFrontend)
		{
			volType = AUD_SPEECH_FRONTEND;
		}

		const audConversationAudibility audibility = GetAudibilityForSpeechContext(speechContext);

		const audMetadataRef soundRef = g_PainSoundSet[painType].Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));
		naAssertf(soundRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");

		m_HasPreparedAndPlayed = false;
		CreateSound_PersistentReference(soundRef, (audSound**)&m_PainSpeechSound, &initParam);

		if (!m_PainSpeechSound)
		{
			naSpeechEntDebugf2(this, "PlayPain() FAILED - !m_PainSpeechSound");
			return false;
		}

		if(!(((audSpeechSound*)m_PainSpeechSound)->InitSpeech(atStringHash(voiceName), GetPainContext(painType), variationNum)))
		{
			bool success = false;
			if(painType == AUD_PAIN_ID_HIGH_FALL_DEATH)
			{
				success = (((audSpeechSound*)m_PainSpeechSound)->InitSpeech(atStringHash(voiceName), GetPainContext(AUD_PAIN_ID_DEATH_HIGH_SHORT), variationNum));
			}
			if(!success)
			{
				naSpeechEntDebugf2(this, "PlayPain() FAILED - Unable to initialise speech in PlayPain");
				naErrorf("Unable to initialise speech in PlayPain %s , %s", voiceName, GetPainContext(painType));
				m_PainSpeechSound->StopAndForget();
				return false;
			}
		}

		m_PainSpeechSound->Prepare(waveSlot);
		m_PlaySpeechWhenFinishedPreparing = true;
		m_UseShadowSounds = true;

		m_CachedSpeechType = AUD_SPEECH_TYPE_PAIN;
		m_CachedVolType = volType;
		m_CachedAudibility = audibility;
		m_SpeechVolumeOffset = 0.0f;
		m_IsFromScript = false;
		m_LastPlayedWaveSlot = waveSlot;
		m_LastPlayedSpeechContextPHash = contextPHash;
		m_LastPlayedSpeechVariationNumber = variationNum;
		m_CurrentlyPlayingVoiceNameHash = atStringHash(voiceName);
		m_CurrentlyPlayingSoundSet = g_PainSoundSet[painType];
		BANK_ONLY(safecpy(m_CurrentlyPlayingSpeechContext, GetPainContext(painType));)

#if GTA_REPLAY
		// Mute/pitch flags are stored in the replay packet during replay playback
		if(!CReplayMgr::IsEditModeActive())
#endif
		{
			m_IsMutedDuringSlowMo = (speechContext ? AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISMUTEDDURINGSLOWMO)!=AUD_TRISTATE_FALSE : true)
				&& (!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowAmbientSpeechInSlowMo));
			m_IsPitchedDuringSlowMo = (speechContext ? AUD_GET_TRISTATE_VALUE(speechContext->Flags, FLAG_ID_SPEECHCONTEXT_ISMUTEDDURINGSLOWMO)!=AUD_TRISTATE_FALSE : true)
				&& (!g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::AllowAmbientSpeechInSlowMo));
		}

#if GTA_REPLAY
		// Store these so we can put them in the Speech Pain packet
		u8 previousVariationForPain = (u8)g_SpeechManager.GetVariationForPain(painVoiceType, painType);
		u8 previousNumTimesPlayed	= g_SpeechManager.GetNumTimesPlayed(painVoiceType, painType);
		u32 previousNextTimeToLoad	= g_SpeechManager.GetNextTimeToLoad(painVoiceType);
#endif // GTA_REPLAY

		// Skip this during replay playback as it advances pain information we don't want handled for us
		// The CPacketSpeechPain will call PlayedPainForReplay instead
		REPLAY_ONLY(if(CReplayMgr::IsEditModeActive() == false))
			g_SpeechManager.PlayedPain(painVoiceType, painType);

		m_VisemeAnimationData = NULL;

#if __BANK
		g_LastRecordedPainValue = damageStats.RawDamage;
		g_DebugNumTimesPainTypePlayed[painType]++;
		char msg[128] = "";
		formatf(msg, 128, "%s %s_%02d", voiceName, isPlayer ? GetPainContext(painType) : g_PainContexts[painType], variationNum);
		AddVFXDebugDrawStruct(msg,m_Parent);
#endif

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketSpeechPain>(
				CPacketSpeechPain(	(u8)painLevel, 
				damageStats, 
				distSqToVolListener,
				(u8)painVoiceType, 
				(u8)painType,
				previousVariationForPain,
				previousNumTimesPlayed,
				previousNextTimeToLoad,
				(u8)g_SpeechManager.GetVariationForPain(painVoiceType, painType), // variationNum
				g_SpeechManager.GetNumTimesPlayed(painVoiceType, painType),
				g_SpeechManager.GetNextTimeToLoad(painVoiceType),
				m_LastPainLevel, 
				m_LastPainTimes.GetElements()), 
				m_Parent, damageStats.Inflictor);
		}
#endif // GTA_REPLAY

		m_LastPainLevel = painLevel;
		m_LastPainTimes[1] = m_LastPainTimes[0];
		m_LastPainTimes[0] = currentTime;
		if (painLevel>=AUD_DEATH_LOW)
		{
			m_PedHasDied = true;
		}

		naSpeechEntDebugf2(this, "PlayPain() SUCCESS");

		// We can actually play the speech, so update everything
		return true;
	}

	naSpeechEntDebugf2(this, "PlayPain() FAILED - Pain already playing");
	return false;
}

const char* audSpeechAudioEntity::GetPainContext(u32 painType) const
{
	if(m_Parent && m_Parent->IsLocalPlayer())
		return g_PlayerPainContexts[painType];
	else
		return g_PainContexts[painType];
}

void audSpeechAudioEntity::PlayWaveLoadedPain(u32 painType, u32 painVoiceType, s32 preDelay)
{
	naSpeechEntDebugf1(this, "PlayWaveLoadedPain() painType: %u painVoiceType: %u preDelay: %d", painType, painVoiceType, preDelay);
	naSpeechEntDebugf2(this, "FreeSlot(false) in PlayWaveLoadedPain");
	FreeSlot(false);
	audSaySuccessValue success = AUD_SPEECH_SAY_FAILED;
	m_WaveLoadedPainType = painType;
	if(Verifyf(painType > AUD_PAIN_ID_NUM_BANK_LOADED && painType < AUD_PAIN_ID_NUM_TOTAL, "Invalid painType value passed to PlayWaveLoadedPain: %u", painType))
		success = Say(GetPainContext(painType), "SPEECH_PARAMS_FORCE", g_SpeechManager.GetWaveLoadedPainVoiceFromVoiceType(painVoiceType, m_Parent), preDelay);

	if(success != AUD_SPEECH_SAY_FAILED)
	{
		if(painType == AUD_PAIN_ID_HIGH_FALL || painType == AUD_PAIN_ID_SUPER_HIGH_FALL)
		{
			// If we haven't registered that we're falling do that now
			if(!m_IsFalling)
			{
				RegisterFallingStart();
			}
			m_bIsFallScreaming = true;
			m_HasPlayedInitialHighFallScream = true;
		}

		if(painType == AUD_PAIN_ID_COUGH)
		{
			m_IsCoughing = true;
		}
	}
}

u32 audSpeechAudioEntity::GetHighDeathPainTypeFromDistance(f32 dist)
{
	f32 longOdds = Clamp((dist - g_MinDistForLongDeath)/ (g_MaxDistanceToPlayPain - g_MinDistForLongDeath), 0.0f, 1.0f);
	f32 shortOdds = Clamp(1.0f - (dist/ g_MaxDistForShortDeath), 0.0f, 1.0f);
	f32 mediumOdds = Clamp(1.0f - longOdds - shortOdds, 0.0f, 1.0f);

	f32 val = audEngineUtil::GetRandomNumberInRange(0.0f, 1.0f);

	if(val < shortOdds)
		return AUD_PAIN_ID_DEATH_HIGH_SHORT;
	if(val < shortOdds + mediumOdds)
		return AUD_PAIN_ID_DEATH_HIGH_MEDIUM;

	return AUD_PAIN_ID_DEATH_HIGH_LONG;
}

audSpeechVolume audSpeechAudioEntity::GetVolTypeForSpeechContext(const SpeechContext* context, const u32 contextPHash) const
{
	audSpeechVolume volType = AUD_SPEECH_NORMAL;

	if(context)
	{
		switch(context->VolumeType)
		{
		case CONTEXT_VOLUME_NORMAL:
		case CONTEXT_VOLUME_PAIN:
			volType = AUD_SPEECH_NORMAL;
			break;
		case CONTEXT_VOLUME_SHOUTED:
			volType = AUD_SPEECH_SHOUT;
			break;
		case CONTEXT_VOLUME_MEGAPHONE:
		case CONTEXT_VOLUME_FROM_HELI:
			volType = AUD_SPEECH_MEGAPHONE;
			break;
		case CONTEXT_VOLUME_FRONTEND:
			volType = AUD_SPEECH_FRONTEND;
			break;
		default:
			naAssertf(false, "Invalid VolumeType on SpeechContext GO: %s value: %u",
				audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(context->NameTableOffset),
				context->VolumeType);
			break;
		}
	}

	if((!m_Parent && contextPHash == g_DrowningContextPHash) || contextPHash == g_DeathUnderwaterPHash)
	{
		volType = AUD_SPEECH_FRONTEND;
	}

#if __BANK
	if(g_ShouldOverrideSpeechVolumeType && m_IsDebugEntity 
		&& naVerifyf(g_SpeechVolumeTypeOverride < AUD_NUM_SPEECH_VOLUMES, "Invalid g_SpeechVolumeTypeOverride - %u", g_SpeechVolumeTypeOverride))
	{
		volType = (audSpeechVolume)g_SpeechVolumeTypeOverride;
	}
#endif

	return volType;
}

audSpeechVolume audSpeechAudioEntity::GetVolTypeForSpeechParamsVolume(const audAmbientSpeechVolume requestedVolume) const
{
	audSpeechVolume volType = AUD_SPEECH_NORMAL;

	switch(requestedVolume)
	{
	case AUD_AMBIENT_SPEECH_VOLUME_UNSPECIFIED:
	case AUD_AMBIENT_SPEECH_VOLUME_NORMAL:
		volType = AUD_SPEECH_NORMAL;
		break;
	case AUD_AMBIENT_SPEECH_VOLUME_FRONTEND:
		volType = AUD_SPEECH_FRONTEND;
		break;
	case AUD_AMBIENT_SPEECH_VOLUME_SHOUTED:
		volType = AUD_SPEECH_SHOUT;
		break;
	case AUD_AMBIENT_SPEECH_VOLUME_MEGAPHONE:
	case AUD_AMBIENT_SPEECH_VOLUME_HELI:
		volType = AUD_SPEECH_MEGAPHONE;
		break;
	default:
		naAssertf(false, "Invalid audAmbientSpeechVolume: %u", requestedVolume);
		break;
	}

	return volType;
}

audSpeechVolume audSpeechAudioEntity::FindVolTypeToUseForAmbientSpeech(const audSpeechVolume contextVolType, const audSpeechVolume paramVolType) const
{
	audSpeechVolume volType = AUD_SPEECH_NORMAL;

	if(contextVolType == AUD_SPEECH_FRONTEND 
		|| paramVolType == AUD_SPEECH_FRONTEND 
		|| (m_Parent && m_Parent->IsLocalPlayer() && m_Parent->GetIsParachuting()))
	{
		volType = AUD_SPEECH_FRONTEND;
	}
	else if(contextVolType == AUD_SPEECH_MEGAPHONE || paramVolType == AUD_SPEECH_MEGAPHONE)
	{
		volType = AUD_SPEECH_MEGAPHONE;
	}
	else if(contextVolType == AUD_SPEECH_SHOUT || paramVolType == AUD_SPEECH_SHOUT)
	{
		volType = AUD_SPEECH_SHOUT;
	}

	return volType;
}

audConversationAudibility audSpeechAudioEntity::GetAudibilityForSpeechContext(const SpeechContext* context) const
{
	audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL;
	if(context)
	{
		audibility = GetAudibilityForSpeechAudibilityType((SpeechAudibilityType)context->Audibility);
	}
	return audibility;
}

audConversationAudibility audSpeechAudioEntity::GetAudibilityForSpeechAudibilityType(const SpeechAudibilityType requestedAudibility) const
{
	audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL;

	switch(requestedAudibility)
	{
	case CONTEXT_AUDIBILITY_NORMAL:
		audibility = AUD_AUDIBILITY_NORMAL;
		break;
	case CONTEXT_AUDIBILITY_CLEAR:
		audibility = AUD_AUDIBILITY_CLEAR;
		break;
	case CONTEXT_AUDIBILITY_CRITICAL:
		audibility = AUD_AUDIBILITY_CRITICAL;
		break;
	case CONTEXT_AUDIBILITY_LEAD_IN:
		audibility = AUD_AUDIBILITY_LEAD_IN;
		break;
	default:
		naAssertf(false, "Invalid SpeechAudibilityType: %u", requestedAudibility);
		break;
	}

#if __BANK
	if(g_ShouldOverrideSpeechAudibility && m_IsDebugEntity 
		&& naVerifyf(g_SpeechAudibilityOverride < AUD_NUM_AUDIBILITIES, "Invalid g_SpeechAudibilityOverride - %u", g_SpeechAudibilityOverride))
	{
		audibility = (audConversationAudibility)g_SpeechAudibilityOverride;
	}
#endif

	return audibility;
}

bool audSpeechAudioEntity::CanPlayPlayerCarHitPedSpeech()
{
	return sm_TimeOfLastPlayerCarHitPedSpeech + g_MinTimeBetweenPlayerCarHitPedSpeech < g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audSpeechAudioEntity::SetPlayerCarHitPedSpeechPlayed()
{
	sm_TimeOfLastPlayerCarHitPedSpeech = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audSpeechAudioEntity::SetCryingContext(const char* context)
{
	safecpy(m_AlternateCryingContext, context);
}

void audSpeechAudioEntity::UnsetCryingContext()
{
	m_AlternateCryingContext[0] = '\0';
}
void audSpeechAudioEntity::StartCrying()
{
	naSpeechEntDebugf3(this, "StartCrying()");

	if(!m_IsCrying && sm_NumPedsCrying < g_MaxPedsCrying)
	{
		m_IsCrying = true;
		sm_NumPedsCrying++;
	}
}

void audSpeechAudioEntity::StopCrying()
{
	naSpeechEntDebugf3(this, "StopCrying()");

	if(m_IsCrying)
	{
		m_IsCrying = false;
		sm_NumPedsCrying--;
	}
}

void audSpeechAudioEntity::TriggerSingleCry()
{
	naSpeechEntDebugf3(this, "TriggerSingleCry()");

	audDamageStats damageStats;
	damageStats.Fatal = false;
	damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
	damageStats.DamageReason = audEngineUtil::ResolveProbability(0.5f) ? AUD_DAMAGE_REASON_WHIMPER : AUD_DAMAGE_REASON_COWER;

	InflictPain(damageStats);
}

void audSpeechAudioEntity::RegisterLungDamage()
{
	naSpeechEntDebugf3(this, "RegisterLungDamage()");

	m_LungDamageStartTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

bool audSpeechAudioEntity::ShouldPlayCoughForDamagedLungs() const
{
	// See how much time has passed since the original damage was applied to the ped
	const u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(currentTime > m_LungDamageNextCoughTime)
	{
		const u32 timeSinceDamageEvent = currentTime - m_LungDamageStartTime;
		const f32 probabilityToPlayCough = sm_TimeSinceDamageToCoughProbability.CalculateValue(static_cast<f32>(timeSinceDamageEvent) * 0.001f);
		if(audEngineUtil::ResolveProbability(probabilityToPlayCough))
		{
			return true;
		}
	}
	return false;
}

s8 audSpeechAudioEntity::GetPedToughnessWithoutSetting() const
{
	if(!m_ToughnessIsSet)
		return -1;

	return m_Toughness;
}

u8 audSpeechAudioEntity::GetPedToughness()
{
	if(!m_ToughnessIsSet)
		ChooseVoiceIfUndecided(0);

	return m_Toughness;
}

u32 audSpeechAudioEntity::GetAngryContextPHashFromAnimTrigger(AnimalVocalAnimTrigger::tAnimTriggers animTrigger)
{
	if(animTrigger.numAngryContexts == 0)
		return 0x7C922D3D; //NULL
	if(animTrigger.numAngryContexts == 1)
		return animTrigger.AngryContexts[0].Context;

	f32 totalWeight = 0.0f;
	for(int j=0; j<animTrigger.numAngryContexts; j++)
	{
		totalWeight += animTrigger.AngryContexts[j].Weight;
	}
	f32 randomNum = audEngineUtil::GetRandomFloat() * totalWeight;
	for(int j=0; j<animTrigger.numAngryContexts-1; j++)
	{
		if(randomNum < animTrigger.AngryContexts[j].Weight)
			return animTrigger.AngryContexts[j].Context;

		randomNum -= animTrigger.AngryContexts[j].Weight;
	}
	return animTrigger.AngryContexts[animTrigger.numAngryContexts-1].Context;
}

u32 audSpeechAudioEntity::GetPlayfulContextPHashFromAnimTrigger(AnimalVocalAnimTrigger::tAnimTriggers animTrigger)
{
	if(animTrigger.numPlayfulContexts == 0)
		return 0x7C922D3D; //NULL
	if(animTrigger.numPlayfulContexts == 1)
		return animTrigger.PlayfulContexts[0].Context;

	f32 totalWeight = 0.0f;
	for(int j=0; j<animTrigger.numPlayfulContexts; j++)
	{
		totalWeight += animTrigger.PlayfulContexts[j].Weight;
	}
	f32 randomNum = audEngineUtil::GetRandomFloat() * totalWeight;
	for(int j=0; j<animTrigger.numPlayfulContexts-1; j++)
	{
		if(randomNum < animTrigger.PlayfulContexts[j].Weight)
			return animTrigger.PlayfulContexts[j].Context;

		randomNum -= animTrigger.PlayfulContexts[j].Weight;
	}
	return animTrigger.PlayfulContexts[animTrigger.numPlayfulContexts-1].Context;
}

void audSpeechAudioEntity::PlayAnimalVocalization(const AnimalVocalAnimTrigger* trigger)
{
	if(m_AnimalParams)
		m_DeferredAnimalVocalAnimTriggers.PushAndGrow(trigger);
}

void audSpeechAudioEntity::ReallyPlayAnimalVocalization(const AnimalVocalAnimTrigger* trigger)
{
	if(g_SpeechManager.GetIsCurrentlyLoadingAnimalBank())
	{
		m_DeferredAnimalVocalAnimTriggers.PushAndGrow(trigger);
		return;
	}

	if(!m_AnimalParams)
	{
		return;
	}

	u32 contextPHash = 0;

	for(int i=0; i<trigger->numAnimTriggers; i++)
	{
		if(trigger->AnimTriggers[i].Animal == m_AnimalParams->Type)
		{
			switch(m_AnimalMood)
			{
			case AUD_ANIMAL_MOOD_ANGRY:
				contextPHash = GetAngryContextPHashFromAnimTrigger(trigger->AnimTriggers[i]);
				break;
			case AUD_ANIMAL_MOOD_PLAYFUL:
			default:
				contextPHash = GetPlayfulContextPHashFromAnimTrigger(trigger->AnimTriggers[i]);
				break;
			}
#if __BANK
			if(g_ForceAnimalMoodAngry)
				contextPHash = GetAngryContextPHashFromAnimTrigger(trigger->AnimTriggers[i]);
			else if(g_ForceAnimalMoodPlayful)
				contextPHash = GetPlayfulContextPHashFromAnimTrigger(trigger->AnimTriggers[i]);
#endif
			break;
		}
	}

	if(contextPHash == 0x7C922D3D) //NULL
		return;

	PlayAnimalVocalization(contextPHash
#if __BANK
		 , audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(trigger->NameTableOffset)
#endif
		);
}

void audSpeechAudioEntity::PlayAnimalVocalization(AnimalType UNUSED_PARAM(animalType),const char* context)
{
	PlayAnimalVocalization(atPartialStringHash(context)
#if __BANK
							, context
#endif
		);
}

void audSpeechAudioEntity::PlayAnimalVocalization(u32 contextPHash
#if __BANK
													, const char* context											  
#endif
												  )
{
	if(m_AnimalType >= NUM_ANIMALTYPE || m_AnimalType == kAnimalNone)
	{
		Errorf("Invalid animal type passed to PlayAnimalVocalization.");
		return;
	}

	if( (m_AnimalType == kAnimalRottweiler || m_AnimalType == kAnimalRetriever || m_AnimalType == kAnimalDog 
		|| m_AnimalType == kAnimalHusky || m_AnimalType == kAnimalShepherd || m_AnimalType == kAnimalSmallDog) && 
		IsAnimalContextABark(contextPHash) && 
		(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisableBarks) || g_ScriptAudioEntity.IsPedInCurrentConversation(m_Parent)) )
	{
		return;
	}

	if(g_SpeechManager.GetIsTennisUsingAnimalBanks())
	{
		return;
	}

	if(IsPainPlaying())
	{
		return;
	}

	bool isPain = contextPHash == g_PainDeathPHash;

	if(!isPain && g_SpeechManager.GetNumAnimalsMakingSound(m_AnimalType) >= m_AnimalParams->MaxSoundInstances)
		return;

	f32 volumeOffset = 0.0f;
	f32 rolloff = 0.0f;
	f32 probability = 1.0f;
	u32 priority = 0;
	bool isOverlappingSound = false;
	GetVoumeAndRolloffForAnimalContext(contextPHash, volumeOffset, rolloff, probability, priority, isOverlappingSound);

	// See if we should stop the current vocalization from playing if it's higher priority, otherwise return out
	if(!isOverlappingSound && (IsAnimalVocalizationPlaying() || IsScriptedSpeechPlaying() || IsAmbientSpeechPlaying()))
	{
		if(isPain || priority > m_AnimalSoundPriority)
		{
			m_PlaySpeechWhenFinishedPreparing = false;		
			FreeSlot();
		}
		else
		{
			return;
		}
	}

	f32 distanceSqToNearestListener = (g_AudioEngine.GetEnvironment().ComputeSqdDistanceRelativeToVolumeListenerV(m_Parent->GetTransform().GetPosition())).Getf();

	if(distanceSqToNearestListener > GetMaxDistanceSqForAnimalContextPHash(contextPHash))
		return;

	if(isPain)
	{
		if( Powf(m_AnimalParams->MinFarDistance, 2.0f) > distanceSqToNearestListener)
			PlayAnimalNearVocalization(contextPHash BANK_ONLY(, context));
		else
			PlayAnimalFarVocalization(contextPHash
#if __BANK
										, context
#endif
			);
	}
	else
	{
		u32 numVars = audSpeechSound::FindNumVariationsForVoiceAndContext(atStringHash(g_AnimalBankNames[m_AnimalType]), contextPHash);
		if(numVars == 0)
		{
			if( Powf(m_AnimalParams->MaxDistanceToBankLoad, 2.0f) < distanceSqToNearestListener)
			{
//#if __BANK
				//Warningf("Audio: Attempting to play animal vocalization from too far away.  Animal: %s Context: %s", g_AnimalBankNames[m_AnimalType], context);
//#endif
				return;
			}
			PlayAnimalNearVocalization(contextPHash BANK_ONLY(, context));
		}
		else
			PlayAnimalFarVocalization(contextPHash);
	}
}

void audSpeechAudioEntity::GetVoumeAndRolloffForAnimalContext(u32 contextPHash, f32& volumeOffset, f32& rolloff, f32& probability, u32& priority, bool& isOverlapping)
{
	if(!m_AnimalParams)
		return;

	for(int i=0; i<m_AnimalParams->numContexts; ++i)
	{
		if(atPartialStringHash(m_AnimalParams->Context[i].ContextName) == contextPHash)
		{
			volumeOffset = m_AnimalParams->Context[i].VolumeOffset;
			rolloff = m_AnimalParams->Context[i].RollOff;
			probability = m_AnimalParams->Context[i].Probability;
			priority = m_AnimalParams->Context[i].Priority;
			isOverlapping = m_AnimalParams->Context[i].ContextBits.BitFields.AllowOverlap;
			return;
		}
	}
}


void audSpeechAudioEntity::PlayAnimalNearVocalization(u32 contextPHash BANK_ONLY(, const char* contextName))
{
	m_PositionForNullSpeaker.Set(ORIGIN);
	m_EntityForNullSpeaker = NULL;
	if(!m_AnimalVocalSound)
		m_PlaySpeechWhenFinishedPreparing = false;

	if(m_AnimalType >= NUM_ANIMALTYPE || m_AnimalType == kAnimalNone)
	{
		Errorf("Invalid animal type passed to PlayAnimalNearVocalization.");
		return;
	}

	if(!m_Parent)
	{
		naAssertf(false, "speechaudioentity has no parent (Cped) assigned during PlayAnimalNearVocalization, aborting");
		return;
	}

	audWaveSlot* waveSlot = NULL;
	u32 voiceName = 0;
	g_SpeechManager.GetWaveSlotAndVoiceNameForAnimal(m_AnimalType, waveSlot, voiceName);

	if(!waveSlot || voiceName==0)
		return;

	f32 volumeOffset = 0.0f;
	f32 rolloff = 0.0f;
	f32 probability = 1.0f;	
	u32 priority = 0;
	bool isOverlapping = false;
	GetVoumeAndRolloffForAnimalContext(contextPHash, volumeOffset, rolloff, probability, priority, isOverlapping);

	if(!audEngineUtil::ResolveProbability(probability))
		return;

	s32 variation = g_SpeechManager.GetVariationForNearAnimalContext(m_AnimalType, contextPHash);
	if(variation <= 0)
		return;

	const bool isPain = contextPHash == g_PainDeathPHash;

	if(m_AnimalType == kAnimalDeer && EXTRACONTENT.GetSpecialTrigger(CExtraContentManager::ST_XMAS))
	{
		if(isPain)
		{
			if(Say("FUCK_OFF", "SPEECH_PARAMS_FORCE_SHOUTED", ATSTRINGHASH("reindeer", 0x6288E279)) == AUD_SPEECH_SAY_SUCCEEDED)
				return;
		}
	}

	audSound** snd = isPain ? (audSound**)&m_PainSpeechSound : (audSound**)&m_AnimalVocalSound;
	audSoundSet* soundSet = isPain ? &g_AnimalPainSoundSet : &g_AnimalNearSoundSet;

	if(m_AnimalType == kAnimalWhale || m_AnimalType == kAnimalDolphin || m_AnimalType == kAnimalOrca)
	{
		if(!Water::IsCameraUnderwater())
			return;
		soundSet = isPain ? &g_WhalePainSoundSet : &g_WhaleNearSoundSet;
	}

	const audMetadataRef soundRef = soundSet->Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));
	naAssertf(soundRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");

	if(isOverlapping)
	{
		snd = NULL;
		for(int i=0; i<AUD_MAX_ANIMAL_OVERLAPPABLE_SOUNDS; ++i)
		{
			if(!m_AnimalOverlappableSound[i])
			{
				snd = (audSound**)&m_AnimalOverlappableSound[i];
				break;
			}
		}
		if(!snd)
			return;
	}
	else if(snd && (*snd))
	{
		return;
	}

	const s32 predelay = isPain ? g_AnimalPainPredelay : 0;

	audSoundInitParams initParam;
	initParam.WaveSlot = waveSlot;
	initParam.AllowLoad = false;
	initParam.AllowOrphaned = true;
	initParam.Predelay = predelay;
	initParam.UpdateEntity = true;
	initParam.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
	initParam.RemoveHierarchy = false;
	naCErrorf(m_Parent->GetPlaceableTracker(), "Unable to get tracker from CPed parent in PlayPain");

	CreateSound_PersistentReference(soundRef, snd, &initParam);

	if (!snd || !*snd)
	{
		return;
	}

	if(!(((audSpeechSound*)(*snd))->InitSpeech(voiceName, contextPHash, variation)))
	{
		naErrorf("Unable to initialise speech in PlayAnimalNearVocalization");
		(*snd)->StopAndForget();
		return;
	}

	const audSpeechType speechType = isPain? AUD_SPEECH_TYPE_ANIMAL_PAIN : AUD_SPEECH_TYPE_ANIMAL_NEAR;
	const audSpeechVolume volType = AUD_SPEECH_NORMAL;
	const audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL;

	const bool inCarAndSceneSaysFrontend = IsInCarFrontendScene();
	const bool useShadowSounds = !isOverlapping && inCarAndSceneSaysFrontend;

	if(!useShadowSounds)
	{
		// Get the position to play positioned speech, and abort if we can't find a place to play it
		Vector3 position;
		if(!GetPositionForSpeechSound(position, volType))
		{
			naErrorf("Unable to get position for speech in PlayAnimalNearVocalization");
			(*snd)->StopAndForget();
			return;
		}

		// As we're about to play new speech, reset the smoothers (I hope this is what's causing a slight ramping on the occasional line)
		ResetSpeechSmoothers();

		audEchoMetrics echo1, echo2;
		audSpeechMetrics metrics;
		const bool metricsProcessed = GetSpeechAudibilityMetrics(VECTOR3_TO_VEC3V(position), speechType, volType, inCarAndSceneSaysFrontend, audibility, metrics, echo1, echo2);
		if(!metricsProcessed)
		{
			naErrorf("Unable to GetSpeechAudibilityMetrics in PlayAnimalNearVocalization");
			(*snd)->StopAndForget();
			return;
		}

		const f32 postSubmixVol = ComputePostSubmixVolume(speechType, volType, inCarAndSceneSaysFrontend, volumeOffset);

		(*snd)->SetRequestedHPFCutoff(metrics.hpf);
		(*snd)->SetRequestedLPFCutoff(metrics.lpf);
		(*snd)->SetRequestedVolume(metrics.vol[AUD_SPEECH_ROUTE_POS]);
		(*snd)->SetRequestedPosition(position);
		(*snd)->SetRequestedPostSubmixVolumeAttenuation(postSubmixVol);
		(*snd)->SetRequestedVolumeCurveScale(rolloff);

#if __BANK
		if(g_ShowSpeechPositions)
		{
			CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
			if(pPlayer)
			{
				const Vector3 &playerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
				CVectorMap::DrawLine(playerPosition, position, Color_green, Color_green); 
			}
		}

		if(g_ListPlayedAnimalContexts)
		{
			f32 distanceToAnimal = 0.0f;
			if(m_Parent)
			{
				distanceToAnimal = Mag(g_AudioEngine.GetEnvironment().GetPanningListenerPosition() - m_Parent->GetTransform().GetPosition()).Getf();
			}
			naWarningf("***Non-Preloaded Animal Vocal - Type: %s Context: %s volType: %s aud: %s dist: %f; volPos: %f; lpf: %f; hpf: %f; postSubmix: %f; rolloff: %f;", 
				g_AnimalBankNames[m_AnimalType], contextName, g_SpeechVolNames[volType], g_ConvAudNames[audibility],
				distanceToAnimal, metrics.vol[AUD_SPEECH_ROUTE_POS], metrics.lpf, metrics.hpf, postSubmixVol, rolloff);
		}
#endif
	}
	
	m_VisemeAnimationData = NULL;

	if(useShadowSounds)
	{
		(*snd)->Prepare(waveSlot);
		m_PlaySpeechWhenFinishedPreparing = true;
	}
	else
	{
		(*snd)->PrepareAndPlay(waveSlot);
	}

	g_SpeechManager.NearAnimalContextPlayed(m_AnimalType, contextPHash);

	if(!m_WasMakingAnimalSoundLastFrame)
		g_SpeechManager.IncrementAnimalsMakingSound(m_AnimalType);
	m_AnimalSoundPriority = priority;

	m_LastPlayedWaveSlot = waveSlot;
	m_CurrentlyPlayingVoiceNameHash = voiceName;
	m_LastPlayedSpeechContextPHash = contextPHash;
	m_LastPlayedSpeechVariationNumber = variation;
	m_CurrentlyPlayingSoundSet = *soundSet;
	m_CachedSpeechType = speechType;
	m_CachedVolType = volType;
	m_CachedAudibility = audibility;
	m_CachedRolloff = rolloff;
	m_SpeechVolumeOffset = volumeOffset;
	m_IsFromScript = false;
	m_UseShadowSounds = useShadowSounds;
	m_IsMutedDuringSlowMo = true;
	m_IsPitchedDuringSlowMo = true;

	BANK_ONLY(safecpy(m_CurrentlyPlayingSpeechContext, contextName);)

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketAnimalVocalization>(
				CPacketAnimalVocalization(m_LastPlayedSpeechContextPHash), m_Parent);
		}
#endif // GTA_REPLAY
}

void audSpeechAudioEntity::PlayAnimalFarVocalization(u32 contextPHash
#if __BANK
													 , const char* context
#endif
													 )
{
	m_PositionForNullSpeaker.Set(ORIGIN);
	m_EntityForNullSpeaker = NULL;
	m_PlaySpeechWhenFinishedPreparing = false;

	if(m_AnimalType >= NUM_ANIMALTYPE || m_AnimalType == kAnimalNone)
	{
		Errorf("Invalid animal type passed to PlayAnimalFarVocalization.");
		return;
	}

	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	
	if(!g_SpeechManager.CanFarAnimalContextPlayed(contextPHash, currentTime))
		return;

	if(!Verifyf(m_Parent, "speechaudioentity has no parent (CPed) assigned in PlayAnimalFarVocalization function"))
		return;
	
	u32 voiceNameHash = atStringHash(g_AnimalBankNames[m_AnimalType]);

	if (m_Parent->ShouldBeDead())
	{
		if(m_PedHasDied)
			return;
		m_PedHasDied = true;
	}
	
	const bool isPain = contextPHash == g_PainDeathPHash;

	if(m_AnimalType == kAnimalDeer && EXTRACONTENT.GetSpecialTrigger(CExtraContentManager::ST_XMAS))
	{
		if(isPain)
		{
			if(Say("FUCK_OFF", "SPEECH_PARAMS_FORCE_SHOUTED", ATSTRINGHASH("reindeer", 0x6288E279)) == AUD_SPEECH_SAY_SUCCEEDED)
				return;
		}
	}

	//Pick a random variation. Do this via our proper managed non-repeating code, then register the one we've use.
	u32 timeContextLastPlayedForThisVoice = 0; // ped would be better, but never mind
	bool hasBeenPlayedRecently = false;
	s32 variationNum = -1;

	variationNum = GetRandomVariation(voiceNameHash, contextPHash, &timeContextLastPlayedForThisVoice, &hasBeenPlayedRecently);//, m_VoiceName);
	
	if (variationNum < 0)
	{
		return;
	}

	f32 volumeOffset = 0.0f;
	f32 rolloff = 0.0f;
	f32 probability = 1.0f;
	u32 priority = 0;
	bool isOverlapping = false;
	GetVoumeAndRolloffForAnimalContext(contextPHash, volumeOffset, rolloff, probability, priority, isOverlapping);

	naAssertf(!isOverlapping, "Waveloading animal sound marked as overlapping.  AnimalType %u contextPHash %u", (u32)m_AnimalType, contextPHash);

	if(!audEngineUtil::ResolveProbability(probability))
		return;

	if(m_AmbientSlotId >= 0)
		FreeSlot();

	bool hadToStopSpeech = false;
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread() ? CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() : THREAD_INVALID;
	s32 slot = g_SpeechManager.GetAmbientSpeechSlot(this, &hadToStopSpeech, -1.0f, scriptThreadId
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
		, context
#endif
		);
	if (slot<0)
	{
		return;
	}

	// We've got a slot, so create our speech sound, and get variation info
	// if we've got a parent, go get the occlusion group off of its pedAudioEntity
	m_AmbientSlotId = slot;
	g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(slot, 0, 
#if __BANK
		"ANIMAL_VOCAL",
#endif
		0);

	audSound** snd = isPain ? (audSound**)&m_PainSpeechSound : (audSound**)&m_AnimalVocalSound;
	audSpeechSound** sndEcho = isPain ? &m_PainEchoSound : &m_AnimalVocalEchoSound;
	audSoundSet* soundSet = isPain ? &g_AnimalPainSoundSet : &g_AnimalFarSoundSet;

	if(m_AnimalType == kAnimalWhale || m_AnimalType == kAnimalDolphin || m_AnimalType == kAnimalOrca)
	{
		if(!Water::IsCameraUnderwater())
			return;
		soundSet = isPain ? &g_WhalePainSoundSet : &g_WhaleFarSoundSet;
	}

	const audMetadataRef soundRef = soundSet->Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));
	const audMetadataRef soundEchoRef = soundSet->Find(ATSTRINGHASH("SpeechEchoSound", 0x4bec8264));

	naAssertf(soundRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");
	naAssertf(soundEchoRef.IsValid(), "Invalid 'SpeechEchoSound' audMetadataRef for speech audSoundSet");

	const s32 predelay = isPain ? g_AnimalPainPredelay : 0;

	// Set up our speech sound and init params
	audSoundInitParams initParam;
	initParam.RemoveHierarchy = false;
	initParam.AllowOrphaned = true;
	initParam.Predelay = predelay;
	initParam.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
	initParam.UpdateEntity = true;

	CreateSound_PersistentReference(soundRef, (audSound**)snd, &initParam);
	if (!(*snd))
	{
		FreeSlot();
		return;
	}

	if((*snd)->GetSoundTypeID() != SpeechSound::TYPE_ID)
	{
		naErrorf("In 'PlayAnimalFarVocalization(...)', have successfully created a sound reference but it's not a speech sound");
		(*snd)->StopAndForget();
		FreeSlot();
		return;
	}

	if(!((audSpeechSound*)(*snd))->InitSpeech(voiceNameHash, contextPHash, variationNum))
	{
		//		Assert(0);
		(*snd)->StopAndForget();
		FreeSlot();
		return;
	}

	// Get all the volumes, post submix volumes, and filters
	const audSpeechType speechType = isPain ? AUD_SPEECH_TYPE_ANIMAL_PAIN : AUD_SPEECH_TYPE_ANIMAL_FAR;
	const audSpeechVolume volType = AUD_SPEECH_NORMAL;
	const audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL;

	// Get the position to play positioned speech, and abort if we can't find a place to play it
	Vector3 position;
	if(!GetPositionForSpeechSound(position, volType))
	{
		naErrorf("Unable to get position for speech in PlayAnimalFarVocalization");
		(*snd)->StopAndForget();
		return;
	}

	// As we're about to play new speech, reset the smoothers (I hope this is what's causing a slight ramping on the occasional line)
	ResetSpeechSmoothers();

	// Get the initial echo data we need to process their volumes and filters in GetSpeechAudibilityMetrics
	// We're not actually audSpeechVolume AUD_SPEECH_SHOUT, but use that for the echo metrics
	audEchoMetrics echo1, echo2;
	ComputeEchoInitMetrics(position, AUD_SPEECH_SHOUT, sndEcho != NULL, false, echo1, echo2);

	audSpeechMetrics metrics;
	const bool metricsProcessed = GetSpeechAudibilityMetrics(VECTOR3_TO_VEC3V(position), speechType, volType, false, audibility, metrics, echo1, echo2);
	if(!metricsProcessed)
	{
		naErrorf("In 'PlayAnimalFarVocalization(...)', unable to GetSpeechAudibilityMetrics");
		(*snd)->StopAndForget();
		FreeSlot();
		return;
	}

	const f32 postSubmixVol = ComputePostSubmixVolume(speechType, volType, false, volumeOffset);

	(*snd)->SetRequestedHPFCutoff(metrics.hpf);
	(*snd)->SetRequestedLPFCutoff(metrics.lpf);
	(*snd)->SetRequestedVolume(metrics.vol[AUD_SPEECH_ROUTE_POS]);
	(*snd)->SetRequestedPosition(position);
	(*snd)->SetRequestedPostSubmixVolumeAttenuation(postSubmixVol);
	(*snd)->SetRequestedVolumeCurveScale(rolloff);

#if __BANK
	if(g_ShowSpeechPositions)
	{
		CPed* pPlayer = CGameWorld::GetMainPlayerInfo()->GetPlayerPed();
		if(pPlayer)
		{
			const Vector3 &playerPosition = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
			CVectorMap::DrawLine(playerPosition, position, Color_red, Color_red); 
		}
	}
#endif


	if(echo1.useEcho)
	{
		CreateEcho(echo1, speechType, AUD_SPEECH_SHOUT, postSubmixVol, predelay, rolloff, voiceNameHash, contextPHash, variationNum, soundEchoRef, sndEcho, initParam);
	}

	audWaveSlot* waveSlot = g_SpeechManager.GetAmbientWaveSlotFromId(m_AmbientSlotId);
	m_VisemeAnimationData = NULL;

	(*snd)->PrepareAndPlay(waveSlot, true, -1, naWaveLoadPriority::Speech);
	if ((*sndEcho))
	{
		(*sndEcho)->PrepareAndPlay(waveSlot, true, -1, naWaveLoadPriority::Speech);
	}

	g_SpeechManager.FarAnimalContextPlayed(contextPHash, currentTime);
	if(!m_WasMakingAnimalSoundLastFrame)
		g_SpeechManager.IncrementAnimalsMakingSound(m_AnimalType);
	m_AnimalSoundPriority = priority;

	// We can actually play the speech, so update everything
	naCErrorf(variationNum>=0, "In 'PlayAnimalFarVocalization(...)' invalid variationNum when about to update sound we just played");
#if __BANK
	// Since voices are being reused, use a ped identifier rather than voice name for history
	const u32 pedIdentifier = m_Parent ? static_cast<u32>(CPed::GetPool()->GetIndex(m_Parent)) : 0U;
	g_SpeechManager.AddVariationToHistory( (m_UsingFakeFullPVG || voiceNameHash==g_SilentVoiceHash) ? pedIdentifier : voiceNameHash, contextPHash, (u32)variationNum, currentTime);
#else
	g_SpeechManager.AddVariationToHistory(voiceNameHash, contextPHash, (u32)variationNum, currentTime);
#endif

	g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(m_AmbientSlotId, voiceNameHash, 
#if __BANK
		context, 
#endif
		variationNum);

	// Store what we're currently playing
	m_LastPlayedWaveSlot = waveSlot;
	m_LastPlayedSpeechContextPHash = contextPHash;
	m_LastPlayedSpeechVariationNumber = variationNum;
	m_CurrentlyPlayingVoiceNameHash = voiceNameHash;
	m_CachedSpeechType = speechType;
	m_CachedVolType = volType;
	m_CachedAudibility = audibility;
	m_SpeechVolumeOffset = 0.0f;
	m_IsFromScript = false;
	m_UseShadowSounds = false;
	m_IsMutedDuringSlowMo = true;
	m_IsPitchedDuringSlowMo = true;

#if __BANK
	if (g_ListPlayedAnimalContexts)
	{
		f32 distanceToPed = 0.0f;
		if(m_Parent)
		{
			distanceToPed = Mag(g_AudioEngine.GetEnvironment().GetPanningListenerPosition() - m_Parent->GetTransform().GetPosition()).Getf();
		}
		naWarningf("***AUD_SPEECH_TYPE_ANIMAL_FAR Speech: %s; time: %u; dist: %f; volOffset: %f; rolloff: %f; lpf: %f; hpf: %f; volPos: %f; volFE: %f;", 
			context, fwTimer::GetTimeInMilliseconds(), distanceToPed, volumeOffset, rolloff, metrics.lpf, metrics.hpf, metrics.vol[AUD_SPEECH_ROUTE_POS], metrics.vol[AUD_SPEECH_ROUTE_FE]);
	}
#endif
#if __DEV
	formatf(m_LastPlayedWaveName, 64, "%s_%02d", context, variationNum);
#endif

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketAnimalVocalization>(
			CPacketAnimalVocalization(m_LastPlayedSpeechContextPHash), m_Parent);
	}
#endif // GTA_REPLAY
}

f32 audSpeechAudioEntity::GetMaxDistanceSqForAnimalContextPHash(u32 contextPHash)
{
	if(m_AnimalParams)
	{
		for(int i=0; i<m_AnimalParams->numContexts; i++)
		{
			if(atPartialStringHash(m_AnimalParams->Context[i].ContextName) == contextPHash)
				return m_AnimalParams->Context[i].MaxDistance * m_AnimalParams->Context[i].MaxDistance;
		}
	}

	return 1600.0f;
}

void audSpeechAudioEntity::UpdatePanting(u32 timeOnAudioTimer0)
{
	static const AnimalVocalAnimTrigger* pantTransitionTrigger = audNorthAudioEngine::GetObject<AnimalVocalAnimTrigger>("ROTTWEILER_PANT_TRANSITION");

	if(g_DisablePanting)
		return;

	if(!m_Parent || !m_Parent->GetMotionData() || !m_Parent->GetPedAudioEntity())
		return;

	CPedMotionData* motionData = m_Parent->GetMotionData();

	//Don't play player breathing if he is riding.  Also, keep track of last time riding so we don't get a big breath if the player
	//		leaps from a sprinting horse.
	if(m_Parent->GetIsInVehicle())
		m_LastTimeInVehicle = timeOnAudioTimer0;

	if (timeOnAudioTimer0-m_LastTimeInVehicle > 500)
	{
		bool isAtLeastRunning = !motionData->GetIsLessThanRun() && m_Parent->GetVelocity().Mag2() > 1.0f;

		switch((int)m_PantState)
		{
		case kPantIdle:
			if(isAtLeastRunning)
			{
				m_PantState = kPantStarting;

				u32 uTimeTileNextStateChange = audEngineUtil::GetRandomNumberInRange(5000,8000);

				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTileNextStateChange;
			}
			break;
		case kPantStarting:
			if(!isAtLeastRunning)
			{
				m_PantState = kPantIdle;
			}
			else
			{
				if(timeOnAudioTimer0 >= m_PantStateChangeTime)
				{
					m_PantState = kPantLow;
				}
			}
			break;
		case kPantLow:
			if(m_BreathSound == NULL)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
				CreateSound_PersistentReference("CHOP_PANTING_SLOW_MASTER", &m_BreathSound, &initParams);

				if(m_BreathSound)
					m_BreathSound->PrepareAndPlay();
				m_BreathingVolumeLin = 1.0f;

				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(12000,20000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;
			}
			if(!isAtLeastRunning)
			{
				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(2000,4000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;

				if(m_BreathSound)
					m_BreathSound->StopAndForget();
				PlayAnimalVocalization(pantTransitionTrigger);

				m_PantState = kPantLowStopping;
			}
			if(m_PantStateChangeTime < timeOnAudioTimer0)
			{
				if(m_BreathSound)
					m_BreathSound->StopAndForget();
				PlayAnimalVocalization(pantTransitionTrigger);
				m_PantState = kPantMed;
			}
			break;
		case kPantMed:
			if(m_BreathSound == NULL)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
				CreateSound_PersistentReference("CHOP_PANTING_MEDIUM_MASTER", &m_BreathSound, &initParams);
				if(m_BreathSound)
					m_BreathSound->PrepareAndPlay();
				m_BreathingVolumeLin = 1.0f;

				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(12000,20000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;
			}
			if(!isAtLeastRunning)
			{
				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(3000,6000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;
	
				if(m_BreathSound)
					m_BreathSound->StopAndForget();
				PlayAnimalVocalization(pantTransitionTrigger);
				m_PantState = kPantMedStopping;
			}
			if(m_PantStateChangeTime < timeOnAudioTimer0)
			{
				if(m_BreathSound)
					m_BreathSound->StopAndForget();
				PlayAnimalVocalization(pantTransitionTrigger);
				m_PantState = kPantHigh;
			}
			break;
		case kPantHigh:
			if(m_BreathSound == NULL)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
				CreateSound_PersistentReference("CHOP_PANTING_HIGH_MASTER", &m_BreathSound, &initParams);
				if(m_BreathSound)
					m_BreathSound->PrepareAndPlay();
				m_BreathingVolumeLin = 1.0f;
			}
			if(!isAtLeastRunning)
			{
				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(8000,20000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;

				if(m_BreathSound)
					m_BreathSound->StopAndForget();
				PlayAnimalVocalization(pantTransitionTrigger);
				m_PantState = kPantHighStopping;
			}
			break;
		case kPantLowStopping:
			if(m_BreathSound == NULL)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
				CreateSound_PersistentReference("CHOP_PANTING_SLOW_MASTER", &m_BreathSound, &initParams);
				if(m_BreathSound)
					m_BreathSound->PrepareAndPlay();
				m_BreathingVolumeLin = 1.0f;
			}
			if(isAtLeastRunning)
			{
				m_PantState = kPantLow;
				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(7000, 12000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;
			}
			else if(timeOnAudioTimer0 >= m_PantStateChangeTime)
			{
				if(m_BreathSound)
				{
					m_BreathSound->StopAndForget();
					PlayAnimalVocalization(pantTransitionTrigger);
				}
				m_PantState = kPantIdle;
			}
			break;
		case kPantMedStopping:
			if(m_BreathSound == NULL)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
				CreateSound_PersistentReference("CHOP_PANTING_MEDIUM_MASTER", &m_BreathSound, &initParams);
				if(m_BreathSound)
					m_BreathSound->PrepareAndPlay();
				m_BreathingVolumeLin = 1.0f;
			}
			if(isAtLeastRunning)
			{
				m_PantState = kPantMed;
				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(7000, 10000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;
			}
			else if(timeOnAudioTimer0 >= m_PantStateChangeTime)
			{
				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(3000, 6000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;
				if(m_BreathSound)
				{
					m_BreathSound->StopAndForget();
					PlayAnimalVocalization(pantTransitionTrigger);
				}
				m_PantState = kPantLowStopping;
			}
			break;
		case kPantHighStopping:
			if(m_BreathSound == NULL)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
				CreateSound_PersistentReference("CHOP_PANTING_HIGH_MASTER", &m_BreathSound, &initParams);
				if(m_BreathSound)
					m_BreathSound->PrepareAndPlay();
				m_BreathingVolumeLin = 1.0f;
			}
			if(isAtLeastRunning)
			{
				m_PantState = kPantHigh;
			}
			else if(timeOnAudioTimer0 >= m_PantStateChangeTime)
			{
				u32 uTimeTilNextStateChange = audEngineUtil::GetRandomNumberInRange(3000, 6000);
				m_PantStateChangeTime = timeOnAudioTimer0 + uTimeTilNextStateChange;
				if(m_BreathSound)
				{
					m_BreathSound->StopAndForget();
					PlayAnimalVocalization(pantTransitionTrigger);
				}
				m_PantState = kPantMedStopping;
			}
			break;
		}
	}
}

bool audSpeechAudioEntity::IsAnimalContextABark(u32 contextPHash)
{
	return contextPHash == 0x70B058CB || //BARK_FAR
		contextPHash == 0xB0EF9479|| //BARK
		contextPHash == 0xE2FB060A; //BARK_SEQ
}

void audSpeechAudioEntity::PlaySharkVictimPain()
{
	m_PlayingSharkPain = true;

	SayWhenSafe("SHARK_SCREAM", "SPEECH_PARAMS_FORCE");
}

void audSpeechAudioEntity::TriggerSharkAttackSound(CPed* target, bool secondCall)
{
	FreeSlot();

	if(!secondCall && target && target->GetSpeechAudioEntity())
	{
		target->GetSpeechAudioEntity()->StopSpeech();
		target->GetSpeechAudioEntity()->PlaySharkVictimPain();
	}

	bool hadToStopSpeech = false;
	scrThreadId scriptThreadId = CTheScripts::GetCurrentGtaScriptThread() ? CTheScripts::GetCurrentGtaScriptThread()->GetThreadId() : THREAD_INVALID;
	s32 slot = g_SpeechManager.GetAmbientSpeechSlot(this, &hadToStopSpeech, -1.0f, scriptThreadId
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
		, "SHARK_ATTACK"
#endif
		);
	if (slot<0)
	{
		return;
	}

	// We've got a slot, so create our speech sound, and get variation info
	// if we've got a parent, go get the occlusion group off of its pedAudioEntity
	m_AmbientSlotId = slot;
	g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(slot, 0, 
#if __BANK
		secondCall ? "SHARK_AWAY" : "SHARK_ATTACK",
#endif
		0);

	audSoundSet* soundSet = &g_AnimalFarSoundSet;

	const audMetadataRef soundRef = soundSet->Find(ATSTRINGHASH("SpeechSound", 0x75e4276d));

	naAssertf(soundRef.IsValid(), "Invalid 'SpeechSound' audMetadataRef for speech audSoundSet");

	// Set up our speech sound and init params
	audSoundInitParams initParam;
	initParam.RemoveHierarchy = false;
	initParam.AllowOrphaned = true;
	initParam.Predelay = secondCall ? 500 : 0;
	initParam.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
	initParam.UpdateEntity = true;

	CreateSound_PersistentReference(soundRef, (audSound**)&m_AnimalVocalSound, &initParam);
	if (!m_AnimalVocalSound)
	{
		FreeSlot();
		return;
	}

	if(m_AnimalVocalSound->GetSoundTypeID() != SpeechSound::TYPE_ID)
	{
		naErrorf("In 'PlayAnimalFarVocalization(...)', have successfully created a sound reference but it's not a speech sound");
		m_AnimalVocalSound->StopAndForget();
		FreeSlot();
		return;
	}

	if(!((audSpeechSound*)m_AnimalVocalSound)->InitSpeech(atStringHash("SHARK"), secondCall ? g_SharkAwayContextPHash : g_SharkAttackContextPHash, 1))
	{
		//		Assert(0);
		m_AnimalVocalSound->StopAndForget();
		FreeSlot();
		return;
	}

	// Get all the volumes, post submix volumes, and filters
	const audSpeechType speechType =  AUD_SPEECH_TYPE_ANIMAL_NEAR;
	const audSpeechVolume volType = AUD_SPEECH_FRONTEND;
	const audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL;
	m_CurrentlyPlayingSoundSet = g_AmbientSoundSet;

	// Get the position to play positioned speech, and abort if we can't find a place to play it
	Vector3 position;
	if(!GetPositionForSpeechSound(position, volType))
	{
		naErrorf("Unable to get position for speech in PlayAnimalFarVocalization");
		m_AnimalVocalSound->StopAndForget();
		return;
	}

	// As we're about to play new speech, reset the smoothers (I hope this is what's causing a slight ramping on the occasional line)
	ResetSpeechSmoothers();

	audEchoMetrics echo1, echo2;
	audSpeechMetrics metrics;
	const bool metricsProcessed = GetSpeechAudibilityMetrics(VECTOR3_TO_VEC3V(position), speechType, volType, false, audibility, metrics, echo1, echo2);
	if(!metricsProcessed)
	{
		naErrorf("In 'PlayAnimalFarVocalization(...)', unable to GetSpeechAudibilityMetrics");
		m_AnimalVocalSound->StopAndForget();
		FreeSlot();
		return;
	}

	//const f32 postSubmixVol = ComputePostSubmixVolume(speechType, volType, false, 0.0f);

	m_AnimalVocalSound->SetRequestedHPFCutoff(metrics.hpf);
	m_AnimalVocalSound->SetRequestedLPFCutoff(metrics.lpf);
	m_AnimalVocalSound->SetRequestedVolume(metrics.vol[AUD_SPEECH_ROUTE_POS]);
	m_AnimalVocalSound->SetRequestedPosition(position);
	m_AnimalVocalSound->SetRequestedPostSubmixVolumeAttenuation(0.0f);

	audWaveSlot* waveSlot = g_SpeechManager.GetAmbientWaveSlotFromId(m_AmbientSlotId);
	m_VisemeAnimationData = NULL;

	m_AnimalVocalSound->Prepare(waveSlot, true);

	if(!m_WasMakingAnimalSoundLastFrame)
		g_SpeechManager.IncrementAnimalsMakingSound(m_AnimalType);

	// Store what we're currently playing
	m_LastPlayedWaveSlot = waveSlot;
	m_LastPlayedSpeechContextPHash = secondCall ? g_SharkAwayContextPHash : g_SharkAttackContextPHash;
	m_LastPlayedSpeechVariationNumber = 1;

	m_CachedSpeechType = speechType;
	m_CachedVolType = volType;
	m_CachedAudibility = audibility;
	m_SpeechVolumeOffset = 0.0f;
	m_IsFromScript = false;
	m_UseShadowSounds = true;
	m_PlaySpeechWhenFinishedPreparing = true;
	m_IsMutedDuringSlowMo = true;
	m_IsPitchedDuringSlowMo = true;

#if __DEV
	formatf(m_LastPlayedWaveName, 64, secondCall ? "SHARK_AWAY_01" : "SHARK_ATTACK_01");
#endif
}

void audSpeechAudioEntity::PlayTennisVocalization(TennisVocalEffectType tennisVFXType)
{
	if(GetIsAnySpeechPlaying() || m_BreathSound || !m_Parent || m_Parent->IsDead())
	{
		naWarningf("Failed to play tennis vocalization.  Either speech is already playing or the ped is dead.");
		return;
	}

	u32 contextPhash = 0;
	u32 voiceHash = 0;
	u32 variation = 0;
	audWaveSlot* waveSlot = g_SpeechManager.GetTennisVocalizationInfo(m_Parent, tennisVFXType, contextPhash, voiceHash, variation);

	if(contextPhash == 0 || voiceHash == 0 || variation == 0 || !waveSlot)
	{
		naWarningf("Failed to play tennis vocalization. Failed to get proper vocalization info.");
		return;
	}

	audSoundInitParams initParam;
	initParam.WaveSlot = waveSlot;
	initParam.AllowLoad = false;
	initParam.AllowOrphaned = true;

	// Are we in the same vehicle as the player? If so, play speech frontend
	naAssertf(m_Parent, "speechaudioentity has no parent (Cped) assigned during PlayPain, will access null ptr");
	naCErrorf(m_Parent->GetPlaceableTracker(), "Unable to get tracker from CPed parent in PlayPain");
	initParam.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
	initParam.RemoveHierarchy = false;

	CreateSound_PersistentReference(g_TennisVocalSoundHash, (audSound**)&m_BreathSound, &initParam);

	if (!m_BreathSound)
	{
		return;
	}

	if(!(((audSpeechSound*)m_BreathSound)->InitSpeech(voiceHash, contextPhash, variation)))
	{
		naErrorf("Unable to initialise speech in PlayTennisVocalization");
		m_BreathSound->StopAndForget();
		return;
	}

	// Get the position to play positioned speech, and abort if we can't find a place to play it
	Vec3V position(V_ZERO);
	const bool success = UpdatePedHeadMatrix();
	if(!success)
	{
		return;	
	}
	position = VECTOR3_TO_VEC3V(m_PedHeadMatrix.d);

	// As we're about to play new speech, reset the smoothers (I hope this is what's causing a slight ramping on the occasional line)
	ResetSpeechSmoothers();

	// Get all the volumes, post submix volumes, and filters
	const audSpeechType speechType = AUD_SPEECH_TYPE_BREATHING;
	const audSpeechVolume volType = AUD_SPEECH_NORMAL;
	const audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL;

	audEchoMetrics echo1, echo2;
	audSpeechMetrics metrics;
	const bool metricsProcessed = GetSpeechAudibilityMetrics(position, speechType, volType, false, audibility, metrics, echo1, echo2);
	if(!metricsProcessed)
	{
		naErrorf("Unable to GetSpeechAudibilityMetrics in PlayTennisVocalization");
		m_BreathSound->StopAndForget();
		return;
	}

	const f32 postSubmixVol = ComputePostSubmixVolume(speechType, volType, false);

	const f32 gunfireFactor = audWeaponAudioEntity::GetGunfireFactor();
	f32 rolloff = (g_BreathingRolloff*(1.0f-gunfireFactor)) + (gunfireFactor*g_BreathingRolloffGunfire);

	m_BreathSound->SetRequestedPostSubmixVolumeAttenuation(postSubmixVol);
	m_BreathSound->SetRequestedVolumeCurveScale(rolloff);
	m_BreathSound->SetRequestedHPFCutoff(metrics.hpf);
	m_BreathSound->SetRequestedLPFCutoff(metrics.lpf);
	m_BreathSound->SetRequestedVolume(metrics.vol[AUD_SPEECH_ROUTE_POS] + audDriverUtil::ComputeDbVolumeFromLinear(m_BreathingVolumeLin));
	m_BreathSound->SetRequestedPosition(position);
	m_BreathingVolumeLin = 1.0f;

	m_VisemeAnimationData = NULL;

	m_BreathSound->PrepareAndPlay(waveSlot);
	//g_SpeechManager.PlayedBreath(eEffectType);

	m_CachedSpeechType = speechType;
	m_CachedVolType = volType;
	m_CachedAudibility = audibility;
	m_SpeechVolumeOffset = 0.0f;
	m_IsFromScript = false;
	m_IsMutedDuringSlowMo = true;
	m_IsPitchedDuringSlowMo = true;
}

void audSpeechAudioEntity::PlayPlayerBreathing(PlayerVocalEffectType eEffectType, s32 predelay, f32 volOffset, bool isOnBike)
{
	if(!g_AudioEngine.IsAudioEnabled())
	{
		return;
	}

	naAssert(eEffectType >= BREATH_EXHALE && eEffectType <= BREATH_SWIM_PRE_DIVE);

	if(GetIsAnySpeechPlaying() || m_BreathSound || !m_Parent || m_Parent->IsDead() || eEffectType >= AUD_NUM_PLAYER_VFX_TYPES ||
		g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::DisablePlayerBreathing))
		return;

	if(eEffectType == BREATH_EXHALE_CYCLING)
	{
		TriggerPainFromPainId(AUD_PAIN_ID_CYCLING_EXHALE, 0);
		return;
	}

	if (!g_SpeechManager.IsPainLoaded(AUD_PAIN_VOICE_TYPE_BREATHING))
	{
		return;
	}

	u32 voiceNameHash = 0;
	u8 variation = 0;
	const char* context = g_SpeechManager.GetBreathingInfo(eEffectType,voiceNameHash, variation);
	audWaveSlot* waveSlot = g_SpeechManager.GetWaveSlotForPain(AUD_PAIN_VOICE_TYPE_BREATHING);

	if(!waveSlot)
		return;

	audSoundInitParams initParam;
	initParam.WaveSlot = waveSlot;
	initParam.AllowLoad = false;
	initParam.AllowOrphaned = true;
	initParam.Predelay = predelay;

	// Are we in the same vehicle as the player? If so, play speech frontend
	naAssertf(m_Parent, "speechaudioentity has no parent (Cped) assigned during PlayPain, will access null ptr");
	naCErrorf(m_Parent->GetPlaceableTracker(), "Unable to get tracker from CPed parent in PlayPain");
	initParam.EnvironmentGroup = m_Parent->GetPedAudioEntity()->GetEnvironmentGroup(true);
	initParam.RemoveHierarchy = false;

	u32 soundHash = isOnBike ? g_PlayerBikeInhaleSound : g_PlayerInhaleSound;
	if(eEffectType == BREATH_EXHALE)
		soundHash = isOnBike ? g_PlayerBikeExhaleSound : g_PlayerExhaleSound;

	m_BreathingOnBike = isOnBike;

	CreateSound_PersistentReference(soundHash, (audSound**)&m_BreathSound, &initParam);

	if (!m_BreathSound)
	{
		return;
	}

	if(!(((audSpeechSound*)m_BreathSound)->InitSpeech(voiceNameHash, context, variation)))
	{
		naErrorf("Unable to initialise speech in PlayPlayerBreathing");
		m_BreathSound->StopAndForget();
		return;
	}

	// Get the position to play positioned speech, and abort if we can't find a place to play it
	Vec3V position(V_ZERO);
	const bool success = UpdatePedHeadMatrix();
	if(!success)
	{
		return;	
	}
	position = VECTOR3_TO_VEC3V(m_PedHeadMatrix.d);

	// As we're about to play new speech, reset the smoothers (I hope this is what's causing a slight ramping on the occasional line)
	ResetSpeechSmoothers();

	// Get all the volumes, post submix volumes, and filters
	const audSpeechType speechType = AUD_SPEECH_TYPE_BREATHING;
	const audSpeechVolume volType = AUD_SPEECH_NORMAL;
	const audConversationAudibility audibility = AUD_AUDIBILITY_NORMAL;

	audEchoMetrics echo1, echo2;
	audSpeechMetrics metrics;
	const bool metricsProcessed = GetSpeechAudibilityMetrics(position, speechType, volType, false, audibility, metrics, echo1, echo2);
	if(!metricsProcessed)
	{
		naErrorf("Unable to GetSpeechAudibilityMetrics in PlayPlayerBreathing");
		m_BreathSound->StopAndForget();
		return;
	}

	const f32 postSubmixVol = ComputePostSubmixVolume(speechType, volType, false);

	const f32 gunfireFactor = audWeaponAudioEntity::GetGunfireFactor();
	f32 rolloff = (g_BreathingRolloff*(1.0f-gunfireFactor)) + (gunfireFactor*g_BreathingRolloffGunfire);

	m_BreathSound->SetRequestedPostSubmixVolumeAttenuation(postSubmixVol + volOffset);
	m_BreathSound->SetRequestedVolumeCurveScale(rolloff);
	m_BreathSound->SetRequestedHPFCutoff(metrics.hpf);
	m_BreathSound->SetRequestedLPFCutoff(metrics.lpf);
	m_BreathSound->SetRequestedVolume(metrics.vol[AUD_SPEECH_ROUTE_POS] + audDriverUtil::ComputeDbVolumeFromLinear(m_BreathingVolumeLin));
	m_BreathSound->SetRequestedPosition(position);

	m_VisemeAnimationData = NULL;

	m_BreathSound->PrepareAndPlay(waveSlot);
	g_SpeechManager.PlayedBreath(eEffectType);

	m_CachedSpeechType = speechType;
	m_CachedVolType = volType;
	m_CachedAudibility = audibility;
	m_SpeechVolumeOffset = 0.0f;
	m_IsFromScript = false;
	m_IsMutedDuringSlowMo = true;
	m_IsPitchedDuringSlowMo = true;

#if __BANK
	char msg[128] = "";
	formatf(msg, 128, "%s %s_%u", waveSlot->GetLoadedBankName(), context, variation);
	AddVFXDebugDrawStruct(msg,m_Parent);
#endif

	//visual effect
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(m_Parent);
	if (pVfxPedInfo==NULL || (eEffectType != BREATH_EXHALE && eEffectType != BREATH_EXHALE_CYCLING) )
	{
		return;
	}

	bool isCutscene = CutSceneManager::GetInstance()->IsCutscenePlayingBack();

	bool isScriptedCutscene = false;
	CPlayerInfo *pPlayerInfo = m_Parent->GetPlayerInfo();
	if (pPlayerInfo && pPlayerInfo->IsControlsScriptDisabled())
	{
		isScriptedCutscene = true;
	}
	
	bool isFirstPersonCameraActive = false;
#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	isFirstPersonCameraActive = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId()) && m_Parent->IsLocalPlayer();
#endif

	// trigger a breath effect 
	if (m_Parent->IsAPlayerPed() &&
		m_Parent->GetCurrentMotionTask() && !m_Parent->GetCurrentMotionTask()->IsInWater() && 
		g_weather.GetTemperature(m_Parent->GetTransform().GetPosition())<pVfxPedInfo->GetBreathPtFxTempEvoMax() && 
		!m_Parent->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) && 
		!camInterface::GetGameplayDirector().IsFirstPersonAiming(m_Parent) && 
		!isFirstPersonCameraActive &&
		!isScriptedCutscene && 
		!isCutscene)
	{
		int boneIndex = m_Parent->GetBoneIndexFromBoneTag(BONETAG_HEAD);
		if (boneIndex!=-1)
		{
			g_vfxPed.TriggerPtFxPedBreath(m_Parent, boneIndex);
		}
	}
}

void audSpeechAudioEntity::UpdatePlayerBreathing()
{
	static const f32 down3DbLin = audDriverUtil::ComputeLinearVolumeFromDb(-3.0f);
	static u8 bikeBreathCount = 0;

	if(!m_Parent || !m_Parent->GetMotionData() || m_AnimalParams )
	{
		m_BreathState = kBreathingIdle;
		return;
	}

	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	bool isWearingScubaMask = m_Parent->GetPedAudioEntity() && m_Parent->GetPedAudioEntity()->GetFootStepAudio().IsWearingScubaMask();

	if(isWearingScubaMask)
	{
		if(m_IsUnderwater && !GetIsAnySpeechPlaying() && !g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::SuppressPlayerScubaBreathing))
		{
			audPedAudioEntity* pedAudEnt = m_Parent ? m_Parent->GetPedAudioEntity() : NULL;
			if(timeInMs - m_LastTimeScubaBreathPlayed > g_TimeBetweenScubaBreaths && pedAudEnt && !pedAudEnt->IsScubaSoundPlaying())
			{
				m_Parent->GetPedAudioEntity()->PlayScubaBreath(!m_LastScubaBreathWasInhale);

				m_LastScubaBreathWasInhale = !m_LastScubaBreathWasInhale;
				m_LastTimeScubaBreathPlayed = timeInMs;
			}
		}
		else
		{
			//avoid playing breath the second we go underwater
			m_LastTimeScubaBreathPlayed = timeInMs;
		}

		return;
	}
	else
	{
		if( CNetwork::IsGameInProgress())
		{
			m_BreathState = kBreathingIdle;
			return;
		}
		if(m_Parent->GetIsSwimming())
		{
			return;
		}
	}
	CPedMotionData* motionData = m_Parent->GetMotionData();
	bool isAtLeastRunning = false;
	bool isSprinting = false;
	bool isPedaling = true;
	CVehicle* bikeObj = NULL;

	//Don't play player breathing if he is in a vehicle.  Also, keep track of last time in vehicle so we don't get a big breath if the player
	//		leaves the vehicle quickly.
	if(m_Parent->GetVehiclePedInside())
	{
		if(m_Parent->GetVehiclePedInside()->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			CTaskMotionOnBicycle* task = (CTaskMotionOnBicycle *)m_Parent->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE);
			bikeObj = static_cast<CVehicle*>(m_Parent->GetVehiclePedInside());
			if(task && task->IsPedStandingAndPedalling())
				m_LastTimePedaling = timeInMs;

			if(timeInMs - m_LastTimePedaling > 2000)
			{
				isPedaling = false;
				m_LastTimeNotPedaling = timeInMs;
			}
		}
		else
			m_LastTimeInVehicle = timeInMs;
	}

	if(g_DisablePlayerBreathing)
		return;

	if((bikeObj && bikeObj->IsInAir()) ) 
		return;

#if __BANK
	g_RunStartBreathVolInc = 30.0f/g_RunStartVolRampUpTimeMs;
	g_JogHighBreathVolDec = -30.0f/g_JogVolRampDownTimeMs1;
	g_JogRampDownVolDec  = -30.0f/g_JogVolRampDownTimeMs2;
	g_BikeStartBreathVolInc = 30.0f/g_BikeStartVolRampUpTimeMs;
#endif
	CTaskParachute* task = (CTaskParachute *)m_Parent->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE);
	
	if(m_Parent->GetUsingRagdoll() || task)
		m_BreathState = kBreathingIdle;

	if (timeInMs-m_LastTimeInVehicle > 500)
	{
		// cancel breath if the character has started talking/playing pain etc
		if(m_BreathSound && GetIsAnySpeechPlaying())
		{
			m_BreathSound->StopAndForget();
		}

		isAtLeastRunning = !motionData->GetIsLessThanRun() && m_Parent->GetVelocity().Mag2() > 1.0f;

		if(isAtLeastRunning)
		{
			if(motionData->GetIsSprinting())
			{
				m_LastTimeSprinting = timeInMs;
				isSprinting = true;
			}
			else
				m_LastTimeRunning = timeInMs;
		}
		else
			m_LastTimeNotRunningOrSprinting = timeInMs;

		switch((int)m_BreathState)
		{
		case kBreathingIdle:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingIdle  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 0.0f);
			if(bikeObj)
			{
				if(timeInMs - m_LastTimeNotPedaling > g_TimePedalingBeforeBreathing)
				{
					m_BreathState = kBreathingRunRampUp;
					m_BreathingVolumeLin = 0.0f;
				}
			}
			else
			{
				if(timeInMs - m_LastTimeNotRunningOrSprinting > g_TimeRunningBeforeBreathing && timeInMs - m_LastTimeSprinting < g_TimeJoggingBeforeLeavingRunBreathing)
				{
					m_BreathState = kBreathingRunRampUp;
					m_BreathingVolumeLin = 0.0f;
				}
			}
			break;
		case kBreathingRunRampUp:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingRunRampUp  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 1.0f);
			if(bikeObj)
			{
				if(!isPedaling)
					m_BreathState = kBreathingRunStop;
			}
			else
			{
				if(!isAtLeastRunning)
					m_BreathState = kBreathingRunStop;
				else if(!isSprinting && timeInMs - m_LastTimeSprinting < g_TimeJoggingBeforeLeavingRunBreathing && m_BreathingVolumeLin > down3DbLin)
					m_BreathState = kBreathingJogHighBreathing;
			}
			

			m_BreathingVolumeLin += bikeObj ? g_BikeStartBreathVolInc : g_RunStartBreathVolInc;
			if(m_BreathingVolumeLin >= 1.0f)
			{
				m_BreathingVolumeLin = 1.0f;
				m_BreathState = kBreathingRun;
			}

			if(!m_BreathSound)
			{
				u32 predelay = bikeObj ? audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBikeBreath, g_MaxPredelayOnBikeBreath):
						audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBreath, g_MaxPredelayOnBreath);
				PlayPlayerBreathing(m_LastBreathWasInhale ? BREATH_EXHALE : BREATH_INHALE, 
									predelay, audDriverUtil::ComputeDbVolumeFromLinear(m_BreathingVolumeLin), bikeObj != NULL);
				m_LastBreathWasInhale = !m_LastBreathWasInhale;
			}
			break;
		case kBreathingRun:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingRun  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 2.0f);
			if( (!bikeObj && !isAtLeastRunning) || (bikeObj && !isPedaling))
				m_BreathState = kBreathingRunStop;
			else if(!bikeObj && !isSprinting && timeInMs - m_LastTimeSprinting > g_TimeJoggingBeforeLeavingRunBreathing)
				m_BreathState = kBreathingJogHighBreathing;
			if(!m_BreathSound)
			{
				bool shouldPlayCycleBreath = false;
				if(bikeObj && m_LastBreathWasInhale && bikeBreathCount==2)
				{
					if(timeInMs - m_LastTimeNotPedaling > g_TimeToPedalBeforeCycleExhale)
						shouldPlayCycleBreath = true;
					else
					{
						bool isGoingUphill = (bikeObj->GetMatrix().b().GetYf() * bikeObj->GetMatrix().c().GetYf() < 0.0f );
						f32 angle0 = 0.0f;
						f32 angle1 = 0.0f;
						if(bikeObj->GetWheel(0) && bikeObj->GetWheel(0)->GetIsTouching())
						{
							const Vector3& normal0 = bikeObj->GetWheel(0)->GetHitNormal();
							f32 mdo = sqrtf( normal0.x * normal0.x + normal0.y * normal0.y );
							angle0 = atan2( normal0.z, mdo );
						}
						if(bikeObj->GetWheel(1) && bikeObj->GetWheel(1)->GetIsTouching())
						{
							const Vector3& normal1 = bikeObj->GetWheel(1)->GetHitNormal();
							f32 mdo = sqrtf( normal1.x * normal1.x + normal1.y * normal1.y );
							angle1 = atan2( normal1.z, mdo );
						}

						f32 angle = Max(angle0, angle1);
						shouldPlayCycleBreath = ( 90.f -  fabs(RtoD * angle) > g_BikeSlopeAngle && isGoingUphill );
					}
				}
					
				u32 predelay = bikeObj ? audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBikeBreath, g_MaxPredelayOnBikeBreath) :
					audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBreath, g_MaxPredelayOnBreath);
				PlayPlayerBreathing(m_LastBreathWasInhale ? (shouldPlayCycleBreath ? BREATH_EXHALE_CYCLING : BREATH_EXHALE) : BREATH_INHALE, 
					predelay, audDriverUtil::ComputeDbVolumeFromLinear(m_BreathingVolumeLin), bikeObj != NULL);

				if(bikeObj && m_LastBreathWasInhale)
				{
					bikeBreathCount++;
					bikeBreathCount %= 3;
				}

				m_LastBreathWasInhale = !m_LastBreathWasInhale;
			}
			break;
		case kBreathingRunStop:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingRunStop  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 3.0f);
			if(!m_BreathSound)
			{
				if(m_LastBreathWasInhale)
				{
					{
						Say("BREATH_EXHAUSTED");
						m_LastBreathWasInhale = !m_LastBreathWasInhale;
					}
					m_BreathState = kBreathingIdle;
				}
				else
				{
					PlayPlayerBreathing(m_LastBreathWasInhale ? BREATH_EXHALE : BREATH_INHALE, 
						audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBreath, g_MaxPredelayOnBreath), bikeObj != NULL);
					m_LastBreathWasInhale = !m_LastBreathWasInhale;
				}
			}
			break;
		case kBreathingJogHighBreathing:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingJogHighBreathing  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 2.0f);
			if(!isAtLeastRunning)
				m_BreathState = kBreathingJogStop;
			else if(isSprinting)
				m_BreathState = kBreathingRunRampUp;
			else
			{
				if(m_BreathingVolumeLin > down3DbLin)
					m_BreathingVolumeLin += g_JogHighBreathVolDec;
				else if(m_BreathingVolumeLin <= down3DbLin)
				{
					m_BreathState = kBreathingJogRampDown;
				}
			}

			if(!m_BreathSound)
			{
				PlayPlayerBreathing(m_LastBreathWasInhale ? BREATH_EXHALE : BREATH_INHALE, 
					audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBreath, g_MaxPredelayOnBreath),
					audDriverUtil::ComputeDbVolumeFromLinear(m_BreathingVolumeLin), bikeObj != NULL);
				m_LastBreathWasInhale = !m_LastBreathWasInhale;
			}
			break;
		case kBreathingJogRampDown:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingJogRampDown  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 1.0f);
			if(!isAtLeastRunning)
				m_BreathState = kBreathingJogStop;
			else if(isSprinting)
				m_BreathState = kBreathingRunRampUp;

			m_BreathingVolumeLin += g_JogRampDownVolDec;
			if(m_BreathingVolumeLin <= 0.0f)
			{
				m_BreathingVolumeLin = 0.0f;
				m_BreathState = kBreathingJogLowBreathing;
			}

			if(!m_BreathSound)
			{
				PlayPlayerBreathing(m_LastBreathWasInhale ? BREATH_EXHALE : BREATH_INHALE, 
					audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBreath, g_MaxPredelayOnBreath),
					audDriverUtil::ComputeDbVolumeFromLinear(m_BreathingVolumeLin), bikeObj != NULL);
				m_LastBreathWasInhale = !m_LastBreathWasInhale;
			}
			break;
		case kBreathingJogLowBreathing:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingJogLowBreathing  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 1.0f);
			if(!isAtLeastRunning)
				m_BreathState = kBreathingJogStop;
			else if(isSprinting)
				m_BreathState = kBreathingRunRampUp;

			if(timeInMs > m_TimeToNextPlayJogLowBreath)
			{
				if(!m_BreathSound)
				{
					//TODO: Change to jog low breath
					PlayPlayerBreathing(m_LastBreathWasInhale ? BREATH_EXHALE : BREATH_INHALE, 
						audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBreath, g_MaxPredelayOnBreath), bikeObj != NULL);
				}
				m_TimeToNextPlayJogLowBreath = timeInMs + audEngineUtil::GetRandomNumberInRange(g_MinTimeBetweenJogLowBreath, g_MaxTimeBetweenJogLowBreath);
			}
			break;
		case kBreathingJogStop:
#if __BANK
			if(g_EnableBreathingSpew)
				naDisplayf("BreathingJogStop  Vol:%f", m_BreathingVolumeLin);
#endif
			g_AudioEngine.GetSoundManager().SetVariableValue(ATSTRINGHASH("Game.Player.BreathState", 0x2DD1E053), 3.0f);
			if(m_BreathingVolumeLin <= 0.5f)
				m_BreathState = kBreathingIdle;
			else if(!m_BreathSound)
			{
				if(m_LastBreathWasInhale)
				{
					{
						Say("BREATH_EXHAUSTED");
						m_LastBreathWasInhale = !m_LastBreathWasInhale;
					}
					m_BreathState = kBreathingIdle;
				}
				else
				{
					PlayPlayerBreathing(m_LastBreathWasInhale ? BREATH_EXHALE : BREATH_INHALE, 
						audEngineUtil::GetRandomNumberInRange(g_MinPredelayOnBreath, g_MaxPredelayOnBreath), bikeObj != NULL);
					m_LastBreathWasInhale = !m_LastBreathWasInhale;
				}
			}
			break;
		}
	}
}

#if __BANK
void audSpeechAudioEntity::DisplayVoiceDebugInfo()
{
	// Get all our info - do we have a voice assigned? Do we have a pvg name? Do we have a full voice, a (non)driving mini? Does voice exist?
	char voiceDebugInfo[64];
	/*
	u8 assignedPVG = 0;
	if (GetPedVoiceGroup() != ATSTRINGHASH("VOICE_PLY_CR", 0x664D1B03))
	{
		assignedPVG = 1;
	}
	u8 assignedVoice = 0;
	if (m_AmbientVoiceNameHash!=0)
	{
		assignedVoice = 1;
	}
	u8 isAFullVoice = 0;
	if (g_SpeechManager.GetPrimaryVoice(GetPedVoiceGroup()) == m_AmbientVoiceNameHash)
	{
		isAFullVoice = 1;
	}
	s32 full = -1;
	s32 gang = -1;
	s32 mini = -1;
	s32 miniDriving = -1;
	g_SpeechManager.GetMaxReferencesForPVG(GetPedVoiceGroup(), full, gang, mini, miniDriving);
	formatf(voiceDebugInfo, 63, "%d%d%d%d:%d:%d:%d:%d", assignedPVG, assignedVoice, audSpeechSound::DoesVoiceExist(m_AmbientVoiceNameHash), isAFullVoice, full, gang, mini, miniDriving);//, m_FailedToGetMiniVoice);
	*/
	const char* pvg = "No PVG";
	if (GetPedVoiceGroup() != ATSTRINGHASH("VOICE_PLY_CR", 0x0664d1b03))
	{
		pvg = "PVG";
	}

	if (m_AmbientVoiceNameHash==0)
	{
		formatf(voiceDebugInfo, 63, "%s: No Voice Aloc", pvg);
	}
	else if (m_AmbientVoiceNameHash == g_NoVoiceHash)
	{
		formatf(voiceDebugInfo, 63, "%s: Failed To Find Voice", pvg);
	}
	else
	{
		formatf(voiceDebugInfo, 63, "%s: %u", pvg, m_AmbientVoiceNameHash);
	}

	grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_Parent->GetTransform().GetPosition()) + Vector3(0.f,0.f,0.5f), Color32(255,0,0),voiceDebugInfo);
}
#endif

void audSpeechAudioEntity::UpdateClass()
{
#if __BANK
	if(g_LaughterTrackEditorEnabled)
	{
		CPad& debugPad = CControlMgr::GetDebugPad();
		RadioTrackTextIds::tTextId textID;
		bool addNewTextID = false;

		if(debugPad.ButtonCircleJustDown())
		{
			textID.TextId = ATSTRINGHASH("TV_BAD", 0xF132D341);
			FindPlayerPed()->GetSpeechAudioEntity()->Say("TV_BAD");
			addNewTextID = true;
		}
		else if(debugPad.ButtonCrossJustDown())
		{
			textID.TextId = ATSTRINGHASH("TV_GOOD", 0x7149C386);
			FindPlayerPed()->GetSpeechAudioEntity()->Say("TV_GOOD");
			addNewTextID = true;
		}

		if(addNewTextID)
		{
			const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

			if(currentVideo && stricmp(currentVideo->m_Name.GetCStr(), g_LaughterTrackTVShowName) == 0)
			{
				f32 currentPlayTime = CTVPlaylistManager::GetCurrentTVShowTime();
				f32 playTimeSeconds = CTVPlaylistManager::ScaleToRealTime(currentPlayTime);
				u32 playTimeMs = (u32)(playTimeSeconds * 1000.0f);
				textID.OffsetMs = playTimeMs;
				g_LaughterTrackEditorTrackIDs.PushAndGrow(textID);
			}
		}
	}
#endif
}

#if __BANK

void audSpeechAudioEntity::UpdateDebug()
{
	fwEntity* debugEntity = g_PickerManager.GetSelectedEntity();
	m_IsDebugEntity = m_Parent && (m_Parent == debugEntity || (!debugEntity && m_Parent->IsLocalPlayer()));

	if(m_Parent)
	{
#if __DEV
		// show the ped
		if (g_DisplayVoiceInfo)
		{
			if (g_DisplayVoiceInfoForAllPeds || m_IsDebugEntity)
			{
				if (g_AudioEngine.GetEnvironment().ComputeDistanceToClosestPanningListener(m_Parent->GetTransform().GetPosition()) < g_DisplayVoiceInfoDistance)
				{
					DisplayVoiceDebugInfo();
				}
			}
		}
#endif// __DEV
		if(PARAM_phoneConversationHistoryTest.Get() && audEngineUtil::ResolveProbability(0.001f))
		{
			s32 convVar = g_SpeechManager.GetPhoneConversationVariationFromVoiceHash(m_AmbientVoiceNameHash);
			if(convVar > 0)
				g_SpeechManager.AddPhoneConversationVariationToHistory(m_AmbientVoiceNameHash, convVar, fwTimer::GetTimeInMilliseconds());
		}

		if(g_DebugVolumeMetrics && m_IsDebugEntity)
		{
			Vector3 debugPos;
			const bool posSuccess = GetPositionForSpeechSound(debugPos, (audSpeechVolume)g_SpeechVolumeTypeOverride);
			if(posSuccess)
			{
				audEchoMetrics echo1, echo2;
				ComputeEchoInitMetrics(debugPos, (audSpeechVolume)g_SpeechVolumeTypeOverride, true, true, echo1, echo2);

				audSpeechMetrics debugMetrics;
				GetSpeechAudibilityMetrics(VECTOR3_TO_VEC3V(debugPos), (audSpeechType)g_SpeechTypeOverride, 
											(audSpeechVolume)g_SpeechVolumeTypeOverride, g_DebugIsInCarFEScene, 
											(audConversationAudibility)g_SpeechAudibilityOverride, debugMetrics, echo1, echo2);
			}
		}
		if (g_MakePedSpeak && m_IsDebugEntity)
		{
			s32 variation = g_AnimTestSpeechVariation;
			Say(g_TestContext, g_PreloadOnly ? "SPEECH_PARAMS_FORCE_PRELOAD_ONLY" : "SPEECH_PARAMS_FORCE", atStringHash(g_TestVoice), -1, NULL, 0, -1, 1.0f,  false, &variation);
			g_MakePedSpeak = false;
		}
		if (g_TriggerPreload && m_IsDebugEntity)
		{
			PlayPreloadedSpeech_FromScript();
			g_TriggerPreload = false;
		}
		if (g_MakePedDoMegaphone && m_IsDebugEntity)
		{
			Say("PULL_OVER_WARNING", "SPEECH_PARAMS_FORCE", ATSTRINGHASH("M_M_FATCOP_01_BLACK", 0x00831b5c9));
			g_MakePedDoMegaphone = false;
		}
		if (g_MakePedDoHeli && m_IsDebugEntity)
		{
			Say("COP_HELI_MEGAPHONE", "SPEECH_PARAMS_FORCE", ATSTRINGHASH("M_Y_HELI_COP", 0x074fe01ea));
			g_MakePedDoHeli = false;
		}
		if (g_MakePedDoCoverMe && m_IsDebugEntity)
		{
			Say("CONV_GANG_STATE", "SPEECH_PARAMS_FORCE", ATSTRINGHASH("M_RUS_FULL_01", 0x09b69e673));
			g_MakePedDoCoverMe = false;
		}
		if (g_MakePedDoBlocked && m_IsDebugEntity)
		{
			Say("BLOCKED_VEHICLE", "SPEECH_PARAMS_FORCE", ATSTRINGHASH("M_Y_COP_BLACK", 0x0e573b589));
			g_MakePedDoBlocked = false;
		}
		if (g_MakePedDoPanic && m_IsDebugEntity)
		{
			Say("PANIC", "SPEECH_PARAMS_FORCE", g_PainMale);
			g_MakePedDoPanic = false;
		}
		if (g_TestWallaSound && m_IsDebugEntity)
		{
			audSoundInitParams params;
			Vector3 pos;
			m_Parent->GetBonePosition(pos, BONETAG_HEAD);
			params.Position.Set(pos);
			params.Category = g_SpeechNormalCategory;
			CreateSound_LocalReference("WALLA_SPEECH_TEST", (audSound**)&g_WallaTestSoundPtr, &params);
			if(g_WallaTestSoundPtr)
			{
				g_WallaTestSlot = g_SpeechManager.GetAmbientSpeechSlot(this, NULL, 100.0);
				if(g_WallaTestSlot > -1)
				{
					g_WallaTestSoundPtr->PrepareAndPlay(g_SpeechManager.GetAmbientWaveSlotFromId(g_WallaTestSlot), true, -1);
				}
			}

			g_TestWallaSound = false;
		}
		if(g_WallaTestSlot > -1 && !g_WallaTestSoundPtr)
		{
			g_SpeechManager.FreeAmbientSpeechSlot(g_WallaTestSlot);
		}
		if(g_RunCanChatTest && audEngineUtil::ResolveProbability(0.001f))
		{
			if(!g_ChatTestOtherPed && m_Parent)
				g_ChatTestOtherPed = m_Parent;
			else if(g_ChatTestOtherPed && g_ChatTestOtherPed != m_Parent)
			{
				CanPedHaveChatConversationWithPed(g_ChatTestOtherPed, g_SpeechContextChatStatePHash, g_SpeechContextChatRespPHash);
				g_ChatTestOtherPed = NULL;
			}
		}
		if(g_TriggeredContextTest)
		{

			if(audEngineUtil::ResolveProbability(0.33f)) 
			{
				if(Say("TC_TEST_1") == AUD_SPEECH_SAY_SUCCEEDED)
					Displayf("Played TC_TEST_1");
			}
			/*else if(audEngineUtil::ResolveProbability(0.33f)) 
			{
			if(Say("TC_TEST_2") == AUD_SPEECH_SAY_SUCCEEDED)
			Displayf("Played TC_TEST_2");
			}
			else
			{
			if(Say("TC_TEST_3") == AUD_SPEECH_SAY_SUCCEEDED)
			Displayf("Played TC_TEST_3");
			}*/
		}
		if(m_IsDebugEntity)
		{
			m_IsCrying = g_MakeFocusPedCry;
			SetCryingContext(g_AlternateCryingContext);
		}
	}
}

void audSpeechAudioEntity::PlaySpeechCallback(void)
{
	CPed *player = FindPlayerPed();
	if(player != NULL && player->GetSpeechAudioEntity())
	{
		player->SetIsWaitingForSpeechToPreload(true);
		player->GetSpeechAudioEntity()->PlayScriptedSpeech(g_AnimTestVoiceName, g_AnimTestContextName, 1/*g_AnimTestSpeechVariation*/, NULL, AUD_SPEECH_NORMAL, true);
	}
}

void audSpeechAudioEntity::ResetSpecPainStatsCallback(void)
{
	g_SpeechManager.ResetSpecificPainDrawStats();
}

void audSpeechAudioEntity::StartLaughterTrackEditor()
{
	if(!g_LaughterTrackEditorEnabled)
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			camDebugDirector& debugDirector = camInterface::GetDebugDirector();
			debugDirector.GetFreeCam()->DisableAllControls();
			debugDirector.SetShouldIgnoreDebugPadCameraToggle(true);

			g_LaughterTrackEditorTrackIDs.Reset();
			g_LaughterTrackTVShowName = currentVideo->m_Name.GetCStr();
			g_LaughterTrackEditorEnabled = true;
			g_DebugDrawLaughterTrack = true;
		}
	}
}

void audSpeechAudioEntity::StopLaughterTrackEditor()
{
	if(g_LaughterTrackEditorEnabled)
	{
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		debugDirector.SetShouldIgnoreDebugPadCameraToggle(false);
		g_LaughterTrackEditorEnabled = false;
		g_LaughterTrackTVShowName = NULL;
	}
}

void audSpeechAudioEntity::SaveLaughterTrack()
{
	if(g_LaughterTrackTVShowName)
	{
		char xmlMessage[4096];
		char tempbuffer[256] = {0};

		audAmbientAudioEntity::SerialiseMessageStart(xmlMessage, tempbuffer, "RadioTrackTextIds", g_LaughterTrackTVShowName);

		for(u32 loop = 0; loop < g_LaughterTrackEditorTrackIDs.GetCount(); loop++)
		{
			audAmbientAudioEntity::SerialiseTag(xmlMessage, tempbuffer, "TextId", true);
			audAmbientAudioEntity::SerialiseUInt(xmlMessage, tempbuffer, "OffsetMs", g_LaughterTrackEditorTrackIDs[loop].OffsetMs);
			audAmbientAudioEntity::SerialiseUInt(xmlMessage, tempbuffer, "TextId", g_LaughterTrackEditorTrackIDs[loop].TextId);
			audAmbientAudioEntity::SerialiseTag(xmlMessage, tempbuffer, "TextId", false);
		}

		audAmbientAudioEntity::SerialiseMessageEnd(xmlMessage, tempbuffer, "RadioTrackTextIds");

		naDisplayf("XML message is %d characters long \n", istrlen(xmlMessage));

		audRemoteControl &rc = g_AudioEngine.GetRemoteControl();
		bool success = rc.SendXmlMessage(xmlMessage, istrlen(xmlMessage));
		naAssertf(success, "Failed to send xml message to rave");
		naDisplayf("%s", (success)? "Success":"Failed");
	}
}

void audSpeechAudioEntity::SetChannelCB()
{
	CTVPlaylistManager::SetTVChannel( g_LaughterTrackChannelSelection - 1 );
}

void audSpeechAudioEntity::DebugDraw()
{
	Color32 color(1.0f, 0.0f, 0.0f);
	if(g_TurnOnVFXInfo)
	{
		for(int i=0; i<64; ++i)
		{
			if(sm_VFXDebugDrawStructs[i].msg && sm_VFXDebugDrawStructs[i].time > 0.0f)
			{
				grcDebugDraw::Text(sm_VFXDebugDrawStructs[i].pos, color, sm_VFXDebugDrawStructs[i].msg);
			}
		}
	}
	if(g_TurnOnAmbSpeechInfo)
	{
		for(int i=0; i<64; ++i)
		{
			if(sm_AmbSpeechDebugDrawStructs[i].msg && sm_AmbSpeechDebugDrawStructs[i].time > 0.0f)
			{
				grcDebugDraw::Text(sm_AmbSpeechDebugDrawStructs[i].pos, sm_AmbSpeechDebugDrawStructs[i].color, sm_AmbSpeechDebugDrawStructs[i].msg);
			}
		}

		char buf[64];
		f32 yPos = 0.05f;
		f32 xPos = 0.05f;
		formatf(buf, sizeof(buf), "Angry: %s", g_IsPlayerAngry ? "Yes" : "No");
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_blue1, buf);
	}
	if(g_DisplayLipsyncTimingInfo)
	{
		/*fwEntity *pSelectedEntity = g_PickerManager.GetSelectedEntity();
		if(pSelectedEntity == NULL)
		{
			pSelectedEntity = CGameWorld::FindLocalPlayer();
		}
		if(pSelectedEntity && pSelectedEntity->GetType() == ENTITY_TYPE_PED)
		{
			CPed* pSelectedPed = static_cast<CPed*>(pSelectedEntity);
			if(pSelectedPed && pSelectedPed->GetSpeechAudioEntity() && pSelectedPed->GetSpeechAudioEntity()->GetIsAnySpeechPlaying())
			{*/
				f32 yPos = 0.05f;
				f32 xPos = 0.05f;
				grcDebugDraw::Text(Vector2(xPos, yPos), Color_blue1, sm_VisemeTimingingInfoString);
			/*}
		}*/
	}

	if(g_DebugDrawLaughterTrack)
	{
		const CTVVideoInfo* currentVideo = CTVPlaylistManager::GetCurrentlyPlayingVideo();

		if(currentVideo)
		{
			f32 currentPlayTime = CTVPlaylistManager::GetCurrentTVShowTime();
			f32 playTimeSeconds = CTVPlaylistManager::ScaleToRealTime(currentPlayTime);
			u32 playTimeMs = (u32)(playTimeSeconds * 1000.0f);

			f32 currentDuration = currentVideo->GetDuration();
			f32 currentDurationSeconds = CTVPlaylistManager::ScaleToRealTime(currentDuration);
			u32 durationMs = (u32)(currentDurationSeconds * 1000.0f);

			const RadioTrackTextIds *textIds = audNorthAudioEngine::GetObject<RadioTrackTextIds>(currentVideo->m_Name);
			s32 markerIndex = GetNextLaughterTrackMarkerIndex(textIds, playTimeMs);

			char buf[128];
			f32 xPos = 0.5f;
			f32 yPos = 0.05f;

			formatf(buf, sizeof(buf), "TV Show: %s", currentVideo->m_Name.GetCStr());
			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;

			formatf(buf, sizeof(buf), "Play Time: %d", playTimeMs);
			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;

			formatf(buf, sizeof(buf), "Video Duration: %d", durationMs);
			grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			yPos += 0.02f;

			if(!g_LaughterTrackEditorEnabled)
			{
				formatf(buf, sizeof(buf), "Marker Index: %d", markerIndex);
				grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
			}
		}
	}

	if(g_LaughterTrackEditorEnabled)
	{
		char buf[128];
		f32 xPos = 0.05f;
		f32 yPos = 0.05f - (1.0f * g_LaughterTrackEditorScroll);

		formatf(buf, sizeof(buf), "Editing Track: %s", g_LaughterTrackTVShowName);
		grcDebugDraw::Text(Vector2(xPos, yPos), Color_white, buf);
		yPos += 0.04f;

		for(u32 loop = 0; loop < g_LaughterTrackEditorTrackIDs.GetCount(); loop++)
		{
			bool isGood = g_LaughterTrackEditorTrackIDs[loop].TextId == ATSTRINGHASH("TV_GOOD", 0x7149C386);
			formatf(buf, sizeof(buf), "%d %s", g_LaughterTrackEditorTrackIDs[loop].OffsetMs, isGood? "TV_GOOD" : "TV_BAD");
			grcDebugDraw::Text(Vector2(xPos, yPos), isGood? Color_green : Color_red, buf);
			yPos += 0.02f;
		}

	}
}

void audSpeechAudioEntity::AddVFXDebugDrawStruct(const char* msg, CPed* actor)
{
	if(!msg || !actor)
		return;

	Vector3 pos = VEC3V_TO_VECTOR3(actor->GetTransform().GetPosition());
	pos.y += 1.5;
	Vector3 cameraPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	f32 newDist2 = cameraPos.Dist2(pos);
	f32 fGreatestDist = 0.0f;
	int iFurthestIndex = -1;

	for(int i=0; i<64; ++i)
	{
		if(sm_VFXDebugDrawStructs[i].actor == actor)
		{
			sm_VFXDebugDrawStructs[i].time = 5;
			safecpy(sm_VFXDebugDrawStructs[i].msg, msg, 128);
			sm_VFXDebugDrawStructs[i].pos.Set(pos);
			return;
		}
	}

	for(int i=0; i<64; ++i)
	{
		if(sm_VFXDebugDrawStructs[i].time <= 0)
		{
			sm_VFXDebugDrawStructs[i].time = 5;
			safecpy(sm_VFXDebugDrawStructs[i].msg, msg, 128);
			sm_VFXDebugDrawStructs[i].pos.Set(pos);
			sm_VFXDebugDrawStructs[i].actor = actor;
			return;
		}

		f32 dist2 = cameraPos.Dist2(sm_VFXDebugDrawStructs[i].pos);
		if(dist2>fGreatestDist && newDist2<dist2)
		{
			fGreatestDist = dist2;
			iFurthestIndex = i;
		}
	}

	if(iFurthestIndex>=0)
	{
		sm_VFXDebugDrawStructs[iFurthestIndex].time = 5;
		safecpy(sm_VFXDebugDrawStructs[iFurthestIndex].msg, msg, 128);
		sm_VFXDebugDrawStructs[iFurthestIndex].pos.Set(pos);
		sm_VFXDebugDrawStructs[iFurthestIndex].actor = actor;
	}
}

void audSpeechAudioEntity::RemoveVFXDebugDrawStruct(CPed* actor)
{
	for(int i=0; i<64; ++i)
	{
		if(sm_VFXDebugDrawStructs[i].actor == actor)
		{
			sm_VFXDebugDrawStructs[i].actor = NULL;
			return;
		}
	}
}

void audSpeechAudioEntity::UpdateVFXDebugDrawStructs()
{
	//I'd like to get rid of this once I figure out why it crashes when actors are destroyed.
	if(!g_TurnOnVFXInfo)
		return;

	for(int i=0; i<64; ++i)
	{
		if(!sm_VFXDebugDrawStructs[i].actor || !sm_VFXDebugDrawStructs[i].msg)
		{
			sm_VFXDebugDrawStructs[i].time = 0;
		}
		else
		{
			sm_VFXDebugDrawStructs[i].time -= TIME.GetSeconds();
			if(!sm_VFXDebugDrawStructs[i].actor->IsDead() && sm_VFXDebugDrawStructs[i].actor->GetTransformPtr())
			{
				sm_VFXDebugDrawStructs[i].pos.Set(VEC3V_TO_VECTOR3(sm_VFXDebugDrawStructs[i].actor->GetMatrix().d()));
				sm_VFXDebugDrawStructs[i].pos.z += 1.5f;
			}
		}
	}
}

void audSpeechAudioEntity::AddAmbSpeechDebugDrawStruct(const char* msg, CPed* actor, Color32 color)
{
	if(!msg || !actor)
		return;

	Vector3 pos = VEC3V_TO_VECTOR3(actor->GetTransform().GetPosition());
	pos.z += 1.5;
	Vector3 cameraPos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	f32 newDist2 = cameraPos.Dist2(pos);
	f32 fGreatestDist = 0.0f;
	int iFurthestIndex = -1;

	for(int i=0; i<64; ++i)
	{
		if(sm_AmbSpeechDebugDrawStructs[i].actor == actor)
		{
			sm_AmbSpeechDebugDrawStructs[i].time = 5;
			safecpy(sm_AmbSpeechDebugDrawStructs[i].msg, msg, 128);
			sm_AmbSpeechDebugDrawStructs[i].pos.Set(pos);
			sm_AmbSpeechDebugDrawStructs[i].color = color;
			return;
		}
	}

	for(int i=0; i<64; ++i)
	{
		if(sm_AmbSpeechDebugDrawStructs[i].time <= 0)
		{
			sm_AmbSpeechDebugDrawStructs[i].time = 5;
			safecpy(sm_AmbSpeechDebugDrawStructs[i].msg, msg, 128);
			sm_AmbSpeechDebugDrawStructs[i].pos.Set(pos);
			sm_AmbSpeechDebugDrawStructs[i].actor = actor;
			sm_AmbSpeechDebugDrawStructs[i].color = color;
			return;
		}

		f32 dist2 = cameraPos.Dist2(sm_AmbSpeechDebugDrawStructs[i].pos);
		if(dist2>fGreatestDist && newDist2<dist2)
		{
			fGreatestDist = dist2;
			iFurthestIndex = i;
		}
	}

	if(iFurthestIndex>=0)
	{
		sm_AmbSpeechDebugDrawStructs[iFurthestIndex].time = 5;
		safecpy(sm_AmbSpeechDebugDrawStructs[iFurthestIndex].msg, msg, 128);
		sm_AmbSpeechDebugDrawStructs[iFurthestIndex].pos.Set(pos);
		sm_AmbSpeechDebugDrawStructs[iFurthestIndex].actor = actor;
		sm_AmbSpeechDebugDrawStructs[iFurthestIndex].color = color;
	}
}

void audSpeechAudioEntity::RemoveAmbSpeechDebugDrawStruct(CPed* actor)
{
	for(int i=0; i<64; ++i)
	{
		if(sm_AmbSpeechDebugDrawStructs[i].actor == actor)
		{
			sm_AmbSpeechDebugDrawStructs[i].actor = NULL;
			return;
		}
	}
}

void audSpeechAudioEntity::UpdateAmbSpeechDebugDrawStructs()
{
	if(!g_TurnOnAmbSpeechInfo)
		return;

	for(int i=0; i<64; ++i)
	{
		if(!sm_AmbSpeechDebugDrawStructs[i].actor || !sm_AmbSpeechDebugDrawStructs[i].msg)
		{
			sm_AmbSpeechDebugDrawStructs[i].time = 0;
		}
		else
		{
			sm_AmbSpeechDebugDrawStructs[i].time -= TIME.GetSeconds();
			if(!sm_AmbSpeechDebugDrawStructs[i].actor->IsDead() && sm_AmbSpeechDebugDrawStructs[i].actor->GetTransformPtr())
			{
				sm_AmbSpeechDebugDrawStructs[i].pos.Set(VEC3V_TO_VECTOR3(sm_AmbSpeechDebugDrawStructs[i].actor->GetMatrix().d()));
				sm_AmbSpeechDebugDrawStructs[i].pos.z += 1.5f;
			}
		}
	}
}

void audSpeechAudioEntity::PrintSoundPool(ASSERT_ONLY(const char* context, const u32 contextPHash, const audMetadataRef soundRef))
{
	if(!g_HasPrintedSoundPoolForCancelledPlayback)
	{
		audSound::GetStaticPool().DebugPrintSoundPool();
		g_HasPrintedSoundPoolForCancelledPlayback = true;
	}
	BANK_ONLY(if(m_UsingStreamingBucking))
	{
		Assertf(0, "Cancelling playback of preloaded speech. Context: %s ContextPHash: %u soundName: %u", context, contextPHash, soundRef.Get());
	}
	audSoundPoolStats stats;
	audSound::GetStaticPool().GetPoolUtilisation(stats);
	naWarningf("ReqSettings  ---  Most Free: %u Least Free: %u", stats.mostReqSetsSlotsFree, stats.leastReqSetsSlotsFree);
}

//Found this useful in debugging
//#define DISPLAY_VOICE_HASH(x) Displayf("%s %u", x, atStringHash(x));
//#define DISPLAY_CONTEXT_HASH(x) Displayf("%s %u", x, atPartialStringHash(x));
//
//void audSpeechAudioEntity::DisplayVoiceNameHashes()
//{
//	DISPLAY_VOICE_HASH("M_RUS_FULL_01")
//		DISPLAY_VOICE_HASH("M_RUS_FULL_02")
//		DISPLAY_VOICE_HASH("ALPINE_GUARD_01")
//		DISPLAY_VOICE_HASH("ALPINE_GUARD_03")
//		DISPLAY_VOICE_HASH("ALPINE_GUARD_02")
//		DISPLAY_VOICE_HASH("SILENT_VOICE")
//		DISPLAY_VOICE_HASH("ADPCM_TEST")
//		DISPLAY_VOICE_HASH("DRUG_BUYER")
//		DISPLAY_VOICE_HASH("DRUG_DEALER")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_02_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_02_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hipster_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_M_BevHills_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_M_BevHills_01_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_F_M_BevHills_02_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_M_BevHills_02_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_M_SkidRow_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_M_SkidRow_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Beach_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Beach_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_02_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_01_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_F_Y_Fitness_02_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Fitness_02_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_02_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_03_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_03_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_04_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_04_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_F_Y_Tourist_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Tourist_01_Latino_Mini_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Tourist_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_01_Latino_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_02_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_02_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_Business_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_Business_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_Farmer_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_Farmer_01_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_M_M_Farmer_01_White_Mini_03")
//		DISPLAY_VOICE_HASH("A_M_M_Salton_02_White_Mini_01")
//		DISPLAY_VOICE_HASH("A_M_M_Salton_02_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_02_White_Mini_01")
//		DISPLAY_VOICE_HASH("AIRDRUMMER")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hiker_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_02_WHITE_MINI_02")
//		DISPLAY_VOICE_HASH("A_M_Y_Skater_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_03_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Fitness_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_03_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BeachVesp_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BevHills_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_02_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hipster_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Runner_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_EastSA_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_SouCent_03_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_EastSA_03_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_EastSA_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_FatLatin_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Genfat_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Malibu_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_SoCenLat_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_TranVest_02_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_EastSA_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_GenStreet_02_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StLat_01_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_03_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_M_EastSA_02_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("AIRGUITARIST")
//		DISPLAY_VOICE_HASH("AIRPIANIST")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_01_CHINESE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_GenStreet_01_Chinese_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Polynesian_01_Polynesian_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_M_Ktown_01_Korean_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_M_Salton_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_O_Ktown_01_Korean_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_04_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Epsilon_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Vinewood_04_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Malibu_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Tourist_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BeachVesp_01_CHINESE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Epsilon_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_KTown_02_Korean_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_03_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_M_Bodybuild_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_M_Business_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_03_Chinese_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Vinewood_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Vinewood_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_HillBilly_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BusiCas_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_GenStreet_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StWhi_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_M_Tourist_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_04_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Golfer_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Tourist_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_03_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hipster_03_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Golfer_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_M_FatWhite_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Skater_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_EastSA_02_Latino_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Golfer_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_HillBilly_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Skater_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hippy_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Salton_01_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StWhi_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_02_White_Mini_02")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_02_WHITE_MINI_01")
//		DISPLAY_VOICE_HASH("FRANKLIN_ANGRY")
//		DISPLAY_VOICE_HASH("FRANKLIN_NORMAL")
//		DISPLAY_VOICE_HASH("MICHAEL_ANGRY")
//		DISPLAY_VOICE_HASH("MICHAEL_NORMAL")
//		DISPLAY_VOICE_HASH("TREVOR_ANGRY")
//		DISPLAY_VOICE_HASH("TREVOR_NORMAL")
//		DISPLAY_VOICE_HASH("WAVELOAD_PAIN_MALE")
//		DISPLAY_VOICE_HASH("WAVELOAD_PAIN_FEMALE")
//		DISPLAY_VOICE_HASH("S_M_Y_Cop_01_Black_Mini_01")
//		DISPLAY_VOICE_HASH("S_M_Y_Cop_01_Black_Mini_02")
//		DISPLAY_VOICE_HASH("S_M_Y_Cop_01_White_Mini_01")
//		DISPLAY_VOICE_HASH("S_M_Y_Cop_01_White_Mini_02")
//		DISPLAY_VOICE_HASH("S_F_Y_Cop_01_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("S_F_Y_Cop_01_BLACK_FULL_02")
//		DISPLAY_VOICE_HASH("S_F_Y_Cop_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("S_F_Y_Cop_01_WHITE_FULL_02")
//		DISPLAY_VOICE_HASH("S_M_Y_Cop_01_BLACK_FULL_02")
//		DISPLAY_VOICE_HASH("S_M_Y_HWayCop_01_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("S_M_Y_Cop_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("S_M_Y_Cop_01_WHITE_FULL_02")
//		DISPLAY_VOICE_HASH("S_M_Y_HWayCop_01_BLACK_FULL_02")
//		DISPLAY_VOICE_HASH("S_M_Y_HWayCop_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("S_M_Y_HWayCop_01_WHITE_FULL_02")
//		DISPLAY_VOICE_HASH("ANDYMOON")
//		DISPLAY_VOICE_HASH("BAYGOR")
//		DISPLAY_VOICE_HASH("CLINTON")
//		DISPLAY_VOICE_HASH("IMPORAGE")
//		DISPLAY_VOICE_HASH("JANE")
//		DISPLAY_VOICE_HASH("JEROME")
//		DISPLAY_VOICE_HASH("JESSE")
//		DISPLAY_VOICE_HASH("PAMELADRAKE")
//		DISPLAY_VOICE_HASH("BILLBINDER")
//		DISPLAY_VOICE_HASH("MIME")
//		DISPLAY_VOICE_HASH("GRIFF")
//		DISPLAY_VOICE_HASH("MANI")
//		DISPLAY_VOICE_HASH("ZOMBIE")
//		DISPLAY_VOICE_HASH("LAMAR_NORMAL")
//		DISPLAY_VOICE_HASH("LAMAR_NORMAL")
//		DISPLAY_VOICE_HASH("LAMAR_DRUNK")
//		DISPLAY_VOICE_HASH("CHENG")
//		DISPLAY_VOICE_HASH("COOK")
//		DISPLAY_VOICE_HASH("LESTER")
//		DISPLAY_VOICE_HASH("NERVOUSRON")
//		DISPLAY_VOICE_HASH("TRACEY")
//		DISPLAY_VOICE_HASH("TRANSLATOR")
//		DISPLAY_VOICE_HASH("WADE")
//		DISPLAY_VOICE_HASH("RINGTONES")
//		DISPLAY_VOICE_HASH("A_F_M_BevHills_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_BevHills_02_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Beach_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Fitness_02_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Golfer_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_03_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_04_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_01_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_02_LATINO_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_BevHills_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Beach_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_03_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Fitness_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Tourist_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Salton_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_O_Salton_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BevHills_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BevHills_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_01_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Runner_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Skater_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Fitness_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hiker_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Tourist_01_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_03_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_03_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BeachVesp_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_02_BLACK_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hipster_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Business_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Salton_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_01_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BeachVesp_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_Bodybuild_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_FatWhite_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_04_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_EastSA_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Genfat_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_O_GenStreet_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BevHills_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_02_WHITE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_MusclBeac_02_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_Bodybuild_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_SkidRow_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_O_Salton_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_O_SouCent_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_03_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_EastSA_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_EastSA_02_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Vinewood_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Business_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Malibu_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_SouCent_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Golfer_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_MusclBeac_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_SouCent_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StLat_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_03_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_Beach_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_Downtown_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_EastSA_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_Ktown_01_Korean_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_SkidRow_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_Tramp_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_TrampBeac_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_TrampBeac_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_O_SouCent_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_BevHills_04_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Hipster_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Skater_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_SouCent_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_SouCent_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Vinewood_03_Chinese_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_AfriAmer_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_02_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_BevHills_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Golfer_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Golfer_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Malibu_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Skater_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Skater_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Skidrow_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_SouCent_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_SouCent_03_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_SouCent_04_black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Tramp_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_TrampBeac_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_O_Beach_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_O_SouCent_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_O_SouCent_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_O_SouCent_03_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_O_Tramp_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_02_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BevHills_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Business_03_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Downtown_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_EastSA_02_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Epsilon_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Epsilon_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Gay_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_GenStreet_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_GenStreet_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hippy_01_white_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hipster_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hipster_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Hipster_03_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_MusclBeac_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Skater_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_SouCent_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_SouCent_03_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_SouCent_04_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StBla_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StBla_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StWhi_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_StWhi_02_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Sunbathe_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Sunbathe_01_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_02_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_03_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Vinewood_04_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Business_03_Chinese_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Beach_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_EastSA_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_MusclBeac_02_Chinese_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_EastSA_02_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_SouCent_03_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_FatLatin_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Malibu_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Gay_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_GenStreet_02_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_EastSA_02_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Beach_01_CHINESE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_GenStreet_01_Chinese_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_SouCent_01_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_M_SouCent_02_Black_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Vinewood_02_White_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_SoCenLat_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_TranVest_02_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_MexThug_01_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_Y_Vinewood_04_White_FULL_01")
//		DISPLAY_VOICE_HASH("TIME_OF_DEATH_TEST")
//		DISPLAY_VOICE_HASH("A_F_M_Ktown_02_KOREAN_FULL_01")
//		DISPLAY_VOICE_HASH("A_F_O_Ktown_01_Korean_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Ktown_01_Korean_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_Polynesian_01_Polynesian_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_M_StLat_02_Latino_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_BeachVesp_01_CHINESE_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Epsilon_01_Korean_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_Ktown_01_Korean_FULL_01")
//		DISPLAY_VOICE_HASH("A_M_Y_KTown_02_Korean_FULL_01")
//		DISPLAY_VOICE_HASH("PANIC_WALLA")
//		DISPLAY_VOICE_HASH("G_M_Y_Lost_03_WHITE_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_BallaOrig_01_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_BallaEast_01_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_BallaOrig_01_BLACK_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_BallaSout_01_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_FamDNF_01_BLACK_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_Lost_01_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_Lost_02_LATINO_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_SalvaGoon_01_SALVADORIAN_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_SalvaGoon_01_SALVADORIAN_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_SalvaGoon_02_SALVADORIAN_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_SalvaGoon_02_SALVADORIAN_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_SalvaGoon_03_SALVADORIAN_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_StreetPunk_01_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_StreetPunk_01_BLACK_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_StreetPunk_02_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_BallaEast_01_BLACK_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_BallaSout_01_BLACK_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_FamDNF_01_BLACK_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_Lost_01_BLACK_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_Lost_02_LATINO_MINI_01")
//		DISPLAY_VOICE_HASH("G_M_Y_SalvaGoon_03_SALVADORIAN_MINI_02")
//		DISPLAY_VOICE_HASH("G_M_Y_StreetPunk_02_BLACK_MINI_02")
//		DISPLAY_VOICE_HASH("FRANKLIN_NORMAL")
//		DISPLAY_VOICE_HASH("MICHAEL_NORMAL")
//		DISPLAY_VOICE_HASH("TREVOR_NORMAL")
//}

void audSpeechAudioEntity::MakeSelectedPedConvTester(CPed* pSelectedEntity)
{
	sm_CanHaveConvTestPed = pSelectedEntity;
}

void audSpeechAudioEntity::ConvTesterCB()
{
	fwEntity *pSelectedEntity = g_PickerManager.GetSelectedEntity();
	if(!pSelectedEntity || pSelectedEntity->GetType() != ENTITY_TYPE_PED)
		return;
	CPed* converser = static_cast<CPed*>(pSelectedEntity);
	if(converser && sm_CanHaveConvTestPed)
	{
		if(converser->GetSpeechAudioEntity()->CanPedHaveChatConversationWithPed(sm_CanHaveConvTestPed, atPartialStringHash("CHAT_STATE"), atPartialStringHash("CHAT_RESP"), true))
			Displayf("Conv test passed");
		else
			Displayf("Conv test failed");
	}
}

void MakeSelectedPedConvTesterCB()
{
	fwEntity *pSelectedEntity = g_PickerManager.GetSelectedEntity();
	if(!pSelectedEntity || pSelectedEntity->GetType() != ENTITY_TYPE_PED)
		return;
	CPed* converser = static_cast<CPed*>(pSelectedEntity);
	if(converser)
	{
		audSpeechAudioEntity::MakeSelectedPedConvTester(converser);
	}
}

extern bool g_StressTestSpeech;
extern u32 g_StressTestSpeechVariation;
extern char g_StressTestSpeechVoiceName[128];
extern char g_StressTestSpeechContext[128];
extern u32 g_StressTestMaxStartOffset;
extern bool g_PrintVoiceRefInfo;
void audSpeechAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("audSpeechAudioEntity",false);
		bank.PushGroup("Debug Playback");
			bank.AddText("Test Voice", &g_TestVoice[0], sizeof(g_TestVoice));
			bank.AddText("Test Context", &g_TestContext[0], sizeof(g_TestContext));
			bank.AddToggle("Make Ped Speak", &g_MakePedSpeak);
			bank.AddToggle("g_ShowSpeechRoute", &g_ShowSpeechRoute);
			bank.AddToggle("g_ShowSpeechPositions", &g_ShowSpeechPositions);
			bank.AddToggle("g_MakePedDoMegaphone", &g_MakePedDoMegaphone);
			bank.AddToggle("g_MakePedDoCoverMe", &g_MakePedDoCoverMe);
			bank.AddToggle("g_MakePedDoHeli", &g_MakePedDoHeli);
			bank.AddToggle("g_MakePedDoBlocked", &g_MakePedDoBlocked);
			bank.AddToggle("g_MakePedDoPanic", &g_MakePedDoPanic);
			bank.AddToggle("g_TestWallaSound", &g_TestWallaSound);
			bank.AddToggle("g_RunCanChatTest", &g_RunCanChatTest);
			bank.AddToggle("g_TriggeredContextTest", &g_TriggeredContextTest);
			bank.AddToggle("g_PreloadOnly", &g_PreloadOnly);
			bank.AddToggle("g_TriggerPreload", &g_TriggerPreload);
			bank.AddCombo("SpeechType", &g_SpeechTypeOverride, AUD_NUM_SPEECH_TYPES, g_SpeechTypeNames);
			bank.AddToggle("Override Speech Type", &g_ShouldOverrideSpeechType);
			bank.AddCombo("Speech Volume Type", &g_SpeechVolumeTypeOverride, AUD_NUM_SPEECH_VOLUMES, g_SpeechVolNames);
			bank.AddToggle("Override Volume Type", &g_ShouldOverrideSpeechVolumeType);
			bank.AddCombo("Speech Audibility", &g_SpeechAudibilityOverride, AUD_NUM_AUDIBILITIES, g_ConvAudNames);
			bank.AddToggle("Override Audibility", &g_ShouldOverrideSpeechAudibility);
			bank.AddToggle("IsInCarFrontEndScene", &g_DebugIsInCarFEScene);
			bank.AddToggle("DebugVolumeMetrics", &g_DebugVolumeMetrics);
		bank.PopGroup();
		bank.PushGroup("Headset Distortion");
			bank.AddSlider("Default Distortion", &g_HeadsetDistortionDefault, 0.0f, 1.0f, 0.1f);
			bank.AddSlider("Reduced Distortion", &g_HeadsetDistortionReduced, 0.0f, 1.0f, 0.1f);
		bank.PopGroup();
		bank.PushGroup("Laughter Track");
			bank.AddToggle("Debug Draw TV Laughter Track", &g_DebugDrawLaughterTrack);
			bank.AddButton("Start Editing Current Show", datCallback(CFA(StartLaughterTrackEditor)));
			bank.AddButton("Stop Editing Show", datCallback(CFA(StopLaughterTrackEditor)));
			bank.AddButton("Save Laughter Track", datCallback(CFA(SaveLaughterTrack)));
			bank.AddSlider("Editor Scroll", &g_LaughterTrackEditorScroll, 0.0f, 1.0f, 0.1f);
			bank.AddSeparator("TV Channels");
			const char *channelNames[] = { "None", "Channel 1", "Channel 2", "Channel Script" };
			bank.AddCombo("Force Channel", &g_LaughterTrackChannelSelection, NELEM(channelNames), channelNames, datCallback(SetChannelCB), "Current TV Channel");
			bank.AddButton("Prev Show", CTVPlaylistManager::PrevShowCB);
			bank.AddButton("Next Show", CTVPlaylistManager::NextShowCB);
		bank.PopGroup();
		bank.AddToggle("Turn on VFX Info", &g_TurnOnVFXInfo);
		bank.AddToggle("Turn on Ambient Speech Info", &g_TurnOnAmbSpeechInfo);
		bank.AddToggle("Turn on Lipsync Timing Info", &g_DisplayLipsyncTimingInfo);
		bank.AddToggle("g_ShortenCompleteSay", &g_ShortenCompleteSay);
		bank.AddToggle("g_EnableSilentSpeech", &g_EnableSilentSpeech);
		bank.AddToggle("g_ForceToughCopVoice", &g_ForceToughCopVoice);
		bank.AddToggle("g_FullSpeechPVG", &g_FullSpeechPVG);
		bank.AddToggle("g_EnableEarlyStoppingForUrgentSpeech", &g_EnableEarlyStoppingForUrgentSpeech);
		bank.AddSlider("g_TimeToStopEarlyForUrgentSpeech", &g_TimeToStopEarlyForUrgentSpeech, 0, 10000, 1);
		bank.AddSlider("g_SpeechPlayedEnoughNotToRepeatRatio", &g_SpeechPlayedEnoughNotToRepeatTime, 0, 10000, 1);
		bank.AddSlider("g_TimeBetweenShotScreams", &g_TimeBetweenShotScreams, 0, 30000, 1);
		bank.AddToggle("g_PretendThePlayerIsPissedOff", &g_PretendThePlayerIsPissedOff);
		bank.AddToggle("g_PedsAreBlindRagingOverride", &g_PedsAreBlindRagingOverride);
		bank.AddToggle("g_DisplayVoiceInfo", &g_DisplayVoiceInfo);
		bank.AddToggle("g_DisplayVoiceInfoForAllPeds", &g_DisplayVoiceInfoForAllPeds);
		bank.AddSlider("g_DisplayVoiceInfoDistance", &g_DisplayVoiceInfoDistance, 0.0f, 100.0f, 1.0f);
		bank.AddButton("Play Anim Test Player Speech", datCallback(CFA(PlaySpeechCallback)));
		bank.AddSlider("Speech Variation", &g_AnimTestSpeechVariation, 0, 20, 1);
		bank.AddToggle("Disable Ambient Speech", &g_DisableAmbientSpeech);
		bank.AddToggle("Bypass Streaming Check For Speech", &g_BypassStreamingCheckForSpeech);
		bank.AddToggle("Enable viseme processing", &ms_ShouldProcessVisemes);
		bank.AddToggle("Allow player to speak", &g_AllowPlayerToSpeak);
		bank.AddSlider("Max dist to play speech", &g_MaxDistanceToPlaySpeech, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Max dist to play speech few slots", &g_MaxDistanceToPlaySpeechIfFewSlotsInUse, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Max dist to play cop speech", &g_MaxDistanceToPlayCopSpeech, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Max dist to play pain", &g_MaxDistanceToPlayPain, 0.0f, 100.0f, 1.0f);
		bank.AddSlider("g_MinDistForLongDeath", &g_MinDistForLongDeath, 0.0f, 100.0f, 1.0f);
		bank.AddSlider("g_MaxDistForShortDeath", &g_MaxDistForShortDeath, 0.0f, 100.0f, 1.0f);
		bank.AddSlider("g_HeadshotDeathGargleProb", &g_HeadshotDeathGargleProb, 0.0f, 1.0f, 0.01f);
		bank.AddToggle("g_GiveEveryoneConversationVoices", &g_GiveEveryoneConversationVoices);
		bank.AddToggle("g_UseSpeedGesturingVoices", &g_UseSpeedGesturingVoices);
		bank.AddToggle("g_StreamingSchmeaming", &g_StreamingSchmeaming);
		bank.AddToggle("g_IgnoreBeingInPlayerCar", &g_IgnoreBeingInPlayerCar);
		bank.AddSlider("g_MaxDepthToHearSpeech", &g_MaxDepthToHearSpeech, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("g_MinUnderwaterLinLPF", &g_MinUnderwaterLinLPF, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_UnderwaterReverbWet", &g_UnderwaterReverbWet, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_SpeechOffsetForVisemesMs", &g_SpeechOffsetForVisemesMs, 0, 2000, 10);
		bank.AddToggle("Force Allow Gestures", &g_ForceAllowGestures);
		//bank.AddSlider("g_CoughProb", &g_CoughProb, 1.0f, 200.0f, 1.0f);
		//bank.AddSlider("g_LaughProb", &g_LaughProb, 1.0f, 200.0f, 1.0f);
		bank.AddSlider("g_MinTimeBetweenLaughs", &g_MinTimeBetweenLaughs, 10000, 240000, 100);
		bank.AddSlider("g_MaxTimeBetweenLaughs", &g_MaxTimeBetweenLaughs, 10000, 240000, 100);
		bank.AddSlider("g_MinTimeBetweenCoughs", &g_MinTimeBetweenCoughs, 10000, 240000, 100);
		bank.AddSlider("g_MaxTimeBetweenCoughs", &g_MaxTimeBetweenCoughs, 10000, 240000, 100);
		bank.AddSlider("g_LaughCoughMinDistSq", &g_LaughCoughMinDistSq, 1.0f, 40000.0f, 1.0f);
		bank.AddSlider("g_LaughCoughMaxDistSq", &g_LaughCoughMaxDistSq, 1.0f, 40000.0f, 1.0f);
		bank.AddSlider("g_VolumeIncreaseForPlaceholder", &g_VolumeIncreaseForPlaceholder, -100.0f, 100.0f, 1.0f);
		bank.AddButton("Make selected ped can have conv tester", CFA(MakeSelectedPedConvTesterCB));
		bank.AddButton("Can have conv tester", CFA(audSpeechAudioEntity::ConvTesterCB));
		bank.PushGroup("Runned Over Mid-Distance Reaction",false);
			bank.AddSlider("g_MinDistanceSqForRunnedOverScream", &g_MinDistanceSqForRunnedOverScream, 1.0f, 40000.0f, 1.0f);
			bank.AddSlider("g_MaxDistanceSqForRunnedOverScream", &g_MaxDistanceSqForRunnedOverScream, 1.0f, 40000.0f, 1.0f);
			bank.AddSlider("g_NumScreamsForRunnedOverReaction", &g_NumScreamsForRunnedOverReaction, 1, 5, 1);
			bank.AddSlider("g_MinPedsHitForRunnedOverReaction", &g_MinPedsHitForRunnedOverReaction, 1, 10, 1);
			bank.AddSlider("g_TimespanToHitPedForRunnedOverReaction", &g_TimespanToHitPedForRunnedOverReaction, 1000, 20000, 1);
			bank.AddSlider("g_MinTimeBetweenLastCollAndRunnedOverReaction", &g_MinTimeBetweenLastCollAndRunnedOverReaction, 0, 20000, 1);
			bank.AddSlider("g_TimeToWaitBetweenRunnedOverReactions", &g_TimeToWaitBetweenRunnedOverReactions, 0, 200000, 1);
			bank.AddSlider("g_RunnedOverScreamPredelayVariance", &g_RunnedOverScreamPredelayVariance, 0.0f, 200000.0f, 10.0f);
		bank.PopGroup();
		bank.PushGroup("Panic",false);
			bank.AddSlider("MinPanicSpeechPredelay", &g_MinPanicSpeechPredelay, 0, 10000, 100);
			bank.AddSlider("MaxPanicSpeechPredelay", &g_MaxPanicSpeechPredelay, 0, 10000, 100);
		bank.PopGroup();
		bank.PushGroup("Pain",false);
			bank.PushGroup("Specific Pain", false);
				bank.AddToggle("g_ActivatePainSpecificBankSwapping", &g_ActivatePainSpecificBankSwapping);
				bank.AddToggle("g_DrawSpecificPainInfo", &g_DrawSpecificPainInfo);
				bank.AddToggle("g_OverrideSpecificPainLoaded", &g_OverrideSpecificPainLoaded);
				bank.AddSlider("g_OverrideSpecificPainLevel", &g_OverrideSpecificPainLevel, 0, 2, 1);
				bank.AddToggle("g_OverrideSpecificPainIsMale", &g_OverrideSpecificPainIsMale);
				bank.AddSlider("g_OverridePainVsAimScalar", &g_OverridePainVsAimScalar, 0, 100, 1);
				bank.AddToggle("g_ForceSpecificPainBankSwap", &g_ForceSpecificPainBankSwap);
				bank.AddButton("ResetStats", datCallback(CFA(ResetSpecPainStatsCallback)));
			bank.PopGroup();
			bank.AddToggle("g_DisablePain", &g_DisablePain);
			bank.AddToggle("g_UseTestPainBanks", &g_UseTestPainBanks);
			bank.AddToggle("g_DrawPainPlayedTotals", &g_DrawPainPlayedTotals);
			bank.AddToggle("g_MakeFocusPedCry", &g_MakeFocusPedCry);
			bank.AddText("Crying context", &g_AlternateCryingContext[0], sizeof(g_AlternateCryingContext));
			bank.AddSlider("g_MinTimeBetweenCries", &g_MinTimeBetweenCries, 0, 20000, 1);
			bank.AddSlider("g_MaxTimeBetweenCries", &g_MaxTimeBetweenCries, 0, 20000, 1);
			bank.AddSlider("g_MaxPedsCrying", &g_MaxPedsCrying, 0, 10, 1);
			bank.AddSlider("g_HealthLostForHighDeath", &g_HealthLostForHighDeath, 0.0f, 200.0f, 1.0f);
			bank.AddSlider("g_HealthLostForHighPain", &g_HealthLostForHighPain, 0.0f, 200.0f, 1.0f);
			bank.AddSlider("g_HealthLostForMediumPain", &g_HealthLostForMediumPain, 0.0f, 200.0f, 1.0f);
			bank.AddSlider("g_HealthLostForLowPain", &g_HealthLostForLowPain, 0.0f, 200.0f, 1.0f);
			bank.AddSlider("g_DamageScalarDiffMax", &g_DamageScalarDiffMax, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_RunOverByVehicleDamageScalar", &g_RunOverByVehicleDamageScalar, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("g_ProbabilityOfDeathDrop", &g_ProbabilityOfDeathDrop, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_ProbabilityOfDeathRise", &g_ProbabilityOfDeathRise, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_ProbabilityOfPainDrop", &g_ProbabilityOfPainDrop, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_ProbabilityOfPainRise", &g_ProbabilityOfPainRise, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_TimeBetweenPain", &g_TimeBetweenPain, 0, 5000, 1);
			bank.AddToggle("g_EnablePainedBreathing", &g_EnablePainedBreathing);
			bank.AddSlider("g_TimeBetweenPainInhaleAndExhale", &g_TimeBetweenPainInhaleAndExhale, 0, 5000, 1);
			bank.AddSlider("g_TimeBetweenPainExhaleAndInhale", &g_TimeBetweenPainExhaleAndInhale, 0, 5000, 1);
			bank.AddSlider("g_MinHeightForHighFallContext", &g_MinHeightForHighFallContext, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("g_MinHeightForSuperHighFallContext", &g_MinHeightForSuperHighFallContext, 0.0f, 100.0f, 1.0f);
			bank.AddToggle("g_TestFallingVoice", &g_TestFallingVoice);
			bank.AddSlider("g_FallingScreamReleaseTime", &g_FallingScreamReleaseTime, 0, 1000, 20);
			bank.AddSlider("g_FallingScreamVelocityThresh", &g_FallingScreamVelocityThresh, 0.0f, 10.0f, 0.01f);
			bank.AddSlider("g_FallingScreamMinVelocityWithNoGroundProbe", &g_FallingScreamMinVelocityWithNoGroundProbe, 0.0f, 100.0f, 0.01f);
			bank.AddToggle("g_UseDelayedFallProbe", &g_UseDelayedFallProbe);
			bank.AddSlider("g_MinTimeBetweenLungDamageCough", &g_MinTimeBetweenLungDamageCough, 0, 20000, 1);
			bank.AddSlider("g_MaxTimeBetweenLungDamageCough", &g_MaxTimeBetweenLungDamageCough, 0, 20000, 1);
		bank.PopGroup();
		bank.PushGroup("Player Breathing", false);
			bank.AddToggle("g_DisablePlayerBreathing", &g_DisablePlayerBreathing);
			bank.AddToggle("g_EnableBreathingSpew", &g_EnableBreathingSpew);
			bank.AddSlider("g_TimeRunningBeforeBreathing", &g_TimeRunningBeforeBreathing, 0, 20000, 10);
			bank.AddSlider("g_RunStartVolRampUpTimeMs", &g_RunStartVolRampUpTimeMs, 0.0f, 20000.0f, 10.0f);
			bank.AddSlider("g_JogVolRampDownTimeMs1", &g_JogVolRampDownTimeMs1, 1.0f, 20000.0f, 10.0f);
			bank.AddSlider("g_JogVolRampDownTimeMs2", &g_JogVolRampDownTimeMs2, 1.0f, 20000.0f, 10.0f);
			bank.AddSlider("g_MinTimeBetweenJogLowBreath", &g_MinTimeBetweenJogLowBreath, 0, 10000, 1);
			bank.AddSlider("g_MaxTimeBetweenJogLowBreath", &g_MaxTimeBetweenJogLowBreath, 0, 10000, 1);
			bank.AddSlider("g_MinPredelayOnBreath", &g_MinPredelayOnBreath, 0, 10000, 1);
			bank.AddSlider("g_MaxPredelayOnBreath", &g_MaxPredelayOnBreath, 0, 10000, 1);
			bank.AddSlider("g_TimeJoggingBeforeLeavingRunBreathing", &g_TimeJoggingBeforeLeavingRunBreathing, 0, 10000, 1);
			bank.AddSlider("g_TimePedalingBeforeBreathing", &g_TimePedalingBeforeBreathing, 0, 20000, 1);
			bank.AddSlider("g_BikeStartVolRampUpTimeMs", &g_BikeStartVolRampUpTimeMs, 1.0f, 20000.0f, 10.0f);
			bank.AddSlider("g_MinPredelayOnBikeBreath", &g_MinPredelayOnBikeBreath, 0, 10000, 1);
			bank.AddSlider("g_MaxPredelayOnBikeBreath", &g_MaxPredelayOnBikeBreath, 0, 10000, 1);
			bank.AddSlider("g_TimeNotPedalingBeforeLeavingBikeBreathing", &g_TimeNotPedalingBeforeLeavingBikeBreathing, 0, 10000, 1);
			bank.AddSlider("g_TimeToPedalBeforeCycleExhale", &g_TimeToPedalBeforeCycleExhale, 0, 60000, 1);
			bank.AddSlider("g_BikeSlopeAngle", &g_BikeSlopeAngle, 0.0f, 90.0f, 10.0f);
			bank.AddSlider("g_TimeBetweenScubaBreaths", &g_TimeBetweenScubaBreaths, 0, 5000, 2000);
		bank.PopGroup();
		bank.PushGroup("Fake Gestures",false);
			bank.AddToggle("g_ForceFakeGestures", &g_ForceFakeGestures);
			bank.AddText("Name", &g_FakeGestureName);
			bank.AddSlider("Marker Time", &g_FakeGestureMarkerTime, 0, 3000, 1);
			bank.AddSlider("Start Phase", &g_FakeGestureStartPhase, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("End Phase", &g_FakeGestureEndPhase, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Rate", &g_FakeGestureRate, -1.0f, 5.0f, 0.01f);
			bank.AddSlider("Max Weight", &g_FakeGestureMaxWeight, -1.0f, 5.0f, 0.01f);
			bank.AddSlider("Blend In Time", &g_FakeGestureBlendInTime, -1.0, 5000.0f, 0.01f);
			bank.AddSlider("Blend Out Time", &g_FakeGestureBlendOutTime, -1.0, 5000.0f, 0.01f);
		bank.PopGroup();
		bank.PushGroup("Animals",false);
			bank.AddToggle("g_TestAnimalBankPriority", &g_TestAnimalBankPriority);
			bank.AddToggle("g_ForceAnimalMoodAngry", &g_ForceAnimalMoodAngry);
			bank.AddToggle("g_ForceAnimalMoodPlayful", &g_ForceAnimalMoodPlayful);
			bank.AddToggle("g_DisplayAnimalBanks", &g_DisplayAnimalBanks);
			bank.AddToggle("g_ForceAnimalBankUpdate", &g_ForceAnimalBankUpdate);
			bank.AddSlider("g_AnimalPainPredelay", &g_AnimalPainPredelay, 0, 2000, 10);
			bank.AddToggle("g_DisablePanting", &g_DisablePanting);
			bank.AddToggle("g_DebugTennisLoad", &g_DebugTennisLoad);
			bank.AddToggle("g_DebugTennisUnload", &g_DebugTennisUnload);
		bank.PopGroup();
		bank.PushGroup("AI Triggered Contexts");
			bank.PushGroup("Jacking");
				bank.AddSlider("g_JackingPedsResponseDelay", &g_JackingPedsResponseDelay, -1, 15000, 10);
				bank.AddSlider("g_JackingPedsFirstLineDelay", &g_JackingPedsFirstLineDelay, -1, 15000, 10);
				bank.AddSlider("g_JackingPedsFirstLineDelayPassSide", &g_JackingPedsFirstLineDelayPassSide, -1, 15000, 10);
				bank.AddSlider("g_JackingVictimResponseDelay", &g_JackingVictimResponseDelay, -1, 15000, 10);
				bank.AddSlider("g_JackingVictimFirstLineDelay", &g_JackingVictimFirstLineDelay, -1, 15000, 10);
				bank.AddSlider("g_JackingVictimFirstLineDelayPassSide", &g_JackingVictimFirstLineDelayPassSide, -1, 15000, 10);
				bank.AddSlider("g_MinTimeForJackingReply", &g_MinTimeForJackingReply, -1, 15000, 10);
				bank.AddToggle("g_ForcePlayerThenPedJackingSpeech", &g_ForcePlayerThenPedJackingSpeech);
				bank.AddToggle("g_ForcePedThenPlayerJackingSpeech", &g_ForcePedThenPlayerJackingSpeech);
			bank.PopGroup();
		bank.PopGroup();
		bank.PushGroup("Volume and rolloff",false);
			for(u32 i = 0; i < AUD_NUM_AUDIBILITIES; i++)
			{
				char buf[255];
				formatf(buf, "g_AudibilityVolOffset %s", g_ConvAudNames[i]);
				bank.AddSlider(buf, &g_AudibilityVolOffset[i], -100.0f, 100.0f, 0.1f);
			}
			for(u32 i = 0; i < AUD_NUM_SPEECH_VOLUMES; i++)
			{
				char buf[255];
				formatf(buf, "g_VolTypeVolOffset %s", g_SpeechVolNames[i]);
				bank.AddSlider(buf, &g_VolTypeVolOffset[i], -100.0f, 100.0f, 0.1f);
			}
			for(u32 i = 0; i < AUD_NUM_SPEECH_VOLUMES; i++)
			{
				char buf[255];
				formatf(buf, "g_VolTypeEnvLoudness %s", g_SpeechVolNames[i]);
				bank.AddSlider(buf, &g_VolTypeEnvLoudness[i], 0.0f, 1.0f, 0.01f);
			}
			bank.AddSlider("g_PlayerInCarVolumeOffset", &g_PlayerInCarVolumeOffset, -100.0f, 100.0f, 0.1f);
			bank.AddSlider("g_PlayerOnFootAmbientVolumeOffset", &g_PlayerOnFootAmbientVolumeOffset, -100.0f, 100.0f, 0.1f);
			bank.AddSlider("g_BreathingRolloff", &g_BreathingRolloff, -100.0f, 100.0f, 0.1f);
			bank.AddSlider("g_BreathingRolloffGunfire", &g_BreathingRolloffGunfire, -100.0f, 100.0f, 0.1f);
			bank.AddToggle("g_DisableFESpeechBubble", &g_DisableFESpeechBubble);
			bank.AddToggle("g_DrawFESpeechBubble", &g_DrawFESpeechBubble);
			bank.AddToggle("g_UseHPFOnLeadIn", &g_UseHPFOnLeadIn);
			bank.AddSlider("g_SpeakerConeVolAttenLin", &g_SpeakerConeVolAttenLin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_SpeakerConeLPF", &g_SpeakerConeLPF, 0.0f, 23900.0f, 1.0f);
			bank.AddSlider("g_SpeakerConeAngleInner", &g_SpeakerConeAngleInner, 0.0f, 180.0f, 0.1f);
			bank.AddSlider("g_SpeakerConeAngleOuter", &g_SpeakerConeAngleOuter, 0.0f, 180.0f, 0.1f);
			bank.AddSlider("g_SpeakerConeDistForZeroStrength", &g_SpeakerConeDistForZeroStrength, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("g_SpeakerConeDistForFullStrength", &g_SpeakerConeDistForFullStrength, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("g_ListenerConeVolAttenLin", &g_ListenerConeVolAttenLin, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_ListenerConeLPF", &g_ListenerConeLPF, 0.0f, 23900.0f, 1.0f);
			bank.AddSlider("g_ListenerConeAngleInner", &g_ListenerConeAngleInner, 0.0f, 180.0f, 0.1f);
			bank.AddSlider("g_ListenerConeAngleOuter", &g_ListenerConeAngleOuter, 0.0f, 180.0f, 0.1f);
			bank.AddSlider("Volume smooth rate", &g_SpeechVolumeSmoothRate, 0.0f, 1.0f, 0.001f);
			bank.AddSlider("Linear Volume smooth rate", &g_SpeechLinVolumeSmoothRate, 0.0f, 1.0f, 0.0001f);
			bank.AddSlider("Filter smooth rate", &g_SpeechFilterSmoothRate, 0.0f, 1000.0f, 0.001f);
		bank.PopGroup();
		bank.PushGroup("Echos", false);
			bank.AddSlider("g_EchoDryLevel", &g_EchoDryLevel, 0.0f, 1.0f, 0.01f);
			bank.AddToggle("g_EchoSolo", &g_EchoSolo);
			bank.AddToggle("g_DebugEchoes", &g_DebugEchoes);
			bank.AddSlider("g_SpeechEchoPredelayScalar", &g_SpeechEchoPredelayScalar, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("g_SpeechEchoVolShouted", &g_SpeechEchoVolShouted, -100.0f, 6.0f, 0.01f);
			bank.AddSlider("g_SpeechEchoVolMegaphone", &g_SpeechEchoVolMegaphone, -100.0f, 6.0f, 0.01f);
			bank.AddSlider("g_SpeechEchoMinPredelayDiff", &g_SpeechEchoMinPredelayDiff, 0, 1000, 1);
		bank.PopGroup();
	bank.PopGroup();
	bank.PushGroup("audSpeechManager",false);
		bank.AddToggle("g_DrawSpeechSlots", &g_DrawSpeechSlots);
		bank.AddToggle("g_EnablePlaceholderDialogue", &g_EnablePlaceholderDialogue);
		//bank.AddToggle("g_ChurnSpeech", &g_ChurnSpeech);
		bank.AddSlider("g_TimeBeforeChurningSpeech", &g_TimeBeforeChurningSpeech, 0, 20000, 1);
		bank.AddSlider("g_StreamingBusyTime", &g_StreamingBusyTime, 0, 20000, 1);
		bank.AddSlider("g_SpeechRolloffWhenInACar", &g_SpeechRolloffWhenInACar, 0.0f, 100.0f, 0.1f);
		bank.AddToggle("g_PrintUnknownContexts", &g_PrintUnknownContexts);
		bank.AddToggle("g_ListPlayedAmbientContexts", &g_ListPlayedAmbientContexts);
		bank.AddToggle("g_ListPlayedScriptedContexts", &g_ListPlayedScriptedContexts);
		bank.AddToggle("g_ListPlayedPainContexts", &g_ListPlayedPainContexts);
		bank.AddToggle("g_ListPlayedAnimalContexts", &g_ListPlayedAnimalContexts);
		bank.AddToggle("SpeechStressTest", &g_StressTestSpeech);
		bank.AddToggle("g_PrintVoiceRefInfo", &g_PrintVoiceRefInfo);
		bank.AddText("StressVoice", &g_StressTestSpeechVoiceName[0], sizeof(g_StressTestSpeechVoiceName));
		bank.AddText("StressContext", &g_StressTestSpeechContext[0], sizeof(g_StressTestSpeechContext));
		bank.AddSlider("StressVariation", &g_StressTestSpeechVariation, 0, ~0U, 1);
		bank.AddSlider("StressMaxSO", &g_StressTestMaxStartOffset, 0, 5000, 1);
	bank.PopGroup();
}
#endif
