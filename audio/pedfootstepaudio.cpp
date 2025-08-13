//  
// audio/pedaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "caraudioentity.h"
#include "collisionaudioentity.h"
#include "control/replay/Replay.h"
#include "control/replay/audio/footsteppacket.h"
#include "event/EventSound.h"
#include "objects/ProceduralInfo.h"
#include "gameobjects.h"
#include "game/weather.h"
#include "northaudioengine.h"
#include "glassaudioentity.h"
#include "Stats/StatsInterface.h"
#include "pedaudioentity.h"
#include "pedfootstepaudio.h"
#include "Peds/ped.h"
#include "Peds/PedFootstepHelper.h"
#include "peds/pedintelligence.h"
#include "Peds/rendering/pedVariationPack.h"
#include "peds/rendering/pedvariationds.h"
#include "scriptaudioentity.h"
#include "task/Movement/Climbing/TaskVault.h"
#include "task/Movement/Jumping/TaskJump.h"
#include "system/pad.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "superconductor.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "weapons/Weapon.h"
#include "Weapons/Info/WeaponInfoManager.h"
#include "weatheraudioentity.h"
#include "task/Motion//Locomotion/TaskInWater.h"

AUDIO_PEDS_OPTIMISATIONS()

//OPTIMISATIONS_OFF()

MovementShoeSettings *audPedFootStepAudio::sm_LongGrassFootsepSounds = NULL;
CollisionMaterialSettings *g_BoatFootstepMaterial = NULL;

u32 audPedFootStepAudio::sm_OverriddenPlayerMaterialName = 0;
audSoundSet audPedFootStepAudio::sm_PocketsSoundSet;
audSoundSet audPedFootStepAudio::sm_MolotovSounds;
audSoundSet audPedFootStepAudio::sm_PetrolCanSounds;

audSoundSet audPedFootStepAudio::sm_PedInsideWaterSoundSet;
audSoundSet audPedFootStepAudio::sm_FootstepsUnderRainSoundSet;
// Audio curves
audCurve audPedFootStepAudio::sm_LinearVelToCreakVolumeCurve;
audCurve audPedFootStepAudio::sm_LinearVelToVolumeCurve;
audCurve audPedFootStepAudio::sm_AngularAccToVolumeCurve;
audCurve audPedFootStepAudio::sm_SlopeAngleToFallSpeed;
audCurve audPedFootStepAudio::sm_SlopeAngleToVolume;
audCurve audPedFootStepAudio::sm_ShoeWetnessToCreakiness;
audCurve audPedFootStepAudio::sm_ImpactVelocityToIntensity;
audCurve audPedFootStepAudio::sm_EqualPowerFallCrossFade;
audCurve audPedFootStepAudio::sm_EqualPowerRiseCrossFade;
audCurve audPedFootStepAudio::sm_SurfaceDepthToVolDump;
audCurve audPedFootStepAudio::sm_SurfaceDepthToInterp;
audSmoother audPedFootStepAudio::sm_SlopeDebrisSmoother;

f32 audPedFootStepAudio::sm_FootstepVolDecayOverTime = -0.35f;
f32 audPedFootStepAudio::sm_MaxFootstepVolDecay = -2.f;
f32 audPedFootStepAudio::sm_FootstepImpMagDecayOverTime = 0.035f;
f32 audPedFootStepAudio::sm_MinFootstepImpMagDecay = 0.75f;
f32 audPedFootStepAudio::sm_MoveBlendStopThreshold = 0.5f;
//f32 audPedFootStepAudio::sm_AngularAccThreshold = 0.15f;
f32 audPedFootStepAudio::sm_PlayerFoleyIntensity = 0.0f;
f32	audPedFootStepAudio::sm_MinImpulseMag = 2.5f;
f32	audPedFootStepAudio::sm_MaxImpulseMag = 3.5f;
f32	audPedFootStepAudio::sm_WalkFootstepVolOffset = 0.f;
f32	audPedFootStepAudio::sm_RunFootstepVolOffset = 0.f;
f32	audPedFootStepAudio::sm_NPCFootstepVolOffset = 0.f;
f32	audPedFootStepAudio::sm_NPCRunFootstepRollOffScale = 1.f;
f32 audPedFootStepAudio::sm_CleanUpRatio = 0.1f;
f32 audPedFootStepAudio::sm_MinCleanUpRatio = 0.085f;
f32 audPedFootStepAudio::sm_DryOutRatio = 0.2f;
f32 audPedFootStepAudio::sm_MinDryOutRatio = 0.085f;
f32 audPedFootStepAudio::sm_DistanceThresholdToUpdateGlassiness = 25.f;
f32 audPedFootStepAudio::sm_FeetInsideWaterVolDump = 10.f;
f32 audPedFootStepAudio::sm_FeetInsideWaterImpactDump = 10.f;

f32 audPedFootStepAudio::sm_WaterSplashThreshold = 0.3f;
f32 audPedFootStepAudio::sm_WaterSplashSmallThreshold = 5.f;
f32 audPedFootStepAudio::sm_WaterSplashMediumThreshold = 10.f;
f32 audPedFootStepAudio::sm_StealthPedRollVolOffset = -9.f;
f32 audPedFootStepAudio::sm_BareFeetStealthVol = -2.f;
f32 audPedFootStepAudio::sm_JumpLandCustomImpactStealthVol = -4.f;

u32 audPedFootStepAudio::sm_JumpLandCustomImpactStealthLPFCutOff = 1900;
u32 audPedFootStepAudio::sm_JumpLandCustomImpactStealthAttackTime = 50;
u32 audPedFootStepAudio::sm_BareFeetStealthLPFCutOff = 1900;
u32 audPedFootStepAudio::sm_BareFeetStealthAttackTime = 0;
u32 audPedFootStepAudio::sm_StealthPedRollLPFCutoff = 2500;
u32 audPedFootStepAudio::sm_StealthPedRollAttackTime = 500;
u32 audPedFootStepAudio::sm_FadeInCrouchScene = 20000;
u32 audPedFootStepAudio::sm_FadeOutCrouchScene = 20000;

static f32 sm_BushCreakiness = 0.f;
bool audPedFootStepAudio::sm_MaterialCustomTrack = false;
bool audPedFootStepAudio::sm_ShouldShowPlayerMaterial = false;

extern CPlayerSwitchInterface g_PlayerSwitch;

//footsteps performance
#if __BANK 
rage::bkGroup*	audPedFootStepAudio::sm_WidgetGroup(NULL); 
static int sm_CollisionMaterial = 0;
static int sm_OverridenUpperCloths = 0;
static int sm_OverridenLowerCloths = 0;
static int sm_OverridenExtraCloths = 0;
static int sm_OverridenPlayerShoeType = 0;
static char g_OverrideMaterialName[64] = "\0";
static char g_UpperClothsOverrideName[64] = "\0";
static char g_LowerClothsOverrideName[64] = "\0";
static char g_ExtraClothsOverrideName[64] = "\0";
static char g_PlayerShoeTypeOverrideName[64] = "\0";

static bool s_TriggerBush = false;

bool audPedFootStepAudio::sm_bMouseFootstep = false;
bool audPedFootStepAudio::sm_bMouseFootstepBullet = false;
u32 audPedFootStepAudio::sm_LastTimeDebugStepWasPlayed = 0;
Vector3		audPedFootStepAudio::sm_Position;
phMaterialMgr::Id	audPedFootStepAudio::sm_MaterialId = 0;
u16			audPedFootStepAudio::sm_HitComponent = 0;
CEntity*		audPedFootStepAudio::sm_HitEntity = NULL;
CollisionMaterialSettings* audPedFootStepAudio::sm_Material = NULL;
s32					audPedFootStepAudio::sm_TimeToPlayDebugStep = -1;


CollisionMaterialSettings *g_OverridedMaterialSettings;
char g_PlayerMaterialName[64] = "";

bool g_OverridePlayerShoeType = false;
bool g_OverrideMaterialType = false;
bool g_OverrideUpperCloths = false;
bool g_OverrideLowerCloths = false;
bool g_OverrideExtraCloths = false;
bool g_DisableFootstepSounds = false;
bool g_DisplayGroundMaterial = false;
#endif

#if __DEV
bool g_BreakpointDebugPedFootstep = false;
#endif

const u32 g_audMaxPedMoveStates = 4;
f32 g_PedOnPedMinVolume = -3.0f;
f32 g_PedOnPedMaxVolume = -0.0f;

bank_float g_WeaponRattleReductionInFirstPerson = -12.0f;
bank_float g_BigWeaponRattleReductionInFirstPerson = 0.0f;

//AI footstep events
bank_float g_CrouchedLoudnessFactor = 0.5f;
bank_float g_MaterialLoudness[FOOTSTEPLOUDNESS_MAX] = {0.0f, 0.5f, 1.0f, 2.0f};
bank_float g_MoveStateLoudnessScale[g_audMaxPedMoveStates] = {0.0f, 0.45f, 1.0f, 2.0f};

// Vol offsets and Freq TODO: remove.
bank_float g_audPedWalkFeetVolOffset = -3.f;
bank_float g_audPedWalkFeetRelFreq = 1.f;
bank_float g_audPedRunFeetVolOffset = -1.5f;
bank_float g_audPedRunFeetRelFreq = 1.0f;
bank_float g_audPedSprintFeetVolOffset = 0.f;
bank_float g_audPedSprintFeetRelFreq = 1.f;
bank_float g_audPedCrouchFeetVolOffset = -8.f;
bank_float g_audPedCrouchFeetRelFreq = 1.f;
bank_float g_audPlayerPedFeetVolOffset = 7.0f;
bank_float g_audPedFeetVolOffset = 1.5f;
bank_float g_audStairsFeetVolOffset = 1.5f;

bank_float g_CoverHitWallClothingVolume = -4.0f;
bank_float g_CoverHitWallWeaponVolume = 0.0f;

bool g_IgnoreHeelEventsForRadar = true;

audFootstepEventTuning audPedFootStepAudio::sm_FootstepEventTuning;

f32 g_MoveStateClothesVolumeOffset[g_audMaxPedMoveStates] = 
{
	-100.0f, -6.0f, -3.0f, 0.0f
};
f32 g_MoveStateBumpVolumeOffset[g_audMaxPedMoveStates] = 
{
	-100.0f, -12.0f, -3.0f, 0.0f
};

audScene*	 audPedFootStepAudio::sm_CrouchModeScene = NULL;
audCurve	 audPedFootStepAudio::sm_PlayerStealthToClumsiness;
audCurve	 audPedFootStepAudio::sm_ScriptStealthToAudioStealth;
audCurve	 audPedFootStepAudio::sm_PlayerStealthToToeDelayOffset;
audCurve	 audPedFootStepAudio::sm_PlayerClumsinessToMatSweetenerProb;
audCurve	 audPedFootStepAudio::sm_PlayerClumsinessToMatSweetenerVol;

f32 audPedFootStepAudio::sm_CIJumpLandVolumeOffset = 4.f;
f32 audPedFootStepAudio::sm_ClumsyStepToeDelayOffset = -20;
f32 audPedFootStepAudio::sm_DeltaApplyPerStep = 0.025f;

// Speech related
u32 g_TimeInAirForLandVocal = 600;

#if __BANK
audScene*	 audPedFootStepAudio::sm_ScriptStealthModeScene = NULL;
audCurve audPedFootStepAudio::sm_PedVelocityToStealthSceneApply;

f32 audPedFootStepAudio::sm_OverridedMaterialHardness = 1.f;
f32 audPedFootStepAudio::sm_OverridedShoeHardness = 1.f;
f32 audPedFootStepAudio::sm_OverridedFootSpeed = 1.f;
f32 audPedFootStepAudio::sm_OverridedShoeDirtiness = 0.f;
f32 audPedFootStepAudio::sm_OverridedShoeCreakiness = 0.f;
f32 audPedFootStepAudio::sm_OverridedShoeGlassiness = 0.f;
f32 audPedFootStepAudio::sm_OverridedShoeWetness = 0.f;
f32 audPedFootStepAudio::sm_OverridedPlayerStealth = 1.f;

//s32 audPedFootStepAudio::sm_StealthMixingMode = 0;

bool audPedFootStepAudio::sm_ShowExtraLayersInfo = false;
bool audPedFootStepAudio::sm_ScriptStealthMode = false;
bool audPedFootStepAudio::sm_UsePlayerVersionOnFocusPed = false;
bool audPedFootStepAudio::sm_DontUsePlayerVersion = false;
bool audPedFootStepAudio::sm_OverrideShoeGlassiness = false;
bool audPedFootStepAudio::sm_OverrideShoeCreakiness = false;
bool audPedFootStepAudio::sm_OverridePlayerStealth = false;
bool audPedFootStepAudio::sm_OverrideShoeDirtiness = false;
bool audPedFootStepAudio::sm_OverrideFootSpeed = false;
bool audPedFootStepAudio::sm_OverrideMaterialHardness = false;
bool audPedFootStepAudio::sm_OverrideShoeHardness = false;
bool audPedFootStepAudio::sm_OverrideShoeWetness = false;
bool audPedFootStepAudio::sm_OnlyPlayMaterial = false;
bool audPedFootStepAudio::sm_OnlyPlayMaterialCustom = false;
bool audPedFootStepAudio::sm_OnlyPlayMaterialImpact = false;
bool audPedFootStepAudio::sm_OnlyPlayShoe = false;
bool audPedFootStepAudio::sm_WetFeetInfo = false;
bool audPedFootStepAudio::sm_DontApplyScales = false;
bool audPedFootStepAudio::sm_ForceSoftSteps = false;

bool g_bDrawFootsteps = false;
u32 g_FootstepDebugIndex = 0;
const u32 g_NumDebugFootstepInfos = 5;

struct FootstepDebugInfo
{
	char name[255];
	f32 volume;
	f32 velocity;
	atHashString shoeType;
	Vector3 pos;

}g_FootstepDebugInfo[g_NumDebugFootstepInfos];
bool g_bDrawFootstepEvents = false;
u32 g_FootstepEventDebugIndex = 0;
const u32 g_NumDebugFootstepEventInfos = 5;
struct FootstepEventDebugInfo
{
	audFootstepEvent stepType;
	audFootstepEvent state;
	f32 fDecel;
	f32 fFootSpeed;
	f32 upDistanceToGround;
	Vector3 pos;

}g_FootstepEventDebugInfo[g_NumDebugFootstepEventInfos];

void DebugAudioFootstepEvents(audFootstepEvent stepType, audFootstepEvent state, f32 fDecel, f32 fFootSpeed, f32 upDistanceToGround, const Vector3 &bonePos)
{
	if(g_bDrawFootstepEvents)
	{
		g_FootstepEventDebugInfo[g_FootstepEventDebugIndex].stepType = stepType;
		g_FootstepEventDebugInfo[g_FootstepEventDebugIndex].state = state;
		g_FootstepEventDebugInfo[g_FootstepEventDebugIndex].fDecel = fDecel;
		g_FootstepEventDebugInfo[g_FootstepEventDebugIndex].fFootSpeed = fFootSpeed;
		g_FootstepEventDebugInfo[g_FootstepEventDebugIndex].upDistanceToGround = upDistanceToGround;
		g_FootstepEventDebugInfo[g_FootstepEventDebugIndex].pos = bonePos;

		g_FootstepEventDebugIndex = (g_FootstepEventDebugIndex+1)%g_NumDebugFootstepEventInfos;
	}
}



void DebugAudioFootstep(CollisionMaterialSettings *material, f32 velocity, f32 volume, atHashString shoeType, const Vector3 &pos)
{
	if(g_bDrawFootsteps)
	{
		s32 index = -1;
		for(s32 i = 0; i < static_cast<s32>(PGTAMATERIALMGR->GetNumMaterials()); i++)
		{
			if ( g_audCollisionMaterials[i]->NameTableOffset == material->NameTableOffset)
			{
				index = i;
			}
		}
		if (index != -1)
		{
			//Convert from metadata string format to null terminated format.
			sysMemCpy(g_FootstepDebugInfo[g_FootstepDebugIndex].name,g_audCollisionMaterialNames[index].name, 64);
			g_FootstepDebugInfo[g_FootstepDebugIndex].name[64] = '\0';
		}
		g_FootstepDebugInfo[g_FootstepDebugIndex].volume = volume;
		g_FootstepDebugInfo[g_FootstepDebugIndex].velocity = velocity;
		g_FootstepDebugInfo[g_FootstepDebugIndex].shoeType = shoeType;
		g_FootstepDebugInfo[g_FootstepDebugIndex].pos = pos;

		g_FootstepDebugIndex = (g_FootstepDebugIndex+1)%g_NumDebugFootstepInfos;
	}
}

#endif

u32 ConvertPedMoveBlendRatioToAudMoveState(f32 moveBlendRatio)
{
	if(CPedMotionData::GetIsStill(moveBlendRatio))
	{
		return 0;
	}
	else if(CPedMotionData::GetIsWalking(moveBlendRatio))
	{
		return 1;
	}
	else if(CPedMotionData::GetIsRunning(moveBlendRatio))
	{
		return 2;
	}
	else if(CPedMotionData::GetIsSprinting(moveBlendRatio))
	{
		return 3;
	}
	else
	{
		naAssertf(0, "Unhandled moveblend ratio");
		return 0;
	}
}
//-------------------------------------------------------------------------------------------------------------------
audPedFootStepAudio::audPedFootStepAudio()
{
	m_Parent = NULL;
	m_ParentOwningEntity = NULL;
	m_Pockets = 0;
	m_LastMaterial = 0;
	m_LastComponent = 0;
	m_LastStandingEntity = NULL;
	SetCurrentMaterialSettings(g_audCollisionMaterials[0]);
	//m_OverrideFootstepsWithLongGrass = false;
	m_ShoeSettings = NULL;
	m_UpperClothSettings = NULL;
	m_LowerClothSettings = NULL;
	m_FootstepEventsEnabled = true;
	m_ClothEventsEnabled = true;

	for (u8 i = 0; i < MAX_EXTRA_CLOTHING; i ++)
	{
		m_ExtraClothingSettings[i] = NULL;
	}
	m_LadderSlideSound = NULL;
	m_FootstepLoudness = 0.f;
	BANK_ONLY(g_OverridedMaterialSettings = NULL);
}
//-------------------------------------------------------------------------------------------------------------------
audPedFootStepAudio::~audPedFootStepAudio()
{
	m_LastStandingEntity = NULL;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::InitClass()
{
	sm_LinearVelToCreakVolumeCurve.Init(ATSTRINGHASH("LINEAR_VELOCITY_TO_CREAK_VOLUME", 0x42349A2E));
	sm_LinearVelToVolumeCurve.Init(ATSTRINGHASH("LINEAR_VELOCITY_TO_VOLUME", 0x56AA9632));
	sm_AngularAccToVolumeCurve.Init(ATSTRINGHASH("ANGULAR_ACC_TO_VOLUME", 0xECFD12C1));
	sm_ImpactVelocityToIntensity.Init(ATSTRINGHASH("IMPACT_VEL_TO_INTENSITY", 0x9992F2EA));
	sm_SlopeAngleToFallSpeed.Init(ATSTRINGHASH("SLOPE_ANGLE_TO_FALL_SPEED", 0x1A811712));
	sm_SlopeAngleToVolume.Init(ATSTRINGHASH("SLOPE_ANGLE_TO_VOLUME", 0xd42379c2));
	sm_ShoeWetnessToCreakiness.Init(ATSTRINGHASH("SHOE_WETNESS_TO_CREAKINESS", 0x61959B94));
	sm_PedInsideWaterSoundSet.Init(ATSTRINGHASH("PED_INSIDE_WATER", 0x3B6EB9B4));
	sm_FootstepsUnderRainSoundSet.Init(ATSTRINGHASH("FOOTSTEP_UNDER_RAIN_SOUNDSET", 0xFDDB2E7B));
	sm_PlayerStealthToClumsiness.Init(ATSTRINGHASH("PLAYER_STEALTH_TO_CLUMSINESS", 0x3AC21A4F));
	sm_PlayerClumsinessToMatSweetenerProb.Init(ATSTRINGHASH("PLAYER_CLUMSINESS_TO_MAT_SWEETENER_PROB", 0x24DEA70E));
	sm_PlayerClumsinessToMatSweetenerVol.Init(ATSTRINGHASH("PLAYER_CLUMSINESS_TO_MAT_SWEETENER_VOL", 0x915B08F6));
	sm_PlayerStealthToToeDelayOffset.Init(ATSTRINGHASH("PLAYER_STEALTH_TO_TOE_DELAY_OFFSET", 0xBB7E8D4));
	sm_ScriptStealthToAudioStealth.Init(ATSTRINGHASH("SCRIPT_STEALTH_TO_AUDIO_STEALTH", 0xAC1507E2));
	sm_EqualPowerFallCrossFade.Init(ATSTRINGHASH("EQUAL_POWER_FALL_CROSSFADE", 0xEB62241E));
	sm_EqualPowerRiseCrossFade.Init(ATSTRINGHASH("EQUAL_POWER_RISE_CROSSFADE", 0x81841DEC));
	sm_SurfaceDepthToVolDump.Init(ATSTRINGHASH("SURFACE_DEPTH_TO_VOL_DUMP", 0x47930060));
	sm_SurfaceDepthToInterp.Init(ATSTRINGHASH("SURFACE_DEPTH_TO_INTERP", 0xE564C998));
	sm_SlopeDebrisSmoother.Init(0.02f,0.003f,0.f,1.f);
	BANK_ONLY(sm_PedVelocityToStealthSceneApply.Init(ATSTRINGHASH("PED_VELOCITY_TO_STEALTH_SCENE_APPLY_FACTOR", 0xCE4D6BBD)));
	sm_LongGrassFootsepSounds = audNorthAudioEngine::GetObject<MovementShoeSettings>(ATSTRINGHASH("NORMAL_GRASS", 0x8709A1F0));

	g_BoatFootstepMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_PLASTIC", 0xA14B687A));
	LoadFootstepEventTuning();
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ShutdownClass()
{
#if __BANK
	if(sm_WidgetGroup)
	{
		sm_WidgetGroup->Destroy();
		sm_WidgetGroup = NULL; 
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::Init(audPedAudioEntity *pedAudioEntity)
{
	naAssertf(pedAudioEntity, "Trying to initilize audPedFootStepAudio without an audPedAudioEntity");
	m_Parent = pedAudioEntity;
	CPed *pPed = static_cast <CPed*>(m_Parent->GetOwningEntity());
	naAssertf(pPed, "Trying to initilize audPedFootStepAudio without a valid ped.");
	m_ParentOwningEntity = pPed;

	m_ShoeType.SetHash(ATSTRINGHASH("NONE", 0x1D632BA1));
	m_LowerClothing.SetHash(ATSTRINGHASH("LOWER_DENIM", 0x9BC9C57C));
	m_UpperClothing.SetHash(ATSTRINGHASH("UPPER_COTTON", 0xCCFA5A0C));
	
	m_ShoeSettings = const_cast<ShoeAudioSettings*>(GetShoeSettings());
	m_UpperClothSettings = const_cast<ClothAudioSettings*>(GetUpperBodyClothSounds());
	m_LowerClothSettings = const_cast<ClothAudioSettings*>(GetLowerBodyClothSounds());
	for (u8 i = 0; i < MAX_EXTRA_CLOTHING; i ++)
	{
		m_ExtraClothing[i].SetHash(0);
		m_ExtraClothingSettings[i] = const_cast<ClothAudioSettings*>(GetExtraClothSounds(i));
	}
	m_MoveBlendRatioWhenGoingIntoCover = MOVEBLENDRATIO_STILL;
	for(u32 i = 0; i< MAX_NUM_FEET; i++)
	{
		m_FootWaterInfo[i] = audFootOutOfWater;
		m_FootWetness[i] = 0.f;
	}
	for (u32 i =0 ; i < m_SlopeDebrisEvents.GetMaxCount(); i++)
	{
		downSlopEvent slopeEvent;
		slopeEvent.stepType = AUD_FOOTSTEP_WALK_L;
		slopeEvent.downSlopeDirection = Vec3V(V_ONE_WZERO);
		slopeEvent.slopeAngle = 0.f;
		slopeEvent.debrisSound = NULL;
		slopeEvent.ready = false;
		m_SlopeDebrisEvents[i] = slopeEvent;
		
	}
	m_FootstepEvents.Reset();
	m_FoliageEvents.Reset();
	m_ClothsEvents.Reset();
	m_PetrolCanEvents.Reset();
	m_MolotovEvents.Reset();
	m_ScriptSweetenerEvent.Reset();
	m_WaterEvents.Reset();

	m_PetrolCanPouringSound = NULL;
	m_PetrolCanGlugSound = NULL;
	if(m_ParentOwningEntity && m_ParentOwningEntity->IsLocalPlayer())
	{
		sm_PetrolCanSounds.Init(ATSTRINGHASH("FUEL_CAN_SOUNDSET", 0xBD90EF9A));
	}
	else 
	{
		sm_PetrolCanSounds.Init(ATSTRINGHASH("FUEL_CAN_LITE_SOUNDSET", 0x3FDBE854));
	}

	sm_MolotovSounds.Init(ATSTRINGHASH("PED_MOLOTOV_SOUNDSET", 0xFC1F797D));

	sm_PocketsSoundSet.Init(ATSTRINGHASH("POCKET_SOUNDSET", 0x8C560667));

	m_ShoeDirtiness = 0.f;
	m_LastMoveBlendRatio = 0.f;
	m_HasToPlayPetrolCanStop = true;
	m_HasToPlayMolotovStop = true;
	m_WasInStealthMode = false;
	m_ScriptOverridesMode = false;
	m_ScriptTuningValuesHash = 0;
	m_LastCombatRollTime = 0;
	m_FootVolOffsetOverTime = 0.f;
	m_FootstepImpsMagOffsetOverTime = 1.f;
	m_bLastMoveType = -1;
	m_ModeIndex = 0;
	m_LastPedAngVel = 0.f;
	m_ModeHash = ATSTRINGHASH("DEFAULT", 0xE4DF46D5);
	m_HasInitialisedPockets = false;
	m_ClumsyStep = false;
	m_PlayerStealth = 1.f;
	m_ShouldStopWaveImpact = false;

	m_IsWearingElfHat = false;
	m_IsWearingScubaMask = false;
	m_ForceWearingScubaMask = false;
	m_MaterialOverriddenThisFrame = false;
	m_HasToTriggerSuperJumpLaunch = true;

	m_ShouldTriggerSplash = false;

	m_FootstepsTuningValues = NULL;
	m_LadderSlideSound = NULL;
	m_ClothWindSound = NULL;
	m_WaveImpactSound = NULL;

	m_ForceWetFeetReset = false;

	m_WasAiming = false;
	m_FeetInWater = false;
	m_FeetInWaterUpdatedThisFrame = false;

	m_LastTimePlayedFPShuffle = 0;
	m_LastFPFootstepEvent = AUD_FOOTSTEP_WALK_L;

	m_LastSplashTime = 0;
	m_BonnetSlideRecently = false;
}
//-------------------------------------------------------------------------------------------------------------------
CollisionMaterialSettings *audPedFootStepAudio::GetCurrentMaterialSettings() 
{
#if __BANK
	if(g_OverrideMaterialType)
	{
		if(strlen(g_OverrideMaterialName) > 0)
		{
			CollisionMaterialSettings *overridenMat = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(g_OverrideMaterialName);
			if(overridenMat)
			{
				return overridenMat;
			}
		}
		return g_OverridedMaterialSettings;
	}
	else
	{
		if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OverridePlayerGroundMaterial))
		{
			if(sm_OverriddenPlayerMaterialName != 0)
			{
				CollisionMaterialSettings *overridenMat = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(sm_OverriddenPlayerMaterialName);
				if(overridenMat)
				{
					return overridenMat;
				}
			}
		}
		
		if(ShouldForceSnow() && !m_MaterialOverriddenThisFrame)
		{
			CollisionMaterialSettings *snowMat = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_SNOW_COMPACT", 0x10AFB64F));
			if(snowMat)
			{
				return snowMat;
			}
		}

		return m_CurrentMaterialSettings;
	}
#else
	if(g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::OverridePlayerGroundMaterial))
	{
		if(sm_OverriddenPlayerMaterialName != 0)
		{
			CollisionMaterialSettings *overridenMat = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(sm_OverriddenPlayerMaterialName);
			if(overridenMat)
			{
				return overridenMat;
			}
		}
	}

	if(ShouldForceSnow() && !m_MaterialOverriddenThisFrame)
	{
		CollisionMaterialSettings *snowMat = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_SNOW_COMPACT", 0x10AFB64F));
		if(snowMat)
		{
			return snowMat;
		}
	}

	return m_CurrentMaterialSettings;
#endif
}
//-------------------------------------------------------------------------------------------------------------------
ShoeTypes audPedFootStepAudio::GetShoeType()
{
	if( m_ShoeSettings && naVerifyf(m_ShoeSettings->ShoeType < NUM_SHOETYPES, "wrong shoe type from shoe settings %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_ShoeSettings->NameTableOffset)))
	{
		return (ShoeTypes) m_ShoeSettings->ShoeType;
	}
	return SHOE_TYPE_TRAINERS;
}
//-------------------------------------------------------------------------------------------------------------------
ShoeAudioSettings *audPedFootStepAudio::GetShoeSettings() const
{
	ShoeAudioSettings *settings = NULL;
#if __BANK
	if(g_OverridePlayerShoeType)
	{
		if(strlen(g_PlayerShoeTypeOverrideName) > 0)
		{
			ShoeAudioSettings *overridenShoe = audNorthAudioEngine::GetObject<ShoeAudioSettings>(g_PlayerShoeTypeOverrideName);
			if(naVerifyf(overridenShoe, "No shoe audio settings exists for :%s ", g_PlayerShoeTypeOverrideName))
			{
				return overridenShoe;
			}
		}
		//if(naVerifyf(shoeList, "Couldn't find autogenerated shoe list"))
		//{
		//	naAssertf(sm_OverridenPlayerShoeType < shoeList->numShoes, "Out of bounds");
		//	settings =  audNorthAudioEngine::GetObject<ShoeAudioSettings>(shoeList->Shoe[sm_OverridenPlayerShoeType].ShoeAudioSettings);
		//	return settings;
		//}
	}
#endif

	atHashString shoeType = m_ShoeType;
	if((m_ShoeType.GetHash() == ATSTRINGHASH("SHOE_Heel_trainers", 0xAFA3A42B)) || (m_ShoeType.GetHash() == ATSTRINGHASH("SHOE_HighHeel_trainers", 0xB5DDA876)))
	{
		shoeType = ATSTRINGHASH("SHOE_TRAINERS",3325651536);
	}
	settings = audNorthAudioEngine::GetObject<ShoeAudioSettings>(shoeType);
	if(!settings)
	{
		// TODO: delete old code that search through a list.
		ShoeList *shoeList = audNorthAudioEngine::GetObject<ShoeList>(ATSTRINGHASH("ShoeList",0xCB4E5724));
#if __BANK
		if(g_OverridePlayerShoeType)
		{
			if(naVerifyf(shoeList, "Couldn't find autogenerated shoe list"))
			{
				naAssertf(sm_OverridenPlayerShoeType < shoeList->numShoes, "Out of bounds");
				settings =  audNorthAudioEngine::GetObject<ShoeAudioSettings>(shoeList->Shoe[sm_OverridenPlayerShoeType].ShoeAudioSettings);
				return settings;
			}
		}
#endif
		if(naVerifyf(shoeList, "Couldn't find autogenerated shoe type list"))
		{

			for (s32 i = 0; i < shoeList->numShoes; ++i)
			{
				if( shoeType == shoeList->Shoe[i].ShoeName)
				{
					settings =  audNorthAudioEngine::GetObject<ShoeAudioSettings>(shoeList->Shoe[i].ShoeAudioSettings);
					break;
				}
			}
			//#if __BANK
			//		if(g_AudioEngine.GetRemoteControl().IsPresent() && !settings)
			//		{
			//			naWarningf("Couldn't find audio settings for %s.  Defaulting to SHOE_TRAINERS", shoeType.GetCStr());
			//		}
			//#endif
		}
	}
	if(!settings)
	{
		settings =  audNorthAudioEngine::GetObject<ShoeAudioSettings>(ATSTRINGHASH("SHOE_TRAINERS",3325651536));
	}
	return settings;
}    
//-------------------------------------------------------------------------------------------------------------------
const ClothAudioSettings *audPedFootStepAudio::GetBodyClothSounds(atHashString nameHash,bool extra) const
{
	//BANK_ONLY(Displayf("Cloth hash %s",nameHash.GetCStr()));
	ClothAudioSettings *settings = NULL;
	settings = audNorthAudioEngine::GetObject<ClothAudioSettings>(nameHash);
	if(!settings)
	{
		ClothList *clothList = audNorthAudioEngine::GetObject<ClothList>(ATSTRINGHASH("ClothList",2672428439));
		if(naVerifyf(clothList, "Couldn't find autogenerated cloth list"))
		{
			for (s32 i = 0; i < clothList->numCloths; ++i)
			{
				if( nameHash == clothList->Cloth[i].ClothName)
				{
					settings =  audNorthAudioEngine::GetObject<ClothAudioSettings>(clothList->Cloth[i].ClothAudioSettings);
					break;
				}
			}
	//#if __BANK
	//		if(g_AudioEngine.GetRemoteControl().IsPresent() && !settings)
	//		{
	//			naWarningf("Couldn't find audio settings for %s Defaulting to CLOTH_DEFAULT", nameHash.GetCStr());
	//		}
	//#endif
		}
	}
	if(!settings && !extra)
	{
		settings =  audNorthAudioEngine::GetObject<ClothAudioSettings>(ATSTRINGHASH("CLOTH_DEFAULT",1535203820));
	}
	// Look for the overriden player cloths:
	if(settings	&& settings->PlayerVersion != 0)
	{
		CPed *player = CGameWorld::FindLocalPlayer();
#if __BANK
		CPed *focusPed = NULL;
		if(CDebugScene::FocusEntities_Get(0))
		{
			focusPed = (CPed *)CDebugScene::FocusEntities_Get(0);
		}
		if( (m_ParentOwningEntity == player && !sm_DontUsePlayerVersion ) || (sm_UsePlayerVersionOnFocusPed && focusPed == m_ParentOwningEntity))
		{
			settings = audNorthAudioEngine::GetObject<ClothAudioSettings>(settings->PlayerVersion);
		}
#else
		if(m_ParentOwningEntity == player)
		{
			settings = audNorthAudioEngine::GetObject<ClothAudioSettings>(settings->PlayerVersion);
		}
#endif
	}
	return settings;
}
//-------------------------------------------------------------------------------------------------------------------
const ClothAudioSettings *audPedFootStepAudio::GetUpperBodyClothSounds() const
{
#if __BANK
	if(g_OverrideUpperCloths)
	{
		if(strlen(g_UpperClothsOverrideName) > 0)
		{
			ClothAudioSettings *overridenCloths = audNorthAudioEngine::GetObject<ClothAudioSettings>(g_UpperClothsOverrideName);
			if(naVerifyf(overridenCloths, "No cloth audio settings exists for :%s ", g_UpperClothsOverrideName))
			{
				return overridenCloths;
			}
		}
		ClothAudioSettings *settings = NULL;
		ClothList *clothList = audNorthAudioEngine::GetObject<ClothList>(ATSTRINGHASH("ClothList",2672428439));
		if(naVerifyf(clothList, "Couldn't find autogenerated cloth list"))
		{
			naAssertf(sm_OverridenUpperCloths < clothList->numCloths, "Out of bounds");
			settings =  audNorthAudioEngine::GetObject<ClothAudioSettings>(clothList->Cloth[sm_OverridenUpperCloths].ClothAudioSettings);
		}
		if(g_AudioEngine.GetRemoteControl().IsPresent())
		{
			// Look for the overriden player cloths:
			if(settings	&& settings->PlayerVersion != 0)
			{
				CPed *player = CGameWorld::FindLocalPlayer();
				CPed *focusPed = NULL;
				if(CDebugScene::FocusEntities_Get(0))
				{
					focusPed = (CPed *)CDebugScene::FocusEntities_Get(0);
				}
				if( (m_ParentOwningEntity == player && !sm_DontUsePlayerVersion ) || (sm_UsePlayerVersionOnFocusPed && focusPed == m_ParentOwningEntity))
				{
					settings = audNorthAudioEngine::GetObject<ClothAudioSettings>(settings->PlayerVersion);
				}
			}
			if(naVerifyf(settings,"Couldn't find audio settings for the selected upper cloth"))
			{
				return settings;
			}
		}
	}
#endif
	return GetBodyClothSounds(m_UpperClothing);
}
//-------------------------------------------------------------------------------------------------------------------
const ClothAudioSettings* audPedFootStepAudio::GetCachedExtraClothSounds(const u8 idx) const 
{
	naAssertf(idx < MAX_EXTRA_CLOTHING,"Wrong index for extra clothing");
	return m_ExtraClothingSettings[idx];
}
//-------------------------------------------------------------------------------------------------------------------
const ClothAudioSettings *audPedFootStepAudio::GetExtraClothSounds(const u8 idx) const
{
#if __BANK
	if(g_OverrideExtraCloths && idx == 0)
	{
		if(strlen(g_ExtraClothsOverrideName) > 0)
		{
			ClothAudioSettings *overridenCloths = audNorthAudioEngine::GetObject<ClothAudioSettings>(g_ExtraClothsOverrideName);
			if(naVerifyf(overridenCloths, "No cloth audio settings exists for :%s ", g_ExtraClothsOverrideName))
			{
				return overridenCloths;
			}
		}
		ClothAudioSettings *settings = NULL;
		ClothList *clothList = audNorthAudioEngine::GetObject<ClothList>(ATSTRINGHASH("ClothList",2672428439));
		if(naVerifyf(clothList, "Couldn't find autogenerated cloth list"))
		{
			naAssertf(sm_OverridenExtraCloths < clothList->numCloths, "Out of bounds");
			settings =  audNorthAudioEngine::GetObject<ClothAudioSettings>(clothList->Cloth[sm_OverridenExtraCloths].ClothAudioSettings);
		}
		if(g_AudioEngine.GetRemoteControl().IsPresent())
		{
			// Look for the overriden player cloths:
			if(settings	&& settings->PlayerVersion != 0)
			{
				CPed *player = CGameWorld::FindLocalPlayer();
				CPed *focusPed = NULL;
				if(CDebugScene::FocusEntities_Get(0))
				{
					focusPed = (CPed *)CDebugScene::FocusEntities_Get(0);
				}
				if( (m_ParentOwningEntity == player && !sm_DontUsePlayerVersion ) || (sm_UsePlayerVersionOnFocusPed && focusPed == m_ParentOwningEntity))
				{
					settings = audNorthAudioEngine::GetObject<ClothAudioSettings>(settings->PlayerVersion);
				}
			}
			if(naVerifyf(settings,"Couldn't find audio settings for the selected upper cloth"))
			{
				return settings;
			}
		}
	}
#endif
	naAssertf(idx < MAX_EXTRA_CLOTHING,"Wrong index for extra clothing");
	return GetBodyClothSounds(m_ExtraClothing[idx], true);
}
//-------------------------------------------------------------------------------------------------------------------
const ClothAudioSettings *audPedFootStepAudio::GetLowerBodyClothSounds() const
{
#if __BANK
	if(g_OverrideLowerCloths)
	{
		if(strlen(g_LowerClothsOverrideName) > 0)
		{
			ClothAudioSettings *overridenCloths = audNorthAudioEngine::GetObject<ClothAudioSettings>(g_LowerClothsOverrideName);
			if(naVerifyf(overridenCloths, "No cloth audio settings exists for :%s ", g_LowerClothsOverrideName))
			{
				return overridenCloths;
			}
		}
		ClothAudioSettings *settings = NULL;
		ClothList *clothList = audNorthAudioEngine::GetObject<ClothList>(ATSTRINGHASH("ClothList",2672428439));
		if(naVerifyf(clothList, "Couldn't find autogenerated cloth list"))
		{
			naAssertf(sm_OverridenLowerCloths < clothList->numCloths, "Out of bounds");
			settings =  audNorthAudioEngine::GetObject<ClothAudioSettings>(clothList->Cloth[sm_OverridenLowerCloths].ClothAudioSettings);
		}
		if(g_AudioEngine.GetRemoteControl().IsPresent())
		{
			// Look for the overriden player cloths:
			if(settings	&& settings->PlayerVersion != 0)
			{
				CPed *player = CGameWorld::FindLocalPlayer();
				CPed *focusPed = NULL;
				if(CDebugScene::FocusEntities_Get(0))
				{
					focusPed = (CPed *)CDebugScene::FocusEntities_Get(0);
				}
				if( (m_ParentOwningEntity == player && !sm_DontUsePlayerVersion ) || (sm_UsePlayerVersionOnFocusPed && focusPed == m_ParentOwningEntity))
				{
					settings = audNorthAudioEngine::GetObject<ClothAudioSettings>(settings->PlayerVersion);
				}
			}
			if(naVerifyf(settings,"Couldn't find audio settings for the selected upper cloth"))
			{
				return settings;
			}
		}
	}
#endif
	return GetBodyClothSounds(m_LowerClothing);
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingClothes()
{
	return (m_UpperClothing.GetHash() != ATSTRINGHASH("UPPER_BARE",3652298357))
		 && (m_LowerClothing.GetHash() != ATSTRINGHASH("LOWER_BARE",733390942));
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingScubaMask()
{
	if (m_IsWearingScubaMask || m_ForceWearingScubaMask)
	{
		return true;
	}

	for(int i=0; i<m_ExtraClothing.GetMaxCount(); i++)
	{
		if(m_ExtraClothing[i].GetHash() == ATSTRINGHASH("CLOTH_SCUBA", 0x335ED08A) ||
			m_ExtraClothing[i].GetHash() == ATSTRINGHASH("CLOTH_SCUBA_PLAYER", 0x633B9074))
				return true;
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingScubaSuit()
{
	return (m_UpperClothing.GetHash() == ATSTRINGHASH("UPPER_WATERPROOF", 0x4EB38183))
		|| (m_LowerClothing.GetHash() == ATSTRINGHASH("LOWER_WATERPROOF", 0x84F5BA3F));
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingBikini()
{
	return (m_UpperClothing.GetHash() == ATSTRINGHASH("CLOTH_UPPER_BIKINI_TOP", 0xD8520609))
		|| (m_UpperClothing.GetHash() == ATSTRINGHASH("CLOTH_UPPER_BIKINI_TOP_PLAYER", 0x948F0B7C))
		|| (m_LowerClothing.GetHash() == ATSTRINGHASH("CLOTH_LOWER_BIKINI_SPEEDO", 0x51B1AE8B))
		|| (m_LowerClothing.GetHash() == ATSTRINGHASH("CLOTH_LOWER_BIKINI_SPEEDO_PLAYER", 0x64CD0E2D));
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingKifflomSuit()
{
	return (m_UpperClothing.GetHash() == ATSTRINGHASH("CLOTH_UPPER_KIFFLOM", 0xBC2C1A9F))
		|| (m_UpperClothing.GetHash() == ATSTRINGHASH("CLOTH_UPPER_KIFFLOM_PLAYER", 0x23D3C23E));
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingHeels()
{
	return (m_ShoeType.GetHash() == ATSTRINGHASH("SHOE_HEELS", 0x7735E17F) || (m_ShoeType.GetHash() == ATSTRINGHASH("SHOE_Heel_trainers", 0xAFA3A42B)));
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingHighHeels()
{
	return (m_ShoeType.GetHash() == ATSTRINGHASH("SHOE_HIGH_HEELS", 0xEA2B54C0) || (m_ShoeType.GetHash() == ATSTRINGHASH("SHOE_HighHeel_trainers", 0xB5DDA876)));		
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::IsWearingFlapsInWindClothes()
{
	return ((AUD_GET_TRISTATE_VALUE(GetUpperBodyClothSounds()->Flags, FLAG_ID_CLOTHAUDIOSETTINGS_FLAPSINWIND)==AUD_TRISTATE_TRUE)
		|| (AUD_GET_TRISTATE_VALUE(GetLowerBodyClothSounds()->Flags, FLAG_ID_CLOTHAUDIOSETTINGS_FLAPSINWIND)==AUD_TRISTATE_TRUE));
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::SetScriptSweetenerSoundSet(const u32 soundsetHash)
{
	m_ScriptFootstepSweetenersSoundSet.Init(soundsetHash);
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ResetScriptSweetenerSoundSet()
{
	m_ScriptFootstepSweetenersSoundSet.Reset();
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::SetCurrentMaterialSettings(CollisionMaterialSettings * val) 
{
	m_CurrentMaterialSettings = val;

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord() && m_ParentOwningEntity && m_ParentOwningEntity->GetReplayID() != ReplayIDInvalid)	// m_ParentOwningEntity is NULL when this is called from the constructor
	{	
		CReplayMgr::RecordFx<CPacketPedStandingMaterial>(
			CPacketPedStandingMaterial(m_CurrentMaterialSettings->NameHash), 
			m_ParentOwningEntity);
	}
#endif // GTA_REPLAY
}
//-------------------------------------------------------------------------------------------------------------------
// We also do mobile phone type in here, because it's handy.
void audPedFootStepAudio::InitModelSpecifics()
{
	if (naVerifyf(m_ParentOwningEntity,"Missing parent owning entity in pedfootstepaudio InitModelSpecifics.") && m_ParentOwningEntity->GetFootstepHelper().GetModelPhysicsParams())
	{
		m_FootstepsTuningValues = audNorthAudioEngine::GetObject<ModelFootStepTuning>(m_ParentOwningEntity->GetFootstepHelper().GetModelPhysicsParams()->FootstepTuningValues);
		m_GoodStealthIndex = GetFootstepTuningValues(ATSTRINGHASH("GOOD_STEALTH", 0xA85D5721));
		m_BadStealthIndex =  GetFootstepTuningValues(ATSTRINGHASH("BAD_STEALTH", 0xC08D8BAE));
		if(naVerifyf(m_FootstepsTuningValues,"All peds need footsteps tuning values, please bug the audio team."))
		{
			m_FootstepPitchRatio = audDriverUtil::ConvertPitchToRatio(audEngineUtil::GetRandomNumberInRange(m_FootstepsTuningValues->FootstepPitchRatioMin, m_FootstepsTuningValues->FootstepPitchRatioMax));
		}

		if(NetworkInterface::IsGameInProgress() && m_ParentOwningEntity->IsPlayer())
		{
			UpdateVariationData(PV_COMP_JBIB);
			UpdateVariationData(PV_COMP_ACCS);
			UpdateVariationData(PV_COMP_HAND);
			UpdateVariationData(PV_COMP_TASK);
			UpdateVariationData(PV_COMP_FEET);
			UpdateVariationData(PV_COMP_LOWR);
		}
		else 
		{
			UpdateVariationData(PV_COMP_ACCS);
			UpdateVariationData(PV_COMP_BERD);
			UpdateVariationData(PV_COMP_TASK);
			UpdateVariationData(PV_COMP_FEET);
			UpdateVariationData(PV_COMP_LOWR);
			UpdateVariationData(PV_COMP_UPPR);
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::UpdateClass()
{
#if __BANK
	if(sm_ScriptStealthMode)
	{
		if(!sm_ScriptStealthModeScene)
		{
			MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(ATSTRINGHASH("SCRIPT_STEALTH_MODE_SCENE", 0xF331EF7)); 
			if(sceneSettings)
			{
				DYNAMICMIXER.StartScene(sceneSettings, &sm_ScriptStealthModeScene, NULL);
			}
		}
	}
	else if(sm_ScriptStealthModeScene)
	{
		sm_ScriptStealthModeScene->Stop();
		sm_ScriptStealthModeScene = NULL;
	}
	if(sm_TimeToPlayDebugStep > 0 && fwTimer::GetTimeInMilliseconds() > sm_TimeToPlayDebugStep)
	{
		PlayDebugFootstepEvent(sm_Position,sm_MaterialId,sm_HitComponent,sm_HitEntity,sm_Material);
		sm_TimeToPlayDebugStep = -1;
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::UpdateVariationData(ePedVarComp pedVarComp)
{
	if (!m_ParentOwningEntity)
		return;
	atHashString oldVariation;
	switch(pedVarComp)
	{
	case PV_COMP_FEET:
		oldVariation = m_ShoeType;
		m_ShoeType = m_ParentOwningEntity->GetPedAudioID(PV_COMP_FEET);
		if(m_ShoeType.IsNull() || m_ShoeType == ATSTRINGHASH("NONE",0x1D632BA1))
		{
			m_ShoeType = m_ParentOwningEntity->GetSecondPedAudioID(PV_COMP_LOWR);
		}
		if(m_ShoeType.IsNull() || m_ShoeType == ATSTRINGHASH("NONE",0x1D632BA1))
		{
			m_ShoeType = ATSTRINGHASH("SHOE_TRAINERS", 0xC6396A50);
		}	
		if(m_ShoeType != oldVariation)
		{
			m_ShoeSettings = const_cast<ShoeAudioSettings*>(GetShoeSettings());
		}
		break;
	case PV_COMP_LOWR:
		oldVariation = m_LowerClothing;
		m_LowerClothing = m_ParentOwningEntity->GetPedAudioID(PV_COMP_LOWR);
		if((m_LowerClothing != oldVariation) || !m_HasInitialisedPockets)
		{
			m_LowerClothSettings = const_cast<ClothAudioSettings*>(GetLowerBodyClothSounds());
			UpdatePedPockets();
		}
		UpdateVariationData(PV_COMP_FEET);
		break;
	case PV_COMP_UPPR:
	case PV_COMP_JBIB:
		if (NetworkInterface::IsGameInProgress())
		{
			if ( pedVarComp == PV_COMP_UPPR)
				break;
		}
		else if ( pedVarComp == PV_COMP_JBIB )
		{
			break;
		}
		oldVariation = m_UpperClothing;
		m_UpperClothing = m_ParentOwningEntity->GetPedAudioID((u8)pedVarComp);
		if(m_UpperClothing != oldVariation)
		{
			m_UpperClothSettings = const_cast<ClothAudioSettings*>(GetUpperBodyClothSounds());
		}
		break;
	case PV_COMP_TASK:
		oldVariation = m_ExtraClothing[0];
		m_ExtraClothing[0] = m_ParentOwningEntity->GetPedAudioID(PV_COMP_TASK);
		if(m_ExtraClothing[0] != oldVariation)
		{
			m_ExtraClothingSettings[0] = const_cast<ClothAudioSettings*>(GetExtraClothSounds(0));
		}
		break;
	case PV_COMP_ACCS:
		if(NetworkInterface::IsGameInProgress() && m_ParentOwningEntity->IsPlayer())
		{
			if(m_UpperClothing.IsNull() || m_UpperClothing == ATSTRINGHASH("NONE",0x1D632BA1))
			{
				m_UpperClothing = m_ParentOwningEntity->GetPedAudioID(PV_COMP_ACCS);
				m_UpperClothSettings = const_cast<ClothAudioSettings*>(GetUpperBodyClothSounds());
			}
		}
		else
		{
			oldVariation = m_ExtraClothing[1];
			m_ExtraClothing[1] = m_ParentOwningEntity->GetPedAudioID(PV_COMP_ACCS);
			if(m_ExtraClothing[1] != oldVariation)
			{
				m_ExtraClothingSettings[1] = const_cast<ClothAudioSettings*>(GetExtraClothSounds(1));
			}
		}
		break;
	case PV_COMP_BERD:
		oldVariation = m_ExtraClothing[2];
		m_ExtraClothing[2] = m_ParentOwningEntity->GetPedAudioID((u8)pedVarComp);
		if(m_ExtraClothing[2] != oldVariation)
		{
			m_ExtraClothingSettings[2] = const_cast<ClothAudioSettings*>(GetExtraClothSounds(2));
		}
		break;
	case PV_COMP_HAND:
		oldVariation = m_ExtraClothing[3];
		m_ExtraClothing[3] = m_ParentOwningEntity->GetPedAudioID((u8)pedVarComp);
		if(m_ExtraClothing[3] != oldVariation)
		{
			m_ExtraClothingSettings[3] = const_cast<ClothAudioSettings*>(GetExtraClothSounds(3));
		}
		break;
	default:
		break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::UpdatePedPockets()
{
	m_HasInitialisedPockets = true;
	if (DoesPedHavePockets())
	{
		bool keysInLeft = audEngineUtil::ResolveProbability(m_FootstepsTuningValues->InLeftPocketProbability);
		bool hasKeys = audEngineUtil::ResolveProbability(m_FootstepsTuningValues->HasKeysProbability);
		bool hasMoney = audEngineUtil::ResolveProbability(m_FootstepsTuningValues->HasMoneyProbability);
		if (keysInLeft && hasKeys)
		{
			m_Pockets |= 1;
		}
		else if(!keysInLeft && hasKeys)
		{
			m_Pockets |= 2;
		}
		if (!keysInLeft && hasMoney)
		{
			m_Pockets |= 4;
		}
		else if(keysInLeft && hasMoney)
		{
			m_Pockets |= 8;
		} 
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::ShouldForceSnow()
{	
	if(g_vfxPed.GetUseSnowFootVfxWhenUnsheltered())
	{
		if(m_Parent)
		{
			CPed* parentPed = m_Parent->GetOwningPed();

			if(parentPed)
			{
				if(!parentPed->GetIsShelteredFromRain() &&
				   !GetIsStandingOnVehicle())
				{
					return true;
				}
			}
		}
	}

	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::Update()
{
	if(!m_ParentOwningEntity)
		return;
	bool isLocalPlayer = m_ParentOwningEntity->IsLocalPlayer();

	if(m_ParentOwningEntity->GetFootstepHelper().GetModelPhysicsParams() && m_FootstepsTuningValues)
	{
		if(m_ParentOwningEntity->GetVehiclePedInside())
		{
			ResetWetFeet();
		}
		// Get the footstep tuning values
		u32 modeHash = ATSTRINGHASH("DEFAULT", 0xE4DF46D5);
		if(m_ScriptOverridesMode)
		{
			modeHash = m_ScriptTuningValuesHash;
		}
#if ENABLE_DRUNK
		else if (m_ParentOwningEntity->IsDrunk())
		{
			modeHash = ATSTRINGHASH("DRUNK", 0x3FDBA102);
		}
#endif // ENABLE_DRUNK
		//else if (m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
		//{
		//	modeHash = ATSTRINGHASH("STEALTH", 0x74A0BCB7);
		//}
		else if (DYNAMICMIXER.IsStealthScene())
		{
			modeHash = 	ATSTRINGHASH("NORMAL_STEALTH", 0x7750D2EF);
		}
		if( modeHash != m_ModeHash)
		{
			m_ModeIndex = GetFootstepTuningValues(modeHash);
			m_ModeHash = modeHash;
		}
		if(isLocalPlayer)
		{
			//UpdateVariationData();
			m_PlayerStealth = sm_ScriptStealthToAudioStealth.CalculateValue(StatsInterface::GetFloatStat(STAT_STEALTH_ABILITY.GetStatId()));
#if __BANK
			if(sm_OverridePlayerStealth)
			{
				m_PlayerStealth = sm_OverridedPlayerStealth;
			}
#endif
			bool inStealth = m_ParentOwningEntity->GetMotionData()->GetUsingStealth();
			if(inStealth && audSuperConductor::sm_StopQSOnPlayerInStealth)
			{
				ConductorMessageData message;
				message.conductorName = SuperConductor;
				message.message = StopQuietScene;
				message.bExtraInfo = false;
				message.vExtraInfo = Vector3((f32)audSuperConductor::sm_PlayerInStealthQSFadeOutTime
											,(f32)audSuperConductor::sm_PlayerInStealthQSDelayTime
											,(f32)audSuperConductor::sm_PlayerInStealthQSFadeInTime);
				SUPERCONDUCTOR.SendConductorMessage(message);
			}
			if(!DYNAMICMIXER.IsStealthScene())
			{
				const u32 crouchModeSceneName = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::PlayerOnDLCHeist4Island) ? ATSTRINGHASH("CROUCH_MODE_IH_SCENE", 0xBCDA9B63) : ATSTRINGHASH("CROUCH_MODE_SCENE", 0x5701BDBC);
				const u32 crouchModePatchName = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::PlayerOnDLCHeist4Island) ? ATSTRINGHASH("CROUCH_MODE_IH_PATCH", 0xD06740EF) : ATSTRINGHASH("CROUCH_MODE_PATCH", 0xFE5C0FE1);

				if(inStealth && !sm_CrouchModeScene)
				{					
					MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(crouchModeSceneName); 
					if(sceneSettings)
					{
						DYNAMICMIXER.StartScene(sceneSettings, &sm_CrouchModeScene, NULL);
					}
					if(sm_CrouchModeScene)
					{
						sm_CrouchModeScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8),0.f);
					}
				}
				if(sm_CrouchModeScene)
				{
					// DYNAMIC MIXER SCENES
					audDynMixPatch* patch = sm_CrouchModeScene->GetPatch(crouchModePatchName);					
					f32 currentApply = patch ? patch->GetApplyFactor() : 0.f;
					if(inStealth)
					{
						f32 timeStep = (f32)fwTimer::GetTimeStepInMilliseconds();
						f32 delta = timeStep/(f32)sm_FadeInCrouchScene;
						currentApply += delta;
						currentApply = Clamp(currentApply,0.f,1.f);
					}
					else
					{
						f32 timeStep = (f32)fwTimer::GetTimeStepInMilliseconds();
						f32 delta = timeStep/(f32)sm_FadeOutCrouchScene;
						currentApply -= delta;
						currentApply = Clamp(currentApply,0.f,1.f);
					}
					sm_CrouchModeScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8),currentApply);
					if(currentApply == 0)
					{
						sm_CrouchModeScene->Stop();
						sm_CrouchModeScene = NULL;
					}
				}
			}
			else
			{
				if(sm_CrouchModeScene)
				{
					sm_CrouchModeScene->Stop();
					sm_CrouchModeScene = NULL;
				}
			}
			bool isAiming = m_ParentOwningEntity->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning( CTaskTypes::TASK_AIM_GUN_ON_FOOT );
			if(	(!m_WasAiming && isAiming) || (m_WasAiming && !isAiming) )
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
				m_Parent->CreateAndPlaySound(ATSTRINGHASH("WEAPON_RATTLE_AIM_SOUND", 0x2D384D13), &initParams);
			}
			m_WasAiming = isAiming;
		}
	}
#if __BANK
			if(g_DisplayGroundMaterial)
			{
				CollisionMaterialSettings *material = FindPlayerPed()->GetPedAudioEntity()->GetFootStepAudio().GetCurrentMaterialSettings();

				const char *materialName = NULL;

				if(material)
				{
					materialName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(material->NameTableOffset);	

				}

				if(!materialName)
				{
					materialName = "(null)";
				}
				formatf(g_PlayerMaterialName, "%s", materialName);

				material = GetCurrentMaterialSettings();

				if(material)
				{
					materialName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(material->NameTableOffset);	

				}
				if(!materialName)
				{
					materialName = "(null)";
				}
				char txt[512];
				formatf(txt,"%s",materialName);
				Vector3 headPos = m_ParentOwningEntity->GetBonePositionCached(BONETAG_HEAD);
				grcDebugDraw::Text(headPos,Color_white,txt);
			}
			if(sm_ShowExtraLayersInfo)
			{
				char txt[512];
				Vector3 headPos = m_ParentOwningEntity->GetBonePositionCached(BONETAG_HEAD);
				formatf(txt,"[Glassiness %f] [Dirtiness %f] ",GetGlassiness(),GetShoeDirtiness());
				grcDebugDraw::Text(headPos,Color_white,txt);
			}
#endif

#if DEBUG_DRAW
			if(m_ParentOwningEntity->IsLocalPlayer())
			{
				if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_X, KEYBOARD_MODE_DEBUG, "show player material"))
				{
					sm_ShouldShowPlayerMaterial = !sm_ShouldShowPlayerMaterial;
				}
				if(sm_ShouldShowPlayerMaterial)
				{
					char pedDebug[256] = "";
					sprintf(pedDebug, "f: %s L: %s u: %s E1:%s E2:%s E3:%s E4:%s", m_ShoeType.GetCStr(), m_LowerClothing.GetCStr(), m_UpperClothing.GetCStr(),m_ExtraClothing[0].GetCStr(),m_ExtraClothing[1].GetCStr(),m_ExtraClothing[2].GetCStr(),m_ExtraClothing[3].GetCStr());
					grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_ParentOwningEntity->GetTransform().GetPosition()) + Vector3(0.f,0.f,0.5f), Color32(200,200,200), pedDebug);
				}
			}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::UpdateFootstetVolOffsetOverTime()
{
	// Using the footstep step types to keep track of what was happening last frame.
	// TODO: Improve the logic to change also with material types | hardness, etc... 
	// see what would be interesting. 
	if(m_ParentOwningEntity->GetMotionData()->GetIsWalking() )
	{
		if(m_bLastMoveType == AUD_FOOTSTEP_WALK_L)
		{
			m_FootVolOffsetOverTime += sm_FootstepVolDecayOverTime * fwTimer::GetTimeStep();
			m_FootVolOffsetOverTime = Clamp(m_FootVolOffsetOverTime, sm_MaxFootstepVolDecay, 0.f);
			m_FootstepImpsMagOffsetOverTime -= sm_FootstepImpMagDecayOverTime * fwTimer::GetTimeStep();
			m_FootstepImpsMagOffsetOverTime = Clamp(m_FootstepImpsMagOffsetOverTime, sm_MinFootstepImpMagDecay, 1.f);
		}
		else 
		{
			m_FootstepImpsMagOffsetOverTime = 1.f;
			m_FootVolOffsetOverTime = 0.f;
		}
		m_bLastMoveType = AUD_FOOTSTEP_WALK_L;
	}
	else if(m_ParentOwningEntity->GetMotionData()->GetIsRunning())
	{
		if(m_bLastMoveType == AUD_FOOTSTEP_RUN_L)
		{
			m_FootVolOffsetOverTime += sm_FootstepVolDecayOverTime * fwTimer::GetTimeStep();
			m_FootVolOffsetOverTime = Clamp(m_FootVolOffsetOverTime, sm_MaxFootstepVolDecay, 0.f);
			m_FootstepImpsMagOffsetOverTime -= sm_FootstepImpMagDecayOverTime * fwTimer::GetTimeStep();
			m_FootstepImpsMagOffsetOverTime = Clamp(m_FootstepImpsMagOffsetOverTime, sm_MinFootstepImpMagDecay, 1.f);
		}
		else 
		{
			m_FootstepImpsMagOffsetOverTime = 1.f;
			m_FootVolOffsetOverTime = 0.f;
		}
		m_bLastMoveType = AUD_FOOTSTEP_RUN_L;
	}
	else if(m_ParentOwningEntity->GetMotionData()->GetIsSprinting())
	{
		if(m_bLastMoveType == AUD_FOOTSTEP_SPRINT_L)
		{
			m_FootVolOffsetOverTime += sm_FootstepVolDecayOverTime * fwTimer::GetTimeStep();
			m_FootVolOffsetOverTime = Clamp(m_FootVolOffsetOverTime, sm_MaxFootstepVolDecay, 0.f); 
			m_FootstepImpsMagOffsetOverTime -= sm_FootstepImpMagDecayOverTime * fwTimer::GetTimeStep();
			m_FootstepImpsMagOffsetOverTime = Clamp(m_FootstepImpsMagOffsetOverTime, sm_MinFootstepImpMagDecay, 1.f);
		}
		else 
		{
			m_FootstepImpsMagOffsetOverTime = 1.f;
			m_FootVolOffsetOverTime = 0.f;
		}
		m_bLastMoveType = AUD_FOOTSTEP_SPRINT_L;
	}
	else if (m_ParentOwningEntity->GetMotionData()->GetIsStill())
	{
		m_FootstepImpsMagOffsetOverTime = 1.f;
		m_FootVolOffsetOverTime = 0.f;
	}

}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::UpdateWetness()
{
	if(m_ParentOwningEntity->IsLocalPlayer() && m_ParentOwningEntity->GetFootstepHelper().IsWalkingOnPuddle())
	{
		for (u32 i = 0; i < MAX_NUM_FEET; i++)
		{
			m_FootWetness[i] = 1.f;
		}
	}
	// Override the feet wetness if we are walking on a wet material (i.e. SAND_WET).
	// if the weather is wet (i.e. is raining) get the Max between the actual feet wetness and the weather wetness.
	if(g_weather.GetWetness() > 0.4f && !m_ParentOwningEntity->GetIsInInterior() && !g_PlayerSwitch.IsActive())
	{
		for (u32 i = 0; i < MAX_NUM_FEET; i++)
		{
			m_FootWetness[i] = Max(m_FootWetness[i],g_weather.GetWetness());
			m_FootWetness[i] = Clamp(m_FootWetness[i], 0.f, 1.f);
		}
	}
#if __BANK
	if(sm_WetFeetInfo)
	{
		for (u32 i = 0; i < MAX_NUM_FEET; i++)
		{
			grcDebugDraw::AddDebugOutput("[Feet %d] [Wetness %f]",i,m_FootWetness[i]);
		}

	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ResetEvents(bool isAnAnimal)
{
	m_FoliageEvents.Reset();
	m_FootstepEvents.Reset();
	m_WaterEvents.Reset();
	if(!isAnAnimal)
	{
		m_ClothsEvents.Reset();
		m_PetrolCanEvents.Reset();
		m_MolotovEvents.Reset();
		m_ScriptSweetenerEvent.Reset();
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::DoFootstepStealth()
{
	if (m_FootstepLoudness > 0.0f)
	{
		if(Verifyf(m_ParentOwningEntity->GetPedIntelligence(),"NULL ped intelligence pointer"))
		{
			float fNoise = m_FootstepLoudness;
			// Add a sonar blip onto the minimap for this char if they are either blipped or set to generate footstep events
			if( (m_ParentOwningEntity->GetPedIntelligence()->GetPedStealth().GetFlags().IsFlagSet(CPedStealth::SF_PedGeneratesFootStepEvents) ||
				m_ParentOwningEntity->GetPedConfigFlag( CPED_CONFIG_FLAG_BlippedByScript )) && !m_ParentOwningEntity->IsInjured() )
			{
				CPlayerInfo* pPlayerInfo = m_ParentOwningEntity->GetPlayerInfo();
				if(pPlayerInfo)
				{
					if (m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
					{
						fNoise = 0.0f; // Players shouldn't generate footstep noise while in stealth [12/13/2012 mdawe]
					}
				}
				fNoise = CMiniMap::CreateSonarBlipAndReportStealthNoise(m_ParentOwningEntity, m_ParentOwningEntity->GetTransform().GetPosition(), fNoise, HUD_COLOUR_BLUEDARK);
			}

			if (fNoise > 0.0f && m_ParentOwningEntity->GetPedIntelligence()->GetPedStealth().GetFlags().IsFlagSet(CPedStealth::SF_PedGeneratesFootStepEvents))
			{
				CEventFootStepHeard footStepEvent(m_ParentOwningEntity, fNoise);
				GetEventGlobalGroup()->Add(footStepEvent);

				CEventSuspiciousActivity suspiciousEvent(m_ParentOwningEntity, VEC3V_TO_VECTOR3(m_ParentOwningEntity->GetTransform().GetPosition()), fNoise);
				GetEventGlobalGroup()->AddOrCombine(suspiciousEvent);
			}
		}
		m_FootstepLoudness = 0.f;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessFootsteps()
{
#if __BANK
	if(g_OverrideMaterialType)
	{
		g_OverridedMaterialSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(g_audCollisionMaterialNames[sm_CollisionMaterial].name);
	}
#endif
	bool hasProcessedEvents = false;
	for(int i=0; i < m_FootstepEvents.GetCount(); i++)
	{
		// Double check we've got a variable block at this point, if not create one.
		// There are some system that could potentially trigger a sound when we don't yet have the entity variable block allocated
		// due to different ways of computing LOD distances (for example using the camera instead of the listener)
		if( !m_Parent->HasEntityVariableBlock())
		{
			m_Parent->AllocateVariableBlock();
		}
		hasProcessedEvents = true;
		if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(m_FootstepEvents[i])] >= audLegsUnderWater)
		{
			continue;
		}

		if(!m_ParentOwningEntity->GetFootstepHelper().IsAnAnimal())
		{
			PlayFootstepEvent(m_FootstepEvents[i]);
		}
		else
		{
			PlayAnimalFootStepsEvent(m_FootstepEvents[i]);
		}
		// Dry out the feet if they are out of water or if the weather is dry.
#if GTA_REPLAY
		// Don't automatically dry out the feet if we're playing back a replay
		// as we've recorded the wetness
		if(!CReplayMgr::IsEditModeActive())
#endif // GTA_REPLAY
			if(m_FootstepEvents[i] != AUD_FOOTSTEP_LIFT_L && m_FootstepEvents[i] != AUD_FOOTSTEP_LIFT_R)
			{
				FeetTags foot = m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(m_FootstepEvents[i]);
				if(m_FootWaterInfo[foot] == audFootOutOfWater)
				{
					f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)GetCurrentMaterialSettings()->FootstepMaterialHardness ;
					m_FootWetness[foot] -= Max((m_FootWetness[foot] * sm_DryOutRatio * materialHardness),sm_MinDryOutRatio);
					m_FootWetness[foot] = Clamp(m_FootWetness[foot], 0.f, 1.f);
				}
			}
	}
	if(hasProcessedEvents && m_ParentOwningEntity && m_ParentOwningEntity->IsLocalPlayer())
	{
		CPedFootStepHelper::SetWalkingOnPuddle( false );
	}
#if __BANK
	if(g_bDrawFootsteps )
	{
		for(u32 i = 0; i < g_NumDebugFootstepInfos; i++) 
		{
			char buf[256];
			formatf(buf, sizeof(buf), "[material = %s] [velocity = %f] ",g_FootstepDebugInfo[i].name, g_FootstepDebugInfo[i].velocity);
			grcDebugDraw::Text(g_FootstepDebugInfo[i].pos, Color_black, buf);
			const char* shoeName = g_FootstepDebugInfo[i].shoeType.GetCStr();
			formatf(buf, sizeof(buf), "[vol = %f] [shoe = %s]",g_FootstepDebugInfo[i].volume, shoeName);
			grcDebugDraw::Text(g_FootstepDebugInfo[i].pos + Vector3(0.f,0.f,0.1f), Color_black, buf);
		}
	}
	if(g_bDrawFootstepEvents) 
	{
		for(u32 i = 0; i < g_NumDebugFootstepEventInfos; i++)
		{
			char buf[256];
			formatf(buf, sizeof(buf), "[stepType = %s] [fDecel = %f] ",GetStepTypeName(g_FootstepEventDebugInfo[i].stepType),g_FootstepEventDebugInfo[i].fDecel);
			grcDebugDraw::Text(g_FootstepEventDebugInfo[i].pos, Color_black, buf);
			formatf(buf, sizeof(buf), "[fFootSpeed = %f][upDistanceToGround = %f]",g_FootstepEventDebugInfo[i].fFootSpeed, g_FootstepEventDebugInfo[i].upDistanceToGround);
			grcDebugDraw::Text(g_FootstepEventDebugInfo[i].pos + Vector3(0.f,0.f,0.1f), Color_black, buf);
		}
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessClothEvents(const f32 angularVelocity)
{	
	// Get linear and angular velocities and accelerations.
	for(int i=0; i < m_ClothsEvents.GetCount(); i++)
	{
		PlayClothEvent(m_ClothsEvents[i],angularVelocity);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessBushEvents(const f32 angularVelocity)
{	
	// Get linear and angular velocities and accelerations.
	for(int i=0; i < m_FoliageEvents.GetCount(); i++)
	{
		PlayFoliageEvent(m_FoliageEvents[i], angularVelocity);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessSlopeDebrisEvents()
{	
	for(int i=0; i < m_SlopeDebrisEvents.GetMaxCount(); i++)
	{
		if(!m_SlopeDebrisEvents[i].debrisSound && m_SlopeDebrisEvents[i].ready)
		{
			if ( m_ParentOwningEntity->GetFootstepHelper().IsWalkingOnASlope())
			{
				// First of all get the material settigns to pick the proper debris sound. 
				CollisionMaterialSettings *groundMaterial = GetCurrentMaterialSettings();
				if(groundMaterial && groundMaterial->DebrisOnSlope != g_NullSoundHash)
				{
					// means we haven't started this debris sound yet. 
					audSoundInitParams initParams; 
					Vec3V pos; 
					m_ParentOwningEntity->GetFootstepHelper().GetPositionForPedSound(m_SlopeDebrisEvents[i].stepType, pos);
					initParams.Position = VEC3V_TO_VECTOR3(pos);
					m_Parent->CreateAndPlaySound_Persistent(groundMaterial->DebrisOnSlope,&m_SlopeDebrisEvents[i].debrisSound,&initParams);
				}
			}
			m_SlopeDebrisEvents[i].ready = false;
		}
		else if(m_SlopeDebrisEvents[i].debrisSound)
		{
			f32 movementFactor = sm_SlopeAngleToFallSpeed.CalculateValue(m_SlopeDebrisEvents[i].slopeAngle);
#if __BANK
			if(CPedFootStepHelper::OverrideSlopeVariables())
			{
				movementFactor = CPedFootStepHelper::OverriddenMovementFactor();
			}
#endif
			Vec3V newDebrisPosition = m_SlopeDebrisEvents[i].debrisSound->GetRequestedPosition()
				+  ScalarV(movementFactor) * m_SlopeDebrisEvents[i].downSlopeDirection;
			m_SlopeDebrisEvents[i].debrisSound->SetRequestedPosition(newDebrisPosition);

			f32 desireVolume = (m_ParentOwningEntity->GetFootstepHelper().IsWalkingOnASlope()) ? (f32)sm_SlopeAngleToVolume.CalculateValue(m_SlopeDebrisEvents[i].slopeAngle): 0.f;
			static const f32 incRateMin = 0.002f;
			static const f32 incRateMax = 0.003f;
			static const f32 decRateMin = 0.03f;
			static const f32 decRateMax = 0.07f;
			sm_SlopeDebrisSmoother.SetRates(audEngineUtil::GetRandomNumberInRange(incRateMin, incRateMax), 
				audEngineUtil::GetRandomNumberInRange(decRateMin, decRateMax));
			desireVolume =  sm_SlopeDebrisSmoother.CalculateValue(desireVolume,g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			m_SlopeDebrisEvents[i].debrisSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(desireVolume));
			if(desireVolume <= g_SilenceVolumeLin )
			{
				if(m_SlopeDebrisEvents[i].debrisSound)
				{
					m_SlopeDebrisEvents[i].debrisSound->StopAndForget();
				}
			}
#if __BANK
			if(CPedFootStepHelper::ShowSlopeInfo())
			{
				if(m_SlopeDebrisEvents[i].debrisSound)
				{
					char txt[256]; 
					formatf(txt,"Slope : [Angle %f] [Movement factor %f]",m_SlopeDebrisEvents[i].slopeAngle,movementFactor);
					grcDebugDraw::Text(m_SlopeDebrisEvents[i].debrisSound->GetRequestedPosition(),Color_white,txt);
					grcDebugDraw::Sphere(m_SlopeDebrisEvents[i].debrisSound->GetRequestedPosition(),0.1f,Color_red);
				}
			}
#endif
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessWaterEvents()
{	
	// Get linear and angular velocities and accelerations.
	for(int i=0; i < m_WaterEvents.GetCount(); i++)
	{
		PlayWaterEvent(m_WaterEvents[i]);
	}
	// Clean the feet water info but store the last state
	for(u32 i = 0; i < MAX_NUM_FEET; i++)
	{
		m_FootWaterInfo[i] = audFootOutOfWater;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddWaterEvent(audFootstepEvent stepType)
{
	if(m_WaterEvents.IsFull())
	{
		return;
	}

#if GTA_REPLAY
	// During replay playback we want to avoid the check below
	if(CReplayMgr::IsEditModeActive() && replayVerify(m_WaterEvents.GetCount() < MAX_NUM_FEET))
	{
		m_WaterEvents.Push(stepType);
		return;
	}
#endif // GTA_REPLAY

	//TODO: here is where we can check if we need to play a splash even if the foot is under water based on the movement type. 
	// for example if the foot is underwater, we don't want to play the splash, but if when running it goes out of water we would need it. 
	if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(stepType)] != audFootOutOfWater)
	{
		m_WaterEvents.Push(stepType);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketPedWaterSound>(
				CPacketPedWaterSound((u8)stepType),
				m_ParentOwningEntity);
		}
#endif // GTA_REPLAY
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::UpdateWetFootInfo(eAnimBoneTag boneTag)
{
	// Dont update if script is forcing a reset
	if( m_ForceWetFeetReset )
	{
		return;
	}
	// Get the ragdoll component 
	s32 componentId = m_ParentOwningEntity->GetRagdollComponentFromBoneTag(boneTag);
	if(componentId == RAGDOLL_FOOT_LEFT)
	{
		m_FootWetness[LeftFoot] = 1.f;
		m_FootWaterInfo[LeftFoot] = Max(m_FootWaterInfo[LeftFoot],audFootUnderWater);
	}
	else if (componentId == RAGDOLL_FOOT_RIGHT)
	{
		m_FootWetness[RightFoot] = 1.f;
		m_FootWaterInfo[RightFoot] = Max(m_FootWaterInfo[RightFoot],audFootUnderWater);
	}
	else if (componentId == RAGDOLL_THIGH_LEFT || componentId == RAGDOLL_SHIN_LEFT || componentId == RAGDOLL_BUTTOCKS)
	{
		m_FootWetness[LeftFoot] = 1.f;
		m_FootWaterInfo[LeftFoot] = Max(m_FootWaterInfo[LeftFoot],audLegsUnderWater);
	}
	else if (componentId == RAGDOLL_THIGH_RIGHT || componentId == RAGDOLL_SHIN_RIGHT || componentId == RAGDOLL_BUTTOCKS)
	{
		m_FootWetness[RightFoot] = 1.f;
		m_FootWaterInfo[RightFoot] = Max(m_FootWaterInfo[RightFoot],audLegsUnderWater);
	}
	else if(componentId >= RAGDOLL_SPINE0)
	{
		m_FootWetness[LeftFoot] = 1.f;
		m_FootWaterInfo[LeftFoot] = Max(m_FootWaterInfo[LeftFoot],audBodyUnderWater);
		m_FootWetness[RightFoot] = 1.f;
		m_FootWaterInfo[RightFoot] = Max(m_FootWaterInfo[RightFoot],audBodyUnderWater);
	}

	m_FeetInWater = ((m_FootWaterInfo[LeftFoot] != audFootOutOfWater) || (m_FootWaterInfo[RightFoot] != audFootOutOfWater));
	m_FeetInWaterUpdatedThisFrame= true;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ResetWetFeet(bool forceReset)
{
	for (u32 i = 0; i < MAX_NUM_FEET; i++)
	{
		m_FootWetness[i] = 0.f;
	}
	m_ForceWetFeetReset = forceReset;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessMolotovEvents()
{	
	const CPedWeaponManager* weaponManager = m_ParentOwningEntity->GetWeaponManager();
	if (weaponManager) 
	{
		const CWeapon* weapon = weaponManager->GetEquippedWeapon();
		if (weapon) 
		{
			for(int i=0; i < m_MolotovEvents.GetCount(); i++)
			{
				PlayMolotovEvent(m_MolotovEvents[i]);
			}
			if (weapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_MOLOTOV", 0x24B17070) REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive())) 
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
				if(m_ParentOwningEntity->IsLocalPlayer())
				{
					CPad *pPad = CControlMgr::GetPlayerPad();
					if(!m_ParentOwningEntity->GetMotionData()->GetIsStill() && pPad && pPad->GetLeftStickY() == 0 && m_HasToPlayMolotovStop)
					{
						m_HasToPlayMolotovStop = false;

						if(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() <= MOVEBLENDRATIO_WALK ) 
						{
							m_Parent->CreateAndPlaySound(sm_MolotovSounds.Find(ATSTRINGHASH("STOP_WALK_MOVE", 0x4D10F46)),&initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_MolotovSounds.GetNameHash(),ATSTRINGHASH("STOP_WALK_MOVE", 0x4D10F46), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
						}
						else if(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() <= MOVEBLENDRATIO_RUN ) 
						{
							m_Parent->CreateAndPlaySound(sm_MolotovSounds.Find(ATSTRINGHASH("STOP_RUN_MOVE", 0x8A3C9CFC)),&initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_MolotovSounds.GetNameHash(),ATSTRINGHASH("STOP_RUN_MOVE", 0x8A3C9CFC), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
						}
						else
						{
							m_Parent->CreateAndPlaySound(sm_MolotovSounds.Find(ATSTRINGHASH("STOP_SPRINT_MOVE", 0xF17BD183)),&initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_MolotovSounds.GetNameHash(),ATSTRINGHASH("STOP_SPRINT_MOVE", 0xF17BD183), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
						}
					}
					else if(pPad && pPad->GetLeftStickY() != -0)
					{
						m_HasToPlayMolotovStop = true;
					}
				}
			}
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessScriptSweetenerEvents()
{	
	audSoundInitParams initParams;
	initParams.UpdateEntity = true;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
	for(int i=0; i < m_ScriptSweetenerEvent.GetCount(); i++)
	{
		PlayScriptSweetenerEvent(m_ScriptSweetenerEvent[i]);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessPetrolCanEvents()
{	
	const CPedWeaponManager* weaponManager = m_ParentOwningEntity->GetWeaponManager();
	bool shouldStopSounds = true;
	if (weaponManager) 
	{
		const CWeapon* weapon = weaponManager->GetEquippedWeapon();
		if (weapon) 
		{
			f32 petrolLevel = petrolLevel = (f32)weapon->GetAmmoTotal()/ ((f32)weapon->GetClipSize() - 1000);
			for(int i=0; i < m_PetrolCanEvents.GetCount(); i++)
			{
				PlayPetrolCanEvent(m_PetrolCanEvents[i],petrolLevel);
			}

			const bool bIsPetrolCan = weapon->GetWeaponInfo() && weapon->GetWeaponInfo()->GetGroup() == WEAPONGROUP_PETROLCAN;
			if(bIsPetrolCan REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()))
			{
				shouldStopSounds = false;
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();

				CTaskGun* task = (CTaskGun *)m_ParentOwningEntity->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN);
				if (task && task->GetIsFiring())
				{
					//TEMPORARY thing to be able to listen to the whole sound. ( the max ammor for the petrol can is 2000)
					naAssertf(weapon->GetClipSize() > 1000, "Bad clip size for audio purpose.");
					//f32 petrolLevel = (f32)weapon->GetAmmoTotal()/ ((f32)weapon->GetClipSize());
					if(!m_PetrolCanPouringSound)
					{
						m_Parent->CreateSound_PersistentReference(sm_PetrolCanSounds.Find(ATSTRINGHASH("POURING", 0x76F93140)),&m_PetrolCanPouringSound,&initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(sm_PetrolCanSounds.GetNameHash(), ATSTRINGHASH("POURING", 0x76F93140), &initParams, m_PetrolCanPouringSound, m_ParentOwningEntity, eNoGlobalSoundEntity);)
						if(m_PetrolCanPouringSound)
						{
							m_PetrolCanPouringSound->PrepareAndPlay();
						}
					}
					if(!m_PetrolCanGlugSound)
					{
						m_Parent->CreateSound_PersistentReference(sm_PetrolCanSounds.Find(ATSTRINGHASH("GLUG", 0x71116457)),&m_PetrolCanGlugSound,&initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(sm_PetrolCanSounds.GetNameHash(), ATSTRINGHASH("GLUG", 0x71116457), &initParams, m_PetrolCanGlugSound, m_ParentOwningEntity, eNoGlobalSoundEntity);)
						if(m_PetrolCanGlugSound)
						{
							m_PetrolCanGlugSound->FindAndSetVariableValue(ATSTRINGHASH("PetrolLevel", 0xE48B4038),petrolLevel);
							m_PetrolCanGlugSound->PrepareAndPlay();
						}
					}
					if(m_PetrolCanPouringSound)
					{
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(sm_PetrolCanSounds.GetNameHash(), ATSTRINGHASH("POURING", 0x76F93140), &initParams, m_PetrolCanPouringSound, m_ParentOwningEntity, eNoGlobalSoundEntity,ReplaySound::CONTEXT_INVALID,true);)
					}
					if(m_PetrolCanGlugSound)
					{
						REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(sm_PetrolCanSounds.GetNameHash(), ATSTRINGHASH("GLUG", 0x71116457), &initParams, m_PetrolCanGlugSound, m_ParentOwningEntity, eNoGlobalSoundEntity,ReplaySound::CONTEXT_INVALID,true);)
						m_PetrolCanGlugSound->FindAndSetVariableValue(ATSTRINGHASH("PetrolLevel", 0xE48B4038),petrolLevel);
					}
				}
				else
				{
					if(m_PetrolCanPouringSound)
					{
						m_PetrolCanPouringSound->StopAndForget();
					}
					if(m_PetrolCanGlugSound)
					{
						m_PetrolCanGlugSound->StopAndForget();
					}
				}
				if(m_ParentOwningEntity->IsLocalPlayer())
				{
					CPad *pPad = CControlMgr::GetPlayerPad();
					if(!m_ParentOwningEntity->GetMotionData()->GetIsStill() && pPad && pPad->GetLeftStickY() == 0 && m_HasToPlayPetrolCanStop)
					{
						m_HasToPlayPetrolCanStop = false;
						initParams.SetVariableValue(ATSTRINGHASH("PetrolLevel", 0xE48B4038),petrolLevel);
						if(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() <= MOVEBLENDRATIO_WALK ) 
						{
							m_Parent->CreateAndPlaySound(sm_PetrolCanSounds.Find(ATSTRINGHASH("STOP_WALK_MOVE", 0x4D10F46)),&initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PetrolCanSounds.GetNameHash(),ATSTRINGHASH("STOP_WALK_MOVE", 0x4D10F46), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
						}
						else if(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() <= MOVEBLENDRATIO_RUN ) 
						{
							m_Parent->CreateAndPlaySound(sm_PetrolCanSounds.Find(ATSTRINGHASH("STOP_RUN_MOVE", 0x8A3C9CFC)),&initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PetrolCanSounds.GetNameHash(),ATSTRINGHASH("STOP_RUN_MOVE", 0x8A3C9CFC), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
						}
						else
						{
							m_Parent->CreateAndPlaySound(sm_PetrolCanSounds.Find(ATSTRINGHASH("STOP_SPRINT_MOVE", 0xF17BD183)),&initParams);
							REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PetrolCanSounds.GetNameHash(),ATSTRINGHASH("STOP_SPRINT_MOVE", 0xF17BD183), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
						}
					}
					else if(pPad && pPad->GetLeftStickY() != -0)
					{
						m_HasToPlayPetrolCanStop = true;
					}
				}
			}
		}
	}
	if(shouldStopSounds)
	{
		if(m_PetrolCanPouringSound)
		{
			m_PetrolCanPouringSound->StopAndForget();
		}
		if(m_PetrolCanGlugSound)
		{
			m_PetrolCanGlugSound->StopAndForget();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayPetrolCanEvent(audFootstepEvent event,f32 petrolLevel)
{
	audSoundInitParams initParams;
	initParams.UpdateEntity = true;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
	initParams.SetVariableValue(ATSTRINGHASH("PetrolLevel",0xE48B4038), petrolLevel);

	switch(event)
	{
	case AUD_FOOTSTEP_WALK_L: 
	case AUD_FOOTSTEP_WALK_R:
	case AUD_FOOTSTEP_SOFT_L:
	case AUD_FOOTSTEP_SOFT_R:
		m_Parent->CreateAndPlaySound(sm_PetrolCanSounds.Find(ATSTRINGHASH("WALK_MOVE", 0xC31C9401)),&initParams);
		break;
	case AUD_FOOTSTEP_RUN_L:
	case AUD_FOOTSTEP_RUN_R:
		m_Parent->CreateAndPlaySound(sm_PetrolCanSounds.Find(ATSTRINGHASH("RUN_MOVE", 0xD59BD15D)),&initParams);
		break;
	case AUD_FOOTSTEP_SPRINT_L:
	case AUD_FOOTSTEP_SPRINT_R:
	case AUD_FOOTSTEP_JUMP_LAND_L:
	case AUD_FOOTSTEP_JUMP_LAND_R:
		m_Parent->CreateAndPlaySound(sm_PetrolCanSounds.Find(ATSTRINGHASH("SPRINT_MOVE", 0xFD4428CA)),&initParams);
		break;
	default: 
		m_Parent->CreateAndPlaySound(sm_PetrolCanSounds.Find(ATSTRINGHASH("WALK_MOVE", 0xC31C9401)),&initParams);
		break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayMolotovEvent(audFootstepEvent event)
{
	audSoundInitParams initParams;
	initParams.UpdateEntity = true;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();

	switch(event)
	{
	case AUD_FOOTSTEP_WALK_L: 
	case AUD_FOOTSTEP_WALK_R:
	case AUD_FOOTSTEP_SOFT_L:
	case AUD_FOOTSTEP_SOFT_R:
		m_Parent->CreateAndPlaySound(sm_MolotovSounds.Find(ATSTRINGHASH("WALK_MOVE", 0xC31C9401)),&initParams);
		break;
	case AUD_FOOTSTEP_RUN_L:
	case AUD_FOOTSTEP_RUN_R:
		m_Parent->CreateAndPlaySound(sm_MolotovSounds.Find(ATSTRINGHASH("RUN_MOVE", 0xD59BD15D)),&initParams);
		break;
	case AUD_FOOTSTEP_SPRINT_L:
	case AUD_FOOTSTEP_SPRINT_R:
	case AUD_FOOTSTEP_JUMP_LAND_L:
	case AUD_FOOTSTEP_JUMP_LAND_R:
		m_Parent->CreateAndPlaySound(sm_MolotovSounds.Find(ATSTRINGHASH("SPRINT_MOVE", 0xFD4428CA)),&initParams);
		break;
	default: 
		m_Parent->CreateAndPlaySound(sm_MolotovSounds.Find(ATSTRINGHASH("WALK_MOVE", 0xC31C9401)),&initParams);
		break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayScriptSweetenerEvent(audFootstepEvent event)
{
	audSoundInitParams initParams;
	initParams.UpdateEntity = true;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();

	switch(event)
	{
	case AUD_FOOTSTEP_WALK_L: 
	case AUD_FOOTSTEP_WALK_R:
	case AUD_FOOTSTEP_SOFT_L:
	case AUD_FOOTSTEP_SOFT_R:
		m_Parent->CreateAndPlaySound(m_ScriptFootstepSweetenersSoundSet.Find(ATSTRINGHASH("WALK", 0x83504C9C)),&initParams);
		break;
	case AUD_FOOTSTEP_RUN_L:
	case AUD_FOOTSTEP_RUN_R:
		m_Parent->CreateAndPlaySound(m_ScriptFootstepSweetenersSoundSet.Find(ATSTRINGHASH("RUN", 0x1109B569)),&initParams);
		break;
	case AUD_FOOTSTEP_SPRINT_L:
	case AUD_FOOTSTEP_SPRINT_R:
		m_Parent->CreateAndPlaySound(m_ScriptFootstepSweetenersSoundSet.Find(ATSTRINGHASH("SPRINT", 0xBC29E48)),&initParams);
		break;
	case AUD_FOOTSTEP_JUMP_LAND_L:
	case AUD_FOOTSTEP_JUMP_LAND_R:
		m_Parent->CreateAndPlaySound(m_ScriptFootstepSweetenersSoundSet.Find(ATSTRINGHASH("JUMP_LAND", 0x6B2460BA)),&initParams);
		break;
	default: 
		m_Parent->CreateAndPlaySound(m_ScriptFootstepSweetenersSoundSet.Find(ATSTRINGHASH("WALK", 0x83504C9C)),&initParams);
		break;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayClothEvent(audFootstepEvent event,const f32 angularVelocity)
{
	if(naVerifyf(m_UpperClothSettings,"Failed to get cloth audio settings : Id ->%d", m_UpperClothing.GetHash()))
	{
		PlayClothEvent(event,m_UpperClothSettings,angularVelocity);
	}
	if(naVerifyf(m_LowerClothSettings,"Failed to get cloth audio settings : Id ->%d", m_LowerClothing.GetHash()))
	{
		PlayClothEvent(event,m_LowerClothSettings,angularVelocity);
	}
	for(u8 i = 0; i < MAX_EXTRA_CLOTHING; i ++)
	{
		if(m_ExtraClothingSettings[i])
		{
			PlayClothEvent(event,m_ExtraClothingSettings[i],angularVelocity);
		}
	}
	if(m_IsWearingElfHat)
	{
		audSoundInitParams initParams; 
		initParams.UpdateEntity = true;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();

		m_Parent->CreateAndPlaySound(ATSTRINGHASH("ELF_HAT", 0xB43C5EB),&initParams);
	}
	m_LastMoveBlendRatio = m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY();
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayClothEvent(audFootstepEvent event,const ClothAudioSettings* settings,const f32 angularVelocity)
{
	f32 linearVelocity = m_ParentOwningEntity->GetVelocity().Mag();
	f32 angularAcc = fabs((angularVelocity - m_LastPedAngVel) / fwTimer::GetTimeStep());
	// The idea is to use:
	// state -> to choose the sound ref from the game object.
	// linear velocity and acceleration -> preDelays, if to play the start and stop sound and volumes.
	// angular velocity and acceleration -> to modulate the change direction sound and trigger it or not.
	bool shouldTriggerWeaponRattle = false;
	audSoundInitParams initParams;
	initParams.UpdateEntity = true;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
	audSoundSet clothSoundSet;
	switch(event)
	{
	case AUD_FOOTSTEP_WALK_L: 
	case AUD_FOOTSTEP_WALK_R:
	case AUD_FOOTSTEP_SOFT_L: 
	case AUD_FOOTSTEP_SOFT_R:
		if(settings->WalkSound)
		{
			clothSoundSet.Init(settings->WalkSound);
		}
		break;
	case AUD_FOOTSTEP_RUN_L:
	case AUD_FOOTSTEP_RUN_R:
		if(settings->RunSound)
		{
			clothSoundSet.Init(settings->RunSound);
		}else
		{
			clothSoundSet.Init(settings->WalkSound);
		}
		shouldTriggerWeaponRattle = true;
		break;
	case AUD_FOOTSTEP_SPRINT_L:
	case AUD_FOOTSTEP_SPRINT_R:
	case AUD_FOOTSTEP_JUMP_LAND_L:
	case AUD_FOOTSTEP_JUMP_LAND_R:
		if(settings->SprintSound)
		{
			clothSoundSet.Init(settings->SprintSound);
		}
		else if(m_UpperClothSettings->RunSound)
		{
			clothSoundSet.Init(settings->RunSound);
		}
		else
		{
			clothSoundSet.Init(settings->WalkSound);
		}
		shouldTriggerWeaponRattle = true;
		break;
	default: 
		m_LastMoveBlendRatio = 0.f;
		break;
	}
	f32 foleyIntensity = m_ParentOwningEntity->IsLocalPlayer() ? sm_PlayerFoleyIntensity: 0.f;

	audSound* baseUpperSound = NULL;
	audSound* moveStartUpper = NULL;
	audSound* moveStopUpper = NULL;
	audSound* heavyUpperSound = NULL;
	audSound* bumpUpperSound = NULL;

	if(event == AUD_FOOTSTEP_JUMP_LAND_L || event ==  AUD_FOOTSTEP_JUMP_LAND_R)
	{
		m_Parent->CreateAndPlaySound(settings->JumpLandSound,&initParams);
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		m_Parent->CreateAndPlaySound(ATSTRINGHASH("WEAPON_RATTLE_LAND_SOUND", 0x40140F53), &initParams);
		return;
	}
	m_Parent->CreateSound_LocalReference(clothSoundSet.Find(ATSTRINGHASH("BaseSound", 0x14B6A36D)),&baseUpperSound,&initParams);
	m_Parent->CreateSound_LocalReference(clothSoundSet.Find(ATSTRINGHASH("HeavySound", 0x43D2AF25)),&heavyUpperSound,&initParams);
	m_Parent->CreateSound_LocalReference(clothSoundSet.Find(ATSTRINGHASH("BumpSound", 0x77CD4C76)),&bumpUpperSound,&initParams);

	// Move start
	if(m_LastMoveBlendRatio <= MBR_WALK_BOUNDARY && m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() >= MBR_WALK_BOUNDARY)
	{
		m_Parent->CreateSound_LocalReference(clothSoundSet.Find(ATSTRINGHASH("PlayMoveStart", 0x12A40937)),&moveStartUpper,&initParams);
		if(moveStartUpper)
		{
			moveStartUpper->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
			moveStartUpper->PrepareAndPlay();
		}
	}
	// Move stop
	if(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() <=(MBR_WALK_BOUNDARY + sm_MoveBlendStopThreshold ))
	{
		m_Parent->CreateSound_LocalReference(clothSoundSet.Find(ATSTRINGHASH("PlayMoveStop", 0x5D8626DA)),&moveStopUpper,&initParams);
		if(moveStopUpper)
		{
			moveStopUpper->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
			moveStopUpper->PrepareAndPlay();
		}
	}
	//BaseSound
	if(baseUpperSound)
	{
		baseUpperSound->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
		baseUpperSound->SetRequestedVolume(sm_AngularAccToVolumeCurve.CalculateValue(angularAcc));
		baseUpperSound->PrepareAndPlay();
	}
	//HeavySound
	if(heavyUpperSound)
	{
		heavyUpperSound->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
		heavyUpperSound->SetRequestedVolume(sm_LinearVelToVolumeCurve.CalculateValue(linearVelocity));
		heavyUpperSound->PrepareAndPlay();
	}
	//BumpSound
	if(bumpUpperSound)
	{
		bumpUpperSound->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
		bumpUpperSound->SetRequestedVolume(sm_LinearVelToVolumeCurve.CalculateValue(linearVelocity));
		bumpUpperSound->PrepareAndPlay();
	}
	// Do pockets
	if (event == AUD_FOOTSTEP_WALK_L || event == AUD_FOOTSTEP_LADDER_L_UP || event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_RUN_L || event == AUD_FOOTSTEP_SPRINT_L)
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);

		if (m_Pockets & 1)
		{
			// Play keys!
			m_Parent->CreateAndPlaySound(sm_PocketsSoundSet.Find(ATSTRINGHASH("KEYS", 0x2C99B768)), &initParams);
		}
		else if ((m_Pockets & 4) && (m_ParentOwningEntity->GetMoneyCarried() >= 10))
		{
			// Play money!
			m_Parent->CreateAndPlaySound(sm_PocketsSoundSet.Find(ATSTRINGHASH("MONEY", 0xA49D0765)), &initParams);
		}
	}
	else if (event == AUD_FOOTSTEP_WALK_R || event == AUD_FOOTSTEP_LADDER_R_UP || event == AUD_FOOTSTEP_JUMP_LAND_R || event == AUD_FOOTSTEP_RUN_R || event == AUD_FOOTSTEP_SPRINT_R)
	{
		audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);

		if (m_Pockets & 2)
		{
			// Play keys!
			m_Parent->CreateAndPlaySound(sm_PocketsSoundSet.Find(ATSTRINGHASH("KEYS", 0x2C99B768)), &initParams);
		}
		else if ((m_Pockets & 8) && (m_ParentOwningEntity->GetMoneyCarried() >= 10))
		{
			// Play money!
			m_Parent->CreateAndPlaySound(sm_PocketsSoundSet.Find(ATSTRINGHASH("MONEY", 0xA49D0765)), &initParams);
		}
	}
	if(shouldTriggerWeaponRattle)
	{
		audSoundInitParams initParams;

		bool bFirstPerson = audNorthAudioEngine::IsAFirstPersonCameraActive(CGameWorld::FindLocalPlayer(), false, false, true, true);
		if(bFirstPerson)
		{
			initParams.Volume = g_WeaponRattleReductionInFirstPerson;
			if(m_ParentOwningEntity)
			{
				const CWeapon *weapon = m_ParentOwningEntity->GetWeaponManager() ? m_ParentOwningEntity->GetWeaponManager()->GetEquippedWeapon() : NULL;
				if(weapon)
				{
					u32 weaponHash = weapon->GetWeaponHash();
					if( weaponHash == ATSTRINGHASH("WEAPON_MINIGUN", 0x42BF8A85) || weaponHash == ATSTRINGHASH("WEAPON_MG", 0x9D07F764) ||
					    weaponHash == ATSTRINGHASH("WEAPON_COMBATMG", 0x7FD62962) || weaponHash == ATSTRINGHASH("WEAPON_RPG", 0xB1CA77B1) ||
						weaponHash == ATSTRINGHASH("WEAPON_HEAVYSNIPER", 0xC472FE2) || weaponHash == ATSTRINGHASH("WEAPON_FIREWORK", 0x7F7497E5))
					{
						initParams.Volume = g_BigWeaponRattleReductionInFirstPerson;
					}
				}
			}
		}
		initParams.TrackEntityPosition = true;
		initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		m_Parent->CreateAndPlaySound(ATSTRINGHASH("WEAPON_RATTLE_SOUND", 0x54D20C1), &initParams);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayWaterEvent(audFootstepEvent event)
{
	audSoundInitParams initParams;
	initParams.UpdateEntity = true;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
	if(  m_ParentOwningEntity && m_ParentOwningEntity->GetPedType() != PEDTYPE_ANIMAL)
	{
		audFootWaterInfo footWaterInfo = m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)];
		switch(footWaterInfo)
		{
		case audFootUnderWater:
			if(event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_JUMP_LAND_R)
			{
				m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find(ATSTRINGHASH("Splash_Land", 0x81DD70C)),&initParams);
			}
			else
			{
				m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find(ATSTRINGHASH("Splash", 0x43C9333C)),&initParams);
			}
			break;
		case audLegsUnderWater:
			if(event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_JUMP_LAND_R)
			{
				m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find(ATSTRINGHASH("Legs_Land", 0x91A99D9)),&initParams);
			}
			else
			{
				m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find(ATSTRINGHASH("LegsInsideWater", 0x583DFF3C)),&initParams);
			}
			break;
		case audBodyUnderWater:
			m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find(ATSTRINGHASH("BodyInsideWater", 0xAAAE09BB)),&initParams);
			break;
		default :
			//naWarningf("Triggering footstep water event when out of water, [info %d]",footWaterInfo);
			break;
		}
	}
	else if(  m_ParentOwningEntity )
	{
		CollisionMaterialSettings * waterMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_WATER", 0xD1CB2165));
		if(waterMaterial)
		{
			AnimalFootstepSettings *footstepSettings = GetAnimalFootstepSettings(true, waterMaterial->AnimalReference);
			if(footstepSettings)
			{
				u32 soundRef = GetAnimalFootstepSoundRef(footstepSettings, event);
				m_Parent->CreateAndPlaySound(soundRef,&initParams);
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayFoliageEvent(foliageEvent fEvent,const f32 angularVelocity)
{
	audFootstepEvent event = fEvent.stepType;

	FoliageSettings* foliageSettings = audNorthAudioEngine::GetObject<FoliageSettings>(fEvent.typeNameHash);
	if(!foliageSettings)
	{
		foliageSettings = audNorthAudioEngine::GetObject<FoliageSettings>(ATSTRINGHASH("DEFAULT_FOLIAGE", 0x9706FA73));
	}

	f32 linearVelocity = m_ParentOwningEntity->GetVelocity().Mag();
	//f32 linearAcc  = fabs((linearVelocity - m_LastPedVel) / fwTimer::GetTimeStep());
	f32 angularAcc = fabs((angularVelocity - m_LastPedAngVel) / fwTimer::GetTimeStep());
	// The idea is to use:
	// state -> to choose the sound ref from the game object.
	// linear velocity and acceleration -> preDelays, if to play the start and stop sound and volumes.
	// angular velocity and acceleration -> to modulate the change direction sound and trigger it or not.
	//const UpperClothingAudioSettings *upperClothSettings = GetUpperBodyClothSounds();
	//if(naVerifyf(lowerClothSettings,"Failed to get cloth audio settings : Id ->" + m_LowerClothing))
	//{
	audSoundInitParams initParams;
	initParams.UpdateEntity = true;
	initParams.TrackEntityPosition = true; 
	initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
	audSoundSet bushSoundSet;
	switch(event)
	{
	case AUD_FOOTSTEP_WALK_L: 
	case AUD_FOOTSTEP_WALK_R:
	case AUD_FOOTSTEP_SOFT_L:
	case AUD_FOOTSTEP_SOFT_R:
		bushSoundSet.Init(foliageSettings->Walk);
		break;
	case AUD_FOOTSTEP_RUN_L:
	case AUD_FOOTSTEP_RUN_R:
		bushSoundSet.Init(foliageSettings->Run);
		break;
	case AUD_FOOTSTEP_SPRINT_L:
	case AUD_FOOTSTEP_SPRINT_R:
	case AUD_FOOTSTEP_JUMP_LAND_L:
	case AUD_FOOTSTEP_JUMP_LAND_R:
		bushSoundSet.Init(foliageSettings->Sprint);
		break;
	default: 
		m_LastMoveBlendRatio = 0.f;
		break;
	}
	f32 foleyIntensity = m_ParentOwningEntity->IsLocalPlayer() ? sm_PlayerFoleyIntensity: 0.f;
	audSound* baseSound = NULL;
	audSound* moveStart = NULL;
	audSound* moveStop = NULL;
	audSound* creakSound = NULL;

	m_Parent->CreateSound_LocalReference(bushSoundSet.Find(ATSTRINGHASH("BaseSound", 0x14B6A36D)),&baseSound,&initParams);
	// Move start
	if(m_LastMoveBlendRatio <= MBR_WALK_BOUNDARY && m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() >= MBR_WALK_BOUNDARY)
	{
		m_Parent->CreateSound_LocalReference(bushSoundSet.Find(ATSTRINGHASH("MoveStart", 0xE09AE7AC)),&moveStart,&initParams);
		if(moveStart)
		{
			moveStart->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
			moveStart->PrepareAndPlay();
		}
	}
	// Move stop
	if(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY() <=(MBR_WALK_BOUNDARY + sm_MoveBlendStopThreshold ))
	{
		m_Parent->CreateSound_LocalReference(bushSoundSet.Find(ATSTRINGHASH("MoveStop", 0xFDFD5A12)),&moveStop,&initParams);
		if(moveStop)
		{
			moveStop->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
			moveStop->PrepareAndPlay();
		}
	}
	//BaseSound
	if(baseSound)
	{
		baseSound->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
		baseSound->SetRequestedVolume(sm_AngularAccToVolumeCurve.CalculateValue(angularAcc));
		baseSound->PrepareAndPlay();
	}
	//BumpSound
	if(audEngineUtil::ResolveProbability(sm_BushCreakiness))
	{
		m_Parent->CreateSound_LocalReference(bushSoundSet.Find(ATSTRINGHASH("CreakSound", 0xAA1C6E5)),&creakSound,&initParams);
		if(creakSound)
		{
			creakSound->FindAndSetVariableValue(ATSTRINGHASH("foleyIntensity", 0xFC43960A),foleyIntensity);
			creakSound->SetRequestedVolume(sm_LinearVelToCreakVolumeCurve.CalculateValue(linearVelocity));
			creakSound->PrepareAndPlay();
		}
	}
	//}
	m_LastMoveBlendRatio = m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY();
}
//-------------------------------------------------------------------------------------------------------------------
u8 audPedFootStepAudio::GetFootstepTuningValues(u32 modeHash)
{
	if(m_FootstepsTuningValues)
	{
		for (u32 i = 0; i < m_FootstepsTuningValues->numModes; i++)
		{
			if( m_FootstepsTuningValues->Modes[i].Name == modeHash )
			{
				return (u8)i;
			}
		}
	}
	return 0;
}
#if __BANK 
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayDelayDebugFootstepEvent(const Vector3 &pos,phMaterialMgr::Id materialId,const u16 hitComponent,CEntity* hitEntity,CollisionMaterialSettings* material)
{
	sm_Position = pos;
	sm_MaterialId = materialId;
	sm_HitComponent = hitComponent;
	sm_HitEntity = hitEntity;
	sm_Material = material;
	sm_TimeToPlayDebugStep = fwTimer::GetTimeInMilliseconds() + 500;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayDebugFootstepEvent(const Vector3 &pos,phMaterialMgr::Id materialId,const u16 hitComponent,const CEntity* hitEntity,const CollisionMaterialSettings* material)
{
	if(fwTimer::GetTimeInMilliseconds() < sm_LastTimeDebugStepWasPlayed + 100)
	{
		return;
	}
	CPed *player = CGameWorld::FindLocalPlayer();
	ShoeAudioSettings * shoeSettings =  audNorthAudioEngine::GetObject<ShoeAudioSettings>(ATSTRINGHASH("SHOE_DRESS_SHOES",0xBEBB4F46));
	if (player && shoeSettings)
	 {
		// Play Custom impact
		audSoundSet materialSoundSetRef;
		materialSoundSetRef.Init(material->FootstepCustomImpactSound);
		audMetadataRef soundRef = g_NullSoundRef;
		soundRef = materialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
		if(soundRef != g_NullSoundRef )
		{
			audSound * footstepCustom = NULL;
			audSoundInitParams initParams;
			//initParams.EnvironmentGroup = player->GetPedAudioEntity()->GetEnvironmentGroup(true);
			//f32 tunedVolume = player.->Modes[m_ModeIndex].CustomImpact.VolumeOffset;
			//u32 tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].CustomImpact.LPFCutoff;
			//u32 tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].CustomImpact.AttackTime;
			//// Modes tuning values.
			//if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
			//{
			//	// Volume : 
			//	f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CustomImpact.VolumeOffset);
			//	f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].CustomImpact.VolumeOffset);
			//	tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
			//	// LPF : 
			//	f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CustomImpact.LPFCutoff);
			//	f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].CustomImpact.LPFCutoff);
			//	tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,goodStealthLPFCutoff,badStealthLPFCutoff));
			//	// Attack : 
			//	u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CustomImpact.AttackTime;
			//	u32 badStealthAttackTime = m_FootstepsTuningValues->Modes[m_BadStealthIndex].CustomImpact.AttackTime;
			//	tunedAttackTime = (u32)Lerp(m_PlayerStealth,(f32)badStealthAttackTime,(f32)goodStealthAttackTime);
			//}
			//initParams.Volume += tunedVolume;
			//initParams.Volume += jumpLandVolOffset;
			//initParams.LPFCutoff = Min (initParams.LPFCutoff, tunedLPFCutoff);
			//initParams.AttackTime = Max(initParams.AttackTime, tunedAttackTime); 
			//// Footstep volume roll of over time. 
			//initParams.Volume += m_FootVolOffsetOverTime;
			initParams.Position = pos;
			//initParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
			//initParams.u32ClientVar = (u32)AUD_FOOTSTEP_WALK_L;
			//initParams.UpdateEntity = sm_MaterialCustomTrack;
			//initParams.VolumeCurveScale = npcRunRollOffScale;

			player->GetPedAudioEntity()->CreateSound_LocalReference(soundRef,&footstepCustom, &initParams);
			f32 shoeHardness = BANK_ONLY(sm_OverrideShoeHardness?sm_OverridedShoeHardness:)shoeSettings->ShoeHardness;
			if(footstepCustom)
			{
				f32 footSpeed = BANK_ONLY(sm_OverrideFootSpeed ? sm_OverridedFootSpeed:)0.f;
				footstepCustom->FindAndSetVariableValue(ATSTRINGHASH("Speed", 0xF997622B),footSpeed);
				footstepCustom->FindAndSetVariableValue(ATSTRINGHASH("PlayToe", 0x92C69D81),false);
				footstepCustom->FindAndSetVariableValue(ATSTRINGHASH("ShoeHardness", 0x60A65D5E),shoeHardness);
				footstepCustom->PrepareAndPlay();
			}
		}
		//f32 clumsiness = GetPlayerClumsiness();
		//if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep && audEngineUtil::ResolveProbability(sm_PlayerClumsinessToMatSweetenerProb.CalculateValue(clumsiness)))
		//{																												 
		//	audSoundInitParams initParams;
		//	initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		//	if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
		//	{
		//		initParams.Volume -= sm_FeetInsideWaterVolDump;
		//	}
		//	// Footstep volume roll of over time. 
		//	initParams.Volume += m_FootVolOffsetOverTime;
		//	initParams.Volume += sm_PlayerClumsinessToMatSweetenerVol.CalculateValue(clumsiness);
		//	initParams.Position = pos;
		//	initParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
		//	initParams.u32ClientVar = (u32)event;
		//	initParams.UpdateEntity = sm_MaterialCustomTrack;

		//	m_Parent->CreateAndPlaySound(m_CurrentMaterialSettings->StealthSweetener, &initParams);
		//}

		// Play Material Impact
		if(hitEntity && hitEntity->GetCurrentPhysicsInst() && hitEntity->GetCurrentPhysicsInst()->IsInLevel())
		{
			WorldProbe::CShapeTestFixedResults<> tempResults;
			tempResults[0].SetHitMaterialId(materialId);
			tempResults[0].SetHitComponent(hitComponent);
			tempResults[0].SetHitInst(
				hitEntity->GetCurrentPhysicsInst()->GetLevelIndex(), 
#if LEVELNEW_GENERATION_IDS
				PHLEVEL->GetGenerationID(hitEntity->GetCurrentPhysicsInst()->GetLevelIndex())
#else
				0
#endif // LEVELNEW_GENERATION_IDS
				);
			tempResults[0].SetHitPosition(pos);
			f32 impulseMagnitude = audEngineUtil::GetRandomNumberInRange(sm_MinImpulseMag,sm_MaxImpulseMag);
			impulseMagnitude *= BANK_ONLY(sm_DontApplyScales ? 1.f : ) (material->FootstepImpactScaling / 100.f);
			// Modes tuning values.
			//f32 materialImpactImpulseScale = m_FootstepsTuningValues->Modes[m_ModeIndex].MaterialImpactImpulseScale;
			//if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
			//{
			//	// Volume : 
			//	f32 goodStealthScale = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].MaterialImpactImpulseScale;
			//	f32 badStealthScale = m_FootstepsTuningValues->Modes[m_BadStealthIndex].MaterialImpactImpulseScale;
			//	materialImpactImpulseScale = Lerp(m_PlayerStealth,badStealthScale,goodStealthScale);
			//}
			//impulseMagnitude *= materialImpactImpulseScale;
			//// Footstep volume roll of over time. 
			//impulseMagnitude *= m_FootstepImpsMagOffsetOverTime;
			//if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
			//{		
			//	impulseMagnitude = 0.f;
			//}

			g_CollisionAudioEntity.ReportMeleeCollision(&tempResults[0], impulseMagnitude, player, false, true);
		}

		// Play Shoe 
		u32 shoeSoundRef = shoeSettings->Walk;
		//u32 dirtySoundRef = shoeSettings->DirtyWalk;
		//u32 creakySoundRef = shoeSettings->CreakyWalk;
		//u32 glassySoundRef = shoeSettings->GlassyWalk;
		//u32 wetSoundRef = shoeSettings->WetWalk;
		//f32 volOffset = sm_WalkFootstepVolOffset;

		audSound * shoeSound = NULL;
		audSoundInitParams shoeInitParams;  
		shoeInitParams.EnvironmentGroup = player->GetPedAudioEntity()->GetEnvironmentGroup(true);
		shoeInitParams.UpdateEntity = true;
		// Add volume offsets and scales
		f32 impactVolume = audDriverUtil::ComputeDbVolumeFromLinear(BANK_ONLY(sm_DontApplyScales ? 1.f : )(material->FootstepScaling / 100.f));
		//f32 volOffsetNPC = (m_ParentOwningEntity->IsLocalPlayer()?0.f:sm_NPCFootstepVolOffset);
		//f32 sharedVolume = impactVolume + movementVolOffset + volOffsetNPC;
		//// Feet under water
		//if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
		//{
		//	sharedVolume -= sm_FeetInsideWaterVolDump;
		//}
		//shoeInitParams.Volume = sharedVolume;
		shoeInitParams.Volume = impactVolume;
		// Footsteps roll off over time. 
		//shoeInitParams.Volume += m_FootVolOffsetOverTime;
		shoeInitParams.Position = pos;
		//shoeInitParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
		//shoeInitParams.u32ClientVar = (u32)AUD_FOOTSTEP_WALK_L;
		//// Mode tuning values
		//f32 tunedVolume = m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.VolumeOffset;
		//u32 tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.LPFCutoff;
		//u32 tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.AttackTime;
		//if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
		//{
		//	// Volume : 
		//	f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].Shoe.VolumeOffset);
		//	f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].Shoe.VolumeOffset);
		//	tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
		//	// LPF : 
		//	f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].Shoe.LPFCutoff);
		//	f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].Shoe.LPFCutoff);
		//	tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,goodStealthLPFCutoff,badStealthLPFCutoff));
		//	// Attack : 
		//	u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].Shoe.AttackTime;
		//	u32 badStealthAttackTime = m_FootstepsTuningValues->Modes[m_BadStealthIndex].Shoe.AttackTime;
		//	tunedAttackTime = (u32)Lerp(m_PlayerStealth,(f32)badStealthAttackTime,(f32)goodStealthAttackTime);
		//}
		//shoeInitParams.Volume +=	tunedVolume;
		//shoeInitParams.LPFCutoff =	Min (shoeInitParams.LPFCutoff, tunedLPFCutoff);
		//shoeInitParams.AttackTime = Max(shoeInitParams.AttackTime, tunedAttackTime); 
		//shoeInitParams.VolumeCurveScale = npcVolCurveScale;
		////Create and play sound.	
		f32 toeDelayOffset = 0.f;
		/*if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
		{
			toeDelayOffset = sm_PlayerStealthToToeDelayOffset.CalculateValue(m_PlayerStealth) / 1000.f;
			if(m_ClumsyStep)
			{
				toeDelayOffset = sm_ClumsyStepToeDelayOffset/1000.f;
			}
		}
		if (event == AUD_FOOTSTEP_SCUFF_L || event == AUD_FOOTSTEP_SCUFF_R)
		{
			shoeInitParams.Predelay =	(s32)(toeDelayOffset * 1000.f);
		}*/
		player->GetPedAudioEntity()->CreateSound_LocalReference(shoeSoundRef,&shoeSound, &shoeInitParams);
		f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)material->FootstepMaterialHardness;
		if(shoeSound)
		{
			shoeSound->FindAndSetVariableValue(ATSTRINGHASH("TOE_DELAY", 0x41504027),toeDelayOffset);
			shoeSound->FindAndSetVariableValue(ATSTRINGHASH("MaterialHardness", 0xEDD0ED05),materialHardness);
			shoeSound->PrepareAndPlay();
		}
//		// Play all layers
//		PlayDirtLayer(dirtySoundRef,sharedVolume,npcVolCurveScale,playToe,event,pos);
//		PlayCreakLayer(creakySoundRef,sharedVolume,npcVolCurveScale,event,pos);
//		PlayGlassLayer(glassySoundRef,sharedVolume,npcVolCurveScale,playToe,event,pos);
//		PlayWetLayer(wetSoundRef,sharedVolume,npcVolCurveScale,playToe,event,pos);
//
//#if __BANK
//		if(g_bDrawFootsteps)
//		{
//			Vector3 headPos;
//			m_ParentOwningEntity->GetBonePosition(headPos,BONETAG_HEAD);
//			DebugAudioFootstep(GetCurrentMaterialSettings(),impactVolume,shoeInitParams.Volume,m_ShoeType,headPos);
//		}
//#endif
		sm_LastTimeDebugStepWasPlayed = fwTimer::GetTimeInMilliseconds();
	}
}
#endif 
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayFootstepEvent(audFootstepEvent event, bool /*playWhenNotStanding*/)
{
#if __DEV
	if(g_BreakpointDebugPedFootstep && g_AudioDebugEntity == m_ParentOwningEntity)
	{
		__debugbreak();
	}
#endif

#if __BANK
	if(g_DisableFootstepSounds)
	{
		return;
	}
	if(sm_ForceSoftSteps)
	{
		if((event == AUD_FOOTSTEP_WALK_L) || (event == AUD_FOOTSTEP_JUMP_LAND_L)
			|| (event == AUD_FOOTSTEP_RUN_L) || (event == AUD_FOOTSTEP_SPRINT_L))
		{
			event = AUD_FOOTSTEP_SOFT_L;
		}
		else if ((event == AUD_FOOTSTEP_WALK_R) || (event == AUD_FOOTSTEP_JUMP_LAND_R)
			|| (event == AUD_FOOTSTEP_RUN_R) || (event == AUD_FOOTSTEP_SPRINT_R))
		{
			event = AUD_FOOTSTEP_SOFT_R;
		}
	}
#endif

	if(m_ParentOwningEntity->m_nDEflags.bFrozenByInterior)
	{
		return;
	}

	Vec3V pos;
	m_ParentOwningEntity->GetFootstepHelper().GetPositionForPedSound(event, pos);
	// For the footstep custom sound and dirty/creaky sounds based on the foot speed and movement 
	// calculate which of the three part of the sound we should play (heel, foot, toe) and their intensity.
	u8 playToe = 1;
	if(event < AUD_FOOTSTEP_LADDER_L || event > AUD_FOOTSTEP_LADDER_HAND_R)
	{
		if(m_ParentOwningEntity->IsLocalPlayer())
		{
			CPad *pPad = CControlMgr::GetPlayerPad();
			if (!m_ParentOwningEntity->GetMotionData()->GetIsStill() && pPad && pPad->GetLeftStickY() == 0)
			{
				playToe = 0; 
			}
		}
		// Get the footstep tuning values
		u32 modeHash = m_ModeHash;
		m_ClumsyStep = false;
		if (event != AUD_FOOTSTEP_SCUFF_L && event != AUD_FOOTSTEP_SCUFF_R)
		{
			if (m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
			{
				if(audEngineUtil::ResolveProbability(sm_PlayerStealthToClumsiness.CalculateValue(m_PlayerStealth)))
				{
					modeHash = ATSTRINGHASH("DEFAULT", 0xE4DF46D5);
					m_ClumsyStep = true;
				}
			}
			else if(event == AUD_FOOTSTEP_SOFT_L || event == AUD_FOOTSTEP_SOFT_R)
			{
				modeHash = ATSTRINGHASH("SOFT_STEPS", 0xCA490931);
			}
		}

		if( modeHash != m_ModeHash)
		{
			m_ModeIndex = GetFootstepTuningValues(modeHash);
			m_ModeHash = modeHash;
		}
		//DYNAMIC MIXING UPDATE
		if(sm_CrouchModeScene && m_ParentOwningEntity->IsLocalPlayer())
		{
			// DYNAMIC MIXER SCENES
			const u32 crouchModePatchName = g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::PlayerOnDLCHeist4Island) ? ATSTRINGHASH("CROUCH_MODE_IH_PATCH", 0xD06740EF) : ATSTRINGHASH("CROUCH_MODE_PATCH", 0xFE5C0FE1);
			audDynMixPatch* crouchModePatch = sm_CrouchModeScene->GetPatch(crouchModePatchName);
			f32 currentApply = crouchModePatch ? crouchModePatch->GetApplyFactor() : 0.f;
			// CHeck the footstep based mode.
			if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
			{
				currentApply += sm_DeltaApplyPerStep;
				currentApply = Clamp(currentApply,0.f,1.f);
			}
			else
			{
				currentApply -= sm_DeltaApplyPerStep;
				currentApply = Clamp(currentApply,0.f,1.f);
			}
			sm_CrouchModeScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8),currentApply);
			if(currentApply == 0)
			{
				sm_CrouchModeScene->Stop();
				sm_CrouchModeScene = NULL;
			}
			//else if(sm_StealthMixingMode == 2)
			//{
			//	sm_CrouchModeScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8),sm_PedVelocityToStealthSceneApply.CalculateValue(m_ParentOwningEntity->GetVelocity().Mag()));
			//	naDisplayf("PED VEL %f",m_ParentOwningEntity->GetVelocity().Mag());
			//}
		}
	}
	f32 surfaceDepthVolLin = sm_SurfaceDepthToVolDump.CalculateValue(m_ParentOwningEntity->GetDeepSurfaceInfo().GetDepth());
	f32 surfaceInterp = sm_SurfaceDepthToInterp.CalculateValue(m_ParentOwningEntity->GetDeepSurfaceInfo().GetDepth());
	f32 surfaceDepthVol = audDriverUtil::ComputeDbVolumeFromLinear(surfaceDepthVolLin);
	 
	if((event == AUD_FOOTSTEP_JUMP_LAND_R || event == AUD_FOOTSTEP_JUMP_LAND_L) 
		&& m_ParentOwningEntity->IsLocalPlayer()
		&& !m_ParentOwningEntity->GetIsInWater()
		&& m_ParentOwningEntity->GetSpeechAudioEntity() 
		&& m_ParentOwningEntity->GetFootstepHelper().GetTimeInTheAir() > g_TimeInAirForLandVocal)
	{
		audDamageStats damageStats;
		damageStats.IsFromAnim = true;
		damageStats.DamageReason = AUD_DAMAGE_REASON_MELEE_SMALL_GRUNT;
		m_ParentOwningEntity->GetSpeechAudioEntity()->InflictPain(damageStats);
	}
	
	// Play the material sound and shoe independently
	if(BANK_ONLY(!sm_OnlyPlayShoe && ) naVerifyf(GetCurrentMaterialSettings(), "No material settings on audPedaudioEntity during PlayFootstepevent"))
	{
		// override this if the ped is inside a vehicle; ie walking a motorbike
		if(m_ParentOwningEntity->GetVehiclePedInside())
		{
			if(m_ParentOwningEntity->GetVehiclePedInside()->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_CAR)
			{
				audCarAudioEntity* carEntity = static_cast<audCarAudioEntity*>(m_ParentOwningEntity->GetVehiclePedInside()->GetVehicleAudioEntity());
				if(carEntity)
				{
					CollisionMaterialSettings *matSettings = carEntity->GetCachedMaterialSettings();
					if(matSettings)
					{
						m_CurrentMaterialSettings = matSettings;
					}
				}
			}
		}
		BANK_ONLY(if(!sm_OnlyPlayMaterialImpact))
		{
			PlayFootStepCustomImpactSound(event,VEC3V_TO_VECTOR3(pos),playToe,surfaceInterp);
		}
		BANK_ONLY(if(!sm_OnlyPlayMaterialCustom))
		{
			if (event < AUD_FOOTSTEP_LADDER_L || event > AUD_FOOTSTEP_LADDER_HAND_R)
			{
				if (GetCurrentMaterialSettings()->FootstepImpactScaling > 0)
				{
					PlayFootStepMaterialImpact(VEC3V_TO_VECTOR3(pos),event,surfaceDepthVolLin);
				}
			}
		}
	}	
	BANK_ONLY(if(!sm_OnlyPlayMaterial && !sm_OnlyPlayMaterialCustom && !sm_OnlyPlayMaterialImpact))
	{	
		PlayShoeSound(event,VEC3V_TO_VECTOR3(pos),playToe,surfaceDepthVol);
	}
	TriggerAudioAIEvent();
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::GetCustomMaterialSoundRefsAndOffsets(audFootstepEvent event,audMetadataRef &soundRef,audMetadataRef &wetSoundRef,audMetadataRef &deepSoundRef,f32 &npcRunRollOffScale,f32 &jumpLandVolOffset)
{
	audSoundSet materialSoundSetRef;
	audSoundSet wetMaterialSoundSetRef;
	materialSoundSetRef.Init(GetCurrentMaterialSettings()->FootstepCustomImpactSound);
	if(	AUD_GET_TRISTATE_VALUE(GetCurrentMaterialSettings()->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_DRYMATERIAL)==AUD_TRISTATE_TRUE)
	{
		CollisionMaterialSettings *wetMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(GetCurrentMaterialSettings()->WetMaterialReference);
		if (wetMaterial)
		{
			wetMaterialSoundSetRef.Init(wetMaterial->FootstepCustomImpactSound);
		}
	}
	if(m_ParentOwningEntity->IsLocalPlayer() && m_ParentOwningEntity->GetFootstepHelper().IsWalkingOnPuddle())
	{
		CollisionMaterialSettings *puddleMaterial = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_PUDDLE", 0x350F8E9D));
		if (puddleMaterial)
		{
			materialSoundSetRef.Init(puddleMaterial->FootstepCustomImpactSound);
		}
	}
	if(event == AUD_FOOTSTEP_LIFT_L || event == AUD_FOOTSTEP_LIFT_R)
	{
		deepSoundRef = materialSoundSetRef.Find(ATSTRINGHASH("LIFT", 0x528C6099));
	}
	if(event == AUD_FOOTSTEP_WALK_L || event == AUD_FOOTSTEP_WALK_R
		|| event == AUD_FOOTSTEP_SOFT_L || event == AUD_FOOTSTEP_SOFT_R)
	{
		soundRef = materialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
		deepSoundRef = materialSoundSetRef.Find(ATSTRINGHASH("DEEP_WALK", 0x2124E268));
		if(wetMaterialSoundSetRef.IsInitialised())
		{
			wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
		}
	}
	else if(event == AUD_FOOTSTEP_RUN_L || event == AUD_FOOTSTEP_RUN_R)
	{
		soundRef = materialSoundSetRef.Find(ATSTRINGHASH("RUN", 0x1109B569));
		deepSoundRef = materialSoundSetRef.Find(ATSTRINGHASH("DEEP_WALK", 0x2124E268));
		if(wetMaterialSoundSetRef.IsInitialised())
		{
			wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("RUN", 0x1109B569));
		}
		if(soundRef == g_NullSoundRef || soundRef.Get() == g_NullSoundHash )
		{
			soundRef = materialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
			if(wetMaterialSoundSetRef.IsInitialised())
			{
				wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
			}
		}
		if(!m_ParentOwningEntity->IsLocalPlayer())
		{
			npcRunRollOffScale = sm_NPCRunFootstepRollOffScale;
		}
	}
	else if (event == AUD_FOOTSTEP_SPRINT_L || event == AUD_FOOTSTEP_SPRINT_R)
	{
		soundRef = materialSoundSetRef.Find(ATSTRINGHASH("SPRINT", 0xBC29E48));
		deepSoundRef = materialSoundSetRef.Find(ATSTRINGHASH("DEEP_WALK", 0x2124E268));
		if(wetMaterialSoundSetRef.IsInitialised())
		{
			wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("SPRINT", 0xBC29E48));
		}
		if(soundRef == g_NullSoundRef || soundRef.Get() == g_NullSoundHash)
		{
			soundRef = materialSoundSetRef.Find(ATSTRINGHASH("RUN", 0x1109B569));
			if(wetMaterialSoundSetRef.IsInitialised())
			{
				wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("RUN", 0x1109B569));
			}
			if(soundRef == g_NullSoundRef || soundRef.Get() == g_NullSoundHash)
			{
				soundRef = materialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
				if(wetMaterialSoundSetRef.IsInitialised())
				{
					wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
				}
			}
		}
	}
	else if (event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_JUMP_LAND_R)
	{
		f32 timeInTheAir = (f32)m_ParentOwningEntity->GetFootstepHelper().GetTimeInTheAir() ;
		timeInTheAir /= 1000.f;
		if(timeInTheAir > GetCurrentMaterialSettings()->TimeInAirToTriggerBigLand)
		{
			soundRef = materialSoundSetRef.Find(ATSTRINGHASH("LAND_HEIGHT", 0xD2D5F536));
			deepSoundRef = materialSoundSetRef.Find(ATSTRINGHASH("DEEP_LAND_HEIGHT", 0xE626BF01));
			if(wetMaterialSoundSetRef.IsInitialised())
			{
				wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("LAND_HEIGHT", 0xD2D5F536));
			}
		}
		m_ParentOwningEntity->GetFootstepHelper().ResetTimeInTheAir();
		if(soundRef == g_NullSoundRef || soundRef.Get() == g_NullSoundHash)
		{
			soundRef = materialSoundSetRef.Find(ATSTRINGHASH("LAND", 0xE355D038));
			deepSoundRef = materialSoundSetRef.Find(ATSTRINGHASH("DEEP_LAND_HEIGHT", 0xE626BF01));
			if(wetMaterialSoundSetRef.IsInitialised())
			{
				wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("LAND", 0xE355D038));
			}
			if(soundRef == g_NullSoundRef || soundRef.Get() == g_NullSoundHash)
			{
				soundRef = materialSoundSetRef.Find(ATSTRINGHASH("SPRINT", 0xBC29E48));
				if(wetMaterialSoundSetRef.IsInitialised())
				{
					wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("SPRINT", 0xBC29E48));
				}
				if(soundRef == g_NullSoundRef || soundRef.Get() == g_NullSoundHash)
				{
					soundRef = materialSoundSetRef.Find(ATSTRINGHASH("RUN", 0x1109B569));
					if(wetMaterialSoundSetRef.IsInitialised())
					{
						wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("RUN", 0x1109B569));
					}
					if(soundRef == g_NullSoundRef || soundRef.Get() == g_NullSoundHash)
					{
						soundRef = materialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
						if(wetMaterialSoundSetRef.IsInitialised())
						{
							wetSoundRef = wetMaterialSoundSetRef.Find(ATSTRINGHASH("WALK", 0x83504C9C));
						}
					}
				}
			}
		}
		jumpLandVolOffset = sm_CIJumpLandVolumeOffset;
	}
	else if (event == AUD_FOOTSTEP_LADDER_L || event == AUD_FOOTSTEP_LADDER_R)
	{
		// TODO : Add different ladder types
		audSoundSet ladderSoundSetRef;
		ladderSoundSetRef.Init(ATSTRINGHASH("METAL_SOLID_LADDER", 0xEBECD911));
		if(ladderSoundSetRef.IsInitialised())
		{
			soundRef = ladderSoundSetRef.Find(ATSTRINGHASH("CUSTOM_MATERIAL", 0x5452F794));
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayFootStepCustomImpactSound(audFootstepEvent event, const Vector3 &pos,u8 playToe,f32 surfaceDepthVol)
{
	f32 npcRunRollOffScale = 1.f;
	f32 jumpLandVolOffset = 0.f;
	audMetadataRef soundRef = g_NullSoundRef;
	audMetadataRef wetSoundRef = g_NullSoundRef;
	audMetadataRef deepSoundRef = g_NullSoundRef;
	GetCustomMaterialSoundRefsAndOffsets(event,soundRef,wetSoundRef,deepSoundRef,npcRunRollOffScale,jumpLandVolOffset);

	audSound * footstepCustom = NULL;
	audSoundInitParams initParams,wetInitParams,deepInitParams;
	initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
	//initParams.UpdateEntity = true;
	//f32 impactVolume = m_ParentOwningEntity->GetFootstepHelper().GetFootStepImpactVel() * (GetCurrentMaterialSettings()->FootstepScaling / 100.f) * sm_CustomFootstepImpulseMagnitude;
	//initParams.Volume = m_FootstepVolumeOffset ;
	if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
	{
		initParams.Volume -= sm_FeetInsideWaterVolDump;
	}
	f32 tunedVolume = 0.f;
	u32 tunedLPFCutoff = 24000;
	u32 tunedAttackTime = 0;

	if(m_FootstepsTuningValues)
	{
		tunedVolume = m_FootstepsTuningValues->Modes[m_ModeIndex].CustomImpact.VolumeOffset;
		tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].CustomImpact.LPFCutoff;
		tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].CustomImpact.AttackTime;
	}
	// Modes tuning values.
	if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
	{
		if(m_FootstepsTuningValues)
		{
			// Volume : 
			f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CustomImpact.VolumeOffset);
			f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].CustomImpact.VolumeOffset);
			tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
			// LPF : 
			f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CustomImpact.LPFCutoff);
			f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].CustomImpact.LPFCutoff);
			tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,badStealthLPFCutoff,goodStealthLPFCutoff));
			// Attack : 
			u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CustomImpact.AttackTime;
			u32 badStealthAttackTime = m_FootstepsTuningValues->Modes[m_BadStealthIndex].CustomImpact.AttackTime;
			tunedAttackTime = (u32)Lerp(m_PlayerStealth,(f32)badStealthAttackTime,(f32)goodStealthAttackTime);
		}
		if( event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_JUMP_LAND_R)
		{
			tunedVolume = sm_JumpLandCustomImpactStealthVol;
			tunedLPFCutoff = sm_JumpLandCustomImpactStealthLPFCutOff;
			tunedAttackTime = sm_JumpLandCustomImpactStealthAttackTime;
		}
	}
	initParams.Volume += tunedVolume;
	initParams.Volume += m_ClumsyStep ? -3.f : 0.f;
	initParams.Volume += jumpLandVolOffset;
	initParams.Volume += surfaceDepthVol;
	initParams.LPFCutoff = Min (initParams.LPFCutoff, tunedLPFCutoff);
	initParams.AttackTime = Max(initParams.AttackTime, tunedAttackTime); 
	// Footstep volume roll of over time. 
	initParams.Volume += m_FootVolOffsetOverTime;
	initParams.Position = pos;
	initParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
	initParams.u32ClientVar = (u32)event;
	initParams.UpdateEntity = sm_MaterialCustomTrack;
	initParams.VolumeCurveScale = npcRunRollOffScale;

	wetInitParams = initParams;
	f32 shoeHardness = BANK_ONLY(sm_OverrideShoeHardness?sm_OverridedShoeHardness:) (m_ShoeSettings ? m_ShoeSettings->ShoeHardness : 0.5f);
	if(AUD_GET_TRISTATE_VALUE(GetCurrentMaterialSettings()->Flags, FLAG_ID_COLLISIONMATERIALSETTINGS_DRYMATERIAL)==AUD_TRISTATE_TRUE)
	{
		initParams.Volume += sm_EqualPowerFallCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,Max(g_weather.GetWetness(),GetWetness(event)));
		if(wetSoundRef != g_NullSoundRef)
		{
			wetInitParams.Volume += sm_EqualPowerRiseCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,Max(g_weather.GetWetness(),GetWetness(event)));
			audSound *wetFootstepCustom = NULL;
			m_Parent->CreateSound_LocalReference(wetSoundRef,&wetFootstepCustom, &wetInitParams);
			if(wetFootstepCustom)
			{
				wetFootstepCustom->FindAndSetVariableValue(ATSTRINGHASH("ShoeHardness", 0x60A65D5E),shoeHardness);
				wetFootstepCustom->PrepareAndPlay();
			}
		}
	}
	if(surfaceDepthVol > 0.f)
	{
		deepInitParams = initParams;
		if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
		{
			deepInitParams.Volume += sm_FeetInsideWaterVolDump;
		}
		initParams.Volume += sm_EqualPowerFallCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,surfaceDepthVol);
		if(deepSoundRef != g_NullSoundRef)
		{
			deepInitParams.Volume += sm_EqualPowerRiseCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,surfaceDepthVol);
			audSound *deepFootstepCustom = NULL;
			m_Parent->CreateSound_LocalReference(deepSoundRef,&deepFootstepCustom, &deepInitParams);
			if(deepFootstepCustom)
			{
				deepFootstepCustom->FindAndSetVariableValue(ATSTRINGHASH("ShoeHardness", 0x60A65D5E),shoeHardness);
				deepFootstepCustom->PrepareAndPlay();
			}
		}
	}
	if(soundRef != g_NullSoundRef)
	{
		m_Parent->CreateSound_LocalReference(soundRef,&footstepCustom, &initParams);
		if(footstepCustom)
		{
			f32 footSpeed = BANK_ONLY(sm_OverrideFootSpeed ? sm_OverridedFootSpeed:)0.f;

			footstepCustom->FindAndSetVariableValue(ATSTRINGHASH("Speed", 0xF997622B),footSpeed);
			footstepCustom->FindAndSetVariableValue(ATSTRINGHASH("PlayToe", 0x92C69D81),playToe);
			footstepCustom->FindAndSetVariableValue(ATSTRINGHASH("ShoeHardness", 0x60A65D5E),shoeHardness);
			footstepCustom->PrepareAndPlay();
		}
	}
	f32 clumsiness = GetPlayerClumsiness();
	if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep && audEngineUtil::ResolveProbability(sm_PlayerClumsinessToMatSweetenerProb.CalculateValue(clumsiness)))
	{																												 
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
		{
			initParams.Volume -= sm_FeetInsideWaterVolDump;
		}
		// Footstep volume roll of over time. 
		initParams.Volume += m_FootVolOffsetOverTime;
		initParams.Volume += sm_PlayerClumsinessToMatSweetenerVol.CalculateValue(clumsiness);
		initParams.Volume += surfaceDepthVol;
		initParams.Position = pos;
		initParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
		initParams.u32ClientVar = (u32)event;
		initParams.UpdateEntity = sm_MaterialCustomTrack;

		m_Parent->CreateAndPlaySound(m_CurrentMaterialSettings->StealthSweetener, &initParams);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayFootStepMaterialImpact(const Vector3 &pos,audFootstepEvent event,f32 surfaceDepthScale )
{
	if(m_LastStandingEntity && m_LastStandingEntity->GetCurrentPhysicsInst() && m_LastStandingEntity->GetCurrentPhysicsInst()->IsInLevel())
	{
		WorldProbe::CShapeTestFixedResults<> tempResults;
		tempResults[0].SetHitMaterialId(BANK_ONLY(g_OverrideMaterialType ? sm_CollisionMaterial :)m_LastMaterial);
		tempResults[0].SetHitComponent((u16)m_LastComponent);
		tempResults[0].SetHitInst(
			m_LastStandingEntity->GetCurrentPhysicsInst()->GetLevelIndex(), 
#if LEVELNEW_GENERATION_IDS
			PHLEVEL->GetGenerationID(m_LastStandingEntity->GetCurrentPhysicsInst()->GetLevelIndex())
#else
			0
#endif // LEVELNEW_GENERATION_IDS
			);
		tempResults[0].SetHitPosition(pos);
		f32 impulseMagnitude = audEngineUtil::GetRandomNumberInRange(sm_MinImpulseMag,sm_MaxImpulseMag);
		impulseMagnitude *= BANK_ONLY(sm_DontApplyScales ? 1.f : ) (GetCurrentMaterialSettings()->FootstepImpactScaling / 100.f);
		// Modes tuning values.
		f32 materialImpactImpulseScale = 1.f;
		if(m_FootstepsTuningValues)
		{
			materialImpactImpulseScale = m_FootstepsTuningValues->Modes[m_ModeIndex].MaterialImpactImpulseScale;
			if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
			{
				// Volume : 
				f32 goodStealthScale = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].MaterialImpactImpulseScale;
				f32 badStealthScale = m_FootstepsTuningValues->Modes[m_BadStealthIndex].MaterialImpactImpulseScale;
				materialImpactImpulseScale = Lerp(m_PlayerStealth,badStealthScale,goodStealthScale);
			}
		}
		impulseMagnitude *= materialImpactImpulseScale;
		// Footstep volume roll of over time. 
		impulseMagnitude *= m_FootstepImpsMagOffsetOverTime;
		impulseMagnitude *= surfaceDepthScale;
		if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
		{		
			impulseMagnitude = 0.f;
		}

		g_CollisionAudioEntity.ReportMeleeCollision(&tempResults[0], impulseMagnitude, m_ParentOwningEntity, false, true);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::GetShoeSoundRefsAndOffsets(audFootstepEvent event,u32 &soundRef,u32 &dirtySoundRef,u32 &creakySoundRef,u32 &glassySoundRef,u32 &wetSoundRef,audMetadataRef &underRainSoundRef,f32 &volOffset,f32 &volCurveScale)
{
	if(!m_ShoeSettings)
		return;
	if(event == AUD_FOOTSTEP_WALK_L || event == AUD_FOOTSTEP_WALK_R)
	{
		soundRef = m_ShoeSettings->Walk;
		dirtySoundRef = m_ShoeSettings->DirtyWalk;
		creakySoundRef = m_ShoeSettings->CreakyWalk;
		glassySoundRef = m_ShoeSettings->GlassyWalk;
		wetSoundRef = m_ShoeSettings->WetWalk;
		underRainSoundRef = sm_FootstepsUnderRainSoundSet.Find(ATSTRINGHASH("WALK", 0x83504C9C));
		volOffset = sm_WalkFootstepVolOffset;
	}
	else if(event >= AUD_FOOTSTEP_RUN_L && event <= AUD_FOOTSTEP_RUN_R)
	{
		soundRef = m_ShoeSettings->Run;
		dirtySoundRef = m_ShoeSettings->DirtyRun;
		creakySoundRef = m_ShoeSettings->CreakyRun;
		glassySoundRef = m_ShoeSettings->GlassyRun;
		wetSoundRef = m_ShoeSettings->WetRun;
		underRainSoundRef = sm_FootstepsUnderRainSoundSet.Find(ATSTRINGHASH("RUN", 0x1109B569));
		volOffset = sm_RunFootstepVolOffset;
		if (!m_ParentOwningEntity->IsLocalPlayer())
		{
			volCurveScale = sm_NPCRunFootstepRollOffScale;
		}
	}
	else if (event >= AUD_FOOTSTEP_SPRINT_L && event <= AUD_FOOTSTEP_SPRINT_R)
	{
		soundRef = m_ShoeSettings->Run;
		dirtySoundRef = m_ShoeSettings->DirtyRun;
		creakySoundRef = m_ShoeSettings->CreakyRun;
		glassySoundRef = m_ShoeSettings->GlassyRun;
		wetSoundRef = m_ShoeSettings->WetRun;
		underRainSoundRef = sm_FootstepsUnderRainSoundSet.Find(ATSTRINGHASH("SPRINT", 0xBC29E48));
		volOffset = sm_RunFootstepVolOffset;
	}
	else if (event == AUD_FOOTSTEP_SOFT_L || event == AUD_FOOTSTEP_SOFT_R)
	{
		soundRef = m_ShoeSettings->SoftWalk;
		dirtySoundRef = m_ShoeSettings->DirtyWalk;
		creakySoundRef = m_ShoeSettings->CreakyWalk;
		glassySoundRef = m_ShoeSettings->GlassyWalk;
		wetSoundRef = m_ShoeSettings->WetWalk;
		underRainSoundRef = sm_FootstepsUnderRainSoundSet.Find(ATSTRINGHASH("SOFT", 0x77E2AB45));
		volOffset = sm_WalkFootstepVolOffset;
	}
	else if (event == AUD_FOOTSTEP_SCUFF_L || event == AUD_FOOTSTEP_SCUFF_R)
	{
		soundRef = m_CurrentMaterialSettings->Scuff;
	}
	else if (event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_JUMP_LAND_R)
	{
		soundRef = m_ShoeSettings->Land;
		dirtySoundRef = m_ShoeSettings->DirtyWalk;
		creakySoundRef = m_ShoeSettings->CreakyWalk;
		glassySoundRef = m_ShoeSettings->GlassyWalk;
		wetSoundRef = m_ShoeSettings->WetWalk;
		underRainSoundRef = sm_FootstepsUnderRainSoundSet.Find(ATSTRINGHASH("LAND", 0xE355D038));
		volOffset = sm_RunFootstepVolOffset;
	}
	else if (event == AUD_FOOTSTEP_LADDER_L || event == AUD_FOOTSTEP_LADDER_R)
	{
		soundRef = m_ShoeSettings->LadderShoeDown;
	}
	else if (event == AUD_FOOTSTEP_LADDER_L_UP || event == AUD_FOOTSTEP_LADDER_R_UP)
	{
		soundRef = m_ShoeSettings->LadderShoeUp;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayShoeSound(audFootstepEvent event,const Vector3 &pos,u8 playToe,f32 surfaceDepthVol)
{
	f32 npcVolCurveScale = 1.f;
	f32 movementVolOffset = 0.f;
	u32 soundRef = g_NullSoundHash;
	u32 dirtySoundRef = g_NullSoundHash;
	u32 creakySoundRef = g_NullSoundHash;
	u32 glassySoundRef = g_NullSoundHash;
	u32 wetSoundRef = g_NullSoundHash;
	audMetadataRef underRainSoundRef = g_NullSoundRef;
	GetShoeSoundRefsAndOffsets(event,soundRef,dirtySoundRef,creakySoundRef,glassySoundRef,wetSoundRef,underRainSoundRef,movementVolOffset,npcVolCurveScale);

	audSound * shoeSound = NULL;
	audSoundInitParams shoeInitParams;  
	shoeInitParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
	shoeInitParams.UpdateEntity = true;
	// Add volume offsets and scales
	f32 impactVolume = audDriverUtil::ComputeDbVolumeFromLinear(BANK_ONLY(sm_DontApplyScales ? 1.f : )(GetCurrentMaterialSettings()->FootstepScaling / 100.f));
	f32 volOffsetNPC = (m_ParentOwningEntity->IsLocalPlayer()?0.f:sm_NPCFootstepVolOffset);
	f32 sharedVolume = impactVolume + movementVolOffset + volOffsetNPC;
	f32 sharedVolumeNoImpact = movementVolOffset + volOffsetNPC + surfaceDepthVol;
	// Feet under water
	if(m_FootWaterInfo[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)] > audFootOutOfWater)
	{
		sharedVolume -= sm_FeetInsideWaterVolDump;
		sharedVolumeNoImpact -= sm_FeetInsideWaterVolDump;
	}
	sharedVolume += surfaceDepthVol;
	shoeInitParams.Volume = sharedVolume;
	// Footsteps roll off over time. 
	shoeInitParams.Volume += m_FootVolOffsetOverTime;
	shoeInitParams.Volume += surfaceDepthVol;
	shoeInitParams.Position = pos;
	shoeInitParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
	shoeInitParams.u32ClientVar = (u32)event;
	// Mode tuning values
	f32 tunedVolume = 0.f;
	u32 tunedLPFCutoff = 24000;
	u32 tunedAttackTime = 0;

	if(m_FootstepsTuningValues)
	{
		tunedVolume = m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.VolumeOffset;
		tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.LPFCutoff;
		tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.AttackTime;
	}
	if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep && m_ShoeType != ATSTRINGHASH("SHOE_BAREFOOT", 0x571BA0CC))
	{
		if(m_FootstepsTuningValues)
		{
			// Volume : 
			f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].Shoe.VolumeOffset);
			f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].Shoe.VolumeOffset);
			tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
			// LPF : 
			f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].Shoe.LPFCutoff);
			f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].Shoe.LPFCutoff);
			tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,goodStealthLPFCutoff,badStealthLPFCutoff));
			// Attack : 
			u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].Shoe.AttackTime;
			u32 badStealthAttackTime = m_FootstepsTuningValues->Modes[m_BadStealthIndex].Shoe.AttackTime;
			tunedAttackTime = (u32)Lerp(m_PlayerStealth,(f32)badStealthAttackTime,(f32)goodStealthAttackTime);
		}
	}
	else if (m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
	{
		tunedVolume = sm_BareFeetStealthVol;
		tunedLPFCutoff = sm_BareFeetStealthLPFCutOff;
		tunedAttackTime = sm_BareFeetStealthAttackTime;
	}
	shoeInitParams.Volume +=	tunedVolume;
	shoeInitParams.LPFCutoff =	Min (shoeInitParams.LPFCutoff, tunedLPFCutoff);
	shoeInitParams.AttackTime = Max(shoeInitParams.AttackTime, tunedAttackTime); 
	shoeInitParams.VolumeCurveScale = npcVolCurveScale;
	//Create and play sound.	
	f32 toeDelayOffset = 0.f;
	if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
	{
		toeDelayOffset = sm_PlayerStealthToToeDelayOffset.CalculateValue(m_PlayerStealth) / 1000.f;
		if(m_ClumsyStep)
		{
			toeDelayOffset = sm_ClumsyStepToeDelayOffset/1000.f;
		}
	}
	if (event == AUD_FOOTSTEP_SCUFF_L || event == AUD_FOOTSTEP_SCUFF_R)
	{
		shoeInitParams.Predelay =	(s32)(toeDelayOffset * 1000.f);
	}
	m_Parent->CreateSound_LocalReference(soundRef,&shoeSound, &shoeInitParams);
	f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)GetCurrentMaterialSettings()->FootstepMaterialHardness;
	if(shoeSound)
	{
		shoeSound->FindAndSetVariableValue(ATSTRINGHASH("TOE_DELAY", 0x41504027),toeDelayOffset);
		shoeSound->FindAndSetVariableValue(ATSTRINGHASH("MaterialHardness", 0xEDD0ED05),materialHardness);
		shoeSound->PrepareAndPlay();
	}
	// Play all layers
	PlayDirtLayer(dirtySoundRef,sharedVolume,npcVolCurveScale,playToe,event,pos);
	PlayCreakLayer(creakySoundRef,sharedVolumeNoImpact,npcVolCurveScale,event,pos);
	PlayGlassLayer(glassySoundRef,sharedVolumeNoImpact,npcVolCurveScale,playToe,event,pos);
	PlayWetLayer(wetSoundRef,underRainSoundRef,sharedVolumeNoImpact,npcVolCurveScale,playToe,event,pos);

#if __BANK
	if(g_bDrawFootsteps)
	{
		Vector3 headPos;
		m_ParentOwningEntity->GetBonePosition(headPos,BONETAG_HEAD);
		DebugAudioFootstep(GetCurrentMaterialSettings(),impactVolume,shoeInitParams.Volume,m_ShoeType,headPos);
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayDirtLayer(const u32 soundRef, f32 volume,f32 volCurveScale,u8 playToe,audFootstepEvent event,const Vector3 &pos)
{
	f32 shoeDirtiness = CalculateShoeDirtiness();
	if(shoeDirtiness > 0.f)
	{
		audSound * dirtyShoeSound = NULL;
		audSoundInitParams layersInitParams;  
		layersInitParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		layersInitParams.UpdateEntity = true;
		layersInitParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
		layersInitParams.Position = pos;
		layersInitParams.u32ClientVar = (u32)event;
		// Mode tuning values
		f32 tunedVolume = 0.f;
		u32 tunedLPFCutoff = 24000;
		u32 tunedAttackTime = 0;

		if(m_FootstepsTuningValues)
		{
			tunedVolume = m_FootstepsTuningValues->Modes[m_ModeIndex].DirtLayer.VolumeOffset;
			tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].DirtLayer.LPFCutoff;
			tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].DirtLayer.AttackTime;
			if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
			{
				// Volume : 
				f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].DirtLayer.VolumeOffset);
				f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].DirtLayer.VolumeOffset);
				tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
				// LPF : 
				f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].DirtLayer.LPFCutoff);
				f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].DirtLayer.LPFCutoff);
				tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,goodStealthLPFCutoff,badStealthLPFCutoff));
				// Attack : 
				u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].DirtLayer.AttackTime;
				u32 badStealthAttackTime = m_FootstepsTuningValues->Modes[m_BadStealthIndex].DirtLayer.AttackTime;
				tunedAttackTime = (u32)Lerp(m_PlayerStealth,(f32)badStealthAttackTime,(f32)goodStealthAttackTime);
			}
		}
		layersInitParams.Volume = volume + m_FootVolOffsetOverTime + tunedVolume;
		layersInitParams.LPFCutoff = Min (layersInitParams.LPFCutoff, tunedLPFCutoff);
		layersInitParams.AttackTime = Max(layersInitParams.AttackTime, tunedAttackTime); 
		layersInitParams.VolumeCurveScale = volCurveScale;

		m_Parent->CreateSound_LocalReference(soundRef,&dirtyShoeSound, &layersInitParams);
		if(dirtyShoeSound)
		{
			f32 footSpeed = BANK_ONLY(sm_OverrideFootSpeed ? sm_OverridedFootSpeed:)0.f;
			f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)GetCurrentMaterialSettings()->FootstepMaterialHardness;
			f32 shoeHardness = BANK_ONLY(sm_OverrideShoeHardness?sm_OverridedShoeHardness:) (m_ShoeSettings ? m_ShoeSettings->ShoeHardness : 0.5f);

			dirtyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Speed", 0xF997622B),footSpeed);
			dirtyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("PlayToe", 0x92C69D81),playToe);
			dirtyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Hardness", 0x80F3C3E),materialHardness * shoeHardness);
			dirtyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Dirtiness", 0xB233D437),shoeDirtiness);
			dirtyShoeSound->PrepareAndPlay();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayCreakLayer(const u32 soundRef, f32 volume,f32 volCurveScale,audFootstepEvent event,const Vector3 &pos)
{
	//Play the creaky sound
	f32 shoeCreakiness = GetShoeCreakiness(event);
	if(shoeCreakiness > 0.f)
	{
		audSound * creakyShoeSound = NULL;
		audSoundInitParams layersInitParams;  
		layersInitParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		layersInitParams.UpdateEntity = true;
		layersInitParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
		layersInitParams.Position = pos;
		layersInitParams.u32ClientVar = (u32)event;
		// Mode tuning values
		f32 tunedVolume = 0.f;
		u32 tunedLPFCutoff = 24000;
		u32 tunedAttackTime = 0;

		if(m_FootstepsTuningValues)
		{
			tunedVolume = m_FootstepsTuningValues->Modes[m_ModeIndex].CreakLayer.VolumeOffset;
			tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].CreakLayer.LPFCutoff;
			tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].CreakLayer.AttackTime;
			if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
			{
				// Volume : 
				f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CreakLayer.VolumeOffset);
				f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].CreakLayer.VolumeOffset);
				tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
				// LPF : 
				f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CreakLayer.LPFCutoff);
				f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].CreakLayer.LPFCutoff);
				tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,goodStealthLPFCutoff,badStealthLPFCutoff));
				// Attack : 
				u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CreakLayer.AttackTime;
				u32 badStealthAttackTime = m_FootstepsTuningValues->Modes[m_BadStealthIndex].CreakLayer.AttackTime;
				tunedAttackTime = (u32)Lerp(m_PlayerStealth,(f32)badStealthAttackTime,(f32)goodStealthAttackTime);
			}
		}
		layersInitParams.Volume = volume + m_FootVolOffsetOverTime + tunedVolume;
		layersInitParams.LPFCutoff = Min (layersInitParams.LPFCutoff, tunedLPFCutoff);
		layersInitParams.AttackTime = Max(layersInitParams.AttackTime,tunedAttackTime); 
		layersInitParams.VolumeCurveScale = volCurveScale;

		m_Parent->CreateSound_LocalReference(soundRef,&creakyShoeSound, &layersInitParams);
		if(creakyShoeSound)
		{
			f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)GetCurrentMaterialSettings()->FootstepMaterialHardness;
			creakyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("MaterialHardness", 0xEDD0ED05),materialHardness);
			creakyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Creakiness", 0xC435F6D8),shoeCreakiness);
			creakyShoeSound->PrepareAndPlay();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayGlassLayer(const u32 soundRef,f32 volume,f32 volCurveScale,u8 playToe,audFootstepEvent event,const Vector3 &pos)
{
	// Don't play the glass sound if the ped is aiming while Idling.
	if(event == AUD_FOOTSTEP_SOFT_L || event == AUD_FOOTSTEP_SOFT_R)
	{
		if(m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_HasGunTaskWithAimingState) )
		{
			return;
		}
	}
	//If the ground has broken glass, play the glassy sound
	f32 glassiness = BANK_ONLY(sm_OverrideShoeGlassiness ? sm_OverridedShoeGlassiness :) GetGlassiness(0.005f);
	if(glassiness > 0.f)
	{
		audSound * glassyShoeSound = NULL;
		audSoundInitParams layersInitParams;  
		layersInitParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		layersInitParams.UpdateEntity = true;
		layersInitParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
		layersInitParams.Position = pos;
		layersInitParams.u32ClientVar = (u32)event;
		// Mode tuning values
		f32 tunedVolume = 0.f;
		u32 tunedLPFCutoff = 24000;
		u32 tunedAttackTime = 0;

		if(m_FootstepsTuningValues)
		{
			tunedVolume = m_FootstepsTuningValues->Modes[m_ModeIndex].GlassLayer.VolumeOffset;
			tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].GlassLayer.LPFCutoff;
			tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].GlassLayer.AttackTime;

			if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
			{
				// Volume : 
				f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].GlassLayer.VolumeOffset);
				f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].GlassLayer.VolumeOffset);
				tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
				// LPF : 
				f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].GlassLayer.LPFCutoff);
				f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].GlassLayer.LPFCutoff);
				tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,goodStealthLPFCutoff,badStealthLPFCutoff));
				// Attack : 
				u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].GlassLayer.AttackTime;
				u32 badStealthAttackTime = (u32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].GlassLayer.AttackTime;
				tunedAttackTime = (u32)Lerp(m_PlayerStealth,(f32)badStealthAttackTime,(f32)goodStealthAttackTime);
			}
		}
		layersInitParams.Volume = volume + m_FootVolOffsetOverTime + tunedVolume;
		layersInitParams.LPFCutoff = Min (layersInitParams.LPFCutoff, tunedLPFCutoff);
		layersInitParams.AttackTime = Max(layersInitParams.AttackTime, tunedAttackTime); 
		layersInitParams.VolumeCurveScale = volCurveScale;

		m_Parent->CreateSound_LocalReference(soundRef,&glassyShoeSound, &layersInitParams);
		if(glassyShoeSound)
		{
			f32 footSpeed = BANK_ONLY(sm_OverrideFootSpeed ? sm_OverridedFootSpeed:)0.f;
			f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)GetCurrentMaterialSettings()->FootstepMaterialHardness;
			f32 shoeHardness = BANK_ONLY(sm_OverrideShoeHardness?sm_OverridedShoeHardness:)(m_ShoeSettings ? m_ShoeSettings->ShoeHardness : 0.5f);

			glassyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Speed", 0xF997622B),footSpeed);
			glassyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("PlayToe", 0x92C69D81),playToe);
			glassyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Hardness", 0x80F3C3E),materialHardness * shoeHardness);
			glassyShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Glassiness", 0xB8040FD7),glassiness);
			glassyShoeSound->PrepareAndPlay();
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayWetLayer(const u32 soundRef,const audMetadataRef underRainSoundRef,f32 volume,f32 volCurveScale,u8 playToe,audFootstepEvent event,const Vector3 &pos)
{
	//If the ground is wet, play the wetness layer
	f32 wetness = GetWetness(event);
	if(!m_ForceWetFeetReset && (wetness > 0.0f || g_weather.GetTimeCycleAdjustedRain() > 0.0f) )
	{
		f32 shoeHardness = BANK_ONLY(sm_OverrideShoeHardness?sm_OverridedShoeHardness:)(m_ShoeSettings ? m_ShoeSettings->ShoeHardness : 0.5f);
		f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)GetCurrentMaterialSettings()->FootstepMaterialHardness;

		audSound * wetShoeSound = NULL;
		audSound * underRainSound = NULL;
		audSoundInitParams layersInitParams;  
		layersInitParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
		layersInitParams.UpdateEntity = true;
		layersInitParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(m_FootstepPitchRatio);
		layersInitParams.Position = pos;
		layersInitParams.u32ClientVar = (u32)event;
		// Mode tuning values
		f32 tunedVolume = 0.f;
		u32 tunedLPFCutoff = 24000;
		u32 tunedAttackTime = 0;

		if(m_FootstepsTuningValues)
		{
			tunedVolume = m_FootstepsTuningValues->Modes[m_ModeIndex].WetLayer.VolumeOffset;
			tunedLPFCutoff = m_FootstepsTuningValues->Modes[m_ModeIndex].WetLayer.LPFCutoff;
			tunedAttackTime = m_FootstepsTuningValues->Modes[m_ModeIndex].WetLayer.AttackTime;
			if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && !m_ClumsyStep)
			{
				// Volume : 
				f32 goodStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].WetLayer.VolumeOffset);
				f32 badStealthVolume = audDriverUtil::ComputeLinearVolumeFromDb(m_FootstepsTuningValues->Modes[m_BadStealthIndex].WetLayer.VolumeOffset);
				tunedVolume = audDriverUtil::ComputeDbVolumeFromLinear(Lerp(m_PlayerStealth,badStealthVolume,goodStealthVolume));
				// LPF : 
				f32 goodStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_GoodStealthIndex].WetLayer.LPFCutoff);
				f32 badStealthLPFCutoff = audDriverUtil::ComputeLinearFromHzFrequency((f32)m_FootstepsTuningValues->Modes[m_BadStealthIndex].WetLayer.LPFCutoff);
				tunedLPFCutoff = (u32)audDriverUtil::ComputeHzFrequencyFromLinear(Lerp(m_PlayerStealth,goodStealthLPFCutoff,badStealthLPFCutoff));
				// Attack : 
				u32 goodStealthAttackTime = m_FootstepsTuningValues->Modes[m_GoodStealthIndex].WetLayer.AttackTime;
				u32 badStealthAttackTime = m_FootstepsTuningValues->Modes[m_BadStealthIndex].WetLayer.AttackTime;
				tunedAttackTime = Lerp(m_PlayerStealth,badStealthAttackTime,goodStealthAttackTime);
			}
		}
		layersInitParams.Volume = volume + m_FootVolOffsetOverTime + tunedVolume;
		layersInitParams.LPFCutoff = Min (layersInitParams.LPFCutoff, tunedLPFCutoff);
		layersInitParams.AttackTime = Max(layersInitParams.AttackTime, tunedAttackTime); 
		layersInitParams.VolumeCurveScale = volCurveScale;

		if(wetness > 0.f)
		{	
			m_Parent->CreateSound_LocalReference(soundRef,&wetShoeSound, &layersInitParams);
			if(wetShoeSound)
			{
				f32 footSpeed = BANK_ONLY(sm_OverrideFootSpeed ? sm_OverridedFootSpeed:)0.f;

				wetShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Speed", 0xF997622B),footSpeed);
				wetShoeSound->FindAndSetVariableValue(ATSTRINGHASH("PlayToe", 0x92C69D81),playToe);
				wetShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Hardness", 0x80F3C3E),materialHardness * shoeHardness);
				wetShoeSound->FindAndSetVariableValue(ATSTRINGHASH("Wetness", 0xA53F2E56),wetness);
				wetShoeSound->PrepareAndPlay();
			}
		}
		// sm_IsWalkingOnPuddle is only true when the player ped walks on a shader-generated puddle.
		// Play the 'under rain' sounds.
		if(g_weather.GetTimeCycleAdjustedRain() > 0.f && !m_ParentOwningEntity->GetIsInInterior() && (!m_ParentOwningEntity->IsLocalPlayer() || (m_ParentOwningEntity->IsLocalPlayer() && !m_ParentOwningEntity->GetFootstepHelper().IsWalkingOnPuddle())))
		{
			bool isUnderCover = false;
			if(m_ParentOwningEntity->GetAudioEnvironmentGroup())
			{
				isUnderCover =  ((naEnvironmentGroup*)m_ParentOwningEntity->GetAudioEnvironmentGroup())->IsUnderCover();
			}
			if(!isUnderCover)
			{
				m_Parent->CreateSound_LocalReference(underRainSoundRef,&underRainSound, &layersInitParams);
				if(underRainSound)
				{
					underRainSound->FindAndSetVariableValue(ATSTRINGHASH("Hardness", 0x80F3C3E),materialHardness * shoeHardness);
					underRainSound->FindAndSetVariableValue(ATSTRINGHASH("RainLevel", 0x469E4C87),g_weather.GetTimeCycleAdjustedRain());
					underRainSound->PrepareAndPlay();
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayAnimalFootStepsEvent(audFootstepEvent event, bool playWhenNotStanding)
{
	Vec3V pos;
	f32 freq,vol;

#if __DEV
	if(g_BreakpointDebugPedFootstep && g_AudioDebugEntity == m_ParentOwningEntity)
	{
		__debugbreak();
	}
#endif

#if __BANK
	if(g_DisableFootstepSounds)
	{
		return;
	}
#endif
	m_ParentOwningEntity->GetFootstepHelper().GetPositionForPedSound(event, pos);

	// early out from any footstep events if the ped isnt in contact with the ground
	if((event >= AUD_FOOTSTEP_WALK_L && event <= AUD_FOOTSTEP_SCUFF_R) || (event >= AUD_FOOTSTEP_JUMP_LAND_L && event <= AUD_FOOTSTEP_SOFT_R))
	{
		if(!playWhenNotStanding && !m_ParentOwningEntity->GetIsStanding())
		{
			return;
		}
	}
	if(naVerifyf(GetCurrentMaterialSettings(), "No material settings on audPedaudioEntity during PlayAnimalFootstepevent"))
	{
		AnimalFootstepSettings *footstepSettings = GetAnimalFootstepSettings();
#if __BANK
		if(g_AudioEngine.GetRemoteControl().IsPresent() && !footstepSettings)
		{
			naWarningf("Failed getting animal footstep settings for animal :%s",m_ParentOwningEntity->GetPedModelInfo()->GetModelName());
		}
#endif 
		if(footstepSettings)
		{
			GetVolumeAndRelFreqForFootstep(vol,freq);

			// range check - either one of the normal walk events, jumps or run
			if(/*GetCurrentMaterialSettings()->FootstepScaling > 0 && */(event <= AUD_FOOTSTEP_SCUFF_R || (event >= AUD_FOOTSTEP_JUMP_LAND_L && event <= AUD_FOOTSTEP_SOFT_R)))
			{	
				u32 soundRef = GetAnimalFootstepSoundRef(footstepSettings, event);
				if(event == AUD_FOOTSTEP_SPRINT_L && soundRef == g_NullSoundHash)
				{
					soundRef = GetAnimalFootstepSoundRef(footstepSettings, AUD_FOOTSTEP_RUN_L);
				}
				else if ( event == AUD_FOOTSTEP_SPRINT_R && soundRef == g_NullSoundHash)
				{
					soundRef = GetAnimalFootstepSoundRef(footstepSettings, AUD_FOOTSTEP_RUN_R);
				}
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
				//initParams.Category = g_PedFeetCategories[m_ShoeType - 1];
				initParams.UpdateEntity = true;
				f32 impactVolume = 1.f;//(GetCurrentMaterialSettings()->FootstepScaling / 100.f);
				if(event == AUD_FOOTSTEP_SCUFF_L || event == AUD_FOOTSTEP_SCUFF_R)
				{
					impactVolume = (GetCurrentMaterialSettings()->ScuffstepScaling / 100.f);
				}
				initParams.Volume = vol + audDriverUtil::ComputeDbVolumeFromLinear(impactVolume);
				initParams.Position = VEC3V_TO_VECTOR3(pos);
				initParams.Pitch = (s16) audDriverUtil::ConvertRatioToPitch(freq);
				initParams.u32ClientVar = (u32)event;
				m_Parent->CreateAndPlaySound(soundRef, &initParams);
#if __BANK
				if(g_bDrawFootsteps)
				{
					Vector3 headPos;
					m_ParentOwningEntity->GetBonePosition(headPos,BONETAG_HEAD);
					DebugAudioFootstep(GetCurrentMaterialSettings(),impactVolume,initParams.Volume,m_ShoeType,headPos);
				}
#endif
				// and trigger an ai audio event
				f32 footstepLoudness = 0.0f;
				s8 materialLoudnessType = footstepSettings->AudioEventLoudness;
				if (materialLoudnessType>=0 && materialLoudnessType<FOOTSTEPLOUDNESS_MAX)
				{
					footstepLoudness = CMiniMap::sm_Tunables.Sonar.fSoundRange_FootstepBase * g_MaterialLoudness[materialLoudnessType];
					// If in foliage, use a (potentially) louder noise event. Note that the order we do things in here matters,
					// it probably makes sense to include the speed and crouch factors when in contact with foliage,
					// but not the ground material or the shoes.
					if(m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_InContactWithFoliage) || m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_InContactWithBIGFoliage))
					{
						footstepLoudness = Max(footstepLoudness, CMiniMap::sm_Tunables.Sonar.fSoundRange_FootstepFoliage);
					}

					u32 moveState = ConvertPedMoveBlendRatioToAudMoveState(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY());
					footstepLoudness *= g_MoveStateLoudnessScale[moveState];
					if(m_ParentOwningEntity->GetIsCrouching())
					{
						footstepLoudness *= g_CrouchedLoudnessFactor;
					}
					m_FootstepLoudness = (footstepLoudness > m_FootstepLoudness) ? footstepLoudness : m_FootstepLoudness; 
				}

			}

			if(GetCurrentMaterialSettings()->FootstepImpactScaling > 0 && (event == AUD_FOOTSTEP_WALK_L || event == AUD_FOOTSTEP_WALK_R 
				|| event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_JUMP_LAND_R || event == AUD_FOOTSTEP_RUN_L || event == AUD_FOOTSTEP_RUN_R 
				|| event == AUD_FOOTSTEP_SPRINT_L || event == AUD_FOOTSTEP_SPRINT_R || event == AUD_FOOTSTEP_SOFT_L || event == AUD_FOOTSTEP_SOFT_R))
			{
				if(m_LastStandingEntity && m_LastStandingEntity->GetCurrentPhysicsInst() && m_LastStandingEntity->GetCurrentPhysicsInst()->IsInLevel())
				{
					WorldProbe::CShapeTestFixedResults<> tempResults;
					tempResults[0].SetHitMaterialId(m_LastMaterial);
					tempResults[0].SetHitComponent((u16)m_LastComponent);
					tempResults[0].SetHitInst(
						m_LastStandingEntity->GetCurrentPhysicsInst()->GetLevelIndex(), 
#if LEVELNEW_GENERATION_IDS
						PHLEVEL->GetGenerationID(m_LastStandingEntity->GetCurrentPhysicsInst()->GetLevelIndex())
#else
						0
#endif // LEVELNEW_GENERATION_IDS
						);
					tempResults[0].SetHitPositionV(pos);
					f32 impactVolume = 1.f;//(GetCurrentMaterialSettings()->FootstepImpactScaling / 100.f) ;
					g_CollisionAudioEntity.ReportMeleeCollision(&tempResults[0], impactVolume, m_ParentOwningEntity, false, true);
				}
			}
			if(GetCurrentMaterialSettings()->FootstepImpactScaling > 0 &&(event == AUD_FOOTSTEP_SCUFFHARD_L 
				|| event ==	AUD_FOOTSTEP_SCUFFHARD_R || event == AUD_FOOTSTEP_SCUFF_L || event == AUD_FOOTSTEP_SCUFF_R))
			{
				// LRG: Not sure under what circumstances this might not be in the level, but it can happen.
				if(m_LastStandingEntity && m_LastStandingEntity->GetCurrentPhysicsInst() && m_LastStandingEntity->GetCurrentPhysicsInst()->IsInLevel())
				{
					WorldProbe::CShapeTestFixedResults<> tempResults;
					tempResults[0].SetHitMaterialId(m_LastMaterial);
					tempResults[0].SetHitComponent((u16)m_LastComponent);
					tempResults[0].SetHitInst(
						m_LastStandingEntity->GetCurrentPhysicsInst()->GetLevelIndex(), 
#if LEVELNEW_GENERATION_IDS
						PHLEVEL->GetGenerationID(m_LastStandingEntity->GetCurrentPhysicsInst()->GetLevelIndex())
#else
						0
#endif // LEVELNEW_GENERATION_IDS
						);
					tempResults[0].SetHitPositionV(pos);
					f32 impactVolume = 1.f;//GetCurrentMaterialSettings()->FootstepImpactScaling / 100.f) ;
					g_CollisionAudioEntity.ReportMeleeCollision(&tempResults[0], impactVolume, m_ParentOwningEntity, false, true);
				}
			}

			// only trigger scuffs on WALK events
			if(event == AUD_FOOTSTEP_WALK_L || event == AUD_FOOTSTEP_WALK_R || event == AUD_FOOTSTEP_SOFT_L || event == AUD_FOOTSTEP_SOFT_R )
			{
				//f32 skidAmount = ComputeSkidAmount();
				if((GetCurrentMaterialSettings()->FootstepImpactScaling > 0) && m_FootstepsTuningValues && audEngineUtil::ResolveProbability(m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.DragScuffProbability))
				{
					//Displayf("skid amount: %f", skidAmount);
					u32 soundRef = GetAnimalFootstepSoundRef(footstepSettings, AUD_FOOTSTEP_SCUFF_L);		
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
					initParams.UpdateEntity = true;
					initParams.Position = VEC3V_TO_VECTOR3(pos);
					f32 impactVolume = 1.f;//(GetCurrentMaterialSettings()->FootstepImpactScaling / 100.f);
					initParams.Volume = impactVolume;
					initParams.u32ClientVar = (u32)event;
					m_Parent->CreateAndPlaySound(soundRef, &initParams);
				} // if skidding
			} // if event is walk
		}// if(footstepSettings)		
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::TriggerWaveImpact()
{
	f32 sqdDistance = VEC3V_TO_VECTOR3(m_ParentOwningEntity->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()).Mag2();
	if (sqdDistance <= 225.f)
	{
		m_ShouldStopWaveImpact = false;
		if((m_FootWaterInfo[LeftFoot] < audBodyUnderWater || m_FootWaterInfo[RightFoot] < audBodyUnderWater ) && !m_WaveImpactSound)
		{
			// Trigger a wave wet impact.
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.TrackEntityPosition = true;
			initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
			m_Parent->CreateAndPlaySound_Persistent(sm_PedInsideWaterSoundSet.Find(ATSTRINGHASH("WaveImpact", 0x2ACDF83E)),&m_WaveImpactSound,&initParams);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::TriggerFallInWaterSplash(bool collisionEvent)
{
	if(m_Parent && m_ParentOwningEntity && !m_ParentOwningEntity->GetPedResetFlag( CPED_RESET_FLAG_IsEnteringVehicle )&& m_LastSplashTime + 1000 < g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
	{
		f32 sqdDistance = VEC3V_TO_VECTOR3(m_ParentOwningEntity->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()).Mag2();
		if (sqdDistance <= 225.f)
		{
			f32 cameraDepth = Water::GetCameraWaterDepth();
			bool isVaultingOrParachuting = false;
			if(m_ParentOwningEntity && m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting))
			{
				const CTaskVault* climbTask = static_cast<const CTaskVault*>(m_ParentOwningEntity->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VAULT));
				if(climbTask && climbTask->GetState() == CTaskVault::State_ClamberVault)
				{
					isVaultingOrParachuting = true;
				}
			}

			if(collisionEvent )
			{
				if(m_ParentOwningEntity->GetCapsuleInfo()->IsFish())
				{
					return;
				}
				if (!Water::IsCameraUnderwater() )
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
					initParams.Tracker = m_ParentOwningEntity->GetPlaceableTracker();
					m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find((cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterSmallUnderwater", 0x881969D9) : ATSTRINGHASH("FallingInWaterSmall", 0x9CFBF97A))), &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PedInsideWaterSoundSet.GetNameHash(),(cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterSmallUnderwater", 0x881969D9) : ATSTRINGHASH("FallingInWaterSmall", 0x9CFBF97A)), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
					m_LastSplashTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
				}
			}
			else 
			{

				f32 fallingHeight = m_ParentOwningEntity->GetFallingHeight() - m_ParentOwningEntity->GetTransform().GetPosition().GetZf();
				fallingHeight = Max(fallingHeight, 0.f);
				isVaultingOrParachuting = isVaultingOrParachuting || m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_IsParachuting);
				if(isVaultingOrParachuting || m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringOrExitingVehicle))
				{
					CVehicle* lastVehicle = const_cast<CVehicle*>(m_Parent->GetLastVehicle()); 
					CAmphibiousAutomobile* amphibiousVeh =  (lastVehicle && lastVehicle->InheritsFromAmphibiousAutomobile()) ? static_cast<CAmphibiousAutomobile*>(lastVehicle) : NULL; 
					if(isVaultingOrParachuting || (lastVehicle  && lastVehicle->GetVehicleAudioEntity() && ((lastVehicle->GetVehicleAudioEntity()->GetAudioVehicleType() == AUD_VEHICLE_BOAT) || (amphibiousVeh && amphibiousVeh->IsPropellerSubmerged()))))
					{
						fallingHeight += 0.5f;
					}
				}
				if(m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_IsDiving))
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
					initParams.Tracker = m_ParentOwningEntity->GetPlaceableTracker();
					m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find((cameraDepth > 0 ? ATSTRINGHASH("DiveInWaterUnderwater", 0x409B7CA8) : ATSTRINGHASH("DiveInWater", 0xB699090A))),&initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PedInsideWaterSoundSet.GetNameHash(),cameraDepth > 0 ? ATSTRINGHASH("DiveInWaterUnderwater", 0x409B7CA8) : ATSTRINGHASH("DiveInWater", 0xB699090A), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
					m_LastSplashTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

				}
				else if((fallingHeight > sm_WaterSplashThreshold) || m_ParentOwningEntity->GetFootstepHelper().WasClimbingALadder())
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
					initParams.Tracker = m_ParentOwningEntity->GetPlaceableTracker();
					if( fallingHeight <= sm_WaterSplashSmallThreshold )
					{
						m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find((cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterSmallUnderwater", 0x881969D9) : ATSTRINGHASH("FallingInWaterSmall", 0x9CFBF97A))), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PedInsideWaterSoundSet.GetNameHash(),cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterSmallUnderwater", 0x881969D9) : ATSTRINGHASH("FallingInWaterSmall", 0x9CFBF97A), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
					}
					else if ( fallingHeight <= sm_WaterSplashMediumThreshold)
					{
						m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find((cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterMediumUnderwater", 0xEDFD0206) : ATSTRINGHASH("FallingInWaterMedium", 0x69A6CEC5))), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PedInsideWaterSoundSet.GetNameHash(),cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterMediumUnderwater", 0xEDFD0206) : ATSTRINGHASH("FallingInWaterMedium", 0x69A6CEC5), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
					}
					else
					{
						m_Parent->CreateAndPlaySound(sm_PedInsideWaterSoundSet.Find((cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterHeavyUnderwater", 0x8EE97D58) : ATSTRINGHASH("FallingInWaterHeavy", 0x8A364107))), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_PedInsideWaterSoundSet.GetNameHash(),cameraDepth > 0 ? ATSTRINGHASH("FallingInWaterHeavyUnderwater", 0x8EE97D58) : ATSTRINGHASH("FallingInWaterHeavy", 0x8A364107), &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
					}
					m_LastSplashTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
				}
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::TriggerAudioAIEvent()
{
	// and trigger an ai audio event
	f32 materialHardness = BANK_ONLY(sm_OverrideMaterialHardness?sm_OverridedMaterialHardness:)GetCurrentMaterialSettings()->FootstepMaterialHardness;
	f32 shoeHardness = BANK_ONLY(sm_OverrideShoeHardness?sm_OverridedShoeHardness:)(m_ShoeSettings ? m_ShoeSettings->ShoeHardness : 0.5f);

	float shoeMtrlScale = materialHardness*shoeHardness;

	// Note: this is a bit funky. Based on the material factor and shoe factor ranging from 0.5 to 2.0
	// (those were old min/max values, might be exceeded now), their combined factor could range from 0.25
	// to 4.0. With a lerp value of 0 mapping to the smallest of that and a value of 1 mapping to the greatest,
	// the "normal" lerp value comes out to 0.2, rather than 0.5 as one might expect. We could use a logarithmic
	// scale or something or some other sort of remapping if this becomes too confusing.
	float lerp = Clamp((shoeMtrlScale - 0.25f)/3.75f, 0.0f, 1.0f);

	// Get the noise index for the tuning data, based on the current speed and crouch state.
	const CPedMotionData* pPedMotionData = m_ParentOwningEntity->GetMotionData();
	int level = -1;
	int crouchOffs = m_ParentOwningEntity->GetMotionData()->GetUsingStealth() ? -1 : 0;
	if (pPedMotionData)
	{
		if (pPedMotionData->GetIsWalking())
		{
			level = audFootstepEventTuning::kFootstepWalkStand + crouchOffs;
		}
		else if(pPedMotionData->GetIsRunning())
		{
			level = audFootstepEventTuning::kFootstepRunStand + crouchOffs;
		}
		else if(pPedMotionData->GetIsSprinting())
		{
			level = audFootstepEventTuning::kFootstepSprint;
		}
	}

	f32 footstepLoudness = 0.0f;
	if(level >= 0)
	{
		// If in foliage, use a (potentially) louder noise event.
		if(m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_InContactWithFoliage) || m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_InContactWithBIGFoliage))
		{
			lerp = Max(lerp, 1.0f);	// TODO: Maybe make the 1.0 here tunable.
		}

		// Find the relevant tuning data, SP or MP.
		const audFootstepEventMinMax *tuning;
		if(NetworkInterface::IsGameInProgress())
		{
			tuning = &sm_FootstepEventTuning.m_MultiPlayer[level];
		}
		else
		{
			tuning = &sm_FootstepEventTuning.m_SinglePlayer[level];
		}

		// Lerp based on the material/shoe parameter (0..1), between the min and max distances
		// for the relevant speed and crouchs state.
		naAssertf(tuning->m_MinDist <= tuning->m_MaxDist, "Min distance greater than max distance, for footstep noise (%f vs. %f)", tuning->m_MinDist, tuning->m_MaxDist);
		footstepLoudness = Lerp(lerp, tuning->m_MinDist, tuning->m_MaxDist);
	}

	m_FootstepLoudness = (footstepLoudness > m_FootstepLoudness) ? footstepLoudness : m_FootstepLoudness; 
}
//-------------------------------------------------------------------------------------------------------------------
f32 audPedFootStepAudio::GetGlassiness(f32 glassLevelReduction)
{
	Vec3V pedPos = m_ParentOwningEntity->GetTransform().GetPosition();
	f32 glassiness = 0.f;
	if(!m_ParentOwningEntity->IsLocalPlayer())
	{
		// Check if the current ped is close enough to the listener.
		f32 pedSqrDistanceTolistener = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition() - pedPos)).Mag2();
		if(pedSqrDistanceTolistener < sm_DistanceThresholdToUpdateGlassiness)
		{
			glassiness =  g_GlassAudioEntity.GetGlassiness(pedPos,false,glassLevelReduction);
		}
	}
	else 
	{
		glassiness = g_GlassAudioEntity.GetGlassiness(pedPos,true,glassLevelReduction);
	}
	if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth() && m_FootstepsTuningValues)
	{
		m_WasInStealthMode = true;
		audCurve sweetenerCurve;
		sweetenerCurve.Init(m_FootstepsTuningValues->Modes[m_ModeIndex].GlassLayer.SweetenerCurve);
		if(sweetenerCurve.IsValid())
		{
			glassiness = sweetenerCurve.CalculateValue(glassiness);
			glassiness = Clamp(glassiness,0.f,1.f);
		}
	}
	return glassiness;
}
//-------------------------------------------------------------------------------------------------------------------
f32 audPedFootStepAudio::GetWetness(audFootstepEvent event )
{
#if __BANK
	if(sm_OverrideShoeWetness)
	{
		return sm_OverridedShoeWetness;
	}
#endif
	return m_FootWetness[m_ParentOwningEntity->GetFootstepHelper().GetFeetTagForFootstepEvent(event)];
}
//-------------------------------------------------------------------------------------------------------------------
f32 audPedFootStepAudio::GetShoeDirtiness()
{
	return BANK_ONLY(sm_OverrideShoeDirtiness ? sm_OverridedShoeDirtiness :)m_ShoeDirtiness;
}
//-------------------------------------------------------------------------------------------------------------------
f32 audPedFootStepAudio::CalculateShoeDirtiness()
{
	f32 desireDirtiness = GetCurrentMaterialSettings()->Dirtiness;
	if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
	{
		m_WasInStealthMode = true;
		audCurve goodSweetenerCurve,badSweetenerCurve;
		if(m_FootstepsTuningValues)
		{
			goodSweetenerCurve.Init(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].DirtLayer.SweetenerCurve);
			badSweetenerCurve.Init(m_FootstepsTuningValues->Modes[m_BadStealthIndex].DirtLayer.SweetenerCurve);
		}
		f32 goodDirtiness = 0.f;
		f32 badDirtiness = 0.f;
		if(goodSweetenerCurve.IsValid() && badSweetenerCurve.IsValid())
		{
			goodDirtiness = goodSweetenerCurve.CalculateValue(desireDirtiness);
			badDirtiness = badSweetenerCurve.CalculateValue(desireDirtiness);
			desireDirtiness = Lerp(m_PlayerStealth,badDirtiness,goodDirtiness);
		}
		else if (goodSweetenerCurve.IsValid())
		{
			desireDirtiness = goodSweetenerCurve.CalculateValue(desireDirtiness);
		}
		else if (badSweetenerCurve.IsValid())
		{
			desireDirtiness = badSweetenerCurve.CalculateValue(desireDirtiness);
		}
		desireDirtiness = Clamp(desireDirtiness,0.f,1.f);
	}
	else if (m_WasInStealthMode)
	{
		m_WasInStealthMode = false;
		m_ShoeDirtiness = desireDirtiness;
	}
	f32 ratio = fabs(desireDirtiness - m_ShoeDirtiness) + 0.0001f;
	if(m_ShoeDirtiness != desireDirtiness)
	{
		if(m_ShoeDirtiness > desireDirtiness)
		{
			m_ShoeDirtiness -= Max((ratio * sm_CleanUpRatio * (BANK_ONLY(sm_OverrideMaterialHardness ? sm_OverridedMaterialHardness:) (1.f - GetCurrentMaterialSettings()->FootstepMaterialHardness))), sm_MinCleanUpRatio);
			m_ShoeDirtiness = m_ShoeDirtiness < desireDirtiness ? desireDirtiness : m_ShoeDirtiness;
		}
		else 
		{
			m_ShoeDirtiness = desireDirtiness;
		}
	}
	return BANK_ONLY(sm_OverrideShoeDirtiness ? sm_OverridedShoeDirtiness :)m_ShoeDirtiness;
}
//-------------------------------------------------------------------------------------------------------------------
f32 audPedFootStepAudio::GetShoeCreakiness(audFootstepEvent event)
{
	f32 shoeCreakiness = (m_ShoeSettings ? m_ShoeSettings->ShoeCreakiness : 0.5f);
	shoeCreakiness = Max(shoeCreakiness,(f32)sm_ShoeWetnessToCreakiness.CalculateValue(GetWetness(event)));
	if(m_ParentOwningEntity->GetMotionData()->GetUsingStealth())
	{
		audCurve goodSweetenerCurve,badSweetenerCurve;
		if(m_FootstepsTuningValues)
		{
			goodSweetenerCurve.Init(m_FootstepsTuningValues->Modes[m_GoodStealthIndex].CreakLayer.SweetenerCurve);
			badSweetenerCurve.Init(m_FootstepsTuningValues->Modes[m_BadStealthIndex].CreakLayer.SweetenerCurve);
		}
		f32 goodCreakiness = 0.f;
		f32 badCreakiness = 0.f;
		if(goodSweetenerCurve.IsValid() && badSweetenerCurve.IsValid())
		{
			goodCreakiness = goodSweetenerCurve.CalculateValue(shoeCreakiness);
			badCreakiness = badSweetenerCurve.CalculateValue(shoeCreakiness);
			shoeCreakiness = Lerp(m_PlayerStealth,badCreakiness,goodCreakiness);
		}
		else if (goodSweetenerCurve.IsValid())
		{
			shoeCreakiness = goodSweetenerCurve.CalculateValue(shoeCreakiness);
		}
		else if (badSweetenerCurve.IsValid())
		{
			shoeCreakiness = badSweetenerCurve.CalculateValue(shoeCreakiness);
		}
		shoeCreakiness = Clamp(shoeCreakiness,0.f,1.f);
	}
	return BANK_ONLY(sm_OverrideShoeCreakiness ? sm_OverridedShoeCreakiness :)shoeCreakiness;
}
//-------------------------------------------------------------------------------------------------------------------
f32 audPedFootStepAudio::GetDragScuffProbability()
{
	if(m_FootstepsTuningValues)
	{
		return m_FootstepsTuningValues->Modes[m_ModeIndex].Shoe.DragScuffProbability;
	}
	return 0.f;
}
f32 audPedFootStepAudio::GetPlayerClumsiness()
{
	return sm_PlayerStealthToClumsiness.CalculateValue(m_PlayerStealth);
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::SetFootstepTuningMode(bool scriptOverrides,const u32 modeHash)
{
	m_ScriptOverridesMode = scriptOverrides;
	m_ScriptTuningValuesHash = modeHash;

}
//-------------------------------------------------------------------------------------------------------------------
AnimalFootstepSettings* audPedFootStepAudio::GetAnimalFootstepSettings(bool overrideAnimalRef, audMetadataRef overridenAnimalRef )
{
	AnimalFootstepSettings* footstepSettings = audNorthAudioEngine::GetObject<AnimalFootstepSettings>(ATSTRINGHASH("ANIMALS_GENERIC", 0xDFCBB62));
	AnimalFootstepReference* animalRef = audNorthAudioEngine::GetObject<AnimalFootstepReference>(overrideAnimalRef ? overridenAnimalRef : GetCurrentMaterialSettings()->AnimalReference);
	if(naVerifyf(animalRef,"Failed getting material-animal reference for material: %s.",audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(GetCurrentMaterialSettings()->NameTableOffset)))
	{
		static const u32 s_AnimalLookUpTable [NUM_PEDTYPES - 1][2] = {
			{DOG,ATSTRINGHASH("DOG", 0x8B09733B)},
			{DEER,ATSTRINGHASH("DEER", 0x31E50E10)},
			{BOAR,ATSTRINGHASH("PIG", 0xA8A2736E)},
			{COYOTE,ATSTRINGHASH("DOG", 0x8B09733B)},
			{MTLION,ATSTRINGHASH("MTLION", 0x304E3B6E)},
			{PIG,ATSTRINGHASH("PIG", 0xA8A2736E)},
			{CHIMP,ATSTRINGHASH("CHIMP", 0x73A7CC69)},
			{COW,ATSTRINGHASH("COW", 0xF059F06A)},
			{ROTTWEILER,ATSTRINGHASH("DOG", 0x8B09733B)},
			{ELK,ATSTRINGHASH("DEER", 0x31E50E10)},
			{RETRIEVER,ATSTRINGHASH("DOG", 0x8B09733B)},
			{RAT,ATSTRINGHASH("RAT", 0x22050993)},
			{BIRD,ATSTRINGHASH("BIRD", 0xA0133787)},
			{FISH,ATSTRINGHASH("FISH", 0x612920B7)},
			{SMALL_MAMMAL,ATSTRINGHASH("SMALL_MAMMAL", 0x71883107)},
			{SMALL_DOG,ATSTRINGHASH("SMALL_DOG", 0x8129041)},
			{CAT,ATSTRINGHASH("CAT", 0x4503A9A9)},
			{RABBIT,ATSTRINGHASH("RABBIT", 0xCD3B55FA)}
		};
			if(m_ParentOwningEntity->GetFootstepHelper().GetModelPhysicsParams())
			{
				const u8 pedType = m_ParentOwningEntity->GetFootstepHelper().GetModelPhysicsParams()->PedType;
				if(naVerifyf(pedType > 0, "Wrong ped type"))
				{
					for (u32 i = 0; i < animalRef->numAnimalReferences; i++)
					{
						if(animalRef->AnimalReference[i].Name == s_AnimalLookUpTable[pedType - 1][1]) // -1 to avoid Human type.
						{
							footstepSettings = audNorthAudioEngine::GetObject<AnimalFootstepSettings>(animalRef->AnimalReference[i].AnimalFootstepSettings);
							break;
						}
					}
				}
			}
	}
	return footstepSettings;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::GetVolumeAndRelFreqForFootstep(f32 &vol, f32 &freq)
{
	if(m_ParentOwningEntity->GetIsCrouching())
	{
		vol = g_audPedCrouchFeetVolOffset;
		freq = g_audPedCrouchFeetRelFreq;		
	}
	else
	{
		u32 moveState = ConvertPedMoveBlendRatioToAudMoveState(m_ParentOwningEntity->GetMotionData()->GetCurrentMbrY());
		switch(moveState)
		{
			case 2: // Run
				vol = g_audPedRunFeetVolOffset;
				freq = g_audPedRunFeetRelFreq;
				break;
			case 3: // Sprint
				vol = g_audPedSprintFeetVolOffset;
				freq = g_audPedSprintFeetRelFreq;
				break;
			default:
				vol = g_audPedWalkFeetVolOffset;
				freq = g_audPedWalkFeetRelFreq;
				break;
		}
	}

	if(m_ParentOwningEntity->IsLocalPlayer())
	{
		vol += g_audPlayerPedFeetVolOffset;
	}
	else
	{
		vol += g_audPedFeetVolOffset;
	}
	freq *= m_FootstepPitchRatio;
	if(PGTAMATERIALMGR->GetPolyFlagStairs(m_LastMaterial))
	{
		vol += g_audStairsFeetVolOffset;
	}

	// sound absorption of feet through water...
	if(m_ParentOwningEntity->m_nFlags.bPossiblyTouchesWater && m_ParentOwningEntity->m_nPhysicalFlags.bPossiblyTouchesWaterIsUpToDate)
	{
		float fWaterLevel = 0.0f;
		if (m_ParentOwningEntity->m_Buoyancy.GetStatus() == FULLY_IN_WATER)
		{
			fWaterLevel = 1.0f;
		}
		else if (m_ParentOwningEntity->m_Buoyancy.GetStatus() == PARTIALLY_IN_WATER)
		{
			if(m_ParentOwningEntity->GetRagdollState() <= RAGDOLL_STATE_ANIM)
			{
				fWaterLevel = m_ParentOwningEntity->m_Buoyancy.GetWaterLevelOnSample(0);
			}
			else
			{
				fWaterLevel = m_ParentOwningEntity->m_Buoyancy.GetWaterLevelOnSample(7);
			}
		}
		const f32 waterAttenFactor = 1.f - Min(1.f, fWaterLevel * 2.5f);
		vol += audDriverUtil::ComputeDbVolumeFromLinear(waterAttenFactor);
	}
}

//-------------------------------------------------------------------------------------------------------------------
f32 audPedFootStepAudio::ComputeSkidAmount()
{
    return 0.0f;
// 	if(!m_ParentOwningEntity->IsLocalPlayer())
// 	{
// 		// disable scuffs for non-player peds for now
// 		return 0.f;
// 	}
// 
// 	f32 deltaZ = Max(Abs(m_ParentOwningEntity->GetIkManager().GetSmoothLeftFootDeltaZ()),Abs(m_ParentOwningEntity->GetIkManager().GetSmoothRightFootDeltaZ()));
// 
// 	f32 verticalContrib = Clamp(deltaZ / 0.5f, 0.f, 1.f);
// 		
// 	CAnimPlayer *firstAnim, *secondAnim;
// 	f32 firstBlend, secondBlend;
// 	m_ParentOwningEntity->GetAnimBlender()->GetMainPlayers(AP_LOW, AP_HIGH, firstAnim, firstBlend, secondAnim, secondBlend);
// 
// 	f32 animContrib = 0.f;
// 
// 	if(firstAnim)
// 	{
// 		fwMvAnimId animId = firstAnim->GetAnimId();
// 
// 		if(animId == ANIM_MOVE_RUN_TURN_L
// 			|| animId ==  ANIM_MOVE_RUN_TURN_R
// 			|| animId == ANIM_MOVE_RUN_TURN_L2
// 			|| animId == ANIM_MOVE_RUN_TURN_R2
// 			|| animId == ANIM_MOVE_RUN_TURN_180L
// 			|| animId == ANIM_MOVE_RUN_TURN_180R)
// 		{
// 			animContrib += firstBlend * 0.25f;
// 		}
// 		else if(animId == ANIM_MOVE_SPRINT_TURN_L
// 			|| animId == ANIM_MOVE_SPRINT_TURN_R
// 			|| animId == ANIM_MOVE_SPRINT_TURN_180L
// 			|| animId == ANIM_MOVE_SPRINT_TURN_180R)
// 		{
// 			animContrib += firstBlend * 0.5f;
// 		}
// 	}
// 
// 	animContrib = Clamp(animContrib, 0.0f, 1.0f);
// 
// 	return (animContrib * 0.5f) + (verticalContrib * 0.5f);
}
//-------------------------------------------------------------------------------------------------------------------
u32 audPedFootStepAudio::GetAnimalFootstepSoundRef(const AnimalFootstepSettings *sounds, const audFootstepEvent event) const
{
	// divide by 2 to get event typ e index - we dont care if its left/right
	u32 index = (u32)event / 2;
	if(event == AUD_FOOTSTEP_JUMP_LAND_L || event == AUD_FOOTSTEP_JUMP_LAND_R)
	{
		index = AUD_FOOTSTEP_GO_JUMP_LAND;
	}
	if(event == AUD_FOOTSTEP_RUN_L || event == AUD_FOOTSTEP_RUN_R)
	{
		index = AUD_FOOTSTEP_GO_RUN; // play the run sound
	}
	if(event == AUD_FOOTSTEP_SPRINT_L || event == AUD_FOOTSTEP_SPRINT_R)
	{
		index = AUD_FOOTSTEP_GO_SPRINT;
	}
	if(event == AUD_FOOTSTEP_SOFT_L || event == AUD_FOOTSTEP_SOFT_R)
	{
		index = AUD_FOOTSTEP_GO_WALK;
	}
	switch(index)
	{	
	case AUD_FOOTSTEP_GO_WALK:
		return sounds->WalkAndTrot;
	case AUD_FOOTSTEP_GO_SCUFFHARD:
		return sounds->Scuff;
	case AUD_FOOTSTEP_GO_SCUFF:
		return sounds->Scuff;
	case AUD_FOOTSTEP_GO_JUMP_LAND:
		return sounds->Jump;
	case AUD_FOOTSTEP_GO_RUN:
	case AUD_FOOTSTEP_GO_SPRINT:
		return sounds->Gallop1;
	default:
		return sounds->WalkAndTrot;
	}
}
//-------------------------------------------------------------------------------------------------------------------
u32 audPedFootStepAudio::GetShoeSound(u32 index, OldShoeSettings *sounds) const
{
	switch(index)
	{	
	case AUD_FOOTSTEP_GO_WALK:
		return sounds->Walk;
	case AUD_FOOTSTEP_GO_SCUFFHARD:
		return sounds->ScuffHard;
	case AUD_FOOTSTEP_GO_SCUFF:
		return sounds->Scuff;
	case AUD_FOOTSTEP_GO_LADDER:
		return sounds->Ladder;
	case AUD_FOOTSTEP_GO_JUMP_LAND:
		return sounds->Jump;
	case AUD_FOOTSTEP_GO_RUN:
		return sounds->Run;
	case AUD_FOOTSTEP_GO_SPRINT:
		return sounds->Sprint;
	default:
		return sounds->Walk;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddFootstepEvent(audFootstepEvent stepType)
{
	if (!m_FootstepEventsEnabled)
	{
		return;
	}

	if(!m_FootstepEvents.IsFull())
	{
		m_FootstepEvents.Push(stepType);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketFootStepSound>(
				CPacketFootStepSound((u8)stepType),
				m_ParentOwningEntity);
		}
#endif // GTA_REPLAY
	}
#if __BANK
	if(s_TriggerBush)
	{
		AddBushEvent(stepType);
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddClothEvent(audFootstepEvent stepType)
{
	if (!m_ClothEventsEnabled)
	{
		return;
	}

	if(!m_ClothsEvents.IsFull())
	{
		m_ClothsEvents.Push(stepType);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketPedClothSound>(
				CPacketPedClothSound((u8)stepType),
				m_ParentOwningEntity);
		}
#endif // GTA_REPLAY
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddBushEvent(audFootstepEvent stepType, u32 typeNameHash)
{
	if(!m_FoliageEvents.IsFull())
	{
		foliageEvent event;
		event.stepType = stepType;
		event.typeNameHash = typeNameHash;
		m_FoliageEvents.Push(event);

//#if GTA_REPLAY
//		if(CReplayMgr::ShouldRecord())
//		{
//			CReplayMgr::RecordFx<CPacketPedBushSound>(
//				CPacketPedBushSound((u8)stepType),
//				m_ParentOwningEntity);
//		}
//#endif // GTA_REPLAY
	}

}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddSlopeDebrisEvent(audFootstepEvent stepType,Vec3V_In downSlopeDirection,f32 slopeAngle)
{
	//// First of all get the material settigns to pick the proper debris sound. 
	//CollisionMaterialSettings *groundMaterial = GetCurrentMaterialSettings();
	//if(groundMaterial && groundMaterial->DebrisOnSlope != g_NullSoundHash)
	//{
		downSlopEvent slopeEvent;
		slopeEvent.stepType = stepType;
		slopeEvent.downSlopeDirection = downSlopeDirection;
		slopeEvent.slopeAngle = RtoD * fabs(slopeAngle);
		slopeEvent.debrisSound = NULL;
		slopeEvent.ready = true;
		BANK_ONLY(bool added = false);
		for (u32 i = 0; i < m_SlopeDebrisEvents.GetMaxCount(); i++)
		{
			if(!m_SlopeDebrisEvents[i].ready && !m_SlopeDebrisEvents[i].debrisSound)
			{
				m_SlopeDebrisEvents[i] = slopeEvent;
				BANK_ONLY(added = true);
				break;
			}
		}
#if __BANK 
		if(!added)
		{
			naWarningf("Run out of slots to trigger slope debris even, please contact the audio team.");
		}
#endif
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketPedSlopeDebrisSound>(
				CPacketPedSlopeDebrisSound((u8)stepType, downSlopeDirection, DtoR * slopeAngle),
				m_ParentOwningEntity);
		}
#endif // GTA_REPLAY
	//}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddPetrolCanEvent(audFootstepEvent stepType,bool replay)
{
    if(!audVerifyf(m_ParentOwningEntity, "Failed to find audPedFootStepAudio parent entity"))
    {
        return;
    }

	const CPedWeaponManager* weaponManager = m_ParentOwningEntity->GetWeaponManager();
	if(!m_PetrolCanEvents.IsFull() && weaponManager)
	{
		const CWeaponInfo* weaponInfo = weaponManager->GetEquippedWeaponInfo();
		const bool bIsPetrolCan = weaponInfo && weaponInfo->GetGroup() == WEAPONGROUP_PETROLCAN;
		if (bIsPetrolCan || replay)
		{
			m_PetrolCanEvents.Push(stepType);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketPedPetrolCanSound>(
					CPacketPedPetrolCanSound((u8)stepType),
					m_ParentOwningEntity);
			}
#endif // GTA_REPLAY
		}
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddMolotovEvent(audFootstepEvent stepType,bool replay)
{
	const CPedWeaponManager* weaponManager = m_ParentOwningEntity->GetWeaponManager();
	if(!m_MolotovEvents.IsFull() && weaponManager)
	{
		const CWeapon* weapon = weaponManager->GetEquippedWeapon();
		if ((weapon && weapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_MOLOTOV", 0x24B17070)) || replay) 
		{
			m_MolotovEvents.Push(stepType);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketPedMolotovSound>(
					CPacketPedMolotovSound((u8)stepType),
					m_ParentOwningEntity);
			}
#endif // GTA_REPLAY
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::AddScriptSweetenerEvent(audFootstepEvent stepType)
{
	if(!m_ScriptSweetenerEvent.IsFull())
	{
		m_ScriptSweetenerEvent.Push(stepType);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketPedScriptSweetenerSound>(
				CPacketPedScriptSweetenerSound((u8)stepType),
				m_ParentOwningEntity);
		}
#endif // GTA_REPLAY
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PreUpdate()
{

	if(!m_ParentOwningEntity)
	{
		//Just in case these sounds have been triggered, let's shut them off before returning early.
		StopWindClothSound();
		return;
	}
	if(m_ParentOwningEntity->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ))  
	{
		if(m_HasToTriggerSuperJumpLaunch && m_ParentOwningEntity->GetPlayerInfo() && m_ParentOwningEntity->GetPlayerInfo()->GetPlayerResetFlags().IsFlagSet(CPlayerResetFlags::PRF_SUPER_JUMP_ON))
		{
			audSoundInitParams initParams; 
			initParams.EnvironmentGroup = m_ParentOwningEntity->GetAudioEnvironmentGroup();
			initParams.TrackEntityPosition = true; 
			m_Parent->CreateAndPlaySound(ATSTRINGHASH("SUPER_JUMP_MASTER", 0x8A1DDBA1),&initParams);
			m_HasToTriggerSuperJumpLaunch = false;
		}
	}
	else 
	{
		m_HasToTriggerSuperJumpLaunch = true;
	}
	CollisionMaterialSettings * material = NULL;
	m_MaterialOverriddenThisFrame = false;
	if(m_ParentOwningEntity->GetPedResetFlag(CPED_RESET_FLAG_IsVaulting))
	{
		const CTaskVault* climbTask = static_cast<const CTaskVault*>(m_ParentOwningEntity->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VAULT));
		if(climbTask && climbTask->GetState() == CTaskVault::State_Slide)
		{
			m_BonnetSlideRecently = true;
		}
		if(!m_BonnetSlideRecently && climbTask && (climbTask->GetState() == CTaskVault::State_ClamberStand || climbTask->GetState() == CTaskVault::State_Climb || climbTask->GetState() == CTaskVault::State_ClamberVault))
		{
			if(m_ParentOwningEntity->GetClimbPhysical(true) && m_ParentOwningEntity->GetClimbPhysical(true)->GetIsTypeVehicle() && static_cast<CVehicle*>(m_ParentOwningEntity->GetClimbPhysical(true))->GetVehicleType() == VEHICLE_TYPE_CAR)
			{
				material = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_CAR_METAL", 0xA0231769));
				if(material)
				{
					SetStandingMaterial(material,g_CollisionAudioEntity.GetCarBodyMaterialId());
					m_MaterialOverriddenThisFrame = true;
				}
			}
			else
			{
				material = climbTask->GetHandHold().GetFrontSurfaceMaterial();
				if(material)
				{	
					SetStandingMaterial(material,climbTask->GetHandHold().GetFrontSurfaceMaterialId());
					m_MaterialOverriddenThisFrame = true;
				}
			}
		}
	}
	else 
	{
		m_BonnetSlideRecently = false;
	}
	m_IsWearingElfHat = false;
	for(int index = 0; index < MAX_PROPS_PER_PED; index++)
	{
		CPedPropData::SinglePropData &propData = m_ParentOwningEntity->GetPedDrawHandler().GetPropData().GetPedPropData(index);
		if( propData.m_anchorID == ANCHOR_HEAD && (propData.m_propModelHash == 2258039690 || propData.m_propModelHash == 579227166 || propData.m_propModelHash == 1319284495))
		{
			m_IsWearingElfHat = true;
			break;
		}
	}	

	m_IsWearingScubaMask = false;

	const s32 eyePropIdx = CPedPropsMgr::GetPedPropIdx(m_ParentOwningEntity, ANCHOR_EYES);

	if (eyePropIdx != -1)
	{
		const CTaskMotionSwimming::Tunables::ScubaMaskProp* pScubaProp = CTaskMotionSwimming::FindScubaMaskPropForPed(*m_ParentOwningEntity);
			
		if (pScubaProp && pScubaProp->m_Index == eyePropIdx)
		{
			m_IsWearingScubaMask = true;
		}
	}
	UpdateWetness();
	UpdateFootstetVolOffsetOverTime();
	if(m_ParentOwningEntity->GetIsInVehicle() && m_ParentOwningEntity->GetMyVehicle()->GetVehicleAudioEntity()->GetMinOpenness() <= 0.9f)
	{
		StopWindClothSound();
	}
	if(m_ShouldTriggerSplash)
	{
		TriggerFallInWaterSplash(true);
		m_ShouldTriggerSplash = false;
	}
#if __BANK
	if(sm_WetFeetInfo)
	{
		char txt[256];
		formatf(txt,"Left foot water state : %d",m_FootWaterInfo[LeftFoot]);
		grcDebugDraw::AddDebugOutput(Color_white,txt);
		formatf(txt,"Right foot water state : %d",m_FootWaterInfo[RightFoot]);
		grcDebugDraw::AddDebugOutput(Color_white,txt);
	}
	if(g_AudioEngine.GetRemoteControl().IsPresent())
	{
		m_ShoeSettings = const_cast<ShoeAudioSettings*>(GetShoeSettings());
		m_UpperClothSettings = const_cast<ClothAudioSettings*>(GetUpperBodyClothSounds());
		m_LowerClothSettings = const_cast<ClothAudioSettings*>(GetLowerBodyClothSounds());
		for(u8 i = 0; i < MAX_EXTRA_CLOTHING; i ++)
		{
			m_ExtraClothingSettings[i] = const_cast<ClothAudioSettings*>(GetExtraClothSounds(i));
		}
	}
#endif
}


//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ProcessEvents()
{
	f32 angularVelocity = fabs(m_ParentOwningEntity->GetAngVelocity().Mag());

	bool bFirstPerson = audNorthAudioEngine::IsAFirstPersonCameraActive(m_ParentOwningEntity, false, false, true, true);
	if(bFirstPerson && !m_ParentOwningEntity->GetIsInWater() && !m_ParentOwningEntity->GetUsingRagdoll())
	{
		u32 currentTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		u32 timeElapsed = currentTime - m_LastTimePlayedFPShuffle;
		bool playEvent = false;
		if(angularVelocity > 1.0f && timeElapsed > 600)
		{
			playEvent = true;
		}
		else if(angularVelocity > 0.6f && timeElapsed > 1000)
		{
			playEvent = true;
		}
		else if(angularVelocity > 0.2f && timeElapsed > 1500)
		{
			playEvent = true;
		}
		if(playEvent)
		{
			m_LastTimePlayedFPShuffle = currentTime;
			AddClothEvent(m_LastFPFootstepEvent);
			if(m_LastFPFootstepEvent == AUD_FOOTSTEP_WALK_L)
			{
				m_LastFPFootstepEvent = AUD_FOOTSTEP_WALK_R;
			}
			//Displayf("Angular veocity %f - velocity %f", angularVelocity, m_ParentOwningEntity->GetVelocity().Mag());
		}
	}

	ProcessFootsteps();
	ProcessBushEvents(angularVelocity);
	ProcessClothEvents(angularVelocity);
	ProcessPetrolCanEvents();
	ProcessScriptSweetenerEvents();
	ProcessMolotovEvents();
	ProcessWaterEvents();
	ProcessSlopeDebrisEvents();
	ResetEvents(m_ParentOwningEntity->GetFootstepHelper().IsAnAnimal());
	m_LastPedAngVel = angularVelocity;
	if(!m_FeetInWaterUpdatedThisFrame)
	{
		m_FeetInWater = false;
	}
	m_FeetInWaterUpdatedThisFrame = false;
	if(m_WaveImpactSound && (m_ShouldStopWaveImpact || !m_FeetInWater))
	{
		m_WaveImpactSound->StopAndForget();
	}
	m_ForceWetFeetReset = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::SetStandingMaterial(CollisionMaterialSettings *material, phMaterialMgr::Id matId)
{
	m_LastMaterial = matId; 
	m_CurrentMaterialSettings = material;
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::SetStandingMaterial(phMaterialMgr::Id materialId, CEntity *entity, u32 component)
{
	//m_OverrideFootstepsWithLongGrass = false;
	//if(g_procInfo.CreatesPlants(PGTAMATERIALMGR->UnpackProcId(materialId)))
	//{
	//	m_OverrideFootstepsWithLongGrass = true;
	//}

	// check to see if this is a different surface
	if(HasStandingMaterialChanged(materialId, entity, component))
	{
		ChangeStandingMaterial(materialId, entity, component);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::ChangeStandingMaterial(phMaterialMgr::Id materialId, CEntity *entity, u32 component)
{
	// Caller is supposed to check:
	Assert(HasStandingMaterialChanged(materialId, entity, component));

	if(entity && entity->GetIsTypeVehicle() && ((CVehicle*)entity)->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		if (((CVehicle*)entity)->GetBaseModelInfo() && ((CVehicle*)entity)->GetBaseModelInfo()->GetModelNameHash() == ATSTRINGHASH("TUG", 0x82CAC433))
		{
			SetCurrentMaterialSettings(audNorthAudioEngine::GetObject<CollisionMaterialSettings>(ATSTRINGHASH("AM_BASE_METAL_SOLID_LARGE", 0x4086D1E8)));
		}
		else
		{
			SetCurrentMaterialSettings(g_BoatFootstepMaterial);
		}
		m_LastMaterial = materialId;
		m_LastStandingEntity = entity;
		m_LastComponent = component;
		return;
	}

	//This includes a call to GetMaterialOverride so no need to worry our pretty little heads about calling it seperately
	CollisionMaterialSettings *material = g_CollisionAudioEntity.GetMaterialSettingsFromMatId(entity, materialId, component);
	
	// fall back to default if we couldnt find anything...
	if(!material)
	{
		SetCurrentMaterialSettings(g_audCollisionMaterials[0]);
		m_LastMaterial = 0;
		m_LastComponent = 0;
		m_LastStandingEntity = NULL;
	}
	else
	{
		SetCurrentMaterialSettings(material);
		m_LastMaterial = materialId;
		m_LastStandingEntity = entity;
		m_LastComponent = component;
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::GetIsStandingOnVehicle()
{
	if(m_LastStandingEntity && m_LastStandingEntity->GetIsTypeVehicle())
	{
		return true;
	}
	return false;
}

//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::GetFlapsInWind()
{
	return AUD_GET_TRISTATE_VALUE(m_UpperClothSettings->Flags, FLAG_ID_CLOTHAUDIOSETTINGS_FLAPSINWIND) == AUD_TRISTATE_TRUE ||
		AUD_GET_TRISTATE_VALUE(m_LowerClothSettings->Flags, FLAG_ID_CLOTHAUDIOSETTINGS_FLAPSINWIND) == AUD_TRISTATE_TRUE;
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::SetStandingMaterial(CollisionMaterialSettings * material, CEntity *entity, u32 component)
{
	if(m_LastStandingEntity)
	{
		m_LastStandingEntity = NULL;
	}
	// fall back to default if we couldnt find anything...
	if(!material)
	{
		SetCurrentMaterialSettings(g_audCollisionMaterials[0]);
		m_LastMaterial = 0;
		m_LastComponent = 0;
	}
	else
	{
		SetCurrentMaterialSettings(audCollisionAudioEntity::GetMaterialOverride(entity, material, component));
		m_LastMaterial = 0;
		m_LastStandingEntity = entity;
		m_LastComponent = component;
	}
}





//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::DoesPedHavePockets() const
{
	if( (m_UpperClothSettings && AUD_GET_TRISTATE_VALUE(m_UpperClothSettings->Flags, FLAG_ID_CLOTHAUDIOSETTINGS_HAVEPOCKETS)==AUD_TRISTATE_TRUE)
		||
		(m_LowerClothSettings && AUD_GET_TRISTATE_VALUE(m_LowerClothSettings->Flags, FLAG_ID_CLOTHAUDIOSETTINGS_HAVEPOCKETS)==AUD_TRISTATE_TRUE)
	  )
	{
		return true;
	}
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayPedOnPedBumpSounds(CPed* otherPed, f32 moveBlendRatio)
{
	u32 moveState = ConvertPedMoveBlendRatioToAudMoveState(moveBlendRatio);

	naCErrorf(otherPed, "Tried to play ped on ped bump sounds but there's no other ped");
	if (otherPed)
	{
		// Get upper body clothing sounds for each ped, and play those and a ped impact sound
		const ClothAudioSettings* otherUpperBodyClothingSounds = otherPed->GetPedAudioEntity()->GetFootStepAudio().GetCachedUpperBodyClothSounds();
		if(m_UpperClothSettings != NULL && otherUpperBodyClothingSounds != NULL)
		{
			u32 upperBodyClothingImpact = m_UpperClothSettings->ImpactSound;
			u32 otherUpperBodyClothingImpact = otherUpperBodyClothingSounds->ImpactSound;
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
			initParams.TrackEntityPosition = true;
			initParams.Volume = audEngineUtil::GetRandomNumberInRange(g_PedOnPedMinVolume, g_PedOnPedMaxVolume) + g_MoveStateClothesVolumeOffset[moveState];
			// if they're the same sound, don't risk the chance of getting the same variation, and hence phasing - only play one, but up the volume
			if (upperBodyClothingImpact==otherUpperBodyClothingImpact)
			{
				// play just one, but louder
				initParams.Volume += 3.0f;
				m_Parent->CreateAndPlaySound(upperBodyClothingImpact, &initParams);
			}
			else
			{
				m_Parent->CreateAndPlaySound(upperBodyClothingImpact, &initParams);
				// play the second one as well
				initParams.Volume = audEngineUtil::GetRandomNumberInRange(g_PedOnPedMinVolume, g_PedOnPedMaxVolume) + g_MoveStateClothesVolumeOffset[moveState];
				m_Parent->CreateAndPlaySound(otherUpperBodyClothingImpact, &initParams);
			}
			// and always play the collision
			initParams.Volume = g_MoveStateBumpVolumeOffset[moveState];
			m_Parent->CreateAndPlaySound(ATSTRINGHASH("PED_ON_PED_BUMP", 0x529FBA01), &initParams);
		}
	}
}


//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayCoverHitWallSounds(u32 hash)
{
	// play upper body clothing, some weapon handling, and a custom bump

	// Adjust the overall volume by the move state, and some variance
	static f32 runVolume = -3.0f;
	static f32 walkVolume = -9.0f;
	f32 volumeOffset = 0.0f;
	switch (ConvertPedMoveBlendRatioToAudMoveState(m_MoveBlendRatioWhenGoingIntoCover))
	{
	case 3: // Sprint
		volumeOffset = 0.0f;
		break;
	case 2: // Run
		volumeOffset = runVolume;
		break;
	default:
		volumeOffset = walkVolume;
		break;
	}
	// add some timing variation in
	static s32 minPredelay = 0;
	static s32 maxPredelay = 100;
	s32 weaponPredelay = audEngineUtil::GetRandomNumberInRange(minPredelay, maxPredelay);
	s32 clothingPredelay = audEngineUtil::GetRandomNumberInRange(minPredelay, maxPredelay);

	// Get upper body clothing sounds
	u32 upperBodyClothImpact = 0;
	if (m_UpperClothSettings)
	{
		upperBodyClothImpact = m_UpperClothSettings->ImpactSound;
	}
	audSoundInitParams initParams;
	initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
	initParams.TrackEntityPosition = true;
	initParams.Volume = g_CoverHitWallClothingVolume + volumeOffset + audEngineUtil::GetRandomNumberInRange(-1.0f, 1.0f);
	initParams.Predelay = clothingPredelay;
	m_Parent->CreateAndPlaySound(upperBodyClothImpact, &initParams);
	REPLAY_ONLY(CReplayMgr::ReplayRecordSound(upperBodyClothImpact, &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
	// and play a custom collision
	initParams.Volume = volumeOffset + audEngineUtil::GetRandomNumberInRange(-1.0f, 1.0f);
	initParams.Predelay = 0;
	m_Parent->CreateAndPlaySound(ATSTRINGHASH("ANIM_COVER_HIT_WALL_IMPACT", 0x799403C2), &initParams);
	REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("ANIM_COVER_HIT_WALL_IMPACT", 0x799403C2), &initParams,m_ParentOwningEntity, m_ParentOwningEntity));
	// Don't do the weapon for softer ones
	if (hash!= ATSTRINGHASH("ANIM_COVER_HIT_WALL_SOFTER", 0x23FC32D6))
	{
		// Get weapon rattle sounds
		initParams.Volume = g_CoverHitWallWeaponVolume + volumeOffset + audEngineUtil::GetRandomNumberInRange(-1.0f, 1.0f);
		initParams.Predelay = weaponPredelay;
		const WeaponSettings *weaponSettings = NULL;
		if(m_ParentOwningEntity)
		{
			const CWeapon *weapon = m_ParentOwningEntity->GetWeaponManager() ? m_ParentOwningEntity->GetWeaponManager()->GetEquippedWeapon() : NULL;
			if(weapon)
			{
				weaponSettings = weapon->GetAudioComponent().GetWeaponSettings();
			}
		}
		if(weaponSettings)
		{
			m_Parent->CreateAndPlaySound(weaponSettings->PutDownSound, &initParams);
			m_Parent->CreateAndPlaySound(weaponSettings->RattleSound, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(weaponSettings->PutDownSound, &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(weaponSettings->RattleSound,  &initParams, m_ParentOwningEntity,m_ParentOwningEntity));
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::LoadFootstepEventTuning()
{
	// TODO: Maybe get this through CDataFileMgr?
	PARSER.LoadObject("common:/data/ai/noisetuning", "meta", sm_FootstepEventTuning);
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::LeftLadderFoot(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->GetPedAudioEntity()->GetFootStepAudio().PlayFootstepEvent(AUD_FOOTSTEP_LADDER_L);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketFootStepSound>(CPacketFootStepSound((u8)AUD_FOOTSTEP_LADDER_L), ((CPed*)context));
	}
#endif // GTA_REPLAY

}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::LeftLadderFootUp(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->GetPedAudioEntity()->GetFootStepAudio().PlayFootstepEvent(AUD_FOOTSTEP_LADDER_L_UP);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketFootStepSound>(CPacketFootStepSound((u8)AUD_FOOTSTEP_LADDER_L_UP), ((CPed*)context));
	}
#endif // GTA_REPLAY
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::RightLadderFoot(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->GetPedAudioEntity()->GetFootStepAudio().PlayFootstepEvent(AUD_FOOTSTEP_LADDER_R);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketFootStepSound>(CPacketFootStepSound((u8)AUD_FOOTSTEP_LADDER_R), ((CPed*)context));
	}
#endif // GTA_REPLAY
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::RightLadderFootUp(u32 UNUSED_PARAM(hash), void *context)
{
	((CPed*)context)->GetPedAudioEntity()->GetFootStepAudio().PlayFootstepEvent(AUD_FOOTSTEP_LADDER_R_UP);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketFootStepSound>(CPacketFootStepSound((u8)AUD_FOOTSTEP_LADDER_R_UP), ((CPed*)context));
	}
#endif // GTA_REPLAY
}

//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::LeftLadderHand(u32 UNUSED_PARAM(hash), void *context)
{
	// TODO : Add different ladder types
	audSoundSet ladderSoundSetRef;
	ladderSoundSetRef.Init(ATSTRINGHASH("METAL_SOLID_LADDER", 0xEBECD911));
	if(ladderSoundSetRef.IsInitialised())
	{
		audMetadataRef soundRef = g_NullSoundRef;
		soundRef = ladderSoundSetRef.Find(ATSTRINGHASH("HAND", 0x7AE9FCFC));
		if(soundRef != g_NullSoundRef)
		{
			Vec3V pos;
			((CPed*)context)->GetFootstepHelper().GetPositionForPedSound(AUD_FOOTSTEP_LADDER_HAND_L, pos);
			audSoundInitParams initParams; 
			initParams.Position = VEC3V_TO_VECTOR3(pos);
			initParams.EnvironmentGroup = ((CPed*)context)->GetPedAudioEntity()->GetEnvironmentGroup(true);
			((CPed*)context)->GetPedAudioEntity()->CreateAndPlaySound(soundRef,&initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ladderSoundSetRef.GetNameHash(), ATSTRINGHASH("HAND", 0x7AE9FCFC), &initParams, ((CPed*)context)));
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::RightLadderHand(u32 UNUSED_PARAM(hash), void *context)
{
	// TODO : Add different ladder types
	audSoundSet ladderSoundSetRef;
	ladderSoundSetRef.Init(ATSTRINGHASH("METAL_SOLID_LADDER", 0xEBECD911));
	if(ladderSoundSetRef.IsInitialised())
	{
		audMetadataRef soundRef = g_NullSoundRef;
		soundRef = ladderSoundSetRef.Find(ATSTRINGHASH("HAND", 0x7AE9FCFC));
		if(soundRef != g_NullSoundRef)
		{
			Vec3V pos;
			((CPed*)context)->GetFootstepHelper().GetPositionForPedSound(AUD_FOOTSTEP_LADDER_HAND_R, pos);
			audSoundInitParams initParams; 
			initParams.Position = VEC3V_TO_VECTOR3(pos);
			initParams.EnvironmentGroup = ((CPed*)context)->GetPedAudioEntity()->GetEnvironmentGroup(true);
			((CPed*)context)->GetPedAudioEntity()->CreateAndPlaySound(soundRef,&initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ladderSoundSetRef.GetNameHash(), ATSTRINGHASH("HAND", 0x7AE9FCFC), &initParams, ((CPed*)context)));
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::FootStepHandler(u32 hash, void * context)
{
	audFootstepEvent event;
	if(GetFootStepEventFromHash(hash,event))
	{
		if(!((CPed*)context)->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ))  
		{
			((CPed*)context)->GetPedAudioEntity()->GetFootStepAudio().AddFootstepEvent(event);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
bool audPedFootStepAudio::GetFootStepEventFromHash(const u32 &hash,audFootstepEvent &event)
{
	static const u32 sm_FootstepEventLookUpTabe [12][2] = {
		{ATSTRINGHASH("LEFT_FOOT_WALK", 0x31A570AA),AUD_FOOTSTEP_WALK_L},
		{ATSTRINGHASH("RIGHT_FOOT_WALK", 0x6F2142A0),AUD_FOOTSTEP_WALK_R},
		{ATSTRINGHASH("LEFT_FOOT_SCUFF" , 0xC392AB61),AUD_FOOTSTEP_SCUFF_L},
		{ATSTRINGHASH("RIGHT_FOOT_SCUFF", 0x5F1A0D8B),AUD_FOOTSTEP_SCUFF_R},
		{ATSTRINGHASH("LEFT_FOOT_JUMP" , 0x434CBD8D),AUD_FOOTSTEP_JUMP_LAND_L},
		{ATSTRINGHASH("RIGHT_FOOT_JUMP", 0x92F703D9),AUD_FOOTSTEP_JUMP_LAND_R},
		{ATSTRINGHASH("LEFT_FOOT_RUN" , 0x3DC0D219),AUD_FOOTSTEP_RUN_L},
		{ATSTRINGHASH("RIGHT_FOOT_RUN", 0xF37DAB85),AUD_FOOTSTEP_RUN_R},
		{ATSTRINGHASH("LEFT_FOOT_SPRINT" , 0x6DE979FC),AUD_FOOTSTEP_SPRINT_L},
		{ATSTRINGHASH("RIGHT_FOOT_SPRINT", 0xFD11320D),AUD_FOOTSTEP_SPRINT_R},
		{ATSTRINGHASH("LEFT_FOOT_SOFT" , 0x59290187),AUD_FOOTSTEP_SOFT_L},
		{ATSTRINGHASH("RIGHT_FOOT_SOFT", 0x9FE4C053),AUD_FOOTSTEP_SOFT_R}};
	for (u32 i = 0; i < 12; i++)
	{
		if(sm_FootstepEventLookUpTabe[i][0] == hash) 
		{
			event = (audFootstepEvent)sm_FootstepEventLookUpTabe[i][1];
			return true;
		}
	}
	naAssertf(false,"There is no footstep event associated with the current anim tag %u",hash);
	return false;
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::StartLadderSlide()
{
	if(!m_LadderSlideSound)
	{
		// TODO : Add different ladder types
		audSoundSet ladderSoundSetRef;
		ladderSoundSetRef.Init(ATSTRINGHASH("METAL_SOLID_LADDER", 0xEBECD911));
		if(ladderSoundSetRef.IsInitialised())
		{
			audMetadataRef soundRef = g_NullSoundRef;
			soundRef = ladderSoundSetRef.Find(ATSTRINGHASH("SLIDE_DOWN", 0xC2236009));
			if(soundRef != g_NullSoundRef)
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = m_Parent->GetEnvironmentGroup(true);
				m_Parent->CreateAndPlaySound_Persistent(soundRef, &m_LadderSlideSound, &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(ladderSoundSetRef.GetNameHash(), ATSTRINGHASH("SLIDE_DOWN", 0xC2236009), &initParams, m_LadderSlideSound, m_Parent->GetOwningEntity(), eNoGlobalSoundEntity);)
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::StopLadderSlide()
{
	if (m_LadderSlideSound)
	{
		m_LadderSlideSound->StopAndForget();
		m_ParentOwningEntity->GetFootstepHelper().SetSlidingDownALadder();
	}
	// Set air flags so we trigger jump land sounds.
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::WallClimbHandler(u32 hash, void *context)
{
	CPed *ped = (CPed*)context;

	const u32 events[] = {
		ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_LAUNCH", 0xDD79F372), 
		ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_FOOT", 0x6E1583C3), 
		ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_KNEE", 0xF68FC269),
		ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_SCRAPE", 0xE3E92C86),
		ATSTRINGHASH("ANIM_WALL_OR_FENCE_CLIMB_HAND", 0x63B1018F)
	};

	const u32 soundHashes[5] =
	{
		ATSTRINGHASH("climbLaunch", 0xB034BD1), 
		ATSTRINGHASH("climbFoot", 0x9A390412),
		ATSTRINGHASH("climbKnee", 0x52D81FD),
		ATSTRINGHASH("climbScrape", 0x890EF303),
		ATSTRINGHASH("climbHand", 0x643235D3)
	};

	const u32 numEvents = sizeof(events)/sizeof(events[0]);
	u32 soundIndex = 0;
	FeetTags soundPosition;
	for(; soundIndex < numEvents; soundIndex++)
	{
		if(hash == events[soundIndex])
		{
			break;
		}
	}
	if(soundIndex >= numEvents)
	{
		naErrorf("Invalid climb audio event");
		return;
	}

	if(soundIndex==numEvents-1)
	{
		soundPosition = LeftHand;
	}
	else
	{
		soundPosition = RightHand;
	}

	ClimbingAudioSettings * climbingSettings = audNorthAudioEngine::GetObject<ClimbingAudioSettings>(ped->GetPedAudioEntity()->GetFootStepAudio().GetCurrentMaterialSettings()->ClimbSettings);
	if(climbingSettings)
	{
		audSoundInitParams initParams;
		Vec3V pos;
		initParams.EnvironmentGroup = ped->GetPedAudioEntity()->GetEnvironmentGroup(true);
		ped->GetFootstepHelper().GetPositionForPedSound(soundPosition, pos);
		initParams.Position = VEC3V_TO_VECTOR3(pos);
		ped->GetPedAudioEntity()->CreateAndPlaySound(*(u32 *)(climbingSettings->GetFieldPtr(soundHashes[soundIndex])), &initParams);
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(*(u32 *)(climbingSettings->GetFieldPtr(soundHashes[soundIndex])), &initParams, ped));
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::BonnetSlideHandler(u32 /*hash*/, void *context)
{
	CPed *ped = (CPed*)context;
	if(ped)
	{
		CollisionMaterialSettings * material = NULL;
		const CTaskVault* slide = static_cast<const CTaskVault*>(ped->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_VAULT));
		if(slide)
		{
			material = slide->GetHandHold().GetFrontSurfaceMaterial();
			if(material)
			{	
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = ped->GetPedAudioEntity()->GetEnvironmentGroup(true);
				initParams.Tracker = ped->GetPlaceableTracker();
				ped->GetPedAudioEntity()->CreateAndPlaySound(material->PedSlideSound, &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(material->PedSlideSound, &initParams, ped,ped));
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PedRollHandler(u32 /*hash*/, void *context) 
{
	CPed *ped = (CPed*)context;
	if(ped && ped->GetPedAudioEntity())
	{
		u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		if(now - ped->GetPedAudioEntity()->GetFootStepAudio().m_LastCombatRollTime < 500)
		{
			return;
		}
		ped->GetPedAudioEntity()->GetFootStepAudio().m_LastCombatRollTime = now;
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = ped->GetPedAudioEntity()->GetEnvironmentGroup(true);
		initParams.Tracker = ped->GetPlaceableTracker();
		if(ped->GetMotionData()->GetUsingStealth())
		{
			initParams.Volume = sm_StealthPedRollVolOffset;
			initParams.LPFCutoff = sm_StealthPedRollLPFCutoff;
			initParams.AttackTime = sm_StealthPedRollAttackTime;
		}

		CollisionMaterialSettings * material = ped->GetPedAudioEntity()->GetFootStepAudio().GetCurrentMaterialSettings();
		if(material)
		{	
			ped->GetPedAudioEntity()->CreateAndPlaySound(material->PedRollSound, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(material->PedRollSound, &initParams, ped,ped));
		}
		const ClothAudioSettings *upperCloth = ped->GetPedAudioEntity()->GetFootStepAudio().GetCachedUpperBodyClothSounds();
		if(upperCloth)
		{
			ped->GetPedAudioEntity()->CreateAndPlaySound(upperCloth->PedRollSound, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(upperCloth->PedRollSound, &initParams, ped,ped));
		}
		const ClothAudioSettings *lowerCloth = ped->GetPedAudioEntity()->GetFootStepAudio().GetCachedLowerBodyClothSounds();
		if(lowerCloth)
		{
			ped->GetPedAudioEntity()->CreateAndPlaySound(lowerCloth->PedRollSound, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(lowerCloth->PedRollSound, &initParams, ped,ped));
		}
		for(u8 i = 0; i < MAX_EXTRA_CLOTHING; i++)
		{
			const ClothAudioSettings *extraCloth = ped->GetPedAudioEntity()->GetFootStepAudio().GetCachedExtraClothSounds(i);
			if(extraCloth)
			{
				ped->GetPedAudioEntity()->CreateAndPlaySound(extraCloth->PedRollSound, &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(extraCloth->PedRollSound, &initParams, ped,ped));
			}
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------

void audPedFootStepAudio::PedFwdRollHandler(u32 /*hash*/, void *context) 
{
	CPed *ped = (CPed*)context;
	if(ped && ped->GetPedAudioEntity())
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = ped->GetPedAudioEntity()->GetEnvironmentGroup(true);
		initParams.Tracker = ped->GetPlaceableTracker();
		if(ped->GetIsParachuting())
		{
			ped->GetPedAudioEntity()->CreateAndPlaySound(ATSTRINGHASH("PARACHUTE_LAND_PARACHUTE_RELEASE_MASTER", 0x7452D68), &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(ATSTRINGHASH("PARACHUTE_LAND_PARACHUTE_RELEASE_MASTER", 0x7452D68), &initParams, ped,ped));
		}
		else
		{
			audPedFootStepAudio::PedRollHandler(ATSTRINGHASH("PARACHUTE_LAND_PARACHUTE_RELEASE_MASTER", 0x7452D68), ped) ;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::PlayWindClothSound()
{
	if(!m_ClothWindSound && m_UpperClothSettings 
		&& (!m_ParentOwningEntity->GetIsInVehicle() || (m_ParentOwningEntity->GetIsInVehicle() && m_ParentOwningEntity->GetMyVehicle()->GetVehicleAudioEntity()->GetMinOpenness() > 0.9f)))
	{
		audSoundInitParams initParams;
		initParams.Tracker = m_ParentOwningEntity->GetPlaceableTracker();
		m_Parent->CreateAndPlaySound_Persistent(m_UpperClothSettings->WindSound,&m_ClothWindSound,&initParams);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audPedFootStepAudio::StopWindClothSound()
{
	if(m_ClothWindSound)
	{
		m_ClothWindSound->StopAndForget();
	}
}
//-------------------------------------------------------------------------------------------------------------------
#if __BANK
void audPedFootStepAudio::AddMaterialCollisionWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");

	bank->SetCurrentGroup(*audPedFootStepAudio::sm_WidgetGroup);
	bkWidget* materialWidget =  BANKMGR.FindWidget("Audio/audPedAudioEntity/audPedFootstepAudio/Footsteps/Material Type" ) ;
	if(!materialWidget){
		rage::bkCombo* pMaterialCombo = bank->AddCombo("Material Type", &sm_CollisionMaterial, g_audCollisionMaterialNames.GetCount(), NULL);
		if (pMaterialCombo != NULL)
		{
			for (int i = 0; i < g_audCollisionMaterialNames.GetCount(); i++)
			{
				pMaterialCombo->SetString(i, g_audCollisionMaterialNames[i].name);
			}
		}
		bank->UnSetCurrentGroup(*audPedFootStepAudio::sm_WidgetGroup);
	}
}
void audPedFootStepAudio::AddUpperClothWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");

	bkWidget* upperClothsWidget =  BANKMGR.FindWidget("Audio/audPedAudioEntity/Cloths/UpperCloths" ) ;
	if(!upperClothsWidget){
		ClothList *clothList = audNorthAudioEngine::GetObject<ClothList>(ATSTRINGHASH("ClothList",2672428439));
		if(naVerifyf(clothList, "Couldn't find autogenerated cloth list"))
		{
			rage::bkCombo* pUpperClothsCombo = bank->AddCombo("UpperCloths", &sm_OverridenUpperCloths, clothList->numCloths, NULL);
			if (pUpperClothsCombo != NULL)
			{
				for (int i = 0; i < clothList->numCloths; i++)
				{
					pUpperClothsCombo->SetString(i, audNorthAudioEngine::GetMetadataManager().GetObjectName(clothList->Cloth[i].ClothAudioSettings));
				}
			}
		}
	}
}
void audPedFootStepAudio::AddLowerClothWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");

	bkWidget* lowerClothsWidget =  BANKMGR.FindWidget("Audio/audPedAudioEntity/Cloths/LowerCloths" ) ;
	if(!lowerClothsWidget){
		ClothList *clothList = audNorthAudioEngine::GetObject<ClothList>(ATSTRINGHASH("ClothList",2672428439));
		if(naVerifyf(clothList, "Couldn't find autogenerated cloth list"))
		{
			rage::bkCombo* pLowerClothsCombo = bank->AddCombo("LowerCloths", &sm_OverridenLowerCloths, clothList->numCloths, NULL);
			if (pLowerClothsCombo != NULL)
			{
				for (int i = 0; i < clothList->numCloths; i++)
				{
					pLowerClothsCombo->SetString(i, audNorthAudioEngine::GetMetadataManager().GetObjectName(clothList->Cloth[i].ClothAudioSettings));
				}
			}
		}
	}
}
void audPedFootStepAudio::AddExtraClothWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");

	bkWidget* lowerClothsWidget =  BANKMGR.FindWidget("Audio/audPedAudioEntity/Cloths/ExtraCloths" ) ;
	if(!lowerClothsWidget){
		ClothList *clothList = audNorthAudioEngine::GetObject<ClothList>(ATSTRINGHASH("ClothList",2672428439));
		if(naVerifyf(clothList, "Couldn't find autogenerated cloth list"))
		{
			rage::bkCombo* pExtraClothsCombo = bank->AddCombo("ExtraCloths", &sm_OverridenExtraCloths, clothList->numCloths, NULL);
			if (pExtraClothsCombo != NULL)
			{
				for (int i = 0; i < clothList->numCloths; i++)
				{
					pExtraClothsCombo->SetString(i, audNorthAudioEngine::GetMetadataManager().GetObjectName(clothList->Cloth[i].ClothAudioSettings));
				}
			}
		}
	}
}
void audPedFootStepAudio::AddShoeTypeWidgets()
{
	bkBank* bank = BANKMGR.FindBank("Audio");

	bkWidget* shoeTypeWidget =  BANKMGR.FindWidget("Audio/audPedAudioEntity/audPedFootstepAudio/Footsteps/" ) ;
	if(shoeTypeWidget)
	{
		ShoeList *shoeList = audNorthAudioEngine::GetObject<ShoeList>(ATSTRINGHASH("ShoeList",0xCB4E5724));
		if(naVerifyf(shoeList, "Couldn't find autogenerated shoe list"))
		{
			rage::bkCombo* pShoeTypeCombo = bank->AddCombo("ShoeTypes", &sm_OverridenPlayerShoeType, shoeList->numShoes, NULL);
			if (pShoeTypeCombo != NULL)
			{
				for (int i = 0; i < shoeList->numShoes; i++)
				{
					pShoeTypeCombo->SetString(i, audNorthAudioEngine::GetMetadataManager().GetObjectName(shoeList->Shoe[i].ShoeAudioSettings));
				}
			}
		}
	}
}
//void audPedFootStepAudio::AddStealthSceneModes()
//{
	//bkBank* bank = BANKMGR.FindBank("Audio");

	//bkWidget* stealthModeWidgets =  BANKMGR.FindWidget("Audio/audPedAudioEntity/audPedFootstepAudio/Footsteps/Tuning/Stealth" ) ;
	//if(stealthModeWidgets)
	//{
	//	u32 numTypes = 3;
	//	rage::bkCombo* stealthModesCombo = bank->AddCombo("Stealth mixing modes", &sm_StealthMixingMode,numTypes, NULL);
	//	if (stealthModesCombo != NULL)
	//	{
	//		for(u32 i = 0; i < numTypes; i++)
	//		{
	//			switch (i)
	//			{
	//			case 0:
	//				stealthModesCombo->SetString(i, "NORMAL");
	//				break;
	//			case 1:
	//				stealthModesCombo->SetString(i, "FOOTSTEPS_BASED");
	//				break;
	//			case 2:
	//				stealthModesCombo->SetString(i, "VELOCITY_BASED");
	//				break;
	//			default:
	//				naAssertf(false,"Wrong mixing mode");
	//				break;
	//			}
	//		}
	//	}
	//}
//}

const char* audPedFootStepAudio::GetStepTypeName(audFootstepEvent stepType) 
{
	naAssertf(stepType >= AUD_FOOTSTEP_WALK_L && stepType <= AUD_FOOTSTEP_SPRINT_R, "Bad lowercloth index");
	switch (stepType)
	{
	case AUD_FOOTSTEP_WALK_L:			
	case AUD_FOOTSTEP_WALK_R:				
		return "AUD_FOOTSTEP_WALK";
	case AUD_FOOTSTEP_SCUFFHARD_L:		
	case AUD_FOOTSTEP_SCUFFHARD_R:		
		return "AUD_FOOTSTEP_SCUFFHARD";
	case AUD_FOOTSTEP_SCUFF_L:	
	case AUD_FOOTSTEP_SCUFF_R:			
		return "AUD_FOOTSTEP_SCUFF";
	case AUD_FOOTSTEP_HAND_L:	
	case AUD_FOOTSTEP_HAND_R:				
		return "AUD_FOOTSTEP_HAND";
	case AUD_FOOTSTEP_LADDER_L:		
	case AUD_FOOTSTEP_LADDER_R:			
		return "AUD_FOOTSTEP_LADDER";
	case AUD_FOOTSTEP_LADDER_L_UP:		
	case AUD_FOOTSTEP_LADDER_R_UP:		
		return "AUD_FOOTSTEP_LADDER_UP";
	case AUD_FOOTSTEP_LADDER_HAND_L:				
	case AUD_FOOTSTEP_LADDER_HAND_R:		
		return "AUD_FOOTSTEP_LADDER_HAND";
	case AUD_FOOTSTEP_JUMP_LAND_L:		
	case AUD_FOOTSTEP_JUMP_LAND_R:			
		return "AUD_FOOTSTEP_JUMP_LAND";
	case AUD_FOOTSTEP_RUN_L:				
	case AUD_FOOTSTEP_RUN_R:				
		return "AUD_FOOTSTEP_RUN";
	case AUD_FOOTSTEP_SPRINT_L:				
	case AUD_FOOTSTEP_SPRINT_R:		
		return "AUD_FOOTSTEP_SPRINT";
	case AUD_FOOTSTEP_SOFT_L:				
	case AUD_FOOTSTEP_SOFT_R:		
		return "AUD_FOOTSTEP_SOFT";
	default: break;
	}
	return "INVALID_STEP_NAME";
}
void audPedFootStepAudio::SaveFootstepEventTuning()
{
	// TODO: Maybe get this through CDataFileMgr?
	PARSER.SaveObject("common:/data/ai/noisetuning", "meta", &sm_FootstepEventTuning);
}
//-------------------------------------------------------------------------------------------------------------------
void LeftLadderFootCB()
{
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		audPedFootStepAudio::LeftLadderFoot(0, player);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void LeftLadderFootUpCB()
{
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		audPedFootStepAudio::LeftLadderFootUp(0, player);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void RightLadderFootCB()
{
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		audPedFootStepAudio::RightLadderFoot(0, player);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void RightLadderFootUpCB()
{
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		audPedFootStepAudio::RightLadderFootUp(0, player);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void LeftLadderHandCB()
{
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		audPedFootStepAudio::LeftLadderHand(ATSTRINGHASH("AEF_LADDER_HAND_L",0x970813C9), player);
	}
}

//-------------------------------------------------------------------------------------------------------------------
void RightLadderHandCB()
{
	CPed *player = CGameWorld::FindLocalPlayer();
	if(player)
	{
		audPedFootStepAudio::RightLadderHand(ATSTRINGHASH("AEF_LADDER_HAND_R",0x2392ACE0), player);
	}
}

void audPedFootStepAudio::AddWidgets(bkBank &bank)
{
	bank.PushGroup("audPedFootstepAudio",false);
		bank.AddToggle("Display ground material", &g_DisplayGroundMaterial);  
		bank.AddText("Ground Material", &g_PlayerMaterialName[0], sizeof(g_PlayerMaterialName));
#if __DEV
		bank.AddToggle("Debug Ped Footsteps", &g_BreakpointDebugPedFootstep);
#endif
		bank.AddSlider("Rattle Duck In First Person", &g_WeaponRattleReductionInFirstPerson, -100.0f, 10.0f, 0.1f);
		bank.PushGroup("Anim triggered",false);
			bank.AddSlider("g_CoverHitWallClothingVolume", &g_CoverHitWallClothingVolume, -100.0f, 10.0f, 0.1f);
			bank.AddSlider("g_CoverHitWallWeaponVolume", &g_CoverHitWallWeaponVolume, -100.0f, 10.0f, 0.1f);
			bank.AddButton("LeftLadderFoot", datCallback(CFA(LeftLadderFootCB)));
			bank.AddButton("LeftLadderFootUp", datCallback(CFA(LeftLadderFootUpCB)));
			bank.AddButton("RightLadderFoot", datCallback(CFA(RightLadderFootCB)));
			bank.AddButton("RightLadderFootUp", datCallback(CFA(RightLadderFootUpCB)));
			bank.AddButton("LeftLadderHand", datCallback(CFA(LeftLadderHandCB)));
			bank.AddButton("RightLadderHand", datCallback(CFA(RightLadderHandCB)));
			bank.AddSlider("g_TimeInAirForLandVocal", &g_TimeInAirForLandVocal, 0, 10000, 1);
		bank.PopGroup();
		bank.PushGroup("Falling in water ",false);
			bank.AddSlider("sm_WaterSplashThreshold", &sm_WaterSplashThreshold, 0.f, 24.f, 1.0f);
			bank.AddSlider("sm_WaterSplashSmallThreshold", &sm_WaterSplashSmallThreshold, 0.f, 24.f, 1.0f);
			bank.AddSlider("sm_WaterSplashMediumThreshold", &sm_WaterSplashMediumThreshold, 0.f, 24.f, 1.0f);
		bank.PopGroup();
		bank.PushGroup("Footsteps",false);
			sm_WidgetGroup = smart_cast<bkGroup*>(bank.GetCurrentGroup());
			naAssert(sm_WidgetGroup);
			bank.AddToggle("DisableFootsteps", &g_DisableFootstepSounds);
			bank.AddToggle("Don't modulate the footsteps by the scaling factors", &sm_DontApplyScales);
			bank.AddSeparator();
			bank.AddToggle("Only play material", &sm_OnlyPlayMaterial);
			bank.AddToggle("Only play material custom footstep", &sm_OnlyPlayMaterialCustom);
			bank.AddToggle("Only play material impact", &sm_OnlyPlayMaterialImpact);
			bank.AddToggle("Only play shoe", &sm_OnlyPlayShoe);
			bank.AddToggle("Custom footstep tracks the player.", &sm_MaterialCustomTrack);
			bank.AddToggle("Show extra layers info", &sm_ShowExtraLayersInfo);
			bank.AddSeparator();
			bank.PushGroup("Footstep vol decay over time");
			bank.AddSlider("Footstep Vol Decay Over Time", &sm_FootstepVolDecayOverTime, -100.f, 24.f, 1.0f);
			bank.AddSlider("Max Footstep Vol Decay Over Time", &sm_MaxFootstepVolDecay, -100.f, 24.f, 1.0f);
			bank.AddSlider("Footstep Impulse Mag Decay Over Time", &sm_MinFootstepImpMagDecay, 0.f, 1.f, 0.001f);
			bank.AddSlider("Max Footstep Impulse Mag Decay Over Time", &sm_FootstepImpMagDecayOverTime, 0.f, 1.f, 0.001f);
			bank.PopGroup();
			bank.PushGroup("Tuning",false);
				bank.AddToggle("Force all footsteps to be soft", &sm_ForceSoftSteps);
				bank.AddSlider("Min Footstep impact impulse magnitude", &sm_MinImpulseMag, 0.f, 20.f, 0.1f);
				bank.AddSlider("Max Footstep impact impulse magnitude", &sm_MaxImpulseMag, 0.f, 20.f, 0.1f);
				bank.AddSlider("Walk vol offset (dB)", &sm_WalkFootstepVolOffset, -100.f, 24.f, 1.f);
				bank.AddSlider("Run vol offset (dB)", &sm_RunFootstepVolOffset, -100.f, 24.f, 1.f);
				bank.AddSlider("NPC vol offset (dB)", &sm_NPCFootstepVolOffset, -100.f, 24.f, 1.f);
				bank.AddSlider("NPC run volume curve scale", &sm_NPCRunFootstepRollOffScale, 0.f, 20.f, 0.1f);
				bank.AddSlider("Jump land volume offset for custom impact (dB)", &sm_CIJumpLandVolumeOffset, -100.f, 24.f, 1.f);
				bank.PushGroup("Stealth");
					bank.PushGroup("To be given by script/code");
						bank.AddToggle("Enable/Disable script stealth mode", &sm_ScriptStealthMode);
						bank.AddToggle("Override player stealth", &sm_OverridePlayerStealth);
						bank.AddSlider("PlayerStealth", &sm_OverridedPlayerStealth, 0.f, 1.f, 0.01f);
					bank.PopGroup();
						//audPedFootStepAudio::AddStealthSceneModes();
					bank.PushGroup("Mixing");
						bank.AddSlider("StepBased delta apply", &sm_DeltaApplyPerStep, 0.f, 1.f, 0.01f);
						bank.AddSlider("Fade in crouch scene", &sm_FadeInCrouchScene, 0, 100000, 100);
						bank.AddSlider("Fade out crouch scene", &sm_FadeOutCrouchScene, 0, 100000, 100);
					bank.PopGroup();
					bank.PushGroup("Sweeteners");
						bank.AddSlider("sm_ClumsyStepToeDelayOffset", &sm_ClumsyStepToeDelayOffset, -1000.f, 1000.f, 100.f);
					bank.PopGroup();
					bank.PushGroup("Ped rolls");
						bank.AddSlider("sm_StealthPedRollVolOffset (dB)", &sm_StealthPedRollVolOffset, -100.f, 24.f, 1.f);
						bank.AddSlider("sm_StealthPedRollLPFCutoff", &sm_StealthPedRollLPFCutoff, 0, 24000, 1);
						bank.AddSlider("sm_StealthPedRollAttackTime", &sm_StealthPedRollAttackTime, 0, 1000, 1);
					bank.PopGroup();
					bank.PushGroup("BareFeet",false);
						bank.AddSlider("sm_BareFeetStealthVol (dB)", &sm_BareFeetStealthVol, -100.f, 24.f, 1.f);
						bank.AddSlider("sm_BareFeetStealthLPFCutOff", &sm_BareFeetStealthLPFCutOff, 0, 24000, 1);
						bank.AddSlider("sm_BareFeetStealthAttackTime", &sm_BareFeetStealthAttackTime, 0, 1000, 1);
					bank.PopGroup();
					bank.PushGroup("Custom impact jump lands",false);
						bank.AddSlider("sm_JumpLandCustomImpactStealthVol (dB)", &sm_JumpLandCustomImpactStealthVol, -100.f, 24.f, 1.f);
						bank.AddSlider("sm_JumpLandCustomImpactStealthLPFCutOff", &sm_JumpLandCustomImpactStealthLPFCutOff, 0, 24000, 1);
						bank.AddSlider("sm_JumpLandCustomImpactStealthAttackTime", &sm_JumpLandCustomImpactStealthAttackTime, 0, 1000, 1);
					bank.PopGroup();
				bank.PopGroup();
			bank.PopGroup();
			bank.AddSeparator();
			bank.AddToggle("Show wet foot info ", &sm_WetFeetInfo);
			bank.AddToggle("Draw Footsteps", &g_bDrawFootsteps);
			bank.AddToggle("Draw Footstep Events", &g_bDrawFootstepEvents);
			bank.AddToggle("Show player ped material", &sm_ShouldShowPlayerMaterial);
			bank.AddSeparator();
			bank.AddSlider("WalkVolOffset", &g_audPedWalkFeetVolOffset, -18.0f, 0.0f, 0.5f);
			bank.AddSlider("RunVolOffset", &g_audPedRunFeetVolOffset, -18.0f, 0.0f, 0.5f);
			bank.AddSlider("SprintVolOffset", &g_audPedSprintFeetVolOffset, -18.0f, 0.0f, 0.5f);
			bank.AddSlider("PlayerVolOffset", &g_audPlayerPedFeetVolOffset, -12.0f, 12.0f, 0.5f);
			bank.AddSlider("FeetVolOffset", &g_audPedFeetVolOffset, -12.0f, 12.0f, 0.5f);
			bank.AddSeparator();
			bank.AddToggle("Override Wetness", &sm_OverrideShoeWetness);
			bank.AddSlider("Wetness", &sm_OverridedShoeWetness, 0.f, 1.f, 0.01f);
			bank.AddSlider("Dry out ratio", &sm_DryOutRatio, 0.1f, 1.f, 0.01f);
			bank.AddSlider("Vol dump in water ", &sm_FeetInsideWaterVolDump, 0.f, 40.f, 1.f);
			bank.AddSlider("Impact dump in water ", &sm_FeetInsideWaterImpactDump, 0.f, 40.f, 0.1f);
			bank.AddToggle("Override dirtiness", &sm_OverrideShoeDirtiness);
			bank.AddSlider("Dirtiness", &sm_OverridedShoeDirtiness, 0.f, 1.f, 0.01f);
			bank.AddSlider("Clean up ratio", &sm_CleanUpRatio, 0.1f, 2.f, 0.01f);
			bank.AddToggle("Override creakiness", &sm_OverrideShoeCreakiness);
			bank.AddSlider("Creakiness", &sm_OverridedShoeCreakiness, 0.f, 1.f, 0.01f);
			bank.AddToggle("Override glassiness", &sm_OverrideShoeGlassiness);
			bank.AddSlider("Glassiness", &sm_OverridedShoeGlassiness, 0.f, 1.f, 0.01f);
			bank.AddSlider("Distance threshold to update glassiness", &sm_DistanceThresholdToUpdateGlassiness, 0.f, 100.f, 1.f);
			
			bank.AddToggle("Override material hardness", &sm_OverrideMaterialHardness);
			bank.AddSlider("Overridden material hardness", &sm_OverridedMaterialHardness, 0.f, 1.f, 0.01f);
			bank.AddToggle("Override shoe hardness", &sm_OverrideShoeHardness);
			bank.AddSlider("Overridden shoe hardness", &sm_OverridedShoeHardness, 0.f, 1.f, 0.01f);
			bank.AddToggle("Override foot speed", &sm_OverrideFootSpeed);
			bank.AddSlider("Overridden foot speed", &sm_OverridedFootSpeed, 0.f, 1.f, 0.01f);
			bank.AddToggle("OverridePlayerShoeType", &g_OverridePlayerShoeType);
			if(g_AudioEngine.GetRemoteControl().IsPresent())
			{
				bank.AddText("Player shoe type Override", g_PlayerShoeTypeOverrideName, 64, false);
				audPedFootStepAudio::AddShoeTypeWidgets();
				bank.AddToggle("OverrideMaterialType", &g_OverrideMaterialType);
				bank.AddText("Override material name", g_OverrideMaterialName, 64, false);
			}
		bank.PopGroup();		
		bank.PushGroup("AI Noise Range", false);
			bank.AddButton("Load", datCallback(CFA(LoadFootstepEventTuning)));
			bank.AddButton("Save", datCallback(CFA(SaveFootstepEventTuning)));
			bank.PushGroup("Singleplayer", false);
				audFootstepEventTuning::AddWidgets(bank, sm_FootstepEventTuning.m_SinglePlayer);
			bank.PopGroup();
			bank.PushGroup("Multiplayer", false);
				audFootstepEventTuning::AddWidgets(bank, sm_FootstepEventTuning.m_MultiPlayer);
			bank.PopGroup();
		bank.PopGroup();
		if(g_AudioEngine.GetRemoteControl().IsPresent())
		{
			bank.PushGroup("Cloths",false);
				bank.AddSlider("Player foley intensity",&sm_PlayerFoleyIntensity,0.f, 1.f,0.01f);
				bank.AddSlider("sm_MoveBlendStopThreshold", &sm_MoveBlendStopThreshold, 0.f, 1.f, 0.01f);
				bank.AddToggle("Use Player Version On Focus Ped", &sm_UsePlayerVersionOnFocusPed);
				bank.AddToggle("Use NPC Version for the player", &sm_DontUsePlayerVersion);
				bank.AddToggle("OverrideUpperCloths", &g_OverrideUpperCloths);
				bank.AddText("Upper cloths Override", g_UpperClothsOverrideName, 64, false);
				audPedFootStepAudio::AddUpperClothWidgets();
				bank.AddToggle("OverrideLowerCloths", &g_OverrideLowerCloths);
				bank.AddText("Lower cloths Override", g_LowerClothsOverrideName, 64, false);
				audPedFootStepAudio::AddLowerClothWidgets();
				bank.AddToggle("OverrideExtraCloths", &g_OverrideExtraCloths);
				bank.AddText("Extra cloths Override", g_ExtraClothsOverrideName, 64, false);
				audPedFootStepAudio::AddExtraClothWidgets();
			bank.PopGroup();		
		}
		bank.PushGroup("Bushes",false);
			bank.AddToggle("Trigger bush sounds", &s_TriggerBush);
			bank.AddSlider("Player foley intensity",&sm_PlayerFoleyIntensity,0.f, 1.f,0.01f);
			bank.AddSlider("Bush creakiness", &sm_BushCreakiness, 0.f, 1.f, 0.01f);
		bank.PopGroup();	
	bank.PopGroup();	
}
void audFootstepEventTuning::AddWidgets(bkBank& bank, atRangeArray<audFootstepEventMinMax, kNumFootstepMinMax>& rangeArray)
{
	bank.AddSlider("Walk Crouch Min", &rangeArray[kFootstepWalkCrouch].m_MinDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Walk Crouch Max", &rangeArray[kFootstepWalkCrouch].m_MaxDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Walk Min", &rangeArray[kFootstepWalkStand].m_MinDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Walk Max", &rangeArray[kFootstepWalkStand].m_MaxDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Run Crouch Min", &rangeArray[kFootstepRunCrouch].m_MinDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Run Crouch Max", &rangeArray[kFootstepRunCrouch].m_MaxDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Run Min", &rangeArray[kFootstepRunStand].m_MinDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Run Max", &rangeArray[kFootstepRunStand].m_MaxDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Sprint Min", &rangeArray[kFootstepSprint].m_MinDist, 0.0f, 500.0f, 0.1f);
	bank.AddSlider("Sprint Max", &rangeArray[kFootstepSprint].m_MaxDist, 0.0f, 500.0f, 0.1f);
}

#endif	// __BANK
