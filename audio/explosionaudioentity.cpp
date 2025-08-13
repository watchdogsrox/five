// 
// audio/explosionaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "explosionaudioentity.h"
#include "northaudioengine.h"
#include "audio/environment/environment.h"
#include "frontendaudioentity.h"
#include "weaponaudioentity.h"
#include "audioengine/curve.h"
#include "audioengine/engine.h"
#include "audioengine/engineutil.h"
#include "audiosoundtypes/multitracksound.h"
#include "audiosoundtypes/soundcontrol.h"
#include "event/EventReactions.h"
#include "event/EventGroup.h"
#include "frontend/MiniMap.h"
#include "scene/entity.h"
#include "vehicles/vehicle.h"
#include "vehicles/Boat.h"
#include "profile/profiler.h"
#include "weapons/ExplosionInst.h"
#include "peds/Ped.h"
#include "scene/world/gameWorld.h"
#include "camera/CamInterface.h"
#include "control/replay/Audio/ExplosionAudioPacket.h"

AUDIO_WEAPONS_OPTIMISATIONS() 

EXT_PF_GROUP(TriggerSoundsGroup);
PF_COUNTER(NumExplosions, TriggerSoundsGroup);

#include "debugaudio.h"

PARAM(explosionAudioFailSpew, "Enables printing of soundpool and an assert on first failure to play explosion audio.");

audCurve g_ExplosionDistanceToDebrisPieces, g_DelayDistribution,g_ExplosionDistanceToDelay;
u32 g_UnderwaterExplosionFilterCutoff = 3000;
u32 g_NextDebrisTime = 0;
extern audWeaponAudioEntity g_WeaponAudioEntity;

bank_float g_ExplosionShockwaveIntensityChangeRate = -0.4f;
bank_float g_ExplosionShockwaveRadiusChangeRate = 80.f;
bank_float g_ExplosionShockwaveDist2RadiusFactor = 8.f;
bank_float g_VelocityThreshold = 144.f;
bank_bool g_IgnoreDistancesForExplosionSounds = false;

f32 audExplosionAudioEntity::sm_DistanceThresholdToPlayExplosionDebris = 60.f;

u32 audExplosionAudioEntity::sm_NumExplosionsThisFrame = 0;
u32 audExplosionAudioEntity::sm_DebrisSound = 0;
u32 audExplosionAudioEntity::sm_NumDebrisBitsPlayed = 0;
f32 audExplosionAudioEntity::sm_DebrisVol = 0.0f;
atRangeArray<audLimitedExplosionSoundStruct, AUD_EXPL_TAIL_INSTANCE_LIMIT> audExplosionAudioEntity::sm_TailSndStructs;
atRangeArray<audLimitedExplosionSoundStruct, AUD_EXPL_CAR_DEBRIS_INSTANCE_LIMIT> audExplosionAudioEntity::sm_CarDebrisSndStructs;
atRangeArray<audLimitedExplosionSoundStruct, AUD_MAX_NUM_BULLET_EXPLOSION_SOUNDS> audExplosionAudioEntity::sm_BulletExplosionSndStructs;
atRangeArray<DeferredExplosionStruct, AUD_MAX_NUM_EXPLOSIONS_PER_FRAME> audExplosionAudioEntity::sm_DeferredExplosionStructs;
atRangeArray<audSmokeGrenadeSoundStruct, AUD_MAX_NUM_SMOKE_GRENADE_SOUNDS> audExplosionAudioEntity::sm_SmokeGrenadeSoundStructs;
atFixedArray<u32, AUD_MAX_DEBRIS_BITS> audExplosionAudioEntity::sm_DebrisPlayTimes;

naAudioEntity audExplosionAudioEntity::sm_DebrisAudioEntity;

audExplosionAudioEntity g_audExplosionEntity;

#if __ASSERT
atRangeArray<u32, AUD_FRAMES_TO_TRACK_REQ_EXPLOSIONS> audExplosionAudioEntity::sm_NumExplosionsReqInFrame;
u32 audExplosionAudioEntity::sm_NumRecentReqExplosions = 0;
u32 audExplosionAudioEntity::sm_NumExpReqWriteIndex = 0;
#endif

bool g_EnableDeafeningForExplosions = false;
#if __BANK
bool g_AuditionExplosions = false;
bool audExplosionAudioEntity::sm_PlayOldExplosion = false;
bank_bool g_DoNotLimiExplosionTails = false;
bank_bool g_PlayExplosionSoundsWithDebug = false;
bank_bool g_ShouldPrintPoolIfExplosionSoundFails = true;
#endif

bank_bool g_DisableExplosionSFX = false;

void audExplosionAudioEntity::InitClass()
{
	g_ExplosionDistanceToDebrisPieces.Init("DEBRIS_DISTANCE_TO_NUM_BITS");
	g_ExplosionDistanceToDelay.Init("DEBRIS_DISTANCE_TO_DELAY");
	g_DelayDistribution.Init("DEBRIS_DELAY_DISTRIBUTION");
	g_NextDebrisTime = 0;
	
	for(int i=0; i<AUD_EXPL_TAIL_INSTANCE_LIMIT; i++)
	{
		sm_TailSndStructs[i].Sound = NULL;
		sm_TailSndStructs[i].TimeTriggered = 0;
	}
	for(int i=0; i<AUD_EXPL_CAR_DEBRIS_INSTANCE_LIMIT; i++)
	{
		sm_CarDebrisSndStructs[i].Sound = NULL;
		sm_CarDebrisSndStructs[i].TimeTriggered = 0;
	}
	for(int i=0; i<AUD_MAX_NUM_BULLET_EXPLOSION_SOUNDS; i++)
	{
		sm_BulletExplosionSndStructs[i].Sound = NULL;
		sm_BulletExplosionSndStructs[i].TimeTriggered = 0;
	}
	for(int i=0; i<sm_NumExplosionsThisFrame; ++i)
	{
		sm_DeferredExplosionStructs[i].explosion = NULL;
	}
	for(int i=0; i<AUD_MAX_NUM_SMOKE_GRENADE_SOUNDS; ++i)
	{
		sm_SmokeGrenadeSoundStructs[i].Sound = NULL;
		sm_SmokeGrenadeSoundStructs[i].Position.Set(ORIGIN);
	}

	sm_DebrisAudioEntity.Init();
	g_audExplosionEntity.Init();

#if __BANK
	if(PARAM_explosionAudioFailSpew.Get())
		g_PlayExplosionSoundsWithDebug = true;
#endif

#if __ASSERT
	for(int i=0; i<AUD_FRAMES_TO_TRACK_REQ_EXPLOSIONS; ++i)
		sm_NumExplosionsReqInFrame[i] = 0;
#endif
}

void audExplosionAudioEntity::Init()
{
	naAudioEntity::Init();
	audEntity::SetName("audExplosionAudioEntity");
}

void audExplosionAudioEntity::Update()
{
	for(int i=0; i<sm_NumExplosionsThisFrame && i<sm_DeferredExplosionStructs.GetMaxCount(); ++i)
	{
		phGtaExplosionInst *explosion = sm_DeferredExplosionStructs[i].explosion;
		if(explosion)
		{
			CEntity* explosionOwner = explosion->GetExplosionOwner();
			//Make plane exploding bullets register as exploding bullets, not grenades
			if(sm_DeferredExplosionStructs[i].tag == EXP_TAG_GRENADE && explosionOwner && explosionOwner->GetIsTypeVehicle() && ((CVehicle*)explosionOwner)->GetVehicleType() == VEHICLE_TYPE_PLANE)
				sm_DeferredExplosionStructs[i].tag = EXP_TAG_VEHICLE_BULLET;
			
			g_audExplosionEntity.ReallyPlayExplosion(sm_DeferredExplosionStructs[i].tag, 
														sm_DeferredExplosionStructs[i].position,
														sm_DeferredExplosionStructs[i].entity, 
														sm_DeferredExplosionStructs[i].isUnderwater,
														explosion->GetInteriorLocation(),
														explosion->GetWeaponHash());
		}

		

		sm_DeferredExplosionStructs[i].explosion = NULL;
	}

	const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	bool keepGoing = true;
	while(sm_NumDebrisBitsPlayed < sm_DebrisPlayTimes.GetCount() && keepGoing)
	{
		enum { kDebrisSoundPreemptTimeMs = 100};
		if(sm_DebrisPlayTimes[sm_NumDebrisBitsPlayed] < timeInMs + kDebrisSoundPreemptTimeMs)
		{
			audSoundInitParams initParams;
			initParams.AllowOrphaned = true;
			initParams.Volume = sm_DebrisVol;
			if(sm_DebrisPlayTimes[sm_NumDebrisBitsPlayed] > timeInMs)
			{
				initParams.Predelay = sm_DebrisPlayTimes[sm_NumDebrisBitsPlayed] - timeInMs;
				naAssertf(initParams.Predelay < 5000, "Calculated invalid debris predelay: %u (%u - %u)", initParams.Predelay, sm_DebrisPlayTimes[sm_NumDebrisBitsPlayed], timeInMs);
			}			

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
			{
				audSound* localSnd = NULL;
				sm_DebrisAudioEntity.CreateSound_LocalReference(sm_DebrisSound, &localSnd, &initParams);
				if(!localSnd && g_ShouldPrintPoolIfExplosionSoundFails)
				{
					audSound::GetStaticPool().DebugPrintSoundPool();
					naAssertf(0, "Failed to create explosion sound.  Include full output when reporting.");
					g_ShouldPrintPoolIfExplosionSoundFails = false;
				}
				else if(localSnd)
					localSnd->PrepareAndPlay();
			}
			else 
#endif
			{
				if(g_FrontendAudioEntity.GetSpecialAbilityMode() != audFrontendAudioEntity::kSpecialAbilityModeTrevor)
				{
					sm_DebrisAudioEntity.CreateAndPlaySound(sm_DebrisSound, &initParams);
				}
			}


			++sm_NumDebrisBitsPlayed;
		}
		else
			keepGoing = false;
	}

	if(camInterface::IsFadedOut() REPLAY_ONLY( && !CReplayMgr::IsReplayInControlOfWorld()))
	{
		g_audExplosionEntity.StopAllSounds(false);
		sm_DebrisAudioEntity.StopAllSounds(false);
		sm_DebrisPlayTimes.Reset();
		//StopSmokeGrenadeExplosions();
	}

	sm_NumExplosionsThisFrame = 0;

#if __ASSERT
	if(sm_NumRecentReqExplosions > AUD_MAX_RECENT_EXPLOSIONS)
		naWarningf("Lots of explosions requested of audio in the past second.  Could be a red flag.");
	sm_NumExpReqWriteIndex = (sm_NumExpReqWriteIndex + 1) % AUD_FRAMES_TO_TRACK_REQ_EXPLOSIONS;
	if(naVerifyf(sm_NumRecentReqExplosions >= sm_NumExplosionsReqInFrame[sm_NumExpReqWriteIndex], "Recent audio explosion request bookkeeping has gotten screwed up!"))
	{
		sm_NumRecentReqExplosions -= sm_NumExplosionsReqInFrame[sm_NumExpReqWriteIndex];
		sm_NumExplosionsReqInFrame[sm_NumExpReqWriteIndex] = 0;
	}
	else
	{
		for(int i=0; i<AUD_FRAMES_TO_TRACK_REQ_EXPLOSIONS; ++i)
			sm_NumExplosionsReqInFrame[i] = 0;
		sm_NumRecentReqExplosions = 0;
		sm_NumExpReqWriteIndex = 0;
	}
#endif
}

void audExplosionAudioEntity::PlayExplosion(phGtaExplosionInst *explosion)
{
#if __BANK
	if(g_AuditionExplosions)
		naDisplayf("====================PLAYING %s EXPLOSION===========================",GetExplosionName(explosion->GetExplosionTag()));
#endif
	Vec3V vPos = explosion->GetPosWld();
	BANK_ONLY(if(!sm_PlayOldExplosion))
	{
		PlayExplosion(explosion, explosion->GetExplosionTag(), RCC_VECTOR3(vPos), explosion->GetExplodingEntity(), explosion->GetUnderwaterDepth()>0.0f);
	}
#if __BANK
	else
	{
		PlayOldExplosion(explosion->GetExplosionTag(), RCC_VECTOR3(vPos), explosion->GetExplodingEntity(), explosion->GetUnderwaterDepth()>0.0f);
	}
#endif
}

void audExplosionAudioEntity::PlayExplosion(phGtaExplosionInst *explosion, eExplosionTag tag, const Vector3 &position, CEntity *entity, bool isUnderwater)
{
#if __ASSERT
	sm_NumRecentReqExplosions++;
	sm_NumExplosionsReqInFrame[sm_NumExpReqWriteIndex]++;
#endif
	//naAssertf(sm_NumExplosionsThisFrame < 12, "Trying to trigger more explosion audio in a single frame then allowed. Max %d, current %d", AUD_MAX_NUM_EXPLOSIONS_PER_FRAME, sm_NumExplosionsThisFrame);

	if(sm_NumExplosionsThisFrame < AUD_MAX_NUM_EXPLOSIONS_PER_FRAME)
	{
		sm_DeferredExplosionStructs[sm_NumExplosionsThisFrame].explosion = explosion;
		sm_DeferredExplosionStructs[sm_NumExplosionsThisFrame].tag = tag;
		sm_DeferredExplosionStructs[sm_NumExplosionsThisFrame].position = position;
		sm_DeferredExplosionStructs[sm_NumExplosionsThisFrame].entity= entity;
		sm_DeferredExplosionStructs[sm_NumExplosionsThisFrame].isUnderwater = isUnderwater;
		
	}
	//increment regardless...I want to up when we assert but not increase the array yet...should be okay.
	sm_NumExplosionsThisFrame++;
}

int TimeCompare(const u32* a, const u32* b)
{
	return *a-*b;
}

void audExplosionAudioEntity::ReallyPlayExplosion(eExplosionTag tag, const Vector3 &position, CEntity *entity, bool isUnderwater, fwInteriorLocation intLoc, u32 weaponHash)
{
	naAssertf(GetControllerId()!=AUD_INVALID_CONTROLLER_ENTITY_ID, "audExplosionAudioEntity doesn't have a valid controller");
	PF_INCREMENT(NumExplosions);
	
	naAssert(!sysThreadType::IsUpdateThread() || !audNorthAudioEngine::IsAudioUpdateCurrentlyRunning());
	
	if(g_DisableExplosionSFX || tag == EXP_TAG_DIR_WATER_HYDRANT || (tag == EXP_TAG_MOLOTOV && isUnderwater))
	{
		return;
	}

	if(tag == EXP_TAG_SNOWBALL)
	{
		return;
	}

	//Make plane exploding bullets register as exploding bullets, not grenades
	if(tag == EXP_TAG_GRENADE && entity && entity->GetIsTypeVehicle() && ((CVehicle*)entity)->GetVehicleType() == VEHICLE_TYPE_PLANE)
		tag = EXP_TAG_VEHICLE_BULLET;

	ExplosionAudioSettings* explosionSettings = GetExplosionAudioSettings(tag);
	naAssertf(explosionSettings,"Got null explosion audio settings, that shouldn't be hapenning...");

	rage::audMetadataRef superSlowMoExplosionRef = g_NullSoundRef;
	if(audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		superSlowMoExplosionRef = GetSuperSlowMoExplosionSoundRef(tag);
	}

	audEnvironmentGroupInterface* occlusionGroup = NULL;

	Vector3 pos = position;

	bool useBulletExplosion = false;

	if(weaponHash == ATSTRINGHASH("VEHICLE_WEAPON_PLAYER_SAVAGE", 0x61A31349))
	{
		explosionSettings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSION_SAVAGE_GUN", 0xF8BB5493));
		useBulletExplosion = true;
		if(!explosionSettings)
		{
			return;
		}
	}

	if (entity && entity->GetIsTypeVehicle() && ((CVehicle*)entity)->m_VehicleAudioEntity)
	{
		static const u32 minVehExplosionTimeDelta = 500;
		if((tag == EXP_TAG_TRUCK || tag == EXP_TAG_PLANE || tag == EXP_TAG_BOAT || tag == EXP_TAG_CAR) &&
			((CVehicle*)entity)->m_VehicleAudioEntity->GetLastTimeExplodedVehicle() + minVehExplosionTimeDelta >  g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0))
		{
			return;
		}
		occlusionGroup = ((CVehicle*)entity)->m_VehicleAudioEntity->GetEnvironmentGroup(); 
		((CVehicle*)entity)->m_VehicleAudioEntity->ProcessVehicleExplosion();
	}
	else
	{
		if(tag==EXP_TAG_GRENADE || tag==EXP_TAG_MOLOTOV)
		{
			// We're most likely to get an entityless explosion when a rocket or molotov hits the ground or wall.
			// In that case, we probably actually want to move the occlusion group back down the trajectory.
			// As a quick fix for the most broken case, move it up a bit, so it's not under the ground

			pos += Vector3(0.0f,0.0f,0.5f);
		}
		// We need to rustle up a new occlusion group
		occlusionGroup = naEnvironmentGroup::Allocate("Explosion");
		if (occlusionGroup)
		{
			// Init(Null...) sets no audEntity on the occlusionGorup - as we're on the same thread as the audController,
			// and we're about to kick off a sound, this is fine, and it won't be instantly deleted. 
			((naEnvironmentGroup*)occlusionGroup)->Init(NULL, 20, 0);
			((naEnvironmentGroup*)occlusionGroup)->SetSource(VECTOR3_TO_VEC3V(pos));

			// If we don't have an entity, then use the static fwInteriorLocation to get the interior information
			if(entity)
			{
				((naEnvironmentGroup*)occlusionGroup)->ForceSourceEnvironmentUpdate(entity);
			}
			else
			{
				((naEnvironmentGroup*)occlusionGroup)->SetInteriorInfoWithInteriorLocation(intLoc);
			}
		}
		else
		{
			audErrorf("In PlayExplosion couldn't allocate an occlusion group (tag: %d", tag);
		}
	}

	// NOTE[egarcia]: Bird crap explosions must return before Explosion Heard event is created
	if(tag == EXP_TAG_BIRD_CRAP)
	{
		PlayBirdCrapExplosion(occlusionGroup, pos);
		return;
	}

	// Calling AddOrCombine() here instead of just Add() helps in preventing running out of
	// CEvent objects in the pool, in case multiple explosions go off near each other on the
	// same frame.
	CEventExplosionHeard explosionEvent(pos, CMiniMap::sm_Tunables.Sonar.fSoundRange_Explosion);
	GetEventGlobalGroup()->AddOrCombine(explosionEvent);

	if(tag == EXP_TAG_BZGAS || tag == EXP_TAG_BZGAS_MK2)
	{
		PlaySmokeGrenadeExplosion(entity, occlusionGroup, pos, explosionSettings, isUnderwater);
		return;
	}
	else if(useBulletExplosion || 
		tag == EXP_TAG_VEHICLE_BULLET || tag == EXP_TAG_BULLET || tag == EXP_TAG_RAILGUN || tag == EXP_TAG_EXPLOSIVEAMMO || tag == EXP_TAG_ROGUE_CANNON ||
		tag == EXP_TAG_EXPLOSIVEAMMO_SHOTGUN || tag == EXP_TAG_OPPRESSOR2_CANNON || (tag == EXP_TAG_RAYGUN && weaponHash != ATSTRINGHASH("WEAPON_RAYPISTOL", 0xAF3696A1)))
	{
		REPLAY_ONLY(CReplayMgr::RecordFx(CPacketExplosion(tag, position, isUnderwater, intLoc, weaponHash), entity);)
		PlayBulletExplosion(entity, occlusionGroup, pos, explosionSettings, isUnderwater);
		return;
	}

	CEntity * interiorEntity = NULL;

	bool playVehicleDebris = false;

	u32 explosionSound = explosionSettings->ExplosionSound;

	if(audNorthAudioEngine::IsInSlowMoVideoEditor() && explosionSettings->SlowMoExplosionSound != g_NullSoundHash)
	{
		explosionSound = explosionSettings->SlowMoExplosionSound;
	}
	
	// parse the multitrack sound and kick off its children independently to reduce peak sounds per bucket
	Sound uncompressedMetadata;
	const MultitrackSound *multiTrack = NULL;
	if(superSlowMoExplosionRef.IsValid() && superSlowMoExplosionRef != g_NullSoundRef && audNorthAudioEngine::IsSuperSlowVideoEditor())
	{
		multiTrack = SOUNDFACTORY.DecompressMetadata<MultitrackSound>(superSlowMoExplosionRef, uncompressedMetadata);
	}
	else
	{
		multiTrack = SOUNDFACTORY.DecompressMetadata<MultitrackSound>(explosionSound, uncompressedMetadata);
	}
	if(multiTrack)
	{
		const audCategory *category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(uncompressedMetadata.Category);
		const s16 pitch = uncompressedMetadata.Pitch;

		audSound * localSound = NULL;
		audWeaponInteriorMetrics interiorMetrics;
		if(entity)
		{
			interiorEntity = entity;
		}
		else
		{
			interiorEntity = CGameWorld::FindLocalPlayer();
		}

		if(interiorEntity)
		{
			g_WeaponAudioEntity.GetInteriorMetrics(interiorMetrics, interiorEntity); 
		}


#if __BANK
		if(g_AuditionExplosions)
			naDisplayf("EXPLOSION SOUND Multitrack %u tracks", multiTrack->numSoundRefs);
#endif

		audSoundInitParams initParams;

		BANK_ONLY(	initParams.IsAuditioning = g_AuditionExplosions; );
		initParams.EnvironmentGroup = occlusionGroup;
		initParams.AllowOrphaned = true;
		initParams.Position = pos;
		initParams.Category = category;
		initParams.Pitch = pitch;
		if(isUnderwater)
		{
			initParams.LPFCutoff = g_UnderwaterExplosionFilterCutoff;
			initParams.Volume = -6.f;
		}

		if(tag == EXP_TAG_FIREWORK)
		{
			audSoundInitParams fireworkParams;

			BANK_ONLY(	fireworkParams.IsAuditioning = g_AuditionExplosions; );
			fireworkParams.EnvironmentGroup = occlusionGroup;
			fireworkParams.Position = pos;

			audSoundSet fireworkSounds;
			fireworkSounds.Init(ATSTRINGHASH("FIREWORK_SOUNDS", 0x6A3DC2FA));
			if(fireworkSounds.IsInitialised())
			{
				CreateAndPlaySound(fireworkSounds.Find(ATSTRINGHASH("sparkles", 0xE1B045E2)), &fireworkParams);
			}
		}

		if(tag == EXP_TAG_PETROL_PUMP)
		{
			audSoundInitParams petrolPumpParams;

			BANK_ONLY(	petrolPumpParams.IsAuditioning = g_AuditionExplosions; );
			petrolPumpParams.EnvironmentGroup = occlusionGroup;
			petrolPumpParams.Position = pos;

			CreateAndPlaySound(ATSTRINGHASH("PETROL_PUMP_METAL_EXPLOSION", 0xE1234D11), &petrolPumpParams);
		}

		
		f32 distanceSq = ( pos - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) ).Mag2();
		HandleCodeTriggeredExplosionSound(explosionSound, distanceSq, interiorMetrics.OutsideVisability, &initParams);

		for(u32 i = 0; i < multiTrack->numSoundRefs; i++)
		{			
#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(multiTrack->SoundRef[i].SoundId, initParams);
			else
#endif
			{
				CreateSound_LocalReference(multiTrack->SoundRef[i].SoundId, &localSound, &initParams);

				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), interiorMetrics.OutsideVisability);
					localSound->PrepareAndPlay();
				}
			}
#if __BANK 
			if(g_AuditionExplosions) 
				naDisplayf(" %d -> Playing multitrack child %u ",i, *((u32*)&multiTrack->SoundRef[i].SoundId));
#endif
		}

		if(AUD_GET_TRISTATE_VALUE(explosionSettings->Flags, FLAG_ID_EXPLOSIONAUDIOSETTINGS_ISCAREXPLOSION) == AUD_TRISTATE_TRUE)
		{
			audSoundInitParams initParams;
			BANK_ONLY(	initParams.IsAuditioning = g_AuditionExplosions; );
			initParams.EnvironmentGroup = occlusionGroup;
			initParams.AllowOrphaned = true;
			initParams.Position = pos;
			initParams.Category = category;
			initParams.Pitch = pitch;
			if(isUnderwater)
			{
				initParams.LPFCutoff = g_UnderwaterExplosionFilterCutoff;
				initParams.Volume = -6.f;
			}

			playVehicleDebris = true;
#if __BANK
			if(g_AuditionExplosions)
				naDisplayf("Playing VEHICLE_EXPLOSION");
#endif

			audSound * localSound = NULL;
#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(ATSTRINGHASH("VEHICLE_EXPLOSION", 0x2F04FFD3), initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("VEHICLE_EXPLOSION", 0x2F04FFD3), &localSound, &initParams);

				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), interiorMetrics.OutsideVisability);
					localSound->PrepareAndPlay();
				}
			}
		}

		if(entity)
		{
			if(entity->GetType() == ENTITY_TYPE_VEHICLE)
			{
#if __BANK
				if(g_AuditionExplosions)
					naDisplayf("Triggerin dynamic explosion sounds");
#endif
				((CVehicle *)entity)->GetVehicleAudioEntity()->TriggerDynamicExplosionSounds(isUnderwater);
			}
		}

		if((AUD_GET_TRISTATE_VALUE(explosionSettings->Flags, FLAG_ID_EXPLOSIONAUDIOSETTINGS_DISABLESHOCKWAVE) != AUD_TRISTATE_TRUE))
		{
			audNorthAudioEngine::RegisterLoudSound(pos);
		}
		
		if (g_EnableDeafeningForExplosions && explosionSettings->DeafeningVolume > 0.0f)
		{
			audNorthAudioEngine::GetGtaEnvironment()->RegisterDeafeningSound(pos, explosionSettings->DeafeningVolume);
		}
	}
	else
	{
		naErrorf("Invalid explosion sound");
	}
	
	if(isUnderwater && AUD_GET_TRISTATE_VALUE(explosionSettings->Flags, FLAG_ID_EXPLOSIONAUDIOSETTINGS_DISABLEDEBRIS) != AUD_TRISTATE_TRUE)
	{
		PlayUnderwaterExplosion(pos, entity, occlusionGroup);
	}

	const u32 soundTimerMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	// trigger some random debris near the listener
	if((AUD_GET_TRISTATE_VALUE(explosionSettings->Flags, FLAG_ID_EXPLOSIONAUDIOSETTINGS_DISABLEDEBRIS) != AUD_TRISTATE_TRUE) && soundTimerMs > g_NextDebrisTime && !isUnderwater)
	{
		const u32 randomDebris = ATSTRINGHASH("OPTIMISED_DEBRIS_GENERAL", 0x1BD3AC8B);
		const u32 vehicleDebris = ATSTRINGHASH("VEHICLE_DEBRIS_RANDOM_ONE_SHOTS_LOUDER", 0x11C6D527);
		sm_DebrisSound = playVehicleDebris ? vehicleDebris : randomDebris;
		sm_NumDebrisBitsPlayed = 0;
		f32 debrisTimeScale = 1.f;
		sm_DebrisVol = 0.f;

		bool disableDebris = interiorEntity && interiorEntity->GetIsInInterior();
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer && interiorEntity && ((interiorEntity->GetIsInExterior() && pPlayer->GetIsInInterior()) || (interiorEntity->GetIsInInterior() && 
			(pPlayer->GetIsInExterior() || !interiorEntity->GetAudioInteriorLocation().IsSameLocation(pPlayer->GetAudioInteriorLocation())))))
		{
			disableDebris = true;
		}
		switch(tag)
		{
		case EXP_TAG_GRENADE:
			sm_DebrisVol = -6.f;
			break;
		case EXP_TAG_MOLOTOV:
			disableDebris = true;
			break;
		case EXP_TAG_PLANE:
			sm_DebrisSound = vehicleDebris;
			break;
		default:
			break;
		}
		// trigger some random debris near the listener
		if(!disableDebris && soundTimerMs > g_NextDebrisTime && !isUnderwater)
		{
			Vector3 velocity = CGameWorld::FindLocalPlayerSpeed();
			CVehicle *veh = CGameWorld::FindLocalPlayerVehicle();
			if(veh)
			{
				velocity = veh->GetVelocity();
			}

			if(velocity.Mag2() < (12.f*12.f))
			{
				const f32 distance = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())-pos).Mag();
				const u32 numBits = Min<u32>(AUD_MAX_DEBRIS_BITS, static_cast<u32>(audEngineUtil::GetRandomNumberInRange(0.9f, 1.2f) * g_ExplosionDistanceToDebrisPieces.CalculateValue(distance)));
				const f32 baseDelay = g_ExplosionDistanceToDelay.CalculateValue(distance);
				const u32 timeInMs = soundTimerMs;
				
				sm_DebrisPlayTimes.Resize(numBits);

				for(u32 i = 0; i < numBits; i++)
				{
					const u32 delay = static_cast<u32>(debrisTimeScale * 1000.f * baseDelay * g_DelayDistribution.CalculateValue(audEngineUtil::GetRandomNumberInRange(0.f, 1.f)));

					sm_DebrisPlayTimes[i] = timeInMs + delay;

					if(timeInMs+delay > g_NextDebrisTime)
					{
						g_NextDebrisTime = timeInMs+delay;
					}
				}
				sm_DebrisPlayTimes.QSort(0,-1,TimeCompare);
			}
		}
	}


	// trigger an audio shockwave
	if(!isUnderwater && (AUD_GET_TRISTATE_VALUE(explosionSettings->Flags, FLAG_ID_EXPLOSIONAUDIOSETTINGS_DISABLESHOCKWAVE) != AUD_TRISTATE_TRUE))
	{
		audShockwave shockwave;
		f32 intensityMultiplier = 1.f;
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if(pPlayer && interiorEntity && ((interiorEntity->GetIsInExterior() && pPlayer->GetIsInInterior()) || (interiorEntity->GetIsInInterior() && 
			(pPlayer->GetIsInExterior() || !interiorEntity->GetAudioInteriorLocation().IsSameLocation(pPlayer->GetAudioInteriorLocation())))))
		{
			intensityMultiplier = 0.8f;
		}
		shockwave.centre = pos;
		shockwave.intensity = explosionSettings->ShockwaveIntensity * intensityMultiplier;
		shockwave.intensityChangeRate = g_ExplosionShockwaveIntensityChangeRate;
		shockwave.radius = 1.f;
		shockwave.radiusChangeRate = g_ExplosionShockwaveRadiusChangeRate;
		shockwave.maxRadius = 65.f;
		shockwave.distanceToRadiusFactor = g_ExplosionShockwaveDist2RadiusFactor;
		shockwave.delay = explosionSettings->ShockwaveDelay;
		shockwave.centered = false;
		s32 shockwaveId = kInvalidShockwaveId;
		audNorthAudioEngine::GetGtaEnvironment()->UpdateShockwave(shockwaveId, shockwave);

		if(audNorthAudioEngine::ShouldTriggerPulseHeadset())
		{
			audSoundInitParams initParams;
			initParams.Position = pos;
			g_FrontendAudioEntity.CreateAndPlaySound(g_FrontendAudioEntity.GetPulseHeadsetSounds().Find(ATSTRINGHASH("Explosion", 0xDD91088F)), &initParams);
		}
	}
#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx(CPacketExplosion(tag, position, isUnderwater, intLoc, weaponHash), entity);

		if(explosionSettings->SlowMoExplosionPreSuckSound != g_NullSoundHash)
		{
			CReplayMgr::ReplayPresuckSound(explosionSettings->SlowMoExplosionPreSuckSound, explosionSettings->SlowMoExplosionPreSuckSoundTime, explosionSettings->SlowMoExplosionPreSuckMixerScene, entity);
		}		
	}
#endif // GTA_REPLAY
}
f32 audExplosionAudioEntity::GetExplosionResonanceImpulse(eExplosionTag tag)
{
	if((tag >= EXP_TAG_GRENADE && tag <=EXP_TAG_STICKYBOMB ) || (tag >= EXP_TAG_ROCKET && tag <= EXP_TAG_BIKE)
		|| (tag >= EXP_TAG_BOAT && tag <= EXP_TAG_BULLET) || (tag >= EXP_TAG_TRAIN && tag <= EXP_TAG_PLANE_ROCKET) || tag == EXP_TAG_RAILGUN 
		|| tag == EXP_TAG_VALKYRIE_CANNON || tag == EXP_TAG_BOMBUSHKA_CANNON || tag == EXP_TAG_EXPLOSIVEAMMO || tag == EXP_TAG_EXPLOSIVEAMMO_SHOTGUN 
		|| (tag >= EXP_TAG_HUNTER_BARRAGE && tag <= EXP_TAG_ROGUE_CANNON) || tag == EXP_TAG_OPPRESSOR2_CANNON || tag == EXP_TAG_MORTAR_KINETIC || tag == EXP_TAG_RCTANK_ROCKET)
	{
		return naEnvironment::sm_ExplosionsResonanceImpulse;
	}
	return 0.f;
}
//handle sounds yanked from multitracks for optimization purposes
void audExplosionAudioEntity::HandleCodeTriggeredExplosionSound(u32 soundHash, f32 distanceSq, f32 outsideVisability, audSoundInitParams* initParams)
{
	static const u32 mainExplosion = 0x72779D03; //ATSTRINGHASH("MAIN_EXPLOSION_02", 0x72779D03);
	static const u32 carExplosion = 0x3BC8C6B9; //ATSTRINGHASH("EXPLOSIONS_CAR_MASTER", 0x3BC8C6B9);
	static const u32 grenadeExplosion = 0x4F8767B1; //ATSTRINGHASH("GRENADE_MT", 0x4F8767B1);
	static const f32 twentySq = 20.0f * 20.0f;
	static const f32 eightySq = 80.0f * 80.0f;
	static const f32 oneHundredSq = 100.0f * 100.0f;
	static const f32 oneHundredFifteenSq = 115.0f * 115.0f;
	static const f32 oneHundredFiftySq = 150.0f * 150.0f;

	audSound* localSound = NULL;
	switch(soundHash)
	{
	case mainExplosion:
		if(distanceSq < eightySq || g_IgnoreDistancesForExplosionSounds)
		{
#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSION_DISTORTION_LOOP_DISTANCE_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSION_DISTORTION_LOOP_DISTANCE_WRAPPER", 0x71E5C710), &localSound, initParams);
				if(localSound)
				{
					localSound->PrepareAndPlay();
				}
			}
		}


		if(distanceSq < oneHundredSq || g_IgnoreDistancesForExplosionSounds)
		{
			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_IMPACT_PRE_DISTANCE_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_IMPACT_PRE_DISTANCE_WRAPPER", 0x37FF343D), &localSound, initParams);
				if(localSound)
				{
					localSound->PrepareAndPlay();
				}
			}
		}

		if(distanceSq < oneHundredFifteenSq || g_IgnoreDistancesForExplosionSounds)
		{
			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_MAIN_PRE_DISTANCE_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_MAIN_PRE_DISTANCE_WRAPPER", 0x45E0C674), &localSound, initParams);
				if(localSound)
				{
					localSound->PrepareAndPlay();
				}
			}
		}

		if(distanceSq < oneHundredFiftySq  || g_IgnoreDistancesForExplosionSounds)
		{
			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_INTERIOR_GRENADE_VB_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_INTERIOR_GRENADE_VB_WRAPPER", 0x5FC711F8), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}

			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_INTERIOR_TAIL_VB_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_INTERIOR_TAIL_VB_WRAPPER", 0xEDFC983E), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}
		}

		if(distanceSq > twentySq || g_IgnoreDistancesForExplosionSounds)
		{
			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("MAIN_EXPLOSION_02_DISTANT_CROSSFADE_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("MAIN_EXPLOSION_02_DISTANT_CROSSFADE_WRAPPER", 0xE56A4BDB), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}
		}

#if __BANK
		if(g_DoNotLimiExplosionTails)
		{
			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_INTERIOR_REPORT_VB_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_INTERIOR_REPORT_VB_WRAPPER", 0x6EA8946), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}
		}
		else
#endif
		{
			PlayTailLimitedSound(ATSTRINGHASH("EXPLOSIONS_INTERIOR_REPORT_VB_WRAPPER", 0x6EA8946), outsideVisability, initParams);
		}
		break;
	case carExplosion:
#if __BANK
		if(g_DoNotLimiExplosionTails)
		{
			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_CAR_CLOSE_CRASH_DEBRIS_ENV_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_CAR_CLOSE_CRASH_DEBRIS_ENV_WRAPPER", 0x3D18930A), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}

			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_CAR_DEBRIS_CLOSE_PARTS_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_CAR_DEBRIS_CLOSE_PARTS_WRAPPER", 0xF0ACF783), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}
		}
		else
#endif
		{
			PlayCarDebrisSound(ATSTRINGHASH("EXPLOSIONS_CAR_CLOSE_CRASH_DEBRIS_ENV_WRAPPER", 0x3D18930A), outsideVisability, initParams);
			PlayCarDebrisSound(ATSTRINGHASH("EXPLOSIONS_CAR_DEBRIS_CLOSE_PARTS_WRAPPER", 0xF0ACF783), outsideVisability, initParams);
		}
		break;
	case grenadeExplosion:
#if __BANK
		if(g_DoNotLimiExplosionTails)
		{
			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_INTERIOR_GRENADE_REPORT_VB_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_INTERIOR_GRENADE_REPORT_VB_WRAPPER", 0x4143FE45), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}

			localSound = NULL;

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("EXPLOSIONS_INTERIOR_MAIN_GRENADE_1_VB_WRAPPER"), *initParams);
			else
#endif
			{
				CreateSound_LocalReference(ATSTRINGHASH("EXPLOSIONS_INTERIOR_MAIN_GRENADE_1_VB_WRAPPER", 0x6741C157), &localSound, initParams);
				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
					localSound->PrepareAndPlay();
				}
			}
		}
		else
#endif
		{
			PlayTailLimitedSound(ATSTRINGHASH("EXPLOSIONS_INTERIOR_GRENADE_REPORT_VB_WRAPPER", 0x4143FE45), outsideVisability, initParams);
			PlayTailLimitedSound(ATSTRINGHASH("EXPLOSIONS_INTERIOR_MAIN_GRENADE_1_VB_WRAPPER", 0x6741C157), outsideVisability, initParams);
		}
		break;
	}
}


#if __BANK
void audExplosionAudioEntity::PlayOldExplosion(eExplosionTag tag, const Vector3 &position, CEntity *entity, bool isUnderwater)
{
	naAssertf(GetControllerId()!=AUD_INVALID_CONTROLLER_ENTITY_ID, "audExplosionAudioEntity doesn't have a valid controller");
	PF_INCREMENT(NumExplosions);


	const u32 mainExplosion = ATSTRINGHASH("MAIN_EXPLOSION", 0x02e26727e);
	const u32 grenadeExplosion = ATSTRINGHASH("GRENADE", 0x0b6504bd2);
	const u32 molotovExplosion = ATSTRINGHASH("MOLOTOV_EXPLOSION", 0x029bebbc3);
	const u32 randomDebris = ATSTRINGHASH("DEBRIS_RANDOM_ONE_SHOTS", 0x0af94beae);
	const u32 vehicleDebris = ATSTRINGHASH("VEHICLE_DEBRIS_RANDOM_ONE_SHOTS", 0x006d98111);
	u32 debrisSound = randomDebris;

	u32 explosionSound = 0;
	f32 deafeningVolume = g_SilenceVolume;
	f32 debrisTimeScale = 1.f;
	f32 shockwaveIntensity = 1.f;
	f32 shockwaveDelay = 0.55f;

	f32 debrisVol = 0.f;
	if(g_DisableExplosionSFX || tag == EXP_TAG_DIR_WATER_HYDRANT || (tag == EXP_TAG_MOLOTOV && isUnderwater))
	{
		return;
	}
	bool disableDebris = false;
	bool disableShockwave = false;
	bool isCarExplosion = false;
	switch(tag)
	{
	case EXP_TAG_ROCKET:
		explosionSound = mainExplosion;
		break;
	case EXP_TAG_GRENADE:
		explosionSound = grenadeExplosion;
		debrisVol = -6.f;
		shockwaveIntensity = 0.5f;
		shockwaveDelay = 0.3f;
		break;
	case EXP_TAG_MOLOTOV:
		explosionSound = molotovExplosion;
		disableDebris = true;
		disableShockwave = true;
		break;
	case EXP_TAG_CAR:
	case EXP_TAG_PLANE:
		explosionSound = mainExplosion;
		isCarExplosion = true;
		debrisSound = vehicleDebris;
		break;
	case EXP_TAG_HI_OCTANE:
		explosionSound = mainExplosion;
		break;
	default:
		explosionSound = mainExplosion;
		deafeningVolume = 16.0f;
		break;
	}

	audEnvironmentGroupInterface* occlusionGroup = NULL;

	Vector3 pos = position;
	
	if (entity && entity->GetIsTypeVehicle())
	{
		occlusionGroup = ((CVehicle*)entity)->m_VehicleAudioEntity->GetEnvironmentGroup(); 
	}
	else
	{
		if(tag==EXP_TAG_GRENADE || tag==EXP_TAG_MOLOTOV)
		{
			// offset molotov explosions off of the ground
			pos += Vector3(0.0f,0.0f,0.5f);
		}
		// We need to rustle up a new occlusion group
		occlusionGroup = naEnvironmentGroup::Allocate("OldExplosion");
		if (occlusionGroup)
		{
			// Init(Null...) sets no audEntity on the occlusionGorup - as we're on the same thread as the audController,
			// and we're about to kick off a sound, this is fine, and it won't be instantly deleted. 
			((naEnvironmentGroup*)occlusionGroup)->Init(NULL, 20, 0);
			((naEnvironmentGroup*)occlusionGroup)->SetSource(VECTOR3_TO_VEC3V(pos));
			((naEnvironmentGroup*)occlusionGroup)->ForceSourceEnvironmentUpdate(entity);
		}
	}


	// Calling AddOrCombine() here instead of just Add() helps in preventing running out of
	// CEvent objects in the pool, in case multiple explosions go off near each other on the
	// same frame.
	CEventExplosionHeard explosionEvent(pos, CMiniMap::sm_Tunables.Sonar.fSoundRange_Explosion);
	GetEventGlobalGroup()->AddOrCombine(explosionEvent);

	// parse the multitrack sound and kick off its children independently to reduce peak sounds per bucket
	Sound uncompressedMetadata;
	const MultitrackSound *multiTrack = SOUNDFACTORY.DecompressMetadata<MultitrackSound>(explosionSound, uncompressedMetadata);
	if(multiTrack)
	{
		const audCategory *category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(uncompressedMetadata.Category);
		const s16 pitch = uncompressedMetadata.Pitch;

		for(u32 i = 0; i < multiTrack->numSoundRefs; i++)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = occlusionGroup;
			initParams.AllowOrphaned = true;
			initParams.Position = pos;
			initParams.Category = category;
			initParams.Pitch = pitch;
			if(isUnderwater)
			{
				initParams.LPFCutoff = g_UnderwaterExplosionFilterCutoff;
				initParams.Volume = -6.f;
			}

#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(multiTrack->SoundRef[i].SoundId, initParams);
			else
#endif
			CreateAndPlaySound(multiTrack->SoundRef[i].SoundId, &initParams);
		}

		if(isCarExplosion)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = occlusionGroup;
			initParams.AllowOrphaned = true;
			initParams.Position = pos;
			initParams.Category = category;
			initParams.Pitch = pitch;
			if(isUnderwater)
			{
				initParams.LPFCutoff = g_UnderwaterExplosionFilterCutoff;
				initParams.Volume = -6.f;
			}
#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(atStringHash("VEHICLE_EXPLOSION"), initParams);
			else
#endif
			CreateAndPlaySound("VEHICLE_EXPLOSION", &initParams);
		}

		if(entity)
		{
			if(entity->GetType() == ENTITY_TYPE_VEHICLE)
			{
				((CVehicle *)entity)->GetVehicleAudioEntity()->TriggerDynamicExplosionSounds(isUnderwater);
			}
		}

		if(!isUnderwater)
		{
			audNorthAudioEngine::GetGtaEnvironment()->SpikeResonanceObjects(GetExplosionResonanceImpulse(tag), pos, naEnvironment::sm_ExplosionsResonanceDistance);
		}

		if(!disableShockwave)
		{
			audNorthAudioEngine::RegisterLoudSound(pos);
		}
		
		if (g_EnableDeafeningForExplosions && deafeningVolume > 0.0f)
		{
			audNorthAudioEngine::GetGtaEnvironment()->RegisterDeafeningSound(pos, deafeningVolume);
		}
	}
	else
	{
		naErrorf("Invalid explosion sound");
	}

	if(isUnderwater && !disableDebris)
	{
		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.AllowOrphaned = true;
		initParams.Position = pos;
		initParams.EnvironmentGroup = occlusionGroup;
#if __BANK
		if(g_PlayExplosionSoundsWithDebug)
			PlaySoundWithDebug(ATSTRINGHASH("EXPLOSIONS_UNDERWATER_SPLASH", 0x1CF35931), initParams);
		else
#endif
		CreateAndPlaySound("EXPLOSIONS_UNDERWATER_SPLASH", &initParams);
	}
	
	const u32 soundTimerMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);

	// trigger some random debris near the listener
	if(!disableDebris && soundTimerMs > g_NextDebrisTime && !isUnderwater)
	{
		Vector3 velocity = CGameWorld::FindLocalPlayerSpeed();
		CVehicle *veh = CGameWorld::FindLocalPlayerVehicle();
		if(veh)
		{
			velocity = veh->GetVelocity();
		}

		if(velocity.Mag2() < (12.f*12.f))
		{
			const f32 distance = (VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())-pos).Mag();
			const u32 numBits = static_cast<u32>(audEngineUtil::GetRandomNumberInRange(0.9f, 1.2f) * g_ExplosionDistanceToDebrisPieces.CalculateValue(distance));
			const f32 baseDelay = g_ExplosionDistanceToDelay.CalculateValue(distance);
			const u32 timeInMs = soundTimerMs;
			

			for(u32 i = 0; i < numBits; i++)
			{
				const u32 delay = static_cast<u32>(debrisTimeScale * 1000.f * baseDelay * g_DelayDistribution.CalculateValue(audEngineUtil::GetRandomNumberInRange(0.f, 1.f)));

				audSoundInitParams initParams;
				initParams.Predelay = delay;
				initParams.AllowOrphaned = true;
				initParams.Volume = debrisVol;
#if __BANK
				if(g_PlayExplosionSoundsWithDebug)
					PlaySoundWithDebug(debrisSound, initParams);
				else
#endif
				CreateAndPlaySound(debrisSound, &initParams);

				if(timeInMs+delay > g_NextDebrisTime)
				{
					g_NextDebrisTime = timeInMs+delay;
				}
			}
		}
	}

	// trigger an audio shockwave
	if(!disableShockwave)
	{
		audShockwave shockwave;
		shockwave.centre = pos;
		shockwave.intensity = shockwaveIntensity;
		shockwave.intensityChangeRate = g_ExplosionShockwaveIntensityChangeRate;
		shockwave.radius = 1.f;
		shockwave.radiusChangeRate = g_ExplosionShockwaveRadiusChangeRate;
		shockwave.maxRadius = 65.f;
		shockwave.distanceToRadiusFactor = g_ExplosionShockwaveDist2RadiusFactor;
		shockwave.delay = shockwaveDelay;
		shockwave.centered = false;
		s32 shockwaveId = kInvalidShockwaveId;
		audNorthAudioEngine::GetGtaEnvironment()->UpdateShockwave(shockwaveId, shockwave);
	}
}
#endif	// __BANK

void audExplosionAudioEntity::RemoveExplosion(phGtaExplosionInst *explosion)
{
	for(int i=0; i<sm_NumExplosionsThisFrame && i<sm_DeferredExplosionStructs.GetMaxCount(); ++i)
	{
		if(sm_DeferredExplosionStructs[i].explosion == explosion)
			sm_DeferredExplosionStructs[i].explosion = NULL;
	}
}

ExplosionAudioSettings* audExplosionAudioEntity::GetExplosionAudioSettings(eExplosionTag tag)
{
	ExplosionAudioSettings* settings = NULL;
	switch(tag)
	{
		case EXP_TAG_ROCKET:
		case EXP_TAG_AIR_DEFENCE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_ROCKET", 0x7BEC623B));
			break;
		case EXP_TAG_GRENADE:
		case EXP_TAG_PIPEBOMB:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_GRENADE", 0xC37D3996));
			break;
		case EXP_TAG_GRENADELAUNCHER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_GRENADELAUNCHER", 0xB67FAE22));
			break;
		case EXP_TAG_MORTAR_KINETIC:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_MORTAR_KINETIC", 0x63EFF435));
			break;
		case EXP_TAG_STICKYBOMB:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_STICKYBOMB", 0x31021C1E));
			break;
		case EXP_TAG_PROXMINE:
		case EXP_TAG_VEHICLEMINE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_PROXMINE", 0x8D16B55A));
			break;
		case EXP_TAG_VEHICLEMINE_KINETIC:
		case EXP_TAG_CNC_KINETICRAM:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_VEHICLEMINE_KINETIC", 0xE63B296F));
			break;
		case EXP_TAG_VEHICLEMINE_EMP:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_VEHICLEMINE_EMP", 0x62500B6D));
			break;
		case EXP_TAG_EMPLAUNCHER_EMP:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_EMP_LAUNCHER", 0x9548C646));
			break;
		case EXP_TAG_MINE_CNCSPIKE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_VEHICLEMINE_CNC_SPIKE", 0xA0FFA53F));
			break;
		case EXP_TAG_VEHICLEMINE_SPIKE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_VEHICLEMINE_SPIKE", 0xF1D28C0));
			break;
		case EXP_TAG_VEHICLEMINE_SLICK:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_VEHICLEMINE_SLICK", 0x675B0E5D));
			break;
		case EXP_TAG_VEHICLEMINE_TAR:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_VEHICLEMINE_TAR", 0x5AC97282));
			break;
		case EXP_TAG_BURIEDMINE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_HI_OCTANE", 0xDA502C3F));
			break;
		case EXP_TAG_MOLOTOV:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_MOLOTOV", 0xA23C876F));
			break;
		case EXP_TAG_CAR:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_CAR", 0x603CE36D));
			break;
		case EXP_TAG_PLANE:
		case EXP_TAG_BLIMP:
		case EXP_TAG_BLIMP2:
		case EXP_TAG_SUBMARINE_BIG:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_PLANE", 0x2FE22567));
			break;
		case EXP_TAG_HI_OCTANE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_HI_OCTANE", 0xDA502C3F));
			break;
		case EXP_TAG_TANKSHELL:
		case EXP_TAG_APCSHELL:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_TANKSHELL", 0x7285A301));
			break;
		case EXP_TAG_PETROL_PUMP:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_PETROL_PUMP", 0xA5CCF881));
			break;
		case EXP_TAG_BIKE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BIKE", 0x4DAD94EC));
			break;
		case EXP_TAG_DIR_STEAM:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DIR_STEAM", 0xF693225D));
			break;
		case EXP_TAG_DIR_FLAME:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DIR_FLAME", 0xC5FEF4B));
			break;
		case EXP_TAG_DIR_WATER_HYDRANT:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DIR_WATER_HYDRANT", 0xC95727E));
			break;
		case EXP_TAG_DIR_GAS_CANISTER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DIR_GAS_CANISTER", 0x446AC82));
			break;
		case EXP_TAG_BOAT:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOAT", 0x97A3DDFF));
			break;
		case EXP_TAG_SHIP_DESTROY:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_SHIP_DESTROY", 0xE0331084));
			break;
		case EXP_TAG_TRUCK:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_TRUCK", 0xFB2928B2));
			break;
		case EXP_TAG_BULLET:
		case EXP_TAG_RAILGUN:
		case EXP_TAG_EXPLOSIVEAMMO:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BULLET", 0xCE762528));
			break;
		case EXP_TAG_EXPLOSIVEAMMO_SHOTGUN:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_EXPLOSIVEAMMO_SHOTGUN", 0x7EA75E5));
			break;
		case EXP_TAG_SMOKEGRENADELAUNCHER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_SMOKEGRENADE", 0x8C67D2C));
			break;
		case EXP_TAG_SMOKEGRENADE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_SMOKEGRENADE", 0x8C67D2C));
			break;
		case EXP_TAG_BZGAS:
		case EXP_TAG_BZGAS_MK2:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_SMOKEGRENADE", 0x8C67D2C));
			break;
		case EXP_TAG_GAS_CANISTER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_GAS_CANISTER", 0x9FEC8050));
			break;
		case EXP_TAG_EXTINGUISHER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_EXTINGUISHER", 0x5191AC24));
			break;
		case EXP_TAG_PROGRAMMABLEAR:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_PROGRAMMABLEAR", 0xFC6B7A1B));
			break;
		case EXP_TAG_TRAIN:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_TRAIN", 0x4AB354A1));
			break;
		case EXP_TAG_BARREL:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BARREL", 0x51B704D2));
			break;
		case EXP_TAG_PROPANE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_PROPANE", 0x28F46DA9));
			break;
		case EXP_TAG_DIR_FLAME_EXPLODE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DIR_FLAME_EXPLODE", 0x3FA8DC4D));
			break;
		case EXP_TAG_TANKER:
		case EXP_TAG_GAS_TANK:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_TANKER", 0x4E461F32));
			break;
		case EXP_TAG_BIRD_CRAP:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
			break;
		case EXP_TAG_PLANE_ROCKET:
		case EXP_TAG_HUNTER_BARRAGE:
		case EXP_TAG_RCTANK_ROCKET:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_ROCKET", 0x7BEC623B));
			break;
		case EXP_TAG_VEHICLE_BULLET:
		case EXP_TAG_ROGUE_CANNON:
		case EXP_TAG_OPPRESSOR2_CANNON:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BULLET", 0xCE762528));
			break;
		case EXP_TAG_FIREWORK:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
			break;
		case EXP_TAG_VALKYRIE_CANNON:
		case EXP_TAG_BOMBUSHKA_CANNON:
		case EXP_TAG_HUNTER_CANNON:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_VALKYRIE_CANNON", 0xCC5A2A7B));
			break;
		case EXP_TAG_TORPEDO:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
			break;
		case EXP_TAG_TORPEDO_UNDERWATER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_STROMBERG_TORPEDO", 0x9A931EF2));
			break;
		case EXP_TAG_MINE_UNDERWATER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_UNDERWATER_MINE", 0xFB9D5653));
			break;
		case EXP_TAG_SNOWBALL:
		case EXP_TAG_DONTCARE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
			break;
		case EXP_TAG_BOMB_CLUSTER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOMB_CLUSTER", 0x2D547C81));
			break;
		case EXP_TAG_BOMB_CLUSTER_SECONDARY:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOMB_CLUSTER_SECONDARY", 0x87C434E0));
			break;
		case EXP_TAG_BOMB_WATER_SECONDARY:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOMB_WATER_SECONDARY", 0x3ABF3C96));
			break;
		case EXP_TAG_BOMB_GAS:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOMB_GAS", 0x113A2DA5));
			break;
		case EXP_TAG_BOMB_INCENDIARY:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOMB_INCENDIARY", 0x3E02C9AE));
			break;
		case EXP_TAG_BOMB_STANDARD:
		case EXP_TAG_BOMB_STANDARD_WIDE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOMB_STANDARD", 0x24F28514));
			break;
		case EXP_TAG_BOMB_WATER:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_BOMB_WATER", 0x3FCD46B7));
			break;
		case EXP_TAG_ORBITAL_CANNON:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_ORBITAL_CANNON", 0x94EC0740));
			break;
		case EXP_TAG_SCRIPT_DRONE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_GRENADE", 0xC37D3996));
			break;
		case EXP_TAG_RAYGUN:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_RAYPISTOL", 0x6299BA5B));
			break;
		case EXP_TAG_SCRIPT_MISSILE:
		case EXP_TAG_SCRIPT_MISSILE_LARGE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_ROCKET", 0x7BEC623B));
			break;
		case EXP_TAG_STUNGRENADE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_STUNGRENADE", 0x39A88036));
			break;
		case EXP_TAG_FLASHGRENADE:
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_FLASHGRENADE", 0x6461C6E0));
			break;
		default:
			naAssertf(false,"Wrong explosion tag %d",tag);
			settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DEFAULT", 0xDD691A9B));
			break;
	}
	if(naVerifyf(settings,"Failed getting settings for explosion, tag: %s (%u), defaulting to EXPLOSIONS_DEFAULT", GetExplosionName(tag), tag))
	{
		return settings;
	}
	settings = audNorthAudioEngine::GetObject<ExplosionAudioSettings>(ATSTRINGHASH("EXPLOSIONS_DEFAULT", 0xDD691A9B));
	return settings;
}

rage::audMetadataRef audExplosionAudioEntity::GetSuperSlowMoExplosionSoundRef(eExplosionTag tag)
{
	audSoundSet soundSet;
	soundSet.Init(ATSTRINGHASH("SUPER_SLOWMO_EXPLOSION_SOUNDS", 0x6D8A547E));
	if(!soundSet.IsInitialised())
	{
		return g_NullSoundRef;
	}
	
	rage::audMetadataRef soundRef = g_NullSoundRef;

	switch(tag)
	{
	case EXP_TAG_ROCKET:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_ROCKET", 0x7BEC623B));
		break;
	case EXP_TAG_GRENADE:
	case EXP_TAG_PIPEBOMB:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_GRENADE", 0xC37D3996));
		break;
	case EXP_TAG_GRENADELAUNCHER:
	case EXP_TAG_MORTAR_KINETIC:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_GRENADELAUNCHER", 0xB67FAE22));
		break;
	case EXP_TAG_STICKYBOMB:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_STICKYBOMB", 0x31021C1E));
		break;
	case EXP_TAG_PROXMINE:
	case EXP_TAG_VEHICLEMINE:
	case EXP_TAG_VEHICLEMINE_KINETIC:
	case EXP_TAG_VEHICLEMINE_EMP:
	case EXP_TAG_MINE_CNCSPIKE:
	case EXP_TAG_VEHICLEMINE_SPIKE:
	case EXP_TAG_VEHICLEMINE_SLICK:
	case EXP_TAG_VEHICLEMINE_TAR:
	case EXP_TAG_BURIEDMINE:
	case EXP_TAG_CNC_KINETICRAM:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_PROXMINE", 0x8D16B55A));
		break;
	case EXP_TAG_MOLOTOV:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_MOLOTOV", 0xA23C876F));
		break;
	case EXP_TAG_CAR:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_CAR", 0x603CE36D));
		break;
	case EXP_TAG_PLANE:
	case EXP_TAG_BLIMP:
	case EXP_TAG_BLIMP2:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_PLANE", 0x2FE22567));
		break;
	case EXP_TAG_HI_OCTANE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_HI_OCTANE", 0xDA502C3F));
		break;
	case EXP_TAG_TANKSHELL:
	case EXP_TAG_APCSHELL:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_TANKSHELL", 0x7285A301));
		break;
	case EXP_TAG_PETROL_PUMP:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_PETROL_PUMP", 0xA5CCF881));
		break;
	case EXP_TAG_BIKE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_BIKE", 0x4DAD94EC));
		break;
	case EXP_TAG_DIR_STEAM:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DIR_STEAM", 0xF693225D));
		break;
	case EXP_TAG_DIR_FLAME:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DIR_FLAME", 0xC5FEF4B));
		break;
	case EXP_TAG_DIR_WATER_HYDRANT:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DIR_WATER_HYDRANT", 0xC95727E));
		break;
	case EXP_TAG_DIR_GAS_CANISTER:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DIR_GAS_CANISTER", 0x446AC82));
		break;
	case EXP_TAG_BOAT:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_BOAT", 0x97A3DDFF));
		break;
	case EXP_TAG_SHIP_DESTROY:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_SHIP_DESTROY", 0xE0331084));
		break;
	case EXP_TAG_TRUCK:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_TRUCK", 0xFB2928B2));
		break;
	case EXP_TAG_BULLET:
	case EXP_TAG_RAILGUN:
	case EXP_TAG_EXPLOSIVEAMMO:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_BULLET", 0xCE762528));
		break;
	case EXP_TAG_EXPLOSIVEAMMO_SHOTGUN:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_EXPLOSIVEAMMO_SHOTGUN", 0x7EA75E5));
		break;
	case EXP_TAG_SMOKEGRENADELAUNCHER:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_SMOKEGRENADE", 0x8C67D2C));
		break;
	case EXP_TAG_SMOKEGRENADE:
	case EXP_TAG_BZGAS:
	case EXP_TAG_BZGAS_MK2:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_SMOKEGRENADE", 0x8C67D2C));
		break;
	case EXP_TAG_GAS_CANISTER:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_GAS_CANISTER", 0x9FEC8050));
		break;
	case EXP_TAG_EXTINGUISHER:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_EXTINGUISHER", 0x5191AC24));
		break;
	case EXP_TAG_PROGRAMMABLEAR:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_PROGRAMMABLEAR", 0xFC6B7A1B));
		break;
	case EXP_TAG_TRAIN:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_TRAIN", 0x4AB354A1));
		break;
	case EXP_TAG_BARREL:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_BARREL", 0x51B704D2));
		break;
	case EXP_TAG_PROPANE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_PROPANE", 0x28F46DA9));
		break;
	case EXP_TAG_DIR_FLAME_EXPLODE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DIR_FLAME_EXPLODE", 0x3FA8DC4D));
		break;
	case EXP_TAG_TANKER:
	case EXP_TAG_GAS_TANK:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_TANKER", 0x4E461F32));
		break;
	case EXP_TAG_BIRD_CRAP:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
		break;
	case EXP_TAG_PLANE_ROCKET:
	case EXP_TAG_HUNTER_BARRAGE:
	case EXP_TAG_RCTANK_ROCKET:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_ROCKET", 0x7BEC623B));
		break;
	case EXP_TAG_VEHICLE_BULLET:
	case EXP_TAG_ROGUE_CANNON:
	case EXP_TAG_OPPRESSOR2_CANNON:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_BULLET", 0xCE762528));
		break;
	case EXP_TAG_FIREWORK:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
		break;
	case EXP_TAG_VALKYRIE_CANNON:
	case EXP_TAG_BOMBUSHKA_CANNON:
	case EXP_TAG_HUNTER_CANNON:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_VALKYRIE_CANNON", 0xCC5A2A7B));
		break;
	case EXP_TAG_TORPEDO:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
		break;
	case EXP_TAG_TORPEDO_UNDERWATER:
	case EXP_TAG_MINE_UNDERWATER:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
		break;
	case EXP_TAG_SNOWBALL:
	case EXP_TAG_DONTCARE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DONTCARE", 0x8E4CA0A6));
		break;
	case EXP_TAG_BOMB_CLUSTER:
	case EXP_TAG_BOMB_CLUSTER_SECONDARY:
	case EXP_TAG_BOMB_GAS:
	case EXP_TAG_BOMB_INCENDIARY:
	case EXP_TAG_BOMB_STANDARD:
	case EXP_TAG_BOMB_STANDARD_WIDE:
	case EXP_TAG_BOMB_WATER:
	case EXP_TAG_BOMB_WATER_SECONDARY:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_TANKER", 0x4E461F32));
		break;
	case EXP_TAG_ORBITAL_CANNON:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_ORBITAL_CANNON", 0x94EC0740));
		break;
	case EXP_TAG_SCRIPT_DRONE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_GRENADE", 0xC37D3996));
		break;
	case EXP_TAG_RAYGUN:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_RAYPISTOL", 0x6299BA5B));
		break;
	case EXP_TAG_SCRIPT_MISSILE:
	case EXP_TAG_SCRIPT_MISSILE_LARGE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_ROCKET", 0x7BEC623B));
		break;
	case EXP_TAG_FLASHGRENADE:
	case EXP_TAG_STUNGRENADE:
		soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_GRENADE", 0xC37D3996));
		break;
	default:
		naAssertf(false,"Wrong explosion tag %d",tag);
		break;
	}

	// we don't want this, uncomment for testing
	//if(soundRef == g_NullSoundRef)
	//{
	//	soundRef = soundSet.Find(ATSTRINGHASH("EXPLOSIONS_DEFAULT", 0xDD691A9B));
	//}

	return soundRef;
}

void audExplosionAudioEntity::PlayTailLimitedSound(u32 soundHash, f32 outsideVisability, audSoundInitParams* initParams)
{
	u8 index = GetIndexForLimitedSoundStructUsingTime(&sm_TailSndStructs[0], sm_TailSndStructs.GetMaxCount());
	if(sm_TailSndStructs[index].Sound)
	{
		sm_TailSndStructs[index].Sound->SetReleaseTime(0);
		sm_TailSndStructs[index].Sound->StopAndForget();
	}

	CreateSound_PersistentReference(soundHash, &(sm_TailSndStructs[index].Sound), initParams);
	if(sm_TailSndStructs[index].Sound)
	{
		sm_TailSndStructs[index].Sound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
		sm_TailSndStructs[index].Sound->PrepareAndPlay();
		sm_TailSndStructs[index].TimeTriggered = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}
#if __BANK
	else if(g_PlayExplosionSoundsWithDebug && g_ShouldPrintPoolIfExplosionSoundFails)
	{
		audSound::GetStaticPool().DebugPrintSoundPool();
		naAssertf(0, "Failed to create explosion sound.  Include full output when reporting.");
		g_ShouldPrintPoolIfExplosionSoundFails = false;
	}
#endif
}

void audExplosionAudioEntity::PlayCarDebrisSound(u32 soundHash, f32 outsideVisability, audSoundInitParams* initParams)
{
	u8 index = GetIndexForLimitedSoundStructUsingTime(&sm_CarDebrisSndStructs[0], sm_CarDebrisSndStructs.GetMaxCount());
	if(sm_CarDebrisSndStructs[index].Sound)
	{
		sm_CarDebrisSndStructs[index].Sound->SetReleaseTime(0);
		sm_CarDebrisSndStructs[index].Sound->StopAndForget();
	}


	CreateSound_PersistentReference(soundHash, &(sm_CarDebrisSndStructs[index].Sound), initParams);
	if(sm_CarDebrisSndStructs[index].Sound)
	{
		sm_CarDebrisSndStructs[index].Sound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), outsideVisability);
		sm_CarDebrisSndStructs[index].Sound->PrepareAndPlay();
		sm_CarDebrisSndStructs[index].TimeTriggered = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}
#if __BANK
	else if(g_PlayExplosionSoundsWithDebug && g_ShouldPrintPoolIfExplosionSoundFails)
	{
		audSound::GetStaticPool().DebugPrintSoundPool();
		naAssertf(0, "Failed to create explosion sound.  Include full output when reporting.");
		g_ShouldPrintPoolIfExplosionSoundFails = false;
	}
#endif
}

u8 audExplosionAudioEntity::GetIndexForLimitedSoundStructUsingTime(const audLimitedExplosionSoundStruct* structArray, u32 arraySize)
{
	u8 indexOfOldestSound = 0;
	u32 timeOfOldestSound = ~0U;

	for(u8 i=0; i<arraySize; i++)
	{
		if(structArray[i].Sound == NULL)
			return i;

		if(timeOfOldestSound == ~0U || timeOfOldestSound > structArray[i].TimeTriggered)
		{
			indexOfOldestSound = i;
			timeOfOldestSound = structArray[i].TimeTriggered;
		}
	}

	return indexOfOldestSound;
}

u8 audExplosionAudioEntity::GetIndexForSmokeGrenadeSoundStruct(f32 &distanceSqOfFarthestSound)
{
	Vector3 pos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	u8 indexOfFarthestSound = 0;

	for(u8 i=0; i<AUD_MAX_NUM_SMOKE_GRENADE_SOUNDS; i++)
	{
		if(sm_SmokeGrenadeSoundStructs[i].Sound == NULL)
		{
			distanceSqOfFarthestSound = -1.0f;
			return i;
		}

		f32 distSq = (pos - sm_SmokeGrenadeSoundStructs[i].Position).Mag2();
		if(distanceSqOfFarthestSound == ~0U || distanceSqOfFarthestSound < distSq)
		{
			indexOfFarthestSound = i;
			distanceSqOfFarthestSound = distSq;
		}
	}

	return indexOfFarthestSound;
}

void audExplosionAudioEntity::PlaySmokeGrenadeExplosion(CEntity* entity, audEnvironmentGroupInterface* occlusionGroup, const Vector3& pos, ExplosionAudioSettings* explosionSettings, bool isUnderwater)
{
	audWeaponInteriorMetrics interiorMetrics;
	CEntity * interiorEntity = NULL;
	if(entity)
	{
		interiorEntity = entity;
	}
	else
	{
		interiorEntity = CGameWorld::FindLocalPlayer();
	}

	if(interiorEntity)
	{
		g_WeaponAudioEntity.GetInteriorMetrics(interiorMetrics, interiorEntity); 
	}

	audSoundInitParams initParams;

	BANK_ONLY(	initParams.IsAuditioning = g_AuditionExplosions; );
	initParams.EnvironmentGroup = occlusionGroup;
	initParams.AllowOrphaned = true;
	initParams.Position = pos;
	if(isUnderwater)
	{
		initParams.LPFCutoff = g_UnderwaterExplosionFilterCutoff;
		initParams.Volume = -6.f;
	}

	f32 distanceSq = ( pos - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()) ).Mag2();
	f32 distanceSqOfFarthestSound = -1.0f;
	u8 index = GetIndexForSmokeGrenadeSoundStruct(distanceSqOfFarthestSound);
	if(distanceSqOfFarthestSound == -1.0f || distanceSq < distanceSqOfFarthestSound)
	{
		if(sm_SmokeGrenadeSoundStructs[index].Sound)
		{
			sm_SmokeGrenadeSoundStructs[index].Sound->SetReleaseTime(1000);
			sm_SmokeGrenadeSoundStructs[index].Sound->StopAndForget();
		}
		sm_SmokeGrenadeSoundStructs[index].Position.Set(pos);

		u32 explosionSound = explosionSettings->ExplosionSound;

		if(audNorthAudioEngine::IsInSlowMoVideoEditor() && explosionSettings->SlowMoExplosionSound != g_NullSoundHash)
		{
			explosionSound = explosionSettings->SlowMoExplosionSound;
		}

		CreateSound_PersistentReference(explosionSound, &(sm_SmokeGrenadeSoundStructs[index].Sound), &initParams);
		if(sm_SmokeGrenadeSoundStructs[index].Sound)
		{
			sm_SmokeGrenadeSoundStructs[index].Sound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), interiorMetrics.OutsideVisability);
			sm_SmokeGrenadeSoundStructs[index].Sound->PrepareAndPlay();
		}
#if __BANK
		else if(g_PlayExplosionSoundsWithDebug && g_ShouldPrintPoolIfExplosionSoundFails)
		{
			audSound::GetStaticPool().DebugPrintSoundPool();
			naAssertf(0, "Failed to create explosion sound.  Include full output when reporting.");
			g_ShouldPrintPoolIfExplosionSoundFails = false;
		}
#endif
	}
}

void audExplosionAudioEntity::PlayBirdCrapExplosion(audEnvironmentGroupInterface* occlusionGroup, const Vector3& pos)
{
	audSoundInitParams initParams;

	BANK_ONLY(	initParams.IsAuditioning = g_AuditionExplosions; );
	initParams.EnvironmentGroup = occlusionGroup;
	initParams.Position = pos;

	audSoundSet soundSet;
	soundSet.Init(ATSTRINGHASH("PEYOTE_BIRD_POOP_SOUNDS", 0x6DCF1C4B));
	if(soundSet.IsInitialised())
	{
		CreateAndPlaySound(soundSet.Find(ATSTRINGHASH("POOP_HIT", 0x2B8512EE)), &initParams);
	}
}

void audExplosionAudioEntity::PlayBulletExplosion(CEntity* entity, audEnvironmentGroupInterface* occlusionGroup, const Vector3& pos, ExplosionAudioSettings* explosionSettings, bool isUnderwater)
{
	audWeaponInteriorMetrics interiorMetrics;
	CEntity * interiorEntity = NULL;
	if(entity)
	{
		interiorEntity = entity;
	}
	else
	{
		interiorEntity = CGameWorld::FindLocalPlayer();
	}

	if(interiorEntity)
	{
		g_WeaponAudioEntity.GetInteriorMetrics(interiorMetrics, interiorEntity); 
	}

	audSoundInitParams initParams;

	BANK_ONLY(	initParams.IsAuditioning = g_AuditionExplosions; );
	initParams.EnvironmentGroup = occlusionGroup;
	initParams.AllowOrphaned = true;
	initParams.Position = pos;
	if(isUnderwater)
	{
		initParams.LPFCutoff = g_UnderwaterExplosionFilterCutoff;
		initParams.Volume = -6.f;
	}

	u8 index = GetIndexForLimitedSoundStructUsingTime(&sm_BulletExplosionSndStructs[0], sm_BulletExplosionSndStructs.GetMaxCount());
	
	if(sm_BulletExplosionSndStructs[index].Sound)
	{
		sm_BulletExplosionSndStructs[index].Sound->SetReleaseTime(100);
		sm_BulletExplosionSndStructs[index].Sound->StopAndForget();
	}

	u32 explosionSound = explosionSettings->ExplosionSound;

	if(audNorthAudioEngine::IsInSlowMoVideoEditor() && explosionSettings->SlowMoExplosionSound != g_NullSoundHash)
	{
		explosionSound = explosionSettings->SlowMoExplosionSound;
	}

	// Hacky fix for GTA DLC bug 2986853
	if(explosionSound == ATSTRINGHASH("EXPLOSIONS_BULLET_JET_MASTER", 0xD7E38B53) && isUnderwater)
	{
		explosionSound = ATSTRINGHASH("EXPLOSIONS_BULLET_JET_MASTER_UNDERWATER", 0x5E8D8265);
	}
	else if(explosionSound == ATSTRINGHASH("EXPLOSIONS_BULLET_JET_SLOW_MO", 0xDBBF0C87))
	{
		explosionSound = ATSTRINGHASH("EXPLOSIONS_BULLET_JET_SLOW_MO_UNDERWATER", 0x8B29299E);
	}
	// end hacky fix

	CreateSound_PersistentReference(explosionSound, &(sm_BulletExplosionSndStructs[index].Sound), &initParams);
	if(sm_BulletExplosionSndStructs[index].Sound)
	{
		sm_BulletExplosionSndStructs[index].Sound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), interiorMetrics.OutsideVisability);
		sm_BulletExplosionSndStructs[index].Sound->PrepareAndPlay();
		sm_BulletExplosionSndStructs[index].TimeTriggered = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
	}
#if __BANK
	else if(g_PlayExplosionSoundsWithDebug && g_ShouldPrintPoolIfExplosionSoundFails)
	{
		audSound::GetStaticPool().DebugPrintSoundPool();
		naAssertf(0, "Failed to create explosion sound.  Include full output when reporting.");
		g_ShouldPrintPoolIfExplosionSoundFails = false;
	}
#endif
}

void audExplosionAudioEntity::StopSmokeGrenadeExplosions()
{
	for(int i=0; i<AUD_MAX_NUM_SMOKE_GRENADE_SOUNDS; i++)
	{
		if(sm_SmokeGrenadeSoundStructs[i].Sound)
		{
			sm_SmokeGrenadeSoundStructs[i].Sound->SetReleaseTime(200);
			sm_SmokeGrenadeSoundStructs[i].Sound->StopAndForget();
		}
	}
}

void audExplosionAudioEntity::PlayUnderwaterExplosion(const Vector3 &pos, CEntity *entity, audEnvironmentGroupInterface*occlusionGroup)
{
	// parse the multitrack sound and kick off its children independently to reduce peak sounds per bucket
	CEntity* interiorEntity = NULL;
	Sound uncompressedMetadata;
	const MultitrackSound *multiTrack = SOUNDFACTORY.DecompressMetadata<MultitrackSound>(ATSTRINGHASH("WATER_EXPLOSION", 0xC1728A), uncompressedMetadata);
	if(multiTrack)
	{
		const audCategory *category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(uncompressedMetadata.Category);
		const s16 pitch = uncompressedMetadata.Pitch;

		audSound * localSound = NULL;
		audWeaponInteriorMetrics interiorMetrics;
		if(entity)
		{
			interiorEntity = entity;
		}
		else
		{
			interiorEntity = CGameWorld::FindLocalPlayer();
		}

		if(interiorEntity)
		{
			g_WeaponAudioEntity.GetInteriorMetrics(interiorMetrics, interiorEntity); 
		}


#if __BANK
		if(g_AuditionExplosions)
			naDisplayf("EXPLOSION SOUND Multitrack %u tracks", multiTrack->numSoundRefs);
#endif

		audSoundInitParams initParams;

		BANK_ONLY(	initParams.IsAuditioning = g_AuditionExplosions; );
		initParams.EnvironmentGroup = occlusionGroup;
		initParams.AllowOrphaned = true;
		initParams.Position = pos;
		initParams.Category = category;
		initParams.Pitch = pitch;

		for(u32 i = 0; i < multiTrack->numSoundRefs; i++)
		{			
#if __BANK
			if(g_PlayExplosionSoundsWithDebug)
				PlaySoundWithDebug(multiTrack->SoundRef[i].SoundId, initParams);
			else
#endif
			{
				CreateSound_LocalReference(multiTrack->SoundRef[i].SoundId, &localSound, &initParams);

				if(localSound)
				{
					localSound->FindAndSetVariableValue(ATSTRINGHASH("OutsideVisability", 0xFBBD9B47), interiorMetrics.OutsideVisability);
					localSound->PrepareAndPlay();
				}
			}
#if __BANK 
			if(g_AuditionExplosions) 
				naDisplayf(" %d -> Playing multitrack child %u ",i, *((u32*)&multiTrack->SoundRef[i].SoundId));
#endif
		}
	}
}

#if __BANK
void audExplosionAudioEntity::PlaySoundWithDebug(u32 soundHash, const audSoundInitParams& params)
{
	audSound* snd = NULL;
	sm_DebrisAudioEntity.CreateSound_LocalReference(soundHash, &snd, &params);
	if(!snd && g_ShouldPrintPoolIfExplosionSoundFails)
	{
		audSound::GetStaticPool().DebugPrintSoundPool();
		naAssertf(0, "Failed to create explosion sound.  Include full output when reporting.");
		g_ShouldPrintPoolIfExplosionSoundFails = false;
	}
	else if(snd)
		snd->PrepareAndPlay();
}

void audExplosionAudioEntity::PlaySoundWithDebug(audMetadataRef soundRef, const audSoundInitParams& params)
{
	audSound* snd;
	CreateSound_LocalReference(soundRef, &snd, &params);
	if(!snd && g_ShouldPrintPoolIfExplosionSoundFails)
	{
		audSound::GetStaticPool().DebugPrintSoundPool();
		naAssertf(0, "Failed to create explosion sound.  Include full output when reporting.");
		g_ShouldPrintPoolIfExplosionSoundFails = false;
	}
	else if(snd)
		snd->PrepareAndPlay();
}

const char* audExplosionAudioEntity::GetExplosionName(eExplosionTag tag)
{
	switch(tag)
	{
	case EXP_TAG_ROCKET:
		return "EXPLOSIONS_ROCKET";
	case EXP_TAG_GRENADE:
		return "EXPLOSIONS_GRENADE";
	case EXP_TAG_GRENADELAUNCHER:
		return "EXPLOSIONS_GRENADELAUNCHER";
	case EXP_TAG_STICKYBOMB:
		return "EXPLOSIONS_STICKYBOMB";
	case EXP_TAG_MOLOTOV:
		return "EXPLOSIONS_MOLOTOV";
	case EXP_TAG_CAR:
		return "EXPLOSIONS_CAR";
	case EXP_TAG_PLANE:
		return "EXPLOSIONS_PLANE";
	case EXP_TAG_HI_OCTANE:
		return "EXPLOSIONS_HI_OCTANE";
	case EXP_TAG_TANKSHELL:
	case EXP_TAG_APCSHELL:
		return "EXPLOSIONS_TANKSHELL";
	case EXP_TAG_PETROL_PUMP:
		return "EXPLOSIONS_PETROL_PUMP";
	case EXP_TAG_BIKE:
		return "EXPLOSIONS_BIKE";
	case EXP_TAG_DIR_STEAM:
		return "EXPLOSIONS_DIR_STEAM";
	case EXP_TAG_DIR_FLAME:
		return "EXPLOSIONS_DIR_FLAME";
	case EXP_TAG_DIR_WATER_HYDRANT:
		return "EXPLOSIONS_DIR_WATER_HYDRANT";
	case EXP_TAG_DIR_GAS_CANISTER:
		return "EXPLOSIONS_DIR_GAS_CANISTER";
	case EXP_TAG_BOAT:
		return "EXPLOSIONS_BOAT";
	case EXP_TAG_SHIP_DESTROY:
		return "EXPLOSIONS_SHIP_DESTROY";
	case EXP_TAG_TRUCK:
		return "EXPLOSIONS_TRUCK";
	case EXP_TAG_BULLET:
	case EXP_TAG_RAILGUN:
	case EXP_TAG_EXPLOSIVEAMMO:
		return "EXPLOSIONS_BULLET";
	case EXP_TAG_EXPLOSIVEAMMO_SHOTGUN:
		return "EXPLOSIONS_EXPLOSIVEAMMO_SHOTGUN";
	case EXP_TAG_SMOKEGRENADELAUNCHER:
		return "EXPLOSIONS_SMOKEGRENADE";
	case EXP_TAG_SMOKEGRENADE:
		return "EXPLOSIONS_SMOKEGRENADE";
	case EXP_TAG_BZGAS:
	case EXP_TAG_BZGAS_MK2:
		return "EXPLOSIONS_SMOKEGRENADE";
	case EXP_TAG_GAS_CANISTER:
		return "EXPLOSIONS_GAS_CANISTER";
	case EXP_TAG_EXTINGUISHER:
		return "EXPLOSIONS_EXTINGUISHER";
	case EXP_TAG_PROGRAMMABLEAR:
		return "EXPLOSIONS_PROGRAMMABLEAR";
	case EXP_TAG_TRAIN:
		return "EXPLOSIONS_TRAIN";
	case EXP_TAG_BARREL:
		return "EXPLOSIONS_BARREL";
	case EXP_TAG_PROPANE:
		return "EXPLOSIONS_PROPANE";
	case EXP_TAG_DIR_FLAME_EXPLODE:
		return "EXPLOSIONS_DIR_FLAME_EXPLODE";
	case EXP_TAG_TANKER:
		return "EXPLOSIONS_TANKER";
	case EXP_TAG_VEHICLE_BULLET:
		return "EXPLOSIONS_VEHICLE_BULLET";
	case EXP_TAG_DONTCARE:
		return "EXPLOSIONS_DONTCARE";
	case EXP_TAG_VALKYRIE_CANNON:
	case EXP_TAG_BOMBUSHKA_CANNON:
		return "EXPLOSIONS_GRENADE";
	case EXP_TAG_PIPEBOMB:
		return "EXPLOSIONS_PIPEBOMB";
	case EXP_TAG_TORPEDO:
		return "EXPLOSIONS_TORPEDO";
	case EXP_TAG_TORPEDO_UNDERWATER:
		return "EXPLOSIONS_TORPEDO_UNDERWATER";
	case EXP_TAG_BOMB_CLUSTER:
		return "EXPLOSIONS_BOMB_CLUSTER";
	case EXP_TAG_BOMB_CLUSTER_SECONDARY:
		return "EXPLOSIONS_BOMB_CLUSTER_SECONDARY";
	case EXP_TAG_BOMB_WATER_SECONDARY:
		return "EXPLOSIONS_BOMB_WATER_SECONDARY";
	case EXP_TAG_BOMB_GAS:
		return "EXPLOSIONS_BOMB_GAS";
	case EXP_TAG_BOMB_INCENDIARY:
		return "EXPLOSIONS_BOMB_INCENDIARY";
	case EXP_TAG_BOMB_STANDARD:
	case EXP_TAG_BOMB_STANDARD_WIDE:
		return "EXPLOSIONS_BOMB_STANDARD";
	case EXP_TAG_BOMB_WATER:
		return "EXPLOSIONS_BOMB_WATER";
	case EXP_TAG_MINE_UNDERWATER:
		return "EXPLOSIONS_MINE_UNDERWATER";
	case EXP_TAG_ORBITAL_CANNON:
		return "EXPLOSIONS_ORBITAL_CANNON";
	case EXP_TAG_MORTAR_KINETIC:
		return "EXPLOSIONS_MORTAR_KINETIC";
	case EXP_TAG_VEHICLEMINE_KINETIC:
	case EXP_TAG_CNC_KINETICRAM:
		return "EXPLOSIONS_VEHICLEMINE_KINETIC";
	case EXP_TAG_VEHICLEMINE_EMP:
		return "EXPLOSIONS_VEHICLEMINE_EMP";
	case EXP_TAG_EMPLAUNCHER_EMP:
		return "EXPLOSIONS_EMP_LAUNCHER";
	case EXP_TAG_MINE_CNCSPIKE:
		return "EXPLOSIONS_VEHICLEMINE_CNC_SPIKE";		
	case EXP_TAG_VEHICLEMINE_SPIKE:
		return "EXPLOSIONS_VEHICLEMINE_SPIKE";
	case EXP_TAG_VEHICLEMINE_SLICK:
		return "EXPLOSIONS_VEHICLEMINE_SLICK";
	case EXP_TAG_VEHICLEMINE_TAR:
		return "EXPLOSIONS_VEHICLEMINE_TAR";
	case EXP_TAG_BURIEDMINE:
		return "EXPLOSIONS_BURIEDMINE";
	case EXP_TAG_SCRIPT_DRONE:
		return "EXPLOSIONS_GRENADE";
	case EXP_TAG_RAYGUN:
		return "EXPLOSIONS_BULLET";
	case EXP_TAG_SCRIPT_MISSILE:
		return "EXP_TAG_SCRIPT_MISSILE";
	case EXP_TAG_FLASHGRENADE:
	case EXP_TAG_STUNGRENADE:
		return "EXPLOSIONS_GRENADE";
	case EXP_TAG_SCRIPT_MISSILE_LARGE:
		return "EXP_TAG_SCRIPT_MISSILE_LARGE";
	default:
		return "EXPLOSIONS_DEFAULT";
	}
}

void audExplosionAudioEntity::AddWidgets(bkBank& bank)
{
	bank.PushGroup("ExplosionAudioEntity",false);
	bank.AddToggle("Play old explosions",&sm_PlayOldExplosion);
	bank.AddToggle("Audition explosion",&g_AuditionExplosions);
	bank.AddToggle("g_DoNotLimiExplosionTails explosion", &g_DoNotLimiExplosionTails);
	bank.AddSlider("Shockwave Radius Change Rate", &g_ExplosionShockwaveRadiusChangeRate, 0.f, 200.f, 1.f);
	bank.AddSlider("Max velocity to trigger explosions",&g_VelocityThreshold,0.f,200.f,1.f);
	bank.AddSlider("Min distance to trigger explosions debris",&sm_DistanceThresholdToPlayExplosionDebris,0.f,200.f,1.f);
	bank.AddToggle("g_PlayExplosionSoundsWithDebug", &g_PlayExplosionSoundsWithDebug);
	bank.AddToggle("g_ShouldPrintPoolIfExplosionSoundFails", &g_ShouldPrintPoolIfExplosionSoundFails);
	bank.AddToggle("g_IgnoreDistancesForExplosionSounds", &g_IgnoreDistancesForExplosionSounds);
	bank.PopGroup();
}

#endif	// __BANK
