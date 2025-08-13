// 
// audio/heliaudioentity.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "heliaudioentity.h"
#include "northaudioengine.h"
#include "speechmanager.h"
#include "audio/scannermanager.h"

#include "audioengine/engine.h"
#include "Vehicles/Heli.h"
#include "audiosoundtypes/simplesound.h"
#include "audiohardware/submix.h"
#include "audioeffecttypes/waveshapereffect.h"
#include "camera/cinematic/CinematicDirector.h"
#include "camera/CamInterface.h"
#include "game/weather.h"

#include "Peds/ped.h"

AUDIO_VEHICLES_OPTIMISATIONS()

float g_MinHealthForHeliWarning	= 200.0f; // also declared externally
bank_float g_JetpackStrafeThrusterTriggerLimit	= 2.f;
bank_float g_MinValidThrusterForce			= 0.05f;
bank_u32 g_MinJetpackRetriggerTime			= 0;
bank_float g_HeliIntraFlipProb				= 3.0f;
bank_float g_HeliInterFlipProb				= 3.0f;
bank_float g_HeliDamageRate					= 100.0f;
bank_float g_HeliDamageVolumeRange			= -6.0f;
bank_float g_MinRotorHealthForHeliGoingDown = 200.0f;
bank_float g_HeliReplayStartupTime			= 500.0f;
bank_float g_HeliStartupTime				= 5000.0f;
bank_float g_HeliShutdownWaterTime			= 1000.0f;
bank_float g_HeliWinddownTime				= 10000.0f;
bank_float g_OverridenHeliHealth			= 100.0f;
bank_float g_HeliActivationDistSq			= 300.0f * 300.0f;
bank_s32 g_HeliSlowMoPitch					= -200;
bank_bool g_DebugHeli						= false;
bank_bool g_PlayCabinTone					= false;
bank_bool g_OverrideHeliHealth				= false;
bank_float g_InteriorSmoothingRate			= 0.1f;
bank_float g_HeliSlowmoSmoothingTime		= 2000.0f;
bank_u32 g_TimeBetweenRestartSounds			= 5000;
bank_u32 g_MaxFailedToStartTries			= 5;

#if __BANK
f32 g_HeliRotorTriggerRate = 1.0f;
f32 g_HeliLowPassFilter = 23999.0f;
#endif

extern u32 g_SuperDummyRadiusSq;
extern const u32 g_TemperatureVariableHash;
extern CWeather g_weather;
extern audScannerManager g_AudioScannerManager;

audCurve g_HeliMinigunSpinToVol, g_HeliMinigunSpinToPitch;

audCurve audHeliAudioEntity::sm_SuspensionVolCurve;
audCurve audHeliAudioEntity::sm_GritSpeedToLinVolCurve;
extern AmbientRadioLeakage g_AircraftPositionedPlayerVehicleRadioLeakage;

#if __BANK
f32 g_DebugHeliTrackerFlybyTime = 50.0f;
f32 g_DebugHeliTrackerFlybyDist = 2000.0f;
f32 g_DebugHeliTrackerFlybyHeight = 200.0f;
f32 g_DebugHeliTrackerTimer = 0.0f;
bool g_UseDebugHeliTracker = false;
bool g_DebugSuspension = false;
s32 g_PitchShiftIndex = -1;
bool g_ShowHeliEngineCooling = false;
bool g_EnableCrazyHeliSounds = false;

f32 g_HeliTimeWarpGap = 0.1f;
#endif

extern u32 g_audVehicleLoopReleaseTime;

audCurve audHeliAudioEntity::sm_HeliStartupToNormalCurve;
s16 audHeliAudioEntity::sm_PitchShiftIndex = 0;
s16 audHeliAudioEntity::sm_PitchShift[NUM_NPC_PICH_SHIFTS] = {0, 100, 200};


enum
{
	AUD_HELI_OFF,
	AUD_HELI_STARTING,
	AUD_HELI_ON,
	AUD_HELI_STOPPING
};

// ----------------------------------------------------------------
// audHeliAudioEntity constructor
// ----------------------------------------------------------------
audHeliAudioEntity::audHeliAudioEntity() : 
	  audVehicleAudioEntity()
{
	m_VehicleType = AUD_VEHICLE_HELI;
	m_HeliBank = m_HeliExhaust = m_HeliRotor = m_HeliRearRotor = m_HeliRotorSlowmo = m_HeliRotorLowFreqSlowmo = NULL;
	m_HeliDamage = m_HeliDamageOneShots = m_HeliDamageBelow600 = NULL;
	m_HeliDownwash = m_HeliGoingDown = m_HeliRotorLowFreq = m_DistantRotor = NULL;
	m_CrazyWarning = NULL;
	m_CustomLandingGearLoop = NULL;
	m_FailedToStartSound = NULL;
	
	for(int i=0; i<NUM_VEH_CWHEELS_MAX; i++)
	{
		m_SuspensionSounds[i] = NULL;
		m_LastCompressionChange[i] = 0.0f;
	}

	m_HeliState = AUD_HELI_OFF;
	m_HeliAudioSettings = NULL;
	m_AltitudeWarning = NULL;
	m_HeliDamageWarning = NULL;
	m_ScrapeSound = NULL;
	m_HardScrapeSound = NULL;
	m_StartupSound = NULL;
	m_EngineCoolingSound = NULL;
	m_RotorImpactSound = NULL;
	m_PrevScrapeSound = 0;
	m_SpeechSlotId = -1;
	m_EnableCrazyHeliSounds = false;
	m_RequiresSFXBank = false;
	m_RequiresLowLatencyWaveSlot = true;
	m_Shotdown = false;
	m_ShockwaveId = kInvalidShockwaveId;
	m_PitchShiftIndex = 0;
	m_Exploded = false;
	m_LastRotorImpactTime = 0;
	m_LastRotorImpactSoundTime = 0;
	m_BackFireTime = 0;
	m_PrevLandingGearState = CLandingGear::STATE_LOCKED_DOWN;
	REPLAY_ONLY(m_LastReplaySpeechSlotAllocTime = 0;)
	
	m_FailedToStartCount = 0;
	m_LastFailToStartTime = 0;

	m_PlayGoingDown = false;
	m_HasBeenScannerDecremented = false;

	BANK_ONLY(m_DebugOcclusion = 0.0f;)
}

audHeliAudioEntity::~audHeliAudioEntity() 
{
	CHeli* heli = static_cast<CHeli*>(m_Vehicle);
	if(heli &&(heli->GetIsPoliceDispatched() || heli->GetIsSwatDispatched()) && !m_HasBeenScannerDecremented)
	{
		g_AudioScannerManager.DecrementHelisInChase();
	}
	
	if(m_ShockwaveId != kInvalidShockwaveId)
	{
		audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(m_ShockwaveId);
		m_ShockwaveId = kInvalidShockwaveId;
	}

	StopAndForgetSounds(m_HeliGoingDown, m_ScrapeSound, m_HardScrapeSound, m_HeliDownwash, m_HeliDamageWarning, m_EngineCoolingSound, m_CrazyWarning, m_FailedToStartSound);
	StopHeliEngineSounds();
	FreeSpeechSlot();
};


// ----------------------------------------------------------------
// audHeliAudioEntity Reset
// ----------------------------------------------------------------
void audHeliAudioEntity::Reset()
{
	audVehicleAudioEntity::Reset();
	m_HeliState = AUD_HELI_OFF;
	m_HeliStartupRatio = 0.0f;
	m_HeliStartupSmoother.Init(1.0f/g_HeliStartupTime, 1.0f/g_HeliWinddownTime, true);
	m_HeliShutdownWaterSmoother.Init(1.0f/g_HeliShutdownWaterTime, 1.0f/g_HeliShutdownWaterTime, true);
	m_HeliSlowmoSmoother.Init(1.0f, 1.0f/g_HeliSlowmoSmoothingTime, true);

	m_HeliShouldSkipStartup = false;
	m_MainRotorVolume = 0.0f;
	m_PrevJetpackThrusterLeftForce = 0.f;
	m_PrevJetpackThrusterRightForce = 0.f;
	m_Throttle = 0.0f;
	m_MainRotorSpeed = 0.0f;
	m_PlayGoingDown = false;
	m_Exploded = false;
	m_LastTimeInStrafeMode = 0u;
	m_LastJetpackStrafeTime = 0u;
	m_FailedToStartCount = 0;
	m_LastFailToStartTime = 0;

}

// ----------------------------------------------------------------
// Acquire a wave slot
// ----------------------------------------------------------------
bool audHeliAudioEntity::AcquireWaveSlot()
{
	m_RequiresSFXBank = false;
	m_RequiresLowLatencyWaveSlot = true;

	if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
	{	
		m_RequiresLowLatencyWaveSlot = false;

		if(m_IsFocusVehicle)
		{
			m_RequiresSFXBank = true;

			RequestSFXWaveSlot(ATSTRINGHASH("DLC_XM_PLAYER_JATO_ROCKET_THRUST", 0xF1DFB27E), true);			

			if(!m_SFXWaveSlot)
			{
				return false;
			}
		}
	}	

	if(m_RequiresLowLatencyWaveSlot && !m_LowLatencyWaveSlot)
	{
		RequestLowLatencyWaveSlot(m_HeliAudioSettings->RotorLoop);

		// We're quite limited on low latency slots, so in the event that we can't load the correct bank, default back to something that we can
		if(!m_LowLatencyWaveSlot && sm_LowLatencyWaveSlotManager.GetNumUsedSlots() == sm_LowLatencyWaveSlotManager.GetNumSlots())
		{
			for(u32 loop = 0; loop < sm_LowLatencyWaveSlotManager.GetNumSlots(); loop++)
			{
				const audDynamicWaveSlot* waveSlot = sm_LowLatencyWaveSlotManager.GetWaveSlot(loop);

				if(waveSlot && waveSlot->registeredEntities.GetCount() > 0)
				{
					audVehicleAudioEntity* vehicleEntity = (audVehicleAudioEntity*) waveSlot->registeredEntities[0];
					audAssertf(vehicleEntity, "Registered entity is invalid!");
					
					if(vehicleEntity)
					{
						audAssertf(vehicleEntity->GetAudioVehicleType() == AUD_VEHICLE_HELI, "Registered entity is not a heli! (%d)", vehicleEntity->GetAudioVehicleType());

						if(vehicleEntity->GetAudioVehicleType() == AUD_VEHICLE_HELI)
						{
							HeliAudioSettings* settings = ((audHeliAudioEntity*)vehicleEntity)->GetHeliAudioSettings();

							if(settings)
							{
								audWarningf("Insufficient helicopter slots, reusing already loaded helicopter settings");
								RequestLowLatencyWaveSlot(settings->RotorLoop);

								if(m_LowLatencyWaveSlot)
								{
									m_HeliAudioSettings = settings;
									break;
								}
							}
						}
					}
				}
			}
		}
	}

	if(m_LowLatencyWaveSlot || !m_RequiresLowLatencyWaveSlot)
	{
		if(!m_EngineWaveSlot)
		{
			if(m_HeliAudioSettings->SimpleSoundForLoading != g_NullSoundHash)
			{
				RequestWaveSlot(&audVehicleAudioEntity::sm_StandardWaveSlotManager, m_HeliAudioSettings->SimpleSoundForLoading);
			}
			else
			{
				RequestWaveSlot(&audVehicleAudioEntity::sm_StandardWaveSlotManager, m_HeliAudioSettings->StartUpOneShot);
			}

			// If we didn't acquire a proper wave slot, free up our low latency one so that the two remain in sync
			if(!m_EngineWaveSlot)
			{
				audWaveSlotManager::FreeWaveSlot(m_LowLatencyWaveSlot, this);
				audWaveSlotManager::FreeWaveSlot(m_SFXWaveSlot, this);
			}
		}
	}
	else if(m_RequiresSFXBank)
	{
		audWaveSlotManager::FreeWaveSlot(m_SFXWaveSlot, this);
	}

	return m_EngineWaveSlot != NULL && (!m_RequiresLowLatencyWaveSlot ||m_LowLatencyWaveSlot != NULL) && (!m_RequiresSFXBank || m_SFXWaveSlot != NULL);
}

// ----------------------------------------------------------------
// Get the desired lod for the vehicle
// ----------------------------------------------------------------
audVehicleLOD audHeliAudioEntity::GetDesiredLOD(f32 UNUSED_PARAM(fwdSpeedRatio), u32 distFromListenerSq, bool UNUSED_PARAM(visibleBySniper))
{
	if(m_IsFocusVehicle)
	{
		return AUD_VEHICLE_LOD_REAL;
	}
	else if(RequiresWaveSlot())
	{
		if(distFromListenerSq < (g_HeliActivationDistSq * sm_ActivationRangeScale))
		{
			return AUD_VEHICLE_LOD_REAL;
		}
	}
	
	return distFromListenerSq < g_SuperDummyRadiusSq ? AUD_VEHICLE_LOD_SUPER_DUMMY : AUD_VEHICLE_LOD_DISABLED;
}

// ----------------------------------------------------------------
// ConvertFromDummy
// ----------------------------------------------------------------
void audHeliAudioEntity::ConvertFromDummy()
{
	m_HeliState = AUD_HELI_OFF;
}

// ----------------------------------------------------------------
// ConvertToDisabled
// ----------------------------------------------------------------
void audHeliAudioEntity::ConvertToDisabled()
{
	StopHeliEngineSounds();
}

void audHeliAudioEntity::SetTracker(audSoundInitParams &initParams) 
{
	initParams.TrackEntityPosition = true;
#if __BANK
	if(g_UseDebugHeliTracker)
	{
		initParams.Tracker = &m_DebugTracker;
		initParams.TrackEntityPosition = false;
	}
#endif
}

// ----------------------------------------------------------------
// Initialise our helicopter settings
// ----------------------------------------------------------------
void audHeliAudioEntity::InitHeliSettings()
{
	m_HeliAudioSettings = GetHeliAudioSettings();

	if(m_HeliAudioSettings)
	{
		if(m_Vehicle->GetVehicleType()==VEHICLE_TYPE_HELI || m_Vehicle->GetVehicleType()==VEHICLE_TYPE_AUTOGYRO || m_Vehicle->GetVehicleType()==VEHICLE_TYPE_BLIMP)
		{
			SetupHeliVolumeCones();

			ConditionalWarning(m_BankAngleVolCurve.Init(m_HeliAudioSettings->BankAngleVolumeCurve), "Invalid BankAngleVolumeCurve");
			ConditionalWarning(m_RotorThrottleVolCurve.Init(m_HeliAudioSettings->RotorThrottleVolumeCurve), "Invalid RotorThrottleVolumeCurve");
			ConditionalWarning(m_RotorThrottlePitchCurve.Init(m_HeliAudioSettings->RotorThrottlePitchCurve), "Invalid RotorThrottlePitchCurve");
			ConditionalWarning(m_RearRotorThrottleVolCurve.Init(m_HeliAudioSettings->RearRotorThrottleVolumeCurve), "Invalid RearRotorThrottleVolumeCurve");
			ConditionalWarning(m_ExhaustThrottleVolCurve.Init(m_HeliAudioSettings->ExhaustThrottleVolumeCurve), "Invalid ExhaustThrottleVolumeCurve");
			ConditionalWarning(m_BankThrottleVolumeCurve.Init(m_HeliAudioSettings->BankThrottleVolumeCurve), "Invalid BankThrottleVolumeCurve");
			ConditionalWarning(m_BankThrottlePitchCurve.Init(m_HeliAudioSettings->BankThrottlePitchCurve), "Invalid BankThrottlePitchCurve");
			ConditionalWarning(m_ExhaustThrottlePitchCurve.Init(m_HeliAudioSettings->ExhaustThrottlePitchCurve), "Invalid ExhaustThrottlePitchCurve");
			ConditionalWarning(m_HeliThrottleResonanceCurve1.Init(m_HeliAudioSettings->Filter1ThrottleResonanceCurve), "Invalid Filter1ThrottleResonanceCurve");
			ConditionalWarning(m_HeliThrottleResonanceCurve2.Init(m_HeliAudioSettings->Filter2ThrottleResonanceCurve), "Invalid Filter2ThrottleResonanceCurve");
			ConditionalWarning(m_HeliBankingResonanceCurve.Init(m_HeliAudioSettings->BankingResonanceCurve), "Invalid HeliBankingResonanceCurve");
			ConditionalWarning(m_RotorVolStartupCurve.Init(m_HeliAudioSettings->RotorVolumeStartupCurve), "Invalid RotorVolumeStartupCurve");
			ConditionalWarning(m_BladeVolStartupCurve.Init(m_HeliAudioSettings->BladeVolumeStartupCurve), "Invalid BladeVolumeStartupCurve");
			ConditionalWarning(m_RotorPitchStartupCurve.Init(m_HeliAudioSettings->RotorPitchStartupCurve), "Invalid RotorPitchStartupCurve");
			ConditionalWarning(m_RearRotorVolStartupCurve.Init(m_HeliAudioSettings->RearRotorVolumeStartupCurve), "Invalid RearRotorVolumeStartupCurve");
			ConditionalWarning(m_ExhaustVolStartupCurve.Init(m_HeliAudioSettings->ExhaustVolumeStartupCurve), "Invalid ExhaustVolumeStartupCurve");
			ConditionalWarning(m_RotorSpeedToTriggerSpeedCurve.Init(m_HeliAudioSettings->RotorSpeedToTriggerSpeedCurve), "Invalid RotorSpeedToTriggerSpeedCurve");

			m_ExhaustPitchStartupCurve.Init(m_HeliAudioSettings->ExhaustPitchStartupCurve);
			m_HeliThrottleSmoother.Init(m_HeliAudioSettings->ThrottleSmoothRate, m_HeliAudioSettings->ThrottleSmoothRate, -1.0f, 1.0f);
			m_VehicleModelSoundHash = m_HeliAudioSettings->ScannerModel;
			m_VehicleMakeSoundHash = m_HeliAudioSettings->ScannerMake;
			m_VehicleCategorySoundHash = m_HeliAudioSettings->ScannerCategory;
			m_ScannerVehicleSettingsHash = m_HeliAudioSettings->ScannerVehicleSettings;
			
			ConditionalWarning(m_DamageVolumeCurve.Init(m_HeliAudioSettings->HealthToDamageVolumeCurve), "Invalid HealthToDamageVolumeCurve");
			ConditionalWarning(m_DamageVolumeBelow600Curve.Init(m_HeliAudioSettings->HealthBelow600ToDamageVolumeCurve), "Invalid HealthBelow600ToDamageVolumeCurve");

#if NA_RADIO_ENABLED
			m_AmbientRadioDisabled = (AUD_GET_TRISTATE_VALUE(m_HeliAudioSettings->Flags, FLAG_ID_HELIAUDIOSETTINGS_DISABLEAMBIENTRADIO) == AUD_TRISTATE_TRUE);
			m_RadioType = (RadioType)m_HeliAudioSettings->RadioType;
			m_RadioGenre = (RadioGenre)m_HeliAudioSettings->RadioGenre;
#endif

			m_HasCustomLandingGear = (GetVehicleModelNameHash() == ATSTRINGHASH("AKULA", 0x46699F47) || GetVehicleModelNameHash() == ATSTRINGHASH("Annihilator2", 0x11962E49));
			m_HeliDamageFluctuator.Init(g_HeliDamageRate/1000.0f, g_HeliDamageRate/1000.0f, 0.0f, 0.2f, 0.8f, 1.0f, g_HeliIntraFlipProb/30.0f, g_HeliInterFlipProb/30.0f, 0.0f);

			if(AUD_GET_TRISTATE_VALUE(m_HeliAudioSettings->Flags, FLAG_ID_PLANEAUDIOSETTINGS_HASMISSILELOCKSYSTEM) == AUD_TRISTATE_TRUE)
			{
				m_HasMissileLockWarningSystem = true;
			}
		}
	}
}

// ----------------------------------------------------------------
// Get the helicopter settings data
// ----------------------------------------------------------------
HeliAudioSettings* audHeliAudioEntity::GetHeliAudioSettings()
{
	HeliAudioSettings *settings = NULL;

	if(g_AudioEngine.IsAudioEnabled())
	{
		if(m_ForcedGameObject != 0u)
		{
			settings = audNorthAudioEngine::GetObject<HeliAudioSettings>(m_ForcedGameObject);
			m_ForcedGameObject = 0u;
		}

		if(!settings)
		{
			if(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash() != 0)
			{
				settings = audNorthAudioEngine::GetObject<HeliAudioSettings>(m_Vehicle->GetVehicleModelInfo()->GetAudioNameHash());
			}

			if(!settings)
			{
				settings = audNorthAudioEngine::GetObject<HeliAudioSettings>(GetVehicleModelNameHash());
			}
		}

		if(!audVerifyf(settings, "Couldn't find heli settings for heli: %s", GetVehicleModelName()))
		{
			settings = audNorthAudioEngine::GetObject<HeliAudioSettings>(ATSTRINGHASH("DEFAULT_HELI", 0xA882C8B0));
		}
	}
	return settings;
}

// ----------------------------------------------------------------
// Get the vehicle collision settings
// ----------------------------------------------------------------
VehicleCollisionSettings* audHeliAudioEntity::GetVehicleCollisionSettings() const
{
	if(m_HeliAudioSettings)
	{
		return audNorthAudioEngine::GetObject<VehicleCollisionSettings>(m_HeliAudioSettings->VehicleCollisions);
	}

	return audVehicleAudioEntity::GetVehicleCollisionSettings();
}

// ----------------------------------------------------------------
// Initialise the class
// ----------------------------------------------------------------
void audHeliAudioEntity::InitClass()
{
	StaticConditionalWarning(sm_HeliStartupToNormalCurve.Init(ATSTRINGHASH("HELI_STARTUP_TO_NORMAL", 0x8DCE9DE2)), "Invalid sm_HeliStartupToNormalCurve");
	StaticConditionalWarning(g_HeliMinigunSpinToVol.Init(ATSTRINGHASH("HELI_MINIGUN_SPEED_TO_VOL", 0xED1EB500)), "Invalid minigun speed to vol curve");
	StaticConditionalWarning(g_HeliMinigunSpinToPitch.Init(ATSTRINGHASH("HELI_MINIGUN_SPEED_TO_PITCH", 0xB4EE8A74)), "Invalid minigun speed to pitch curve");
	StaticConditionalWarning(sm_SuspensionVolCurve.Init(ATSTRINGHASH("HELI_SUSPENSION_COMPRESSION_TO_VOL",0xD9991BE)), "Invalid SuspVolCurve");
	StaticConditionalWarning(sm_GritSpeedToLinVolCurve.Init(ATSTRINGHASH("HELI_SPEED_TO_LANDING_GEAR_SCRAPE_VOLUME",0xC02B81D7)), "Invalid GritSpeedCurve");

	audVehicleGrapplingHook::InitClass();
}

// ----------------------------------------------------------------
// Get the leakage for any positional player radio
// ----------------------------------------------------------------
AmbientRadioLeakage audHeliAudioEntity::GetPositionalPlayerRadioLeakage() const
{
	return g_AircraftPositionedPlayerVehicleRadioLeakage;
}

bool audHeliAudioEntity::IsRadioUpgraded() const
{
	return GetVehicleModelNameHash() == ATSTRINGHASH("BLIMP3", 0xEDA4ED97);
}

// ----------------------------------------------------------------
// Init anything helicopter specific
// ----------------------------------------------------------------
bool audHeliAudioEntity::InitVehicleSpecific()
{
	m_MainRotorVolume = g_SilenceVolume;

	m_CrazyFlyingRotorPitchSmoother.Init(20.0f, 50.0f, -1000.0f, 0.0f);
	m_CrazyFlyingDamageSmoother.Init(2.0f, 2.0f, -100.0f, 0.0f);

	InitHeliSettings();

	if(!m_HeliAudioSettings)
	{
		return false;
	}
	m_EngineOffTemperature = 100.0f;

#if 0
	f32 innerMap[5][3] = {	{ 0.0f,  0.0f, 0.0f},
	{ 1.2f,  4.0f, 0.0f},
	{ 1.2f, -8.0f, 0.0f},
	{-1.2f,  4.0f, 0.0f},
	{-1.2f, -8.0f, 0.0f}};

	f32 outerMap[8][3] = {	{ 4.0f,  6.0f,  6.0f},
	{ 4.0f, -12.0f,  6.0f},
	{-4.0f,  6.0f,  6.0f},
	{-4.0f, -12.0f,  6.0f},
	{ 4.0f,  6.0f, -2.0f},
	{ 4.0f, -12.0f, -2.0f},
	{-4.0f,  6.0f, -2.0f},
	{-4.0f, -12.0f, -2.0f}};

	if (m_OcclusionGroup)
	{
		m_OcclusionGroup->SetBounds(innerMap, outerMap);
	}	
#endif
	SetEnvironmentGroupSettings(false, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());

	// GTAV specific fix - should be moved into the gameobject when we get a chance
	if(GetVehicleModelNameHash() == ATSTRINGHASH("SWIFT", 0xEBC24DF2))
	{
		m_LandingGearSoundSet.Init(ATSTRINGHASH("SWIFT_LANDING_GEAR", 0x1CCAA017));
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("SWIFT2", 0x4019CB4C))
	{
		m_LandingGearSoundSet.Init(ATSTRINGHASH("SWIFT2_LANDING_GEAR", 0x55A3666B));
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("SAVAGE", 0xFB133A17))
	{
		m_LandingGearSoundSet.Init(ATSTRINGHASH("SAVAGE_LANDING_GEAR", 0xE1A6253F));
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("VOLATUS", 0x920016F1))
	{
		m_LandingGearSoundSet.Init(ATSTRINGHASH("VOLATUS_LANDING_GEAR", 0x990EA6F3));
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("AKULA", 0x46699F47))
	{
		m_LandingGearSoundSet.Init(ATSTRINGHASH("AKULA_SOUNDS", 0x6CC7612D));
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
	{
		m_LandingGearSoundSet.Init(ATSTRINGHASH("thruster_turbine_sounds", 0x9C4F5C78));
	}
	else if (GetVehicleModelNameHash() == ATSTRINGHASH("Annihilator2", 0x11962E49))
	{
		m_LandingGearSoundSet.Init(ATSTRINGHASH("annihilator2_sounds", 0x7CDB0FE7));
	}

	m_KersSystem.Init(this);
	m_VehicleSpeedToHullSlapVol.Init(ATSTRINGHASH("BOAT_SPEED_TO_HULL_SLAP_VOLUME", 0x7DE7B14A));

	return true;
}

// ----------------------------------------------------------------
// OnFocusVehicleChanged
// ----------------------------------------------------------------
void audHeliAudioEntity::OnFocusVehicleChanged()
{
	if(m_SFXWaveSlot && !m_IsFocusVehicle)
	{
		audWaveSlotManager::FreeWaveSlot(m_SFXWaveSlot, this);
		m_RequiresSFXBank = false;
	}
	else if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30) && m_IsFocusVehicle && m_EngineWaveSlot && !m_SFXWaveSlot)
	{
		RequestSFXWaveSlot(ATSTRINGHASH("DLC_XM_PLAYER_JATO_ROCKET_THRUST", 0xF1DFB27E), true);
		m_RequiresSFXBank = true;
	}
}


// ----------------------------------------------------------------
// Check if the helicopter requires a wave slot
// ----------------------------------------------------------------
bool audHeliAudioEntity::RequiresWaveSlot() const
{
	return ((CHeli*)m_Vehicle)->GetMainRotorSpeed() > 0.0f;
}

// ----------------------------------------------------------------
// Update anything helicopter specific
// ----------------------------------------------------------------
void audHeliAudioEntity::UpdateVehicleSpecific(audVehicleVariables& UNUSED_PARAM(vehicleVariables), audVehicleDSPSettings& dspSettings)
{
#if __BANK
	if (m_HeliAudioSettings)
	{
		m_HeliThrottleSmoother.SetRates(m_HeliAudioSettings->ThrottleSmoothRate, m_HeliAudioSettings->ThrottleSmoothRate);
		m_HeliDamageFluctuator.SetFlipProbabilities(g_HeliIntraFlipProb/30.0f, g_HeliInterFlipProb/30.0f);
		m_HeliDamageFluctuator.SetRates(g_HeliDamageRate/1000.0f, g_HeliDamageRate/1000.0f);
	}
#endif

	if(m_Exploded)
	{
		m_HeliState = AUD_HELI_OFF;
		StopHeliEngineSounds();
		if(m_ShockwaveId != kInvalidShockwaveId)
		{
			audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(m_ShockwaveId);
			m_ShockwaveId = kInvalidShockwaveId;
		}
	}

	if(IsReal() || m_HeliState != AUD_HELI_OFF)
	{
		audHeliValues heliValues;

		// Work out what the engine's doing now, what state we're in, and what we need to do
		bool engineOn = IsEngineOn();

		// See how much we're banking
		// Get the up vector from the Heli, and see it's angle to straight up
		heliValues.banking = Dot(m_Vehicle->GetTransform().GetC(), Vec3V(V_Z_AXIS_WZERO)).Getf();

		m_Throttle = ((CHeli*)m_Vehicle)->GetThrottleControl();
		m_MainRotorSpeed = ((CHeli*)m_Vehicle)->GetMainRotorSpeed();

		f32 entityVariableThrottle = 0.0f;
		f32 entityVariableRevs = 0.0f;
		f32 entityVariableLats = 0.0f;
		f32 fakeEngineFactor = 0.0f;

		if(HasEntityVariableBlock())
		{
			entityVariableThrottle = GetEntityVariableValue(ATSTRINGHASH("fakethrottle", 0xEB27990));
			entityVariableRevs = GetEntityVariableValue(ATSTRINGHASH("fakerevs", 0xCEB98BEB));
			entityVariableLats = GetEntityVariableValue(ATSTRINGHASH("fakelatslip", 0x34DA17A0));

			// Reduce this each frame so that that the behavior gets canceled even if a sound forgets to do so
			fakeEngineFactor = GetEntityVariableValue(ATSTRINGHASH("usefakeengine", 0x91DF7F97));
			SetEntityVariable(ATSTRINGHASH("usefakeengine", 0x91DF7F97), Clamp(fakeEngineFactor - 0.1f, 0.0f, 1.0f));
		}

		if(fakeEngineFactor > 0.1f)
		{
			m_Throttle = entityVariableThrottle;
			heliValues.banking = entityVariableRevs;
			m_MainRotorSpeed = entityVariableLats;
		}

		m_Throttle = Clamp(m_Throttle, 0.0f, 2.0f);
		heliValues.banking = Clamp(heliValues.banking, 0.0f, 1.0f);

		// Transition state and do updates
		switch (m_HeliState)
		{
		case AUD_HELI_OFF:
			if(engineOn)
			{
				if(CReplayMgr::IsEditModeActive() && !CReplayMgr::IsJustPlaying()) // if we are in the editor and doing anything but playing(such as loading) we are not going to try to start the engines, because the LOD system will stop them and then restart
				{
					break;
				}
				if(m_EngineWaveSlot && m_EngineWaveSlot->IsLoaded() && 
				   (!m_RequiresLowLatencyWaveSlot || (m_LowLatencyWaveSlot && m_LowLatencyWaveSlot->IsLoaded())) && 
				   (!m_RequiresSFXBank || (m_SFXWaveSlot && m_SFXWaveSlot->IsLoaded())))
				{
					if (m_HeliShouldSkipStartup) // removed because it was always true preventing scripts from playing the startup sound  || m_WasEngineOnLastFrame)
					{
						// dive straight into being on, then unset this flag so we can be turned off again
#if GTA_REPLAY
						if(CReplayMgr::IsPlaying()) // startup faster on replays
						{
							m_HeliStartupSmoother.Init(1.0f/g_HeliReplayStartupTime, 1.0f/g_HeliWinddownTime, true);
						}
						else
#endif
						{
							m_HeliStartupSmoother.Init(1.0f/g_HeliStartupTime, 1.0f/g_HeliWinddownTime, true);
						}
						heliValues.startup = m_HeliStartupSmoother.CalculateValue(1.0f, fwTimer::GetTimeInMilliseconds());
						StartHeliEngineSounds(heliValues, true); // skip the startup sound. This'll do an Update as part of starting
						m_HeliState = AUD_HELI_ON;
						m_HeliShouldSkipStartup = false;
					}
					else
					{
						heliValues.startup = m_HeliStartupSmoother.CalculateValue(0.0f, fwTimer::GetTimeInMilliseconds());
						if (engineOn)
						{
							m_HeliState = AUD_HELI_STARTING;
							StartHeliEngineSounds(heliValues); 
						}
					}
				}
			}
			break;

		case AUD_HELI_STARTING:
			heliValues.startup = m_HeliStartupSmoother.CalculateValue(1.0f, fwTimer::GetTimeInMilliseconds());
			UpdateHeliEngineSoundStartingStopping(heliValues);
			if (!engineOn)
			{
				m_HeliState = AUD_HELI_STOPPING;

				if(m_StartupSound)
				{
					m_StartupSound->StopAndForget(true);
				}
			}
			else
			{
				if (heliValues.startup >= 1.0f)
				{
					m_HeliState = AUD_HELI_ON;
				}
			}
			if(m_Vehicle && m_Vehicle->GetWillEngineStart())
			{
				m_FailedToStartCount = 0;
				m_LastFailToStartTime = 0;
			}
			break;

		case AUD_HELI_STOPPING:
			if(m_Vehicle->m_nVehicleFlags.bIsDrowning)
				heliValues.startup = m_HeliShutdownWaterSmoother.CalculateValue(m_MainRotorSpeed, fwTimer::GetTimeInMilliseconds());
			else
				heliValues.startup = m_HeliStartupSmoother.CalculateValue(m_MainRotorSpeed, fwTimer::GetTimeInMilliseconds());
			UpdateHeliEngineSoundStartingStopping(heliValues);
			if (engineOn)
			{
				m_HeliState = AUD_HELI_STARTING;
			}
			else
			{
				static bank_float minRotorSpeedForStop = 0.1f;
				if (heliValues.startup <= minRotorSpeedForStop)
				{
					m_HeliState = AUD_HELI_OFF;
					StopHeliEngineSounds();
				}
			}

			if(m_ShockwaveId != kInvalidShockwaveId)
			{
				audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(m_ShockwaveId);
				m_ShockwaveId = kInvalidShockwaveId;
			}
			break;

		case AUD_HELI_ON:
			heliValues.startup = m_HeliStartupSmoother.CalculateValue(1.0f, fwTimer::GetTimeInMilliseconds());
			UpdateHeliEngineSoundsOn(heliValues);
			UpdateCrazyFlying(heliValues);

			if (!engineOn)
			{
				m_HeliState = AUD_HELI_STOPPING;

				if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
				{					
					audSoundSet soundset;
					const u32 soundsetName = ATSTRINGHASH("thruster_turbine_sounds", 0x9C4F5C78);
					const u32 soundName = ATSTRINGHASH("shutdown", 0xE99B0DCB);

					if(soundset.Init(soundsetName))
					{
						audSoundInitParams initParams;
						initParams.UpdateEntity = true;
						initParams.TrackEntityPosition = true;
						initParams.EnvironmentGroup = GetEnvironmentGroup();
						initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_UNKNOWN;
						CreateAndPlaySound(soundset.Find(soundName), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));
					}
				}	
			}

			break;
		}
		
		if (m_HeliState != AUD_HELI_OFF)
		{
			UpdateHeliEngineSounds(heliValues);		
			f32 resonanceScalingFactor = m_HeliBankingResonanceCurve.CalculateValue(heliValues.banking);
			f32 resonance1 = audDriverUtil::ComputeLinearVolumeFromDb(m_HeliThrottleResonanceCurve1.CalculateValue(heliValues.throttle) * resonanceScalingFactor);
			f32 resonance2 = audDriverUtil::ComputeLinearVolumeFromDb(m_HeliThrottleResonanceCurve2.CalculateValue(heliValues.throttle) * resonanceScalingFactor);

			dspSettings.AddDSPParameter(atHashString("Throttle", 0xEA0151DC), heliValues.throttle);
			dspSettings.AddDSPParameter(atHashString("Banking", 0x57F8C1F8), heliValues.banking);
			dspSettings.AddDSPParameter(atHashString("RotorSpeed", 0x4F4CEFD4), m_MainRotorSpeed);
			dspSettings.AddDSPParameter(atHashString("Filter1Resonance", 0x96A1524), resonance1);
			dspSettings.AddDSPParameter(atHashString("Filter2Resonance", 0x7FFE9BC7), resonance2);
			dspSettings.enginePostSubmixAttenuation = m_HeliAudioSettings->RotorVolumePostSubmix/100.f;
		}

	#if __DEV
		if (g_DebugHeli && g_AudioDebugEntity == m_Vehicle)
		{
			char heliDebug[128] = "";
			formatf(heliDebug, sizeof(heliDebug), "state: %d; startup: %.2f; bVol: %.2f, throttle: %f", m_HeliState, heliValues.startup, heliValues.bladeVolume, m_Throttle);
			grcDebugDraw::PrintToScreenCoors(heliDebug,20,20);
		}
	#endif

		if(m_LastHeliDownwashTime + 300 < fwTimer::GetTimeInMilliseconds() && m_HeliDownwash)
		{
			m_HeliDownwash->StopAndForget();
		}

		// altitude warning tone
		if(!m_AltitudeWarning)
		{
			CreateAndPlaySound_Persistent(m_HeliAudioSettings->AltitudeWarning, &m_AltitudeWarning);
		}

		if(m_AltitudeWarning)
		{
			f32 rotorHealth = ((CHeli*)m_Vehicle)->GetMainRotorHealth();
			f32 rearRotorHealth = ((CHeli*)m_Vehicle)->GetRearRotorHealth();
			f32 tailBoomHealth = ((CHeli*)m_Vehicle)->GetTailBoomHealth();
			f32 overallhealth = m_Vehicle->GetHealth();
			f32 health = Min(rotorHealth, rearRotorHealth, tailBoomHealth, overallhealth);

			//if( !m_Vehicle->m_nVehicleFlags.bEngineOn || 
			//	CGameWorld::FindLocalPlayerVehicle() != m_Vehicle || 
			//	health > g_MinHealthForHeliWarning || 
			//	(CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->IsDead()))
			f32 airSpeedZ = m_CachedVehicleVelocity.z;

			static bank_float verticalSpeed = -20.0f;
			if( CGameWorld::FindLocalPlayerVehicle() != m_Vehicle || 
				health > g_MinHealthForHeliWarning || 
				!AreWarningSoundsValid() ||
				(CGameWorld::FindLocalPlayer() && CGameWorld::FindLocalPlayer()->IsDead()))
			{
				m_AltitudeWarning->SetRequestedVolume(g_SilenceVolume);
			}
			else
			{
				m_AltitudeWarning->SetRequestedVolume(0.f);
				if(airSpeedZ < verticalSpeed && ((rotorHealth < g_MinRotorHealthForHeliGoingDown) || (!m_Vehicle->m_nVehicleFlags.bEngineOn)))
					TriggerGoingDownSound();
			}
		}

		static bank_float mainRotorSpeedForDamageWarning = 0.3f;
 		if(m_IsPlayerVehicle && m_Vehicle->GetVehicleDamage()->GetEngineHealth() < 200.0f && m_Vehicle->GetStatus() != STATUS_WRECKED && !m_HeliDamageWarning && m_MainRotorSpeed >= mainRotorSpeedForDamageWarning && AreWarningSoundsValid())
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			CreateAndPlaySound_Persistent(m_HeliAudioSettings->DamageWarning, &m_HeliDamageWarning, &initParams);
		}
		else if(m_HeliDamageWarning && (m_MainRotorSpeed < mainRotorSpeedForDamageWarning || m_Vehicle->GetStatus() == STATUS_WRECKED || !m_IsPlayerVehicle || m_Vehicle->GetVehicleDamage()->GetEngineHealth() > 200.0f || !AreWarningSoundsValid()))
		{
			m_HeliDamageWarning->StopAndForget();
		}
		UpdateSuspension();
		UpdateWheelSounds();
		UpdateDamageSounds(heliValues);
		UpdateRotorImpactSounds(heliValues);
		m_KersSystem.Update();
		UpdateCustomLandingGearSounds();
		UpdateJetPackSounds();

#if __BANK
		ShowHeliValues(heliValues);
#endif
	}
	else
	{
		StopAndForgetSounds(m_ScrapeSound, m_HardScrapeSound, m_HeliDownwash, m_HeliDamageWarning, m_RotorImpactSound, m_CrazyWarning, m_CustomLandingGearLoop);
		m_KersSystem.StopAllSounds();
	}

	if (IsSeaPlane())
	{
		if (!IsDisabled())
		{
			UpdateSeaPlane();
		}
	}

	if(m_PlayGoingDown)
	{
		PlayGoingDownSound();
	}
	if(!m_HeliGoingDown)
	{
		// In replay playback, the heli owns the wave slot reference but not the sound, so allow a little leeway between allocating the slot and freeing it so
		// that the sound has a chance to trigger and bump up the slot reference count (which will automatically prevent anything else from stealing the slot
		// whilst it is playing.
		REPLAY_ONLY(if(!CReplayMgr::IsEditorActive() || (g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0) - m_LastReplaySpeechSlotAllocTime) > 1000))
		{
			FreeSpeechSlot();
		}
	}
	if(m_Vehicle->GetStatus() == STATUS_WRECKED)
	{
		if(m_CrazyWarning)
		{
			m_CrazyWarning->StopAndForget();
		}
	}
	UpdateEngineCooling();

	SetEnvironmentGroupSettings(false, audNorthAudioEngine::GetOcclusionManager()->GetMaxOcclusionPathDepth());

#if __BANK
	UpdateDebug();
#endif
}

void audHeliAudioEntity::PostUpdate()
{
	audVehicleAudioEntity::PostUpdate();
	UpdateShockwave();
}

bool audHeliAudioEntity::CalculateIsInWater() const
{
	if (IsSeaPlane())
	{
		return m_Vehicle->m_Buoyancy.GetSubmergedLevel() >= 0.05f;
	}

	return audVehicleAudioEntity::CalculateIsInWater();
}

u32 audHeliAudioEntity::GetWaveHitSound() const 
{ 
	if (IsSeaPlane())
	{
		return ATSTRINGHASH("BOAT_WAVE_HIT_MEDIUM", 0x5D9D2B9B);
	}

	return audVehicleAudioEntity::GetWaveHitSound();
}

u32 audHeliAudioEntity::GetWaveHitBigSound() const
{ 
	if (IsSeaPlane())
	{
		return ATSTRINGHASH("BOAT_WAVE_HIT_BIG_AIR", 0x73E4FA98);
	}

	return audVehicleAudioEntity::GetWaveHitSound();
}

u32 audHeliAudioEntity::GetIdleHullSlapSound() const
{
	if (IsSeaPlane())
	{
		return ATSTRINGHASH("IDLE_HULL_SLAP", 0xFFFF62BC);
	}

	return audVehicleAudioEntity::GetIdleHullSlapSound();
}

f32 audHeliAudioEntity::GetIdleHullSlapVolumeLinear(f32 speed) const
{
	return m_VehicleSpeedToHullSlapVol.CalculateValue(speed);
}

void audHeliAudioEntity::UpdateSeaPlane()
{
	if (CalculateIsInWater() && GetVehicleModelNameHash() == ATSTRINGHASH("SEASPARROW", 0xD4AE63D9))
	{		
		if (!m_WaterTurbulenceSound)
		{
			audSoundInitParams initParams;
			initParams.UpdateEntity = true;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			SetTracker(initParams);
			CreateAndPlaySound_Persistent(ATSTRINGHASH("seasparrow_water_turbulence", 0x4C4922), &m_WaterTurbulenceSound, &initParams);
		}
	}
	else if (m_WaterTurbulenceSound)
	{
		m_WaterTurbulenceSound->StopAndForget();
	}
}

void audHeliAudioEntity::UpdateShockwave()
{
	bool shockwaveValid = false;
	CPed* pPlayer = CGameWorld::FindLocalPlayer();	

	if(m_Vehicle && pPlayer)
	{
		u32 distFromListenerSq = static_cast<u32>(MagSquared(m_Vehicle->GetTransform().GetPosition() - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()).Getf());
		bool shouldTriggerShockwave = m_Vehicle->GetIsInExterior() && pPlayer->GetIsInExterior();
		shouldTriggerShockwave = shouldTriggerShockwave || (m_Vehicle->GetIsInInterior() && pPlayer && pPlayer->GetIsInInterior() && m_Vehicle->GetAudioInteriorLocation().IsSameLocation(pPlayer->GetAudioInteriorLocation()));
		shouldTriggerShockwave &= (IsReal() && m_HeliState == AUD_HELI_ON);
		
		if(shouldTriggerShockwave && distFromListenerSq < (120.f*120.f))
		{
			audShockwave shockwave;
			shockwave.centre = VEC3V_TO_VECTOR3(m_Vehicle->GetTransform().GetPosition()) + Vector3(0.f,0.f,3.5f);
			shockwave.maxRadius = shockwave.radius = naEnvironment::sm_HeliShockwaveRadius;
			shockwave.intensity = 1.f;
			shockwave.centered = true;
			audNorthAudioEngine::GetGtaEnvironment()->UpdateShockwave(m_ShockwaveId, shockwave);
			shockwaveValid = true;
		}
	}

	if(!shockwaveValid && m_ShockwaveId != kInvalidShockwaveId)
	{
		audNorthAudioEngine::GetGtaEnvironment()->FreeShockwave(m_ShockwaveId);
		m_ShockwaveId = kInvalidShockwaveId;
	}
}			

void audHeliAudioEntity::UpdateCrazyFlying(audHeliValues& heliValues)
{
	if(m_CrazyWarning && m_HeliDamageWarning)
	{
		m_CrazyWarning->StopAndForget();
	}

#if __BANK
	if(g_EnableCrazyHeliSounds)
		m_EnableCrazyHeliSounds = true;
#endif

	if(!m_EnableCrazyHeliSounds)
	{
		if(m_CrazyWarning)
		{
			m_CrazyWarning->StopAndForget();
		}
		return;
	}

	static bank_float bank = 0.9f;
	if(heliValues.banking < bank)
	{
		if(m_IsPlayerVehicle && !m_CrazyWarning && !m_HeliDamageWarning)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			CreateAndPlaySound_Persistent(m_HeliAudioSettings->DamageWarning, &m_CrazyWarning, &initParams);
		}

		static bank_float smooth = -1000.0f;
		heliValues.rotorPitch += (s32)m_CrazyFlyingRotorPitchSmoother.CalculateValue(smooth);
		heliValues.damageVolume = m_CrazyFlyingDamageSmoother.CalculateValue(0.0f);
	}
	else 
	{
		if(m_CrazyWarning)
			m_CrazyWarning->StopAndForget();

		heliValues.rotorPitch += (s32)m_CrazyFlyingRotorPitchSmoother.CalculateValue(0.0f);
		heliValues.damageVolume = m_CrazyFlyingDamageSmoother.CalculateValue(-100.0f);
	}



}


// ----------------------------------------------------------------
// audHeliAudioEntity SetHeliShouldSkipStartup
// ----------------------------------------------------------------
void audHeliAudioEntity::SetHeliShouldSkipStartup(bool skipStartup)
{
	// don't set this to if we're in anything other than a stopped state - it's almost certainly not what's intended
	if (m_HeliState == AUD_HELI_OFF)
	{
		m_HeliShouldSkipStartup = skipStartup;
	}
}


// -------------------------------------------------------------------------------
// Get the engine submix synth def
// -------------------------------------------------------------------------------
u32 audHeliAudioEntity::GetEngineSubmixSynth() const
{
	if(ShouldUseDSPEffects() && m_HeliAudioSettings)
	{
		return m_HeliAudioSettings->EngineSynthDef;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixSynth();
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix synth preset
// -------------------------------------------------------------------------------
u32 audHeliAudioEntity::GetEngineSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects() && m_HeliAudioSettings)
	{
		return m_HeliAudioSettings->EngineSynthPreset;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixSynthPreset();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth def
// -------------------------------------------------------------------------------
u32 audHeliAudioEntity::GetExhaustSubmixSynth() const
{
	if(ShouldUseDSPEffects() && m_HeliAudioSettings)
	{
		return m_HeliAudioSettings->ExhaustSynthDef;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixSynth();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix synth preset
// -------------------------------------------------------------------------------
u32 audHeliAudioEntity::GetExhaustSubmixSynthPreset() const
{
	if(ShouldUseDSPEffects() && m_HeliAudioSettings)
	{
		return m_HeliAudioSettings->ExhaustSynthPreset;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixSynthPreset();
	}
}

// -------------------------------------------------------------------------------
// Get the engine submix voice
// -------------------------------------------------------------------------------
u32 audHeliAudioEntity::GetEngineSubmixVoice() const			
{
	if(m_HeliAudioSettings)
	{
		return m_HeliAudioSettings->EngineSubmixVoice;
	}
	else
	{
		return audVehicleAudioEntity::GetEngineSubmixVoice();
	}
}

// -------------------------------------------------------------------------------
// Get the exhaust submix voice
// -------------------------------------------------------------------------------
u32 audHeliAudioEntity::GetExhaustSubmixVoice() const			
{
	if(m_HeliAudioSettings)
	{
		return m_HeliAudioSettings->ExhaustSubmixVoice;
	}
	else
	{
		return audVehicleAudioEntity::GetExhaustSubmixVoice();
	}
}

// ----------------------------------------------------------------
// audHeliAudioEntity StartHeliEngineSounds
// ----------------------------------------------------------------
void audHeliAudioEntity::StartHeliEngineSounds(audHeliValues& heliValues, bool instantStart)
{
	if (m_HeliBank || m_HeliExhaust || m_HeliRearRotor || m_HeliRotor || m_HeliRotorLowFreq || m_DistantRotor)
	{
		naErrorf("Can't start new heli sounds when they're already playing");
		return;
	}

	if (m_HeliAudioSettings)
	{
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		SetTracker(initParams);
		if(instantStart)// REPLAY_ONLY( && !CReplayMgr::IsPlaying()))
		{
			if(CReplayMgr::IsPlaying())
			{
				// if we've skipped startup, give them a 500ms attack time, otherwise they chop in
				initParams.AttackTime = 300;
			}
			else
			{
				// if we've skipped startup, give them a 2 second attack time, otherwise they chop in
				initParams.AttackTime = sm_LodSwitchFadeTime;
			}
		}
		if((m_Vehicle->m_nVehicleFlags.bCreatedByFactory && m_Vehicle->m_nVehicleFlags.bSkipEngineStartup REPLAY_ONLY( && !CReplayMgr::IsPlaying())) || m_WasScheduledCreation)
		{
			initParams.AttackTime = sm_ScheduledVehicleSpawnFadeTime;
		}
		if(m_Vehicle->m_nVehicleFlags.bSkipEngineStartup)
		{
			instantStart = true;
		}
		CreateSound_PersistentReference(m_HeliAudioSettings->BankingLoop, &m_HeliBank, &initParams);
		CreateSound_PersistentReference(m_HeliAudioSettings->ExhaustLoop, &m_HeliExhaust, &initParams);
		CreateSound_PersistentReference(m_HeliAudioSettings->RotorLowFreqLoop, &m_HeliRotorLowFreq, &initParams);
		CreateSound_PersistentReference(m_HeliAudioSettings->DistantLoop, &m_DistantRotor, &initParams);

		Assign(initParams.EffectRoute, GetEngineEffectRoute());
		CreateSound_PersistentReference(m_HeliAudioSettings->RotorLoop, &m_HeliRotor, &initParams);
		initParams.EffectRoute = 0;

		// Rear rotor doesn't get a tracker
#if __BANK
		if(g_UseDebugHeliTracker)
			SetTracker(initParams);
		else
#endif
		{
			initParams.Tracker = NULL;
			initParams.TrackEntityPosition = false;
		}
		CreateSound_PersistentReference(m_HeliAudioSettings->RearRotorLoop, &m_HeliRearRotor, &initParams);

		if(instantStart)
		{
			UpdateHeliEngineSoundsOn(heliValues);
		}
		else
		{
			UpdateHeliEngineSoundStartingStopping(heliValues);
		}

		if (m_HeliBank)
		{
			m_HeliBank->SetReleaseTime(g_audVehicleLoopReleaseTime);
			m_HeliBank->PrepareAndPlay();
		}

		if (m_HeliExhaust)
		{
			m_HeliExhaust->SetReleaseTime(g_audVehicleLoopReleaseTime);
			m_HeliExhaust->PrepareAndPlay();
		}

		if (m_HeliRearRotor)
		{
			m_HeliRearRotor->SetReleaseTime(g_audVehicleLoopReleaseTime);
			m_HeliRearRotor->PrepareAndPlay();
		}

		if (m_HeliRotor)
		{
			m_HeliRotor->SetReleaseTime(g_audVehicleLoopReleaseTime);
			m_HeliRotor->PrepareAndPlay();
		}

		if(m_HeliRotorLowFreq)
		{
			m_HeliRotorLowFreq->SetReleaseTime(g_audVehicleLoopReleaseTime);
			m_HeliRotorLowFreq->PrepareAndPlay();
		}

		if (m_DistantRotor)
		{
			m_DistantRotor->SetReleaseTime(g_audVehicleLoopReleaseTime);
			m_DistantRotor->PrepareAndPlay();
		}


		// Kick off our one-shot startup sound, unless we're instant starting
		if (!instantStart)
		{
			if(m_StartupSound)
			{
				m_StartupSound->StopAndForget();
			}

			audSoundInitParams initParams;
			SetTracker(initParams);
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			CreateAndPlaySound_Persistent(m_HeliAudioSettings->StartUpOneShot, &m_StartupSound, &initParams);
		}

		m_PitchShiftIndex = 0;
		if(!m_IsPlayerVehicle)
		{
			sm_PitchShiftIndex = (sm_PitchShiftIndex + 1) % NUM_NPC_PICH_SHIFTS;
			m_PitchShiftIndex = sm_PitchShiftIndex;
		}
		
		m_EngineEffectWetSmoother.Reset();
		m_EngineEffectWetSmoother.CalculateValue(0.0f,fwTimer::GetTimeStep());
		UpdateHeliEngineSounds(heliValues);
	}
}


void audHeliAudioEntity::UpdateDamageSounds(audHeliValues& heliValues)
{
	if (m_HeliAudioSettings && (m_IsPlayerVehicle || m_EnableCrazyHeliSounds) && m_HeliState != AUD_HELI_OFF)
	{
		f32 engineHealth = m_Vehicle->GetVehicleDamage()->GetEngineHealth();
		if(!m_HeliDamage && (m_EnableCrazyHeliSounds || (engineHealth < 950.0f && engineHealth > 600.0f)) )
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			SetTracker(initParams);

			CreateSound_PersistentReference(m_HeliAudioSettings->DamageLoop, &m_HeliDamage, &initParams);

			if(m_HeliDamage)
			{
				m_HeliDamage->SetReleaseTime(g_audVehicleLoopReleaseTime);
				m_HeliDamage->PrepareAndPlay();
				m_HeliDamage->SetRequestedVolume(heliValues.damageVolume);
			}
			
			if(m_HeliDamageBelow600)
			{
				m_HeliDamageBelow600->StopAndForget();
			}
			if (m_HeliDamageOneShots)
			{
				m_HeliDamageOneShots->StopAndForget();
			}
		}
		if(engineHealth <= 600.0f)
		{
			if(!m_HeliDamageBelow600)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				SetTracker(initParams);

				CreateSound_PersistentReference(m_HeliAudioSettings->DamageBelow600Loop, &m_HeliDamageBelow600, &initParams);

				if(m_HeliDamageBelow600)
				{
					m_HeliDamageBelow600->SetReleaseTime(g_audVehicleLoopReleaseTime);
					m_HeliDamageBelow600->PrepareAndPlay();
					m_HeliDamageBelow600->SetRequestedVolume(heliValues.damageVolumeBelow600);
				}
				if(m_HeliDamage)
				{
					m_HeliDamage->StopAndForget();
				}
			}

			if(!m_HeliDamageOneShots)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				SetTracker(initParams);

				CreateSound_PersistentReference(m_HeliAudioSettings->DamageOneShots, &m_HeliDamageOneShots, &initParams);

				if (m_HeliDamageOneShots)
				{
					m_HeliDamageOneShots->SetReleaseTime(g_audVehicleLoopReleaseTime);
					m_HeliDamageOneShots->PrepareAndPlay();
					m_HeliDamageOneShots->SetRequestedVolume(heliValues.damageVolumeBelow600);
				}
			}
		}
	}
	else 
	{
		if(m_HeliDamage)
			m_HeliDamage->StopAndForget();
		if(m_HeliDamageBelow600)
			m_HeliDamageBelow600->StopAndForget();
		if(m_HeliDamageOneShots)
			m_HeliDamageOneShots->StopAndForget();
	}

	u32 currentTime = fwTimer::GetTimeInMilliseconds();
	CHeli* heli = static_cast<CHeli*>(m_Vehicle);
	if(m_HeliState != AUD_HELI_OFF && heli->m_Transmission.GetCurrentlyMissFiring() && currentTime > (m_BackFireTime + 2000))
	{
		TriggerBackFire();
	}

}

void audHeliAudioEntity::TriggerBackFire()
{
	if(m_HeliAudioSettings == NULL)
		return;

	audSoundInitParams initParams;
	initParams.EnvironmentGroup = m_EnvironmentGroup;
	SetTracker(initParams);

	if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
	{
		CreateAndPlaySound(ATSTRINGHASH("thruster_missfire", 0xE9ED2B72), &initParams);
	}
	else
	{
		CreateAndPlaySound(ATSTRINGHASH("HELICOPTER_MISSFIRE", 0xE9062F56), &initParams);
	}

	m_BackFireTime = fwTimer::GetTimeInMilliseconds();
}


bool audHeliAudioEntity::IsEngineOn() const
{
	bool engineOn = m_Vehicle->m_nVehicleFlags.bEngineOn;

	if(m_Vehicle->GetStatus() == STATUS_WRECKED)
	{
		engineOn = false;
	}

	return engineOn;
}
// ----------------------------------------------------------------
// audHeliAudioEntity UpdateHeliEngineSounds
// ----------------------------------------------------------------
void audHeliAudioEntity::UpdateHeliEngineSounds(audHeliValues& heliValues)
{
#if __BANK
	SetupHeliVolumeCones();
#endif

	s32 slowMoRotorPitch = 0;

	s16 pitchShift = sm_PitchShift[m_PitchShiftIndex];
#if __BANK
	if(m_IsPlayerVehicle && g_PitchShiftIndex != -1)
	{
		pitchShift = sm_PitchShift[g_PitchShiftIndex];
	}
#endif

	// ensure there is always some predelay on the blades when in slow mo
	if(fwTimer::GetTimeWarp()<1.f || audNorthAudioEngine::IsInSlowMoVideoEditor())
	{
		slowMoRotorPitch = g_HeliSlowMoPitch;
	}

	f32 slowmoVolume = UpdateSlowmoRotorSounds();
	
	// Set the position of the rear rotor
	CVehicleModelInfo* modelInfo = (CVehicleModelInfo *)m_Vehicle->GetBaseModelInfo();
	audAssert(modelInfo);
	
	// check for broken-off rotors
	f32 brokenRotorVolume = 0.0f;
	f32 rearRotorBrokenVolume = 0.0f;
	if (((CHeli*)m_Vehicle)->GetMainRotorHealth()<=0.0f)
	{
		brokenRotorVolume = g_SilenceVolume;
	}
	if (((CHeli*)m_Vehicle)->GetRearRotorHealth()<=0.0f || ((CHeli*)m_Vehicle)->GetTailBoomHealth()<=0.0f)
	{
		rearRotorBrokenVolume = g_SilenceVolume;
	}
	if(((CHeli*)m_Vehicle)->GetIsCargobob())
	{
		if(brokenRotorVolume == g_SilenceVolume && rearRotorBrokenVolume != g_SilenceVolume)
		{
			brokenRotorVolume = 0.0f;
		}
	}

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

	static s32 heliPitchClamp = -600;

	f32 timewarp = fwTimer::GetTimeWarpActive();
	if(timewarp > 0.0f)
	{
		timewarp = 1.0f/timewarp;
	}

	f32 onlyReplayWarp = 1.0f;
	f32 onlyReplayLFFilter = 23999.0f;
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		timewarp = CReplayMgr::GetMarkerSpeed();
		if(timewarp < 0.36f)
		{
			onlyReplayLFFilter = 600.0f;
		}
		else if(timewarp <= 0.55f)
		{
			onlyReplayLFFilter = 1200.0f;
		}
		static const audThreePointPiecewiseLinearCurve replayTimeScaleCurve(0.05f, 0.175f, 0.475f, 0.55f, 2.0f, 2.0f);
		timewarp = replayTimeScaleCurve.CalculateValue(timewarp);
		timewarp = 1.0f/timewarp;
		onlyReplayWarp = timewarp;
		
		BANK_ONLY( if(g_HeliLowPassFilter < 23000.0f) onlyReplayLFFilter = g_HeliLowPassFilter; )
		BANK_ONLY( if(g_HeliRotorTriggerRate != 1.0f) onlyReplayWarp = g_HeliRotorTriggerRate; )
	}
#endif
	if(m_HeliRotor)
	{
		if(onlyReplayLFFilter < 23000.0f)
		{
			m_HeliRotor->SetRequestedLPFCutoff(onlyReplayLFFilter);
		}
		m_HeliRotor->SetRequestedVolume(brokenRotorVolume + (m_HeliRotorSlowmo? slowmoVolume : 0.0f));
		m_HeliRotor->SetRequestedPitch(Max(heliPitchClamp, heliValues.rotorPitch+slowMoRotorPitch+pitchShift));
		m_HeliRotor->FindAndSetVariableValue(ATSTRINGHASH("triggerRate", 0x38F36A3D), heliValues.triggerRate*onlyReplayWarp);
		m_HeliRotor->FindAndSetVariableValue(ATSTRINGHASH("ROTOR_VOLUME", 0x43824A32), heliValues.rotorVolume);
		m_HeliRotor->FindAndSetVariableValue(ATSTRINGHASH("BLADE_VOLUME", 0x5785CE8E), heliValues.bladeVolume);
		m_HeliRotor->FindAndSetVariableValue(ATSTRINGHASH("BankingAngle", 0xD6BB74F6), heliValues.banking);
		m_HeliRotor->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), heliValues.throttle);
	}

	if(m_HeliRotorSlowmo)
	{
		m_HeliRotorSlowmo->SetRequestedVolume(brokenRotorVolume);
		m_HeliRotorSlowmo->SetRequestedPitch(Max(heliPitchClamp, heliValues.rotorPitch+pitchShift));
		m_HeliRotorSlowmo->FindAndSetVariableValue(ATSTRINGHASH("triggerRate", 0x38F36A3D), heliValues.triggerRate*timewarp);
		m_HeliRotorSlowmo->FindAndSetVariableValue(ATSTRINGHASH("ROTOR_VOLUME", 0x43824A32), heliValues.rotorVolume);
		m_HeliRotorSlowmo->FindAndSetVariableValue(ATSTRINGHASH("BLADE_VOLUME", 0x5785CE8E), heliValues.bladeVolume);
		m_HeliRotorSlowmo->FindAndSetVariableValue(ATSTRINGHASH("BankingAngle", 0xD6BB74F6), heliValues.banking);
		m_HeliRotorSlowmo->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), heliValues.throttle);
	}

	if(m_HeliRotorLowFreq)
	{
		m_HeliRotorLowFreq->SetRequestedVolume(brokenRotorVolume + (m_HeliRotorLowFreqSlowmo? slowmoVolume : 0.0f));
		m_HeliRotorLowFreq->SetRequestedPitch(Max(heliPitchClamp, heliValues.rotorPitch+slowMoRotorPitch+pitchShift));
		m_HeliRotorLowFreq->FindAndSetVariableValue(ATSTRINGHASH("triggerRate", 0x38F36A3D), heliValues.triggerRate*onlyReplayWarp);
		m_HeliRotorLowFreq->FindAndSetVariableValue(ATSTRINGHASH("ROTOR_VOLUME", 0x43824A32), heliValues.rotorVolume);
		m_HeliRotorLowFreq->FindAndSetVariableValue(ATSTRINGHASH("BLADE_VOLUME", 0x5785CE8E), heliValues.bladeVolume);
		m_HeliRotorLowFreq->FindAndSetVariableValue(ATSTRINGHASH("BankingAngle", 0xD6BB74F6), heliValues.banking);
		m_HeliRotorLowFreq->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), heliValues.throttle);
	}

	if(m_HeliRotorLowFreqSlowmo)
	{
		m_HeliRotorLowFreqSlowmo->SetRequestedVolume(brokenRotorVolume);
		m_HeliRotorLowFreqSlowmo->SetRequestedPitch(Max(heliPitchClamp, heliValues.rotorPitch+pitchShift));
		m_HeliRotorLowFreqSlowmo->FindAndSetVariableValue(ATSTRINGHASH("triggerRate", 0x38F36A3D), heliValues.triggerRate*timewarp);
		m_HeliRotorLowFreqSlowmo->FindAndSetVariableValue(ATSTRINGHASH("ROTOR_VOLUME", 0x43824A32), heliValues.rotorVolume);
		m_HeliRotorLowFreqSlowmo->FindAndSetVariableValue(ATSTRINGHASH("BLADE_VOLUME", 0x5785CE8E), heliValues.bladeVolume);
		m_HeliRotorLowFreqSlowmo->FindAndSetVariableValue(ATSTRINGHASH("BankingAngle", 0xD6BB74F6), heliValues.banking);
		m_HeliRotorLowFreqSlowmo->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), heliValues.throttle);
	}

	if(m_DistantRotor)
	{
		m_DistantRotor->SetRequestedVolume(brokenRotorVolume);
		m_DistantRotor->SetRequestedPitch(Max(heliPitchClamp, heliValues.rotorPitch+slowMoRotorPitch+pitchShift));
		m_DistantRotor->FindAndSetVariableValue(ATSTRINGHASH("triggerRate", 0x38F36A3D), heliValues.triggerRate*timewarp);
		m_DistantRotor->FindAndSetVariableValue(ATSTRINGHASH("ROTOR_VOLUME", 0x43824A32), heliValues.rotorVolume);
		m_DistantRotor->FindAndSetVariableValue(ATSTRINGHASH("BLADE_VOLUME", 0x5785CE8E), heliValues.bladeVolume);
		m_DistantRotor->FindAndSetVariableValue(ATSTRINGHASH("BankingAngle", 0xD6BB74F6), heliValues.banking);
		m_DistantRotor->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), heliValues.throttle);
	}


	if(m_HeliRearRotor)
	{
		m_HeliRearRotor->SetRequestedVolume(heliValues.rearRotorVolume + rearRotorBrokenVolume);

BANK_ONLY(if(!g_UseDebugHeliTracker))
		{
			s32 rearRotorBoneIndex = modelInfo->GetBoneIndex(HELI_ROTOR_REAR);
			if(rearRotorBoneIndex > -1)
			{
				Matrix34 rearRotorMatrix = RCC_MATRIX34(m_Vehicle->GetSkeletonData().GetDefaultTransform(rearRotorBoneIndex));
				rearRotorMatrix.d = m_Vehicle->TransformIntoWorldSpace(rearRotorMatrix.d);
				m_HeliRearRotor->SetRequestedPosition(rearRotorMatrix.d);
			}
		}
	}

	if(m_HeliExhaust)
	{
		m_HeliExhaust->SetRequestedVolume(heliValues.exhaustVolume);
		m_HeliExhaust->SetRequestedPitch(heliValues.exhaustPitch + pitchShift);
		m_HeliExhaust->FindAndSetVariableValue(ATSTRINGHASH("BankingAngle", 0xD6BB74F6), heliValues.banking);
		m_HeliExhaust->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), heliValues.throttle);

		if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
		{
			f32 rollAngle = m_Vehicle->GetTransform().GetRoll() * RtoD;
			f32 pitchAngle = AcosfSafe(Dot(m_Vehicle->GetTransform().GetB(), Vec3V(V_Z_AXIS_WZERO)).Getf()) * RtoD;
			f32 yawAngle = m_Vehicle->GetTransform().GetHeading() * RtoD;
			f32 angularVelocity = GetCachedAngularVelocity().Mag();
			m_HeliExhaust->FindAndSetVariableValue(ATSTRINGHASH("throttleInput", 0x918028C4), heliValues.throttleInput);
			m_HeliExhaust->FindAndSetVariableValue(ATSTRINGHASH("rollAngle", 0x94FC4D71), rollAngle);
			m_HeliExhaust->FindAndSetVariableValue(ATSTRINGHASH("pitchAngle", 0x809A226D), pitchAngle);
			m_HeliExhaust->FindAndSetVariableValue(ATSTRINGHASH("yawAngle", 0x15DE02B1), yawAngle);
			m_HeliExhaust->FindAndSetVariableValue(ATSTRINGHASH("angularVelocity", 0xC90EACC2), angularVelocity);
		}
	}

	if(m_HeliBank)
	{
		m_HeliBank->SetRequestedVolume(heliValues.bankingVolume);
		m_HeliBank->SetRequestedPitch(heliValues.bankingPitch + pitchShift);
		m_HeliBank->FindAndSetVariableValue(ATSTRINGHASH("throttle", 0xEA0151DC), heliValues.throttle);
	}

	if(m_HeliDamage)
	{
		m_HeliDamage->SetRequestedVolume(heliValues.damageVolume);
	}

	if(m_HeliDamageBelow600)
	{
		m_HeliDamageBelow600->SetRequestedVolume(heliValues.damageVolume);
	}

	if(m_HeliDamageOneShots)
	{
		m_HeliDamageOneShots->SetRequestedVolume(heliValues.damageVolume);
	}
	
	// Update filters
	m_MainRotorVolume = heliValues.rotorVolume + brokenRotorVolume;
}

// ----------------------------------------------------------------
// Updates engine sounds when the engine is turned on
// ----------------------------------------------------------------
void audHeliAudioEntity::UpdateHeliEngineSoundsOn(audHeliValues& heliValues)
{
	// Use the throttle value from CHeli - this is already quite smoothed, possibly too smoothed for us
	heliValues.throttle = m_Throttle;
	f32 throttleInput = m_Throttle;

	if(m_IsPlayerDrivingVehicle)
	{
		const CPed* playerPed = FindPlayerPed();

		if(playerPed)
		{
			const CControl* pedControl = playerPed->GetControlFromPlayer();

			if(pedControl)
			{
				throttleInput = 1.0f + (pedControl->GetVehicleFlyThrottleUp().GetNorm01() - pedControl->GetVehicleFlyThrottleDown().GetNorm01());
			}				
		}
	}

	heliValues.throttleInput = throttleInput;
	heliValues.rotorVolume = m_RotorThrottleVolCurve.CalculateValue(m_Throttle);
	heliValues.bladeVolume = m_RotorThrottleVolCurve.CalculateValue(m_Throttle);
	heliValues.rotorPitch = (s32)m_RotorThrottlePitchCurve.CalculateValue(m_Throttle);
	heliValues.rearRotorVolume = m_RearRotorThrottleVolCurve.CalculateValue(m_Throttle);
	heliValues.exhaustVolume = m_ExhaustThrottleVolCurve.CalculateValue(m_Throttle);
	heliValues.bankingVolume = m_BankThrottleVolumeCurve.CalculateValue(m_Throttle);
	heliValues.triggerRate = m_RotorSpeedToTriggerSpeedCurve.CalculateValue(m_MainRotorSpeed);

	f32 vehicleHealth = m_Vehicle->GetHealth();
	if (g_OverrideHeliHealth)
	{
		vehicleHealth = g_OverridenHeliHealth;
	}
	heliValues.health = Clamp(vehicleHealth/1000.0f, 0.0f, 1.0f);
	heliValues.damageVolume = audDriverUtil::ComputeDbVolumeFromLinear(m_DamageVolumeCurve.CalculateValue(vehicleHealth));
	heliValues.damageVolumeBelow600 = audDriverUtil::ComputeDbVolumeFromLinear(m_DamageVolumeBelow600Curve.CalculateValue(vehicleHealth));
	// Do a few pitch changes too
	heliValues.bankingPitch = (s32)m_BankThrottlePitchCurve.CalculateValue(m_Throttle);
	heliValues.exhaustPitch = (s32)m_ExhaustThrottlePitchCurve.CalculateValue(m_Throttle);

	f32 bankingAngleVolume = m_BankAngleVolCurve.CalculateValue(heliValues.banking);

	// Calculate coning attenuations
	const Mat34V mat = m_Vehicle->GetMatrix();
	heliValues.rotorVolume += (m_EngineVolumeCone.ComputeAttenuation(mat) + m_RotorThumpVolumeCone.ComputeAttenuation(mat));
	heliValues.bladeVolume += (m_EngineVolumeCone.ComputeAttenuation(mat) + m_RotorBladeVolumeCone.ComputeAttenuation(mat));
	heliValues.rearRotorVolume += m_RearRotorVolumeCone.ComputeAttenuation(mat);
	heliValues.exhaustVolume += m_ExhaustVolumeCone.ComputeAttenuation(mat);
	heliValues.bankingVolume += bankingAngleVolume;

	if (m_Vehicle->m_nVehicleFlags.bIsDrowning)
	{
		m_HeliDamageFluctuator.SetBands(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		m_HeliDamageFluctuator.SetBands(0.0f, 0.2f, 0.8f, 1.0f);
	}

	f32 damageVolumeOffsetLinear = m_HeliDamageFluctuator.CalculateValue();
	f32 damageVolumeOffset = g_HeliDamageVolumeRange*damageVolumeOffsetLinear;

	heliValues.damageVolume += damageVolumeOffset;
	heliValues.damageVolumeBelow600 += damageVolumeOffset;

	// duck the exhaust inversely to the main damage loop
	f32 exhastDamageVolumeOffset = g_HeliDamageVolumeRange - damageVolumeOffset;
	exhastDamageVolumeOffset *= (1.0f-heliValues.health);

	heliValues.exhaustVolume += exhastDamageVolumeOffset;

}

// ----------------------------------------------------------------
// Update engine sounds when its in the process of starting/stopping
// ----------------------------------------------------------------
void audHeliAudioEntity::UpdateHeliEngineSoundStartingStopping(audHeliValues& heliValues)
{
	// get the usual in-flight values, then interp between them and us, so it's fully them when we hand over control
	UpdateHeliEngineSoundsOn(heliValues);

	f32 starting = sm_HeliStartupToNormalCurve.CalculateValue(heliValues.startup);
	f32 regular = 1.0f - starting;

	audHeliValues startingHeliValues;
	startingHeliValues.throttle = 0.0f;
	startingHeliValues.rotorVolume = audDriverUtil::ComputeDbVolumeFromLinear(m_RotorVolStartupCurve.CalculateValue(heliValues.startup));
	startingHeliValues.bladeVolume = audDriverUtil::ComputeDbVolumeFromLinear(m_BladeVolStartupCurve.CalculateValue(heliValues.startup));
	startingHeliValues.rearRotorVolume = audDriverUtil::ComputeDbVolumeFromLinear(m_RearRotorVolStartupCurve.CalculateValue(heliValues.startup));
	startingHeliValues.exhaustVolume = audDriverUtil::ComputeDbVolumeFromLinear(m_ExhaustVolStartupCurve.CalculateValue(heliValues.startup));
	startingHeliValues.bankingVolume = g_SilenceVolume;
	startingHeliValues.damageVolume = g_SilenceVolume;
	startingHeliValues.damageVolumeBelow600 = g_SilenceVolume;
	startingHeliValues.health = 1.0f;

	// Do a few pitch changes too
	startingHeliValues.bankingPitch = 0;
	startingHeliValues.rotorPitch = (s32)m_RotorPitchStartupCurve.CalculateValue(heliValues.startup);
	startingHeliValues.exhaustPitch = (s32)m_ExhaustPitchStartupCurve.CalculateValue(heliValues.startup);
	// Calculate coning attenuations
	const Mat34V mat = m_Vehicle->GetMatrix();
	startingHeliValues.rotorVolume += (m_EngineVolumeCone.ComputeAttenuation(mat) + m_RotorThumpVolumeCone.ComputeAttenuation(mat));
	startingHeliValues.bladeVolume += (m_EngineVolumeCone.ComputeAttenuation(mat) + m_RotorBladeVolumeCone.ComputeAttenuation(mat));
	startingHeliValues.rearRotorVolume += m_RearRotorVolumeCone.ComputeAttenuation(mat);
	startingHeliValues.exhaustVolume += m_ExhaustVolumeCone.ComputeAttenuation(mat);

	// now interp
	heliValues.throttle = starting*startingHeliValues.throttle + regular*heliValues.throttle;
	heliValues.rotorVolume = starting*startingHeliValues.rotorVolume + regular*heliValues.rotorVolume;
	heliValues.bladeVolume = starting*startingHeliValues.bladeVolume + regular*heliValues.bladeVolume;
	heliValues.rearRotorVolume = starting*startingHeliValues.rearRotorVolume + regular*heliValues.rearRotorVolume;
	heliValues.exhaustVolume = starting*startingHeliValues.exhaustVolume + regular*heliValues.exhaustVolume;
	heliValues.bankingVolume = starting*startingHeliValues.bankingVolume + regular*heliValues.bankingVolume;
	heliValues.damageVolume = starting*startingHeliValues.damageVolume + regular*heliValues.damageVolume;
	heliValues.damageVolumeBelow600 = starting*startingHeliValues.damageVolumeBelow600 + regular*heliValues.damageVolumeBelow600;
	heliValues.bankingPitch = (s32)(starting*(f32)(startingHeliValues.bankingPitch) + regular*(f32)(heliValues.bankingPitch));
	heliValues.exhaustPitch = (s32)(starting*(f32)(startingHeliValues.exhaustPitch) + regular*(f32)(heliValues.exhaustPitch));
	heliValues.rotorPitch = (s32)(starting*(f32)(startingHeliValues.rotorPitch) + regular*(f32)(heliValues.rotorPitch));
	heliValues.triggerRate = heliValues.triggerRate;

	if (m_Vehicle->m_nVehicleFlags.bIsDrowning)
	{
		m_HeliDamageFluctuator.SetBands(1.0f, 1.0f, 1.0f, 1.0f);
	}
	else
	{
		m_HeliDamageFluctuator.SetBands(0.0f, 0.2f, 0.8f, 1.0f);
	}

	f32 damageVolumeOffsetLinear = m_HeliDamageFluctuator.CalculateValue();
	f32 damageVolumeOffset = g_HeliDamageVolumeRange*damageVolumeOffsetLinear;

	heliValues.damageVolume += damageVolumeOffset;
	heliValues.damageVolumeBelow600 += damageVolumeOffset;

	// duck the exhaust inversely to the main damage loop
	f32 exhastDamageVolumeOffset = g_HeliDamageVolumeRange - damageVolumeOffset;
	exhastDamageVolumeOffset *= (1.0f-heliValues.health);

	heliValues.exhaustVolume += exhastDamageVolumeOffset;

}

// ----------------------------------------------------------------
// Stop all the helicopter related sounds
// ----------------------------------------------------------------
void audHeliAudioEntity::StopHeliEngineSounds()
{
	StopAndForgetSounds(m_HeliBank, m_HeliExhaust, m_HeliRotor, m_HeliRotorLowFreq, m_HeliRearRotor, m_HeliDamage, m_HeliDamageBelow600, m_HeliDamageOneShots);
	StopAndForgetSounds(m_DistantRotor, m_HeliDamageWarning, m_HeliRotorSlowmo, m_HeliRotorLowFreqSlowmo);

	if(((CHeli*)m_Vehicle)->GetMainRotorSpeed() < 0.1f && m_HeliGoingDown)
	{
		StopGoingDownSound();
	}
	m_EngineOffTemperature = m_Vehicle->m_EngineTemperature;
}

void audHeliAudioEntity::VehicleExploded() 
{
	StopGoingDownSound();
	m_Exploded = true;
}

// ----------------------------------------------------------------
// Play the tail break sound
// ----------------------------------------------------------------
void audHeliAudioEntity::PlayTailBreakSound()
{
	if(m_HeliAudioSettings)
		CreateDeferredSound(m_HeliAudioSettings->TailBreak, m_Vehicle, NULL, true, true);
}

// ----------------------------------------------------------------
// Play the rotor break sound
// ----------------------------------------------------------------
void audHeliAudioEntity::PlayRotorBreakSound()
{
	if(m_HeliAudioSettings)
		CreateDeferredSound(m_HeliAudioSettings->RotorBreak, m_Vehicle, NULL, true, true);	
}

// ----------------------------------------------------------------
// Play rear rotor break sound
// ----------------------------------------------------------------
void audHeliAudioEntity::PlayRearRotorBreakSound()
{
	if(m_HeliAudioSettings)
		CreateDeferredSound(m_HeliAudioSettings->RearRotorBreak, m_Vehicle, NULL, true, true);
}

void audHeliAudioEntity::PlayHookDeploySound()
{
	if(m_HeliAudioSettings)
		CreateDeferredSound(m_HeliAudioSettings->CableDeploy, m_Vehicle, NULL, true, true);	
}

// ----------------------------------------------------------------
// audHeliAudioEntity GetCabinToneSound
// ----------------------------------------------------------------
u32 audHeliAudioEntity::GetCabinToneSound() const
{
	if(m_HeliAudioSettings)
	{
		return m_HeliAudioSettings->CabinToneLoop;
	}

	return audVehicleAudioEntity::GetCabinToneSound();
}

// ----------------------------------------------------------------
// audHeliAudioEntity GetVehicleRainSound
// ----------------------------------------------------------------
u32 audHeliAudioEntity::GetVehicleRainSound(bool interiorView) const
{
	if(m_HeliAudioSettings)
	{
		if(interiorView)
		{
			return m_HeliAudioSettings->VehicleRainSoundInterior;
		}
		else
		{
			return m_HeliAudioSettings->VehicleRainSound;
		}		
	}

	return audVehicleAudioEntity::GetVehicleRainSound(interiorView);
}

// ----------------------------------------------------------------
// Setup volume cones
// ----------------------------------------------------------------
void audHeliAudioEntity::SetupHeliVolumeCones()
{
	Vec3V frontConeDir = Vec3V(V_Y_AXIS_WZERO);
	Vec3V rearConeDir = -Vec3V(V_Y_AXIS_WZERO);
	Vec3V upConeDir = Vec3V(V_Z_AXIS_WZERO);
	Vec3V downConeDir = -Vec3V(V_Z_AXIS_WZERO);

	m_EngineVolumeCone.Init(frontConeDir, m_HeliAudioSettings->RotorConeAtten/100.f, m_HeliAudioSettings->RotorConeFrontAngle, m_HeliAudioSettings->RotorConeRearAngle);
	m_ExhaustVolumeCone.Init(rearConeDir, m_HeliAudioSettings->ExhaustConeAtten/100.f, m_HeliAudioSettings->ExhaustConeFrontAngle, m_HeliAudioSettings->ExhaustConeRearAngle);

	m_RotorBladeVolumeCone.Init(upConeDir, m_HeliAudioSettings->BladeConeAtten/100.f, m_HeliAudioSettings->BladeConeUpAngle, m_HeliAudioSettings->BladeConeDownAngle);
	m_RotorThumpVolumeCone.Init(downConeDir, m_HeliAudioSettings->ThumpConeAtten/100.f, m_HeliAudioSettings->ThumpConeDownAngle, m_HeliAudioSettings->ThumpConeUpAngle);
	m_RearRotorVolumeCone.Init(rearConeDir, m_HeliAudioSettings->RearRotorConeAtten/100.f, m_HeliAudioSettings->RearRotorConeFrontAngle, m_HeliAudioSettings->RearRotorConeRearAngle);
}

// ----------------------------------------------------------------
// Trigger downwash effect
// ----------------------------------------------------------------
void audHeliAudioEntity::TriggerDownwash(const u32 soundHash, const Vector3 &pos, const f32 distEvo, const f32 UNUSED_PARAM(speedEvo), const f32 UNUSED_PARAM(angleEvo), const f32 rotorEvo)
{
	if(!m_HeliDownwash || m_HeliDownwashHash != soundHash)
	{
		if(m_HeliDownwash)
		{
			m_HeliDownwash->StopAndForget();
		}
		audSoundInitParams initParams;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		CreateAndPlaySound_Persistent(soundHash, &m_HeliDownwash, &initParams);
		m_HeliDownwashHash = soundHash;
	}

	m_LastHeliDownwashTime = fwTimer::GetTimeInMilliseconds();

	if(m_HeliDownwash)
	{
#if __BANK
		if(g_UseDebugHeliTracker)
			m_HeliDownwash->SetRequestedPosition(m_DebugTracker.GetPosition());
		else
#endif
			m_HeliDownwash->SetRequestedPosition(pos);

		const f32 dbVol = audDriverUtil::ComputeDbVolumeFromLinear(rotorEvo * Min(1.f, 2.f*(1.f-distEvo)));
		m_HeliDownwash->SetRequestedVolume(dbVol+m_MainRotorVolume);
	}
}

void audHeliAudioEntity::TriggerEngineFailedToStart() 
{
	if(m_Vehicle && m_Vehicle->GetWillEngineStart())
	{
		m_FailedToStartCount = 0;
		m_LastFailToStartTime = 0;
		return; 
	}
	
	u32 currentTime = fwTimer::GetTimeInMilliseconds();
	if(currentTime > m_LastFailToStartTime + g_TimeBetweenRestartSounds && m_FailedToStartCount < g_MaxFailedToStartTries)
	{
		if(m_HeliAudioSettings)
		{
			if(!m_FailedToStartSound)
			{
				audSoundInitParams initParams;
				initParams.EnvironmentGroup = m_EnvironmentGroup;
				SetTracker(initParams);
				CreateAndPlaySound_Persistent(m_HeliAudioSettings->StartUpFailOneShot, &m_FailedToStartSound, &initParams);
			}
		}

		m_FailedToStartCount++;
		m_LastFailToStartTime = currentTime;
	}
}

// ----------------------------------------------------------------
// Deploy landing gear
// ----------------------------------------------------------------
void audHeliAudioEntity::DeployLandingGear()
{
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(!m_HasCustomLandingGear)
	{
		if(m_LandingGearSoundSet.IsInitialised())
		{
			TriggerLandingGearSound(m_LandingGearSoundSet, modelNameHash == ATSTRINGHASH("THRUSTER", 0x58CDAF30)? ATSTRINGHASH("legs_down", 0xE91F08B4) : ATSTRINGHASH("deploy", 0xC9200261));
		}
	}
}

// ----------------------------------------------------------------
// Retract landing gear
// ----------------------------------------------------------------
void audHeliAudioEntity::RetractLandingGear()
{	
	const u32 modelNameHash = GetVehicleModelNameHash();

	if(m_HasCustomLandingGear)
	{
		if(m_LandingGearSoundSet.IsInitialised())
		{
			TriggerLandingGearSound(m_LandingGearSoundSet, modelNameHash == ATSTRINGHASH("THRUSTER", 0x58CDAF30)? ATSTRINGHASH("legs_up", 0x18C3884B) : ATSTRINGHASH("retract", 0x8E1F66E6));
		}
	}
}

// ----------------------------------------------------------------
// Trigger a door open sound
// ----------------------------------------------------------------
void audHeliAudioEntity::TriggerDoorOpenSound(eHierarchyId doorId)
{
	if(!m_HasCustomLandingGear || (doorId != VEH_FOLDING_WING_L && doorId != VEH_FOLDING_WING_R))
	{
		if(m_HeliAudioSettings)
		{
			u32 hash = m_HeliAudioSettings->DoorOpen;
			if(doorId == VEH_DOOR_DSIDE_R)
			{
				hash = m_HeliAudioSettings->SecondaryDoorStartOpen;
			}
			TriggerDoorSound(hash, doorId);
		}
	}
}

// ----------------------------------------------------------------
// Trigger a door shut sound
// ----------------------------------------------------------------
void audHeliAudioEntity::TriggerDoorCloseSound(eHierarchyId doorId, const bool UNUSED_PARAM(isBroken))
{
	if(!m_HasCustomLandingGear || (doorId != VEH_FOLDING_WING_L && doorId != VEH_FOLDING_WING_R))
	{
		// if it's the player vehicle, and they're pissed off - slam the door. Note that this'll have his passengers slam the door too - oh well.
		f32 volumeOffset = 0.0f;

		if (m_Vehicle && m_Vehicle == CGameWorld::FindLocalPlayerVehicle() && 
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

		if(m_HeliAudioSettings)
		{
			u32 hash = m_HeliAudioSettings->DoorClose;
			if(doorId == VEH_DOOR_DSIDE_R && m_HeliAudioSettings->SecondaryDoorClose != g_NullSoundHash)
			{
				hash = m_HeliAudioSettings->SecondaryDoorClose;
			}
			TriggerDoorSound(m_HeliAudioSettings->DoorClose, doorId, volumeOffset);
		}
	}
}

// ----------------------------------------------------------------
// Trigger door open sound
// ----------------------------------------------------------------
void audHeliAudioEntity::TriggerDoorFullyOpenSound(eHierarchyId doorId)
{
	if(m_HeliAudioSettings)
	{
		if(!m_HasCustomLandingGear || (doorId != VEH_FOLDING_WING_L && doorId != VEH_FOLDING_WING_R))
		{
			u32 hash = m_HeliAudioSettings->DoorLimit;
			if(doorId == VEH_DOOR_DSIDE_R && m_HeliAudioSettings->SecondaryDoorLimit != g_NullSoundHash)
			{
				hash = m_HeliAudioSettings->SecondaryDoorLimit;
			}
			TriggerDoorSound(m_HeliAudioSettings->DoorLimit, doorId);
		}
	}
}

void audHeliAudioEntity::TriggerDoorStartOpenSound(eHierarchyId doorId) 
{
	if(m_HeliAudioSettings)
	{
		if(!m_HasCustomLandingGear || (doorId != VEH_FOLDING_WING_L && doorId != VEH_FOLDING_WING_R))
		{
			u32 hash = m_HeliAudioSettings->DoorOpen;
			if(doorId == VEH_DOOR_DSIDE_R && m_HeliAudioSettings->SecondaryDoorStartOpen != g_NullSoundHash)
			{
				hash = m_HeliAudioSettings->SecondaryDoorStartOpen;
			}		
			TriggerDoorSound(hash, doorId);
		}
	}
}

void audHeliAudioEntity::TriggerDoorStartCloseSound(eHierarchyId doorId) 
{
	if(m_HeliAudioSettings && m_Vehicle->GetStatus() != STATUS_WRECKED)
	{
		if(doorId == VEH_DOOR_DSIDE_R && m_HeliAudioSettings->SecondaryDoorStartClose != g_NullSoundHash)
		{
			u32 hash = m_HeliAudioSettings->SecondaryDoorStartClose;
			TriggerDoorSound(hash, doorId);
		}
	}
}

// ----------------------------------------------------------------
// Trigger going down sound
//
void audHeliAudioEntity::TriggerGoingDownSound()
{
	m_PlayGoingDown = true;
}

void audHeliAudioEntity::PlayGoingDownSound()
{
	m_PlayGoingDown = false;

	if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
	{
		return;
	}

	// Only allow if there are lots of slots free, and we're not already playing the sound
	if(m_Shotdown || (g_SpeechManager.GetNumVacantAmbientSlots() <= 3) || m_HeliGoingDown)
	{
		return;
	}

	if(m_Vehicle->m_nVehicleFlags.bIsDrowning || ((CHeli*)m_Vehicle)->GetMainRotorSpeed() < 0.1f)
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
			//m_PlayingRules[index].speechSlot = speechSlot;
			initParams.WaveSlot = speechSlot;
			initParams.AllowLoad = true;
			initParams.PrepareTimeLimit = 5000;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			SetTracker(initParams);
			g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(speechSlotId, 0, 
#if __BANK
				"HELI_GOING_DOWN",
#endif
				0);

			u32 soundHash = m_IsPlayerVehicle ? ATSTRINGHASH("HeliGoingDownPlayer", 0x2CF24ECF) : ATSTRINGHASH("HeliGoingDown", 0xB60C2298);
			CreateAndPlaySound_Persistent(audVehicleAudioEntity::GetExtrasSoundSet()->Find(soundHash), &m_HeliGoingDown, &initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(audVehicleAudioEntity::GetExtrasSoundSet()->GetNameHash(), soundHash, &initParams, m_HeliGoingDown, m_Vehicle, eNoGlobalSoundEntity, ReplaySound::CONTEXT_HELI_GOING_DOWN);)
		}
		else
		{
			g_SpeechManager.FreeAmbientSpeechSlot(speechSlotId, true);
		}
	}
	g_AudioScannerManager.TriggerCopDispatchInteraction(SCANNER_CDIT_HELI_MAYDAY_DISPATCH);
	m_Shotdown = true;
}

void audHeliAudioEntity::StopGoingDownSound()
{
	if(m_HeliGoingDown)
	{
		m_HeliGoingDown->StopAndForget();
	}

	CHeli* heli = static_cast<CHeli*>(m_Vehicle);
	if(heli && (heli->GetIsPoliceDispatched() || heli->GetIsSwatDispatched()) && !m_HasBeenScannerDecremented)
	{
		g_AudioScannerManager.DecrementHelisInChase();
		m_HasBeenScannerDecremented = true;
	}
}


void audHeliAudioEntity::FreeSpeechSlot()
{
	if(m_SpeechSlotId >= 0)
	{
		g_SpeechManager.FreeAmbientSpeechSlot(m_SpeechSlotId, true);
		m_SpeechSlotId = -1;
	}
}

// ----------------------------------------------------------------
// Get a sound from the object data
// ----------------------------------------------------------------
u32 audHeliAudioEntity::GetSoundFromObjectData(u32 fieldHash) const
{ 
	if(m_HeliAudioSettings) 
	{ 
		u32 *ret = (u32 *)m_HeliAudioSettings->GetFieldPtr(fieldHash);
		return (ret) ? *ret : 0; 
	} 
	return 0;
}

void audHeliAudioEntity::UpdateJetPackSounds()
{
	if(GetVehicleModelNameHash() == ATSTRINGHASH("THRUSTER", 0x58CDAF30))
	{
		CHeli* heli = (CHeli*)m_Vehicle;
		const u32 timeMs = fwTimer::GetTimeInMilliseconds();
		const bool inStrafeMode = heli->GetStrafeMode();

		if(inStrafeMode || timeMs - m_LastTimeInStrafeMode < 200)
		{
			const f32 thrusterForce = ((CHeli*)m_Vehicle)->GetJetpackStrafeForceScale();
			const f32 thrusterLeftForce = Max(thrusterForce, 0.f);
			const f32 thrusterRightForce = Abs(Min(thrusterForce, 0.f));

			const f32 thrustLeftChangeRate = (thrusterLeftForce - m_PrevJetpackThrusterLeftForce) * fwTimer::GetInvTimeStep();
			const f32 thrustRightChangeRate = (thrusterRightForce - m_PrevJetpackThrusterRightForce) * fwTimer::GetInvTimeStep();

			const bool triggerThrusterLeft = thrustLeftChangeRate >= g_JetpackStrafeThrusterTriggerLimit;
			const bool triggerThrusterRight = thrustRightChangeRate >= g_JetpackStrafeThrusterTriggerLimit;
			
			if((triggerThrusterLeft || triggerThrusterRight) && timeMs - m_LastJetpackStrafeTime > g_MinJetpackRetriggerTime)
			{
				if (m_Vehicle->m_nVehicleFlags.bEngineOn && !m_Vehicle->m_nVehicleFlags.bIsDrowning && !m_Vehicle->m_nVehicleFlags.bDisableParticles)
				{
					audSoundSet soundset;
					const u32 soundsetName = ATSTRINGHASH("thruster_turbine_sounds", 0x9C4F5C78);
					const u32 soundName = ATSTRINGHASH("strafe", 0xC90654B7);

					if(soundset.Init(soundsetName))
					{
						audSoundInitParams initParams;
						initParams.u32ClientVar = triggerThrusterRight ? (u32)AUD_VEHICLE_SOUND_AIRBRAKE_R : (u32)AUD_VEHICLE_SOUND_AIRBRAKE_L;
						initParams.UpdateEntity = true;
						initParams.EnvironmentGroup = GetEnvironmentGroup();
						CreateAndPlaySound(soundset.Find(soundName), &initParams);
						REPLAY_ONLY(CReplayMgr::ReplayRecordSound(soundsetName, soundName, &initParams, m_Vehicle));
						m_LastJetpackStrafeTime = timeMs;
					}
				}
			}	

			m_PrevJetpackThrusterLeftForce = thrusterLeftForce;
			m_PrevJetpackThrusterRightForce = thrusterRightForce;
		}
		else
		{
			m_PrevJetpackThrusterLeftForce = 0.f;
			m_PrevJetpackThrusterRightForce = 0.f;
		}

		if(inStrafeMode)
		{
			m_LastTimeInStrafeMode = timeMs;
		}
	}	
}


void audHeliAudioEntity::UpdateCustomLandingGearSounds()
{
	if(m_HasCustomLandingGear)
	{
		audSoundInitParams initParams;
		initParams.UpdateEntity = true;
		initParams.EnvironmentGroup = m_EnvironmentGroup;
		initParams.TrackEntityPosition = true;
		initParams.u32ClientVar = (u32)AUD_VEHICLE_SOUND_UNKNOWN;

		const CLandingGear::eLandingGearPublicStates landingGearState = ((CHeli*)m_Vehicle)->GetLandingGear().GetPublicState();

		if(landingGearState != m_PrevLandingGearState)
		{			
			if(m_CustomLandingGearLoop)
			{
				m_CustomLandingGearLoop->StopAndForget(true);
			}

			switch(landingGearState)
			{
			case CLandingGear::STATE_LOCKED_DOWN:
				break;
			case CLandingGear::STATE_LOCKED_UP:
				{
					CreateAndPlaySound(m_LandingGearSoundSet.Find(ATSTRINGHASH("wheel_flaps_close", 0xF29107C4)), &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_LandingGearSoundSet.GetNameHash(), ATSTRINGHASH("wheel_flaps_close", 0xF29107C4), &initParams, m_Vehicle));
				}
				break;
			case CLandingGear::STATE_DEPLOYING:
				{
					CreateAndPlaySound(m_LandingGearSoundSet.Find(ATSTRINGHASH("wheel_flaps_open", 0xE2C58753)), &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSound(m_LandingGearSoundSet.GetNameHash(), ATSTRINGHASH("wheel_flaps_open", 0xE2C58753), &initParams, m_Vehicle));
				}
				break;
			case CLandingGear::STATE_RETRACTING:
				{
					CreateAndPlaySound_Persistent(m_LandingGearSoundSet.Find(ATSTRINGHASH("landing_gear_retract", 0xD020783)), &m_CustomLandingGearLoop, &initParams);
					REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_LandingGearSoundSet.GetNameHash(), ATSTRINGHASH("landing_gear_retract", 0xD020783), &initParams, m_CustomLandingGearLoop, m_Vehicle));
				}
				break;
			case CLandingGear::STATE_BROKEN:
				break;
			default:
				break;
			}

			m_PrevLandingGearState = landingGearState;
		}
		// If we're in the deploying state, defer the wheel deploy loop until the bay doors are fully open
		else if(landingGearState == CLandingGear::STATE_DEPLOYING && !m_CustomLandingGearLoop && ((CHeli*)m_Vehicle)->GetLandingGear().GetAuxDoorOpenRatio() >= 0.95f)
		{
			CreateAndPlaySound_Persistent(m_LandingGearSoundSet.Find(ATSTRINGHASH("landing_gear_deploy", 0x18CA1D46)), &m_CustomLandingGearLoop, &initParams);
			REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(m_LandingGearSoundSet.GetNameHash(), ATSTRINGHASH("landing_gear_deploy", 0x18CA1D46), &initParams, m_CustomLandingGearLoop, m_Vehicle));
		}
	}
}

void audHeliAudioEntity::TriggerRotorImpactSounds()
{
	m_LastRotorImpactTime = fwTimer::GetTimeInMilliseconds();
}

void audHeliAudioEntity::UpdateRotorImpactSounds(audHeliValues& heliValues)
{
	u32 currentTime = fwTimer::GetTimeInMilliseconds();

	static bank_s32 delayMult = 2;      // multiple of actual retrigger rate because it's not a low latency sound
	static bank_s32 stopTimeOut = 500;
	if(m_Vehicle->GetStatus() != STATUS_WRECKED && (currentTime < (m_LastRotorImpactTime + stopTimeOut)))
	{
		if(!m_RotorImpactSound)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			static bank_float volume = 0.0f;
			initParams.Volume = volume;
			SetTracker(initParams);
			CreateAndPlaySound_Persistent(ATSTRINGHASH("HELICOPTER_ROTOR_GENERIC_COLLISION_RETRIG", 0x22E7400), (audSound**)&m_RotorImpactSound, &initParams);

			REPLAY_ONLY(CReplayMgr::ReplayRecordSoundPersistant(ATSTRINGHASH("HELICOPTER_ROTOR_GENERIC_COLLISION_RETRIG", 0x22E7400), &initParams, (audSound*)m_RotorImpactSound, GetOwningEntity(), eNoGlobalSoundEntity);)
		}
	}
	else
	{
		if(m_RotorImpactSound)
			m_RotorImpactSound->StopAndForget();
	}

	if(m_RotorImpactSound)
	{
		f32 volume = audDriverUtil::ComputeDbVolumeFromLinear(m_RearRotorVolStartupCurve.CalculateValue(m_MainRotorSpeed));
		m_RotorImpactSound->SetRequestedVolume(volume);
		if(m_RotorImpactSound->GetSoundTypeID() == RetriggeredOverlappedSound::TYPE_ID)
		{
			s32 delay = (s32)(heliValues.triggerRate * 1000.0f * (f32)delayMult);
			delay = Clamp(delay, 0, 1000);
			m_RotorImpactSound->SetDelayTime(delay);
		}
	}
}

f32 audHeliAudioEntity::UpdateSlowmoRotorSounds()
{
	f32 volume = 0.0f;

	// never use slowmo heli sounds in replay
	bool isSlowMo = audNorthAudioEngine::IsInSlowMo() REPLAY_ONLY( && !CReplayMgr::IsEditModeActive());
	if(isSlowMo)
	{
		volume = m_HeliSlowmoSmoother.CalculateValue(0.0f, g_AudioEngine.GetTimeInMilliseconds()); //should be a timer that doesn't slow down
		volume = audDriverUtil::ComputeDbVolumeFromLinear(volume);
		
		if (m_HeliAudioSettings)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			SetTracker(initParams);

			if(!m_HeliRotorLowFreqSlowmo && m_HeliRotorLowFreq) // only start it up if the main rotor sound is playing, prevent starts during shut down
			{					
				CreateAndPlaySound_Persistent(m_HeliAudioSettings->SlowMoRotorLowFreqLoop, &m_HeliRotorLowFreqSlowmo, &initParams);
			}

			if(!m_HeliRotorSlowmo && m_HeliRotor) // only start it up if the main rotor sound is playing, prevent starts during shut down
			{					
				Assign(initParams.EffectRoute, GetEngineEffectRoute());
				CreateAndPlaySound_Persistent(m_HeliAudioSettings->SlowMoRotorLoop, &m_HeliRotorSlowmo, &initParams);
			}
		}
	}
	else
	{
		volume = m_HeliSlowmoSmoother.CalculateValue(1.0f, g_AudioEngine.GetTimeInMilliseconds()); //should be a timer that doesn't slow down
		volume = audDriverUtil::ComputeDbVolumeFromLinear(volume);
		if(m_HeliRotorSlowmo)
		{
			m_HeliRotorSlowmo->Stop();
		}

		if(m_HeliRotorLowFreqSlowmo)
		{
			m_HeliRotorLowFreqSlowmo->Stop();
		}
	}
	return volume;
}

void audHeliAudioEntity::UpdateWheelSounds()
{
	CWheel* touchingWheel = NULL;
	for(u32 loop = 0; loop < m_Vehicle->GetNumWheels(); loop++)
	{
		CWheel* wheel = m_Vehicle->GetWheel(loop);
		if(wheel && wheel->GetIsTouching())
		{
			touchingWheel = wheel;
		}
	}

	u32 gritSound = g_NullSoundHash;
	f32 finalVolLin = 0.0f;
	f32 speed = 0.0f;
	if(touchingWheel)
	{
		speed = m_CachedVehicleVelocity.Mag();

		f32 gritSpeedVol = sm_GritSpeedToLinVolCurve.CalculateValue(Abs(speed));
		finalVolLin = touchingWheel->GetIsTouching() ? gritSpeedVol : 0.0f;

		if(m_CachedMaterialSettings)
		{
			if(touchingWheel->GetFragChild(0) > 0) // this means it's an actual wheel, not a runner
			{
				gritSound = m_CachedMaterialSettings->DetailTyreRoll;
			}
			else
			{
				gritSound = m_CachedMaterialSettings->ScrapeSound;
			}
		}
	}

	static bank_float minSpeedThreshold = 0.01f;
	if(m_ScrapeSound && (gritSound != m_PrevScrapeSound || speed < minSpeedThreshold))
	{
		m_ScrapeSound->StopAndForget();
		if(m_HardScrapeSound)
		{
			m_HardScrapeSound->StopAndForget();
		}
		BANK_ONLY(if(g_DebugSuspension)	Displayf("Stop scrape speed %f", speed);)
	}

	if(gritSound != g_NullSoundHash)
	{
		if(finalVolLin > g_SilenceVolumeLin && speed > minSpeedThreshold)
		{
			audSoundInitParams initParams;
			initParams.EnvironmentGroup = m_EnvironmentGroup;
			initParams.TrackEntityPosition = true;
			if(!m_ScrapeSound)
			{
				CreateAndPlaySound_Persistent(gritSound, &m_ScrapeSound, &initParams);
				BANK_ONLY(if(g_DebugSuspension)	Displayf("Play scrape speed %f vol %f", speed, finalVolLin);)
			}
			if(touchingWheel->GetFragChild(0) == 0 && m_CachedMaterialSettings->HardImpact != g_NullSoundHash && !m_HardScrapeSound)
			{
				CreateAndPlaySound_Persistent(m_HeliAudioSettings->HardScrape, &m_HardScrapeSound, &initParams);
				BANK_ONLY(if(g_DebugSuspension)	Displayf("Play scrape speed %f vol %f", speed, finalVolLin);)
			}

			if(m_ScrapeSound)
			{
				m_ScrapeSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(finalVolLin));
			}
			if(m_HardScrapeSound)
			{
				m_HardScrapeSound->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(finalVolLin));
			}
		}
		else if(m_ScrapeSound)
		{
			if(m_ScrapeSound)
			{
				m_ScrapeSound->StopAndForget();
				BANK_ONLY(if(g_DebugSuspension)	Displayf("Stop scrape speed %f", speed);)
			}
			if(m_HardScrapeSound)
			{
				m_HardScrapeSound->StopAndForget();
			}
		}
	}

	m_PrevScrapeSound = gritSound;
}

void audHeliAudioEntity::UpdateSuspension()
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

			f32 ratio = Clamp((absCompressionChange - m_HeliAudioSettings->MinSuspCompThresh) / (m_HeliAudioSettings->MaxSuspCompThres - m_HeliAudioSettings->MinSuspCompThresh - (damageFactor*2.f)), 0.0f,1.0f);
#if __BANK
			if(g_DebugSuspension)
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
				if(compressionChange*m_LastCompressionChange[i] < 0.f)
				{
					// suspension has changed direction - cancel previous sound
					m_SuspensionSounds[i]->StopAndForget();
				}
				else
				{
					// Disabling volume update so that we hear the whole one shot rather than it being played then immediately muted
					// m_SuspensionSounds[i]->SetRequestedVolume(audDriverUtil::ComputeDbVolumeFromLinear(sm_SuspensionVolCurve.CalculateValue(ratio)));
					m_SuspensionSounds[i]->SetRequestedPosition(pos);
				}
			}

			if(absCompressionChange > m_HeliAudioSettings->MinSuspCompThresh)
			{
				if(!m_SuspensionSounds[i])
				{
					// trigger a suspension sound
					u32 soundHash;
					if(compressionChange > 0)
					{
						BANK_ONLY(if(g_DebugSuspension) Displayf("Suspension Down");)
						soundHash = m_HeliAudioSettings->SuspensionDown;
					}
					else
					{
						BANK_ONLY(if(g_DebugSuspension) Displayf("Suspension Up");)
						soundHash = m_HeliAudioSettings->SuspensionUp;
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

void audHeliAudioEntity::UpdateEngineCooling()
{
	CHeli* heli = static_cast<CHeli*>(m_Vehicle);

	if(heli)
	{
		static const f32 engineCooldownTemperatureRange = 20.0f;
		const f32 baseTemperature = g_weather.GetTemperature(m_Vehicle->GetTransform().GetPosition());
		const bool shouldEngineCoolingStop = (m_Vehicle->m_nVehicleFlags.bIsDrowning ||  m_Vehicle->m_nVehicleFlags.bEngineOn || m_Vehicle->m_EngineTemperature <= m_EngineOffTemperature - engineCooldownTemperatureRange);
		const bool shouldEngineCoolingStart = (!m_Vehicle->m_nVehicleFlags.bIsDrowning && ((m_HeliState == AUD_HELI_STOPPING || m_HeliState == AUD_HELI_OFF) && m_Vehicle->m_EngineTemperature > baseTemperature + 50.f));
		const f32 engineCoolingProgress = Clamp((engineCooldownTemperatureRange - (m_EngineOffTemperature - m_Vehicle->m_EngineTemperature))/engineCooldownTemperatureRange, 0.0f, 1.0f);

#if __BANK
		if(g_ShowHeliEngineCooling)
		{
			char tempString[128];
			sprintf(tempString, "Cooling Progress %4.2f    OffTemp %4.2f    Temp:%4.2f   Occ:%4.2f", engineCoolingProgress, m_EngineOffTemperature, m_Vehicle->m_EngineTemperature, m_DebugOcclusion);
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
				CreateSound_PersistentReference(m_HeliAudioSettings->EngineCooling, &m_EngineCoolingSound, &initParams);

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

bool audHeliAudioEntity::AircraftWarningVoiceIsMale()
{
	if(m_HeliAudioSettings)
	{
		return AUD_GET_TRISTATE_VALUE(m_HeliAudioSettings->Flags, FLAG_ID_HELIAUDIOSETTINGS_AIRCRAFTWARNINGVOICEISMALE) == AUD_TRISTATE_TRUE;
	}
	return true;
}

#if GTA_REPLAY
void audHeliAudioEntity::ProcessReplayAudioState(u8 PacketState)
{
	if( m_HeliState == AUD_HELI_OFF && PacketState == AUD_HELI_ON && !m_HeliShouldSkipStartup )
	{
		SetHeliShouldSkipStartup(true);
	}
}

void audHeliAudioEntity::SetSpeechSlotID(s32 speechSlotId)
{
	m_SpeechSlotId = speechSlotId;
	m_LastReplaySpeechSlotAllocTime = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);	
}
#endif //GTA_REPLAY

#if __BANK

void audHeliAudioEntity::ShowHeliValues(audHeliValues& heliValues)
{
	CHeli* heli = static_cast<CHeli*>(m_Vehicle);

	if(heli && g_DebugHeli && (m_IsPlayerVehicle || heli == g_pFocusEntity))
	{
		s16 pitchShift = sm_PitchShift[m_PitchShiftIndex];
#if __BANK
		if(m_IsPlayerVehicle && g_PitchShiftIndex != -1)
		{
			pitchShift = sm_PitchShift[g_PitchShiftIndex];
		}
#endif

		char tempString[512];
		static bank_float lineInc = 0.015f;
		f32 lineBase = 0.1f;
		formatf(tempString, "bladeVolume     %4.2f", heliValues.bladeVolume); 		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString ); 	lineBase += lineInc;
		formatf(tempString, "rotorVolume     %4.2f", heliValues.rotorVolume);		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "rotorPitch      %5d", heliValues.rotorPitch+pitchShift);grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );	lineBase += lineInc;
		formatf(tempString, "exhaustVolume   %4.2f", heliValues.exhaustVolume);		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "exhaustPitch    %5d", heliValues.exhaustPitch+pitchShift);grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );	lineBase += lineInc;
		formatf(tempString, "bankingVolume   %4.2f", heliValues.bankingVolume);		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "bankingPitch    %5d", heliValues.bankingPitch+pitchShift);grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );	lineBase += lineInc;
		formatf(tempString, "rearRotorVolume %4.2f", heliValues.rearRotorVolume);	grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "throttleInput   %4.2f", heliValues.throttleInput);		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "throttle        %4.2f", heliValues.throttle);			grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "startup         %4.2f", heliValues.startup);			grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "banking         %4.2f", heliValues.banking);			grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "health          %4.2f", heliValues.health);			grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "damageVolume    %4.2f    %s", heliValues.damageVolume, m_HeliDamage?"on":"off");					grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "damageVolume600 %4.2f    %s", heliValues.damageVolumeBelow600, m_HeliDamageBelow600?"on":"off");	grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "occlusion		 %4.2f", m_DebugOcclusion);				grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "triggerRate     %4.2f", heliValues.triggerRate);		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "Throttle        %4.2f", m_Throttle);					grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "RotorSpeed      %4.2f", m_MainRotorSpeed);				grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );		lineBase += lineInc;
		formatf(tempString, "EngineOn flag   %d        EngineOn %d", m_Vehicle->m_nVehicleFlags.bEngineOn, IsEngineOn()); grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString ); lineBase += lineInc;
		formatf(tempString, "Health          %4.2f     MissFire %d  RecoverMissFire %d   Time %4.2f", 
			m_Vehicle->GetVehicleDamage()->GetEngineHealth(), heli->m_Transmission.GetCurrentlyMissFiring(), 
			heli->m_Transmission.GetCurrentlyRecoveringFromMissFiring(),
			heli->m_Transmission.GetOverrideMissFireTime());
		grcDebugDraw::Text(Vector2(0.1f, lineBase), Color32(255,255,255), tempString );
	}
}
// ----------------------------------------------------------------
// Update debug stuff
// ----------------------------------------------------------------
void audHeliAudioEntity::UpdateDebug()
{
	// If this is enabled, we fudge the tracker position to simulate the plane flying overhead every x seconds 
	if(g_UseDebugHeliTracker)
	{
		const ScalarV distanceTravelled = ScalarV((g_DebugHeliTrackerFlybyDist/2.0f) - (g_DebugHeliTrackerFlybyDist * (g_DebugHeliTrackerTimer/g_DebugHeliTrackerFlybyTime)));
		const Vec3V height(0.0f, 0.0f, g_DebugHeliTrackerFlybyHeight);
		Vec3V newPosition = m_Vehicle->GetTransform().GetPosition() + (m_Vehicle->GetTransform().GetForward() * distanceTravelled);
		newPosition = newPosition + height;
		m_DebugTracker.SetPosition(RCC_VECTOR3(newPosition));

		//for(u32 loop = 0; loop < PLANE_LOOPS_MAX; loop++)
		//{
		//	if(m_PlaneLoops[loop])
		//	{
		//		m_PlaneLoops[loop]->SetRequestedPosition(VECTOR3_TO_VEC3V(newPosition));
		//	}
		//}

		m_ExhaustOffsetPos = newPosition;
		m_EngineOffsetPos = newPosition;

		g_DebugHeliTrackerTimer += fwTimer::GetTimeStep();

		if(g_DebugHeliTrackerTimer > g_DebugHeliTrackerFlybyTime)
		{
			g_DebugHeliTrackerTimer = 0.0f;
		}

		static bank_float size = 1.0f;
		grcDebugDraw::Sphere(newPosition, size, Color_red);
	}
}


// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
void audHeliAudioEntity::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Helicopters", false);


	
		bank.AddSlider("g_HeliRotorTriggerRate", &g_HeliRotorTriggerRate, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("g_HeliLowPassFilter", &g_HeliLowPassFilter, 0.0f, 23999.0f, 100.0f);
		
		bank.AddSlider("Time Between Failed Restart Sounds(ms)", &g_TimeBetweenRestartSounds, 0, 10000, 1);
		bank.AddSlider("Max Failed To Start Tries", &g_MaxFailedToStartTries, 0, 10, 1);

		bank.AddSlider("g_HeliStartupTime(ms)", &g_HeliStartupTime, 0.0f, 10000.0f, 1.0f);
		bank.AddSlider("g_HeliWinddownTime(ms)", &g_HeliWinddownTime, 0.0f, 10000.0f, 1.0f);
		bank.AddSlider("g_HeliSlowMoPitch", &g_HeliSlowMoPitch, -3200, 0, 1);
		bank.AddSlider("Player PitchShiftIndex", &g_PitchShiftIndex, -1, 2, 1);
		bank.AddToggle("g_DebugHeli", &g_DebugHeli);
		bank.AddToggle("Show Engine Cooling", &g_ShowHeliEngineCooling);
		bank.AddToggle("EnableCrazyHeliSounds", &g_EnableCrazyHeliSounds);
		bank.AddSlider("g_HeliTimeWarpGap", &g_HeliTimeWarpGap, 0.f, 1000.f, 0.001f);
		bank.AddSlider("g_HeliDamageRate", &g_HeliDamageRate, 0.0f, 10000.f, 1.0f);
		bank.AddSlider("g_HeliInterFlipProb", &g_HeliInterFlipProb, 0.0f, 1000.f, 1.0f);
		bank.AddSlider("g_HeliIntraFlipProb", &g_HeliIntraFlipProb, 0.0f, 1000.f, 1.0f);
		bank.AddToggle("g_OverrideHeliHealth", &g_OverrideHeliHealth);
		bank.AddSlider("g_OverridenHeliHealth", &g_OverridenHeliHealth, 0.0f, 1000.f, 1.0f);
		bank.AddSlider("g_HeliDamageVolumeRange", &g_HeliDamageVolumeRange, -100.0f, 0.f, 1.0f);
		bank.AddSlider("g_MinHealthForHeliWarning", &g_MinHealthForHeliWarning, 0.0f, 1000.f, 1.0f);
		bank.AddSlider("Jetpack Thruster Trigger Limit", &g_JetpackStrafeThrusterTriggerLimit, 0.0f, 5.f, 0.01f);

		bank.AddToggle("g_PlayCabinTone", &g_PlayCabinTone);
		bank.AddToggle("g_DebugSuspension", &g_DebugSuspension);

		bank.PushGroup("Debug Flybys", false);
			bank.AddToggle("Enable Debug Flybys", &g_UseDebugHeliTracker);
			bank.AddSlider("Flyby Time", &g_DebugHeliTrackerFlybyTime, 0.0f, 1000.0f, 0.5f);
			bank.AddSlider("Flyby Distance", &g_DebugHeliTrackerFlybyDist, 0.0f, 10000.0f, 10.0f);
			bank.AddSlider("Flyby Height", &g_DebugHeliTrackerFlybyHeight, 0.0f, 10000.0f, 10.0f);
		bank.PopGroup();

	bank.PopGroup();
}
#endif
