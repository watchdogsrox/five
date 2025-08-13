// 
// audio/audTrailerAudioEntity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#include "audioengine/engine.h"
#include "audio/northaudioengine.h"
#include "audiosoundtypes/simplesound.h"
#include "traileraudioentity.h"
#include "vehicles/trailer.h"

AUDIO_VEHICLES_OPTIMISATIONS()

BANK_ONLY(bool g_DebugDrawTrailers = false;)

audCurve audTrailerAudioEntity::sm_TrailerAngleToVolumeCurve;

extern u32 g_SuperDummyRadiusSq;

// ----------------------------------------------------------------
// audTrailerAudioEntity constructor
// ----------------------------------------------------------------
audTrailerAudioEntity::audTrailerAudioEntity() : 
audVehicleAudioEntity()
{
	m_VehicleType = AUD_VEHICLE_TRAILER;
	m_LinkStressSound = NULL;
}

// ----------------------------------------------------------------
// audTrailerAudioEntity reset
// ----------------------------------------------------------------
void audTrailerAudioEntity::Reset()
{
	audVehicleAudioEntity::Reset();
	m_IsParentVehiclePlayer = false;
	m_IsAttached = false;
	m_LinkAngle = 0.0f;
	m_LinkAngleLastFrame = 0.0f;
	m_LinkAngleChangeRate = 0.0f;
	m_LinkAngleChangeRateLastFrame = 0.0f;
	m_LinkAngleChangeRateChangeRate = 0.0f;
	m_LinkAngleChangeRateSmoother.Init(1.0f);
	m_LinkStressVolSmoother.Init(0.1f, 0.1f);
}

// ----------------------------------------------------------------
// Init the trailer class
// ----------------------------------------------------------------
void audTrailerAudioEntity::InitClass()
{
	StaticConditionalWarning(sm_TrailerAngleToVolumeCurve.Init(ATSTRINGHASH("TRAILER_LINKS_ANGLE_CR_TO_VOLUME", 0xB4D01D40)), "Invalid TrailerAngleToVolumeCurve");
}

// ----------------------------------------------------------------
// Get the trailer settings data
// ----------------------------------------------------------------
TrailerAudioSettings* audTrailerAudioEntity::GetTrailerAudioSettings()
{
	TrailerAudioSettings* settings = NULL;

	if(g_AudioEngine.IsAudioEnabled())
	{
		if(m_ForcedGameObject != 0u)
		{
			settings = audNorthAudioEngine::GetObject<TrailerAudioSettings>(m_ForcedGameObject);
			m_ForcedGameObject = 0u;
		}

		if(!settings)
		{
			if(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash() != 0)
			{
				settings = audNorthAudioEngine::GetObject<TrailerAudioSettings>(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash());
			}

			if(!settings)
			{
				settings = audNorthAudioEngine::GetObject<TrailerAudioSettings>(GetVehicleModelNameHash());
			}
		}

		if(!settings)
		{
			// To avoid code-data dependency, default to the old default
			naWarningf("audio: couldn't find trailer settings for trailer: %s", GetVehicleModelName());
			settings = audNorthAudioEngine::GetObject<TrailerAudioSettings>(ATSTRINGHASH("tanker", 0xD46F4737));
		}
	}

	return settings;
}

// ----------------------------------------------------------------
// Initialise the trailer settings
// ----------------------------------------------------------------
bool audTrailerAudioEntity::InitVehicleSpecific()
{
	m_TrailerAudioSettings = GetTrailerAudioSettings();

	if(m_TrailerAudioSettings)
	{
		if(GetVehicleModelNameHash() == ATSTRINGHASH("TRAILERSMALL2", 0x8FD54EBB))
		{
			m_HasMissileLockWarningSystem = true;
		}

		SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());

		m_Vehicle->GetBaseModelInfo()->SetAudioCollisionSettings(audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(m_TrailerAudioSettings->ModelCollisionSettings), m_TrailerAudioSettings->ModelCollisionSettings);
		return true;
	}

	return false;
}

// ----------------------------------------------------------------
// Get the clatter sound
// ----------------------------------------------------------------
audMetadataRef audTrailerAudioEntity::GetClatterSound() const
{
	if(m_TrailerAudioSettings)
	{
		return GetClatterSoundFromType((ClatterType)m_TrailerAudioSettings->ClatterType);
	}

	return audVehicleAudioEntity::GetClatterSound();
}

// ----------------------------------------------------------------
// Get the turret sound set
// ----------------------------------------------------------------
u32 audTrailerAudioEntity::GetTurretSoundSet() const
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("TRAILERSMALL2", 0x8FD54EBB))
	{				
		return ATSTRINGHASH("DLC_Gunrunning_trailersmall2_Turret_Sounds", 0xF6BB60F1);
	}

	return audVehicleAudioEntity::GetTurretSoundSet();
}

// ----------------------------------------------------------------
// Get the desired boat LOD
// ----------------------------------------------------------------
audVehicleLOD audTrailerAudioEntity::GetDesiredLOD(f32 fwdSpeedRatio, u32 distFromListenerSq, bool UNUSED_PARAM(visibleBySniper))
{
	if(m_IsParentVehiclePlayer)
	{
		return AUD_VEHICLE_LOD_REAL;
	}
	else if(!m_IsAttached && fwdSpeedRatio < 0.01f)
	{
		return distFromListenerSq < g_SuperDummyRadiusSq ? AUD_VEHICLE_LOD_SUPER_DUMMY : AUD_VEHICLE_LOD_DISABLED;
	}
	else
	{
		if(distFromListenerSq < (50 * 50))
		{
			return AUD_VEHICLE_LOD_REAL;
		}
		else
		{
			return distFromListenerSq < g_SuperDummyRadiusSq ? AUD_VEHICLE_LOD_SUPER_DUMMY : AUD_VEHICLE_LOD_DISABLED;
		}
	}
}

// ----------------------------------------------------------------
// Update anything trailer specific
// ----------------------------------------------------------------
void audTrailerAudioEntity::UpdateVehicleSpecific(audVehicleVariables& vehicleVariables, audVehicleDSPSettings& UNUSED_PARAM(variables)) 
{
	CTrailer* trailer = (CTrailer*)m_Vehicle;
	CVehicle* attachParent = trailer->GetAttachParentVehicle();
	m_IsParentVehiclePlayer = false;

	if(attachParent)
	{
		audVehicleAudioEntity* parentVehicle = attachParent->GetVehicleAudioEntity();
		m_IsPlayerVehicle |= parentVehicle->IsPlayerVehicle();
		m_IsAttached = true;
	}
	else
	{
		m_IsAttached = false;
	}

	// Trailers bypass regular LOD system since they don't require wave slots/submixes
	SetLOD(GetDesiredLOD(vehicleVariables.fwdSpeedRatio, vehicleVariables.distFromListenerSq, false));

	if(IsReal() && attachParent)
	{
		m_LinkAngle = RtoD * Dot(attachParent->GetTransform().GetForward(), m_Vehicle->GetTransform().GetForward()).Getf();

		if(m_FirstUpdate)
		{
			m_LinkAngleLastFrame = m_LinkAngle;
		}

		m_LinkAngleChangeRate = Abs(m_LinkAngle - m_LinkAngleLastFrame) * fwTimer::GetInvTimeStep();
		m_LinkAngleChangeRateChangeRate = Abs(m_LinkAngleChangeRate - m_LinkAngleChangeRateLastFrame) * fwTimer::GetInvTimeStep();
		m_LinkAngleChangeRateChangeRate *= m_TrailerAudioSettings->LinkStressSensitivityScalar;
		f32 smoothedChangeRate = m_LinkAngleChangeRateSmoother.CalculateValue(m_LinkAngleChangeRate);
		f32 linkVolume = audDriverUtil::ComputeDbVolumeFromLinear(m_LinkStressVolSmoother.CalculateValue(sm_TrailerAngleToVolumeCurve.CalculateValue(m_LinkAngleChangeRateChangeRate)));

		if(!m_LinkStressSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = GetEnvironmentGroup();
			initParams.TrackEntityPosition = true;
			initParams.Volume = linkVolume;
			CreateAndPlaySound_Persistent(m_TrailerAudioSettings->LinkStressSound, &m_LinkStressSound, &initParams);
		}

		if(m_LinkStressSound)
		{
			m_LinkStressSound->FindAndSetVariableValue(ATSTRINGHASH("AngleChangeRate", 0x6952443F), smoothedChangeRate);
			m_LinkStressSound->SetRequestedVolume(linkVolume + (m_TrailerAudioSettings->LinkStressVolumeBoost * 0.01f));
		}

		m_LinkAngleLastFrame = m_LinkAngle;
		m_LinkAngleChangeRateLastFrame = m_LinkAngleChangeRate;
		m_FirstUpdate = false;
	}
	else
	{
		StopAndForgetSounds(m_LinkStressSound);
		m_FirstUpdate = true;
	}

	SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionFullPathDepth());
	
#if __BANK
	if(g_DebugDrawTrailers)
	{
		if(m_IsAttached && m_IsParentVehiclePlayer)
		{
			UpdateDebug();
		}
	}
#endif
}

// ----------------------------------------------------------------
// Trigger a trailer bump sound
// ----------------------------------------------------------------
void audTrailerAudioEntity::TriggerTrailerBump(f32 volume)
{
	if(!IsDisabled())
	{
		naAssertf(m_Vehicle->InheritsFromTrailer(), "Playing a trailer bump on a vehicle that's not a trailer");

		audSoundInitParams damageParams;
		damageParams.EnvironmentGroup = GetEnvironmentGroup();
		damageParams.Volume = volume;
		damageParams.TrackEntityPosition = true;

		if(!m_TrailerAudioSettings)
		{
			m_TrailerAudioSettings = GetTrailerAudioSettings();
		}

		if(naVerifyf(m_TrailerAudioSettings, "Trailer does not have trailer audio settings"))
		{ 
			damageParams.Volume += m_TrailerAudioSettings->TrailerBumpVolumeBoost * 0.01f;
			CreateAndPlaySound(m_TrailerAudioSettings->BumpSound, &damageParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_TrailerAudioSettings->BumpSound, &damageParams, m_Vehicle));
		}
	}
}

// ----------------------------------------------------------------
// Get the clatter sensitivity scalar
// ----------------------------------------------------------------
f32 audTrailerAudioEntity::GetClatterSensitivityScalar() const
{
	if(m_TrailerAudioSettings)
	{
		return m_TrailerAudioSettings->ClatterSensitivityScalar;
	}

	return 1.0f;
}

// ----------------------------------------------------------------
// Get the clatter volume boost
// ----------------------------------------------------------------
f32 audTrailerAudioEntity::GetClatterVolumeBoost() const
{
	if(m_TrailerAudioSettings)
	{
		return m_TrailerAudioSettings->ClatterVolumeBoost * 0.01f;
	}

	return 0.0f;
}

// ----------------------------------------------------------------
// Get chassis stress sensitivity scalar
// ----------------------------------------------------------------
f32 audTrailerAudioEntity::GetChassisStressSensitivityScalar() const
{
	if(m_TrailerAudioSettings)
	{
		return m_TrailerAudioSettings->ChassisStressSensitivityScalar;
	}

	return 1.0f;
}

// ----------------------------------------------------------------
// Get chassis stress volume boost
// ----------------------------------------------------------------
f32 audTrailerAudioEntity::GetChassisStressVolumeBoost() const		
{
	if(m_TrailerAudioSettings)
	{
		return m_TrailerAudioSettings->ChassisStressVolumeBoost * 0.01f;
	}

	return 0.0f;
}

// ----------------------------------------------------------------
// Get the chassis stress sound
// ----------------------------------------------------------------
audMetadataRef audTrailerAudioEntity::GetChassisStressSound() const
{
	if(m_TrailerAudioSettings)
	{
		ClatterType clatterType = (ClatterType)m_TrailerAudioSettings->ClatterType;
		return GetChassisStressSoundFromType(clatterType);
	}

	return g_NullSoundRef;
}


// ----------------------------------------------------------------
// Get a sound from the object data
// ----------------------------------------------------------------
u32 audTrailerAudioEntity::GetSoundFromObjectData(u32 fieldHash) const
{
	if(m_TrailerAudioSettings) 
	{ 
		u32 *ret = (u32 *)m_TrailerAudioSettings->GetFieldPtr(fieldHash);
		return (ret) ? *ret : 0; 
	} 
	return 0;
}

// -------------------------------------------------------------------
// Get model audio collision settings for the trailer if set
// -------------------------------------------------------------------
ModelAudioCollisionSettings* audTrailerAudioEntity::GetModelCollisionSettings()
{
	if(m_TrailerAudioSettings)
	{
		return audNorthAudioEngine::GetObject<ModelAudioCollisionSettings>(m_TrailerAudioSettings->ModelCollisionSettings);
	}
	return NULL;
}


#if __BANK
// ----------------------------------------------------------------
// Update debug stuff
// ----------------------------------------------------------------
void audTrailerAudioEntity::UpdateDebug()
{
	char tempString[128];
	formatf(tempString, "Link Angle: %.02f", m_LinkAngle);
	grcDebugDraw::Text(Vector2(0.1f, 0.11f), Color32(0,0,255), tempString );

	formatf(tempString, "Link Angle Change Rate: %.02f", m_LinkAngleChangeRate);
	grcDebugDraw::Text(Vector2(0.1f, 0.13f), Color32(0,0,255), tempString );

	formatf(tempString, "Link Angle Change Rate (Smoothed): %.02f", m_LinkAngleChangeRateSmoother.GetLastValue());
	grcDebugDraw::Text(Vector2(0.1f, 0.15f), Color32(0,0,255), tempString );

	formatf(tempString, "Link Angle Change Rate Change Rate: %.02f", m_LinkAngleChangeRateChangeRate);
	grcDebugDraw::Text(Vector2(0.1f, 0.17f), Color32(0,0,255), tempString );
}

// ----------------------------------------------------------------
// Add any bicycle related RAG widgets
// ----------------------------------------------------------------
void audTrailerAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Trailers", false);
		bank.AddToggle("Show Debug Info", &g_DebugDrawTrailers);
	bank.PopGroup();
}

#endif
