// 
// audio/audBicycleAudioEntity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#include "audioengine/engine.h"
#include "audio/northaudioengine.h"
#include "audiosoundtypes/simplesound.h"
#include "bicycleaudioentity.h"
#include "task/Vehicle/TaskInVehicle.h"
#include "Peds/PedIntelligence.h"
#include "game/weather.h"
#include "vehicles/wheel.h"
#include "vehicles/bike.h"
#include "renderer/HierarchyIds.h"

#if GTA_REPLAY
#include "control/replay/ReplayExtensions.h"
#endif // GTA_REPLAY

AUDIO_VEHICLES_OPTIMISATIONS()

bool g_ShowBicycleDebugInfo = false;
extern CWeather g_weather;
u32 g_BicycleActivationDistSq = 35 * 35;

audCurve audBicycleAudioEntity::sm_SuspensionVolCurve;
audCurve audBicycleAudioEntity::sm_GritLeanAngleToLinVolCurve;
audCurve audBicycleAudioEntity::sm_GritSpeedToLinVolCurve;

// ----------------------------------------------------------------
// audBicycleAudioEntity constructor
// ----------------------------------------------------------------
audBicycleAudioEntity::audBicycleAudioEntity() : 
audVehicleAudioEntity()
{
	m_VehicleType = AUD_VEHICLE_BICYCLE;
	m_SprocketSound = NULL;
	m_ChainSound = NULL;
	m_TyreGritSound = NULL;

	for(u32 i = 0; i < 2; i++)
	{
		m_SuspensionSounds[i] = NULL;
	}		
}

// ----------------------------------------------------------------
// audBicycleAudioEntity reset
// ----------------------------------------------------------------
void audBicycleAudioEntity::Reset()
{
	audVehicleAudioEntity::Reset();
	m_BicycleAudioSettings = NULL;
	m_WasSprinting = false;
	m_BrakeBlockPlayed = false;
	m_InvPedalRadius = -1.0f;
	m_PrevTyreGritSound = g_NullSoundHash;

	for(u32 i = 0; i < 2; i++)
	{
		m_LastCompressionChange[i] = 0.f;
	}	
}

// ----------------------------------------------------------------
// Get the bicycle settings data
// ----------------------------------------------------------------
BicycleAudioSettings* audBicycleAudioEntity::GetBicycleAudioSettings()
{
	BicycleAudioSettings* settings = NULL;

	if(g_AudioEngine.IsAudioEnabled())
	{
		if(m_ForcedGameObject != 0u)
		{
			settings = audNorthAudioEngine::GetObject<BicycleAudioSettings>(m_ForcedGameObject);
			m_ForcedGameObject = 0u;
		}

		if(!settings)
		{
			if(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash() != 0)
			{
				settings = audNorthAudioEngine::GetObject<BicycleAudioSettings>(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash());
			}

			if(!settings)
			{
				settings = audNorthAudioEngine::GetObject<BicycleAudioSettings>(GetVehicleModelNameHash());
			}
		}

		if(!audVerifyf(settings, "Couldn't find bicycle settings for bicycle: %s", GetVehicleModelName()))
		{
			settings = audNorthAudioEngine::GetObject<BicycleAudioSettings>(ATSTRINGHASH("SCORCHER", 0xF4E1AA15));
		}

		s32 leftPedalId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(BMX_PEDAL_L);
		s32 rightPedalId = m_Vehicle->GetVehicleModelInfo()->GetBoneIndex(BMX_PEDAL_R);

		if(leftPedalId >-1 && rightPedalId >-1)
		{
			Vector3 pedalLPos = m_Vehicle->GetLocalMtx(leftPedalId).d;
			Vector3 pedalRPos = m_Vehicle->GetLocalMtx(rightPedalId).d;
			m_InvPedalRadius = 1.0f/abs(pedalLPos.z - pedalRPos.z);
		}
	}

	return settings;
}

// ----------------------------------------------------------------
// Initialise any static class variables
// ----------------------------------------------------------------
void audBicycleAudioEntity::InitClass()
{
	StaticConditionalWarning(sm_SuspensionVolCurve.Init(ATSTRINGHASH("SUSPENSION_COMPRESSION_TO_VOL", 0x2BEC9A45)), "Invalid SuspVolCurve");
	StaticConditionalWarning(sm_GritLeanAngleToLinVolCurve.Init(ATSTRINGHASH("BICYCLE_GRIT_LEAN_ANGLE_TO_LIN_VOLUME", 0xEC8D95)), "Invalid GritLeanAngleCurve");
	StaticConditionalWarning(sm_GritSpeedToLinVolCurve.Init(ATSTRINGHASH("BICYCLE_GRIT_SPEED_TO_LIN_VOLUME", 0xA9DDE9EF)), "Invalid GritSpeedCurve");
}

// ----------------------------------------------------------------
// Initialise the bicycle settings
// ----------------------------------------------------------------
bool audBicycleAudioEntity::InitVehicleSpecific()
{
	m_BicycleAudioSettings = GetBicycleAudioSettings();

	if(m_BicycleAudioSettings)
	{
		SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());

		return true;
	}

	return false;
}
void audBicycleAudioEntity::StopHorn(bool /*checkForCarModShop*/, bool /*onlyStopIfPlayer*/)
{
	m_PlayerHornOn = false;
}
u32 audBicycleAudioEntity::GetVehicleHornSoundHash(bool UNUSED_PARAM(ignoreMods))
{
	if(m_BicycleAudioSettings)
	{
		return m_BicycleAudioSettings->BellSound;
	}
	else 
	{
		m_BicycleAudioSettings = GetBicycleAudioSettings();

		if(m_BicycleAudioSettings)
		{
			return m_BicycleAudioSettings->BellSound;
		}
	}
	return 0;
}
// ----------------------------------------------------------------
// Update anything bicycle specific
// ----------------------------------------------------------------
void audBicycleAudioEntity::UpdateVehicleSpecific(audVehicleVariables& UNUSED_PARAM(vehicleVariables), audVehicleDSPSettings& UNUSED_PARAM(variables)) 
{
	if(IsReal())
	{
		if(m_EngineWaveSlot && m_EngineWaveSlot->IsLoaded())
		{
#if __BANK
			if(g_ShowBicycleDebugInfo && m_IsPlayerVehicle)
			{
				UpdateDebug();
			}
#endif

			UpdateDriveTrainSounds();
			UpdateSuspension();
			UpdateBrakeSounds();
			UpdateWheelSounds();
		}
	}

	SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());
}

// ----------------------------------------------------------------
// Acquire a wave slot
// ----------------------------------------------------------------
bool audBicycleAudioEntity::AcquireWaveSlot()
{
	RequestWaveSlot(&audVehicleAudioEntity::sm_StandardWaveSlotManager, m_BicycleAudioSettings->ChainLoop, false);
	return m_EngineWaveSlot != NULL;
}

// ----------------------------------------------------------------
// Get the desired boat LOD
// ----------------------------------------------------------------
audVehicleLOD audBicycleAudioEntity::GetDesiredLOD(f32 UNUSED_PARAM(fwdSpeedRatio), u32 distFromListenerSq, bool UNUSED_PARAM(visibleBySniper))
{
	if(!m_Vehicle->GetDriver() && !m_IsPlayerVehicle && GetDriveWheel()->GetGroundSpeed() < 0.1f)
	{
		return AUD_VEHICLE_LOD_DISABLED;
	}
	else if(m_IsFocusVehicle || distFromListenerSq < (g_BicycleActivationDistSq * sm_ActivationRangeScale))
	{
		return AUD_VEHICLE_LOD_REAL;
	}

	return AUD_VEHICLE_LOD_DISABLED;
}

// -------------------------------------------------------------------------------
// Convert the bicycle to disabled
// -------------------------------------------------------------------------------
void audBicycleAudioEntity::ConvertToDisabled()
{
	StopAndForgetSounds(m_ChainSound, m_SprocketSound, m_TyreGritSound);
}

// ----------------------------------------------------------------
// GetShallowWaterWheelSound
// ----------------------------------------------------------------
audMetadataRef audBicycleAudioEntity::GetShallowWaterWheelSound() const
{
	return sm_ExtrasSoundSet.Find(ATSTRINGHASH("BicycleWheelWaterLoop", 0x71F65A6B));
}

// ----------------------------------------------------------------
// Update anything to do with the drive train
// ----------------------------------------------------------------
void audBicycleAudioEntity::UpdateDriveTrainSounds()
{
	audSoundInitParams initParams;
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	initParams.Tracker = m_Vehicle->GetPlaceableTracker();
	CWheel* wheel = GetDriveWheel();
	audAssert(wheel);
	
	f32 bikeSpeed = wheel->GetGroundSpeed();
	CTaskMotionOnBicycle* task = NULL;
	
	if(m_Vehicle->GetDriver())
	{
		task = (CTaskMotionOnBicycle *)m_Vehicle->GetDriver()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE);
	}

#if GTA_REPLAY
	if(CReplayMgr::IsEnabled() && CReplayMgr::ShouldRecord() && m_Vehicle && task)
	{
		//if we don't have the extension then we need to get one
		if(ReplayBicycleExtension::HasExtension(m_Vehicle) == false)
		{
			if(!ReplayBicycleExtension::Add(m_Vehicle))
			{
				replayAssertf(false, "Failed to add a bicycle extension...ran out?");
			}
		}

		if(ReplayBicycleExtension::HasExtension(m_Vehicle) == true)
		{
			ReplayBicycleExtension::SetIsPedalling(m_Vehicle, task->IsPedPedalling());
			ReplayBicycleExtension::SetPedallingRate(m_Vehicle, task->GetPedalRate());
		}
	}
#endif // GTA_REPLAY

	f32 timeScale = fwTimer::GetTimeWarpActive();
	REPLAY_ONLY(timeScale *= CReplayMgr::GetMarkerSpeed());

	if ((task && task->IsPedPedalling())	
		REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && ReplayBicycleExtension::HasExtension(m_Vehicle) && ReplayBicycleExtension::GetIsPedalling(m_Vehicle))))
	{
		if(!m_ChainSound)
		{
			CreateAndPlaySound_Persistent(m_BicycleAudioSettings->ChainLoop, &m_ChainSound, &initParams);
			CreateAndPlaySound(m_BicycleAudioSettings->RePedalSound, &initParams);
		}
		else
		{
			m_ChainSound->FindAndSetVariableValue(ATSTRINGHASH("SPEED", 0xf997622b), bikeSpeed * timeScale);
		} 
	}
	else
	{
		StopAndForgetSounds(m_ChainSound);
	}

	if(((task && !task->IsPedPedalling()) || (!m_Vehicle->GetDriver()) 
		REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && ReplayBicycleExtension::HasExtension(m_Vehicle) && !ReplayBicycleExtension::GetIsPedalling(m_Vehicle))))
		&& bikeSpeed > 0.1f && m_Vehicle->GetStatus() != STATUS_WRECKED)
	{
		if(!m_SprocketSound)
		{
			CreateAndPlaySound_Persistent(m_BicycleAudioSettings->SprocketSound, &m_SprocketSound, &initParams);
		}
		else
		{
			m_SprocketSound->FindAndSetVariableValue(ATSTRINGHASH("SPEED", 0xf997622b), bikeSpeed * timeScale);
		}
	}
	else
	{
		StopAndForgetSounds(m_SprocketSound);
	}

	if ((task && task->GetPedalRate() > 1.0f) 
		REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && ReplayBicycleExtension::HasExtension(m_Vehicle) && ReplayBicycleExtension::GetPedallingRate(m_Vehicle) > 1.0f)))
	{
		if(!m_WasSprinting)
		{
			CreateAndPlaySound(m_BicycleAudioSettings->GearChangeSound, &initParams);
		}

		m_WasSprinting = true;
	}
	else
	{
		m_WasSprinting = false;
	}

	CPed* driver = m_Vehicle->GetDriver();

	if(driver && m_InvPedalRadius > 0.0f)
	{
		Vector3 footLeftPosition;
		Vector3 footRightPosition;

		footLeftPosition = driver->GetBonePositionCached(BONETAG_L_PH_FOOT);
		footRightPosition = driver->GetBonePositionCached(BONETAG_R_PH_FOOT);

		f32 footDist = abs(footLeftPosition.GetZ() - footRightPosition.GetZ());
		f32 footDistRatio = footDist * m_InvPedalRadius;

#if __BANK
		if(g_ShowBicycleDebugInfo && m_IsPlayerVehicle)
		{
			static const char *meterNames[] = {"Pedal Ratio"};
			static audMeterList meterList;
			static f32 meterValues[1];

			meterValues[0] = footDistRatio;
			meterList.left = 790.f;
			meterList.bottom = 620.f;
			meterList.width = 400.f;
			meterList.height = 200.f;
			meterList.values = &meterValues[0];
			meterList.names = meterNames;
			meterList.numValues = sizeof(meterNames)/sizeof(meterNames[0]);
			audNorthAudioEngine::DrawLevelMeters(&meterList);
		}
#endif
		
		if(m_WheelDetailLoop)
		{
			m_WheelDetailLoop->FindAndSetVariableValue(ATSTRINGHASH("BICYCLE_PEDAL_STROKE_GAMESET_VB", 0xAF3C7472), footDistRatio);
		}
	}
}

// ----------------------------------------------------------------
// Update anything to do with wheels
// ----------------------------------------------------------------
void audBicycleAudioEntity::UpdateWheelSounds()
{
	f32 bikeSpeed = GetDriveWheel()->GetGroundSpeed();
	f32 gritLeanAngleVol = sm_GritLeanAngleToLinVolCurve.CalculateValue(Abs(((CBike*)m_Vehicle)->GetLeanAngle()));
	f32 gritSpeedVol = sm_GritSpeedToLinVolCurve.CalculateValue(Abs(bikeSpeed));
	f32 finalVolLin = GetDriveWheel()->GetIsTouching() ? gritSpeedVol * gritLeanAngleVol : 0.0f;
	u32 gritSound = g_NullSoundHash;
	
	if(m_CachedMaterialSettings)
	{
		gritSound = m_CachedMaterialSettings->BicycleTyreGritSound;
	}

	if(m_TyreGritSound && gritSound != m_PrevTyreGritSound)
	{
		m_TyreGritSound->StopAndForget();
	}

	if(gritSound != g_NullSoundHash)
	{
		if(finalVolLin > g_SilenceVolumeLin)
		{
			if(!m_TyreGritSound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				initParams.TrackEntityPosition = true;
				CreateAndPlaySound_Persistent(gritSound, &m_TyreGritSound, &initParams);
			}

			if(m_TyreGritSound)
			{
				m_TyreGritSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(finalVolLin));
			}
		}
		else if(m_TyreGritSound)
		{
			m_TyreGritSound->StopAndForget();
		}
	}

	m_PrevTyreGritSound = gritSound;
}

// ----------------------------------------------------------------
// Update anything to do with suspension
// ----------------------------------------------------------------
void audBicycleAudioEntity::UpdateSuspension()
{
	if(m_IsPlayerVehicle)
	{
		// suspension sounds
		for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
		{
			Vector3 pos;
			GetWheelPosition(i, pos);

			f32 compressionChange = m_Vehicle->GetWheel(i)->GetCompressionChange() * fwTimer::GetInvTimeStep();

			// use the max of the previous frame and this frame, since physics is double stepped otherwise we'll miss some
			f32 compression = Max(m_Vehicle->GetWheel(i)->GetCompression()-m_Vehicle->GetWheel(i)->GetCompressionChange(),m_Vehicle->GetWheel(i)->GetCompression());

			if(m_Vehicle->m_nVehicleFlags.bIsAsleep)
			{
				// ignore physics if asleep or the wheel isn't touching anything
				compressionChange = 0.0f;
				compression = 0.0f;
			}

			const f32 absCompressionChange = Abs(compressionChange);
			f32 damageFactor = 1.0f - m_Vehicle->GetWheel(i)->GetSuspensionHealth() * SUSPENSION_HEALTH_DEFAULT_INV;
			f32 ratio = Clamp((absCompressionChange - m_BicycleAudioSettings->MinSuspCompThresh) / (m_BicycleAudioSettings->MaxSuspCompThres - m_BicycleAudioSettings->MinSuspCompThresh - (damageFactor*2.f)), 0.0f,1.0f);

			// suspension bottoming out
			static bank_float maxScalar = 1.5f;
			if(compression >= m_Vehicle->GetWheel(i)->GetMaxTravelDelta() * maxScalar)
			{
				GetCollisionAudio().TriggerJumpLand(5.f);
			}

			if(m_SuspensionSounds[i])
			{
				if(compressionChange*m_LastCompressionChange[i] < 0.f)
				{
					// suspension has changed direction - cancel previous sound
					m_SuspensionSounds[i]->StopAndForget();
				}
				else
				{
					m_SuspensionSounds[i]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(ratio)));
					m_SuspensionSounds[i]->SetRequestedPosition(pos);
				}
			}

			if(absCompressionChange > m_BicycleAudioSettings->MinSuspCompThresh)
			{
				if(!m_SuspensionSounds[i])
				{
					// trigger a suspension sound
					u32 soundHash;
					if(compressionChange > 0)
					{
						soundHash = m_BicycleAudioSettings->SuspensionDown;
					}
					else
					{
						soundHash = m_BicycleAudioSettings->SuspensionUp;
					}

					audSoundInitParams initParams;
					initParams.UpdateEntity = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.Position = pos;
					initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(ratio));
					CreateAndPlaySound_Persistent(soundHash, &m_SuspensionSounds[i], &initParams);
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Update the brake sounds
// ----------------------------------------------------------------
void audBicycleAudioEntity::UpdateBrakeSounds()
{
	f32 force = 0.0f;

	for(s32 i = 0; i < m_Vehicle->GetNumWheels(); i++)
	{
		CWheel *wheel = m_Vehicle->GetWheel(i);
		force = Max(force, wheel->GetBrakeForce());
	}

	f32 brakePedal = Clamp(force*10.0f, 0.0f, 1.0f);
	f32 bikeSpeed = GetDriveWheel()->GetGroundSpeed();

	if(brakePedal >= 1.0f && bikeSpeed > 3.0f)
	{
		if(!m_BrakeBlockPlayed)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.Tracker = m_Vehicle->GetPlaceableTracker();

			if(g_weather.GetWetness() > 0.5f)
			{
				CreateAndPlaySound(m_BicycleAudioSettings->BrakeBlockWet, &initParams);
			}
			else
			{
				CreateAndPlaySound(m_BicycleAudioSettings->BrakeBlock, &initParams);
			}
			
			m_BrakeBlockPlayed = true;
		}
	}
	// Can play again once we've released the brake
	else if(m_Vehicle->GetBrake() == 0.0f && brakePedal == 0.0f)
	{
		m_BrakeBlockPlayed = false;
	}
}

// ----------------------------------------------------------------
// Get the jump landing sound
// ----------------------------------------------------------------
u32 audBicycleAudioEntity::GetJumpLandingSound(bool UNUSED_PARAM(interiorView)) const
{
	if(m_BicycleAudioSettings)
	{
		return m_BicycleAudioSettings->JumpLandSound;
	}

	return g_NullSoundHash;
}

// ----------------------------------------------------------------
// Get the damaged jump landing sound
// ----------------------------------------------------------------
u32 audBicycleAudioEntity::GetDamagedJumpLandingSound(bool UNUSED_PARAM(interiorView)) const
{
	if(m_BicycleAudioSettings)
	{
		return m_BicycleAudioSettings->DamagedJumpLandSound;
	}

	return g_NullSoundHash;
}

// ----------------------------------------------------------------
// Get the volume trim on the road noise
// ----------------------------------------------------------------
f32 audBicycleAudioEntity::GetRoadNoiseVolumeScale() const
{
	if(m_BicycleAudioSettings)
	{
		return m_BicycleAudioSettings->RoadNoiseVolumeScale;
	}

	return audVehicleAudioEntity::GetRoadNoiseVolumeScale();
}

// ----------------------------------------------------------------
// Get the volume trim on the skid sounds
// ----------------------------------------------------------------
f32 audBicycleAudioEntity::GetSkidVolumeScale() const
{
	if(m_BicycleAudioSettings)
	{
		return m_BicycleAudioSettings->SkidVolumeScale;
	}

	return audVehicleAudioEntity::GetSkidVolumeScale();
}

// ----------------------------------------------------------------
// Get the vehicle collision settings
// ----------------------------------------------------------------
VehicleCollisionSettings* audBicycleAudioEntity::GetVehicleCollisionSettings() const
{
	if(m_BicycleAudioSettings)
	{
		return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(m_BicycleAudioSettings->VehicleCollisions);
	}

	return audVehicleAudioEntity::GetVehicleCollisionSettings();
}

// ----------------------------------------------------------------
// GetDriveWheel
// ----------------------------------------------------------------
CWheel* audBicycleAudioEntity::GetDriveWheel() const
{
	for(u32 loop = 0; loop < m_Vehicle->GetNumWheels(); loop++)
	{
		CWheel* wheel = m_Vehicle->GetWheel(loop);
		
		if(wheel->GetHierarchyId() == BIKE_WHEEL_R)
		{
			return wheel;
		}
	}

	return NULL;
}

// ----------------------------------------------------------------
// Get a sound from the object data
// ----------------------------------------------------------------
u32 audBicycleAudioEntity::GetSoundFromObjectData(u32 fieldHash) const
{
	if(m_BicycleAudioSettings) 
	{ 
		u32 *ret = (u32 *)m_BicycleAudioSettings->GetFieldPtr(fieldHash);
		return (ret) ? *ret : 0; 
	} 
	return 0;
}

#if __BANK
// ----------------------------------------------------------------
// Update debug stuff
// ----------------------------------------------------------------
void audBicycleAudioEntity::UpdateDebug()
{
	if(m_Vehicle->GetDriver())
	{
		CTaskMotionOnBicycle* task = (CTaskMotionOnBicycle *)m_Vehicle->GetDriver()->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_ON_BICYCLE);

		if (task)
		{
			CWheel* wheel = GetDriveWheel();

			if(wheel)
			{
				f32 bikeSpeed = wheel->GetGroundSpeed();
				f32 pedalSpeed = task->GetPedalRate();

				char tempString[128];
				sprintf(tempString, "Throttle:%.02f Pedal Rate:%.02f Wheel Speed:%.02f Ped Pedalling:%s Lean Angle: %0.02f", m_Vehicle->GetThrottle(), pedalSpeed, bikeSpeed, task->IsPedPedalling()?"YES":"NO", ((CBike*)m_Vehicle)->GetLeanAngle());
				grcDebugDraw::Text(Vector2(0.25f, 0.05f), Color32(255,255,255), tempString);
			}
		}
	}
}

// ----------------------------------------------------------------
// Add any bicycle related RAG widgets
// ----------------------------------------------------------------
void audBicycleAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Bicycles", false);
		bank.AddToggle("Show Debug Info", &g_ShowBicycleDebugInfo);
	bank.PopGroup();
}
#endif
