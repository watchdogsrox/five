//  
// audio/pedaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
 
#include "phbound/boundcomposite.h"
#include "gameobjects.h"
#include "northaudioengine.h"
#include "glassaudioentity.h"
#include "hashactiontable.h"
#include "frontendaudioentity.h"
#include "pedaudioentity.h"
#include "radioslot.h"
#include "scriptaudioentity.h"
#include "speechaudioentity.h"
#include "speechmanager.h"
#include "collisionaudioentity.h"
#include "weatheraudioentity.h"
#include "caraudioentity.h"
#include "vehicleaudioentity.h"
#include "vehiclecollisionaudio.h"
 
#include "animation/animmanager.h"
#include "audio/music/musicplayer.h"
#include "audioengine/categorymanager.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiohardware/driverutil.h"
#include "audiosoundtypes/envelopesound.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/sound.h"
#include "task/Combat/TaskCombatMelee.h"
#include "camera/CamInterface.h"
#include "control/replay/Replay.h"
#include "control/replay/ReplayExtensions.h"
#include "control/replay/audio/CollisionAudioPacket.h"
#include "control/replay/audio/footsteppacket.h"
#include "control/replay/audio/SoundPacket.h"
#include "debugaudio.h"
#include "frontend/MiniMap.h"
#include "fwdebug/picker.h"
#include "grcore/debugdraw.h"
#include "game/ModelIndices.h"
#include "game/weather.h"
#include "game/wind.h"
#include "bank/bank.h"
#include "modelinfo/PedModelInfo.h"
#include "music/musicplayer.h"
#include "system/pad.h"
#include "parser/manager.h"
#include "peds/action/actionmanager.h"
#include "peds/HealthConfig.h"
#include "peds/ped.h"
#include "Peds/PedFootstepHelper.h"
#include "peds/pedintelligence.h"
#include "peds/pedintelligence/PedAILod.h"
#include "Peds/rendering/pedVariationPack.h"
#include "peds/rendering/pedvariationds.h"
#include "peds/PedHelmetComponent.h"
#include "peds/PedMoveBlend/PedmoveBlendOnFoot.h"
#include "peds/PedPhoneComponent.h"
#include "peds/PedDebugVisualiser.h"
#include "physics/GtaInst.h"
#include "physics/gtaMaterialManager.h"
#include "physics/physics.h"
#include "grprofile/drawmanager.h"
#include "scene/world/GameWorldHeightMap.h"
#include "system/controlMgr.h"
#include "system/timer.h"
#include "task/Movement/Climbing/TaskVault.h"
#include "task/Movement/TaskParachute.h"
#include "task/Movement/Climbing/TaskRappel.h"
#include "task/Weapons/Gun/TaskGun.h"
#include "vehicles/vehicle.h"
#include "vehicles/Boat.h"
#include "vfx/systems/vfxped.h"
#include "weapons/Weapon.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/Info/ScenarioInfo.h"
#include "pedscenariomanager.h"

AUDIO_PEDS_OPTIMISATIONS()

PFD_DECLARE_GROUP(Audio);
PFD_DECLARE_SUBGROUP(Creature, Audio);
EXT_PFD_DECLARE_ITEM_SLIDER(physAudioMaxDistance);
PFD_DECLARE_ITEM(locoGaitType, Color32(0.f,1.f,1.f,1.f), Creature);
PFD_DECLARE_ITEM(footBoneUp, Color32(1.f,0.f,0.f,1.f), Creature);
PFD_DECLARE_ITEM(footBoneDown, Color32(0.f,0.f,1.f,1.f), Creature);
PFD_DECLARE_ITEM(footBoneDragging, Color32(1.f,1.f,0.f,1.f), Creature);
PFD_DECLARE_ITEM(footBonePlanted, Color32(1.f,0.f,1.f,1.f), Creature);
PFD_DECLARE_ITEM(footBoneMoving, Color32(1.f,0.5f,0.f,1.f), Creature);
PFD_DECLARE_ITEM(groundPos, Color32(0.f,1.f,0.f,1.f), Creature);
PFD_DECLARE_ITEM(groundNorm, Color32(0.f,1.f,0.f,1.f), Creature);
//PFD_DECLARE_ITEM(groundMaterial, Color32(1.f,1.f,1.f,1.f), Creature);
PFD_DECLARE_ITEM(audioMaterial, Color32(1.f,1.f,1.f,1.f), Creature);
PFD_DECLARE_ITEM(footSpeed, Color32(1.f,0.f,0.f,1.f), Creature);
PFD_DECLARE_ITEM(footGroundDist, Color32(1.f,0.f,0.f,1.f), Creature);
PFD_DECLARE_ITEM(boneTranslationLength, Color32(1.f,0.f,0.f,1.f), Creature);
PFD_DECLARE_ITEM(headToMouthOffset, Color32(1.f,0.f,0.f,1.f), Creature);

PF_PAGE(PedAudioEntityPage, "PedAudioEntity");
PF_GROUP(PedAudioEntityGroup);
PF_LINK(PedAudioEntityPage, PedAudioEntityGroup);
PF_COUNTER(PedEnvGroups, PedAudioEntityGroup);

XPARAM(audiodesigner);

bank_u32 g_audPedAirTimeForBodyImpacts = 500;
bank_float g_AudPedVelForLoudImpacts = 3.f;
bank_u32 g_audPedAirTimeForBigDeathImpacts = 1000;
float g_DeathImpactMagScale = 2.f;
u32 g_audPedTimeForShootingDeathImpact = 3000;
u32 g_TimeForCarJackCollision = 10000;
u32 g_PedScrapeDebrisTime = 500;

const u32 g_RingtoneVoiceNameHash = ATSTRINGHASH("RINGTONES", 0x01425ae94);
const u32 g_DLCRingtoneVoiceNameHash = ATSTRINGHASH("DLC_RINGTONES", 0x0aa349ade);

bank_bool g_DisableHeartbeat = false;
bank_bool g_DisableHeartbeatFromInjury = true;
bank_bool g_PrintHeartbeatApply = false;
bank_u32 g_TimeNotSprintingBeforeHeartbeatStops = 4000;
bank_u32 g_TimeNotHurtBeforeHeartbeatStops = 8000;

u32 g_LastVehicleContactTimeForForcedRagdollImpact = 100;
u32 g_MinMeleeRepeatTime = 200;

BANK_ONLY(bank_float g_audStairsDeltaZThreshold = 0.1f);
BANK_ONLY(bool g_DebugPedVehicleOverUnderSounds = false;)

u32 audPedAudioEntity::sm_PedImpactTimeFilter = 100;

f32 g_PelvisHeightForSecondStaggerGrunt = 0.4f;

u32 audPedAudioEntity::sm_BirdTakeOffWriteIndex;
atRangeArray<audBirdTakeOffStruct, AUD_NUM_BIRD_TAKE_OFFS_ARRAY_SIZE> audPedAudioEntity::sm_BirdTakeOffArray;
u32 audPedAudioEntity::sm_TimeOfLastBirdTakeOff = 0;

static u32 g_MaxMolotovVelocityRangeTimeMs = 30;

u16 audPedAudioEntity::sm_LodAnimLoopTimeout = 500;


audCurve g_PedImpulseToBoneBreakProbCurve;
audCurve g_PedVehImpactBoneBreakProbCurve;
audCurve g_PedVehImpulseToImpactVolCurve;
audCurve g_PedVehOverUnderVolCurve;
audCurve g_PedBikeImplulseToImpactVolCurve;
audCurve g_PedBicyleImplulseToImpactVolCurve;
audCurve g_PedImpulseMagToClothCurve;
audCurve g_PedVehImpulseTimeScalarCurve;

audCurve audPedAudioEntity::sm_MeleeDamageToClothVol;
audCurve audPedAudioEntity::sm_MolotovFuseVolCurve;
audCurve audPedAudioEntity::sm_MolotovFusePitchCurve;

audCurve g_LegBoneVelocityToSwishVol;
audCurve g_LegBoneVelocityToSwishPitch;

f32 audPedAudioEntity::sm_InnerUpdateFrames = 3.0f;
f32 audPedAudioEntity::sm_OuterUpdateFrames = 3.0f;
f32 audPedAudioEntity::sm_InnerCheapUpdateFrames = 6.0f;
f32 audPedAudioEntity::sm_OuterCheapUpdateFrames = 9.0f;
f32 audPedAudioEntity::sm_CheapDistance = 20.0f;
f32 audPedAudioEntity::sm_PedHighLODDistanceThresholdSqd = 30.f * 30.f;
f32 audPedAudioEntity::sm_SmallAnimalThreshold = 0.5f;

f32 audPedAudioEntity::sm_MaxMotorbikeCollisionVelocity = 10.f;
f32 audPedAudioEntity::sm_MaxBicycleCollisionVelocity = 10.f;
f32 audPedAudioEntity::sm_MinHeightForWindNoise = 10.f;  
f32 audPedAudioEntity::sm_HighVelForWindNoise = 20.f;

f32 audPedAudioEntity::sm_ImpactMagForShotFallBoneBreak = 1.f;
u32 audPedAudioEntity::sm_AirTimeForLiveBigimpacts = 1000;

float audPedAudioEntity::sm_PedWalkingImpactFactor = 0.75f;
float audPedAudioEntity::sm_PedRunningImpactFactor = 1.f;
float audPedAudioEntity::sm_PedLightImpactFactor = 1.f;
float audPedAudioEntity::sm_PedMediumImpactFactor = 0.8f;
float audPedAudioEntity::sm_PedHeavyImpactFactor = 0.5f;
u32 audPedAudioEntity::sm_EntityImpactTimeFilter = 500;
u32 audPedAudioEntity::sm_LastTimePlayerRingtoneWasPlaying = 0;

extern bool g_PedsMakeSolidCollisions;
extern u32 g_MinTimeBetweenCollisions;

audHashActionTable audPedAudioEntity::sm_PedAnimActionTable;

f32 audPedAudioEntity::sm_SqdDistanceThresholdToTriggerShellCasing = 100.f;
u32 audPedAudioEntity::sm_TimeThresholdToTriggerShellCasing = 500;
u32 audPedAudioEntity::sm_LifeTimePedEnvGroup = 10000;
u32 audPedAudioEntity::sm_MeleeOverrideSoundNameHash;
float audPedAudioEntity::sm_VehicleImpactForHeadphonePulse = 5.f;
audWaveSlot* audPedAudioEntity::sm_ScriptMissionSlot;
RegdPed audPedAudioEntity::sm_LastTarget;

f32 g_MinDistSqToPlayFarAnimalSound = 25.0f * 25.0f;
u32 g_CowMooHash = ATSTRINGHASH("COW_MOO", 0x40FE4214);
u32 g_CowMooFarHash = ATSTRINGHASH("COW_MOO_FAR", 0xEFA2F4BE);
u32 g_CowMooRandomHash = ATSTRINGHASH("COW_MOO_RANDOM", 0xBC878BA9);
u32 g_CowMooFarRandomHash = ATSTRINGHASH("COW_MOO_FAR_RANDOM", 0x72BC49D8);
u32 g_RetrieverBarkHash = ATSTRINGHASH("RETRIEVER_BARK", 0x36654E96);
u32 g_RetrieverBarkFarHash = ATSTRINGHASH("RETRIEVER_BARK_FAR", 0xB428759);
u32 g_RetrieverBarkRandomHash = ATSTRINGHASH("RETRIEVER_BARK_RANDOM", 0xE78B73CF);
u32 g_RetrieverBarkFarRandomHash = ATSTRINGHASH("RETRIEVER_BARK_FAR_RANDOM", 0x3D701746);
u32 g_RottweilerBarkHash = ATSTRINGHASH("ROTTWEILER_BARK", 0x5119B18A);
u32 g_RottweilerBarkFarHash = ATSTRINGHASH("ROTTWEILER_BARK_FAR", 0x164386FB);
u32 g_RottweilerBarkRandomHash = ATSTRINGHASH("ROTTWEILER_BARK_RANDOM", 0x5FD2AF6F);
u32 g_RottweilerBarkFarRandomHash = ATSTRINGHASH("ROTTWEILER_BARK_FAR_RANDOM", 0x28CA2FE4);

f32 g_TennisLiteNullWeight = 0.0f;
f32 g_TennisLiteLiteWeight = 1.0f;
f32 g_TennisLiteMedWeight =  0.0f;
f32 g_TennisLiteStrongWeight =  0.0f;
f32 g_TennisLiteWeightTotal = 1.0f;
f32 g_TennisMedNullWeight =  0.0f;
f32 g_TennisMedLiteWeight =  0.0f;
f32 g_TennisMedMedWeight = 1.0f;
f32 g_TennisMedStrongWeight =  0.0f;
f32 g_TennisMedWeightTotal = 1.0f;
f32 g_TennisStrongNullWeight = 0.0f;
f32 g_TennisStrongLiteWeight = 0.0f;
f32 g_TennisStrongMedWeight = 0.0f;
f32 g_TennisStrongStrongWeight = 1.0f;
f32 g_TennisStrongWeightTotal = 1.0f;


extern f32 g_HealthLostForMediumPain;
extern f32 g_HealthLostForHighPain;
extern f32 g_HealthLostForHighDeath;
extern bool g_EnablePainedBreathing;
extern u32 g_TimeBetweenPainInhaleAndExhale;
extern u32 g_TimeBetweenPainExhaleAndInhale;
extern audPedScenarioManager g_PedScenarioManager;

bank_float g_MaxSplashBoneSpeed = 10.f;
BANK_ONLY(bank_float g_SplashSpeed = 1.0f);
bank_float g_SplashDecay = 0.85f;

bool g_ForceMonoRingtones = false;
u32 g_MaxCellPhoneLoops = 2;

f32 g_BulletCasingXOffset = 0.5f;
f32 g_BulletCasingYOffset = 0.f;
f32 g_BulletCasingZOffset = -1.2f;

#if __BANK
bool audPedAudioEntity::sm_ShowMeleeDebugInfo = false;
u32 g_OverriddenRingtone = 0;
bool g_ShowPedsEnvGroup = false;
bool g_ToggleRingtone = false;
bool g_DebugDrawScenarioManager = false;
bool g_ResetEnvGroupLifeTime = false;
bool g_ForceRappel = false;
bool g_ForceRappelDescend = false;
#endif

#if __DEV
bool g_BreakpointDebugPedUpdate = false;
#endif

u32 g_PedCarImpactTime = 250;
u32 g_PedCarUnderTime = 1000;

u32 g_MaxTimeToTrackBirdTakeOff = 3000;
u8 g_MinNumBirdTakeOffsToPlaySound = 3;

f32 g_MinMovingWindSpeedSq = 70.0f;

audSoundSet audPedAudioEntity::sm_KnuckleOverrideSoundSet;
audSoundSet audPedAudioEntity::sm_PedCollisionSoundSet;
audSoundSet audPedAudioEntity::sm_PedVehicleCollisionSoundSet;

audSoundSet audPedAudioEntity::sm_BikeMeleeHitPedSounds;
audSoundSet audPedAudioEntity::sm_BikeMeleeHitSounds;
audSoundSet audPedAudioEntity::sm_BikeMeleeSwipeSounds;

static u32 s_DefaultBikeMeleeHitName[NUM_EWEAPONEFFECTGROUP] = 
{
	ATSTRINGHASH("DEFAULT_UNARMED", 0x264760E)//WEAPON_EFFECT_GROUP_PUNCH_KICK = 0,
	,ATSTRINGHASH("DEFAULT_MELEE_WOOD", 0x14CEFE2B)//WEAPON_EFFECT_GROUP_MELEE_WOOD,
	,ATSTRINGHASH("DEFAULT_MELEE_METAL", 0xDEF06355)//WEAPON_EFFECT_GROUP_MELEE_METAL,
	,ATSTRINGHASH("DEFAULT_MELEE_SHARP", 0xDB9C1D3C)//WEAPON_EFFECT_GROUP_MELEE_SHARP,
	,ATSTRINGHASH("DEFAULT_MELEE_GENERIC", 0x13E4DDD6)//WEAPON_EFFECT_GROUP_MELEE_GENERIC,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_PISTOL_SMALL,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_PISTOL_LARGE,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_PISTOL_SILENCED,
	,ATSTRINGHASH("DEFAULT_UNARMED", 0x264760E)//WEAPON_EFFECT_GROUP_RUBBER,
	,ATSTRINGHASH("DEFAULT_WEAPON_MED", 0xD4892F57)//WEAPON_EFFECT_GROUP_SMG,
	,ATSTRINGHASH("DEFAULT_WEAPON_LARGE", 0xB3FDF638)//WEAPON_EFFECT_GROUP_SHOTGUN
	,ATSTRINGHASH("DEFAULT_WEAPON_LARGE", 0xB3FDF638)//WEAPON_EFFECT_GROUP_RIFLE_ASSAULT
	,ATSTRINGHASH("DEFAULT_WEAPON_LARGE", 0xB3FDF638)//WEAPON_EFFECT_GROUP_RIFLE_SNIPER
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_ROCKET,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_GRENADE,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_MOLOTOV,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_FIRE,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_EXPLOSION,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_LASER,
	,ATSTRINGHASH("DEFAULT_WEAPON_SMALL", 0xA223BD89)//WEAPON_EFFECT_GROUP_STUNGUN,
	,ATSTRINGHASH("DEFAULT_WEAPON_LARGE", 0xB3FDF638)//WEAPON_EFFECT_GROUP_HEAVY_MG
	,ATSTRINGHASH("DEFAULT_WEAPON_LARGE", 0xB3FDF638)//WEAPON_EFFECT_GROUP_VEHICLE_MG
};

void audPedAudioEntity::InitClass()
{
	// default handler handles cases where a sound has to be triggered with no special logic
	sm_PedAnimActionTable.SetDefaultActionHandler(&audPedAudioEntity::DefaultPedActionHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("HORN_OFF", 0x76B9FDAC), &audVehicleAudioEntity::HornOffHandler);
	// footstep handlers
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("LEFT_FOOT_WALK", 0x31A570AA), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RIGHT_FOOT_WALK", 0x6F2142A0), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("LEFT_FOOT_SCUFF" , 0xC392AB61), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RIGHT_FOOT_SCUFF", 0x5F1A0D8B), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("LEFT_FOOT_JUMP" , 0x434CBD8D), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RIGHT_FOOT_JUMP", 0x92F703D9), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("LEFT_FOOT_RUN" , 0x3DC0D219), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RIGHT_FOOT_RUN", 0xF37DAB85), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("LEFT_FOOT_SPRINT" , 0x6DE979FC), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RIGHT_FOOT_SPRINT", 0xFD11320D), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("LEFT_FOOT_SOFT" , 0x59290187), &audPedFootStepAudio::FootStepHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RIGHT_FOOT_SOFT", 0x9FE4C053), &audPedFootStepAudio::FootStepHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AEF_LADDER_FOOT_L", 0x4275F5EE), &audPedFootStepAudio::LeftLadderFoot);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AEF_LADDER_FOOT_L_UP", 0xE0186D84), &audPedFootStepAudio::LeftLadderFootUp);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AEF_LADDER_FOOT_R", 0xD204150C), &audPedFootStepAudio::RightLadderFoot);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AEF_LADDER_FOOT_R_UP", 0xCD4EE1AC), &audPedFootStepAudio::RightLadderFootUp);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AEF_LADDER_HAND_L", 0x970813C9), &audPedFootStepAudio::LeftLadderHand);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AEF_LADDER_HAND_R", 0x2392ACE0), &audPedFootStepAudio::RightLadderHand);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_HAND", 0x63B1018F), &audPedFootStepAudio::WallClimbHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_FOOT", 0x6E1583C3), &audPedFootStepAudio::WallClimbHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_LAUNCH", 0xDD79F372), &audPedFootStepAudio::WallClimbHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_KNEE", 0xF68FC269), &audPedFootStepAudio::WallClimbHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_SCRAPE", 0xE3E92C86), &audPedFootStepAudio::WallClimbHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("BONNET_SLIDE", 0x5E5D5914), &audPedFootStepAudio::BonnetSlideHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PED_ROLL", 0x8868E1A4), &audPedFootStepAudio::PedRollHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PARACHUTE_LAND_PARACHUTE_RELEASE_MASTER", 0x7452D68), &audPedFootStepAudio::PedFwdRollHandler);

	// melee
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_ANIMS_UPPERCUT", 0xEFB28E0F), &audPedAudioEntity::MeleeOverrideHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("FIST_TO_BACK_BODY", 0xDB3855D8), &audPedAudioEntity::MeleeOverrideHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_SLAP", 0xCADA6CC), &audPedAudioEntity::MeleeOverrideHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("FIST_TO_FACE", 0x5BB45295), &audPedAudioEntity::MeleeOverrideHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_HEAD_HIT", 0xDFEA0B45), &audPedAudioEntity::MeleeHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_BODY_HIT", 0xDEF82ADD), &audPedAudioEntity::MeleeHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_FABRIC_SWING_LRG", 0x985DA85F), &audPedAudioEntity::MeleeSwingHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_FABRIC_SWING_MED", 0x726E7B84), &audPedAudioEntity::MeleeSwingHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_FABRIC_SWING_SML", 0x6FB36871), &audPedAudioEntity::MeleeSwingHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_FABRIC_SWING_QUIET_LRG", 0x4CDB9FF1), &audPedAudioEntity::MeleeSwingHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_FABRIC_SWING_QUIET_MED", 0xF8869509), &audPedAudioEntity::MeleeSwingHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_COMBAT_FABRIC_SWING_QUIET_SML", 0x6B23E605), &audPedAudioEntity::MeleeSwingHandler);

	// end footstep handlers

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_COVER_HIT_WALL", 0xC9F0E603), &audPedAudioEntity::CoverHitWallHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_COVER_HIT_WALL_SOFTER", 0x23FC32D6), &audPedAudioEntity::CoverHitWallHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_PED_TOOL_IMPACT", 0x5B9FF9F0), &audPedAudioEntity::PedToolImpactHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_AMMO_SHOP_GUN_HEFT", 0xDFEB923B), &audPedAudioEntity::GunHeftHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_AMMO_SHOP_PUT_DOWN_GUN", 0x7F63A87B), &audPedAudioEntity::PutDownGunHandler);

	// anim-triggered speech handlers
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_FIGHT_WATCH", 0xA3124482), &audPedAudioEntity::SayFightWatchHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_STEPPED_IN_SOMETHING", 0x8AFA9E47), &audPedAudioEntity::SaySteppedInSomethingHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_WALKIE_TALKIE", 0xFB18A381), &audPedAudioEntity::SayWalkieTalkieHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_STRIP_CLUB", 0x2E2CF4C4), &audPedAudioEntity::SayStripClubHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_DRINK", 0x28984F85), &audPedAudioEntity::SayDrinkHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_GRUNT_CLIMB_EASY_UP", 0x7226C77F), &audPedAudioEntity::SayGruntClimbEasyUpHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_GRUNT_CLIMB_HARD_UP", 0x9218F091), &audPedAudioEntity::SayGruntClimbHardUpHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_GRUNT_CLIMB_OVER", 0xDE2FE974), &audPedAudioEntity::SayGruntClimbOverHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_INHALE", 0x3DCE6DB2), &audPedAudioEntity::SayInhaleHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_JACKING", 0x835364F8), &audPedAudioEntity::SayJackingHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_DYING_MOAN", 0xD7E67271), &audPedAudioEntity::SayDyingMoanHandler);

	// whistle is done from an anim - we intercept and play speech if it's the player
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_PLAYER_WHISTLE_FOR_CAB", 0xDAA5E486), &audPedAudioEntity::PlayerWhistleForCabHandler);
	// pain handlers, so we can interrupt speech and play pain from an anim
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_LOW", 0x4EA71DF9), &audPedAudioEntity::PainLowHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_MEDIUM", 0xC0698860), &audPedAudioEntity::PainMediumHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_HIGH", 0xBCF84976), &audPedAudioEntity::PainHighHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("DEATH_HIGH_SHORT", 0xCA9DD0BA), &audPedAudioEntity::DeathHighShortHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("DEATH_LOW", 0x2C0A11CB), &audPedAudioEntity::DeathLowHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PED_IMPACT", 0xD9CADDD9), &audPedAudioEntity::PedImpactHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_SHOVE", 0xB7E94D4F), &audPedAudioEntity::PainShoveHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_WHEEZE", 0xF7D1170B), &audPedAudioEntity::PainWheezeHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SCREAM_SHOCKED", 0xD9D4BDE1), &audPedAudioEntity::ScreamShockedHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("DEATH_GARGLE", 0xCE5DECFB), &audPedAudioEntity::DeathGargleHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("DEATH_HIGH_LONG", 0x582D2180), &audPedAudioEntity::DeathHighLongHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("DEATH_HIGH_MEDIUM", 0xCFFA1787), &audPedAudioEntity::DeathHighMediumHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_GARGLE", 0x20849A9), &audPedAudioEntity::PainGargleHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("CLIMB_LARGE", 0xD222D542), &audPedAudioEntity::ClimbLargeHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("CLIMB_SMALL", 0x268C47C3), &audPedAudioEntity::ClimbSmallHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("JUMP", 0x2E90C5D5), &audPedAudioEntity::JumpHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_SMALL_GRUNT", 0x474B446E), &audPedAudioEntity::MeleeSmallGrunt);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("MELEE_LARGE_GRUNT", 0x8DB8B06E), &audPedAudioEntity::MeleeLargeGrunt);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("COWER", 0x3376FD5D), &audPedAudioEntity::CowerHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("WHIMPER", 0x7CECAC01), &audPedAudioEntity::WhimperHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("COUGH", 0xD0AFA4FD), &audPedAudioEntity::CoughHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SNEEZE", 0x8ED17AA0), &audPedAudioEntity::SneezeHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_LOW_FRONTEND", 0xCE704F44), &audPedAudioEntity::PainLowFrontendHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PAIN_HIGH_FRONTEND", 0xE2C3F1), &audPedAudioEntity::PainHighFrontendHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_LAUGH", 0x3782D98E), &audPedAudioEntity::SayLaughHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SAY_CRY", 0x4FAD2690), &audPedAudioEntity::SayCryHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("PLAYER_WHISTLE", 0xDAD0E487), &audPedAudioEntity::PlayerWhistleHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("HIGH_FALL", 0xC5430292), &audPedAudioEntity::HighFallHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("VOCAL_EXERCISE", 0x150D8FC6), &audPedAudioEntity::VocalExerciseHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("EXHALE_CYCLING", 0x5F7C64CC), &audPedAudioEntity::ExhaleCyclingHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_PED_BURGER_EAT", 0x7CC59DD1), &audPedAudioEntity::AnimPedBurgerEatHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_VEH_JACK_HEAD_HIT_WHEEL", 0x2264661A), &audPedAudioEntity::HeadHitSteeringWheelHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_BLIP_THROTTLE", 0x6BC6B124), &audPedAudioEntity::BlipThrottleHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SCRIPTED_CONVERSATION_TRIGGER", 0x23B981C6), &audPedAudioEntity::ConversationTriggerHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ANIM_STOP_SPEECH", 0x6D321D05), &audPedAudioEntity::StopSpeechHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("SPACE_RANGER_SPEECH", 0x8A2AE4E0), &audPedAudioEntity::SpaceRangerSpeechHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("WEAPON_FIRE", 0xDF8E89EB), &audPedAudioEntity::WeaponFireHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AUTO_FIRE_START", 0xC2A57744), &audPedAudioEntity::WeaponFireHandler); 
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("AUTO_FIRE_STOP", 0x3106F0FB), &audPedAudioEntity::WeaponFireHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("WEAPON_THROW", 0x3A5B4E71), &audPedAudioEntity::WeaponThrowHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("COW_MOO", 0x40FE4214), &audPedAudioEntity::CowMooHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("COW_MOO_RANDOM", 0xBC878BA9), &audPedAudioEntity::CowMooRandomHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RETRIEVER_BARK", 0x36654E96), &audPedAudioEntity::RetrieverBarkHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("RETRIEVER_BARK_RANDOM", 0xE78B73CF), &audPedAudioEntity::RetrieverBarkRandomHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ROTTWEILER_BARK", 0x5119B18A), &audPedAudioEntity::RottweilerBarkHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("ROTTWEILER_BARK_RANDOM", 0x5FD2AF6F), &audPedAudioEntity::RottweilerBarkRandomHandler);

	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("TENNIS_VFX_LITE", 0xB156DF5D), &audPedAudioEntity::TennisLiteHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("TENNIS_VFX_MED", 0xA8DCE5A), &audPedAudioEntity::TennisMedHandler);
	sm_PedAnimActionTable.AddActionHandler(ATSTRINGHASH("TENNIS_VFX_STRONG", 0x93D4C1E9), &audPedAudioEntity::TennisStrongHandler);

	sm_MeleeOverrideSoundNameHash = ATSTRINGHASH("OVERRIDE", 0x6E54651D);

	sm_ScriptMissionSlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("MISSION_SCRIPT_SLOT", 0x79DA6CAF));

	g_LegBoneVelocityToSwishVol.Init(ATSTRINGHASH("BONE_VELOCITY_TO_VOLUME", 0x7EDB290F));
	g_LegBoneVelocityToSwishPitch.Init(ATSTRINGHASH("BONE_VELOCITY_TO_PITCH", 0x4038DDE5));

	g_PedImpulseToBoneBreakProbCurve.Init(ATSTRINGHASH("PED_IMPULSE_TO_BONE_BREAK_PROB", 0x2EBBA629));
	g_PedVehImpactBoneBreakProbCurve.Init(ATSTRINGHASH("PED_VEH_IMPULSE_TO_BONE_BREAK_PROB", 0x2F040F06));
	g_PedVehImpulseToImpactVolCurve.Init(ATSTRINGHASH("PED_VEH_IMPULSE_TO_IMPACT", 0xDF6C8117));
	g_PedVehOverUnderVolCurve.Init(ATSTRINGHASH("PED_VEH_OVER_UNDER_CURVE", 0x5E479528));
	if(!g_PedVehOverUnderVolCurve.IsValid())
	{
		g_PedVehOverUnderVolCurve.Init(ATSTRINGHASH("PED_VEH_IMPULSE_TO_IMPACT", 0xDF6C8117));
	}
	g_PedBikeImplulseToImpactVolCurve.Init(ATSTRINGHASH("PED_BIKE_IMPULSE_TO_IMPACT", 0xA3F08808));
	g_PedBicyleImplulseToImpactVolCurve.Init(ATSTRINGHASH("PED_BICYCLE_IMPULSE_TO_IMPACT", 0xDB91ADDD));
	g_PedImpulseMagToClothCurve.Init(ATSTRINGHASH("PED_IMPULSE_MAG_TO_CLOTH", 0xDC4B4325));
	
	g_PedVehImpulseTimeScalarCurve.Init(ATSTRINGHASH("PED_VEH_IMPULSE_TIME_SCALAR", 0xEB2E1D5));

	sm_MeleeDamageToClothVol.Init(ATSTRINGHASH("MELEE_DAMAGE_DONE_TO_CLOTHING_VOL", 0xFF22125D));
	sm_MolotovFuseVolCurve.Init(ATSTRINGHASH("MOLOTOV_FUSE_VOL_CURVE", 0xB9352242));
	sm_MolotovFusePitchCurve.Init(ATSTRINGHASH("MOLOTOV_FUSE_PITCH_CURVE", 0xD53E51B3));
	sm_PedCollisionSoundSet.Init(ATSTRINGHASH("PED_COLLISION_SET", 0x2004A5C4));
	sm_PedVehicleCollisionSoundSet.Init(ATSTRINGHASH("PED_VEH_COLLISIONS_SOUNDSET", 0x9680E151));
	sm_KnuckleOverrideSoundSet.Init(ATSTRINGHASH("KNUCKLE_OVERRIDE_SOUNDS", 0x30F165A4));

	InitTennisVocalizationRaveSettings();
	// Init footstep audio
	audPedFootStepAudio::InitClass();

	sm_BirdTakeOffWriteIndex = 0;

	sm_LastTarget = NULL;
}
void audPedAudioEntity::InitMPSpecificSounds()
{
	sm_BikeMeleeHitPedSounds.Init(ATSTRINGHASH("BIKE_MELEE_PED_SOUNDS", 0xC35825EC));
	sm_BikeMeleeHitSounds.Init(ATSTRINGHASH("BIKE_MELEE_SOUNDS", 0xFD7530DC));
	sm_BikeMeleeSwipeSounds.Init(ATSTRINGHASH("BIKE_MELEE_SWIPE_SOUNDS", 0x55D0EA80));
}
void audPedAudioEntity::ShutdownClass()
{
	audPedFootStepAudio::ShutdownClass();
}
void audPedAudioEntity::DefaultPedActionHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;
	AudBaseObject* obj =(rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(hash);
	if(obj)
	{
		switch(obj->ClassId)
		{
		case AnimalVocalAnimTrigger::TYPE_ID:
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalVocalAnimTrigger*)obj);
			}
			return;
		default:
			break;
		}
	}
	
	ped->GetPedAudioEntity()->PlayAnimTriggeredSound(hash);
}

void audPedAudioEntity::HeadHitSteeringWheelHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;
	
	CVehicle *veh = ped->GetVehiclePedInside();

	if(veh && veh->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
	{
		audCarAudioEntity* carEntity = static_cast<audCarAudioEntity*>(veh->GetVehicleAudioEntity());
		carEntity->BlipHorn();
	}

	// trigger the impact sound too
	ped->GetPedAudioEntity()->PlayAnimTriggeredSound(hash);
}

void audPedAudioEntity::BlipThrottleHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed *ped = (CPed*)context;

	CVehicle *veh = ped->GetVehiclePedInside();

	if(veh && veh->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
	{
		audCarAudioEntity* carEntity = static_cast<audCarAudioEntity*>(veh->GetVehicleAudioEntity());
		carEntity->BlipThrottle();
	}
}

void audPedAudioEntity::WeaponFireHandler(u32 hash, void*context)
{
	CPed *ped = (CPed*)context;

	ped->GetPedAudioEntity()->SetWeaponAnimEvent(hash);
}

void audPedAudioEntity::WeaponThrowHandler(u32 UNUSED_PARAM(hash), void*context)
{
	CPed *ped = (CPed*)context;

	ped->GetPedAudioEntity()->WeaponThrow();
}

void audPedAudioEntity::CowMooHandler(u32 UNUSED_PARAM(hash), void*context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		f32 dist2 = (VEC3V_TO_VECTOR3(ped->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		AudBaseObject* obj = NULL;
		if(dist2 < g_MinDistSqToPlayFarAnimalSound)
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_CowMooHash);
		else
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_CowMooFarHash);

		if(obj)
		{
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalVocalAnimTrigger*)obj);
			}
		}
	}
}

void audPedAudioEntity::CowMooRandomHandler(u32 UNUSED_PARAM(hash), void*context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		f32 dist2 = (VEC3V_TO_VECTOR3(ped->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		AudBaseObject* obj = NULL;
		if(dist2 < g_MinDistSqToPlayFarAnimalSound)
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_CowMooRandomHash);
		else
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_CowMooFarRandomHash);

		if(obj)
		{
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalVocalAnimTrigger*)obj);
			}
		}
	}
}

void audPedAudioEntity::RetrieverBarkHandler(u32 UNUSED_PARAM(hash), void*context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		f32 dist2 = (VEC3V_TO_VECTOR3(ped->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		AudBaseObject* obj = NULL;
		if(dist2 < g_MinDistSqToPlayFarAnimalSound)
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RetrieverBarkHash);
		else
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RetrieverBarkFarHash);

		if(obj)
		{
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalVocalAnimTrigger*)obj);
			}
		}
	}
}

void audPedAudioEntity::RetrieverBarkRandomHandler(u32 UNUSED_PARAM(hash), void*context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		f32 dist2 = (VEC3V_TO_VECTOR3(ped->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		AudBaseObject* obj = NULL;
		if(dist2 < g_MinDistSqToPlayFarAnimalSound)
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RetrieverBarkRandomHash);
		else
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RetrieverBarkFarRandomHash);

		if(obj)
		{
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalVocalAnimTrigger*)obj);
			}
		}
	}
}

void audPedAudioEntity::RottweilerBarkHandler(u32 UNUSED_PARAM(hash), void*context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		f32 dist2 = (VEC3V_TO_VECTOR3(ped->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		AudBaseObject* obj = NULL;
		if(dist2 < g_MinDistSqToPlayFarAnimalSound)
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RottweilerBarkHash);
		else
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RottweilerBarkFarHash);

		if(obj)
		{
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalVocalAnimTrigger*)obj);
			}
		}
	}
}

void audPedAudioEntity::RottweilerBarkRandomHandler(u32 UNUSED_PARAM(hash), void*context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		f32 dist2 = (VEC3V_TO_VECTOR3(ped->GetMatrix().d() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		AudBaseObject* obj = NULL;
		if(dist2 < g_MinDistSqToPlayFarAnimalSound)
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RottweilerBarkRandomHash);
		else
			obj = (rage::AudBaseObject*)audNorthAudioEngine::GetMetadataManager().GetObjectMetadataPtr(g_RottweilerBarkFarRandomHash);

		if(obj)
		{
			if(ped && ped->GetSpeechAudioEntity())
			{
				ped->GetSpeechAudioEntity()->PlayAnimalVocalization((AnimalVocalAnimTrigger*)obj);
			}
		}
	}
}

void audPedAudioEntity::ConversationTriggerHandler(u32 UNUSED_PARAM(hash), void* context)
{
	CPed *ped = (CPed*)context;

	g_ScriptAudioEntity.HandleConversaionAnimTrigger(ped);
}

void audPedAudioEntity::StopSpeechHandler(u32 UNUSED_PARAM(hash), void* context)
{
	CPed *ped = (CPed*)context;
	if(ped && ped->GetSpeechAudioEntity())
		ped->GetSpeechAudioEntity()->MarkToStopSpeech(); //must be done on main thread
}

void audPedAudioEntity::SpaceRangerSpeechHandler(u32 UNUSED_PARAM(hash), void* context)
{
	CPed *ped = (CPed*)context;
	if(ped && ped->GetSpeechAudioEntity())
		ped->GetSpeechAudioEntity()->MarkToSaySpaceRanger(); //must be done on main thread
}

void audPedAudioEntity::PedToolImpactHandler(u32 UNUSED_PARAM(hash), void *UNUSED_PARAM(context))
{
#if 0 // CS
	u32 soundHash = 0;
	CPed *ped = (CPed*)context;
	static const audStringHash sledgehammer("SLEDGEHAMMER_MULTI");
	static const audStringHash pickaxe("PICK_AXE_MULTI");

	//Displayf("%u", ped->GetPedAudioEntity()->m_SuseAudio);

	if(ped->GetWeaponManager() && ped->GetWeaponManager()->GetWeaponObject() && ped->GetWeaponManager()->GetWeapon() && 
	   ped->GetWeaponManager()->GetWeapon()->GetWeaponType()==WEAPONTYPE_OBJECT)
	{
		// grab the model index
		s32 modelIndex = ped->GetWeaponManager()->GetWeaponObject()->GetModelId();
		
		if(modelIndex == miPickAxe)
		{
			soundHash = pickaxe;
		}
		else if(modelIndex == miSledgehammer)
		{
			soundHash = sledgehammer;
		}
		else
		{
			Assert(!"Ped hitting things with unknown tool");
		}
		ped->GetPedAudioEntity()->PlayAnimTriggeredSound(soundHash);
	}
#endif // 0
}		

void audPedAudioEntity::GunHeftHandler(u32 UNUSED_PARAM(hash),	void *context)
{
	CPed *ped = (CPed*)context;

	const WeaponSettings *weaponSettings = NULL;
	const CPedWeaponManager *pedWeaponMgr = ped->GetWeaponManager();
	if(pedWeaponMgr && pedWeaponMgr->GetEquippedWeaponObject() && pedWeaponMgr->GetEquippedWeaponObject()->GetWeapon())
		weaponSettings = pedWeaponMgr->GetEquippedWeaponObject()->GetWeapon()->GetAudioComponent().GetWeaponSettings();

	if(weaponSettings)
	{
		ped->GetPedAudioEntity()->PlayAnimTriggeredSound(weaponSettings->HeftSound);
	}
}

void audPedAudioEntity::PutDownGunHandler(u32 UNUSED_PARAM(hash),	void *context)
{
	CPed *ped = (CPed*)context;

	const WeaponSettings *weaponSettings = NULL;
	const CPedWeaponManager *pedWeaponMgr = ped->GetWeaponManager();
	if(pedWeaponMgr && pedWeaponMgr->GetEquippedWeaponObject() && pedWeaponMgr->GetEquippedWeaponObject()->GetWeapon())
		weaponSettings = pedWeaponMgr->GetEquippedWeaponObject()->GetWeapon()->GetAudioComponent().GetWeaponSettings();

	if(weaponSettings)
	{
		ped->GetPedAudioEntity()->PlayAnimTriggeredSound(weaponSettings->PutDownSound);
		ped->GetPedAudioEntity()->PlayAnimTriggeredSound(weaponSettings->RattleSound);
	}
}

// Speech contexts triggered by anims - each one needs a custom handler
void audPedAudioEntity::SayFightWatchHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed* ped = static_cast<CPed*>(context);
	if(ped)
	{
		ped->NewSay("GENERIC_CHEER",
			ped->IsMale() ? ATSTRINGHASH("WAVELOAD_PAIN_MALE", 0x804C18BB) : ATSTRINGHASH("WAVELOAD_PAIN_FEMALE", 0x0332128ab), 
			true, true);
	}
}

void audPedAudioEntity::SaySteppedInSomethingHandler(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->NewSay("STEPPED_IN_SHIT");
}

void audPedAudioEntity::SayWalkieTalkieHandler(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->NewSay("WALKIE_TALKIE");
}

void audPedAudioEntity::SayStripClubHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed* ped = static_cast<CPed*>(context);
	if(ped)
	{
		ped->NewSay("GENERIC_CHEER",
			ped->IsMale() ? ATSTRINGHASH("WAVELOAD_PAIN_MALE", 0x804C18BB) : ATSTRINGHASH("WAVELOAD_PAIN_FEMALE", 0x0332128ab), 
			true, true);
	}
}

void audPedAudioEntity::SayDrinkHandler(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->NewSay("GENERIC_DRINK");
}

void audPedAudioEntity::SayGruntClimbEasyUpHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->IsLocalPlayer() && !NetworkInterface::IsGameInProgress())
	{
		((CPed*)context)->NewSay("GRUNT_CLIMB_EASY_UP", audSpeechAudioEntity::GetLocalPlayerPainVoice(), true, true);
	}
}

void audPedAudioEntity::SayGruntClimbHardUpHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->IsLocalPlayer() && !NetworkInterface::IsGameInProgress())
	{
		((CPed*)context)->NewSay("GRUNT_CLIMB_HARD_UP", audSpeechAudioEntity::GetLocalPlayerPainVoice(), true, true);
	}
}

void audPedAudioEntity::SayGruntClimbOverHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->IsLocalPlayer() && !NetworkInterface::IsGameInProgress())
	{
		((CPed*)context)->NewSay("GRUNT_CLIMB_OVER", audSpeechAudioEntity::GetLocalPlayerPainVoice(), true, true);
	}
}

void audPedAudioEntity::SayDyingMoanHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DYING_MOAN;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::SayInhaleHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.DamageReason = AUD_DAMAGE_REASON_INHALE;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::SayJackingHandler(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->NewSay("");
}

void audPedAudioEntity::PlayerWhistleForCabHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;
	ped->GetPedAudioEntity()->PlayAnimTriggeredSound(hash);
	((CPed*)context)->NewSay("TAXI_HAIL", 0, false, false, 700);
}

void audPedAudioEntity::PlayerWhistleHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;
	if(ped)
		ped->GetPedAudioEntity()->PlayAnimTriggeredPlayerWhistle(hash);
}

void audPedAudioEntity::PainLowHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain - 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PainHighHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PainMediumHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain - 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::DeathHighShortHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = true;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighDeath + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.ForceDeathShort = true;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PedImpactHandler(u32 UNUSED_PARAM(hash), void * context)
{
	if (((CPed*)context)->GetAudioEntity())
	{
		Vector3 pos = VEC3V_TO_VECTOR3(((CPed*)context)->GetTransform().GetPosition());
		audPedAudioEntity * pedAudio = (audPedAudioEntity*)(((CPed*)context)->GetAudioEntity());
		pedAudio->TriggerImpactSounds(pos, NULL, NULL, 10.f, RAGDOLL_SPINE0, 0, false);
	}
}

void audPedAudioEntity::DeathLowHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = true;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE;
		damageStats.RawDamage = g_HealthLostForHighDeath - 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PainShoveHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_SHOVE;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PainWheezeHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_WHEEZE;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::ScreamShockedHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_SCREAM_SHOCKED;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::DeathGargleHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = true;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PlayGargle = true;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::DeathHighLongHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = true;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighDeath + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.ForceDeathLong = true;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::DeathHighMediumHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighDeath + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.ForceDeathMedium = true;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PainGargleHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = true;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::ClimbLargeHandler(u32 UNUSED_PARAM(hash), void *context)
{
	audSpeechAudioEntity* audEnt =((CPed*)context)->GetSpeechAudioEntity();
	if (audEnt && !audEnt->GetIsAnySpeechPlaying())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = false;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_CLIMB_LARGE;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::ClimbSmallHandler(u32 UNUSED_PARAM(hash), void *context)
{
	audSpeechAudioEntity* audEnt =((CPed*)context)->GetSpeechAudioEntity();
	if (audEnt && !audEnt->GetIsAnySpeechPlaying())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = false;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_CLIMB_SMALL;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::JumpHandler(u32 UNUSED_PARAM(hash), void *context)
{
	audSpeechAudioEntity* audEnt =((CPed*)context)->GetSpeechAudioEntity();
	if (audEnt && !audEnt->GetIsAnySpeechPlaying())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = false;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_JUMP;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::MeleeSmallGrunt(u32 UNUSED_PARAM(hash), void *context)
{
	audSpeechAudioEntity* audEnt =((CPed*)context)->GetSpeechAudioEntity();
	if (audEnt && !audEnt->GetIsAnySpeechPlaying())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = false;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE_SMALL_GRUNT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::MeleeLargeGrunt(u32 UNUSED_PARAM(hash), void *context)
{
	audSpeechAudioEntity* audEnt =((CPed*)context)->GetSpeechAudioEntity();
	if (audEnt && !audEnt->GetIsAnySpeechPlaying())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = false;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE_LARGE_GRUNT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::CowerHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = false;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_COWER;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::WhimperHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.PlayGargle = false;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_WHIMPER;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::HighFallHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		((CPed*)context)->GetSpeechAudioEntity()->SetBlockFallingVelocityCheck();

		audDamageStats damageStats;
		damageStats.RawDamage = 0.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_FALLING;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::VocalExerciseHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (audEngineUtil::ResolveProbability(0.0333f) && ((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_SHOVE;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::ExhaleCyclingHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		((CPed*)context)->GetSpeechAudioEntity()->MarkToPlayCyclingBreath();
	}
}

void audPedAudioEntity::CoughHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_COUGH;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::SneezeHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_SNEEZE;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PainLowFrontendHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForMediumPain - 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();
		damageStats.IsFrontend = true;

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::PainHighFrontendHandler(u32 UNUSED_PARAM(hash), void *context)
{
	if (((CPed*)context)->GetSpeechAudioEntity())
	{
		audDamageStats damageStats;
		damageStats.Fatal = false;
		damageStats.IsFromAnim = true;
		damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
		damageStats.DamageReason = AUD_DAMAGE_REASON_DEFAULT;
		damageStats.PedWasAlreadyDead = ((CPed*)context)->ShouldBeDead();
		damageStats.IsFrontend = true;

		((CPed*)context)->GetSpeechAudioEntity()->ReallyInflictPain(damageStats);
	}
}

void audPedAudioEntity::SayCryHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed* ped = static_cast<CPed*>(context);
	if(ped && ped->GetSpeechAudioEntity())
	{
		ped->GetSpeechAudioEntity()->TriggerSingleCry();
	}
}

void audPedAudioEntity::SayLaughHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed* ped = static_cast<CPed*>(context);
	if(ped)
	{
		ped->NewSay("LAUGH",
			ped->IsMale() ? ATSTRINGHASH("WAVELOAD_PAIN_MALE", 0x804C18BB) : ATSTRINGHASH("WAVELOAD_PAIN_FEMALE", 0x0332128ab), 
			true, true);
	}
}


void audPedAudioEntity::AnimPedBurgerEatHandler(u32 hash, void *context)
{
	if (context && ((CPed*)context)->IsLocalPlayer())
	{
		DefaultPedActionHandler(hash, context);
	}
}


void audPedAudioEntity::CoverHitWallHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;
	if (ped)
	{
		ped->GetPedAudioEntity()->GetFootStepAudio().PlayCoverHitWallSounds(hash);
	}
}

void audPedAudioEntity::TennisLiteHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed *ped = (CPed*)context;
	audSpeechAudioEntity* audEnt = ped ? ped->GetSpeechAudioEntity() : NULL;
	if (!audEnt)
	{
		return;
	}

	f32 randNum = audEngineUtil::GetRandomNumberInRange(0.0f, g_TennisLiteWeightTotal);
	if(randNum < g_TennisLiteNullWeight)
	{
		return;
	}
	randNum -= g_TennisLiteNullWeight;
	if(randNum < g_TennisLiteLiteWeight)
	{
		audEnt->PlayTennisVocalization(TENNIS_VFX_LITE);
		return;
	}
	randNum -= g_TennisLiteLiteWeight;
	if(randNum < g_TennisLiteMedWeight)
	{
		audEnt->PlayTennisVocalization(TENNIS_VFX_MED);
		return;
	}
	audEnt->PlayTennisVocalization(TENNIS_VFX_STRONG);
}

void audPedAudioEntity::TennisMedHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed *ped = (CPed*)context;
	audSpeechAudioEntity* audEnt = ped ? ped->GetSpeechAudioEntity() : NULL;
	if (!audEnt)
	{
		return;
	}

	f32 randNum = audEngineUtil::GetRandomNumberInRange(0.0f, g_TennisMedWeightTotal);
	if(randNum < g_TennisMedNullWeight)
	{
		return;
	}
	randNum -= g_TennisMedNullWeight;
	if(randNum < g_TennisMedLiteWeight)
	{
		audEnt->PlayTennisVocalization(TENNIS_VFX_LITE);
		return;
	}
	randNum -= g_TennisMedLiteWeight;
	if(randNum < g_TennisMedMedWeight)
	{
		audEnt->PlayTennisVocalization(TENNIS_VFX_MED);
		return;
	}
	audEnt->PlayTennisVocalization(TENNIS_VFX_STRONG);
}

void audPedAudioEntity::TennisStrongHandler(u32 UNUSED_PARAM(hash), void *context)
{
	CPed *ped = (CPed*)context;
	audSpeechAudioEntity* audEnt = ped ? ped->GetSpeechAudioEntity() : NULL;
	if (!audEnt)
	{
		return;
	}

	f32 randNum = audEngineUtil::GetRandomNumberInRange(0.0f, g_TennisStrongWeightTotal);
	if(randNum < g_TennisStrongNullWeight)
	{
		return;
	}
	randNum -= g_TennisStrongNullWeight;
	if(randNum < g_TennisStrongLiteWeight)
	{
		audEnt->PlayTennisVocalization(TENNIS_VFX_LITE);
		return;
	}
	randNum -= g_TennisStrongLiteWeight;
	if(randNum < g_TennisStrongMedWeight)
	{
		audEnt->PlayTennisVocalization(TENNIS_VFX_MED);
		return;
	}
	audEnt->PlayTennisVocalization(TENNIS_VFX_STRONG);
}

void audPedAudioEntity::PlayAnimTriggeredSound(u32 soundHash)
{
	if(!m_Ped->m_nDEflags.bFrozenByInterior)
	{
		// url:bugstar:7732980 - Precision Rifle : Updated Reload SFX
		if (soundHash == ATSTRINGHASH("SNP_BOLT_PLYR_ACTION_MASTER", 0xD5C768FE))
		{
			if (const CPedWeaponManager* pedWeaponManager = m_Ped->GetWeaponManager())
			{
				if (const CWeapon* weapon = pedWeaponManager->GetEquippedWeapon())
				{
					if (weapon->GetAudioComponent().GetSettingsHash() == ATSTRINGHASH("audio_item_precision_rifle", 0xE364DCB3))
					{
						soundHash = ATSTRINGHASH("Precision_Rifle_Bolt_Action_Master", 0xDB0238A8);
					}
				}
			}
		}

		audSoundInitParams initParams;
		initParams.WaveSlot = sm_ScriptMissionSlot;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.TrackEntityPosition = true;
		initParams.AllowLoad = false;
		CreateAndPlaySound(soundHash, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundHash, &initParams, m_Ped));
	}
}

void audPedAudioEntity::PlayAnimTriggeredPlayerWhistle(u32 soundHash)
{
	if(m_PlayerWhistleSound)
		return;

	if(!m_Ped->m_nDEflags.bFrozenByInterior && m_Ped->GetSpeechAudioEntity())
	{
		m_Ped->GetSpeechAudioEntity()->MarkToStopSpeech();

		audSoundInitParams initParams;
		initParams.WaveSlot = sm_ScriptMissionSlot;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.TrackEntityPosition = true;
		initParams.AllowLoad = false;
		CreateAndPlaySound_Persistent(soundHash, &m_PlayerWhistleSound, &initParams);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundHash, &initParams, m_PlayerWhistleSound, m_Ped, eNoGlobalSoundEntity););
	}
}

void audPedAudioEntity::PlayScubaBreath(bool inhale)
{
	if(!m_Ped)
		return;

	m_ShouldUpdateScubaBubblePtFX = false;

	if(!m_Ped->m_nDEflags.bFrozenByInterior && !m_ScubaSound)
	{
		audSoundInitParams initParams;
		initParams.WaveSlot = sm_ScriptMissionSlot;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.TrackEntityPosition = true;
		initParams.AllowLoad = false;
		CreateAndPlaySound_Persistent(inhale ? "SCUBA_BREATHE_IN" : "SCUBA_BREATHE_OUT", &m_ScubaSound, &initParams);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(inhale ? atStringHash("SCUBA_BREATHE_IN") : atStringHash("SCUBA_BREATHE_OUT"), &initParams, m_ScubaSound, m_Ped, eNoGlobalSoundEntity););

		//visual effect
		CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(m_Ped);
		if (pVfxPedInfo==NULL || inhale)
		{
			return;
		}

		m_ShouldUpdateScubaBubblePtFX = true;
	}
}

void audPedAudioEntity::StartElectrocutionSound()
{
	audSoundInitParams initParams;
	initParams.EnvironmentGroup = GetEnvironmentGroup(true);
	initParams.TrackEntityPosition = true;
	initParams.AllowLoad = false;
	CreateAndPlaySound_Persistent("PLAYER_ELECTROCUTION_SOUND", &m_ElectrocutionSound, &initParams);
}

void audPedAudioEntity::IsBeingElectrocuted(bool isElectocuted) 
{
	m_IsBeingElectrocuted = isElectocuted; 
	if(isElectocuted)
		m_TimeElectrocutionLastSet = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audPedAudioEntity::InitTennisVocalizationRaveSettings()
{
	TennisVocalizationSettings* tennisSettings = audNorthAudioEngine::GetObject<TennisVocalizationSettings>("TENNIS_VFX_SETTINGS");
	if(tennisSettings)
	{
		g_TennisLiteNullWeight = tennisSettings->Lite.NullWeight;
		g_TennisLiteLiteWeight = tennisSettings->Lite.LiteWeight;
		g_TennisLiteMedWeight = tennisSettings->Lite.MedWeight;
		g_TennisLiteStrongWeight = tennisSettings->Lite.StrongWeight;
		g_TennisLiteWeightTotal = g_TennisLiteNullWeight + g_TennisLiteLiteWeight + g_TennisLiteMedWeight + g_TennisLiteStrongWeight;

		g_TennisMedNullWeight = tennisSettings->Med.NullWeight;
		g_TennisMedLiteWeight = tennisSettings->Med.LiteWeight;
		g_TennisMedMedWeight = tennisSettings->Med.MedWeight;
		g_TennisMedStrongWeight = tennisSettings->Med.StrongWeight;
		g_TennisMedWeightTotal = g_TennisMedNullWeight + g_TennisMedLiteWeight + g_TennisMedMedWeight + g_TennisMedStrongWeight;

		g_TennisStrongNullWeight = tennisSettings->Strong.NullWeight;
		g_TennisStrongLiteWeight = tennisSettings->Strong.LiteWeight;
		g_TennisStrongMedWeight = tennisSettings->Strong.MedWeight;
		g_TennisStrongStrongWeight = tennisSettings->Strong.StrongWeight;
		g_TennisStrongWeightTotal = g_TennisStrongNullWeight + g_TennisStrongLiteWeight + g_TennisStrongMedWeight + g_TennisStrongStrongWeight;
	}
}

void audPedAudioEntity::MeleeOverrideHandler(u32 hash, void *context)
{
	if(!sm_KnuckleOverrideSoundSet.IsInitialised())
	{
		sm_KnuckleOverrideSoundSet.Init(ATSTRINGHASH("KNUCKLE_OVERRIDE_SOUNDS", 0x30F165A4));
	}
	// This is a bit of a hack to get the already tagged anim event FIST_TO_FACE and select the correct sound if wearing the knuckle duster.
	CPed *ped = (CPed*)context;
	if (ped && ped->GetPedIntelligence())
	{
		audMetadataRef soundRef = g_NullSoundRef;
		CTaskMelee* pTask = ped->GetPedIntelligence()->GetTaskMelee();
		if(pTask)
		{
			CWeapon *weapon = NULL;
			CPed *attacker = (CPed*)pTask->GetTargetEntity();
			if(attacker && attacker->GetWeaponManager())
			{
				weapon = attacker->GetWeaponManager()->GetEquippedWeapon();

			}
			if(ped->GetWeaponManager() && (!weapon || weapon->GetWeaponHash() != ATSTRINGHASH("WEAPON_KNUCKLE", 0xD8DF3C3C)))
			{
				weapon = ped->GetWeaponManager()->GetEquippedWeapon();
			}
		// check for the water cases first.
		//u32 slowMoHitSoundNameHash = g_NullSoundHash;
			if(weapon)
			{
				if(weapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_KNUCKLE", 0xD8DF3C3C))
				{
					soundRef = sm_KnuckleOverrideSoundSet.Find(hash);
				}
			}
		}
		Vector3 position = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
		ped->GetBonePosition(position,BONETAG_HEAD);
#if __BANK
		if(sm_ShowMeleeDebugInfo)
		{
			char buf[256];
			formatf (buf,"Melee sound playing : %s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(hash));
			naDisplayf("%s",buf);
		}
#endif
		audSoundInitParams initParams;
		initParams.Position = position;
		initParams.EnvironmentGroup = ped->GetAudioEnvironmentGroup();
		initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_MeleeDamageToClothVol.CalculateValue(0.5f));
		if( soundRef != g_NullSoundRef)
		{
			ped->GetPedAudioEntity()->CreateAndPlaySound(soundRef,&initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("KNUCKLE_OVERRIDE_SOUNDS", 0x30F165A4),hash, &initParams, ped, NULL, eNoGlobalSoundEntity));
		}
		else
		{
			ped->GetPedAudioEntity()->CreateAndPlaySound(hash,&initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hash, &initParams, ped, NULL, eNoGlobalSoundEntity));
		}
	}
}

void audPedAudioEntity::MeleeHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;
	if (ped)
	{
		CTaskMelee* pTask = ped->GetPedIntelligence()->GetTaskMelee();
		if(pTask)
		{
			CPed *attacker = (CPed*)pTask->GetTargetEntity();
			if(attacker)
			{
				u32 audioHash = ATSTRINGHASH("MELEE_COMBAT_KNIFE", 0x204875EF);
				audioHash = audPedAudioEntity::ValidateMeleeWeapon(audioHash, attacker);

				//See if we have a special hit sound for this melee combat attack.
				u32 hitSoundNameHash = 0;
				u32 slowMoHitSoundNameHash = g_NullSoundHash;

				Vector3 position = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());
				MeleeCombatSettings *meleeSettings = audNorthAudioEngine::GetObject<MeleeCombatSettings>(audioHash);
				if(meleeSettings)
				{
					switch(hash)
					{
					case 0xDFEA0B45:// MELEE_HEAD_HIT
						hitSoundNameHash = meleeSettings->HeadTakeDown;
						slowMoHitSoundNameHash = meleeSettings->SlowMoHeadTakeDown;
						ped->GetBonePosition(position,BONETAG_HEAD);
						break;

					case 0xDEF82ADD:// MELEE_BODY_HIT
						hitSoundNameHash = meleeSettings->BodyTakeDown;
						slowMoHitSoundNameHash = meleeSettings->SlowMoBodyTakeDown;
						ped->GetBonePosition(position,BONETAG_SPINE0);
						break;
					default:
						naAssertf(false,"current audio tag not supported");
						break;
					}
#if __BANK
					if(sm_ShowMeleeDebugInfo)
					{
						char buf[256];
						formatf (buf,"Melee sound playing : %s",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(hitSoundNameHash));
						naDisplayf("%s",buf);
					}
#endif
					if(hitSoundNameHash > 0)
					{
#if __BANK
						if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && slowMoHitSoundNameHash != g_NullSoundHash)
						{
							hitSoundNameHash = slowMoHitSoundNameHash;
						}
#endif

						audSoundInitParams initParams;
						initParams.Position = position;
						initParams.EnvironmentGroup = attacker->GetAudioEnvironmentGroup();
						initParams.Pitch = (ped->GetPedModelInfo()->IsMale() ? 0 : 600);
						attacker->GetPedAudioEntity()->CreateAndPlaySound(hitSoundNameHash, &initParams);

						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hitSoundNameHash, &initParams, attacker, NULL, eNoGlobalSoundEntity, slowMoHitSoundNameHash));
					}

					if(meleeSettings->PedResponseSound > 0)
					{
						u32 pedResponseSound = meleeSettings->PedResponseSound;						

#if __BANK || GTA_REPLAY
						u32 slowMoResponseSound = meleeSettings->SlowMoPedResponseSound;

#if __BANK
						if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && slowMoResponseSound != g_NullSoundHash)
						{
							pedResponseSound = slowMoResponseSound;
						}
#endif
#endif

						audSoundInitParams initParams;
						initParams.Position = position;
						initParams.EnvironmentGroup = ped->GetAudioEnvironmentGroup();
						initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_MeleeDamageToClothVol.CalculateValue(0.5f));
						ped->GetPedAudioEntity()->CreateAndPlaySound(pedResponseSound,&initParams);

						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(pedResponseSound, &initParams, ped, NULL, eNoGlobalSoundEntity, slowMoResponseSound));
					}
				}
			}
		}
	}
}


void audPedAudioEntity::MeleeSwingHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;
	if (ped)
	{
		const ClothAudioSettings *upperCloth = ped->GetPedAudioEntity()->GetFootStepAudio().GetCachedUpperBodyClothSounds();
		audSoundSet meleeSoundSet;
		if(upperCloth && upperCloth->MeleeSwingSound)
		{
			meleeSoundSet.Init(upperCloth->MeleeSwingSound);
			audMetadataRef meleeSwingSoundRef = meleeSoundSet.Find(hash);

			Vector3 position = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());

			audSoundInitParams initParams;
			initParams.Position = position;
			initParams.EnvironmentGroup = ped->GetAudioEnvironmentGroup();
			ped->GetPedAudioEntity()->CreateAndPlaySound(meleeSwingSoundRef, &initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(upperCloth->MeleeSwingSound, hash, &initParams, ped));
		}
	}
}


void audPedAudioEntity::HandleAnimEventFlag(u32 hash)
{
	sm_PedAnimActionTable.InvokeActionHandler(hash, m_Ped);
}

audPedAudioEntity::audPedAudioEntity()
{
	m_Ped = NULL;
	m_EnvironmentGroup = NULL;
	m_ActiveScenario = NULL;
	m_MolotovFuseSound = NULL;
	m_AmbientAnimLoop = NULL;
	m_RingToneSound = NULL;
	m_GarbledTwoWaySound = NULL;
	m_TimeLastStartedMobileCall = 0;
	m_LastWaterSplashTime = 0;
	m_TrouserSwishSound = NULL;
	m_MobileRingSound = NULL;
	m_LastBigSplashTime = 0;
	m_WasInWater = false;
	m_PrevScenario = -1;
	m_LastFallToDeathTime = 0;
	REPLAY_ONLY(m_forceUpdateVariablesForReplay = false;)
	m_LastMeleeMaterial = 0;
	m_LastMeleeMaterialImpactTime = 0;
	m_LastMeleeHitTime = 0;
	m_LastMeleeCollisionWasPed = false;
	m_PedWasFiringForReplay = false;
	m_PedTriggerAutoFireStopForReplay = false;
}

audPedAudioEntity::~audPedAudioEntity()
{
	if(m_Ped)
	{
		DYNAMICMIXER.RemoveEntityFromMixGroup(m_Ped);
	}
	
	RemoveEnvironmentGroup(naAudioEntity::SHUTDOWN);

	StopRingTone_Internal();

	if(m_ActiveScenario)
	{
		m_ActiveScenario->RemovePed(this);
		m_ActiveScenario = NULL;
	}

//	audRadioSlot::FindAndStopEmitter(&m_RadioEmitter);
}

void audPedAudioEntity::Init(CPed *ped)
{
	naAssertf(ped, "Attempting to init an audPedAudioEntity with a null ped.");
	naAudioEntity::Init();
	if(ped->IsLocalPlayer())
	{
		audEntity::SetName("audPedAudioEntity - player");
	}
	else
	{
		audEntity::SetName("audPedAudioEntity");
	}

	naAssertf(!m_Ped || m_Ped.Get() == ped, "PedAudioEntity already references a ped :  %p and it's getting initialize with a different one  %p",m_Ped.Get(),ped); 
	m_Ped = ped;
	m_LastVehicle = NULL;
	m_BJVehicle = NULL;
	m_GunfightVolOffset = 0.f;
	m_WeaponInventoryListener.Init(ped);

	if(	ped->GetInventory() )
		ped->GetInventory()->RegisterListener(&m_WeaponInventoryListener);

	//m_LastClimbOutOfWaterTime = 0;
	//m_WasInWater = false;
	//m_RadioEmitter.SetEmitter(ped);

	m_EnvironmentGroup = NULL;

	f32 increaseDecreaseRate = 1.0f / (f32)g_MaxMolotovVelocityRangeTimeMs;
	m_MolotovVelocitySmoother.Init(increaseDecreaseRate, true);

	m_BoneBreakState = 0;
	m_LastCarImpactTime = 0;
	m_LastCarOverUnderTime = 0;
	m_LastBulletImpactTime = 0;
	m_LastMeleeImpactTime = 0;
	
	m_LastTimeShellCasingPlayed = 0;
	m_LastImpactTime = 0;
	m_LastHeadImpactTime = 0;
	m_LastBigdeathImpactTime = 0;
	m_LastTimeInVehicle = 0;
	m_WasInVehicle = false;
	m_WasInVehicleAndNowInAir = false;
	m_PlayerMixGroupIndex = -1;
	m_LastTimeOnGround = 0;
	m_LastScrapeDebrisTime = 0;
	m_LastStickyBombTime = 0;
	m_LastImpactMag = 0.f;
	m_LastScrapeTime = 0;
	m_LastRappelStopTime = 0;
	m_LastTimeValidGroundWasTrain = 0;

	m_ArrestingLoop = NULL;
	m_MovingWindSound = NULL;

	m_ParachuteScene = NULL;
	m_SkyDiveScene = NULL;
	m_ParachuteSound = NULL;
	m_ParachuteRainSound = NULL;
	m_FreeFallSound = NULL;
	m_ParachuteDeploySound = NULL;
	m_HeartBeatSound = NULL;
	m_HeartbeatTimeSprinting = 0;

	m_PlayerWhistleSound = NULL;
	m_ElectrocutionSound = NULL;
	m_TimeElectrocutionLastSet = 0;

	m_ScubaSound = NULL;

	m_LockedOnSound = NULL;
	m_AcquringLockSound = NULL;
	m_NotLockedOnSound = NULL;
	m_RailgunHumSound = NULL;
	m_RappelLoopSound = NULL;
	m_RailgunHumSoundHash = 0u;
	m_ParachuteState = 0;

	m_TimeOfLastPainBreath = 0;
	m_LastPainBreathWasInhale = false;

	m_WeaponAnimEvent = 0;

	m_WasInCombat = false;
	m_WasTarget = false;

	m_HasPlayedDeathImpact = false;
	m_HasPlayedRunOver = false;
	m_HasPlayedBigAnimalImpact = false;
	m_HasToTriggerBreathSound = false;
	m_BreathRate = 0.f;

	m_EntityRandomSeed = audEngineUtil::GetRandomFloat();

	m_RingtoneSoundRef = g_NullSoundRef;
	m_RingtoneSlotID = -1;
	m_WaitingForRingToneSlot = false;
	m_IsDLCRingtone = false;
	m_RingToneTimeLimit = -1;
	m_RingToneLoopLimit = -1;
	m_ShouldPlayRingTone = false;
	m_TriggerRingtoneAsHUDSound = false;
	m_IsBeingElectrocuted = false;
	m_IsStaggering = false;
	m_IsStaggerFalling = false;
	m_HasPlayedInitialStaggerSounds = false;
	m_HasPlayedSecondStaggerSound = false;
	m_HasJustBeenCarJacked = false;
	m_NeedsToDoScrapeEnd = false;
	m_LastVehicleContactTime = 0;
	m_LastEntityImpactTime = 0;
	m_LastCarJackTime = 0;
	m_ShouldUpdateScubaBubblePtFX = false;
	m_PlayedKillShotAudio = false;

	m_PedUnderCover = false;

	//Init footstep audio 
	m_FootStepAudio.Init(this);

	m_EnvironmentGroupLifeTime = sm_LifeTimePedEnvGroup;

#if GTA_REPLAY
	m_RailgunHumSoundRecorded = false;
	m_RingtoneSoundSetHash = g_NullSoundHash;
	m_RingtoneSoundHash = g_NullSoundHash;
	m_RingToneSoundStartTime = 0;
	m_ReplayVel.Zero();
#endif
}

void audPedAudioEntity::Resurrect()
{
	m_PlayedKillShotAudio = false;
}

void audPedAudioEntity::InitOcclusion(bool forceUpdate)
{
	CreateEnvironmentGroup("Peds");

	if(naVerifyf(m_EnvironmentGroup, "Couldn't allocate occlusion group for audPedAudioEntity: call to naEnvironmentGroup::AllocateEnvironmentGroup() returned null"))
	{
		m_EnvironmentGroup->Init(this, sm_CheapDistance, 2000);
		if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled())
		{
			m_EnvironmentGroup->SetUsePortalOcclusion(true);
			m_EnvironmentGroup->SetMaxPathDepth(audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());
			if(forceUpdate)
			{
				m_EnvironmentGroup->ForceSourceEnvironmentUpdate(m_Ped);
			}
		}
	}
}
naEnvironmentGroup* audPedAudioEntity::GetEnvironmentGroup(bool create) 		
{
	if(create)
	{
		// Reset the life time
#if __BANK 
		if ( g_ResetEnvGroupLifeTime )
		{
			ResetEnvironmentGroupLifeTime();
		}
#endif
		if (!m_EnvironmentGroup)
		{
			m_EnvironmentGroupLifeTime = sm_LifeTimePedEnvGroup;
			InitOcclusion(true);
		}
	}
	return m_EnvironmentGroup; 
}

void audPedAudioEntity::ResetEnvironmentGroupLifeTime()
{
	if(m_EnvironmentGroup)
	{
		m_EnvironmentGroupLifeTime = sm_LifeTimePedEnvGroup;
	}
}

void audPedAudioEntity::AllocateVariableBlock()
{
	if(HasEntityVariableBlock())
	{
		return;
	}
	AllocateEntityVariableBlock(ATSTRINGHASH("PED_VARIABLES", 0x549DD7FB));
	if(HasEntityVariableBlock())
	{
		SetEntityVariable(ATSTRINGHASH("randomSeed", 0x69AB332B), m_EntityRandomSeed);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedAudioEntity::SelectRingTone()
{
	if(m_Ped && m_Ped->GetSpeechAudioEntity())
	{
		PedVoiceGroups* voiceGroup = audNorthAudioEngine::GetObject<PedVoiceGroups>(m_Ped->GetSpeechAudioEntity()->GetPedVoiceGroup());
		if(voiceGroup)
		{
			SoundHashList* ringtoneSounds = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObject<SoundHashList>(voiceGroup->RingtoneSounds); 
			if(naVerifyf(ringtoneSounds && ringtoneSounds->numSoundHashes > 0, "PVG has no ringtone sounds."))
			{
				if(naVerifyf(ringtoneSounds->CurrentSoundIdx < ringtoneSounds->numSoundHashes, "Invalid ringtone sound index."))
				{
					m_RingtoneSoundRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(ringtoneSounds->SoundHashes[ringtoneSounds->CurrentSoundIdx].SoundHash);
#if GTA_REPLAY
					m_RingtoneSoundHash = ringtoneSounds->SoundHashes[ringtoneSounds->CurrentSoundIdx].SoundHash;
					m_RingtoneSoundSetHash = g_NullSoundHash;
#endif
					ringtoneSounds->CurrentSoundIdx = (ringtoneSounds->CurrentSoundIdx + 1) % ringtoneSounds->numSoundHashes;
				}
			}
		}
	}
}

void audPedAudioEntity::PlayRingTone(s32 timeLimit, bool triggerAsHudSound)
{
	if(!g_AudioEngine.IsAudioEnabled())
		return;

	m_TriggerRingtoneAsHUDSound = triggerAsHudSound;
	m_ShouldPlayRingTone = true;
	m_RingToneTimeLimit = timeLimit;
	m_RingToneLoopLimit = -1;

	if(m_RingtoneSoundRef == g_NullSoundRef)
		SelectRingTone();
}

void audPedAudioEntity::StopRingTone()
{
	m_ShouldPlayRingTone = false;
	m_RingToneTimeLimit = -1;
}

void audPedAudioEntity::PlayRingTone_Internal()
{
	if(m_RingToneSound)
	{
		return;
	}

	if(m_RingtoneSlotID == -1)
	{
		bool hadToStop = false;

		// Extra high priority for player peds as script will be requesting this
		const f32 priority = m_Ped->IsLocalPlayer()? 100.0f : 3.0f;
		m_RingtoneSlotID = g_SpeechManager.GetAmbientSpeechSlot(NULL, &hadToStop, priority);
		if(m_RingtoneSlotID == -1)
		{
			//couldn't get a slot...cancel playing
			m_ShouldPlayRingTone = false;
			return;
		}
		g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(m_RingtoneSlotID, 0, 
#if __BANK
			"RINGTONE",
#endif
			0);
		if(hadToStop)
		{
			m_WaitingForRingToneSlot = true;
			return;
		}
	}
	else if(m_WaitingForRingToneSlot && !g_SpeechManager.IsSlotVacant(m_RingtoneSlotID))
		return;

	m_WaitingForRingToneSlot = false;

	audWaveSlot* waveSlot = g_SpeechManager.GetAmbientWaveSlotFromId(m_RingtoneSlotID);
	if (!waveSlot)
	{
		Assertf(0, "Got null waveslot trying to start ringtone sound with ID %i.", m_RingtoneSlotID);
		g_SpeechManager.FreeAmbientSpeechSlot(m_RingtoneSlotID, true);
		m_RingtoneSlotID = -1;
		m_ShouldPlayRingTone = false;
		return;
	}

	audSoundInitParams initParams;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = GetEnvironmentGroup(true);
	initParams.WaveSlot = waveSlot;

	if(m_TriggerRingtoneAsHUDSound)
	{
		initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("HUD", 0x944FA9D9));
	}
	
	CreateSound_PersistentReference(m_RingtoneSoundRef, (audSound**)&m_RingToneSound, &initParams);

	if (!m_RingToneSound)
	{
		g_SpeechManager.FreeAmbientSpeechSlot(m_RingtoneSlotID, true);
		m_RingtoneSlotID = -1;
		m_ShouldPlayRingTone = false;
		return;
	}

	if(m_RingToneLoopLimit < 0)
		m_RingToneLoopLimit = g_MaxCellPhoneLoops;
	else
		m_RingToneLoopLimit--;

	if(m_RingToneLoopLimit <= 0)
	{
		StopRingTone();
	}
	else
	{
		m_RingToneSound->PrepareAndPlay(waveSlot, true, m_RingToneTimeLimit);

#if GTA_REPLAY
		m_RingToneSoundStartTime = fwTimer::GetReplayTimeInMilliseconds();
#endif
	}
}

void audPedAudioEntity::StopRingTone_Internal()
{
	if(m_RingToneSound)
	{
		m_RingToneSound->StopAndForget();
	}
	if(m_WaitingForRingToneSlot)
	{
		m_RingtoneSlotID = -1;
	}

	if(m_RingtoneSlotID >= 0)
	{
		g_SpeechManager.FreeAmbientSpeechSlot(m_RingtoneSlotID, true);
		m_RingtoneSlotID = -1;
	}
}

bool audPedAudioEntity::IsRingTonePlaying()
{
	return (m_RingToneSound != NULL || m_ShouldPlayRingTone);
}

bool audPedAudioEntity::GetShouldDuckRadioForPlayerRingtone()
{ 
	return g_AudioEngine.GetTimeInMilliseconds() - sm_LastTimePlayerRingtoneWasPlaying < 250; 
}

void audPedAudioEntity::PlayGarbledTwoWay()
{
	if (!m_GarbledTwoWaySound)
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		CreateSound_PersistentReference(ATSTRINGHASH("MOBILE_TWO_WAY_GARBLED", 0xEC511933), &m_GarbledTwoWaySound, &initParams);
		if(m_GarbledTwoWaySound)
		{
			m_GarbledTwoWaySound->PrepareAndPlay();

			if(m_Ped)
			{
				REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(ATSTRINGHASH("MOBILE_TWO_WAY_GARBLED", 0xEC511933), &initParams, m_GarbledTwoWaySound, m_Ped, eNoGlobalSoundEntity);)
			}
		}
	}
}

void audPedAudioEntity::StopGarbledTwoWay()
{
	if(m_GarbledTwoWaySound)
	{
		m_GarbledTwoWaySound->StopAndForget();
	}
}

bool audPedAudioEntity::IsGarbledTwoWayPlaying()
{
	return (m_GarbledTwoWaySound!=NULL);
}

bool audPedAudioEntity::IsFreeForAMobileCall()
{
	if ((m_TimeLastStartedMobileCall + 300) < fwTimer::GetTimeInMilliseconds())
	{
		return true;
	}
	return false;
}

void audPedAudioEntity::StartedMobileCall()
{
	m_TimeLastStartedMobileCall = fwTimer::GetTimeInMilliseconds();
}

void audPedAudioEntity::PlayArrestingLoop()
{
	if(!m_ArrestingLoop)
	{
		audSoundInitParams initParams; 
		initParams.Tracker = m_Ped->GetPlaceableTracker();
		CreateAndPlaySound_Persistent(ATSTRINGHASH("CNC_ARRESTING", 0x65BAAD3CU),&m_ArrestingLoop,&initParams);
	}
}
void audPedAudioEntity::StopArrestingLoop()
{
	if(m_ArrestingLoop)
	{
		m_ArrestingLoop->StopAndForget();
	}
}
void audPedAudioEntity::PlayCuffingSounds()
{
	audSoundInitParams initParams; 
	initParams.Tracker = m_Ped->GetPlaceableTracker();
	CreateAndPlaySound(ATSTRINGHASH("CNC_ARRESTING_HALFWAY", 0xD9D7CD4AU),&initParams);
	CreateAndPlaySound(ATSTRINGHASH("CNC_ARRESTING_SUCCESS", 0x285881F1U),&initParams);

}
void audPedAudioEntity::UpdateClass()
{
	audPedFootStepAudio::UpdateClass();

	UpdateMichaelSpeacialAbilityForPeds();

	u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(timeInMs - sm_TimeOfLastBirdTakeOff < g_MaxTimeToTrackBirdTakeOff)
		UpdateBirdTakingOffSounds(timeInMs);

#if __BANK
	if(g_DebugDrawScenarioManager)
	{
		g_PedScenarioManager.DebugDraw();
	}
#endif

	const CPed* player = FindPlayerPed();
	if(player && (!player->IsMale() || player->GetPedType() == PEDTYPE_ANIMAL))
	{
		audSpeechSound::SetExclusionPrefix("NFG");
	}
	else
	{
		audSpeechSound::SetExclusionPrefix(NULL);
	}
}

void audPedAudioEntity::Update()
{
	if( naVerifyf(m_Ped,"Null ped reference on pedaudioentity, this shouldn't happen."))
	{

		m_RightHandCachedPos = VECTOR3_TO_VEC3V(m_Ped->GetBonePositionCached(BONETAG_R_HAND));
		m_LeftHandCachedPos = VECTOR3_TO_VEC3V(m_Ped->GetBonePositionCached(BONETAG_L_HAND));


		const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

#if __DEV
		if(g_AudioDebugEntity == m_Ped)
		{
			if (m_EnvironmentGroup)
			{
				m_EnvironmentGroup->SetAsDebugEnvironmentGroup(true);
			}
			SetAsDebugEntity(true);
			if(g_BreakpointDebugPedUpdate)
			{
				__debugbreak();
			}
		}
		else
		{
			if (m_EnvironmentGroup)
			{
				m_EnvironmentGroup->SetAsDebugEnvironmentGroup(false);
			}
			SetAsDebugEntity(false);
		}
#endif
#if !__FINAL
		if(PARAM_audiodesigner.Get())
		{
			InitTennisVocalizationRaveSettings();
		}
#endif
		// Update ped variables IS_PLAYER and PED_TYPE.
		ScalarV distanceToVolListSqd = g_AudioEngine.GetEnvironment().ComputeSqdDistanceRelativeToVolumeListenerV(m_Ped->GetTransform().GetPosition());
		if (m_Ped->IsLocalPlayer() || IsLessThanOrEqual(distanceToVolListSqd, ScalarV(sm_PedHighLODDistanceThresholdSqd)).Getb() REPLAY_ONLY(|| m_forceUpdateVariablesForReplay))
		{
			REPLAY_ONLY(m_forceUpdateVariablesForReplay = false);
			m_Ped->GetPlaceableTracker()->CalculateOrientation();
			if(!HasEntityVariableBlock())
			{
				AllocateVariableBlock();
			}
			UpdatePedVariables();

			m_FootStepAudio.Update();

			// Ped in water
			if(!m_Ped->GetCapsuleInfo()->IsFish() && !m_WasInWater && m_Ped->GetIsInWater())
			{
				m_FootStepAudio.TriggerFallInWaterSplash();
			}
			m_WasInWater = m_Ped->GetIsInWater();

			if(m_Ped->GetWeaponManager())
			{
				// Equipped molotov
				CWeapon *weapon = m_Ped->GetWeaponManager()->GetEquippedWeapon();
				CObject *weaponObject =  m_Ped->GetWeaponManager()->GetEquippedWeaponObject(); 

				if(m_Ped->GetWeaponManager()->GetEquippedWeaponHash() == ATSTRINGHASH("WEAPON_MOLOTOV", 0x24B17070) && (!m_Ped->GetIsInVehicle() || ( weapon && weaponObject && weaponObject->GetIsVisible())))
				{
					UpdateEquippedMolotov();
				}
				else
				{
					if(m_MolotovFuseSound)
					{
						m_MolotovFuseSound->StopAndForget();
					}
				}
			}
			else
			{
				if(m_MolotovFuseSound)
				{
					m_MolotovFuseSound->StopAndForget();
				}
			}

			// Ped staggering
			if(m_IsStaggering)
			{
				UpdateStaggering();
			}

			if(m_ShouldPlayRingTone)
			{
				PlayRingTone_Internal();
			}
			else
			{
				StopRingTone_Internal();
			}

			// Ped electrocuted
			if(m_IsBeingElectrocuted)
			{
				if(!m_ElectrocutionSound)
				{
					StartElectrocutionSound();
				}
				//safety check
				if(now - m_TimeElectrocutionLastSet > 2000)
				{
					m_IsBeingElectrocuted = false;
					if(m_ElectrocutionSound)
						m_ElectrocutionSound->StopAndForget();
				}
			}
			else
			{
				if(m_ElectrocutionSound)
					m_ElectrocutionSound->StopAndForget();
			}

			// Heart beat and breathing
			if(!m_Ped->IsLocalPlayer())
			{
				//for non-player ped's update pained breathing
				UpdateBreathing();
			}
			else
			{
				if(m_RingToneSound)
				{
#if GTA_REPLAY
					if(CReplayMgr::ShouldRecord())
					{
						CReplayMgr::RecordPersistantFx<CPacketSoundPhoneRing>(
							CPacketSoundPhoneRing(m_RingtoneSoundSetHash, m_RingtoneSoundHash, m_TriggerRingtoneAsHUDSound, m_RingToneTimeLimit, fwTimer::GetReplayTimeInMilliseconds() - m_RingToneSoundStartTime),
							CTrackedEventInfo<replaytrackedSound>(m_RingToneSound), 
							m_Ped, true);
					}
#endif
					sm_LastTimePlayerRingtoneWasPlaying = g_AudioEngine.GetTimeInMilliseconds();
				}
				UpdateHeartbeat();
			}

			if(m_Ped->GetUsingRagdoll() && m_LastVehicleContact && now - m_LastVehicleContactTime < g_LastVehicleContactTimeForForcedRagdollImpact) 
			{
				audVehicleCollisionContext collisionContext;
				collisionContext.impactMag = m_LastVehicleContact->GetVelocity().Mag();
				collisionContext.collisionEvent.otherComponent = RAGDOLL_SPINE0;
				Vector3 pos = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
				PlayVehicleImpact(pos, m_LastVehicleContact, &collisionContext); 
			}
		}else
		{
			ReleaseVariableBlock();
			if(m_MolotovFuseSound)
			{
				m_MolotovFuseSound->StopAndForget();
			}
			m_IsStaggering = false;
			StopRingTone_Internal();
			if(m_ElectrocutionSound)
				m_ElectrocutionSound->StopAndForget();
		}

		if(m_ShouldUpdateScubaBubblePtFX && m_ScubaSound)
		{
			int boneIndex = m_Ped->GetBoneIndexFromBoneTag(BONETAG_HEAD);
			if (boneIndex!=BONETAG_INVALID)
			{
				g_vfxPed.UpdatePtFxPedBreathWater(m_Ped, boneIndex);
			}
		}

		//if(!m_Ped->GetPedIntelligence()->IsPedSwimming() && m_WasInWater)
		//{
		//	if(m_LastClimbOutOfWaterTime + 1000 < timeInMs)
		//	{
		//		audSoundInitParams initParams;
		//		initParams.EnvironmentGroup = m_EnvironmentGroup;
		//		initParams.TrackEntityPosition = true;
		//		CreateAndPlaySound(ATSTRINGHASH("PED_CLIMB_OUT", 0xCF6E6D23), &initParams);
		//		m_LastClimbOutOfWaterTime = timeInMs;
		//	}
		//}

		//m_WasInWater = m_Ped->GetPedIntelligence()->IsPedSwimming();


		// See if we're the player, and update source environment more quickly if it is
		// Also, if we're in a car, piggy-back on the car's occlusion group.
		//naCErrorf(m_EnvironmentGroup, "No occlusion group set on audPedAudioEntity during Update");
		if (m_EnvironmentGroup && m_Ped->IsPlayer())
		{
			m_EnvironmentGroup->SetSourceEnvironmentUpdates(200);
		}
		else if (m_EnvironmentGroup)
		{
			m_EnvironmentGroup->SetSourceEnvironmentUpdates(1000);
		}

		if (m_EnvironmentGroup && m_Ped && m_Ped->GetVehiclePedInside())
		{
			m_EnvironmentGroup->SetLinkedEnvironmentGroup((naEnvironmentGroup*)(const_cast<CVehicle*>(m_Ped->GetVehiclePedInside())->GetVehicleAudioEntity()->GetEnvironmentGroup()));
		}
		else if (m_EnvironmentGroup)
		{
			m_EnvironmentGroup->SetLinkedEnvironmentGroup(NULL);
		}

		if( m_Ped && m_Ped->GetIsInVehicle())
		{
			m_LastVehicle = m_Ped->GetMyVehicle();
			m_BJVehicle = NULL;
		}

		// don't occlude the player (TODO case this for camera modes)
		if (m_EnvironmentGroup && m_Ped->IsLocalPlayer())
		{ 
			m_EnvironmentGroup->SetNotOccluded(true);
		}
		else if (m_EnvironmentGroup)
		{
			m_EnvironmentGroup->SetNotOccluded(false);
		}

		if(now - m_LastScrapeTime > 100 && m_NeedsToDoScrapeEnd)
		{
			if(m_Ped->IsLocalPlayer() && now - m_LastScrapeDebrisTime > g_PedScrapeDebrisTime && m_Ped->GetFootstepHelper().IsWalkingOnASlope())
			{
				if(audEngineUtil::ResolveProbability(CPedFootStepHelper::GetSlopeDebrisProb()))
				{
					Vec3V downSlopeDirection (V_ONE_WZERO);
					m_Ped->GetFootstepHelper().GetSlopeDirection(downSlopeDirection);
					m_Ped->GetPedAudioEntity()->GetFootStepAudio().AddSlopeDebrisEvent(AUD_FOOTSTEP_WALK_L,downSlopeDirection, m_Ped->GetFootstepHelper().GetSlopeAngle());
					m_LastScrapeDebrisTime = now;
				}
			}
			m_NeedsToDoScrapeEnd = false;
		}

		if(m_Ped->GetUsingRagdoll() && m_LastVehicleContact && now - m_LastVehicleContactTime < g_LastVehicleContactTimeForForcedRagdollImpact) 
		{
			audVehicleCollisionContext collisionContext;
			collisionContext.impactMag = m_LastVehicleContact->GetVelocity().Mag();
			collisionContext.collisionEvent.otherComponent = RAGDOLL_SPINE0;
			Vector3 pos = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
			PlayVehicleImpact(pos, m_LastVehicleContact, &collisionContext); 
		}


		if(m_Ped->IsLocalPlayer())
		{
			UpdateMovingWindSounds();

			const CWeaponInfo *pWeaponInfo = m_Ped->GetWeaponManager() ? m_Ped->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
			if(pWeaponInfo && pWeaponInfo->GetAudioHash() == ATSTRINGHASH("AUDIO_ITEM_HOMING_LAUNCHER", 0x2C383A3C))
			{
				audSoundSet homingLauncherSet;
				homingLauncherSet.Init(ATSTRINGHASH("HOMING_LAUNCHER_SET", 0xC97C7ED4));
				if(naVerifyf(homingLauncherSet.IsInitialised(), "Homing launcher set isn't present")) 
				{
					if(m_Ped->GetPlayerInfo()->GetTargeting().GetOnFootHomingLockOnState(m_Ped) == CPlayerPedTargeting::OFH_ACQUIRING_LOCK_ON)
					{
						if(m_LockedOnSound)
						{
							m_LockedOnSound->StopAndForget();
						}
						if(m_NotLockedOnSound)
						{
							m_NotLockedOnSound->StopAndForget();
						}
						if(!m_AcquringLockSound)
						{
							CreateAndPlaySound_Persistent(homingLauncherSet.Find(ATSTRINGHASH("aquiring_lock", 0xCEA6E9E8)), &m_AcquringLockSound);
						}
					}
					else if(m_Ped->GetPlayerInfo()->GetTargeting().GetOnFootHomingLockOnState(m_Ped) == CPlayerPedTargeting::OFH_NOT_LOCKED)
					{
						if(m_AcquringLockSound)
						{
							m_AcquringLockSound->StopAndForget();
						}
						if(m_LockedOnSound)
						{
							m_LockedOnSound->StopAndForget();
						}
						if(!m_NotLockedOnSound)
						{
							CreateAndPlaySound_Persistent(homingLauncherSet.Find(ATSTRINGHASH("green_lock", 0xF515AF1D)), &m_NotLockedOnSound);
						}
					}
					else if(m_Ped->GetPlayerInfo()->GetTargeting().GetOnFootHomingLockOnState(m_Ped) == CPlayerPedTargeting::OFH_LOCKED_ON)
					{
						if(m_AcquringLockSound)
						{
							m_AcquringLockSound->StopAndForget();
						}
						if(m_NotLockedOnSound)
						{
							m_NotLockedOnSound->StopAndForget();
						}
						if(!m_LockedOnSound)
						{
							CreateAndPlaySound_Persistent(homingLauncherSet.Find(ATSTRINGHASH("locked_on", 0xB970427D)), &m_LockedOnSound);
						}
					}
					else
					{
						if(m_LockedOnSound)
						{
							m_LockedOnSound->StopAndForget();
						}
						if(m_AcquringLockSound)
						{
							m_AcquringLockSound->StopAndForget();
						}
						if(m_NotLockedOnSound)
						{
							m_NotLockedOnSound->StopAndForget();
						}
					}
				}
			}
			else 
			{
				if(m_LockedOnSound)
				{
					m_LockedOnSound->StopAndForget();
				}
				if(m_AcquringLockSound)
				{
					m_AcquringLockSound->StopAndForget();
				}
				if(m_NotLockedOnSound)
				{
					m_NotLockedOnSound->StopAndForget();
				}
			}
		}
	}
}

void audPedAudioEntity::UpdateRappel()
{
	bool isDescending = false;

	if (m_Ped->GetPedResetFlag(CPED_RESET_FLAG_IsRappelling) BANK_ONLY(|| g_ForceRappel))
	{
		if (NetworkInterface::IsGameInProgress() && m_Ped->IsControlledByLocalOrNetworkPlayer() && !CReplayMgr::IsEditModeActive())
		{
			const f32 velocity = m_Ped->GetVelocity().z;			
			isDescending = velocity < -1.f BANK_ONLY(|| g_ForceRappelDescend);

			audSoundSet rappelSoundSet;

			if (isDescending)
			{
				if (!m_RappelLoopSound)
				{
					if (rappelSoundSet.Init(ATSTRINGHASH("GTAO_Rappel_Sounds", 0xD3544425)))
					{
						audSoundInitParams initParams;
						initParams.EnvironmentGroup = m_EnvironmentGroup;
						initParams.TrackEntityPosition = true;
						CreateAndPlaySound_Persistent(rappelSoundSet.Find(ATSTRINGHASH("Rappel_Loop", 0x8686E982)), &m_RappelLoopSound, &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(ATSTRINGHASH("GTAO_Rappel_Sounds", 0xD3544425), ATSTRINGHASH("Rappel_Loop", 0x8686E982), &initParams, m_RappelLoopSound, m_Ped, eNoGlobalSoundEntity););
					}
				}
			}
			else if (m_RappelLoopSound)
			{
				m_RappelLoopSound->StopAndForget(true);

				if (fwTimer::GetTimeInMilliseconds() - m_LastRappelStopTime)
				{
					if (rappelSoundSet.Init(ATSTRINGHASH("GTAO_Rappel_Sounds", 0xD3544425)))
					{
						audSoundInitParams initParams;
						initParams.EnvironmentGroup = m_EnvironmentGroup;
						initParams.TrackEntityPosition = true;
						CreateAndPlaySound(rappelSoundSet.Find(ATSTRINGHASH("Rappel_Stop", 0x8C550483)), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("GTAO_Rappel_Sounds", 0xD3544425), ATSTRINGHASH("Rappel_Stop", 0x8C550483), &initParams, m_Ped););
					}
				}

				m_LastRappelStopTime = fwTimer::GetTimeInMilliseconds();
			}
		}
	}	

	if (!isDescending && m_RappelLoopSound)
	{
		m_RappelLoopSound->StopAndForget(true);
	}
}

void audPedAudioEntity::TriggerRappelLand()
{
	if (NetworkInterface::IsGameInProgress() && m_Ped->IsControlledByLocalOrNetworkPlayer())
	{
		audSoundSet rappelSoundSet;

		if (rappelSoundSet.Init(ATSTRINGHASH("GTAO_Rappel_Sounds", 0xD3544425)))
		{
			audSoundInitParams initParams;
			initParams.TrackEntityPosition = true;
			CreateDeferredSound(rappelSoundSet.Find(ATSTRINGHASH("Rappel_Land", 0xEF33F213)), m_Ped, &initParams, true, true);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("GTAO_Rappel_Sounds", 0xD3544425), ATSTRINGHASH("Rappel_Land", 0xEF33F213), &initParams, m_Ped));
		}
	}
}

void audPedAudioEntity::SetCachedBJVehicle(CVehicle* vehicle)
{
	m_BJVehicle = vehicle;
}

CVehicle* audPedAudioEntity::GetBJVehicle()
{
	if(audNorthAudioEngine::IsFirstPersonActive())
	{
		CPed *pLocalPlayer = CGameWorld::FindLocalPlayerSafe();
		if(pLocalPlayer && pLocalPlayer->GetPedAudioEntity())
		{
			CVehicle* cachedBJVehicle = pLocalPlayer->GetPedAudioEntity()->m_BJVehicle;
			return cachedBJVehicle;
		}
	}
	return NULL;
}


void audPedAudioEntity::UpdateEquippedMolotov()
{
	if(m_Ped)
	{
		if(!m_MolotovFuseSound)
		{
			audSoundInitParams initParams;			
			Vec3V pos;

			CPedEquippedWeapon::eAttachPoint weaponHand = m_Ped->GetWeaponManager()->GetEquippedWeaponAttachPoint();
			pos = weaponHand == CPedEquippedWeapon::AP_LeftHand ? m_LeftHandCachedPos : m_RightHandCachedPos;
			
			initParams.EnvironmentGroup = GetEnvironmentGroup(true);
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = (u32)AUD_FOOTSTEP_HAND_R;			
			initParams.Position = VEC3V_TO_VECTOR3(pos);
			CreateAndPlaySound_Persistent(GetFootStepAudio().GetMolotovSounds().Find(ATSTRINGHASH("IDLE", 0x71C21326)), &m_MolotovFuseSound, &initParams);
		}

		if(m_MolotovFuseSound)
		{
			// factor in local bone velocity and ped world velocity
			const f32 velocity = m_MolotovVelocitySmoother.CalculateValue(m_Ped->GetFootstepHelper().GetFootVelocity(RightHand).Getf() * Min((m_Ped->GetVelocity().Mag()*fwTimer::GetTimeStep())*CPedFootStepHelper::sm_MaxBoneVelocityInv,1.f), fwTimer::GetTimeInMilliseconds());

			// factor in wind
			const f32 windVel = g_WeatherAudioEntity.GetWindStrength();

			// derive vol/pitch
			f32 vol = (9.f * velocity) + (4.f * windVel);
			f32 pitch = (4.5f * velocity) + (3.f * windVel);

			m_MolotovFuseSound->SetRequestedVolume(sm_MolotovFuseVolCurve.CalculateValue(vol));
			m_MolotovFuseSound->SetRequestedPitch(audDriverUtil::ConvertRatioToPitch(sm_MolotovFusePitchCurve.CalculateValue(pitch)));
		}
	}
}

bool audPedAudioEntity::HasBeenOnTrainRecently()
{
	u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	return currentTime - m_LastTimeValidGroundWasTrain < 1500;
}

void audPedAudioEntity::UpdateMovingWindSounds()
{
	static const u32 freefallSound = ATSTRINGHASH("PLAYER_AT_SPEED_FREEFALL_MASTER", 0x3EAF2005);

	CPed* ped = static_cast<CPed*>(m_Ped);
	if(m_PedHasReleasedParachute && !ped->GetIsParachuting())
		m_PedHasReleasedParachute = false;

	bool playMovingWind = false; 
	bool playClothElement = true;
	f32 speedSq = 0.0f;

	if(m_Ped && m_Ped->GetIsInVehicle())
	{
		CVehicle* vehicle = m_Ped->GetMyVehicle();

		if(vehicle)
		{
			audVehicleAudioEntity* vehicleAudioEntity = vehicle->GetVehicleAudioEntity();

			if(vehicleAudioEntity)
			{
				if(vehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_CAR || vehicleAudioEntity->GetAudioVehicleType() == AUD_VEHICLE_BICYCLE)
				{
					if(fwTimer::GetTimeInMilliseconds() - vehicleAudioEntity->GetLastTimeOnGround() > 250)
					{
						// Only take the Z velocity so that jumping upwards (eg. from a ramp) doesn't trigger wind
						f32 velocityZ = vehicleAudioEntity->GetCachedVelocity().z;

						if(velocityZ < 0.0f)
						{
							speedSq = velocityZ * velocityZ;

							if(!vehicle->InheritsFromBike() && !vehicle->InheritsFromBicycle() && !vehicle->InheritsFromQuadBike() && !vehicle->InheritsFromAmphibiousQuadBike())
							{
								playClothElement = false;
							}
						}
					}
				}
			}
		}
	}
	else
	{
		speedSq =  m_Ped->GetVelocity().Mag2();
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			speedSq = GetReplayVelocity().Mag2();
		}
#endif
	}
	
	if(speedSq > g_MinMovingWindSpeedSq)
	{
		if(m_Ped->GetIsInVehicle() || !m_Ped->IsOnGround())
		{
			Vector3 pos = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());

			f32 heightBasedOnMap = pos.z - CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(pos.x, pos.y, pos.x + 1.0f, pos.y + 1.0f);
			if(heightBasedOnMap > 100.0f && !m_Ped->GetIsInVehicle())
			{
				playMovingWind = true;
			}
			else
			{
				const float fSecondSurfaceInterp=0.0f;
				bool hitGround = false;
				float ground = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp, pos.x, pos.y, pos.z, &hitGround);

				if(hitGround)
				{
					f32 height = pos.z - ground;
					if(height > 5.0f)
					{
						playMovingWind = true;
					}
				}
			}
		}
	}

	if(playMovingWind && (ped->GetFootstepHelper().GetAudioPedType() != BIRD))
	{
		if(ped && (!ped->GetIsParachuting() || m_PedHasReleasedParachute))
		{
			if(!m_MovingWindSound)
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = GetEnvironmentGroup(true);
				CreateAndPlaySound_Persistent(freefallSound, &m_MovingWindSound, &initParams);
			}

			if(m_MovingWindSound)
			{
				m_MovingWindSound->FindAndSetVariableValue(ATSTRINGHASH("ClothAmount", 0xC917D572), playClothElement? 1.0f : 0.0f);
			}
		}
		else if(m_MovingWindSound)
		{
			m_MovingWindSound->StopAndForget();
		}
	}
	else if(m_MovingWindSound)
	{
		m_MovingWindSound->StopAndForget();
	}
}

void audPedAudioEntity::NotifyVehicleContact(CVehicle *vehicle) 
{
	m_LastVehicleContact = vehicle; 
	m_LastVehicleContactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audPedAudioEntity::UpdatePedVariables()
{
	u32 isPlayer = m_Ped->IsLocalPlayer(); 
	u32 pedType = AUD_PEDTYPE_MAN;
	switch(m_Ped->GetPedType())
	{
	case PEDTYPE_CIVMALE:
		pedType = AUD_PEDTYPE_MAN;
		break;
	case PEDTYPE_PROSTITUTE:
	case PEDTYPE_CIVFEMALE:
		pedType = AUD_PEDTYPE_WOMAN;
		break;
	case PEDTYPE_SWAT:
	case PEDTYPE_COP:
		pedType = AUD_PEDTYPE_COP;
		break;
	case PEDTYPE_GANG_ALBANIAN:
	case PEDTYPE_GANG_BIKER_1:
	case PEDTYPE_GANG_BIKER_2:
	case PEDTYPE_GANG_ITALIAN:
	case PEDTYPE_GANG_RUSSIAN:
	case PEDTYPE_GANG_RUSSIAN_2:
	case PEDTYPE_GANG_IRISH:
	case PEDTYPE_GANG_JAMAICAN:
	case PEDTYPE_GANG_AFRICAN_AMERICAN:
	case PEDTYPE_GANG_KOREAN:
	case PEDTYPE_GANG_CHINESE_JAPANESE:
	case PEDTYPE_GANG_PUERTO_RICAN:
		pedType = AUD_PEDTYPE_GANG;
		break;
	case PEDTYPE_PLAYER_0:
		pedType = AUD_PEDTYPE_MICHAEL;
		break;
	case PEDTYPE_PLAYER_1:
		pedType = AUD_PEDTYPE_FRANKLIN;
		break;
	case PEDTYPE_PLAYER_2:
		pedType = AUD_PEDTYPE_TREVOR;
		break;
	case PEDTYPE_DEALER:
	case PEDTYPE_MEDIC:
	case PEDTYPE_FIRE:
	case PEDTYPE_CRIMINAL:
	case PEDTYPE_BUM:
	case PEDTYPE_SPECIAL:
	case PEDTYPE_MISSION:
	case PEDTYPE_LAST_PEDTYPE:
	case PEDTYPE_ANIMAL:
	case PEDTYPE_ARMY:
		break;
	case PEDTYPE_NETWORK_PLAYER:
		if(m_Ped->IsMale())
		{
			pedType = AUD_PEDTYPE_MAN;
		}
		else 
		{
			pedType = AUD_PEDTYPE_WOMAN;
		}
		break;
	default:
		naAssertf(false,"Wrong ped type");
		break;
	}
	if(HasEntityVariableBlock())
	{
		SetEntityVariable(ATSTRINGHASH("IS_PLAYER", 2204881433),(f32)isPlayer);
		SetEntityVariable(ATSTRINGHASH("PED_TYPE", 1716495284), (f32)pedType);
	}
}

void audPedAudioEntity::OnScenarioStarted(s32 scenarioType, const CScenarioInfo* scenarioInfo)
{
	// Clear any old scenarios we might have lying around
	if(m_ActiveScenario && m_PrevScenario != scenarioType)
	{
		OnScenarioStopped(m_PrevScenario);
	}

	if(!m_ActiveScenario)
	{
		if(scenarioType >= 0)
		{
			if(scenarioInfo)
			{
				m_ActiveScenario = g_PedScenarioManager.StartScenario(this, scenarioInfo->GetHash(), scenarioInfo->GetPropHash());
				m_PrevScenario = scenarioType;
			}
		}
	}
}

void audPedAudioEntity::OnScenarioStopped(s32 scenarioType)
{
	if(m_PrevScenario == scenarioType)
	{
		if(m_ActiveScenario)
		{
			m_ActiveScenario->RemovePed(this);
			m_ActiveScenario = NULL;
		}
	}
}

void audPedAudioEntity::TriggerShellCasing(const audWeaponAudioComponent  &weaponAudio)
{
	if(m_LastTimeShellCasingPlayed + sm_TimeThresholdToTriggerShellCasing < g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) && m_Ped && m_Ped->GetWeaponManager())
	{
		Vec3V pos;

		CPedEquippedWeapon::eAttachPoint weaponHand = m_Ped->GetWeaponManager()->GetEquippedWeaponAttachPoint();
		Vec3V handPos = weaponHand == CPedEquippedWeapon::AP_LeftHand ? m_LeftHandCachedPos : m_RightHandCachedPos;
		pos = m_Ped->GetTransform().Transform(m_Ped->GetTransform().UnTransform(handPos) + Vec3V(g_BulletCasingXOffset, g_BulletCasingYOffset , g_BulletCasingZOffset));

		ScalarV sqdDistance = MagSquared(Subtract(pos,g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()));
		if (IsTrue(sqdDistance <= ScalarV(sm_SqdDistanceThresholdToTriggerShellCasing)))
		{
			const WeaponSettings *weaponSettings = weaponAudio.GetWeaponSettings(m_Ped);

			if(weaponSettings && !m_Ped->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInTheAir))
			{
				CollisionMaterialSettings *material = m_FootStepAudio.GetCurrentMaterialSettings();
				if(m_FootStepAudio.GetAreFeetInWater())
				{
					material = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_WATER", 0xD1CB2165));
				}
				if(material)
				{
					audSoundInitParams initParams;
					initParams.Position = VEC3V_TO_VECTOR3(pos);
					u32 soundHash = g_NullSoundHash;
					u32 slowMoSoundHash = g_NullSoundHash;

					switch(weaponSettings->ShellCasing)
					{
					case SHELLCASING_PLASTIC:
						{
							soundHash = material->PlasticShellCasing;
							slowMoSoundHash = material->PlasticShellCasingSlow;
						}
						break;
					case SHELLCASING_METAL_SMALL:
						{														
							soundHash = material->MetalShellCasingSmall; 
							slowMoSoundHash = material->MetalShellCasingSmallSlow;
						}
						break;
					case SHELLCASING_METAL:
						{
							soundHash = material->MetalShellCasingMedium;
							slowMoSoundHash = material->MetalShellCasingMediumSlow;
						}
						break;
					case SHELLCASING_METAL_LARGE:
						{							
							soundHash = material->MetalShellCasingLarge;
							slowMoSoundHash = material->MetalShellCasingLargeSlow;
						}
						break;
					}

					initParams.Predelay = 500;
					if(AUD_GET_TRISTATE_VALUE(material->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_SHELLCASINGSIGNOREHARDNESS) != AUD_TRISTATE_TRUE)
					{
						initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(material->Hardness*0.01f);
					}

					if(fwTimer::GetTimeWarp()<1.f BANK_ONLY(|| audNorthAudioEngine::IsForcingSlowMoVideoEditor()))
					{
						soundHash = slowMoSoundHash;
					}

					CreateDeferredSound(soundHash, m_Ped, &initParams, true);
					m_LastTimeShellCasingPlayed = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundHash, &initParams, m_Ped, NULL, eNoGlobalSoundEntity, slowMoSoundHash));
				}
			}
		}
	}
}

void audPedAudioEntity::UpdateBreathing() 
{
	//don't play breathing unless health is low
	//TODO: Tie this to limping animation (which doesn't seem to be in place currently)
	if(!g_EnablePainedBreathing || !m_Ped || m_Ped->IsDead() || !m_Ped->GetPedHealthInfo() || !m_Ped->GetSpeechAudioEntity() ||
		m_Ped->GetPedHealthInfo()->GetFatiguedHealthThreshold() < m_Ped->GetHealth() )
		return;

	u32 currentTime = fwTimer::GetTimeInMilliseconds();
	if(m_LastPainBreathWasInhale && currentTime > m_TimeOfLastPainBreath + g_TimeBetweenPainInhaleAndExhale)
	{
		audDamageStats damageStats;
		damageStats.DamageReason = AUD_DAMAGE_REASON_EXHALE;
		m_Ped->GetSpeechAudioEntity()->InflictPain(damageStats);
		m_TimeOfLastPainBreath = currentTime;
		m_LastPainBreathWasInhale = false;
	}
	else if(!m_LastPainBreathWasInhale && currentTime > m_TimeOfLastPainBreath + g_TimeBetweenPainExhaleAndInhale)
	{
		audDamageStats damageStats;
		damageStats.DamageReason = AUD_DAMAGE_REASON_INHALE;
		m_Ped->GetSpeechAudioEntity()->InflictPain(damageStats);
		m_TimeOfLastPainBreath = currentTime;
		m_LastPainBreathWasInhale = true;
	}
}

void audPedAudioEntity::TriggerBreathSound(const f32 breathRate)
{
	m_HasToTriggerBreathSound = true;
	m_BreathRate = breathRate;
}
void audPedAudioEntity::TriggerBreathSound()
{
	m_HasToTriggerBreathSound = false;
	if(m_Ped->IsPlayer() && m_Ped->GetSpeechAudioEntity() && !m_Ped->GetSpeechAudioEntity()->IsPainPlaying() && !m_Ped->GetSpeechAudioEntity()->IsScriptedSpeechPlaying() && 
		!m_Ped->GetSpeechAudioEntity()->IsAmbientSpeechPlaying())
	{		
		audSoundInitParams initParams;
		Vec3V pos;
		m_Ped->GetFootstepHelper().GetPositionForPedSound(ExtraMouth, pos);
		initParams.Position = VEC3V_TO_VECTOR3(pos);
		initParams.u32ClientVar = ExtraMouth;
		initParams.UpdateEntity = true;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);

		// rescale to 0.0f -> 1.0f (where 1.0f means fast)
		const f32 breathRateDiff = VFXPED_BREATH_RATE_BASE - VFXPED_BREATH_RATE_SPRINT;
		const f32 breathRatio = (m_BreathRate - VFXPED_BREATH_RATE_SPRINT) / breathRateDiff;
		
		initParams.SetVariableValue(ATSTRINGHASH("breathRate", 0xF03798D8), 1.0f - breathRatio);

		const u32 soundNameHash = (m_Ped->GetPedModelInfo()->IsMale() ?
										ATSTRINGHASH("PLAYER_BREATH", 0xB310D83) :
										ATSTRINGHASH("FEMALE_PLAYER_BREATH", 0x657B6D2E));
		CreateAndPlaySound(soundNameHash, &initParams);
	}
}

void audPedAudioEntity::StartHeartbeat(bool fromSprinting)
{
	if(g_DisableHeartbeat)
		return;

	if(fromSprinting)
		m_LastTimeHurtFromSprinting = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(!m_Ped || !m_Ped->IsLocalPlayer() || m_HeartBeatSound)
		return;

	u32 soundName = ATSTRINGHASH("MICHAEL_HEARTBEAT_WRAPPER", 0x4A0A887D);
	if (m_Ped->GetPedType() == PEDTYPE_PLAYER_1)
		soundName = ATSTRINGHASH("FRANKLIN_HEARTBEAT_WRAPPER", 0x7F1DA912);
	if (m_Ped->GetPedType() == PEDTYPE_PLAYER_2)
		soundName = ATSTRINGHASH("TREVOR_HEARTBEAT_WRAPPER", 0xA47D4BBF);

	CreateAndPlaySound_Persistent(soundName, &m_HeartBeatSound);
	if(m_HeartBeatSound)
		m_HeartBeatSound->SetRequestedPan(0);

	m_HeartbeatTimeSprinting = 0;
	m_HeartbeatTimeNotSprinting = 0;
}

void audPedAudioEntity::StopHeartbeat()
{
	if(g_DisableHeartbeat)
		return;

	m_HeartbeatTimeSprinting = 0;

	if(m_HeartBeatSound)
		m_HeartBeatSound->StopAndForget();
}

void audPedAudioEntity::UpdateHeartbeat()
{
	if(g_DisableHeartbeat)
		return;

	if(!m_Ped)
	{
		StopHeartbeat();
		return;
	}

	if(m_Ped->GetMotionData() && m_HeartBeatSound)
	{
		bool isPedaling = false;
		if(m_Ped->GetVehiclePedInside())
		{
			CVehicle* vehicle = m_Ped->GetVehiclePedInside();
			if(vehicle->GetVehicleType() != VEHICLE_TYPE_BICYCLE || !m_Ped->GetSpeechAudioEntity())
			{
				StopHeartbeat();
				return;
			}

			if(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_Ped->GetSpeechAudioEntity()->GetLastTimePedaling() < 2000)
				isPedaling = true;
		}

		bool isSprinting = isPedaling || (m_Ped->GetMotionData()->GetIsSprinting() && m_Ped->GetVelocity().Mag2() > 1.0f);
		if(isSprinting)
		{
			m_HeartbeatTimeNotSprinting = 0;
			m_HeartbeatTimeSprinting += fwTimer::GetCamTimeStepInMilliseconds();
		}
		else if(m_HeartbeatTimeSprinting != 0)
		{
			m_HeartbeatTimeNotSprinting += fwTimer::GetCamTimeStepInMilliseconds();
			if(m_HeartbeatTimeSprinting != 0)
			{
				m_HeartbeatTimeSprinting -= fwTimer::GetCamTimeStepInMilliseconds();

				if(m_HeartbeatTimeSprinting > 10000)
					m_HeartbeatTimeSprinting = 0;
			}
		}

		m_HeartbeatTimeSprinting = Clamp(m_HeartbeatTimeSprinting, (u32)0, (u32)10000);

		if(g_DisableHeartbeatFromInjury)
		{
			m_HeartBeatSound->FindAndSetVariableValue(ATSTRINGHASH("HeartBeatTimeRunning", 0xFF4D7F76), (f32)m_HeartbeatTimeSprinting);
			if(m_HeartbeatTimeNotSprinting >= g_TimeNotSprintingBeforeHeartbeatStops || m_Ped->GetHealth() == m_Ped->GetMaxHealth() ||
				g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_LastTimeHurtFromSprinting > g_TimeNotHurtBeforeHeartbeatStops)
			{
				StopHeartbeat();
				return;
			}
		}
	}

	if(!g_DisableHeartbeatFromInjury)
	{
		f32 healthRatio = m_Ped->GetHealth() / m_Ped->GetMaxHealth();
#if __BANK
		f32	applyValue = Max((1.0f-healthRatio)*10000, (f32)m_HeartbeatTimeSprinting);
		if(g_PrintHeartbeatApply)
			naDisplayf("Heartbeat Apply From %s: %f", applyValue == m_HeartbeatTimeSprinting ? "SPRINTING" : "HEALTH", applyValue);
#endif
		if(healthRatio < 0.74f || m_HeartbeatTimeSprinting > 0)
		{
			if(!m_HeartBeatSound)
				StartHeartbeat(false);
			if(m_HeartBeatSound)
			{
				f32	applyValue = Max((1.0f-healthRatio)*10000, (f32)m_HeartbeatTimeSprinting);
				if(applyValue == m_HeartbeatTimeSprinting)
				{
					m_HeartBeatSound->FindAndSetVariableValue(ATSTRINGHASH("HeartBeatTimeRunning", 0xFF4D7F76), applyValue);
					m_HeartBeatSound->FindAndSetVariableValue(ATSTRINGHASH("HeartBeatHealth", 0x295A7FFF), 0.0f);
				}
				else
				{
					m_HeartBeatSound->FindAndSetVariableValue(ATSTRINGHASH("HeartBeatTimeRunning", 0xFF4D7F76), 0.0f);
					m_HeartBeatSound->FindAndSetVariableValue(ATSTRINGHASH("HeartBeatHealth", 0x295A7FFF), applyValue);
				}
			}
		}
		else if(m_HeartBeatSound)
			StopHeartbeat();
	}
}

void audPedAudioEntity::SetWeaponAnimEvent(u32 eventHash)
{
	m_WeaponAnimEvent = eventHash;
}

void audPedAudioEntity::WeaponThrow()
{
	if(m_Ped->GetWeaponManager() && m_Ped->GetWeaponManager()->GetEquippedWeaponHash() == ATSTRINGHASH("WEAPON_MOLOTOV", 0x24B17070))
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.TrackEntityPosition = true;
		CreateAndPlaySound(GetFootStepAudio().GetMolotovSounds().Find(ATSTRINGHASH("THROW", 0xCDEC4431)), &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(GetFootStepAudio().GetMolotovSounds().GetNameHash(), ATSTRINGHASH("THROW", 0xCDEC4431), &initParams, GetOwningEntity()));
	}	
}

BANK_ONLY(bool g_PrintPedAnimEvents = false;)
BANK_ONLY(bool g_PrintFocusPedAnimEvents = false;)

//-------------------------------------------------------------------------------------------------------------------
void audPedAudioEntity::ProcessAnimEvents()
{
	for(int i=0; i< AUDENTITY_NUM_ANIM_EVENTS; i++)
	{
		if(m_AnimEvents[i])
		{
#if __BANK
			if(g_PrintPedAnimEvents)
			{
				naDisplayf("Processing anim event %u on ped", m_AnimEvents[i]);
			}
			if(g_PrintFocusPedAnimEvents && m_Ped == CPedDebugVisualiserMenu::GetFocusPed())
			{
				naDisplayf("Processing anim event %u on focus ped", m_AnimEvents[i]);
			}
#endif

			if(!HasEntityVariableBlock())
			{
				AllocateVariableBlock();
			}

			HandleAnimEventFlag(m_AnimEvents[i]);
			m_AnimEvents[i] = 0;
		}
	}

	if(m_Ped->GetPedAiLod().IsLodFlagSet(CPedAILod::AL_LodTimesliceAnimUpdate))
	{
		m_AnimLoopTimeout = sm_LodAnimLoopTimeout;
	}
	else
	{
		m_AnimLoopTimeout = naAudioEntity::sm_BaseAnimLoopTimeout;
	}

	naAudioEntity::ProcessAnimEvents();
}

void audPedAudioEntity::PlayMeleeCombatBikeSwipe()
{
	if(m_Ped)
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.TrackEntityPosition = true;

		const CWeapon *weapon = m_Ped && m_Ped->GetWeaponManager() ? m_Ped->GetWeaponManager()->GetEquippedWeapon() : NULL;

		u32 weaponHash = weapon ? weapon->GetWeaponHash() : ATSTRINGHASH("UNARMED", 0xE2E6A20D);
		u32 defaultWeaponHash = weapon ? s_DefaultBikeMeleeHitName[weapon->GetWeaponInfo()->GetEffectGroup()] : ATSTRINGHASH("DEFAULT_UNARMED", 0x264760E);

		audMetadataRef swipeSoundRef = g_NullSoundRef;
		u32 soundFieldHash = weaponHash;
		swipeSoundRef = sm_BikeMeleeSwipeSounds.Find(weaponHash);
		if (!swipeSoundRef.IsValid() && weapon &&  weapon->GetWeaponInfo())
		{
			swipeSoundRef = sm_BikeMeleeSwipeSounds.Find(defaultWeaponHash);
			soundFieldHash = defaultWeaponHash;
		}
		if (!swipeSoundRef.IsValid())
		{
			swipeSoundRef = sm_BikeMeleeSwipeSounds.Find(ATSTRINGHASH("DEFAULT_SWIPE", 0xA9722F1E));
			soundFieldHash = ATSTRINGHASH("DEFAULT_SWIPE", 0xA9722F1E);
		}

		CreateAndPlaySound(swipeSoundRef, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_BikeMeleeSwipeSounds.GetNameHash(),soundFieldHash, &initParams, GetOwningEntity()));

		if(m_Ped->IsLocalPlayer())
		{
			// as far as music is concerned, throwing a punch is equivalent to firing a weapon
			g_InteractiveMusicManager.GetIdleMusic().NotifyPlayerFiredWeapon();
		}
	}
}
void audPedAudioEntity::PlayMeleeCombatSwipe(u32 meleeCombatObjectNameHash)
{
	if(m_Ped)
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.TrackEntityPosition = true;
		MeleeCombatSettings *meleeSettings = audNorthAudioEngine::GetObject<MeleeCombatSettings>(meleeCombatObjectNameHash);
		u32 swipeSoundHash = ATSTRINGHASH("MELEE_COMBAT_SWIPE_MULTI", 0xCBB345D8);
		if(meleeSettings)
		{
			if(meleeSettings->SwipeSound != g_NullSoundHash)
			{
				swipeSoundHash = meleeSettings->SwipeSound;
			}
		}
#if __BANK
		if(sm_ShowMeleeDebugInfo)
		{
			char buf[256];
			formatf (buf,"SWIPE : [Audio GO: %s] [Sound: %s]",audNorthAudioEngine::GetMetadataManager().GetObjectName(meleeCombatObjectNameHash)
				,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(swipeSoundHash));
			naDisplayf("%s",buf);
		}
#endif
		CreateAndPlaySound(swipeSoundHash, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(swipeSoundHash, &initParams, GetOwningEntity()));

		if(m_Ped->IsLocalPlayer())
		{
			// as far as music is concerned, throwing a punch is equivalent to firing a weapon
			g_InteractiveMusicManager.GetIdleMusic().NotifyPlayerFiredWeapon();
		}
	}
}
u32 audPedAudioEntity::ValidateMeleeWeapon(const u32 hash, CPed* ped)
{
	if(ped)
	{
		const CWeapon *weapon = ped->GetWeaponManager() ? ped->GetWeaponManager()->GetEquippedWeapon() : NULL;
		// check for the water cases first.
		if(hash == ATSTRINGHASH("MELEE_COMBAT_KNIFE_WATER", 0x94D9DC7B))
		{
			if(weapon && weapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263))
			{
				return ATSTRINGHASH("MELEE_COMBAT_JAB", 0x2773E48);
			}
		}
		else if(hash == ATSTRINGHASH("MELEE_COMBAT_KNIFE_UNDERWATER", 0xF4F8D328))
		{
			if(weapon && weapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263))
			{
				return ATSTRINGHASH("MELEE_COMBAT_JAB_UNDERWATER", 0x80F1DD0C);
			}
		}
		else if(weapon)
		{
			static const u32 weaponTableEntries = 22;
			static const u32 s_WeaponLookUpTable [weaponTableEntries][2] = {
				{ATSTRINGHASH("WEAPON_KNIFE", 0x99B507EA),ATSTRINGHASH("MELEE_COMBAT_KNIFE", 0x204875EF)},
				{ATSTRINGHASH("WEAPON_NIGHTSTICK", 0x678B81B1),ATSTRINGHASH("MELEE_COMBAT_NIGHTSTICK", 0xC8CBAFF0)},
				{ATSTRINGHASH("WEAPON_HAMMER", 0x4E875F73),ATSTRINGHASH("MELEE_COMBAT_HAMMER", 0x4584D0A4)},
				{ATSTRINGHASH("WEAPON_BAT", 0x958A4A8F),ATSTRINGHASH("MELEE_COMBAT_WOODEN_BAT", 0xE194EE43)},
				{ATSTRINGHASH("WEAPON_CROWBAR", 0x84BD7BFD),ATSTRINGHASH("MELEE_COMBAT_CROWBAR", 0xD693DC89)},
				{ATSTRINGHASH("WEAPON_GOLFCLUB", 0x440E4788),ATSTRINGHASH("MELEE_COMBAT_GOLFCLUB", 0x7DF7AEA5)},
				{ATSTRINGHASH("WEAPON_HATCHET", 0xF9DCBF2D),ATSTRINGHASH("MELEE_COMBAT_HATCHET", 0x46B8413E)},
				{ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263),ATSTRINGHASH("MELEE_COMBAT_FINISH_HOOK", 0x31E7D382)},
				{ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263),ATSTRINGHASH("MELEE_COMBAT_HOOK", 0x4C055607)},
				{ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263),ATSTRINGHASH("MELEE_COMBAT_HOOK_POOR", 0x45590B71)},
				{ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263),ATSTRINGHASH("MELEE_COMBAT_JAB", 0x2773E48)},
				{ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263),ATSTRINGHASH("MELEE_COMBAT_LOW", 0xE228D7A6)},
				{ATSTRINGHASH("WEAPON_UNARMED", 0xA2719263),ATSTRINGHASH("MELEE_COMBAT_UPPERCUT", 0xCEBD065A)},
				{ATSTRINGHASH("WEAPON_KNUCKLE", 0xD8DF3C3C),ATSTRINGHASH("MELEE_COMBAT_KNUCKLE_DUSTER", 0x348F9B15)},
				{ATSTRINGHASH("WEAPON_MACHETE", 0xDD5DF8D9),ATSTRINGHASH("MELEE_COMBAT_MACHETE", 0x56FA8E77)},
				{ATSTRINGHASH("WEAPON_FLASHLIGHT", 0x8BB05FD7),ATSTRINGHASH("MELEE_COMBAT_FLASHLIGHT", 0x19C3C5D6)},
				{ATSTRINGHASH("WEAPON_BATTLEAXE", 0xCD274149),ATSTRINGHASH("MELEE_COMBAT_BATTLEAXE", 0xFA095C2F)},
				{ATSTRINGHASH("WEAPON_POOLCUE", 0x94117305),ATSTRINGHASH("MELEE_COMBAT_POOLCUE", 0x337F883E)},
				{ATSTRINGHASH("WEAPON_WRENCH", 0x19044EE0),ATSTRINGHASH("MELEE_COMBAT_WRENCH", 0x7A6B2A77)},
				{ATSTRINGHASH("WEAPON_SWITCHBLADE", 0xDFE37640),ATSTRINGHASH("MELEE_COMBAT_SWITCHBLADE", 0xB192D7F8)},
				{ATSTRINGHASH("WEAPON_BATON", 0xD3B9662E),ATSTRINGHASH("arc1_melee_combat_baton", 0x3839AEF4)},
				{ATSTRINGHASH("WEAPON_STONE_HATCHET", 0x3813FC08),ATSTRINGHASH("MELEE_COMBAT_CEREMONIAL_HATCHET", 0x586041AB)}};

				bool validate = false; 
				for (u32 i = 0; i < weaponTableEntries; i++)
				{
					if(hash == s_WeaponLookUpTable[i][1])
					{
						validate = true;
						break;
					}
				}
				if( validate )
				{
					for (u32 i = 0; i < weaponTableEntries; i++)
					{
						if(weapon->GetWeaponHash() == s_WeaponLookUpTable[i][0])
						{
							return s_WeaponLookUpTable[i][1];
						}
					}
				}
		}
	}
	return hash;
}

u32 g_MinTimeForSameMeleeMaterial = 100;

void audPedAudioEntity::PlayMeleeCombatHit(u32 meleeCombatObjectNameHash, const WorldProbe::CShapeTestHitPoint& refResult, const CActionResult * actionResult
#if __BANK
										   , const char* soundName
#endif 
										   )
{

	const CWeapon *weapon = m_Ped && m_Ped->GetWeaponManager() ? m_Ped->GetWeaponManager()->GetEquippedWeapon() : NULL;

	bool validateWeapon = true;

	if(weapon && weapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_KNUCKLE", 0xD8DF3C3C))
	{
		eAnimBoneTag strikeTag = actionResult->GetStrikeBoneSet()->GetStrikeBoneTag(0);
		if(strikeTag < BONETAG_R_UPPERARM || strikeTag > BONETAG_R_FINGER42)
		{
			validateWeapon = false;
		}
	}
	if(validateWeapon)
	{
		meleeCombatObjectNameHash = audPedAudioEntity::ValidateMeleeWeapon(meleeCombatObjectNameHash,m_Ped);
	}

	bool hasHitPed = false;
	const phInst *instance = refResult.GetHitInst();
	CEntity* entity = NULL;
	if(instance)
	{
		entity = CPhysics::GetEntityFromInst((phInst *)instance);
		if(entity)
		{
			hasHitPed = (entity->GetType() == ENTITY_TYPE_PED);
		}
	}

	// Don't trigger alternating ground/ped collisions in quick succession (possible when eg. striking the ground through a ped)
	if (hasHitPed != m_LastMeleeCollisionWasPed && fwTimer::GetTimeInMilliseconds() - m_LastMeleeHitTime < g_MinMeleeRepeatTime)
	{
		return;
	}

	//See if we have a special hit sound for this melee combat attack.
	u32 hitSoundNameHash = 0;
	u32 slowMoHitSoundHash = g_NullSoundHash;
	u32 slowMoPresuckSoundHash = g_NullSoundHash;
	s32 slowMoPresuckTime = g_NullSoundHash;
	MeleeCombatSettings *meleeSettings = audNorthAudioEngine::GetObject<MeleeCombatSettings>(meleeCombatObjectNameHash);
	if(meleeSettings)
	{
		if(hasHitPed)
		{
			hitSoundNameHash = meleeSettings->PedHitSound;
			slowMoHitSoundHash = meleeSettings->SlowMoPedHitSound;

			if(((CPed*)entity)->GetPedType() == PEDTYPE_ANIMAL)
			{
				hitSoundNameHash = meleeSettings->SmallAnimalHitSound;
				slowMoHitSoundHash = meleeSettings->SlowMoSmallAnimalHitSound;

				if(((CPed*)entity)->GetBoundRadius() > sm_SmallAnimalThreshold)
				{
					hitSoundNameHash = meleeSettings->BigAnimalHitSound;
					slowMoHitSoundHash = meleeSettings->SlowMoBigAnimalHitSound;
				}
			}
		}
		else
		{
			hitSoundNameHash = meleeSettings->GeneralHitSound;
		}
	}
	// always use the weapon strike sound if there is one, so that pistol whipping etc works
	if(m_Ped)
	{
		const CWeapon *weapon = m_Ped->GetWeaponManager() ? m_Ped->GetWeaponManager()->GetEquippedWeapon() : NULL;
		if(weapon && 
			(hitSoundNameHash == 0 || hitSoundNameHash == sm_MeleeOverrideSoundNameHash || // override is used for knife
			(!weapon->GetWeaponInfo()->GetIsUnarmed() && !weapon->GetWeaponInfo()->GetIsMelee())))
		{
			const WeaponSettings *weaponSettings = weapon->GetAudioComponent().GetWeaponSettings();
			if(weaponSettings)
			{
				if(hasHitPed)
				{
					hitSoundNameHash = weaponSettings->PedStrikeSound;
					slowMoHitSoundHash = weaponSettings->SlowMoPedStrikeSound;
					slowMoPresuckSoundHash = weaponSettings->SlowMoPedStrikeSoundPresuck;
					slowMoPresuckTime = weaponSettings->SlowMoPedStrikeSoundPresuckTime;

					if(((CPed*)entity)->GetPedType() == PEDTYPE_ANIMAL)
					{
						hitSoundNameHash = weaponSettings->SmallAnimalStrikeSound;
						slowMoHitSoundHash = weaponSettings->SlowMoSmallAnimalStrikeSound;
						slowMoPresuckSoundHash = weaponSettings->SlowMoSmallAnimalStrikeSoundPresuck;
						slowMoPresuckTime = weaponSettings->SlowMoSmallAnimalStrikeSoundPresuckTime;

						if(((CPed*)entity)->GetBoundRadius() > sm_SmallAnimalThreshold)
						{
							hitSoundNameHash = weaponSettings->BigAnimalStrikeSound;
							slowMoHitSoundHash = weaponSettings->SlowMoBigAnimalStrikeSound;
							slowMoPresuckSoundHash = weaponSettings->SlowMoBigAnimalStrikeSoundPresuck;
							slowMoPresuckTime = weaponSettings->SlowMoBigAnimalStrikeSoundPresuckTime;
						}
					}
				}
				else
				{
					hitSoundNameHash = weaponSettings->GeneralStrikeSound;
				}
			}
		}
#if __BANK
		if(sm_ShowMeleeDebugInfo)
		{
			char buf[256];
			formatf (buf,"%s : [AR: %s] [Tag: %s] [Audio GO: %s] [Sound: %s] time %u",(hasHitPed ? "PED HIT": "GENERAL HIT")
				,actionResult->GetName(),soundName,audNorthAudioEngine::GetMetadataManager().GetObjectName(meleeCombatObjectNameHash)
				,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(hitSoundNameHash), fwTimer::GetTimeInMilliseconds());
			naDisplayf("%s",buf);
		}

#endif
		const CEntity * entity = refResult.GetHitEntity();
		u32 component = refResult.GetHitComponent();
		// extract the material data from the collision and see if we have a model specific override
		phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(refResult.GetHitMaterialId());
		CollisionMaterialSettings * materialSettings = g_CollisionAudioEntity.GetMaterialOverride(entity, g_audCollisionMaterials[(s32)unpackedMtlIdA], component);

		if(materialSettings)
		{
			if((fwTimer::GetTimeInMilliseconds() - m_LastMeleeMaterialImpactTime < g_MinTimeForSameMeleeMaterial) && m_LastMeleeMaterial == materialSettings->HardImpact)
			{
				return;
			}
			m_LastMeleeMaterial = materialSettings->HardImpact;
		}
		else if((fwTimer::GetTimeInMilliseconds() - m_LastMeleeMaterialImpactTime < g_MinTimeForSameMeleeMaterial))
		{
			return;
		}

		m_LastMeleeMaterialImpactTime = fwTimer::GetTimeInMilliseconds();


		if(hitSoundNameHash > 0)
		{
			if(hasHitPed || (materialSettings && 
				((AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_PEDSARESOLID) == AUD_TRISTATE_TRUE) ||
				(materialSettings->HardImpact != g_NullSoundHash || materialSettings->SolidImpact != g_NullSoundHash))))
			{
				Vector3 position = refResult.GetHitPosition();

				audSoundInitParams initParams;
				//initParams.EnvironmentGroup = GetEnvironmentGroup(true);
				initParams.Position = position;
				initParams.Pitch = (m_Ped->GetPedModelInfo()->IsMale() ? 0 : 600);

#if __BANK
				if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && slowMoHitSoundHash != g_NullSoundHash)
				{
					hitSoundNameHash = slowMoHitSoundHash;
				}
#endif
				CreateDeferredSound(hitSoundNameHash,m_Ped, &initParams,true);
				m_LastMeleeHitTime = fwTimer::GetTimeInMilliseconds();
				m_LastMeleeCollisionWasPed = hasHitPed;

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::ReplayRecordSound(hitSoundNameHash, &initParams, GetOwningEntity(), NULL, eNoGlobalSoundEntity, slowMoHitSoundHash);

					if(slowMoPresuckSoundHash != g_NullSoundHash)
					{
						CReplayMgr::ReplayPresuckSound(slowMoPresuckSoundHash, slowMoPresuckTime, 0u, GetOwningEntity());
					}
				}
#endif
			}

		}
		if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor)
		{
			CreateAndPlaySound(ATSTRINGHASH("TREVOR_MELEE_MASTER", 0x7D8EFBF2));
		}
		if(hasHitPed)
		{
			((CPed*)CPhysics::GetEntityFromInst((phInst *)instance))->GetPedAudioEntity()->TriggerMeleeResponse(meleeCombatObjectNameHash,refResult, 0.5f);
		}
		else
		{
			audNorthAudioEngine::GetGtaEnvironment()->SpikeResonanceObjects(naEnvironment::sm_MeleeResoImpulse, refResult.GetHitPosition(), naEnvironment::sm_WeaponsResonanceDistance,entity);

			bool isPunch = false, isKick = false;

			if(actionResult->GetSoundHashString().GetHash() == ATSTRINGHASH("MELEE_COMBAT_KICK", 0x201B2A5A))
			{
				isKick = true;
				if(GetFootStepAudio().GetShoeSettings()->ShoeType != SHOE_TYPE_BARE && materialSettings && (materialSettings->HardImpact != g_NullSoundHash || materialSettings->SolidImpact != g_NullSoundHash))
				{
					audSoundInitParams initParams;
					initParams.Position = VEC3V_TO_VECTOR3(refResult.GetPosition());
					CreateDeferredSound(GetFootStepAudio().GetShoeSettings()->Kick, m_Ped, &initParams, true);

					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(GetFootStepAudio().GetShoeSettings()->Kick, &initParams, GetOwningEntity()));
				}
			}
			else if(weapon && weapon->GetWeaponInfo()->GetIsUnarmed())
			{
				isPunch = true;
			}

			g_CollisionAudioEntity.ReportMeleeCollision(&refResult,1.f, m_Ped, false, false, isPunch, isKick);			

		}
	}
}

void audPedAudioEntity::PlayBikeMeleeCombatHit(const WorldProbe::CShapeTestHitPoint& refResult)
{
	const CWeapon *weapon = m_Ped && m_Ped->GetWeaponManager() ? m_Ped->GetWeaponManager()->GetEquippedWeapon() : NULL;
	
	u32 weaponHash = weapon ? weapon->GetWeaponHash() : ATSTRINGHASH("UNARMED", 0xE2E6A20D);
	u32 defaultWeaponHash = weapon ? s_DefaultBikeMeleeHitName[weapon->GetWeaponInfo()->GetEffectGroup()] : ATSTRINGHASH("DEFAULT_UNARMED", 0x264760E);

	bool hasHitPed = false;
	const phInst *instance = refResult.GetHitInst();
	CEntity* entity = NULL;
	if(instance)
	{
		entity = CPhysics::GetEntityFromInst((phInst *)instance);
		if(entity)
		{
			hasHitPed = (entity->GetType() == ENTITY_TYPE_PED);
		}
	}

	audMetadataRef hitSoundRef = g_NullSoundRef;
	u32 soundsetHash = g_NullSoundHash;
	u32 hitSoundHash = g_NullSoundHash;
	if(hasHitPed)
	{
		hitSoundRef = sm_BikeMeleeHitPedSounds.Find(weaponHash);
		hitSoundHash = weaponHash;
		soundsetHash = sm_BikeMeleeHitPedSounds.GetNameHash();
		if (!hitSoundRef.IsValid())
		{
			hitSoundRef = sm_BikeMeleeHitPedSounds.Find(defaultWeaponHash);
			hitSoundHash = defaultWeaponHash;
		}
	}
	else
	{
		hitSoundRef = sm_BikeMeleeHitSounds.Find(weaponHash);
		hitSoundHash = weaponHash;
		soundsetHash = sm_BikeMeleeHitSounds.GetNameHash();
		if (!hitSoundRef.IsValid())
		{
			hitSoundRef = sm_BikeMeleeHitSounds.Find(defaultWeaponHash);
			hitSoundHash = defaultWeaponHash;
		}
	}

	//const CEntity * entity = refResult.GetHitEntity();
	u32 component = refResult.GetHitComponent();
	// extract the material data from the collision and see if we have a model specific override
	phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(refResult.GetHitMaterialId());
	CollisionMaterialSettings * materialSettings = g_CollisionAudioEntity.GetMaterialOverride(entity, g_audCollisionMaterials[(s32)unpackedMtlIdA], component);
	
	if(materialSettings)
	{
		if((fwTimer::GetTimeInMilliseconds() - m_LastMeleeMaterialImpactTime < g_MinTimeForSameMeleeMaterial) && m_LastMeleeMaterial == materialSettings->HardImpact)
		{
			return;
		}
		m_LastMeleeMaterial = materialSettings->HardImpact;
	}
	else if((fwTimer::GetTimeInMilliseconds() - m_LastMeleeMaterialImpactTime < g_MinTimeForSameMeleeMaterial))
	{
		return;
	}
	
	m_LastMeleeMaterialImpactTime = fwTimer::GetTimeInMilliseconds();


	if (hitSoundRef.IsValid())
	{
		if(hasHitPed || (materialSettings && 
			((AUD_GET_TRISTATE_VALUE(materialSettings->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_PEDSARESOLID) == AUD_TRISTATE_TRUE) ||
			(materialSettings->HardImpact != g_NullSoundHash || materialSettings->SolidImpact != g_NullSoundHash))))
		{
			Vector3 position = refResult.GetHitPosition();

			audSoundInitParams initParams;
			//initParams.EnvironmentGroup = GetEnvironmentGroup(true);
			initParams.Position = position;
			initParams.Pitch = (m_Ped->GetPedModelInfo()->IsMale() ? 0 : 600);

			CreateDeferredSound(hitSoundRef,m_Ped, &initParams,true);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::ReplayRecordSound(soundsetHash,hitSoundHash, &initParams, GetOwningEntity());
			}
#endif
		}

	}

	audNorthAudioEngine::GetGtaEnvironment()->SpikeResonanceObjects(naEnvironment::sm_MeleeResoImpulse, refResult.GetHitPosition(), naEnvironment::sm_WeaponsResonanceDistance,entity);

	bool isPunch = weaponHash == ATSTRINGHASH("UNARMED", 0xE2E6A20D);
	bool isKick = false;
// 
// 	if(actionResult->GetSoundHashString().GetHash() == ATSTRINGHASH("MELEE_COMBAT_KICK", 0x201B2A5A))
// 	{
// 		isKick = true;
// 		if(GetFootStepAudio().GetShoeSettings()->ShoeType != SHOE_TYPE_BARE && materialSettings && (materialSettings->HardImpact != g_NullSoundHash || materialSettings->SolidImpact != g_NullSoundHash))
// 		{
// 			audSoundInitParams initParams;
// 			initParams.Position = VEC3V_TO_VECTOR3(refResult.GetPosition());
// 			CreateDeferredSound(GetFootStepAudio().GetShoeSettings()->Kick, m_Ped, &initParams, true);
// 
// 			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(GetFootStepAudio().GetShoeSettings()->Kick, &initParams, GetOwningEntity()));
// 		}
// 	}
// 	else if(weapon && weapon->GetWeaponInfo()->GetIsUnarmed())
// 	{
// 		isPunch = true;
// 	}

	g_CollisionAudioEntity.ReportMeleeCollision(&refResult,1.f, m_Ped, false, false, isPunch, isKick);			

}

void audPedAudioEntity::TriggerMeleeResponse(u32 meleeCombatObjectNameHash,const WorldProbe::CShapeTestHitPoint& refResult,f32 damageDone)
{
	MeleeCombatSettings *settings = audNorthAudioEngine::GetObject<MeleeCombatSettings>(meleeCombatObjectNameHash);

	if(settings && (settings->PedResponseSound > 0))
	{
		audSoundInitParams initParams;
		initParams.Position = refResult.GetHitPosition();
		//initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_MeleeDamageToClothVol.CalculateValue(damageDone));

		u32 soundHash = g_NullSoundHash;
		u32 slowMoSoundHash = g_NullSoundHash;

		if(m_Ped->GetPedType() == PEDTYPE_ANIMAL)
		{
			if(m_Ped->GetBoundRadius() > sm_SmallAnimalThreshold)
			{
				soundHash = settings->BigAnimalResponseSound;
				slowMoSoundHash = settings->SlowMoBigAnimalResponseSound;
			}
			else
			{
				soundHash = settings->SmallAnimalResponseSound;
				slowMoSoundHash = settings->SlowMoSmallAnimalResponseSound;
			}
		}
		else
		{
			soundHash = settings->PedResponseSound;
			slowMoSoundHash = settings->SlowMoPedResponseSound;	
		}

		if(soundHash != g_NullSoundHash)
		{
#if __BANK
			if(audNorthAudioEngine::IsForcingSlowMoVideoEditor() && slowMoSoundHash != g_NullSoundHash)
			{
				soundHash = slowMoSoundHash;
			}
#endif

			CreateDeferredSound(soundHash, m_Ped,&initParams,true);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundHash, &initParams, GetOwningEntity(), NULL, eNoGlobalSoundEntity, slowMoSoundHash));
		}
	}

	m_LastMeleeImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audPedAudioEntity::HandleWeaponFire( bool isSilenced )
{
	// triggered when this ped fires a weapon - if we're playing the molotov sound then stop it
	// it'll be triggered again next frame but it's nice to have a small gap
	if(m_MolotovFuseSound)
	{
		m_MolotovFuseSound->StopAndForget();
	}

	if(m_Ped->IsLocalPlayer())
	{
		g_InteractiveMusicManager.GetIdleMusic().NotifyPlayerFiredWeapon();
		// Send message to the superconductor to stop the QUITE_SCENE
		if(!isSilenced && audSuperConductor::sm_StopQSOnPlayerShot && !NetworkInterface::IsGameInProgress() && m_Ped->GetWeaponManager() && m_Ped->GetWeaponManager()->GetEquippedWeaponInfo() && m_Ped->GetWeaponManager()->GetEquippedWeaponInfo()->GetHash() != WEAPONTYPE_GRENADELAUNCHER )
		{
			ConductorMessageData message;
			message.conductorName = SuperConductor;
			message.message = StopQuietScene;
			message.bExtraInfo = false;
			message.vExtraInfo = Vector3((f32)audSuperConductor::sm_PlayerShotQSFadeOutTime
										,(f32)audSuperConductor::sm_PlayerShotQSDelayTime
										,(f32)audSuperConductor::sm_PlayerShotQSFadeInTime);
			SUPERCONDUCTOR.SendConductorMessage(message);
		}
	}
}

void audPedAudioEntity::StartAmbientAnim(const crClip *UNUSED_PARAM(pClip))
{
	/*naCErrorf(pClip, "Null clip passed into audPedAudioEntity::StartAmbientAnim");
	if(pClip)
	{
		s32 audioHash = 0;
		float fStartPhase = 0.0f;
		float fStopPhase = Min(pClip->Convert30FrameToPhase(1.0f), 1.0f);

		const CClipEventTags::CEventTag<CClipEventTags::CAudioEventTag>* pTag = CClipEventTags::FindLastEventTag<CClipEventTags::CAudioEventTag>(pClip, CClipEventTags::Audio, fStartPhase, fStopPhase);

		if (pTag)
		{
			audioHash = pTag->GetData()->GetId();
		Printf("name = %s tag = %d\n", pClip->GetName(), pTag->GetKey());
		}

		if(audioHash)
		{
			audSoundInitParams params;
			params.EnvironmentGroup = GetEnvironmentGroup(true);
			params.TrackEntityPosition = true;
			params.WaveSlot = sm_ScriptMissionSlot;
			CreateSound_PersistentReference((u32)audioHash, &m_AmbientAnimLoop, &params);
			if(m_AmbientAnimLoop)
			{
				m_AmbientAnimLoop->PrepareAndPlay();
			}
		}
	}*/
}

void audPedAudioEntity::StopAmbientAnim()
{
	if(m_AmbientAnimLoop)
	{
		m_AmbientAnimLoop->StopAndForget();
	}
}

void audPedAudioEntity::UpdateSound(audSound* UNUSED_PARAM(sound), audRequestedSettings *reqSets, u32 UNUSED_PARAM(timeInMs))
{
	Vec3V pos;
	u32 ePos;
	reqSets->GetClientVariable(ePos);
	if(m_Ped && m_Ped->GetWeaponManager())
	{
		if(m_Ped->GetWeaponManager()->GetEquippedWeaponHash() == ATSTRINGHASH("WEAPON_MOLOTOV", 0x24B17070))
		{
			CPedEquippedWeapon::eAttachPoint weaponHand = m_Ped->GetWeaponManager()->GetEquippedWeaponAttachPoint();
			pos = weaponHand == CPedEquippedWeapon::AP_LeftHand ? m_LeftHandCachedPos : m_RightHandCachedPos;
		}
		else
		{
			m_Ped->GetFootstepHelper().GetPositionForPedSound((audFootstepEvent)ePos, pos);
		}
		reqSets->SetPosition(VEC3V_TO_VECTOR3(pos));
	}
}
void audPedAudioEntity::PreUpdateService(u32 UNUSED_PARAM(timeInMs))
{
	if(m_EnvironmentGroup && m_Ped)
	{
		m_EnvironmentGroup->SetSource(m_Ped->GetTransform().GetPosition());
#if __BANK
	if ( g_ShowPedsEnvGroup)
	{
		grcDebugDraw::Sphere(m_Ped->GetTransform().GetPosition() + m_Ped->GetTransform().GetB() + Vec3V(ScalarV(0.5f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(0, 175, 175), true,1);
	}
#endif
	}
	ProcessAnimEvents();

	if( m_EnvironmentGroup )
	{
		PF_INCREMENT(PedEnvGroups);
		// Update the env. group lifetime
		m_EnvironmentGroupLifeTime -= fwTimer::GetTimeStepInMilliseconds();
		m_EnvironmentGroupLifeTime = Max(m_EnvironmentGroupLifeTime,0);
		if ( m_EnvironmentGroupLifeTime <= 0)
		{
			RemoveEnvironmentGroup();
		}
	}

	if(GetEnvironmentGroup())
 	{
		m_PedUnderCover = ((naEnvironmentGroup*)GetEnvironmentGroup())->IsUnderCover();
 		if(m_Ped->IsLocalPlayer())
 		{
 			CreateEnvironmentGroup("PedMixGroup",PLAYER_MIXGROUP);
 			if(naVerifyf(GetEnvironmentGroup(), "Failed to create environment group for local player"))
 			{
 				int mixGroupIdx = GetEnvironmentGroup()->GetMixGroupIndex();
 				if(m_PlayerMixGroupIndex < 0)
 				{
 					if(mixGroupIdx < 0)
 					{
 						DYNAMICMIXER.AddEntityToMixGroup(m_Ped, ATSTRINGHASH("PLAYER_GROUP", 0xC5FA40AC));
 						m_PlayerMixGroupIndex = GetEnvironmentGroup()->GetMixGroupIndex();
 					}
 
 				}
 				else if(mixGroupIdx != m_PlayerMixGroupIndex)
 				{
 					m_PlayerMixGroupIndex = -1;
 					RemoveEnvironmentGroup(PLAYER_MIXGROUP);
 				}
 			}
 		}
 		else if(m_PlayerMixGroupIndex >= 0)
 		{
 			int mixGroupIdx = GetEnvironmentGroup()->GetMixGroupIndex();
 			if(m_PlayerMixGroupIndex == mixGroupIdx)
 			{
 				DYNAMICMIXER.RemoveEntityFromMixGroup(m_Ped);
 			}
 			m_PlayerMixGroupIndex = -1;
 			RemoveEnvironmentGroup(PLAYER_MIXGROUP);
 		}
 	}
	else
	{
		m_PedUnderCover = false;
	}	
//	// script triggered mobile phone
//	u32 ringState = (m_Ped && m_Ped->GetPhoneComponent()) ? m_Ped->GetPhoneComponent()->GetMobileRingState() : 0xff;
//#if __BANK
//	if(g_OverriddenRingtone != 0 && m_Ped && m_Ped->IsLocalPlayer())
//	{
//		ringState = g_OverriddenRingtone;
//	}
//#endif
//	if(ringState != 0xff)
//	{
//		if(!m_MobileRingSound)
//		{
//			char soundName[64];
//			const char *const standardBase = "MOBILE_PHONE_RING_%02d";
//			const char *const monoBase = "MONO_PHONE_RING_%02d";
//			const char *formatStr = standardBase;
//			if(ringState > 10 && (g_ForceMonoRingtones || !m_Ped->IsLocalPlayer()))
//			{
//				formatStr = monoBase;
//			}
//
//			formatf(soundName, sizeof(soundName), formatStr, ringState);
//			audSoundInitParams initParams;
//			initParams.EnvironmentGroup = GetEnvironmentGroup(true);
//			audWaveSlot *waveSlot = audWaveSlot::FindWaveSlot(ATSTRINGHASH("PLAYER_RINGTONE", 0xD2645E26));
//			initParams.WaveSlot = waveSlot;
//
//			if(m_Ped->IsLocalPlayer())
//			{
//				initParams.Pan = 0;
//				initParams.EffectRoute = EFFECT_ROUTE_FRONT_END;
//			}
//			else
//			{
//				initParams.TrackEntityPosition = true;
//			}
//			CreateSound_PersistentReference(soundName, &m_MobileRingSound, &initParams);
//			if(m_MobileRingSound)
//			{
//				m_MobileRingSound->SetRequestedDopplerFactor(0.f);
//				m_MobileRingSound->PrepareAndPlay(waveSlot, true, 2500);
//			}		
//		}
//	}
//	if(m_MobileRingSound)
//	{
//		m_MobileRingSound->SetRequestedVolume(0.f);
//		if(ringState == 0xff)
//		{
//			m_MobileRingSound->StopAndForget();
//		}
//	}
#if __BANK
	if(m_Ped->IsLocalPlayer())
	{
		if(g_ToggleRingtone)
		{
			if(m_ShouldPlayRingTone)
				StopRingTone();
			else
				PlayRingTone();
	
			g_ToggleRingtone = false;
		}
	}
	

#endif
	// pre-update the footstep audio
	m_FootStepAudio.PreUpdate();
}

void audPedAudioEntity::UpdateMichaelSpeacialAbilityForPeds()
{
	bool failed = true;
	CPed *pPlayer = CGameWorld::FindLocalPlayer();
	if( pPlayer && (g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMichael || g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMultiplayer))
	{
		const CPlayerInfo* playerInfo = pPlayer->GetPlayerInfo();
		if(playerInfo)
		{
			const CEntity* target = playerInfo->GetTargeting().GetTarget();
			if(target && (target->GetType() == ENTITY_TYPE_PED))
			{
				CPed* pTargetPed = const_cast<CPed*>((CPed*)target); 
				if(pTargetPed && pTargetPed->GetPedAudioEntity() && !pTargetPed->GetIsDeadOrDying())
				{
					failed = false;
					if(pTargetPed != sm_LastTarget)
					{
						if(sm_LastTarget && sm_LastTarget->GetPedAudioEntity())
						{
							sm_LastTarget->GetPedAudioEntity()->StopSlowMoHeartSound();
						}
						pTargetPed->GetPedAudioEntity()->PlaySlowMoHeartSound();
						sm_LastTarget = pTargetPed;
					}
					pTargetPed->GetPedAudioEntity()->UpdateSlowMoHeartBeatSound();
				}
			}
		}
	}
	if (failed)
	{
		if(sm_LastTarget && sm_LastTarget->GetPedAudioEntity())
		{
			sm_LastTarget->GetPedAudioEntity()->StopSlowMoHeartSound();
		}
	}
}
void audPedAudioEntity::PlaySlowMoHeartSound()
{
	if(!m_HeartBeatSound)
	{
		u32 soundName = ATSTRINGHASH("NPC_HEART_BEAT", 0x52FCCBE4);
		CreateAndPlaySound_Persistent(soundName, &m_HeartBeatSound);
	}
}
void audPedAudioEntity::UpdateSlowMoHeartBeatSound()
{
	CPed *pPlayer = CGameWorld::FindLocalPlayer();

	if(pPlayer && m_HeartBeatSound && pPlayer->GetSpecialAbility())
	{
		m_HeartBeatSound->SetRequestedPan(0);
		m_HeartBeatSound->FindAndSetVariableValue(ATSTRINGHASH("SpecialAbility", 0xADB374E6), (f32)pPlayer->GetSpecialAbility()->GetRemainingNormalized());
	}
}
void audPedAudioEntity::StopSlowMoHeartSound()
{
	if(m_HeartBeatSound)
	{
		m_HeartBeatSound->StopAndForget();
		m_HeartBeatSound = NULL;
	}
}

void audPedAudioEntity::PostUpdate()
{
	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	bool isInTheAir = m_Ped->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir );
	if(!isInTheAir || m_Ped->GetIsInVehicle())
	{
		m_LastTimeOnGround = now;
	}

	if(m_Ped->GetIsInVehicle())
	{
		m_LastTimeInVehicle = now;
		m_WasInVehicle = true;
		m_WasInVehicleAndNowInAir = false;
	}
	else if(m_WasInVehicle)
	{
		m_WasInVehicle = false;
		if(isInTheAir || m_Ped->GetUsingRagdoll())
		{
			m_WasInVehicleAndNowInAir = true;
		}
	}
	else if(m_WasInVehicleAndNowInAir && !m_Ped->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ) && !m_Ped->GetUsingRagdoll())
	{
		m_WasInVehicleAndNowInAir = false;
	}

	if(m_WeaponAnimEvent)
	{
		CWeapon *weapon = m_Ped->GetWeaponManager()->GetEquippedWeapon();
		if(weapon)
		{
			g_WeaponAudioEntity.HandleAnimFireEvent(m_WeaponAnimEvent, weapon, m_Ped);
		}
		m_WeaponAnimEvent = 0;
	}

	ProcessParachuteEvents();

	UpdateRappel();

	if(m_HasToTriggerBreathSound)
	{
		TriggerBreathSound();
	}
	//char txt[256];
	//formatf(txt,"[CurrentX %f[Current Y %f] [Last %f]",m_Ped->GetMotionData()->GetCurrentMbrY(),m_Ped->GetMotionData()->GetCurrentMbrX(),m_LastMoveBlendRatio );
	//grcDebugDraw::AddDebugOutput(Color_white,txt);
	// post-update footstep audio 
	m_FootStepAudio.ProcessEvents();
	UpdateWeaponHum();
}

void audPedAudioEntity::UpdateWeaponHum()
{
	// if the player is wearing the railgun, play the hum sound.
	if (m_Ped && m_Ped->IsLocalPlayer())
	{
		CWeapon *weapon = m_Ped->GetWeaponManager()->GetEquippedWeapon();
		CObject *weaponObject = m_Ped->GetWeaponManager()->GetEquippedWeaponObject();
		const bool isRayGun = weapon && weapon->GetWeaponInfo() && audWeaponAudioEntity::IsRayGun(weapon->GetWeaponInfo()->GetAudioHash());

		if (weapon && weaponObject && weaponObject->GetIsVisible() && (weapon->GetWeaponHash() == ATSTRINGHASH("weapon_railgun", 0x6D544C99) || isRayGun) && (weapon->GetState() == CWeapon::STATE_READY || isRayGun) && weapon->GetAmmoInClip() != 0)//&& !m_Ped->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting) && !m_Ped->GetPedResetFlag(CPED_RESET_FLAG_IsClimbing))
		{
			u32 soundHash = ATSTRINGHASH("SPL_RAILGUN_Hum", 0x14F402F0);

			if (isRayGun)
			{
				const u32 weaponAudioHash = weapon->GetWeaponInfo()->GetAudioHash();

				if (weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RAYMINIGUN", 0x142840C6))
				{
					soundHash = ATSTRINGHASH("DLC_AW_Raygun_Hum_Minigun", 0x5A127FD5);
				}
				else if (weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RAYCARBINE", 0x78E492D8))
				{
					soundHash = ATSTRINGHASH("DLC_AW_Raygun_Hum_Rifle", 0xC905731);
				}
				else if (weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RAYPISTOL", 0xADEC5478))
				{
					soundHash = ATSTRINGHASH("DLC_AW_Raygun_Hum_Pistol", 0x850E5BF7);
				}
			}

			Vec3V weaponPos = weapon->GetEntity()->GetTransform().GetPosition() + (ScalarV(0.6f) *  weapon->GetEntity()->GetTransform().GetRight());

			if (soundHash != m_RailgunHumSoundHash && m_RailgunHumSound)
			{
				m_RailgunHumSound->StopAndForget();
			}

			// play hum sound. 
			if (!m_RailgunHumSound)
			{
				audSoundInitParams initParams;
				initParams.Position = VEC3V_TO_VECTOR3(weaponPos);
				initParams.EnvironmentGroup = weapon->GetEntity()->GetAudioEnvironmentGroup();
				CreateAndPlaySound_Persistent(soundHash, &m_RailgunHumSound, &initParams);
				m_RailgunHumSoundHash = soundHash;
			}
			else
			{
				m_RailgunHumSound->SetRequestedOrientation(weapon->GetEntity()->GetTransform().GetOrientation());
				m_RailgunHumSound->SetRequestedPosition(weaponPos);
			}
#if GTA_REPLAY
			if (CReplayMgr::IsRecording() && m_RailgunHumSound)
			{
				bool update = m_RailgunHumSoundRecorded;
				audSoundInitParams initParams;
				initParams.Position = VEC3V_TO_VECTOR3(weaponPos);
				initParams.EnvironmentGroup = weapon->GetEntity()->GetAudioEnvironmentGroup();
				CReplayMgr::ReplayRecordSoundPersistant(soundHash, &initParams, m_RailgunHumSound, m_Ped, eNoGlobalSoundEntity, ReplaySound::CONTEXT_INVALID, update);
				m_RailgunHumSoundRecorded = true;
			}
#endif
		}
		else if (m_RailgunHumSound)
		{
			REPLAY_ONLY(m_RailgunHumSoundRecorded = false;);
			m_RailgunHumSound->StopAndForget();
		}
	}
}

#if GTA_REPLAY
bool audPedAudioEntity::ReplayIsParachuting()
{
	return (CReplayMgr::IsEditModeActive() && ReplayParachuteExtension::HasExtension(m_Ped) && ReplayParachuteExtension::GetIsParachuting(m_Ped));
}

u8 audPedAudioEntity::ReplayGetParachutingState()
{
	if(CReplayMgr::IsEditModeActive() && ReplayParachuteExtension::HasExtension(m_Ped))
	{
		return  ReplayParachuteExtension::GetParachutingState(m_Ped);
	}
	return 0;
}
#endif // GTA_REPLAY

void audPedAudioEntity::ProcessParachuteEvents() 
{
	if(m_Ped->GetIsParachuting() || m_Ped->GetPedResetFlag(CPED_RESET_FLAG_IsFalling) REPLAY_ONLY(|| ReplayIsParachuting()))
	{
		CTaskParachute* task = (CTaskParachute *)m_Ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PARACHUTE);	
		if(task REPLAY_ONLY(|| ReplayIsParachuting()))
		{
			int state = 0;
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				state = ReplayGetParachutingState();
			}
			else
#endif // GTA_REPLAY
			{
				state = task->GetState();
			}
			
			if (state == CTaskParachute::State_Skydiving || state == CTaskParachute::State_TransitionToSkydive)
			{
				if(m_ParachuteSound)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = GetEnvironmentGroup(true);
					m_ParachuteSound->StopAndForget();
					CreateAndPlaySound(ATSTRINGHASH("PARACHUTE_RELEASE", 0x6C1CA4), &initParams);
				}
				if(m_ParachuteDeploySound)
				{
					m_ParachuteDeploySound->StopAndForget();
				}
				if(!m_FreeFallSound)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = GetEnvironmentGroup(true);
					initParams.TrackEntityPosition = true;

					CreateAndPlaySound_Persistent(ATSTRINGHASH("PARACHUTE_SKYDIVE", 0xF3FC795E), &m_FreeFallSound, &initParams);
				}
				if(!m_SkyDiveScene && m_Ped->IsLocalPlayer())
				{
					DYNAMICMIXER.StartScene(ATSTRINGHASH("PARACHUTES_SKYDIVE_SCENE", 0x49B98E75), &m_SkyDiveScene);
				}
		
			}
			else if(state == CTaskParachute::State_Deploying)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = GetEnvironmentGroup(true);
				initParams.TrackEntityPosition = true;
	
				if(m_FreeFallSound)
				{
					m_FreeFallSound->StopAndForget();
				}

				bool allowParachuteSound = true;
#if GTA_REPLAY
				allowParachuteSound = (CReplayMgr::IsReplayInControlOfWorld() ? CReplayMgr::IsJustPlaying() : true);
#endif
				if(!m_ParachuteSound && allowParachuteSound)
				{
					if(!m_ParachuteDeploySound)
					{
						CreateAndPlaySound_Persistent(ATSTRINGHASH("PARACHUTE_DEPLOY", 0xB12C9B1), &m_ParachuteDeploySound, &initParams);
					}
					CreateAndPlaySound_Persistent(ATSTRINGHASH("PARACHUTE_DESCENT", 0x9231C867), &m_ParachuteSound, &initParams);  
				}
				if(!m_ParachuteScene && m_Ped->IsLocalPlayer())
				{
					DYNAMICMIXER.StartScene(ATSTRINGHASH("PARACHUTES_PARACHUTE_DESCENT_SCENE", 0x9D699F02), &m_ParachuteScene);
					if(!m_ParachuteRainSound && g_weather.GetTimeCycleAdjustedRain() != 0.f)
					{
						CreateAndPlaySound_Persistent(ATSTRINGHASH("PARACHUTE_RAIN", 0x919C72D3), &m_ParachuteRainSound, &initParams);  
					}
				}
				if(m_SkyDiveScene)
				{
					m_SkyDiveScene->Stop();
				}
			}
			else if (state == CTaskParachute::State_Parachuting)
			{
#if GTA_REPLAY
				if(CReplayMgr::IsEditModeActive())
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = GetEnvironmentGroup(true);
					initParams.TrackEntityPosition = true;

					if(!m_ParachuteSound)
					{
						CreateAndPlaySound_Persistent("PARACHUTE_DESCENT", &m_ParachuteSound, &initParams);  
					}
					if(!m_ParachuteScene && m_Ped->IsLocalPlayer())
					{
						DYNAMICMIXER.StartScene(ATSTRINGHASH("PARACHUTES_PARACHUTE_DESCENT_SCENE", 0x9D699F02), &m_ParachuteScene);
						if(!m_ParachuteRainSound && g_weather.GetTimeCycleAdjustedRain() != 0.f)
						{
							CreateAndPlaySound_Persistent(ATSTRINGHASH("PARACHUTE_RAIN", 0x919C72D3), &m_ParachuteRainSound, &initParams);  
						}
					}
				}
#endif
				if(m_SkyDiveScene)
				{
					m_SkyDiveScene->Stop();
				}
			}
			else //land, crashland, streaming 
			{
				if(m_ParachuteSound)
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = GetEnvironmentGroup(true);
						
					m_ParachuteSound->StopAndForget();
					CreateAndPlaySound(ATSTRINGHASH("PARACHUTE_RELEASE", 0x6C1CA4), &initParams);
				}
				if(m_ParachuteScene)
				{
					m_ParachuteScene->Stop();
				}
				if(m_SkyDiveScene)
				{
					m_SkyDiveScene->Stop();
				}
				if(m_FreeFallSound)
				{
					m_FreeFallSound->StopAndForget();
				}
				if(m_ParachuteDeploySound)
				{
					m_ParachuteDeploySound->StopAndForget();
				}
				if(m_ParachuteRainSound)
				{
					m_ParachuteRainSound->StopAndForget();
				}
			}
			m_ParachuteState = (u32)state;
		}
		else
		{ 
			if(m_ParachuteSound || m_FreeFallSound)
			{
				if(m_ParachuteSound)
				{
					m_ParachuteSound->StopAndForget();
				}

				if(m_FreeFallSound)
				{
					m_FreeFallSound->StopAndForget();
				}
			}
			if(m_ParachuteScene)
			{
				m_ParachuteScene->Stop();
			}
			if(m_SkyDiveScene)
			{
				m_SkyDiveScene->Stop();
			}
			if(m_ParachuteDeploySound)
			{
				m_ParachuteDeploySound->StopAndForget();
			}
			if(m_ParachuteRainSound)
			{
				m_ParachuteRainSound->StopAndForget();
			}
		}	
	}
	else if(m_ParachuteSound || m_FreeFallSound || m_ParachuteDeploySound || m_ParachuteRainSound)
	{
		if(m_ParachuteRainSound)
		{
			m_ParachuteRainSound->StopAndForget();
		}
		if(m_ParachuteSound)
		{
			m_ParachuteSound->StopAndForget();
		}
		if(m_FreeFallSound)
		{
			m_FreeFallSound->StopAndForget();
		}
		if(m_ParachuteDeploySound)
		{
			m_ParachuteDeploySound->StopAndForget();
		}
		if(m_ParachuteScene)
		{
			m_ParachuteScene->Stop();
		}
		if(m_SkyDiveScene)
		{
			m_SkyDiveScene->Stop();
		}
	}
}

void audPedAudioEntity::TriggerFallToDeath()
{
	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(now > m_LastFallToDeathTime + 1000)
	{

		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("PED_FALL_TO_DEATH", 0xCBF63745)), &initParams);

		m_LastFallToDeathTime = now;

	}
}

CollisionMaterialSettings* audPedAudioEntity::GetScrapeMaterialSettings()
{
	const ClothAudioSettings * lowerSettings =  m_FootStepAudio.GetCachedLowerBodyClothSounds();
	if(lowerSettings)
	{
		return audNorthAudioEngine::GetObject<CollisionMaterialSettings>(m_FootStepAudio.GetCachedLowerBodyClothSounds()->ScrapeMaterialSettings); 
	}
	return NULL;
}

static const u32 g_BoneBreakSounds[RAGDOLL_NUM_COMPONENTS] =
{
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//0 Buttocks Char_Pelvis 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//1 Thigh_Left Char_L_Thigh 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//2 Shin_Left Char_L_Calf 
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//3 Foot_Left Char_L_Foot 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//4 Thigh_Right Char_R_Thigh 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//5 Shin_Right Char_R_Calf 
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//6 Foot_Right Char_R_Foot 
	ATSTRINGHASH("BONE_BREAKS_TORSO", 0xDBEBEC16),	//7 Spine0 Char_Spine 
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//8 Spine1 Char_Spine1 
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//9 Spine2 Char_Spine2 
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//10 Spine3 Char_Spine3 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//11 Neck Char_Neck 
	ATSTRINGHASH("BONE_BREAKS_HEAD", 0x7B19E39C),	//12 Head Char_Head 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//13 Clavicle_Left Char_L_Clavicle 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//14 Upper_Arm_Left Char_L_UpperArm 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//15 Lower_Arm_Left Char_L_ForeArm 
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//16 Hand_Left Char_L_Hand 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//17 Clavicle_Right Char_R_Clavicle 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//18 Upper_Arm_Right Char_R_UpperArm 
	ATSTRINGHASH("BONE_BREAKS_LIMB", 0x8E34ABE),	//19 Lower_Arm_Right Char_R_ForeArm 
	ATSTRINGHASH("NULL_SOUND", 0xE38FCF16),	//20 Hand_Right Char_R_Hand 
};

static const u32 g_VehicleImpactSounds[RAGDOLL_NUM_COMPONENTS] =
{
	ATSTRINGHASH("PED_VEHICLE_TORSO_IMPACT", 0x3172F806),	//0 Buttocks Char_Pelvis 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//1 Thigh_Left Char_L_Thigh 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//2 Shin_Left Char_L_Calf 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//audStringHash("NULL_SOUND"),	//3 Foot_Left Char_L_Foot 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//4 Thigh_Right Char_R_Thigh 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//5 Shin_Right Char_R_Calf 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//audStringHash("NULL_SOUND"),	//6 Foot_Right Char_R_Foot 
	ATSTRINGHASH("PED_VEHICLE_TORSO_IMPACT", 0x3172F806),	//7 Spine0 Char_Spine 
	ATSTRINGHASH("PED_VEHICLE_TORSO_IMPACT", 0x3172F806),	//8 Spine1 Char_Spine1 
	ATSTRINGHASH("PED_VEHICLE_TORSO_IMPACT", 0x3172F806),	//9 Spine2 Char_Spine2 
	ATSTRINGHASH("PED_VEHICLE_TORSO_IMPACT", 0x3172F806),	//10 Spine3 Char_Spine3 
	ATSTRINGHASH("PED_VEHICLE_TORSO_IMPACT", 0x3172F806),	//11 Neck Char_Neck 
	ATSTRINGHASH("PED_VEHICLE_HEAD_IMPACT", 0xB3299C9C),	//12 Head Char_Head 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//13 Clavicle_Left Char_L_Clavicle 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//14 Upper_Arm_Left Char_L_UpperArm 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//15 Lower_Arm_Left Char_L_ForeArm 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//audStringHash("NULL_SOUND"),	//16 Hand_Left Char_L_Hand 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//17 Clavicle_Right Char_R_Clavicle 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//18 Upper_Arm_Right Char_R_UpperArm 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//19 Lower_Arm_Right Char_R_ForeArm 
	ATSTRINGHASH("PED_VEHICLE_LIMB_IMPACT", 0x95C87517),	//audStringHash("NULL_SOUND"),	//20 Hand_Right Char_R_Hand 
};

void audPedAudioEntity::UpBodyfallForShooting()
{
	m_LastBulletImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

bool audPedAudioEntity::ShouldUpBodyfallSounds()
{
	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	return (ShouldUpBodyfallSoundsFromShooting() || timeInMs < m_LastMeleeImpactTime + 1000 || timeInMs < m_LastCarImpactTime + 1000 || timeInMs > m_LastTimeOnGround + g_audPedAirTimeForBodyImpacts || m_Ped->GetVelocity().Mag() > g_AudPedVelForLoudImpacts);
}

bool audPedAudioEntity::ShouldUpBodyfallSoundsFromShooting()
{
	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	return timeInMs < m_LastBulletImpactTime + 1000;
}

void audPedAudioEntity::PedFellToDeath()
{
	m_LastFallToDeathTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

}

void audPedAudioEntity::HandleCarJackImpact(const Vector3 &pos)
{
	const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(now - m_LastCarJackTime < g_TimeForCarJackCollision)
	{
		audSoundInitParams params;
		params.EnvironmentGroup = GetEnvironmentGroup(true);
		params.Position = pos;
		params.Category = g_UppedCollisionCategory;
		CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("CarJackImpact", 0xE6247F3)), &params);
	}
	m_HasJustBeenCarJacked = false;
}


void audPedAudioEntity::TriggerImpactSounds(const Vector3 &pos, CollisionMaterialSettings *UNUSED_PARAM(otherMaterial), CEntity *otherEntity, f32 impulseMag, u32 pedComponent, u32 UNUSED_PARAM(otherComponent), bool isMeleeImpact)
{	

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketPedImpactPacket>(
			CPacketPedImpactPacket(pos, impulseMag, pedComponent, isMeleeImpact), GetOwningEntity(), otherEntity);
	}
#endif

	m_Ped->GetFootstepHelper().ResetAirFlags();

	const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(m_HasJustBeenCarJacked && (!otherEntity || (!otherEntity->GetIsTypePed() && !otherEntity->GetIsTypeVehicle())))
	{
		HandleCarJackImpact(pos);
	}


	bool upBodyfallFromShooting = ShouldUpBodyfallSoundsFromShooting();
	const f32 volLin = g_PedImpulseMagToClothCurve.CalculateValue(impulseMag);
	const f32 deathVolLin = g_PedImpulseMagToClothCurve.CalculateValue(g_DeathImpactMagScale*impulseMag);

	const audCategory* categoryToUse = upBodyfallFromShooting ? g_UppedCollisionCategory : NULL;

	u32 lastImpactTime = m_LastImpactTime;
	u32 lastHeadImpactTime = m_LastHeadImpactTime;

	if(isMeleeImpact)
	{
		m_LastMeleeImpactTime = now;
	}

	bool playDeathImpact = false;
	//We always want to play this is applicable, even if there's been another recent collision
	if(!isMeleeImpact && pedComponent >= RAGDOLL_SPINE0 && (!otherEntity || !otherEntity->GetIsTypePed()))
	{
		if((!m_HasPlayedDeathImpact && m_Ped->GetHealth() <= m_Ped->GetPedHealthInfo()->GetDyingHealthThreshold()))
		{
			playDeathImpact = true;
		}
	}

	if(playDeathImpact)
	{	
		audSoundInitParams deathParams;
		deathParams.EnvironmentGroup = GetEnvironmentGroup(true);
		deathParams.Position = pos;
		deathParams.Category = g_UppedCollisionCategory;
		m_HasPlayedDeathImpact = true;
		m_LastImpactTime = now;
		deathParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(deathVolLin);
		if(m_LastBulletImpactTime+g_audPedTimeForShootingDeathImpact > now)
		{
			CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("death_impact", 0xdab1677f)), &deathParams);
		}
		else 
		{
			CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("death_impact_melee", 0x2292854F)), &deathParams);
		}
	}
	else if(m_Ped->GetHealth() > m_Ped->GetPedHealthInfo()->GetDyingHealthThreshold())
	{
		m_HasPlayedDeathImpact = false;
	}
	
	if((now - lastImpactTime < sm_PedImpactTimeFilter) && (now - lastHeadImpactTime < sm_PedImpactTimeFilter))
	{
		return;
	}

	if(m_Ped->GetPedResetFlag(CPED_RESET_FLAG_InContactWithFoliage) || m_Ped->GetPedResetFlag(CPED_RESET_FLAG_InContactWithBIGFoliage))
	{
		m_FootStepAudio.AddBushEvent(AUD_FOOTSTEP_WALK_L, m_Ped->GetContactedFoliageHash());
	}
	
	if(m_Ped->IsLocalPlayer() && now - m_LastScrapeDebrisTime > g_PedScrapeDebrisTime && m_Ped->GetFootstepHelper().IsWalkingOnASlope() &&  now - m_LastScrapeTime < 100)
	{
		if(audEngineUtil::ResolveProbability(CPedFootStepHelper::GetSlopeDebrisProb()))
		{
			Vec3V downSlopeDirection (V_ONE_WZERO);
			m_Ped->GetFootstepHelper().GetSlopeDirection(downSlopeDirection);
			m_Ped->GetPedAudioEntity()->GetFootStepAudio().AddSlopeDebrisEvent(AUD_FOOTSTEP_WALK_L,downSlopeDirection, m_Ped->GetFootstepHelper().GetSlopeAngle());
			m_LastScrapeDebrisTime = now;
		}
	}

	u32 lastTimeOnGround = m_LastTimeOnGround;

	bool isDeadAndBigImpact = (m_Ped->IsDead() || (m_LastFallToDeathTime + 1000 > now)) && now > (lastTimeOnGround + g_audPedAirTimeForBigDeathImpacts) && now > (m_LastTimeInVehicle+g_audPedAirTimeForBigDeathImpacts) && now > (lastImpactTime + g_audPedAirTimeForBigDeathImpacts);

	if(m_WasInVehicleAndNowInAir && (!otherEntity || !otherEntity->GetIsTypeVehicle() || !((CVehicle*)otherEntity)->GetVehicleAudioEntity()->IsLastPlayerVeh()))
	{
		if((!otherEntity || !otherEntity->GetIsTypePed()) && (now > (m_LastTimeInVehicle + sm_AirTimeForLiveBigimpacts)))
		{
			isDeadAndBigImpact = true;
		}
		m_WasInVehicleAndNowInAir = false;  
	}

	if(isMeleeImpact)
	{
		isDeadAndBigImpact = false;
	}
	
	if(m_LastBigdeathImpactTime + 1000 < now)
	{
		if(isDeadAndBigImpact)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = GetEnvironmentGroup(true);
			initParams.Position = pos;
			initParams.Category = g_UppedCollisionCategory;
			m_LastBigdeathImpactTime = now;
			m_LastImpactTime = now;

			if(m_Ped->IsLocalPlayer())
			{
				if(audNorthAudioEngine::ShouldTriggerPulseHeadset())
				{
					CreateAndPlaySound(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("PlayerMapImpact", 0x32774922)));
				}
				CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("DEAD_PLAYER_LARGE_IMPACT", 0x65B06A5C)), &initParams);

				CMiniMap::CreateSonarBlipAndReportStealthNoise(m_Ped, m_Ped->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_ObjectCollision, HUD_COLOUR_BLUEDARK);
			}
			else
			{
				CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("DEAD_PED_LARGE_IMPACT", 0x29FB2A9A)), &initParams);
			}
		}
		else if(m_Ped->GetUsingRagdoll() && (now > (lastTimeOnGround + g_audPedAirTimeForBigDeathImpacts)) && !m_HasPlayedDeathImpact && (now > (lastImpactTime + g_audPedAirTimeForBodyImpacts)))
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = GetEnvironmentGroup(true);
			initParams.Position = pos;
			initParams.Category = g_UppedCollisionCategory;
			m_LastBigdeathImpactTime = now;
			m_LastImpactTime = now;
			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(deathVolLin);
			CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("death_impact_melee", 0x2292854F)), &initParams);	
		}
	}

	if( otherEntity && otherEntity->GetType() == ENTITY_TYPE_VEHICLE)
	{
		CVehicleModelInfo * vehicleInfo = (CVehicleModelInfo *)(otherEntity->GetBaseModelInfo());
		if(vehicleInfo && vehicleInfo->GetVehicleType() != VEHICLE_TYPE_BICYCLE)
		{
			// scale up other collisions after a car collision
			const f32 carImpactScalar = g_PedVehImpulseTimeScalarCurve.CalculateValue((f32)(now - m_LastCarImpactTime));
			impulseMag *= carImpactScalar;
		}
		else if(now < lastImpactTime + 500)
		{
			return;
		}
	}
	else
	{
		// vehicle impacts do their own bone break stuff
		// dont trigger breaks for 2 seconds after a gunshot
		if(now > m_LastBulletImpactTime + 2000 || isDeadAndBigImpact)
		{
			if(audEngineUtil::ResolveProbability(g_PedImpulseToBoneBreakProbCurve.CalculateValue(impulseMag)) || isDeadAndBigImpact )
			{
				// only trigger limb breaks for melee stuff
				if((!isMeleeImpact || g_BoneBreakSounds[pedComponent] == g_BoneBreakSounds[1]))
				{
					TriggerBoneBreak((RagdollComponent)pedComponent);
				}
			}
		}
	}
		
	// all ped collisions can trigger cloth impacts
	if(pedComponent >= RAGDOLL_SPINE0)  
	{
		if(volLin > g_SilenceVolumeLin)
		{
			m_LastImpactTime = now;
			if(pedComponent == RAGDOLL_HEAD)
			{
				m_LastHeadImpactTime = now;
			}

			f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(volLin); 
			if(m_FootStepAudio.GetCachedUpperBodyClothSounds() || pedComponent == RAGDOLL_HEAD)
			{
				u32 soundHash = g_NullSoundHash;
				if(pedComponent != RAGDOLL_HEAD)
				{
					soundHash = m_FootStepAudio.GetCachedUpperBodyClothSounds()->ImpactSound;
				}
				else if(m_Ped->GetHelmetComponent() &&  m_Ped->GetHelmetComponent()->IsHelmetEnabled())
				{
					soundHash = ATSTRINGHASH("HELMET_SOUND", 0x5CA13A7F);
				}
				else if(pedComponent == RAGDOLL_HEAD)
				{
					soundHash = ATSTRINGHASH("HEAD_IMPACT", 0x37F70B98);
				}

				audSoundInitParams initParams;
				initParams.EnvironmentGroup = GetEnvironmentGroup(true);
				initParams.Position = pos;
				initParams.Volume = dbVol;
				if(categoryToUse)
				{
					initParams.Category = categoryToUse;
				}
				CreateAndPlaySound(soundHash, &initParams);
			}
			

			if(m_Ped->IsLocalPlayer())
			{
				CMiniMap::CreateSonarBlipAndReportStealthNoise(m_Ped, m_Ped->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_ObjectCollision, HUD_COLOUR_BLUEDARK);
			}
		}
	}
	else
	{
		if(m_FootStepAudio.GetCachedLowerBodyClothSounds() || pedComponent == RAGDOLL_HEAD)
		{
			const f32 volLin = g_PedImpulseMagToClothCurve.CalculateValue(impulseMag);
			if(volLin > g_SilenceVolumeLin) 
			{
				m_LastImpactTime = now;
				if(pedComponent == RAGDOLL_HEAD)
				{
					m_LastHeadImpactTime = now;
				}

				f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(volLin);

				u32 soundHash = g_NullSoundHash;
				if(pedComponent != RAGDOLL_HEAD)
				{
					soundHash = m_FootStepAudio.GetCachedLowerBodyClothSounds()->ImpactSound;
				}
				else if(m_Ped->GetHelmetComponent() &&  m_Ped->GetHelmetComponent()->IsHelmetEnabled())
				{
					soundHash = ATSTRINGHASH("HELMET_SOUND", 0x5CA13A7F);
				}
				else if(pedComponent == RAGDOLL_HEAD)
				{
					soundHash = ATSTRINGHASH("HEAD_IMPACT", 0x37F70B98);
				}

				audSoundInitParams initParams;
				initParams.EnvironmentGroup = GetEnvironmentGroup(true);
				initParams.Position = pos;
				initParams.Volume = dbVol;
				if(categoryToUse)
				{
					initParams.Category = categoryToUse;
				}
				CreateAndPlaySound(soundHash, &initParams);

				if(m_Ped->IsLocalPlayer())
				{
					CMiniMap::CreateSonarBlipAndReportStealthNoise(m_Ped, m_Ped->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_ObjectCollision, HUD_COLOUR_BLUEDARK);
				}
			}
		}
	}
}

void audPedAudioEntity::HandlePreComputeContact(phContactIterator &impacts)
{
	CEntity* otherEntity = CPhysics::GetEntityFromInst(impacts.GetOtherInstance());

	if(otherEntity && otherEntity->GetIsTypeObject())
	{
		u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

		if(m_LastImpactEntity && otherEntity == m_LastImpactEntity)
		{
			if(now < (m_LastEntityImpactTime + sm_EntityImpactTimeFilter))
			{
				m_LastEntityImpactTime = now;
				return;
			}
		}
		// extract the material data from the collision and see if we have a model specific override
		phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(impacts.GetOtherMaterialId());
		phMaterialMgr::Id unpackedPedMatId = PGTAMATERIALMGR->UnpackMtlId(impacts.GetMyMaterialId());
		
		CollisionMaterialSettings * materialSettings = g_CollisionAudioEntity.GetMaterialOverride(otherEntity, g_audCollisionMaterials[(s32)unpackedMtlIdA], impacts.GetOtherComponent());
		CollisionMaterialSettings * pedMaterialSettings = g_audCollisionMaterials[(s32)unpackedPedMatId];

		f32 impactMag = GetPlayerImpactMagOverride(0.f, otherEntity, materialSettings);
		if(impactMag)
		{
			audCollisionEvent collisionEvent;
			collisionEvent.entities[0] = m_Ped;
			collisionEvent.entities[1] = otherEntity;
			collisionEvent.components[0] = (u16)impacts.GetMyComponent();
			collisionEvent.components[1] = (u16)impacts.GetOtherComponent();
			collisionEvent.materialSettings[0] = pedMaterialSettings;
			collisionEvent.materialSettings[1] = materialSettings;
			collisionEvent.impulseMagnitudes[0] = 0.f;
			collisionEvent.impulseMagnitudes[1] = impactMag * g_CollisionAudioEntity.GetAudioWeightScaling(g_CollisionAudioEntity.GetAudioWeight(otherEntity), g_CollisionAudioEntity.GetAudioWeight(m_Ped));
			collisionEvent.positions[0] = VEC3V_TO_VECTOR3(impacts.GetMyPosition());
			collisionEvent.positions[1] = VEC3V_TO_VECTOR3(impacts.GetOtherPosition());
			collisionEvent.type = AUD_COLLISION_TYPE_IMPACT;
			g_CollisionAudioEntity.HandlePlayerEvent(otherEntity);		
			g_CollisionAudioEntity.ProcessImpactSounds(&collisionEvent);
		}
	}
}

f32 audPedAudioEntity::GetPlayerImpactMagOverride(f32 impactMag, CEntity * otherEntity, CollisionMaterialSettings * otherMaterial)
{
	if(!m_Ped || !m_Ped->IsPlayer() || !otherEntity || otherEntity->GetIsTypePed() || !otherMaterial || (AUD_GET_TRISTATE_VALUE(otherMaterial->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_PEDSARESOLID) != AUD_TRISTATE_TRUE && !g_PedsMakeSolidCollisions))
	{
		return impactMag;
	}

	if(m_Ped->GetMotionData() && m_Ped->GetMotionData()->GetIsStill())
	{
		return 0.f;
	}

	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);


	if(m_LastImpactEntity && otherEntity == m_LastImpactEntity)
	{
		if(now < (m_LastEntityImpactTime + sm_EntityImpactTimeFilter))
		{
			m_LastEntityImpactTime = now;
			return 0.f;
		}
	}
	else if(now < m_LastEntityImpactTime + 100)
	{
		m_LastEntityImpactTime = now;
		return 0.f;
	}

	m_LastImpactEntity = otherEntity;
	m_LastEntityImpactTime = now;

	f32 movementFactor;

	if(m_Ped->GetMotionData()->GetIsSprinting() || m_Ped->GetMotionData()->GetIsRunning())
	{
		movementFactor = sm_PedRunningImpactFactor;
	}
	else
	{
		movementFactor = sm_PedWalkingImpactFactor;
	}

	f32 weightFactor;
	AudioWeight weight = g_CollisionAudioEntity.GetAudioWeight(otherEntity);

	if(weight <= AUDIO_WEIGHT_L)
	{
		weightFactor = sm_PedLightImpactFactor;
	}
	else if(weight ==  AUDIO_WEIGHT_M)
	{
		weightFactor = sm_PedMediumImpactFactor;
	}
	else
	{
		weightFactor = sm_PedHeavyImpactFactor;
	}

	//naErrorf("Override ped impact is %f, Max %f, weight %f, move %f, entity %s(%p) time %u", otherMaterial->MaxImpulseMag*weightFactor*movementFactor, otherMaterial->MaxImpulseMag, weightFactor, movementFactor, otherEntity->GetModelName(), otherEntity, fwTimer::GetTimeInMilliseconds()); 

	return (otherMaterial->MaxImpulseMag*weightFactor*movementFactor*100.f)/otherMaterial->ImpulseMagScalar;
}

void audPedAudioEntity::HandleJumpingImpactRagdoll()
{
	audSoundInitParams initParams;
	CreateDeferredSound(GetPedCollisionSoundset()->Find(ATSTRINGHASH("JumpingImpactRagdoll", 0xC5BB669D)), m_Ped, &initParams, true, true);
	m_LastMeleeImpactTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}


void audPedAudioEntity::PlayVehicleImpact(const Vector3 &pos, CVehicle *veh, const audVehicleCollisionContext * context)
{
	const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if((now - m_LastCarImpactTime) <= g_PedCarImpactTime )
	{
		m_LastCarImpactTime = now;
		return; 
	}
	m_LastCarImpactTime = now;
	
	if(audEngineUtil::ResolveProbability(g_PedVehImpactBoneBreakProbCurve.CalculateValue(context->impactMag)))
	{
		TriggerBoneBreak((RagdollComponent)context->collisionEvent.otherComponent);
	}

	const u32 bikeSound = ATSTRINGHASH("PED_MOTORBIKE_COLLISION", 0xF0EAEF1);

	f32 linVol = g_PedVehImpulseToImpactVolCurve.CalculateValue(context->impactMag);
	if(linVol > g_SilenceVolumeLin)
	{
		if(m_Ped->IsLocalPlayer() && veh->GetVehicleType() != VEHICLE_TYPE_BICYCLE && context->impactMag > sm_VehicleImpactForHeadphonePulse && audNorthAudioEngine::ShouldTriggerPulseHeadset())
		{
			CreateAndPlaySound(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("PlayerVehicleImpact", 0x8F19D977)));
		}

		u32 soundHash = g_VehicleImpactSounds[context->collisionEvent.otherComponent];

		if(veh->GetVehicleType() == VEHICLE_TYPE_BIKE)
		{
			soundHash = bikeSound;
			linVol = g_PedBikeImplulseToImpactVolCurve.CalculateRescaledValue(0.f, 1.f, 0.f, sm_MaxMotorbikeCollisionVelocity,context->impactMag);
		}
		else if(veh->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
		{
			soundHash = ATSTRINGHASH("BICYCLE_COLLISIONS_MULTI", 0xFFB8F44D);
			linVol = g_PedBicyleImplulseToImpactVolCurve.CalculateRescaledValue(0.f, 1.f, 0.f, sm_MaxBicycleCollisionVelocity,context->impactMag);
		}

		const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(linVol);
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.Position = pos;
		initParams.Volume = dbVol;

		CreateAndPlaySound(soundHash, &initParams);


		f32 vehSpeed = veh->GetVelocity().Mag();

		AnimalParams * animalParams = audNorthAudioEngine::GetObject<AnimalParams>(m_Ped->GetPedModelInfo()->GetAnimalAudioObject());
		if(animalParams)
		{
			if(vehSpeed > animalParams->VehicleSpeedForBigImpact &&  
				(m_Ped->GetHealth() > m_Ped->GetPedHealthInfo()->GetDyingHealthThreshold() || (m_Ped->HasBeenDamagedByHash(WEAPONTYPE_RUNOVERBYVEHICLE) && !m_HasPlayedBigAnimalImpact)))
			{
				CreateAndPlaySound(animalParams->BigVehicleImpact, &initParams);
				if(m_Ped->GetHealth() <= m_Ped->GetPedHealthInfo()->GetDyingHealthThreshold())
				{
					m_HasPlayedBigAnimalImpact = true;
				}
			}
		}
	}
}

void audPedAudioEntity::PlayUnderOverVehicleSounds(const Vector3 &pos, CVehicle *veh, const audVehicleCollisionContext * context)
{
	const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(veh->GetVehicleType() == VEHICLE_TYPE_BICYCLE)
	{
		return;
	}

	f32 linVol = g_PedVehOverUnderVolCurve.CalculateValue(veh->GetVelocity().Mag());
	if(linVol > g_SilenceVolumeLin)
	{
		const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(linVol);

		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.Position = pos;
		initParams.Volume = dbVol;

		audMetadataRef extraSound;

		if((context->GetTypeFlag(AUD_VEH_COLLISION_BOTTOM) || context->GetTypeFlag(AUD_VEH_COLLISION_WHEEL_BOTTOM) || m_Ped->GetPedResetFlag(CPED_RESET_FLAG_VehicleCrushingRagdoll)) && !m_Ped->GetPedResetFlag(CPED_RESET_FLAG_RagdollOnVehicle) )
		{
			if((now - m_LastCarOverUnderTime) > g_PedCarUnderTime )
			{
				AnimalParams * animalParams = audNorthAudioEngine::GetObject<AnimalParams>(m_Ped->GetPedModelInfo()->GetAnimalAudioObject());
				if(animalParams)
				{
					if(!m_HasPlayedRunOver && m_Ped->GetHealth() <= m_Ped->GetPedHealthInfo()->GetDyingHealthThreshold())
					{
						if(m_Ped->HasBeenDamagedByHash(WEAPONTYPE_RUNOVERBYVEHICLE))
						{
							CreateAndPlaySound(animalParams->RunOverSound, &initParams);
							m_HasPlayedRunOver = true;
						}
					}
				}

				extraSound = sm_PedVehicleCollisionSoundSet.Find(ATSTRINGHASH("under", 0xE7D3C572));
				
#if __BANK
				if(g_DebugPedVehicleOverUnderSounds)
				{
					grcDebugDraw::Sphere(m_Ped->GetTransform().GetPosition() + m_Ped->GetTransform().GetB() + Vec3V(ScalarV(0.f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(255, 0, 0), true, 5);
				}
#endif

			}
				m_LastCarOverUnderTime = now;
		}
		else if(m_Ped->GetPedResetFlag(CPED_RESET_FLAG_RagdollOnVehicle) || context->GetTypeFlag(AUD_VEH_COLLISION_BODY))
		{
			if((now - m_LastCarOverUnderTime) > g_PedCarUnderTime )
			{	
#if __BANK
				if(g_DebugPedVehicleOverUnderSounds)
				{
					grcDebugDraw::Sphere(m_Ped->GetTransform().GetPosition() + m_Ped->GetTransform().GetB() + Vec3V(ScalarV(0.5f), ScalarV(0.f), ScalarV(1.5f)), 0.5f, Color32(0, 175, 175), true, 5);
				}
#endif

				extraSound = sm_PedVehicleCollisionSoundSet.Find(ATSTRINGHASH("car_body", 0x76E386C2));
			}
				m_LastCarOverUnderTime = now;
		}

		CreateAndPlaySound(extraSound, &initParams);	
	}
}


bool audPedAudioEntity::TriggerBoneBreak(const RagdollComponent comp)
{
	bool ret = false;
	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(now < m_LastMeleeImpactTime + 5000)
	{
		// inhibit bone breaks for 10 seconds after a melee impact
		return false;
	}
	// no bone break sounds for the player if he's alive
	if(!(m_BoneBreakState & (1<<comp)) && (m_Ped->IsDead() || (m_LastFallToDeathTime + 1000 > now) || !m_Ped->IsLocalPlayer()))
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.TrackEntityPosition = true;
		CreateAndPlaySound(g_BoneBreakSounds[comp], &initParams);
		ret = true;

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(g_BoneBreakSounds[comp], &initParams, GetOwningEntity()));
	}
	// only 'break' bones if the ped is dead
	if(m_Ped->IsDead())
	{
		m_BoneBreakState |= (1<<comp);
	}
	return ret;
}

void audPedAudioEntity::TriggerBigSplash()
{
	if(m_LastBigSplashTime + 1000 < fwTimer::GetTimeInMilliseconds())
	{
		//m_FeetWetness = 1.f;
		m_LastBigSplashTime = fwTimer::GetTimeInMilliseconds();
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = GetEnvironmentGroup(true);
		initParams.Position = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
		CreateAndPlaySound(ATSTRINGHASH("PED_WATER_LAND", 0xB44EC0A2), &initParams);

		if (m_Ped->IsLocalPlayer())
		{
			CMiniMap::CreateSonarBlipAndReportStealthNoise(m_Ped, m_Ped->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_WaterSplashLarge, HUD_COLOUR_BLUEDARK);
		}
	}
}

void audPedAudioEntity::UpdateStaggering()
{
	if(m_Ped && m_Ped != FindPlayerPed())
	{
		if(!m_HasPlayedInitialStaggerSounds)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = GetEnvironmentGroup(true);
			initParams.Position = VEC3V_TO_VECTOR3(m_Ped->GetTransform().GetPosition());
			CreateAndPlaySound(ATSTRINGHASH("PED_ON_PED_BUMP", 0x529FBA01), &initParams);

			if(m_Ped->GetSpeechAudioEntity() && !m_Ped->GetSpeechAudioEntity()->GetIsAnySpeechPlaying())
			{
				audDamageStats damageStats;
				damageStats.Fatal = false;
				damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
				damageStats.DamageReason = AUD_DAMAGE_REASON_SHOVE;
				m_Ped->GetSpeechAudioEntity()->InflictPain(damageStats);
			}

			m_HasPlayedInitialStaggerSounds = true;
		}
		else if(m_Ped->GetPedIntelligence())
		{
			CTask *pTaskSimplest = m_Ped->GetPedIntelligence()->GetTaskActiveSimplest();
			if(!pTaskSimplest)
				m_IsStaggering = false;	
			else if(pTaskSimplest->GetTaskType()==CTaskTypes::TASK_GET_UP)
			{
				if(FindPlayerPed() && FindPlayerPed()->GetSpeechAudioEntity())
					FindPlayerPed()->GetSpeechAudioEntity()->Say("KNOCK_OVER_PED", "SPEECH_PARAMS_STANDARD_SHORT_LOAD");
				m_IsStaggering = false;	
			}
			else if(!m_HasPlayedSecondStaggerSound && m_IsStaggerFalling)
			{
				
				Vector3 vecPelvisPosition(VEC3_ZERO);
				m_Ped->GetBonePosition(vecPelvisPosition, BONETAG_PELVISROOT);

				const float fSecondSurfaceInterp=0.0f;
				bool hitGround = false;
				float ground = WorldProbe::FindGroundZFor3DCoord(fSecondSurfaceInterp, vecPelvisPosition.x, vecPelvisPosition.y, vecPelvisPosition.z, &hitGround);

				if(!hitGround)
					return;

				f32 height = vecPelvisPosition.z - ground;

				if(height > g_PelvisHeightForSecondStaggerGrunt)
					return;

				if(m_Ped->GetSpeechAudioEntity() && !m_Ped->GetSpeechAudioEntity()->GetIsAnySpeechPlaying())
				{
					audDamageStats damageStats;
					damageStats.Fatal = false;
					damageStats.RawDamage = g_HealthLostForHighPain + 1.0f;
					damageStats.DamageReason = AUD_DAMAGE_REASON_SHOVE;
					m_Ped->GetSpeechAudioEntity()->InflictPain(damageStats);
				}

				m_HasPlayedSecondStaggerSound = true;
			}
		}
		else
			m_IsStaggering = false;
	}
	else
		m_IsStaggering = false;
}

void audPedAudioEntity::SetIsStaggering()
{
	if(!m_IsStaggering)
	{
		m_IsStaggering = true;
		m_IsStaggerFalling = false;
		m_HasPlayedInitialStaggerSounds = false;
		m_HasPlayedSecondStaggerSound = false;
	}
}

void audPedAudioEntity::SetIsStaggerFalling()
{
	m_IsStaggerFalling = true;
}

void audPedAudioEntity::OnIncendiaryAmmoFireStarted()
{	
	audSoundSet soundSet;
	const u32 soundSetName = ATSTRINGHASH("SPECIAL_AMMO_TYPE_BULLET_IMPACTS", 0x5F8529E3);
	const u32 fieldName = ATSTRINGHASH("INCENDIARY_AMMO_PED_FIRE_START", 0xA9D91869);

	if(soundSet.Init(soundSetName))
	{		
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		CreateDeferredSound(soundSet.Find(fieldName), m_Ped, &initParams, true, true);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundSetName, fieldName, &initParams, m_Ped));
	}	
}

void audPedAudioEntity::RegisterBirdTakingOff()
{
	static Vector3 north = Vector3(0.0f, 1.0f, 0.0f); //N
	static Vector3 east = Vector3(1.0f, 0.0f, 0.0f); //E

	if(!m_Ped || !FindPlayerPed())
		return;

	sm_BirdTakeOffWriteIndex++;
	if(sm_BirdTakeOffWriteIndex >= sm_BirdTakeOffArray.GetMaxCount())
	{
		sm_BirdTakeOffWriteIndex %= sm_BirdTakeOffArray.GetMaxCount();
	}

	sm_TimeOfLastBirdTakeOff = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Bird = m_Ped;
	sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Time = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].IsPigeon = m_Ped && m_Ped->GetSpeechAudioEntity() && 
						m_Ped->GetSpeechAudioEntity()->GetAnimalType() == kAnimalPigeon;

	Vector3 playerToBirdNorm = VEC3V_TO_VECTOR3(m_Ped->GetMatrix().d() - FindPlayerPed()->GetMatrix().d());
	playerToBirdNorm.Normalize();
	f32 northDot = north.Dot(playerToBirdNorm);
	f32 eastDot = east.Dot(playerToBirdNorm);

	if(northDot >= 0.9239f) //approx sin(67.5 deg)
		sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_N;
	else if(northDot >= 0.3827f) //approx sin(22.5 deg)
	{
		if(eastDot >= 0.0f)
			sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_NE;
		else
			sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_NW;
	}
	else if(northDot >= -0.3827f)
	{
		if(eastDot >= 0.0f)
			sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_E;
		else
			sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_W;
	}
	else if(northDot >= -0.9239f)
	{
		if(eastDot >= 0.0f)
			sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_SE;
		else
			sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_SW;
	}
	else
		sm_BirdTakeOffArray[sm_BirdTakeOffWriteIndex].Direction = AUD_BDIR_S;
}

void audPedAudioEntity::UpdateBirdTakingOffSounds(u32 timeInMs)
{
	CPed** earlyBirdData = Alloca(CPed*, AUD_NUM_BDIR);
	atUserArray<CPed*> earlyBirds(earlyBirdData, AUD_NUM_BDIR, true);
	u32* numBirdsData = Alloca(u32, AUD_NUM_BDIR);
	atUserArray<u32> numBirds(numBirdsData, AUD_NUM_BDIR, true);
	u32* numPigeonsData = Alloca(u32, AUD_NUM_BDIR);
	atUserArray<u32> numPigeons(numPigeonsData, AUD_NUM_BDIR, true);
	
	for(int i=0; i<AUD_NUM_BDIR; ++i)
	{
		earlyBirds[i] = NULL;
		numBirds[i] = 0;
		numPigeons[i] = 0;
	}

	u32 earliestTimeToCheck = timeInMs - g_MaxTimeToTrackBirdTakeOff;

	for(int i=0; i<sm_BirdTakeOffArray.GetMaxCount(); i++)
	{
		int index = int(sm_BirdTakeOffWriteIndex) - i;
		if(index < 0)
			index += sm_BirdTakeOffArray.GetMaxCount();

		if(sm_BirdTakeOffArray[index].Time == ~0U)
			break;
		if(sm_BirdTakeOffArray[index].Time < earliestTimeToCheck)
			break;

		if(sm_BirdTakeOffArray[index].Bird && sm_BirdTakeOffArray[index].Bird->GetPedAudioEntity())
			earlyBirds[sm_BirdTakeOffArray[index].Direction] = sm_BirdTakeOffArray[index].Bird;
		if(sm_BirdTakeOffArray[index].IsPigeon)
			numPigeons[sm_BirdTakeOffArray[index].Direction]++;
		else
			numBirds[sm_BirdTakeOffArray[index].Direction]++;
	}

	for(int dir=0; dir<AUD_NUM_BDIR; dir++)
	{
		if(numBirds[dir] + numPigeons[dir] > g_MinNumBirdTakeOffsToPlaySound)
		{
			if(earlyBirds[dir] && earlyBirds[dir]->GetPedAudioEntity())
				earlyBirds[dir]->GetPedAudioEntity()->PlayBirdTakingOffSound(numPigeons[dir] >= numBirds[dir]);

			for(int j=0; j<sm_BirdTakeOffArray.GetMaxCount(); j++)
			{
				if(sm_BirdTakeOffArray[j].Direction == dir)
					sm_BirdTakeOffArray[j].Time = ~0U;
			}
		}
	}
}

void audPedAudioEntity::PlayBirdTakingOffSound(bool playPigeon)
{
	audSoundInitParams initParams;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = GetEnvironmentGroup(true);
	if(playPigeon)
		CreateAndPlaySound(ATSTRINGHASH("FLOCK_OF_PIGEONS_FLY_AWAY_MASTER", 0xC163A9B1), &initParams);
	else
		CreateAndPlaySound(ATSTRINGHASH("FLOCK_OF_BIRDS_FLY_AWAY_MASTER", 0xB1342CFC), &initParams);
}

void audPedAudioEntity::SetLastScrapeTime()
{
	m_LastScrapeTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

void audPedAudioEntity::TriggerScrapeStartImpact(f32 volume)
{
	audSoundInitParams initParams;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = GetEnvironmentGroup(true);
	initParams.Volume = volume;
	CreateAndPlaySound(sm_PedCollisionSoundSet.Find(ATSTRINGHASH("ScrapeStart", 0x1B98F86C)), &initParams);

	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	if(m_Ped->IsLocalPlayer() && now - m_LastScrapeDebrisTime > g_PedScrapeDebrisTime && m_Ped->GetFootstepHelper().IsWalkingOnASlope())
	{
		if(audEngineUtil::ResolveProbability(CPedFootStepHelper::GetSlopeDebrisProb()))
		{
			Vec3V downSlopeDirection (V_ONE_WZERO);
			m_Ped->GetFootstepHelper().GetSlopeDirection(downSlopeDirection);
			m_Ped->GetPedAudioEntity()->GetFootStepAudio().AddSlopeDebrisEvent(AUD_FOOTSTEP_WALK_L,downSlopeDirection,m_Ped->GetFootstepHelper().GetSlopeAngle());
			m_LastScrapeDebrisTime = now;
		}
	}

	m_NeedsToDoScrapeEnd = true;
}


#if __BANK

char g_AnimEventDebug[128] = "PLAYER_WHISTLE";

void PlayAnimTriggeredEvent(void)
{
	fwEntity *pSelectedEntity = g_PickerManager.GetSelectedEntity();
	if(pSelectedEntity == NULL)
	{
		pSelectedEntity = CGameWorld::FindLocalPlayer();
	}
	if(pSelectedEntity != NULL && pSelectedEntity->GetType() == ENTITY_TYPE_PED)
	{
		((CPed*)pSelectedEntity)->GetPedAudioEntity()->HandleAnimEventFlag(g_AnimEventDebug);
	}
}


void audPedAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("audPedAudioEntity",false);	
	bank.AddSlider("Min Melee Repeat Time", &g_MinMeleeRepeatTime, 0, 1000, 10);
	bank.AddSlider("sm_LifeTimePedEnvGroup", &sm_LifeTimePedEnvGroup, 0, 10000000, 1);
	bank.AddToggle("Reset env group life time when requested", &g_ResetEnvGroupLifeTime);
	bank.AddToggle("g_ShowPedsEnvGroup", &g_ShowPedsEnvGroup);
	bank.AddSlider("Lod anim timeout (looping)", &sm_LodAnimLoopTimeout, 0, 10000, 100);
	bank.AddSlider("g_PedCarImpactTime", &g_PedCarImpactTime, 0, 1000, 10);
	bank.AddSlider("g_PedCarunderTime", &g_PedCarUnderTime, 0, 5000, 10);
	bank.AddSlider("sm_PedImpactTimeFilter", &sm_PedImpactTimeFilter, 0, 1000, 10);
	bank.AddSlider("sm_MaxBicycleCollisionVelocity", &sm_MaxBicycleCollisionVelocity, 0.f, 30.f, 0.1f);
	bank.AddSlider("sm_MaxMotorbikeCollisionVelocity", &sm_MaxMotorbikeCollisionVelocity, 0.f, 30.f, 0.1f);
	bank.AddSlider("sm_MinHeightForWindNoise", &sm_MinHeightForWindNoise, 0.f, 1000.f, 1.f);
	bank.AddSlider("sm_HightVelForWindNoise", &sm_MinHeightForWindNoise, 0.f, 1000.f, 1.f);
	bank.AddToggle("Debug Draw Scenario Manager", &g_DebugDrawScenarioManager);

#if __DEV
	bank.AddToggle("Debug Ped Update", &g_BreakpointDebugPedUpdate);
#endif
	bank.PushGroup("Ped Occlusion",false);
		bank.AddSlider("InnerUpdateFrames", &sm_InnerUpdateFrames, 0.0f, 10.0f, 1.0f);
		bank.AddSlider("OuterUpdateFrames", &sm_OuterUpdateFrames, 0.0f, 10.0f, 1.0f);
		bank.AddSlider("InnerCheapUpdateFrames", &sm_InnerCheapUpdateFrames, 0.0f, 10.0f, 1.0f);
		bank.AddSlider("OuterCheapUpdateFrames", &sm_OuterCheapUpdateFrames, 0.0f, 10.0f, 1.0f);
		bank.AddSlider("CheapDistance", &sm_CheapDistance, 0.0f, 40.0f, 1.0f);
	bank.PopGroup();

	bank.PushGroup("Bullet Casings", false);
		bank.AddSlider("X Offset", &g_BulletCasingXOffset, -10.f, 10.f, 0.1f);
		bank.AddSlider("Y Offset", &g_BulletCasingYOffset, -10.f, 10.f, 0.1f);
		bank.AddSlider("Z Offset", &g_BulletCasingZOffset, -10.f, 10.f, 0.1f);		
	bank.PopGroup();

	bank.PushGroup("Ped Rappel",false);
		bank.AddToggle("Force Rappel", &g_ForceRappel);
		bank.AddToggle("Force Rappel Descend", &g_ForceRappelDescend);
	bank.PopGroup();
	
	bank.PushGroup("Melee Combat",false);
		bank.AddToggle("Show debug info.", &sm_ShowMeleeDebugInfo);
		bank.AddSlider("sm_SmallAnimalThreshold", &sm_SmallAnimalThreshold, 0.0f, 10.0f, 0.1f);
	bank.PopGroup();
		bank.AddSlider("sm_SqdDistanceThresholdToTriggerShellCasing", &sm_SqdDistanceThresholdToTriggerShellCasing, 0.0f, 10000.0f, 0.1f);
	bank.AddSlider("sm_TimeThresholdToTriggerShellCasing", &sm_TimeThresholdToTriggerShellCasing, 0, 1000, 1);
	
	bank.PushGroup("Anim triggered",false);
		bank.AddButton("Play Anim Triggered event", datCallback(CFA(PlayAnimTriggeredEvent)));
		bank.AddText("Anim Event", &g_AnimEventDebug[0], sizeof(g_AnimEventDebug));
		bank.AddToggle("Print anim events", &g_PrintPedAnimEvents);
		bank.AddToggle("Print focus ped anim events", &g_PrintFocusPedAnimEvents);
	bank.PopGroup();

	bank.AddSlider("Ped air time for upping impact sounds (ms)", &g_audPedAirTimeForBodyImpacts, 0, 10000, 1); 
	bank.AddSlider("Ped velocity for upping impact sounds", &g_AudPedVelForLoudImpacts, 0.f, 100.f, 0.5f); 
	bank.AddSlider("Ped air time for big death impacts", &g_audPedAirTimeForBigDeathImpacts, 0, 10000, 1); 
	bank.AddSlider("Impact mag for post shot bone break", &sm_ImpactMagForShotFallBoneBreak, 0.f, 10.f, 0.1f);
	bank.AddSlider("Air time for live big fall impacts", &sm_AirTimeForLiveBigimpacts, 0, 10000, 100);
	bank.AddSlider("Time for shooting death impacts", &g_audPedTimeForShootingDeathImpact, 0, 10000, 10);
	bank.AddSlider("Time for car jack impacts", &g_TimeForCarJackCollision, 0, 60000, 100);
	bank.AddSlider("Death impact mag scale", &g_DeathImpactMagScale, 0.f, 10.f, 0.1f);
	bank.AddSlider("VehicleContactTimeForForcedRagdollImpact", &g_LastVehicleContactTimeForForcedRagdollImpact, 0, 10000, 100);
	bank.AddToggle("g_DebugPedVehicleOverUnderSounds", &g_DebugPedVehicleOverUnderSounds);

	bank.AddSlider("g_PelvisHeightForSecondStaggerGrunt", &g_PelvisHeightForSecondStaggerGrunt, 0.01f, 1.f, 0.01f);

	bank.AddToggle("ForceMonoRingtones", &g_ForceMonoRingtones);
	bank.AddSlider("OverrideRingtone", &g_OverriddenRingtone, 0, 35, 1);
	bank.AddToggle("Toggle ringtone from player.", &g_ToggleRingtone);
	bank.AddToggle("g_DisableHeartbeat", &g_DisableHeartbeat);
	bank.AddToggle("g_DisableHeartbeatFromInjury", &g_DisableHeartbeatFromInjury);
	bank.AddToggle("g_PrintHeartbeatApply", &g_PrintHeartbeatApply);
	bank.AddSlider("g_TimeNotSprintingBeforeHeartbeatStops", &g_TimeNotSprintingBeforeHeartbeatStops, 0, 15000, 10);
	bank.AddSlider("g_TimeNotHurtBeforeHeartbeatStops", &g_TimeNotHurtBeforeHeartbeatStops, 0, 60000, 10);

	bank.AddSlider("sm_PedWalkingImpactFactor", &sm_PedWalkingImpactFactor, 0.f, 1.f, 0.1f);
	bank.AddSlider("sm_PedRunningImpactFactor", &sm_PedRunningImpactFactor, 0.f, 1.f, 0.1f);
	bank.AddSlider("sm_PedLightImpactFactor", &sm_PedLightImpactFactor, 0.f, 1.f, 0.1f);
	bank.AddSlider("sm_PedMediumImpactFactor", &sm_PedMediumImpactFactor, 0.f, 1.f, 0.1f);
	bank.AddSlider("sm_PedHeavyImpactFactor", &sm_PedHeavyImpactFactor, 0.f, 1.f, 0.1f);
	bank.AddSlider("sm_EntityImpactTimeFilter", &sm_EntityImpactTimeFilter, 0, 100000, 100);
	bank.AddSlider("sm_VehicleImpactForHeadphonePulse", &sm_VehicleImpactForHeadphonePulse, 0.f, 100.f, 1.f);
	bank.AddSlider("g_MinTimeForSameMeleeMaterial", &g_MinTimeForSameMeleeMaterial, 0, 10000, 1);
	audPedFootStepAudio::AddWidgets(bank);
	bank.PopGroup();
}
#endif
//PURPOSE
//Supports audDynamicEntitySound queries
u32 audPedAudioEntity::QuerySoundNameFromObjectAndField(const u32 *objectRefs, u32 numRefs)
{
	naAssertf(objectRefs, "A null object ref array ptr has been passed into audBoatAudioEntity::QuerySoundNameFromObjectAndField");
	naAssertf(numRefs >= 2,"The object ref list should always have at least two entries when entering QuerySoundNameFromObjectAndField");

	if(numRefs ==2)
	{
		//We're at the end of the object ref chain, see if we have a valid object to query
		if(*objectRefs == ATSTRINGHASH("UPPER_CLOTHING", 0x6DA26E2F))
		{
			++objectRefs;
			ClothAudioSettings *upperCloth = const_cast<ClothAudioSettings *>(m_FootStepAudio.GetCachedUpperBodyClothSounds());
			if(upperCloth)
			{
				u32 *ret = (u32 *)upperCloth->GetFieldPtr(*objectRefs);
				return (ret) ? *ret : 0;
			}
		}
		else if(*objectRefs == ATSTRINGHASH("LOWER_CLOTHING", 0x8C90DADA))
		{
			++objectRefs;
			ClothAudioSettings *lowerCloth = const_cast<ClothAudioSettings *>(m_FootStepAudio.GetCachedLowerBodyClothSounds());
			if(lowerCloth)
			{
				u32 *ret = (u32 *)lowerCloth->GetFieldPtr(*objectRefs);
				return (ret) ? *ret : 0;
			}
		}
		else if(*objectRefs == ATSTRINGHASH("WEAPON", 0xE68A4A98))
		{
			WeaponSettings *weaponSettings = NULL;
			const CPedWeaponManager *pedWeaponMgr = m_Ped->GetWeaponManager();
			if(pedWeaponMgr && pedWeaponMgr->GetEquippedWeaponObject() && pedWeaponMgr->GetEquippedWeaponObject()->GetWeapon())
			{
				//temporary const_cast until Rave is changed to output GetFieldPtr as a const function
				weaponSettings = const_cast<WeaponSettings *>(pedWeaponMgr->GetEquippedWeaponObject()->GetWeapon()->GetAudioComponent().GetWeaponSettings(m_Ped));

				if(weaponSettings)
				{
					++objectRefs;
					u32 *ret = ((u32 *)weaponSettings->GetFieldPtr(*objectRefs));
					return (ret) ? *ret : 0;
				}
			}
		}
		else if(*objectRefs == ATSTRINGHASH("MAP_MATERIAL", 0x72277E17))
		{
			if(GetFootStepAudio().GetCurrentMaterialSettings())
			{
				++objectRefs;
				u32 *ret ((u32 *)GetFootStepAudio().GetCurrentMaterialSettings()->GetFieldPtr(*objectRefs));
				return (ret) ? *ret : 0;
			}
		}
		else
		{
			naWarningf("Attempting to query an object field in pedaudioentity but object is not a supported type");
		}
	}
	else
	{
		//Pass the reference chain onto the next audioentity if it's a supported type
		--numRefs;
		if(*objectRefs == ATSTRINGHASH("VEHICLE", 0xDD245B9C))
		{
			++objectRefs;
			CVehicle *veh = m_Ped->GetVehiclePedInside();
			if(veh)
			{
				return veh->GetVehicleAudioEntity()->QuerySoundNameFromObjectAndField(objectRefs, numRefs);
			}
		}
		else
		{
			naWarningf("Unsupported object type (%d) in reference chain passed into audPedAudioEntity::QuerySoundNameFromObjectAndField", *objectRefs);	
		}
	}
	return 0; 
}

Vec3V_Out audPedAudioEntity::GetPosition() const
{
	Assert(m_Ped);
	return m_Ped->GetTransform().GetPosition();
}

audCompressedQuat audPedAudioEntity::GetOrientation() const
{
	Assert(m_Ped);
	return m_Ped->GetPlaceableTracker()->GetOrientation();
}

void audPedAudioEntity::AddAnimEvent(const audAnimEvent & event)
{
	fwAudioEntity::AddAnimEvent(event);

	if (m_Ped && m_Ped->IsLocalPlayer() && m_Ped->GetIsSwimming())
	{
		const u32 uMinTimeBetweenWaterBlips = 250;
		const u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
		if (uCurrentTime > m_LastWaterSplashTime + uMinTimeBetweenWaterBlips)
		{
			m_LastWaterSplashTime = uCurrentTime;
			CMiniMap::CreateSonarBlipAndReportStealthNoise(m_Ped, m_Ped->GetTransform().GetPosition(), CMiniMap::sm_Tunables.Sonar.fSoundRange_WaterSplashSmall, HUD_COLOUR_BLUEDARK);
		}
	}
}

const CPedModelInfo* audPedAudioEntity::GetPedModelInfo()
{ 
	return m_Ped->GetPedModelInfo(); 
}

ePedType audPedAudioEntity::GetPedType() 
{ 
	return m_Ped->GetPedType();
}
bool audPedAudioEntity::GetIsInWater()
{
	return m_Ped && m_Ped->GetIsInWater();
}

void audPedAudioEntity::HandleCarWasJacked()
{
	m_HasJustBeenCarJacked = true;
	m_LastCarJackTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

#if __BANK
void audPedAudioEntity::DrawBoundingBox(const Color32 color)
{
	const Vector3 & vMin = m_Ped->GetBoundingBoxMin();
	const Vector3 & vMax = m_Ped->GetBoundingBoxMax();
	CDebugScene::Draw3DBoundingBox(vMin, vMax, MAT34V_TO_MATRIX34(m_Ped->GetMatrix()), color);
}
#endif

/******************************************/
// audWeaponInventoryListener
/******************************************/

void audWeaponInventoryListener::ItemRemoved(u32 weaponHash)
{
	if(m_Ped->IsLocalPlayer())
	{
		g_WeaponAudioEntity.RemovePlayerInventoryWeapon(weaponHash);
	}
}

void audWeaponInventoryListener::AllItemsRemoved()
{
	if(m_Ped->IsLocalPlayer())
	{
		g_WeaponAudioEntity.RemoveAllPlayerInventoryWeapons();
	}
}
