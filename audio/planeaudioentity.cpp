// 
// audio/audPlaneAudioEntity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 
#include "audioengine/engine.h"
#include "audio/northaudioengine.h"
#include "audiosoundtypes/envelopesound.h"
#include "audiosoundtypes/simplesound.h"
#include "audiohardware/submix.h"
#include "audioeffecttypes/waveshapereffect.h"

#include "Camera/CamInterface.h"
#include "Camera/Cinematic/CinematicDirector.h"

#include "control/replay/Replay.h"

#include "planeaudioentity.h"
#include "scriptaudioentity.h"
#include "Vehicles/Planes.h"
#include "game/weather.h"
#include "speechmanager.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"

u32 g_audPlaneLoopReleaseTime = 300;
f32 g_PlaneVolumeTrims[PLANE_LOOPS_MAX];

const u32 g_DefaultTyreSqueal = ATSTRINGHASH("TYRE_LAND_RANDOM", 0x67A8D590);

extern u32 g_SuperDummyRadiusSq;
extern const u32 g_TemperatureVariableHash;
extern CWeather g_weather;
extern AircraftWarningSettings* g_AircraftWarningSettings;
extern CPlayerSwitchInterface g_PlayerSwitch;
AmbientRadioLeakage g_AircraftPositionedPlayerVehicleRadioLeakage = LEAKAGE_CRAZY_LOUD;

#if __BANK
bool g_ShowDebugInfo = false;
bool g_ShowEngineCooling = false;
bool g_ShowFlyby = false;
bool g_ShowEngineInfo = false;
bool g_ShowEnginePosition = false;
bool g_ShowExhaustPosition = false;
bool g_ShowPropPosition = false;
bool g_PlanesEnabled = true;
bool g_DebugPlaneSuspension = false;
bool g_ForceFlightMode = false;

bool g_UseDebugPlaneTracker = false;
f32 g_DebugTrackerFlybyEngineSpeed = 0.75f;
f32 g_DebugTrackerFlybyTime = 50.0f;
f32 g_DebugTrackerFlybyDist = 2000.0f;
f32 g_DebugTrackerFlybyHeight = 200.0f;
f32 g_DebugTrackerTimer = 0.0f;
f32 g_ForcedFlightModeValue = 0.0f;

float g_OverRideEngineDamage[2] =  {-1.0f, -1.0f};
bool g_ApplyEngineDamage = false;

bool g_ForceStallWarningOn = false;
f32 g_ForcedStallWarningSeverity = 0.0f;

extern f32 g_PositionedPlayerVehicleRadioReverbSmall;
extern f32 g_PositionedPlayerVehicleRadioReverbMedium;
extern f32 g_PositionedPlayerVehicleRadioReverbLarge;
extern bool g_PositionedPlayerVehicleRadioEnabled;
#endif

bank_float g_MinClosingRateForFlyby = 0.5f;
bank_float g_MinClosingRateForFlybyAway = -0.2f;
bank_float g_FlyingAwayDistanceToTrigger = 75.0f;

bank_float g_NpcEngineSpeedForAfterBurner = 0.75f;

bank_float g_MinSpeedForJetFlyby = 20.0f;
bank_float g_MinSpeedForPropFlyby = 10.0f; // some prop planes flying around 15.0f

bank_u32 g_TireSquealAttack = 30;
bank_u32 g_TireSquealDecay = 0;
bank_u32 g_TireSquealSustain = 100;
bank_u32 g_TireSquealHold = 100;
bank_s32 g_TireSquealRelease = 50;

static bank_u32 g_RadioOnDelay = 4000;

#define PEELING_ROLL_LIMIT 90.0f
#define PEELING_PITCH_LIMIT 90.0f
#define PEELING_PITCH_UP_LIMIT 30.0f
#define PEELING_STICK_DEFLECTION 0.9f

audCurve audPlaneAudioEntity::sm_BankingAngleToWindNoiseVolumeCurve;
audCurve audPlaneAudioEntity::sm_BankingAngleToWindNoisePitchCurve;
audCurve audPlaneAudioEntity::sm_SuspensionVolCurve;
audCurve audPlaneAudioEntity::sm_JetClosingToFlybyDistance;
audCurve audPlaneAudioEntity::sm_PropClosingToFlybyDistance;


// ----------------------------------------------------------------
// audPlaneAudioEntity constructor
// ----------------------------------------------------------------
audPlaneAudioEntity::audPlaneAudioEntity() : 
audVehicleAudioEntity()
{
	for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
	{
		m_PlaneLoops[loop] = NULL;
	}

	for(int i=0; i<NUM_VEH_CWHEELS_MAX; i++)
	{
		m_SuspensionSounds[i] = NULL;
		m_LastCompressionChange[i] = 0.0f;
	}

	m_WindSound = NULL;
	m_EngineCoolingSound = NULL;
	m_VehicleType = AUD_VEHICLE_PLANE;
	m_StallWarning = NULL;
	m_LandingGearDeploy = NULL;
	m_LandingGearRetract = NULL;
	m_CropDustingSound = NULL;
	m_NextMissFireSoundTime = 0;
	m_BackFireTime = 0;
	m_FakeEngineSpeed = -10.0f;

	m_LastDownwashTime = 0;
	m_DownwashSound = NULL;
	m_DownwashHash = g_NullSoundHash;
	m_DownwashVolume = -100.0f;
	m_DownwashHeight = -1.0f;

	m_DiveSoundPitch = 0;
	m_DiveSoundVolumeLin = 0.0f;

	m_DiveSound = NULL;
	m_FlybySound = NULL;
	m_IgnitionSound = NULL;
	m_VTOLModeSwitchSound = NULL;
	m_GliderSound = NULL;
	m_ClosingRate = 0.0f;
	m_LastDistanceFromListener = 5000000.0f;
	m_DistanceFromListener = 500000.0f;
	m_SpeechSlotId = -1;
	m_NumClosingSamples = 0;
	m_TimeLastClosingRateUpdated = 0;
	m_ClosingRateAccum = 0.0f;
	m_FlybyStartTime = 0;
	m_FlybyComingAtYou = true;
	m_InARecording = false;
	m_HasPlayedShutdownSound = false;
	m_IsUsingStealthMicrolightGameobject = false;

	m_IgnitionTime = -1;
	
	for(u32 loop = 0; loop < m_LastControlSurfaceRotation.GetMaxCount(); loop++)
	{
		m_LastControlSurfaceRotation[loop] = 0.0f;
	}
	m_ControlSurfaceMovementSound[0] = NULL;
	m_ControlSurfaceMovementSound[1] = NULL;
	m_ControlSurfaceMovementSound[2] = NULL;


	for(u32 loop = 0; loop < PLANE_NUM_PROPELLERS; loop++)
	{
		m_PropellorSpeedMult[loop] = 0.0f;
	}

	m_AirSpeed = 0.0f;

#if __BANK
	m_DebugDivePitchLoops = 0;
	m_DebugVertAirSpeed = 0.0f;
	m_DebugDivingRate = 0.001f;
	m_DebugOcclusion = 0.0f;
#endif

}

audPlaneAudioEntity::~audPlaneAudioEntity() 
{
	StopEngine(true);
	StopAndForgetSounds(m_DownwashSound, m_WindSound, m_EngineCoolingSound, m_FlybySound, m_DiveSound);
	FreeSpeechSlot();
}

void audPlaneAudioEntity::InitClass()
{
	StaticConditionalWarning(sm_BankingAngleToWindNoiseVolumeCurve.Init(ATSTRINGHASH("PLANE_BANKING_ANGLE_TO_WIND_NOISE_VOL", 0xAE5BFDF5)), "Invalid AIR_SPEED_TO_WIND_NOISE_VOLUME_CURVE");
	StaticConditionalWarning(sm_BankingAngleToWindNoisePitchCurve.Init(ATSTRINGHASH("PLANE_BANKING_ANGLE_TO_WIND_NOISE_PITCH", 0x5E7EB1A7)), "Invalid PLANE_AIR_SPEED_TO_WIND_NOISE_PITCH");
	StaticConditionalWarning(sm_SuspensionVolCurve.Init(ATSTRINGHASH("HELI_SUSPENSION_COMPRESSION_TO_VOL",0xD9991BE)), "Invalid SuspVolCurve");
	StaticConditionalWarning(sm_JetClosingToFlybyDistance.Init(ATSTRINGHASH("JETS_CLOSING_RATE_TO_FLYBY_DISTANCE", 0x7EA21C78)), "Invalid JETS_CLOSING_RATE_TO_FLYBY_DISTANCE Curve");
	StaticConditionalWarning(sm_PropClosingToFlybyDistance.Init(ATSTRINGHASH("PROP_CLOSING_RATE_TO_FLYBY_DISTANCE", 0x8857DE09)), "Invalid PROP_CLOSING_RATE_TO_FLYBY_DISTANCE Curve");
}

// ----------------------------------------------------------------
// Get the plane settings data
// ----------------------------------------------------------------
PlaneAudioSettings* audPlaneAudioEntity::GetPlaneAudioSettings()
{
	PlaneAudioSettings* settings = NULL;

	if(g_AudioEngine.IsAudioEnabled())
	{
		if(m_ForcedGameObject != 0u)
		{
			settings = audNorthAudioEngine::GetObject<PlaneAudioSettings>(m_ForcedGameObject);
			m_ForcedGameObject = 0u;
		}

		if(!settings)
		{
			// Special case GO for microlight stealth propellor mod
			if(GetVehicleModelNameHash() == ATSTRINGHASH("MICROLIGHT", 0x96E24857))
			{
				const u8 modIndex = m_Vehicle->GetVariationInstance().GetModIndex(VMT_EXHAUST);

				if(modIndex == 2)
				{
					settings = audNorthAudioEngine::GetObject<PlaneAudioSettings>(ATSTRINGHASH("MICROLIGHT_STEALTHY", 0x34D5AB02));
					m_IsUsingStealthMicrolightGameobject = true;
				}
				else
				{
					m_IsUsingStealthMicrolightGameobject = false;
				}
			}

			if(!settings && m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash() != 0)
			{
				settings = audNorthAudioEngine::GetObject<PlaneAudioSettings>(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash());
			}

			if(!settings)
			{
				settings = audNorthAudioEngine::GetObject<PlaneAudioSettings>(GetVehicleModelNameHash());
			}
		}

		if(!audVerifyf(settings, "Couldn't find plane settings for plane: %s", GetVehicleModelName()))
		{
			CPlane* plane = static_cast<CPlane*>(m_Vehicle);

			if(plane)
			{
				if(m_IsJet)
				{
					settings = audNorthAudioEngine::GetObject<PlaneAudioSettings>(ATSTRINGHASH("DEFAULT_JET", 0xA7351714));
				}
				else
				{
					settings = audNorthAudioEngine::GetObject<PlaneAudioSettings>(ATSTRINGHASH("DEFAULT_PROP_PLANE", 0xE924FA7));
				}
			}
		}
	}

	return settings;
}

// ----------------------------------------------------------------
// Get the vehicle collision settings
// ----------------------------------------------------------------
VehicleCollisionSettings* audPlaneAudioEntity::GetVehicleCollisionSettings() const
{
	if(m_PlaneAudioSettings)
	{
		return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(m_PlaneAudioSettings->VehicleCollisions);
	}

	return audVehicleAudioEntity::GetVehicleCollisionSettings();
}

// ----------------------------------------------------------------
// Initialise the plane settings
// ----------------------------------------------------------------
void audPlaneAudioEntity::Reset()
{
	audVehicleAudioEntity::Reset();

	for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
	{
		m_PlaneLoopHashes[loop] = g_NullSoundHash;
		g_PlaneVolumeTrims[loop] = 0.0f;
		m_PlaneLoopTypes[loop] = AUD_VEHICLE_SOUND_UNKNOWN;
	}

	m_PlaneLoopTypes[PLANE_LOOP_ENGINE] = AUD_VEHICLE_SOUND_ENGINE;
	m_PlaneLoopTypes[PLANE_LOOP_EXHAUST] = AUD_VEHICLE_SOUND_EXHAUST;

	m_PlaneAudioSettings = NULL;
	m_PlaneStatus = AUD_PLANE_OFF;
	m_TimeInAir = 0.0f;
	m_StallWarningOn = false;
	m_StallWarningEnabled = true;
	m_IsPeeling = false;
	m_IsJet = false;
	m_OnGround = false;
	m_EngineSpeedSmoother.Reset();

	for(s32 i = 0; i < MAX_PLANE_WHEELS; i++)
	{
		m_WheelInAir[i] = false;
	}
}

// ----------------------------------------------------------------
// Initialise the plane settings
// ----------------------------------------------------------------
bool audPlaneAudioEntity::InitVehicleSpecific()
{
#if __BANK
	if(!g_PlanesEnabled)
		return false;
#endif

	for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
	{
		m_PlaneLoopHashes[loop] = g_NullSoundHash;
		g_PlaneVolumeTrims[loop] = 0.0f;
	}

	m_PropellorHealthFactorLastFrame = 0.0f;
	m_PeelingFactor = 0.0f;
	m_BankingAngle = 0.0f;
	m_EngineSpeed = 0.0f;
	m_FakeEngineSpeed = -10.0f;
	m_PlaneAudioSettings = NULL;
	m_StallWarning = NULL;
	m_LandingGearDeploy = NULL;
	m_LandingGearRetract = NULL;
	m_CropDustingSound = NULL;
	m_PlaneStatus = AUD_PLANE_OFF;
	m_TimeInAir = 0.0f;
	m_StallWarningOn = false;
	m_IsPeeling = false;
	m_IsJet = false;
	m_OnGround = false;
	m_EngineInstantOnOff = false;
	m_EngineOffTemperature = 100.0f;
	m_ClosingRate = 0.0f;
	m_LastDistanceFromListener = 5000000.0f;
	m_TimeStallStarted = 0;
	
	for(s32 i = 0; i < MAX_PLANE_WHEELS; i++)
	{
		m_WheelInAir[i] = false;
	}

	m_PlaneAudioSettings = GetPlaneAudioSettings();

	if(m_PlaneAudioSettings)
	{
		m_PlaneLoopHashes[PLANE_LOOP_ENGINE] = m_PlaneAudioSettings->EngineLoop;
		m_PlaneLoopHashes[PLANE_LOOP_EXHAUST] = m_PlaneAudioSettings->ExhaustLoop;
		m_PlaneLoopHashes[PLANE_LOOP_PROPELLOR] = m_PlaneAudioSettings->PropellorLoop;
		m_PlaneLoopHashes[PLANE_LOOP_IDLE] = m_PlaneAudioSettings->IdleLoop;
		m_PlaneLoopHashes[PLANE_LOOP_DISTANCE] = m_PlaneAudioSettings->DistanceLoop;
		m_PlaneLoopHashes[PLANE_LOOP_BANKING] = m_PlaneAudioSettings->BankingLoop;
		m_PlaneLoopHashes[PLANE_LOOP_AFTERBURNER] = m_PlaneAudioSettings->AfterburnerLoop;
		m_PlaneLoopHashes[PLANE_LOOP_DAMAGE] = m_PlaneAudioSettings->JetDamageLoop;

		SetUpVolumeCurves();

		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_ENGINE].Init(m_PlaneAudioSettings->EngineThrottleVolumeCurve), "Invalid EngineThrottleVolumeCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_ENGINE].Init(m_PlaneAudioSettings->EngineThrottlePitchCurve), "Invalid EngineThrottlePitchCurve");
		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_EXHAUST].Init(m_PlaneAudioSettings->ExhaustThrottleVolumeCurve), "Invalid ExhaustThrottleVolumeCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_EXHAUST].Init(m_PlaneAudioSettings->ExhaustThrottlePitchCurve), "Invalid ExhaustThrottlePitchCurve");
		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_PROPELLOR].Init(m_PlaneAudioSettings->PropellorThrottleVolumeCurve), "Invalid PropellorThrottleVolumeCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_PROPELLOR].Init(m_PlaneAudioSettings->PropellorThrottlePitchCurve), "Invalid PropellorThrottlePitchCurve");
		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_IDLE].Init(m_PlaneAudioSettings->IdleThrottleVolumeCurve), "Invalid IdleThrottleVolumeCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_IDLE].Init(m_PlaneAudioSettings->IdleThrottlePitchCurve), "Invalid IdleThrottlePitchCurve");
		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_DISTANCE].Init(m_PlaneAudioSettings->DistanceThrottleVolumeCurve), "Invalid DistanceThrottleVolumeCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_DISTANCE].Init(m_PlaneAudioSettings->DistanceThrottlePitchCurve), "Invalid DistanceThrottlePitchCurve");
		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_BANKING].Init(m_PlaneAudioSettings->BankingThrottleVolumeCurve), "Invalid BankingThrottleVolumeCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_BANKING].Init(m_PlaneAudioSettings->BankingThrottlePitchCurve), "Invalid BankingThrottlePitchCurve");
		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_AFTERBURNER].Init(m_PlaneAudioSettings->AfterburnerThrottleVolCurve), "Invalid AfterburnerThrottleVolCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_AFTERBURNER].Init(m_PlaneAudioSettings->AfterburnerThrottlePitchCurve), "Invalid AfterburnerThrottlePitchCurve");
		ConditionalWarning(m_PlaneLoopVolumeCurves[PLANE_LOOP_DAMAGE].Init(m_PlaneAudioSettings->DamageEngineSpeedVolumeCurve), "Invalid DamageEngineSpeedVolumeCurve");
		ConditionalWarning(m_PlaneLoopPitchCurves[PLANE_LOOP_DAMAGE].Init(m_PlaneAudioSettings->DamageEngineSpeedPitchCurve), "Invalid DamageEngineSpeedPitchCurve");
		ConditionalWarning(m_BankingAngleVolCurve.Init(m_PlaneAudioSettings->BankingAngleVolCurve), "Invalid BankingAngleVolCurve");
		ConditionalWarning(m_PeelingPitchCurve.Init(m_PlaneAudioSettings->PeelingPitchCurve), "Invalid PeelingPitchCurve");

		// Distance just has the one volume curve
		ConditionalWarning(m_DistanceEffectVolCurve.Init(m_PlaneAudioSettings->DistanceVolumeCurve), "Invalid DistanceVolumeCurve");
		ConditionalWarning(m_DamageVolumeCurve.Init(m_PlaneAudioSettings->DamageHealthVolumeCurve), "Invalid PLANE_JET_DAMAGE_VOL Curve");
		m_VehicleSpeedToHullSlapVol.Init(m_PlaneAudioSettings->IdleHullSlapSpeedToVol);
		
		SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());

		if(AUD_GET_TRISTATE_VALUE(m_PlaneAudioSettings->Flags, FLAG_ID_PLANEAUDIOSETTINGS_HASMISSILELOCKSYSTEM) == AUD_TRISTATE_TRUE)
		{
			m_HasMissileLockWarningSystem = true;
		}

		CPlane* plane = static_cast<CPlane*>(m_Vehicle);
		if((plane && plane->GetNumPropellors() == 0) || (AUD_GET_TRISTATE_VALUE(m_PlaneAudioSettings->Flags, FLAG_ID_PLANEAUDIOSETTINGS_ISJET) == AUD_TRISTATE_TRUE))
		{
			m_IsJet = true;
		}

		if(GetVehicleModelNameHash() == ATSTRINGHASH("STARLING", 0x9A9EB7DE))
		{
			m_IsJet = true;
			m_EngineInstantOnOff = true;
		}

		if(plane)
		{
			Vector3 planePosition = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
			Vector3 planeToListener = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(0)) - planePosition;
			m_DistanceFromListener = planeToListener.Mag();
			m_LastDistanceFromListener = m_DistanceFromListener;
		}

		m_angularVelocitySmoother.Init(0.1f);
		
		float divingRate = (float)m_PlaneAudioSettings->DivingFactor;
		if(divingRate > 100.0f)
			divingRate = 1.0f;
		static bank_float pullupRateMult = 2.5f;
		m_angleBasedPitchLoopSmoother.Init(divingRate, divingRate*pullupRateMult, 0.0f, (float)m_PlaneAudioSettings->MaxDivePitch);

		static bank_float volIncRate = 0.01f;
		m_angleBasedVolumeSmoother.Init(volIncRate, volIncRate*pullupRateMult, 0.0f, 1.0f);
		m_angleBasedPitchSmoother.Init(divingRate, divingRate*pullupRateMult, 0.0f, (float)m_PlaneAudioSettings->MaxDivePitch);

#if __BANK
		m_DebugDivingRate = divingRate;
#endif
		m_peelingPitchSmoother.Init(0.001f, true);
		m_EngineSpeedSmoother.Init(m_PlaneAudioSettings->NPCEngineSmoothAmount, true);
		static bank_float upRate = 0.75f;
		static bank_float downRate = 0.25f;
		m_DistantSoundVolumeSmoother.Init(upRate, downRate, -100.0f, 0.0f);
		static bank_float dopplerRate = 0.01f;
		m_DopplerSmoother.Init(dopplerRate, true);
	

#if NA_RADIO_ENABLED
		m_AmbientRadioDisabled = (AUD_GET_TRISTATE_VALUE(m_PlaneAudioSettings->Flags, FLAG_ID_PLANEAUDIOSETTINGS_DISABLEAMBIENTRADIO) == AUD_TRISTATE_TRUE);
		m_RadioType = (RadioType)m_PlaneAudioSettings->RadioType;
		m_RadioGenre = (RadioGenre)m_PlaneAudioSettings->RadioGenre;
#endif

		// GTAV specific fix for new DLC VTOL aircraft
		const u32 modelNameHash = GetVehicleModelNameHash();
		
		if(modelNameHash == ATSTRINGHASH("HYDRA", 0x39D6E83F))
		{
			m_VTOLThrusterSoundset.Init(ATSTRINGHASH("HYDRA_THRUSTERS_SOUNDSET", 0x6B68804C));
		}
		else if(modelNameHash == ATSTRINGHASH("TULA", 0x3E2E4F8A))
		{
			m_VTOLThrusterSoundset.Init(ATSTRINGHASH("DLC_Smuggler_Tula_Sounds", 0xA569570B));
		}
		else if(modelNameHash == ATSTRINGHASH("AVENGER", 0x81BD2ED0))
		{
			m_VTOLThrusterSoundset.Init(ATSTRINGHASH("DLC_Christmas2017_Avenger_Sounds", 0x2FDD8496));
		}

		m_KersSystem.Init(this);

		return true;
	}

	return false;
}

void audPlaneAudioEntity::PostUpdate()
{
	audVehicleAudioEntity::PostUpdate();

#if __BANK
	if(IsReal() || m_PlaneStatus == AUD_PLANE_STOPPING)
	{
		ShowDebugInfo(m_DebugPlaneValues);
	}
#endif

	if(GetVehicleModelNameHash() == ATSTRINGHASH("MICROLIGHT", 0x96E24857))
	{
		const u8 modIndex = m_Vehicle->GetVariationInstance().GetModIndex(VMT_EXHAUST);

		if((modIndex == 2 && !m_IsUsingStealthMicrolightGameobject) || (modIndex != 2 && m_IsUsingStealthMicrolightGameobject))
		{
			m_ForcedGameObjectResetRequired = true;
			m_IsUsingStealthMicrolightGameobject = modIndex == 2;
		}		
	}
}

// ----------------------------------------------------------------
// Acquire a wave slot
// ----------------------------------------------------------------
bool audPlaneAudioEntity::AcquireWaveSlot()
{
	if(m_IsFocusVehicle && GetVehicleModelNameHash() == ATSTRINGHASH("STARLING", 0x9A9EB7DE))
	{
		m_RequiresSFXBank = true;
		RequestSFXWaveSlot(ATSTRINGHASH("DLC_SMUGGLER_STARLING_ROCKET_THRUST_BANK", 0x122AB4CB), true);
	}
	else if(m_IsFocusVehicle && m_KersSystem.GetKersSoundSetName() != 0u)
	{
		m_RequiresSFXBank = true;
		RequestSFXWaveSlot(ATSTRINGHASH("DLC_SMUGGLER_PLAYER_JATO_ROCKET_THRUST", 0x70BA015), true);
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("AVENGER", 0x81BD2ED0))
	{
		m_RequiresSFXBank = true;
		RequestSFXWaveSlot(m_PlaneAudioSettings->PropellorLoop, false);
	}
	else
	{
		m_RequiresSFXBank = false;
	}
	
	if(m_RequiresSFXBank && !m_SFXWaveSlot)
	{
		return false;
	}

	if(m_PlaneAudioSettings->SimpleSoundForLoading != g_NullSoundHash)
	{
		RequestWaveSlot(&audVehicleAudioEntity::sm_StandardWaveSlotManager, m_PlaneAudioSettings->SimpleSoundForLoading);
	}
	else
	{
		RequestWaveSlot(&audVehicleAudioEntity::sm_StandardWaveSlotManager, m_PlaneAudioSettings->BankingLoop);
	}

	if(m_RequiresSFXBank)
	{
		// If we didn't manage to acquire a primary wave slot, release the SFX slot so we don't hog it
		if(m_SFXWaveSlot && !m_EngineWaveSlot)
		{
			audWaveSlotManager::FreeWaveSlot(m_SFXWaveSlot, this);
		}

		return m_SFXWaveSlot != NULL && m_EngineWaveSlot != NULL;
	}
	else
	{
		return m_EngineWaveSlot != NULL;
	}
}

// ----------------------------------------------------------------
// OnFocusVehicleChanged
// ----------------------------------------------------------------
void audPlaneAudioEntity::OnFocusVehicleChanged()
{
	if(m_SFXWaveSlot && !m_IsFocusVehicle)
	{
		audWaveSlotManager::FreeWaveSlot(m_SFXWaveSlot, this);
	}
	else if(m_IsFocusVehicle && m_EngineWaveSlot && !m_SFXWaveSlot)
	{
		if(GetVehicleModelNameHash() == ATSTRINGHASH("STARLING", 0x9A9EB7DE))
		{
			RequestSFXWaveSlot(ATSTRINGHASH("DLC_SMUGGLER_STARLING_ROCKET_THRUST_BANK", 0x122AB4CB), true);
		}
		else if(m_KersSystem.GetKersSoundSetName() != 0u)
		{
			RequestSFXWaveSlot(ATSTRINGHASH("DLC_SMUGGLER_PLAYER_JATO_ROCKET_THRUST", 0x70BA015), true);
		}		
	}

	StopAndForgetSounds(m_GliderSound);
}

// ----------------------------------------------------------------
// Set up the volume curves
// ----------------------------------------------------------------
void audPlaneAudioEntity::SetUpVolumeCurves()
{
	Vec3V frontConeDir = Vec3V(V_Y_AXIS_WZERO);
	Vec3V rearConeDir = -Vec3V(V_Y_AXIS_WZERO);
	// Vec3V upConeDir = Vec3V(V_Z_AXIS_WZERO);
	// Vec3V downConeDir = -Vec3V(V_Z_AXIS_WZERO);

	m_PlaneLoopVolumeCones[PLANE_LOOP_ENGINE].Init(frontConeDir, m_PlaneAudioSettings->EngineConeAtten/100.f, m_PlaneAudioSettings->EngineConeFrontAngle, m_PlaneAudioSettings->EngineConeRearAngle);
	m_PlaneLoopVolumeCones[PLANE_LOOP_EXHAUST].Init(rearConeDir, m_PlaneAudioSettings->ExhaustConeAtten/100.f, m_PlaneAudioSettings->ExhaustConeFrontAngle, m_PlaneAudioSettings->ExhaustConeRearAngle);
	m_PlaneLoopVolumeCones[PLANE_LOOP_PROPELLOR].Init(frontConeDir, m_PlaneAudioSettings->PropellorConeAtten/100.f, m_PlaneAudioSettings->PropellorConeFrontAngle, m_PlaneAudioSettings->PropellorConeRearAngle);
	m_PlaneLoopVolumeCones[PLANE_LOOP_IDLE].Init(frontConeDir, 0.0f, 0.0f, 0.0f);
	m_PlaneLoopVolumeCones[PLANE_LOOP_DISTANCE].Init(frontConeDir, 0.0f, 0.0f, 0.0f);
	m_PlaneLoopVolumeCones[PLANE_LOOP_BANKING].Init(frontConeDir, 0.0f, 0.0f, 0.0f);
	m_PlaneLoopVolumeCones[PLANE_LOOP_AFTERBURNER].Init(rearConeDir, m_PlaneAudioSettings->ExhaustConeAtten/100.f, m_PlaneAudioSettings->ExhaustConeFrontAngle, m_PlaneAudioSettings->ExhaustConeRearAngle);
	m_PlaneLoopVolumeCones[PLANE_LOOP_DAMAGE].Init(frontConeDir, m_PlaneAudioSettings->EngineConeAtten/100.f, m_PlaneAudioSettings->EngineConeFrontAngle, m_PlaneAudioSettings->EngineConeRearAngle);
}

void audPlaneAudioEntity::SetTracker(audSoundInitParams &initParams) 
{
	initParams.TrackEntityPosition = true;
#if __BANK
	if(g_UseDebugPlaneTracker)
	{
		initParams.Tracker = &m_DebugTracker;
		initParams.TrackEntityPosition = false;
	}
#endif
}

// ----------------------------------------------------------------
// Check if the engine is on
// ----------------------------------------------------------------
bool audPlaneAudioEntity::IsEngineOn() const
{
	CPlane* plane = static_cast<CPlane*>(m_Vehicle);
	static f32 minEngineSpeed = 0.09f;
	bool engineOn = m_Vehicle->m_nVehicleFlags.bEngineOn || (!m_EngineInstantOnOff && plane && plane->GetEngineSpeed() > minEngineSpeed) || (!m_EngineInstantOnOff && !m_IsPlayerVehicle && m_EngineSpeed > minEngineSpeed);

	if(plane && m_PlaneAudioSettings)
	{
		// For planes with the engines attached to the wings, we want to stop the engine sounds as soon as either the wings or the engines fall off
		if(AUD_GET_TRISTATE_VALUE(m_PlaneAudioSettings->Flags, FLAG_ID_PLANEAUDIOSETTINGS_ENGINESATTACHEDTOWINGS) == AUD_TRISTATE_TRUE)
		{	
			if((plane->GetAircraftDamage().HasSectionBrokenOff(plane, CAircraftDamage::ENGINE_L) || plane->GetAircraftDamage().HasSectionBrokenOff(plane, CAircraftDamage::WING_L)) &&
				(plane->GetAircraftDamage().HasSectionBrokenOff(plane, CAircraftDamage::ENGINE_R) || plane->GetAircraftDamage().HasSectionBrokenOff(plane, CAircraftDamage::WING_R)))
			{
				engineOn = false;
			}
		}
		// Other planes have engines either built into the main body of the aircraft or attached to the body by some point other than the wings, so just check the engines themselves
		else
		{
			if(plane->GetAircraftDamage().HasSectionBrokenOff(plane, CAircraftDamage::ENGINE_L) &&
				plane->GetAircraftDamage().HasSectionBrokenOff(plane, CAircraftDamage::ENGINE_R))
			{
				engineOn = false;
			}
		}

		if(m_Vehicle->GetStatus() == STATUS_WRECKED)
		{
			engineOn = false;
		}

//#if __BANK
//		if(g_ShowDebugInfo)
//		{
//			static float size = 1.0f;
//			grcDebugDraw::Sphere(m_Vehicle->GetVehiclePosition(), size, engineOn?Color_green:Color_red);
//			static float offset = -8.0f;
//			grcDebugDraw::Sphere(m_Vehicle->GetVehiclePosition() + Vec3V(0.f,0.f, offset), size, engineOn?Color_green:Color_red);
//		}
//#endif
	}

	return engineOn;
}

// ----------------------------------------------------------------
// audPlaneAudioEntity::GetPropellorHealthFactor
// ----------------------------------------------------------------
f32 audPlaneAudioEntity::GetPropellorHealthFactor() const
{
	CPlane* plane = static_cast<CPlane*>(m_Vehicle);

#if __BANK
	if(plane && g_ApplyEngineDamage)
	{
		if(g_OverRideEngineDamage[0] != -1.0f)
		{
			if(plane->GetNumPropellors() > 1)
				plane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_L, g_OverRideEngineDamage[0]);
			else
				plane->GetVehicleDamage()->SetEngineHealth(g_OverRideEngineDamage[0]);
		}
		if(plane->GetNumPropellors() > 0 && g_OverRideEngineDamage[1] != -1.0f)
		{
			plane->GetAircraftDamage().SetSectionHealth(CAircraftDamage::ENGINE_R, g_OverRideEngineDamage[1]);
		}
		g_ApplyEngineDamage = false;
	}
#endif

	f32 propellorHealthFactor = 1.0f;

	if(plane->GetNumPropellors() > 0 && !m_IsJet)
	{
		propellorHealthFactor = 0.0f;

		for(u32 loop = 0; loop < plane->GetNumPropellors(); loop++)
		{
			if(m_PropellorSpeedMult[loop] > 0.0f)
			{
				propellorHealthFactor += 1.0f;
			}
		}

		propellorHealthFactor /= plane->GetNumPropellors();
	}

	return propellorHealthFactor;
}

void audPlaneAudioEntity::SetPropellorSpeedMult(s32 index, f32 mult)
{
	if(index >= 0 && index < PLANE_NUM_PROPELLERS)
	{
		m_PropellorSpeedMult[index] = mult;
	}
}

void audPlaneAudioEntity::AdjustControlSurface(int partId, f32 newRotation)
{
	if(partId < PLANE_RUDDER || partId > PLANE_AILERON_R)
		return;

	int partIndex = partId - PLANE_RUDDER;
	CPlane* plane = static_cast<CPlane*>(m_Vehicle);
		
	static bank_float surfaceMovementFactor = 0.7f;
	if(fabs(m_LastControlSurfaceRotation[partIndex] - newRotation) > surfaceMovementFactor)
	{
		m_LastControlSurfaceRotation[partIndex] = newRotation;

		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		SetTracker(initParams);

		if(!m_OnGround)
		{
			static bank_float volume = -6.0f;
			initParams.Volume = volume;
		}

		switch(partIndex)
		{
		case 0:
		case 1:
			if(!m_ControlSurfaceMovementSound[0])
			{
				if(!plane->GetAircraftDamage().HasSectionBrokenOff(plane, partIndex == 0? CAircraftDamage::RUDDER : CAircraftDamage::RUDDER_2))
				{
					CreateAndPlaySound_Persistent(m_PlaneAudioSettings->Rudder, &m_ControlSurfaceMovementSound[0], &initParams);
				}
			}
			break;
		case 2:
		case 3:
			if(!m_ControlSurfaceMovementSound[1])
			{
				if(!plane->GetAircraftDamage().HasSectionBrokenOff(plane, partIndex == 2? CAircraftDamage::ELEVATOR_L : CAircraftDamage::ELEVATOR_R))
				{
					CreateAndPlaySound_Persistent(m_PlaneAudioSettings->Elevator, &m_ControlSurfaceMovementSound[1], &initParams);
				}
			}
			break;
		case 4:
		case 5:
			if(!m_ControlSurfaceMovementSound[2])
			{
				if(!plane->GetAircraftDamage().HasSectionBrokenOff(plane, partIndex == 4? CAircraftDamage::AILERON_L : CAircraftDamage::AILERON_R))
				{
					CreateAndPlaySound_Persistent(m_PlaneAudioSettings->Aileron, &m_ControlSurfaceMovementSound[2], &initParams);
				}
			}
			break;
		}
	}
}

bool audPlaneAudioEntity::IsPlayingStartupSequence() const
{
	if(m_IgnitionSound || (m_IgnitionTime > 0 && !m_Vehicle->m_nVehicleFlags.bSkipEngineStartup))
	{
		return true;
	}
	return false;
}

u32 audPlaneAudioEntity::GetTurretSoundSet() const
{	
	return ATSTRINGHASH("Plane_Turret_Sounds", 0xC96E8DD7);	
}

// ----------------------------------------------------------------
// Update anything plane specific
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateVehicleSpecific(audVehicleVariables& UNUSED_PARAM(vehicleVariables), audVehicleDSPSettings& dspSettings)
{
	audPlaneValues planeValues;
	f32 propellorHealthFactor = GetPropellorHealthFactor();

	if(IsReal() || m_PlaneStatus == AUD_PLANE_STOPPING)
	{
		CPlane* plane = static_cast<CPlane*>(m_Vehicle);
		bool engineOn = IsEngineOn();

		m_EngineSpeed = plane->GetEngineSpeed();
		if(!m_IsPlayerVehicle && m_Vehicle->m_nVehicleFlags.bKeepEngineOnWhenAbandoned)
		{
			static bank_float fakeEngineSpeed = 0.5f;
			m_EngineSpeed = fakeEngineSpeed;
		}
		// planes can have some very erratic throttle/engine speeds
		if(m_IsPlayerVehicle || (!m_IsJet && m_PlaneStatus == AUD_PLANE_STOPPING))
		{
			static bank_float playerEngineSmoother = 0.01f;
			m_EngineSpeedSmoother.SetRate(playerEngineSmoother);
		}
		else
		{
			m_EngineSpeedSmoother.SetRate(m_PlaneAudioSettings->NPCEngineSmoothAmount);
		}
#if GTA_REPLAY
		if(!CReplayMgr::IsEditModeActive())
#endif
		{
			m_EngineSpeed = m_EngineSpeedSmoother.CalculateValue(m_EngineSpeed);
		}

#if __BANK
		if(g_UseDebugPlaneTracker)
		{
			m_EngineSpeed = g_DebugTrackerFlybyEngineSpeed;
		}
#endif
		m_BankingAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetC(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
		
		f32 entityVariableThrottle = 0.0f;
		f32 entityVariableRevs = 0.0f;
		f32 fakeEngineFactor = 0.0f;

		if(HasEntityVariableBlock())
		{
			entityVariableThrottle = GetEntityVariableValue(ATSTRINGHASH("fakethrottle", 0xEB27990));
			entityVariableRevs = GetEntityVariableValue(ATSTRINGHASH("fakerevs", 0xCEB98BEB));

			// Reduce this each frame so that that the behavior gets canceled even if a sound forgets to do so
			fakeEngineFactor = GetEntityVariableValue(ATSTRINGHASH("usefakeengine", 0x91DF7F97));
			SetEntityVariable(ATSTRINGHASH("usefakeengine", 0x91DF7F97), Clamp(fakeEngineFactor - 0.1f, 0.0f, 1.0f));
		}

		if(fakeEngineFactor > 0.1f)
		{
			m_EngineSpeed = entityVariableThrottle;
			m_BankingAngle = entityVariableRevs;
			m_InARecording = true;
			m_Vehicle->m_nVehicleFlags.bForceActiveDuringPlayback = true;
		}
		else
		{
			m_InARecording = false;
		}


		// This check is needed to avoid us missing the frame that the engine turns on, and therefore having an incorrect m_WasEngineOnLastFrame
		// flag (causing us to use the wrong attack time, miss the ignition, etc.). Basically, GetPropellorSpeedMult() (used by GetPropellorHealthFactor()) only returns
		// values > 0.0f once the engine speed is > 0.0f, so shouldn't be relied upon until the engine has actually started.
		u32 currentTime = fwTimer::GetTimeInMilliseconds();
		if(engineOn && propellorHealthFactor == 0.0f && m_EngineSpeed > 0.0f)
		{
			if(!plane->m_Transmission.GetCurrentlyMissFiring() && currentTime > (m_BackFireTime + 1000))
				engineOn = false;
		}
		
		planeValues.propellorHealthFactor = propellorHealthFactor;

		switch(m_PlaneStatus)
		{
		case AUD_PLANE_OFF:
			if(engineOn)
			{
				if(m_EngineWaveSlot && m_EngineWaveSlot->IsLoaded())
				{
					StartEngine();

					if(m_PlaneStatus != AUD_PLANE_OFF)
					{
						UpdateEngineOn(planeValues);
					}
				}
				m_DiveSoundVolumeLin = 0.0f;
			}
			break;

		case AUD_PLANE_STARTING:
			m_PlaneStatus = AUD_PLANE_ON;
			if(m_EngineSpeed <= m_FakeEngineSpeed)
			{
				m_EngineSpeed = m_FakeEngineSpeed;
			}
			//fall through

		case AUD_PLANE_ON:
			if(engineOn)
			{
				m_EngineOffTemperature = 100.0f;

				//check if we have a jet shutting down
				if(!m_Vehicle->m_nVehicleFlags.bEngineOn && m_IsJet)
				{
					m_PlaneStatus = AUD_PLANE_STOPPING;
					// get the starting engine speed
					m_FakeEngineSpeed = m_EngineSpeed;
					m_EngineOffTemperature = m_Vehicle->m_EngineTemperature;
				}
				else if(m_EngineSpeed <= m_FakeEngineSpeed)
				{
					m_EngineSpeed = m_FakeEngineSpeed;
					ShutdownJetEngine(); // this keeps adjusting the m_FakeEngineSpeed until they match
				}
				else
				{
					m_FakeEngineSpeed = -10.0f;
				}

				if(!m_IsJet && m_Vehicle->m_nVehicleFlags.bEngineOn && !m_WasEngineOnLastFrame && currentTime > (m_BackFireTime + 2000))
				{
					// engine was off and is restarting, but no restart sound. We get a black puff from engines, so give a backfire sound to match.
					TriggerBackFire();
				}

				UpdateStallWarning(planeValues);
				UpdateEngineOn(planeValues);
				UpdateTricks();
			}
			else
			{
				if(m_IsJet)
				{
					// get the starting engine speed
					m_FakeEngineSpeed = m_EngineSpeed;
				}
				m_PlaneStatus = AUD_PLANE_STOPPING;
				m_EngineOffTemperature = m_Vehicle->m_EngineTemperature;
			}
			break;
		case AUD_PLANE_STOPPING:
			if(m_Vehicle->GetStatus() == STATUS_WRECKED)
			{
				m_DiveSoundVolumeLin = 0.0f;
				if(m_IsJet && m_Vehicle->GetIsInWater())
				{
					ShutdownJetEngine();
					m_EngineSpeed = m_FakeEngineSpeed;
					UpdateEngineOn(planeValues);
				}
				else
				{
					StopEngine();
				}
			}
			else
			{
				if(m_Vehicle->m_nVehicleFlags.bEngineOn)
				{
					if(m_EngineWaveSlot && m_EngineWaveSlot->IsLoaded())
					{
						StartEngine();
					}
				}
				if(m_IsJet)
				{
					ShutdownJetEngine();
					m_EngineSpeed = m_FakeEngineSpeed;
				}
				else
				{
					static bank_float minEngineSpeedToStop = 0.03f;
					if(m_EngineSpeed < minEngineSpeedToStop || m_EngineInstantOnOff)
					{
						StopEngine();
					}

				}
				UpdateEngineOn(planeValues);
			}
			break;
		default:
			break;
		}

		if(m_PlaneStatus != AUD_PLANE_OFF)
		{			
			if(planeValues.haveBeenCalculated)
			{
				UpdateSounds(planeValues);

				f32 throttle = plane->GetThrottle();

				if(!m_IsPlayerVehicle && m_EngineSpeed > g_NpcEngineSpeedForAfterBurner)
				{
					throttle = 1.0f;
				}

#if __BANK
				if(g_UseDebugPlaneTracker)
				{
					throttle = 1.0f;
				}
#endif
				dspSettings.AddDSPParameter(atHashString("Throttle", 0xEA0151DC), throttle);
				dspSettings.AddDSPParameter(atHashString("RPM", 0x5B924509), m_Vehicle->m_Transmission.GetRevRatio());

				if(m_IsJet)
				{
					dspSettings.enginePostSubmixAttenuation = planeValues.postSubmixVolumes[PLANE_LOOP_ENGINE];
				}
				else
				{
					dspSettings.enginePostSubmixAttenuation = planeValues.postSubmixVolumes[PLANE_LOOP_PROPELLOR];
				}

				dspSettings.exhaustPostSubmixAttenuation = planeValues.postSubmixVolumes[PLANE_LOOP_EXHAUST];
			}				
		}

		UpdateSuspension(); // must be done before tyre squeals so we can check m_TimeInAir before it goes 0
		UpdateTyreSqueals();
		UpdateWindNoise(planeValues);
		UpdateControlSurfaces();
		UpdateDiveSound(planeValues);
		UpdateGliding();
		m_KersSystem.Update();

		m_DownwashVolume = planeValues.loopVolumes[PLANE_LOOP_ENGINE];
		if(m_LastDownwashTime + 300 < currentTime && m_DownwashSound)
		{
			m_DownwashSound->StopAndForget();
		}
		
		if(m_IsPlayerVehicle && m_IsPlayerSeatedInVehicle && m_Vehicle->GetStatus() != STATUS_WRECKED)
		{
			if(m_IgnitionTime > 0)
			{
				m_IgnitionTime -= fwTimer::GetTimeStepInMilliseconds();
			}
		}
		else
		{
			if(!m_IsPlayerSeatedInVehicle)
			{
				m_IgnitionTime = g_RadioOnDelay;
			}
		}

#if __BANK		
		UpdateDebug();
		SetUpVolumeCurves();
#endif
	}
	else
	{
		m_IgnitionTime = g_RadioOnDelay;
		StopAndForgetSounds(m_WindSound, m_DownwashSound, m_FlybySound, m_DiveSound, m_GliderSound);
		m_KersSystem.StopAllSounds();
	}
	
	UpdateVTOL();
	UpdateEngineCooling();
	UpdateFlyby();

	if(IsSeaPlane())
	{
		if(IsReal() || IsDummy())
		{
			UpdateSeaPlane();
		}
	}

	if(!m_FlybySound)
	{
		FreeSpeechSlot();
	}
#if __BANK
	if(g_ShowEngineInfo)
	{
		char tempString[128];
		formatf(tempString, "Occ %4.2f  Dop %4.2f  ESpd %4.2f  EON %d  AEON %d", m_DebugOcclusion, m_DopplerSmoother.GetLastValue(), m_EngineSpeed, m_Vehicle->m_nVehicleFlags.bEngineOn, IsEngineOn());
		grcDebugDraw::Text(m_Vehicle->GetTransform().GetPosition(), Color_green, tempString);
	}

	m_DebugPlaneValues = planeValues;
#endif

	m_DownwashHeight = -1.0f;

	m_PropellorHealthFactorLastFrame = propellorHealthFactor;

	SetEnvironmentGroupSettings(true, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());
}

void audPlaneAudioEntity::UpdateControlSurfaces()
{
	if(IsReal())
	{
		CPlane* plane = static_cast<CPlane*>(m_Vehicle);

		AdjustControlSurface(PLANE_RUDDER, plane->GetYaw());
		AdjustControlSurface(PLANE_RUDDER_2, plane->GetYaw());
	
		// Elevators (pitch)
		AdjustControlSurface(PLANE_ELEVATOR_L, plane->GetPitch());
		AdjustControlSurface(PLANE_ELEVATOR_R, plane->GetPitch());

		// Ailerons (roll)
		AdjustControlSurface(PLANE_AILERON_L, plane->GetRoll());
		AdjustControlSurface(PLANE_AILERON_R, plane->GetRoll());
	}
}

// ----------------------------------------------------------------
// Get the desired lod for the vehicle
// ----------------------------------------------------------------
audVehicleLOD audPlaneAudioEntity::GetDesiredLOD(f32 UNUSED_PARAM(fwdSpeedRatio), u32 distFromListenerSq, bool UNUSED_PARAM(visibleBySniper))
{
	if(m_IsFocusVehicle || m_Vehicle->m_nVehicleFlags.bUsedForPilotSchool)
	{
		return AUD_VEHICLE_LOD_REAL;
	}
	else if(RequiresWaveSlot())
	{
		if(m_IsFocusVehicle || (distFromListenerSq < (m_PlaneAudioSettings->IsRealLODRange * sm_ActivationRangeScale)) )
		{
			return AUD_VEHICLE_LOD_REAL;
		}
	}

	return distFromListenerSq < g_SuperDummyRadiusSq ? AUD_VEHICLE_LOD_SUPER_DUMMY : AUD_VEHICLE_LOD_DISABLED;
}

// ----------------------------------------------------------------
// Turn on the engines
// ----------------------------------------------------------------
void audPlaneAudioEntity::StartEngine()
{
	audSoundInitParams initParams;
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	SetTracker(initParams);

	CPlane* plane = static_cast<CPlane*>(m_Vehicle);

	if(plane)
	{
		m_NumClosingSamples = 0;
		m_TimeLastClosingRateUpdated = fwTimer::GetTimeInMilliseconds();
		m_ClosingRateAccum = 0.0f;

		if(m_Vehicle && !m_Vehicle->m_nVehicleFlags.bSkipEngineStartup && !m_WasEngineOnLastFrame && !m_PlaneLoops[PLANE_LOOP_ENGINE])
		{
			if(m_IgnitionSound)
			{
				m_IgnitionSound->StopAndForget();
			}
			CreateAndPlaySound_Persistent(m_PlaneAudioSettings->Ignition, &m_IgnitionSound, &initParams);
		}

		for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
		{
			if(!m_PlaneLoops[loop] && m_PlaneLoopHashes[loop] != g_NullSoundHash &&
				loop != PLANE_LOOP_AFTERBURNER) // Afterburner stops/starts as required, rather than constantly playing
			{
				if((loop == PLANE_LOOP_ENGINE && m_IsJet) ||
				   (loop == PLANE_LOOP_PROPELLOR && !m_IsJet))
				{
					Assign(initParams.EffectRoute, GetEngineEffectRoute());
				}
				else if(loop == PLANE_LOOP_EXHAUST || loop == PLANE_LOOP_AFTERBURNER )
				{
					Assign(initParams.EffectRoute, GetExhaustEffectRoute());
				}
				else
				{
					initParams.EffectRoute = 0;
				}

				// We missed the engine actually starting (probably because we were set to disabled) so fade in
				u32 defaultPlaneAttackTime = 1000;
#if GTA_REPLAY
				// always use a quick fade in for replay player vehicle
				if(CReplayMgr::IsPlaying())
				{
					m_WasScheduledCreation = false;
					if(m_IsPlayerVehicle)
					{
						defaultPlaneAttackTime = 0;
					}
				}
#endif
				if(m_Vehicle->m_nVehicleFlags.bSkipEngineStartup || m_WasScheduledCreation || m_WasEngineOnLastFrame)
				{
					static bank_u32 fadeInTime = 8000;
					initParams.AttackTime = m_WasScheduledCreation ? fadeInTime : defaultPlaneAttackTime;
				}
				else
				{
					initParams.AttackTime = defaultPlaneAttackTime; // default plane attack time, helps us hear startup sound
				}

				CreateAndPlaySound_Persistent(m_PlaneLoopHashes[loop], &m_PlaneLoops[loop], &initParams);

				if(m_PlaneLoops[loop])
				{
					BANK_ONLY(if(!g_UseDebugPlaneTracker))
					{
						audVehicleSounds vehicleSoundType = AUD_VEHICLE_SOUND_ENGINE;

						if(loop == PLANE_LOOP_EXHAUST ||
							loop == PLANE_LOOP_AFTERBURNER)
						{
							vehicleSoundType = AUD_VEHICLE_SOUND_EXHAUST;
						}

						m_PlaneLoops[loop]->SetUpdateEntity(true);
						m_PlaneLoops[loop]->SetClientVariable((u32)vehicleSoundType);
					}
				}
				//else
				//{		
				//	StopEngine();
				//	return;
				//}
			}
		}

		m_PlaneStatus = AUD_PLANE_STARTING;
	}

	m_WasScheduledCreation = false;
}

// -------------------------------------------------------------------------------
// Update gliding
// -------------------------------------------------------------------------------
void audPlaneAudioEntity::UpdateGliding()
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(modelNameHash == ATSTRINGHASH("STARLING", 0x9A9EB7DE))
	{	
		const bool shouldPlayGliderSound = !m_OnGround && !m_Vehicle->IsRocketBoosting();
		
		if(shouldPlayGliderSound)
		{
			if(!m_GliderSound)
			{
				audSoundSet soundset;
				const u32 soundsetName = m_IsFocusVehicle? ATSTRINGHASH("DLC_Smuggler_Starling_Sounds", 0xC99D5122) : ATSTRINGHASH("DLC_Smuggler_Starling_NPC_Sounds", 0x1C2ADE23);
				const u32 soundFieldName = ATSTRINGHASH("glide", 0xF9209D38);

				if(soundset.Init(soundsetName))
				{
					audSoundInitParams initParams;
					initParams.TrackEntityPosition = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					CreateAndPlaySound_Persistent(soundset.Find(soundFieldName), &m_GliderSound, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(soundsetName, soundFieldName, &initParams, m_GliderSound, GetOwningEntity()));
				}
			}
		}
		else
		{
			if(m_GliderSound)
			{
				m_GliderSound->StopAndForget(true);
			}
		}
	}
}

// ----------------------------------------------------------------
// Update the plane sounds
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateSounds(audPlaneValues& planeValues)
{
	CPlane* plane = static_cast<CPlane*>(m_Vehicle);

	u32 currentTime = fwTimer::GetTimeInMilliseconds();

	if(plane && m_PlaneAudioSettings)
	{
		bool playerInInterior = audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior();
		if(!m_IsPlayerVehicle && playerInInterior)
		{
			f32 exteriorOcclusionFactor = audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionFor3DPosition(VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()));
			static bank_float maxOcclusion = 0.25f;
			exteriorOcclusionFactor = Clamp(exteriorOcclusionFactor, 0.0f, maxOcclusion);
			BANK_ONLY(m_DebugOcclusion = exteriorOcclusionFactor;)
			if(m_EnvironmentGroup)
			{
				m_EnvironmentGroup->SetExtraOcclusionValue(exteriorOcclusionFactor);
			}
		}
		else
		{
			if(m_EnvironmentGroup)
			{
				m_EnvironmentGroup->SetExtraOcclusionValue(0.0f);
			}
			BANK_ONLY(m_DebugOcclusion = 0.0f;)
		}

		if(m_IsJet)
		{
			static bank_float afterburnerStartThrottle = 0.0f;
			float throttle = plane->GetThrottle();

			if(!m_IsPlayerVehicle && m_EngineSpeed > g_NpcEngineSpeedForAfterBurner)
			{
				throttle = 1.0f;
			}
			if(throttle > afterburnerStartThrottle && m_PlaneStatus != AUD_PLANE_STOPPING && m_PlaneStatus != AUD_PLANE_OFF && !m_Vehicle->m_nVehicleFlags.bIsDrowning    BANK_ONLY( || g_UseDebugPlaneTracker) )
			{
				if(!m_PlaneLoops[PLANE_LOOP_AFTERBURNER] && m_EngineWaveSlot)
				{
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					Assign(initParams.EffectRoute, GetExhaustEffectRoute());
#if __BANK
					if(g_UseDebugPlaneTracker)
					{
						initParams.EffectRoute = 0;
					}
#endif
					CreateSound_PersistentReference(m_PlaneLoopHashes[PLANE_LOOP_AFTERBURNER], &m_PlaneLoops[PLANE_LOOP_AFTERBURNER], &initParams);

					if(m_PlaneLoops[PLANE_LOOP_AFTERBURNER])
					{
						m_PlaneLoops[PLANE_LOOP_AFTERBURNER]->SetUpdateEntity(true);
						m_PlaneLoops[PLANE_LOOP_AFTERBURNER]->SetClientVariable((u32)AUD_VEHICLE_SOUND_EXHAUST);
						m_PlaneLoops[PLANE_LOOP_AFTERBURNER]->PrepareAndPlay(m_EngineWaveSlot->waveSlot,false,15000);
					}
				}
			}
			else if(m_PlaneLoops[PLANE_LOOP_AFTERBURNER])
			{
				m_PlaneLoops[PLANE_LOOP_AFTERBURNER]->Stop();
			}
		}
	}

	static bank_float distanceForDoppler = 200.0f;

    // Fix for Distant plane sounds pitching up after being deleted from the world. Speed goes to 0 and thus the pitch goes up(when the plane is flying away from you).
    // Doppler now smooths back to 1 as the plane approaches.   
	f32 doppler = m_DopplerSmoother.CalculateValue(m_DistanceFromListener > distanceForDoppler ? 0.0f : 1.0f);
	if(fwTimer::IsGamePaused() || (m_IsPlayerVehicle && !camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera()))
		doppler = 0.0f;
	for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
	{
		if(m_PlaneLoops[loop])
		{
			m_PlaneLoops[loop]->SetRequestedVolume(planeValues.loopVolumes[loop]);
			m_PlaneLoops[loop]->SetRequestedPitch(planeValues.loopPitches[loop]);
			m_PlaneLoops[loop]->SetRequestedPostSubmixVolumeAttenuation(planeValues.postSubmixVolumes[loop]);
			m_PlaneLoops[loop]->SetRequestedDopplerFactor(doppler);

			if(NetworkInterface::IsGameInProgress())
			{
				if(loop == PLANE_LOOP_ENGINE || loop == PLANE_LOOP_EXHAUST || loop == PLANE_LOOP_PROPELLOR || loop == PLANE_LOOP_AFTERBURNER)
				{
					m_PlaneLoops[loop]->FindAndSetVariableValue(ATSTRINGHASH("BoostAmount", 0x5EADB832), m_Vehicle->GetBoostAmount());
					m_PlaneLoops[loop]->FindAndSetVariableValue(ATSTRINGHASH("BoostTimer", 0x4925EC4E), m_Vehicle->GetSpeedUpBoostDuration());
				}
			}
		}
	}

	bool stallWarningOn = m_StallWarningOn && m_StallWarningEnabled && AreWarningSoundsValid();

#if __BANK
	if(g_ForceStallWarningOn)
	{
		stallWarningOn = true;
	}
#endif

	if(stallWarningOn)
	{
		if(!m_StallWarning)
		{
			CreateSound_PersistentReference(m_PlaneAudioSettings->StallWarning, &m_StallWarning);

			if(m_StallWarning)
			{
				m_StallWarning->PrepareAndPlay();
			}
		}
	}
	else
	{
		StopAndForgetSounds(m_StallWarning);
	}

	if(m_StallWarning)
	{
		Vector3 velocity = m_CachedVehicleVelocity;

		if(velocity.z < 0.0f)
		{
			velocity.z = 0.0f;
		}

		f32 airSpeed = velocity.Mag();
		f32 stallSeverity = Clamp(1.0f - (airSpeed/25.0f), 0.0f, 1.0f);

#if __BANK
		if(g_ForceStallWarningOn)
		{
			stallSeverity = g_ForcedStallWarningSeverity;
		}
#endif
		m_StallWarning->FindAndSetVariableValue(ATSTRINGHASH("StallSeverity", 0x573A0501), stallSeverity);
	}

	if( plane->m_Transmission.GetCurrentlyMissFiring() && currentTime > (m_BackFireTime + 2000))
	{
		TriggerBackFire();
	}

	// prop plane miss fire
	if(!m_IsJet)
	{
		if(plane->m_Transmission.GetCurrentlyRecoveringFromMissFiring())
		{
			f32 engineSpeed = Clamp(m_EngineSpeed, 0.0f, 1.0f);
			s32 predelay = -1;
			f32 volume = -6.0f;

			static bank_float missFireBelowEngineSpeed = 0.4f;
			if(engineSpeed < missFireBelowEngineSpeed && engineSpeed > 0.0f)
			{
				if(m_NextMissFireSoundTime > (currentTime + 1000))
				{
					m_NextMissFireSoundTime = currentTime;
				}
				if(currentTime > m_NextMissFireSoundTime)
				{
					static bank_s32 divFactor = 1;
					predelay = (s32)((missFireBelowEngineSpeed - engineSpeed) * 1000.0f)/divFactor;
					predelay = Clamp(predelay, 0, 1000);

					static bank_float volFactor = 0.6f;
					f32 linearVolume = Clamp(volFactor - engineSpeed, 0.0f, 1.0f);
					volume = audDriverUtil::ComputeDbVolumeFromLinear(linearVolume);
				}
			}
			else if(engineSpeed >= missFireBelowEngineSpeed)
			{
				if(currentTime > m_NextMissFireSoundTime && currentTime > (m_BackFireTime + 2000))
				{
					f32 health = m_Vehicle->GetVehicleDamage()->GetEngineHealth();
					s32 multFactor = (s32)(health / 100.0f);
					s32 maxRange = (s32)((engineSpeed) * 1000.0f)*multFactor;
					predelay = audEngineUtil::GetRandomNumberInRange(maxRange/2, maxRange);
					
					if(audEngineUtil::GetRandomFloat() > 0.75f)
					{
						TriggerBackFire();
					}
				}
			}

			if(predelay != -1)
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.Volume = volume;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				CreateAndPlaySound(m_PlaneAudioSettings->EngineMissFire, &initParams);
				m_NextMissFireSoundTime = fwTimer::GetTimeInMilliseconds() + predelay;
			}
		}
	}
}

// ----------------------------------------------------------------
// Calculate if the plane is in the water
// ----------------------------------------------------------------
bool audPlaneAudioEntity::CalculateIsInWater() const
{
	if(IsSeaPlane())
	{
		return m_Vehicle->GetIsInWater();
	}

	return audVehicleAudioEntity::CalculateIsInWater();
}

// ----------------------------------------------------------------
// Get the idle hull slap volume
// ----------------------------------------------------------------
f32 audPlaneAudioEntity::GetIdleHullSlapVolumeLinear(f32 speed) const
{
	return m_VehicleSpeedToHullSlapVol.CalculateValue(speed);
}

// ----------------------------------------------------------------
// audPlaneAudioEntity GetIdleHullSlapSound
// ----------------------------------------------------------------
u32 audPlaneAudioEntity::GetIdleHullSlapSound() const
{
	if(m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->IdleHullSlapLoop;
	}

	return audVehicleAudioEntity::GetIdleHullSlapSound();
}

// ----------------------------------------------------------------
// audPlaneAudioEntity GetLeftWaterSound
// ----------------------------------------------------------------
u32 audPlaneAudioEntity::GetLeftWaterSound() const
{
	if(m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->LeftWaterSound;
	}

	return audVehicleAudioEntity::GetLeftWaterSound();
}

// ----------------------------------------------------------------
// audPlaneAudioEntity GetWaveHitSound
// ----------------------------------------------------------------
u32 audPlaneAudioEntity::GetWaveHitSound() const
{
	if(m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->WaveHitSound;
	}

	return audVehicleAudioEntity::GetWaveHitSound();
}

// ----------------------------------------------------------------
// audPlaneAudioEntity GetWaveHitBigSound
// ----------------------------------------------------------------
u32 audPlaneAudioEntity::GetWaveHitBigSound() const
{
	if(m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->WaveHitBigAirSound;
	}

	return audVehicleAudioEntity::GetWaveHitBigSound();
}

#if __BANK
void audPlaneAudioEntity::ShowDebugInfo(audPlaneValues& planeValues)
{
#define NEXT_LINE	grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString ); lineBase += lineInc;

	CPlane* plane = static_cast<CPlane*>(m_Vehicle);

	if(g_ShowDebugInfo && (m_IsPlayerVehicle || plane == g_pFocusEntity))
	{
		char tempString[2048];
		static bank_float lineInc = 0.015f;
		f32 lineBase = 0.1f;
		formatf(tempString, "        VOLUME PITCH SUBMIX_VOL"); NEXT_LINE		
			formatf(tempString, "ENGINE   %8.4f %5d %8.4f", planeValues.loopVolumes[0], planeValues.loopPitches[0], planeValues.postSubmixVolumes[0]);  NEXT_LINE
			formatf(tempString, "EXHAUST  %8.4f %5d %8.4f", planeValues.loopVolumes[1], planeValues.loopPitches[1], planeValues.postSubmixVolumes[1]);  NEXT_LINE
			formatf(tempString, "IDLE     %8.4f %5d %8.4f", planeValues.loopVolumes[2], planeValues.loopPitches[2], planeValues.postSubmixVolumes[2]);  NEXT_LINE
			formatf(tempString, "DISTANT  %8.4f %5d %8.4f", planeValues.loopVolumes[3], planeValues.loopPitches[3], planeValues.postSubmixVolumes[3]);  NEXT_LINE
			formatf(tempString, "PROP     %8.4f %5d %8.4f", planeValues.loopVolumes[4], planeValues.loopPitches[4], planeValues.postSubmixVolumes[4]);  NEXT_LINE
			formatf(tempString, "BANKING  %8.4f %5d %8.4f", planeValues.loopVolumes[5], planeValues.loopPitches[5], planeValues.postSubmixVolumes[5]);  NEXT_LINE
			formatf(tempString, "AFTER    %8.4f %5d %8.4f", planeValues.loopVolumes[6], planeValues.loopPitches[6], planeValues.postSubmixVolumes[6]);  NEXT_LINE
			formatf(tempString, "DAM      %8.4f %5d %8.4f", planeValues.loopVolumes[7], planeValues.loopPitches[7], planeValues.postSubmixVolumes[7]);  NEXT_LINE
			formatf(tempString, "DIVING L %8.4f %5d %8.4f", 0.0f, m_DebugDivePitchLoops, 0.0f);  NEXT_LINE
			formatf(tempString, "DIVING S %8.4f %5d %8.4f %8.4f", audDriverUtil::ComputeDbVolumeFromLinear(m_DiveSoundVolumeLin) + planeValues.loopVolumes[PLANE_LOOP_ENGINE], m_DiveSoundPitch, 0.0f, m_DiveSoundVolumeLin);  NEXT_LINE
			formatf(tempString, "VERT_SPD %8.4f     SPEED %8.4f      CLOSING %8.4f", m_DebugVertAirSpeed, m_AirSpeed, m_ClosingRate);  NEXT_LINE
			formatf(tempString, "NUM_PROPS  %d     IS_JET %d     ON_GRND %d     CONTACT %d", plane->GetNumPropellors(), m_IsJet, m_OnGround, m_Vehicle->HasContactWheels());  NEXT_LINE
			formatf(tempString, "DOWNWASH HEIGHT %8.4f", m_DownwashHeight);  NEXT_LINE
			formatf(tempString, "THROTTLE %8.4f", static_cast<CPlane*>(m_Vehicle)->GetThrottle());  NEXT_LINE
			formatf(tempString, "THROTTLE CONTROL %8.4f", static_cast<CPlane*>(m_Vehicle)->GetThrottleControl());  NEXT_LINE
			formatf(tempString, "BOOST TIMER %8.4f     BOOST AMOUNT %8.4f", m_Vehicle->GetSpeedUpBoostDuration(), m_Vehicle->GetBoostAmount()); NEXT_LINE
			formatf(tempString, "ENG SPEED  %8.4f     ENG_ON %d    AUD_ENG_ON %d", m_EngineSpeed, m_Vehicle->m_nVehicleFlags.bEngineOn, IsEngineOn());  NEXT_LINE
			formatf(tempString, "ENG HEALTH %8.4f     MISSFIRE %d  RECOVER_MISSFIRE %d   TIME %8.4f",  			
			m_Vehicle->GetVehicleDamage()->GetEngineHealth(), plane->m_Transmission.GetCurrentlyMissFiring(), 
			plane->m_Transmission.GetCurrentlyRecoveringFromMissFiring(),
			plane->m_Transmission.GetOverrideMissFireTime());   NEXT_LINE

	}
}
#endif

// ----------------------------------------------------------------
// Update the engine sounds
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateEngineOn(audPlaneValues& planeValues)
{
	const Mat34V mat = m_Vehicle->GetMatrix();
	CPlane* plane = static_cast<CPlane*>(m_Vehicle);
	f32 engineSpeed = Clamp(m_EngineSpeed, 0.0f, 1.0f);

	f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;

	if(pitchAngle < 90.0f)
	{
		pitchAngle = 90.0f;
	}

	pitchAngle -= 90.0f;
	f32 pitchProportion = 1.0f - ((90.0f - pitchAngle)/90.0f);

	m_AirSpeed = m_CachedVehicleVelocity.Mag();
	f32 airSpeedZ = m_CachedVehicleVelocity.z;

	// only dive bomb if going fast and height is dropping
	static bank_float resetPitchProportion = 0.2f;
	if(pitchProportion < resetPitchProportion && (m_AirSpeed < m_PlaneAudioSettings->DiveAirSpeedThreshold || airSpeedZ > m_PlaneAudioSettings->DivingRateApproachingGround))
		pitchProportion = 0.0f;
	
	float divingRate = (float)m_PlaneAudioSettings->DivingFactor;
	if(divingRate > 100.0f)
		divingRate = 1.0f;
	s32 diveBombPitchChangeLoops = (s32)(m_angleBasedPitchLoopSmoother.CalculateValue(divingRate * m_PlaneAudioSettings->MaxDivePitch * pitchProportion));

	m_DiveSoundPitch = (s32)(m_angleBasedPitchSmoother.CalculateValue((f32)m_PlaneAudioSettings->DivingSoundPitchFactor * m_PlaneAudioSettings->MaxDiveSoundPitch * pitchProportion));

	m_DiveSoundVolumeLin = m_angleBasedVolumeSmoother.CalculateValue((f32)m_PlaneAudioSettings->DivingSoundVolumeFactor * pitchProportion);
	m_DiveSoundVolumeLin = Clamp(m_DiveSoundVolumeLin, 0.0f, 1.0f);

#if __BANK
	m_DebugDivePitchLoops = diveBombPitchChangeLoops;
	m_DebugVertAirSpeed = airSpeedZ;

	if(divingRate != m_DebugDivingRate)
	{
		static bank_float pullupRateMult = 2.5f;
		m_angleBasedPitchLoopSmoother.Init(divingRate, divingRate*pullupRateMult, 0.0f, (float)m_PlaneAudioSettings->MaxDivePitch);
		m_angleBasedPitchSmoother.Init(divingRate, divingRate*pullupRateMult, 0.0f, (float)m_PlaneAudioSettings->MaxDivePitch);
		m_DebugDivingRate = divingRate;

		static bank_float incRate = 0.01f;
		m_angleBasedVolumeSmoother.Init(divingRate*incRate, divingRate*incRate*pullupRateMult, 0.0f, 1.0f);

	}
#endif

		
	for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
	{
		f32 adjustedEngineSpeed = engineSpeed;

		if(loop == PLANE_LOOP_ENGINE || loop == PLANE_LOOP_EXHAUST || loop == PLANE_LOOP_PROPELLOR)
		{
			adjustedEngineSpeed *= planeValues.propellorHealthFactor;
		}

		planeValues.loopPitches[loop] = (s32) m_PlaneLoopPitchCurves[loop].CalculateValue(adjustedEngineSpeed);
		planeValues.loopPitches[loop] += diveBombPitchChangeLoops;
		planeValues.loopVolumes[loop] = m_PlaneLoopVolumeCurves[loop].CalculateValue(adjustedEngineSpeed);
		planeValues.postSubmixVolumes[loop] = m_PlaneLoopVolumeCones[loop].ComputeAttenuation(mat);
		planeValues.postSubmixVolumes[loop] += g_PlaneVolumeTrims[loop];

		if(loop == PLANE_LOOP_BANKING)
		{
			if(m_PlaneAudioSettings->BankingStyle == kRotationAngle)
			{
				planeValues.loopVolumes[loop] += m_BankingAngleVolCurve.CalculateValue(m_BankingAngle);
			}
			if(!m_IsJet || m_PlaneAudioSettings->BankingStyle == kRotationSpeed)
			{				
				f32 angularVelocity = GetCachedAngularVelocity().Mag();

				// Treat peeling in the same way as spinning
				if(m_IsPeeling && !m_OnGround)
				{
					angularVelocity += 5.0f;

					static bank_float fPeelingInc = 0.009f;
					m_PeelingFactor += fPeelingInc;
				}
				else
				{
					if(m_PeelingFactor > 0.5f)
						m_PeelingFactor = 0.5f;
					static bank_float fPeelingDec = 0.008f;
					m_PeelingFactor -= fPeelingDec;
				}
				m_PeelingFactor = Clamp(m_PeelingFactor, 0.0f, 1.0f);
				//Displayf("Peeling %f", m_PeelingFactor);
				planeValues.loopPitches[PLANE_LOOP_ENGINE] += (s32)m_PeelingPitchCurve.CalculateValue(m_PeelingFactor);
				planeValues.loopPitches[PLANE_LOOP_EXHAUST] += (s32)m_PeelingPitchCurve.CalculateValue(m_PeelingFactor);
				planeValues.loopPitches[PLANE_LOOP_BANKING] += (s32)m_PeelingPitchCurve.CalculateValue(m_PeelingFactor);
				planeValues.loopPitches[PLANE_LOOP_PROPELLOR] += (s32)m_PeelingPitchCurve.CalculateValue(m_PeelingFactor);

				planeValues.loopPitches[PLANE_LOOP_AFTERBURNER] += (s32)(m_PeelingPitchCurve.CalculateValue(m_PeelingFactor) * m_PlaneAudioSettings->PeelingAfterburnerPitchScalingFactor);
				
				f32 angularVelocitySmoothed = m_angularVelocitySmoother.CalculateValue(angularVelocity);
				if(m_InARecording)
				{
					planeValues.loopVolumes[loop] += m_BankingAngleVolCurve.CalculateValue(m_BankingAngle);
				}
				else
				{
					planeValues.loopVolumes[loop] += m_BankingAngleVolCurve.CalculateValue(angularVelocitySmoothed);
				}
			}
		}
		else if(loop == PLANE_LOOP_AFTERBURNER)
		{
			f32 throttle = 0.0f;
			if(m_Vehicle->m_nVehicleFlags.bEngineOn && IsEngineOn())
			{
				throttle = plane->GetThrottle();
				
				if(!m_IsPlayerVehicle && m_EngineSpeed > g_NpcEngineSpeedForAfterBurner)
				{
					throttle = 1.0f;
				}
			}
#if __BANK
			if(g_UseDebugPlaneTracker)
			{
				throttle = 0.5f;
			}
#endif
			planeValues.loopVolumes[loop] = m_PlaneLoopVolumeCurves[loop].CalculateValue(throttle);
			//Displayf("throttle %f abvol %f", throttle, planeValues.loopVolumes[loop]);
		}
		else if(loop == PLANE_LOOP_DISTANCE)
		{
			Vector3 planePosition = BANK_ONLY(g_UseDebugPlaneTracker? (Vector3) m_DebugTracker.GetPosition() : )VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
			Vector3 planeToListener = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(0)) - planePosition;
			m_DistanceFromListener = planeToListener.Mag();
			planeValues.loopVolumes[loop] += m_DistanceEffectVolCurve.CalculateValue(m_DistanceFromListener);

			if(!m_IsPlayerVehicle BANK_ONLY(|| g_UseDebugPlaneTracker)) 
			{
				static f32 maxDist = 100.0f;
				static f32 minDist = 0.0f;
				static f32 maxPitch = 10.0f;
				static f32 minPitch = 0.0f;

				static const audThreePointPiecewiseLinearCurve pitchCurve(minDist, maxPitch, maxDist/2.0f, maxPitch/2.0f, maxDist, minPitch);

				planeValues.loopPitches[loop] = (s32) pitchCurve.CalculateValue(m_DistanceFromListener);
			}
		}
		else if(loop == PLANE_LOOP_DAMAGE)
		{
			if(m_IsJet)
			{
				f32 vehicleHealth = m_Vehicle->GetVehicleDamage()->GetEngineHealth();
				planeValues.loopVolumes[loop] += audDriverUtil::ComputeDbVolumeFromLinear( m_DamageVolumeCurve.CalculateValue(vehicleHealth) );
			}
			else
			{
				planeValues.loopVolumes[loop] = g_SilenceVolume;
			}

		}
	}

	// we turn off the prop sound for jets that the player is flying. This sound is used for distant jet sounds.
	if(m_IsJet && (m_IsPlayerVehicle || m_PlaneStatus != AUD_PLANE_ON))
	{
		planeValues.loopVolumes[PLANE_LOOP_PROPELLOR] = -100.0f;
	}

	static bank_float downwashHeight = 0.2f;
	static bank_float airSpeedToTurnOnRoar = 30.0f;
	if(m_IsJet && !m_IsPlayerVehicle && ((m_OnGround && m_AirSpeed < airSpeedToTurnOnRoar) || (m_DownwashHeight > 0.0f && m_DownwashHeight < downwashHeight) || m_AirSpeed == 0.0f))
	{
		if(m_OnGround && m_AirSpeed > airSpeedToTurnOnRoar)
		{
			planeValues.loopVolumes[PLANE_LOOP_DISTANCE] = m_DistantSoundVolumeSmoother.CalculateValue(planeValues.loopVolumes[PLANE_LOOP_DISTANCE]);
			if(m_IsJet) // jets use the prop slot for up close in air roaring sound, need to smooth this as we get off the ground
			{
				planeValues.loopVolumes[PLANE_LOOP_PROPELLOR] += planeValues.loopVolumes[PLANE_LOOP_DISTANCE];  // this will fade out the jet flyby sweetner sound when on ground
			}
		}
		else
		{
			planeValues.loopVolumes[PLANE_LOOP_DISTANCE] = m_DistantSoundVolumeSmoother.CalculateValue(-100.0f);
			planeValues.loopVolumes[PLANE_LOOP_PROPELLOR] += planeValues.loopVolumes[PLANE_LOOP_DISTANCE];  // this will fade out the jet flyby sweetner sound when on ground
		}
	}
	else
	{
		planeValues.loopVolumes[PLANE_LOOP_DISTANCE] = m_DistantSoundVolumeSmoother.CalculateValue(planeValues.loopVolumes[PLANE_LOOP_DISTANCE]);
		if(m_IsJet) // jets use the prop slot for up close in air roaring sound, need to smooth this as we get off the ground
		{
			planeValues.loopVolumes[PLANE_LOOP_PROPELLOR] += planeValues.loopVolumes[PLANE_LOOP_DISTANCE];  // this will fade out the jet flyby sweetner sound when on ground
		}
	}

	planeValues.haveBeenCalculated = true;
}

// ----------------------------------------------------------------
// Update the flight mode for VTOL planes
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateVTOL()
{
	CPlane* plane = static_cast<CPlane*>(m_Vehicle);

	// Only update these variable whilst the engine is on
	if(plane->GetVerticalFlightModeAvaliable() && m_Vehicle->m_nVehicleFlags.bEngineOn)
	{
		f32 flightMode = plane->GetVerticalFlightModeRatio();

#if __BANK
		if(g_ForceFlightMode)
		{
			flightMode = g_ForcedFlightModeValue;
		}
		else
		{
			g_ForcedFlightModeValue = flightMode;
		}
#endif

		f32 downwashHeight = m_DownwashHeight >= 0.0f? m_DownwashHeight : 1.0f;
		f32 throttle = plane->GetThrottleControl();

		if(m_PlaneLoops[PLANE_LOOP_ENGINE])
		{
			m_PlaneLoops[PLANE_LOOP_ENGINE]->FindAndSetVariableValue(ATSTRINGHASH("flightmode", 0x1DFCE7B5), flightMode);
			m_PlaneLoops[PLANE_LOOP_ENGINE]->FindAndSetVariableValue(ATSTRINGHASH("distancetoground", 0x2A4943B4), downwashHeight);
			m_PlaneLoops[PLANE_LOOP_ENGINE]->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), throttle);
		}

		if(m_PlaneLoops[PLANE_LOOP_EXHAUST])
		{
			m_PlaneLoops[PLANE_LOOP_EXHAUST]->FindAndSetVariableValue(ATSTRINGHASH("flightmode", 0x1DFCE7B5), flightMode);
			m_PlaneLoops[PLANE_LOOP_EXHAUST]->FindAndSetVariableValue(ATSTRINGHASH("distancetoground", 0x2A4943B4), downwashHeight);
			m_PlaneLoops[PLANE_LOOP_EXHAUST]->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), throttle);
		}

		if(m_PlaneLoops[PLANE_LOOP_AFTERBURNER])
		{
			m_PlaneLoops[PLANE_LOOP_AFTERBURNER]->FindAndSetVariableValue(ATSTRINGHASH("flightmode", 0x1DFCE7B5), flightMode);
			m_PlaneLoops[PLANE_LOOP_AFTERBURNER]->FindAndSetVariableValue(ATSTRINGHASH("distancetoground", 0x2A4943B4), downwashHeight);
			m_PlaneLoops[PLANE_LOOP_AFTERBURNER]->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), throttle);
		}

		if(m_PlaneLoops[PLANE_LOOP_IDLE])
		{
			m_PlaneLoops[PLANE_LOOP_IDLE]->FindAndSetVariableValue(ATSTRINGHASH("flightmode", 0x1DFCE7B5), flightMode);
		}

		if(m_PlaneLoops[PLANE_LOOP_PROPELLOR])
		{
			m_PlaneLoops[PLANE_LOOP_PROPELLOR]->FindAndSetVariableValue(ATSTRINGHASH("flightmode", 0x1DFCE7B5), flightMode);
		}

		if(m_PlaneLoops[PLANE_LOOP_BANKING])
		{
			m_PlaneLoops[PLANE_LOOP_BANKING]->FindAndSetVariableValue(ATSTRINGHASH("flightmode", 0x1DFCE7B5), flightMode);
		}
	}
}

// ----------------------------------------------------------------
// Update sea plane specific stuff
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateSeaPlane()
{	
	if(CalculateIsInWater())
	{
		if(!m_WaterTurbulenceSound)
		{
			if(m_PlaneAudioSettings->WaterTurbulenceSound != g_NullSoundHash)
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				SetTracker(initParams);
				CreateAndPlaySound_Persistent(m_PlaneAudioSettings->WaterTurbulenceSound, &m_WaterTurbulenceSound, &initParams);
			}
		}	
	}
	else if(m_WaterTurbulenceSound)
	{
		m_WaterTurbulenceSound->StopAndForget();
	}			
}

// ----------------------------------------------------------------
// Called when the the 
// ----------------------------------------------------------------
void audPlaneAudioEntity::OnVerticalFlightModeChanged(f32 newFlightModeRatio)
{
	if(m_VTOLThrusterSoundset.IsInitialised())
	{
		if(m_VTOLModeSwitchSound)
		{
			m_VTOLModeSwitchSound->StopAndForget();
		}

		audMetadataRef switchSound = m_VTOLThrusterSoundset.Find(newFlightModeRatio == 1.0f? ATSTRINGHASH("thrusters_up", 0xA4515F19) : ATSTRINGHASH("thrusters_down", 0x5C8C8AC0));
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound_Persistent(switchSound, &m_VTOLModeSwitchSound, &initParams);

		if(m_VTOLModeSwitchSound)
		{
			m_VTOLModeSwitchSound->SetUpdateEntity(true);
			m_VTOLModeSwitchSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
		}
	}
}

void audPlaneAudioEntity::UpdateEngineCooling()
{
	CPlane* plane = static_cast<CPlane*>(m_Vehicle);

	if(plane)
	{
		static bank_float engineCooldownTemperatureRange = 20.0f;
		const f32 baseTemperature = g_weather.GetTemperature(m_Vehicle->GetTransform().GetPosition());
		const bool shouldEngineCoolingStop = (m_Vehicle->m_nVehicleFlags.bIsDrowning ||  m_Vehicle->m_nVehicleFlags.bEngineOn || m_Vehicle->m_EngineTemperature <= m_EngineOffTemperature - engineCooldownTemperatureRange);
		const bool shouldEngineCoolingStart = (!m_Vehicle->m_nVehicleFlags.bIsDrowning && ((m_PlaneStatus == AUD_PLANE_OFF || m_PlaneStatus == AUD_PLANE_STOPPING) && m_Vehicle->m_EngineTemperature > baseTemperature + 30.f));
		f32 engineCoolingProgress = Clamp((engineCooldownTemperatureRange - (m_EngineOffTemperature - m_Vehicle->m_EngineTemperature))/engineCooldownTemperatureRange, 0.0f, 1.0f);
		if(m_Vehicle->m_nVehicleFlags.bEngineOn)
		{
			engineCoolingProgress = 1.0f;
		}

#if __BANK
		if(g_ShowEngineCooling)
		{
			char tempString[128];
			formatf(tempString, "Cooling Progress %4.2f    OffTemp %4.2f    Temp:%4.2f    Base:%4.2f Occ %4.2f", engineCoolingProgress, m_EngineOffTemperature, m_Vehicle->m_EngineTemperature, baseTemperature, m_DebugOcclusion);
			grcDebugDraw::Text(m_Vehicle->GetTransform().GetPosition(), Color_green, tempString);
		}
#endif

		if(m_EngineCoolingSound)
		{
			if(shouldEngineCoolingStop)
			{
				m_EngineCoolingSound->StopAndForget();
			}
			else
			{
				((audSound*)m_EngineCoolingSound)->FindAndSetVariableValue(g_TemperatureVariableHash, engineCoolingProgress);
			}
		}
		else
		{
			if(shouldEngineCoolingStart && !shouldEngineCoolingStop)
			{
				audSoundInitParams initParams;

				initParams.EnvironmentGroup = m_EnvironmentGroup;
				CreateSound_PersistentReference(ATSTRINGHASH("HEAT_STRESS_HEAT_TICK_LOOP", 0x578B87D4), &m_EngineCoolingSound, &initParams);

				if(m_EngineCoolingSound)
				{
					m_EngineCoolingSound->SetUpdateEntity(true);
					m_EngineCoolingSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_ENGINE);
					((audSound*)m_EngineCoolingSound)->FindAndSetVariableValue(g_TemperatureVariableHash, engineCoolingProgress);

					m_EngineCoolingSound->PrepareAndPlay();
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Stop all the sounds
// ----------------------------------------------------------------
void audPlaneAudioEntity::StopEngine(bool fadeOut)
{
	static bank_u32 fadeOutTime = 5000;
	for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
	{
		if(m_PlaneLoops[loop])
		{
			if(!m_PlaneLoops[loop]->IsBeingReleased())
			{
				if(fadeOut)
				{
					m_PlaneLoops[loop]->SetRequestedDopplerFactor(0.0f);
					m_PlaneLoops[loop]->SetReleaseTime(fadeOutTime);
				}
				else
				{
					static bank_u32 defaultFadeOut = 1000;
					m_PlaneLoops[loop]->SetReleaseTime(defaultFadeOut);
				}
			}

			if(m_PlaneLoops[loop])
			{
				m_PlaneLoops[loop]->StopAndForget();
			}
		}
	}

	if(m_StallWarning)
	{
		if(fadeOut)
		{
			m_StallWarning->SetReleaseTime(fadeOutTime);
		}

		StopAndForgetSounds(m_StallWarning);
	}

	if(m_WindSound)
	{
		m_WindSound->StopAndForget();
	}

	if(m_DiveSound)
	{
		m_DiveSound->StopAndForget();
	}	

	m_HasPlayedShutdownSound = false;
	m_PlaneStatus = AUD_PLANE_OFF;
}

void audPlaneAudioEntity::ShutdownJetEngine()
{
	if(!m_HasPlayedShutdownSound)
	{
		// GTA special case for the Starling - extra shutdown behaviour
		if(m_PlaneStatus != AUD_PLANE_OFF && GetVehicleModelNameHash() == ATSTRINGHASH("STARLING", 0x9A9EB7DE))
		{
			audSoundSet soundset;
			const u32 soundsetName = m_IsFocusVehicle? ATSTRINGHASH("DLC_Smuggler_Starling_Sounds", 0xC99D5122) : ATSTRINGHASH("DLC_Smuggler_Starling_NPC_Sounds", 0x1C2ADE23);
			const u32 soundFieldName = ATSTRINGHASH("shutdown", 0xE99B0DCB);

			if(soundset.Init(soundsetName))
			{
				audSoundInitParams initParams;
				initParams.TrackEntityPosition = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				CreateAndPlaySound(soundset.Find(soundFieldName), &initParams);
				REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundFieldName, &initParams, GetOwningEntity()));
			}
		}

		m_HasPlayedShutdownSound = true;
	}

	if(m_FakeEngineSpeed == -10.0f)
		return;

	if(m_FakeEngineSpeed > -10.0f && m_FakeEngineSpeed < 0.0f)
	{
		m_FakeEngineSpeed = -10.0f;
		if(m_PlaneStatus != AUD_PLANE_ON) // don't stop the engine sound when we are just getting the fake engine to match the real engine
		{
			StopEngine();
		}
	}
	else if(!m_OnGround && m_FakeEngineSpeed < 0.03f && m_PlaneStatus == AUD_PLANE_STOPPING && !IsEngineOn())  // this is for gliding, detects when we are at the lowest point in engine speed
	{
		StopEngine();
	}
	else 
	{
		CPlane* plane = static_cast<CPlane*>(m_Vehicle);
		if(!m_OnGround && m_FakeEngineSpeed < plane->GetEngineSpeed())
		{
			m_FakeEngineSpeed = plane->GetEngineSpeed();
		}
		else
		{
			static bank_float instantOnOffDecTime = 0.05f;

			if(m_OnGround || !m_IsPlayerVehicle)
			{
				if(m_Vehicle->m_nVehicleFlags.bIsDrowning)
				{
					static bank_float decInAir = 0.01f;
					m_FakeEngineSpeed -= m_EngineInstantOnOff? instantOnOffDecTime : decInAir;
				}
				else
				{
					static bank_float decOnGround = 0.0005f;
					m_FakeEngineSpeed -= m_EngineInstantOnOff? instantOnOffDecTime : decOnGround;
				}
			}
			else
			{
				if(m_Vehicle->m_nVehicleFlags.bIsDrowning)
				{
					static bank_float decInAir = 0.01f;
					m_FakeEngineSpeed -= m_EngineInstantOnOff? instantOnOffDecTime : decInAir;
				}
				else
				{
					static bank_float decInAir = 0.004f;
					m_FakeEngineSpeed -= m_EngineInstantOnOff? instantOnOffDecTime : decInAir;
				}
			}
			//Displayf("Engine Speed %f", m_FakeEngineSpeed);
		}
	}
}


// -------------------------------------------------------------------------------
// Convert the plane to disabled
// -------------------------------------------------------------------------------
void audPlaneAudioEntity::ConvertToDisabled()
{
	if(m_PlaneStatus != AUD_PLANE_OFF && m_PlaneStatus != AUD_PLANE_STOPPING)
	{
		StopEngine(true);
	}
}

// ----------------------------------------------------------------
// Update stall warning
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateStallWarning(audPlaneValues& UNUSED_PARAM(planeValues))
{
	u32 currentTime = g_AudioEngine.GetTimeInMilliseconds();

	f32 airSpeed = m_CachedVehicleVelocity.Mag();
	f32 airSpeedZ = m_CachedVehicleVelocity.z;
	f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;

	if(!m_StallWarningOn)
	{
		// Don't warn during takeoff
		if(m_IsPlayerVehicle &&
		   m_TimeInAir > 5.0f)
		{
			if(pitchAngle < 37.5f)
			{
				if(airSpeedZ < 17.5f)
				{
					m_StallWarningOn = true;
				}
			}
		}
	}

	if(g_PlayerSwitch.IsActive())
	{
		m_StallWarningOn = false;
	}
	
	if(m_StallWarningOn)
	{
		if(m_IsPlayerVehicle)
		{
			if(m_TimeStallStarted == 0)
			{
				m_TimeStallStarted = currentTime; 
			}
			else if(currentTime - m_TimeStallStarted > (g_AircraftWarningSettings ? g_AircraftWarningSettings->PlaneWarningStall.MinTimeInStateToTrigger : 1500) )
			{
				g_ScriptAudioEntity.TriggerAircraftWarningSpeech(AUD_AW_PLANE_WARNING_STALL);
			}
		}
		
		if(airSpeed > 25.0f )
		{
			if(pitchAngle < 40.0f &&
				airSpeedZ < 0.0f)
			{
				// Case where the plane has picked up speed but only because its falling vertically backwards - need
				// to keep stall warning on in this situation
			}
			else
			{
				m_StallWarningOn = false;
			}
		}
		else if(pitchAngle > 90.0f ||
				m_TimeInAir <= 5.0f) // Crashed!
		{
			m_StallWarningOn = false;
		}
	}
	else if(m_IsPlayerVehicle)
	{
		m_TimeStallStarted = 0;
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix synth def
// -------------------------------------------------------------------------------
u32 audPlaneAudioEntity::GetEngineSubmixSynth() const
{
	if(ShouldUseDSPEffects() && m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->EngineSynthDef;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixSynth();
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix synth preset
// -------------------------------------------------------------------------------
u32 audPlaneAudioEntity::GetEngineSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects() && m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->EngineSynthPreset;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixSynthPreset();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth def
// -------------------------------------------------------------------------------
u32 audPlaneAudioEntity::GetExhaustSubmixSynth() const
{
	if(ShouldUseDSPEffects() && m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->ExhaustSynthDef;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixSynth();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth preset
// -------------------------------------------------------------------------------
u32 audPlaneAudioEntity::GetExhaustSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects() && m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->ExhaustSynthPreset;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixSynthPreset();
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix voice
// -------------------------------------------------------------------------------
u32 audPlaneAudioEntity::GetEngineSubmixVoice() const			
{
	if(m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->EngineSubmixVoice;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixVoice();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix voice
// -------------------------------------------------------------------------------
u32 audPlaneAudioEntity::GetExhaustSubmixVoice() const			
{
	if(m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->ExhaustSubmixVoice;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixVoice();
	}
}

// ----------------------------------------------------------------
// audPlaneAudioEntity GetCabinToneSound
// ----------------------------------------------------------------
u32 audPlaneAudioEntity::GetCabinToneSound() const
{
	if(m_PlaneAudioSettings)
	{
		return m_PlaneAudioSettings->CabinToneLoop;
	}

	return audVehicleAudioEntity::GetCabinToneSound();
}

// ----------------------------------------------------------------
// Trigger a tyre squeal when the wheel hits the ground
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateTyreSqueals()
{
	bool planeInAir = true;
		
	bool contactWheels = m_Vehicle->HasContactWheels();

	static bank_float distanceToFilterInstantInAirs = 400.0f;

	// For every wheel...
	for(s32 i = 0; i < m_Vehicle->GetNumWheels() && i < MAX_PLANE_WHEELS; i++)
	{
		bool wheelInAir = !(m_Vehicle->GetWheel(i)->GetIsTouching() || m_Vehicle->GetWheel(i)->GetWasTouching());

		// If the wheel is not in the air...
		if(!wheelInAir && contactWheels)
		{
			planeInAir = false;
			
			// And the wheel was previously in the air...
			if(m_WheelInAir[i])
			{
				// Then play our skid touchdown sound if we're traveling at a reasonable speed
				static bank_float fSkidVelocity = 0.15f;
				if(m_DistanceFromListener < distanceToFilterInstantInAirs && m_CachedVehicleVelocity.Mag() > fSkidVelocity && !m_Vehicle->GetIsInWater())
				{
					audEnvelopeSound* squealSound = NULL;
					audSoundInitParams initParams;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.Predelay = audEngineUtil::GetRandomNumberInRange(0, 200);
					SetTracker(initParams);
					u32 mainSkidHash = (m_CachedMaterialSettings?m_CachedMaterialSettings->MainSkid:g_DefaultTyreSqueal);
					if(mainSkidHash == g_NullSoundHash)
						mainSkidHash = g_DefaultTyreSqueal;

					CreateSound_LocalReference(ATSTRINGHASH("WHEEL_SKID_ENVELOPE", 0xEBABCAB5),(audSound**)&squealSound, &initParams);
					if(squealSound)
					{
						squealSound->SetRequestedEnvelope(g_TireSquealAttack, g_TireSquealDecay, g_TireSquealSustain, g_TireSquealHold, g_TireSquealRelease);
						squealSound->SetChildSoundReference(mainSkidHash);
						squealSound->SetClientVariable(GetWheelSoundUpdateClientVar(i));
						squealSound->SetUpdateEntity(true);
						squealSound->PrepareAndPlay();
					}
				}
			}

			m_WheelInAir[i] = false;
		}
	}

	// Wheels only count as being in the air if every wheel on the whole plane is also in the air - this prevents lots of skidding
	// sounds playing when wheels are quickly bumping along the surface, or when the physics is being indecisive
	if(planeInAir)
	{
		static bank_float minTimeToBeConsideredInAir = 0.5f;
		if(m_TimeInAir > minTimeToBeConsideredInAir)
		{
			for(s32 i = 0; i < MAX_PLANE_WHEELS; i++)
			{
				m_WheelInAir[i] = true;
			}
		}

		m_TimeInAir += fwTimer::GetTimeStep();
	}
	else
	{
		m_TimeInAir = 0.0f;
	}

	// check if we are far away and only in air for a tiny amount of time, this is likely the planes doing weird stuff on the collision
	//static bank_float distanceToFilterInstantInAirs = 250.0f;
	//static bank_float farAwayMinTimeToBeConsideredInAir = 1.0f;
	if(contactWheels) //planeInAir && m_DistanceFromListener > distanceToFilterInstantInAirs && m_TimeInAir < farAwayMinTimeToBeConsideredInAir)
	{
		m_OnGround = true;
	}
	else
	{
		m_OnGround = !planeInAir;
	}
}


void audPlaneAudioEntity::UpdateSuspension()
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

		f32 ratio = Clamp((absCompressionChange - m_PlaneAudioSettings->MinSuspCompThresh) / (m_PlaneAudioSettings->MaxSuspCompThres - m_PlaneAudioSettings->MinSuspCompThresh - (damageFactor*2.f)), 0.0f,1.0f);
		
		//if(m_IsPlayerVehicle)
		//	Displayf("Suspension Compression %f  LinearVol %f   change %f", ratio, (f32)sm_SuspensionVolCurve.CalculateValue(ratio), compressionChange);

#if __BANK
		if(g_DebugPlaneSuspension)
		{
			static f32 peakComp = 0.0f;
			if(absCompressionChange > peakComp)
			{
				peakComp= absCompressionChange;
				Displayf("Peak Suspension Compression %f  LinearVol %f", peakComp, (f32)sm_SuspensionVolCurve.CalculateValue(ratio));
			}
		}
#endif
		if(m_SuspensionSounds[i])
		{
			// Disabling volume update so that we hear the whole one shot rather than it being played then immediately muted
			// m_SuspensionSounds[i]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(ratio)));
			m_SuspensionSounds[i]->SetRequestedPosition(pos);
		}

		static bank_float wtfThatsTooHigh = 2.0f;
		static bank_float minTimeInAir = 1.0f;
		if(absCompressionChange > m_PlaneAudioSettings->MinSuspCompThresh && compressionChange < wtfThatsTooHigh && m_TimeInAir > minTimeInAir )
		{
			if(!m_SuspensionSounds[i])
			{
				// trigger a suspension sound
				if(compressionChange > 0)
				{
					BANK_ONLY(if(g_DebugPlaneSuspension) Displayf("Suspension Down");)
					
					u32	soundHash = m_PlaneAudioSettings->SuspensionDown;
					audSoundInitParams initParams;
					initParams.UpdateEntity = true;
					initParams.EnvironmentGroup = m_EnvironmentGroup;
					initParams.Position = pos;
					initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(ratio));
					CreateAndPlaySound_Persistent(soundHash, &m_SuspensionSounds[i], &initParams);
				}
			}
		}

		static bank_float suspensionUpThreshold = -0.1f;
		if(compressionChange < suspensionUpThreshold)
		{
			u32	soundHash = m_PlaneAudioSettings->SuspensionUp;
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.Position = pos;

			static bank_float fudge = 0.5f;
			f32 vol = Clamp(ratio+fudge, 0.0f, 1.0f);
			initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(vol));
			CreateAndPlaySound(soundHash, &initParams);
		}
	}
}

// ----------------------------------------------------------------
// Get the volume trim on the road noise
// ----------------------------------------------------------------
f32 audPlaneAudioEntity::GetRoadNoiseVolumeScale() const
{
	static bank_float roadNoiseVolumeScale = 1.0f;
	return roadNoiseVolumeScale;
}

u32 audPlaneAudioEntity::GetNPCRoadNoiseSound() const
{
	return ATSTRINGHASH("NPC_ROADNOISE_PASSES_DEFAULT", 0xC3F6AD5);
}

void audPlaneAudioEntity::UpdateDiveSound(audPlaneValues& planeValues)
{
	if(m_DiveSoundVolumeLin > 0.01f && m_Vehicle->GetStatus() != STATUS_WRECKED)
	{
		if(!m_DiveSound)
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			SetTracker(initParams);
			CreateAndPlaySound_Persistent(m_PlaneAudioSettings->DivingSound, &m_DiveSound, &initParams);
		}
		if(m_DiveSound)
		{
			m_DiveSound->SetRequestedPitch(m_DiveSoundPitch);
			m_DiveSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(m_DiveSoundVolumeLin) + planeValues.loopVolumes[PLANE_LOOP_ENGINE]);
			m_DiveSound->SetRequestedPostSubmixVolumeAttenuation(planeValues.postSubmixVolumes[PLANE_LOOP_ENGINE]);

		}
	}
	else
	{
		if(m_DiveSound)
			m_DiveSound->StopAndForget(true);
	}


}

// ----------------------------------------------------------------
// Play some wind noise when plane is in the air and has no engines
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateWindNoise(audPlaneValues& planeValues)
{
	f32 airSpeed = m_CachedVehicleVelocity.Mag();
	f32 engineSpeed = Clamp(m_EngineSpeed, 0.0f, 1.0f);
	static bank_float minAirSpeedForWind = 0.05f;
	if(m_IsPlayerVehicle && !m_OnGround && !m_Vehicle->HasContactWheels() && !m_Vehicle->GetIsInWater() && m_TimeInAir > 1.0f && (engineSpeed < 0.3f || planeValues.propellorHealthFactor <= 0.5f) && airSpeed > minAirSpeedForWind)
	{
		if(!m_WindSound)
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			SetTracker(initParams);
			CreateAndPlaySound_Persistent(m_PlaneAudioSettings->WindNoise, &m_WindSound, &initParams);
		}

		if(m_WindSound)
		{
			f32 volume = sm_BankingAngleToWindNoiseVolumeCurve.CalculateValue(m_BankingAngle);
			m_WindSound->SetRequestedVolume(volume);
			f32 pitch = sm_BankingAngleToWindNoisePitchCurve.CalculateValue(m_BankingAngle);
			m_WindSound->SetRequestedPitch((s32)pitch);
			//Displayf("%f", m_BankingAngle);
		}
	}
	else if(m_WindSound)
	{
		m_WindSound->StopAndForget();
	}
}


void audPlaneAudioEntity::BreakOffSection(int nHitSection)
{
	// play a little wind noise when a part breaks off in mid air
	switch(nHitSection)
	{
	case CAircraftDamage::ELEVATOR_L:
	case CAircraftDamage::ELEVATOR_R:
	case CAircraftDamage::AILERON_L:
	case CAircraftDamage::AILERON_R:
	case CAircraftDamage::RUDDER:
	case CAircraftDamage::RUDDER_2:
	case CAircraftDamage::AIRBRAKE_L:
	case CAircraftDamage::AIRBRAKE_R:
		{
			static bank_float minAirSpeedForWindPuff = 25.0f;
			if(!m_OnGround && m_IsPlayerVehicle && m_TimeInAir > 5.0f && m_CachedVehicleVelocity.Mag() > minAirSpeedForWindPuff)
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				SetTracker(initParams);
				CreateAndPlaySound("FRAGMENT_WIND_NOISE", &initParams);
			}
		}
		break;

	default:
		break;
	}
}


void audPlaneAudioEntity::TriggerPropBreak()
{
	if(m_PlaneAudioSettings == NULL)
		return;
	
	CreateDeferredSound(m_PlaneAudioSettings->PropellorBreakOneShot, m_Vehicle, NULL, true, true);
}


void audPlaneAudioEntity::TriggerBackFire()
{
	if(m_PlaneAudioSettings == NULL)
		return;

	audSoundInitParams initParams;
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	SetTracker(initParams);

	if(m_IsJet)
	{
		CreateAndPlaySound(m_PlaneAudioSettings->EngineMissFire, &initParams);
	}
	else
	{
		const u32 vehicleName = GetVehicleModelNameHash();

		if (vehicleName != ATSTRINGHASH("MICROLIGHT", 0x96E24857) && vehicleName == ATSTRINGHASH("MICROLIGHT_STEALTHY", 0x34D5AB02))
		{
			CreateAndPlaySound(ATSTRINGHASH("PROP_PLANE_BACKFIRE", 0x63D0699C), &initParams);
		}
	}

	m_BackFireTime = fwTimer::GetTimeInMilliseconds();
}

void audPlaneAudioEntity::TriggerDownwash(const u32 soundHash, const Vector3 &pos, const f32 distEvo, const f32 speedEvo)
{
	//veh_air_turbulance_default
	//veh_air_turbulance_sand
	//veh_air_turbulance_dirt
	//veh_air_turbulance_water
	//veh_air_turbulance_foliage
	
	SetDownwashHeightFactor(distEvo);

	if(!m_DownwashSound || m_DownwashHash != soundHash)
	{
		if(m_DownwashSound)
		{
			m_DownwashSound->StopAndForget();
		}
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound_Persistent(soundHash, &m_DownwashSound, &initParams);
		m_DownwashHash = soundHash;
	}

	m_LastDownwashTime = fwTimer::GetTimeInMilliseconds();

	if(m_DownwashSound)
	{
#if __BANK
		if(g_UseDebugPlaneTracker)
			m_DownwashSound->SetRequestedPosition(m_DebugTracker.GetPosition());
		else
#endif
			m_DownwashSound->SetRequestedPosition(pos);

		const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(speedEvo * Min(1.f, 2.f*(1.f-distEvo)));
		m_DownwashSound->SetRequestedVolume(dbVol + m_DownwashVolume);

		//Displayf("downwash vol %f + %f = %f", dbVol, m_DownwashVolume, dbVol+m_DownwashVolume);
	}
}

void audPlaneAudioEntity::UpdateFlyby()
{
	f32 distanceFromListener = m_DistanceFromListener;

	if(distanceFromListener > 2000.0f || m_LastDistanceFromListener > 2000.0f)
	{
		m_LastDistanceFromListener = distanceFromListener;
		return;
	}

	if(m_IsPlayerVehicle && camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera())
	{
		distanceFromListener = g_audEnvironment->ComputeDistanceToPanningListener(m_Vehicle->GetVehiclePosition());
	}

	m_ClosingRateAccum += (m_LastDistanceFromListener - distanceFromListener);
	u32 currentTime = fwTimer::GetTimeInMilliseconds();
	m_NumClosingSamples++;
	if(currentTime > m_TimeLastClosingRateUpdated + 1000)
	{
		m_ClosingRate = m_ClosingRateAccum / m_NumClosingSamples;
		m_NumClosingSamples = 0;
		m_ClosingRateAccum = 0.0f;
	}
	m_LastDistanceFromListener = distanceFromListener;

	f32 distanceToTrigger = 150.0f;
	f32 minAirSpeed = g_MinSpeedForJetFlyby;
	if(m_IsJet && sm_JetClosingToFlybyDistance.IsValid())
	{
		distanceToTrigger = sm_JetClosingToFlybyDistance.CalculateValue(m_ClosingRate);
	}
	else if(sm_PropClosingToFlybyDistance.IsValid())
	{
		minAirSpeed = g_MinSpeedForPropFlyby;
		distanceToTrigger = sm_PropClosingToFlybyDistance.CalculateValue(m_ClosingRate);
	}

	static bank_float minClosingRateForInVehicleConematicCam = 0.2f;
	static bank_float tooCloseDistance = 15.0f;
	static bank_float minEngineSpeed = 0.25f;
	f32 minClosingRate = g_MinClosingRateForFlyby;
	if(m_IsPlayerVehicle && camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera())
		minClosingRate = minClosingRateForInVehicleConematicCam;
	if(!m_FlybySound && m_Vehicle->m_nVehicleFlags.bEngineOn && (m_EngineSpeed > minEngineSpeed) && (!m_IsPlayerVehicle || camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera()) &&
		!m_OnGround && (m_DownwashHeight == -1.0f || m_DownwashHeight > 1.0f) && 
		m_AirSpeed > minAirSpeed && m_ClosingRate > minClosingRate && distanceFromListener < distanceToTrigger)
	{
		m_FlybyComingAtYou = true;
		TriggerFlyby();
	}
	else if(!m_FlybySound && m_Vehicle->m_nVehicleFlags.bEngineOn && (m_EngineSpeed > minEngineSpeed) && m_IsPlayerVehicle && camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera() &&
		!m_OnGround && (m_DownwashHeight == -1.0f || m_DownwashHeight > 1.0f) && 
		m_AirSpeed > minAirSpeed && (m_ClosingRate > -10.0f && m_ClosingRate < g_MinClosingRateForFlybyAway) && (distanceFromListener > tooCloseDistance && distanceFromListener < g_FlyingAwayDistanceToTrigger))
	{
		m_FlybyComingAtYou = false;
		TriggerFlyby();
	}

	if(m_FlybySound)
	{
		static bank_u32 closingRateThresholdTime = 2500;
		static bank_s32 defaultRelease = 5000;
		static bank_s32 quickRelease = 1000;
		s32 release = defaultRelease;
		if(currentTime < (m_FlybyStartTime + closingRateThresholdTime))
		{
			release = quickRelease;
		}

		static bank_float distanceToStop = 150.0f;
		if(!m_Vehicle->m_nVehicleFlags.bEngineOn || m_OnGround || (m_FlybyComingAtYou && !m_IsPlayerVehicle && m_ClosingRate <= 0.0f) || (m_IsPlayerVehicle && (distanceFromListener > distanceToStop || distanceFromListener < tooCloseDistance)) || (m_IsPlayerVehicle && !camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera()))
		{
			m_FlybySound->SetReleaseTime(release);
			m_FlybySound->StopAndForget();
		}
	}
	if(!m_FlybySound)
	{
		FreeSpeechSlot();
	}

#if __BANK
	if(g_ShowFlyby && (!m_IsPlayerVehicle || camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera()))
	{
		Vector3 planePosition = BANK_ONLY(g_UseDebugPlaneTracker? (Vector3) m_DebugTracker.GetPosition() : )VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition());
		Vector3 listener = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition(0));
		if(m_IsPlayerVehicle && camInterface::GetCinematicDirector().IsRenderingAnyInVehicleCinematicCamera())
		{
			listener = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0));
		}
		grcDebugDraw::Line(listener, planePosition, m_FlybySound ? Color_green : (distanceFromListener < distanceToTrigger ? Color_blue : Color_red));

		char tempString[128];
		formatf(tempString, "Closing %4.2f  Dist %4.2f  ListDist %4.2f", m_ClosingRate, distanceToTrigger, distanceFromListener);
		grcDebugDraw::Text(m_Vehicle->GetTransform().GetPosition(), m_FlybySound ? Color_green : Color_red, tempString);
	}
#endif

}

//void audPlaneAudioEntity::TriggerFlyby2()
//{
//	audSoundInitParams initParams;
//	initParams.EnvironmentGroup = m_EnvironmentGroup;
//	SetTracker(initParams);
//	//CreateAndPlaySound_Persistent(m_PlaneAudioSettings->FlybySound, &m_FlybySound, &initParams);
//	CreateAndPlaySound_Persistent(ATSTRINGHASH("SPL_RPG_DIST_FLIGHT_MASTER", 0xE8ED5661), &m_FlybySound, &initParams);
//}

void audPlaneAudioEntity::TriggerFlyby()
{
	// Only allow if there are lots of slots free, and we're not already playing the sound
	if((g_SpeechManager.GetNumVacantAmbientSlots() <= 3) || m_FlybySound)
	{
		return;
	}

	if(m_Vehicle->m_nVehicleFlags.bIsDrowning)
	{
		return;
	}

	// Try to grab a free ambient speech slot at low priority
	s32 speechSlotId = g_SpeechManager.GetAmbientSpeechSlot(NULL, NULL, -1.0f);

	if(speechSlotId >= 0)
	{
		audWaveSlot* speechSlot = g_SpeechManager.GetAmbientWaveSlotFromId(speechSlotId);

		// Success! Use this slot to load and play our asset
		if(speechSlot)
		{
			audSoundInitParams initParams;
			m_SpeechSlotId = speechSlotId;
			initParams.WaveSlot = speechSlot;
			initParams.AllowLoad = true;
			initParams.PrepareTimeLimit = 5000;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			SetTracker(initParams);
			g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(speechSlotId, 0, 
#if __BANK
				"JET_PLANE_FLYBY",
#endif
				0);

			if(m_FlybyComingAtYou && !m_IsPlayerVehicle)
			{
				if(m_PlaneAudioSettings->FlybySound == g_NullSoundHash && m_IsJet)
				{
					u32 soundHash = ATSTRINGHASH("JetFlyby", 0xB05301CB);
					CreateAndPlaySound_Persistent(audVehicleAudioEntity::GetExtrasSoundSet()->Find(soundHash), &m_FlybySound, &initParams);
				}
				else
				{
					CreateAndPlaySound_Persistent(m_PlaneAudioSettings->FlybySound, &m_FlybySound, &initParams);
				}
			}
			else
			{
				CreateAndPlaySound_Persistent(m_PlaneAudioSettings->FlyAwaySound, &m_FlybySound, &initParams);
			}
			m_FlybyStartTime = fwTimer::GetTimeInMilliseconds();
		}
		else
		{
			g_SpeechManager.FreeAmbientSpeechSlot(speechSlotId, true);
		}
	}
}

void audPlaneAudioEntity::FreeSpeechSlot()
{
	if(m_SpeechSlotId >= 0)
	{
		g_SpeechManager.FreeAmbientSpeechSlot(m_SpeechSlotId, true);
		m_SpeechSlotId = -1;
	}
}


#if __BANK
// ----------------------------------------------------------------
// Update debug stuff
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateDebug()
{
	// If this is enabled, we fudge the tracker position to simulate the plane flying overhead every x seconds 
	if(g_UseDebugPlaneTracker)
	{
		const ScalarV distanceTravelled = ScalarV((g_DebugTrackerFlybyDist/2.0f) - (g_DebugTrackerFlybyDist * (g_DebugTrackerTimer/g_DebugTrackerFlybyTime)));
		const Vec3V height(0.0f, 0.0f, g_DebugTrackerFlybyHeight);
		Vec3V newPosition = m_Vehicle->GetTransform().GetPosition() + (m_Vehicle->GetTransform().GetForward() * distanceTravelled);
		newPosition = newPosition + height;
		m_DebugTracker.SetPosition(RCC_VECTOR3(newPosition));

		for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
		{
			if(m_PlaneLoops[loop])
			{
				m_PlaneLoops[loop]->SetRequestedPosition(newPosition);
			}
		}

		m_ExhaustOffsetPos = newPosition;
		m_EngineOffsetPos = newPosition;

		//Displayf("Position: %f, %f, %f", newPosition.x, newPosition.y, newPosition.z);

		g_DebugTrackerTimer += fwTimer::GetTimeStep();

		if(g_DebugTrackerTimer > g_DebugTrackerFlybyTime)
		{
			g_DebugTrackerTimer = 0.0f;
		}

		static bank_float size = 1.0f;
		grcDebugDraw::Sphere(newPosition, size, Color_red);

	}

	CPlane* plane = static_cast<CPlane*>(m_Vehicle);

	if(plane && g_ShowDebugInfo && (m_IsPlayerVehicle || plane == g_pFocusEntity))
	{
		f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;
		f32 bankingAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetC(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
		f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
		f32 yawAngle = m_Vehicle->GetTransform().GetHeading() * RtoD;
		f32 angularVelocity = GetCachedAngularVelocity().Mag();
		f32 airSpeed = m_CachedVehicleVelocity.Mag();

		char tempString[64];
		formatf(tempString, "Roll:%.02f Banking:%.02f Pitch:%.02f Yaw:%.02f Ang:%.02f Spd:%.02f", rollAngle, bankingAngle, pitchAngle, yawAngle, angularVelocity, airSpeed);
		grcDebugDraw::Text(Vector2(0.25f, 0.05f), Color32(255,255,255), tempString );

		static const char *meterNames[] = {"Vel0", "Vel1", "Vel2", "Speed", "Engn", "REngn", "Roll", "Pitch", "Yaw" };
		static audMeterList meterList;
		static f32 meterValues[9];

		const Vector3 angVelocity = GetCachedAngularVelocity();
		meterValues[0] = Abs(angVelocity[0]);
		meterValues[1] = Abs(angVelocity[1]);
		meterValues[2] = Abs(angVelocity[2]);
		meterValues[3] = angVelocity.Mag();
		meterValues[4] = m_EngineSpeed;
		meterValues[5] = plane->GetEngineSpeed();
		meterValues[6] = Abs(rollAngle/500.0f);
		meterValues[7] = pitchAngle/500.0f;
		meterValues[8] = yawAngle/500.0f;

		meterList.left = 740.f;
		meterList.bottom = 420.f;
		meterList.width = 400.f;
		meterList.height = 200.f;
		meterList.values = &meterValues[0];
		meterList.names = meterNames;
		meterList.numValues = sizeof(meterNames)/sizeof(meterNames[0]);
		audNorthAudioEngine::DrawLevelMeters(&meterList);
	}

	static bank_float length = 10.0f;
	Vec3V position = m_Vehicle->GetTransform().GetPosition();
	if(g_ShowEnginePosition)
	{
		if(m_PlaneAudioSettings->EngineConeFrontAngle > 90)
		{
			f32 radius = tan((m_PlaneAudioSettings->EngineConeFrontAngle - 90)*DtoR) * length;
			grcDebugDraw::Cone(position, position - (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_green, false, false);
		}
		else
		{
			f32 radius = tan((m_PlaneAudioSettings->EngineConeFrontAngle)*DtoR) * length;
			grcDebugDraw::Cone(position, position + (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_green, false, false);
		}

		if(m_PlaneAudioSettings->EngineConeRearAngle > 90)
		{
			f32 radius = tan((m_PlaneAudioSettings->EngineConeRearAngle - 90)*DtoR) * length;
			grcDebugDraw::Cone(position, position - (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_red, false, false);
		}
		else
		{
			f32 radius = tan((m_PlaneAudioSettings->EngineConeRearAngle)*DtoR) * length;
			grcDebugDraw::Cone(position, position + (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_red, false, false);
		}
	}
	if(g_ShowExhaustPosition)
	{
		if(m_PlaneAudioSettings->ExhaustConeFrontAngle > 90)
		{
			f32 radius = tan((m_PlaneAudioSettings->ExhaustConeFrontAngle - 90)*DtoR) * length;
			grcDebugDraw::Cone(position, position + (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_green, false, false);
		}
		else
		{
			f32 radius = tan((m_PlaneAudioSettings->ExhaustConeFrontAngle)*DtoR) * length;
			grcDebugDraw::Cone(position, position - (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_green, false, false);
		}

		if(m_PlaneAudioSettings->ExhaustConeRearAngle > 90)
		{
			f32 radius = tan((m_PlaneAudioSettings->ExhaustConeRearAngle - 90)*DtoR) * length;
			grcDebugDraw::Cone(position, position + (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_red, false, false);
		}
		else
		{
			f32 radius = tan((m_PlaneAudioSettings->ExhaustConeRearAngle)*DtoR) * length;
			grcDebugDraw::Cone(position, position - (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_red, false, false);
		}
	}
	if(g_ShowPropPosition)
	{
		if(m_PlaneAudioSettings->PropellorConeFrontAngle > 90)
		{
			f32 radius = tan((m_PlaneAudioSettings->PropellorConeFrontAngle - 90)*DtoR) * length;
			grcDebugDraw::Cone(position, position - (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_green, false, false);
		}
		else
		{
			f32 radius = tan((m_PlaneAudioSettings->PropellorConeFrontAngle)*DtoR) * length;
			grcDebugDraw::Cone(position, position + (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_green, false, false);
		}

		if(m_PlaneAudioSettings->PropellorConeRearAngle > 90)
		{
			f32 radius = tan((m_PlaneAudioSettings->PropellorConeRearAngle - 90)*DtoR) * length;
			grcDebugDraw::Cone(position, position - (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_red, false, false);
		}
		else
		{
			f32 radius = tan((m_PlaneAudioSettings->PropellorConeRearAngle)*DtoR) * length;
			grcDebugDraw::Cone(position, position + (m_Vehicle->GetTransform().GetForward() * ScalarV(length)), radius, Color_red, false, false);
		}

	}
}
#endif

// ----------------------------------------------------------------
// Update 'tricks'
// ----------------------------------------------------------------
void audPlaneAudioEntity::UpdateTricks()
{
	f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
	f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;
	f32 yawAngle = m_Vehicle->GetTransform().GetHeading() * RtoD;
	UNUSED_VAR(yawAngle);
	m_IsPeeling = false;

	// If we're doing a Top-Gun style horizontal peel, we rotate the aircraft sideways, keep its pitch level, then quickly pull away
	static bank_float fRollAngleLimit = PEELING_ROLL_LIMIT;
	if(fabs(rollAngle) > 90.0f - fRollAngleLimit && fabs(rollAngle) < 90.0f + fRollAngleLimit)
	{
		static bank_float fPeelingPitchLimit = PEELING_PITCH_LIMIT;
		static bank_float fPeelingPitchUpLimit = PEELING_PITCH_UP_LIMIT;
		if(fabs(pitchAngle) > 90.0f - fPeelingPitchUpLimit && fabs(pitchAngle) < 90.0f + fPeelingPitchLimit)
		{
			CPlane* plane = static_cast<CPlane*>(m_Vehicle);

			static bank_float fPitchControl = PEELING_STICK_DEFLECTION;
			if(plane->GetPitchControl() >= fPitchControl)
			{
				m_IsPeeling = true;

#if __BANK
				if(g_ShowDebugInfo)
				{
					char tempString[64];
					formatf(tempString, "PEELING");
					grcDebugDraw::Text(Vector2(0.5f, 0.5f), Color32(255,255,255), tempString);
				}
#endif
			}
		}
	}
}

// ----------------------------------------------------------------
// Trigger a door open sound
// ----------------------------------------------------------------
void audPlaneAudioEntity::TriggerDoorOpenSound(eHierarchyId doorId)
{
	if(m_PlaneAudioSettings)
	{
		TriggerDoorSound(m_PlaneAudioSettings->DoorOpen, doorId);
	}
}

// ----------------------------------------------------------------
// Trigger a door shut sound
// ----------------------------------------------------------------
void audPlaneAudioEntity::TriggerDoorCloseSound(eHierarchyId doorId, const bool UNUSED_PARAM(isBroken))
{
	// if it's the player vehicle, and they're pissed off - slam the door. Note that this'll have his passengers slam the door too - oh well.
	f32 volumeOffset = 0.0f;

	if (m_Vehicle && m_IsPlayerVehicle && 
		CGameWorld::GetMainPlayerInfo()->PlayerIsPissedOff())
	{
		if (doorId == VEH_DOOR_DSIDE_F || 
			doorId == VEH_DOOR_DSIDE_R ||
			doorId == VEH_DOOR_PSIDE_F ||
			doorId == VEH_DOOR_PSIDE_R)
		{
			volumeOffset = 6.0f;
		}
	}

	if(m_PlaneAudioSettings)
	{
		TriggerDoorSound(m_PlaneAudioSettings->DoorClose, doorId, volumeOffset);
	}
}

// ----------------------------------------------------------------
// Trigger door open sound
// ----------------------------------------------------------------
void audPlaneAudioEntity::TriggerDoorFullyOpenSound(eHierarchyId doorId)
{
	if(m_PlaneAudioSettings)
	{
		TriggerDoorSound(m_PlaneAudioSettings->DoorLimit, doorId);
	}
}

void audPlaneAudioEntity::TriggerDoorStartOpenSound(eHierarchyId doorIndex) 
{
	if(m_PlaneAudioSettings && m_Vehicle->GetStatus() != STATUS_WRECKED)
	{
		if(m_PlaneAudioSettings->DoorStartOpen == g_NullSoundHash)
		{
			TriggerDoorSound(m_PlaneAudioSettings->DoorOpen, doorIndex);
		}
		TriggerDoorSound(m_PlaneAudioSettings->DoorStartOpen, doorIndex);
	}
}

void audPlaneAudioEntity::TriggerDoorStartCloseSound(eHierarchyId doorIndex) 
{
	if(m_PlaneAudioSettings && m_Vehicle->GetStatus() != STATUS_WRECKED)
	{
		TriggerDoorSound(m_PlaneAudioSettings->DoorStartClose, doorIndex);
	}
}

// ----------------------------------------------------------------
// audPlaneAudioEntity GetVehicleRainSound
// ----------------------------------------------------------------
u32 audPlaneAudioEntity::GetVehicleRainSound(bool interiorView) const
{
	if(m_PlaneAudioSettings)
	{
		if(interiorView)
		{
			return m_PlaneAudioSettings->VehicleRainSoundInterior;
		}
		else
		{
			return m_PlaneAudioSettings->VehicleRainSound;
		}		
	}

	return audVehicleAudioEntity::GetVehicleRainSound(interiorView);
}

// ----------------------------------------------------------------
// Deploy landing gear
// ----------------------------------------------------------------
void audPlaneAudioEntity::DeployLandingGear()
{
	if(m_PlaneAudioSettings)
	{
		StopAndForgetSounds(m_LandingGearRetract, m_LandingGearDeploy);

		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		SetTracker(initParams);

		CreateSound_PersistentReference(m_PlaneAudioSettings->LandingGearDeploy, &m_LandingGearDeploy, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_PlaneAudioSettings->LandingGearDeploy, &initParams, m_LandingGearDeploy, m_Vehicle, eNoGlobalSoundEntity);)

		if(m_LandingGearDeploy)
		{
			m_LandingGearDeploy->SetUpdateEntity(true);
			m_LandingGearDeploy->SetClientVariable((u32)AUD_VEHICLE_SOUND_UNKNOWN);
			m_LandingGearDeploy->PrepareAndPlay();
		}
	}
}

// ----------------------------------------------------------------
// Retract landing gear
// ----------------------------------------------------------------
void audPlaneAudioEntity::RetractLandingGear()
{
	if(m_PlaneAudioSettings)
	{
		if(m_PlaneAudioSettings->LandingGearRetract == g_NullSoundHash)
			return;

		StopAndForgetSounds(m_LandingGearRetract, m_LandingGearDeploy);

		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		SetTracker(initParams);

		CreateSound_PersistentReference(m_PlaneAudioSettings->LandingGearRetract, &m_LandingGearRetract, &initParams);

		REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_PlaneAudioSettings->LandingGearRetract, &initParams, m_LandingGearRetract, m_Vehicle, eNoGlobalSoundEntity);)

		if(m_LandingGearRetract)
		{
			m_LandingGearRetract->SetUpdateEntity(true);
			m_LandingGearRetract->SetClientVariable((u32)AUD_VEHICLE_SOUND_UNKNOWN);
			m_LandingGearRetract->PrepareAndPlay();
		}
	}
}

// ----------------------------------------------------------------
// Set the crop dusting active or not
// ----------------------------------------------------------------
void audPlaneAudioEntity::SetCropDustingActive(bool active)
{
	if(m_PlaneAudioSettings)
	{
		if(!active)
		{
			StopAndForgetSounds(m_CropDustingSound);
		}
		else
		{
			if(!m_CropDustingSound)
			{
				audSoundInitParams initParams;
				initParams.UpdateEntity = true;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				SetTracker(initParams);

				CreateSound_PersistentReference(sm_ExtrasSoundSet.Find(ATSTRINGHASH("CropDuster_SprayLoop", 1764320963)), &m_CropDustingSound, &initParams);

				if(m_CropDustingSound)
				{
					m_CropDustingSound->SetUpdateEntity(true);
					m_CropDustingSound->SetClientVariable((u32)AUD_VEHICLE_SOUND_UNKNOWN);
					m_CropDustingSound->PrepareAndPlay();
				}
			}
		}
	}
}

// ----------------------------------------------------------------
// Set the stall warning sound enabled or not
// ----------------------------------------------------------------
void audPlaneAudioEntity::SetStallWarningEnabled(bool enabled)
{
	m_StallWarningEnabled = enabled;

	if(!m_StallWarningEnabled)
	{
		StopAndForgetSounds(m_StallWarning);
	}
}

// ----------------------------------------------------------------
// Get a sound from the object data
// ----------------------------------------------------------------
u32 audPlaneAudioEntity::GetSoundFromObjectData(u32 fieldHash) const
{
	if(m_PlaneAudioSettings) 
	{ 
		u32 *ret = (u32 *)m_PlaneAudioSettings->GetFieldPtr(fieldHash);
		return (ret) ? *ret : 0; 
	} 
	return 0;
}

bool audPlaneAudioEntity::AircraftWarningVoiceIsMale()
{
	if(m_PlaneAudioSettings)
	{
		return AUD_GET_TRISTATE_VALUE(m_PlaneAudioSettings->Flags, FLAG_ID_PLANEAUDIOSETTINGS_AIRCRAFTWARNINGVOICEISMALE) == AUD_TRISTATE_TRUE;
	}
	return true;
}

AmbientRadioLeakage audPlaneAudioEntity::GetPositionalPlayerRadioLeakage() const
{
	return g_AircraftPositionedPlayerVehicleRadioLeakage;
}

#if __BANK
void audPlaneAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Planes", false);
		bank.AddToggle("Planes Enabled", &g_PlanesEnabled);
		bank.AddToggle("Show Debug Info", &g_ShowDebugInfo);
		bank.AddToggle("Show Engine Cooling", &g_ShowEngineCooling);
		bank.AddToggle("Show Flyby", &g_ShowFlyby);
		bank.AddToggle("Show Engine Info", &g_ShowEngineInfo);

		bank.PushGroup("Flyby", false);
			bank.AddSlider("Min Closing Rate", &g_MinClosingRateForFlyby, 0.0f, 10.0f, 0.1f);
			bank.AddSlider("Min Jet Speed To Trigger", &g_MinSpeedForJetFlyby, 0.0f, 100.0f, 0.5f);
			bank.AddSlider("Min Prop Speed To Trigger", &g_MinSpeedForPropFlyby, 0.0f, 100.0f, 0.5f);
		bank.PopGroup();

		bank.PushGroup("Cones", false);
			bank.AddToggle("Show Engine Cone", &g_ShowEnginePosition);
			bank.AddToggle("Show Exhaust Cone", &g_ShowExhaustPosition);
			bank.AddToggle("Show Prop Cone", &g_ShowPropPosition);
		bank.PopGroup();

		bank.PushGroup("Flight Mode", false);
			bank.AddToggle("Force Flight Mode", &g_ForceFlightMode);
			bank.AddSlider("Forced Flight Mode", &g_ForcedFlightModeValue, 0.0f, 10.0f, 0.1f);
		bank.PopGroup();

		bank.PushGroup("Passenger Radio", false);		
			bank.AddToggle("Positioned Player Vehicle Radio Enabled", &g_PositionedPlayerVehicleRadioEnabled);
			bank.AddSlider("Reverb Small", &g_PositionedPlayerVehicleRadioReverbSmall, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Reverb Medium", &g_PositionedPlayerVehicleRadioReverbMedium, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Reverb Large", &g_PositionedPlayerVehicleRadioReverbLarge, 0.0f, 1.0f, 0.01f);
			const char *leakageNames[NUM_AMBIENTRADIOLEAKAGE];
			for(s32 l = LEAKAGE_BASSY_LOUD; l < NUM_AMBIENTRADIOLEAKAGE; l++)
			{
				leakageNames[l] = AmbientRadioLeakage_ToString((AmbientRadioLeakage)l);
			}
			bank.AddCombo("Passenger Radio Leakage", (s32*)&g_AircraftPositionedPlayerVehicleRadioLeakage, NUM_AMBIENTRADIOLEAKAGE, leakageNames);
		bank.PopGroup();

		bank.AddToggle("g_DebugPlaneSuspension", &g_DebugPlaneSuspension);
		bank.AddToggle("Force Stall Warning", &g_ForceStallWarningOn);
		bank.AddSlider("Stall Warning Severity", &g_ForcedStallWarningSeverity, 0.0f, 1.0f, 0.1f);
		bank.AddSlider("Engine Volume Trim", &g_PlaneVolumeTrims[PLANE_LOOP_ENGINE], -100.0f, 0.0f, 0.5f);
		bank.AddSlider("Exhaust Volume Trim", &g_PlaneVolumeTrims[PLANE_LOOP_EXHAUST], -100.0f, 0.0f, 0.5f);
		bank.AddSlider("Propellor Volume Trim", &g_PlaneVolumeTrims[PLANE_LOOP_PROPELLOR], -100.0f, 0.0f, 0.5f);
		bank.AddSlider("Idle Volume Trim", &g_PlaneVolumeTrims[PLANE_LOOP_IDLE], -100.0f, 0.0f, 0.5f);
		bank.AddSlider("Distance Volume Trim", &g_PlaneVolumeTrims[PLANE_LOOP_DISTANCE], -100.0f, 0.0f, 0.5f);
		bank.AddSlider("Banking Volume Trim", &g_PlaneVolumeTrims[PLANE_LOOP_BANKING], -100.0f, 0.0f, 0.5f);
		bank.AddSlider("Afterburner Volume Trim", &g_PlaneVolumeTrims[PLANE_LOOP_AFTERBURNER], -100.0f, 0.0f, 0.5f);

		bank.PushGroup("Tyre Squeal", false);
			bank.AddSlider("Attack", &g_TireSquealAttack, 0, 1000, 1);
			bank.AddSlider("Decay", &g_TireSquealDecay, 0, 1000, 1);
			bank.AddSlider("Sustain", &g_TireSquealSustain, 0, 255, 1);
			bank.AddSlider("Hold", &g_TireSquealHold, 0, 1000, 1);
			bank.AddSlider("Release", &g_TireSquealRelease, 0, 1000, 1);
		bank.PopGroup();

		bank.PushGroup("Debug Flybys", false);
			bank.AddToggle("Enable Debug Flybys", &g_UseDebugPlaneTracker);
			bank.AddSlider("Flyby Engine Speed", &g_DebugTrackerFlybyEngineSpeed, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("Flyby Time", &g_DebugTrackerFlybyTime, 0.0f, 1000.0f, 0.5f);
			bank.AddSlider("Flyby Distance", &g_DebugTrackerFlybyDist, 0.0f, 10000.0f, 10.0f);
			bank.AddSlider("Flyby Height", &g_DebugTrackerFlybyHeight, 0.0f, 10000.0f, 10.0f);
		bank.PopGroup();

		bank.PushGroup("Players Plane", false);
			bank.AddSlider("Override Engine Damage Left(Main)", &g_OverRideEngineDamage[0], -1.0f, 1000.0f, 1.0f);
			bank.AddSlider("Override Engine Damage Right", &g_OverRideEngineDamage[1], -1.0f, 1000.0f, 1.0f);
			bank.AddToggle("Apply Damage", &g_ApplyEngineDamage);
		bank.PopGroup();


	bank.PopGroup();
}
#endif
