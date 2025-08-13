// 
// audio/weaponaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//


#include "weaponaudioentity.h"
#include "debugaudio.h"   
#include "ambience/ambientaudioentity.h"

// Rage headers
#include "grcore/debugdraw.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiosoundtypes/soundcontrol.h"
#include "audiosoundtypes/simplesound.h" 
#include "Peds/PedIntelligence.h"
#include "spatialdata/sphere.h"
#include "task/Combat/TaskCombatMelee.h"


// Framework headers
#include "fwmaths/geomutil.h"

// Game headers
#include "audio/frontendaudioentity.h"
#include "audio/northaudioengine.h" 
#include "audio/environment/microphones.h"
#include "Control/Replay/Replay.h"
#include "Control/Replay/Audio/WeaponAudioPacket.h"
#include "Control/Replay/Audio/BulletByAudioPacket.h"
#include "camera/CamInterface.h"
#include "vehicles/vehicle.h"
#include "vehicles/Boat.h"
#include "camera/caminterface.h"
#include "debugaudio.h"
#include "objects/object.h"
#include "peds/ped.h"
#include "peds/ped.h"
#include "camera/replay/ReplayDirector.h"
#include "scene/RefMacros.h"

#include "weapons/info/weaponinfo.h"
#include "weapons/weapon.h"

AUDIO_WEAPONS_OPTIMISATIONS() 

namespace rage
{
	XPARAM(noaudio);

}

XPARAM(noaudiohardware);

class audOcclusionGroup;

static u8 g_MaxWeaponFiresPlayedPerFrame = 1;
static u32 s_AutoFireReleaseTime = 200;
REPLAY_ONLY(static u32 s_ReplayAutoFireReleaseTime = 500;)

static f32 g_BulletBySphereRadius = 1.0f;
static s32 g_BulletByMinTimeMs = 100, g_BulletByMaxTimeMs = 450;
static f32 g_ExplosionBulletByDistance = 10.0f;
static s32 g_ExplosionBulletByPredelay = 200;
static u32 g_AudNumPlayerWeaponSlots = 0;

f32 audWeaponAudioEntity::sm_InteriorSilencedWeaponWetness = 0.1f;

#if 1 //__BANK ...We want to make this bank again when the interior metrics are hooked up to meaningful values
bool g_ForceUseInteriorMetrics = false;
bool g_DebugInteriorMetrics = false;
float g_DebugInteriorWetness = 0.f;
float g_DebugOutsideVisability = 0.2f;
u32 g_DebugInteriorLpf = 24000;
u32 g_DebugInteriorPredelay = 0;
float g_DebugInteriorHold = 0.f;
#endif

f32 g_InteriorTransitionTime = 0.5f; 

bool g_DisableTightSpotInteriorGuns = true;
bool g_PositionReportOppositeEcho = true;
bool g_PlayBothEchos = false;

waterCannonHitInfo audWeaponAudioEntity::sm_WaterCannonOnPedsInfo;
waterCannonHitInfo audWeaponAudioEntity::sm_WaterCannonOnVehInfo;

bool audWeaponAudioEntity::sm_IsWaitingOnEquippedWeaponLoad = false;
bool audWeaponAudioEntity::sm_UseBackupWeapons = false;
bool audWeaponAudioEntity::sm_IsWaitingOnNextWeaponLoad = false;
bool audWeaponAudioEntity::sm_WantsToLoadPlayerEquippedWeapon = false;
bool audWeaponAudioEntity::sm_UsingPlayerDistanceCam = false;
bool audWeaponAudioEntity::sm_Smooth = true;
f32 audWeaponAudioEntity::sm_TimeToMuteWaterCannon = 950.f;
bool audWeaponAudioEntity::sm_PlayerGunDampingOn = false;
u32 audWeaponAudioEntity::sm_LastTimePlayerGunFired = 0;
u32 audWeaponAudioEntity::sm_LastTimePlayerNotFiredGunForAWhile = 0; // this is the last time the player had gone g_NotFiredForAWhileTime without shooting.
f32 audWeaponAudioEntity::sm_PlayerGunDuckingAttenuation = 0.0f;

f32 audWeaponAudioEntity::sm_NpcReportIntensity = 0.f;
f32 audWeaponAudioEntity::sm_NpcReportIntensityDecreaseRate = 0.25f;
f32 audWeaponAudioEntity::sm_NpcReportCullingDistanceSq = 20.f*20.f;
f32 audWeaponAudioEntity::sm_IntensityForNpcReportDistanceCulling = 8.f;
f32 audWeaponAudioEntity::sm_IntensityForNpcReportFullCulling = 8.f;

u32 audWeaponAudioEntity::sm_PlayerHitBoostHoldTime = 1000;
f32 audWeaponAudioEntity::sm_PlayerhitBoostSmoothTime = 0.25f;
float audWeaponAudioEntity::sm_PlayerHitBoostVol = 0.f;
float audWeaponAudioEntity::sm_PlayerHitBoostRolloff = 1.f;
bool audWeaponAudioEntity::sm_SkipMinigunSpinUp = false;
u32 audWeaponAudioEntity::sm_BlockWeaponSpinFrame = 0;
u32 audWeaponAudioEntity::sm_WeaponAnimTimeFilter = 100;
u32 audWeaponAudioEntity::sm_AutoEmptyShotTimeFilter = 150;

audSoundSet audWeaponAudioEntity::sm_WaterCannonSounds;
audSoundSet audWeaponAudioEntity::sm_BulletByUnderwaterSounds;
audSoundSet audWeaponAudioEntity::sm_BulletByGeneralSounds;

audWaveSlotManager audWeaponAudioEntity::sm_PlayerWeaponSlotManager;
audPlayerWeaponSlot audWeaponAudioEntity::sm_PlayerEquippedWeaponSlot;
audPlayerWeaponSlot audWeaponAudioEntity::sm_ScriptWeaponSlot;
audPlayerWeaponSlot audWeaponAudioEntity::sm_PlayerNextWeaponSlot;

u32 audWeaponAudioEntity::sm_EmptyShotFilter = 50;

audCategory * g_WeaponCategory;

u32 audWeaponAudioEntity::sm_LastTimeSomeoneNearbyFiredAGun = 0;
u32 audWeaponAudioEntity::sm_LastTimeEnemyNearbyFiredAGun = 0;
f32 audWeaponAudioEntity::sm_GunfireFactor = 0.0f;
u32 g_TimeSinceSomeoneNearbyFiredToReduceFactor = 500;
f32 g_GunFiredFactorIncrease = 0.05f;
f32 g_GunNotFiredFactorDecrease = -0.025f;
f32 g_MaxDistanceToCountTowardFactor = 60.0f;

f32 g_EchoVolumeDrop = 0.0f;
u32 g_EchoDepth = 1;
u32 g_maxAutoSoundHoldTime = 500;
u32 g_MinAutoEchoLoopCount = 2;
u32 g_MinAutoEchoLoopCountIgnoreTime = 200;

f32 g_DefaultGunPostSubmixAttenuation = -3.0f;
u32 g_NotFiredForAWhileTime = 5000;
u32 g_BeenFiringForAWhileTime = 2000;
f32 g_DuckingAttenuationPerShot = -0.1f;
#if __BANK
bool g_PlayerGunNeverDamped = false;
bool g_PlayerGunAlwaysDamped = false;

bool g_AuditionWeaponFire = false;

bool g_bForceWeaponEchoSettings = false;
f32 g_fWeaponEchoAttenuation = -6.0f;
u32 g_uWeaponEchoPredelay = 50;

f32 g_ForceWaitForPlayerEquippedWeapon = 0.f;
u32 g_ForceWaitForPlayerEquippedWeaponTime = 0;

#endif

bool g_DebugWeaponTimings = false;
bool g_PlayerUsesNpcWeapons = false;


const u32 g_PedUpperBodyClothes = ATSTRINGHASH("PED_CLOTHING_UPPER", 0x0a1bf01c7);
const u32 g_PedLowerBodyClothes = ATSTRINGHASH("PED_CLOTHING_LOWER", 0x0a32e0d03);
const u32 g_CarPedInside = ATSTRINGHASH("CAR_PED_INSIDE", 0x06ae40991);
const u32 g_HeliPedInside = ATSTRINGHASH("HELI_PED_INSIDE", 0x0a3e70e3c);
const u32 g_HeliWeaponOn = ATSTRINGHASH("HELI_WEAPON_ON", 0x08296d80e);
const u32 g_BoatPedInside = ATSTRINGHASH("BOAT_PED_INSIDE", 0x0a4b96b67);
const u32 g_MotorbikePedInside = ATSTRINGHASH("MOTORBIKE_PED_INSIDE", 0x018ca9b44);
const u32 g_PedWeapon = ATSTRINGHASH("PED_WEAPON", 0x0bfa06c78);

audSoundSet audWeaponAudioEntity::sm_CodeSounds;
audSound * audWeaponAudioEntity::sm_ThermalGoggleSound; 
audSound * audWeaponAudioEntity::sm_ThermalGoggleOffSound;

bool audWeaponAudioEntity::sm_ThermalGogglesOn;
u32 audWeaponAudioEntity::sm_PlayThermalGogglesSound = 0;  // 0 = nosound, soundhash = play the sound


audWeaponFireEvent audWeaponAudioEntity::sm_WeaponFireEventList[g_MaxWeaponFireEventsPerFrame];
audWeaponAutoFireEvent audWeaponAudioEntity::sm_AutoWeaponsFiringList[g_MaxWeaponFireEventsPerFrame];
audWeaponFireEvent audWeaponAudioEntity::sm_LastWeaponFireEventPlayed;
audWeaponFireEvent audWeaponAudioEntity::sm_LastWeaponFireEventPlayedAgainstPlayer;

u8 audWeaponAudioEntity::sm_NumWeaponFireEventsThisFrame = 0;

// a global weapon audio entity for use with projectiles, script triggered weapon fire etc 
audWeaponAudioEntity g_WeaponAudioEntity;

PF_PAGE(PlayerGunVolumePage, "PlayerGunVolume");
PF_GROUP(PlayerGunVolumeGroup);
PF_LINK(PlayerGunVolumePage, PlayerGunVolumeGroup);
PF_VALUE_FLOAT(PlayerGunVolume, PlayerGunVolumeGroup);
PF_VALUE_FLOAT(GunfireFactor, PlayerGunVolumeGroup);

PARAM(noweaponecho, "disables gun reports");
PARAM(rdrreportaudio, "turns on testing of RDR report system");

atRangeArray<weaponAnimEvent, WEAPON_AUDENTITY_NUM_ANIM_EVENTS> audWeaponAudioEntity::sm_WeaponAnimEvents;
atRangeArray<waterCannonHitInfo, NUM_MATERIALTYPE> audWeaponAudioEntity::sm_WaterCannonHitMapInfo;
atRangeArray<f32, 8> audWeaponAudioEntity::sm_ReportGoodness;
atRangeArray<Vector3, 24> audWeaponAudioEntity::sm_RelativeScanPositions;

atFixedArray<audProjectileData, g_MaxAudProjectileEvents> audWeaponAudioEntity::sm_ProjectileEvents;
atRangeArray<audWeaponSpinEvent, g_MaxAudWeaponSpinEvents> audWeaponAudioEntity::sm_WeaponSpinEvents;
atRangeArray<audProjectileCookEvent, g_MaxAudCookProjectileEvents> audWeaponAudioEntity::sm_ProjectileCookEvents;

audCurve audWeaponAudioEntity::sm_ReportGoodnessToAttenuationCurve;
audCurve audWeaponAudioEntity::sm_ReportGoodnessToPredelayCurve;
audCurve audWeaponAudioEntity::sm_ConductorEmphasizeFactorToVol;

audCurve audWeaponAudioEntity::sm_InteriorWetShotVolumeCurve;
audCurve audWeaponAudioEntity::sm_InteriorDryShotVolumeCurve;
audCurve audWeaponAudioEntity::sm_OutsideVisibilityReportVolumeCurve;

audCurve audWeaponAudioEntity::sm_StickyBombImpulseVolumeCurve;


bool audWeaponAudioEntity::sm_bIsReportStuffInited = false;
bool g_ShouldDrawReportGoodness = false;
f32 g_MaxDistanceForEnemyReport = 30.0f;

#if __BANK
bool g_DebugPlayerWeaponSlots = false;
#endif


#define REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD 0.06f
#define REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD		0.21f

void audWeaponFireInfo::PopulatePresuckInfo(const WeaponSettings *settings, audWeaponFireInfo::PresuckInfo &presuckInfo)
{
	if(!settings)
	{
		presuckInfo.presuckSound = g_NullSoundHash;
		presuckInfo.presuckTime = 0;
		presuckInfo.presuckScene = NULL_HASH;
		return;
	}

	f32 markerSpeed = 1.0f;
#if GTA_REPLAY
	if(audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		markerSpeed = CReplayMgr::GetMarkerSpeed();
#if __BANK
		if(audNorthAudioEngine::IsForcingSlowMoVideoEditor())
			markerSpeed = 0.2f;
		if(audNorthAudioEngine::IsForcingSuperSlowMoVideoEditor())
			markerSpeed = 0.05f;
#endif
	}
#endif

	bool cienmaticSlowMo = audNorthAudioEngine::IsInCinematicSlowMo();

	if(isSuppressed && settings->SuppressedFireSound != g_NullSoundHash)
	{
		presuckInfo.presuckSound = g_NullSoundHash;
		if((cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoSuppressedFireSoundPresuck != g_NullSoundHash)			
		{
			presuckInfo.presuckSound = settings->SlowMoSuppressedFireSoundPresuck;
		}
		if(markerSpeed <= 0.06f && settings->SuperSlowMoSuppressedFireSoundPresuck != g_NullSoundHash)			
		{
			presuckInfo.presuckSound = settings->SuperSlowMoSuppressedFireSoundPresuck;
		}

		presuckInfo.presuckTime = 0;
		if(cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD)			
		{
			presuckInfo.presuckTime = settings->SlowMoSuppressedFireSoundPresuckTime;
		}
		if(markerSpeed <= 0.06f && settings->SuperSlowMoSuppressedFireSoundPresuckTime != 9999)			
		{
			presuckInfo.presuckTime = settings->SuperSlowMoSuppressedFireSoundPresuckTime;
		}
	}
	else
	{
		presuckInfo.presuckSound = g_NullSoundHash;
		if((cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoFireSoundPresuck != g_NullSoundHash)			
		{
			presuckInfo.presuckSound = settings->SlowMoFireSoundPresuck;
		}
		if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoFireSoundPresuck != g_NullSoundHash)			
		{
			presuckInfo.presuckSound = settings->SuperSlowMoFireSoundPresuck;
		}


		presuckInfo.presuckTime = 0;
		if(cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD)			
		{
			presuckInfo.presuckTime = settings->SlowMoFireSoundPresuckTime;
		}
		if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoFireSoundPresuckTime != 9999)			
		{
			presuckInfo.presuckTime = settings->SuperSlowMoFireSoundPresuckTime;
		}
	}
}

u32 audWeaponFireInfo::GetWeaponFireSound(const WeaponSettings * settings, const bool isLocalPlayer, audWeaponFireInfo::PresuckInfo* presuckInfo, bool isFinalKillShot) const
{
	f32 markerSpeed = 1.0f;
#if GTA_REPLAY
	if(audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		markerSpeed = CReplayMgr::GetMarkerSpeed();
#if __BANK
		if(audNorthAudioEngine::IsForcingSlowMoVideoEditor())
			markerSpeed = 0.2f;
		if(audNorthAudioEngine::IsForcingSuperSlowMoVideoEditor())
			markerSpeed = 0.05f;
#endif
	}
#endif

	bool cienmaticSlowMo = audNorthAudioEngine::IsInCinematicSlowMo();

	bool bIsSubmarine = parentEntity && parentEntity->GetIsTypeVehicle() && (((CVehicle*)parentEntity.Get())->InheritsFromSubmarine() || ((CVehicle*)parentEntity.Get())->InheritsFromSubmarineCar());
	if(Water::IsCameraUnderwater() && !bIsSubmarine)
	{
		return g_WeaponAudioEntity.GetUnderWaterFireSound();
	}
	if(settings)
	{
		if(settings->FireSound != g_NullSoundHash || settings->AutoSound != g_NullSoundHash)
		{
			if(isLocalPlayer && (g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMichael || g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMultiplayer))
			{	
				if(settings->FireSound == ATSTRINGHASH("SPL_STUN_MASTER", 0x744173DB))
				{
					return g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMultiplayer ? 
							ATSTRINGHASH("GTAO_Special_Ability_MP_Stun_Gun_Master", 0xCE390653) : ATSTRINGHASH("SPECIAL_ABILITY_MICHAEL_STUN_GUN_MASTER", 0x97906B1F);
				}

				if(audNorthAudioEngine::ShouldTriggerPulseHeadset())
				{
					return isSuppressed ? ATSTRINGHASH("SPECIAL_ABILITY_MICHAEL_SILENCED_GUN_PULSE", 0xFAF8B723) :
						ATSTRINGHASH("SPECIAL_ABILITY_MICHAEL_GUN_PULSE", 0xFB3F431F);
				}

				if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeMultiplayer)
				{
					return isSuppressed ? ATSTRINGHASH("GTAO_Special_Ability_MP_Silencer_Master", 0xD7729D27)
						: ATSTRINGHASH("GTAO_Special_Ability_MP_Gun_Master", 0xF8551A1C);		
				}
				else
				{
					return isSuppressed ? ATSTRINGHASH("SPECIAL_ABILITY_MICHAEL_SILENCER_MASTER", 0x7F7DD4E7) 
						:  ATSTRINGHASH("SPECIAL_ABILITY_MICHAEL_GUN_MASTER", 0xEA3C1868);		
				}
			}
			else if(isLocalPlayer && g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor)
			{
				return ATSTRINGHASH("TREVOR_WEAPON_FIRE_MASTER", 0x56C27B9B);
			}
			else if(isLocalPlayer && g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeFranklin)
			{
				return ATSTRINGHASH("FRANKLIN_GUN_MASTER", 0xB99335D4);
			}
		}

		if(isSuppressed && settings->SuppressedFireSound != g_NullSoundHash)
		{
			if(presuckInfo)
			{
				presuckInfo->presuckSound = g_NullSoundHash;
				if((cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoSuppressedFireSoundPresuck != g_NullSoundHash)			
				{
					presuckInfo->presuckSound = settings->SlowMoSuppressedFireSoundPresuck;
				}
				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoSuppressedFireSoundPresuck != g_NullSoundHash)			
				{
					presuckInfo->presuckSound = settings->SuperSlowMoSuppressedFireSoundPresuck;
				}

				presuckInfo->presuckTime = 0;
				if(cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD)			
				{
					presuckInfo->presuckTime = settings->SlowMoSuppressedFireSoundPresuckTime;
				}
				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoSuppressedFireSoundPresuckTime != 9999)			
				{
					presuckInfo->presuckTime = settings->SuperSlowMoSuppressedFireSoundPresuckTime;
				}

			}

			if((isFinalKillShot || audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor()))
			{
				u32 fireSound = settings->SuppressedFireSound;

				if((isFinalKillShot || cienmaticSlowMo || markerSpeed  <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoSuppressedFireSound != g_NullSoundHash)			
				{
					fireSound = settings->SlowMoSuppressedFireSound;
				}

				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoSuppressedFireSound != g_NullSoundHash)
				{
					fireSound = settings->SuperSlowMoSuppressedFireSound;
				}
				return fireSound;
			}
			else
			{
				return settings->SuppressedFireSound;
			}			
		}
		else
		{
			if(presuckInfo)
			{
				presuckInfo->presuckSound = g_NullSoundHash;
				if((cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoFireSoundPresuck != g_NullSoundHash)			
				{
					presuckInfo->presuckSound = settings->SlowMoFireSoundPresuck;
				}
				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoFireSoundPresuck != g_NullSoundHash)			
				{
					presuckInfo->presuckSound = settings->SuperSlowMoFireSoundPresuck;
				}


				presuckInfo->presuckTime = 0;
				if(cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD)			
				{
					presuckInfo->presuckTime = settings->SlowMoFireSoundPresuckTime;
				}
				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoFireSoundPresuckTime != 9999)			
				{
					presuckInfo->presuckTime = settings->SuperSlowMoFireSoundPresuckTime;
				}
			}

			if((isFinalKillShot || audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor()))
			{
				u32 fireSound = settings->FireSound;

				if((isFinalKillShot || cienmaticSlowMo || markerSpeed  <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoFireSound != g_NullSoundHash)			
				{
					fireSound = settings->SlowMoFireSound;
				}

				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoFireSound != g_NullSoundHash)			
				{
					fireSound = settings->SuperSlowMoFireSound;
				}
				
				return fireSound;
			}
			else
			{
				return settings->FireSound;
			}
		}		
	}
	return g_NullSoundHash;
}

const WeaponSettings * audWeaponFireInfo::FindWeaponSettings(bool BANK_ONLY(warnOnBackup))  
{
	// HL: Somewhat hacky fix for url:bugstar:5518385. This code forces vehicle mounted grenade launchers use the high quality audio *only* if the player is
	// driving the vehicle (ie. passengers use resident/NPC assets). Because vehicle grenade launches are just selected from the weapon menu rather than being 
	// equippable objects, remote players are unaware that the driver has selected them, and so we can't guarantee that the high quality bank will be loaded.
	if (parentEntity && parentEntity->GetIsTypeVehicle() && settingsHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_GRENADELAUNCHER", 0xF1459D83))
	{
		CVehicle* parentVehicle = (CVehicle*)parentEntity.Get();

		if (parentVehicle->ContainsLocalPlayer() && parentVehicle->GetDriver() != CGameWorld::FindLocalPlayer())
		{
			return g_WeaponAudioEntity.GetWeaponSettings(audioSettingsRef);
		}
	}
	// End hacky fix

	ItemAudioSettings * audioSettings = audNorthAudioEngine::GetMetadataManager().GetObject<ItemAudioSettings>(audioSettingsRef);

	if((parentEntity && !g_PlayerUsesNpcWeapons && ((parentEntity->GetIsTypePed() && ((CPed*)parentEntity.Get())->IsLocalPlayer()))) || (parentEntity && parentEntity->GetIsTypeVehicle() && ((CVehicle*)parentEntity.Get())->ContainsLocalPlayer())) //Check if player ped
	{
		WeaponSettings * playerSettings = g_WeaponAudioEntity.GetWeaponSettings(audioSettingsRef, ATSTRINGHASH("player", 0x6F0783F5));
		
		if(g_WeaponAudioEntity.IsWaitingOnEquippedWeaponLoad() || g_WeaponAudioEntity.ShouldUseBackupWeapons())
		{
			//In rare cases we can be waiting for a weapon to load but not the equipped one if the latter doesn't need to
			//load any audio
			if(playerSettings && (playerSettings->BankSound == g_NullSoundHash || playerSettings->BankSound == 0))
			{
				return playerSettings;
			}

#if __BANK
			if(warnOnBackup && settingsHash != g_WeaponAudioEntity.GetEquippedWeaponAudioHash())
			{
				audWarningf("Player weapon audio not loaded for %s, using backup audio", audNorthAudioEngine::GetMetadataManager().GetObjectName(settingsHash));
			}
#endif

			u32 contextHash = ATSTRINGHASH("player_backup", 0x321e6781);
			u32 contextHash2 = ATSTRINGHASH("backup", 0xFD8506AE);
			WeaponSettings * backupSettings = NULL;

			if(audioSettings)
			{
				for(int i=0; i < audioSettings->numContextSettings; i++)
				{
					if(audioSettings->ContextSettings[i].Context == contextHash || audioSettings->ContextSettings[i].Context == contextHash2)
					{
						backupSettings = audNorthAudioEngine::GetObject<WeaponSettings>(audioSettings->ContextSettings[i].WeaponSettings);
						break;
					}
				}
			}

			if(backupSettings)
			{
				return backupSettings;
			}
		}
		return playerSettings;
	}
	return g_WeaponAudioEntity.GetWeaponSettings(audioSettingsRef);
}

u32 audWeaponFireInfo::GetWeaponEchoSound(const WeaponSettings * settings) const
{
	if(isSuppressed && settings->SuppressedEchoSound != g_NullSoundHash)
	{
		return settings->SuppressedEchoSound;
	}  
	return settings->EchoSound;
}

audWeaponAudioComponent::audWeaponAudioComponent() : 
		m_AutoFireLoop(NULL)
		,m_SpinSound(NULL)
		,m_InteriorAutoLoop(NULL)
		,m_PulseHeadsetSound(NULL)
		,m_SettingsHash(0)
		,m_FrameLastEmptyShotFired(0)
		,m_TimeLastEmptyShotFired(0)
		,m_LastImpactTime(0)
		,m_AnimIsAutoFiring(false)
		,m_LastHitPlayerTime(0)
		,m_LastAnimTime(0)
		,m_LastAnimHash(0)
		,m_SpecialAbilityMode(audFrontendAudioEntity::kSpecialAbilityModeNone)
		,m_SpinBoneIndex(-1)
		,m_ForceNPCVersion(false)
{
}

void audWeaponAudioComponent::Init(u32 audioHash, CWeapon * weapon, bool isScriptWeapon)
{
	m_ItemSettings = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(audioHash);

	m_Weapon = weapon;
	m_SettingsHash = audioHash;
	m_IsScriptWeapon = isScriptWeapon;
	m_WasDropped = false;

#if __DEV
	WeaponSettings * weaponSettings = audWeaponAudioEntity::GetWeaponSettings(audioHash);
	g_WeaponAudioEntity.ValidateWeaponSettings(weaponSettings);
#endif
}

void audWeaponAudioComponent::HandleHitPlayer()
{
	m_LastHitPlayerTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
}

u32 audWeaponAudioEntity::GetPlayerBankIdFromWeaponSettings(WeaponSettings * settings)
{
	if(PARAM_noaudio.Get())
	{
		return AUD_INVALID_BANK_ID;
	}

	if(settings)
	{ 
		// This helper class is passed down to the sound factory's ProcessHierarchy function, and gets called 
		// on every sound that we encounter in the hierarchy
		class WeaponBankIdFn : public audSoundFactory::audSoundProcessHierarchyFn
		{
		public:

			WeaponBankIdFn() 
			{ 
				m_LoadBankId = AUD_INVALID_BANK_ID;
			}

			void operator()(u32 classID, const void* soundData)
			{
				if(classID == SimpleSound::TYPE_ID)
				{
					const SimpleSound * sound = reinterpret_cast<const SimpleSound*>(soundData);
					if(sound)
					{
						m_LoadBankId = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->WaveRef.BankName);
					}
				}

			}

			u32 m_LoadBankId;
		};

		WeaponBankIdFn weaponBankIdFinder;
		SOUNDFACTORY.ProcessHierarchy(settings->BankSound, weaponBankIdFinder);

		return weaponBankIdFinder.m_LoadBankId;
	}
	return AUD_INVALID_BANK_ID;
}

#if __DEV

//Check that none of our sounds are using banks that we won't load in
void audWeaponAudioEntity::ValidateWeaponSettings(WeaponSettings * settings)
{
	if(PARAM_noaudio.Get())
	{
		return;
	}

	if(settings)
	{ 
		class WeaponBankQueryFn : public audSoundFactory::audSoundProcessHierarchyFn
		{
		public:
			u32 bankID;

			WeaponBankQueryFn() { bankID = AUD_INVALID_BANK_ID; }

			void operator()(u32 classID, const void* soundData)
			{
				if (classID == SimpleSound::TYPE_ID)
				{
					const SimpleSound * sound = reinterpret_cast<const SimpleSound*>(soundData);
					bankID = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->WaveRef.BankName);
				}
			}
		};

		// This helper class is passed down to the sound factory's ProcessHierarchy function, and gets called 
		// on every sound that we encounter in the hierarchy
		class WeaponBankValidationFn : public audSoundFactory::audSoundProcessHierarchyFn
		{
		public:

			WeaponBankValidationFn() 
			{ 
				m_LoadBankId = AUD_INVALID_BANK_ID;
				m_HadError = false;
				m_CurrentSound = NULL;
				m_CurrenSettings = NULL;
			}

			void operator()(u32 classID, const void* soundData)
			{
				u32 bankID = AUD_INVALID_BANK_ID;

				if(classID == SimpleSound::TYPE_ID)
				{
					const SimpleSound * sound = reinterpret_cast<const SimpleSound*>(soundData);
					if(sound)
					{
						bankID = g_AudioEngine.GetSoundManager().GetFactory().GetBankIndexFromMetadataRef(sound->WaveRef.BankName);
					}
				}

				if(bankID != AUD_INVALID_BANK_ID)
				{
					audWaveSlot* waveSlot = audWaveSlot::FindLoadedBankWaveSlot(bankID);
					if(!naVerifyf((waveSlot && waveSlot->IsStatic()) || bankID == m_LoadBankId, 
						"Weapon settings %s contains a wave from bank %s in its %s which is neither static nor the dynamically load bank (%s), slot name: %s see following tty for details", m_CurrenSettings, g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankID), m_CurrentSound, g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(m_LoadBankId), waveSlot ? waveSlot->GetSlotName() : "(no waveslot)"))
					{
						m_HadError = true;
					}
				}
			}

			void SetSoundRefName(const char * soundRefName) {m_CurrentSound = soundRefName;}
			void SetSettingsName(const char * settingsName) {m_CurrenSettings = settingsName;}

			u32 m_LoadBankId;
			bool m_HadError;
			const char * m_CurrentSound;
			const char * m_CurrenSettings;
		};

		WeaponBankQueryFn bankQueryFn;
		SOUNDFACTORY.ProcessHierarchy(settings->BankSound, bankQueryFn);
		u32 loadBankId = bankQueryFn.bankID;
		
		WeaponBankValidationFn weaponBankValidator;
		weaponBankValidator.m_LoadBankId = loadBankId;
		weaponBankValidator.SetSettingsName(audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset));

		weaponBankValidator.SetSoundRefName("FireSound");
		SOUNDFACTORY.ProcessHierarchy(settings->FireSound, weaponBankValidator); 

		weaponBankValidator.SetSoundRefName("SuppressedFireSound");
		SOUNDFACTORY.ProcessHierarchy(settings->SuppressedFireSound, weaponBankValidator);

		weaponBankValidator.SetSoundRefName("AutoSound");
		SOUNDFACTORY.ProcessHierarchy(settings->AutoSound, weaponBankValidator);

		weaponBankValidator.SetSoundRefName("ReportSound");
		SOUNDFACTORY.ProcessHierarchy(settings->ReportSound, weaponBankValidator);
	}
}
#endif


WeaponSettings *audWeaponAudioComponent::GetWeaponSettings(const CEntity * parentEntity) const
{
	if((parentEntity && ((parentEntity->GetIsTypePed() && ((CPed*)parentEntity)->IsLocalPlayer() && !g_PlayerUsesNpcWeapons))) || (parentEntity && parentEntity->GetIsTypeVehicle() && ((CVehicle*)parentEntity)->ContainsLocalPlayer())) //Check if player ped
	{
		MicrophoneSettings * mic = audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings();
		bool useDistanceMic = false;
		if(mic && AUD_GET_TRISTATE_VALUE(mic->Flags, FLAG_ID_MICROPHONESETTINGS_DISTANTPLAYERWEAPONS) == AUD_TRISTATE_TRUE)
		{
			useDistanceMic = true;
		}
		if(!useDistanceMic)
		{
			return g_WeaponAudioEntity.GetWeaponSettings(m_ItemSettings, ATSTRINGHASH("player", 0x6F0783F5));
		}
	}
	return g_WeaponAudioEntity.GetWeaponSettings(m_ItemSettings);
}

audWeaponAudioComponent::~audWeaponAudioComponent()
{
	if(m_AutoFireLoop)
	{
		m_AutoFireLoop->StopAndForget();
	}
	if(m_SpinSound)
	{
		m_SpinSound->StopAndForget();
	}
	if(m_InteriorAutoLoop)
	{
		m_InteriorAutoLoop->StopAndForget();
	}
	if(m_PulseHeadsetSound)
	{
		m_PulseHeadsetSound->StopAndForget();
	}
	RemoveFromAutoFiringList();
}

audWeaponAudioEntity::audWeaponAudioEntity() : 
m_NextBulletByTimeMs(0)
{
}

void audWeaponAudioEntity::Init() 
{
	//m_WeaponHash = weaponHash; //doesn't seem to be used at all

	g_WeaponCategory = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("WEAPONS", 0x803e392c));
	
	if(!sm_bIsReportStuffInited)
	{
		for (int i=0; i<24; i++)
		{
			sm_RelativeScanPositions[i].Zero();
			f32 angleInRadians = 2.0f * PI * ((f32)i)/24.0f;
			sm_RelativeScanPositions[i].x = rage::Sinf(angleInRadians);
			sm_RelativeScanPositions[i].y = rage::Cosf(angleInRadians);	
		}
		for (int i=0; i<8; i++)
		{
			sm_ReportGoodness[i] = 0.0f;
		}

		sm_ReportGoodnessToAttenuationCurve.Init(ATSTRINGHASH("REPORT_GOODNESS_TO_ATTENUATION", 0x8B4491F1));
		sm_ReportGoodnessToPredelayCurve.Init(ATSTRINGHASH("REPORT_GOODNESS_TO_PREDELAY", 0x5558A5D));

		for (u32 i = HOLLOW_METAL ; i < NUM_MATERIALTYPE; i++)
		{
			sm_WaterCannonHitMapInfo[i].smoother.Init(0.001f,0.001f,0.f,1.f); 
			sm_WaterCannonHitMapInfo[i].sound = NULL;
			sm_WaterCannonHitMapInfo[i].volume = 1.f;
			sm_WaterCannonHitMapInfo[i].hitByWaterCannon = false;
		}

		sm_InteriorWetShotVolumeCurve.Init(ATSTRINGHASH("INTERIOR_WET_SHOT_VOLUME", 0xC20A3148));
		sm_InteriorDryShotVolumeCurve.Init(ATSTRINGHASH("INTERIOR_DRY_SHOT_VOLUME", 0xD83C4607));
		sm_OutsideVisibilityReportVolumeCurve.Init(ATSTRINGHASH("INTERIOR_OUTSIDE_VISABILITY_REPORT", 0x29BC32B2));
		sm_StickyBombImpulseVolumeCurve.Init(ATSTRINGHASH("STICKY_BOMB_IMPULSE_VOLUME", 0x6BEEBD38));

		sm_bIsReportStuffInited = true;
	}
	sm_WaterCannonOnPedsInfo.smoother.Init(0.001f,0.001f,0.f,1.f);
	sm_WaterCannonOnVehInfo.smoother.Init(0.001f,0.001f,0.f,1.f);

	sm_WaterCannonSounds.Init(ATSTRINGHASH("FIRE_TRUCK_WATER_CANNON_SOUNDSET", 0x46DB7A9E));
	sm_BulletByGeneralSounds.Init(ATSTRINGHASH("BULLET_BYS_SOUNDSET", 0xB91BDEFA));
	sm_BulletByUnderwaterSounds.Init(ATSTRINGHASH("BULLET_BYS_UNDERWATER_SOUNDSET", 0xD421775));

	sm_WaterCannonOnPedsInfo.sound = NULL;
	sm_WaterCannonOnVehInfo.sound = NULL;
	m_WaterCannonPedHitDistSqd = LARGE_FLOAT;
	// Seems SetUpWeapon() which calls Init() on us can be called more than once - which causes audEntity to assert.
	if (GetControllerId()==AUD_INVALID_CONTROLLER_ENTITY_ID)
	{
		naAudioEntity::Init();
		audEntity::SetName("audWeaponAudioEntity");
	}
#if USE_CONDUCTORS
	sm_ConductorEmphasizeFactorToVol.Init(ATSTRINGHASH("GUNFIGHT_CONDUCTOR_TEST", 0x28DABF0E));
	m_IntensityFactor = 0.f;
#endif

	char slotName[32];
	for(u32 i = 0; i < g_AudMaxPlayerWeaponSlots; i++)
	{
		formatf(slotName, "PLAYER_WEAPON_%d", i); 

		if(!sm_PlayerWeaponSlotManager.AddWaveSlot(slotName, "PLAYER_WEAPON"))
		{
			break;
		}
		g_AudNumPlayerWeaponSlots++;

	}

	for(u32 i = 0; i < WEAPON_AUDENTITY_NUM_ANIM_EVENTS; i++)
	{
		sm_WeaponAnimEvents[i].hash = 0;
		sm_WeaponAnimEvents[i].weapon = NULL;
	}

	sm_CodeSounds.Init(ATSTRINGHASH("CODED_WEAPON_SOUNDS", 0xB148A7E7));

	sm_ThermalGoggleSound = NULL;
	sm_ThermalGogglesOn = false;
	sm_PlayThermalGogglesSound = 0;
	m_LastDoubleActionWeaponSoundTime = 0;
	sm_ProjectileEvents.Reset();
}

bool audWeaponAudioEntity::SuppressorIsVisible(const CWeapon *weapon)
{
	if (weapon)
	{
		return weapon->GetSuppressorComponent() && weapon->GetSuppressorComponent()->GetDrawable() && weapon->GetSuppressorComponent()->GetDrawable()->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY);
	}
	return false;
}

bool audWeaponAudioEntity::EquippedWeaponIsVisible(const CPed *ped)
{
	if (ped)
	{
		return ped->GetWeaponManager() && ped->GetWeaponManager()->GetEquippedWeaponObject() && ped->GetWeaponManager()->GetEquippedWeaponObject()->GetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY);
	}
	return false;
}

void audWeaponAudioEntity::HandleAnimFireEvent(u32 hash, CWeapon *weapon, CPed *ped)
{
	audWeaponAudioComponent &weaponAudio = weapon->GetAudioComponent();

	bool isSilenced = weapon->GetIsSilenced();
	if (isSilenced && weapon->GetSuppressorComponent() && EquippedWeaponIsVisible(ped))
	{
		isSilenced = SuppressorIsVisible(weapon);
	}

	switch(hash)
	{
	case 0xDF8E89EB : //ATSTRINGHASH("WEAPON_FIRE", 0xDF8E89EB)
		weaponAudio.PlayWeaponFire(ped, isSilenced);	
		break;
	case 0xC2A57744 : //ATSTRINGHASH("AUTO_FIRE_START", 0xC2A57744)
		weaponAudio.PlayWeaponFire(ped, isSilenced);
		weaponAudio.SetAnimIsAutoFiring(true);
		break;
	case 0x3106F0FB : //ATSTRINGHASH("AUTO_FIRE_STOP", 0x3106F0FB)
		weaponAudio.SetAnimIsAutoFiring(false);
		break;
	default:
		break;
	}
}

void audWeaponAudioEntity::AddWeaponAnimEvent(const audAnimEvent &event,CObject *weapon)
{
	BANK_ONLY(bool success = false); 
	for(u32 i = 0; i < WEAPON_AUDENTITY_NUM_ANIM_EVENTS; i++)
	{
		if(sm_WeaponAnimEvents[i].hash == 0)
		{
			sm_WeaponAnimEvents[i].hash = event.hash;
			sm_WeaponAnimEvents[i].weapon = weapon;
			BANK_ONLY(success = true); 
			break;
		}
	}
#if __BANK 
	if(!success)
	{
		naAssertf(false,"Run out of slots to add new weapon anim events, please increase the num of events.");
	}
#endif
}


void audWeaponAudioEntity::SetEmphasizeFactor(f32 factor)
{
	m_IntensityFactor = factor;
}

void audWeaponAudioEntity::GetInteriorMetrics(audWeaponInteriorMetrics & interior, CEntity * parent)
{
	if(parent)
	{
		bool isInterior = parent->GetIsInInterior(); 
		f32 wetness = 0.f;
		f32 outsideVisability = 1.f;
		u32 lowPass = 24000;
		u32 predelay = 0;
		f32 hold = 0.f;

		if(isInterior)
		{
			InteriorWeaponMetrics * metrics = audNorthAudioEngine::GetObject<InteriorWeaponMetrics>(ATSTRINGHASH("INTERIOR_WEAPON_LIVINGROOM_LARGE", 0x2EC29F31));
			const InteriorSettings* interiorSettings = NULL;
			const InteriorRoom* roomSettings = NULL;
			audNorthAudioEngine::GetGtaEnvironment()->GetInteriorSettingsForEntity(parent, interiorSettings, roomSettings);
			if(interiorSettings && roomSettings)
			{
				InteriorWeaponMetrics * roomMetrics = audNorthAudioEngine::GetObject<InteriorWeaponMetrics>(roomSettings->WeaponMetrics);
				if(roomMetrics)
				{
					metrics = roomMetrics;
				}
				else if (parent && CNetwork::IsGameInProgress())
				{
					const CInteriorInst* intInst = CInteriorInst::GetInteriorForLocation(parent->GetAudioInteriorLocation());
				
					// url:bugstar:6791016 - Sonar Jammer : When player is in the XM17 Sub interior full gun reports play - sounds weird
					if (intInst && intInst->GetModelNameHash() == ATSTRINGHASH("xm_x17dlc_int_sub", 0x71F9D1B0))
					{
						metrics = audNorthAudioEngine::GetObject<InteriorWeaponMetrics>(ATSTRINGHASH("INTERIOR_WEAPON_SUBMARINE", 0x40ABC5BA));
					}
				}
			}

			if(metrics)
			{
				wetness = metrics->Wetness;
				outsideVisability = metrics->Visability;
				lowPass = metrics->LPF;
				predelay = metrics->Predelay;
				hold = metrics->Hold;
			}
		}


	#if 1 //__BANK ...We want to make this BANK again when the interior metrics are hooked up to meaningful values
		if(g_DebugInteriorMetrics && (isInterior || g_ForceUseInteriorMetrics))
		{
			wetness = g_DebugInteriorWetness;
			outsideVisability = g_DebugOutsideVisability;
			lowPass = g_DebugInteriorLpf;
			predelay = g_DebugInteriorPredelay;
			hold = g_DebugInteriorHold;
		}
	#endif

		f32 smoothingAdd = ((f32)(audNorthAudioEngine::GetCurrentTimeInMs()-interior.LastTimeForCalc))/g_InteriorTransitionTime;
		u32 smoothingFilters = (u32)(smoothingAdd*24000); 
		interior.LastTimeForCalc = audNorthAudioEngine::GetCurrentTimeInMs();
		

		if(wetness != interior.Wetness)
		{
			f32 smoothedWetness = wetness < interior.Wetness ? Max(wetness, interior.Wetness-smoothingAdd) : Min(wetness, interior.Wetness+smoothingAdd);
			interior.Wetness = smoothedWetness;
		}
		if(outsideVisability != interior.OutsideVisability)
		{
			f32 smoothedOutside = outsideVisability < interior.OutsideVisability ? Max(outsideVisability, interior.OutsideVisability-smoothingAdd) : Min(outsideVisability, interior.OutsideVisability+smoothingAdd);
			interior.OutsideVisability = smoothedOutside;
		}
		if(lowPass != interior.Lpf)
		{
			u32 smoothedLpf = lowPass < interior.Lpf ? Max(lowPass, interior.Lpf-smoothingFilters) : Min(lowPass, interior.Lpf+smoothingFilters);
			interior.Lpf = smoothedLpf;
		}

		if(predelay != interior.Predelay)
		{
			u32 smoothedPredelay = predelay < interior.Predelay ? Max(predelay, interior.Predelay-smoothingFilters) : Min(predelay, interior.Predelay+smoothingFilters);
			interior.Predelay = smoothedPredelay;
		}

		if(hold != interior.Hold)
		{
			float smoothedHold = hold < interior.Hold ? Max(hold, interior.Hold-smoothingFilters) : Min(hold, interior.Hold+smoothingFilters);
			interior.Hold = smoothedHold;			
		}
	}
}

void audWeaponAudioComponent::TriggerEmptyShot(CVehicle* parent, const CVehicleWeapon* vehicleWeapon)
{
	if(parent)
	{
		audVehicleAudioEntity* vehicleAudioEntity = parent->GetVehicleAudioEntity();

		if(vehicleAudioEntity && vehicleAudioEntity->IsFocusVehicle())
		{
			u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

			WeaponSettings * weaponSettings = GetWeaponSettings(parent);
			u32 weaponHash = vehicleWeapon->GetHash();

			if(weaponSettings)
			{
				// Special ability forces all auto-fire weapons into single shot

				if((((AUD_GET_TRISTATE_VALUE(weaponSettings->Flags, FLAG_ID_WEAPONSETTINGS_ISPLAYERAUTOFIRE)== AUD_TRISTATE_TRUE)) ||
					((AUD_GET_TRISTATE_VALUE(weaponSettings->Flags, FLAG_ID_WEAPONSETTINGS_ISNPCAUTOFIRE)== AUD_TRISTATE_TRUE))))
				{
					if(m_TimeLastEmptyShotFired + audWeaponAudioEntity::sm_AutoEmptyShotTimeFilter < now)
					{
						m_FrameLastEmptyShotFired = 0;
					}
				}
			}
			if(m_FrameLastEmptyShotFired + 1 <  fwTimer::GetFrameCount())
			{				
				audSoundSet dryFireSounds;
				dryFireSounds.Init(ATSTRINGHASH("EMPTY_WEAPON_SOUNDS", 0x13AC8CD3));

				if(dryFireSounds.IsInitialised())
				{
					audMetadataRef dryFireSound = dryFireSounds.Find(weaponHash);

					if(dryFireSound.IsValid())
					{
						audSoundInitParams initParams;
						initParams.Position = VEC3V_TO_VECTOR3(parent->GetTransform().GetPosition());
						initParams.TrackEntityPosition = true;
						vehicleAudioEntity->CreateAndPlaySound(dryFireSound, &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(dryFireSounds.GetNameHash(), weaponHash, &initParams, parent));
					}
				}

				m_TimeLastEmptyShotFired = now;
			}

			m_FrameLastEmptyShotFired = fwTimer::GetFrameCount();
		}		
	}
}

void audWeaponAudioComponent::TriggerEmptyShot(CPed * ped)
{
	u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	WeaponSettings * weaponSettings = GetWeaponSettings(ped);
	if(weaponSettings)
	{
		// Special ability forces all auto-fire weapons into single shot

		if((((AUD_GET_TRISTATE_VALUE(weaponSettings->Flags, FLAG_ID_WEAPONSETTINGS_ISPLAYERAUTOFIRE)== AUD_TRISTATE_TRUE)) ||
			((AUD_GET_TRISTATE_VALUE(weaponSettings->Flags, FLAG_ID_WEAPONSETTINGS_ISNPCAUTOFIRE)== AUD_TRISTATE_TRUE))))
		{
			if(m_TimeLastEmptyShotFired + audWeaponAudioEntity::sm_AutoEmptyShotTimeFilter < now)
			{
				m_FrameLastEmptyShotFired = 0;
			}
		}
	}
	if(m_FrameLastEmptyShotFired + 1 <  fwTimer::GetFrameCount())
	{
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(ped->GetTransform().GetPosition());

		audSoundSet dryFireSounds;
		audMetadataRef dryFireSound;
		dryFireSounds.Init(ATSTRINGHASH("EMPTY_WEAPON_SOUNDS", 0x13AC8CD3));
		if(dryFireSounds.IsInitialised() && ped && ped->GetWeaponManager())
		{
			dryFireSound = dryFireSounds.Find(ped->GetWeaponManager()->GetEquippedWeaponHash());
		}

		if(dryFireSound.IsValid())
		{
			g_WeaponAudioEntity.CreateAndPlaySound(dryFireSound, &initParams);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::ReplayRecordSound(ATSTRINGHASH("EMPTY_WEAPON_SOUNDS", 0x13AC8CD3), ped->GetWeaponManager()->GetEquippedWeaponHash(), &initParams, NULL, NULL, eWeaponSoundEntity);
			}
#endif
		}
		else
		{
			g_WeaponAudioEntity.CreateAndPlaySound(g_WeaponAudioEntity.FindSound(ATSTRINGHASH("EMPTY_FIRE", 0x90b0f84c)), &initParams);

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::ReplayRecordSound(ATSTRINGHASH("CODED_WEAPON_SOUNDS", 0xB148A7E7), ATSTRINGHASH("EMPTY_FIRE", 0x90b0f84c), &initParams, NULL, NULL, eWeaponSoundEntity);
			}
#endif
		}

		m_TimeLastEmptyShotFired = now;

		// Apply small trigger rumble effect synced with the sound
		CControlMgr::GetMainPlayerControl().ApplyRecoilEffect(50, 0.15f, 0.4f);
	}
	m_FrameLastEmptyShotFired = fwTimer::GetFrameCount();
}


void audWeaponAudioComponent::PlayWeaponFire(CEntity *parentEntity, bool isSilenced, bool forceSingleShot, bool isKillShot)
{
	// In case it's a script weapon
	if(m_IsScriptWeapon)
	{
		return;
	}

#if __BANK
	if(g_DebugWeaponTimings && parentEntity)
	{
		grcDebugDraw::Sphere(parentEntity->GetTransform().GetPosition(), 0.5f, Color32(255,255,0), true, 5);
	}
#endif

	naAssertf(g_WeaponAudioEntity.GetControllerId()!=AUD_INVALID_CONTROLLER_ENTITY_ID, "Invalid controller entity");

	if(parentEntity && parentEntity->GetType() == ENTITY_TYPE_PED)
	{
		const CWeapon* pWeapon = ((CPed*)parentEntity)->GetWeaponManager()->GetEquippedWeapon();
		if (isSilenced && pWeapon && pWeapon->GetSuppressorComponent() && g_WeaponAudioEntity.EquippedWeaponIsVisible((CPed*)parentEntity))
		{
			isSilenced = g_WeaponAudioEntity.SuppressorIsVisible(pWeapon);
		}
		((CPed*)parentEntity)->GetPedAudioEntity()->HandleWeaponFire(isSilenced);
	}
	if(GetWeaponSettings(parentEntity) == NULL)
	{
		return;
	}

	if(g_WeaponAudioEntity.GetControllerId()!=AUD_INVALID_CONTROLLER_ENTITY_ID && (g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame < g_MaxWeaponFireEventsPerFrame))
	{
		//Cache this weapon fire event so we can decide which ones to play later in the game frame.
		g_WeaponAudioEntity.sm_WeaponFireEventList[g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame].fireInfo.isSuppressed = isSilenced;
		g_WeaponAudioEntity.sm_WeaponFireEventList[g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame].fireInfo.audioSettingsRef = m_ItemSettings;
		g_WeaponAudioEntity.sm_WeaponFireEventList[g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame].fireInfo.parentEntity = parentEntity;
		g_WeaponAudioEntity.sm_WeaponFireEventList[g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame].fireInfo.forceSingleShot = forceSingleShot;
		g_WeaponAudioEntity.sm_WeaponFireEventList[g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame].fireInfo.settingsHash = m_SettingsHash;
		g_WeaponAudioEntity.sm_WeaponFireEventList[g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame].fireInfo.isKillShot = isKillShot;

		g_WeaponAudioEntity.sm_WeaponFireEventList[g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame].weapon = m_Weapon;

		g_WeaponAudioEntity.sm_NumWeaponFireEventsThisFrame++;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::CleanWaterCannonMapFlags()
{
	for(u32 i = 0; i < NUM_MATERIALTYPE; i++)
	{
		sm_WaterCannonHitMapInfo[ i].hitByWaterCannon = false;
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::CleanWaterCannonFlags()
{
	sm_WaterCannonOnPedsInfo.hitByWaterCannon = false;
	sm_WaterCannonOnVehInfo.hitByWaterCannon = false;
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::PlayWaterCannonHit(Vec3V_In hitPosition,const CEntity *hitEntity,phMaterialMgr::Id matId,u16 component)
{
	phMaterialMgr::Id unpackedMtlIdA = PGTAMATERIALMGR->UnpackMtlId(matId);
	CollisionMaterialSettings * materialSettings = g_CollisionAudioEntity.GetMaterialOverride(hitEntity, g_audCollisionMaterials[(s32)unpackedMtlIdA],component);
	MaterialType materialType = (MaterialType)materialSettings->MaterialType;
	sm_WaterCannonHitMapInfo[(u32)materialType].volume = 1.f;
	sm_WaterCannonHitMapInfo[(u32)materialType].hitByWaterCannon = true;
	if(!sm_WaterCannonHitMapInfo[(u32)materialType].sound)
	{
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(hitPosition);
		initParams.EnvironmentGroup = hitEntity? hitEntity->GetAudioEnvironmentGroup() : NULL;
		CreateAndPlaySound_Persistent(sm_WaterCannonSounds.Find(MaterialType_ToString(materialType)),&sm_WaterCannonHitMapInfo[(u32)materialType].sound,&initParams);
	}
	else
	{
		SmoothWaterCannonOnMap(1.f, 1.f,materialType);
		sm_WaterCannonHitMapInfo[(u32)materialType].sound->SetRequestedPosition(hitPosition);
	}
	//naDisplayf("map material: %s, component: %u", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(materialSettings->NameTableOffset), component);
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::PlayWaterCannonHitVehicle(CVehicle *vehicle, Vec3V_In hitPosition)
{
	sm_WaterCannonOnVehInfo.volume = 1.f;
	sm_WaterCannonOnVehInfo.hitByWaterCannon = true;
	if(!sm_WaterCannonOnVehInfo.sound)
	{
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(hitPosition);
		initParams.EnvironmentGroup = vehicle->GetAudioEnvironmentGroup();
		CreateAndPlaySound_Persistent(sm_WaterCannonSounds.Find(ATSTRINGHASH("VEHICLES", 0x74B31BE3)),&sm_WaterCannonOnVehInfo.sound,&initParams);
	}
	else
	{
		SmoothWaterCannonOnVeh(1.f, 1.f);
		sm_WaterCannonOnVehInfo.sound->SetRequestedPosition(hitPosition);
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::PlayWaterCannonHitPed(CPed *ped, Vec3V_In hitPosition)
{
	sm_WaterCannonOnPedsInfo.volume = 1.f;
	sm_WaterCannonOnPedsInfo.hitByWaterCannon = true;
	if(!sm_WaterCannonOnPedsInfo.sound)
	{
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(hitPosition);
		initParams.EnvironmentGroup = ped->GetAudioEnvironmentGroup();
		CreateAndPlaySound_Persistent(sm_WaterCannonSounds.Find(ATSTRINGHASH("PEDS", 0x8DA12117)),&sm_WaterCannonOnPedsInfo.sound,&initParams);
	}
	else
	{
		SmoothWaterCannonOnPed(1.f, 1.f);
		f32 distanceToListenerSqd = g_AudioEngine.GetEnvironment().ComputeSqdDistanceRelativeToVolumeListenerV(hitPosition).Getf();
		if(distanceToListenerSqd < m_WaterCannonPedHitDistSqd)
		{
			// So we only do it once each update.
			sm_WaterCannonOnPedsInfo.sound->SetRequestedPosition(hitPosition);
			m_WaterCannonPedHitDistSqd = distanceToListenerSqd;
		}
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::TriggerWeaponReloadConfirmation(Vector3& position, const CVehicle* vehicle)
{
	if(vehicle && ((CVehicleModelInfo*)(vehicle->GetBaseModelInfo()))->GetModelNameHash() == ATSTRINGHASH("CHERNOBOG", 0xD6BC7523))
	{
		TriggerVehicleWeaponSound(position, vehicle, ATSTRINGHASH("xm_vehicle_chernobog_missile_reloaded_master", 0xE707186C));
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::TriggerNotReadyToFireSound(Vector3& position, const CVehicle* vehicle)
{		
	if(vehicle && ((CVehicleModelInfo*)(vehicle->GetBaseModelInfo()))->GetModelNameHash() == ATSTRINGHASH("CHERNOBOG", 0xD6BC7523))
	{
		TriggerVehicleWeaponSound(position, vehicle, ATSTRINGHASH("xm_vehicle_chernobog_missile_empty_master", 0xB61DA797));
	}
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::TriggerVehicleWeaponSound(Vector3& position, const CVehicle* vehicle, const u32 soundNameHash REPLAY_ONLY(, bool record))
{
	if(vehicle)
	{		
		audSoundInitParams initParams;
		initParams.Position = position;
		CreateDeferredSound(soundNameHash, vehicle, &initParams, true);

#if GTA_REPLAY
		if(record)
		{
			CReplayMgr::ReplayRecordSound(soundNameHash, &initParams, vehicle);
		}
	}
#endif
}
//-------------------------------------------------------------------------------------------------------------------
void audWeaponAudioEntity::ProcessAnimEvents()
{
}

bool IsReloadHash(u32 hash)
{
	return hash == ATSTRINGHASH("Reload1", 0xEC423F2D) || hash == ATSTRINGHASH("Reload2", 0xAE97C3CD) || ATSTRINGHASH("Reload3", 0xA0452728);
}

void audWeaponAudioEntity::ProcessWeaponAnimEvents()
{
	for(int i=0; i< WEAPON_AUDENTITY_NUM_ANIM_EVENTS; i++)
	{
		if(sm_WeaponAnimEvents[i].hash != 0)
		{	
			CObject *weapon = sm_WeaponAnimEvents[i].weapon;
			if(weapon && weapon->GetWeapon())
			{
				audMetadataRef soundRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(sm_WeaponAnimEvents[i].hash);
				CPed *player = CGameWorld::FindLocalPlayer();
				bool isPlayerWeapon = false; 
				if(player)
				{
					if(player->GetWeaponManager())
					{
						CWeapon	*pWeapon = weapon->GetWeapon();
						CWeapon *playerWeapon = player->GetWeaponManager()->GetEquippedWeapon();
						isPlayerWeapon = (pWeapon == playerWeapon);
					}
				} 
				u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
				if(sm_WeaponAnimEvents[i].hash == weapon->GetWeapon()->GetAudioComponent().GetLastAnimHash())
				{
					if(now <= weapon->GetWeapon()->GetAudioComponent().GetLastAnimTime() + sm_WeaponAnimTimeFilter)
					{
						sm_WeaponAnimEvents[i].hash = 0;
						return;
					}
				}
				weapon->GetWeapon()->GetAudioComponent().SetLastAnimTime(now);
				weapon->GetWeapon()->GetAudioComponent().SetLastAnimHash(sm_WeaponAnimEvents[i].hash);

				WeaponSettings *settings = weapon->GetWeapon()->GetAudioComponent().GetWeaponSettings((isPlayerWeapon ? player : NULL));
				if(settings)
				{
					audSoundSet reloadSoundset;
					reloadSoundset.Init(settings->ReloadSounds);
					if(reloadSoundset.IsInitialised())
					{
						audMetadataRef soundSetComp = reloadSoundset.Find(sm_WeaponAnimEvents[i].hash);
						if(soundSetComp.IsValid())
						{
							soundRef = soundSetComp;							
						}
						else
						{
							// Reset the soundset so that we don't record it into the replay packet
							reloadSoundset.Reset();
						}
					}
					audSoundInitParams initParams;
					BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);
					initParams.Tracker = weapon->GetPlaceableTracker();
					const CPed* owningPed = weapon->GetWeapon()->GetOwner();
					if(owningPed)
					{
						initParams.EnvironmentGroup = owningPed->GetAudioEnvironmentGroup();
						if(owningPed->IsLocalPlayer())
						{
							bool isFirstPerson = audNorthAudioEngine::IsAFirstPersonCameraActive(CGameWorld::FindLocalPlayer(), false);
							if(isFirstPerson)
							{								
								initParams.Tracker = owningPed->GetPlaceableTracker(); // maybe a better tracker or position is needed???
							}
						}
					}
					initParams.UpdateEntity = true;
					initParams.AllowOrphaned = true;
					g_WeaponAudioEntity.CreateAndPlaySound( soundRef , &initParams);

					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(reloadSoundset.GetNameHash(), sm_WeaponAnimEvents[i].hash, &initParams, NULL, weapon, eWeaponSoundEntity));
				}
			}
		}
		//Reset event
		sm_WeaponAnimEvents[i].hash = 0;
		sm_WeaponAnimEvents[i].weapon = NULL;
	}
}

void audWeaponAudioEntity::PreUpdateService(u32 timeInMs)
{
	naAudioEntity::PreUpdateService(timeInMs);
	TriggerDeferredProjectileEvents();

	if(sm_Smooth)
	{
		if(sm_WaterCannonOnVehInfo.sound)
		{
			f32 newVolume = sm_WaterCannonOnVehInfo.smoother.CalculateValue(sm_WaterCannonOnVehInfo.volume, timeInMs);
			sm_WaterCannonOnVehInfo.sound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(newVolume));
			if(newVolume <= g_SilenceVolumeLin )
			{
				sm_WaterCannonOnVehInfo.sound->StopAndForget();
			}
		}
		if(sm_WaterCannonOnPedsInfo.sound)
		{
			f32 newVolume = sm_WaterCannonOnPedsInfo.smoother.CalculateValue(sm_WaterCannonOnPedsInfo.volume, timeInMs);
			sm_WaterCannonOnPedsInfo.sound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(newVolume));
			if(newVolume <= g_SilenceVolumeLin )
			{
				sm_WaterCannonOnPedsInfo.sound->StopAndForget();
			}
		}
		for (u32 i = 0; i < NUM_MATERIALTYPE; i++)
		{
			if(sm_WaterCannonHitMapInfo[i].sound)
			{
				f32 newVolume = sm_WaterCannonHitMapInfo[i].smoother.CalculateValue(sm_WaterCannonHitMapInfo[i].volume, timeInMs);
				sm_WaterCannonHitMapInfo[i].sound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(newVolume));
				if(newVolume <= g_SilenceVolumeLin )
				{
					sm_WaterCannonHitMapInfo[i].sound->StopAndForget();
				}
			}
		}
	}
}
void audWeaponAudioEntity::PostUpdate()
{
	ProcessWeaponAnimEvents();

	for(int i=0; i<g_MaxAudWeaponSpinEvents; i++)
	{
		sm_WeaponSpinEvents[i].HandleWeaponSpin();
	}
	for(int i=0; i<g_MaxAudCookProjectileEvents; i++)
	{
		sm_ProjectileCookEvents[i].HandleCookProjectile();
	}

	if(sm_NpcReportIntensity > 0.f)
	{
		sm_NpcReportIntensity = sm_NpcReportIntensity - sm_NpcReportIntensity*sm_NpcReportIntensityDecreaseRate*fwTimer::GetTimeStep();
		if(sm_NpcReportIntensity < 1.f)
		{
			sm_NpcReportIntensity = 0.f;
		}
	}

	if(!sm_WaterCannonOnVehInfo.hitByWaterCannon && sm_WaterCannonOnVehInfo.sound)
	{
		if(sm_Smooth)
		{
			SmoothWaterCannonOnVeh(1.f/(sm_TimeToMuteWaterCannon), 0.f);
		}
		else
		{
			sm_WaterCannonOnVehInfo.sound->StopAndForget();
		}
	}
	if(!sm_WaterCannonOnPedsInfo.hitByWaterCannon && sm_WaterCannonOnPedsInfo.sound)
	{
		if(sm_Smooth)
		{
			SmoothWaterCannonOnPed(1.f/(sm_TimeToMuteWaterCannon), 0.f);
		}
		else
		{
			sm_WaterCannonOnPedsInfo.sound->StopAndForget();
		}
	}
	for (u32 i = 0; i < NUM_MATERIALTYPE; i++)
	{
		if(!sm_WaterCannonHitMapInfo[i].hitByWaterCannon && sm_WaterCannonHitMapInfo[i].sound)
		{
			if(sm_Smooth)
			{
				SmoothWaterCannonOnMap(1.f/(sm_TimeToMuteWaterCannon), 0.f,(MaterialType)i);
			}
			else
			{
				sm_WaterCannonHitMapInfo[i].sound->StopAndForget();
			}
		}
	}
}
void audWeaponAudioEntity::SmoothWaterCannonOnMap(f32 rate,f32 desireLinVol,MaterialType matType)
{
	// Set rates and desire volume
	sm_WaterCannonHitMapInfo[matType].volume = desireLinVol;
	sm_WaterCannonHitMapInfo[matType].smoother.SetRates(rate,rate);
	// If the smoother hasn't been used yet, initialize it with the current volume (1 - desire)
	if(!sm_WaterCannonHitMapInfo[matType].smoother.IsInitialized())
	{
		sm_WaterCannonHitMapInfo[matType].smoother.CalculateValue(1.f-desireLinVol,audNorthAudioEngine::GetCurrentTimeInMs());
	}
}
void audWeaponAudioEntity::SmoothWaterCannonOnVeh(f32 rate,f32 desireLinVol)
{
	// Set rates and desire volume
	sm_WaterCannonOnVehInfo.volume = desireLinVol;
	sm_WaterCannonOnVehInfo.smoother.SetRates(rate,rate);
	// If the smoother hasn't been used yet, initialize it with the current volume (1 - desire)
	if(!sm_WaterCannonOnVehInfo.smoother.IsInitialized())
	{
		sm_WaterCannonOnVehInfo.smoother.CalculateValue(1.f-desireLinVol,audNorthAudioEngine::GetCurrentTimeInMs());
	}
}
void audWeaponAudioEntity::SmoothWaterCannonOnPed(f32 rate,f32 desireLinVol)
{
	// Set rates and desire volume
	sm_WaterCannonOnPedsInfo.volume = desireLinVol;
	sm_WaterCannonOnPedsInfo.smoother.SetRates(rate,rate);
	// If the smoother hasn't been used yet, initialize it with the current volume (1 - desire)
	if(!sm_WaterCannonOnPedsInfo.smoother.IsInitialized())
	{
		sm_WaterCannonOnPedsInfo.smoother.CalculateValue(1.f-desireLinVol,audNorthAudioEngine::GetCurrentTimeInMs());
	}
}

void audWeaponSpinEvent::HandleWeaponSpin()
{
	if(isActive && entity)
	{
		CWeapon * forcedWeapon = weapon.Get();
		if(entity->GetIsTypePed())
		{
			CPed * ped = (CPed*)entity.Get();
			if(ped->GetWeaponManager())
			{
				CWeapon * weapon = forcedWeapon ? forcedWeapon : ped->GetWeaponManager()->GetEquippedWeapon();

				if(weapon && (weapon->GetWeaponHash() == ATSTRINGHASH("weapon_minigun", 0x42BF8A85) || forcedWeapon))
				{
					bool playerAudioLoaded =  g_WeaponAudioEntity.GetEquippedWeaponAudioHash() == ATSTRINGHASH("AUDIO_ITEM_MINIGUN", 0x674A2973) || g_WeaponAudioEntity.GetEquippedWeaponAudioHash() == ATSTRINGHASH("AUDIO_ITEM_VALKYRIE_MINIGUN", 0x967CB54B) || g_WeaponAudioEntity.GetEquippedWeaponAudioHash() == ATSTRINGHASH("AUDIO_ITEM_RAYMINIGUN", 0x142840C6);
					//If the equipped weapon has not been set as minigun, we won't have the player waves loaded in e.g. a weapon is being forced but hasn't been equipped on the ped
					weapon->GetAudioComponent().SpinUpWeapon(entity, !playerAudioLoaded);
				}
			}
		}
		else if(entity->GetIsTypeVehicle())
		{
			CWeapon * weapon = forcedWeapon;
			
			if(weapon)
			{
				// Player spin up sound is specific to the minigun weapon bank, so we need to use the NPC version unless we're actually using the minigun
				weapon->GetAudioComponent().SpinUpWeapon(entity, weapon->GetWeaponHash() != ATSTRINGHASH("weapon_minigun", 0x42BF8A85), boneIndex);			
			}
		}
		else if(entity->GetIsTypeObject())
		{
			CObject *object = (CObject *)entity.Get();
			if(object)
			{
				CWeapon *weapon = forcedWeapon ? forcedWeapon : object->GetWeapon();
				if(weapon)
				{
					// Player spin up sound is specific to the minigun weapon bank, so we need to use the NPC version unless we're actually using the minigun
					weapon->GetAudioComponent().SpinUpWeapon(weapon->GetOwner(), weapon->GetWeaponHash() != ATSTRINGHASH("weapon_minigun", 0x42BF8A85));
				}
			}
		}
	}
	isActive = false;
	entity = NULL;
	weapon = NULL;
}

void audProjectileCookEvent::HandleCookProjectile()
{
	if(isActive && entity)
	{
		if(entity->GetIsTypePed())
		{
			CPed * ped = (CPed*)entity.Get();
			if(ped->GetWeaponManager())
			{
				CWeapon * weapon = ped->GetWeaponManager()->GetEquippedWeapon();
				if(weapon)
				{
					weapon->GetAudioComponent().PlayCookProjectileSound(entity);
				}
			}
		}
		else if(entity->GetIsTypeVehicle())
		{
			CVehicle * vehicle = (CVehicle *)entity.Get();
			CPed * ped = vehicle->GetDriver();
			if(ped && ped->GetWeaponManager())
			{
				CWeapon * weapon = ped->GetWeaponManager()->GetEquippedWeapon();
				if(weapon)
				{
					weapon->GetAudioComponent().PlayCookProjectileSound(entity);
				}
			}
		}
	}
	isActive = false;
	entity = NULL;
}

void audWeaponAudioEntity::SpinUpWeapon(const CEntity * parentEntity, CWeapon * weapon, s32 boneIndex)
{
	for(int i=0; i < g_MaxAudWeaponSpinEvents; i++)
	{
		if(!sm_WeaponSpinEvents[i].isActive)
		{
			sm_WeaponSpinEvents[i].isActive = true;
			sm_WeaponSpinEvents[i].entity = parentEntity;
			sm_WeaponSpinEvents[i].weapon = weapon;
			sm_WeaponSpinEvents[i].boneIndex = boneIndex;
			break;
		}
	}
}

void audWeaponAudioEntity::CookProjectile(const CEntity * parentEntity)
{
	for(int i=0; i < g_MaxAudCookProjectileEvents; i++)
	{
		if(!sm_ProjectileCookEvents[i].isActive)
		{
			sm_ProjectileCookEvents[i].isActive = true;
			sm_ProjectileCookEvents[i].entity = parentEntity;
			break;
		}
	}
}

void audWeaponAudioComponent::SpinUpWeapon(const CEntity * parentEntity, bool forceNPCVersion, s32 boneIndex)
{
	if(!m_SpinSound)
	{
		m_SpinOwner = parentEntity;
		m_SpinBoneIndex = boneIndex;
		m_ForceNPCVersion = forceNPCVersion;
		audWeaponAudioEntity::PlaySpinUpSound(parentEntity, &m_SpinSound, forceNPCVersion, false, boneIndex);		
	}
}

void audWeaponAudioComponent::CookProjectile(const CEntity * parentEntity)
{
	if(!m_SpinSound)
	{
		g_WeaponAudioEntity.CookProjectile(parentEntity);
	}
}

void audWeaponAudioComponent::PlayCookProjectileSound(const CEntity * entity)
{
	if(!m_SpinSound && entity && entity->GetAudioEntity())
	{	audSoundInitParams initParams;
		initParams.TrackEntityPosition = true;

		entity->GetAudioEntity()->CreateAndPlaySound_Persistent(g_WeaponAudioEntity.FindSound(ATSTRINGHASH("GrenadeTimeout", 0xD18540C)), &m_SpinSound, &initParams);
	}
}

void audWeaponAudioComponent::StopCookProjectile()
{
	if(m_SpinSound)
	{
		m_SpinSound->StopAndForget();
	}
}

void audWeaponAudioComponent::SpinDownWeapon()
{
	if(m_SpinSound)
	{
		m_SpinSound->StopAndForget(true);
	} 
}

void audWeaponAudioComponent::UpdateSpin()
{
#if GTA_REPLAY
	if(CReplayMgr::IsRecording() && m_SpinSound && m_SpinOwner)
	{

		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordPersistantFx<CPacketWeaponSpinUp>(
				CPacketWeaponSpinUp(true, m_SpinBoneIndex, m_ForceNPCVersion), 
				CTrackedEventInfo<tTrackedSoundType>(m_SpinSound),
				m_SpinOwner,
				true);
		}
	}
#endif
}

void audWeaponAudioEntity::TriggerFlashlight(const CWeapon* weapon,bool active)
{
	if(weapon)
	{
		audSoundInitParams initParams;
		initParams.Position = VEC3V_TO_VECTOR3(weapon->GetEntity()->GetTransform().GetPosition());
		initParams.Volume = 0.f;

		u32 fieldHash = ATSTRINGHASH("rifleFlashlight", 0x1EEE94D5);
	
		if(weapon->GetWeaponHash() == ATSTRINGHASH("WEAPON_FLASHLIGHT", 0x8BB05FD7))
		{
			if(active)
			{
				fieldHash = ATSTRINGHASH("flashlightOn", 0x5EF7274F);
			}
			else
			{
				fieldHash = ATSTRINGHASH("flashlightOff", 0x66ECAB94);
			}
		}
		
		CreateAndPlaySound(FindSound(fieldHash), &initParams);

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::ReplayRecordSound(ATSTRINGHASH("CODED_WEAPON_SOUNDS", 0xB148A7E7), fieldHash, &initParams, NULL, NULL, eWeaponSoundEntity);
		}
#endif
	}
}

void audWeaponAudioEntity::ToggleThermalVision(bool turnOn, bool forcePlay)
{
	if(!turnOn && (sm_ThermalGogglesOn || forcePlay))
	{
		if(sm_ThermalGoggleSound)
		{
			sm_ThermalGoggleSound->StopAndForget();
		}
		sm_PlayThermalGogglesSound = ATSTRINGHASH("ThermalVisionOff", 0x5A1DC4BB); // off sound
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketThermalScopeAudio>(CPacketThermalScopeAudio(turnOn));
		}
#endif
	}
	else if(turnOn && (!sm_ThermalGogglesOn || forcePlay))
	{
		if(sm_ThermalGoggleOffSound)
		{
			sm_ThermalGoggleOffSound->StopAndForget();
		}
		sm_PlayThermalGogglesSound = ATSTRINGHASH("ThermalVision", 0x655D7D08);
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketThermalScopeAudio>(CPacketThermalScopeAudio(turnOn));
		}
#endif

	}

	sm_ThermalGogglesOn = turnOn;

}

void audWeaponAudioEntity::UpdateThermalVision()
{
	if(sm_PlayThermalGogglesSound != 0)
	{
		audSoundInitParams initParams;
		initParams.Pan = 0;
		if(sm_PlayThermalGogglesSound == ATSTRINGHASH("ThermalVisionOff", 0x5A1DC4BB)) // off sound
		{
			if(sm_ThermalGoggleOffSound)
			{
				sm_ThermalGoggleOffSound->StopAndForget();
			}
			CreateAndPlaySound_Persistent(FindSound(sm_PlayThermalGogglesSound), &sm_ThermalGoggleOffSound, &initParams); 
		}
		else if(sm_PlayThermalGogglesSound == ATSTRINGHASH("ThermalVision", 0x655D7D08)) // on sound
		{
			if(sm_ThermalGoggleSound)
			{
				sm_ThermalGoggleSound->StopAndForget();
			}
			CreateAndPlaySound_Persistent(FindSound(sm_PlayThermalGogglesSound), &sm_ThermalGoggleSound, &initParams); 
		}
		sm_PlayThermalGogglesSound = 0;
	}
}


void audWeaponAudioEntity::TriggerStickyBombAttatch(Vec3V_In position, CEntity* stuckEntity, u32 stuckComponent, phMaterialMgr::Id matId, float impulse, CEntity* owner, audMetadataRef stickSoundRef)
{
	audSoundInitParams initParams;
	initParams.Position = VEC3V_TO_VECTOR3(position);
	initParams.EnvironmentGroup = g_CollisionAudioEntity.GetOcclusionGroup(position, stuckEntity, matId);
	initParams.IsStartOffsetPercentage = true;
	initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_StickyBombImpulseVolumeCurve.CalculateValue(impulse));

	if(!sm_StickyBombImpulseVolumeCurve.IsValid())
	{
		initParams.Volume = 0.f;
	}


	audPedAudioEntity * pedAudio = NULL;
	if(owner && owner->GetIsTypePed())
	{
		pedAudio = ((CPed*)owner)->GetPedAudioEntity();
	}

	static u32 lastAttatchTime = 0;

	if(pedAudio)
	{
		lastAttatchTime = pedAudio->GetLastStickyBombTime();
	}

	if(lastAttatchTime + 100 > g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
	{
		return;
	}

	lastAttatchTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	if(pedAudio)
	{
		pedAudio->SetLastStickyBombTime(lastAttatchTime);
	}

	s32 mati = 0; 
	if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(matId)<(phMaterialMgr::Id)g_audCollisionMaterials.GetCount(), "Unpacked mtl id too large"))
	{
		if(naVerifyf(PGTAMATERIALMGR->UnpackMtlId(matId)<(phMaterialMgr::Id)0xffff, "Unpacked mtl id too big for s32 cast"))
		{ 
			mati = static_cast<s32>(PGTAMATERIALMGR->UnpackMtlId(matId));
		}
	}

	CollisionMaterialSettings * hitSettings = g_CollisionAudioEntity.GetMaterialOverride(stuckEntity,g_audCollisionMaterials[mati], stuckComponent);

	bool isVehicle = false;
	
	if(stuckEntity && stuckEntity->GetIsTypeVehicle() && stuckEntity->GetAudioEntity())
	{
		isVehicle = true;
		hitSettings = ((audVehicleAudioEntity *)stuckEntity->GetAudioEntity())->GetCollisionAudio().GetVehicleCollisionMaterialSettings();
	}

	u32 hitSound = g_NullSoundHash, softHitSound = g_NullSoundHash;

	//naErrorf("Sticky bomb material %s (base %s)", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(hitSettings->NameTableOffset), audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(g_audCollisionMaterials[mati]->NameTableOffset)); 

	if(hitSettings)
	{

		if(hitSettings->HardImpact != g_NullSoundHash)
		{
			hitSound = hitSettings->HardImpact;
		}
		else
		{
			hitSound = hitSettings->SolidImpact;
		}
		
		softHitSound = hitSettings->SoftImpact;

		if(isVehicle)
		{
			hitSound = hitSettings->BulletImpactSound;
		}

		CreateAndPlaySound(hitSound, &initParams);
		CreateAndPlaySound(softHitSound, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(hitSound, &initParams, NULL, NULL, eWeaponSoundEntity));
		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(softHitSound, &initParams,  NULL, NULL, eWeaponSoundEntity));
	}

	//if(initParams.Volume == 0.f)
	//{
	//	audNorthAudioEngine::GetGtaEnvironment()->SpikeResonanceObjects(impulse, VEC3V_TO_VECTOR3(position), 0.5f);
	//}

	initParams.Volume = 0.f;

	CreateAndPlaySound(stickSoundRef, &initParams);	 

	//TODO: need to change this to account for the possibility of having a proximity mine instead of a sticky bomb
	REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_CodeSounds.GetNameHash(), ATSTRINGHASH("sticky_bomb_hit", 2782906396), &initParams, NULL, NULL, eWeaponSoundEntity));
}

void audWeaponAudioEntity::PlaySpinUpSound(const CEntity * parentEntity, audSound ** spinSound, bool forceNPCVersion, bool skipSpinUp, s32 boneIndex)
{
	const audEnvironmentGroupInterface* occlusionGroup = NULL;
	Vector3 sourcePosition;
	sourcePosition.Zero();
	const audTracker *tracker = parentEntity->GetPlaceableTracker();

	if(fwTimer::GetFrameCount() == sm_BlockWeaponSpinFrame)
	{
		return;
	}

	if(spinSound)
	{
		if(*spinSound)
		{
			return;
		}

		if (parentEntity && parentEntity->GetIsTypePed())
		{
			occlusionGroup = ((CPed*)parentEntity)->GetWeaponEnvironmentGroup();
		}
		else if (parentEntity && parentEntity->GetIsTypeVehicle())
		{
			occlusionGroup = ((CVehicle*)parentEntity)->GetAudioEnvironmentGroup(); 
		}

		if(tracker)
		{
			sourcePosition = tracker->GetPosition();
		}
		else if(parentEntity)
		{
			sourcePosition = VEC3V_TO_VECTOR3(parentEntity->GetTransform().GetPosition());
		}

		audSoundInitParams initParams; 
		BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);

		audEntity* audioEntity = &g_WeaponAudioEntity;

		if(boneIndex != -1 && parentEntity && parentEntity->GetIsTypeVehicle() && parentEntity->GetAudioEntity())
		{
			audioEntity = parentEntity->GetAudioEntity();
			initParams.UpdateEntity = true;
			initParams.u32ClientVar = (u32(AUD_VEHICLE_SOUND_CUSTOM_BONE)&0xffff) | ((u32)boneIndex << 16);
		}
		else
		{
			initParams.Tracker = tracker;
			initParams.Position = sourcePosition;
		}
		
		initParams.EnvironmentGroup = occlusionGroup;  
		if(sm_PlayerEquippedWeaponSlot.waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->IsLoaded())
		{
			initParams.WaveSlot = sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot;
		}
		
		bool isPlayerWeapon = parentEntity && ((parentEntity->GetIsTypePed() && ((CPed*)parentEntity)->IsLocalPlayer()) || (parentEntity->GetIsTypeVehicle() && parentEntity == FindPlayerVehicle()));
		bool useTampa3DualMinigunSounds = false;
		bool useRaygunSounds = false;		
		bool useVehicleRaygunSounds = false;		

		if (parentEntity->GetIsTypePed())
		{
			const CWeaponInfo *pWeaponInfo = ((CPed*)parentEntity)->GetWeaponManager() ? ((CPed*)parentEntity)->GetWeaponManager()->GetEquippedWeaponInfo() : NULL;
			if (pWeaponInfo && pWeaponInfo->GetAudioHash() == ATSTRINGHASH("AUDIO_ITEM_RAYMINIGUN", 0x142840C6))
			{
				useRaygunSounds = true;
			}
			else if (pWeaponInfo && pWeaponInfo->GetAudioHash() == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_MINIGUN_LASER", 0x55B60F3))
			{
				useVehicleRaygunSounds = true;
			}
		}
		
		if(parentEntity && parentEntity->GetIsTypeVehicle())
		{
			CVehicle* parentVehicle = (CVehicle*)parentEntity;
			CVehicleModelInfo *model = (CVehicleModelInfo*)(parentVehicle->GetBaseModelInfo());

			if(model->GetModelNameHash() == ATSTRINGHASH("TAMPA3", 0xB7D9F7F1))
			{
				const u32 roofModIndex = parentVehicle->GetVariationInstance().GetModIndex(VMT_ROOF);

				if(roofModIndex != INVALID_MOD)
				{
					useTampa3DualMinigunSounds = true;
				}
			}
			else if (model->GetModelNameHash() == ATSTRINGHASH("OPPRESSOR2", 0x7B54A9D3))
			{
				isPlayerWeapon = false;
			}
			else if (model->GetModelNameHash() == ATSTRINGHASH("Deathbike2", 0x93F09558))
			{
				const u32 chassisModIndex = parentVehicle->GetVariationInstance().GetModIndex(VMT_CHASSIS5);

				if (chassisModIndex != INVALID_MOD)
				{
					useVehicleRaygunSounds = true;
				}
			}
		}

		if(isPlayerWeapon && !g_PlayerUsesNpcWeapons && !forceNPCVersion)
		{
			if(sm_SkipMinigunSpinUp || skipSpinUp )
			{
				if (useVehicleRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("w_vehicle_SPL_RAYMINIGUN_SERVO_NOSPINUP_MASTER", 0x22BFA04C), spinSound, &initParams);
				}
				else if (useRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("DLC_AW_SPL_RAYMINIGUN_SERVO_NOSPINUP_MASTER", 0x50A9E6FF), spinSound, &initParams);
				}
				else if(useTampa3DualMinigunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("gr_vehicle_tampa_dualminigun_servo_nospinup_master", 0x2259A591), spinSound, &initParams);
				}
				else
				{
					audioEntity->CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("MINIGUN_SPIN_NO_INTRO", 0x351B1EDA)), spinSound, &initParams);
				}
			}
			else
			{
				if (useVehicleRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("w_vehicle_SPL_RAYMINIGUN_SERVO_MASTER", 0xDDE02672), spinSound, &initParams);
				}
				if (useRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("DLC_AW_SPL_RAYMINIGUN_SERVO_MASTER", 0x808BC3B3), spinSound, &initParams);					
				}
				else if(useTampa3DualMinigunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("gr_vehicle_tampa_dualminigun_servo_spinup_master", 0xC887C2CB), spinSound, &initParams);
				}	
				else
				{
					audioEntity->CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("MINIGUN_SPIN", 0x930026)), spinSound, &initParams);
				}
			}
		}
		else
		{
			if(sm_SkipMinigunSpinUp || skipSpinUp)
			{
				if (useVehicleRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("w_vehicle_SPL_RAYMINIGUN_NPC_SERVO_NOSPINUP_MASTER", 0x100BCCEE), spinSound, &initParams);
				}
				else if (useRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("DLC_AW_SPL_RAYMINIGUN_NPC_SERVO_NOSPINUP_MASTER", 0x9D88B2D9), spinSound, &initParams);
				}
				else if(useTampa3DualMinigunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("gr_vehicle_tampa_dualminigun_npc_servo_nospinup_master", 0x4D4EB33), spinSound, &initParams);
				}
				else
				{
					audioEntity->CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("MINIGUN_NPC_NO_INTRO", 0x9169397)), spinSound, &initParams);
				}
			}
			else
			{
				if (useVehicleRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("w_vehicle_SPL_RAYMINIGUN_NPC_SERVO_MASTER", 0xF581CBB), spinSound, &initParams);
				}
				else if (useRaygunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("DLC_AW_SPL_RAYMINIGUN_NPC_SERVO_MASTER", 0x1124C582), spinSound, &initParams);
				}
				else if(useTampa3DualMinigunSounds)
				{
					audioEntity->CreateAndPlaySound_Persistent(ATSTRINGHASH("gr_vehicle_tampa_dualminigun_npc_servo_spinup_master", 0x682AC498), spinSound, &initParams);
				}
				else
				{
					audioEntity->CreateAndPlaySound_Persistent(FindSound(ATSTRINGHASH("MINIGUN_NPC", 0xcb2d3b70)), spinSound, &initParams);
				}
			}
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordPersistantFx<CPacketWeaponSpinUp>(
				CPacketWeaponSpinUp(skipSpinUp, boneIndex, forceNPCVersion), 
				CTrackedEventInfo<tTrackedSoundType>(*spinSound),
				(CEntity*)parentEntity,
				true);
		}
#endif // GTA_REPLAY
	}
}

void audWeaponAudioEntity::PlayWeaponReport(audWeaponFireInfo * fireInfo, const audWeaponInteriorMetrics * interior, const WeaponSettings * weaponSettings, float fDist, bool dontPlayEcho)
{
#if !__FINAL
	if (PARAM_noweaponecho.Get())
		return;
#endif

	if(!fireInfo || fireInfo->isSuppressed)
	{
		return;
	}

	f32 outsideVisability = interior->OutsideVisability;
	f32 interiorVolumeScaling = 1.f;

	if(outsideVisability < 1.f)
	{
		interiorVolumeScaling = sm_OutsideVisibilityReportVolumeCurve.CalculateValue(outsideVisability);
	}

	if(interiorVolumeScaling == 0.f)
	{
		return;
	}

	f32 markerSpeed = 1.0f;
#if GTA_REPLAY
	if(audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		markerSpeed = CReplayMgr::GetMarkerSpeed();
#if __BANK
		if(audNorthAudioEngine::IsForcingSlowMoVideoEditor())
			markerSpeed = 0.2f;
		if(audNorthAudioEngine::IsForcingSuperSlowMoVideoEditor())
			markerSpeed = 0.05f;
#endif
	}
#endif
	bool cinematicSlowMo = audNorthAudioEngine::IsInCinematicSlowMo();

	Vector3 position = VEC3V_TO_VECTOR3(fireInfo->parentEntity->GetTransform().GetPosition());

	s8 weaponSlotForCustomEchos = -1;

	audSoundInitParams initParams;
	if(sm_PlayerEquippedWeaponSlot.waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->IsLoaded())
	{
		initParams.WaveSlot = sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot;
	}
	BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);
	initParams.AllowOrphaned = true;
	if (fireInfo->parentEntity && fireInfo->parentEntity->GetIsTypePed() && ((CPed*)fireInfo->parentEntity.Get())->GetPedAudioEntity() )
	{
		initParams.Volume = ((CPed*)fireInfo->parentEntity.Get())->GetPedAudioEntity()->GetGunfightVolumeOffset();
	}
	if(fireInfo->parentEntity && fireInfo->parentEntity->GetIsTypePed() && ((CPed*)fireInfo->parentEntity.Get())->IsLocalPlayer() && !g_PlayerUsesNpcWeapons)
	{
		CPed* ped =(CPed*)fireInfo->parentEntity.Get();

		if(ped->GetSelectedWeaponInfo())
			weaponSlotForCustomEchos = (s8)ped->GetSelectedWeaponInfo()->GetWeaponWheelSlot();

		f32 fVolume[4];
		s32 sPredelay[4];
		Vector3 vPos[4];
		u32 bestIndex = 0;
		u32 secondBestIndex = 1;

		//const sdAudio::GlobalWeaponParams* pGlobalWeaponGameParams = rdrAudioManager::GetGlobalWeaponParams();

		f32 scale = 1.f;
		if (fDist>=0 && PARAM_rdrreportaudio.Get())
		{
			f32 fUnClampedScale = 1.0f - (fDist / g_MaxDistanceForEnemyReport /*pGlobalWeaponGameParams->MaxDistanceForEnemyReport*/);
			//fUnClampedScale *= 1.0f - rdrAudioManager::GetAudioEnvironment()->GetAmountToOccludeOutsideWorldAfterPortals();
			scale = Clamp(fUnClampedScale, 0.0f, 1.0f);
		}

		GetReportMetrics(fireInfo, weaponSettings, vPos, fVolume, sPredelay, &bestIndex, &secondBestIndex);

		Vec3V firePos = fireInfo->parentEntity->GetTransform().GetPosition();

		f32 gunfightVolumeOffset = initParams.Volume;
		initParams.Volume += audDriverUtil::ComputeDbVolumeFromLinear(audDriverUtil::ComputeLinearVolumeFromDb(fVolume[bestIndex]) * scale * interiorVolumeScaling);

		if(initParams.Volume > g_SilenceVolume)
		{
			Vector3 vFinalPos(vPos[bestIndex]+VEC3V_TO_VECTOR3(firePos));
			if(PARAM_rdrreportaudio.Get())
			{
				vFinalPos.Add(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()));
			}

			initParams.Position = vFinalPos;
			initParams.Predelay = sPredelay[bestIndex];
			//if (AUD_GET_TRISTATE_VALUE(pWeaponParams->Flags, sdAudio::FLAG_ID_WEAPONPARAMS_INDIVIDUALMULTIONESHOTS)==AUD_TRISTATE_TRUE)
			//	PlayIndividualMultiOneshots(pWeaponParams->Player.Rapport, &initParams);
			//else

			u32 reportSound = weaponSettings->ReportSound;
			if(audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
			{
				if((cinematicSlowMo || markerSpeed  <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && weaponSettings->SlowMoReportSound != g_NullSoundHash)
				{
					reportSound = weaponSettings->SlowMoReportSound;
				}
				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && weaponSettings->SuperSlowMoReportSound != g_NullSoundHash)
				{
					reportSound = weaponSettings->SuperSlowMoReportSound;
				}
			}

			g_WeaponAudioEntity.CreateAndPlaySound(reportSound, &initParams);

			//second report
			initParams.Volume = gunfightVolumeOffset + audDriverUtil::ComputeDbVolumeFromLinear(audDriverUtil::ComputeLinearVolumeFromDb(fVolume[secondBestIndex]) * scale * interiorVolumeScaling);

			if(initParams.Volume > g_SilenceVolume)
			{
				//vPos[secondBestIndex].Scale(100.0f);

				Vector3 vFinalPos(vPos[secondBestIndex]+VEC3V_TO_VECTOR3(firePos));
				if(PARAM_rdrreportaudio.Get())
				{
					vFinalPos.Add(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()));
				}

				initParams.Position = vFinalPos;
				initParams.Predelay = sPredelay[secondBestIndex];
				//if (AUD_GET_TRISTATE_VALUE(pWeaponParams->Flags, sdAudio::FLAG_ID_WEAPONPARAMS_INDIVIDUALMULTIONESHOTS)==AUD_TRISTATE_TRUE)
				//	PlayIndividualMultiOneshots(pWeaponParams->Player.Rapport, &initParams);
				//else
				g_WeaponAudioEntity.CreateAndPlaySound(reportSound, &initParams);
			}
		}
	}	
	else if(fireInfo->parentEntity)  
	{
		if(sm_NpcReportIntensity > sm_IntensityForNpcReportFullCulling)
		{
			return;
		}		
		initParams.Position = position;
		initParams.EnvironmentGroup = fireInfo->parentEntity->GetAudioEnvironmentGroup();
		initParams.UpdateEntity = true;
		initParams.AllowOrphaned = true;

		initParams.Volume += audDriverUtil::ComputeDbVolumeFromLinear(interiorVolumeScaling);
		if(initParams.Volume <= g_SilenceVolume)
		{
			return;
		}

		if(sm_NpcReportIntensity > sm_IntensityForNpcReportDistanceCulling && 
			MagSquared(VECTOR3_TO_VEC3V(position) - g_AudioEngine.GetEnvironment().GetPanningListenerPosition()).Getf() > sm_NpcReportCullingDistanceSq)
		{
			return;
		}

		u32 reportSound = weaponSettings->ReportSound;
		if((cinematicSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && weaponSettings->SlowMoReportSound != g_NullSoundHash)
		{
			reportSound = weaponSettings->SlowMoReportSound;
		}
		if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && weaponSettings->SuperSlowMoReportSound != g_NullSoundHash)
		{
			reportSound = weaponSettings->SuperSlowMoReportSound;
		}

		g_WeaponAudioEntity.CreateAndPlaySound(reportSound, &initParams);
		sm_NpcReportIntensity += 1.f;
	}

	u32 echoSound = fireInfo->GetWeaponEchoSound(weaponSettings);

	if(!dontPlayEcho && echoSound && echoSound != g_NullSoundHash && fireInfo->parentEntity && !fireInfo->parentEntity->GetIsInInterior()) 
	{
		f32 initialAttenuation = 0.0f;
		EnvironmentRule* envRule = g_AmbientAudioEntity.GetAmbientEnvironmentRule();
		bool overrideEchos = envRule && AUD_GET_TRISTATE_VALUE(envRule->Flags, FLAG_ID_ENVIRONMENTRULE_OVERRIDEECHOS);
		if(envRule && overrideEchos)
		{
			if(envRule->EchoSoundList != 0 && weaponSlotForCustomEchos != -1)
			{
				audSoundSet echoSet;
				echoSet.Init(envRule->EchoSoundList);
				if(echoSet.IsInitialised())
				{
					u32 customEchoSound = GetCustomEchoSoundForWeapon(echoSet, (eWeaponWheelSlot)weaponSlotForCustomEchos);
					if(customEchoSound != 0 && customEchoSound != g_NullSoundHash)
						echoSound = customEchoSound;
				}
			}
			initialAttenuation = g_AmbientAudioEntity.GetAmbientEnvironmentRuleScale() * envRule->BaseEchoVolumeModifier;
		}
		PlayTwoEchos(echoSound, 0, position, initialAttenuation, 0, 0, overrideEchos ? envRule : NULL);
	}
}

void audWeaponAudioEntity::SetReportGoodness(u8 index, f32 echoSuitability)
{
	if(naVerifyf(index < sm_ReportGoodness.GetMaxCount(), "Inappropriate index value passed into SetReportGoodness"))
	{
		if(g_PositionReportOppositeEcho)
			sm_ReportGoodness[index] = 1.0f - echoSuitability;
		else
			sm_ReportGoodness[index] = echoSuitability;

		sm_ReportGoodness[index] = Clamp(sm_ReportGoodness[index], 0.0f, 1.0f);
	}
}

void audWeaponAudioEntity::SetPlayerDoubleActionWeaponPhase(const CWeapon* weapon, f32 prevPhase, f32 currentPhase)
{
	if(weapon)
	{
		u32 soundsetName = 0;

		switch(weapon->GetWeaponInfo()->GetHash())
		{
		case 0x97EA20B8: // WEAPON_DOUBLEACTION
			soundsetName = ATSTRINGHASH("PTL_DOUBLEACTION_REVOLVER_PLYR_RELOADS_MONO_SS", 0x3F4A7C77);
			break;

		case 0xCB96392F: // WEAPON_REVOLVER_MK2
			soundsetName = ATSTRINGHASH("PTL_HEAVY_REVOLVER_MK2_PLYR_RELOADS_MONO_SS", 0x727A89DE);
			break;

		default:
			break;
		}

		if(soundsetName != 0u)
		{
			const u32 timeMs = fwTimer::GetTimeInMilliseconds();

			if(timeMs - m_LastDoubleActionWeaponSoundTime > 200)
			{
				u32 soundHash = 0;

				if(prevPhase == 0.0f && currentPhase > 0.0f)
				{
					soundHash = ATSTRINGHASH("Hammer_Pull", 0xFABB388);			
				}
				else if(prevPhase > 0.0f && currentPhase == 0.0f)
				{
					soundHash = ATSTRINGHASH("Hammer_Reset", 0x7AFE17AA);
				}

				if(soundHash != 0u)
				{
					audSoundSet soundSet;

					if(soundSet.Init(soundsetName))
					{
						CreateDeferredSound(soundSet.Find(soundHash), weapon->GetEntity(), NULL, true, true);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundSet.GetNameHash(), soundHash, NULL, NULL, weapon->GetEntity(), eWeaponSoundEntity));
						m_LastDoubleActionWeaponSoundTime = timeMs;
					}
				}
			}
		}
	}
}

void audWeaponAudioEntity::GetReportMetrics(const audWeaponFireInfo * fireInfo, const WeaponSettings * settings, Vector3* vPos, f32* fVolume, s32* sPredelay, u32* bestIndex, u32* secondBestIndex)
{
	f32 fReportVolumeDelta = settings->ReportVolumeDelta; // -2.0f;
	u32 sReportPredelayDelta = settings->ReportPredelayDelta; // 50;

	naAssertf(bestIndex && secondBestIndex, "Failed to pass in valid pointers for best indicies.");
	f32 fGoodness[4];
	f32 fBestGoodness = 0.f;
	u8 uBestIndex = 0;

	for(u8 i=0; i<4; i++)
	{
		u8 uMaxIndex = (u8)sm_ReportGoodness.size();
		u8 uIndex1=(i*2)%uMaxIndex;
		u8 uIndex2=(uIndex1+1)%uMaxIndex;
		u8 uIndex3=(uIndex1+2)%uMaxIndex;

		vPos[i] = sm_RelativeScanPositions[uIndex2*3];

		fGoodness[i] = sm_ReportGoodness[uIndex1] + sm_ReportGoodness[uIndex2] + sm_ReportGoodness[uIndex3];
		fGoodness[i] /= 3.0f;

		if(fGoodness[i] > fBestGoodness)
		{
			fBestGoodness = fGoodness[i];
			uBestIndex = i;
		}
	}

	Vector3 listenerPosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
	Vector3 positionOnUnitCircle = VEC3V_TO_VECTOR3(fireInfo->parentEntity->GetTransform().GetPosition()) - listenerPosition;
	positionOnUnitCircle.z = 0.0f;
	positionOnUnitCircle.NormalizeSafe();
	f32 elevation = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition()).GetZ();


	//vPos[0].x = positionOnUnitCircle.x;
	//vPos[0].y = positionOnUnitCircle.y;
	//vPos[0].z = elevation;

	//If we're the player, let's assume the direction is straight ahead.
	CEntity * parentEntity = fireInfo->parentEntity;
	if(parentEntity && parentEntity->GetIsTypePed() && ((CPed*)parentEntity)->IsLocalPlayer() && !g_PlayerUsesNpcWeapons)
	{
		vPos[0].Scale(100.0f);
		vPos[0].z = elevation;

		vPos[1].Scale(100.0f);
		vPos[1].z = elevation;

		vPos[2].Scale(100.0f);
		vPos[2].z = elevation;

		vPos[3].Scale(100.0f);
		vPos[3].z = elevation;
	}
	else
	{
		f32 cosA = positionOnUnitCircle.y;
		f32 sinA = -positionOnUnitCircle.x;

		f32 cosB = 0; //B is 90...the amount we're going to increment through each step
		f32 sinB = 1;

		for(int i = 1; i < 4; i++)
		{
			f32 newCos = cosA*cosB - sinA*sinB;
			f32 newSin = cosA*sinB + sinA*cosB;

			vPos[i].x = -newSin;
			vPos[i].y = newCos;
			vPos[i].z = elevation;

			cosA = newCos;
			sinA = newSin;
		}
	}

	if(bestIndex)
		(*bestIndex) = uBestIndex;
	u8 uIndex1=uBestIndex;
	u8 uIndex2=(uBestIndex+1)%4;
	u8 uIndex3=(uBestIndex+2)%4;
	u8 uIndex4=(uBestIndex+3)%4;

	if(secondBestIndex)
	{
		if(fGoodness[uIndex2] > fGoodness[uIndex3] && fGoodness[uIndex2] > fGoodness[uIndex4])
			(*secondBestIndex) = uIndex2;
		else if(fGoodness[uIndex3] > fGoodness[uIndex2] && fGoodness[uIndex3] > fGoodness[uIndex4])
			(*secondBestIndex) = uIndex3;
		else
			(*secondBestIndex) = uIndex4;
	}

	fVolume[uIndex1] = 0.f; //audNorthAudioEngine::GetGtaEnvironment()->GetReportAttenuationCurve().CalculateValue(fBestGoodness);
	sPredelay[uIndex1] = 0; // audNorthAudioEngine::GetGtaEnvironment()->GetReportPredelayCurve().CalculateValue(fBestGoodness);

	if(secondBestIndex)
	{
		fVolume[(*secondBestIndex)] = fVolume[uIndex1] + fReportVolumeDelta;
		sPredelay[(*secondBestIndex)] = sPredelay[uIndex1] + sReportPredelayDelta;
	}
}



void audWeaponAudioEntity::AddToAutoFiringList(const audWeaponFireInfo * fireInfo, audSound ** autoFireSound, audSound ** interiorAutoSound, audSound ** pulseHeadsetSound)
{
	s32 firstEmptySlot = -1;
	for(s32 i=0; i < g_MaxWeaponFireEventsPerFrame; i++)
	{
		u32 autoRelease = s_AutoFireReleaseTime;
#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive() && (audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor()))
		{
			autoRelease = s_ReplayAutoFireReleaseTime;
		}
#endif // GTA_REPLAY

		if(!g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireLoop && !g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop)
		{
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireTimeOut = fwTimer::GetTimeInMilliseconds() + autoRelease;
			firstEmptySlot = i;
		}

		if(g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].fireInfo.parentEntity == fireInfo->parentEntity)
		{
			sm_AutoWeaponsFiringList[i].fireInfo.lastPlayerHitTime = fireInfo->lastPlayerHitTime;
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireTimeOut = fwTimer::GetTimeInMilliseconds() + autoRelease;
			return;
		}
	}

	if(firstEmptySlot != -1)
	{
		g_WeaponAudioEntity.sm_AutoWeaponsFiringList[firstEmptySlot].m_AutoFireLoop = autoFireSound;
		g_WeaponAudioEntity.sm_AutoWeaponsFiringList[firstEmptySlot].m_InteriorAutoLoop = interiorAutoSound;	
		g_WeaponAudioEntity.sm_AutoWeaponsFiringList[firstEmptySlot].m_PulseHeadsetSound = pulseHeadsetSound;	
		g_WeaponAudioEntity.sm_AutoWeaponsFiringList[firstEmptySlot].fireInfo.Init(*fireInfo);
	}
}

void audWeaponAudioComponent::RemoveFromAutoFiringList() 
{
	for(s32 i=0; i < g_MaxWeaponFireEventsPerFrame; i++)
	{
		if(g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireLoop == &m_AutoFireLoop)
		{
			if(m_AutoFireLoop)
			{
				m_AutoFireLoop->StopAndForget();
			}
			//naDisplayf("Removing from firing list %d", i);
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireLoop = NULL;
			if(m_InteriorAutoLoop)
			{
				m_InteriorAutoLoop->StopAndForget();
			}
			if(m_PulseHeadsetSound)
			{
				m_PulseHeadsetSound->StopAndForget();
			}
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop = NULL;
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_PulseHeadsetSound = NULL;
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].fireInfo.parentEntity = NULL;
		}
	}
}

void audWeaponAudioEntity::ProcessCachedWeaponFireEvents(void)
{
	UpdateThermalVision();

	m_PlayerWeaponLoad.HandlePlayerWeaponLoad();
	
	if(sm_WantsToLoadPlayerEquippedWeapon)
	{
		AddAllInventoryWeaponsOnPed(CGameWorld::FindLocalPlayer());
	}

	ProcessEquippedWeapon();
	ProcessNextWeapon();

	PF_SET(PlayerGunVolume, sm_PlayerGunDuckingAttenuation+6.0f);
	u32 timeInMs = fwTimer::GetTimeInMilliseconds();

	audWeaponFireEvent *nearestFireEvent;
	u32 numNonPriorityFireEventsProcessed = 0;

	do 
	{
		f32 distanceToNearestFireEvent = 1000.0f;
		nearestFireEvent = NULL;
		s32 nearestFireEventIndex = -1;

		bool isPriorityEvent = false;

		//Find the nearest valid weapon fire event.
		for(u8 eventIndex=0; eventIndex<sm_NumWeaponFireEventsThisFrame; eventIndex++)
		{
			CEntity *parentEntity = sm_WeaponFireEventList[eventIndex].fireInfo.parentEntity;
			if(parentEntity)
			{
				bool isLocalPlayer = false;
				bool isLocalPlayerVehicle = false;
				bool isAlreadyAutofiring = false;

				if(parentEntity->GetIsTypePed())
				{
					CPed* ped = (CPed*)parentEntity;
					isLocalPlayer = ped->IsLocalPlayer() && !g_PlayerUsesNpcWeapons;
					isLocalPlayerVehicle = ped->GetVehiclePedInside() == FindPlayerVehicle();
				}
                else if (parentEntity->GetIsTypeVehicle())
                {
                    isLocalPlayerVehicle = parentEntity == FindPlayerVehicle();
                }

				isAlreadyAutofiring = (sm_WeaponFireEventList[eventIndex].weapon && sm_WeaponFireEventList[eventIndex].weapon->GetAudioComponent().m_AutoFireLoop != NULL);				

				if(isLocalPlayer || isLocalPlayerVehicle || isAlreadyAutofiring)
				{
					nearestFireEventIndex = eventIndex;
					isPriorityEvent = true;
					break;
				}
				else
				{
					if(numNonPriorityFireEventsProcessed < g_MaxWeaponFiresPlayedPerFrame)
					{
						Vector3 distanceVector = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition() -
							parentEntity->GetTransform().GetPosition());
						f32 distance = distanceVector.Mag();

						if(distance < distanceToNearestFireEvent || sm_WeaponFireEventList[eventIndex].fireInfo.isKillShot)
						{
							if((nearestFireEventIndex == -1) || (nearestFireEventIndex != -1 && !sm_WeaponFireEventList[nearestFireEventIndex].fireInfo.isKillShot) || (sm_WeaponFireEventList[eventIndex].fireInfo.isKillShot && distance < distanceToNearestFireEvent))
							{
								distanceToNearestFireEvent = distance;
								nearestFireEventIndex = eventIndex;
							}
						}
					}
				}
			}
		}

		if(nearestFireEventIndex >= 0)
		{
			nearestFireEvent = &(sm_WeaponFireEventList[nearestFireEventIndex]);

#if __BANK
			if(sm_WeaponFireEventList[nearestFireEventIndex].fireInfo.isKillShot)
			{
				Displayf("We have a kill shot");
			}
#endif
			CEntity *parentEntity = nearestFireEvent->fireInfo.parentEntity;
			CPed* pPlayer = CGameWorld::FindLocalPlayer();
			bool enemy = false;
			if (naVerifyf(pPlayer,"Can't find local player."))
			{	
				if(parentEntity && parentEntity->GetIsTypePed() && (((CPed*)parentEntity)->GetPedIntelligence()) && (((CPed*)parentEntity)->GetPedIntelligence()->GetQueriableInterface()->GetHostileTarget() == pPlayer))
				{
					enemy = true;
				}
			}
			// Update our nearby-gunfire factor, if the fire event has a fire sound
			if (distanceToNearestFireEvent < g_MaxDistanceToCountTowardFactor && nearestFireEvent->fireInfo.GetWeaponFireSound(nearestFireEvent->fireInfo.FindWeaponSettings(false), pPlayer == nearestFireEvent->fireInfo.parentEntity) != g_NullSoundHash)
			{
				sm_LastTimeSomeoneNearbyFiredAGun = timeInMs;
				if (enemy)
				{	
					sm_LastTimeEnemyNearbyFiredAGun = timeInMs;
				}
			}

			// Store a copy of the last fire event played for the conductors. 
			if(parentEntity && parentEntity->GetIsTypePed() &&(!((CPed*)parentEntity)->IsLocalPlayer() || g_PlayerUsesNpcWeapons))
			{
				sm_LastWeaponFireEventPlayed = sm_WeaponFireEventList[nearestFireEventIndex];
				if (enemy)
				{	
					sm_LastWeaponFireEventPlayedAgainstPlayer = sm_WeaponFireEventList[nearestFireEventIndex];
				}
			}
			PlayWeaponFireEvent(nearestFireEvent);
#if USE_GUN_TAILS
			CEntity *parentEntity = sm_WeaponFireEventList[nearestFireEvent].parentEntity;
			if(parentEntity)
			{
				audNorthAudioEngine::GetGunFireAudioEntity()->AddGunFireEvent(VEC3V_TO_VECTOR3(parentEntity->GetTransform().GetPosition())
					,sm_WeaponFireEventList[nearestFireEvent].weaponSettings->TailEnergyValue);
			}
#endif
			sm_WeaponFireEventList[nearestFireEventIndex].fireInfo.parentEntity = NULL;

			if(!isPriorityEvent)
			{
				numNonPriorityFireEventsProcessed++;
			}
		}
	} while (nearestFireEvent);	

	bool wasUsingPlayerDistanceCam = sm_UsingPlayerDistanceCam;
	MicrophoneSettings * mic = audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings();
	if(mic)
	{
		sm_UsingPlayerDistanceCam = AUD_GET_TRISTATE_VALUE(mic->Flags, FLAG_ID_MICROPHONESETTINGS_DISTANTPLAYERWEAPONS) == AUD_TRISTATE_TRUE;
	}
	else if(audNorthAudioEngine::GetMicrophones().IsCinematicMicActive())
	{
		sm_UsingPlayerDistanceCam = true;
	}
	
	u32 autoRelease = s_AutoFireReleaseTime;
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && (audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor()))
	{
		autoRelease = s_ReplayAutoFireReleaseTime;
	}
#endif // GTA_REPLAY

	for(s32 i=0; i< g_MaxWeaponFireEventsPerFrame; i++) 
	{
		if(sm_AutoWeaponsFiringList[i].m_AutoFireLoop && sm_AutoWeaponsFiringList[i].fireInfo.parentEntity 
			&& sm_AutoWeaponsFiringList[i].fireInfo.parentEntity->GetIsTypePed() && *sm_AutoWeaponsFiringList[i].m_AutoFireLoop)
		{
			CPed * ped = (CPed *)sm_AutoWeaponsFiringList[i].fireInfo.parentEntity.Get();  

			if(!ped->IsLocalPlayer())
			{
				const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
				sm_AutoWeaponsFiringList[i].fireInfo.hitPlayerSmoother.Init(sm_PlayerhitBoostSmoothTime);
				
				f32 applyFactor;
				if(sm_AutoWeaponsFiringList[i].fireInfo.lastPlayerHitTime + sm_PlayerHitBoostHoldTime > now )
				{
					applyFactor = sm_AutoWeaponsFiringList[i].fireInfo.hitPlayerSmoother.CalculateValue(1.f, fwTimer::GetTimeStep());
				}
				else
				{
					applyFactor = sm_AutoWeaponsFiringList[i].fireInfo.hitPlayerSmoother.CalculateValue(0.f, fwTimer::GetTimeStep());
				}

				(*sm_AutoWeaponsFiringList[i].m_AutoFireLoop)->SetRequestedPostSubmixVolumeAttenuation(sm_PlayerHitBoostVol*applyFactor);
				(*sm_AutoWeaponsFiringList[i].m_AutoFireLoop)->SetRequestedVolumeCurveScale(1.f+ ((sm_PlayerHitBoostRolloff-1.f)*applyFactor));
				
				if(sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop && *sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop)
				{
					(*sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop)->SetRequestedPostSubmixVolumeAttenuation(sm_PlayerHitBoostVol*applyFactor);
					(*sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop)->SetRequestedVolumeCurveScale(1.f+ ((sm_PlayerHitBoostRolloff-1.f)*applyFactor));
				}
			}

			CWeapon * weapon = ped->GetWeaponManager()->GetEquippedWeapon();
			if(weapon && weapon->GetAudioComponent().GetAnimIsAutoFiring())
			{
				if(ped->IsUsingFiringVariation())
				{
					sm_AutoWeaponsFiringList[i].m_AutoFireTimeOut = timeInMs + autoRelease;
					continue;
				}
				else
				{
					weapon->GetAudioComponent().SetAnimIsAutoFiring(false);
				}
			}

			//Let's have at least a frame to play the shots
			if(((sm_AutoWeaponsFiringList[i].m_AutoFireTimeOut - autoRelease + 33) > (fwTimer::GetTimeInMilliseconds())))
			{
				continue;
			}

			if(ped && 
				((!ped->IsFiring()
				REPLAY_ONLY(&& (!CReplayMgr::IsPlaying() || !(ped->GetPedAudioEntity() && ped->GetPedAudioEntity()->GetWasFiringforReplay()) || ped->GetPedAudioEntity()->GetTriggerAutoWeaponStopForReplay()))) ||
				(ped->IsLocalPlayer() && (wasUsingPlayerDistanceCam != sm_UsingPlayerDistanceCam))))
			{
 				StopAutomaticFire(sm_AutoWeaponsFiringList[i].fireInfo, sm_AutoWeaponsFiringList[i].m_AutoFireLoop, sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop, sm_AutoWeaponsFiringList[i].m_PulseHeadsetSound);
				sm_AutoWeaponsFiringList[i].m_AutoFireLoop = NULL;
				sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop = NULL;
				sm_AutoWeaponsFiringList[i].m_PulseHeadsetSound = NULL;

				ped->GetPedAudioEntity()->SetTriggerAutoWeaponStopForReplay(false);
			}

		}

		u32 timeOut = sm_AutoWeaponsFiringList[i].m_AutoFireTimeOut;

#if GTA_REPLAY
		bool isMinigun = sm_WeaponFireEventList[i].fireInfo.settingsHash == ATSTRINGHASH("AUDIO_ITEM_MINIGUN", 0x674A2973);
		bool isVehicleFlamethrower = sm_AutoWeaponsFiringList[i].fireInfo.settingsHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_MINI_FLAMETHROWER", 0xC444739B) || sm_AutoWeaponsFiringList[i].fireInfo.settingsHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_FLAMETHROWER_SCIFI", 0xD6B8E406) || sm_AutoWeaponsFiringList[i].fireInfo.settingsHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_FLAMETHROWER", 0x58917377);

		if((audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor()) && CReplayMgr::IsPlaying())
		{
			timeOut -= autoRelease;
			timeOut += (u32)(autoRelease*CReplayMgr::GetMarkerSpeed());
		}
#endif
		
		//We want to stop automatic/interior auto sounds if they go over the timeout
		//But not while playing replay when a ped is firing, or we're in slow mo and not a minigun
		if((sm_AutoWeaponsFiringList[i].m_AutoFireLoop || sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop) && 
			(timeOut < timeInMs)
			REPLAY_ONLY( && (!CReplayMgr::IsPlaying() || 
			(!isMinigun && !isVehicleFlamethrower && (audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())) || 
			(sm_AutoWeaponsFiringList[i].fireInfo.parentEntity && !sm_AutoWeaponsFiringList[i].fireInfo.parentEntity->GetIsTypePed()))))
		{
#if RSG_BANK & GTA_REPLAY
			if(CReplayMgr::IsPlaying())
			{
				naDisplayf("Replay: StopAutomaticFire due to timeout on %s at time %u", audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_AutoWeaponsFiringList[i].fireInfo.audioSettingsRef), fwTimer::GetTimeInMilliseconds());
			}
#endif
			StopAutomaticFire(sm_AutoWeaponsFiringList[i].fireInfo, sm_AutoWeaponsFiringList[i].m_AutoFireLoop, sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop, sm_AutoWeaponsFiringList[i].m_PulseHeadsetSound);
			sm_AutoWeaponsFiringList[i].m_AutoFireLoop = NULL;
			sm_AutoWeaponsFiringList[i].m_InteriorAutoLoop = NULL;
			sm_AutoWeaponsFiringList[i].m_PulseHeadsetSound = NULL;
		}
	}

	//Remove the old registered references.
	for(u8 eventIndex=0; eventIndex<sm_NumWeaponFireEventsThisFrame; eventIndex++)
	{
		sm_WeaponFireEventList[eventIndex].fireInfo.parentEntity = NULL;
	}

	sm_NumWeaponFireEventsThisFrame = 0;

	// Update the nearby gunfire factor
	if (sm_LastTimeSomeoneNearbyFiredAGun == timeInMs)
	{
		// We just fired at least one gun within range
		sm_GunfireFactor = Min(sm_GunfireFactor+g_GunFiredFactorIncrease, 1.0f);
	}
	else if ((sm_LastTimeSomeoneNearbyFiredAGun+g_TimeSinceSomeoneNearbyFiredToReduceFactor) < timeInMs)
	{
		// We didn't fire a gun within range in the last wee while
		sm_GunfireFactor = Max(sm_GunfireFactor+g_GunNotFiredFactorDecrease, 0.0f);
	}
	PF_SET(GunfireFactor, sm_GunfireFactor);
}

void audWeaponAudioEntity::PlayWeaponFireEvent(audWeaponFireEvent *fireEvent)
{
	const WeaponSettings * weaponSettings = fireEvent->fireInfo.FindWeaponSettings();

	naAssert(weaponSettings);
	if(!weaponSettings)
	{
		return;
	}

#if __BANK
	if(g_DebugWeaponTimings && fireEvent->fireInfo.parentEntity)
	{
		grcDebugDraw::Sphere(fireEvent->fireInfo.parentEntity->GetTransform().GetPosition(), 0.5f, Color32(0,255,0), true, 5);
	}
#endif

	bool isPlayerGun = false;
	if (fireEvent->fireInfo.parentEntity && fireEvent->fireInfo.parentEntity->GetIsTypePed() && (static_cast<CPed*>(fireEvent->fireInfo.parentEntity.Get()))->IsLocalPlayer() && !g_PlayerUsesNpcWeapons)
	{
		isPlayerGun = true;
	}
	if(audNorthAudioEngine::GetMicrophones().IsCinematicMicActive())
	{
		isPlayerGun = false;
	}


	if(fireEvent->weapon)
	{
		fireEvent->fireInfo.lastPlayerHitTime = fireEvent->weapon->GetAudioComponent().GetLastPlayerHitTime();
	}
	else
	{
		return;
	}

	bool isVehicleFlamethrower = false;
	eWeaponEffectGroup effectGroup = EWEAPONEFFECTGROUP_MAX; // Defaulted to an effectGroup value that we don't care.
	if(fireEvent->weapon->GetWeaponInfo())
	{
		effectGroup = fireEvent->weapon->GetWeaponInfo()->GetEffectGroup();

		if (fireEvent->weapon->GetWeaponInfo()->GetAudioHash() == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_FLAMETHROWER_SCIFI", 0xD6B8E406) ||
			fireEvent->weapon->GetWeaponInfo()->GetAudioHash() == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_FLAMETHROWER", 0x58917377) ||
			fireEvent->weapon->GetWeaponInfo()->GetAudioHash() == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_MINI_FLAMETHROWER", 0xC444739B))
		{
			isVehicleFlamethrower = true;
		}
	}

	// See url:bugstar:5518617. We should probably do this for all vehicle mounted weapons but being cautious. During replay, the fireEvent explicit position
	// is already populated by the replay packet, so we don't want to overwrite it
#if GTA_REPLAY
	if(!CReplayMgr::IsEditModeActive())
#endif
	{
		if (isVehicleFlamethrower && fireEvent && fireEvent->weapon && fireEvent->weapon->GetMuzzleBoneIndex() >= 0)
		{
			fireEvent->fireInfo.explicitPosition = fireEvent->weapon->GetMuzzleEntity()->TransformIntoWorldSpace(fireEvent->weapon->GetMuzzleEntity()->GetObjectMtx(fireEvent->weapon->GetMuzzleBoneIndex()).d);
		}
		else
		{
			fireEvent->fireInfo.explicitPosition.Zero();
		}
	}	

	// Special ability and slow-mo video editor forces all auto-fire weapons into single shot
	bool replayForceSingleShot = false;
#if GTA_REPLAY
	if(audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		if(CReplayMgr::GetMarkerSpeed() <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD BANK_ONLY( || audNorthAudioEngine::IsForcingSlowMoVideoEditor() || audNorthAudioEngine::IsForcingSuperSlowMoVideoEditor()))
		{
			if (!isVehicleFlamethrower)
			{
				replayForceSingleShot = true;
			}
		}
	}
#endif

	bool forceSingleShot = (fireEvent->fireInfo.forceSingleShot && !fireEvent->fireInfo.isSuppressed) ||
			replayForceSingleShot || audNorthAudioEngine::IsInCinematicSlowMo() ||
			(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeNone && isPlayerGun && weaponSettings->AutoSound != ATSTRINGHASH("SPL_MINIGUN2_PLAYER_MASTER", 0x91ED0FA));

	if(!forceSingleShot && ((AUD_GET_TRISTATE_VALUE(weaponSettings->Flags, FLAG_ID_WEAPONSETTINGS_ISPLAYERAUTOFIRE)== AUD_TRISTATE_TRUE) ||
		(AUD_GET_TRISTATE_VALUE(weaponSettings->Flags, FLAG_ID_WEAPONSETTINGS_ISNPCAUTOFIRE)== AUD_TRISTATE_TRUE))) 
	{
		audWeaponAudioComponent & weaponAudio = fireEvent->weapon->GetAudioComponent();

		if(weaponAudio.m_AutoFireLoop && isPlayerGun && 
			g_FrontendAudioEntity.GetSpecialAbilityMode() != weaponAudio.m_SpecialAbilityMode 
			&& (g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor || 
			weaponAudio.m_SpecialAbilityMode == audFrontendAudioEntity::kSpecialAbilityModeTrevor))
		{
			weaponAudio.m_AutoFireLoop->StopAndForget();
		}
		weaponAudio.m_SpecialAbilityMode = g_FrontendAudioEntity.GetSpecialAbilityMode();
		StartAutomaticFire(fireEvent->fireInfo, &weaponAudio.m_AutoFireLoop, &weaponAudio.m_InteriorAutoLoop,effectGroup, &weaponAudio.m_PulseHeadsetSound);
	}
	else
	{ 
		HandleFire(fireEvent->fireInfo,effectGroup);
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		if(isPlayerGun && fireEvent->fireInfo.parentEntity && fireEvent->fireInfo.parentEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(fireEvent->fireInfo.parentEntity.Get());	
			if(pPed)
			{
				CPedWeaponManager* weaponManager = pPed->GetWeaponManager();
				if(weaponManager)
				{
					CWeapon* equippedWeapon = weaponManager->GetEquippedWeapon();
					if(equippedWeapon)
					{
						fireEvent->fireInfo.ammo = equippedWeapon->GetAmmoInClip();
					}
				}
			}
		}
		f32	fGunFightVolumeOffset = 0.0f; 
		if (fireEvent->fireInfo.parentEntity && fireEvent->fireInfo.parentEntity->GetIsTypePed() && ((CPed*)fireEvent->fireInfo.parentEntity.Get())->GetPedAudioEntity() )
		{
			fGunFightVolumeOffset = ((CPed*)fireEvent->fireInfo.parentEntity.Get())->GetPedAudioEntity()->GetGunfightVolumeOffset();
		}

		
		// In order to get the CWeapon on replay playback for vehicle weapons we need to know which seat the weapon is used from
		// We don't have the ped in all cases, loop through the vehicle weapons to find the seat idx.
		int seatIdx = -1;
		if (fireEvent->fireInfo.parentEntity && fireEvent->fireInfo.parentEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(fireEvent->fireInfo.parentEntity.Get());
			CVehicleWeaponMgr* pWeaponMgr = pVehicle->GetVehicleWeaponMgr();
			if(pWeaponMgr)
			{
				int weaponIdx = -1;
				for(int i = 0; i < pWeaponMgr->GetNumVehicleWeapons() && weaponIdx == -1; ++i)
				{
					CVehicleWeapon* pWeapon = pWeaponMgr->GetVehicleWeapon(i);
					if(!pWeapon)
						break;

					switch(pWeapon->GetType())
					{
						case VGT_VEHICLE_WEAPON_BATTERY:
						{
							CVehicleWeaponBattery* pWeaponBattery = static_cast<CVehicleWeaponBattery*>(pWeapon);

							for(int j = 0; j < pWeaponBattery->GetNumWeaponsInBattery(); ++j)
							{
								CVehicleWeapon* pWeaponFromBattery = pWeaponBattery->GetVehicleWeapon(j);

								if( pWeaponFromBattery && pWeaponFromBattery->GetType() == VGT_FIXED_VEHICLE_WEAPON &&
									static_cast<CFixedVehicleWeapon*>(pWeaponFromBattery)->GetWeapon() == fireEvent->weapon)
								{
									weaponIdx = i;
									break;
								}
							}
							break;
						}

						case VGT_FIXED_VEHICLE_WEAPON:
						{	
							if(static_cast<CFixedVehicleWeapon*>(pWeapon)->GetWeapon() == fireEvent->weapon)
							{
								weaponIdx = i;
							}
							break;
						}

						default:
							break;
					}					
				}

				if(weaponIdx != -1)
					seatIdx = pWeaponMgr->GetSeatIndexFromWeaponIdx(weaponIdx);
			}
		}
		
		CReplayMgr::RecordFx<CPacketWeaponSoundFireEvent>(
			CPacketWeaponSoundFireEvent(fireEvent->fireInfo, fGunFightVolumeOffset, fireEvent->weapon->GetWeaponHash(), seatIdx), fireEvent->fireInfo.parentEntity, fireEvent->weapon->GetEntity());
	}
#endif // GTA_REPLAY
}

void audWeaponAudioEntity::StartAutomaticFire(audWeaponFireInfo &fireInfo, audSound ** autoFireSound, audSound ** interiorAutoSound,const eWeaponEffectGroup effectGroup, audSound ** pulseHeadsetSound)
{
	audEnvironmentGroupInterface* occlusionGroup = NULL;
	Vector3 sourcePosition;
	sourcePosition.Zero();
	audTracker *tracker = fireInfo.parentEntity->GetPlaceableTracker();

	audWeaponInteriorMetrics interior;
	g_WeaponAudioEntity.GetInteriorMetrics(interior, fireInfo.parentEntity);

	const WeaponSettings * settings = fireInfo.FindWeaponSettings();

	if(!naVerifyf(settings, "Couldn't find weapon settings from fireinfo with ItemSettings %s", 
		audNorthAudioEngine::GetMetadataManager().GetObjectName(fireInfo.audioSettingsRef)))
	{
		return;
	}

	if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed())
	{
		CPed* parentPed = static_cast<CPed*>(fireInfo.parentEntity.Get());
		occlusionGroup = parentPed->GetWeaponEnvironmentGroup(true);

		if (occlusionGroup)
		{
			audPedAudioEntity* pedAudioEntity = parentPed->GetPedAudioEntity();

			if (pedAudioEntity)
			{
				pedAudioEntity->ResetEnvironmentGroupLifeTime();
			}
		}
	}
	else if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypeVehicle())
	{
		occlusionGroup = ((CVehicle*)(fireInfo.parentEntity).Get())->GetAudioEnvironmentGroup();
	}

	if (fireInfo.explicitPosition.IsNonZero())
	{
		sourcePosition = fireInfo.explicitPosition;
		tracker = NULL;
	}
	else if (tracker)
	{
		sourcePosition = tracker->GetPosition();
	}



	f32 interiorWetness = interior.Wetness;

	if(fireInfo.isSuppressed)
	{
		interiorWetness*= sm_InteriorSilencedWeaponWetness;
	}

	f32 interiorDryScaling = sm_InteriorDryShotVolumeCurve.CalculateValue(interiorWetness);
	f32 interiorWetScaling = sm_InteriorWetShotVolumeCurve.CalculateValue(interiorWetness);

#if __BANK
	if(g_DebugWeaponTimings && fireInfo.parentEntity)
	{
		grcDebugDraw::Sphere(fireInfo.parentEntity->GetTransform().GetPosition(), 0.5f, Color32(0,255,0), true, 5);
	}
#endif

	f32 postSubmixAttenuation = g_DefaultGunPostSubmixAttenuation;

	bool isPlayerGun = false;
	if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed() && (static_cast<CPed*>(fireInfo.parentEntity.Get()))->IsLocalPlayer() && !g_PlayerUsesNpcWeapons)
	{
		isPlayerGun = true;
		postSubmixAttenuation = PlayerGunFired();
	}

	// only do loud sound stuff for weapons with fire sounds
	if(!fireInfo.isSuppressed )
	{
		audNorthAudioEngine::RegisterLoudSound(sourcePosition,isPlayerGun);
		audNorthAudioEngine::GetGtaEnvironment()->SpikeResonanceObjects(GetWeaponResonanceImpulse( effectGroup), sourcePosition, naEnvironment::sm_WeaponsResonanceDistance);
	}
	const WeaponSettings* weaponSettings = fireInfo.FindWeaponSettings();
	
	u32 autoRelease = s_AutoFireReleaseTime;
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive() && (audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor()))
	{
		autoRelease = s_ReplayAutoFireReleaseTime;
	}
#endif // GTA_REPLAY

	u32 fireSound = fireInfo.isSuppressed ? weaponSettings->SuppressedFireSound : weaponSettings->AutoSound;

	// B*4191672 - If the weapon swaps from player->NPC while still firing, update the panning so that we at least ensure 
	// that it fades off at distance correctly instead of being stuck frontend forever. The other option is to just stop
	// and start the whole sound in this situation, but this approach sounds much less jarring.
	if(CNetwork::IsGameInProgress() && !CReplayMgr::IsEditModeActive())
	{
		if(*autoFireSound && fireSound != fireInfo.autoFireSound)
		{
			const Sound* soundMetadataCompressed = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataPtr(fireSound);

			if(soundMetadataCompressed)
			{
				Sound uncompressedMetadata;
				audSound::DecompressMetadata_Untyped(soundMetadataCompressed, uncompressedMetadata);
				(*autoFireSound)->SetRequestedPan(uncompressedMetadata.Pan);
				(*autoFireSound)->SetRequestedShouldAttenuateOverDistance(AUD_GET_TRISTATE_VALUE(uncompressedMetadata.Flags, FLAG_ID_SOUND_DISTANCEATTENUATION));
			}

			fireSound = fireInfo.autoFireSound;
		}
	}	

	for(s32 i=0; i < g_MaxWeaponFireEventsPerFrame; i++)
	{
		if(g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireLoop && 
			*g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireLoop && 
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].fireInfo.parentEntity == fireInfo.parentEntity)
		{
			g_WeaponAudioEntity.sm_AutoWeaponsFiringList[i].m_AutoFireTimeOut = fwTimer::GetTimeInMilliseconds() + autoRelease;

			if (fireInfo.explicitPosition.IsNonZero())
			{
				if (*autoFireSound)
				{
					(*autoFireSound)->SetRequestedPosition(fireInfo.explicitPosition);
				}

				if (*interiorAutoSound)
				{
					(*interiorAutoSound)->SetRequestedPosition(fireInfo.explicitPosition);
				}
			}

			return;
		}
	}

	f32 markerSpeed = 1.0f;
#if GTA_REPLAY
	if(audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		markerSpeed = CReplayMgr::GetMarkerSpeed();
#if __BANK
		if(audNorthAudioEngine::IsForcingSlowMoVideoEditor())
			markerSpeed = 0.2f;
		if(audNorthAudioEngine::IsForcingSuperSlowMoVideoEditor())
			markerSpeed = 0.05f;
#endif
	}
#endif
	bool cienmaticSlowMo = audNorthAudioEngine::IsInCinematicSlowMo();

	if(*autoFireSound == NULL)
	{
		audSoundInitParams initParams;
		BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);
		initParams.Tracker = tracker;
		initParams.Position = sourcePosition;
		initParams.EnvironmentGroup = occlusionGroup;
		initParams.UpdateEntity = true;
		if(sm_PlayerEquippedWeaponSlot.waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->IsLoaded())
		{
			initParams.WaveSlot = sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot;
		}
		initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(interiorDryScaling);
		if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed() && ((CPed*)fireInfo.parentEntity.Get())->GetPedAudioEntity() )
		{
			initParams.Volume += ((CPed*)fireInfo.parentEntity.Get())->GetPedAudioEntity()->GetGunfightVolumeOffset();
		}		

		if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor && fireSound == ATSTRINGHASH("SPL_MINIGUN2_PLAYER_MASTER", 0x91ED0FA))
		{
			fireSound = ATSTRINGHASH("TREVOR_MINI_GUN_FIRE_MASTER", 0x2AE131D9);
		}

		g_WeaponAudioEntity.CreateAndPlaySound_Persistent(fireSound, autoFireSound, &initParams);
		fireInfo.autoFireSound = fireSound;

		if(isPlayerGun && g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeNone && audNorthAudioEngine::ShouldTriggerPulseHeadset() && !*pulseHeadsetSound)
		{
			if(fireSound == ATSTRINGHASH("SPL_MINIGUN2_PLAYER_MASTER", 0x91ED0FA))
			{
				CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("Minigun_FireLoop", 0xAB401B5B)), pulseHeadsetSound);
			}
			else if(fireSound == ATSTRINGHASH("LMG_MG_PLAYER_MASTER", 0x696F4A85))
			{
				CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("MG_FireLoop", 0xFFE700AE)), pulseHeadsetSound);
			}
			else if(fireSound == ATSTRINGHASH("LMG_COMBAT2_PLAYER_MASTER", 0x1EA5E048))
			{
				CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("CombatMG_FireLoop", 0xB002D84E)), pulseHeadsetSound);
			}
			else if(fireSound == ATSTRINGHASH("LMG_ASSAULT_PLAYER_MASTER", 0x289FE16F))
			{
				CreateAndPlaySound_Persistent(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("AssaultMG_FireLoop", 0x200945E5)), pulseHeadsetSound);
			}
		}

#if __BANK
		if(g_DebugWeaponTimings)
		{
			naErrorf("Auto sound %s, time %u", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(fireSound), fwTimer::GetTimeInMilliseconds());
		}
#endif

		if(*autoFireSound)
		{
			//naDisplayf("Setting auto fadeout time to %d, line %d fireevent %p", fireEvent->weaponAudio->m_AutoFireTimeOut, __LINE__, fireEvent); 
			if(fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed())
				(static_cast<CPed*>(fireInfo.parentEntity.Get()))->SetLastTimeStartedAutoFireSound(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
			
			AddToAutoFiringList(&fireInfo, autoFireSound, interiorAutoSound, pulseHeadsetSound);

			if(interiorWetScaling > 0.f && interiorAutoSound) 
			{  
				//Keep the same init params but 
				audSoundInitParams interiorParams = initParams;
				interiorParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(interiorWetScaling);
				if(*interiorAutoSound)
				{
					(*interiorAutoSound)->StopAndForget();
				}

				u32 interiorShotSound = weaponSettings->InteriorShotSound;

				if((cienmaticSlowMo || markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoInteriorShotSound != g_NullSoundHash)
				{
					interiorShotSound = settings->SlowMoInteriorShotSound;
				}
				if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoInteriorShotSound != g_NullSoundHash)
				{
					interiorShotSound = settings->SuperSlowMoInteriorShotSound;
				}

				g_WeaponAudioEntity.CreateAndPlaySound_Persistent(interiorShotSound, interiorAutoSound, &interiorParams);
				if(*interiorAutoSound) 
				{
					(*interiorAutoSound)->FindAndSetVariableValue(ATSTRINGHASH("interior_hold", 0x5BB4EAD3), interior.Hold);
					(*interiorAutoSound)->SetRequestedLPFCutoff(interior.Lpf);
					(*interiorAutoSound)->FindAndSetVariableValue(ATSTRINGHASH("PreDelay", 1610825783), (f32)interior.Predelay);
				}
			}
		}
	}
}

f32 audWeaponAudioEntity::GetWeaponResonanceImpulse(eWeaponEffectGroup effectGroup)
{
	if(effectGroup >= WEAPON_EFFECT_GROUP_PUNCH_KICK && effectGroup <=WEAPON_EFFECT_GROUP_MELEE_GENERIC )
	{
		return naEnvironment::sm_MeleeResoImpulse;	
	}
	else if (effectGroup >= WEAPON_EFFECT_GROUP_PISTOL_SMALL  && effectGroup <= WEAPON_EFFECT_GROUP_PISTOL_LARGE)
	{
		return naEnvironment::sm_PistolResoImpulse;	
	}
	else if (effectGroup >= WEAPON_EFFECT_GROUP_SMG  && effectGroup <= WEAPON_EFFECT_GROUP_ROCKET)
	{
		return naEnvironment::sm_RifleResoImpulse;	
	}
	return 0.f;
}
void audWeaponAudioEntity::StopAutomaticFire(audWeaponFireInfo & fireInfo, audSound ** autoFireLoop, audSound ** interiorAutoLoop, audSound ** pulseHeadsetSound)
{
	if(autoFireLoop && *autoFireLoop)
	{
		(*autoFireLoop)->StopAndForget();
	}

	audWeaponInteriorMetrics interior;
	if(fireInfo.parentEntity)
	{
		const WeaponSettings *weaponSettings = fireInfo.FindWeaponSettings(false);
		
		GetInteriorMetrics(interior, fireInfo.parentEntity);
		PlayWeaponReport(&fireInfo, &interior, weaponSettings, 0.f, true);

		u32 timeAutoSoundPlayed = 0;
		s8 weaponSlotForCustomEchos = -1;
		if(fireInfo.parentEntity->GetIsTypePed())
		{
			timeAutoSoundPlayed = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - (static_cast<CPed*>(fireInfo.parentEntity.Get()))->GetLastTimeStartedAutoFireSound();
			CPed* ped =(CPed*)fireInfo.parentEntity.Get();
			if(ped->IsLocalPlayer() && ped->GetSelectedWeaponInfo())
			{
				weaponSlotForCustomEchos = (s8)ped->GetSelectedWeaponInfo()->GetWeaponWheelSlot();
			}
		}

		u32 echoSound = fireInfo.GetWeaponEchoSound(weaponSettings);
		if(echoSound && echoSound != g_NullSoundHash && !fireInfo.parentEntity->GetIsInInterior()) 
		{
			f32 initialAttenuation = 0.0f;
			EnvironmentRule* envRule = g_AmbientAudioEntity.GetAmbientEnvironmentRule();
			bool overrideEchos = envRule && AUD_GET_TRISTATE_VALUE(envRule->Flags, FLAG_ID_ENVIRONMENTRULE_OVERRIDEECHOS);
			if(envRule && overrideEchos)
			{
				if(envRule->EchoSoundList != 0 && weaponSlotForCustomEchos != -1)
				{
					audSoundSet echoSet;
					echoSet.Init(envRule->EchoSoundList);
					if(echoSet.IsInitialised())
					{
						u32 customEchoSound = GetCustomEchoSoundForWeapon(echoSet, (eWeaponWheelSlot)weaponSlotForCustomEchos);
						if(customEchoSound != 0 && customEchoSound != g_NullSoundHash)
							echoSound = customEchoSound;
					}
				}
				initialAttenuation = g_AmbientAudioEntity.GetAmbientEnvironmentRuleScale() * envRule->BaseEchoVolumeModifier;
			}
			Vector3 position = VEC3V_TO_VECTOR3(fireInfo.parentEntity->GetTransform().GetPosition());
			PlayTwoEchos(echoSound, 0, position, initialAttenuation, 0, timeAutoSoundPlayed, overrideEchos ? envRule : NULL);
		}
	}

	if(interiorAutoLoop && *interiorAutoLoop)
	{
		(*interiorAutoLoop)->StopAndForget();
	}

	if(pulseHeadsetSound && *pulseHeadsetSound)
	{
		(*pulseHeadsetSound)->StopAndForget();
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketWeaponAutoFireStop>(CPacketWeaponAutoFireStop(true), fireInfo.parentEntity);
	}
#endif

	fireInfo.parentEntity = NULL;
}

void audWeaponAudioEntity::HandleFire(audWeaponFireInfo &fireInfo,const eWeaponEffectGroup effectGroup)
{
	audEnvironmentGroupInterface* occlusionGroup = NULL;
	Vector3 sourcePosition;
	sourcePosition.Zero();
	audTracker *tracker = fireInfo.parentEntity->GetPlaceableTracker();

	audWeaponInteriorMetrics interiorMetrics;
	g_WeaponAudioEntity.GetInteriorMetrics(interiorMetrics, fireInfo.parentEntity);

	const WeaponSettings * settings = fireInfo.FindWeaponSettings();
	
	if(!naVerifyf(settings, "Couldn't find weapon settings from fireinfo with ItemSettings %s", 
		audNorthAudioEngine::GetMetadataManager().GetObjectName(fireInfo.audioSettingsRef)))
	{
		return;
	}	

	audWeaponFireInfo::PresuckInfo presuckInfo;
	u32 fireSound = fireInfo.GetWeaponFireSound(settings, 
				fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed() && ((CPed*)fireInfo.parentEntity.Get())->IsLocalPlayer(), &presuckInfo, fireInfo.isKillShot);
	u32 interiorShotSound = settings->InteriorShotSound;

	f32 markerSpeed = 1.0f;
#if GTA_REPLAY
	if(audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		markerSpeed = CReplayMgr::GetMarkerSpeed();
#if __BANK
		if(audNorthAudioEngine::IsForcingSlowMoVideoEditor())
			markerSpeed = 0.2f;
		if(audNorthAudioEngine::IsForcingSuperSlowMoVideoEditor())
			markerSpeed = 0.05f;
#endif
	}
#endif
	bool cienmaticSlowMo = audNorthAudioEngine::IsInCinematicSlowMo();

	if((cienmaticSlowMo || markerSpeed  <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD) && settings->SlowMoInteriorShotSound != g_NullSoundHash)
	{
		interiorShotSound = settings->SlowMoInteriorShotSound;
	}
	if(markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD && settings->SuperSlowMoInteriorShotSound != g_NullSoundHash)
	{
		interiorShotSound = settings->SuperSlowMoInteriorShotSound;
	}

	if(((AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_WEAPONSETTINGS_ISPLAYERAUTOFIRE)== AUD_TRISTATE_TRUE) ||
		(AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_WEAPONSETTINGS_ISNPCAUTOFIRE)== AUD_TRISTATE_TRUE)) && settings->SlowMoInteriorShotSound == g_NullSoundHash)
	{
		interiorShotSound = g_NullSoundHash;
	}

	//If we have a silenced auto weapon here, something went wrong and there's a chance it could get stuck on
	if(settings->AutoSound != g_NullSoundHash && settings->AutoSound != ATSTRINGHASH("SILENCE", 0xD0D7570A) && fireSound == settings->SuppressedFireSound)
	{
		return; 
	}

	if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed())
	{
		occlusionGroup = (static_cast<CPed*>(fireInfo.parentEntity.Get()))->GetWeaponEnvironmentGroup();
	}
	else if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypeVehicle())
	{
		occlusionGroup = ((CVehicle*)fireInfo.parentEntity.Get())->GetAudioEnvironmentGroup();  
	}

	if(tracker)
	{
		sourcePosition = tracker->GetPosition();
	}



	f32 interiorWetness = interiorMetrics.Wetness;

	if(fireInfo.isSuppressed)
	{
		interiorWetness*= sm_InteriorSilencedWeaponWetness;
	}

	f32 interiorDryScaling = sm_InteriorDryShotVolumeCurve.CalculateValue(interiorWetness);
	f32 interiorWetScaling = sm_InteriorWetShotVolumeCurve.CalculateValue(interiorWetness);

#if __BANK
	if(g_DebugWeaponTimings && fireInfo.parentEntity)
	{
		grcDebugDraw::Sphere(fireInfo.parentEntity->GetTransform().GetPosition(), 0.5f, Color32(0,255,0), true, 5);
	}
#endif

	f32 postSubmixAttenuation = g_DefaultGunPostSubmixAttenuation;

	bool isPlayerGun = false;
	bool isPlayerVehicle = false;
	if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed() && (static_cast<CPed*>(fireInfo.parentEntity.Get()))->IsLocalPlayer() && !g_PlayerUsesNpcWeapons)
	{
		isPlayerGun = true;
		postSubmixAttenuation = PlayerGunFired();
	}

	if(fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypeVehicle() && ((CVehicle*)fireInfo.parentEntity.Get())->ContainsLocalPlayer())
	{
		isPlayerVehicle = true;
	}

	// only do loud sound stuff for weapons with fire sounds
	if(!fireInfo.isSuppressed )
	{
		audNorthAudioEngine::RegisterLoudSound(sourcePosition,isPlayerGun);
		audNorthAudioEngine::GetGtaEnvironment()->SpikeResonanceObjects(GetWeaponResonanceImpulse(effectGroup), sourcePosition, naEnvironment::sm_WeaponsResonanceDistance);
	}


	audSoundInitParams initParams; 
	initParams.UpdateEntity = true;
	if(sm_PlayerEquippedWeaponSlot.waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->IsLoaded())
	{
		initParams.WaveSlot = sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot;
	}
	if(fireInfo.isKillShot)
	{
		tracker = NULL;
		sourcePosition = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
		initParams.UpdateEntity = false;
	}

	BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);
	initParams.Tracker = tracker;
	initParams.Position = sourcePosition;
	initParams.EnvironmentGroup = occlusionGroup;  
	if (fireInfo.parentEntity && fireInfo.parentEntity->GetIsTypePed() && ((CPed*)fireInfo.parentEntity.Get())->GetPedAudioEntity() )
	{
		initParams.Volume = ((CPed*)fireInfo.parentEntity.Get())->GetPedAudioEntity()->GetGunfightVolumeOffset();
	}
	if(isPlayerGun && naMicrophones::IsSniping() && AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_WEAPONSETTINGS_RECOCKWHENSNIPING) == AUD_TRISTATE_TRUE)
	{
		CreateAndPlaySound(sm_CodeSounds.Find(ATSTRINGHASH("sniper_cock", 0xC9981E7B)), &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSound(sm_CodeSounds.GetNameHash(), ATSTRINGHASH("sniper_cock", 0xC9981E7B), &initParams, NULL, fireInfo.parentEntity, eWeaponSoundEntity));
	}

	initParams.PostSubmixAttenuation = postSubmixAttenuation; 

	initParams.Volume += audDriverUtil::ComputeDbVolumeFromLinear(interiorDryScaling);

	const u32 now = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	const CrossfadeSound *crossfadeSound = SOUNDFACTORY.DecompressMetadata<CrossfadeSound>(fireSound);


	if(fireInfo.lastPlayerHitTime + sm_PlayerHitBoostHoldTime > now )
	{
			initParams.PostSubmixAttenuation += sm_PlayerHitBoostVol;
			initParams.VolumeCurveScale += sm_PlayerHitBoostRolloff;
	}

	audSound* nearSound = NULL;
	audSound* farSound = NULL;

	if(crossfadeSound)
	{
#if __BANK
		if(g_DebugWeaponTimings)
		{
			naErrorf("Crossfade sound %s time %u", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(fireSound), fwTimer::GetTimeInMilliseconds());  
		}
#endif
		const f32 dist2ToListener = (sourcePosition - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();

		if(dist2ToListener <= crossfadeSound->MinDistance*crossfadeSound->MinDistance)
		{
			// only play the near sound
			g_WeaponAudioEntity.CreateSound_LocalReference(crossfadeSound->NearSoundRef, &nearSound, &initParams);
		}
		else if(dist2ToListener >= crossfadeSound->MaxDistance*crossfadeSound->MaxDistance)
		{
			// only play the far sound
			g_WeaponAudioEntity.CreateSound_LocalReference(crossfadeSound->FarSoundRef, &farSound, &initParams);
		}
		else
		{
			// perform crossfade
			const f32 distToListener = sqrtf(dist2ToListener);
			const f32 distFactor = (distToListener - crossfadeSound->MinDistance) / (crossfadeSound->MaxDistance-crossfadeSound->MinDistance);

			initParams.Volume += audDriverUtil::ComputeDbVolumeFromLinear(1.f - distFactor);
			g_WeaponAudioEntity.CreateSound_LocalReference(crossfadeSound->NearSoundRef, &nearSound, &initParams);

			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(distFactor);
			g_WeaponAudioEntity.CreateSound_LocalReference(crossfadeSound->FarSoundRef, &farSound, &initParams);
		}
	}
	else
	{	
#if __BANK
		if(g_DebugWeaponTimings)
		{
			naErrorf("Singlefire sound %s time %u", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(fireSound), fwTimer::GetTimeInMilliseconds());  
		}
#endif
		g_WeaponAudioEntity.CreateSound_LocalReference(fireSound, &nearSound, &initParams);
	}

	if(isPlayerGun && (audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor()) && fireInfo.ammo == 0)
	{
		if(nearSound)
		{
			nearSound->FindAndSetVariableValue(ATSTRINGHASH("IsFinalBullet", 0xFA41EB98), 1.f);
		}
		if(farSound)
		{
			farSound->FindAndSetVariableValue(ATSTRINGHASH("IsFinalBullet", 0xFA41EB98), 1.f);
		}
	}

	if(nearSound)
	{
		nearSound->PrepareAndPlay();
	}

	if(farSound)
	{
		farSound->PrepareAndPlay();
	}

	if(g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeNone && audNorthAudioEngine::ShouldTriggerPulseHeadset())
	{
		if(isPlayerVehicle && fireSound == ATSTRINGHASH("SPL_TANKSHELL_PLAYER_MASTER", 0x7C8CC967))
		{
			g_WeaponAudioEntity.CreateAndPlaySound(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("Tank_Fire", 0xBB0AF7FB)));
		}
		else if(isPlayerGun && fireSound == ATSTRINGHASH("SPL_GRENADE_PLAYER_MASTER", 0xA2A2CF85))
		{
			g_WeaponAudioEntity.CreateAndPlaySound(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("GrenadeLauncher_Fire", 0x2DEA5398)));
		}
	}

	audSoundInitParams interiorParams = initParams;

	bool canDoInteriorSound = true;

	if(audNorthAudioEngine::IsInSlowMo() && (((AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_WEAPONSETTINGS_ISPLAYERAUTOFIRE)== AUD_TRISTATE_TRUE)) ||
		((AUD_GET_TRISTATE_VALUE(settings->Flags, FLAG_ID_WEAPONSETTINGS_ISNPCAUTOFIRE)== AUD_TRISTATE_TRUE))))
	{
		canDoInteriorSound = false;
	}

	if(canDoInteriorSound)
	{
		if(interiorWetScaling > 0.f)
		{
			interiorParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(interiorWetScaling);
			interiorParams.Predelay = interiorMetrics.Predelay;

			interiorParams.SetVariableValue(ATSTRINGHASH("interior_hold", 1538583251), interiorMetrics.Hold);
			interiorParams.LPFCutoff = interiorMetrics.Lpf;
	
			g_WeaponAudioEntity.CreateAndPlaySound(interiorShotSound, &interiorParams);
		}
		else if(fireInfo.parentEntity == FindPlayerPed() && !g_DisableTightSpotInteriorGuns)
		{
			f32 interiorSoundVolume = -100.0f;
			f32 interiorSoundHold = 0.0f;
			audNorthAudioEngine::GetGtaEnvironment()->GetInteriorGunSoundMetricsForExterior(interiorSoundVolume, interiorSoundHold);
			interiorParams.Volume = interiorSoundVolume;

			interiorParams.SetVariableValue(ATSTRINGHASH("interior_hold", 0x5BB4EAD3), interiorSoundHold);
			g_WeaponAudioEntity.CreateAndPlaySound(interiorShotSound, &interiorParams);
		}
	}


	PlayWeaponReport(&fireInfo, &interiorMetrics, settings, 0.f);
}

f32 audWeaponAudioEntity::PlayerGunFired()
{
#if __BANK
	if (g_PlayerGunAlwaysDamped)
	{
		sm_PlayerGunDuckingAttenuation = g_DefaultGunPostSubmixAttenuation;
		sm_PlayerGunDampingOn = true;
	}
	else if (g_PlayerGunNeverDamped)
	{
		sm_PlayerGunDuckingAttenuation = 0.0f;
		sm_PlayerGunDampingOn = false;
	}
#endif
	// Update what state we're in, and calculate a current offset volume
	u32 timeInMs = fwTimer::GetTimeInMilliseconds();
	if (sm_PlayerGunDampingOn)
	{
		// See if we've gone sufficiently long since the last shot that we should be back up to full volume again.
		// (could stay in the ducking state but reduce the attenuation if not quite long enough - see if snapping back is shit)
		if ((sm_LastTimePlayerGunFired + g_NotFiredForAWhileTime) < timeInMs)
		{
			sm_LastTimePlayerGunFired = timeInMs;
			sm_PlayerGunDampingOn = false;
			sm_PlayerGunDuckingAttenuation = 0.0f;
			sm_LastTimePlayerNotFiredGunForAWhile = timeInMs;
			return 0.0f;
		}
		else
		{
			// continue damping even more, unless we're fully damped
			sm_LastTimePlayerGunFired = timeInMs;
			sm_PlayerGunDuckingAttenuation = Max(sm_PlayerGunDuckingAttenuation + g_DuckingAttenuationPerShot, g_DefaultGunPostSubmixAttenuation);
			return sm_PlayerGunDuckingAttenuation;
		}
	}
	else
	{
		sm_LastTimePlayerGunFired = timeInMs;
		// See how long ago we didn't fire for a while - if it's long enough, we start ducking
		if ((sm_LastTimePlayerNotFiredGunForAWhile + g_BeenFiringForAWhileTime) < timeInMs)
		{
			sm_PlayerGunDampingOn = true; // so next time, we'll start ducking
		}
		sm_PlayerGunDuckingAttenuation = 0.0f; // don't bother actually ducking for this one
		return 0.0f;
	}
}

void audWeaponAudioEntity::PlayTwoEchos(u32 echoSound, u32 existingPredelay, Vector3& sourcePosition, f32 existingAttenuation, u32 echoDepth, u32 timeAutoSoundPlayed, EnvironmentRule* envRule)
{
	if(envRule)
	{
		if(echoDepth >= envRule->EchoNumber)
			return;
	}
	else if (echoDepth >= g_EchoDepth)
	{
		return;
	}

	existingAttenuation += g_EchoVolumeDrop;

	u32 minEchoLoopCount = timeAutoSoundPlayed < g_MinAutoEchoLoopCountIgnoreTime ? 1 : g_MinAutoEchoLoopCount;

	u32 loopNumber = 0;
	if(timeAutoSoundPlayed != 0)
	{
		f32 loopScale = Clamp((f32)(timeAutoSoundPlayed)/(f32)(g_maxAutoSoundHoldTime), 0.0f, 1.0f);
		loopNumber = (u32)(loopScale * 4) + minEchoLoopCount;
	}

	s32 bestEcho = -1;
	s32 bestOppositeEcho = -1;
	bestEcho = audNorthAudioEngine::GetGtaEnvironment()->GetBestEchoForSourcePosition(sourcePosition, &bestOppositeEcho);
	if (bestEcho>-1)
	{
		Vector3 position;
		u32 predelay = 0;
		f32 attenuation = 0.0f;
		audNorthAudioEngine::GetGtaEnvironment()->CalculateEchoDelayAndAttenuation(bestEcho, sourcePosition, &predelay, &attenuation, &position, echoDepth);
#if __BANK
		if(g_bForceWeaponEchoSettings)
		{
			attenuation = g_fWeaponEchoAttenuation;
			predelay = g_uWeaponEchoPredelay;
		}
#endif
		if(envRule)
		{
			f32 ruleScale = g_AmbientAudioEntity.GetAmbientEnvironmentRuleScale();
			attenuation = Min(6.0f, (attenuation * (1.0f -ruleScale)) + (envRule->EchoAttenuation * ruleScale));
			u32 ruleEchoDelay = (u32)(ruleScale * envRule->EchoDelay);
			u32 ruleEchoDelayVariance = (u32)(ruleScale * audEngineUtil::GetRandomNumberInRange(-1 * envRule->EchoDelayVariance, envRule->EchoDelayVariance));
			u32 nonruleEchoDelay = (u32)((1.0f - ruleScale) * predelay);
			predelay = Max(0, (s32)(ruleEchoDelay + nonruleEchoDelay + ruleEchoDelayVariance));
		}
		audSoundInitParams initParams;
		BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);
		initParams.Position = position;
		initParams.Volume = Max(-100.0f, attenuation + existingAttenuation);
		initParams.Predelay = predelay + existingPredelay;
		if(sm_PlayerEquippedWeaponSlot.waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->IsLoaded())
		{
			initParams.WaveSlot = sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot;
		}
		if(timeAutoSoundPlayed != 0)
		{
			if(echoDepth == 0)
			{
				if(timeAutoSoundPlayed >= initParams.Predelay)
					initParams.Predelay = 0;
				else
					initParams.Predelay -= timeAutoSoundPlayed;
			}
		}

		if(timeAutoSoundPlayed != 0)
		{
			initParams.SetVariableValue(ATSTRINGHASH("AUTO_ECHO_LOOP_COUNT", 0xE53436C4), (f32)loopNumber);
		}

		g_WeaponAudioEntity.CreateAndPlaySound(echoSound, &initParams);
	
		PlayTwoEchos(echoSound, predelay+existingPredelay, position, existingAttenuation+attenuation, echoDepth+1, 0, envRule);
	}
	if (g_PlayBothEchos && bestOppositeEcho>-1)
	{
		Vector3 position;
		u32 predelay = 0;
		f32 attenuation = 0.0f;
		audNorthAudioEngine::GetGtaEnvironment()->CalculateEchoDelayAndAttenuation(bestOppositeEcho, sourcePosition, &predelay, &attenuation, &position, echoDepth);
#if __BANK
		if(g_bForceWeaponEchoSettings)
		{
			attenuation = g_fWeaponEchoAttenuation;
			predelay = g_uWeaponEchoPredelay;
		}
#endif
		if(envRule)
		{
			f32 ruleScale = g_AmbientAudioEntity.GetAmbientEnvironmentRuleScale();
			attenuation = Min(6.0f, (attenuation * (1.0f -ruleScale)) + (envRule->EchoAttenuation * ruleScale));
			u32 ruleEchoDelay = (u32)(ruleScale * envRule->EchoDelay);
			u32 ruleEchoDelayVariance = (u32)(ruleScale * audEngineUtil::GetRandomNumberInRange(-1 * envRule->EchoDelayVariance, envRule->EchoDelayVariance));
			u32 nonruleEchoDelay = (u32)((1.0f - ruleScale) * predelay);
			predelay = Max(0, (s32)(ruleEchoDelay + nonruleEchoDelay + ruleEchoDelayVariance));
		}
		audSoundInitParams initParams;
		initParams.Position = position;
		BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);
		initParams.Volume = Max(-100.0f, attenuation + existingAttenuation);
		initParams.Predelay = predelay + existingPredelay;
		if(sm_PlayerEquippedWeaponSlot.waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot && sm_PlayerEquippedWeaponSlot.waveSlot->IsLoaded())
		{
			initParams.WaveSlot = sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot;
		}
		if(timeAutoSoundPlayed != 0)
		{
			if(echoDepth == 0)
			{
				if(timeAutoSoundPlayed >= initParams.Predelay)
					initParams.Predelay = 0;
				else
					initParams.Predelay -= timeAutoSoundPlayed;
			}
		}

		if(timeAutoSoundPlayed != 0)
		{
			initParams.SetVariableValue(ATSTRINGHASH("AUTO_ECHO_LOOP_COUNT", 0xE53436C4), (f32)loopNumber);
		}

		g_WeaponAudioEntity.CreateAndPlaySound(echoSound, &initParams);

		/*Vector3 positionForNextReflection;
		Vector3* echoPosition = audNorthAudioEngine::GetGtaEnvironment()->GetEchoPosition();
		positionForNextReflection = echoPosition[bestEcho];
		Vector3 aWeeBitAway = positionForNextReflection-g_AudioEngine.GetEnvironment().GetVolumeListenerPosition();
		aWeeBitAway.NormalizeSafe();
		positionForNextReflection.Add(aWeeBitAway);*/
		PlayTwoEchos(echoSound, predelay+existingPredelay, position, existingAttenuation+attenuation, echoDepth+1, 0, envRule);
	}
}

u32 audWeaponAudioEntity::GetCustomEchoSoundForWeapon(audSoundSet echoSet, eWeaponWheelSlot wheelSlot)
{
	switch(wheelSlot)
	{
	case WEAPON_WHEEL_SLOT_PISTOL:
		return echoSet.Find("Pistol").Get();
	case WEAPON_WHEEL_SLOT_SMG:
		return echoSet.Find("SMG").Get();
	case WEAPON_WHEEL_SLOT_RIFLE:
		return echoSet.Find("Rifle").Get();
	case WEAPON_WHEEL_SLOT_SNIPER:
		return echoSet.Find("Sniper").Get();
	case WEAPON_WHEEL_SLOT_SHOTGUN:
		return echoSet.Find("Shotgun").Get();
	case WEAPON_WHEEL_SLOT_HEAVY:
		return echoSet.Find("Heavy").Get();
	default:
		return 0;
	}
}

void audWeaponAudioEntity::PlayBulletBy(u32 weaponAudioHash, const Vector3 &vecStart, const Vector3 &vecEnd, const s32 predelay)
{
	if (weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_FLAMETHROWER", 0x58917377) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_FLAMETHROWER_SCIFI", 0xD6B8E406) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_MINI_FLAMETHROWER", 0xC444739B))
	{
		return;
	}

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSoundBulletBy>(CPacketSoundBulletBy(vecStart, vecEnd, predelay, weaponAudioHash));
	}
#endif // GTA_REPLAY

	if(audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings() 
		&& AUD_GET_TRISTATE_VALUE(audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings()->Flags,FLAG_ID_MICROPHONESETTINGS_DISABLEBULLETSBY) != AUD_TRISTATE_TRUE)
	{
		// check to see if this bullet intersects with a sphere in 0.5m front of the camera
		Vector3 v1,v2;
		Vector3 forwardNorm = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1());
		forwardNorm.Normalize();
		Vector3 centre = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition()) + (forwardNorm*0.5f);
		Vector3 forward = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1());
		Vector3 right = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol0());

		spdSphere sphere = spdSphere(RCC_VEC3V(centre), ScalarV(g_BulletBySphereRadius));

		Vector3 w = (vecEnd - vecStart);
		w.Normalize();

		if(fwGeom::IntersectSphereRay(sphere, vecStart, w, v1, v2))
		{
			Vector3 firstIntersectingLine = (v1 - vecStart);
			Vector3 secondIntersectingLine = (v2 - vecStart);
			f32 origLength2 = (vecEnd - vecStart).Mag2();

			// check the intersecting line isn't any longer than the original line, and is same direction as the ray
			if(firstIntersectingLine.Mag2() < origLength2 && secondIntersectingLine.Mag2() < origLength2 && DotProduct(v1 - vecStart, w) >= 0.0f && DotProduct(v2 - vecStart, w) >= 0.0f)
			{
				u32 now = fwTimer::GetTimeInMilliseconds(), nextTimetoPlay = m_NextBulletByTimeMs;
#if GTA_REPLAY
				if(CReplayMgr::IsPlaying())
				{
					now = CReplayMgr::GetGameTimeAtRelativeFrame(0);
					if(now + g_BulletByMaxTimeMs < nextTimetoPlay) //looks like the editor has been rewound, let's reset the time
					{
						nextTimetoPlay = now;
					}
				}
#endif
				if(now >= nextTimetoPlay)
				{
					m_NextBulletByTimeMs = now + audEngineUtil::GetRandomNumberInRange(g_BulletByMinTimeMs, g_BulletByMaxTimeMs);

					// calc vectors from centre of sphere to intersection points, ignoring height
					v1 = (v1 - centre);
					v1.z = 0.0f;
					v1.Normalize();

					v2 = (v2 - centre);
					v2.z = 0.0f;
					v2.Normalize();

					// calculate angles between these vectors and camera forward
					f32 inDp = DotProduct(v1, forward);
					f32 outDp = DotProduct(v2, forward);
					f32 inAngle = ( RtoD * rage::AcosfSafe(inDp));
					f32 outAngle = ( RtoD * rage::AcosfSafe(outDp));

					if(DotProduct(v1, right) < 0.0f)
					{
						inAngle = 360.f - inAngle;
					}
					if(DotProduct(v2, right) < 0.0f)
					{
						outAngle = 360.f - outAngle;
					}

					//naDisplayf("in dp: %f in angle: %f, out dp: %f out angle: %f", inDp, inAngle, outDp, outAngle);			

					audSoundInitParams initParams;
					BANK_ONLY(initParams.IsAuditioning = g_AuditionWeaponFire;);

					initParams.Pan = (s16)inAngle;
					initParams.Predelay = predelay;

					if (audWeaponAudioEntity::IsRubberBulletGun(weaponAudioHash) || audWeaponAudioEntity::IsRiotShotgun(weaponAudioHash))
					{
						audSoundSetFieldRef customSoundRef;
						customSoundRef.Set(audWeaponAudioEntity::IsRubberBulletGun(weaponAudioHash) ? ATSTRINGHASH("Rubber_Bullet_Sounds", 0x50AB02CD) : ATSTRINGHASH("Riot_Shotgun_Sounds", 0x992CA88C), ATSTRINGHASH("Zip_By", 0x6AAAB67B));
						CreateAndPlaySound(customSoundRef.GetMetadataRef(), &initParams);
					}
					else if (IsRayGun(weaponAudioHash))
					{
						CreateAndPlaySound(ATSTRINGHASH("Laser_Bullets_Zip_By_Master", 0x786563F2), &initParams);
					}
					else
					{
						audSoundSet& soundSet = (Water::GetCameraWaterDepth() > 0.f) ? sm_BulletByUnderwaterSounds : sm_BulletByGeneralSounds;
						audMetadataRef soundIn = soundSet.Find(ATSTRINGHASH("BulletByIn", 0x41A9ACE7));
						audMetadataRef soundOut = soundSet.Find(ATSTRINGHASH("BulletByOut", 0x13C0C933));

#if GTA_REPLAY
						u32 soundInSlowMo = ATSTRINGHASH("BulletByInSlowMo", 0x2674C29A);
						u32 soundOutSlowMo = ATSTRINGHASH("BulletByOutSlowMo", 0x3B71DE0D);
						u32 soundInSuperSlowMo = ATSTRINGHASH("BulletByInSuperSlowMo", 0xC32FC09B);
						u32 soundOutSuperSlowMo = ATSTRINGHASH("BulletByOutSuperSlowMo", 0x4E8611F2);

						if (audNorthAudioEngine::IsInSlowMoVideoEditor() || audNorthAudioEngine::IsSuperSlowVideoEditor())
						{
							float markerSpeed = CReplayMgr::GetMarkerSpeed();

							if (markerSpeed <= REPLAY_SLOW_MO_MARKER_SPEED_THRESHOLD)
							{
								if (soundSet.Find(soundInSlowMo).IsValid())
								{
									soundIn = soundSet.Find(soundInSlowMo);
								}

								if (soundSet.Find(soundOutSlowMo).IsValid())
								{
									soundOut = soundSet.Find(soundOutSlowMo);
								}
							}
							if (markerSpeed <= REPLAY_SUPER_SLOW_MO_MARKER_SPEED_THRESHOLD)
							{
								if (soundSet.Find(soundInSuperSlowMo).IsValid())
								{
									soundIn = soundSet.Find(soundInSuperSlowMo);
								}

								if (soundSet.Find(soundOutSuperSlowMo).IsValid())
								{
									soundOut = soundSet.Find(soundOutSuperSlowMo);
								}
							}
						}
#endif

						CreateAndPlaySound(soundIn, &initParams);

						initParams.Pan = (s16)outAngle;
						CreateAndPlaySound(soundOut, &initParams);
					}
				}
			}
		}

		//grcDebugDraw::Sphere(centre, g_BulletBySphereRadius, Color32(255,0,0), false);
	}
}

bool audWeaponAudioEntity::IsRayGun(u32 weaponAudioHash)
{
	return weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RAYMINIGUN", 0x142840C6) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RAYCARBINE", 0x78E492D8) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RAYPISTOL", 0xADEC5478) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_50_CAL_LASER_TURRET", 0xD88BC488) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_DUAL_50_CAL_LASER_TURRET", 0xE05AC7D4) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_MINIGUN_LASER", 0x55B60F3) ||
		weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_VEHICLE_WEAPON_MINITANK_PLASMA_CANNON", 0xEE747AA7);
}

bool audWeaponAudioEntity::IsRubberBulletGun(u32 weaponAudioHash)
{
	return weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RUBBERGUN", 0x3D4AE4FE);
}

bool audWeaponAudioEntity::IsRiotShotgun(u32 weaponAudioHash)
{
	return weaponAudioHash == ATSTRINGHASH("AUDIO_ITEM_RIOT_SHOTGUN", 0x2C7DC065);
}

void audWeaponAudioEntity::PlayExplosionBulletBy(const Vector3 &vecStart)
{
	Vector3 forwardNorm = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().GetCol1());
	forwardNorm.Normalize();
	Vector3 centre = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition()) + (forwardNorm*0.5f);
	// Have the vector go from the explosion, straight through the sphere centre, and out the other side a wee way
	Vector3 vecEnd = (centre - vecStart);
	if (vecEnd.Mag2() < (g_ExplosionBulletByDistance*g_ExplosionBulletByDistance))
	{
		vecEnd.Scale(1.5f);
		vecEnd.Add(vecStart);
		PlayBulletBy(0u, vecStart, vecEnd, g_ExplosionBulletByPredelay);
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			CReplayMgr::RecordFx<CPacketSoundExplosionBulletBy>(CPacketSoundExplosionBulletBy(vecStart));
		}
#endif

	}
}

void audWeaponAudioEntity::AttachProjectileLoop(const u32 soundHash, CObject *obj, audSound **sound, const bool underwater)
{ 
	SYS_CS_SYNC(m_ProjectileLock);

	if(!sm_ProjectileEvents.IsFull())
	{
		audProjectileData & projectile = sm_ProjectileEvents.Append();
		audSoundSet projectileSounds;
		projectileSounds.Init(soundHash);
		if(projectileSounds.IsInitialised())
		{
			projectile.soundKeyHash = underwater ? ATSTRINGHASH("underwater", 0xA2663418) : ATSTRINGHASH("flight", 0x2570A839);
			projectile.soundSetHash = soundHash;
			projectile.object = obj;
			projectile.soundPtr = sound;
			projectile.isUnderwater = underwater;
		}
	}	
}

void audWeaponAudioEntity::AttachProjectileLoop(const char * soundName, CObject *obj, audSound **sound, bool underwater)
{ 
	SYS_CS_SYNC(m_ProjectileLock);

	if(!sm_ProjectileEvents.IsFull())
	{
		audProjectileData & projectile = sm_ProjectileEvents.Append();

		u32 soundMapKey = atPartialStringHash(soundName);
		if(underwater)
		{
			soundMapKey = atStringHash("_UNDERWATER", soundMapKey);
		}
		else
		{
			soundMapKey = atFinalizeHash(soundMapKey);		
		}


		projectile.soundKeyHash = soundMapKey;
		projectile.soundSetHash = ATSTRINGHASH("CODED_WEAPON_SOUNDS", 0xB148A7E7);
		projectile.object = obj;
		projectile.soundPtr = sound;
		projectile.isUnderwater = underwater;
	}

}

void audWeaponAudioEntity::TriggerDeferredProjectileEvents()
{
	SYS_CS_SYNC(m_ProjectileLock);

	for(int i=0; i<sm_ProjectileEvents.GetCount(); i++)
	{
		if(sm_ProjectileEvents[i].object && sm_ProjectileEvents[i].soundPtr)
		{
			if((*sm_ProjectileEvents[i].soundPtr))
			{
				(*sm_ProjectileEvents[i].soundPtr)->StopAndForget();
			}
			audSoundInitParams initParams;
			initParams.Tracker = sm_ProjectileEvents[i].object->GetPlaceableTracker();
			audSoundSet soundSet;
			soundSet.Init(sm_ProjectileEvents[i].soundSetHash);
			if(soundSet.IsInitialised())
			{
				g_WeaponAudioEntity.CreateSound_PersistentReference(soundSet.Find(sm_ProjectileEvents[i].soundKeyHash), sm_ProjectileEvents[i].soundPtr, &initParams);
		
				REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(sm_ProjectileEvents[i].soundSetHash, sm_ProjectileEvents[i].soundKeyHash, &initParams, *sm_ProjectileEvents[i].soundPtr, NULL, eWeaponSoundEntity);)
			
				if((*sm_ProjectileEvents[i].soundPtr))
				{
					u32 clientVar = (sm_ProjectileEvents[i].isUnderwater?1:0);
					(*sm_ProjectileEvents[i].soundPtr)->SetClientVariable(clientVar);
					(*sm_ProjectileEvents[i].soundPtr)->PrepareAndPlay();
				}
			}
		}
	}

	sm_ProjectileEvents.Reset();
}


void audWeaponAudioEntity::ReleaseScriptWeapon()
{
	if(sm_ScriptWeaponSlot.waveSlot)
	{
		sm_PlayerWeaponSlotManager.FreeWaveSlot(sm_ScriptWeaponSlot.waveSlot);
	}
}



bool audWeaponAudioEntity::AddScriptWeapon(u32 itemHash)
{
	bool isAlreadyLoaded = false;
	audPlayerWeaponSlot * weaponSlot = &sm_ScriptWeaponSlot;
	
	if(!weaponSlot->waveSlot)
	{
		weaponSlot->Init();
	}
	else if(weaponSlot->contextHash == itemHash)
	{
		isAlreadyLoaded = true;
	}
	else
	{
		naAssertf(!weaponSlot->waveSlot, "Trying to load weapon audio from script but there's already one loaded; release the previous weapon audio");
		return false;
	}

	if(!isAlreadyLoaded)
	{
		weaponSlot->contextHash = itemHash;
		LoadDynamicWeaponAudio(weaponSlot, itemHash);
		return weaponSlot->waveSlot!= NULL && weaponSlot->waveSlot->IsLoaded();
	}

	return weaponSlot->waveSlot->IsLoaded(); //the wave bank is already loaded eg as a player weapon
	
}

void audWeaponAudioEntity::ProcessEquippedWeapon()
{
	if(IsWaitingOnEquippedWeaponLoad())
	{
		audPlayerWeaponSlot * equippedSlot = &sm_PlayerEquippedWeaponSlot;

		if(equippedSlot->waveSlot)
		{
			if(equippedSlot->waveSlot->IsLoaded())
			{
				BANK_ONLY(naDisplayf("Weapon audio for %s loaded. Bank: %s (%u)", audNorthAudioEngine::GetMetadataManager().GetObjectName(equippedSlot->audioHash), equippedSlot->waveSlot->waveSlot->GetLoadedBankName(), __LINE__));
				sm_IsWaitingOnEquippedWeaponLoad = false;
				sm_UseBackupWeapons = false;
			}
			return;
		}

		CPed * player = CGameWorld::FindLocalPlayer();
		if(player)
		{
			CPedWeaponManager * weaponMgr =  player->GetWeaponManager();
			if(weaponMgr)
			{
				CWeapon * weapon = weaponMgr->GetEquippedWeapon();
				if(weapon)
				{
					u32 weaponHash = weapon->GetWeaponHash();
					CPedInventory * invent = player->GetInventory();
					if(invent && invent->GetWeapon(weaponHash) && invent->GetWeapon(weaponHash)->GetInfo())
					{
						u32 audioHash = invent->GetWeapon(weaponHash)->GetInfo()->GetAudioHash();
						HandlePlayerEquippedWeapon(weaponHash, audioHash);
					}
				}
			}
		}
	}
}

void audWeaponAudioEntity::ProcessNextWeapon()
{
	if(sm_IsWaitingOnNextWeaponLoad)
	{
		CPed * player = CGameWorld::FindLocalPlayer();
		if(player)
		{
			u32 weaponHash = GetNextWeapon(player);
			if(weaponHash )
			{

				audPlayerWeaponSlot * nextSlot = &sm_PlayerNextWeaponSlot; 
				
				if(nextSlot->waveSlot && nextSlot->contextHash==weaponHash)
				{
					if(nextSlot->waveSlot->IsLoaded())
					{
						sm_IsWaitingOnNextWeaponLoad = false;
					}
					return;
				}

				
				CPedInventory * invent = player->GetInventory();
				if(invent)
				{
					CWeaponItem * weaponItem = invent->GetWeapon(weaponHash);
					if(weaponItem && weaponItem->GetInfo())
					{
						u32 audioHash = weaponItem->GetInfo()->GetAudioHash();
						AddNextInventoryWeapon(player, weaponHash, audioHash);
					}
				}
			}
			else ///no next weapon so let's not keep trying
			{
				//sm_IsWaitingOnNextWeaponLoad = false; 
			}
		}
	}
}

void audWeaponAudioEntity::AddPlayerInventoryWeapon()
{
	//sm_IsWaitingOnNextWeaponLoad = true;
}

void audWeaponAudioEntity::AddNextInventoryWeapon(CPed* player, u32 weaponHash, u32 audioHash)
{
	if(!audioHash || !weaponHash)
	{
		return;
	}

	if(player && weaponHash == GetNextWeapon(player))
	{
		bool isAlreadyLoaded = false;

		audPlayerWeaponSlot * weaponSlot = &sm_PlayerNextWeaponSlot;
		if(!weaponSlot->waveSlot)
		{
			weaponSlot->Init();
		}
		else if(weaponSlot->contextHash == weaponHash)
		{
			isAlreadyLoaded = true;
		}
		else
		{
			sm_PlayerWeaponSlotManager.FreeWaveSlot(weaponSlot->waveSlot);
			weaponSlot->Init();
		}
		
		if(!isAlreadyLoaded)
		{
			weaponSlot->contextHash = weaponHash;
			LoadDynamicWeaponAudio(weaponSlot, audioHash);
		}

		if(!weaponSlot->waveSlot || weaponSlot->waveSlot->IsLoaded())
		{
			sm_IsWaitingOnNextWeaponLoad = true;
		}
		else
		{
			sm_IsWaitingOnNextWeaponLoad = false;
		}
	}
}

void audPlayerWeaponLoadEvent::AddPlayerWeaponLoadEvent(u32 inWeaponHash, u32 inAudioHash)
{
	weaponHash = inWeaponHash;
	audioHash = inAudioHash;
	waitingForLoad = true;
}

void audPlayerWeaponLoadEvent::HandlePlayerWeaponLoad()
{
	if(waitingForLoad)
	{
		g_WeaponAudioEntity.HandlePlayerEquippedWeapon(weaponHash, audioHash BANK_ONLY(, true));
		waitingForLoad = false;
	}
}

void audWeaponAudioEntity::AddPlayerEquippedWeapon(u32 weaponHash, u32 audioHash)
{
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketSndLoadWeaponData>(CPacketSndLoadWeaponData(weaponHash, audioHash, sm_PlayerEquippedWeaponSlot.audioHash));
	}
#endif // GTA_REPLAY

	m_PlayerWeaponLoad.AddPlayerWeaponLoadEvent(weaponHash, audioHash);
}

bool audWeaponAudioEntity::HandlePlayerEquippedWeapon(u32 weaponHash, u32 audioHash, bool BANK_ONLY(isEquippingNow))
{
	bool isAlreadyLoaded = false;

	if(!audioHash || !weaponHash) 
	{
		return true;
	}

	//First sort out loading our equipped weapon audio
	audPlayerWeaponSlot * weaponSlot = &sm_PlayerEquippedWeaponSlot;
	if(!weaponSlot->waveSlot)
	{
		weaponSlot->Init();
	}
	else if(weaponSlot->contextHash == weaponHash)
	{
		isAlreadyLoaded = true;
	}
	else 
	{
		AddPlayerPreviousWeapon(weaponSlot->contextHash, weaponSlot->audioHash);

		sm_PlayerWeaponSlotManager.FreeWaveSlot(weaponSlot->waveSlot);
		weaponSlot->Init();
	}

	if(!isAlreadyLoaded)
	{
#if __BANK
		if(g_ForceWaitForPlayerEquippedWeapon > 0 && isEquippingNow)
		{
			g_ForceWaitForPlayerEquippedWeaponTime = audNorthAudioEngine::GetCurrentTimeInMs() + (u32)(g_ForceWaitForPlayerEquippedWeapon*1000);
		}
#endif

		weaponSlot->contextHash = weaponHash;
		if(!LoadDynamicWeaponAudio(weaponSlot, audioHash))
		{
			return false;
		}
		if(!weaponSlot->waveSlot)
		{
#if RSG_BANK
			const char * settingsName = audNorthAudioEngine::GetMetadataManager().GetObjectName(audioHash);
			if(!settingsName)
			{
				settingsName = "NO OBJECT";
			}
			naDisplayf("Failed to create a weapon waveslot for audio settings %s hash %u", settingsName, audioHash);
#endif

		}
	}

	if(!weaponSlot->waveSlot)
	{
		sm_IsWaitingOnEquippedWeaponLoad = false;
	}
	else
	{
		sm_IsWaitingOnEquippedWeaponLoad = true;
	}


	//If we've forced a weapon wait we won't have a waveslot so need to fake
#if __BANK
	if(g_ForceWaitForPlayerEquippedWeaponTime > audNorthAudioEngine::GetCurrentTimeInMs())
	{
		sm_IsWaitingOnEquippedWeaponLoad = true;
	}
#endif


	return true;
}

bool audWeaponAudioEntity::IsWaitingOnEquippedWeaponLoad() 
{ 
	if(PARAM_noaudiohardware.Get())
	{
		return false;
	}
#if __BANK
	if(audNorthAudioEngine::GetCurrentTimeInMs() <= g_ForceWaitForPlayerEquippedWeaponTime)
	{
		return true;
	}
#endif

		return sm_IsWaitingOnEquippedWeaponLoad; 
}

void audWeaponAudioEntity::AddPlayerPreviousWeapon(u32 weaponHash, u32 audioHash)
{
	bool isAlreadyLoaded = false;

	if(!audioHash || !weaponHash)
	{
		return;
	}

	//First sort out loading our equipped weapon audio
	audPlayerWeaponSlot * weaponSlot = &sm_PlayerNextWeaponSlot;
	if(!weaponSlot->waveSlot)
	{
		weaponSlot->Init();
	}
	else if(weaponSlot->contextHash == weaponHash)
	{
		isAlreadyLoaded = true;
	}
	else 
	{
		sm_PlayerWeaponSlotManager.FreeWaveSlot(weaponSlot->waveSlot);
		weaponSlot->Init();
	}

	if(!isAlreadyLoaded)
	{
		weaponSlot->contextHash = weaponHash;
		LoadDynamicWeaponAudio(weaponSlot, audioHash);
	}
}

bool audWeaponAudioEntity::LoadDynamicWeaponAudio(audPlayerWeaponSlot * weaponSlot, u32 audioHash)
{
	if(!audioHash || !weaponSlot->contextHash || g_PlayerUsesNpcWeapons || PARAM_noaudio.Get())
	{
		return true; 
	} 

	weaponSlot->audioHash = audioHash;

	if(!naVerifyf(!weaponSlot->waveSlot, "We should already have freed this wave slot before calling LoadDynamicWeapon audio (audio settings: %s", audNorthAudioEngine::GetMetadataManager().GetObjectName(audioHash)))
	{
		sm_PlayerWeaponSlotManager.FreeWaveSlot(weaponSlot->waveSlot);
		naAssertf(!weaponSlot->waveSlot, "Tried and failed to free weapon waveslot, for settings %s", audNorthAudioEngine::GetMetadataManager().GetObjectName(audioHash));
		weaponSlot->waveSlot = NULL;
	}


	WeaponSettings * settings = GetWeaponSettings(audioHash, ATSTRINGHASH("player", 0x6f0783f5), true);
	if(settings BANK_ONLY(&& g_ForceWaitForPlayerEquippedWeaponTime < audNorthAudioEngine::GetCurrentTimeInMs()))
	{
		BANK_ONLY(naDisplayf("Loading weapon audio from bank sound %s", g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectName(settings->BankSound));)
		u32 bankId = GetPlayerBankIdFromWeaponSettings(settings);
		if(bankId < AUD_INVALID_BANK_ID)
		{
#if __ASSERT
			audWaveSlot* waveSlot = audWaveSlot::FindLoadedBankWaveSlot(bankId);
			if(waveSlot && waveSlot->IsStatic())
			{
				naAssertf(false,"Already found a statically loaded bank :%s on weaponsettings %s for the context :%u",waveSlot->GetBankName(bankId), audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset), weaponSlot->contextHash);
			}
#endif
			weaponSlot->waveSlot = sm_PlayerWeaponSlotManager.LoadBank(bankId, AUD_WAVE_SLOT_ANY, false, NULL, naWaveLoadPriority::PlayerInteractive);
			if(!weaponSlot->waveSlot)
			{
				sm_UseBackupWeapons = true;	
				BANK_ONLY(naErrorf("Failed to get a waveslot when loading audio for %s", audNorthAudioEngine::GetMetadataManager().GetObjectName(audioHash));)
			}
			else
			{
				sm_UseBackupWeapons = false;
				BANK_ONLY(naDisplayf("Started loading audio for %s", audNorthAudioEngine::GetMetadataManager().GetObjectName(audioHash));)
			}
		}
		else
		{
			sm_UseBackupWeapons = false; //Never use backup weapons if this weapon doesn't require a bank load
		}

#if __DEV
		ValidateWeaponSettings(settings);
#endif
	}
	else
	{
		naWarningf("Couldn't get weapon settings from Item %u", audioHash);
	}

	return true;
}

void audWeaponAudioEntity::RemovePlayerInventoryWeapon(u32 UNUSED_PARAM(weaponHash)) 
{
	//for(u32 i=0; i < g_AudNumPlayerWeaponSlots; i++)
	//{
	//	audPlayerWeaponSlot * slot = &sm_PlayerWeaponSlots[i];
	//	if(slot->waveSlot)
	//	{
	//		if(slot->contextHash == weaponHash)
	//		{
	//			/*slot->isInventory = false;
	//			slot->weaponHash = NULL_HASH;
	//			slot->lastUsedTime = fwTimer::GetTimeInMilliseconds();
	//			sm_PlayerWeaponSlotManager.FreeWaveSlot(slot->waveSlot); 
	//			*/

	//			break;
	//		}
	//	}
	//}
}

void audWeaponAudioEntity::RemoveAllPlayerInventoryWeapons()
{
	for(u32 i=0; i < g_AudNumPlayerWeaponSlots; i++)
	{
		//audPlayerWeaponSlot * slot = &sm_PlayerWeaponSlots[i];
		/*if(slot->waveSlot && slot->isInventory)
		{
			slot->isInventory = false;
			slot->weaponHash = NULL_HASH;
			slot->lastUsedTime = fwTimer::GetTimeInMilliseconds();
		}*/
	}
}

void audWeaponAudioEntity::AddAllInventoryWeaponsOnPed(CPed * ped)
{
	if(ped->IsLocalPlayer())
	{
		sm_WantsToLoadPlayerEquippedWeapon = true;

		if(ped->GetWeaponManager() && ped->GetWeaponManager()->GetEquippedWeapon())
		{
			if(HandlePlayerEquippedWeapon(ped->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash(), ped->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetAudioHash()))
			{
				sm_WantsToLoadPlayerEquippedWeapon = false;

#if GTA_REPLAY
				if(CReplayMgr::IsEditModeActive() == false)
				{
					CReplayMgr::RecordFx<CPacketSndLoadWeaponData>(CPacketSndLoadWeaponData(ped->GetWeaponManager()->GetEquippedWeapon()->GetWeaponHash(), ped->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetAudioHash(), sm_PlayerEquippedWeaponSlot.audioHash));
				}
#endif // GTA_REPLAY
			}
		}
	}
}

u32 audWeaponAudioEntity::GetNextWeapon(CPed * ped, u32 * UNUSED_PARAM(audioHash))
{
	const CWeaponInfoBlob::SlotList& slotList = CWeaponInfoManager::GetSlotListBest();

//	naCondLogf(!ped, DIAG_SEVERITY_FATAL, "pPed is NULL");
	naAssert(ped->GetInventory());

	CPedWeaponManager* weaponManager = ped->GetWeaponManager();
	if(!weaponManager)
	{
		return 0;
	}

	CWeapon * weapon = weaponManager->GetEquippedWeapon();
	if(!weapon)
	{
		return 0;
	}

	
	//u32 equippedWeapon =  weapon->GetWeaponHash();
	//u32 ammoHash = weapon->GetWeaponInfo()->GetAmmoInfo()->GetHash();


	for(s32 i = 0; i < slotList.GetCount(); i++)
	{
		//naErrorf("Looking for weapon hash [%s]", slotList[i].GetCStr());
		const CWeaponItem* pItem = ped->GetInventory()->GetWeaponBySlot(slotList[i].GetHash());
		if(pItem)
		{
			//naErrorf("Item info hash %u, ammo %u equipped weapon hash %u amm %u", pItem->GetInfo()->GetHash(), pItem->GetInfo()->GetAmmoInfo()->GetHash(), equippedWeapon, ammoHash);
			if(ped->GetWeaponManager()->GetWeaponSelector()->GetIsWeaponValid(ped, pItem->GetInfo(), true, false) && pItem->GetInfo()->GetModelIndex() != ped->GetWeaponManager()->GetEquippedWeapon()->GetWeaponInfo()->GetModelIndex()) // && pItem->GetInfo()->GetAmmoInfo()->GetHash()!= ammoHash)
			{
				WeaponSettings * settings = GetWeaponSettings( pItem->GetInfo()->GetAudioHash(), ATSTRINGHASH("player", 0x6f0783f5));
				if(settings && settings->BankSound != g_NullSoundHash)
				{
					/*if(audioHash)
					{
						*audioHash = pItem->GetInfo()->GetAudioHash();
					}*/

					return pItem->GetInfo()->GetHash();  
				}
			}
		}
	}
	return 0;

}

WeaponSettings * audWeaponAudioEntity::GetWeaponSettings(u32 audioHash, u32 contextHash, bool forceContext)
{
	static u32 playerHash = ATSTRINGHASH("player", 0x6f0783f5); 
	if(contextHash == playerHash && !forceContext)
	{
		if(audNorthAudioEngine::GetMicrophones().IsCinematicMicActive())
		{
			contextHash = NULL_HASH;
		}
		else
		{
			MicrophoneSettings * mic = audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings();
			if(mic && AUD_GET_TRISTATE_VALUE(mic->Flags, FLAG_ID_MICROPHONESETTINGS_DISTANTPLAYERWEAPONS) == AUD_TRISTATE_TRUE)
			{
				contextHash = NULL_HASH;
			}
		}
	}

	if(audioHash)
	{
		ItemAudioSettings *audioSettings = audNorthAudioEngine::GetObject<ItemAudioSettings>(audioHash);
		WeaponSettings * settings = NULL;

		if(audioSettings)
		{
			if(contextHash != NULL_HASH)
			{
				for(int i=0; i < audioSettings->numContextSettings; i++)
				{
					if(audioSettings->ContextSettings[i].Context == contextHash)
					{
						settings = audNorthAudioEngine::GetObject<WeaponSettings>(audioSettings->ContextSettings[i].WeaponSettings);
						break;
					}
				}
			}

			if(!settings && contextHash != playerHash && contextHash != NULL_HASH)
			{
				for(int i=0; i < audioSettings->numContextSettings; i++)
				{
					if(audioSettings->ContextSettings[i].Context == playerHash)
					{
						settings = audNorthAudioEngine::GetObject<WeaponSettings>(audioSettings->ContextSettings[i].WeaponSettings);
						break;
					}
				}
			}

			if(!settings)
			{
				settings = audNorthAudioEngine::GetObject<WeaponSettings>(audioSettings->WeaponSettings);
			}
#if __BANK
			else if(contextHash == playerHash && settings->BankSound != g_NullSoundHash)
			{
				if(!sm_PlayerEquippedWeaponSlot.waveSlot)
				{
					naErrorf("Using player weapon settings %s but there is no equipped weapon waveslot", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset));
				}
				else if(!sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot)
				{
					naErrorf("Using player weapon settings %s but equipped weapon waveslot doesn't have a slot set on it", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset));
				}
				else if(!sm_PlayerEquippedWeaponSlot.waveSlot->IsLoaded())
				{
					naErrorf("Using player weapon settings %s but waveslot (%s) isn't loaded", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset), sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot->GetLoadedBankName());
				}
				else if(g_DebugPlayerWeaponSlots)
				{
					naErrorf("Using player weapon settings %s with waveslot %s (secondary slot: %s) ", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(settings->NameTableOffset), sm_PlayerEquippedWeaponSlot.waveSlot->waveSlot->GetLoadedBankName(), sm_PlayerNextWeaponSlot.waveSlot && sm_PlayerNextWeaponSlot.waveSlot->waveSlot ? sm_PlayerNextWeaponSlot.waveSlot->waveSlot->GetLoadedBankName() : "None");
				}
			}
#endif
		}
		return settings;
	}
	return NULL;
}

WeaponSettings * audWeaponAudioEntity::GetWeaponSettings(audMetadataRef settingsRef, u32 contextHash)
{
	static u32 playerHash = ATSTRINGHASH("player", 0x6f0783f5); 
	WeaponSettings * settings = NULL;

	if(contextHash == playerHash)
	{
		if(audNorthAudioEngine::GetMicrophones().IsCinematicMicActive())
		{
			contextHash = NULL_HASH;
		}
		else
		{
			MicrophoneSettings * mic = audNorthAudioEngine::GetMicrophones().GetMicrophoneSettings();
			if(mic && AUD_GET_TRISTATE_VALUE(mic->Flags, FLAG_ID_MICROPHONESETTINGS_DISTANTPLAYERWEAPONS) == AUD_TRISTATE_TRUE)
			{
				contextHash = NULL_HASH;
			}
#if GTA_REPLAY
			if(CReplayMgr::IsPlaying() && camInterface::GetReplayDirector().IsFreeCamActive())
			{
				contextHash = NULL_HASH;
			}
#endif
		}
	}

	const ItemAudioSettings * audioSettings = audNorthAudioEngine::GetMetadataManager().GetObject<ItemAudioSettings>(settingsRef);

	if(audioSettings)
	{
		if(contextHash != NULL_HASH)
		{
			for(int i=0; i < audioSettings->numContextSettings; i++)
			{
				if(audioSettings->ContextSettings[i].Context == contextHash)
				{
					settings = audNorthAudioEngine::GetObject<WeaponSettings>(audioSettings->ContextSettings[i].WeaponSettings);
					break;
				}
			}
		}

		if(!settings && contextHash != playerHash && contextHash != NULL_HASH)
		{
			for(int i=0; i < audioSettings->numContextSettings; i++)
			{
				if(audioSettings->ContextSettings[i].Context == playerHash)
				{
					settings = audNorthAudioEngine::GetObject<WeaponSettings>(audioSettings->ContextSettings[i].WeaponSettings);
					break;
				}
			}
		}

		if(!settings)
		{
			settings = audNorthAudioEngine::GetObject<WeaponSettings>(audioSettings->WeaponSettings);
		}
	}
	return settings;
}

void audWeaponAudioComponent::SetLastImpactTime()
{
	m_LastImpactTime = fwTimer::GetTimeInMilliseconds();
}

#if __DEV
void audWeaponAudioEntity::DebugDraw()
{
	if (PARAM_rdrreportaudio.Get())
	{
		if(g_ShouldDrawReportGoodness)
		{
			for(int i=0; i<4; ++i)
			{
				u8 uMaxIndex = (u8)sm_ReportGoodness.size();
				u8 uIndex1=(i*2)%uMaxIndex;
				u8 uIndex2=(uIndex1+1)%uMaxIndex;
				u8 uIndex3=(uIndex1+2)%uMaxIndex;

				Vec3V vOffset = RCC_VEC3V(sm_RelativeScanPositions[uIndex2*3]);
				vOffset *= ScalarV(V_TEN);
				Vec3V vPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition() + vOffset;
				vPos.SetZ(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition()).GetZ());

				f32 fGoodness = sm_ReportGoodness[uIndex1] + sm_ReportGoodness[uIndex2] + sm_ReportGoodness[uIndex3];
				fGoodness /= 3.0f;

				Vec3V vMinPos(vPos);
				Vec3V vMaxPos(vPos);
				Vec3V offset(ScalarVFromF32(0.25f + (0.25f * fGoodness)));
				vMinPos -= offset;
				vMaxPos += offset;

				Color32 col(1.0f, 1.0f-fGoodness, fGoodness);

				grcDebugDraw::BoxAxisAligned(vMinPos, vMaxPos, col);
			}
		}
	}

	if(g_DebugPlayerWeaponSlots)
	{
		char tempString[256];
		audPlayerWeaponSlot * indexSlot = &sm_PlayerEquippedWeaponSlot;
		if(indexSlot)
		{
			const char * audioName = audNorthAudioEngine::GetMetadataManager().GetObjectName(indexSlot->audioHash);
			if(!audioName)
			{
				audioName = "NO_NAME";
			}
			audDynamicWaveSlot * dynSlot = indexSlot->waveSlot;
			audWaveSlot * waveSlot = dynSlot?dynSlot->waveSlot:NULL;
				formatf(tempString, "Equipped Slot, waveSlot %p (waveslot %p, state %d), context %u, audioName %s loaded bank id %d, loaded bank name %s", dynSlot, waveSlot, waveSlot?waveSlot->GetBankLoadingStatus(waveSlot->GetLoadedBankId()):-1,
				indexSlot->contextHash, audioName,  waveSlot?waveSlot->GetLoadedBankId():-1,  waveSlot?waveSlot->GetLoadedBankName():"");
			grcDebugDraw::Text(Vector2(0.05f, 0.05f), Color32(0,0,255), tempString );
		}
		
		indexSlot = &sm_ScriptWeaponSlot;
		if(indexSlot)
		{
			const char * audioName = audNorthAudioEngine::GetMetadataManager().GetObjectName(indexSlot->audioHash);
			if(!audioName)
			{
				audioName = "NO_NAME";
			}
			audDynamicWaveSlot * dynSlot = indexSlot->waveSlot;
			audWaveSlot * waveSlot = dynSlot?dynSlot->waveSlot:NULL;
			formatf(tempString, "Script Slot, waveSlot %p (waveslot %p, state %d), context %u, audioName %s, loaded bank id %d, loaded bank name %s"
				, dynSlot, waveSlot, waveSlot?waveSlot->GetBankLoadingStatus(waveSlot->GetLoadedBankId()):-1,
				indexSlot->contextHash, audioName, waveSlot?waveSlot->GetLoadedBankId():-1, waveSlot?waveSlot->GetLoadedBankName():"");
			grcDebugDraw::Text(Vector2(0.05f, 0.05f+0.05f), Color32(0,0,255), tempString );
		}
		
		indexSlot = &sm_PlayerNextWeaponSlot;
		if(indexSlot)
		{
			const char * audioName = audNorthAudioEngine::GetMetadataManager().GetObjectName(indexSlot->audioHash);
			if(!audioName)
			{
				audioName = "NO_NAME";
			}
			audDynamicWaveSlot * dynSlot = indexSlot->waveSlot;
			audWaveSlot * waveSlot = dynSlot?dynSlot->waveSlot:NULL;
			formatf(tempString, "Next Slot, waveSlot %p (waveslot %p, state %d), context %u, audioName %s, loaded bank id %d, loaded bank name %s",
				dynSlot, waveSlot, waveSlot?waveSlot->GetBankLoadingStatus(waveSlot->GetLoadedBankId()):-1,
				indexSlot->contextHash, audioName, waveSlot?waveSlot->GetLoadedBankId():-1, waveSlot?waveSlot->GetLoadedBankName():"");
			grcDebugDraw::Text(Vector2(0.05f, 0.05f+0.1f), Color32(0,0,255), tempString );
		}
	}
}
#endif

#if __BANK
extern u32 g_UnderwaterExplosionFilterCutoff;
extern float g_RotaryTurretAudioSpinSpeed;
void audWeaponAudioEntity::AddWidgets(bkBank &bank)
{
	
	bank.PushGroup("Weapons",false);	
		bank.AddSlider("Min Auto Echo Loop Count", &g_MinAutoEchoLoopCount, 0, 10, 1);
		bank.AddSlider("Min Auto Echo Loop Ignore Time", &g_MinAutoEchoLoopCountIgnoreTime, 0, 1000, 10);		
		bank.AddSlider("Veh rotary gun spin spd", &g_RotaryTurretAudioSpinSpeed, 0.f, 10.f, 0.1f);
		bank.AddSlider("Automatic fire timeout", &s_AutoFireReleaseTime, 0, 10000, 10);
		bank.AddToggle("g_AuditionWeaponFire", &g_AuditionWeaponFire);
		bank.AddSlider("Max weapon fires played per frame", &g_MaxWeaponFiresPlayedPerFrame, 0, g_MaxWeaponFireEventsPerFrame, 1);
		bank.AddSlider("Echo volume drop", &g_EchoVolumeDrop, -24.0f, 24.0f, 0.1f);
		bank.AddSlider("g_EchoDepth", &g_EchoDepth, 0, 5, 1);
		bank.AddToggle("g_PositionReportOppositeEcho", &g_PositionReportOppositeEcho);
		bank.AddToggle("g_PlayBothEchos", &g_PlayBothEchos);
		bank.AddToggle("Override Weapon Echo Settings", &g_bForceWeaponEchoSettings);
		bank.AddSlider("Weapon Echo Attenuation", &g_fWeaponEchoAttenuation, -24.0f, 6.0f, 0.1f);
		bank.AddSlider("Weapon Echo Predelay", &g_uWeaponEchoPredelay, 40, 3000, 1);
		bank.AddToggle("g_DisableTightSpotInteriorGuns", &g_DisableTightSpotInteriorGuns);
		bank.AddSlider("UnderwaterExplosionCutoff", &g_UnderwaterExplosionFilterCutoff, 100, kVoiceFilterLPFMaxCutoff, 1);
		bank.AddSlider("g_BulletBySphereRadius", &g_BulletBySphereRadius, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("g_BulletByMinTimeMs", &g_BulletByMinTimeMs, 0, 5000, 10);
		bank.AddSlider("g_BulletByMaxTimeMs", &g_BulletByMaxTimeMs, 0, 5000, 10);
		bank.AddSlider("g_ExplosionBulletByDistance", &g_ExplosionBulletByDistance, 0.0f, 50.0f, 0.1f);
		bank.AddSlider("g_ExplosionBulletByPredelay", &g_ExplosionBulletByPredelay, 0, 1000, 1);
		bank.AddSlider("g_DefaultGunPostSubmixAttenuation", &g_DefaultGunPostSubmixAttenuation, -20.0f, 0.0f, 0.1f);
		bank.AddSlider("g_DuckingAttenuationPerShot", &g_DuckingAttenuationPerShot, -1.0f, 0.0f, 0.01f);
		bank.AddSlider("g_NotFiredForAWhileTime", &g_NotFiredForAWhileTime, 0, 30000, 1);
		bank.AddSlider("g_BeenFiringForAWhileTime", &g_BeenFiringForAWhileTime, 0, 30000, 1);
		bank.AddToggle("g_PlayerGunAlwaysDamped", &g_PlayerGunAlwaysDamped);
		bank.AddToggle("g_PlayerGunNeverDamped", &g_PlayerGunNeverDamped);
		bank.AddToggle("g_PlayerUsesNpcWeapons", &g_PlayerUsesNpcWeapons);
		bank.AddSlider("g_TimeSinceSomeoneNearbyFiredToReduceFactor", &g_TimeSinceSomeoneNearbyFiredToReduceFactor, 0, 10000, 1);
		bank.AddSlider("g_GunFiredFactorIncrease", &g_GunFiredFactorIncrease, 0.0f, 1.0f, 0.01f);
		bank.AddSlider("g_GunNotFiredFactorDecrease", &g_GunNotFiredFactorDecrease, -1.0f, 0.0f, 0.001f);
		bank.AddSlider("g_MaxDistanceToCountTowardFactor", &g_MaxDistanceToCountTowardFactor, 0.0f, 100.0f, 1.0f);
		bank.AddToggle("Debug Weapon Timings", &g_DebugWeaponTimings);
		bank.AddToggle("Debug PlayerWeapon Slots", &g_DebugPlayerWeaponSlots);
		bank.AddSlider("Force wait for equipped weapon (seconds)", &g_ForceWaitForPlayerEquippedWeapon, 0.f, 100.f, 1.f);
		bank.AddSlider("Empty weapon shot timing", &sm_EmptyShotFilter, 0, 5000, 10);
		bank.PushGroup("Interiors");
			bank.AddToggle("Force use interior metrics", &g_ForceUseInteriorMetrics);
			bank.AddToggle("Debug Interior Metrics", &g_DebugInteriorMetrics);
			bank.AddSlider("Debug Interior Wetness", &g_DebugInteriorWetness, 0.f, 1.f, 0.05f);
			bank.AddSlider("Debug Outside Visibility", &g_DebugOutsideVisability, 0.f, 1.f, 0.05f);
			bank.AddSlider("Debug Interior LPF", &g_DebugInteriorLpf, 0, 24000, 100);
			bank.AddSlider("Debug Interior Predelay", &g_DebugInteriorPredelay, 0, 1000, 10);
			bank.AddSlider("Debug Interior Hold variable", &g_DebugInteriorHold, 0.f, 1.f, 0.01f);
			bank.AddSlider("Interior silenced Weapon Wetness", &sm_InteriorSilencedWeaponWetness, 0.f, 1.f, 0.05f);
			bank.AddSlider("Interior transition time", &g_InteriorTransitionTime, 0.01f, 5.f, 0.01f);
		bank.PopGroup();
		bank.PushGroup("Water cannon");
			bank.AddToggle("sm_Smooth", &sm_Smooth);
			bank.AddSlider("sm_TimeToMuteWaterCannonOnVeh (ms)", &sm_TimeToMuteWaterCannon, 0.f, 10000.f, 0.05f);
		bank.PopGroup();
		bank.AddSlider("NpcReportIntensity", &sm_NpcReportIntensity, 0.f, 100.f, 1.f);
		bank.AddSlider("NpcReportIntensitydecreaseRate", &sm_NpcReportIntensityDecreaseRate, 0.f, 10.f, 0.1f);
		bank.AddSlider("NpcReportCullingDistanceSq", &sm_NpcReportCullingDistanceSq, 0.f, 10000.f, 1.f);
		bank.AddSlider("NpcReportIntensityForDistanceCulling", &sm_IntensityForNpcReportDistanceCulling, 0.f, 100.f, 1.f);
		bank.AddSlider("NpcReportIntensityForFullCulling", &sm_IntensityForNpcReportFullCulling, 0.f, 100.f, 1.f);
		bank.AddSlider("PlayerHitBoostVol", &sm_PlayerHitBoostVol, 0.f, 24.f, 1.f);
		bank.AddSlider("PlayerHitBoostRolloff", &sm_PlayerHitBoostRolloff, 1.f, 10.f, 0.1f);
		bank.AddSlider("PlayerHitHoldTime", &sm_PlayerHitBoostHoldTime, 0, 10000, 100);
		bank.AddSlider("PlayerHitReleaseTime", &sm_PlayerhitBoostSmoothTime, 0.01f, 10.f, 0.1f);
		bank.AddToggle("Skip minigun intro", &sm_SkipMinigunSpinUp);
		bank.AddSlider("Anim time filter", &sm_WeaponAnimTimeFilter, 0, 1000, 10);
		bank.AddSlider("Auto Empty shot time filter", &sm_AutoEmptyShotTimeFilter, 0, 1000, 10);
		bank.PopGroup();

	if (PARAM_rdrreportaudio.Get())
	{
		bank.PushGroup("Report Goodness",false);
		bank.AddToggle("Draw Report Goodness", &g_ShouldDrawReportGoodness);
		bank.AddSlider("ReportGoodness0", &sm_ReportGoodness[0], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("ReportGoodness1", &sm_ReportGoodness[1], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("ReportGoodness2", &sm_ReportGoodness[2], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("ReportGoodness3", &sm_ReportGoodness[3], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("ReportGoodness4", &sm_ReportGoodness[4], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("ReportGoodness5", &sm_ReportGoodness[5], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("ReportGoodness6", &sm_ReportGoodness[6], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("ReportGoodness7", &sm_ReportGoodness[7], 0.0f, 1.0f, 0.1f);
		bank.AddSlider("MaxDistForEnemyReport", &g_MaxDistanceForEnemyReport, 0.0f, 100.0f, 1.0f);
		bank.PopGroup();
	}

}
#endif

